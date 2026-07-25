# Ad Production Accelerator - Phase 1 Execution

Date: 2026-05-29

Source: `docs/planned/MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md`

## Phase 1 Goal

広告動画の量産に向けて、composition 内の差し替え対象を `TemplateSlot` として記録できる最小の土台を作る。

この段階では CSV import や batch export には入らず、既存プロジェクトを壊さない slot metadata の保存・復元・検証に絞る。

## 2026-07-25 実装監査

`TemplateSlot`／`TemplateVariation`／`OutputVariant` の型、JSON 変換、automation の define/list/apply 入口は実装を確認した。selected-layer UI、target layer の削除・復元時の警告、required slot validation の runtime 到達性は未確認のため、Phase 1 は metadata 基盤実装済み・UI／runtime 検証待ちとする。

## Scope

### In

- text layer / media layer を slot 対象として表現する metadata
- stable slot id と display name の分離
- target `LayerID` の保持
- required flag / default value の保持
- composition JSON への保存・復元
- selected layer から slot 状態を確認できる最小 UI 入口

### Out

- CSV / JSON variation import
- batch export job generation
- responsive layout 自動補正
- text overflow overlay
- media relink UI
- render path / Diligent / D3D12 backend 変更

## Current Boundary Note

- `TemplateSlot` は最初から render path に入れない
- slot は layer の名前そのものではなく、composition 側の production metadata として扱う
- layer rename、timeline 表示、既存 property path と混ざらないようにする
- Phase 1 の validation は「参照先 layer が存在するか」「required slot が空でないか」までに留める

## First Files

1. `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
2. `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
3. `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
4. `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
5. `docs/WIDGET_MAP.md`

必要なら確認:

1. `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
2. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
3. `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`

## Proposed Data Shape

`TemplateSlot`:

- `id`: stable id, project 内で一意
- `displayName`: UI 表示用の名前
- `targetLayerId`: 差し替え先 layer
- `valueType`: `text` / `image` / `media`
- `defaultValue`: 文字列または asset path 参照
- `required`: 未入力を警告するか
- `enabled`: 一時的に slot を無効化できるか

最初は `color` / `number` を入れない。text と media が通れば、variation import の価値検証に十分。

## Serialization Draft

Composition JSON に次のような production metadata を追加する候補:

```json
{
  "productionTemplate": {
    "version": 1,
    "slots": [
      {
        "id": "slot_headline",
        "displayName": "Headline",
        "targetLayerId": "...",
        "valueType": "text",
        "defaultValue": "",
        "required": true,
        "enabled": true
      }
    ]
  }
}
```

注意:

- key は既存 composition JSON と衝突しない名前にする
- unknown version は無視できるようにする
- slot なし composition は空 metadata として扱う

## First Move

1. `ArtifactAbstractComposition::toJson()` / `fromJson()` の保存境界を確認する
2. composition に production template metadata を持たせる場所を決める
3. `TemplateSlot` の型を追加する候補を決める
4. 保存・復元だけを通す
5. selected layer に対して slot 追加・解除できる UI 入口を最小で置く

## Tasks

### 1. Model / Metadata

- `TemplateSlot` 相当の型を追加する
- slot list を composition に保持する
- target layer 削除時の扱いを決める

Done:

- slot list を空で保存・復元できる
- slot を追加した composition が保存・復元後も同じ target layer を指す

### 2. Validation

- missing target layer を warning にする
- required slot の empty default を warning にする
- duplicate display name は warning にするが、stable id が違えば保存は許可する

Done:

- validation result を UI / export 前処理で再利用できる形にできる
- validation が render path を呼ばない

### 3. Minimal UI Entry

- selected layer に対して `Make Template Slot` 相当の入口を置く
- slot display name / required / default value だけ編集できる
- 入口は Property Editor か Inspector の既存 selected-layer 文脈に寄せる

Done:

- 新規 global signal-slot 経路を増やさず、既存選択状態から表示できる
- QtCSS / QColorDialog を使わない

### 4. Backward Compatibility

- slot metadata がない既存 project は変化なし
- unknown slot value type は warning だけで読み飛ばす
- target layer が消えていても project load 自体は失敗させない

Done:

- old project load path が slot metadata を要求しない
- broken slot があっても composition は開ける

## Recommended Order

1. JSON 保存境界を読む
2. `TemplateSlot` の置き場を決める
3. composition metadata の保存・復元を実装する
4. validation helper を作る
5. selected layer から slot を追加できる最小 UI を足す
6. Phase 2 の CSV import に渡す data shape を確認する

## Done Criteria

- composition が template slot metadata を保持できる
- 保存・再読み込みで slot が失われない
- target layer missing / required empty / duplicate name を検出できる
- 既存 project の保存・読み込み挙動が変わらない
- Phase 2 の variation import が参照できる slot list がある

## Follow-Up Phase 2 Hook

Phase 2 はこの Phase 1 の slot list を前提に、CSV column と slot display name / stable id の対応を作る。

そのため Phase 1 では、display name だけに依存しない stable id を必ず残す。

## Static audit follow-up (2026-07-25)

- `Composition.TemplateSlot` and `ParametricComposition` provide stable slot IDs, display names, target/value metadata, required flags, variation values, and JSON round-trip structures.
- The current evidence is stronger for the reusable metadata/data contract than for the originally proposed selected-layer UI and composition-specific validation path. A repository-wide source scan did not establish complete missing-target, required-empty, and duplicate-display-name diagnostics at the selected-layer workflow boundary.
- Backward-compatible parsing and automation entry points are present, but UI/runtime reachability remains unverified. No build or runtime verification was performed under the repository policy.
