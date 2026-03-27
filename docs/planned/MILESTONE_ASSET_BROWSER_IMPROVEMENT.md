# マイルストーン: アセットブラウザ改善 (Unity 風)

> 2026-03-21 作成

## 現状サマリー

`ArtifactAssetBrowser` (1081行) は基本機能が揃っている。グリッド表示、サムネイル生成、検索、タイプフィルタ、外部 D&D インポート、ファイル詳細パネルが動作。

主な欠落: リストビュー、ソート、キーボード操作、ブラウザ↔Project 同期、お気に入り、ホバープレビュー、内部 D&D。

UI 面では、Qt 標準ウィジェットのままだとレイアウトと操作感の自由度が足りないため、左ペインは owner-draw 化を優先し、右ペインも将来的に同じ方向へ寄せる前提で設計する。

2026-03-27 時点で、左ペインに選択数 / Type / Status / Search を出すステータスバーを追加し、owner-draw 化の前段として状態把握を強めた。
2026-03-27 時点で、アセットブラウザー右ペインに Icon/List 切替ボタンを追加し、Phase 1 のビュー切替を進めた。
2026-03-27 時点で、左ペインの owner-draw 化に向けて、Qt 標準ビューの依存を減らしつつ、状態表示とビュー切替を優先して前進している。
2026-03-27 時点で、Name / Type のソート切替を追加し、Phase 1 のソート基盤を前進させた。

---

## Phase 0: Owner-Draw 基盤 (P0)

| タスク | 対象ファイル | 見積 |
|---|---|---|
| 左ペインの完全 owner-draw 化 (ヘッダー/行/選択/ホバー) | `ArtifactAssetBrowser.cppm` | +180行 |
| 左ペインの D&D / インライン操作を owner-draw へ集約 | `ArtifactAssetBrowser.cppm` | +120行 |
| Qt 標準ビュー依存の削減方針を固定 | `ArtifactAssetBrowser.cppm` + `AssetMenuModel.cppm` | +40行 |
| 左ペインの状態表示バー (Selected / Type / Status / Search) | `ArtifactAssetBrowser.cppm` | 実装済み |

## Phase 1: ビュー切替 & ソート (P0)

| タスク | 対象ファイル | 見積 |
|---|---|---|
| リストビュー切替 (Icon/List) | `ArtifactAssetBrowser.cppm` | 実装済み |
| Name/Type/Size/Date 列の表示 | `AssetMenuModel.cppm` | +40行 |
| ソートドロップダウン (名前/タイプ/サイズ/日付) | `ArtifactAssetBrowser.cppm` | +30行 |
| `QSortFilterProxyModel` によるソート適用 | `ArtifactAssetBrowser.cppm` | +50行 |
| 現在の selection / type / search を保ったままビューを切り替える | `ArtifactAssetBrowser.cppm` | 実装中 |

---

## Phase 2: キーボード操作 & ステータス表示 (P0)

| タスク | 対象ファイル | 見積 |
|---|---|---|
| キーボード操作 (矢印/Delete/Ctrl+A/Ctrl+C/V) | `ArtifactAssetBrowser.cppm` `keyPressEvent` 埋め | +80行 |
| サムネイル上のステータスバッジ (✓/⚠/🗑) | `AssetMenuModel.cppm` + デリゲート | +100行 |
| ステータスフィルタ (All/Imported/Missing/Unused) | `ArtifactAssetBrowser.cppm` | +40行 |
| フォルダ間の内部 D&D 移動 | `ArtifactAssetBrowser.cppm` + `AssetMenuModel.cppm` | +80行 |

---

## Phase 3: ナビゲーション & プレビュー (P1)

| タスク | 対象ファイル | 見積 |
|---|---|---|
| ブレッドクラムナビゲーションバー | `ArtifactAssetBrowser.cppm` | +80行 |
| ホバープレビューポップアップ (300ms 遅延) | 新規 `HoverThumbnailPopupWidget` | +120行 |
| お気に入り (addFavorite/removeFavorite 実装) | `AssetDirectoryModel.cppm` | +60行 |
| 仮想カテゴリ (Recent/Favorites/All Images/Unused) | `AssetDirectoryModel.cppm` | +100行 |

---

## Phase 4: 同期 & インスペクタ (P1)

| タスク | 対象ファイル | 見積 |
|---|---|---|
| ブラウザ ↔ Project View 選択同期 | `ArtifactAssetBrowser.cppm` + `ArtifactProjectManagerWidget.cppm` | +60行 |
| 右側インスペクタパネル (メタデータ/依存関係) | 新規 or `ArtifactAssetBrowser.cppm` | +200行 |
| FileTypeDetector 統一 (ハードコード排除) | `ArtifactAssetBrowser.cppm` | +30行 |
| Undo 連携 (インポート操作) | `UndoManager.cppm` | +60行 |

---

## Phase 5: 高度な機能 (P2)

| タスク | 対象ファイル | 見積 |
|---|---|---|
| アセット依存関係追跡 | `AssetDatabase.ixx` 拡張 | +150行 |
| "Find References" 操作 | `ArtifactAssetBrowser.cppm` | +40行 |
| "Select Unused" 操作 | `ArtifactAssetBrowser.cppm` | +40行 |
| リンク切れ再リンクワークフロー | 新規ダイアログ | +100行 |

---

## Phase 6: 右ペイン owner-draw 拡張 (P2 / 将来)

| タスク | 対象ファイル | 見積 |
|---|---|---|
| 右ペインのメタデータ表示を owner-draw 化 | `ArtifactAssetBrowser.cppm` | +120行 |
| 依存関係 / プレビュー / アクション領域の描画統一 | `ArtifactAssetBrowser.cppm` | +160行 |
| Qt 標準コンテキストに残す箇所の最小化 | `ArtifactAssetBrowser.cppm` | +60行 |

---

## 優先度マトリクス

| 優先 | タスク | 理由 |
|---|---|---|
| **最優先** | Phase 0: Owner-Draw 基盤 | Qt 標準 UI の制約を外すための土台 |
| **最優先** | Phase 1: ビュー切替 & ソート | 基本的な見やすさに直結 |
| **最優先** | Phase 2: キーボード操作 | 操作効率の最低限 |
| **高** | Phase 3: ナビゲーション & プレビュー | UX 品質 |
| **高** | Phase 4: 同期 & インスペクタ | ワークフロー統合 |
| **中** | Phase 5: 高度な機能 | プロ仕様の効率化 |
| **中** | Phase 6: 右ペイン owner-draw 拡張 | 将来的な UI 一貫性確保 |

---

## 既存マイルストーン参照

- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md` — M-ASSET-1〜6
- `Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md` — M-PV-1〜5
- `docs/planned/MILESTONES_BACKLOG.md` — M-AS-1〜8
