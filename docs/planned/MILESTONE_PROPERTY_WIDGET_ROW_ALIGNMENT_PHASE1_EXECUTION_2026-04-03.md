# Property Widget Row Alignment / Phase 1 Execution (2026-04-03)

**Target:** `M-UI-23 Property Widget Row Alignment / Inspector Layout`

## Purpose

`ArtifactPropertyWidget` の見た目を、まずは「揃って読める」状態にする。  
この phase では row の geometry と section header の配置を固める。

## Scope

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorRowWidget.cppm`

## Work Items

1. label / value / action 列の基準幅を固定する
2. keyframe triangle / reset / navigation の位置を統一する
3. checkbox / combobox / slider / color bar の行高さを揃える
4. `Transform` / `Effect` / `Expression` の header baseline と badge 位置を合わせる
5. group header の余白と展開 affordance を揃える

## Done When

- 主要 row が同じルールで並ぶ
- 各 row の操作位置が予測しやすい
- section header が widget ごとにぶれない

