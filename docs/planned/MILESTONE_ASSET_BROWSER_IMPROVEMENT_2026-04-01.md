# Milestone: Asset Browser Improvement (2026-04-01)

**Status:** Phase 1 (P0) Complete
**Goal:** アセットブラウザの基盤強化とUX改善

---

## Phase 1: P0 基盤改善

### 1.1 ファイルシステム監視
- `QFileSystemWatcher` でアセットディレクトリを監視
- ファイル追加/削除/変更を自動検知してリフレッシュ
- 監視対象: プロジェクトアセットルート + パッケージディレクトリ

### 1.2 TBB 並列サムネイル生成
- 現在のシングルスレッドバッチ (4枚/16ms) を TBB 並列化
- OpenCV 動画デコード、画像読み込みを `tbb::parallel_for` 化
- サムネイルキューのワーカースレッドプール化

### 1.3 インポートキャッシュ最適化
- `refreshImportedAssetCache()` の全ツリー走査を差分更新に変更
- `projectChanged` 時の増分スキャン
- `QSet` ベースの高速ルックアップ維持

**見積: 6h**

---

## Phase 2: P1 UX 向上

### 2.1 ブレッドクラムナビゲーション
- パスラベルをパンくずリストに変更
- 各セグメントクリックで即座に移動
- 現在のディレクトリ階層を視覚的に表示

### 2.2 ホバープレビューポップアップ
- 300ms 遅延で大きなプレビュー画像をポップアップ表示
- 画像/動画/音声のプレビュー対応
- ツールチップ形式でメタデータ併記

### 2.3 お気に入り機能
- `addFavorite`/`removeFavorite` の実装
- ツリーの仮想ノードとして「Favorites」セクション追加
- お気に入りの永続化（QSettings）

### 2.4 サイズ/日付ソート
- `AssetMenuItem` に `fileSize`/`modifiedTime` 追加
- ソートドロップダウンに Size / Date Modified 追加
- 昇順/降順切替

**見積: 8h**

---

## Phase 3: P2 機能拡張

### 3.1 Find References / Select Unused
- コンテキストメニューに「Find References」追加
- 「Select Unused」で未使用アセットをハイライト/フィルタ
- プロジェクト内参照先のリスト表示

### 3.2 内部 D&D 移動
- フォルダ間でのファイル移動
- 確認ダイアログ + ファイルシステム操作
- Undo 連携

### 3.3 ディスクサムネイルキャッシュ
- 有効期限付きディスクキャッシュ（ファイルベース）
- 再起動後もキャッシュ維持
- キャッシュサイズ制限とLRU削除

### 3.4 Undo 連携
- インポート/削除/リリンク操作を `UndoManager` に登録
- 一括操作のマクロコマンド化

**見積: 10h**

---

## Recommended Order

| 順序 | フェーズ | 見積 | 優先度 |
|---|---|---|---|
| 1 | **Phase 1: P0 基盤改善** | 6h | P0 |
| 2 | **Phase 2: P1 UX 向上** | 8h | P1 |
| 3 | **Phase 3: P2 機能拡張** | 10h | P2 |

**総見積: ~24h**

---

## 既存の関連ファイル

| ファイル | 内容 |
|---------|------|
| `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | メイン実装 (2543行) |
| `Artifact/src/Asset/AssetDirectoryModel.cppm` | ディレクトリツリーモデル |
| `Artifact/src/Asset/AssetMenuModel.cppm` | ファイルリストモデル |
