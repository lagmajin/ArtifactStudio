# Milestone: Asset Browser Optimization (2026-03-29)

**Status:** Planning / Partial Implementation
**Goal:** アセットブラウザのフォルダ読み込み速度を劇的に向上させ、数千ファイルのディレクトリでも UI が固まらないようにする。

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
| インポートパス・セットの実装 | ⏳ 次回 | AI | ProjectService 連携 |
| `applyFilters` の TBB 化 | 📋 未着手 | AI | |
| サムネイル生成の並列化 | 📋 未着手 | AI | OpenCV 並列処理 |

## 2026-07-25 実装監査

- thumbnail の非同期生成・世代管理・memory／disk cache、sequence 検出、unused path のスナップショット走査など、周辺の性能改善は確認できる。
- 一方、import 済みパスを一括 `QSet` 化する O(1) lookup、`applyFilters` の TBB parallel scan、thumbnail warmup の TBB task_group 並列化は確認できない。
- したがって UI の負荷軽減に向けた部分実装はあるが、計画書の主要な三段階と数千ファイル時の性能目標は未検証であり、Planning／Partial Implementation を維持する。