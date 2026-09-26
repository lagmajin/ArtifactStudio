# Project View 改善Ver（2026-09-26）

**最終更新:** 2026-09-26

現行の Project View（`project-view-tree-runtime-2026-09-21.png`）を基準に、**対抗案ではなく「同じ画面をより読みやすくする改善案」**として4案を生成した。既存の採用画像を上書き・縮小・削除はしていない。すべて新規ファイル。

生成方法: `generate_project_view_improvement_mockups.py`（Pillow による手続き描画。既存の `generate_mockups.py` と同じ方式）。`python generate_project_view_improvement_mockups.py` で再生成できる。

## 4案

| 案 | ファイル | 主張 |
| --- | --- | --- |
| A | `project-view-improvement-a-dense-tree-2026-09-26.png` | ヘッダー1行に集約した高密度ツリー + 右に詳細ペイン。列は `Name / Type / Status / Size / Modified`。既存の責務を最もそのままに保つ基準案。 |
| B | `project-view-improvement-b-inline-strip-2026-09-26.png` | 一覧を全幅を広げ、選択項目の情報を**下部固定ストリップ**へ寄せる。横幅を優先したい場合。 |
| C | `project-view-improvement-c-tile-grid-2026-09-26.png` | 視覚アセットはタイルグリッド、構造はツリーのまま。識別子を先頭に置き、種別は右下へ弱く配置。 |
| D | `project-view-improvement-d-empty-state-2026-09-26.png` | project 未読込状態。重複していた empty state を1つに統合し、イラストを縮小、action を併記。 |

## 共通の改善点（4案すべて）

- ヘッダーを検索 / 種別固定幅 / compact Tree・Tile switch / Unused の **1行** に集約（2段目の帯を廃止）
- context 行は `Project / name / scope` の breadcrumb + 結果件数のみ（`Tree · All · All items` の重複列挙を廃止）
- Tree／Tile 切替を segmented control に統一（`Tree` だけの単独ボタンと `Tile` 選択状態を統合）
- action icon は 28px の低彩度ボタンに統一（実測スクリーンショットにあった色付きプレートの抑制）
- surface band / separator のコントラストを下げ、row hover・selection・status を優先

## 制約と位置づけ

- **概念画像であり実装仕様の確定ではない。**
- `ArtifactProjectManagerWidget` の責務、item model、selection 同期、Asset Browser との分離は変更しない。
- 既存テーマトークン内で表現 있으며、`QColorDialog`・QtCSS・新規 signal/slot は導入しない。
- 採用範囲は「既存テーマ内の密度・コントラスト・情報階層」。新機能の追加提案ではない。
- 生成物のみ。ビルド・実機比較は未実施（`docs/DOC_LIFECYCLE.md` 準拠）。
