module;

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QThread>
#include <QWaitCondition>
#include <QtGlobal>

#include <functional>

#include <opencv2/core/mat.hpp>

export module ArtifactPr.MediaFrameDecoder;

import Image.ImageF32x4_RGBA;

export namespace ArtifactPr {

using FramePosition = int64_t;

/// 1 フレームのデコード結果。
/// image は canonical RGBA の ImageF32x4_RGBA (元解像度のまま)。
struct FrameDecodeResult {
    QString filePath;
    FramePosition sourceFrame = 0;
    ArtifactCore::ImageF32x4_RGBA image;
    quint64 generation = 0;
    bool valid = false;
};

/// 1 フレームのデコード要求。
/// sourceFrame はメディア先頭からのフレーム番号 (0 始まり)。
/// stillImage=true の場合は cv::imread で読み sourceFrame を無視する。
struct FrameDecodeRequest {
    QString filePath;
    FramePosition sourceFrame = 0;
    bool stillImage = false;
    quint64 generation = 0;
};

/// 非同期フレームデコードサービス。
///
/// cv::VideoCapture (FFmpeg backend) / cv::imread を worker thread 上で使い、
/// 指定フレームを canonical RGBA の ImageF32x4_RGBA として返す。
/// 同一 filePath の要求は latest-wins で間引く。
/// 完了は publishFrame 経由で GUI thread のコールバックに渡す
/// (signal/slot 接続は新規追加しない方針のため、std::function で受ける)。
class MediaFrameDecoder : public QObject {
public:
    /// GUI thread 上で呼ばれる完了コールバック。
    using FrameReadyCallback = std::function<void(const FrameDecodeResult&)>;

    explicit MediaFrameDecoder(QObject* parent = nullptr);
    ~MediaFrameDecoder() override;

    /// GUI thread 側コールバックを設定 (呼び出しは 1 回想定)。
    void setFrameReadyCallback(FrameReadyCallback callback);

    /// フレーム decode を非同期要求。同一 filePath の古い要求は破棄される。
    void request(const FrameDecodeRequest& req);

    /// decoder cache を全消去 (project unload 等)。
    void clearCache();

    /// 起動 / 停止。
    void start();
    void stop();

private:
    void publishFrame(const FrameDecodeResult& result);

    class Worker;
    QThread workerThread_;
    Worker* worker_ = nullptr;
    FrameReadyCallback callback_;
    mutable QMutex callbackMutex_;
};

} // namespace ArtifactPr
