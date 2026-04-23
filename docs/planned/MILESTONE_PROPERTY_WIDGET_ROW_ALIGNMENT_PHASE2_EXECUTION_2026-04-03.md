# Property Widget Row Alignment / Phase 2 Execution (2026-04-03)

**Target:** `M-UI-23 Property Widget Row Alignment / Inspector Layout`

## Purpose

`ArtifactPropertyWidget` の row-level interaction を、見た目の整列に合わせて揃える。  
この phase では操作の入口と `PropertyEditor` への責務移行を進める。

## Scope

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorRowWidget.cppm`
- `Artifact/docs/PROPERTY_EDITOR_AUDIT_2026-03-11.md`

## Work Items

1. numeric row の slider / drag / spin の責務を分ける
2. color row の click / drag を明示する
3. keyframe navigation を row-level affordance にする
4. reset button の位置とサイズを共通化する
5. `ArtifactPropertyWidget` の ad-hoc row construction を `PropertyEditor` 側へ寄せる

## Done When

- row ごとの操作が一貫する
- `ArtifactPropertyWidget` が layout orchestration に集中できる
- 新しい row を同じ型で増やせる

