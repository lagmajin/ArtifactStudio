module;
#include <QApplication>
#include <QFileDialog>
#include <QString>
#include <QThread>
#include <wobjectimpl.h>

module ArtifactPr.ExportDialog;

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Export Settings"));
    setMinimumWidth(400);
    setModal(true);

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(QStringLiteral("Output File:")));
    outputPathEdit_ = new QLineEdit(QStringLiteral("output.mp4"));
    auto* browseBtn = new QPushButton(QStringLiteral("Browse..."));
    connect(browseBtn, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
    auto* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(outputPathEdit_);
    pathLayout->addWidget(browseBtn);
    layout->addLayout(pathLayout);

    layout->addWidget(new QLabel(QStringLiteral("Resolution:")));
    resolutionCombo_ = new QComboBox();
    resolutionCombo_->addItems({QStringLiteral("1920x1080"), QStringLiteral("1280x720"), QStringLiteral("3840x2160"), QStringLiteral("Match Sequence")});
    layout->addWidget(resolutionCombo_);

    layout->addWidget(new QLabel(QStringLiteral("Codec:")));
    codecCombo_ = new QComboBox();
    codecCombo_->addItems({QStringLiteral("H.264 (MP4)"), QStringLiteral("H.265 (HEVC)"), QStringLiteral("ProRes"), QStringLiteral("DNxHD")});
    layout->addWidget(codecCombo_);

    layout->addWidget(new QLabel(QStringLiteral("Frame Rate:")));
    framerateCombo_ = new QComboBox();
    framerateCombo_->addItems({QStringLiteral("24 fps"), QStringLiteral("25 fps"), QStringLiteral("30 fps"), QStringLiteral("60 fps"), QStringLiteral("Match Sequence")});
    layout->addWidget(framerateCombo_);

    layout->addWidget(new QLabel(QStringLiteral("Quality:")));
    qualitySlider_ = new QSlider(Qt::Horizontal);
    qualitySlider_->setMinimum(1);
    qualitySlider_->setMaximum(100);
    qualitySlider_->setValue(80);
    layout->addWidget(qualitySlider_);

    progressLabel_ = new QLabel();
    progressBar_ = new QProgressBar();
    progressBar_->setVisible(false);
    layout->addWidget(progressLabel_);
    layout->addWidget(progressBar_);

    auto* buttonLayout = new QHBoxLayout();
    auto* exportBtn = new QPushButton(QStringLiteral("Export"));
    connect(exportBtn, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addStretch();
    buttonLayout->addWidget(exportBtn);
    buttonLayout->addWidget(cancelBtn);
    layout->addLayout(buttonLayout);
}

void ExportDialog::onBrowseClicked()
{
    QString file = QFileDialog::getSaveFileName(this, QStringLiteral("Save Export"),
        QStringLiteral("output.mp4"), QStringLiteral("MP4 Files (*.mp4);;All Files (*)"));
    if (!file.isEmpty()) {
        outputPathEdit_->setText(file);
    }
}

void ExportDialog::onExportClicked()
{
    progressBar_->setVisible(true);
    progressBar_->setValue(0);
    progressLabel_->setText(QStringLiteral("Exporting..."));

    for (int i = 0; i <= 100; i += 10) {
        progressBar_->setValue(i);
        QApplication::processEvents();
        QThread::msleep(100);
    }

    progressLabel_->setText(QStringLiteral("Export complete!"));
    accept();
}

W_OBJECT_IMPL(ExportDialog)
