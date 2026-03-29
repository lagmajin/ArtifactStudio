# アセット管理改善 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** ArtifactAssetBrowser, ArtifactProjectManagerWidget, ArtifactProjectModel

---

## 概要

アセットブラウザとプロジェクト管理の機能を改善し、大規模プロジェクトでも効率的に作業できるようにする。

---

## 発見された問題点

### ★★★ 問題 1: 大量アセットでのパフォーマンス低下

**場所:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`

**問題:**
- 1000 件以上のアセットで UI が重い
- サムネイル読み込みが同期的
- フィルタリングが遅い

**影響:**
- 大規模プロジェクトで作業不能
- 起動に時間がかかる

**工数:** 10-14 時間

---

### ★★★ 問題 2: 未使用アセットの検出が不完全

**場所:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm:2814-2861`

**問題:**
- 参照カウントが正確でない
- 間接参照（ネストされたコンポジション）を検出できない
- 誤って「未使用」と表示される

**影響:**
- 必要なアセットを削除してしまうリスク
- 手動での確認が必要

**工数:** 8-10 時間

---

### ★★ 問題 3: サムネイルキャッシュの未整備

**場所:** 全体

**問題:**
- サムネイルが毎回生成される
- キャッシュの管理がない
- ディスク使用量が増加

**工数:** 6-8 時間

---

### ★★ 問題 4: メタデータの不足

**場所:** `Artifact/src/Project/ArtifactProjectModel.cppm`

**問題:**
- 解像度、フレームレート、 duration などの表示がない
- 検索メタデータが不足
- タグ付け機能がない

**工数:** 8-10 時間

---

### ★ 問題 5: バッチ操作の不足

**場所:** 全体

**問題:**
- 複数選択での操作が限定
- 一括リネームがない
- 一括タグ付けがない

**工数:** 6-8 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **パフォーマンス改善** | 10-14h | 🔴 高 |
| **未使用アセット検出** | 8-10h | 🔴 高 |

### P1（重要）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **サムネイルキャッシュ** | 6-8h | 🟡 中 |
| **メタデータ拡充** | 8-10h | 🟡 中 |

### P2（推奨）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **バッチ操作** | 6-8h | 🟢 低 |

**合計工数:** 38-50 時間

---

## Phase 構成

### Phase 1: パフォーマンス改善

- 目的:
  - 大量アセットでも軽快に動作

- 作業項目:
  - 仮想リストの実装
  - サムネイルの非同期読み込み
  - フィルタリングの最適化

- 完了条件:
  - 10000 件でも 60fps 維持
  - スクロールが滑らか

- 実装案:
  ```cpp
  // 仮想リストの実装
  class AssetBrowserView : public QListView {
      Q_OBJECT
      
  public:
      AssetBrowserView(QWidget* parent = nullptr)
          : QListView(parent) {
          setUniformItemSizes(true);  // 高速化
          setBatchSize(100);  // バッチ処理
          setLayoutMode(QListView::Batched);
          
          // サムネイルの非同期読み込み
          thumbnailLoader_ = new ThumbnailLoader();
          connect(thumbnailLoader_, &ThumbnailLoader::thumbnailLoaded,
                  this, [this](const QString& path, const QPixmap& thumb) {
                      updateThumbnail(path, thumb);
                  });
      }
      
  private:
      ThumbnailLoader* thumbnailLoader_;
  };
  
  class ThumbnailLoader : public QObject {
      Q_OBJECT
      
  public:
      ThumbnailLoader() {
          // ワーカースレッドで処理
          workerThread_ = new QThread();
          worker_ = new ThumbnailWorker();
          worker_->moveToThread(workerThread_);
          workerThread_->start();
          
          connect(this, &ThumbnailLoader::loadRequested,
                  worker_, &ThumbnailWorker::load);
          connect(worker_, &ThumbnailWorker::loaded,
                  this, &ThumbnailLoader::thumbnailLoaded);
      }
      
      void loadThumbnail(const QString& path) {
          emit loadRequested(path);
      }
      
  signals:
      void loadRequested(const QString& path);
      void thumbnailLoaded(const QString& path, const QPixmap& thumb);
      
  private:
      QThread* workerThread_;
      ThumbnailWorker* worker_;
  };
  ```

### Phase 2: 未使用アセット検出の改善

- 目的:
  - 正確な未使用アセット検出

- 作業項目:
  - 再帰的な参照チェック
  - 間接参照の検出
  - 参照カウントの正確化

- 完了条件:
  - 誤検出ゼロ
  - 大規模プロジェクトでも高速

- 実装案:
  ```cpp
  class AssetUsageChecker {
  public:
      struct UsageInfo {
          QString assetPath;
          QList<QString> usedInCompositions;
          QList<QString> usedInLayers;
          bool isUsed() const {
              return !usedInCompositions.isEmpty() || 
                     !usedInLayers.isEmpty();
          }
      };
      
      static QMap<QString, UsageInfo> checkUsage(
          ArtifactProject* project)
      {
          QMap<QString, UsageInfo> usageMap;
          
          // 全てのアセットを登録
          for (const auto& asset : project->allAssets()) {
              usageMap[asset->path()] = UsageInfo();
          }
          
          // 再帰的に使用箇所をチェック
          for (const auto& comp : project->allCompositions()) {
              checkCompositionUsage(comp, usageMap);
          }
          
          return usageMap;
      }
      
  private:
      static void checkCompositionUsage(
          const ArtifactCompositionPtr& comp,
          QMap<QString, UsageInfo>& usageMap)
      {
          for (const auto& layer : comp->allLayer()) {
              // 直接参照をチェック
              if (auto imageLayer = 
                  std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
                  QString path = imageLayer->sourcePath();
                  if (usageMap.contains(path)) {
                      usageMap[path].usedInLayers.append(
                          layer->layerName());
                  }
              }
              
              // PreComposition の場合、再帰的にチェック
              if (auto precompLayer = 
                  std::dynamic_pointer_cast<ArtifactPreCompositionLayer>(layer)) {
                  if (auto nestedComp = precompLayer->composition()) {
                      checkCompositionUsage(nestedComp, usageMap);
                  }
              }
          }
      }
  };
  ```

### Phase 3: サムネイルキャッシュ

- 目的:
  - 高速なサムネイル表示

- 作業項目:
  - ディスクキャッシュ
  - メモリキャッシュ
  - 自動削除

- 完了条件:
  - 2 回目以降は即時表示
  - キャッシュ容量を管理

### Phase 4: メタデータ拡充

- 目的:
  - 詳細な情報表示

- 作業項目:
  - 解像度、フレームレート、duration
  - コーデック情報
  - タグ付け

- 完了条件:
  - 主要メタデータを表示
  - メタデータで検索可能

### Phase 5: バッチ操作

- 目的:
  - 複数アセットの効率的な操作

- 作業項目:
  - 複数選択
  - 一括リネーム
  - 一括タグ付け

- 完了条件:
  - 100 件でも一括操作可能

---

## 技術的課題

### 1. 仮想リストの実装

**課題:**
- Qt の標準機能との互換性

**解決案:**
- QListView のカスタマイズ
- delegate の最適化

### 2. 参照チェックの性能

**課題:**
- 大規模プロジェクトで遅い

**解決案:**
- 並列処理
- キャッシュ
- 増分チェック

### 3. キャッシュ管理

**課題:**
- ディスク容量の増加

**解決案:**
- LRU による自動削除
- 容量制限
- 圧縮

---

## 期待される効果

### 性能向上

| 指標 | 現在 | 改善後 | 向上率 |
|------|------|--------|--------|
| **1000 件表示** | 5 秒 | 0.5 秒 | -90% |
| **サムネイル表示** | 同期 | 非同期 | - |
| **未使用検出** | 不完全 | 完全 | +100% |

### ユーザー体験

- 大規模プロジェクトも軽快
- 未使用アセットを安全に削除
- メタデータで素早く検索

---

## 関連ドキュメント

- `docs/planned/MILESTONE_APP_LAYER_IMPROVEMENTS_2026-03-28.md` - アプリ層改善
- `docs/planned/MILESTONE_UI_UX_UNIFICATION_2026-03-28.md` - UI/UX 統一

---

## 実装順序の推奨

1. **Phase 1: パフォーマンス改善** - 基盤
2. **Phase 2: 未使用検出** - 信頼性
3. **Phase 3: サムネイルキャッシュ** - 速度
4. **Phase 4: メタデータ** - 検索性
5. **Phase 5: バッチ操作** - 効率

---

**文書終了**
