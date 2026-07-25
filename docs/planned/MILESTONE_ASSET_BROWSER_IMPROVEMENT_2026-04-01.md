# Milestone: Asset Browser Improvement (2026-04-01)

**Status:** Phase 3 (P2) In Progress
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

実装済み: 非同期サムネイル／波形生成、ホバープレビュー、Size／Date／Typeソート、昇順／降順切替、ブレッドクラム、Favorites仮想ノード、FavoritesのQSettings永続化、All Favorites導線。
実装追加: コンテキストメニューの `Find References` で、プロジェクト内全Compositionの参照Layerを一覧表示。
実装追加: `Unused` フィルタをAsset Browserツールバーへ公開し、未使用アセット一覧へ直接切り替え可能。
実装追加: サムネイルをファイルパス・サイズ・更新時刻でキー化したPNGディスクキャッシュへ保存し、再起動後も再利用可能。
実装追加: Assetルート配下のファイルをフォルダ間で内部D&D移動でき、既存ファイルへの上書きとルート外移動を拒否。
実装追加: リネームと内部D&D移動を `MoveAssetFileCommand` としてUndo/Redo履歴へ登録。
未完了: インポート／削除／リリンクのUndo、ディスクキャッシュのLRU・有効期限管理。

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
- [x] フォルダ間でのファイル移動
- [x] Assetルート外・上書き・ディレクトリ移動を拒否
- [x] Undo 連携

### 3.3 ディスクサムネイルキャッシュ
- [x] ファイルベースのディスクキャッシュ
- [x] 再起動後もキャッシュ維持
- [x] 256 MiB上限と最終更新時刻ベースの古いキャッシュ削除

### 3.4 Undo 連携
- [x] リネーム／内部D&D移動を `UndoManager` に登録
- [x] 通常ファイルの「Add to Project」登録を `UndoManager` に登録
- [x] ファイル削除を退避付き `UndoManager` 操作として登録
- [x] フォルダ削除のファイルシステムUndo対応
- [ ] フォルダ内Footage登録のUndo/Redo再構築
- [x] リリンク操作を `UndoManager` に登録
- [ ] 一括操作のマクロコマンド化

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
