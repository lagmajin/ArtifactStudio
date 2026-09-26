# Project Open Picker 採用モック

**最終更新:** 2026-09-26

## 採用画像

- `project-open-picker-dcc-concept-2026-09-12.png`（採用・不変）
- `project-open-v2-candidate-2026-09-26.png`（Ver2 候補・未採用）
- `project-open-v2-before-after-2026-09-26.png`（現行 vs Ver2 対比）

## 採用範囲

- Places、project tile、選択projectの詳細という3ペイン構成
- recent／favorite、検索、grid/list切替、System Picker fallback
- 最終保存時刻、composition／asset数、health、外部source状態の事前表示
- charcoal面とcool blueの選択／primary accent

## Ver2 候補（2026-09-26 追加）の位置づけ

`docs/analysis/` および実コード照合により、**現行の実装は採用モックの意図をまだ満たしていない**と
確認した。`ArtifactImportAssetsDialog.cppm:263-371` の `ArtifactProjectOpenPickerDialog` は
110 行程度で、Places は `QListWidget` の2項目のみ、tile は
`QIcon::fromTheme("document-open")` の同一アイコンとテキストのみ、inspector は固定文言の
ラベルのみである。health／composition 数／asset 数／外部 source 警告／preview 画像／
Favorites／grid-list 切替はいずれも未実装。

Ver2 候補は**採用モックの 3 ペイン骨格を維持したまま、その情報密度を実際に埋める**ことを
目的にした。方向は 3 案あったが、「情報密度の充填」を採用している。

| 現状 | Ver2 候補 |
| --- | --- |
| 同一の汎用アイコン | project ごとの合成プレビュー |
| 解像度・尺情報なし | Modified / Duration を tile 内に |
| inspector は固定文言 | Version / Last Saved / Compositions / Assets / Status |
| health 未表示 | Healthy / Missing sources を色付きドットで |
| 外部 source 未表示 | tile と inspector の両方で警告 |
| Favorites なし | tile にスター、Places に Favorites |
| grid/list 切替なし | ツールバーに切替ボタン |

生成スクリプトは `generate_project_open_v2_mockups.py`（PIL。既存の
`generate_mockups.py` と同じ流儀）。既存の採用画像は読み取り専用で、
上書き・リサイズ・削除は行わない。

**未実施:** Ver2 のコード実装。ビルド・実機確認はいずれもユーザーの明示指示が必要。

## 責務境界

- Project候補の探索とopen前の概要確認だけを担当する。
- 実際のload、migration、missing-source検証、recent更新は既存Project Service経路へ渡す。
- healthが未計算の場合はHealthyと推測せず、UnknownまたはNot checkedと表示する。

## 実装状況

- `ArtifactProjectOpenPickerDialog` を既存の Import Assets Dialog module に追加した。
- File メニューと Main Window の Open Project 要求は、recent project のタイル選択と System Picker fallback を経由する。
- 実際のload、migration、missing-source検証、recent更新は既存の Project Service 経路のままで、選択UIから新しい検証処理は追加していない。

## 生成プロンプト要約

ArtifactStudioのDCC密度で、recent project tile、preview、保存時刻、project version、health、外部source警告、System Picker fallbackを持つOpen Project専用画面を指定した。
