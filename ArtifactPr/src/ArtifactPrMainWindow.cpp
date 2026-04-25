#include "ArtifactPrMainWindow.hpp"
#include "ArtifactPrEditorEngine.hpp"
#include "VideoPlayerWidget.hpp"

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
#include <QThread>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QWindow>
#include <QMouseEvent>

namespace {

using FramePosition = ArtifactPr::FramePosition;

const int FRAME_WIDTH = 2;
const int MIN_CLIP_WIDTH = 20;
const int TRIM_HANDLE_WIDTH = 6;

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

class ProjectPanel : public QWidget
{
    W_OBJECT(ProjectPanel)
public:
    explicit ProjectPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* label = new QLabel(QStringLiteral("Project"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        tree_ = new QTreeWidget(this);
        tree_->setHeaderHidden(true);
        tree_->setMinimumHeight(120);
        tree_->setContextMenuPolicy(Qt::CustomContextMenu);
        layout->addWidget(tree_, 1);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &ProjectPanel::refreshProjectTree);
        refreshProjectTree(engine->currentSequence());
    }

    void refreshProjectTree(const ArtifactPr::DemoSequence& seq)
    {
        tree_->clear();

        auto* projectNode = new QTreeWidgetItem(QStringList{QStringLiteral("Project")});
        projectNode->setExpanded(true);

        auto* sequencesNode = new QTreeWidgetItem(QStringList{QStringLiteral("Sequences")});
        sequencesNode->setExpanded(true);

        auto* seqItem = new QTreeWidgetItem(QStringList{seq.name});
        seqItem->setData(0, Qt::UserRole, 0);
        sequencesNode->addChild(seqItem);

        projectNode->addChild(sequencesNode);
        projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Media")}));
        projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Bins")}));
        projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Exports")}));
        tree_->addTopLevelItem(projectNode);

        tree_->expandAll();
    }

private:
    QTreeWidget* tree_ = nullptr;
};

W_OBJECT_IMPL(ProjectPanel)

class MediaPanel : public QWidget
{
    W_OBJECT(MediaPanel)
public:
    explicit MediaPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* toolbar = new QHBoxLayout();
        toolbar->setContentsMargins(0, 0, 0, 0);

        auto* importBtn = new QPushButton(QStringLiteral("Import"));
        importBtn->setMaximumWidth(70);
        connect(importBtn, &QPushButton::clicked, this, &MediaPanel::onImportClicked);
        toolbar->addWidget(importBtn);
        toolbar->addStretch();

        auto* label = new QLabel(QStringLiteral("Media"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        toolbar->addWidget(label);
        layout->addLayout(toolbar);

        list_ = new QListWidget(this);
        list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        list_->setAcceptDrops(true);
        list_->setDropIndicatorShown(true);
        list_->setDragEnabled(true);
        connect(list_, &QListWidget::itemDoubleClicked, this, &MediaPanel::onItemDoubleClicked);
        layout->addWidget(list_, 1);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &MediaPanel::refreshMediaList);
        refreshMediaList(engine->currentSequence());
    }

    void refreshMediaList(const ArtifactPr::DemoSequence&)
    {
        list_->clear();

        auto* engine = ArtifactPr::EditorEngine::instance();
        const auto& seq = engine->currentSequence();

        for (const auto& track : seq.videoTracks) {
            for (const auto& clip : track.clips) {
                list_->addItem(clip.name);
            }
        }
        for (const auto& track : seq.audioTracks) {
            for (const auto& clip : track.clips) {
                list_->addItem(clip.name);
            }
        }
    }

    void addMediaFile(const QString& filePath, const QString& displayName)
    {
        auto* item = new QListWidgetItem(displayName);
        item->setData(Qt::UserRole, filePath);
        list_->addItem(item);
    }

signals:
    void mediaSelected(const QString& filePath) W_SIGNAL(mediaSelected, filePath);

private Q_SLOTS:
    void onImportClicked()
    {
        QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Import Media"), QString(),
            QStringLiteral("Video Files (*.mp4 *.avi *.mov *.mkv);;Audio Files (*.mp3 *.wav *.aac);;All Files (*)"));

        if (!files.isEmpty()) {
            for (const auto& file : files) {
                addMediaFile(file, QFileInfo(file).fileName());
            }
        }
    }

    void onItemDoubleClicked(QListWidgetItem* item)
    {
        QString filePath = item->data(Qt::UserRole).toString();
        if (!filePath.isEmpty()) {
            Q_EMIT mediaSelected(filePath);
        }
    }

private:
    QListWidget* list_ = nullptr;
};

W_OBJECT_IMPL(MediaPanel)

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
        descLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
        layout->addWidget(descLabel);

        proxyList_ = new QListWidget();
        proxyList_->setStyleSheet(QStringLiteral("background-color: #2a2a2a; color: white; border: none;"));
        layout->addWidget(proxyList_, 1);

        auto* buttonLayout = new QHBoxLayout();

        auto* createProxyBtn = new QPushButton(QStringLiteral("Create Proxy"));
        createProxyBtn->setStyleSheet(QStringLiteral("background-color: #4a6a8a; color: white; padding: 6px; border-radius: 3px;"));
        connect(createProxyBtn, &QPushButton::clicked, this, &ProxyPanel::onCreateProxy);
        buttonLayout->addWidget(createProxyBtn);

        auto* useProxyBtn = new QPushButton(QStringLiteral("Use Proxy"));
        useProxyBtn->setStyleSheet(QStringLiteral("background-color: #4a8a4a; color: white; padding: 6px; border-radius: 3px;"));
        connect(useProxyBtn, &QPushButton::clicked, this, &ProxyPanel::onUseProxy);
        buttonLayout->addWidget(useProxyBtn);

        layout->addLayout(buttonLayout);

        auto* infoLabel = new QLabel(QStringLiteral("Tip: Proxies are lower\nresolution versions for\nsmoother editing."));
        infoLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));
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
    }

    void loadMedia(const QString& filePath)
    {
        if (!filePath.isEmpty()) {
            videoPlayer_->loadFile(filePath);
            videoPlayer_->play();
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
        setMinimumWidth(30);
    }

    void setLevel(float level)
    {
        level_ = qBound(0.0f, level, 1.0f);
        update();
    }

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

        QColor peakColor = QColor(255, 255, 255);
        int peakHeight = qMin(4, static_cast<int>(r.height() * peak_));
        if (peakHeight > 0) {
            QRect peakRect = r.adjusted(0, r.height() - peakHeight, 0, 0);
            p.fillRect(peakRect, peakColor);
        }

        QPen gridPen(QColor(60, 60, 60));
        for (int i = 0; i <= 10; ++i) {
            int y = r.top() + (r.height() * i) / 10;
            p.setPen(gridPen);
            p.drawLine(r.left(), y, r.right(), y);
        }

        p.setPen(QColor(100, 100, 100));
        p.drawRect(r);
    }

public slots:
    void updateLevel(float level)
    {
        level_ = qBound(0.0f, level, 1.0f);
        if (level_ > peak_) {
            peak_ = level_;
        }
        update();
    }

    void resetPeak()
    {
        peak_ = level_;
        update();
    }

private:
    float level_;
    float peak_ = 0.0f;
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
        auto* resetPeakBtn = new QPushButton(QStringLiteral("Reset Peak"));
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

        auto* label = new QLabel(QStringLiteral("Transitions"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        auto* descLabel = new QLabel(QStringLiteral("Select transition type:"));
        layout->addWidget(descLabel);

        auto* buttonLayout = new QVBoxLayout();
        buttonLayout->setSpacing(4);

        auto addTransBtn = [this, buttonLayout](const QString& name, ArtifactPr::TransitionType type, const QString& color) {
            auto* btn = new QPushButton(name);
            btn->setStyleSheet(QStringLiteral("background-color: %1; color: white; padding: 8px; border-radius: 4px;").arg(color));
            connect(btn, &QPushButton::clicked, this, [type]() {
                auto* engine = ArtifactPr::EditorEngine::instance();
                engine->addTransitionAtPlayhead(type, 12);
            });
            buttonLayout->addWidget(btn);
            return btn;
        };

        addTransBtn(QStringLiteral("Crossfade"), ArtifactPr::TransitionType::Crossfade, QStringLiteral("#c89664"));
        addTransBtn(QStringLiteral("Dip to Black"), ArtifactPr::TransitionType::DipToBlack, QStringLiteral("#646464"));
        addTransBtn(QStringLiteral("Wipe Left"), ArtifactPr::TransitionType::WipeLeft, QStringLiteral("#9664c8"));
        addTransBtn(QStringLiteral("Wipe Right"), ArtifactPr::TransitionType::WipeRight, QStringLiteral("#c86496"));

        layout->addLayout(buttonLayout);

        layout->addStretch();

        auto* infoLabel = new QLabel(QStringLiteral("Tip: Select a clip and\nposition playhead at\nclip boundary to apply."));
        infoLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));
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

        auto* label = new QLabel(QStringLiteral("Effects"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        auto* descLabel = new QLabel(QStringLiteral("Basic effects:"));
        layout->addWidget(descLabel);

        auto* searchEdit = new QLineEdit();
        searchEdit->setPlaceholderText(QStringLiteral("Search effects..."));
        searchEdit->setStyleSheet(QStringLiteral("background-color: #333; color: white; padding: 4px; border: 1px solid #555; border-radius: 3px;"));
        layout->addWidget(searchEdit);

        auto* effectsList = new QListWidget();
        effectsList->setStyleSheet(QStringLiteral("background-color: #2a2a2a; color: white; border: none;"));

        QStringList effects = {
            QStringLiteral("Color Correction > Brightness/Contrast"),
            QStringLiteral("Color Correction > Hue/Saturation"),
            QStringLiteral("Color Correction > Color Wheels"),
            QStringLiteral("Blur > Gaussian Blur"),
            QStringLiteral("Blur > Box Blur"),
            QStringLiteral("Sharpen > Unsharp Mask"),
            QStringLiteral("Stylize > Glow"),
            QStringLiteral("Stylize > Posterize"),
            QStringLiteral("Transform > Scale"),
            QStringLiteral("Transform > Rotate"),
            QStringLiteral("Audio > Gain"),
            QStringLiteral("Audio > Equalizer")
        };

        for (const auto& effect : effects) {
            auto* item = new QListWidgetItem(effect);
            item->setData(Qt::UserRole, effect);
            effectsList->addItem(item);
        }

        layout->addWidget(effectsList, 1);

        auto* infoLabel = new QLabel(QStringLiteral("Tip: Drag effects to clips\nin the timeline."));
        infoLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));
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

        auto* label = new QLabel(QStringLiteral("Clip Properties"));
        QFont titleFont = label->font();
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleFont.setBold(true);
        label->setFont(titleFont);
        layout->addWidget(label);

        clipNameLabel_ = new QLabel(QStringLiteral("No clip selected"));
        clipNameLabel_->setStyleSheet(QStringLiteral("color: #888;"));
        layout->addWidget(clipNameLabel_);

        auto* volumeLabel = new QLabel(QStringLiteral("Volume:"));
        layout->addWidget(volumeLabel);

        volumeSlider_ = new QSlider(Qt::Horizontal);
        volumeSlider_->setMinimum(0);
        volumeSlider_->setMaximum(200);
        volumeSlider_->setValue(100);
        volumeSlider_->setTickPosition(QSlider::TicksBelow);
        volumeSlider_->setTickInterval(25);
        volumeSlider_->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { border: 1px solid #555; height: 8px; background: #333; border-radius: 4px; }"
            "QSlider::handle:horizontal { background: #4a9eff; width: 14px; margin: -3px 0; border-radius: 7px; }"
            "QSlider::sub-page:horizontal { background: #4a9eff; border-radius: 4px; }"));
        connect(volumeSlider_, &QSlider::valueChanged, this, &ClipPropertiesPanel::onVolumeChanged);
        layout->addWidget(volumeSlider_);

        volumeValueLabel_ = new QLabel(QStringLiteral("100%"));
        volumeValueLabel_->setAlignment(Qt::AlignCenter);
        layout->addWidget(volumeValueLabel_);

        auto* speedLabel = new QLabel(QStringLiteral("Speed:"));
        layout->addWidget(speedLabel);

        speedCombo_ = new QComboBox();
        speedCombo_->addItem(QStringLiteral("25%"), QVariant(0.25));
        speedCombo_->addItem(QStringLiteral("50%"), QVariant(0.5));
        speedCombo_->addItem(QStringLiteral("100%"), QVariant(1.0));
        speedCombo_->addItem(QStringLiteral("200%"), QVariant(2.0));
        speedCombo_->addItem(QStringLiteral("400%"), QVariant(4.0));
        speedCombo_->setCurrentIndex(2);
        speedCombo_->setStyleSheet(QStringLiteral("background-color: #333; color: white; padding: 4px; border: 1px solid #555; border-radius: 3px;"));
        connect(speedCombo_, &QComboBox::currentIndexChanged, this, &ClipPropertiesPanel::onSpeedChanged);
        layout->addWidget(speedCombo_);

        reverseCheck_ = new QCheckBox(QStringLiteral("Reverse"));
        reverseCheck_->setStyleSheet(QStringLiteral("color: white;"));
        connect(reverseCheck_, &QCheckBox::toggled, this, &ClipPropertiesPanel::onReverseToggled);
        layout->addWidget(reverseCheck_);

        layout->addStretch();

        infoLabel_ = new QLabel(QStringLiteral("Select a clip to\nedit its properties."));
        infoLabel_->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));
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
            clipNameLabel_->setText(QStringLiteral("No clip selected"));
            volumeSlider_->setValue(100);
            volumeValueLabel_->setText(QStringLiteral("100%"));
            speedCombo_->setCurrentIndex(2);
            reverseCheck_->setChecked(false);
            infoLabel_->setText(QStringLiteral("Select a clip to\nedit its properties."));
            return;
        }

        clipNameLabel_->setText(clip->name);
        int volumePercent = static_cast<int>(clip->volume * 100);
        volumeSlider_->setValue(volumePercent);
        volumeValueLabel_->setText(QStringLiteral("%1%").arg(volumePercent));

        int speedIndex = speedCombo_->findData(QVariant(clip->speed));
        if (speedIndex >= 0) {
            speedCombo_->setCurrentIndex(speedIndex);
        }

        reverseCheck_->setChecked(clip->reversed);

        QString info = QStringLiteral("Duration: %1 frames\nStart: %2\nSource: %3-%4")
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
            engine->setClipVolume(clipId, value / 100.0);
            volumeValueLabel_->setText(QStringLiteral("%1%").arg(value));
        }
    }

    void onSpeedChanged(int index)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        QString clipId = engine->selectedClipId();
        if (!clipId.isEmpty()) {
            double speed = speedCombo_->itemData(index).toDouble();
            engine->setClipSpeed(clipId, speed);
        }
    }

    void onReverseToggled(bool checked)
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        QString clipId = engine->selectedClipId();
        if (!clipId.isEmpty()) {
            engine->setClipReversed(clipId, checked);
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
        setStyleSheet(QStringLiteral("background-color: #2a2a2a; color: #777;"));
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
        toolbarWidget->setStyleSheet(QStringLiteral("background-color: #252525;"));
        auto* toolbarLayout = new QHBoxLayout(toolbarWidget);
        toolbarLayout->setContentsMargins(4, 2, 4, 2);
        toolbarLayout->setSpacing(8);

        auto* zoomLabel = new QLabel(QStringLiteral("Zoom:"));
        zoomLabel->setStyleSheet(QStringLiteral("color: #888;"));
        toolbarLayout->addWidget(zoomLabel);

        zoomSlider_ = new QSlider(Qt::Horizontal);
        zoomSlider_->setMinimum(1);
        zoomSlider_->setMaximum(10);
        zoomSlider_->setValue(5);
        zoomSlider_->setMaximumWidth(120);
        zoomSlider_->setTickPosition(QSlider::NoTicks);
        zoomSlider_->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { border: 1px solid #555; height: 6px; background: #333; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #4a9eff; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        connect(zoomSlider_, &QSlider::valueChanged, this, &TimelinePanel::onZoomChanged);
        toolbarLayout->addWidget(zoomSlider_);

        zoomLevelLabel_ = new QLabel(QStringLiteral("1.0x"));
        zoomLevelLabel_->setStyleSheet(QStringLiteral("color: #aaa; min-width: 40px;"));
        toolbarLayout->addWidget(zoomLevelLabel_);

        toolbarLayout->addStretch();

        sequenceInfo_ = new QLabel(QStringLiteral("No sequence"));
        sequenceInfo_->setStyleSheet(QStringLiteral("color: #888; padding: 2px 8px;"));
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

        auto* renameAction = menu->addAction(QStringLiteral("Rename Marker..."));
        connect(renameAction, &QAction::triggered, [this, engine, markerId]() {
            for (const auto& marker : engine->markers()) {
                if (marker.id == markerId) {
                    bool ok;
                    QString newName = QInputDialog::getText(this, QStringLiteral("Rename Marker"),
                        QStringLiteral("Marker Name:"), QLineEdit::Normal, marker.name, &ok);
                    if (ok && !newName.isEmpty()) {
                        engine->setMarkerName(markerId, newName);
                    }
                    break;
                }
            }
        });

        auto* goToAction = menu->addAction(QStringLiteral("Go To Marker"));
        connect(goToAction, &QAction::triggered, [engine, markerId]() {
            for (const auto& marker : engine->markers()) {
                if (marker.id == markerId) {
                    engine->seekToFrame(marker.position);
                    break;
                }
            }
        });

        menu->addSeparator();

        auto* deleteAction = menu->addAction(QStringLiteral("Delete Marker"));
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

        auto* speedMenu = menu->addMenu(QStringLiteral("Speed"));

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

        auto* reverseAction = menu->addAction(QStringLiteral("Reverse"));
        reverseAction->setCheckable(true);
        reverseAction->setChecked(clip->reversed);
        connect(reverseAction, &QAction::triggered, [engine, clipId](bool checked) { engine->setClipReversed(clipId, checked); });

        menu->addSeparator();

        auto* volumeMenu = menu->addMenu(QStringLiteral("Volume"));

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

        auto* nameAction = menu->addAction(QStringLiteral("Rename Clip..."));
        connect(nameAction, &QAction::triggered, [this, engine, clipId]() {
            auto* clip = engine->findClip(clipId);
            if (!clip) return;
            bool ok;
            QString newName = QInputDialog::getText(this, QStringLiteral("Rename Clip"),
                QStringLiteral("Clip Name:"), QLineEdit::Normal, clip->name, &ok);
            if (ok && !newName.isEmpty()) {
                engine->setClipName(clipId, newName);
                refreshTimeline(engine->currentSequence());
            }
        });

        menu->addSeparator();

        auto* deleteAction = menu->addAction(QStringLiteral("Delete"));
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
        clip->startFrame = snappedStart;

        Q_EMIT engine->projectModified();
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

        clip->startFrame = newStart;
        clip->duration = newDuration;
        clip->sourceIn += frameDelta;

        Q_EMIT engine->projectModified();
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

        clip->duration = newDuration;
        clip->sourceOut += frameDelta;

        Q_EMIT engine->projectModified();
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

        auto* durationMenu = menu->addMenu(QStringLiteral("Duration"));

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

        auto* deleteAction = menu->addAction(QStringLiteral("Delete Transition"));
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
        nameLabel->setStyleSheet(QStringLiteral("background-color: #252525; color: #aaa; padding: 2px 8px; border-right: 1px solid #333;"));
        trackLayout->addWidget(nameLabel);

        auto* trackContent = new QWidget();
        trackContent->setStyleSheet(QStringLiteral("background-color: #1e1e1e;"));
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

class TransportBarWidget : public QWidget
{
    W_OBJECT(TransportBarWidget)
public:
    explicit TransportBarWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(6);

        auto addButton = [layout](const QString& text, const char* member) {
            auto* button = new QPushButton(text);
            button->setFixedWidth(40);
            button->setFixedHeight(28);
            layout->addWidget(button);
            return button;
        };

        stopBtn_ = addButton(QStringLiteral("■"), SLOT(onStopClicked()));
        stepBackBtn_ = addButton(QStringLiteral("◀◀"), SLOT(onStepBackClicked()));
        playBtn_ = addButton(QStringLiteral("▶"), SLOT(onPlayClicked()));
        stepFwdBtn_ = addButton(QStringLiteral("▶▶"), SLOT(onStepFwdClicked()));

        timecode_ = new QLabel(QStringLiteral("00:00:00:00"));
        QFont timecodeFont = timecode_->font();
        timecodeFont.setBold(true);
        timecode_->setFont(timecodeFont);
        layout->addWidget(timecode_);

        speedLabel_ = new QLabel(QStringLiteral("1x"));
        speedLabel_->setMinimumWidth(40);
        speedLabel_->setAlignment(Qt::AlignCenter);
        layout->addWidget(speedLabel_);

        layout->addStretch(1);

        auto* exportButton = new QPushButton(QStringLiteral("Export"));
        connect(exportButton, &QPushButton::clicked, this, &TransportBarWidget::onExportClicked);
        layout->addWidget(exportButton);

        auto* engine = ArtifactPr::EditorEngine::instance();
        connect(engine, &ArtifactPr::EditorEngine::playbackStateChanged, this, &TransportBarWidget::updatePlayState);
        connect(engine, &ArtifactPr::EditorEngine::playbackSpeedChanged, this, &TransportBarWidget::updateSpeedDisplay);
        connect(engine, &ArtifactPr::EditorEngine::currentFrameChanged, this, &TransportBarWidget::updateTimecode);

        playbackTimer_ = new QTimer(this);
        connect(playbackTimer_, &QTimer::timeout, this, &TransportBarWidget::onPlaybackTick);
    }

private Q_SLOTS:
    void onStopClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        playbackTimer_->stop();
        engine->stop();
    }

    void onStepBackClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        playbackTimer_->stop();
        engine->stepBackward();
    }

    void onPlayClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        engine->togglePlayPause();

        if (engine->isPlaying()) {
            playbackTimer_->start(33);
        } else {
            playbackTimer_->stop();
        }
    }

    void onStepFwdClicked()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        playbackTimer_->stop();
        engine->stepForward();
    }

    void onPlaybackTick()
    {
        auto* engine = ArtifactPr::EditorEngine::instance();
        auto frame = engine->currentFrame();
        int speed = static_cast<int>(engine->playbackSpeed());

        if (speed > 1) {
            if (frame < engine->currentSequence().duration) {
                engine->setCurrentFrame(frame + speed / 2);
            } else {
                playbackTimer_->stop();
                engine->stop();
            }
        } else if (speed < -1) {
            if (frame > 0) {
                engine->setCurrentFrame(frame + speed / 2);
            } else {
                playbackTimer_->stop();
                engine->stop();
            }
        } else {
            if (frame < engine->currentSequence().duration) {
                engine->setCurrentFrame(frame + 1);
            } else {
                playbackTimer_->stop();
                engine->stop();
            }
        }
    }

    void onExportClicked()
    {
        Q_EMIT requestExport();
    }

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
    }

    void updatePlayState(bool isPlaying) { playBtn_->setText(isPlaying ? QStringLiteral("⏸") : QStringLiteral("▶")); }

    void updateSpeedDisplay(ArtifactPr::PlaybackSpeed speed)
    {
        int speedVal = static_cast<int>(speed);
        if (speedVal == 0) {
            speedLabel_->setText(QStringLiteral("停止"));
        } else if (speedVal == 1) {
            speedLabel_->setText(QStringLiteral("一時停止"));
        } else if (speedVal > 1) {
            speedLabel_->setText(QStringLiteral("%1x").arg(speedVal / 2));
        } else {
            speedLabel_->setText(QStringLiteral("%1x R").arg(-speedVal / 2));
        }
    }

signals:
    void requestExport() W_SIGNAL(requestExport);

private:
    QPushButton *stopBtn_ = nullptr;
    QPushButton *stepBackBtn_ = nullptr;
    QPushButton *playBtn_ = nullptr;
    QPushButton *stepFwdBtn_ = nullptr;
    QLabel* timecode_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QTimer* playbackTimer_ = nullptr;
};

W_OBJECT_IMPL(TransportBarWidget)

namespace {

class ExportDialog : public QDialog
{
    W_OBJECT(ExportDialog)
public:
    ExportDialog(QWidget* parent = nullptr)
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

private Q_SLOTS:
    void onBrowseClicked()
    {
        QString file = QFileDialog::getSaveFileName(this, QStringLiteral("Save Export"),
            QStringLiteral("output.mp4"), QStringLiteral("MP4 Files (*.mp4);;All Files (*)"));
        if (!file.isEmpty()) {
            outputPathEdit_->setText(file);
        }
    }

    void onExportClicked()
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

private:
    QLineEdit* outputPathEdit_;
    QComboBox* resolutionCombo_;
    QComboBox* codecCombo_;
    QComboBox* framerateCombo_;
    QSlider* qualitySlider_;
    QLabel* progressLabel_;
    QProgressBar* progressBar_;
};

W_OBJECT_IMPL(ExportDialog)

} // namespace

ArtifactPrMainWindow::ArtifactPrMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("ArtifactPr"));
    resize(1600, 980);

    auto* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    auto* engine = ArtifactPr::EditorEngine::instance();

    auto* fileMenu = menuBar->addMenu(QStringLiteral("File"));

    auto* newAction = fileMenu->addAction(QStringLiteral("New Project"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, [this, engine]() {
        engine->newProject();
    });

    auto* openAction = fileMenu->addAction(QStringLiteral("Open Project..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, [this, engine]() {
        QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("Open Project"),
            QString(), QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        if (!filePath.isEmpty()) {
            engine->loadProject(filePath);
        }
    });

    auto* saveAction = fileMenu->addAction(QStringLiteral("Save Project"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, [this, engine]() {
        QString filePath = QFileDialog::getSaveFileName(this, QStringLiteral("Save Project"),
            engine->currentProject().name + QStringLiteral(".apr"),
            QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        if (!filePath.isEmpty()) {
            engine->saveProject(filePath);
        }
    });

    auto* saveAsAction = fileMenu->addAction(QStringLiteral("Save Project As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, [this, engine]() {
        QString filePath = QFileDialog::getSaveFileName(this, QStringLiteral("Save Project As"),
            engine->currentProject().name + QStringLiteral(".apr"),
            QStringLiteral("ArtifactPr Project (*.apr);;All Files (*)"));
        if (!filePath.isEmpty()) {
            engine->saveProject(filePath);
        }
    });

    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Import Media..."));
    auto* exportAction = fileMenu->addAction(QStringLiteral("Export..."));
    exportAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(exportAction, &QAction::triggered, this, &ArtifactPrMainWindow::onExportTriggered);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Exit"), qApp, &QApplication::quit);

    auto* editMenu = menuBar->addMenu(QStringLiteral("Edit"));

    auto* undoAction = editMenu->addAction(QStringLiteral("Undo"));
    undoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Z")));
    connect(undoAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::undo);

    auto* redoAction = editMenu->addAction(QStringLiteral("Redo"));
    redoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Z")));
    connect(redoAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::redo);

    editMenu->addSeparator();

    auto* deleteAction = editMenu->addAction(QStringLiteral("Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::deleteSelectedClip);

    auto* rippleDeleteAction = editMenu->addAction(QStringLiteral("Ripple Delete"));
    rippleDeleteAction->setShortcut(QKeySequence(QStringLiteral("Shift+Delete")));
    connect(rippleDeleteAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::rippleDeleteSelectedClip);

    auto* duplicateAction = editMenu->addAction(QStringLiteral("Duplicate"));
    duplicateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(duplicateAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::duplicateSelectedClip);

    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Cut"));
    editMenu->addAction(QStringLiteral("Copy"));
    editMenu->addAction(QStringLiteral("Paste"));

    auto* snapAction = editMenu->addAction(QStringLiteral("Snap to Clips"));
    snapAction->setCheckable(true);
    snapAction->setChecked(true);
    connect(snapAction, &QAction::triggered, [engine](bool checked) {
        engine->setSnapEnabled(checked);
    });

    auto* sequenceMenu = menuBar->addMenu(QStringLiteral("Sequence"));
    auto* bladeAction = sequenceMenu->addAction(QStringLiteral("Blade at Playhead"));
    bladeAction->setShortcut(QKeySequence(QStringLiteral("C")));
    connect(bladeAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::splitClipAtPlayhead);

    auto* addTransitionMenu = sequenceMenu->addMenu(QStringLiteral("Add Transition"));
    addTransitionMenu->addAction(QStringLiteral("Crossfade"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::Crossfade);
    });
    addTransitionMenu->addAction(QStringLiteral("Dip to Black"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::DipToBlack);
    });
    addTransitionMenu->addAction(QStringLiteral("Wipe Left"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeLeft);
    });
    addTransitionMenu->addAction(QStringLiteral("Wipe Right"), [engine]() {
        engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeRight);
    });

    sequenceMenu->addAction(QStringLiteral("New Sequence"));
    sequenceMenu->addAction(QStringLiteral("Sequence Settings..."));

    auto* markerMenu = menuBar->addMenu(QStringLiteral("Marker"));
    auto* addMarkerAction = markerMenu->addAction(QStringLiteral("Add Marker at Playhead"));
    addMarkerAction->setShortcut(QKeySequence(QStringLiteral("M")));
    connect(addMarkerAction, &QAction::triggered, [engine]() {
        engine->addMarker(engine->currentFrame());
    });
    auto* clearMarkersAction = markerMenu->addAction(QStringLiteral("Clear All Markers"));
    connect(clearMarkersAction, &QAction::triggered, engine, &ArtifactPr::EditorEngine::clearMarkers);

    menuBar->addMenu(QStringLiteral("Render"));

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
    auto* transitionDock = new ads::CDockWidget(QStringLiteral("Transitions"));
    transitionDock->setWidget(transitionPanel);
    transitionDock->setFeatures(ads::CDockWidget::AllDockWidgetFeatures);
    dockManager->addDockWidget(ads::RightDockWidgetArea, transitionDock);

    auto* effectsPanel = new EffectsPanel();
    auto* effectsDock = new ads::CDockWidget(QStringLiteral("Effects"));
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
    statusBarWidget->showMessage(QStringLiteral("Ready - J/K/L: Playback | C: Blade | T: Crossfade | W: Wipe | M: Marker | Del: Delete | Ctrl+Z: Undo"));

    dockManager->setStyleSheet(QStringLiteral(
        "ads::CDockWidget { titleBarIconVisible: false; }"
        "ads CDockAreaWidget { background-color: #1a1a1a; }"));
}

void ArtifactPrMainWindow::keyPressEvent(QKeyEvent* event)
{
    auto* engine = ArtifactPr::EditorEngine::instance();

    switch (event->key()) {
    case Qt::Key_K:
        if (event->isAutoRepeat()) break;
        engine->pause();
        break;

    case Qt::Key_J:
        if (event->isAutoRepeat()) break;
        engine->shuttleReverse();
        break;

    case Qt::Key_L:
        if (event->isAutoRepeat()) break;
        engine->shuttleForward();
        break;

    case Qt::Key_Space:
        if (event->isAutoRepeat()) break;
        engine->togglePlayPause();
        break;

    case Qt::Key_Home:
        engine->seekToFrame(0);
        break;

    case Qt::Key_End:
        engine->seekToFrame(engine->currentSequence().duration);
        break;

    case Qt::Key_T:
        if (!event->isAutoRepeat()) {
            if (event->modifiers() & Qt::ShiftModifier) {
                engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::DipToBlack);
            } else {
                engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::Crossfade);
            }
        }
        break;

    case Qt::Key_W:
        if (!event->isAutoRepeat()) {
            if (event->modifiers() & Qt::ShiftModifier) {
                engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeRight);
            } else {
                engine->addTransitionAtPlayhead(ArtifactPr::TransitionType::WipeLeft);
            }
        }
        break;

    case Qt::Key_C:
        if (!event->isAutoRepeat() && (event->modifiers() & Qt::ControlModifier)) {
            if (!engine->selectedClipId().isEmpty()) {
                engine->copyClip(engine->selectedClipId());
            }
        } else if (!event->isAutoRepeat()) {
            engine->splitClipAtPlayhead();
        }
        break;

    case Qt::Key_X:
        if (!event->isAutoRepeat() && (event->modifiers() & Qt::ControlModifier)) {
            if (!engine->selectedClipId().isEmpty()) {
                engine->cutClip(engine->selectedClipId());
            }
        }
        break;

    case Qt::Key_V:
        if (!event->isAutoRepeat() && (event->modifiers() & Qt::ControlModifier)) {
            engine->pasteClip(engine->currentFrame());
        }
        break;

    case Qt::Key_I:
        if (!event->isAutoRepeat()) {
            engine->setInPoint(engine->currentFrame());
        }
        break;

    case Qt::Key_O:
        if (!event->isAutoRepeat()) {
            engine->setOutPoint(engine->currentFrame());
        }
        break;

    case Qt::Key_Equal:
    case Qt::Key_Plus:
        if (!event->isAutoRepeat()) {
            Q_EMIT requestZoomIn();
        }
        break;

    case Qt::Key_Minus:
        if (!event->isAutoRepeat()) {
            Q_EMIT requestZoomOut();
        }
        break;

    default:
        QMainWindow::keyPressEvent(event);
    }
}

void ArtifactPrMainWindow::onExportTriggered()
{
    ExportDialog dialog(this);
    dialog.exec();
}

W_OBJECT_IMPL(ArtifactPrMainWindow)
