module;

#include <QAbstractVideoSurface>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QVideoFrame>

export module ArtifactPr.VideoSurface;

export namespace ArtifactPr {

/// 1 つの video frame の snapshot。
/// valid = false の場合は描画しない。
struct PrFrameSnapshot {
    QImage image;
    qint64 ptsMs = 0;
    bool valid = false;
};

/// QMediaPlayer から来る QVideoFrame をバッファリングし、
/// 最新 frame だけを UI thread に渡す video surface。
///
/// AGENTS.md 整合:
/// - QImage は IO / 互換境界のみ。描画は paint() で QPixmap 化。
/// - 新規 connect は最小限 (UI thread への frame 通知のみ)。
///
/// QAbstractVideoSurface は別スレッドから呼ばれるため、内部は mutex 保護。
/// UI thread への最新 frame 通知は direct connection で即時渡し、
/// 重複 frame は latest-wins で破棄する。
class PrVideoSurface : public QAbstractVideoSurface {
    Q_OBJECT
public:
    explicit PrVideoSurface(QObject* parent = nullptr);
    ~PrVideoSurface() override;

    // QAbstractVideoSurface
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const override;

    bool present(const QVideoFrame& frame) override;

    /// 最新 frame の image を取得 (UI thread から呼ばれる)。
    /// 取得後、内部 frame は無効化される。
    PrFrameSnapshot takeSnapshot();

    /// メディア切替前に保留中の旧 frame を破棄する。
    void clear();

    /// 最後に present した frame の PTS (ms)。UI thread から参照可能。
    qint64 lastPtsMs() const;

    /// 統計: drop した frame 数。
    int droppedFrameCount() const;

Q_SIGNALS:
    void framePresented(ArtifactPr::PrFrameSnapshot snapshot);

private:
    mutable QMutex mutex_;
    QImage latestImage_;
    qint64 latestPtsMs_ = 0;
    bool hasNewFrame_ = false;
    int droppedCount_ = 0;
};

} // namespace ArtifactPr

Q_DECLARE_METATYPE(ArtifactPr::PrFrameSnapshot)
