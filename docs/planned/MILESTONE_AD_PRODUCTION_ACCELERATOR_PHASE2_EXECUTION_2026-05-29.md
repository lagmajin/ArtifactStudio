> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md](MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md)

# Ad Production Accelerator - Phase 2 Execution

Date: 2026-05-29

Source: `docs/planned/MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md`

Depends on: `docs/planned/MILESTONE_AD_PRODUCTION_ACCELERATOR_PHASE1_EXECUTION_2026-05-29.md`

## Phase 2 Goal

Phase 1 の `TemplateSlot` list を前提に、CSV / JSON から広告 variation を読み込み、slot value map として preview できる状態を作る。

この段階では composition の複製や render queue 登録は行わない。まずは「行データを安全に解釈し、どの slot に何が入るか」を確認できることを優先する。

## 2026-07-25 実装監査

Core の汎用 CSV parser と `TemplateVariation` の JSON 保存、WorkspaceAutomation の単一 variation 適用入口は存在する。しかし、Phase 2 固有の CSV／JSON variation importer、stable id／display name の mapping、row-level warning／disabled state、variation preview list をまとめる実装は確認できない。したがって Phase 2 は data shape と再利用可能な parser 基盤のみで、完了条件は未達、runtime 検証も未実施とする。

## Scope

### In

- CSV 1 row = 1 `TemplateVariation`
- JSON array = multiple `TemplateVariation`
- column name / JSON key と slot の対応
- required slot missing の warning
- unknown column / unknown slot の warning
- variation preview list
- variation data の project への optional 保存

### Out

- batch export job generation
- actual layer value application during render
- responsive layout / text overflow detection
- media relink workflow
- external spreadsheet sync
- cloud or database integration

## Boundary Note

- `ArtifactProjectImporter` / `ArtifactProjectExporter` は project file 用なので、variation import と混ぜない
- variation import は current composition の production template metadata を読む補助機能として扱う
- import 失敗を project load 失敗にしない
- invalid row は warning として残し、valid row まで捨てない

## First Files

1. `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
2. `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
3. `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
4. `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
5. `Artifact/src/Render/ArtifactRenderQueueService.cppm`

候補として新規追加:

1. `Artifact/src/Production/ArtifactTemplateVariation.cppm`
2. `Artifact/src/Production/ArtifactVariationDataImporter.cppm`
3. `Artifact/include/Production/ArtifactTemplateVariation.ixx`

実際の置き場は既存 module 境界を確認してから決める。

## Data Shape

`TemplateVariation`:

- `id`: stable id
- `displayName`: row label
- `sourcePath`: imported file path, optional
- `sourceRowIndex`: CSV row index, optional
- `slotValues`: slot id to value map
- `warnings`: row-level validation result
- `enabled`: batch 対象に含めるか

`TemplateSlotValue`:

- `slotId`
- `rawValue`
- `normalizedValue`
- `valueType`
- `validationState`

## CSV Mapping Rules

最初は単純な header mapping でよい。

1. header が slot stable id と一致すれば最優先
2. header が slot display name と一致すれば次点
3. 一致しない column は unknown column warning
4. 同じ slot に複数 column が当たる場合は stable id を優先し、display name 側を warning

例:

```csv
variation_id,Headline,CTA,slot_price
spring_sale_01,春のセール,今すぐ見る,1980円
spring_sale_02,週末限定,購入する,2480円
```

## JSON Mapping Rules

JSON は CSV より構造化できるが、Phase 2 では薄く扱う。

```json
[
  {
    "id": "spring_sale_01",
    "displayName": "Spring Sale 01",
    "values": {
      "Headline": "春のセール",
      "CTA": "今すぐ見る",
      "slot_price": "1980円"
    }
  }
]
```

注意:

- `values` object がある場合はそこを slot value map とする
- `values` がない場合は root object の key を slot candidate として扱う
- unknown key は warning にする

## Encoding Policy

初期対応:

- UTF-8
- UTF-8 BOM

検討:

- Shift_JIS / CP932 は広告運用や EC 現場では出る可能性が高い
- ただし Phase 2 初手で自動判定を頑張りすぎない
- 読み込み失敗時に encoding hint を出す方が先

## Validation

### Import-Level Warnings

- no active composition
- no template slots
- duplicate headers
- unsupported encoding
- empty file
- malformed CSV / JSON

### Row-Level Warnings

- required slot missing
- unknown column
- duplicate slot mapping
- type mismatch
- media path missing
- empty variation id

### Done

- import-level error と row-level warning を分けて保持できる
- valid row は invalid row と混ざっても preview できる
- warning のある variation も disabled state で残せる

## Minimal UI Entry

入口候補:

- Inspector / Property Editor の production template section
- Composition menu の `Import Variation Data...`
- Project View の current composition context menu

Phase 2 の推奨:

- まずは Composition menu または Inspector 起点にする
- Project View asset 化は後回し
- preview list は modal ではなく、既存 panel に寄せられるか確認する

UI で必要な最小表示:

- variation display name
- warning count
- enabled checkbox
- source row index
- slot value summary

## First Move

1. Phase 1 の slot list API を確認する
2. `TemplateVariation` の保持先を決める
3. CSV parser の最小仕様を決める
4. slot mapping helper を作る
5. import result / validation result を分ける
6. preview list UI に渡せる model shape にする

## Recommended Order

1. variation data model
2. CSV read and header parse
3. slot mapping
4. validation result
5. JSON import
6. preview list
7. optional project serialization

## Done Criteria

- CSV から複数 variation を読み込める
- JSON array から複数 variation を読み込める
- slot stable id / display name の両方で mapping できる
- required missing / unknown column / duplicate mapping を warning にできる
- valid row と invalid row を分けて preview できる
- Phase 3 の layout validation に渡せる `TemplateVariation` list がある

## Follow-Up Phase 3 Hook

Phase 3 は selected variation を composition preview に一時適用し、text fit / safe area / anchor rule の warning を出す。

そのため Phase 2 では、variation を実体 composition として複製しない。slot value map と validation state を保つだけにする。

## Static audit follow-up (2026-07-25)

- A generic CSV/report export parser and `TemplateVariation` JSON persistence exist, and Workspace Automation exposes single-variation application and variation-to-queue entry points.
- No dedicated Phase 2 importer/model was confirmed that performs CSV row parsing, JSON-array import, stable-ID/display-name mapping, row-level warnings, disabled invalid rows, and a preview list as one contract.
- The Phase 2 done criteria therefore remain unmet; current implementation is reusable parser/data infrastructure only. No build or runtime verification was performed under the repository policy.
