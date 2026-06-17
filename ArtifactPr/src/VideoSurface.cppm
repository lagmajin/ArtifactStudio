module;

#include <QAbstractVideoBuffer>
#include <QDebug>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QVideoFrame>

export module ArtifactPr.VideoSurface;

import ArtifactPr.VideoSurface;

namespace ArtifactPr {

PrVideoSurface::PrVideoSurface(QObject* parent)
    : QAbstractVideoSurface(parent) {
    qRegisterMetaType<PrFrameSnapshot>("PrFrameSnapshot");
}

PrVideoSurface::~PrVideoSurface() = default;

QList<QVideoFrame::PixelFormat> PrVideoSurface::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const {
    Q_UNUSED(handleType);
    return {
        QVideoFrame::Format_RGB32,
        QVideoFrame::Format_ARGB32,
        QVideoFrame::Format_ARGB32_Premultiplied,
        QVideoFrame::Format_RGB24,
        QVideoFrame::Format_BGR24,
        QVideoFrame::Format_BGR32,
        QVideoFrame::Format_BGRA32,
        QVideoFrame::Format_ABGR32,
        QVideoFrame::Format_YUV420P,
        QVideoFrame::Format_YUV422P,
        QVideoFrame::Format_YUYV,
        QVideoFrame::Format_UYVY,
        QVideoFrame::Format_NV12,
        QVideoFrame::Format_NV21,
        QVideoFrame::Format_Jpeg,
    };
}

bool PrVideoSurface::present(const QVideoFrame& frame) {
    if (!frame.isValid()) return false;

    QVideoFrame localFrame = frame;  // QVideoFrame は copy-on-write
    if (!localFrame.map(QVideoFrame::ReadOnly)) {
        return false;
    }

    QImage img = localFrame.toImage();
    localFrame.unmap();

    if (img.isNull()) {
        return false;
    }

    qint64 pts = frame.startTime() / 1000;  // us -> ms

    {
        QMutexLocker lock(&mutex_);
        if (hasNewFrame_) {
            // 古い frame を破棄 (latest-wins)
            ++droppedCount_;
        }
        latestImage_ = std::move(img);
        latestPtsMs_ = pts;
        hasNewFrame_ = true;
    }

    PrFrameSnapshot snap;
    snap.image = latestImage_;
    snap.ptsMs = pts;
    snap.valid = true;
    Q_EMIT framePresented(snap);
    return true;
}

PrFrameSnapshot PrVideoSurface::takeSnapshot() {
    QMutexLocker lock(&mutex_);
    PrFrameSnapshot snap;
    if (hasNewFrame_) {
        snap.image = latestImage_;
        snap.ptsMs = latestPtsMs_;
        snap.valid = true;
        hasNewFrame_ = false;
    }
    return snap;
}

qint64 PrVideoSurface::lastPtsMs() const {
    QMutexLocker lock(&mutex_);
    return latestPtsMs_;
}

int PrVideoSurface::droppedFrameCount() const {
    QMutexLocker lock(&mutex_);
    return droppedCount_;
}

} // namespace ArtifactPr