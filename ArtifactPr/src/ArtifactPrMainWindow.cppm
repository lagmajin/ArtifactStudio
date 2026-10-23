module;

#include <wobjectdefs.h>
#include <wobjectimpl.h>

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollArea>
#include <QComboBox>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QDir>
#include <QThread>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QWindow>
#include <QMouseEvent>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

module ArtifactPr.MainWindow;

import ArtifactPr.AudioPreviewMixer;
import ArtifactPr.ClipEffects;
import ArtifactPr.EditorEngine;
import ArtifactPr.ExportDialog;
import ArtifactPr.GpuProgramMonitor;
import ArtifactPr.MediaFrameDecoder;
import ArtifactPr.MediaPanel;
import ArtifactPr.MediaThumbnailer;
import ArtifactPr.ProjectPanel;
import ArtifactPr.SequenceCompositor;
import ArtifactPr.TransportBarWidget;
import ArtifactPr.TimelinePanel;
import ArtifactPr.Translation;
import ArtifactPr.VideoPlayerWidget;
import ArtifactPr.AppTheme;
import Dock.Manager;
import Dock.NativeDockSurface;
import Widgets.Utils.CSS;

import Image.ImageF32x4_RGBA;
import FloatRGBA;

namespace {

using ArtifactPr::trText;
using FramePosition = ArtifactPr::FramePosition;

int sequenceFrameRate(const ArtifactPr::DemoSequence& sequence)
{
    bool ok = false;
    const double parsed = sequence.frameRate.section(QChar(' '), 0, 0).toDouble(&ok);
    if (!ok || parsed <= 0.0) return 30;
    return qMax(1, static_cast<int>(parsed + 0.5));
}

QString percentLabel(int value)
{
    return QStringLiteral("%1%").arg(value);
}

/// legacy Transition から clipId に一致するフェードスパンを組む。
/// カーブ式は ClipEffects::transitionOpacityFactor (エクスポートと同一)。
QVector<ArtifactPr::TransitionFadeSpan> transitionFadeSpansFor(
    const QVector<ArtifactPr::Transition>& transitions,
    const QString& trackId,
    const QString& clipId)
{
    QVector<ArtifactPr::TransitionFadeSpan> spans;
    for (const auto& trans : transitions) {
        if (trans.trackId != trackId) continue;
        const bool isLeft = trans.leftClipId == clipId;
        if (!isLeft && trans.rightClipId != clipId) continue;
        ArtifactPr::TransitionFadeSpan span;
        span.startFrame = trans.startFrame;
        span.duration = trans.duration;
        span.isLeft = isLeft;
        span.curve = trans.type == ArtifactPr::TransitionType::DipToBlack
            ? ArtifactPr::TransitionFadeCurve::DipToBlack
            : ArtifactPr::TransitionFadeCurve::Linear;
        spans.append(span);
    }
    return spans;
}



class ProxyPanel : public QWidget
{
    W_OBJECT(ProxyPanel)
public:
    explicit ProxyPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(8);

        auto* label = new QLabel(trText("pr.proxy.title", "Proxy Settings"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        auto* descLabel = new QLabel(
            trText("pr.proxy.description", "Manage proxy files for smoother editing:"));
        layout->addWidget(descLabel);

        proxyList_ = new QListWidget();
        {
            // 昇格 token (inputBackgroundColor) を正規参照。空は Dark 既定へ fallback。
            const QColor fromToken(ArtifactCore::currentDCCTheme().inputBackgroundColor);
            const QColor legacy = ArtifactPr::prLegacyColors().panelBackgroundAlt;
            QPalette p = proxyList_->palette();
            p.setColor(QPalette::Base, fromToken.isValid() ? fromToken : legacy);
            proxyList_->setPalette(p);
        }
        layout->addWidget(proxyList_, 1);

        auto* buttonLayout = new QHBoxLayout();

        auto* createProxyBtn = new QPushButton(trText("pr.proxy.create", "Create Proxy"));
        {
            // 昇格 token (buttonInfoColor) を正規参照。旧 JSON 由来の空は Dark 既定へ fallback。
            const QColor fromToken(ArtifactCore::currentDCCTheme().buttonInfoColor);
            const QColor legacy = ArtifactPr::prLegacyColors().buttonProxyCreate;
            createProxyBtn->setProperty(ArtifactPr::kPropAccentColor,
                fromToken.isValid() ? fromToken : legacy);
        }
        connect(createProxyBtn, &QPushButton::clicked, this, &ProxyPanel::onCreateProxy);
        buttonLayout->addWidget(createProxyBtn);

        auto* useProxyBtn = new QPushButton(trText("pr.proxy.use", "Use Proxy"));
        {
            // 昇格 token (buttonSuccessColor) を正規参照。旧 JSON 由来の空は Dark 既定へ fallback。
            const QColor fromToken(ArtifactCore::currentDCCTheme().buttonSuccessColor);
            const QColor legacy = ArtifactPr::prLegacyColors().buttonProxyUse;
            useProxyBtn->setProperty(ArtifactPr::kPropAccentColor,
                fromToken.isValid() ? fromToken : legacy);
        }
        connect(useProxyBtn, &QPushButton::clicked, this, &ProxyPanel::onUseProxy);
        buttonLayout->addWidget(useProxyBtn);

        layout->addLayout(buttonLayout);

        auto* infoLabel = new QLabel(
            trText("pr.proxy.tip",
                   "Tip: Proxies are lower\nresolution versions for\nsmoother editing."));
        layout->addWidget(infoLabel);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::clipSelectionChanged, this, &ProxyPanel::onClipSelected);
    }

private slots:
    void onClipSelected(const QString& clipId)
    {
        proxyList_->clear();
        if (clipId.isEmpty()) return;

        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        auto* item = new QListWidgetItem(clip->name);
        item->setData(Qt::UserRole, clipId);
        proxyList_->addItem(item);
    }

    void onCreateProxy()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(engine->selectedClipId());
        if (!clip) return;

        auto* item = new QListWidgetItem(QStringLiteral("[Proxy] %1").arg(clip->name));
        item->setData(Qt::UserRole, clip->id);
        item->setBackground(QColor(60, 60, 40));
        proxyList_->addItem(item);
    }

    void onUseProxy()
    {
        QListWidgetItem* item = proxyList_->currentItem();
        if (!item) return;

        QString clipId = item->data(Qt::UserRole).toString();
        qDebug() << "Using proxy for clip:" << clipId;
    }

private:
    QListWidget* proxyList_ = nullptr;
};

W_OBJECT_IMPL(ProxyPanel)

// Standard button activation (mouse, keyboard and accessibility) owns callbacks.
class SourceRangeButton final : public QPushButton {
public:
    SourceRangeButton(const QString& text, std::function<void()> action)
        : QPushButton(text), action_(std::move(action)) {}
protected:
    void nextCheckState() override {
        QPushButton::nextCheckState();
        if (action_) action_();
    }
private:
    std::function<void()> action_;
};

class SourceMonitorPanel : public QWidget
{
    W_OBJECT(SourceMonitorPanel)
public:
    explicit SourceMonitorPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);

        auto* label = new QLabel(trText("pr.source.title", "Source Monitor"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        headerLayout->addWidget(label);
        headerLayout->addStretch();

        auto* setInBtn = new QPushButton(QStringLiteral("In"));
        setInBtn->setMaximumWidth(36);
        connect(setInBtn, &QPushButton::clicked, this, &SourceMonitorPanel::onSetInClicked);
        headerLayout->addWidget(setInBtn);

        auto* setOutBtn = new QPushButton(QStringLiteral("Out"));
        setOutBtn->setMaximumWidth(36);
        connect(setOutBtn, &QPushButton::clicked, this, &SourceMonitorPanel::onSetOutClicked);
        headerLayout->addWidget(setOutBtn);

        auto* insertBtn = new QPushButton(trText("pr.source.insert", "Insert"));
        insertBtn->setToolTip(
            trText("pr.source.insert_tooltip", "Insert source range at the playhead"));
        connect(insertBtn, &QPushButton::clicked, this, &SourceMonitorPanel::onInsertClicked);
        headerLayout->addWidget(insertBtn);

        auto* overwriteBtn = new QPushButton(trText("pr.source.overwrite", "Overwrite"));
        overwriteBtn->setToolTip(
            trText("pr.source.overwrite_tooltip", "Overwrite the timeline at the playhead"));
        connect(overwriteBtn, &QPushButton::clicked, this, &SourceMonitorPanel::onOverwriteClicked);
        headerLayout->addWidget(overwriteBtn);

        auto* fitInsert = new SourceRangeButton(trText("pr.source.fit_insert", "Fit Insert"),
            [this]() { insertSourceAtPlayhead(false, true); });
        fitInsert->setToolTip(
            trText("pr.source.fit_tooltip",
                   "Fit source In/Out to timeline In/Out (four-point edit)"));
        headerLayout->addWidget(fitInsert);
        auto* fitOverwrite = new SourceRangeButton(trText("pr.source.fit_overwrite", "Fit Overwrite"),
            [this]() { insertSourceAtPlayhead(true, true); });
        fitOverwrite->setToolTip(fitInsert->toolTip());
        headerLayout->addWidget(fitOverwrite);

        layout->addLayout(headerLayout);

        videoPlayer_ = new VideoPlayerWidget();
        videoPlayer_->setMinimumSize(320, 180);
        layout->addWidget(videoPlayer_, 1);

        auto* markersLayout = new QHBoxLayout();
        markersLayout->setContentsMargins(0, 0, 0, 0);
        inLabel_ = new QLabel(trText("pr.source.in_empty", "In: --"));
        outLabel_ = new QLabel(trText("pr.source.out_empty", "Out: --"));
        markersLayout->addWidget(inLabel_);
        markersLayout->addWidget(outLabel_);
        markersLayout->addStretch();
        layout->addLayout(markersLayout);

        // Thumbnail strip (PrSourceMonitor 拡張)
        thumbnailStrip_ = new QListWidget();
        thumbnailStrip_->setViewMode(QListView::IconMode);
        thumbnailStrip_->setFlow(QListView::LeftToRight);
        thumbnailStrip_->setWrapping(false);
        thumbnailStrip_->setIconSize(QSize(80, 45));
        thumbnailStrip_->setMaximumHeight(80);
        thumbnailStrip_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        layout->addWidget(thumbnailStrip_);

        thumbnailer_ = new ArtifactPr::MediaThumbnailer(this);
        connect(thumbnailer_, &ArtifactPr::MediaThumbnailer::thumbnailReady,
                this, &SourceMonitorPanel::onThumbnailReady);
        thumbnailer_->start();
    }

    void loadMedia(const QString& filePath)
    {
        if (!filePath.isEmpty()) {
            videoPlayer_->loadFile(filePath);
            videoPlayer_->play();
            currentFilePath_ = filePath;
            sourceIn_ = 0;
            sourceOut_ = -1;
            inLabel_->setText(trText("pr.source.in_initial", "In: 0"));
            outLabel_->setText(trText("pr.source.out_initial", "Out: end"));
            generateThumbnailStrip(filePath);
        }
    }

    void generateThumbnailStrip(const QString& filePath)
    {
        thumbnailStrip_->clear();

        // 5 箇所 (0% / 25% / 50% / 75% / 100%) のサムネを非同期要求
        const int kStripCount = 5;
        for (int i = 0; i < kStripCount; ++i) {
            auto* item = new QListWidgetItem(trText("pr.media.loading", "loading..."));
            thumbnailStrip_->addItem(item);

            ArtifactPr::ThumbnailRequest req;
            req.filePath = filePath;
            req.targetSize = QSize(80, 45);
            req.seekToMs = qint64(i) * 1000;  // 1 秒間隔 (簡易)
            item->setData(Qt::UserRole, req.seekToMs);
            // プレースホルダ判定はテキスト比較ではなく data role で行う。
            // テキストは翻訳されるため比較の基準にできない。
            item->setData(Qt::UserRole + 1, true);
            thumbnailer_->request(req);
        }
    }

private Q_SLOTS:
    void onThumbnailReady(ArtifactPr::MediaThumbnail thumb)
    {
        if (!thumb.valid || thumb.filePath != currentFilePath_) return;
        // 要求した seek 時刻に対応する slot を置換
        for (int i = 0; i < thumbnailStrip_->count(); ++i) {
            auto* item = thumbnailStrip_->item(i);
            if (item && item->data(Qt::UserRole + 1).toBool() &&
                item->data(Qt::UserRole).toLongLong() == thumb.seekToMs) {
                item->setText(QString());
                item->setIcon(QPixmap::fromImage(thumb.image));
                return;  // 1 つ置換して抜ける
            }
        }
    }

private Q_SLOTS:
    void onSetInClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        qint64 positionMs = videoPlayer_->position();
        const int fps = sequenceFrameRate(engine->currentSequence());
        FramePosition frame = (positionMs * fps) / 1000;
        sourceIn_ = frame;
        if (sourceOut_ >= 0 && sourceOut_ <= sourceIn_) sourceOut_ = -1;
        inLabel_->setText(trText("pr.source.in_frame", "In: %1").arg(frame));
    }

    void onSetOutClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        qint64 positionMs = videoPlayer_->position();
        const int fps = sequenceFrameRate(engine->currentSequence());
        FramePosition frame = (positionMs * fps) / 1000;
        sourceOut_ = frame;
        outLabel_->setText(trText("pr.source.out_frame", "Out: %1").arg(frame));
    }

    void insertSourceAtPlayhead(bool overwrite, bool fit = false)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (currentFilePath_.isEmpty()) return;

        const QString suffix = QFileInfo(currentFilePath_).suffix().toLower();
        const bool isAudio = suffix == QStringLiteral("mp3")
            || suffix == QStringLiteral("wav")
            || suffix == QStringLiteral("aac")
            || suffix == QStringLiteral("flac");
        const bool isImage = suffix == QStringLiteral("png")
            || suffix == QStringLiteral("jpg")
            || suffix == QStringLiteral("jpeg")
            || suffix == QStringLiteral("bmp")
            || suffix == QStringLiteral("tif")
            || suffix == QStringLiteral("tiff")
            || suffix == QStringLiteral("webp");

        QString trackId;
        const auto& tracks = isAudio
            ? engine->currentSequence().audioTracks
            : engine->currentSequence().videoTracks;
        for (const auto& track : tracks) {
            trackId = track.id;
            break;
        }
        if (trackId.isEmpty()) return;

        ArtifactPr::DemoClip source;
        source.name = QFileInfo(currentFilePath_).completeBaseName();
        source.sourceFile = currentFilePath_;
        source.sourceIn = sourceIn_;
        const auto mediaFrames = (videoPlayer_->duration() * sequenceFrameRate(engine->currentSequence())) / 1000;
        source.sourceOut = sourceOut_ >= 0 ? sourceOut_ :
            (isImage ? sourceIn_ + qMax<FramePosition>(90, engine->currentSequence().duration) : mediaFrames);
        if (source.sourceOut <= source.sourceIn ||
            (!isImage && mediaFrames > 0 && source.sourceOut > mediaFrames)) return;
        source.duration = source.sourceOut - source.sourceIn;
        source.color = isImage ? QStringLiteral("#a04aff") : QStringLiteral("#4a9eff");
        engine->editSourceRange(trackId, source, overwrite, fit);
    }

    void onInsertClicked() { insertSourceAtPlayhead(false); }
    void onOverwriteClicked() { insertSourceAtPlayhead(true); }

private:
    VideoPlayerWidget* videoPlayer_ = nullptr;
    QLabel* inLabel_ = nullptr;
    QLabel* outLabel_ = nullptr;
    QListWidget* thumbnailStrip_ = nullptr;
    ArtifactPr::MediaThumbnailer* thumbnailer_ = nullptr;
    QString currentFilePath_;
    FramePosition sourceIn_ = 0;
    FramePosition sourceOut_ = -1;
};

W_OBJECT_IMPL(SourceMonitorPanel)

class SequenceCanvasWidget : public QWidget
{
public:
    explicit SequenceCanvasWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(320, 180);
        setCursor(Qt::OpenHandCursor);
        setToolTip(trText("pr.viewport.tooltip", "Mouse wheel: zoom. Middle-drag: pan. Double-click: fit. Ctrl+double-click: 100%."));
        // QPalette::Window を黒に寄せてレターボックス部を黒で見せる
        setAutoFillBackground(true);
        QPalette canvasPalette = palette();
        canvasPalette.setColor(QPalette::Window, QColor(Qt::black));
        setPalette(canvasPalette);
    }

    /// GPU present がこのウィジェットを native render surface として使う場合、
    /// Qt の paint 経路は迂回される。CPU 合成 fallback 用の属性は GPU 有効時に
    /// 外さないと、プログラムが黒のまま何も描画されない。
    void setGpuPresentEnabled(bool enabled)
    {
        gpuPresent_ = enabled;
        if (enabled) {
            setAttribute(Qt::WA_NativeWindow);
            setAttribute(Qt::WA_DontCreateNativeAncestors);
            setAttribute(Qt::WA_PaintOnScreen);
            setAttribute(Qt::WA_NoSystemBackground);
            setAutoFillBackground(false);
        }
    }

    bool gpuPresentEnabled() const { return gpuPresent_; }

    /// GPU 有効時に Fit / zoom / pan が変化したら通知する。CPU 経路では
    /// update() で再描画されるため、このコールバックは GPU 時だけ呼ばれる。
    void setGpuInvalidateCallback(std::function<void()> callback)
    {
        gpuInvalidateCallback_ = std::move(callback);
    }

    /// pan / zoom 操作後の再描画要求。GPU 有効時は swap chain への
    /// 再 present を行い、無効時は通常の Qt 再描画に任せる。
    void requestGpuRepaint()
    {
        if (!gpuPresent_) {
            update();
            return;
        }
        if (gpuInvalidateCallback_) {
            gpuInvalidateCallback_();
        }
    }

    void setFrame(const ArtifactCore::ImageF32x4_RGBA& frame)
    {
        frame_ = frame;
        if (gpuPresent_) {
            // GPU 経路では QImage 化しない (AGENTS.md: GPU 正常時は
            // QImage / QPainter を使わない)。resize 無効時の表示は GPU 側が
            // swap chain で保持するため、ここでは何もしない。
            return;
        }
        // QImage 化は表示境界での明示変換 (AGENTS.md 整合)。
        cachedImage_ = frame_.toQImage();
        update();
    }

    void resetToBlack()
    {
        frame_ = ArtifactCore::ImageF32x4_RGBA();
        cachedImage_ = QImage();
        if (!gpuPresent_) {
            resetView();
        }
        update();
    }

    bool hasFrame() const { return !frame_.isEmpty(); }

    /// GPU 経路でも CPU 経路と同じ Fit / zoom / pan の配置を再現するための
    /// 正規化済みビュー矩形 (0..1, 上下反転) を返す。GPU が無効な場合は
    /// nullptr を返し、呼び出し側は CPU 表示に 任せる。
    ///
    /// view は「テクスチャ空間で描画矩形が占める範囲」を表す:
    /// scaleX/scaleY は 1  Fitz 時に 1.0 となる正規化スケール、offset は
    /// 中心からのずらし。
    bool gpuViewTransform(float& scaleX, float& scaleY,
                          float& offsetX, float& offsetY) const
    {
        if (!gpuPresent_ || frame_.isEmpty() || width() <= 0 || height() <= 0) {
            return false;
        }
        const qreal scale = fitScale() * zoom_;
        if (scale <= 0.0) {
            return false;
        }
        const QPointF center = QRectF(rect()).center() + panOffset_;
        const qreal drawW = frame_.width() * scale;
        const qreal drawH = frame_.height() * scale;
        // 描画矩形を widget 中央基準で [-1,1] の NDC に写す。
        scaleX = static_cast<float>(drawW / width());
        scaleY = static_cast<float>(drawH / height());
        offsetX = static_cast<float>((center.x() - drawW * 0.5 - width() * 0.5)
                                    / (width() * 0.5));
        offsetY = static_cast<float>((center.y() - drawH * 0.5 - height() * 0.5)
                                    / (height() * 0.5));
        return true;
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        if (gpuPresent_) {
            // GPU swap chain が surface を所有している。
            return;
        }
        QPainter p(this);
        p.fillRect(rect(), Qt::black);

        if (cachedImage_.isNull()) {
            return;
        }

        const QSize source = cachedImage_.size();
        const qreal scale = fitScale() * zoom_;
        const int drawW = static_cast<int>(source.width() * scale);
        const int drawH = static_cast<int>(source.height() * scale);
        const QPointF center = QRectF(rect()).center() + panOffset_;
        const int offsetX = static_cast<int>(std::lround(center.x() - drawW * 0.5));
        const int offsetY = static_cast<int>(std::lround(center.y() - drawH * 0.5));

        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QRect(offsetX, offsetY, drawW, drawH), cachedImage_);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (sourceSize().isEmpty() || event->angleDelta().y() == 0) {
            event->ignore();
            return;
        }

        const qreal oldScale = fitScale() * zoom_;
        const qreal factor = std::pow(1.0015, event->angleDelta().y());
        zoom_ = std::clamp(zoom_ * factor, kMinimumZoom, kMaximumZoom);
        const qreal newScale = fitScale() * zoom_;
        const QPointF pointer = event->position();
        const QPointF oldCenter = QRectF(rect()).center() + panOffset_;
        panOffset_ = pointer - QRectF(rect()).center()
            - (pointer - oldCenter) * (newScale / oldScale);
        requestGpuRepaint();
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::MiddleButton) {
            panning_ = true;
            panStart_ = event->position();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!panning_) {
            QWidget::mouseMoveEvent(event);
            return;
        }

        panOffset_ += event->position() - panStart_;
        panStart_ = event->position();
        requestGpuRepaint();
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::MiddleButton && panning_) {
            panning_ = false;
            setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mouseDoubleClickEvent(event);
            return;
        }

        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            showActualPixels();
        } else {
            resetView();
            requestGpuRepaint();
        }
        event->accept();
    }

private:
    /// ソースフレームの基準サイズ。GPU 有効時は QImage を作らないので
    /// frame_ を、CPU 経路では 変換済み cachedImage_ を使う。
    QSize sourceSize() const
    {
        if (gpuPresent_) {
            return frame_.isEmpty() ? QSize() : QSize(frame_.width(), frame_.height());
        }
        return cachedImage_.size();
    }

    qreal fitScale() const
    {
        const QSize source = sourceSize();
        if (source.width() <= 0 || source.height() <= 0
            || width() <= 0 || height() <= 0) {
            return 1.0;
        }
        return std::min(static_cast<qreal>(width()) / source.width(),
                        static_cast<qreal>(height()) / source.height());
    }

    void resetView()
    {
        zoom_ = 1.0;
        panOffset_ = QPointF();
    }

    void showActualPixels()
    {
        const qreal scale = fitScale();
        zoom_ = scale > 0.0 ? std::clamp(1.0 / scale, kMinimumZoom, kMaximumZoom) : 1.0;
        panOffset_ = QPointF();
        requestGpuRepaint();
    }

    static constexpr qreal kMinimumZoom = 0.1;
    static constexpr qreal kMaximumZoom = 16.0;
    ArtifactCore::ImageF32x4_RGBA frame_;
    QImage cachedImage_;
    qreal zoom_ = 1.0;
    QPointF panOffset_;
    QPointF panStart_;
    bool panning_ = false;
    bool gpuPresent_ = false;
    std::function<void()> gpuInvalidateCallback_;
};

class ProgramMonitorPanel : public QWidget
{
    W_OBJECT(ProgramMonitorPanel)

    enum class GpuState {
        NotTried,
        Ready,
        Disabled,
    };

public:
    explicit ProgramMonitorPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* label = new QLabel(trText("pr.monitor.title", "Program Monitor"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        preview_ = new SequenceCanvasWidget();
        preview_->setMinimumSize(320, 180);
        layout->addWidget(preview_, 1);

        preview_->setGpuInvalidateCallback([this]() { presentGpuFrame(); });

        gpuRenderer_ = std::make_unique<ArtifactPr::GpuProgramMonitorRenderer>();
        gpuStatusLabel_ = new QLabel();
        gpuStatusLabel_->setObjectName(QStringLiteral("programMonitorGpuStatus"));
        QFont statusFont = gpuStatusLabel_->font();
        statusFont.setPointSize(qMax(6, statusFont.pointSize() - 1));
        gpuStatusLabel_->setFont(statusFont);
        layout->addWidget(gpuStatusLabel_);
        updateGpuStatusLabel();

        timecode_ = new QLabel(QStringLiteral("00:00:00:00"));
        QFont tcFont = timecode_->font();
        tcFont.setBold(true);
        timecode_->setFont(tcFont);
        layout->addWidget(timecode_);

        decoder_ = new ArtifactPr::MediaFrameDecoder(this);
        decoder_->setFrameReadyCallback([this](const ArtifactPr::FrameDecodeResult& result) {
            onFrameDecoded(result);
        });
        decoder_->start();

        audioMixer_ = std::make_unique<ArtifactPr::AudioPreviewMixer>();
        audioMixer_->setLevelCallback([this](float leftPeakDb, float rightPeakDb) {
            if (meterPanel_) {
                meterPanel_->updateFromLevels(leftPeakDb, rightPeakDb);
            }
        });

        auto* engine = ArtifactPr::EditorEngine::instance();

        connect(engine, &ArtifactPr::EditorEngine::currentFrameChanged, this, &ProgramMonitorPanel::updateTimecode);
        connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &ProgramMonitorPanel::onSequenceChanged);
        connect(engine, &ArtifactPr::EditorEngine::playbackStateChanged, this, &ProgramMonitorPanel::onPlaybackStateChanged);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (gpuState_ != GpuState::Ready) {
            return;
        }
        // Program Monitor の resize は連続発生し頻度も高くない
        // (パネルリサイズ / ウィンドウ最大化) ため、debounce せず
        // swapChain_->Resize() に直接任せる。
        if (gpuRenderer_->recreateSwapChain(preview_)) {
            presentGpuFrame();
        }
    }

    /// AudioMeterPanel を後から登録 (MainWindow 構築順の解決用)。
    void setMeterPanel(AudioMeterPanel* panel) { meterPanel_ = panel; }

    /// TimelinePanel へ実波形を提供する (B2)。
    std::function<const QVector<float>*(const QString&)> waveformProvider() const
    {
        return [this](const QString& filePath) {
            return audioMixer_ ? audioMixer_->waveformPeaks(filePath) : nullptr;
        };
    }

private:
    /// GPU present 経路の初期化。ウィジェットが実際のサイズを持つ最初の
    /// フレームで一度だけ试行し、失敗したら CPU 表示へ留まる。
    bool ensureGpuPreviewReady()
    {
        if (gpuState_ == GpuState::Ready) {
            return true;
        }
        if (gpuState_ == GpuState::Disabled) {
            return false;
        }

        if (!preview_->isVisible() && gpuAttempts_ == 0) {
            // まだレイアウトに入っていないので先延ばしする。
            return false;
        }

        ++gpuAttempts_;
        if (!gpuRenderer_->ensureInitialized(preview_)) {
            gpuState_ = GpuState::Disabled;
            updateGpuStatusLabel();
            qWarning() << "[ProgramMonitor] GPU preview unavailable, using CPU path:"
                       << gpuRenderer_->lastFailureReason();
            return false;
        }

        preview_->setGpuPresentEnabled(true);
        gpuState_ = GpuState::Ready;
        updateGpuStatusLabel();
        return true;
    }

    void updateGpuStatusLabel()
    {
        if (!gpuStatusLabel_) {
            return;
        }
        switch (gpuState_) {
        case GpuState::Ready:
            gpuStatusLabel_->setText(trText("pr.monitor.gpu_status", "GPU Preview: %1").arg(gpuRenderer_ ? gpuRenderer_->backendName()
                                                        : QStringLiteral("unknown")));
            break;
        case GpuState::Disabled:
            gpuStatusLabel_->setText(trText("pr.monitor.gpu_fallback", "GPU Preview: CPU fallback"));
            break;
        case GpuState::NotTried:
            gpuStatusLabel_->setText(trText("pr.monitor.gpu_pending", "GPU Preview: pending"));
            break;
        }
    }

    /// GPU 経路が有効なときだけ present する。
    void presentGpuFrame()
    {
        if (gpuState_ != GpuState::Ready) {
            return;
        }
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        if (preview_->gpuViewTransform(scaleX, scaleY, offsetX, offsetY)) {
            gpuRenderer_->setViewTransform(scaleX, scaleY, offsetX, offsetY);
        }
        gpuRenderer_->renderAndPresent();

        // device loss / surface loss は Present() が例外を投げて通知する。
        // 記録された reason があれば GPU 経路を破棄して CPU 表示へ退避し、
        // export の CPU 出力には影響させない。
        const QString reason = gpuRenderer_->lastFailureReason();
        if (reason.isEmpty()) {
            return;
        }
        qWarning() << "[ProgramMonitor] GPU present failed, falling back to CPU:"
                   << reason;
        gpuRenderer_->reset();
        preview_->setGpuPresentEnabled(false);
        preview_->setAutoFillBackground(true);
        QPalette canvasPalette = preview_->palette();
        canvasPalette.setColor(QPalette::Window, QColor(Qt::black));
        preview_->setPalette(canvasPalette);
        preview_->setFrame(gpuLastComposedFrame_);
        gpuState_ = GpuState::Disabled;
        updateGpuStatusLabel();
    }

    /// 合成済みフレームを表示する。GPU 経路が有効なら upload して present、
    /// 無効なら CPU の QImage 表示に任せる。
    void displayComposedFrame(const ArtifactCore::ImageF32x4_RGBA& composed)
    {
        if (ensureGpuPreviewReady()) {
            // requestGeneration_ を upload にも通し、遅延到着した古い合成
            // 結果が新しいフレームの後に present されるのを防ぐ。
            gpuLastComposedFrame_ = composed;
            gpuRenderer_->setFrame(composed, requestGeneration_);
            presentGpuFrame();
            return;
        }
        preview_->setFrame(composed);
    }

    /// audioTracks の音声クリップを mixer へ反映 (mute/solo 考慮)。
    void syncAudioClips(const ArtifactPr::DemoSequence& sequence)
    {
        if (!audioMixer_) return;
        bool anySolo = false;
        for (const auto& track : sequence.audioTracks) {
            if (track.solo) { anySolo = true; break; }
        }

        QVector<ArtifactPr::PreviewAudioClip> clips;
        for (const auto& track : sequence.audioTracks) {
            if (track.muted) continue;
            if (anySolo && !track.solo) continue;
            for (const auto& clip : track.clips) {
                if (!clip.enabled || clip.sourceFile.isEmpty()) continue;
                ArtifactPr::PreviewAudioClip entry;
                entry.id = clip.id;
                entry.filePath = clip.sourceFile;
                entry.startFrame = clip.startFrame;
                entry.durationFrames = clip.duration;
                entry.sourceIn = clip.sourceIn;
                entry.volume = clip.volume * ArtifactPr::audioGainFactor(clip.effects);
                ArtifactPr::audioEqualizerDb(clip.effects,
                                             entry.eqLowDb, entry.eqMidDb, entry.eqHighDb);
                clips.append(entry);
            }
        }
        audioMixer_->setClips(clips);
    }

private Q_SLOTS:
    void updateTimecode(ArtifactPr::FramePosition frame)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        const int fps = sequenceFrameRate(engine->currentSequence());
        int totalSeconds = static_cast<int>(frame) / fps;
        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;
        int frames = static_cast<int>(frame) % fps;

        timecode_->setText(QStringLiteral("%1:%2:%3:%4")
                               .arg(hours, 2, 10, QChar('0'))
                               .arg(minutes, 2, 10, QChar('0'))
                               .arg(seconds, 2, 10, QChar('0'))
                               .arg(frames, 2, 10, QChar('0')));

        requestPreviewFrame(frame);
    }

    void onSequenceChanged(const ArtifactPr::DemoSequence& seq)
    {
        Q_UNUSED(seq);
        // 編集反映のたびに要求を無効化し、次フレーム位置で再合成する。
        ++requestGeneration_;
        decoder_->clearCache();
        if (gpuState_ == GpuState::Ready) {
            // GPU 経路では preview_->setFrame() の QImage 化を行わないため、
            // renderer 側のキャッシュを明示的に落とす必要がある。
            gpuRenderer_->setFrame(ArtifactCore::ImageF32x4_RGBA());
            presentGpuFrame();
        } else {
            preview_->resetToBlack();
        }
        syncAudioClips(seq);
        requestPreviewFrame(ArtifactPr::EditorEngine::instance()->currentFrame());
    }

    void onPlaybackStateChanged(bool isPlaying)
    {
        if (!audioMixer_) return;
        if (isPlaying) {
            audioMixer_->play(ArtifactPr::EditorEngine::instance()->currentFrame());
        } else {
            audioMixer_->pause();
        }
    }

private:
    /// 表示中の 1 クリップ要求 (z-order 保持のため QVector で管理)。
    struct PendingClipRequest {
        QString clipId;
        ArtifactPr::FrameDecodeRequest request;
        double opacity = 1.0;
    };

    /// 指定 timeline フレーム位置で表示すべきクリップを収集し、
    /// 各クリップのソースフレーム decode を要求する。
    void requestPreviewFrame(FramePosition frame)
    {
        ++requestGeneration_;
        const quint64 generation = requestGeneration_;

        QVector<PendingClipRequest> pending;
        const ArtifactPr::DemoSequence sequence =
            ArtifactPr::EditorEngine::instance()->currentSequence();

        // solo が一つでも有効なら solo トラックのみ表示 (D3)
        bool anySolo = false;
        for (const auto& track : sequence.videoTracks) {
            if (track.solo) { anySolo = true; break; }
        }

        for (const auto& track : sequence.videoTracks) {
            if (track.muted) continue;
            if (anySolo && !track.solo) continue;
            for (const auto& clip : track.clips) {
                if (!clip.enabled) continue;
                if (clip.sourceFile.isEmpty()) continue;
                if (frame < clip.startFrame
                    || frame >= clip.startFrame + clip.duration) {
                    continue;
                }

                FramePosition sourceFrame = clip.sourceIn
                    + static_cast<FramePosition>(
                        std::llround(static_cast<double>(frame - clip.startFrame) * clip.speed));
                if (clip.reversed) {
                    sourceFrame = clip.sourceOut - (sourceFrame - clip.sourceIn);
                    sourceFrame = qBound(clip.sourceIn, sourceFrame,
                                         qMax(clip.sourceIn, clip.sourceOut - 1));
                }
                sourceFrame = qMax(sourceFrame, FramePosition(0));

                PendingClipRequest entry;
                entry.clipId = clip.id;
                entry.opacity = clip.opacity
                    * ArtifactPr::transitionOpacityFactor(
                        transitionFadeSpansFor(sequence.transitions, track.id, clip.id), frame);
                entry.request.filePath = clip.sourceFile;
                entry.request.stillImage = isStillImagePath(clip.sourceFile);
                entry.request.sourceFrame = sourceFrame;
                entry.request.generation = generation;
                pending.append(entry);
            }
        }

        pendingRequests_ = pending;
        decodedFrames_.clear();

        if (pending.isEmpty()) {
            // 表示クリップなし → 黒画面へ戻す (generation 更新済みなので
            // 遅延到着した旧結果は破棄される)
            if (gpuState_ == GpuState::Ready) {
                gpuRenderer_->setFrame(ArtifactCore::ImageF32x4_RGBA());
                presentGpuFrame();
            } else {
                preview_->resetToBlack();
            }
            return;
        }

        for (const PendingClipRequest& entry : pending) {
            decoder_->request(entry.request);
        }
    }

    void onFrameDecoded(const ArtifactPr::FrameDecodeResult& result)
    {
        if (result.generation != requestGeneration_) return;
        if (!result.valid) return;
        // 同一 filePath の要求が複数クリップあっても 1 つのデコードで共有される
        bool needed = false;
        for (const PendingClipRequest& entry : pendingRequests_) {
            if (entry.request.filePath == result.filePath) {
                needed = true;
                break;
            }
        }
        if (!needed) return;
        if (decodedFrames_.contains(result.filePath)) return;

        decodedFrames_.insert(result.filePath, result.image);

        // 未完了の要求が残っていれば待つ (次の onFrameDecoded で再評価)
        for (const PendingClipRequest& entry : pendingRequests_) {
            if (!decodedFrames_.contains(entry.request.filePath)) {
                return;
            }
        }

        // 全クリップのフレームが揃ったので z-order 順に合成して表示
        QVector<ArtifactPr::CompositeLayer> layers;
        layers.reserve(pendingRequests_.size());
        auto* engine = ArtifactPr::EditorEngine::instance();
        for (const PendingClipRequest& entry : pendingRequests_) {
            const auto found = decodedFrames_.constFind(entry.request.filePath);
            if (found == decodedFrames_.constEnd()) continue;
            ArtifactPr::CompositeLayer layer;
            // デコード結果は複数クリップで共有されるため DeepCopy してから
            // クリップエフェクトを適用する。
            ArtifactCore::ImageF32x4_RGBA frame = found.value().DeepCopy();
            if (const auto* clip = engine->findClip(entry.clipId)) {
                ArtifactPr::applyClipEffects(frame, clip->effects);
            }
            layer.frame = std::move(frame);
            layer.opacity = entry.opacity;
            layers.append(layer);
        }

        const QSize canvasSize = monitorResolution(
            ArtifactPr::EditorEngine::instance()->currentSequence());
        const ArtifactCore::FloatRGBA background(0.0f, 0.0f, 0.0f, 1.0f);
        displayComposedFrame(
            ArtifactPr::composeSequenceLayers(canvasSize, layers, background));
    }

    static QSize monitorResolution(const ArtifactPr::DemoSequence& sequence)
    {
        const QStringList parts = sequence.resolution.split(QChar('x'));
        bool okW = false;
        bool okH = false;
        const int w = parts.value(0).toInt(&okW);
        const int h = parts.value(1).toInt(&okH);
        if (okW && okH && w > 0 && h > 0) {
            return QSize(w, h);
        }
        return QSize(1920, 1080);
    }

    static bool isStillImagePath(const QString& path)
    {
        const QString suffix = QFileInfo(path).suffix().toLower();
        static const QSet<QString> imageExts = {
            QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("bmp"), QStringLiteral("tif"), QStringLiteral("tiff"),
            QStringLiteral("webp")
        };
        return imageExts.contains(suffix);
    }

    ArtifactPr::MediaFrameDecoder* decoder_ = nullptr;
    std::unique_ptr<ArtifactPr::AudioPreviewMixer> audioMixer_;
    std::unique_ptr<ArtifactPr::GpuProgramMonitorRenderer> gpuRenderer_;
    AudioMeterPanel* meterPanel_ = nullptr;
    SequenceCanvasWidget* preview_ = nullptr;
    QLabel* timecode_ = nullptr;
    QLabel* gpuStatusLabel_ = nullptr;
    quint64 requestGeneration_ = 0;
    int gpuAttempts_ = 0;
    GpuState gpuState_ = GpuState::NotTried;
    ArtifactCore::ImageF32x4_RGBA gpuLastComposedFrame_;
    QVector<PendingClipRequest> pendingRequests_;
    QHash<QString, ArtifactCore::ImageF32x4_RGBA> decodedFrames_;
};

W_OBJECT_IMPL(ProgramMonitorPanel)

class AudioMeterWidget : public QWidget
{
    W_OBJECT(AudioMeterWidget)
public:
    AudioMeterWidget(QWidget* parent = nullptr)
        : QWidget(parent), level_(0.0f)
    {
        setMinimumHeight(120);
        setMinimumWidth(40);

        // peak hold の自動 decay タイマ (60fps)
        peakHoldTimer_ = new QTimer(this);
        peakHoldTimer_->setInterval(16);
        connect(peakHoldTimer_, &QTimer::timeout, this, [this]() {
            if (peakHoldCountdown_ > 0) {
                --peakHoldCountdown_;
            } else {
                // decay: 1 秒で 50% 減衰
                peakHold_ *= 0.992f;
                if (peakHold_ < level_) peakHold_ = level_;
            }
            update();
        });
        peakHoldTimer_->start();
    }

    void setLevel(float level)
    {
        level_ = qBound(0.0f, level, 1.0f);
        if (level_ > peak_) {
            peak_ = level_;
        }
        if (level_ > peakHold_) {
            peakHold_ = level_;
            peakHoldCountdown_ = 60;  // 1 秒 hold
        }
        update();
    }

    void setShowDbLabels(bool enabled) { showDbLabels_ = enabled; update(); }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRect r = rect().adjusted(2, 2, -2, -2);

        p.fillRect(r, QColor(30, 30, 30));

        float dbScale = level_;
        int meterHeight = static_cast<int>(r.height() * dbScale);

        QColor meterColor;
        if (dbScale > 0.85f) {
            meterColor = QColor(255, 60, 60);
        } else if (dbScale > 0.70f) {
            meterColor = QColor(255, 200, 60);
        } else {
            meterColor = QColor(60, 220, 80);
        }

        QRect meterRect = r.adjusted(0, r.height() - meterHeight, 0, 0);
        p.fillRect(meterRect, meterColor);

        // peak hold (白い横線)
        int peakHoldHeight = static_cast<int>(r.height() * peakHold_);
        if (peakHoldHeight > 0) {
            QRect peakRect = r.adjusted(0, r.height() - peakHoldHeight - 2, 0, r.height() - peakHoldHeight);
            p.fillRect(peakRect, QColor(255, 255, 255));
        }

        // グリッド線
        QPen gridPen(QColor(60, 60, 60));
        for (int i = 0; i <= 10; ++i) {
            int y = r.top() + (r.height() * i) / 10;
            p.setPen(gridPen);
            p.drawLine(r.left(), y, r.right(), y);
        }

        // dB スケール (左)
        if (showDbLabels_) {
            QFont dbFont = p.font();
            dbFont.setPointSize(7);
            p.setFont(dbFont);
            p.setPen(QColor(120, 120, 120));
            // -60 / -48 / -36 / -24 / -12 / -6 / 0 dB
            const double dbs[] = { 0, -6, -12, -24, -36, -48, -60 };
            for (double db : dbs) {
                const double fraction = (db >= 0.0) ? 1.0 : (db + 60.0) / 60.0;
                int y = r.bottom() - static_cast<int>(r.height() * fraction);
                p.drawText(QRect(2, y - 5, 16, 10),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           QString::number(static_cast<int>(db)));
            }
        }

        p.setPen(QColor(100, 100, 100));
        p.drawRect(r);
    }

public slots:
    void updateLevel(float level)
    {
        setLevel(level);
    }

    void resetPeak()
    {
        peak_ = level_;
        peakHold_ = level_;
        update();
    }

private:
    float level_;
    float peak_ = 0.0f;
    float peakHold_ = 0.0f;
    int peakHoldCountdown_ = 0;
    QTimer* peakHoldTimer_ = nullptr;
    bool showDbLabels_ = true;
};

W_OBJECT_IMPL(AudioMeterWidget)

class AudioMeterPanel : public QWidget
{
    W_OBJECT(AudioMeterPanel)
public:
    explicit AudioMeterPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* label = new QLabel(trText("pr.audio.meters", "Audio Meters"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        auto* meterLayout = new QHBoxLayout();
        meterLayout->setSpacing(4);

        for (int i = 0; i < 2; ++i) {
            auto* meterWidget = new AudioMeterWidget();
            meterWidgets_.push_back(meterWidget);
            meterLayout->addWidget(meterWidget);
        }

        layout->addLayout(meterLayout);

        auto* buttonLayout = new QHBoxLayout();
        auto* resetPeakBtn = new QPushButton(trText("pr.audio.reset_peak", "Reset Peak"));
        connect(resetPeakBtn, &QPushButton::clicked, this, &AudioMeterPanel::onResetPeakClicked);
        buttonLayout->addWidget(resetPeakBtn);
        buttonLayout->addStretch();
        layout->addLayout(buttonLayout);

        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, &AudioMeterPanel::onMeterTick);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::playbackStateChanged, this, &AudioMeterPanel::onPlaybackStateChanged);
    }

private Q_SLOTS:
    void onMeterTick()
    {
        // 実データ経路: AudioPreviewMixer のレベルコールバックが
        // updateFromLevels() を呼ぶ。タイマーは減衰描画の維持用。
        for (auto* meter : meterWidgets_) {
            meter->update();
        }
    }

    void onPlaybackStateChanged(bool isPlaying)
    {
        if (isPlaying) {
            meterTimer_->start(50);
        } else {
            meterTimer_->stop();
            for (auto* meter : meterWidgets_) {
                meter->updateLevel(0.0f);
            }
        }
    }

    void onResetPeakClicked()
    {
        for (auto* meter : meterWidgets_) {
            meter->resetPeak();
        }
    }

public:
    /// AudioPreviewMixer の level callback から呼ぶ (UI thread)。
    void updateFromLevels(float leftPeakDb, float rightPeakDb)
    {
        if (meterWidgets_.size() >= 1) {
            meterWidgets_[0]->updateLevel(dbToMeterLevel(leftPeakDb));
        }
        if (meterWidgets_.size() >= 2) {
            meterWidgets_[1]->updateLevel(dbToMeterLevel(rightPeakDb));
        }
    }

private:
    static float dbToMeterLevel(float db)
    {
        // -60dBFS..0dBFS を 0..1 へ線形マップ
        constexpr float kMinDb = -60.0f;
        const float clamped = qBound(kMinDb, db, 0.0f);
        return (clamped - kMinDb) / -kMinDb;
    }

    QVector<AudioMeterWidget*> meterWidgets_;
    QTimer* meterTimer_ = nullptr;
};

W_OBJECT_IMPL(AudioMeterPanel)

class TransitionPanel : public QWidget
{
    W_OBJECT(TransitionPanel)
public:
    explicit TransitionPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(8);

        auto* label = new QLabel(trText("pr.panel.transitions.title", "Transitions"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        auto* descLabel = new QLabel(trText("pr.panel.transitions.select_type", "Select transition type:"));
        layout->addWidget(descLabel);

        auto* buttonLayout = new QVBoxLayout();
        buttonLayout->setSpacing(4);

        auto addTransBtn = [this, buttonLayout](const QString& name, ArtifactPr::TransitionType type, const QString& color) {
            auto* btn = new QPushButton(name);
            btn->setProperty(ArtifactPr::kPropAccentColor, QColor(color));
            connect(btn, &QPushButton::clicked, this, [type]() {
                auto* engine = ArtifactPr::EditorEngine::instance();
                engine->addTransitionAtPlayhead(type, 12);
            });

            // drag source 化: custom MIME data に type を入れる
            btn->installEventFilter(this);  // eventFilter で mousePress / mouseMove を拾う
            btn->setProperty("artifactTransitionType", static_cast<int>(type));
            btn->setProperty("artifactTransitionName", name);

            buttonLayout->addWidget(btn);
            return btn;
        };

        addTransBtn(trText("pr.transition.crossfade", "Crossfade"), ArtifactPr::TransitionType::Crossfade, QStringLiteral("#c89664"));
        addTransBtn(trText("pr.transition.dip_to_black", "Dip to Black"), ArtifactPr::TransitionType::DipToBlack, QStringLiteral("#646464"));
        addTransBtn(trText("pr.transition.wipe_left", "Wipe Left"), ArtifactPr::TransitionType::WipeLeft, QStringLiteral("#9664c8"));
        addTransBtn(trText("pr.transition.wipe_right", "Wipe Right"), ArtifactPr::TransitionType::WipeRight, QStringLiteral("#c86496"));

        layout->addLayout(buttonLayout);

        layout->addStretch();

        auto* infoLabel = new QLabel(trText("pr.panel.transitions.tip", "Tip: Select a clip and\nposition playhead at\nclip boundary to apply."));
        layout->addWidget(infoLabel);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::transitionChanged, this, &TransitionPanel::onTransitionChanged);
    }

private slots:
    void onTransitionChanged()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto transitions = engine->transitions();
        update();
    }
};

W_OBJECT_IMPL(TransitionPanel)

class EffectsPanel : public QWidget
{
    W_OBJECT(EffectsPanel)
public:
    explicit EffectsPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(8);

        auto* label = new QLabel(trText("pr.panel.effects.title", "Effects"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        searchEdit_ = new QLineEdit();
        searchEdit_->setPlaceholderText(trText("pr.panel.effects.search", "Search effects..."));
        {
            const QColor fromToken(ArtifactCore::currentDCCTheme().inputBackgroundColor);
            const QColor legacy = ArtifactPr::prLegacyColors().inputBackground;
            QPalette p = searchEdit_->palette();
            p.setColor(QPalette::Base, fromToken.isValid() ? fromToken : legacy);
            searchEdit_->setPalette(p);
        }
        layout->addWidget(searchEdit_);

        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget();
        rowsLayout_ = new QVBoxLayout(content);
        rowsLayout_->setContentsMargins(0, 0, 0, 0);
        rowsLayout_->setSpacing(4);
        rowsLayout_->addStretch(1);
        scroll->setWidget(content);
        layout->addWidget(scroll, 1);

        infoLabel_ = new QLabel(trText("pr.panel.effects.select_clip", "Select a clip to edit its effects."));
        layout->addWidget(infoLabel_);

        buildEffectRows();

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::clipSelectionChanged, this, &EffectsPanel::onClipSelected);
        connect(engine, &ArtifactPr::EditorEngine::clipChanged, this, &EffectsPanel::onClipSelected);
        connect(searchEdit_, &QLineEdit::textChanged, this, [this]() { applySearchFilter(); });

        updateWidgetsFromClip();
    }

private slots:
    void onClipSelected(const QString& clipId)
    {
        Q_UNUSED(clipId);
        updateWidgetsFromClip();
    }

private:
    struct ParamSpec {
        const char* key;
        const char* labelEn;
        const char* labelKey;
        double min;
        double max;
        double step;
        double defaultValue;
        int decimals;
    };

    struct ParamWidget {
        QString key;
        QDoubleSpinBox* spin = nullptr;
        double defaultValue = 0.0;
    };

    struct EffectRow {
        QString effectId;
        QCheckBox* enableCheck = nullptr;
        QWidget* paramsWidget = nullptr;
        QVector<ParamWidget> params;
    };

    void buildEffectRows()
    {
        addEffectRow("brightnessContrast",
                     "Color Correction > Brightness/Contrast", "pr.effect.brightnessContrast", {
            { "brightness", "Brightness", "pr.effect.param.brightnessContrast.brightness", -1.0, 1.0, 0.01, 0.0, 2 },
            { "contrast", "Contrast", "pr.effect.param.brightnessContrast.contrast", 0.0, 2.0, 0.01, 1.0, 2 },
        });
        addEffectRow("hueSaturation",
                     "Color Correction > Hue/Saturation", "pr.effect.hueSaturation", {
            { "hue", "Hue", "pr.effect.param.hueSaturation.hue", -180.0, 180.0, 1.0, 0.0, 1 },
            { "saturation", "Saturation", "pr.effect.param.hueSaturation.saturation", 0.0, 2.0, 0.01, 1.0, 2 },
            { "lightness", "Lightness", "pr.effect.param.hueSaturation.lightness", -1.0, 1.0, 0.01, 0.0, 2 },
        });
        addEffectRow("colorWheels",
                     "Color Correction > Color Wheels", "pr.effect.colorWheels", {
            { "shadowsR", "Shadows R", "pr.effect.param.colorWheels.shadowsR", -0.5, 0.5, 0.01, 0.0, 2 },
            { "shadowsG", "Shadows G", "pr.effect.param.colorWheels.shadowsG", -0.5, 0.5, 0.01, 0.0, 2 },
            { "shadowsB", "Shadows B", "pr.effect.param.colorWheels.shadowsB", -0.5, 0.5, 0.01, 0.0, 2 },
            { "midtonesR", "Midtones R", "pr.effect.param.colorWheels.midtonesR", -0.5, 0.5, 0.01, 0.0, 2 },
            { "midtonesG", "Midtones G", "pr.effect.param.colorWheels.midtonesG", -0.5, 0.5, 0.01, 0.0, 2 },
            { "midtonesB", "Midtones B", "pr.effect.param.colorWheels.midtonesB", -0.5, 0.5, 0.01, 0.0, 2 },
            { "highlightsR", "Highlights R", "pr.effect.param.colorWheels.highlightsR", -0.5, 0.5, 0.01, 0.0, 2 },
            { "highlightsG", "Highlights G", "pr.effect.param.colorWheels.highlightsG", -0.5, 0.5, 0.01, 0.0, 2 },
            { "highlightsB", "Highlights B", "pr.effect.param.colorWheels.highlightsB", -0.5, 0.5, 0.01, 0.0, 2 },
        });
        addEffectRow("gaussianBlur",
                     "Blur > Gaussian Blur", "pr.effect.gaussianBlur", {
            { "radius", "Radius (px)", "pr.effect.param.gaussianBlur.radius", 0.0, 50.0, 0.5, 8.0, 1 },
        });
        addEffectRow("boxBlur",
                     "Blur > Box Blur", "pr.effect.boxBlur", {
            { "radius", "Radius (px)", "pr.effect.param.boxBlur.radius", 0.0, 50.0, 0.5, 8.0, 1 },
        });
        addEffectRow("unsharpMask",
                     "Sharpen > Unsharp Mask", "pr.effect.unsharpMask", {
            { "amount", "Amount", "pr.effect.param.unsharpMask.amount", 0.0, 3.0, 0.05, 1.0, 2 },
            { "radius", "Radius (px)", "pr.effect.param.unsharpMask.radius", 0.0, 20.0, 0.5, 3.0, 1 },
        });
        addEffectRow("glow",
                     "Stylize > Glow", "pr.effect.glow", {
            { "intensity", "Intensity", "pr.effect.param.glow.intensity", 0.0, 2.0, 0.05, 0.8, 2 },
            { "radius", "Radius (px)", "pr.effect.param.glow.radius", 0.0, 50.0, 0.5, 12.0, 1 },
            { "threshold", "Threshold", "pr.effect.param.glow.threshold", 0.0, 1.0, 0.01, 0.6, 2 },
        });
        addEffectRow("posterize",
                     "Stylize > Posterize", "pr.effect.posterize", {
            { "levels", "Levels", "pr.effect.param.posterize.levels", 2.0, 32.0, 1.0, 8.0, 0 },
        });
        addEffectRow("scale",
                     "Transform > Scale", "pr.effect.scale", {
            { "percent", "Scale (%)", "pr.effect.param.scale.percent", 10.0, 400.0, 5.0, 100.0, 0 },
        });
        addEffectRow("rotate",
                     "Transform > Rotate", "pr.effect.rotate", {
            { "angle", "Angle (deg)", "pr.effect.param.rotate.angle", -180.0, 180.0, 1.0, 0.0, 1 },
        });
        addEffectRow("audioGain",
                     "Audio > Gain", "pr.effect.audioGain", {
            { "gainDb", "Gain (dB)", "pr.effect.param.audioGain.gainDb", -24.0, 24.0, 0.5, 0.0, 1 },
        });
        addEffectRow("audioEqualizer",
                     "Audio > Equalizer", "pr.effect.audioEqualizer", {
            { "lowDb", "Low (200Hz, dB)", "pr.effect.param.audioEqualizer.lowDb", -12.0, 12.0, 0.5, 0.0, 1 },
            { "midDb", "Mid (1kHz, dB)", "pr.effect.param.audioEqualizer.midDb", -12.0, 12.0, 0.5, 0.0, 1 },
            { "highDb", "High (4kHz, dB)", "pr.effect.param.audioEqualizer.highDb", -12.0, 12.0, 0.5, 0.0, 1 },
        });
    }

    void addEffectRow(const char* effectId, const char* labelEn, const char* labelKey,
                      const QVector<ParamSpec>& params)
    {
        EffectRow row;
        row.effectId = QString::fromLatin1(effectId);

        auto* rowWidget = new QWidget();
        auto* rowLayout = new QVBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(2);

        row.enableCheck = new QCheckBox(trText(labelKey, labelEn));
        row.enableCheck->setEnabled(false);
        rowLayout->addWidget(row.enableCheck);

        row.paramsWidget = new QWidget();
        auto* paramGrid = new QGridLayout(row.paramsWidget);
        paramGrid->setContentsMargins(20, 0, 0, 0);
        paramGrid->setSpacing(2);
        int gridRow = 0;
        for (const ParamSpec& spec : params) {
            auto* paramLabel = new QLabel(trText(spec.labelKey, spec.labelEn));
            auto* spin = new QDoubleSpinBox();
            spin->setRange(spec.min, spec.max);
            spin->setSingleStep(spec.step);
            spin->setDecimals(spec.decimals);
            spin->setValue(spec.defaultValue);
            spin->setEnabled(false);
            {
                const QColor fromToken(ArtifactCore::currentDCCTheme().inputBackgroundColor);
                const QColor legacy = ArtifactPr::prLegacyColors().inputBackground;
                QPalette p = spin->palette();
                p.setColor(QPalette::Base, fromToken.isValid() ? fromToken : legacy);
                spin->setPalette(p);
            }
            paramGrid->addWidget(paramLabel, gridRow, 0);
            paramGrid->addWidget(spin, gridRow, 1);
            ++gridRow;

            ParamWidget pw;
            pw.key = QString::fromLatin1(spec.key);
            pw.spin = spin;
            pw.defaultValue = spec.defaultValue;
            row.params.append(pw);

            connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                    [this]() { applyEffectsToEngine(); });
        }
        rowLayout->addWidget(row.paramsWidget);
        row.paramsWidget->setVisible(false);

        connect(row.enableCheck, &QCheckBox::toggled, this,
                [this, row]() {
                    row.paramsWidget->setVisible(row.enableCheck->isChecked());
                    applyEffectsToEngine();
                });

        rowsLayout_->insertWidget(rowsLayout_->count() - 1, rowWidget);
        rows_.append(row);
    }

    void updateWidgetsFromClip()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(engine->selectedClipId());
        const bool hasClip = clip != nullptr;

        updating_ = true;
        for (const EffectRow& row : rows_) {
            row.enableCheck->setEnabled(hasClip);
            const bool enabled = hasClip
                && ArtifactPr::effectEnabled(clip->effects, row.effectId);
            row.enableCheck->setChecked(enabled);
            row.paramsWidget->setVisible(enabled);
            for (const ParamWidget& pw : row.params) {
                pw.spin->setEnabled(hasClip);
                pw.spin->setValue(hasClip
                    ? ArtifactPr::effectParam(clip->effects, row.effectId, pw.key,
                                              pw.defaultValue)
                    : pw.defaultValue);
            }
        }
        updating_ = false;

        infoLabel_->setText(hasClip
            ? trText("pr.panel.effects.editing", "Editing effects of the selected clip.")
            : trText("pr.panel.effects.select_clip", "Select a clip to edit its effects."));
        applySearchFilter();
    }

    void applyEffectsToEngine()
    {
        if (updating_) return;
        auto* engine = ArtifactPr::EditorEngine::instance();
        const QString clipId = engine->selectedClipId();
        if (!engine->findClip(clipId)) return;

        // fx.* キーを UI 状態から毎回再構築する (無効化エフェクトの残骸を残さない)。
        QMap<QString, QVariant> effects;
        for (const EffectRow& row : rows_) {
            if (!row.enableCheck->isChecked()) continue;
            effects.insert(QStringLiteral("fx.%1.enabled").arg(row.effectId),
                           QStringLiteral("1"));
            for (const ParamWidget& pw : row.params) {
                effects.insert(QStringLiteral("fx.%1.%2").arg(row.effectId, pw.key),
                               QString::number(pw.spin->value()));
            }
        }
        engine->setClipEffects(clipId, effects);
    }

    void applySearchFilter()
    {
        const QString filter = searchEdit_->text().trimmed();
        for (const EffectRow& row : rows_) {
            const bool visible = filter.isEmpty()
                || row.enableCheck->text().contains(filter, Qt::CaseInsensitive);
            row.enableCheck->parentWidget()->setVisible(visible);
        }
    }

private:
    QLineEdit* searchEdit_ = nullptr;
    QVBoxLayout* rowsLayout_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QVector<EffectRow> rows_;
    bool updating_ = false;
};

W_OBJECT_IMPL(EffectsPanel)

class ClipPropertiesPanel : public QWidget
{
    W_OBJECT(ClipPropertiesPanel)
public:
    explicit ClipPropertiesPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(8);

        auto* label = new QLabel(trText("pr.panel.clip_properties", "Clip Properties"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        clipNameLabel_ = new QLabel(trText("pr.clip.no_selection", "No clip selected"));
        layout->addWidget(clipNameLabel_);

        auto* volumeLabel = new QLabel(trText("pr.clip.volume", "Volume:"));
        layout->addWidget(volumeLabel);

        volumeSlider_ = new QSlider(Qt::Horizontal);
        volumeSlider_->setMinimum(0);
        volumeSlider_->setMaximum(200);
        volumeSlider_->setValue(100);
        volumeSlider_->setTickPosition(QSlider::TicksBelow);
        volumeSlider_->setTickInterval(25);
        connect(volumeSlider_, &QSlider::valueChanged, this, &ClipPropertiesPanel::onVolumeChanged);
        layout->addWidget(volumeSlider_);

        volumeValueLabel_ = new QLabel(percentLabel(100));
        volumeValueLabel_->setAlignment(Qt::AlignCenter);
        layout->addWidget(volumeValueLabel_);

        auto* speedLabel = new QLabel(trText("pr.clip.speed", "Speed:"));
        layout->addWidget(speedLabel);

        speedCombo_ = new QComboBox();
        speedCombo_->addItem(percentLabel(25), QVariant(0.25));
        speedCombo_->addItem(percentLabel(50), QVariant(0.5));
        speedCombo_->addItem(percentLabel(100), QVariant(1.0));
        speedCombo_->addItem(percentLabel(200), QVariant(2.0));
        speedCombo_->addItem(percentLabel(400), QVariant(4.0));
        speedCombo_->setCurrentIndex(2);
        {
            const QColor fromToken(ArtifactCore::currentDCCTheme().inputBackgroundColor);
            const QColor legacy = ArtifactPr::prLegacyColors().inputBackground;
            QPalette p = speedCombo_->palette();
            p.setColor(QPalette::Base, fromToken.isValid() ? fromToken : legacy);
            speedCombo_->setPalette(p);
        }
        connect(speedCombo_, &QComboBox::currentIndexChanged, this, &ClipPropertiesPanel::onSpeedChanged);
        layout->addWidget(speedCombo_);

        reverseCheck_ = new QCheckBox(trText("pr.action.reverse", "Reverse"));
        connect(reverseCheck_, &QCheckBox::toggled, this, &ClipPropertiesPanel::onReverseToggled);
        layout->addWidget(reverseCheck_);

        auto* opacityLabel = new QLabel(trText("pr.clip.opacity", "Opacity:"));
        layout->addWidget(opacityLabel);

        opacitySlider_ = new QSlider(Qt::Horizontal);
        opacitySlider_->setRange(0, 100);
        opacitySlider_->setValue(100);
        opacitySlider_->setTickPosition(QSlider::TicksBelow);
        opacitySlider_->setTickInterval(25);
        connect(opacitySlider_, &QSlider::valueChanged, this, &ClipPropertiesPanel::onOpacityChanged);
        layout->addWidget(opacitySlider_);

        opacityValueLabel_ = new QLabel(percentLabel(100));
        opacityValueLabel_->setAlignment(Qt::AlignCenter);
        layout->addWidget(opacityValueLabel_);

        // ---- V/A リンク編集 (NLE LinkingService) ----
        auto* linkHeader = new QLabel(trText("pr.clip.av_link", "AV Link"));
        QFont linkFont = linkHeader->font();
        linkFont.setBold(true);
        linkHeader->setFont(linkFont);
        layout->addWidget(linkHeader);

        auto* linkButtonRow = new QHBoxLayout();
        linkButton_ = new QPushButton(trText("pr.action.link", "Link"));
        linkButton_->setToolTip(trText(
            "pr.clip.link_tooltip",
            "Link the selected clip into an AV link group so trim, slide and "
            "selection propagate to its peers."));
        connect(linkButton_, &QPushButton::clicked, this, &ClipPropertiesPanel::onLinkClicked);
        linkButtonRow->addWidget(linkButton_);

        unlinkButton_ = new QPushButton(trText("pr.action.unlink", "Unlink"));
        unlinkButton_->setToolTip(trText("pr.clip.unlink_tooltip", "Remove the selected clip from its AV link group."));
        connect(unlinkButton_, &QPushButton::clicked, this, &ClipPropertiesPanel::onUnlinkClicked);
        linkButtonRow->addWidget(unlinkButton_);
        layout->addLayout(linkButtonRow);

        linkTrimCheck_ = new QCheckBox(trText("pr.clip.link_trim", "Link trim"));
        linkTrimCheck_->setToolTip(trText("pr.clip.link_trim_tooltip", "Propagate trim edits to linked peers."));
        linkTrimCheck_->setChecked(true);
        connect(linkTrimCheck_, &QCheckBox::toggled, this, &ClipPropertiesPanel::onLinkFlagToggled);
        layout->addWidget(linkTrimCheck_);

        linkSelectionCheck_ = new QCheckBox(trText("pr.clip.link_selection", "Link selection"));
        linkSelectionCheck_->setToolTip(trText("pr.clip.link_selection_tooltip", "Select linked peers together with this clip."));
        linkSelectionCheck_->setChecked(true);
        connect(linkSelectionCheck_, &QCheckBox::toggled, this, &ClipPropertiesPanel::onLinkFlagToggled);
        layout->addWidget(linkSelectionCheck_);

        linkStatusLabel_ = new QLabel();
        QFont linkStatusFont = linkStatusLabel_->font();
        linkStatusFont.setPointSize(qMax(6, linkStatusFont.pointSize() - 1));
        linkStatusLabel_->setFont(linkStatusFont);
        layout->addWidget(linkStatusLabel_);

        layout->addStretch();

        infoLabel_ = new QLabel(trText("pr.clip.select_to_edit", "Select a clip to\nedit its properties."));
        layout->addWidget(infoLabel_);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::clipSelectionChanged, this, &ClipPropertiesPanel::onClipSelected);
        connect(engine, &ArtifactPr::EditorEngine::clipChanged, this, &ClipPropertiesPanel::onClipChanged);
    }

private slots:
    void onClipSelected(const QString& clipId)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);

        if (!clip) {
            clipNameLabel_->setText(trText("pr.clip.no_selection", "No clip selected"));
            volumeSlider_->setValue(100);
            volumeValueLabel_->setText(percentLabel(100));
            speedCombo_->setCurrentIndex(2);
            reverseCheck_->setChecked(false);
            opacitySlider_->setValue(100);
            opacityValueLabel_->setText(percentLabel(100));
            infoLabel_->setText(trText("pr.clip.select_to_edit", "Select a clip to\nedit its properties."));
            refreshLinkState();
            return;
        }

        clipNameLabel_->setText(clip->name);
        int volumePercent = static_cast<int>(clip->volume * 100);
        volumeSlider_->setValue(volumePercent);
        volumeValueLabel_->setText(percentLabel(volumePercent));

        int speedIndex = speedCombo_->findData(QVariant(clip->speed));
        if (speedIndex >= 0) {
            speedCombo_->setCurrentIndex(speedIndex);
        }

        reverseCheck_->setChecked(clip->reversed);

        int opacityPercent = static_cast<int>(clip->opacity * 100);
        opacitySlider_->blockSignals(true);
        opacitySlider_->setValue(opacityPercent);
        opacitySlider_->blockSignals(false);
        opacityValueLabel_->setText(percentLabel(opacityPercent));

        QString info = trText("pr.clip.duration_info", "Duration: %1 frames\nStart: %2\nSource: %3-%4")
            .arg(clip->duration).arg(clip->startFrame).arg(clip->sourceIn).arg(clip->sourceOut);
        infoLabel_->setText(info);
        refreshLinkState();
    }

    void onClipChanged(const QString& clipId)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (clipId == engine->selectedClipId()) {
            onClipSelected(clipId);
        }
    }

    void onLinkClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (engine->selectedClipId().isEmpty()) {
            return;
        }
        engine->linkSelectedClips(/*videoAudioLinked=*/true,
                                  linkSelectionCheck_->isChecked(),
                                  linkTrimCheck_->isChecked());
        refreshLinkState();
    }

    void onUnlinkClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (engine->selectedClipId().isEmpty()) {
            return;
        }
        engine->unlinkSelectedClips();
        refreshLinkState();
    }

    void onLinkFlagToggled()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (engine->selectedClipLinkGroupId() == 0) {
            return;
        }
        const bool trim = linkTrimCheck_->isChecked();
        const bool selection = linkSelectionCheck_->isChecked();
        engine->setSelectedClipLinkFlags(/*videoAudioLinked=*/true,
                                         /*moveLinked=*/true,
                                         selection,
                                         trim);
    }

    /// リンク状態表示とボタンの有効/無効を、現在の選択に同期する。
    void refreshLinkState()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        const bool hasSelection = !engine->selectedClipId().isEmpty();
        linkButton_->setEnabled(hasSelection);
        unlinkButton_->setEnabled(hasSelection);
        linkTrimCheck_->setEnabled(hasSelection);
        linkSelectionCheck_->setEnabled(hasSelection);

        if (!hasSelection) {
            linkStatusLabel_->clear();
            return;
        }

        const quint64 groupId = engine->selectedClipLinkGroupId();
        if (groupId == 0) {
            linkStatusLabel_->setText(trText("pr.clip.not_linked", "Not linked"));
            return;
        }
        const QVector<QString> peers = engine->linkedPeerClipIds();
        linkStatusLabel_->setText(trText("pr.clip.linked_peers", "Linked to %1 peer(s)").arg(peers.size()));
    }

    void onVolumeChanged(int value)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        QString clipId = engine->selectedClipId();
        if (!clipId.isEmpty()) {
            auto* clip = engine->findClip(clipId);
            if (clip) {
                double oldVolume = clip->volume;
                double newVolume = value / 100.0;
                if (qFuzzyCompare(oldVolume, newVolume)) {
                    volumeValueLabel_->setText(percentLabel(value));
                    return;
                }
                // UndoCommand 経由で volume 変更を適用
                auto* cmd = new ArtifactPr::ClipPropertyCommand(
                    clipId,
                    ArtifactPr::ClipPropertyCommand::Kind::Volume,
                    oldVolume, newVolume);
                cmd->redo();
                engine->pushUndo(cmd);
                volumeValueLabel_->setText(percentLabel(value));
            }
        }
    }

    void onSpeedChanged(int index)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        QString clipId = engine->selectedClipId();
        if (!clipId.isEmpty()) {
            auto* clip = engine->findClip(clipId);
            if (clip) {
                double oldSpeed = clip->speed;
                double newSpeed = speedCombo_->itemData(index).toDouble();
                if (qFuzzyCompare(oldSpeed, newSpeed)) return;
                auto* cmd = new ArtifactPr::ClipPropertyCommand(
                    clipId,
                    ArtifactPr::ClipPropertyCommand::Kind::Speed,
                    oldSpeed, newSpeed);
                cmd->redo();
                engine->pushUndo(cmd);
            }
        }
    }

    void onReverseToggled(bool checked)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        QString clipId = engine->selectedClipId();
        if (!clipId.isEmpty()) {
            auto* clip = engine->findClip(clipId);
            if (clip) {
                bool oldReversed = clip->reversed;
                if (oldReversed == checked) return;
                auto* cmd = new ArtifactPr::ClipPropertyCommand(
                    clipId,
                    ArtifactPr::ClipPropertyCommand::Kind::Reverse,
                    oldReversed, checked);
                cmd->redo();
                engine->pushUndo(cmd);
            }
        }
    }

    void onOpacityChanged(int value)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        QString clipId = engine->selectedClipId();
        if (!clipId.isEmpty()) {
            auto* clip = engine->findClip(clipId);
            if (clip) {
                double oldOpacity = clip->opacity;
                double newOpacity = value / 100.0;
                if (qFuzzyCompare(oldOpacity, newOpacity)) {
                    opacityValueLabel_->setText(percentLabel(value));
                    return;
                }
                // NLE ストア経由 (Undo 可能) の opacity 変更
                engine->setClipOpacity(clipId, newOpacity);
            }
        }
        opacityValueLabel_->setText(percentLabel(value));
    }

private:
    QLabel* clipNameLabel_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* volumeValueLabel_ = nullptr;
    QComboBox* speedCombo_ = nullptr;
    QCheckBox* reverseCheck_ = nullptr;
    QSlider* opacitySlider_ = nullptr;
    QLabel* opacityValueLabel_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QPushButton* linkButton_ = nullptr;
    QPushButton* unlinkButton_ = nullptr;
    QCheckBox* linkTrimCheck_ = nullptr;
    QCheckBox* linkSelectionCheck_ = nullptr;
    QLabel* linkStatusLabel_ = nullptr;
};

W_OBJECT_IMPL(ClipPropertiesPanel)


} // namespace

namespace {

} // namespace

ArtifactPrMainWindow::ArtifactPrMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    ArtifactPr::loadSharedShortcuts();
    setWindowTitle(trText("pr.app.title", "ArtifactPr"));
    resize(1600, 980);

    auto* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    auto* engine = ArtifactPr::EditorEngine::instance();

    const auto confirmProjectSwitch = [this, engine]() -> bool {
        if (!projectDirty_) return true;

        const auto result = QMessageBox::question(
            this, trText("pr.dialog.unsaved.title", "Unsaved Changes"),
            trText("pr.dialog.unsaved.switch_project", "Save changes before switching projects?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (result == QMessageBox::Cancel) return false;
        if (result == QMessageBox::Discard) return true;

        QString filePath = engine->projectFilePath();
        if (filePath.isEmpty()) {
            filePath = QFileDialog::getSaveFileName(
                this, trText("pr.action.save_project", "Save Project"),
                engine->currentProject().name + QStringLiteral(".apr"),
                QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        }
        return !filePath.isEmpty() && engine->saveProject(filePath);
    };

    auto* fileMenu = menuBar->addMenu(trText("pr.menu.file", "File"));

    auto* newAction = fileMenu->addAction(trText("pr.action.new_project", "New Project"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, [this, engine, confirmProjectSwitch]() {
        if (!confirmProjectSwitch()) return;
        engine->newProject();
    });

    auto* openAction = fileMenu->addAction(trText("pr.action.open_project", "Open Project..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, [this, engine, confirmProjectSwitch]() {
        if (!confirmProjectSwitch()) return;
        QString filePath = QFileDialog::getOpenFileName(
            this, trText("pr.dialog.open_project.title", "Open Project"),
            QString(), QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        if (!filePath.isEmpty()) {
            engine->loadProject(filePath);
        }
    });

    auto* saveAction = fileMenu->addAction(trText("pr.action.save_project", "Save Project"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, [this, engine]() {
        QString filePath = engine->projectFilePath();
        if (filePath.isEmpty()) {
            filePath = QFileDialog::getSaveFileName(
                this, trText("pr.action.save_project", "Save Project"),
                engine->currentProject().name + QStringLiteral(".apr"),
                QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        }
        if (!filePath.isEmpty()) {
            engine->saveProject(filePath);
        }
    });

    auto* saveAsAction = fileMenu->addAction(trText("pr.action.save_project_as", "Save Project As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, [this, engine]() {
        QString filePath = QFileDialog::getSaveFileName(
            this, trText("pr.dialog.save_project_as.title", "Save Project As"),
            engine->currentProject().name + QStringLiteral(".apr"),
            QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        if (!filePath.isEmpty()) {
            engine->saveProject(filePath);
        }
    });

    fileMenu->addSeparator();
    auto* importMediaAction = fileMenu->addAction(trText("pr.action.import_media", "Import Media..."));
    connect(importMediaAction, &QAction::triggered, [this, engine]() {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, trText("pr.dialog.import_media.title", "Import Media"), QString(),
            QStringLiteral("Media Files (*.mp4 *.avi *.mov *.mkv *.mp3 *.wav *.aac *.flac);;Video Files (*.mp4 *.avi *.mov *.mkv);;Audio Files (*.mp3 *.wav *.aac *.flac);;All Files (*)"));
        for (const auto& file : files) {
            const QString suffix = QFileInfo(file).suffix().toLower();
            const bool isAudio = suffix == QStringLiteral("mp3")
                || suffix == QStringLiteral("wav")
                || suffix == QStringLiteral("aac")
                || suffix == QStringLiteral("flac");
            const QString mediaType = isAudio ? QStringLiteral("audio") : QStringLiteral("video");
            bool alreadyRegistered = false;
            for (const auto& media : engine->mediaPool()) {
                if (media.filePath == file && media.type == mediaType) {
                    alreadyRegistered = true;
                    break;
                }
            }
            if (!alreadyRegistered) {
                engine->addMediaToPool(file, QFileInfo(file).fileName(), mediaType);
            }
        }
        if (!files.isEmpty() && mediaPanel_) {
            mediaPanel_->refreshMediaList(engine->currentSequence());
        }
    });
    auto* exportAction = fileMenu->addAction(trText("pr.action.export", "Export..."));
    exportAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(exportAction, &QAction::triggered, this, &ArtifactPrMainWindow::onExportTriggered);
    fileMenu->addSeparator();
    fileMenu->addAction(trText("pr.action.exit", "Exit"), qApp, &QApplication::quit);

    auto* editMenu = menuBar->addMenu(trText("pr.menu.edit", "Edit"));

    auto* undoAction = editMenu->addAction(trText("pr.action.undo", "Undo"));
    connect(undoAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::undo);

    auto* redoAction = editMenu->addAction(trText("pr.action.redo", "Redo"));
    connect(redoAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::redo);

    editMenu->addSeparator();

    auto* deleteAction = editMenu->addAction(trText("pr.action.delete", "Delete"));
    connect(deleteAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::deleteSelectedClip);

    auto* rippleDeleteAction = editMenu->addAction(trText("pr.action.ripple_delete", "Ripple Delete"));
    connect(rippleDeleteAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::rippleDeleteSelectedClip);

    auto* duplicateAction = editMenu->addAction(trText("pr.action.duplicate", "Duplicate"));
    duplicateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(duplicateAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::duplicateSelectedClip);

    editMenu->addSeparator();

    // The editor already owns clipboard semantics and undo integration. Keep
    // the menu actions as thin commands so keyboard, menu, and future context
    // menu entry points all follow the same edit path.
    auto* cutAction = editMenu->addAction(trText("pr.action.cut", "Cut"));
    connect(cutAction, &QAction::triggered, [engine]() {
        const QString clipId = engine->selectedClipId();
        if (clipId.isEmpty()) return;
        engine->copyClip(clipId);
        engine->deleteSelectedClip();
    });

    auto* copyAction = editMenu->addAction(trText("pr.action.copy", "Copy"));
    connect(copyAction, &QAction::triggered, [engine]() {
        const QString clipId = engine->selectedClipId();
        if (!clipId.isEmpty()) engine->copyClip(clipId);
    });

    auto* pasteAction = editMenu->addAction(trText("pr.action.paste", "Paste"));
    connect(pasteAction, &QAction::triggered, [engine]() {
        if (engine->hasClipboard()) engine->pasteClip(engine->currentFrame());
    });

    auto* snapAction = editMenu->addAction(trText("pr.action.snap_to_clips", "Snap to Clips"));
    snapAction->setCheckable(true);
    snapAction->setChecked(true);
    connect(snapAction, &QAction::triggered, [engine](bool checked) {
        engine->setSnapEnabled(checked);
    });

    auto* sequenceMenu = menuBar->addMenu(trText("pr.menu.sequence", "Sequence"));
    auto* bladeAction = sequenceMenu->addAction(trText("pr.action.blade_at_playhead", "Blade at Playhead"));
    connect(bladeAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::splitClipAtPlayhead);

    auto* addTransitionMenu = sequenceMenu->addMenu(trText("pr.action.add_transition", "Add Transition"));
    addTransitionMenu->addAction(trText("pr.transition.crossfade", "Crossfade"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::Crossfade);
    });
    addTransitionMenu->addAction(trText("pr.transition.dip_to_black", "Dip to Black"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::DipToBlack);
    });
    addTransitionMenu->addAction(trText("pr.transition.wipe_left", "Wipe Left"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeLeft);
    });
    addTransitionMenu->addAction(trText("pr.transition.wipe_right", "Wipe Right"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeRight);
    });

    auto* newSequenceAction = sequenceMenu->addAction(trText("pr.action.new_sequence", "New Sequence"));
    newSequenceAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    connect(newSequenceAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::newSequence);
    auto* switchSequenceAction = sequenceMenu->addAction(trText("pr.action.switch_sequence", "Switch Sequence..."));
    connect(switchSequenceAction, &QAction::triggered, this, [this, engine]() {
        const auto sequences = engine->currentProject().sequences;
        if (sequences.isEmpty()) return;

        QStringList names;
        int currentIndex = 0;
        for (int i = 0; i < sequences.size(); ++i) {
            names.append(sequences[i].name);
            if (sequences[i].id == engine->currentSequence().id) currentIndex = i;
        }

        bool accepted = false;
        const QString selected = QInputDialog::getItem(
            this, trText("pr.dialog.switch_sequence.title", "Switch Sequence"),
            trText("pr.menu.sequence", "Sequence"), names, currentIndex, false, &accepted);
        if (!accepted) return;
        const int selectedIndex = names.indexOf(selected);
        if (selectedIndex >= 0) engine->selectSequence(sequences[selectedIndex].id);
    });
    auto* sequenceSettingsAction = sequenceMenu->addAction(trText("pr.action.sequence_settings", "Sequence Settings..."));
    connect(sequenceSettingsAction, &QAction::triggered, this, [this, engine]() {
        auto sequence = engine->currentSequence();
        bool accepted = false;
        const QString name = QInputDialog::getText(
            this, trText("pr.dialog.sequence_settings.title", "Sequence Settings"),
            trText("pr.field.name", "Name"), QLineEdit::Normal, sequence.name, &accepted);
        if (!accepted) return;

        const QString resolution = QInputDialog::getText(
            this, trText("pr.dialog.sequence_settings.title", "Sequence Settings"),
            trText("pr.field.resolution", "Resolution"), QLineEdit::Normal, sequence.resolution, &accepted);
        if (!accepted) return;

        const QString frameRate = QInputDialog::getText(
            this, trText("pr.dialog.sequence_settings.title", "Sequence Settings"),
            trText("pr.field.frame_rate", "Frame Rate"), QLineEdit::Normal, sequence.frameRate, &accepted);
        if (accepted) {
            engine->updateSequenceSettings(name, resolution, frameRate);
        }
    });

    auto* markerMenu = menuBar->addMenu(trText("pr.menu.marker", "Marker"));
    auto* addMarkerAction = markerMenu->addAction(trText("pr.action.add_marker_at_playhead", "Add Marker at Playhead"));
    connect(addMarkerAction, &QAction::triggered, [engine]() {
        engine->addMarker(engine->currentFrame());
    });
    auto* clearMarkersAction = markerMenu->addAction(trText("pr.action.clear_all_markers", "Clear All Markers"));
    connect(clearMarkersAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::clearMarkers);

    auto* helpMenu = menuBar->addMenu(trText("pr.menu.help", "Help"));
    auto* shortcutsAction = helpMenu->addAction(trText("pr.action.keyboard_shortcuts", "Keyboard Shortcuts..."));
    connect(shortcutsAction, &QAction::triggered, this, [this]() {
        if (!helpDialog_) {
            helpDialog_ = new ArtifactPr::ShortcutHelpDialog(this);
            helpDialog_->setRegistry(shortcutRegistry_);
        }
        helpDialog_->show();
        helpDialog_->raise();
        helpDialog_->activateWindow();
    });

    // QtAdvancedDocking を撤去し、共有の dock surface を使う。
    auto* dockSurface = new Dock::NativeDockSurface(qApp->palette());
    setCentralWidget(dockSurface);

    auto* projectPanel = new ProjectPanel();
    dockSurface->addDockWidget(QStringLiteral("project"), QStringLiteral("Project"),
                               projectPanel, Dock::DockArea::Left);

    mediaPanel_ = new MediaPanel();
    auto* mediaPanel = mediaPanel_;
    dockSurface->addDockWidgetToTab(QStringLiteral("media"), QStringLiteral("Media"),
                                    mediaPanel, QStringLiteral("project"));

    auto* sourceMonitorPanel = new SourceMonitorPanel();
    dockSurface->addDockWidget(QStringLiteral("sourceMonitor"),
                               QStringLiteral("Source Monitor"),
                               sourceMonitorPanel, Dock::DockArea::Right);

    connect(mediaPanel, &MediaPanel::mediaSelected, sourceMonitorPanel, &SourceMonitorPanel::loadMedia);

    auto* programMonitorPanel = new ProgramMonitorPanel();
    dockSurface->addDockWidgetToTab(QStringLiteral("programMonitor"),
                                    QStringLiteral("Program Monitor"),
                                    programMonitorPanel, QStringLiteral("sourceMonitor"));

    // Create the timeline before wiring the program monitor's waveform provider.
    auto* timelinePanel = new ArtifactPrTimelinePanel();
    dockSurface->addDockWidget(QStringLiteral("timeline"), QStringLiteral("Timeline"),
                               timelinePanel, Dock::DockArea::Bottom);

    auto* audioMeterPanel = new AudioMeterPanel();
    dockSurface->addDockWidget(QStringLiteral("audioMeters"),
                               trText("pr.audio.meters", "Audio Meters"),
                               audioMeterPanel, Dock::DockArea::Right);

    programMonitorPanel->setMeterPanel(audioMeterPanel);
    timelinePanel->setWaveformProvider(programMonitorPanel->waveformProvider());

    auto* transitionPanel = new TransitionPanel();
    dockSurface->addDockWidget(QStringLiteral("transitions"),
                               trText("pr.panel.transitions.title", "Transitions"),
                               transitionPanel, Dock::DockArea::Right);

    auto* effectsPanel = new EffectsPanel();
    dockSurface->addDockWidget(QStringLiteral("effects"),
                               trText("pr.panel.effects.title", "Effects"),
                               effectsPanel, Dock::DockArea::Right);

    auto* proxyPanel = new ProxyPanel();
    dockSurface->addDockWidget(QStringLiteral("proxy"), QStringLiteral("Proxy"),
                               proxyPanel, Dock::DockArea::Right);

    auto* clipPropsPanel = new ClipPropertiesPanel();
    dockSurface->addDockWidget(QStringLiteral("clipProperties"),
                               QStringLiteral("Clip Properties"),
                               clipPropsPanel, Dock::DockArea::Right);

    dockSurface_ = dockSurface;

    // 10 パネルをすべて登録し終えてからレイアウトを復元する。
    // 先に復元すると、未登録の dock が drop される。
    restoreDockLayout();

    connect(this, &ArtifactPrMainWindow::requestZoomIn, [timelinePanel]() {
        auto* slider = timelinePanel->findChild<QSlider*>("zoomSlider_");
        if (slider) slider->setValue(qMin(10, slider->value() + 1));
    });
    connect(this, &ArtifactPrMainWindow::requestZoomOut, [timelinePanel]() {
        auto* slider = timelinePanel->findChild<QSlider*>("zoomSlider_");
        if (slider) slider->setValue(qMax(1, slider->value() - 1));
    });

    transportBar_ = new TransportBarWidget();
    auto* statusBarWidget = new QStatusBar();
    setStatusBar(statusBarWidget);
    statusBarWidget->addPermanentWidget(transportBar_, 1);
    statusBarWidget->showMessage(trText("pr.status.ready", "Ready - Keyboard Shortcuts are configurable from Help"));

    // status notifier: 編集中の操作を 3 秒間表示
    connect(&statusNotifier_, &ArtifactPr::PrStatusNotifier::temporaryMessage,
            statusBarWidget, [statusBarWidget](const QString& msg, int timeoutMs) {
        statusBarWidget->showMessage(msg, timeoutMs);
    });
    connect(&statusNotifier_, &ArtifactPr::PrStatusNotifier::helpRequested,
            this, [this]() {
                if (!helpDialog_) {
                    helpDialog_ = new ArtifactPr::ShortcutHelpDialog(this);
                    helpDialog_->setRegistry(shortcutRegistry_);
                }
                helpDialog_->show();
                helpDialog_->raise();
                helpDialog_->activateWindow();
            });

    // EditorEngine からの project 変更 / undo / redo を status に転送
    connect(ArtifactPr::EditorEngine::instance(), &ArtifactPr::EditorEngine::projectModified,
            this, &ArtifactPrMainWindow::onProjectModified);
    connect(ArtifactPr::EditorEngine::instance(), &ArtifactPr::EditorEngine::projectSaved,
            this, [this](bool success, const QString&) {
        auto* editor = ArtifactPr::EditorEngine::instance();
        if (success && !editor->isAutoSaveInProgress()) {
            projectDirty_ = false;
            setWindowTitle(trText("pr.app.title", "ArtifactPr"));
        }
    });
    connect(ArtifactPr::EditorEngine::instance(), &ArtifactPr::EditorEngine::projectLoaded,
            this, [this](bool success, const QString&) {
        if (success) {
            projectDirty_ = false;
            setWindowTitle(trText("pr.app.title", "ArtifactPr"));
        }
    });
    // undo / redo は EditorEngine の signal に直接接続 (slot 経由でなく connect で十分)
    connect(ArtifactPr::EditorEngine::instance(), &ArtifactPr::EditorEngine::sequenceChanged,
            this, [this](const ArtifactPr::DemoSequence& sequence) {
        statusNotifier_.notify(QStringLiteral("Sequence modified"));
        if (timecodeOverlay_) {
            timecodeOverlay_->setFps(sequenceFrameRate(sequence));
        }
    });

    // Auto-save: 60 秒ごとに現在の project を temp ファイルに保存
    autoSaveTimer_ = new QTimer(this);
    autoSaveTimer_->setInterval(60 * 1000);  // 60 秒
    auto* engine = ArtifactPr::EditorEngine::instance();
    engine->setAutoSaveEnabled(true);
    engine->setAutoSaveIntervalSec(60);
    // デフォルトの autoSave 先は Documents / artifactpr_autosave.apr
    const QString autoSavePath = QDir::homePath() + QStringLiteral("/artifactpr_autosave.apr");
    engine->setAutoSaveFilePath(autoSavePath);
    connect(autoSaveTimer_, &QTimer::timeout, this, [engine]() {
        engine->runAutoSave();
    });
    autoSaveTimer_->start();

    // Timecode overlay (右上に黄色 timecode)
    timecodeOverlay_ = new ArtifactPr::TimecodeOverlayWidget(this);
    timecodeOverlay_->setFps(sequenceFrameRate(engine->currentSequence()));
    timecodeOverlay_->move(width() - timecodeOverlay_->width() - 8, 8);
    timecodeOverlay_->raise();
    timecodeOverlay_->show();
    connect(ArtifactPr::EditorEngine::instance(), &ArtifactPr::EditorEngine::currentFrameChanged,
            this, [this](ArtifactPr::FramePosition frame) {
                timecodeOverlay_->setCurrentFrame(static_cast<int>(frame));
            });

    // KDDockWidgets 個別の setStyleSheet は使用禁止。
    // Dock の背景色は PrProxyStyle + DCC theme (backgroundColor) が
    // QProxyStyle::polish() 経由で自動的に適用する。
}

void ArtifactPrMainWindow::keyPressEvent(QKeyEvent* event)
{
    auto* engine = ArtifactPr::EditorEngine::instance();

    // PrShortcutRegistry ベース dispatch。
    // QKeySequence(QKeyCombination) で構築し、registry 内の keys 文字列と
    // 完全一致するエントリを探す。autoRepeat は常に無視。
    if (event->isAutoRepeat()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    // Shared configurable bindings take precedence over the legacy local
    // registry. This makes changes made in Artifact's shortcut settings work
    // in ArtifactPr without duplicating persistence or conflict logic.
    const auto dispatchShared = [&](const QString& actionName) -> bool {
        if (!ArtifactPr::matchesSharedShortcut(event, actionName)) return false;
        if (actionName == QStringLiteral("undo")) engine->undo();
        else if (actionName == QStringLiteral("redo")) engine->redo();
        else if (actionName == QStringLiteral("togglePlayPause")) engine->togglePlayPause();
        else if (actionName == QStringLiteral("splitClip")) engine->splitClipAtPlayhead();
        else if (actionName == QStringLiteral("deleteClip")) engine->deleteSelectedClip();
        else if (actionName == QStringLiteral("addMarker")) engine->addMarker(engine->currentFrame());
        else if (actionName == QStringLiteral("zoomIn")) Q_EMIT requestZoomIn();
        else if (actionName == QStringLiteral("zoomOut")) Q_EMIT requestZoomOut();
        else if (actionName == QStringLiteral("copyClip") && !engine->selectedClipId().isEmpty())
            engine->copyClip(engine->selectedClipId());
        else if (actionName == QStringLiteral("cutClip") && !engine->selectedClipId().isEmpty())
            engine->cutClip(engine->selectedClipId());
        else if (actionName == QStringLiteral("pasteClip")) engine->pasteClip(engine->currentFrame());
        else if (actionName == QStringLiteral("slipClip") && !engine->selectedClipId().isEmpty())
            engine->slipClip(engine->selectedClipId(), 5);
        else if (actionName == QStringLiteral("slideClip") && !engine->selectedClipId().isEmpty())
            engine->slideClip(engine->selectedClipId(), 5);
        else if (actionName == QStringLiteral("shuttleReverse")) engine->shuttleReverse();
        else if (actionName == QStringLiteral("shuttleForward")) engine->shuttleForward();
        else if (actionName == QStringLiteral("setInPoint")) engine->setInPoint(engine->currentFrame());
        else if (actionName == QStringLiteral("setOutPoint")) engine->setOutPoint(engine->currentFrame());
        else if (actionName == QStringLiteral("pause")) engine->pause();
        else if (actionName == QStringLiteral("seekToStart")) engine->seekToFrame(0);
        else if (actionName == QStringLiteral("seekToEnd")) engine->seekToFrame(engine->currentSequence().duration);
        else if (actionName == QStringLiteral("rippleDeleteClip") && !engine->selectedClipId().isEmpty())
            engine->rippleDeleteClipAt(engine->selectedClipId());
        else if (actionName == QStringLiteral("addCrossfade")) engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::Crossfade);
        else if (actionName == QStringLiteral("addDipToBlack")) engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::DipToBlack);
        else if (actionName == QStringLiteral("addWipeLeft")) engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeLeft);
        else if (actionName == QStringLiteral("addWipeRight")) engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeRight);
        else if (actionName == QStringLiteral("showHelp")) {
            if (!helpDialog_) helpDialog_ = new ArtifactPr::ShortcutHelpDialog(this);
            helpDialog_->setRegistry(shortcutRegistry_);
            helpDialog_->show();
            helpDialog_->raise();
            helpDialog_->activateWindow();
        }
        else if (actionName == QStringLiteral("zoomInAlt")) Q_EMIT requestZoomIn();
        return true;
    };
    for (const auto& actionName : {QStringLiteral("undo"), QStringLiteral("redo"),
                                   QStringLiteral("togglePlayPause"), QStringLiteral("splitClip"),
                                   QStringLiteral("deleteClip"), QStringLiteral("addMarker"),
                                   QStringLiteral("zoomIn"), QStringLiteral("zoomOut"),
                                   QStringLiteral("copyClip"), QStringLiteral("cutClip"),
                                   QStringLiteral("pasteClip"), QStringLiteral("slipClip"),
                                   QStringLiteral("slideClip"), QStringLiteral("shuttleReverse"),
                                   QStringLiteral("shuttleForward"), QStringLiteral("setInPoint"),
                                   QStringLiteral("setOutPoint"), QStringLiteral("pause"),
                                   QStringLiteral("seekToStart"), QStringLiteral("seekToEnd"),
                                   QStringLiteral("rippleDeleteClip"), QStringLiteral("addCrossfade"),
                                   QStringLiteral("addDipToBlack"), QStringLiteral("addWipeLeft"),
                                   QStringLiteral("addWipeRight"), QStringLiteral("showHelp"),
                                   QStringLiteral("zoomInAlt")}) {
        if (dispatchShared(actionName)) {
            event->accept();
            return;
        }
    }

    const auto keyString = QKeySequence(event->key() | event->modifiers()).toString();

    const auto shortcuts = shortcutRegistry_.resolved();
    bool handled = false;
    for (const auto& sc : shortcuts) {
        if (sc.keys == keyString) {
            handled = true;
            if (sc.name == QStringLiteral("togglePlayPause")) engine->togglePlayPause();
            else if (sc.name == QStringLiteral("pause")) engine->pause();
            else if (sc.name == QStringLiteral("shuttleReverse")) engine->shuttleReverse();
            else if (sc.name == QStringLiteral("shuttleForward")) engine->shuttleForward();
            else if (sc.name == QStringLiteral("seekToStart")) engine->seekToFrame(0);
            else if (sc.name == QStringLiteral("seekToEnd")) engine->seekToFrame(engine->currentSequence().duration);
            else if (sc.name == QStringLiteral("splitClip")) engine->splitClipAtPlayhead();
            else if (sc.name == QStringLiteral("copyClip") && !engine->selectedClipId().isEmpty())
                engine->copyClip(engine->selectedClipId());
            else if (sc.name == QStringLiteral("cutClip") && !engine->selectedClipId().isEmpty())
                engine->cutClip(engine->selectedClipId());
            else if (sc.name == QStringLiteral("pasteClip"))
                engine->pasteClip(engine->currentFrame());
            else if (sc.name == QStringLiteral("undo")) engine->undo();
            else if (sc.name == QStringLiteral("redo")) engine->redo();
            else if (sc.name == QStringLiteral("slipClip") && !engine->selectedClipId().isEmpty())
                engine->slipClip(engine->selectedClipId(), 5);
            else if (sc.name == QStringLiteral("slideClip") && !engine->selectedClipId().isEmpty())
                engine->slideClip(engine->selectedClipId(), 5);
            else if (sc.name == QStringLiteral("deleteClip") && !engine->selectedClipId().isEmpty())
                engine->deleteSelectedClip();
            else if (sc.name == QStringLiteral("rippleDeleteClip") && !engine->selectedClipId().isEmpty())
                engine->rippleDeleteClipAt(engine->selectedClipId());
            else if (sc.name == QStringLiteral("addCrossfade"))
                engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::Crossfade);
            else if (sc.name == QStringLiteral("addDipToBlack"))
                engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::DipToBlack);
            else if (sc.name == QStringLiteral("addWipeLeft"))
                engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeLeft);
            else if (sc.name == QStringLiteral("addWipeRight"))
                engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeRight);
            else if (sc.name == QStringLiteral("setInPoint")) engine->setInPoint(engine->currentFrame());
            else if (sc.name == QStringLiteral("setOutPoint")) engine->setOutPoint(engine->currentFrame());
            else if (sc.name == QStringLiteral("addMarker")) engine->addMarker(engine->currentFrame());
            else if (sc.name == QStringLiteral("zoomIn") || sc.name == QStringLiteral("zoomInAlt"))
                Q_EMIT requestZoomIn();
            else if (sc.name == QStringLiteral("zoomOut"))
                Q_EMIT requestZoomOut();
            else if (sc.name == QStringLiteral("showHelp")) {
                if (!helpDialog_) {
                    helpDialog_ = new ArtifactPr::ShortcutHelpDialog(this);
                    helpDialog_->setRegistry(shortcutRegistry_);
                }
                helpDialog_->show();
                helpDialog_->raise();
                helpDialog_->activateWindow();
            }
            else
                handled = false;
            break;
        }
    }

    if (!handled) {
        QMainWindow::keyPressEvent(event);
    }
}

void ArtifactPrMainWindow::onExportTriggered()
{
    ExportDialog dialog(this);
    dialog.exec();
}

void ArtifactPrMainWindow::onProjectModified()
{
    if (projectDirty_) return;
    projectDirty_ = true;
    setWindowTitle(trText("pr.app.title_modified", "ArtifactPr*"));
}

void ArtifactPrMainWindow::onUndoRedo()
{
    onProjectModified();
}

void ArtifactPrMainWindow::closeEvent(QCloseEvent* event)
{
    // レイアウトは未保存のプロジェクトとは独立なので、dirty に関係なく
    // 保存する。QADS 版は saveState() を呼んでいたが、TODO 扱いで
    // 永続化されていなかった。
    saveDockLayout();

    if (!projectDirty_) {
        event->accept();
        return;
    }

    const auto result = QMessageBox::question(
        this, trText("pr.dialog.unsaved.title", "Unsaved Changes"),
        trText("pr.dialog.unsaved.close", "Save changes before closing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (result == QMessageBox::Cancel) {
        event->ignore();
        return;
    }
    if (result == QMessageBox::Discard) {
        event->accept();
        return;
    }

    auto* engine = ArtifactPr::EditorEngine::instance();
    QString filePath = engine->projectFilePath();
    if (filePath.isEmpty()) {
        filePath = QFileDialog::getSaveFileName(
            this, trText("pr.action.save_project", "Save Project"),
            engine->currentProject().name + QStringLiteral(".apr"),
            QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
    }
    if (filePath.isEmpty() || !engine->saveProject(filePath)) {
        event->ignore();
        return;
    }
    event->accept();
}

void ArtifactPrMainWindow::saveDockLayout()
{
    if (!dockSurface_) {
        return;
    }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("ArtifactStudio"), QStringLiteral("ArtifactPr"));
    settings.setValue(QStringLiteral("dock/layout"),
                      QString::fromLatin1(dockSurface_->saveLayoutState()));
    settings.setValue(QStringLiteral("dock/geometry"), saveGeometry());
}

void ArtifactPrMainWindow::restoreDockLayout()
{
    if (!dockSurface_) {
        return;
    }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("ArtifactStudio"), QStringLiteral("ArtifactPr"));
    // ウィンドウの幾何は dock より先に復元する。DockSurface が
    // splitter サイズを確定させるのに親のサイズが必要。
    const QByteArray geometry =
        settings.value(QStringLiteral("dock/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray layout =
        settings.value(QStringLiteral("dock/layout")).toString().toLatin1();
    if (!layout.isEmpty()) {
        dockSurface_->restoreLayoutState(layout);
    }
}

W_OBJECT_IMPL(ArtifactPrMainWindow)
