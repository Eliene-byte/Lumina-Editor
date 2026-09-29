#include "ProjectSerializer.h"

#include <algorithm>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

#include "JsonHelpers.h"
#include "core/NodeRegistry.h"

namespace lmn::io {
namespace {

// --- composicoes -----------------------------------------------------------

QJsonObject compToJson(const Composition& c) {
    QJsonObject o;
    o["id"] = static_cast<qint64>(c.id());
    o["nm"] = QString::fromStdString(c.name());
    o["w"] = c.size().width;
    o["h"] = c.size().height;
    o["fps"] = c.fps();
    o["in"] = c.duration().in;
    o["out"] = c.duration().out;
    o["wa"] = QJsonArray{c.workArea().in, c.workArea().out};
    if (c.transparentBackground()) o["tr"] = true;
    if (c.background() != Color{0.0, 0.0, 0.0, 1.0}) {
        const Color bg = c.background();
        o["bg"] = QJsonArray{bg.r, bg.g, bg.b, bg.a};
    }

    QJsonArray nodes;
    for (NodeId id : c.graph().nodeIds()) {
        if (const Node* n = c.graph().node(id)) nodes.append(nodeToJson(*n));
    }
    o["nd"] = nodes;
    if (c.graph().output() != kInvalidId) {
        o["out_node"] = static_cast<qint64>(c.graph().output());
    }
    return o;
}

void compFromJson(const QJsonObject& o, Composition& c) {
    if (o.contains("nm")) c.setName(o["nm"].toString().toStdString());
    c.setSize(Size{o["w"].toInt(1920), o["h"].toInt(1080)});
    c.setFps(o["fps"].toDouble(30.0));
    c.setDuration(TimeRange{o["in"].toDouble(0.0), o["out"].toDouble(5.0)});
    if (const QJsonArray wa = o["wa"].toArray(); wa.size() == 2) {
        c.setWorkArea(TimeRange{wa[0].toDouble(), wa[1].toDouble()});
    }
    c.setTransparentBackground(o["tr"].toBool());
    if (const QJsonArray bg = o["bg"].toArray(); bg.size() == 4) {
        c.setBackground(Color{bg[0].toDouble(), bg[1].toDouble(), bg[2].toDouble(),
                              bg[3].toDouble()});
    }

    c.graph().clear();
    if (const QJsonArray nodes = o["nd"].toArray()) {
        for (const auto& n : nodes) {
            const QJsonObject no = n.toObject();
            const NodeId id = static_cast<NodeId>(no["id"].toInt());
            const std::string type = no["ty"].toString().toStdString();
            if (id == kInvalidId) continue;

            // Instancia pelo registro para receber as propriedades padrao, e
            // depois sobrescreve com o que veio do arquivo.
            Node node = NodeRegistry::instance().create(type, id);
            nodeFromJson(no, node);
            c.graph().addNode(std::move(node));
        }
    }
    c.graph().setOutput(o.contains("out_node")
                            ? static_cast<NodeId>(o["out_node"].toInt())
                            : kInvalidId);
}

// --- timeline --------------------------------------------------------------

QJsonObject clipToJson(const Clip& clip) {
    QJsonObject o;
    o["id"] = static_cast<qint64>(clip.id);
    o["comp"] = static_cast<qint64>(clip.comp);
    if (!clip.name.empty()) o["nm"] = QString::fromStdString(clip.name);
    if (clip.media != 0) o["md"] = static_cast<qint64>(clip.media);
    o["st"] = clip.start;
    o["si"] = clip.sourceIn;
    o["so"] = clip.sourceOut;
    if (clip.speed != 1.0) o["sp"] = clip.speed;
    if (!clip.enabled) o["en"] = false;
    if (clip.locked) o["lk"] = true;
    if (clip.volume != 1.0) o["vol"] = clip.volume;

    auto transToJson = [](const Transition& tr) {
        QJsonObject t;
        t["ty"] = QString::fromLatin1(transitionTypeName(tr.type));
        t["d"] = tr.duration;
        if (!tr.centered) t["c"] = false;
        if (tr.amount != 1.0) t["am"] = tr.amount;
        return t;
    };
    if (clip.hasTransitionIn) o["ti"] = transToJson(clip.transitionIn);
    if (clip.hasTransitionOut) o["to"] = transToJson(clip.transitionOut);
    return o;
}

Clip clipFromJson(const QJsonObject& o) {
    Clip c;
    c.id = static_cast<ClipId>(o["id"].toInt());
    c.comp = static_cast<CompId>(o["comp"].toInt());
    c.name = o["nm"].toString().toStdString();
    c.media = static_cast<MediaId>(o["md"].toInt());
    c.start = o["st"].toDouble(0.0);
    c.sourceIn = o["si"].toDouble(0.0);
    c.sourceOut = o["so"].toDouble(c.sourceIn + 1.0);
    c.speed = o["sp"].toDouble(1.0);
    c.enabled = o.contains("en") ? o["en"].toBool() : true;
    c.locked = o["lk"].toBool();
    c.volume = o["vol"].toDouble(1.0);

    auto transFromJson = [](const QJsonObject& t, Transition& tr) {
        tr.type = transitionTypeFromName(t["ty"].toString().toStdString());
        tr.duration = t["d"].toDouble(0.5);
        tr.centered = t.contains("c") ? t["c"].toBool() : true;
        tr.amount = t["am"].toDouble(1.0);
    };
    if (o.contains("ti")) {
        transFromJson(o["ti"].toObject(), c.transitionIn);
        c.hasTransitionIn = true;
    }
    if (o.contains("to")) {
        transFromJson(o["to"].toObject(), c.transitionOut);
        c.hasTransitionOut = true;
    }
    return c;
}

QJsonObject timelineToJson(const Timeline& tl) {
    QJsonObject o;
    o["in"] = tl.range().in;
    o["out"] = tl.range().out;

    QJsonArray stacks;
    for (const auto& s : tl.stacks()) {
        QJsonObject so;
        so["id"] = static_cast<qint64>(s.id());
        so["nm"] = QString::fromStdString(s.name());
        so["ix"] = s.index();
        if (s.muted()) so["mu"] = true;
        if (s.solo()) so["so"] = true;
        if (s.locked()) so["lk"] = true;
        if (!s.visible()) so["vs"] = false;
        if (s.opacity() != 1.0) so["op"] = s.opacity();

        QJsonArray clips;
        for (const auto& c : s.clips()) clips.append(clipToJson(c));
        so["cl"] = clips;
        stacks.append(so);
    }
    o["st"] = stacks;
    return o;
}

void timelineFromJson(const QJsonObject& o, Timeline& tl) {
    tl = Timeline{};
    tl.setRange(TimeRange{o["in"].toDouble(0.0), o["out"].toDouble(10.0)});

    for (const auto& s : o["st"].toArray()) {
        const QJsonObject so = s.toObject();
        Stack* stack = tl.addStack(so["nm"].toString().toStdString());
        stack->setIndex(so["ix"].toInt(stack->index()));
        stack->setMuted(so["mu"].toBool());
        stack->setSolo(so["so"].toBool());
        stack->setLocked(so["lk"].toBool());
        stack->setVisible(so.contains("vs") ? so["vs"].toBool() : true);
        stack->setOpacity(so["op"].toDouble(1.0));

        for (const auto& c : so["cl"].toArray()) {
            stack->clips().push_back(clipFromJson(c.toObject()));
        }
        // Ordena por posicao: o arquivo pode ter os clips fora de ordem se
        // foram movidos por script.
        std::stable_sort(stack->clips().begin(), stack->clips().end(),
                         [](const Clip& a, const Clip& b) { return a.start < b.start; });
    }
}

// --- acervo ----------------------------------------------------------------

QJsonObject mediaToJson(const MediaItem& m) {
    QJsonObject o;
    o["id"] = static_cast<qint64>(m.id);
    o["ty"] = QString::fromLatin1(mediaTypeName(m.type));
    o["path"] = QString::fromStdString(m.path);
    if (!m.name.empty()) o["nm"] = QString::fromStdString(m.name);
    if (m.duration > 0.0) o["du"] = m.duration;
    if (m.size.width > 0) o["w"] = m.size.width;
    if (m.size.height > 0) o["h"] = m.size.height;
    if (m.fps > 0.0) o["fps"] = m.fps;
    if (m.channels > 0) o["ch"] = m.channels;
    if (m.sampleRate > 0) o["sr"] = m.sampleRate;
    if (m.rotation != 0.0) o["rot"] = m.rotation;
    if (m.hasAlpha) o["al"] = true;
    if (m.isOffline) o["off"] = true;
    if (!m.proxyPath.empty()) o["pp"] = QString::fromStdString(m.proxyPath);
    if (m.proxyScale != 0.25) o["ps"] = m.proxyScale;
    if (m.useProxy) o["up"] = true;
    return o;
}

MediaItem mediaFromJson(const QJsonObject& o) {
    MediaItem m;
    m.id = static_cast<MediaId>(o["id"].toInt());
    m.type = mediaTypeFromName(o["ty"].toString().toStdString());
    m.path = o["path"].toString().toStdString();
    m.name = o["nm"].toString().toStdString();
    m.duration = o["du"].toDouble(0.0);
    m.size = Size{o["w"].toInt(0), o["h"].toInt(0)};
    m.fps = o["fps"].toDouble(0.0);
    m.channels = o["ch"].toInt(0);
    m.sampleRate = o["sr"].toInt(0);
    m.rotation = o["rot"].toDouble(0.0);
    m.hasAlpha = o["al"].toBool();
    m.isOffline = o["off"].toBool();
    m.proxyPath = o["pp"].toString().toStdString();
    m.proxyScale = o["ps"].toDouble(0.25);
    m.useProxy = o.contains("up") ? o["up"].toBool() : true;
    return m;
}

}  // namespace

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

std::string ProjectSerializer::toJson(const Project& project) {
    QJsonObject root;
    root["lumina"] = Project::kFileVersion;
    root["app"] = QStringLiteral("Lumina");
    root["name"] = QString::fromStdString(project.name());
    root["w"] = project.size().width;
    root["h"] = project.size().height;
    root["fps"] = project.fps();

    // Gerenciament de cor
    QJsonObject cm;
    cm["work"] = static_cast<int>(project.color().working);
    cm["disp"] = static_cast<int>(project.color().display);
    cm["exp"] = project.color().exposure;
    cm["gam"] = project.color().gamma;
    root["cm"] = cm;

    // Saida
    QJsonObject out;
    const OutputSettings& os = project.output();
    out["w"] = os.size.width;
    out["h"] = os.size.height;
    out["fps"] = os.fps;
    out["cont"] = QString::fromStdString(os.container);
    out["vc"] = QString::fromStdString(os.videoCodec);
    out["ac"] = QString::fromStdString(os.audioCodec);
    out["pres"] = QString::fromStdString(os.preset);
    out["br"] = os.videoBitrateKbps;
    out["abr"] = os.audioBitrateKbps;
    out["au"] = os.withAudio;
    out["al"] = os.alphaChannel;
    out["px"] = os.useProxy;
    if (!os.rangeIsAuto()) out["rg"] = QJsonArray{os.range().in, os.range().out};
    root["out"] = out;

    // Composicoes
    QJsonArray comps;
    for (const auto& [id, c] : project.compositions()) comps.append(compToJson(c));
    root["comps"] = comps;

    // Timeline
    root["tl"] = timelineToJson(project.timeline());

    // Acervo
    QJsonArray media;
    for (const auto& m : project.media().items()) media.append(mediaToJson(m));
    root["md"] = media;

    // Compacto: um projeto de edicao tem muito numero repetido e nenhuma
    // legibilidade a ganhar com indentacao.
    return QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
}

bool ProjectSerializer::fromJson(const std::string& json, Project& project,
                                  std::string* error) {
    const auto fail = [error](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(json), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail("JSON invalido na linha " + std::to_string(parseError.offset) + ": " +
                    parseError.errorString().toStdString());
    }
    if (!doc.isObject()) return fail("a raiz do projeto nao e um objeto");

    const QJsonObject root = doc.object();
    if (!root.contains("lumina")) {
        return fail("isto nao e um projeto Lumina (falta a chave 'lumina')");
    }
    const int version = root["lumina"].toInt(0);
    if (version > Project::kFileVersion) {
        return fail("projeto salvo por uma versao mais nova do Lumina (formato " +
                    std::to_string(version) + ", esta versao le " +
                    std::to_string(Project::kFileVersion) + ")");
    }

    project.clear();

    if (root.contains("name")) project.setName(root["name"].toString().toStdString());
    if (root.contains("w")) {
        project.setSize(Size{root["w"].toInt(1920), root["h"].toInt(1080)});
    }
    if (root.contains("fps")) project.setFps(root["fps"].toDouble(30.0));

    if (const QJsonObject cm = root["cm"].toObject(); !cm.isEmpty()) {
        auto& c = project.color();
        c.working = static_cast<WorkingSpace>(cm["work"].toInt(0));
        c.display = static_cast<DisplayTransform>(cm["disp"].toInt(1));
        c.exposure = cm["exp"].toDouble(0.0);
        c.gamma = cm["gam"].toDouble(1.0);
    }
    if (const QJsonObject o = root["out"].toObject(); !o.isEmpty()) {
        auto& os = project.output();
        os.size = Size{o["w"].toInt(1920), o["h"].toInt(1080)};
        os.fps = o["fps"].toDouble(30.0);
        os.container = o["cont"].toString().toStdString();
        os.videoCodec = o["vc"].toString().toStdString();
        os.audioCodec = o["ac"].toString().toStdString();
        os.preset = o["pres"].toString().toStdString();
        os.videoBitrateKbps = o["br"].toInt(12000);
        os.audioBitrateKbps = o["abr"].toInt(192);
        os.withAudio = o["au"].toBool();
        os.alphaChannel = o["al"].toBool();
        os.useProxy = o["px"].toBool();
        if (const QJsonArray r = o["rg"].toArray(); r.size() == 2) {
            os.rangeOverride = TimeRange{r[0].toDouble(), r[1].toDouble()};
        }
    }

    // As composicoes precisam existir antes da timeline, porque os clips
    // referenciam composicoes por id.
    for (const auto& c : root["comps"].toArray()) {
        const QJsonObject co = c.toObject();
        const CompId id = static_cast<CompId>(co["id"].toInt());
        Composition comp(id, co["nm"].toString().toStdString(),
                         Size{co["w"].toInt(1920), co["h"].toInt(1080)},
                         co["fps"].toDouble(30.0), 5.0);
        compFromJson(co, comp);
        project.compositions().emplace(id, std::move(comp));
    }
    if (project.compositions().empty()) {
        // Projeto sem nenhuma composicao: cria uma para o editor nao abrir
        // com area de trabalho vazia.
        project.createDefaultComposition();
    }

    if (root.contains("tl")) timelineFromJson(root["tl"].toObject(), project.timeline());
    for (const auto& m : root["md"].toArray()) {
        project.media().add(mediaFromJson(m.toObject()));
    }

    project.markClean();
    return true;
}

SaveResult ProjectSerializer::save(const Project& project, const std::string& path) {
    QElapsedTimer timer;
    timer.start();

    SaveResult result;
    const std::string json = toJson(project);
    result.bytes = json.size();

    // QSaveFile escreve num temporario e so substitui o original no fim: uma
    // queda de energia no meio da escrita nao destroi o projeto.
    QSaveFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.error = "nao foi possivel abrir para escrita: " +
                       file.errorString().toStdString();
        return result;
    }
    const qint64 written = file.write(json.data(), static_cast<qint64>(json.size()));
    if (written != static_cast<qint64>(json.size())) {
        result.error = "escrita incompleta: " + file.errorString().toStdString();
        return result;
    }
    if (!file.commit()) {
        result.error = "falha ao finalizar a escrita: " + file.errorString().toStdString();
        return result;
    }

    result.ok = true;
    result.milliseconds = timer.nsecsElapsed() / 1e6;
    return result;
}

LoadResult ProjectSerializer::load(const std::string& path, Project& project) {
    QElapsedTimer timer;
    timer.start();

    LoadResult result;

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = "nao foi possivel ler o arquivo: " +
                       file.errorString().toStdString();
        return result;
    }
    const QByteArray data = file.readAll();
    file.close();

    std::string error;
    if (!fromJson(std::string(data.constData(), static_cast<size_t>(data.size())),
                  project, &error)) {
        result.error = std::move(error);
        return result;
    }

    // Verificacao de integridade: conexoes apontando para nos inexistentes
    // indicam arquivo editado a mao ou gravado por uma versao com bug.
    int missing = 0;
    for (const auto& [id, comp] : project.compositions()) {
        for (NodeId nid : comp.graph().nodeIds()) {
            const Node* n = comp.graph().node(nid);
            if (!n) continue;
            for (int s = 0; s < n->inputCount(); ++s) {
                const NodeId from = n->input(s);
                if (from != kInvalidId && !comp.graph().contains(from)) ++missing;
            }
        }
    }
    result.missingReferences = missing;

    result.ok = true;
    result.version = 1;
    result.milliseconds = timer.nsecsElapsed() / 1e6;
    return result;
}

QString ProjectSerializer::fileDialogFilter() {
    return QStringLiteral("Projetos Lumina (*.lumina);;Todos os arquivos (*)");
}

QString ProjectSerializer::autosavePathFor(const QString& projectPath) {
    if (projectPath.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        return QDir(dir).filePath(QStringLiteral("autosave.lumina"));
    }
    QDir dir = QFileInfo(projectPath).absoluteDir();
    dir.mkdir(QStringLiteral(".lumina"));
    return dir.filePath(QStringLiteral(".lumina/autosave.lumina"));
}

QString ProjectSerializer::exportDirectoryFor(const QString& projectPath) {
    if (projectPath.isEmpty()) return QDir::homePath();
    return QFileInfo(projectPath).absolutePath();
}

QString ProjectSerializer::proxyDirectoryFor(const QString& projectPath) {
    QDir base = projectPath.isEmpty()
                    ? QDir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation))
                    : QFileInfo(projectPath).absoluteDir();
    base.mkdir(QStringLiteral("proxies"));
    return base.filePath(QStringLiteral("proxies"));
}

}  // namespace lmn::io
