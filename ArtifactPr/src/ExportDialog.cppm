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
    setWindowTitle(QStringLiteral("Export Settings"));
    setMinimumWidth(400);
    setModal(true);

    auto* engine = ArtifactPr::EditorEngine::instance();
    const ArtifactPr::DemoSequence sequence = engine->currentSequence();

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
    resolutionCombo_->addItems({
        QStringLiteral("Match Sequence"),
        QStringLiteral("1920x1080"),
        QStringLiteral("1280x720"),
        QStringLiteral("3840x2160"),
    });
    resolutionCombo_->setCurrentIndex(0);
    layout->addWidget(resolutionCombo_);

    layout->addWidget(new QLabel(QStringLiteral("Format:")));
    codecCombo_ = new QComboBox();
    codecCombo_->addItems({
        QStringLiteral("H.264 (MP4)"),
        QStringLiteral("H.265 (HEVC)"),
        QStringLiteral("ProRes"),
        QStringLiteral("DNxHD"),
        QStringLiteral("PNG Sequence"),
        QStringLiteral("JPEG Sequence"),
        QStringLiteral("Audio Only (WAV)"),
        QStringLiteral("Audio Only (MP3)"),
    });

    // format 変更時に拡張子と音声 checkbox の有効状態を更新
    connect(codecCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        QString fmt = codecCombo_->itemText(idx);
        QString path = outputPathEdit_->text();
        const auto replaceExtension = [&path](const QString& extension) {
            const int slash = qMax(path.lastIndexOf(QChar('/')), path.lastIndexOf(QChar('\\')));
            const int dot = path.lastIndexOf(QChar('.'));
            const int extensionStart = dot > slash ? dot : path.size();
            return path.left(extensionStart) + extension;
        };
        if (fmt == QStringLiteral("PNG Sequence")) {
            outputPathEdit_->setText(replaceExtension(QStringLiteral(".png")));
        } else if (fmt == QStringLiteral("JPEG Sequence")) {
            outputPathEdit_->setText(replaceExtension(QStringLiteral(".jpg")));
        } else if (fmt == QStringLiteral("ProRes") || fmt == QStringLiteral("DNxHD")) {
            outputPathEdit_->setText(replaceExtension(QStringLiteral(".mov")));
        } else if (fmt == QStringLiteral("Audio Only (WAV)")) {
            outputPathEdit_->setText(replaceExtension(QStringLiteral(".wav")));
        } else if (fmt == QStringLiteral("Audio Only (MP3)")) {
            outputPathEdit_->setText(replaceExtension(QStringLiteral(".mp3")));
        } else {
            outputPathEdit_->setText(replaceExtension(QStringLiteral(".mp4")));
        }
        // 音声 checkbox は動画形式のみ有効
        const ArtifactPr::ExportFormat format{selectedFormat()};
        includeAudioCheck_->setEnabled(format.isVideoFile());
    });
    layout->addWidget(codecCombo_);

    layout->addWidget(new QLabel(QStringLiteral("Frame Rate:")));
    framerateCombo_ = new QComboBox();
    framerateCombo_->addItems({
        QStringLiteral("Match Sequence"),
        QStringLiteral("24 fps"),
        QStringLiteral("25 fps"),
        QStringLiteral("30 fps"),
        QStringLiteral("60 fps"),
    });
    framerateCombo_->setCurrentIndex(0);
    layout->addWidget(framerateCombo_);

    layout->addWidget(new QLabel(QStringLiteral("Quality:")));
    qualitySlider_ = new QSlider(Qt::Horizontal);
    qualitySlider_->setMinimum(1);
    qualitySlider_->setMaximum(100);
    qualitySlider_->setValue(80);
    layout->addWidget(qualitySlider_);

    includeAudioCheck_ = new QCheckBox(QStringLiteral("Include Audio (mixed tracks)"));
    includeAudioCheck_->setChecked(true);
    layout->addWidget(includeAudioCheck_);

    progressLabel_ = new QLabel();
    progressBar_ = new QProgressBar();
    progressBar_->setVisible(false);
    layout->addWidget(progressLabel_);
    layout->addWidget(progressBar_);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    exportButton_ = new QPushButton(QStringLiteral("Export"));
    connect(exportButton_, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    cancelButton_ = new QPushButton(QStringLiteral("Cancel"));
    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (engine->isExporting()) {
            engine->cancelExport();
            progressLabel_->setText(QStringLiteral("Cancelling..."));
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
    QString file = QFileDialog::getSaveFileName(this, QStringLiteral("Save Export"),
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
    const QString resolutionChoice = resolutionCombo_->currentText();
    if (resolutionChoice == QStringLiteral("Match Sequence")) {
        parseResolutionParts(sequence.resolution, &width, &height);
    } else {
        parseResolutionParts(resolutionChoice, &width, &height);
    }

    int fps = 0;
    const QString rateChoice = framerateCombo_->currentText();
    if (rateChoice == QStringLiteral("Match Sequence")) {
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
        if (errorMessage) *errorMessage = QStringLiteral("出力ファイルが指定されていません");
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
            *errorMessage = QStringLiteral("出力ディレクトリを作成できません: %1").arg(dir.absolutePath());
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
        QMessageBox::warning(this, QStringLiteral("Export"), error);
        return;
    }

    // レンジ: in/out ポイント。out <= in の場合はシーケンス全長へフォールバック。
    std::int64_t startFrame = engine->inPoint();
    std::int64_t endFrame = engine->outPoint();
    if (endFrame <= startFrame) {
        startFrame = 0;
        endFrame = std::max<std::int64_t>(0, engine->currentSequence().duration - 1);
        if (endFrame < startFrame) {
            QMessageBox::warning(this, QStringLiteral("Export"),
                                 QStringLiteral("シーケンスが空のため書き出すフレームがありません"));
            return;
        }
    }

    const ArtifactPr::RenderPlan plan =
        engine->createRenderPlan(ArtifactPr::RenderQualityPreset::Full, startFrame, endFrame);
    if (!plan.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("レンダープランを作成できませんでした"));
        return;
    }

    progressBar_->setVisible(true);
    progressBar_->setValue(0);
    progressLabel_->setText(QStringLiteral("Exporting..."));
    exportButton_->setEnabled(false);

    engine->startExport(plan, settings);
}

void ExportDialog::onExportFinished(bool success, const QString& message)
{
    exportButton_->setEnabled(true);
    if (success) {
        progressLabel_->setText(QStringLiteral("Export complete!"));
        accept();
    } else {
        progressLabel_->setText(message.isEmpty()
            ? QStringLiteral("Export failed")
            : message);
        progressBar_->setValue(0);
    }
}

W_OBJECT_IMPL(ExportDialog)
