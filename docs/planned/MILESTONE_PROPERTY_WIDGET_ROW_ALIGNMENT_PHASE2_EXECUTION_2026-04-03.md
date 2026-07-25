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

## Static Audit (2026-07-25)

Phase 2 の row-level surface は `ArtifactPropertyEditorRowWidget` に集約されている。numeric 用の slider／drag／spin 系 editor、color／toggle／rotation 用の専用 editor、keyframe toggle・前後 navigation・reset・expression・favorite の共通 action、tooltip／hover visibility／owner-draw geometry を確認できる。`ArtifactPropertyWidget` は `addRowsFromProperties()` と `alignPropertyRowLabels()` を中心に group／selection の orchestration を担い、row の action handler API も `PropertyEditor` 側に存在する。

ただし、全 property type で slider／drag／spin の責務分離が同じ規則になること、color drag の入力と keyframe／reset／expression の handler が全 row に接続されること、reference link／pick-whip affordance の共通配置、ad-hoc row 構築の完全撤去、undo／redo を含む runtime 一貫性は未検証である。新規 row を同じ型で追加できる契約も、静的 API の存在以上には確認できない。

判定: **Phase 2 の共通 row surface は実装済み。** 全 editor／handler の接続確認と Phase 3〜4 の責務移行は継続中である。
