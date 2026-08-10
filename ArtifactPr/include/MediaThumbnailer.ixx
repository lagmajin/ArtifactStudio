module;

#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QWaitCondition>
#include <QtGlobal>

export module ArtifactPr.MediaThumbnailer;

export namespace ArtifactPr {

/// 1 つのメディアファイルから抽出した thumbnail。
/// QImage は IO / 互換境界でのみ使用 (taste 整合)。
/// MediaPanel 表示用には QPixmap 経由に変換される。
struct MediaThumbnail {
    QString filePath;
    QString cacheKey;
    QImage image;          // 32x18 〜 320x180 (request による)
    qint64 durationMs = 0; // 0 = 取得失敗
    qint64 seekToMs = 0;
    quint64 generation = 0;
    bool valid = false;
};

/// thumbnail 取得要求。
/// 同じ filePath／サイズ／seek 時刻の request は最新のみ処理する。
struct ThumbnailRequest {
    QString filePath;
    QSize targetSize = QSize(160, 90);
    qint64 seekToMs = 1000;  // この時刻付近のフレームを抽出
    quint64 generation = 0;  // MediaThumbnailer が request 時に付与する世代
};

/// 非同期 thumbnail 生成サービス。
///
/// QThread ベースで thumbnail を生成し、QObject::thread() == GUI thread で
/// thumbnailReady signal を emit する。
/// 重複 request は latest-wins 戦略で間引き、UI thread を保護する。
///
/// AGENTS.md 整合:
/// - QImage は IO / 互換境界のみ (生成結果を MediaPanel が QPixmap 化)
/// - 新規 connect は内部スレッド間のみ (UI thread への signal emit は既存 pattern)
class MediaThumbnailer : public QObject {
public:
    explicit MediaThumbnailer(QObject* parent = nullptr);
    ~MediaThumbnailer() override;

    /// thumbnail を非同期要求。同一 request key の古い要求は破棄される。
    void request(const ThumbnailRequest& req);

    /// cache を全消去 (project unload 等)。
    void clearCache();

    /// cache 内の thumbnail を取得 (同期)。無ければ invalid な struct を返す。
    MediaThumbnail cached(const QString& filePath) const;
    MediaThumbnail cached(const ThumbnailRequest& request) const;

    /// 起動 / 停止。
    void start();
    void stop();

Q_SIGNALS:
    /// thumbnail 生成完了。UI thread で呼ばれる。
    void thumbnailReady(ArtifactPr::MediaThumbnail thumb) const;

private:
    void publishThumbnail(const MediaThumbnail& thumbnail);

    class Worker;
    QThread workerThread_;
    Worker* worker_ = nullptr;

    mutable QMutex cacheMutex_;
    QHash<QString, MediaThumbnail> cache_;
    quint64 cacheGeneration_ = 0;
};

} // namespace ArtifactPr
