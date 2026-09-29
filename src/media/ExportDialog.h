// ExportDialog.h - Dialogo de exportacao.
//
// Mostra o que vai sair, o tempo estimado e o progresso de verdade. Um
// exportador sem progresso faz o usuario achar que travou e clicar em cancelar.
#pragma once

#include <QDialog>

#include "FrameExporter.h"
#include "core/Project.h"
#include "ui/PlaybackController.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSpinBox;

namespace lmn::media {

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    ExportDialog(Project& project, PlaybackController& playback,
                 QWidget* parent = nullptr);
    ~ExportDialog() override;

    // Usado pela janela principal para mostrar o desfecho.
    [[nodiscard]] QString resultSummary() const { return m_summary; }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onBrowse();
    void onSettingsChanged();
    void onStart();
    void onCancel();
    void onProgressTick();
    void onFinished();

private:
    [[nodiscard]] TimeRange chosenRange() const;
    void updateEstimate();

    Project& m_project;
    PlaybackController& m_playback;
    FrameExporter::Job m_job;

    QLineEdit* m_path = nullptr;
    QComboBox* m_format = nullptr;
    QComboBox* m_composition = nullptr;
    QComboBox* m_range = nullptr;
    QSpinBox* m_quality = nullptr;
    QCheckBox* m_withAudio = nullptr;
    QCheckBox* m_alpha = nullptr;
    QCheckBox* m_hardware = nullptr;
    QSlider* m_bitrate = nullptr;
    QLabel* m_estimate = nullptr;
    QLabel* m_status = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_cancel = nullptr;

    class QTimer* m_timer = nullptr;
    QString m_summary;
};

}  // namespace lmn::media
