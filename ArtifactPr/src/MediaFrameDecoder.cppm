module;

#include <QDebug>
#include <QHash>
#include <QMetaObject>
#include <QMutexLocker>
#include <QQueue>
#include <QString>

#include <algorithm>
#include <map>
#include <utility>
#include <variant>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

export module ArtifactPr.MediaFrameDecoder;

import ArtifactPr.MediaFrameDecoder;
import Image.ImageF32x4_RGBA;
import Codec.FFmpegVideoDecoder;
import Video.VideoFrame;

namespace ArtifactPr {

// =====================================================================
// MediaFrameDecoder::Worker
// ---------------------------------------------------------------------
// 別スレッドで動く。request queue から FrameDecodeRequest を取り出し、
// 1 フレームずつデコードする。
//
// 同じ filePath の要求は latest-wins: queue 内の古い同一 path 要求は
// 破棄して、最新のものだけ処理する。
//
// 動画は cv::VideoCapture (FFmpeg backend)、静止画は cv::imread。
// VideoCapture は filePath ごとにキャッシュし、連続フレーム要求は
// seek なしの逐次 read() fast path で処理する。
// =====================================================================
class MediaFrameDecoder::Worker : public QObject {
public:
    explicit Worker(MediaFrameDecoder* owner)
        : QObject(nullptr), owner_(owner) {}

    void enqueue(const FrameDecodeRequest& req) {
        QMutexLocker lock(&queueMutex_);

        // latest-wins: 同じ filePath の request が queue にあれば除去
        auto it = std::remove_if(queue_.begin(), queue_.end(),
            [&req](const FrameDecodeRequest& r) {
                return r.filePath == req.filePath;
            });
        queue_.erase(it, queue_.end());

        queue_.enqueue(req);
        queueCond_.wakeOne();
    }

    void clearCache() {
        QMutexLocker lock(&queueMutex_);
        captures_.clear();
        lastDecodedFrame_.clear();
    }

    void stop() {
        QMutexLocker lock(&queueMutex_);
        stop_ = true;
        queueCond_.wakeAll();
    }

public Q_SLOTS:
    void process() {
        while (true) {
            FrameDecodeRequest req;
            {
                QMutexLocker lock(&queueMutex_);
                while (queue_.isEmpty() && !stop_) {
                    queueCond_.wait(&queueMutex_);
                }
                if (stop_) return;
                req = queue_.dequeue();
            }

            FrameDecodeResult result = decodeSync(req);

            // 結果を GUI thread へ戻す。callback は owner 側で管理。
            auto* owner = owner_;
            QMetaObject::invokeMethod(owner, [owner, result]() {
                owner->publishFrame(result);
            }, Qt::QueuedConnection);
        }
    }

private:
    /// 1 フレームを同期デコード。失敗時は valid=false の結果を返す。
    /// 動画は Core FFmpegVideoDecoder (RGB24 出力)、静止画は ImageF32x4_RGBA::load。
    FrameDecodeResult decodeSync(const FrameDecodeRequest& req) {
        FrameDecodeResult result;
        result.filePath = req.filePath;
        result.sourceFrame = req.sourceFrame;
        result.generation = req.generation;
        result.valid = false;

        if (req.stillImage) {
            ArtifactCore::ImageF32x4_RGBA image;
            if (!image.load(req.filePath)) {
                return result;
            }
            result.image = image;
            result.valid = true;
            return result;
        }

        if (!ensureCapture(req.filePath)) {
            return result;
        }
        ArtifactCore::FFmpegVideoDecoder& decoder = captures_[req.filePath];

        // 逐次読み fast path: 要求フレーム == 直前デコード+1。
        // ジャンプ時は seek + PTS ウォーク付きの exact-frame デコードを使う
        // （BACKWARD シークの着地点はキーフレームのため、単発 decodeNext では
        // 要求フレームの手前の絵を掴んでキャッシュがずれる）。
        const qint64 expected = lastDecodedFrame_.value(req.filePath, -1) + 1;
        const auto decoded = req.sourceFrame == expected
            ? decoder.decodeNextVideoFrameRaw()
            : decoder.decodeFrameAtRaw(req.sourceFrame);
        const auto* cpuFrame = std::get_if<ArtifactCore::CpuVideoFrame>(&decoded);
        if (!cpuFrame || !cpuFrame->isValid()) {
            // EOF 等で失敗したら cache を閉じて次回再オープン
            captures_.erase(req.filePath);
            lastDecodedFrame_.erase(req.filePath);
            return result;
        }
        lastDecodedFrame_[req.filePath] = req.sourceFrame;

        // Core デコーダ出力は RGB24 (stride 考慮) → canonical RGBA8 へ詰め替え
        cv::Mat rgb24(cpuFrame->meta.height, cpuFrame->meta.width, CV_8UC3,
                      cpuFrame->bytes.data(), cpuFrame->strideBytes);
        cv::Mat rgba8;
        cv::cvtColor(rgb24, rgba8, cv::COLOR_RGB2RGBA);
        if (rgba8.empty()) {
            return result;
        }

        ArtifactCore::ImageF32x4_RGBA image;
        image.setFromRGBA8(rgba8.data, rgba8.cols, rgba8.rows);
        result.image = image;
        result.valid = true;
        return result;
    }

    bool ensureCapture(const QString& filePath) {
        if (captures_.count(filePath) > 0) {
            return true;  // FFmpegVideoDecoder は open 済みフラグを内部管理しないため open 成功済みのみ保持する
        }
        ArtifactCore::FFmpegVideoDecoder decoder;
        if (!decoder.openFile(filePath)) {
            return false;
        }
        captures_.emplace(filePath, std::move(decoder));
        lastDecodedFrame_[filePath] = -1;
        return true;
    }

    QMutex queueMutex_;
    QWaitCondition queueCond_;
    QQueue<FrameDecodeRequest> queue_;
    bool stop_ = false;

    std::map<QString, ArtifactCore::FFmpegVideoDecoder> captures_;
    QHash<QString, qint64> lastDecodedFrame_;
    MediaFrameDecoder* owner_ = nullptr;
};

// =====================================================================
// MediaFrameDecoder (main, GUI thread 側)
// =====================================================================

MediaFrameDecoder::MediaFrameDecoder(QObject* parent)
    : QObject(parent), worker_(new Worker(this)) {
    worker_->moveToThread(&workerThread_);
    QObject::connect(&workerThread_, &QThread::started,
                     worker_, &Worker::process);
}

MediaFrameDecoder::~MediaFrameDecoder() {
    stop();
    workerThread_.quit();
    workerThread_.wait();
    delete worker_;
}

void MediaFrameDecoder::setFrameReadyCallback(FrameReadyCallback callback) {
    QMutexLocker lock(&callbackMutex_);
    callback_ = std::move(callback);
}

void MediaFrameDecoder::request(const FrameDecodeRequest& req) {
    worker_->enqueue(req);
}

void MediaFrameDecoder::clearCache() {
    worker_->clearCache();
}

void MediaFrameDecoder::start() {
    workerThread_.start();
}

void MediaFrameDecoder::stop() {
    worker_->stop();
}

void MediaFrameDecoder::publishFrame(const FrameDecodeResult& result) {
    FrameReadyCallback callback;
    {
        QMutexLocker lock(&callbackMutex_);
        callback = callback_;
    }
    if (callback) {
        callback(result);
    }
}

} // namespace ArtifactPr
