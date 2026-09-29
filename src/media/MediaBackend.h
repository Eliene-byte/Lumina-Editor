// MediaBackend.h - Interface de decodificacao e codificacao.
//
// A UI e o compositor nunca falam com FFmpeg diretamente: falam com esta
// interface. Isso permite (a) compilar o editor inteiro sem FFmpeg, mocking em
// teste e (b) trocar a implementacao (AV1, NVDEC, DirectX) sem tocar na UI.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/MediaPool.h"
#include "core/Types.h"

namespace lmn::media {

// Um quadro decodificado, em RGBA de ponto flutuante ou de 8 bits por canal.
struct VideoFrame {
    int width = 0;
    int height = 0;
    int stride = 0;            // bytes por linha
    bool float16 = false;      // origem em ponto flutuante
    std::vector<uint8_t> data; // RGBA, 4 bytes (ou 8 se float16) por pixel
    Time pts = 0.0;            // tempo de apresentacao
    int64_t frameIndex = -1;

    [[nodiscard]] bool valid() const { return width > 0 && height > 0 && !data.empty(); }
    [[nodiscard]] size_t bytesPerPixel() const { return float16 ? 8 : 4; }
};

// Formatos que sabemos ler sem perguntar ao FFmpeg.
enum class ContainerKind : uint8_t {
    Unknown = 0,
    Mp4, Mov, Avi, Mkv, Webm, Wav, Mp3, Flac,
    Jpeg, Png, Tiff, Bmp, Exr, Psd, Svg,
    ImageSequence,
    Count,
};

struct StreamInfo {
    int index = 0;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    Time duration = 0.0;
    int channels = 0;
    int sampleRate = 0;
    bool hasAlpha = false;
    std::string codec;
};

struct MediaProbe {
    bool ok = false;
    std::string error;
    ContainerKind container = ContainerKind::Unknown;
    Time duration = 0.0;
    Size size{0, 0};
    double fps = 0.0;
    bool hasAudio = false;
    bool hasVideo = false;
    int videoStreams = 0;
    int audioStreams = 0;
    std::vector<StreamInfo> streams;
    // Preenchido para sequencias de imagem, que nao tem um unico arquivo.
    int sequenceFrames = 0;
    std::string pattern;   // ex.: "frame_%04d.png"
};

// QuaisHW o backend conseguiu iniciar.
struct DecodeCapabilities {
    bool available = false;
    std::string decoder;        // "ffmpeg", "nvenc", "qsv", "d3d11va"
    bool hardwareDecode = false;
    bool hardwareEncode = false;
    std::vector<std::string> hardwareDecoders;
    std::vector<std::string> hardwareEncoders;
    std::string buildInfo;
};

class VideoSource {
public:
    virtual ~VideoSource() = default;

    // Abre o arquivo. Retorna false e preenche 'error'.
    virtual bool open(const std::string& path, std::string* error) = 0;
    virtual void close() = 0;

    [[nodiscard]] virtual const MediaProbe& probe() const = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;

    // Devolve o quadro mais proximo de 'time'. O ponteiro pertence ao source e
    // vale ate a proxima chamada. nullptr = ainda nao disponivel.
    virtual const VideoFrame* frameAt(Time time) = 0;

    // Avisa que o proximo acesso sera em 'time', para o decoder comecar a
    // trabalhar. E o que faz a timeline rolar sem engasgo.
    virtual void prefetch(Time time) { (void)time; }

    // Troca para o proxy. Nao recarrega se ja estiver no proxy.
    virtual void setUseProxy(bool on) { (void)on; }
    [[nodiscard]] virtual bool isUsingProxy() const { return false; }
};

class VideoSourcePool {
public:
    // Fontes sao compartilhadas: dois nos usando o mesmo arquivo nao devem
    // abrir dois decodificadores.
    virtual std::shared_ptr<VideoSource> acquire(const MediaItem& item) = 0;
    virtual void release(const std::shared_ptr<VideoSource>& source) = 0;
    virtual void clear() = 0;
    [[nodiscard]] virtual size_t openCount() const = 0;
};

class Encoder {
public:
    virtual ~Encoder() = default;

    struct Settings {
        std::string path;
        Size size{1920, 1080};
        double fps = 30.0;
        std::string container = "mp4";
        std::string videoCodec = "h264";
        std::string audioCodec = "aac";
        std::string preset = "medium";
        int videoBitrateKbps = 12000;
        int audioBitrateKbps = 192;
        bool withAudio = false;
        bool alphaChannel = false;
        bool hardware = true;    // tenta NVENC/QSV/AMF antes do software
    };

    virtual bool begin(const Settings& settings, std::string* error) = 0;
    // Aceita RGBA de 8 bits por canal (o compositor entrega float).
    virtual bool writeFrame(const uint8_t* rgba, int width, int height, int stride,
                            Time time) = 0;
    virtual bool finish(std::string* error) = 0;
    [[nodiscard]] virtual bool isHardware() const = 0;
};

class MediaBackend {
public:
    virtual ~MediaBackend() = default;

    [[nodiscard]] virtual const char* name() const = 0;
    [[nodiscard]] virtual DecodeCapabilities capabilities() const = 0;

    virtual MediaProbe probeFile(const std::string& path) const = 0;

    virtual std::shared_ptr<VideoSourcePool> createSourcePool() = 0;
    virtual std::unique_ptr<Encoder> createEncoder() = 0;

    // Gera um proxy leve (ProRes Proxy, DNxHR LB ou H.264) para editar em
    // maquina fraca. Devolve o caminho do arquivo gerado.
    virtual bool generateProxy(const MediaItem& item, const std::string& outputPath,
                               double scale, std::string* error) = 0;

    // Numero de threads de decodificacao. 0 = decidir pelo hardware.
    virtual void setDecodeThreads(int threads) = 0;
    [[nodiscard]] virtual int decodeThreads() const = 0;
};

// Devolve o backend disponivel. Com LUMINA_ENABLE_FFMPEG=OFF devolve um
// stub funcional que responde "nao ha midia" sem quebrar o editor.
MediaBackend& backend();

// Injeta um backend (usado em teste). Passar nullptr restaura o padrao.
void setBackend(MediaBackend* backend);

// Se a rota de proxy deve ser usada para este item.
bool shouldUseProxy(const MediaItem& item, bool globalProxyEnabled);

}  // namespace lmn::media
