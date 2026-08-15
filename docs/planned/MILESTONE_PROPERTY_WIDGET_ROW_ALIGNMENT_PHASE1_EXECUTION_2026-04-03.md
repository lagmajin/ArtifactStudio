# Property Widget Row Alignment / Phase 1 Execution (2026-04-03)

**最終更新:** 2026-08-15

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

## Static Audit (2026-08-15)

Phase 1 の geometry contract は現行ソースに反映されている。row 最小高さ 34、label 幅 132、row margin／spacing、navigation／keyframe／reset／expression の固定寸法が `PropertyEditor` 側に定数化され、row widget の owner-draw geometry で label／editor／action の位置を算出している。Property Widget 側でも `alignPropertyRowLabels()` を effect／channel／transform／group に適用し、section header／badge／group padding の実装を確認できる。

ただし、checkbox／combobox／slider／color editor の全組み合わせで同一高さになること、Property Widget と Inspector の header baseline／badge 位置が実画面で一致すること、展開 affordance のキーボード・runtime挙動は未検証である。定数が `ArtifactPropertyEditor.cppm` と `ArtifactPropertyEditorShared.cppm` に重複している箇所もあり、単一の共通契約として完全に統合されたとは言い切れない。

判定: **Phase 1 の静的実装は概ね完了。** runtime／visual 検証と寸法定数の完全集約が残っている。

### Current-Code Recheck (2026-08-15)

`alignPropertyRowLabels()` は effect／channel／transform／通常 group の各経路で利用され、PropertyEditor row の label／editor／action geometry、keyframe／reset／navigation の共通配置も継続して確認できる。Phase 1 の静的完了判定は維持する。一方、editor 種別ごとの実画面高さ、Inspector との header baseline、keyboard 展開、寸法定数の重複解消は runtime または追加整理が必要である。
