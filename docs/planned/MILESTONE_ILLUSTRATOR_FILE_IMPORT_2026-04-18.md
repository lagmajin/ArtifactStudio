# マイルストーン: Illustrator AI Transfer / Import Bridge

作成日: 2026-04-18  
更新日: 2026-06-06  
対象: `ArtifactCore` + `Artifact` + `docs/planned/`

---

## Current Progress

- `.ai` / `.pdf` / `.eps` / `.svg` の import 判定を `AssetImporter` 側で明示化
- `PDF-compatible .ai` を `Document` 系として検出しやすくした
- `ArtifactCore` に `VectorImportResult` / `VectorSourceKind` / `VectorImportMode` を追加
- `makeVectorImportResult()` で path ベースの骨格 result を作れるようにした
- まだ parser 本体と unsupported feature 抽出は未完成

## Goal

Illustrator / Affinity Designer 由来のベクター素材を、
単なる raster fallback ではなく、Artifact の asset / layer / shape workflow に乗せる。

このマイルストーンは「AI ファイルを開ける」だけではなく、次を段階的に成立させる。

- project asset として認識できる
- composition に配置できる
- fallback preview と editable conversion の責務を分けられる
- shape / text / image のどこまで編集可能かを UI で明示できる
- 将来の SVG / PDF / vector source と共通基盤を持てる

---

## Why Cross-Cutting

Illustrator / Affinity 系の転送は app 側の import dialog だけで完結しない。

- `ArtifactCore` 側では vector source / import result / editable shape conversion の契約が必要
- `Artifact` 側では asset registration / drag&drop / place / convert / relink / property 表示が必要
- rendering 側では preview fallback と editable layer path の両立が必要

したがって、これは `Asset System`、`Vector Layer Import`、`Shape Layer Enhancement` をまたぐ横断 workstream として扱う。

---

## Positioning

このマイルストーンは次の既存計画をつなぐ bridge として扱う。

- `MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md`
- `MILESTONE_SHAPE_LAYER_ENHANCEMENT_2026-04-28.md`
- `MILESTONE_SOURCE_ABSTRACTION_CORE_2026-03-25.md`

特に最初の段階では、`.ai` や `.afdesign` の native 完全再現ではなく、
`PDF-compatible AI` / `EPS` / `PDF` / `SVG` / `Affinity exported PDF/SVG` を安全に受ける経路を優先する。

---

## Non-Goals

- 初回から Illustrator 完全互換を狙うこと
- 初回から Affinity native 完全互換を狙うこと
- appearance / blend / live effect / envelope / mesh の完全再現
- arbitrary plugin effect の再現
- 録画的なベクター編集履歴の移植

---

## Import Strategy

### First Principle

最初から「完全 editable import」を前提にしない。

1. `Asset ingest`
2. `Preview placement`
3. `Editable conversion`

の 3 段に分ける。

### Supported First-Step Inputs

- `PDF-compatible .ai`
- `.pdf`
- `.eps`
- `.svg`
- `Affinity exported .pdf`
- `Affinity exported .svg`

### Deferred Inputs

- native `.afdesign`
- native `.afphoto`
- native `.afpub`

### Fallback Policy

- editable conversion に失敗しても asset 自体は保持する
- preview-only import と editable import を明示的に分ける
- unsupported feature は silent drop せず、import report に残す
- Affinity 系は first-step では `export bridge` を正式導線とし、native import は investigation 扱いにとどめる

---

## Vector Asset Import v1

初期実装の着地点は `native DCC import` ではなく、`Vector Asset Import v1` として固定する。

### v1 Scope

- `PDF-compatible .ai`
- `.pdf`
- `.eps`
- `.svg`
- `Affinity exported .pdf`
- `Affinity exported .svg`

### v1 User Story

- Illustrator から保存した `PDF-compatible .ai` を project に入れられる
- Affinity Designer から書き出した `PDF` または `SVG` を project に入れられる
- asset browser / project view から composition へ preview 配置できる
- 条件の合う素材だけ `Convert To Editable Layers` を試せる
- 変換できない部分は import report で読める

### v1 Done

- vector asset として import / save / reload / relink が通る
- `Place As Preview` が成立する
- `Convert To Editable Layers` は `basic path / fill / stroke / group` のみ先行対応する
- preview-only / editable-partial / editable-success の区別が UI で読める
- Affinity は `native` ではなく `export bridge` として正式サポート表記にする

### v1 Non-Goals

- native `.afdesign` 読み込み
- Illustrator / Affinity の live effect 完全再現
- text 完全保持
- advanced blend / appearance / mesh の完全移植

---

## Core / App Split

### ArtifactCore Responsibilities

- vector source descriptor
- import result contract
- parsed node tree
- shape/text/image conversion result
- unsupported feature report
- editable conversion policy

### Artifact Responsibilities

- asset browser import
- project registration
- place into composition
- preview layer generation
- convert to shape layers / text layers
- import options / error surface / relink

---

## Phase 1: Core Vector Import Contract

目的:
Illustrator / Affinity-export 由来 asset を app 側が安全に扱える最小 contract を `ArtifactCore` に置く。

含めるもの:

- vector source kind
- page / artboard info
- bounds
- node summary
- text/image/path presence
- unsupported feature list
- import mode (`previewOnly`, `editableAttempted`, `editablePartial`)

完了条件:

- `.ai` / `.pdf` / `.eps` / `.svg` の import result を同じ shape で返せる
- app 側が renderer 実装詳細を知らずに asset 状態を読める
- partial import を success/failure の二値で潰さない

想定 Core 接続先:

- source abstraction
- vector source descriptor
- shape conversion helpers
- diagnostics / import report model

---

## Phase 2: Preview-First Asset Ingest

目的:
Illustrator / Affinity-export asset を project に入れて、まずは壊れず置ける状態にする。

機能:

- `.ai` / `.pdf` / `.eps` / `.svg` を asset browser / project view で認識
- import 時に vector asset として登録
- preview thumbnail を生成
- composition へ preview layer として配置
- missing / relink の対象に含める

完了条件:

- user がファイルを project に追加できる
- project 保存再読込後も asset が残る
- preview placement が software / Diligent で極端に破綻しない

想定 App 接続先:

- `ArtifactAssetBrowser`
- `ArtifactProjectManagerWidget`
- `ArtifactProjectService`
- `ArtifactProjectModel`

---

## Phase 3: Editable Conversion Slice

目的:
よくある Illustrator / Affinity-export 素材を、編集可能な layer 群へ変換する最初の slice を通す。

最初に狙う対象:

- basic path fill
- basic stroke
- group hierarchy
- clipping path の最小対応
- simple placed image

後回しにする対象:

- gradient mesh
- appearance stack
- complex live effect
- unknown blend constructs

変換先:

- shape layer
- text layer
- image layer
- group / hierarchy representation

完了条件:

- `Convert Selected Vector Asset To Editable Layers` が成立する
- 代表的なロゴ / 図形 / lower-third 素材で shape conversion が通る
- 変換不能要素は import report で読める

関連:

- `Shape Layer Enhancement`
- `Vector / SVG Layer Import`

---

## Phase 4: App Workflow Integration

目的:
Illustrator / Affinity-export 転送を app 内の正規 workflow として見える形にする。

機能:

- import options dialog
- `Place As Preview`
- `Convert To Editable Layers`
- `Outline Text On Import` option
- source badge / import mode badge
- import report surface
- `Affinity Export Guide` への導線

UI 文法:

- preview-only なのか editable なのかを asset で読める
- partial conversion は warning として見える
- convert action は asset / layer の両方から呼べる

完了条件:

- user が import mode を誤認しない
- asset browser と project view の両方から workflow へ入れる
- convert 後も source asset との関係が追える

---

## Phase 5: Shape Workflow Bridge

目的:
Illustrator / Affinity-export から来た shape を、Artifact の通常 shape workflow に接続する。

機能:

- imported path を shape layer editing に乗せる
- fill / stroke / dash の property 編集
- group / clipping 情報の保持
- transform / opacity / keyframe 接続

完了条件:

- convert 後の shape が単なる baked preview ではない
- shape layer enhancement 側の既存 operator 導線に将来つなげられる
- imported vector が timeline / property / composition editor で孤立しない

---

## Phase 6: Format Expansion And Hard Cases

目的:
first-step import の先に、互換性と再現範囲を広げる。

候補:

- multi-artboard handling
- CMYK to RGB policy
- font mapping / missing font diagnostics
- deeper PDF object coverage
- partial text preservation policy
- Affinity native format investigation

完了条件:

- 変換可否の判断が docs ではなく import report で読める
- feature support が format ごとに整理される

---

## Suggested First User-Facing Commands

- `Import Vector Asset`
- `Place Vector Preview Into Composition`
- `Convert Selected Vector Asset To Editable Layers`
- `Relink Missing Vector Asset`
- `Show Vector Import Report`

---

## Recommended Order

1. `Phase 1: Core Vector Import Contract`
2. `Phase 2: Preview-First Asset Ingest`
3. `Phase 3: Editable Conversion Slice`
4. `Phase 4: App Workflow Integration`
5. `Phase 5: Shape Workflow Bridge`
6. `Phase 6: Format Expansion And Hard Cases`

---

## Success Criteria

- `.ai` / `.pdf` / `.eps` / `.svg` asset を project に取り込める
- Affinity exported `PDF/SVG` を正式導線として取り込める
- preview と editable conversion が別モードとして整理される
- 代表的な Illustrator / Affinity-export 素材を editable shape layer 群へ変換できる
- unsupported feature が silent failure にならない
- `ArtifactCore` と `Artifact` の責務境界が文書上でも実装上でも追える

---

## Related

- [MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md)
- [MILESTONE_SHAPE_LAYER_ENHANCEMENT_2026-04-28.md](X:/Dev/ArtifactStudio/docs/MILESTONE_SHAPE_LAYER_ENHANCEMENT_2026-04-28.md)
- [MILESTONE_SOURCE_ABSTRACTION_CORE_2026-03-25.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_SOURCE_ABSTRACTION_CORE_2026-03-25.md)
