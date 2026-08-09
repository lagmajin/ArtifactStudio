module;

#include <QDebug>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QSize>
#include <QString>
#include <QThread>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWaitCondition>
#include <algorithm>
#include <utility>

export module ArtifactPr.MediaThumbnailer;

import ArtifactPr.MediaThumbnailer;

namespace ArtifactPr {

// =====================================================================
// MediaThumbnailer::Worker
// ---------------------------------------------------------------------
// 別スレッドで動く。request queue から ThumbnailRequest を取り出し、
// 1 つずつ thumbnail を生成する。
//
// 重複 filePath は latest-wins: queue 内の古い同じ filePath の request
// は破棄して、最新のものだけ処理する。
//
// thumbnail 抽出には QMediaPlayer + QVideoSink を使う。
// seek して最初の videoFrameChanged を待ってから frame を image 化。
// 60 秒タイムアウトしたら失敗扱い (large / broken file 対策)。
// =====================================================================
class MediaThumbnailer::Worker : public QObject {
public:
    explicit Worker(MediaThumbnailer* owner)
        : QObject(nullptr), owner_(owner) {}

    void enqueue(const ThumbnailRequest& req) {
        QMutexLocker lock(&queueMutex_);

        // latest-wins: 同じ filePath が queue にあれば除去
        auto it = std::remove_if(queue_.begin(), queue_.end(),
            [&req](const ThumbnailRequest& r) {
                return r.filePath == req.filePath;
            });
        queue_.erase(it, queue_.end());

        queue_.enqueue(req);
        queueCond_.wakeOne();
    }

    void stop() {
        QMutexLocker lock(&queueMutex_);
        stop_ = true;
        queueCond_.wakeAll();
    }

public Q_SLOTS:
    void process() {
        while (true) {
            ThumbnailRequest req;
            {
                QMutexLocker lock(&queueMutex_);
                while (queue_.isEmpty() && !stop_) {
                    queueCond_.wait(&queueMutex_);
                }
                if (stop_) return;
                req = queue_.dequeue();
            }

            MediaThumbnail thumb = generateSync(req);

            // 結果を GUI thread へ戻し、所有側で cache と signal を更新する。
            auto* owner = owner_;
            QMetaObject::invokeMethod(owner, [owner, thumb]() {
                owner->publishThumbnail(thumb);
            }, Qt::QueuedConnection);
        }
    }

private:
    /// 1 つの thumbnail を同期生成。失敗時は invalid な struct を返す。
    MediaThumbnail generateSync(const ThumbnailRequest& req) {
        MediaThumbnail result;
        result.filePath = req.filePath;
        result.image = QImage();
        result.durationMs = 0;
        result.valid = false;

        QMediaPlayer player;
        QVideoSink sink;
        player.setVideoSink(&sink);

        QObject::connect(&sink, &QVideoSink::videoFrameChanged,
                         this, [](const QVideoFrame& frame) {
            Q_UNUSED(frame);
        });

        player.setSource(QUrl::fromLocalFile(req.filePath));

        // 60 秒タイムアウト
        QElapsedTimer timer;
        timer.start();
        const int kTimeoutMs = 60000;

        // duration 取得
        while (player.duration() <= 0 && timer.elapsed() < kTimeoutMs) {
            QThread::msleep(20);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        result.durationMs = player.duration();

        // seek
        player.setPosition(static_cast<qint64>(req.seekToMs));

        // frame 待ち
        QVideoFrame capturedFrame;
        bool captured = false;
        QObject::disconnect(&sink, nullptr, nullptr, nullptr);  // 一旦解除
        QObject::connect(&sink, &QVideoSink::videoFrameChanged,
                         this, [&capturedFrame, &captured](const QVideoFrame& frame) {
            if (!captured && frame.isValid()) {
                capturedFrame = frame;
                captured = true;
            }
        });

        timer.restart();
        while (!captured && timer.elapsed() < kTimeoutMs) {
            QThread::msleep(20);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }

        if (captured && capturedFrame.isValid()) {
            QImage img = capturedFrame.toImage();
            if (!img.isNull()) {
                result.image = img.scaled(req.targetSize,
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
                result.valid = true;
            }
        }

        player.stop();
        return result;
    }

    QMutex queueMutex_;
    QWaitCondition queueCond_;
    QQueue<ThumbnailRequest> queue_;
    bool stop_ = false;
    MediaThumbnailer* owner_ = nullptr;
};

// =====================================================================
// MediaThumbnailer (main, GUI thread 側)
// =====================================================================

MediaThumbnailer::MediaThumbnailer(QObject* parent)
    : QObject(parent), worker_(new Worker(this)) {
    worker_->moveToThread(&workerThread_);
    QObject::connect(&workerThread_, &QThread::started,
                     worker_, &Worker::process);
}

MediaThumbnailer::~MediaThumbnailer() {
    stop();
    workerThread_.quit();
    workerThread_.wait();
    delete worker_;
}

void MediaThumbnailer::request(const ThumbnailRequest& req) {
    // cache に既にあれば即時 emit
    {
        QMutexLocker lock(&cacheMutex_);
        auto it = cache_.constFind(req.filePath);
        if (it != cache_.constEnd() && it.value().valid) {
            MediaThumbnail hit = it.value();
            // UI thread に post して emit
            QMetaObject::invokeMethod(this, [this, hit]() {
                Q_EMIT thumbnailReady(hit);
            }, Qt::QueuedConnection);
            return;
        }
    }
    worker_->enqueue(req);
}

void MediaThumbnailer::clearCache() {
    QMutexLocker lock(&cacheMutex_);
    cache_.clear();
}

void MediaThumbnailer::publishThumbnail(const MediaThumbnail& thumbnail)
{
    if (!thumbnail.valid || thumbnail.filePath.isEmpty()) return;

    {
        QMutexLocker lock(&cacheMutex_);
        cache_.insert(thumbnail.filePath, thumbnail);
    }
    Q_EMIT thumbnailReady(thumbnail);
}

MediaThumbnail MediaThumbnailer::cached(const QString& filePath) const {
    QMutexLocker lock(&cacheMutex_);
    auto it = cache_.constFind(filePath);
    if (it != cache_.constEnd()) {
        return it.value();
    }
    return MediaThumbnail{};
}

void MediaThumbnailer::start() {
    workerThread_.start();
}

void MediaThumbnailer::stop() {
    worker_->stop();
}

} // namespace ArtifactPr
