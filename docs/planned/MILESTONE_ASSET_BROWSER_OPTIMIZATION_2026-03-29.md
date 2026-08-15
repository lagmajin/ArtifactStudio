# Milestone: Asset Browser Optimization (2026-03-29)

**最終更新:** 2026-08-15
**Status:** 部分実装（パス判定・キャッシュ改善済み、並列スキャン／並列サムネイル生成は未着手）
**Goal:** アセットブラウザのフォルダ読み込み速度を劇的に向上させ、数千ファイルのディレクトリでも UI が固まらないようにする。

## 2026-08-15 現行コード監査

Asset Browser には imported path の `QSet` キャッシュ、thumbnail の memory／disk cache、mtime による stale 判定、generation token、512 件の memory 上限、status／type／search／sort の一覧処理が実装されている。Phase 1 とサムネイル再利用の基盤は現行コードに反映されている。

一方、`applyFilters` は UI 側の逐次走査のままで、TBB の `parallel_for` は確認できない。thumbnail warmup も timer から最大 4 件ずつ処理する方式で、TBB `task_group` による並列デコードは未確認。数千ファイル時の 0.1 秒目標、UI 非ブロック性、実測 hit rate は runtime 計測が必要である。

判定: **Phase 1 は実装済み、Phase 2〜3 は pending。**

---

## 現状の課題 (Bottleneck Analysis)

`ArtifactAssetBrowser::Impl::applyFilters` がディレクトリを開くたびに UI スレッドで実行されており、以下の要因で低速化している：

1.  **`isImportedAssetPath` の非効率な探索**:
    - ディレクトリ内の各ファイルについて、プロジェクトツリーを再帰探索している (O(N * M))。
    - 探索のたびに `QFileInfo::canonicalFilePath()` (ディスクIO) を呼び出している。
2.  **シングルスレッド処理**:
    - 大量ファイルのステータスチェック（インポート済みか、未使用か、欠落しているか）を 1 スレッドで行っているため、数千ファイルで数秒のフリーズが発生する。
3.  **サムネイル生成のシーケンシャル処理**:
    - 一度に 4 枚ずつ生成しているが、CPU コアが余っていても並列化されていない。

---

## 改善計画

### Phase 1: インポート情報のキャッシュ化 (O(N*M) -> O(N)) ✅ **実装検討中**
- `ArtifactProjectService` または `AssetBrowser` 側で、プロジェクト内の全インポート済みアセットの「正規化パス」を `QSet` に保持する。
- `isImportedAssetPath` はこの `QSet` を引くだけ (O(1)) に変更。

### Phase 2: TBB による並列スキャン (Parallel Filtering)
- `applyFilters` のループを `tbb::parallel_for` で実行。
- 各スレッドで `AssetMenuItem` を生成し、最後にメインスレッドで `assetModel` に流し込む。
- `isMissingAssetPath` (ディスク存在確認) も並列化の恩恵を受ける。

### Phase 3: TBB によるサムネイル並列生成
- `processThumbnailWarmupBatch` を `tbb::task_group` 化。
- OpenCV によるビデオフレーム抽出や、画像のデコードを複数コアで同時実行。

---

## 期待される効果

- 1000 ファイルのディレクトリ表示: 数秒 -> 0.1秒以下（ほぼ瞬時）
- 大規模プロジェクトでのフィルタリング: フリーズの解消
- 書き出し前の「未使用アセット検索」等の高速化

---

## 実装スケジュール

| 項目 | 状態 | 担当 | 備考 |
|------|------|------|------|
| インポートパス・セットの実装 | ✅ 実装済み (2026-07-30) | AI | ProjectService の Footage / sequence path を正規化して Asset Browser 側でキャッシュ |
| `applyFilters` の TBB 化 | 📋 未着手 | AI | |
| サムネイル生成の並列化 | 📋 未着手 | AI | OpenCV 並列処理 |

## 2026-07-25 実装監査

- thumbnail の非同期生成・世代管理・memory／disk cache、sequence 検出、unused path のスナップショット走査など、周辺の性能改善は確認できる。
- 一方、`applyFilters` の TBB parallel scan と thumbnail warmup の TBB `task_group` 並列化は確認できない。
- 2026-07-30 に imported Footage と sequence frame の正規化 path を `QSet` に構築し、`applyFilters()` 内の imported 判定をプロジェクトツリー再帰から O(1) lookup へ切り替えた。キャッシュは各一覧更新の先頭で再構築し、既存の `clearThumbnailCache()` 経路でも無効化するため、import／relink 後の status 表示が古いまま残らない。
- 同日、thumbnail cache にファイルの最終更新日時を併記し、cache hit 時に差し替え後の stale thumbnail を破棄して再生成するようにした。既存の memory／disk cache と非同期 generation token は維持する。
- disk cache は既存実装で absolute path・size・mtime を digest key に含め、30 日 TTL と容量上限も持つことを確認したため、memory cache との stale 判定の責務を重複させない。
- memory thumbnail cache に 512 entries の上限を追加し、一覧を大量に巡回しても `QHash` が無制限に増えないようにした。超過時は cache entry を退避し、disk cache は引き続き再利用できる。
- thumbnail diagnostics の `Ready` 判定も同じ mtime freshness helper を使うようにし、差し替え済みファイルを stale なまま Ready と表示しないようにした。
- `applyFilters` の TBB parallel scan、thumbnail warmup の TBB task_group 並列化、数千ファイル時の性能目標と runtime 計測は未検証であり、Planning／Partial Implementation を維持する。
