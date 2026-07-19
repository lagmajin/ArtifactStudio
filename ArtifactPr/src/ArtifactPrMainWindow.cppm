module;

#include <wobjectdefs.h>
#include <wobjectimpl.h>
#include <DockManager.h>
#include <DockWidget.h>

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLocale>
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
#include <QSplitter>
#include <QStatusBar>
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

module ArtifactPr.MainWindow;

import ArtifactPr.EditorEngine;
import ArtifactPr.ExportDialog;
import ArtifactPr.MediaPanel;
import ArtifactPr.MediaThumbnailer;
import ArtifactPr.ProjectPanel;
import ArtifactPr.TransportBarWidget;
import ArtifactPr.VideoPlayerWidget;
import ArtifactPr.AppTheme;

namespace {

using FramePosition = ArtifactPr::FramePosition;

const int FRAME_WIDTH = 2;
const int MIN_CLIP_WIDTH = 20;
const int TRIM_HANDLE_WIDTH = 6;

bool isJapaneseSystemLocale()
{
    return QLocale::system().language() == QLocale::Japanese;
}

QString uiText(const char* english, const char* japanese)
{
    return isJapaneseSystemLocale() ? QString::fromUtf8(japanese) : QString::fromUtf8(english);
}

QString percentLabel(int value)
{
    return QStringLiteral("%1%").arg(value);
}

QString trUi(const char* english, const char* japanese)
{
    return isJapaneseSystemLocale() ? QString::fromUtf8(japanese) : QString::fromUtf8(english);
}

class TimelineClipWidget : public QWidget
{
    W_OBJECT(TimelineClipWidget)
public:
    TimelineClipWidget(const QString& clipId, const QString& name, const QString& color, bool selected, bool isAudio, QWidget* parent = nullptr)
        : QWidget(parent), clipId_(clipId), name_(name), color_(color), selected_(selected), isAudio_(isAudio)
    {
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setCursor(Qt::SizeHorCursor);
        setAttribute(Qt::WA_Hover);

        if (isAudio_) {
            generateWaveform();
        }
    }

    QString clipId() const { return clipId_; }
    void setSelected(bool s) { selected_ = s; update(); }

    void setSpeed(double speed) { speed_ = speed; }
    void setReversed(bool reversed) { reversed_ = reversed; }

signals:
    void clipSelected(const QString& clipId) W_SIGNAL(clipSelected, clipId);
    void clipMoved(const QString& clipId, int deltaX) W_SIGNAL(clipMoved, clipId, deltaX);
    void clipTrimLeft(const QString& clipId, int deltaX) W_SIGNAL(clipTrimLeft, clipId, deltaX);
    void clipTrimRight(const QString& clipId, int deltaX) W_SIGNAL(clipTrimRight, clipId, deltaX);
    void clipRightClicked(const QString& clipId, const QPoint& pos) W_SIGNAL(clipRightClicked, clipId, pos);

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRect r = rect().adjusted(1, 1, -1, -1);

        QColor baseColor(color_);
        QColor bgColor = selected_ ? baseColor.lighter(120) : baseColor;
        bgColor.setAlpha(220);
        p.setBrush(bgColor);

        QColor borderColor = selected_ ? Qt::white : baseColor.darker(140);
        p.setPen(QPen(borderColor, selected_ ? 2 : 1));
        p.drawRoundedRect(r, 3, 3);

        if (selected_) {
            QColor highlightColor(255, 255, 255, 60);
            p.fillRect(r.adjusted(2, 2, -2, -2), highlightColor);
        }

        if (isAudio_ && !waveform_.isEmpty()) {
            drawWaveform(p, r);
        }

        p.setPen(selected_ ? Qt::white : Qt::white);
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);

        QString displayName = name_;
        if (width() < 60) {
            displayName = name_.left(3);
        } else if (width() < 100) {
            displayName = name_.left(6);
        }
        p.drawText(r.adjusted(6, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter, displayName);
    }

private:
    void generateWaveform()
    {
        waveform_.clear();
        int sampleCount = 100;
        for (int i = 0; i < sampleCount; ++i) {
            float amplitude = 0.3f + 0.7f * static_cast<float>(QRandomGenerator::global()->generateDouble());
            waveform_.append(amplitude);
        }
    }

    void drawWaveform(QPainter& p, const QRect& r)
    {
        QColor waveColor(255, 255, 255, 120);
        p.setPen(waveColor);

        int centerY = r.center().y();
        int waveHeight = r.height() * 0.6f;

        QPainterPath path;
        path.moveTo(r.left(), centerY);

        for (int i = 0; i < waveform_.size(); ++i) {
            float x = r.left() + (static_cast<float>(i) / waveform_.size()) * r.width();
            float y = centerY - waveform_[i] * waveHeight * 0.5f;
            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }

        for (int i = waveform_.size() - 1; i >= 0; --i) {
            float x = r.left() + (static_cast<float>(i) / waveform_.size()) * r.width();
            float y = centerY + waveform_[i] * waveHeight * 0.5f;
            path.lineTo(x, y);
        }
        path.closeSubpath();

        QColor fillColor(200, 200, 200, 40);
        p.setBrush(fillColor);
        p.drawPath(path);

        p.setPen(QColor(255, 255, 255, 80));
        p.setBrush(Qt::NoBrush);

        for (int i = 0; i < waveform_.size(); ++i) {
            float x = r.left() + (static_cast<float>(i) / waveform_.size()) * r.width();
            float yTop = centerY - waveform_[i] * waveHeight * 0.5f;
            float yBottom = centerY + waveform_[i] * waveHeight * 0.5f;
            p.drawLine(QPointF(x, yTop), QPointF(x, yBottom));
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::RightButton) {
            Q_EMIT clipRightClicked(clipId_, event->globalPos());
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            dragStartPos_ = event->pos();
            isDragging_ = false;
            isTrimmingLeft_ = false;
            isTrimmingRight_ = false;

            if (event->pos().x() < TRIM_HANDLE_WIDTH) {
                isTrimmingLeft_ = true;
            } else if (event->pos().x() > width() - TRIM_HANDLE_WIDTH) {
                isTrimmingRight_ = true;
            }

            Q_EMIT clipSelected(clipId_);
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event->buttons() & Qt::LeftButton) {
            if (!isTrimmingLeft_ && !isTrimmingRight_) {
                int diff = event->pos().x() - dragStartPos_.x();
                if (!isDragging_ && qAbs(diff) > 5) {
                    isDragging_ = true;
                }
                if (isDragging_) {
                    Q_EMIT clipMoved(clipId_, diff);
                    dragStartPos_ = event->pos();
                }
            } else {
                int diff = event->pos().x() - dragStartPos_.x();
                if (qAbs(diff) > 2) {
                    if (isTrimmingLeft_) {
                        Q_EMIT clipTrimLeft(clipId_, diff);
                    } else {
                        Q_EMIT clipTrimRight(clipId_, diff);
                    }
                    dragStartPos_ = event->pos();
                }
            }
        }
    }

    QString clipId_;
    QString name_;
    QString color_;
    bool selected_;
    bool isAudio_;
    QVector<float> waveform_;
    double speed_ = 1.0;
    bool reversed_ = false;
    QPoint dragStartPos_;
    bool isDragging_;
    bool isTrimmingLeft_;
    bool isTrimmingRight_;
};

W_OBJECT_IMPL(TimelineClipWidget)

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

        auto* label = new QLabel(QStringLiteral("Proxy Settings"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        auto* descLabel = new QLabel(QStringLiteral("Manage proxy files for smoother editing:"));
        layout->addWidget(descLabel);

        proxyList_ = new QListWidget();
        {
            QPalette p = proxyList_->palette();
            p.setColor(QPalette::Base, ArtifactPr::prLegacyColors().panelBackgroundAlt);
            proxyList_->setPalette(p);
        }
        layout->addWidget(proxyList_, 1);

        auto* buttonLayout = new QHBoxLayout();

        auto* createProxyBtn = new QPushButton(QStringLiteral("Create Proxy"));
        createProxyBtn->setProperty(ArtifactPr::kPropAccentColor, ArtifactPr::prLegacyColors().buttonProxyCreate);
        connect(createProxyBtn, &QPushButton::clicked, this, &ProxyPanel::onCreateProxy);
        buttonLayout->addWidget(createProxyBtn);

        auto* useProxyBtn = new QPushButton(QStringLiteral("Use Proxy"));
        useProxyBtn->setProperty(ArtifactPr::kPropAccentColor, ArtifactPr::prLegacyColors().buttonProxyUse);
        connect(useProxyBtn, &QPushButton::clicked, this, &ProxyPanel::onUseProxy);
        buttonLayout->addWidget(useProxyBtn);

        layout->addLayout(buttonLayout);

        auto* infoLabel = new QLabel(QStringLiteral("Tip: Proxies are lower\nresolution versions for\nsmoother editing."));
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

        auto* label = new QLabel(QStringLiteral("Source Monitor"));
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

        layout->addLayout(headerLayout);

        videoPlayer_ = new VideoPlayerWidget();
        videoPlayer_->setMinimumSize(320, 180);
        layout->addWidget(videoPlayer_, 1);

        auto* markersLayout = new QHBoxLayout();
        markersLayout->setContentsMargins(0, 0, 0, 0);
        inLabel_ = new QLabel(QStringLiteral("In: --"));
        outLabel_ = new QLabel(QStringLiteral("Out: --"));
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
            generateThumbnailStrip(filePath);
        }
    }

    void generateThumbnailStrip(const QString& filePath)
    {
        thumbnailStrip_->clear();

        // 5 箇所 (0% / 25% / 50% / 75% / 100%) のサムネを非同期要求
        const int kStripCount = 5;
        for (int i = 0; i < kStripCount; ++i) {
            auto* item = new QListWidgetItem(QStringLiteral("loading..."));
            thumbnailStrip_->addItem(item);

            ArtifactPr::ThumbnailRequest req;
            req.filePath = filePath;
            req.targetSize = QSize(80, 45);
            req.seekToMs = qint64(i) * 1000;  // 1 秒間隔 (簡易)
            thumbnailer_->request(req);
        }
    }

private Q_SLOTS:
    void onThumbnailReady(ArtifactPr::MediaThumbnail thumb)
    {
        if (!thumb.valid || thumb.filePath != currentFilePath_) return;
        // 該当 filePath の最初の "loading..." を置換
        for (int i = 0; i < thumbnailStrip_->count(); ++i) {
            auto* item = thumbnailStrip_->item(i);
            if (item && item->text() == QStringLiteral("loading...")) {
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
        FramePosition frame = positionMs / 33;
        engine->setInPoint(frame);
        inLabel_->setText(QStringLiteral("In: %1").arg(frame));
    }

    void onSetOutClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        qint64 positionMs = videoPlayer_->position();
        FramePosition frame = positionMs / 33;
        engine->setOutPoint(frame);
        outLabel_->setText(QStringLiteral("Out: %1").arg(frame));
    }

private:
    VideoPlayerWidget* videoPlayer_ = nullptr;
    QLabel* inLabel_ = nullptr;
    QLabel* outLabel_ = nullptr;
    QListWidget* thumbnailStrip_ = nullptr;
    ArtifactPr::MediaThumbnailer* thumbnailer_ = nullptr;
    QString currentFilePath_;
};

W_OBJECT_IMPL(SourceMonitorPanel)

class ProgramMonitorPanel : public QWidget
{
    W_OBJECT(ProgramMonitorPanel)
public:
    explicit ProgramMonitorPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* label = new QLabel(QStringLiteral("Program Monitor"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        preview_ = new VideoPlayerWidget();
        preview_->setMinimumSize(320, 180);
        layout->addWidget(preview_, 1);

        timecode_ = new QLabel(QStringLiteral("00:00:00:00"));
        QFont tcFont = timecode_->font();
        tcFont.setBold(true);
        timecode_->setFont(tcFont);
        layout->addWidget(timecode_);

        auto* engine = ArtifactPr::EditorEngine::instance();

        connect(engine, &ArtifactPr::EditorEngine::currentFrameChanged, this, &ProgramMonitorPanel::updateTimecode);
        connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &ProgramMonitorPanel::onSequenceChanged);
        connect(engine, &ArtifactPr::EditorEngine::playbackStateChanged, this, &ProgramMonitorPanel::onPlaybackStateChanged);
        connect(engine, &ArtifactPr::EditorEngine::clipSelectionChanged, this, &ProgramMonitorPanel::onClipSelected);
    }

private Q_SLOTS:
    void updateTimecode(ArtifactPr::FramePosition frame)
    {
        int fps = 30;
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

        auto* engine = ArtifactPr::EditorEngine::instance();
        qint64 positionMs = (frame * 1000) / fps;
        preview_->seek(positionMs);
    }

    void onSequenceChanged(const ArtifactPr::DemoSequence& seq) { }

    void onPlaybackStateChanged(bool isPlaying)
    {
        if (isPlaying) {
            preview_->play();
        } else {
            preview_->pause();
        }
    }

    void onClipSelected(const QString& clipId)
    {
        if (clipId.isEmpty()) return;
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        if (clip && !clip->sourceFile.isEmpty()) {
            preview_->loadFile(clip->sourceFile);
        }
    }

private:
    VideoPlayerWidget* preview_ = nullptr;
    QLabel* timecode_ = nullptr;
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

        auto* label = new QLabel(QStringLiteral("Audio Meters"));
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
        auto* resetPeakBtn = new QPushButton(trUi("Reset Peak", "ピークをリセット"));
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
        float baseLevel = 0.3f + static_cast<float>(QRandomGenerator::global()->bounded(40)) / 100.0f;

        for (auto* meter : meterWidgets_) {
            float variation = static_cast<float>(QRandomGenerator::global()->bounded(20)) / 100.0f;
            float level = baseLevel + variation;
            meter->updateLevel(qMin(1.0f, level));
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

private:
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

        auto* label = new QLabel(trUi("Transitions", "トランジション"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        auto* descLabel = new QLabel(uiText("Select transition type:", "トランジション種別を選択:"));
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

        addTransBtn(uiText("Crossfade", "クロスフェード"), ArtifactPr::TransitionType::Crossfade, QStringLiteral("#c89664"));
        addTransBtn(uiText("Dip to Black", "黒へフェード"), ArtifactPr::TransitionType::DipToBlack, QStringLiteral("#646464"));
        addTransBtn(uiText("Wipe Left", "左にワイプ"), ArtifactPr::TransitionType::WipeLeft, QStringLiteral("#9664c8"));
        addTransBtn(uiText("Wipe Right", "右にワイプ"), ArtifactPr::TransitionType::WipeRight, QStringLiteral("#c86496"));

        layout->addLayout(buttonLayout);

        layout->addStretch();

        auto* infoLabel = new QLabel(uiText(
            "Tip: Select a clip and\nposition playhead at\nclip boundary to apply.",
            "ヒント: クリップを選択し、\n再生位置を境界に合わせて\n適用してください。"));
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

        auto* label = new QLabel(trUi("Effects", "エフェクト"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        auto* descLabel = new QLabel(trUi("Basic effects:", "基本エフェクト:"));
        layout->addWidget(descLabel);

        auto* searchEdit = new QLineEdit();
        searchEdit->setPlaceholderText(trUi("Search effects...", "エフェクトを検索..."));
        {
            QPalette p = searchEdit->palette();
            p.setColor(QPalette::Base, ArtifactPr::prLegacyColors().inputBackground);
            searchEdit->setPalette(p);
        }
        layout->addWidget(searchEdit);

        auto* effectsList = new QListWidget();
        {
            QPalette p = effectsList->palette();
            p.setColor(QPalette::Base, ArtifactPr::prLegacyColors().panelBackgroundAlt);
            effectsList->setPalette(p);
        }

        QStringList effects = {
            trUi("Color Correction > Brightness/Contrast", "色補正 > 明るさ/コントラスト"),
            trUi("Color Correction > Hue/Saturation", "色補正 > 色相/彩度"),
            trUi("Color Correction > Color Wheels", "色補正 > カラーホイール"),
            trUi("Blur > Gaussian Blur", "ぼかし > ガウスぼかし"),
            trUi("Blur > Box Blur", "ぼかし > ボックスぼかし"),
            trUi("Sharpen > Unsharp Mask", "シャープ > アンシャープマスク"),
            trUi("Stylize > Glow", "スタイライズ > グロー"),
            trUi("Stylize > Posterize", "スタイライズ > ポスタリゼーション"),
            trUi("Transform > Scale", "変形 > スケール"),
            trUi("Transform > Rotate", "変形 > 回転"),
            trUi("Audio > Gain", "オーディオ > ゲイン"),
            trUi("Audio > Equalizer", "オーディオ > イコライザー")
        };

        for (const auto& effect : effects) {
            auto* item = new QListWidgetItem(effect);
            item->setData(Qt::UserRole, effect);
            effectsList->addItem(item);
        }

        layout->addWidget(effectsList, 1);

        auto* infoLabel = new QLabel(trUi(
            "Tip: Drag effects to clips\nin the timeline.",
            "ヒント: エフェクトをタイムライン上の\nクリップへドラッグしてください。"));
        layout->addWidget(infoLabel);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::clipSelectionChanged, this, &EffectsPanel::onClipSelected);
    }

private slots:
    void onClipSelected(const QString& clipId)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        bool hasEffects = clip && !clip->effects.isEmpty();
    }

private:
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

        auto* label = new QLabel(trUi("Clip Properties", "クリッププロパティ"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        clipNameLabel_ = new QLabel(uiText("No clip selected", "クリップが選択されていません"));
        layout->addWidget(clipNameLabel_);

        auto* volumeLabel = new QLabel(trUi("Volume:", "ボリューム:"));
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

        auto* speedLabel = new QLabel(trUi("Speed:", "速度:"));
        layout->addWidget(speedLabel);

        speedCombo_ = new QComboBox();
        speedCombo_->addItem(percentLabel(25), QVariant(0.25));
        speedCombo_->addItem(percentLabel(50), QVariant(0.5));
        speedCombo_->addItem(percentLabel(100), QVariant(1.0));
        speedCombo_->addItem(percentLabel(200), QVariant(2.0));
        speedCombo_->addItem(percentLabel(400), QVariant(4.0));
        speedCombo_->setCurrentIndex(2);
        {
            QPalette p = speedCombo_->palette();
            p.setColor(QPalette::Base, ArtifactPr::prLegacyColors().inputBackground);
            speedCombo_->setPalette(p);
        }
        connect(speedCombo_, &QComboBox::currentIndexChanged, this, &ClipPropertiesPanel::onSpeedChanged);
        layout->addWidget(speedCombo_);

        reverseCheck_ = new QCheckBox(trUi("Reverse", "逆再生"));
        connect(reverseCheck_, &QCheckBox::toggled, this, &ClipPropertiesPanel::onReverseToggled);
        layout->addWidget(reverseCheck_);

        layout->addStretch();

        infoLabel_ = new QLabel(trUi("Select a clip to\nedit its properties.", "クリップを選択して\nプロパティを編集してください。"));
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
            clipNameLabel_->setText(uiText("No clip selected", "クリップが選択されていません"));
            volumeSlider_->setValue(100);
            volumeValueLabel_->setText(percentLabel(100));
            speedCombo_->setCurrentIndex(2);
            reverseCheck_->setChecked(false);
            infoLabel_->setText(uiText("Select a clip to\nedit its properties.", "クリップを選択して\nプロパティを編集してください。"));
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

        QString info = uiText(
            "Duration: %1 frames\nStart: %2\nSource: %3-%4",
            "長さ: %1 フレーム\n開始: %2\nソース: %3-%4")
            .arg(clip->duration).arg(clip->startFrame).arg(clip->sourceIn).arg(clip->sourceOut);
        infoLabel_->setText(info);
    }

    void onClipChanged(const QString& clipId)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        if (clipId == engine->selectedClipId()) {
            onClipSelected(clipId);
        }
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
                engine->pushUndo(cmd);
            }
        }
    }

private:
    QLabel* clipNameLabel_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* volumeValueLabel_ = nullptr;
    QComboBox* speedCombo_ = nullptr;
    QCheckBox* reverseCheck_ = nullptr;
    QLabel* infoLabel_ = nullptr;
};

W_OBJECT_IMPL(ClipPropertiesPanel)

class TimelineRulerWidget : public QWidget
{
    W_OBJECT(TimelineRulerWidget)
public:
    TimelineRulerWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfaceTimelineRuler));
        setAutoFillBackground(true);
        setAttribute(Qt::WA_Hover);
        setMouseTracking(true);
    }

    void setMarkers(const QVector<ArtifactPr::Marker>& markers) { markers_ = markers; }
    void setTransitions(const QVector<ArtifactPr::Transition>& transitions) { transitions_ = transitions; }

signals:
    void markerClicked(const QString& markerId) W_SIGNAL(markerClicked, markerId);
    void markerRightClicked(const QString& markerId, const QPoint& pos) W_SIGNAL(markerRightClicked, markerId, pos);
    void markerMoved(const QString& markerId, int newFrame) W_SIGNAL(markerMoved, markerId, newFrame);

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRect rect = this->rect();

        p.fillRect(rect, QColor(42, 42, 42));

        QFont font = p.font();
        font.setPointSize(8);
        p.setFont(font);

        int duration = 400;
        int tickStep = 30;

        for (int frame = 0; frame < duration; frame += tickStep) {
            int x = frame * FRAME_WIDTH;

            p.setPen(QColor(80, 80, 80));
            p.drawLine(x, rect.height() - 8, x, rect.height());

            p.setPen(QColor(150, 150, 150));
            int seconds = frame / 30;
            p.drawText(x + 2, 10, QStringLiteral("%1s").arg(seconds));
        }

        for (const auto& marker : markers_) {
            int x = static_cast<int>(marker.position) * FRAME_WIDTH;
            QColor markerColor = marker.color;
            markerColor.setAlpha(220);
            p.setPen(markerColor);
            p.drawLine(x, 4, x, rect.height() - 8);

            p.setBrush(markerColor);
            QPoint triangle[3] = {
                QPoint(x - 4, 0),
                QPoint(x + 4, 0),
                QPoint(x, 6)
            };
            p.drawPolygon(triangle, 3);

            if (!marker.name.isEmpty() && marker.name != QStringLiteral("Marker %1").arg(1)) {
                p.setPen(Qt::white);
                p.drawText(x + 6, 12, marker.name);
            }
        }

        for (const auto& trans : transitions_) {
            int x = static_cast<int>(trans.startFrame) * FRAME_WIDTH;
            int w = qMax(4, static_cast<int>(trans.duration) * FRAME_WIDTH);

            QColor transColor;
            switch (trans.type) {
            case ArtifactPr::TransitionType::Crossfade:
                transColor = QColor(200, 150, 100);
                break;
            case ArtifactPr::TransitionType::DipToBlack:
                transColor = QColor(100, 100, 100);
                break;
            case ArtifactPr::TransitionType::WipeLeft:
            case ArtifactPr::TransitionType::WipeRight:
                transColor = QColor(150, 100, 200);
                break;
            default:
                transColor = QColor(180, 180, 100);
            }
            transColor.setAlpha(180);
            p.fillRect(x, rect.height() - 6, w, 4, transColor);
        }

        auto* engine = ArtifactPr::EditorEngine::instance();
        int playheadX = static_cast<int>(engine->currentFrame()) * FRAME_WIDTH;
        p.setPen(QColor(255, 50, 50));
        p.drawLine(playheadX, 0, playheadX, rect.height());
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            int clickX = event->pos().x();
            for (const auto& marker : markers_) {
                int markerX = static_cast<int>(marker.position) * FRAME_WIDTH;
                if (qAbs(clickX - markerX) < 8) {
                    selectedMarkerId_ = marker.id;
                    dragStartX_ = clickX;
                    isDraggingMarker_ = true;
                    Q_EMIT markerClicked(marker.id);
                    return;
                }
            }
        } else if (event->button() == Qt::RightButton) {
            int clickX = event->pos().x();
            for (const auto& marker : markers_) {
                int markerX = static_cast<int>(marker.position) * FRAME_WIDTH;
                if (qAbs(clickX - markerX) < 8) {
                    Q_EMIT markerRightClicked(marker.id, event->globalPos());
                    return;
                }
            }
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (isDraggingMarker_ && (event->buttons() & Qt::LeftButton)) {
            int diff = event->pos().x() - dragStartX_;
            int frameDelta = diff / FRAME_WIDTH;
            if (qAbs(frameDelta) > 0) {
                auto* engine = ArtifactPr::EditorEngine::instance();
                for (const auto& marker : markers_) {
                    if (marker.id == selectedMarkerId_) {
                        FramePosition newPos = qMax(0, marker.position + frameDelta);
                        Q_EMIT markerMoved(selectedMarkerId_, static_cast<int>(newPos));
                        dragStartX_ = event->pos().x();
                        break;
                    }
                }
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        isDraggingMarker_ = false;
        selectedMarkerId_.clear();
    }

private:
    QVector<ArtifactPr::Marker> markers_;
    QVector<ArtifactPr::Transition> transitions_;
    QString selectedMarkerId_;
    int dragStartX_;
    bool isDraggingMarker_ = false;
};

W_OBJECT_IMPL(TimelineRulerWidget)

class TimelineRulerWidget;

class TransitionWidget : public QWidget
{
    W_OBJECT(TransitionWidget)
public:
    TransitionWidget(const ArtifactPr::Transition& trans, QWidget* parent = nullptr)
        : QWidget(parent), trans_(trans), originalDuration_(trans.duration)
    {
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }

    ArtifactPr::Transition transition() const { return trans_; }
    void setTransition(const ArtifactPr::Transition& trans) { trans_ = trans; update(); }

signals:
    void transitionResized(const QString& transitionId, int newDuration) W_SIGNAL(transitionResized, transitionId, newDuration);
    void transitionRightClicked(const QString& transitionId, const QPoint& pos) W_SIGNAL(transitionRightClicked, transitionId, pos);

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor transColor;
        switch (trans_.type) {
        case ArtifactPr::TransitionType::Crossfade:
            transColor = QColor(200, 150, 100);
            break;
        case ArtifactPr::TransitionType::DipToBlack:
            transColor = QColor(100, 100, 100);
            break;
        case ArtifactPr::TransitionType::WipeLeft:
        case ArtifactPr::TransitionType::WipeRight:
            transColor = QColor(150, 100, 200);
            break;
        default:
            transColor = QColor(180, 180, 100);
        }

        QRect r = rect().adjusted(1, 1, -1, -1);
        transColor.setAlpha(160);
        p.setBrush(transColor);
        p.setPen(QColor(255, 255, 255, 100));
        p.drawRect(r);

        QColor handleColor(255, 255, 255, 60);
        p.fillRect(r.left(), r.top(), 4, r.height(), handleColor);
        p.fillRect(r.right() - 4, r.top(), 4, r.height(), handleColor);

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPointSize(7);
        p.setFont(f);

        QString name;
        switch (trans_.type) {
        case ArtifactPr::TransitionType::Crossfade:
            name = "XF";
            break;
        case ArtifactPr::TransitionType::DipToBlack:
            name = "DIP";
            break;
        case ArtifactPr::TransitionType::WipeLeft:
            name = "W<L";
            break;
        case ArtifactPr::TransitionType::WipeRight:
            name = "W>R";
            break;
        default:
            name = "TR";
        }
        p.drawText(r, Qt::AlignCenter, name);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            dragStartPos_ = event->pos();
            isResizingLeft_ = (event->pos().x() < 8);
            isResizingRight_ = (event->pos().x() > width() - 8);
            isDragging_ = false;
        } else if (event->button() == Qt::RightButton) {
            Q_EMIT transitionRightClicked(trans_.id, event->globalPos());
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event->buttons() & Qt::LeftButton) {
            int diff = event->pos().x() - dragStartPos_.x();
            if (qAbs(diff) > 3) {
                int durationDelta = diff / FRAME_WIDTH;
                int newDuration = qMax(6, originalDuration_ + durationDelta);
                Q_EMIT transitionResized(trans_.id, newDuration);
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        originalDuration_ = trans_.duration;
    }

private:
    ArtifactPr::Transition trans_;
    int originalDuration_;
    QPoint dragStartPos_;
    bool isResizingLeft_;
    bool isResizingRight_;
    bool isDragging_;
};

W_OBJECT_IMPL(TransitionWidget)

class TimelinePanel : public QWidget
{
    W_OBJECT(TimelinePanel)
public:
    explicit TimelinePanel(QWidget* parent = nullptr)
        : QWidget(parent), zoomLevel_(1.0f)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        ruler_ = new TimelineRulerWidget();
        ruler_->setMinimumHeight(24);
        layout->addWidget(ruler_);

        auto* toolbarWidget = new QWidget();
        toolbarWidget->setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfacePanelToolbar));
        toolbarWidget->setAutoFillBackground(true);
        auto* toolbarLayout = new QHBoxLayout(toolbarWidget);
        toolbarLayout->setContentsMargins(4, 2, 4, 2);
        toolbarLayout->setSpacing(8);

        auto* zoomLabel = new QLabel(trUi("Zoom:", "ズーム:"));
        toolbarLayout->addWidget(zoomLabel);

        zoomSlider_ = new QSlider(Qt::Horizontal);
        zoomSlider_->setMinimum(1);
        zoomSlider_->setMaximum(10);
        zoomSlider_->setValue(5);
        zoomSlider_->setMaximumWidth(120);
        zoomSlider_->setTickPosition(QSlider::NoTicks);
        connect(zoomSlider_, &QSlider::valueChanged, this, &TimelinePanel::onZoomChanged);
        toolbarLayout->addWidget(zoomSlider_);

        zoomLevelLabel_ = new QLabel(QStringLiteral("1.0x"));
        zoomLevelLabel_->setMinimumWidth(40);
        toolbarLayout->addWidget(zoomLevelLabel_);

        toolbarLayout->addStretch();

        sequenceInfo_ = new QLabel(trUi("No sequence", "シーケンスがありません"));
        toolbarLayout->addWidget(sequenceInfo_);

        layout->addWidget(toolbarWidget);

        scrollArea_ = new QScrollArea(this);
        scrollArea_->setWidgetResizable(true);
        scrollArea_->setFrameShape(QFrame::NoFrame);
        scrollArea_->setMinimumHeight(180);
        scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

        timelineHost_ = new QWidget();
        timelineLayout_ = new QVBoxLayout(timelineHost_);
        timelineLayout_->setContentsMargins(0, 0, 0, 0);
        timelineLayout_->setSpacing(2);

        scrollArea_->setWidget(timelineHost_);
        layout->addWidget(scrollArea_, 1);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &TimelinePanel::refreshTimeline);
        connect(engine, &ArtifactPr::EditorEngine::currentFrameChanged, this, &TimelinePanel::onFrameChanged);
        connect(engine, &ArtifactPr::EditorEngine::clipSelectionChanged, this, &TimelinePanel::onClipSelectionChanged);
        connect(engine, &ArtifactPr::EditorEngine::markerChanged, this, &TimelinePanel::onMarkerChanged);
        connect(engine, &ArtifactPr::EditorEngine::transitionChanged, this, &TimelinePanel::onTransitionChanged);

        connect(ruler_, &TimelineRulerWidget::markerClicked, this, &TimelinePanel::onMarkerClicked);
        connect(ruler_, &TimelineRulerWidget::markerRightClicked, this, &TimelinePanel::onMarkerRightClicked);
        connect(ruler_, &TimelineRulerWidget::markerMoved, this, &TimelinePanel::onMarkerMoved);

        ruler_->installEventFilter(this);

        refreshTimeline(engine->currentSequence());
    }

    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void refreshTimeline(const ArtifactPr::DemoSequence& seq);
    void onFrameChanged(ArtifactPr::FramePosition frame);
    void onClipSelectionChanged(const QString& clipId);
    void onMarkerChanged();
    void onTransitionChanged();
    void onMarkerClicked(const QString& markerId);
    void onMarkerRightClicked(const QString& markerId, const QPoint& pos);
    void onMarkerMoved(const QString& markerId, int newFrame);
    void onZoomChanged(int value);
    void onClipSelected(const QString& clipId);
    void onClipRightClicked(const QString& clipId, const QPoint& pos);
    void onClipMoved(const QString& clipId, int deltaX);
    void onClipTrimLeft(const QString& clipId, int deltaX);
    void onClipTrimRight(const QString& clipId, int deltaX);
    void onTransitionResized(const QString& transitionId, int newDuration);
    void onTransitionRightClicked(const QString& transitionId, const QPoint& pos);

private:
    TimelineRulerWidget* ruler_ = nullptr;
    QLabel* sequenceInfo_ = nullptr;
    QSlider* zoomSlider_ = nullptr;
    QLabel* zoomLevelLabel_ = nullptr;
    float zoomLevel_;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* timelineHost_ = nullptr;
    QVBoxLayout* timelineLayout_ = nullptr;

    void addTrackRow(const ArtifactPr::DemoTrack& track, const QVector<ArtifactPr::Transition>& transitions);
};

W_OBJECT_IMPL(TimelinePanel)

bool TimelinePanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ruler_ && event->type() == QEvent::MouseButtonDblClick) {
        auto* me = static_cast<QMouseEvent*>(event);
        FramePosition frame = me->pos().x() / FRAME_WIDTH;
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->addMarker(frame);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void TimelinePanel::refreshTimeline(const ArtifactPr::DemoSequence& seq)
    {
        while (timelineLayout_->count() > 0) {
            auto* item = timelineLayout_->takeAt(0);
            if (item->widget())
                delete item->widget();
            delete item;
        }

        sequenceInfo_->setText(QStringLiteral("%1 | %2 | Duration: %3 | Markers: %4 | Transitions: %5")
                                   .arg(seq.name).arg(seq.frameRate).arg(seq.duration)
                                   .arg(seq.markers.size()).arg(seq.transitions.size()));

        ruler_->setMarkers(seq.markers);
        ruler_->setTransitions(seq.transitions);
        ruler_->update();

        for (const auto& track : seq.videoTracks) {
            addTrackRow(track, seq.transitions);
        }
        for (const auto& track : seq.audioTracks) {
            addTrackRow(track, seq.transitions);
        }

        timelineLayout_->addStretch(1);
    }

void TimelinePanel::onFrameChanged(ArtifactPr::FramePosition) { ruler_->update(); }
void TimelinePanel::onClipSelectionChanged(const QString&) { refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence()); }
void TimelinePanel::onMarkerChanged() { ruler_->update(); }
void TimelinePanel::onTransitionChanged() { refreshTimeline(ArtifactPr::EditorEngine::instance()->currentSequence()); }

void TimelinePanel::onMarkerClicked(const QString& markerId)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        for (const auto& marker : engine->markers()) {
            if (marker.id == markerId) {
                engine->seekToFrame(marker.position);
                break;
            }
        }
    }

void TimelinePanel::onMarkerRightClicked(const QString& markerId, const QPoint& pos)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();

        auto* menu = new QMenu();

        auto* renameAction = menu->addAction(trUi("Rename Marker...", "マーカー名を変更..."));
        connect(renameAction, &QAction::triggered, [this, engine, markerId]() {
            for (const auto& marker : engine->markers()) {
                if (marker.id == markerId) {
                    bool ok;
                    QString newName = QInputDialog::getText(this, trUi("Rename Marker", "マーカー名を変更"),
                        trUi("Marker Name:", "マーカー名:"), QLineEdit::Normal, marker.name, &ok);
                    if (ok && !newName.isEmpty()) {
                        engine->setMarkerName(markerId, newName);
                    }
                    break;
                }
            }
        });

        auto* goToAction = menu->addAction(trUi("Go To Marker", "マーカーへ移動"));
        connect(goToAction, &QAction::triggered, [engine, markerId]() {
            for (const auto& marker : engine->markers()) {
                if (marker.id == markerId) {
                    engine->seekToFrame(marker.position);
                    break;
                }
            }
        });

        menu->addSeparator();

        auto* deleteAction = menu->addAction(trUi("Delete Marker", "マーカーを削除"));
        connect(deleteAction, &QAction::triggered, [engine, markerId]() {
            engine->deleteMarker(markerId);
        });

        menu->exec(pos);
        delete menu;
    }

void TimelinePanel::onMarkerMoved(const QString& markerId, int newFrame)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->moveMarker(markerId, newFrame);
        ruler_->update();
    }

void TimelinePanel::onZoomChanged(int value)
    {
        zoomLevel_ = value / 5.0f;
        zoomLevelLabel_->setText(QStringLiteral("%1x").arg(zoomLevel_, 0, 'f', 1));

        int frameWidth = qMax(1, static_cast<int>(2 * zoomLevel_));

        for (auto* child : timelineHost_->findChildren<QWidget*>()) {
            child->update();
        }
        ruler_->update();
    }

void TimelinePanel::onClipSelected(const QString& clipId)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->selectClip(clipId);
    }

void TimelinePanel::onClipRightClicked(const QString& clipId, const QPoint& pos)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->selectClip(clipId);

        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        auto* menu = new QMenu();

        auto* speedMenu = menu->addMenu(trUi("Speed", "速度"));

        auto* speed100 = speedMenu->addAction(QStringLiteral("100%"));
        connect(speed100, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 1.0); });

        auto* speed200 = speedMenu->addAction(QStringLiteral("200%"));
        connect(speed200, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 2.0); });

        auto* speed50 = speedMenu->addAction(QStringLiteral("50%"));
        connect(speed50, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 0.5); });

        auto* speed25 = speedMenu->addAction(QStringLiteral("25%"));
        connect(speed25, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 0.25); });

        auto* speed400 = speedMenu->addAction(QStringLiteral("400%"));
        connect(speed400, &QAction::triggered, [engine, clipId]() { engine->setClipSpeed(clipId, 4.0); });

        menu->addSeparator();

        auto* reverseAction = menu->addAction(trUi("Reverse", "逆再生"));
        reverseAction->setCheckable(true);
        reverseAction->setChecked(clip->reversed);
        connect(reverseAction, &QAction::triggered, [engine, clipId](bool checked) { engine->setClipReversed(clipId, checked); });

        menu->addSeparator();

        auto* volumeMenu = menu->addMenu(trUi("Volume", "ボリューム"));

        auto* vol100 = volumeMenu->addAction(QStringLiteral("100%"));
        connect(vol100, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 1.0); });

        auto* vol150 = volumeMenu->addAction(QStringLiteral("150%"));
        connect(vol150, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 1.5); });

        auto* vol200 = volumeMenu->addAction(QStringLiteral("200%"));
        connect(vol200, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 2.0); });

        auto* vol50 = volumeMenu->addAction(QStringLiteral("50%"));
        connect(vol50, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 0.5); });

        auto* vol0 = volumeMenu->addAction(QStringLiteral("0% (Mute)"));
        connect(vol0, &QAction::triggered, [engine, clipId]() { engine->setClipVolume(clipId, 0.0); });

        menu->addSeparator();

        auto* nameAction = menu->addAction(trUi("Rename Clip...", "クリップ名を変更..."));
        connect(nameAction, &QAction::triggered, [this, engine, clipId]() {
            auto* clip = engine->findClip(clipId);
            if (!clip) return;
            bool ok;
            QString newName = QInputDialog::getText(
                this,
                uiText("Rename Clip", "クリップ名の変更"),
                uiText("Clip Name:", "クリップ名:"),
                QLineEdit::Normal, clip->name, &ok);
            if (ok && !newName.isEmpty()) {
                engine->setClipName(clipId, newName);
                refreshTimeline(engine->currentSequence());
            }
        });

        menu->addSeparator();

        auto* deleteAction = menu->addAction(uiText("Delete", "削除"));
        deleteAction->setShortcut(QKeySequence::Delete);
        connect(deleteAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::deleteSelectedClip);

        menu->exec(pos);
        delete menu;
    }

void TimelinePanel::onClipMoved(const QString& clipId, int deltaX)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        int frameDelta = deltaX / FRAME_WIDTH;
        if (frameDelta == 0) return;

        FramePosition newStart = qMax(0, clip->startFrame + frameDelta);
        FramePosition snappedStart = engine->snapToNearest(newStart, true);
        engine->moveClip(clipId, snappedStart);
        refreshTimeline(engine->currentSequence());
    }

void TimelinePanel::onClipTrimLeft(const QString& clipId, int deltaX)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        int frameDelta = deltaX / FRAME_WIDTH;
        if (frameDelta == 0) return;

        FramePosition newStart = clip->startFrame + frameDelta;
        FramePosition newDuration = clip->duration - frameDelta;

        if (newDuration < 5) return;
        if (newStart < 0) return;

        FramePosition snappedStart = engine->snapToNearest(newStart, true);
        if (snappedStart != newStart) {
            frameDelta = snappedStart - clip->startFrame;
            newStart = snappedStart;
            newDuration = clip->duration - frameDelta;
        }

        engine->trimClip(clipId,
                         newStart,
                         newDuration,
                         clip->sourceIn + frameDelta,
                         clip->sourceOut);
        refreshTimeline(engine->currentSequence());
    }

void TimelinePanel::onClipTrimRight(const QString& clipId, int deltaX)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto* clip = engine->findClip(clipId);
        if (!clip) return;

        int frameDelta = deltaX / FRAME_WIDTH;
        if (frameDelta == 0) return;

        FramePosition newDuration = clip->duration + frameDelta;

        if (newDuration < 5) return;

        engine->trimClip(clipId,
                         clip->startFrame,
                         newDuration,
                         clip->sourceIn,
                         clip->sourceOut + frameDelta);
        refreshTimeline(engine->currentSequence());
    }

void TimelinePanel::onTransitionResized(const QString& transitionId, int newDuration)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto seq = engine->currentSequence();
        auto& transitions = seq.transitions;
        for (auto& trans : transitions) {
            if (trans.id == transitionId) {
                trans.duration = newDuration;
                engine->setCurrentSequence(seq);
                Q_EMIT engine->transitionChanged();
                Q_EMIT engine->projectModified();
                break;
            }
        }
    }

void TimelinePanel::onTransitionRightClicked(const QString& transitionId, const QPoint& pos)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto seq = engine->currentSequence();

        auto* menu = new QMenu();

        auto* durationMenu = menu->addMenu(trUi("Duration", "長さ"));

        auto* dur6 = durationMenu->addAction(QStringLiteral("6 frames"));
        connect(dur6, &QAction::triggered, [this, engine, seq, transitionId]() mutable {
            auto updatedSeq = seq;
            for (auto& trans : updatedSeq.transitions) {
                if (trans.id == transitionId) { trans.duration = 6; break; }
            }
            engine->setCurrentSequence(updatedSeq);
            Q_EMIT engine->transitionChanged();
            refreshTimeline(updatedSeq);
        });

        auto* dur12 = durationMenu->addAction(QStringLiteral("12 frames"));
        connect(dur12, &QAction::triggered, [this, engine, seq, transitionId]() mutable {
            auto updatedSeq = seq;
            for (auto& trans : updatedSeq.transitions) {
                if (trans.id == transitionId) { trans.duration = 12; break; }
            }
            engine->setCurrentSequence(updatedSeq);
            Q_EMIT engine->transitionChanged();
            refreshTimeline(updatedSeq);
        });

        auto* dur24 = durationMenu->addAction(QStringLiteral("24 frames"));
        connect(dur24, &QAction::triggered, [this, engine, seq, transitionId]() mutable {
            auto updatedSeq = seq;
            for (auto& trans : updatedSeq.transitions) {
                if (trans.id == transitionId) { trans.duration = 24; break; }
            }
            engine->setCurrentSequence(updatedSeq);
            Q_EMIT engine->transitionChanged();
            refreshTimeline(updatedSeq);
        });

        auto* dur48 = durationMenu->addAction(QStringLiteral("48 frames"));
        connect(dur48, &QAction::triggered, [this, engine, seq, transitionId]() mutable {
            auto updatedSeq = seq;
            for (auto& trans : updatedSeq.transitions) {
                if (trans.id == transitionId) { trans.duration = 48; break; }
            }
            engine->setCurrentSequence(updatedSeq);
            Q_EMIT engine->transitionChanged();
            refreshTimeline(updatedSeq);
        });

        menu->addSeparator();

        auto* deleteAction = menu->addAction(trUi("Delete Transition", "トランジションを削除"));
        connect(deleteAction, &QAction::triggered, [engine, transitionId]() {
            engine->deleteTransition(transitionId);
        });

        menu->exec(pos);
        delete menu;
    }

void TimelinePanel::addTrackRow(const ArtifactPr::DemoTrack& track, const QVector<ArtifactPr::Transition>& transitions)
    {
        QString trackColor = (track.kind == QStringLiteral("video")) ? QStringLiteral("#4a9eff") : QStringLiteral("#4eff4a");

        auto* trackWidget = new QWidget();
        trackWidget->setMinimumHeight(32);
        auto* trackLayout = new QHBoxLayout(trackWidget);
        trackLayout->setContentsMargins(0, 0, 0, 0);
        trackLayout->setSpacing(0);

        auto* nameLabel = new QLabel(track.name);
        nameLabel->setMinimumWidth(50);
        nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        nameLabel->setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfacePanelToolbar));
        nameLabel->setAutoFillBackground(true);
        trackLayout->addWidget(nameLabel);

        auto* trackContent = new QWidget();
        trackContent->setProperty(ArtifactPr::kPropSurfaceKind, QString::fromUtf8(ArtifactPr::kSurfaceTrackContent));
        trackContent->setAutoFillBackground(true);
        trackContent->setMinimumHeight(28);
        auto* contentLayout = new QHBoxLayout(trackContent);
        contentLayout->setContentsMargins(4, 2, 4, 2);
        contentLayout->setSpacing(0);

        auto* engine = ArtifactPr::EditorEngine::instance();
        QString selectedId = engine->selectedClipId();

        for (const auto& clip : track.clips) {
            bool isAudio = (track.kind == QStringLiteral("audio"));
            auto* clipWidget = new TimelineClipWidget(clip.id, clip.name, trackColor, clip.id == selectedId, isAudio, trackContent);
            int clipWidth = qMax(MIN_CLIP_WIDTH, static_cast<int>(clip.duration * FRAME_WIDTH));
            clipWidget->setMinimumWidth(clipWidth);
            clipWidget->setMaximumWidth(clipWidth);

            if (clip.speed != 1.0) {
                clipWidget->setSpeed(clip.speed);
            }
            if (clip.reversed) {
                clipWidget->setReversed(true);
            }

            connect(clipWidget, &TimelineClipWidget::clipSelected, this, &TimelinePanel::onClipSelected);
            connect(clipWidget, &TimelineClipWidget::clipMoved, this, &TimelinePanel::onClipMoved);
            connect(clipWidget, &TimelineClipWidget::clipTrimLeft, this, &TimelinePanel::onClipTrimLeft);
            connect(clipWidget, &TimelineClipWidget::clipTrimRight, this, &TimelinePanel::onClipTrimRight);
            connect(clipWidget, &TimelineClipWidget::clipRightClicked, this, &TimelinePanel::onClipRightClicked);

            contentLayout->addWidget(clipWidget);

            for (const auto& trans : transitions) {
                if (trans.trackId == track.id && (trans.rightClipId == clip.id || trans.leftClipId == clip.id)) {
                    auto* transWidget = new TransitionWidget(trans, trackContent);
                    int transWidth = qMax(MIN_CLIP_WIDTH, static_cast<int>(trans.duration * FRAME_WIDTH));
                    transWidget->setMinimumWidth(transWidth);
                    transWidget->setMaximumWidth(transWidth);
                    connect(transWidget, &TransitionWidget::transitionResized, this, &TimelinePanel::onTransitionResized);
                    connect(transWidget, &TransitionWidget::transitionRightClicked, this, &TimelinePanel::onTransitionRightClicked);
                    contentLayout->addWidget(transWidget);
                }
            }
        }
        contentLayout->addStretch(1);

        int totalWidth = qMax(800, static_cast<int>(engine->currentSequence().duration * FRAME_WIDTH) + 100);
        trackContent->setMinimumWidth(totalWidth);
        trackLayout->addWidget(trackContent, 1);
        timelineLayout_->addWidget(trackWidget);
    }
} // namespace

namespace {

} // namespace

ArtifactPrMainWindow::ArtifactPrMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(trUi("ArtifactPr", "ArtifactPr"));
    resize(1600, 980);

    auto* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    auto* engine = ArtifactPr::EditorEngine::instance();

    auto* fileMenu = menuBar->addMenu(uiText("File", "ファイル"));

    auto* newAction = fileMenu->addAction(uiText("New Project", "新規プロジェクト"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, [this, engine]() {
        engine->newProject();
    });

    auto* openAction = fileMenu->addAction(uiText("Open Project...", "プロジェクトを開く..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, [this, engine]() {
        QString filePath = QFileDialog::getOpenFileName(
            this, uiText("Open Project", "プロジェクトを開く"),
            QString(), QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        if (!filePath.isEmpty()) {
            engine->loadProject(filePath);
        }
    });

    auto* saveAction = fileMenu->addAction(uiText("Save Project", "プロジェクトを保存"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, [this, engine]() {
        QString filePath = QFileDialog::getSaveFileName(
            this, uiText("Save Project", "プロジェクトを保存"),
            engine->currentProject().name + QStringLiteral(".apr"),
            QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        if (!filePath.isEmpty()) {
            engine->saveProject(filePath);
        }
    });

    auto* saveAsAction = fileMenu->addAction(uiText("Save Project As...", "名前を付けて保存..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, [this, engine]() {
        QString filePath = QFileDialog::getSaveFileName(
            this, uiText("Save Project As", "名前を付けて保存"),
            engine->currentProject().name + QStringLiteral(".apr"),
            QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        if (!filePath.isEmpty()) {
            engine->saveProject(filePath);
        }
    });

    fileMenu->addSeparator();
    fileMenu->addAction(uiText("Import Media...", "メディアを読み込む..."));
    auto* exportAction = fileMenu->addAction(uiText("Export...", "書き出し..."));
    exportAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(exportAction, &QAction::triggered, this, &ArtifactPrMainWindow::onExportTriggered);
    fileMenu->addSeparator();
    fileMenu->addAction(uiText("Exit", "終了"), qApp, &QApplication::quit);

    auto* editMenu = menuBar->addMenu(uiText("Edit", "編集"));

    auto* undoAction = editMenu->addAction(uiText("Undo", "元に戻す"));
    undoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Z")));
    connect(undoAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::undo);

    auto* redoAction = editMenu->addAction(uiText("Redo", "やり直し"));
    redoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Z")));
    connect(redoAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::redo);

    editMenu->addSeparator();

    auto* deleteAction = editMenu->addAction(uiText("Delete", "削除"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::deleteSelectedClip);

    auto* rippleDeleteAction = editMenu->addAction(uiText("Ripple Delete", "リップル削除"));
    rippleDeleteAction->setShortcut(QKeySequence(QStringLiteral("Shift+Delete")));
    connect(rippleDeleteAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::rippleDeleteSelectedClip);

    auto* duplicateAction = editMenu->addAction(uiText("Duplicate", "複製"));
    duplicateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(duplicateAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::duplicateSelectedClip);

    editMenu->addSeparator();
    editMenu->addAction(uiText("Cut", "切り取り"));
    editMenu->addAction(uiText("Copy", "コピー"));
    editMenu->addAction(uiText("Paste", "貼り付け"));

    auto* snapAction = editMenu->addAction(uiText("Snap to Clips", "クリップにスナップ"));
    snapAction->setCheckable(true);
    snapAction->setChecked(true);
    connect(snapAction, &QAction::triggered, [engine](bool checked) {
        engine->setSnapEnabled(checked);
    });

    auto* sequenceMenu = menuBar->addMenu(uiText("Sequence", "シーケンス"));
    auto* bladeAction = sequenceMenu->addAction(uiText("Blade at Playhead", "再生位置でカット"));
    bladeAction->setShortcut(QKeySequence(QStringLiteral("C")));
    connect(bladeAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::splitClipAtPlayhead);

    auto* addTransitionMenu = sequenceMenu->addMenu(uiText("Add Transition", "トランジションを追加"));
    addTransitionMenu->addAction(uiText("Crossfade", "クロスフェード"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::Crossfade);
    });
    addTransitionMenu->addAction(uiText("Dip to Black", "黒へフェード"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::DipToBlack);
    });
    addTransitionMenu->addAction(uiText("Wipe Left", "左にワイプ"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeLeft);
    });
    addTransitionMenu->addAction(uiText("Wipe Right", "右にワイプ"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeRight);
    });

    sequenceMenu->addAction(uiText("New Sequence", "新規シーケンス"));
    sequenceMenu->addAction(uiText("Sequence Settings...", "シーケンス設定..."));

    auto* markerMenu = menuBar->addMenu(uiText("Marker", "マーカー"));
    auto* addMarkerAction = markerMenu->addAction(uiText("Add Marker at Playhead", "再生位置にマーカー追加"));
    addMarkerAction->setShortcut(QKeySequence(QStringLiteral("M")));
    connect(addMarkerAction, &QAction::triggered, [engine]() {
        engine->addMarker(engine->currentFrame());
    });
    auto* clearMarkersAction = markerMenu->addAction(uiText("Clear All Markers", "マーカーをすべて削除"));
    connect(clearMarkersAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::clearMarkers);

    menuBar->addMenu(uiText("Render", "レンダー"));

    auto* dockManager = new ads::CDockManager(this);
    setCentralWidget(dockManager);

    auto* projectPanel = new ProjectPanel();
    auto* projectDock = new ads::CDockWidget(QStringLiteral("Project"));
    projectDock->setWidget(projectPanel);
    projectDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    auto* projectArea = dockManager->addDockWidget(ads::LeftDockWidgetArea, projectDock);

    auto* mediaPanel = new MediaPanel();
    auto* mediaDock = new ads::CDockWidget(QStringLiteral("Media"));
    mediaDock->setWidget(mediaPanel);
    mediaDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidgetTabToArea(mediaDock, projectArea);
    projectDock->raise();

    auto* sourceMonitorPanel = new SourceMonitorPanel();
    auto* sourceDock = new ads::CDockWidget(QStringLiteral("Source Monitor"));
    sourceDock->setWidget(sourceMonitorPanel);
    sourceDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    auto* sourceArea = dockManager->addDockWidget(ads::RightDockWidgetArea, sourceDock);

    connect(mediaPanel, &MediaPanel::mediaSelected, sourceMonitorPanel, &SourceMonitorPanel::loadMedia);

    auto* programMonitorPanel = new ProgramMonitorPanel();
    auto* programDock = new ads::CDockWidget(QStringLiteral("Program Monitor"));
    programDock->setWidget(programMonitorPanel);
    programDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidgetTabToArea(programDock, sourceArea);
    sourceDock->raise();

    auto* audioMeterPanel = new AudioMeterPanel();
    auto* audioMeterDock = new ads::CDockWidget(QStringLiteral("Audio Meters"));
    audioMeterDock->setWidget(audioMeterPanel);
    audioMeterDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidget(ads::RightDockWidgetArea, audioMeterDock);

    auto* transitionPanel = new TransitionPanel();
    auto* transitionDock = new ads::CDockWidget(trUi("Transitions", "トランジション"));
    transitionDock->setWidget(transitionPanel);
    transitionDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidget(ads::RightDockWidgetArea, transitionDock);

    auto* effectsPanel = new EffectsPanel();
    auto* effectsDock = new ads::CDockWidget(trUi("Effects", "エフェクト"));
    effectsDock->setWidget(effectsPanel);
    effectsDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidget(ads::RightDockWidgetArea, effectsDock);

    auto* proxyPanel = new ProxyPanel();
    auto* proxyDock = new ads::CDockWidget(QStringLiteral("Proxy"));
    proxyDock->setWidget(proxyPanel);
    proxyDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidget(ads::RightDockWidgetArea, proxyDock);

    auto* clipPropsPanel = new ClipPropertiesPanel();
    auto* clipPropsDock = new ads::CDockWidget(QStringLiteral("Clip Properties"));
    clipPropsDock->setWidget(clipPropsPanel);
    clipPropsDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidget(ads::RightDockWidgetArea, clipPropsDock);

    auto* timelinePanel = new TimelinePanel();
    auto* timelineDock = new ads::CDockWidget(QStringLiteral("Timeline"));
    timelineDock->setWidget(timelinePanel);
    timelineDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidget(ads::BottomDockWidgetArea, timelineDock);

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
    statusBarWidget->showMessage(trUi(
        "Ready - J/K/L: Playback | C: Blade | T: Crossfade | W: Wipe | M: Marker | Del: Delete | Ctrl+Z: Undo",
        "準備完了 - J/K/L: 再生 | C: カット | T: クロスフェード | W: ワイプ | M: マーカー | Del: 削除 | Ctrl+Z: 元に戻す"));

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
    // undo / redo は EditorEngine の signal に直接接続 (slot 経由でなく connect で十分)
    connect(ArtifactPr::EditorEngine::instance(), &ArtifactPr::EditorEngine::sequenceChanged,
            this, [this]() {
        statusNotifier_.notify(QStringLiteral("Sequence modified"));
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
    timecodeOverlay_->setFps(30);
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

    const auto keyString = QKeySequence(event->key() | event->modifiers()).toString();

    const auto& shortcuts = shortcutRegistry_.all();
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

W_OBJECT_IMPL(ArtifactPrMainWindow)
