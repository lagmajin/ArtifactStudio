module;
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QSlider>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdint>
#include <wobjectimpl.h>

module ArtifactPr.ExportDialog;

import ArtifactPr.EditorEngine;
import ArtifactPr.SequenceExporter;
import ArtifactPr.Translation;

using ArtifactPr::trText;

namespace {

int parseFrameRate(const QString& text)
{
    // "30 fps" / "29.97 fps" → fps 値
    const QStringList parts = text.split(QChar(' '));
    bool ok = false;
    const int fps = qRound(parts.value(0).toDouble(&ok));
    if (ok && fps > 0) {
        return fps;
    }
    return 30;
}

bool parseResolutionParts(const QString& resolution, int* width, int* height)
{
    const QStringList parts = resolution.split(QChar('x'));
    bool okW = false;
    bool okH = false;
    const int w = parts.value(0).toInt(&okW);
    const int h = parts.value(1).toInt(&okH);
    if (okW && okH && w > 0 && h > 0) {
        *width = w;
        *height = h;
        return true;
    }
    return false;
}

} // namespace

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(trText("pr.export.title", "Export Settings"));
    setMinimumWidth(400);
    setModal(true);

    auto* engine = ArtifactPr::EditorEngine::instance();
    const ArtifactPr::DemoSequence sequence = engine->currentSequence();

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(trText("pr.export.output_path", "Output File:")));
    outputPathEdit_ = new QLineEdit(QStringLiteral("output.mp4"));
    auto* browseBtn = new QPushButton(trText("pr.export.browse", "Browse..."));
    connect(browseBtn, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
    auto* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(outputPathEdit_);
    pathLayout->addWidget(browseBtn);
    layout->addLayout(pathLayout);

    layout->addWidget(new QLabel(trText("pr.export.resolution", "Resolution:")));
    resolutionCombo_ = new QComboBox();
    // 表示文言は翻訳されるため、値の判定は userData 側で行う。
    resolutionCombo_->addItem(trText("pr.export.resolution_match", "Match Sequence"),
                             QStringLiteral("match"));
    resolutionCombo_->addItem(QStringLiteral("1920x1080"), QStringLiteral("1920x1080"));
    resolutionCombo_->addItem(QStringLiteral("1280x720"), QStringLiteral("1280x720"));
    resolutionCombo_->addItem(QStringLiteral("3840x2160"), QStringLiteral("3840x2160"));
    resolutionCombo_->setCurrentIndex(0);
    layout->addWidget(resolutionCombo_);

    layout->addWidget(new QLabel(trText("pr.export.format", "Format:")));
    codecCombo_ = new QComboBox();
    // 表示文言は翻訳されるため、値の判定は userData 側で行う。
    codecCombo_->addItem(trText("pr.export.codec_h264", "H.264 (MP4)"),
                          QStringLiteral("h264"));
    codecCombo_->addItem(trText("pr.export.codec_h265", "H.265 (HEVC)"),
                          QStringLiteral("h265"));
    codecCombo_->addItem(trText("pr.export.codec_prores", "ProRes"),
                          QStringLiteral("prores"));
    codecCombo_->addItem(trText("pr.export.codec_dnxhd", "DNxHD"),
                          QStringLiteral("dnxhd"));
    codecCombo_->addItem(trText("pr.export.codec_png", "PNG Sequence"),
                          QStringLiteral("png"));
    codecCombo_->addItem(trText("pr.export.codec_jpeg", "JPEG Sequence"),
                          QStringLiteral("jpeg"));
    codecCombo_->addItem(trText("pr.export.codec_wav", "Audio Only (WAV)"),
                          QStringLiteral("wav"));
    codecCombo_->addItem(trText("pr.export.codec_mp3", "Audio Only (MP3)"),
                          QStringLiteral("mp3"));

    // format 変更時に拡張子と音声 checkbox の有効状態を更新
    connect(codecCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() {
        QString path = outputPathEdit_->text();
        const auto replaceExtension = [&path](const QString& extension) {
            const int slash = qMax(path.lastIndexOf(QChar('/')), path.lastIndexOf(QChar('\\')));
            const int dot = path.lastIndexOf(QChar('.'));
            const int extensionStart = dot > slash ? dot : path.size();
            return path.left(extensionStart) + extension;
        };
        // 表示文字列ではなく enum で分岐する。表示文言は翻訳されるため、
        // 文字列比較では翻訳時に破綻する。
        QString extension = QStringLiteral(".mp4");
        switch (selectedFormat()) {
        case ArtifactPr::ExportFormat::Value::PngSequence:
            extension = QStringLiteral(".png");
            break;
        case ArtifactPr::ExportFormat::Value::JpegSequence:
            extension = QStringLiteral(".jpg");
            break;
        case ArtifactPr::ExportFormat::Value::ProResMov:
        case ArtifactPr::ExportFormat::Value::DnxhdMov:
            extension = QStringLiteral(".mov");
            break;
        case ArtifactPr::ExportFormat::Value::WavAudio:
            extension = QStringLiteral(".wav");
            break;
        case ArtifactPr::ExportFormat::Value::Mp3Audio:
            extension = QStringLiteral(".mp3");
            break;
        default:
            break;
        }
        outputPathEdit_->setText(replaceExtension(extension));
        // 音声 checkbox は動画形式のみ有効
        const ArtifactPr::ExportFormat format{selectedFormat()};
        includeAudioCheck_->setEnabled(format.isVideoFile());
    });
    layout->addWidget(codecCombo_);

    layout->addWidget(new QLabel(trText("pr.export.frame_rate", "Frame Rate:")));
    framerateCombo_ = new QComboBox();
    // 表示文言は翻訳されるため、値の判定は userData 側で行う。
    framerateCombo_->addItem(trText("pr.export.frame_rate_match", "Match Sequence"),
                            QStringLiteral("match"));
    const QString fpsFormat = trText("pr.export.frame_rate_fps", "%1 fps");
    framerateCombo_->addItem(fpsFormat.arg(24), QStringLiteral("24"));
    framerateCombo_->addItem(fpsFormat.arg(25), QStringLiteral("25"));
    framerateCombo_->addItem(fpsFormat.arg(30), QStringLiteral("30"));
    framerateCombo_->addItem(fpsFormat.arg(60), QStringLiteral("60"));
    framerateCombo_->setCurrentIndex(0);
    layout->addWidget(framerateCombo_);

    layout->addWidget(new QLabel(trText("pr.export.quality", "Quality:")));
    qualitySlider_ = new QSlider(Qt::Horizontal);
    qualitySlider_->setMinimum(1);
    qualitySlider_->setMaximum(100);
    qualitySlider_->setValue(80);
    layout->addWidget(qualitySlider_);

    includeAudioCheck_ = new QCheckBox(
        trText("pr.export.include_audio", "Include Audio (mixed tracks)"));
    includeAudioCheck_->setChecked(true);
    layout->addWidget(includeAudioCheck_);

    progressLabel_ = new QLabel();
    progressBar_ = new QProgressBar();
    progressBar_->setVisible(false);
    layout->addWidget(progressLabel_);
    layout->addWidget(progressBar_);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    exportButton_ = new QPushButton(trText("pr.export.action", "Export"));
    connect(exportButton_, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    cancelButton_ = new QPushButton(trText("pr.export.cancel", "Cancel"));
    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (engine->isExporting()) {
            engine->cancelExport();
            progressLabel_->setText(trText("pr.export.cancelling", "Cancelling..."));
        } else {
            reject();
        }
    });
    buttonLayout->addWidget(exportButton_);
    buttonLayout->addWidget(cancelButton_);
    layout->addLayout(buttonLayout);

    // 既存シグナル (初めて使用する)。新規 signal/slot 接続はこれらのみ。
    connect(engine, &ArtifactPr::EditorEngine::exportProgress, this, [this](int percent) {
        progressBar_->setValue(percent);
    });
    connect(engine, &ArtifactPr::EditorEngine::exportFinished,
            this, &ExportDialog::onExportFinished);
}

ArtifactPr::ExportFormat::Value ExportDialog::selectedFormat() const
{
    switch (codecCombo_->currentIndex()) {
    case 1: return ArtifactPr::ExportFormat::Value::HevcMp4;
    case 2: return ArtifactPr::ExportFormat::Value::ProResMov;
    case 3: return ArtifactPr::ExportFormat::Value::DnxhdMov;
    case 4: return ArtifactPr::ExportFormat::Value::PngSequence;
    case 5: return ArtifactPr::ExportFormat::Value::JpegSequence;
    case 6: return ArtifactPr::ExportFormat::Value::WavAudio;
    case 7: return ArtifactPr::ExportFormat::Value::Mp3Audio;
    case 0:
    default:
        return ArtifactPr::ExportFormat::Value::H264Mp4;
    }
}

void ExportDialog::onBrowseClicked()
{
    const ArtifactPr::ExportFormat format{selectedFormat()};
    QString filter = QStringLiteral("MP4 Files (*.mp4)");
    if (format.isAudioOnly()) {
        filter = format.value == ArtifactPr::ExportFormat::Value::WavAudio
            ? QStringLiteral("WAV Audio (*.wav)")
            : QStringLiteral("MP3 Audio (*.mp3)");
    } else if (format.isImageSequence()) {
        filter = format.value == ArtifactPr::ExportFormat::Value::PngSequence
            ? QStringLiteral("PNG Sequence (*.png)")
            : QStringLiteral("JPEG Sequence (*.jpg)");
    } else if (format.value == ArtifactPr::ExportFormat::Value::ProResMov ||
               format.value == ArtifactPr::ExportFormat::Value::DnxhdMov) {
        filter = QStringLiteral("MOV Files (*.mov)");
    }
    QString file = QFileDialog::getSaveFileName(
        this, trText("pr.export.save_dialog", "Save Export"),
        outputPathEdit_->text(), filter);
    if (!file.isEmpty()) {
        outputPathEdit_->setText(file);
    }
}

ArtifactPr::ExportSettings ExportDialog::collectSettings(QString* errorMessage) const
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    const ArtifactPr::DemoSequence sequence = engine->currentSequence();

    ArtifactPr::ExportSettings settings;
    settings.outputPath = outputPathEdit_->text().trimmed();
    settings.format = ArtifactPr::ExportFormat{selectedFormat()};
    settings.quality = qualitySlider_->value();

    int width = 0;
    int height = 0;
    // 値は userData 側から読む。表示文言は翻訳されるため currentText() は使わない。
    const QString resolutionChoice = resolutionCombo_->currentData().toString();
    if (resolutionChoice == QStringLiteral("match")) {
        parseResolutionParts(sequence.resolution, &width, &height);
    } else {
        parseResolutionParts(resolutionChoice, &width, &height);
    }

    int fps = 0;
    const QString rateChoice = framerateCombo_->currentData().toString();
    if (rateChoice == QStringLiteral("match")) {
        fps = parseFrameRate(sequence.frameRate);
    } else {
        fps = parseFrameRate(rateChoice);
    }

    if (width <= 0 || height <= 0) {
        width = 1920;
        height = 1080;
    }
    settings.width = width;
    settings.height = height;
    settings.fps = fps;
    settings.includeAudio = settings.format.isVideoFile()
        ? includeAudioCheck_->isChecked()
        : false;

    if (settings.outputPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = trText("pr.export.error.no_output",
                                    "Output file is not specified");
        }
        return settings;
    }
    if (!settings.outputPath.contains(QChar('.'))) {
        if (settings.format.isVideoFile()) {
            settings.outputPath += QStringLiteral(".mp4");
        } else if (settings.format.value == ArtifactPr::ExportFormat::Value::WavAudio) {
            settings.outputPath += QStringLiteral(".wav");
        } else if (settings.format.value == ArtifactPr::ExportFormat::Value::Mp3Audio) {
            settings.outputPath += QStringLiteral(".mp3");
        }
    }
    if (errorMessage) {
        const QFileInfo info(settings.outputPath);
        const QDir dir = info.dir();
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            *errorMessage = trText("pr.export.error.no_output_dir",
                                    "Cannot create the output directory: %1")
                                .arg(dir.absolutePath());
        }
    }
    return settings;
}

void ExportDialog::onExportClicked()
{
    auto* engine = ArtifactPr::EditorEngine::instance();
    if (engine->isExporting()) {
        return;
    }

    QString error;
    const ArtifactPr::ExportSettings settings = collectSettings(&error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, trText("pr.export.action", "Export"), error);
        return;
    }

    // レンジ: in/out ポイント。out <= in の場合はシーケンス全長へフォールバック。
    std::int64_t startFrame = engine->inPoint();
    std::int64_t endFrame = engine->outPoint();
    if (endFrame <= startFrame) {
        startFrame = 0;
        endFrame = std::max<std::int64_t>(0, engine->currentSequence().duration - 1);
        if (endFrame < startFrame) {
            QMessageBox::warning(this, trText("pr.export.action", "Export"),
                                 trText("pr.export.error.empty_sequence",
                                        "The sequence is empty, so there is no frame to export"));
            return;
        }
    }

    const ArtifactPr::RenderPlan plan =
        engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full, startFrame, endFrame);
    if (!plan.isValid()) {
        QMessageBox::warning(this, trText("pr.export.action", "Export"),
                             trText("pr.export.error.no_plan",
                                    "Could not create the render plan"));
        return;
    }

    progressBar_->setVisible(true);
    progressBar_->setValue(0);
    progressLabel_->setText(trText("pr.export.running", "Exporting..."));
    exportButton_->setEnabled(false);

    engine->startExport(plan, settings);
}

void ExportDialog::onExportFinished(bool success, const QString& message)
{
    exportButton_->setEnabled(true);
    if (success) {
        progressLabel_->setText(trText("pr.export.complete", "Export complete!"));
        accept();
    } else {
        progressLabel_->setText(message.isEmpty()
            ? trText("pr.export.failed", "Export failed")
            : message);
        progressBar_->setValue(0);
    }
}

W_OBJECT_IMPL(ExportDialog)
