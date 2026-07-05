# マイルストーン: アセットブラウザー デザイン監査 (2026-07-04)

> 作成: 2026-07-04
> 元依頼: 「アセットブラウザーのデザインをもっと見やすくできるか、問題点は？」

## 監査サマリー

`ArtifactAssetBrowser.cppm`（3,500行）のコードと、既存の計画文書群を横断的に分析した結果をまとめる。基本機能は揃っているが、Unity/Blender 級のアセットブラウザーと比較すると、以下の領域で見劣りする。

---

## 🔴 P0（最優先）: 左ペイン「Library Hub」の情報密度不足

### 問題
- `currentPathLabel_` / `leftHubSummaryLabel_` / `leftHubRecentLabel_` / `leftHubSelectionLabel_` が単なる QLabel の縦積み。文字情報だけなので「何が危ないか」が瞬時に読めない。
- 状態バッジ（Imported / Missing / Unused / Favorite）が Type 文字列（`"Favorite • Missing • PNG"`）に埋め込まれているだけで、chip/badge による視覚的ハイライトがない。
- `recentFolderButtons_` は最大3つの QToolButton。仮想カテゴリ（Recent/Favorites/All Images/Missing/Unused）のクリック1回で飛べる入口がない。
- プロジェクト未ロード時は「Open a folder to browse assets」というテキストのみ。素材ハブとしての誘導がない。

### 参照
- `docs/planned/MILESTONE_ASSET_BROWSER_LEFT_PANE_HUB_2026-04-23.md` — 左ペイン Hub 構想（Phase 1〜4 未達）

---

## 🔴 P0: Owner-Draw 化が未完

### 問題
- グリッド表示は `QListView::IconMode`。アイテム描画の自由度が低く、サムネイル上のバッジ（✓/⚠/🗑）やホバーエフェクトのカスタマイズが困難。
- Phase 0「左ペイン完全 owner-draw 化」は +180行見積もりで未達。現在は QLabel + QFrame + QToolButton のまま。
- Phase 6「メタデータ表示を owner-draw 化」は完了とあるが、File Details は依然 QLabel。依存関係グラフ等は不在。

### 参照
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md` Phase 0, Phase 6

---

## 🟡 P1（高優先）: サムネイル視認性とファイル情報

### 問題
- **サムネイルバッジ不在**: ファイルが Imported/Favorite/Missing/Unused かは右クリックメニューか Type 文字列でしか判別できない。サムネイル上に小さなアイコンオーバーレイがない。
- **長いファイル名の省略が粗い**: `Qt::ElideMiddle` で `"very_long_asset_name_v03_final.png"` → `"very_long...final.png"`。情報が落ちる。
- **サムネイルサイズ変更がスライダーのみ**: 一般的なプリセットボタン（小64px / 中128px / 大256px）がない。25〜256px の連続スライダーだけ。
- **リストビュー時の列表示が未完成**: Name/Type/Size/Date 列表示は `AssetMenuModel.cppm` で +40行見積もりのまま（Phase 1）。
- **ホバープレビューが粗い**: 300ms遅延 popup は実装済みだが、`generateThumbnail()` 直呼びで重い。キャッシュ最適化未了。動画/音声プレビュー未対応。

### 参照
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md` Phase 1, Phase 2, Phase 3
- `docs/planned/MILESTONE_ASSET_BROWSER_HOVER_PREVIEW_2026-06-28.md` — Next Slice に軽量化タスク記載

---

## 🟡 P1: ナビゲーション導線の不足

### 問題
- **ブレッドクラムが弱い**: コードには `breadcrumbBar` があるが、パンくず形式の階層クリック遷移が不十分（Phase 3: +80行見積もり）。
- **フォルダツリーとの連携が弱い**: フォルダツリーで選択しても右ペインに即反映されるが、現在地のハイライトや展開状態の視覚的フィードバックが弱い。
- **履歴（Back/Forward）がない**: フォルダ移動の undo/redo ナビゲーションがない。

---

## 🟡 P1: 情報アーキテクチャの問題

### 問題
- **ツールバーが混雑**: 検索バー、Up/Refresh、View切替、Typeフィルタ5ボタン、Statusフィルタ5ボタン、Sortコンボ、Sort順序ボタンが1列に詰め込みすぎ。
- **File Details が最低限**: 表示項目はファイル名、サイズ、解像度、フレームレート、duration のみ。メタデータ（作成日、カメラ情報、カラースペース、Alpha有無）や依存関係表示がない。
- **フォルダとファイルの区切りが弱い**: フォルダは一覧の先頭にソートされるが、視覚的セパレータや「Folders」セクションヘッダーがない。

---

## 🔵 P2（中優先）: 未実装の高度機能

| 問題 | マイルストーン | 詳細 |
|---|---|---|
| タグシステム不在 | M-AB-12 | タグ付け・タグフィルタ・タグクラウドがない |
| 依存関係追跡不在 | Phase 5 | Find References / Select Unused / リンク切れ再リンクワークフローがない |
| 高度ソート不在 | M-AB-11 | 複数キーソート、自然順序ソートがない |
| 内部D&D移動不在 | Phase 2 | フォルダ間ドラッグ移動ができない |
| Sequence Grouping 未達 | M-AB-2 | 連番ファイルの自動グルーピングが不完全 |
| AI サポート不在 | M-AB-15 | 自動タグ付け、類似性検索、レコメンド機能がない |

---

## ⚙️ コード設計上の問題

| 問題 | 詳細 |
|---|---|
| **3,500行の単一ファイル** | サムネイル生成（WIC/Shell/OIIO/OpenCV/FFmpeg）、波形生成、ホバープレビュー、コンテキストメニュー、ファイル操作が全部 `.cppm` にフラットに詰まっている |
| **QImage 多用** | プロジェクトルールでは QImage 新規採用禁止だが、サムネイル生成で WIC/Shell/OIIO から QImage に変換して使っている（既存コードのため許容） |

---

## 改善の推奨優先順位

| 順位 | 領域 | 内容 |
|---|---|---|
| 1 | **左ペイン Hub のリッチ化** | 状態バッジ、クイックナビゲーションボタン（Recent/Favorites/Missing/Unused）、空状態の改善 |
| 2 | **右ペイン QListView → カスタム delegate 化** | サムネイルバッジオーバーレイ、ホバーエフェクト改善 |
| 3 | **ツールバー整理** | フィルターボタンをドロップダウンやトグルチップに集約、検索バーを目立たせる |
| 4 | **ブレッドクラム + 履歴ナビゲーション** の完成 | Back/Forward、パンくずクリック遷移 |
| 5 | **File Details の拡充** | メタデータ、依存関係、プレビュー拡大 |

---

## 関連文書

- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md` — Phase 0〜6 の全体計画
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` — Phase 1〜3 の詳細計画
- `docs/planned/MILESTONE_ASSET_BROWSER_LEFT_PANE_HUB_2026-04-23.md` — 左ペイン Hub 構想
- `docs/planned/MILESTONE_ASSET_BROWSER_HOVER_PREVIEW_2026-06-28.md` — ホバープレビュー (M-AB-4)
- `docs/planned/MILESTONE_ASSET_BROWSER_ADVANCED_SORT_2026-06-28.md` — 高度ソート (M-AB-11)
- `docs/planned/MILESTONE_ASSET_BROWSER_TAG_SYSTEM_2026-06-28.md` — タグシステム (M-AB-12)
- `docs/planned/MILESTONE_ASSET_BROWSER_AI_SUPPORT_2026-06-28.md` — AI サポート (M-AB-15)
- `docs/planned/MILESTONE_ASSET_BROWSER_RELINK_WORKFLOW_2026-06-28.md` — 再リンク (M-AB-10)
- `docs/planned/MILESTONE_ASSET_BROWSER_SEQUENCE_GROUPING_2026-03-31.md` — シーケンス (M-AB-2)
- `docs/planned/MILESTONES_BACKLOG.md` — 全体バックログ
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` — メイン実装 (3,500行)
- `Artifact/include/Widgets/ArtifactAssetBrowser.ixx` — インターフェース
- `Artifact/src/Asset/AssetMenuModel.cppm` — アイテムモデル
- `docs/WIDGET_MAP.md` — ウィジェット責務マップ

---

## 備考

- 既存の「選択数バッジを常時表示しない」というルール（AGENTS.md）は維持する。状態表示は行ハイライトや既存の状態表示で表現する。
- QtCSS / `setStyleSheet()` の新規追加は禁止。見た目調整は `QPalette`、owner-draw、`QProxyStyle`、既存 theme token で解決する。
- `QColorDialog` の新規使用禁止。色選択は `FloatColorPicker` を使用する。
- 新規シグナル＆スロット接続は禁止。既存のイベント経路やサービスを再利用する。
