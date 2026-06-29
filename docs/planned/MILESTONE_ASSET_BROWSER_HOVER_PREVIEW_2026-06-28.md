# Milestone: Asset Browser Hover Preview (M-AB-4)

**マイルストーンID**: M-AB-4
**作成日**: 2026-06-28
**優先度**: P1 (High)
**推定工数**: 1-2日
**カテゴリ**: Asset Browser / UX / Preview
**状態**: Planned
**依存**: M-AB (Asset Browser base)

---

## 目的

アセットブラウザーにホバープレビュー機能を実装する。ユーザーがファイルにマウスをホバーすると、300msのディレイ後、大画面のプレビューがポップアップ表示される。これにより、サムネイルだけでは判断しにくいアセットの内容を素早く確認できる。

---

## 背景

### 現状
- アセットブラウザーにはサムネイル表示機能はあるが、小さいサイズ（デフォルト96px）では詳細な内容が確認しにくい
- 既存の`MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` (Phase 2.2) でホバープレビューの概念が言及されているが未実装
- `ArtifactAssetBrowser.cppm`では`generateThumbnail()`がサムネイル生成を担当
- `AssetMenuItem`構造体には`icon`フィールドがあるが、ホバープレビュー用の大画像は用意されていない

## 進捗メモ

- 2026-06-29: Asset Browser 側に 300ms 遅延の hover popup を追加し、既存 thumbnail / preview 生成をそのまま大きく見せる初期 slice を入れた
- 今後は image/video の大きめプレビュー精度、メタデータの充実、必要なら専用 popup の描画整理を進める

## Next Slice

- hover popup の表示条件を軽量化する
- `generateThumbnail()` 直呼びを避けて、既存 thumbnail cache / preview cache を優先する
- file move / mouse move ごとの再生成を抑えて、同一 path の再表示はキャッシュ再利用に寄せる
- popup の作成/破棄や `QTimer` 接続の再構成を減らす
- 画像以外の大きめ preview は、後段で専用 reader に分ける

### 要件
- 300msのディレイ後にプレビューポップアップを表示
- 画像、動画、音声ファイルに対応
- 画像: フルサイズまたは適切なサイズにスケールした静止画
- 動画: 最初のフレームまたはポスターフレーム
- 音声: 波形プレビュー
- ツールチップ形式でメタデータを併記表示
- マウスが離れると即座に閉じる
- プレビュー表示中はサムネイル生成をスキップ

### ユースケース
1. 多数の類似したサムネイルから特定のアセットを特定
2. 画像の細部（テキスト、ロゴ、色など）を確認
3. 動画ファイルの最初のフレームを確認
4. 音声ファイルの波形を確認
5. シーケンスアセットの代表フレームを確認

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **新規** | `Artifact/src/Widgets/Asset/ArtifactHoverPreviewPopup.cppm` | ホバープレビューポップアップウィジェット |
| **新規** | `Artifact/include/Widgets/Asset/ArtifactHoverPreviewPopup.ixx` | ヘッダー |
| **新規** | `Artifact/src/Widgets/Asset/HoverPreviewManager.cppm` | プレビュー管理クラス |
| **新規** | `Artifact/include/Widgets/Asset/HoverPreviewManager.ixx` | ヘッダー |
| **変更** | `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | ホバーイベント処理、マウス追跡 |
| **変更** | `Artifact/include/Widgets/ArtifactAssetBrowser.ixx` | シグナル/スロット追加 |
| **新規** | `ArtifactCore/include/Asset/HoverPreviewCache.ixx` | プレビューキャッシュ |
| **新規** | `ArtifactCore/src/Asset/HoverPreviewCache.cppm` | プレビューキャッシュ実装 |

---

## 変更詳細

### 1. HoverPreviewCache - プレビューキャッシュ

**新規ファイル**: `ArtifactCore/include/Asset/HoverPreviewCache.ixx`

```cpp
module;
#include <string>
#include <memory>
#include <unordered_map>

export module Asset.HoverPreviewCache;

import std;
import QImage;

namespace ArtifactCore {

/**
 * @brief ホバープレビュー用の画像キャッシュ
 *サムネイルより高解像度のプレビュー画像をキャッシュする
 */
export class HoverPreviewCache {
public:
    static HoverPreviewCache& instance();
    
    // キャッシュ操作
    bool hasPreview(const std::string& path) const;
    QImage getPreview(const std::string& path) const;
    void setPreview(const std::string& path, const QImage& image);
    void removePreview(const std::string& path);
    void clear();
    void clearUnused(int maxSize = 100);
    
    // 非同期読み込み
    void loadPreviewAsync(const std::string& path, std::function<void(const QImage&)> callback);
    void cancelLoad(const std::string& path);
    
    // 設定
    void setMaxCacheSize(int size);
    void setPreviewSize(const QSize& size);
    QSize previewSize() const;
    
private:
    HoverPreviewCache();
    ~HoverPreviewCache();
    
    HoverPreviewCache(const HoverPreviewCache&) = delete;
    HoverPreviewCache& operator=(const HoverPreviewCache&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
```

**新規ファイル**: `ArtifactCore/src/Asset/HoverPreviewCache.cppm`

```cpp
module;
#include <QImage>
#include <QSize>
#include <QCache>
#include <QFuture>
#include <QFutureWatcher>
#include <mutex>
#include <unordered_map>
#include <list>

import Asset.HoverPreviewCache;

namespace ArtifactCore {

class HoverPreviewCache::Impl {
public:
    QCache<std::string, QImage> cache_;
    QSize previewSize_ = QSize(512, 512);
    std::mutex mutex_;
    std::unordered_map<std::string, QFuture<QImage>> loadingFutures_;
    std::list<std::string> lruList_;
};

HoverPreviewCache::HoverPreviewCache() : impl_(std::make_unique<Impl>()) {
    impl_->cache_.setMaxCost(100); // 最大100画像
}

HoverPreviewCache::~HoverPreviewCache() = default;

HoverPreviewCache& HoverPreviewCache::instance() {
    static HoverPreviewCache instance;
    return instance;
}

bool HoverPreviewCache::hasPreview(const std::string& path) const {
    return impl_->cache_.contains(path);
}

QImage HoverPreviewCache::getPreview(const std::string& path) const {
    if (auto* image = impl_->cache_.object(path)) {
        return *image;
    }
    return QImage();
}

void HoverPreviewCache::setPreview(const std::string& path, const QImage& image) {
    if (image.isNull()) return;
    
    std::lock_guard lock(impl_->mutex_);
    impl_->cache_.insert(path, new QImage(image.scaled(
        impl_->previewSize_, 
        Qt::KeepAspectRatio, 
        Qt::SmoothTransformation
    )));
    
    // LRU更新
    impl_->lruList_.remove(path);
    impl_->lruList_.push_front(path);
    
    // 古いエントリを削除
    while (impl_->lruList_.size() > impl_->cache_.maxCost()) {
        auto& oldest = impl_->lruList_.back();
        impl_->cache_.remove(oldest);
        impl_->lruList_.pop_back();
    }
}

void HoverPreviewCache::removePreview(const std::string& path) {
    impl_->cache_.remove(path);
    impl_->lruList_.remove(path);
}

void HoverPreviewCache::clear() {
    impl_->cache_.clear();
    impl_->lruList_.clear();
}

void HoverPreviewCache::clearUnused(int maxSize) {
    // 実装は省略
}

void HoverPreviewCache::setMaxCacheSize(int size) {
    impl_->cache_.setMaxCost(size);
}

void HoverPreviewCache::setPreviewSize(const QSize& size) {
    impl_->previewSize_ = size;
}

QSize HoverPreviewCache::previewSize() const {
    return impl_->previewSize_;
}

void HoverPreviewCache::loadPreviewAsync(const std::string& path, std::function<void(const QImage&)> callback) {
    std::lock_guard lock(impl_->mutex_);
    
    // 既にキャッシュにある場合
    if (hasPreview(path)) {
        if (callback) callback(getPreview(path));
        return;
    }
    
    // 既に読み込み中の場合
    if (impl_->loadingFutures_.find(path) != impl_->loadingFutures_.end()) {
        // コールバックを登録
        return;
    }
    
    // 非同期で読み込み
    auto future = QtConcurrent::run([path]() {
        // TODO: 画像ロードロジック（WIC/OIIO/FFmpeg対応）
        return QImage();
    });
    
    auto watcher = new QFutureWatcher<QImage>();
    watcher->setFuture(future);
    
    QObject::connect(watcher, &QFutureWatcher<QImage>::finished, [this, path, callback, watcher]() {
        std::lock_guard lock(impl_->mutex_);
        QImage result = watcher->result();
        if (!result.isNull()) {
            setPreview(path, result);
        }
        impl_->loadingFutures_.erase(path);
        watcher->deleteLater();
        if (callback) callback(result);
    });
    
    impl_->loadingFutures_[path] = future;
}

void HoverPreviewCache::cancelLoad(const std::string& path) {
    std::lock_guard lock(impl_->mutex_);
    auto it = impl_->loadingFutures_.find(path);
    if (it != impl_->loadingFutures_.end()) {
        it->second.cancel();
        impl_->loadingFutures_.erase(it);
    }
}

} // namespace ArtifactCore
```

### 2. HoverPreviewPopup - プレビューポップアップウィジェット

**新規ファイル**: `Artifact/include/Widgets/Asset/ArtifactHoverPreviewPopup.ixx`

```cpp
module;
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <memory>

export module Widgets.Asset.HoverPreviewPopup;

import std;
import QImage;
import Asset.HoverPreviewCache;

namespace Artifact {

class ArtifactHoverPreviewPopupImpl;

/**
 * @brief ホバープレビューを表示するポップアップウィジェット
 *ツールチップのようにマウスに追従して表示される
 */
export class ArtifactHoverPreviewPopup : public QWidget {
    Q_OBJECT
    W_OBJECT(ArtifactHoverPreviewPopup)

public:
    explicit ArtifactHoverPreviewPopup(QWidget* parent = nullptr);
    ~ArtifactHoverPreviewPopup();
    
    // プレビューを設定
    void setPreviewImage(const QImage& image);
    void setPreviewPath(const std::string& path);
    
    // 情報を設定
    void setFileName(const QString& name);
    void setFileInfo(const QString& info);
    
    // 位置を更新
    void updatePosition(const QPoint& globalPos);
    
    // 表示/非表示
    void showPreview(const QPoint& globalPos);
    void hidePreview();
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    void loadPreviewIfNeeded();
    
    std::unique_ptr<ArtifactHoverPreviewPopupImpl> impl_;
};

} // namespace Artifact
```

**新規ファイル**: `Artifact/src/Widgets/Asset/ArtifactHoverPreviewPopup.cppm`

```cpp
module;
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QTimer>
#include <QFileInfo>

import Widgets.Asset.HoverPreviewPopup;
import Asset.HoverPreviewCache;

namespace Artifact {

class ArtifactHoverPreviewPopupImpl {
public:
    QLabel* imageLabel_ = nullptr;
    QLabel* nameLabel_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QTimer* hideTimer_ = nullptr;
    std::string currentPath_;
    bool isLoading_ = false;
};

ArtifactHoverPreviewPopup::ArtifactHoverPreviewPopup(QWidget* parent)
    : QWidget(parent, Qt::ToolTip), impl_(std::make_unique<Impl>()) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    
    // レイアウト
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    
    impl_->imageLabel_ = new QLabel(this);
    impl_->imageLabel_->setAlignment(Qt::AlignCenter);
    impl_->imageLabel_->setMinimumSize(256, 256);
    impl_->imageLabel_->setMaximumSize(512, 512);
    impl_->imageLabel_->setStyleSheet("background-color: #333;");
    layout->addWidget(impl_->imageLabel_);
    
    // ファイル情報
    auto* infoLayout = new QHBoxLayout();
    infoLayout->setSpacing(8);
    
    impl_->nameLabel_ = new QLabel(this);
    impl_->nameLabel_->setStyleSheet("color: #fff; font-weight: bold;");
    infoLayout->addWidget(impl_->nameLabel_);
    
    impl_->infoLabel_ = new QLabel(this);
    impl_->infoLabel_->setStyleSheet("color: #aaa;");
    infoLayout->addWidget(impl_->infoLabel_);
    infoLayout->addStretch();
    
    layout->addLayout(infoLayout);
    
    // タイマー
    impl_->hideTimer_ = new QTimer(this);
    impl_->hideTimer_->setSingleShot(true);
    impl_->hideTimer_->setInterval(100);
    connect(impl_->hideTimer_, &QTimer::timeout, this, &ArtifactHoverPreviewPopup::hide);
    
    setVisible(false);
}

ArtifactHoverPreviewPopup::~ArtifactHoverPreviewPopup() = default;

void ArtifactHoverPreviewPopup::setPreviewImage(const QImage& image) {
    if (image.isNull()) return;
    impl_->imageLabel_->setPixmap(QPixmap::fromImage(image));
    impl_->isLoading_ = false;
}

void ArtifactHoverPreviewPopup::setPreviewPath(const std::string& path) {
    if (impl_->currentPath_ == path) return;
    impl_->currentPath_ = path;
    impl_->isLoading_ = true;
    loadPreviewIfNeeded();
}

void ArtifactHoverPreviewPopup::setFileName(const QString& name) {
    impl_->nameLabel_->setText(name);
}

void ArtifactHoverPreviewPopup::setFileInfo(const QString& info) {
    impl_->infoLabel_->setText(info);
}

void ArtifactHoverPreviewPopup::updatePosition(const QPoint& globalPos) {
    // 画面の端に表示しないように位置調整
    QRect screenRect = QApplication::desktop()->screenGeometry();
    QPoint pos = globalPos;
    
    // ポップアップサイズを考慮
    QSize popupSize = sizeHint();
    if (popupSize.isEmpty()) popupSize = QSize(300, 300);
    
    // 右端を超える場合
    if (pos.x() + popupSize.width() > screenRect.right()) {
        pos.setX(screenRect.right() - popupSize.width() - 10);
    }
    
    // 下端を超える場合
    if (pos.y() + popupSize.height() > screenRect.bottom()) {
        pos.setY(screenRect.bottom() - popupSize.height() - 10);
    }
    
    move(pos);
}

void ArtifactHoverPreviewPopup::showPreview(const QPoint& globalPos) {
    updatePosition(globalPos);
    
    if (impl_->isLoading_) {
        // ローディング中は占位符を表示
        impl_->imageLabel_->setText("Loading...");
    }
    
    show();
    raise();
    activateWindow();
}

void ArtifactHoverPreviewPopup::hidePreview() {
    hide();
    impl_->currentPath_.clear();
}

void ArtifactHoverPreviewPopup::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    
    // 枠線を描画
    QPainter painter(this);
    painter.setPen(QColor(150, 150, 150));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ArtifactHoverPreviewPopup::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    impl_->hideTimer_->start();
}

void ArtifactHoverPreviewPopup::loadPreviewIfNeeded() {
    if (impl_->currentPath_.empty()) return;
    
    auto& cache = ArtifactCore::HoverPreviewCache::instance();
    
    if (cache.hasPreview(impl_->currentPath_)) {
        setPreviewImage(cache.getPreview(impl_->currentPath_));
    } else {
        // 非同期でロード
        cache.loadPreviewAsync(impl_->currentPath_, [this](const QImage& image) {
            if (!image.isNull()) {
                setPreviewImage(image);
            }
        });
    }
}

} // namespace Artifact
```

### 3. HoverPreviewManager - プレビューマネージャー

**新規ファイル**: `Artifact/include/Widgets/Asset/HoverPreviewManager.ixx`

```cpp
module;
#include <QObject>
#include <QPoint>
#include <memory>

export module Widgets.Asset.HoverPreviewManager;

import std;

namespace Artifact {

class ArtifactHoverPreviewPopup;

class HoverPreviewManagerImpl;

/**
 * @brief ホバープレビューを管理するシングルトンクラス
 */
export class HoverPreviewManager : public QObject {
    Q_OBJECT
public:
    static HoverPreviewManager& instance();
    
    // プレビューを表示
    void showPreview(const QString& path, const QPoint& globalPos, 
                     const QString& name = QString(), const QString& info = QString());
    
    // プレビューを非表示
    void hidePreview();
    
    // 設定
    void setDelay(int milliseconds);
    void setPreviewSize(const QSize& size);
    
    // 状態
    bool isPreviewVisible() const;
    
signals:
    void previewRequested(const QString& path);
    void previewShown(const QString& path);
    void previewHidden();
    
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    
private:
    HoverPreviewManager();
    ~HoverPreviewManager();
    
    HoverPreviewManager(const HoverPreviewManager&) = delete;
    HoverPreviewManager& operator=(const HoverPreviewManager&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
```

**新規ファイル**: `Artifact/src/Widgets/Asset/HoverPreviewManager.cppm`

```cpp
module;
#include <QObject>
#include <QPoint>
#include <QTimer>
#include <QEvent>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QApplication>

import Widgets.Asset.HoverPreviewManager;
import Widgets.Asset.HoverPreviewPopup;
import Asset.HoverPreviewCache;

namespace Artifact {

class HoverPreviewManager::Impl {
public:
    ArtifactHoverPreviewPopup* popup_ = nullptr;
    QTimer* showTimer_ = nullptr;
    QString currentPath_;
    QPoint lastPos_;
    int delay_ = 300; // 300ms
};

HoverPreviewManager::HoverPreviewManager() : impl_(std::make_unique<Impl>()) {
    impl_->popup_ = new ArtifactHoverPreviewPopup();
    impl_->showTimer_ = new QTimer(this);
    impl_->showTimer_->setSingleShot(true);
    
    connect(impl_->showTimer_, &QTimer::timeout, [this]() {
        if (!impl_->currentPath_.isEmpty()) {
            impl_->popup_->showPreview(impl_->lastPos_);
            emit previewShown(impl_->currentPath_);
        }
    });
}

HoverPreviewManager::~HoverPreviewManager() {
    delete impl_->popup_;
}

HoverPreviewManager& HoverPreviewManager::instance() {
    static HoverPreviewManager instance;
    return instance;
}

void HoverPreviewManager::showPreview(const QString& path, const QPoint& globalPos, 
                                       const QString& name, const QString& info) {
    if (path.isEmpty()) {
        hidePreview();
        return;
    }
    
    if (impl_->currentPath_ == path && impl_->popup_->isVisible()) {
        impl_->popup_->updatePosition(globalPos);
        return;
    }
    
    impl_->currentPath_ = path;
    impl_->lastPos_ = globalPos;
    
    impl_->popup_->setPreviewPath(path.toStdString());
    impl_->popup_->setFileName(name);
    impl_->popup_->setFileInfo(info);
    
    emit previewRequested(path);
    impl_->showTimer_->start(impl_->delay_);
}

void HoverPreviewManager::hidePreview() {
    impl_->showTimer_->stop();
    impl_->popup_->hidePreview();
    impl_->currentPath_.clear();
    emit previewHidden();
}

void HoverPreviewManager::setDelay(int milliseconds) {
    impl_->delay_ = milliseconds;
}

void HoverPreviewManager::setPreviewSize(const QSize& size) {
    impl_->popup_->setPreviewSize(size);
    ArtifactCore::HoverPreviewCache::instance().setPreviewSize(size);
}

bool HoverPreviewManager::isPreviewVisible() const {
    return impl_->popup_->isVisible();
}

bool HoverPreviewManager::eventFilter(QObject* watched, QEvent* event) {
    // マウスイベントを監視
    if (event->type() == QEvent::Enter) {
        auto* enterEvent = static_cast<QEnterEvent*>(event);
        // 対象ウィジェットに入ったらプレビューを表示
    } else if (event->type() == QEvent::Leave) {
        hidePreview();
    } else if (event->type() == QEvent::MouseMove) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        // マウス移動中は位置を更新
    }
    
    return QObject::eventFilter(watched, event);
}

} // namespace Artifact
```

### 4. ArtifactAssetBrowser への統合

**ファイル**: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

```cpp
// Impl クラスにフィールド追加
class ArtifactAssetBrowser::Impl {
    // ... 既存フィールド
    
    // ホバープレビュー
    QPoint lastHoverPos_;
    QString lastHoverPath_;
    QTimer* hoverTimer_ = nullptr;  // 300msディレイ用
};

// コンストラクタでタイマー初期化
ArtifactAssetBrowser::Impl::Impl(ArtifactAssetBrowser* self)
    : // ... 既存初期化
{
    hoverTimer_ = new QTimer(self);
    hoverTimer_->setSingleShot(true);
    hoverTimer_->setInterval(300);
    
    connect(hoverTimer_, &QTimer::timeout, [this, self]() {
        auto& manager = HoverPreviewManager::instance();
        manager.showPreview(lastHoverPath_, lastHoverPos_, 
                           QFileInfo(lastHoverPath_).fileName());
    });
}

// マウスムーブイベントハンドラー
bool ArtifactAssetBrowser::Impl::handleMouseMoveEvent(QMouseEvent* event) {
    // アイテムビュー上のマウス位置を取得
    if (fileView_) {
        QPoint pos = fileView_->viewport()->mapToGlobal(event->pos());
        QModelIndex index = fileView_->indexAt(event->pos());
        
        if (index.isValid()) {
            auto item = assetModel_->itemAt(index.row());
            QString path = QString::fromUtf8(item.path.c_str());
            
            if (path != lastHoverPath_) {
                lastHoverPath_ = path;
                lastHoverPos_ = pos;
                
                // タイマーを再起動
                hoverTimer_->start();
                
                // 古いプレビューをキャンセル
                HoverPreviewManager::instance().hidePreview();
            }
        } else {
            // アイテムの外に出たらプレビューを隠す
            lastHoverPath_.clear();
            hoverTimer_->stop();
            HoverPreviewManager::instance().hidePreview();
        }
    }
    
    return false;
}

// 非表示時にプレビューを隠す
void ArtifactAssetBrowser::Impl::handleLeaveEvent(QEvent* event) {
    hoverTimer_->stop();
    HoverPreviewManager::instance().hidePreview();
    lastHoverPath_.clear();
}
```

---

## タスク分割 (優先度付き)

### 優先度レベル
- **P0 (Critical)**: コア機能、なければ動作しない
- **P1 (High)**: 主要機能、ないと使い勝手が悪い
- **P2 (Medium)**: 便利機能、あってもなくても動作する
- **P3 (Low)**: 見栄え/UX向上、なくても機能する

### Phase 1: 核心基盤 (0.5日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `HoverPreviewCache` クラス設計 | P0 | 1h | なし | ✅ |
| `HoverPreviewCache::Impl` 実装 | P0 | 2h | 上記 | ✅ |
|`loadPreviewAsync` 非同期ロード実装 | P0 | 2h | 上記 | ✅ |
| キャッシュ管理（LRU、サイズ制限） | P0 | 1h | 上記 | ✅ |

### Phase 2: UIコンポーネント (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `ArtifactHoverPreviewPopup` クラス設計 | P1 | 1h | Phase 1 | ✅ |
| `ArtifactHoverPreviewPopup::Impl` 実装 | P1 | 2h | 上記 | ✅ |
| ポップアップ表示/非表示ロジック | P1 | 1h | 上記 | ✅ |
| 画像表示とメタデータ表示 | P1 | 1h | 上記 | ✅ |

### Phase 3: マネージャー (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `HoverPreviewManager` クラス設計 | P1 | 1h | Phase 2 | ✅ |
| `HoverPreviewManager::Impl` 実装 | P1 | 2h | 上記 | ✅ |
| ディレイ管理と表示制御 | P1 | 1h | 上記 | ✅ |
| シングルトンパターン実装 | P1 | 0.5h | 上記 | ✅ |

### Phase 4: AssetBrowser 統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `ArtifactAssetBrowser::Impl` にフィールド追加 | P1 | 0.5h | Phase 3 | ❌ (UIスレッド) |
| マウスムーブイベント処理 | P1 | 2h | 上記 | ❌ (UIスレッド) |
| 表示/非表示タイミング制御 | P1 | 1h | 上記 | ❌ (UIスレッド) |
| `fileView_` にイベントフィルタ追加 | P1 | 1h | 上記 | ❌ (UIスレッド) |

### Phase 5: 画像ロード拡張 (0.5日) - **P2**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| WIC対応のプレビューロード | P2 | 2h | Phase 1 | ✅ |
| OIIO対応のプレビューロード | P2 | 2h | Phase 1 | ✅ |
| FFmpeg対応（動画最初のフレーム） | P2 | 2h | Phase 1 | ✅ |
| 波形プレビュー（音声ファイル） | P2 | 2h | Phase 1 | ✅ |
| シーケンスの代表フレーム選択 | P2 | 1h | Phase 1 | ✅ |

### 並行作業可能性
- **Phase 1 (Core)**: 独立して並行可能
- **Phase 2 (UI)**: Phase 1完了後、独立して並行可能
- **Phase 3 (Manager)**: Phase 2完了後、独立して並行可能
- **Phase 4 (Integration)**: UIスレッド依存のため、基本的には直列実施
- **Phase 5 (Image Loading)**: Phase 1完了後、独立して並行可能

---

## テスト戦略

### 1. 自動テスト (Unit Tests)

#### P0 - 必須テスト (全てパスすること)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `HoverPreviewCache::setPreview`/`getPreview` | 自動 | 正しく保存/復元できる | 1h |
| `HoverPreviewCache::hasPreview` | 自動 | 存在チェックが正しい | 0.5h |
| `HoverPreviewCache::clear` | 自動 | キャッシュがクリアされる | 0.5h |
| `HoverPreviewCache::loadPreviewAsync` | 自動 | 非同期ロードが完了する | 1h |
| `HoverPreviewCache::cancelLoad` | 自動 | ロードがキャンセルされる | 0.5h |
| LRU削除テスト | 自動 | 最大サイズを超えると古いエントリが削除 | 1h |

#### P1 - 主要テスト (可能な限りパス)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `HoverPreviewPopup::showPreview` | 自動 | ポップアップが表示される | 1h |
| `HoverPreviewPopup::hidePreview` | 自動 | ポップアップが隠される | 0.5h |
| `HoverPreviewPopup::updatePosition` | 自動 | 位置が画面内に調整される | 1h |
| `HoverPreviewManager::showPreview` | 自動 | ディレイ後に表示 | 1h |
| `HoverPreviewManager::hidePreview` | 自動 | 即座に隠される | 0.5h |
| マウスムーブイベント処理 | 自動 | ホバー中にタイマーが起動 | 1h |

#### P2 - 統合テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| 画像ファイルのプレビュー表示 | 手動 | 正しく表示される | 1h |
| 動画ファイルのプレビュー表示 | 手動 | 最初のフレームが表示される | 1h |
| 音声ファイルの波形表示 | 手動 | 波形が表示される | 1h |
| シーケンスのプレビュー表示 | 手動 | 代表フレームが表示される | 1h |

### 2. 手動テスト (Manual Tests)

#### UXテスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| 300msディレイ後の表示 | 適切なタイミングで表示 | 1h |
| マウス離れた際の非表示 | 即座に隠される | 0.5h |
| 画面端での位置調整 | 画面外に表示されない | 1h |
| 複数アイテム間の移動 | プレビューが切り替わる | 1h |
| 素早いマウス移動時の追従 | なめらかに追従 | 1h |

#### 視覚的テスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| プレビュー画質 | 高解像度で表示 | 1h |
| プレビューサイズ | 適切なサイズ | 0.5h |
| メタデータ表示 | 分かりやすい | 0.5h |
| ローディング表示 | 見やすい | 0.5h |

### 3. テスト実行計画

#### Phase 1: Unit Tests (Cache)
```
日数: 0.5日
対象: HoverPreviewCache クラス
方法: Google Test / Catch2
実行: CIパイプラインで自動実行
```

#### Phase 2: Unit Tests (UI Components)
```
日数: 0.5日
対象: HoverPreviewPopup + HoverPreviewManager
方法: 自動テスト + 手動テスト
実行: CIパイプライン (自動) + QAチーム (手動)
```

#### Phase 3: Integration Tests
```
日数: 0.5日
対象: AssetBrowser 統合
方法: 手動テスト
実行: QAチーム
```

#### Phase 4: UX Tests
```
日数: 0.5日
対象: 完成品
方法: 手動テスト
実行: UXデザイナー + QAチーム
```

### 4. テスト完了基準
- [ ] P0全てのテストがパス
- [ ] P1の80%以上のテストがパス
- [ ] 重大なバグなし
- [ ] プレビュー表示がなめらか
- [ ] 300msディレイが適切
- [ ] 画面端でクリッピングなし
- [ ] 全てのファイルタイプに対応

---

## 成果物

1. **コア機能**: ホバープレビューの表示/非表示
2. **API**: `HoverPreviewCache` クラス
3. **UIコンポーネント**: `ArtifactHoverPreviewPopup`
4. **管理クラス**: `HoverPreviewManager`
5. **統合**: AssetBrowser への統合
6. **画像ロード**: 画像/動画/音声/シーケンス対応

---

## 依存関係

### 必要な前提条件
- **M-AB (Asset Browser 基盤)** - 基本的なAsset Browser機能
- **`ArtifactAssetBrowser.cppm`** - 統合先
- **`AssetMenuItem`** - アイテム情報取得

### 連携する機能
- `HoverPreviewCache` - プレビューキャッシュ管理
- `HoverPreviewPopup` - ポップアップUI
- `HoverPreviewManager` - 表示制御
- `ArtifactAssetBrowser` - ホストアプリケーション

### 影響を受ける機能
- `fileView_` のマウスイベント処理
-サムネイル生成の優先度（ホバー中はサムネイル生成をスキップ）

---

## リスクと対策

### リスク1: パフォーマンス低下
**内容**: 多数のプレビューロードでパフォーマンスが低下
**対策**:
- 非同期ロードを実装
- キャッシュサイズに上限を設ける（100画像）
- LRUアルゴリズムで古いエントリを自動削除
- ロード中はプレビューを表示せず、完了後に表示

### リスク2: メモリ使用量
**内容**: 高解像度プレビューによるメモリ消費
**対策**:
- プレビューサイズを512x512に制限
- 最大キャッシュ数を100に制限
- 画像をアップスケールしない（元画像サイズが小さい場合は拡大）

### リスク3: UIのちらつき
**内容**: 素早いマウス移動時にプレビューがちらつく
**対策**:
- 300msのディレイを設ける
- 移動中は古いプレビューをすぐに隠す
- 新しいプレビューはディレイ後に表示

### リスク4: 画面外表示
**内容**: 画面の端でプレビューが半分表示される
**対策**:
- 画面サイズを取得し、位置を調整
- プレビューサイズを考慮した位置計算
- 画面端にマージンを設ける

---

## テスト項目

- [ ] 画像ファイルのホバープレビュー表示
- [ ] 動画ファイルのホバープレビュー表示
- [ ] 音声ファイルの波形プレビュー表示
- [ ] シーケンスのホバープレビュー表示
- [ ] 300msディレイ後の表示
- [ ] マウス離れた際の非表示
- [ ] 画面端での位置調整
- [ ] 複数アイテム間の移動
- [ ] ローディング中の表示
- [ ] プレビューキャッシュの動作
- [ ] メモリ使用量の制限

---

## 完了基準

- [ ] 全てのファイルタイプに対応したプレビュー表示
- [ ] 300msディレイが適切に動作
- [ ] 画面端でクリッピングなし
- [ ] パフォーマンスに影響なし
- [ ] メモリ使用量が制限内
- [ ] 全てのテスト項目がパス
- [ ] UXテストで問題なし
- [ ] ドキュメントが更新

---

## 関連文書

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` - 統合先メインファイル
- `Artifact/include/Asset/AssetMenuModel.ixx` - アイテム構造
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` - 元のロードマップ
- `docs/planned/MILESTONES_BACKLOG.md` - マイルストーンバックログ

---

## メモ

- ホバープレビューはユーザーの作業効率を大幅に向上させる
- 既存のサムネイル機能との整合性を保つ
- 将来的には、プレビューをカスタマイズ可能にする（サイズ、ディレイ時間など）
- タッチデバイス対応は将来の拡張として検討
