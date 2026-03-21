# マイルストーン: アセットブラウザ改善 (Unity 風)

> 2026-03-21 作成

## 現状サマリー

`ArtifactAssetBrowser` (1081行) は基本機能が揃っている。グリッド表示、サムネイル生成、検索、タイプフィルタ、外部 D&D インポート、ファイル詳細パネルが動作。

主な欠落: リストビュー、ソート、キーボード操作、ブラウザ↔Project 同期、お気に入り、ホバープレビュー、内部 D&D。

---

## Phase 1: ビュー切替 & ソート (P0)

| タスク | 対象ファイル | 見積 |
|---|---|---|
| リストビュー切替 (Icon/List) | `ArtifactAssetBrowser.cppm` | +60行 |
| Name/Type/Size/Date 列の表示 | `AssetMenuModel.cppm` | +40行 |
| ソートドロップダウン (名前/タイプ/サイズ/日付) | `ArtifactAssetBrowser.cppm` | +30行 |
| `QSortFilterProxyModel` によるソート適用 | `ArtifactAssetBrowser.cppm` | +50行 |

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

## 優先度マトリクス

| 優先 | タスク | 理由 |
|---|---|---|
| **最優先** | Phase 1: ビュー切替 & ソート | 基本的な見やすさに直結 |
| **最優先** | Phase 2: キーボード操作 | 操作効率の最低限 |
| **高** | Phase 3: ナビゲーション & プレビュー | UX 品質 |
| **高** | Phase 4: 同期 & インスペクタ | ワークフロー統合 |
| **中** | Phase 5: 高度な機能 | プロ仕様の効率化 |

---

## 既存マイルストーン参照

- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md` — M-ASSET-1〜6
- `Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md` — M-PV-1〜5
- `docs/planned/MILESTONES_BACKLOG.md` — M-AS-1〜8
