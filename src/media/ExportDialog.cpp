#include "ExportDialog.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

#include "io/ProjectSerializer.h"

namespace lmn::media {

ExportDialog::ExportDialog(Project& project, PlaybackController& playback, QWidget* parent)
    : QDialog(parent), m_project(project), m_playback(playback) {
    setWindowTitle(QStringLiteral("Exportar"));
    setModal(true);
    resize(480, 420);

    auto* outer = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    outer->addLayout(form);

    // --- Destino ---
    auto* pathRow = new QWidget();
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    m_path = new QLineEdit(pathRow);
    m_path->setText(
        io::ProjectSerializer::exportDirectoryFor(
            QString::fromStdString(m_project.filePath())) +
        QStringLiteral("/saida.mp4"));
    pathLayout->addWidget(m_path, 1);
    auto* browse = new QPushButton(QStringLiteral("..."), pathRow);
    connect(browse, &QPushButton::clicked, this, &ExportDialog::onBrowse);
    pathLayout->addWidget(browse);
    form->addRow(QStringLiteral("Arquivo"), pathRow);

    // --- Formato ---
    m_format = new QComboBox();
    m_format->addItems({QStringLiteral("Video (mp4)"), QStringLiteral("Video (matroska)"),
                        QStringLiteral("Imagem PNG")});
    form->addRow(QStringLiteral("Formato"), m_format);

    m_composition = new QComboBox();
    for (const auto& [id, comp] : m_project.compositions()) {
        m_composition->addItem(QString::fromStdString(comp.name()),
                               static_cast<uint32_t>(id));
    }
    if (m_project.compositionCount() > 0) {
        m_composition->setCurrentIndex(0);
    }
    form->addRow(QStringLiteral("Composicao"), m_composition);

    m_range = new QComboBox();
    m_range->addItems({QStringLiteral("Toda a area de trabalho"),
                       QStringLiteral("Somente o quadro atual")});
    form->addRow(QStringLiteral("Intervalo"), m_range);

    // --- Qualidade ---
    m_quality = new QSpinBox();
    m_quality->setRange(0, 51);
    m_quality->setValue(20);
    m_quality->setToolTip(QStringLiteral("CRF: menor = melhor. 18-23 e a faixa util."));
    form->addRow(QStringLiteral("CRF"), m_quality);

    m_bitrate = new QSlider(Qt::Horizontal);
    m_bitrate->setRange(1, 100);
    m_bitrate->setValue(40);
    form->addRow(QStringLiteral("Taxa de bits"), m_bitrate);

    m_withAudio = new QCheckBox(QStringLiteral("Incluir audio"));
    m_withAudio->setChecked(true);
    form->addRow(QString(), m_withAudio);

    m_alpha = new QCheckBox(QStringLiteral("Canal alpha"));
    m_alpha->setToolTip(QStringLiteral("Gera arquivo com transparencia. Exige codec "
                                       "que suporte, como ProRes 4444 ou VP9."));
    form->addRow(QString(), m_alpha);

    m_hardware = new QCheckBox(QStringLiteral("Usar aceleracao de hardware"));
    m_hardware->setChecked(true);
    m_hardware->setToolTip(QStringLiteral("NVENC, QSV ou AMF, se o hardware permitir. "
                                         "Desligue para comparar qualidade."));
    form->addRow(QString(), m_hardware);

    form->addRow(new QLabel(QStringLiteral("Resolucao"), this),
                 new QLabel(QStringLiteral("%1 x %2  a  %3 fps")
                                .arg(m_project.size().width)
                                .arg(m_project.size().height)
                                .arg(m_project.fps(), 0, 'g', 3),
                            this));

    // --- Estimativa e progresso ---
    m_estimate = new QLabel(this);
    m_estimate->setWordWrap(true);
    m_estimate->setObjectName(QStringLiteral("hint"));
    outer->addWidget(m_estimate);

    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    outer->addWidget(m_progress);

    m_status = new QLabel(this);
    outer->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(this);
    m_start = buttons->addButton(QStringLiteral("Exportar"),
                                 QDialogButtonBox::AcceptRole);
    m_cancel = buttons->addButton(QStringLiteral("Fechar"),
                                  QDialogButtonBox::RejectRole);
    outer->addWidget(buttons);

    m_timer = new QTimer(this);
    m_timer->setInterval(120);

    connect(m_start, &QPushButton::clicked, this, &ExportDialog::onStart);
    connect(m_cancel, &QPushButton::clicked, this, &ExportDialog::onCancel);
    connect(m_timer, &QTimer::timeout, this, &ExportDialog::onProgressTick);

    // QWidget::changed nao existe no Qt. Cada controle tem o sinal proprio, e
    // QWidget::changed compilaria em nada.
    connect(m_format, &QComboBox::currentIndexChanged, this,
            &ExportDialog::onSettingsChanged);
    connect(m_range, &QComboBox::currentIndexChanged, this,
            &ExportDialog::onSettingsChanged);
    connect(m_bitrate, &QSlider::valueChanged, this, &ExportDialog::onSettingsChanged);
    connect(m_alpha, &QCheckBox::toggled, this, &ExportDialog::onSettingsChanged);
    connect(m_withAudio, &QCheckBox::toggled, this, &ExportDialog::onSettingsChanged);
    connect(m_composition, &QComboBox::currentIndexChanged, this,
            &ExportDialog::onSettingsChanged);

    updateEstimate();
}

ExportDialog::~ExportDialog() = default;

TimeRange ExportDialog::chosenRange() const {
    if (m_range->currentIndex() == 1) {
        const Time now = m_playback.currentTime();
        return TimeRange{now, now + 1.0 / std::max(1.0, m_project.fps())};
    }
    return m_playback.workArea();
}

void ExportDialog::updateEstimate() {
    const CompId comp =
        static_cast<CompId>(m_composition->currentData().toUInt());
    const Composition* c = m_project.composition(comp);
    if (!c) return;

    const TimeRange range = chosenRange();
    const int frames = std::max(1, static_cast<int>(std::ceil(range.duration() * c->fps())));

    // Estimativa honesta: mede um frame e multiplica. Sem amostra, o numero
    // seria uma invencao, e o usuario confiaria nele.
    m_estimate->setText(QStringLiteral("%1 quadros de %2 x %3\nAtualize o tempo real "
                                       "depois de exportar uma vez.")
                            .arg(frames)
                            .arg(c->size().width)
                            .arg(c->size().height));
}

void ExportDialog::onSettingsChanged() {
    updateEstimate();
}

void ExportDialog::onBrowse() {
    const bool isImage = m_format->currentIndex() == 2;
    const QString filter = isImage ? QStringLiteral("Video (*.mp4 *.mov *.mkv *.avi)")
                                   : QStringLiteral("Video (*.mp4 *.mov *.mkv *.avi);;Imagem (*.png)");
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Salvar como"), m_path->text(), filter);
    if (!path.isEmpty()) m_path->setText(path);
}

void ExportDialog::onStart() {
    if (m_job.isRunning()) return;

    const CompId comp = static_cast<CompId>(m_composition->currentData().toUInt());
    const TimeRange range = chosenRange();
    const QString path = m_path->text();
    if (path.isEmpty()) {
        m_status->setText(QStringLiteral("Escolha um arquivo de destino"));
        return;
    }

    m_progress->setVisible(true);
    m_progress->setRange(0, 0);   // indeterminado ate o total ser conhecido
    m_start->setEnabled(false);
    m_summary.clear();

    if (m_format->currentIndex() == 2) {
        const QString dir = QFileInfo(path).absolutePath();
        m_job.startSequence(m_project, comp, range, dir.toStdString(), "frame");
    } else {
        Encoder::Settings settings;
        settings.path = path.toStdString();
        settings.size = m_project.output().size;
        settings.fps = m_project.fps();
        settings.container = m_format->currentIndex() == 1 ? "matroska" : "mp4";
        settings.videoCodec = "h264";
        settings.withAudio = m_withAudio->isChecked();
        settings.alphaChannel = m_alpha->isChecked();
        settings.hardware = m_hardware->isChecked();
        settings.videoBitrateKbps =
            static_cast<int>(500 + m_bitrate->value() / 100.0 * 60000);
        m_job.startMovie(m_project, comp, range, path.toStdString(), settings);
    }
    m_timer->start();
}

void ExportDialog::onProgressTick() {
    m_status->setText(QString::fromStdString(m_job.progressText()));

    const int total = m_job.frameCount();
    if (total > 0) {
        m_progress->setRange(0, total);
        m_progress->setValue(m_job.frameIndex());
    }

    if (m_job.finished()) {
        m_timer->stop();
        onFinished();
    }
}

void ExportDialog::onFinished() {
    m_progress->setVisible(false);
    m_start->setEnabled(true);

    const ExportResult& r = m_job.result();
    m_summary = QString::fromStdString(r.summary());
    if (r.ok) {
        m_status->setText(QStringLiteral("Concluido: %1").arg(m_summary));
    } else {
        m_status->setText(QStringLiteral("Erro: %1")
                              .arg(QString::fromStdString(r.error)));
    }
}

void ExportDialog::onCancel() {
    if (m_job.isRunning()) {
        // O primeiro cancelamento pede parada; o segundo fecha. Sem isso,
        // um usuario impaciente fecha a janela e perde o arquivo em Encode.
        m_job.cancel();
        m_status->setText(QStringLiteral("Cancelando..."));
        m_timer->start();
        return;
    }
    reject();
}

void ExportDialog::closeEvent(QCloseEvent* event) {
    if (m_job.isRunning()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Exportar"),
            QStringLiteral("A exportacao ainda esta em andamento. Fechar mesmo assim?"),
            QMessageBox::Yes | QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_job.cancel();
        m_job.wait();
    }
    event->accept();
}

}  // namespace lmn::media
