# Project Open Picker 採用モック

**最終更新:** 2026-09-12

## 採用画像

- `project-open-picker-dcc-concept-2026-09-12.png`

## 採用範囲

- Places、project tile、選択projectの詳細という3ペイン構成
- recent／favorite、検索、grid/list切替、System Picker fallback
- 最終保存時刻、composition／asset数、health、外部source状態の事前表示
- charcoal面とcool blueの選択／primary accent

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
