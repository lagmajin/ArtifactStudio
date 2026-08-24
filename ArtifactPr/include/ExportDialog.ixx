module;
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QSlider>
#include <QThread>
#include <QVBoxLayout>
#include <wobjectdefs.h>

export module ArtifactPr.ExportDialog;

import ArtifactPr.EditorEngine;
import ArtifactPr.SequenceExporter;

export class ExportDialog : public QDialog
{
    W_OBJECT(ExportDialog)
public:
    ExportDialog(QWidget* parent = nullptr);

private Q_SLOTS:
    void onBrowseClicked();
    void onExportClicked();

private:
    void onExportFinished(bool success, const QString& message);
    ArtifactPr::ExportFormat::Value selectedFormat() const;
    ArtifactPr::ExportSettings collectSettings(QString* errorMessage) const;

    QLineEdit* outputPathEdit_ = nullptr;
    QComboBox* resolutionCombo_ = nullptr;
    QComboBox* codecCombo_ = nullptr;
    QComboBox* framerateCombo_ = nullptr;
    QSlider* qualitySlider_ = nullptr;
    QLabel* progressLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
};
