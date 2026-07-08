# 他アプリ横断 Artifact 機能機会レポート — 2026-07-04

作成日: 2026-07-04  
対象: Artifact / ArtifactCore の現行ソース、および `docs/analysis/` / `docs/planned/`  
目的: 他アプリの機能をそのまま複製せず、Artifact の既存レイヤー・プロパティ・タイムライン・レンダー基盤へ自然に導入できる機能を選別する。

## 1. 結論

Artifact が次に狙うべき領域は、巨大な NLE / 3D DCC / ノードコンポジタの模倣ではない。

優先すべきなのは次の 4 本である。

1. **公開プロパティ + データバインド**
   - Rive / Apple Motion / Notch の「制作物を再利用可能な部品として渡す」能力
2. **ライブ信号を既存プロパティへ接続**
   - TouchDesigner / Notch の OSC・MIDI・音声リアクティブ制作
3. **Generator / Modifier / Field の統一 UX**
   - Cinema 4D / Unreal Motion Design のクローン・エフェクター操作
4. **状態・バリエーション・複数フォーマット制作**
   - Rive の State Machine、Unreal の Scene State、Figma 的な variant、広告制作の responsive output

この 4 本は別々の大機能に見えるが、内部的には次の一本の流れにまとめられる。

`外部値 / データ / 状態` → `公開プロパティ` → `既存 property evaluation` → `layer / component / field` → `preview / render / export`

最優先候補は **Published Controls**, **Live Control Recording**, **Audio Reactive Binding**, **State Variants**, **Field Authoring UX** の 5 件。

## 2. 調査方法と判定

### 2.1 判定記号

| 記号 | 意味 |
|---|---|
| READY | 既存コードを薄く接続すれば価値を出せる |
| FOUNDATION | 基盤または計画があり、中規模の統合で実現可能 |
| GAP | 有用だが主要基盤が不足 |
| DUPLICATE | 既存マイルストーンとほぼ重複。新規計画を増やさない |
| DEFER | Artifact の主戦場から外れる、または依存が重すぎる |

### 2.2 現行ツリーから確認した主な受け皿

| 受け皿 | 現状 |
|---|---|
| 外部コントローラー割当 | `ArtifactCore/src/Control/ArtifactExternalControlManager.cppm` に MIDI / OSC 形式の address と property の対応管理がある |
| Parametric Composition | `ArtifactCore/src/Composition/ParametricComposition.cppm` に text / image / matte / bool 等の input binding と JSON 保存がある |
| 音声解析 | `ArtifactCore/src/Audio/AudioAnalyzer.cppm` に spectrum と low / mid 等の解析経路がある |
| Event 基盤 | `ArtifactCore/src/Event/EventBus.cppm` がある |
| Generator / Field | `MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md` と layer component pipeline 群が既に計画済み |
| Responsive output | `MILESTONE_RESPONSIVE_LAYOUT_COMPOSITION_2026-06-05.md` が既に計画済み |
| CSV / JSON variation | Ad Production Accelerator Phase 2 が既に計画済み |
| State / Variant | `MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md` があるが、制作状態と版管理の責務分離が必要 |

したがって、旧来の「ライブ入力・データバインド・Field が完全にゼロ」という評価は更新が必要である。コア断片は存在し、現在の課題は **編集 UI、評価契約、記録、export contract の統合** にある。

## 3. 参照アプリから抽出した設計パターン

### 3.1 Rive

Rive は timeline animation を State / Transition / Layer で組み合わせ、Data Binding の View Model property をデザインと runtime code の契約にしている。値型は number / bool / string / enum / color / list / image / nested model まで広い。

Artifact に必要なのは Rive editor の複製ではなく、次の二点である。

- composition 内の property を安定 ID 付きで **Published Control** にする
- Published Control を state、CSV / JSON、外部 controller、export runtime の共通入口にする

### 3.2 TouchDesigner

TouchDesigner は画像、制御信号、データ、geometry 等を別の operator family として扱い、必要な箇所で変換・参照する。MIDI / OSC / audio 等の時系列値を任意の parameter に流す操作が強い。

Artifact では全面的な node editor 化は避け、既存 Property Editor に次を足す方が適切である。

- `Source: Static / Keyframes / Expression / Audio / MIDI / OSC / Data`
- source 値の monitor
- scale / offset / clamp / smooth
- 入力を keyframe へ記録する arm / record

### 3.3 Cinema 4D / Unreal Motion Design

両者は Cloner と Effector を中心に、位置・回転・scale・color 等へ influence を与える。Cinema 4D Fields と Unreal の shaped effector は、空間範囲、falloff、noise、複数 influence の合成を視覚的に扱う。

Artifact には generator / modifier / field の計画が既にあるため、新しい clone system を追加してはいけない。必要なのは以下である。

- field を viewport で直接選択・移動・scale
- field list の reorder / enable / blend / weight
- layer property と clone attribute の共通 influence 表現
- 2D shape、text、particle、cloner で共有できる限定的な field contract

### 3.4 Apple Motion / Notch

Apple Motion は rig widget と公開 parameter により、テンプレート利用者へ必要な操作だけを渡す。Notch Block も exposed property を host から制御できる自己完結した content package として扱う。

Artifact のテンプレート機能にも「編集可能項目を明示する契約」が必要である。全 Property Editor を渡すのではなく、作者が公開した値だけを簡潔な control panel として提示する。

### 3.5 Blender

Blender の Geometry Nodes / Simulation Zone は強力だが、Artifact に全面導入すると別 DCC 化する。取り込むべきなのは以下に限定する。

- modifier の非破壊 stack
- reusable preset / asset
- simulation の明示的な state / bake / reset
- node graph そのものではなく、必要時に graph-backed component を格納できる設計余地

### 3.6 Notch のライブ制作

Notch は OSC、audio capture、audio band、attack / decay / smooth、exposed property、standalone / block export を一続きの制作体験にしている。

Artifact ではまずライブ送出製品を目指さず、**ライブ入力をモーション制作に使い、結果を keyframe 化できる**ところまでが適切である。

## 4. 導入候補 Top 20

採点は 5 点満点。`価値 / 流用 / 差別化` が高いほど有利、`コスト` は高いほど不利。

| 順位 | 機能 | 主な参照 | 判定 | 価値 | 流用 | 差別化 | コスト | 推奨 |
|---:|---|---|---|---:|---:|---:|---:|---|
| 1 | Published Controls | Rive / Motion / Notch | READY | 5 | 5 | 5 | 2 | P0 |
| 2 | Live Control Recording | TouchDesigner / Notch | READY | 5 | 4 | 5 | 2 | P0 |
| 3 | Audio Reactive Property Binding | Notch / TouchDesigner | READY | 5 | 4 | 4 | 2 | P0 |
| 4 | Composition State Variants | Rive / Unreal | FOUNDATION | 5 | 4 | 5 | 3 | P0 |
| 5 | Field Authoring UX | C4D / Unreal | FOUNDATION | 5 | 4 | 4 | 3 | P0 |
| 6 | Data-bound Template Instances | Rive / Figma 的 contract | FOUNDATION | 5 | 4 | 5 | 3 | P1 |
| 7 | Input Mapping Transform | TouchDesigner | READY | 4 | 5 | 4 | 1 | P0 |
| 8 | Responsive Layout Preview Matrix | Rive / Figma | DUPLICATE | 5 | 4 | 4 | 3 | 既存計画へ統合 |
| 9 | CSV / JSON Batch Variations | Cavalry 的 data workflow | DUPLICATE | 5 | 4 | 5 | 3 | 既存計画へ統合 |
| 10 | Reusable Component Instance Override | Rive / Figma | FOUNDATION | 5 | 3 | 5 | 4 | P1 |
| 11 | State Transition Graph | Rive | FOUNDATION | 4 | 3 | 5 | 4 | P1 |
| 12 | Layer / Composition Takes | C4D / Unreal Scene State | FOUNDATION | 4 | 3 | 4 | 3 | P1 |
| 13 | Field-driven Text / Shape Attributes | C4D | FOUNDATION | 4 | 4 | 4 | 3 | P1 |
| 14 | Controller Learn Mode | MIDI / OSC tools | READY | 4 | 4 | 4 | 2 | P1 |
| 15 | Control Dashboard / Performance View | TouchDesigner / Notch | FOUNDATION | 4 | 3 | 4 | 3 | P1 |
| 16 | Bake Live / Procedural Result | Blender / Houdini | FOUNDATION | 5 | 3 | 4 | 3 | P1 |
| 17 | Runtime Package Manifest | Rive / Notch Block | GAP | 4 | 3 | 5 | 4 | P2 |
| 18 | Spout / NDI Texture I/O | TouchDesigner / Notch | GAP | 3 | 2 | 4 | 4 | P2 |
| 19 | Full Visual Operator Graph | TouchDesigner / Nuke | DEFER | 3 | 1 | 2 | 5 | 見送り |
| 20 | Full Broadcast Rundown / Playout | Unreal / Notch | DEFER | 3 | 1 | 3 | 5 | 別製品領域 |

## 5. P0 の具体化

### 5.1 Published Controls

作者が composition / template の任意 property を公開し、利用者には公開値だけを表示する。

最小仕様:

- stable control ID
- display name / group / order
- type / default / min / max / enum choices
- source property path
- read-only / hidden / required
- JSON serialize
- template instance ごとの override

既存接続:

- `ParametricCompositionInputBinding`
- Project Template Gallery
- Ad Production Accelerator
- 将来の runtime / web control manifest

避けること:

- Qt の signal を公開 control ごとに増やさない
- raw widget pointer を binding model に保持しない
- Property Editor の複製パネルを作らない

### 5.2 Live Control Recording

MIDI / OSC 等の入力を property へ一時適用し、record 中だけ sampling して keyframe 化する。

最小仕様:

- input monitor
- record arm
- sample rate
- dead zone
- smoothing
- key reduction
- cancel で元状態へ戻す

`ArtifactExternalControlManager` は address mapping の受け皿になる。新規の中央集権 signal wiring は作らず、既存 property mutation / command / event 経路へ接続する。

### 5.3 Audio Reactive Property Binding

AudioAnalyzer の解析結果を property source として扱う。

最小 source:

- amplitude
- low / mid / high band
- selected frequency range
- beat / transient は後続

最小 transform:

- gain
- offset
- clamp
- attack
- release
- smooth
- invert

初期段階は preview evaluation と bake-to-keyframes に限定し、render 時の非決定的な live capture は対象外にする。

実装メモ 2026-07-04:

- core 側に `ArtifactAbstractComposition::applyAudioAnalysis(...)` を追加済み
- `AudioAnalyzer::AnalysisResult` の `rms / peak / lowIntensity / midIntensity / highIntensity` を
  `audio.amplitude / audio.peak / audio.low / audio.mid / audio.high` address として既存
  `ExternalControlManager` → `setLayerPropertyValue(...)` 経路へ流せるようにした
- まだ UI は無く、authoring は後続
- beat / transient / selected frequency range / bake-to-keyframes は未着手

### 5.4 Composition State Variants

Rive の state machine をそのまま入れず、まず名前付き property override 集合を作る。

例:

- `Idle`
- `Hover`

### 5.5 Field-driven Text / Shape Attributes

最小実装メモ 2026-07-04:

- `ArtifactTextLayer` の glyph animator 評価へ、layer field を追加 weight として掛ける経路を追加
- 初期対応は `artifact.field.radial` / `artifact.field.box` / `artifact.field.linear` / `artifact.field.noise`
- 各 glyph の `basePosition + bounds.center()` を使って 0..1 influence を計算し、
  既存 `TextAnimatorEngine::applyAnimatorStack(...)` の重みに乗算する
- color / stroke override も 0/1 切替ではなく weight ベースで blend するように拡張
- field authoring 側に `centerX / centerY / angle` と簡易 preview summary を追加
- つまり現時点では「field が text animator の影響マスクとして効く」段階
- field center の個別 UI、shape layer への同方式展開、field 専用 preview は未着手
- `Selected`
- `Sale`
- `Sold Out`
- `16:9`
- `9:16`

`LayerVariant`、responsive layout variant、revision を混同しない。

- Revision: 過去状態
- Layer Variant: 単一 layer の派生案
- Composition State: 複数 property / layer の同時状態
- Responsive Variant: output geometry に伴うレイアウト差分

Phase 1 は state の作成・複製・切替・override 表示。Transition graph は Phase 2 以降。

実装メモ 2026-07-04:

- core 側に `CompositionStateVariant` / `CompositionStatePropertyOverride` を追加済み
- composition JSON に `stateVariants` と `activeStateVariantId` を保存する
- `setActiveStateVariantId(...)` で state 切替時の property 一括適用ができる
- override には `baselineValue` を持たせ、前 state を抜ける際に次 state が触らない property を戻せるようにした
- まだ UI は無く、state 作成 / 複製 / override capture / active 切替の操作面は後続

### 5.5 Field Authoring UX

新しい field engine ではなく、既存 Generator / Modifier / Field 計画の操作面を固める。

最小仕様:

- viewport overlay で範囲表示
- field gizmo
- linear / radial / box
- falloff curve
- invert
- weight
- list reorder
- enable / solo
- deterministic preview

既存の transform gizmo / mask edit mode と tool ownership が競合しないことを先に定義する。

## 6. 既存計画へ統合し、新規 milestone を増やさない項目

| 候補 | 統合先 |
|---|---|
| Responsive Layout Preview Matrix | `MILESTONE_RESPONSIVE_LAYOUT_COMPOSITION_2026-06-05.md` |
| CSV / JSON Batch Variations | `MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md` と Phase 2 |
| Generator / Modifier / Field model | `MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md` |
| Shape modifier stack | `MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md` |
| Layer variants | `MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md` |
| Reactive event rules | `MILESTONE_REACTIVE_EVENT_SYSTEM_2026-03-28.md` |

新規 milestone を作る場合も、上記の親 milestone に接続する実行 slice として扱う。

## 7. 見送るべき模倣

### 7.1 全面 node editor 化

TouchDesigner / Nuke / Houdini 型の graph は強力だが、Artifact の主要導線である layer + property + timeline を二重化する。まず property source と component stack で価値を出す。

### 7.2 Magnetic timeline / multicam

ArtifactPr の NLE 領域であり、Artifact 本体へ混ぜると timeline の責務が崩れる。

### 7.3 フル 3D DCC 化

Geometry Nodes、modeling、shader graph、scene assembly の全面導入は Blender / Houdini と競争することになる。Artifact では motion design に必要な procedural generator と field に限定する。

### 7.4 フル broadcast playout

Unreal Rundown / Notch media-server integration は運用、冗長化、同期、hardware I/O まで責任範囲が広い。まず reusable runtime package と exposed control contract が成立した後に再評価する。

## 8. 推奨ロードマップ

### Slice A: Control Contract

1. Published Control schema
2. Parametric Composition との対応
3. JSON round-trip
4. 公開 control だけを表示する既存 Property Editor の filter / presentation

### Slice B: Reactive Sources

1. source kind model
2. scale / offset / clamp / smooth
3. AudioAnalyzer source
4. ExternalControl source
5. bake-to-keyframes

### Slice C: State and Variation

1. Composition State override set
2. state switch preview
3. CSV / JSON row → Published Controls
4. responsive variant との責務分離

### Slice D: Field UX

1. field list
2. viewport gizmo
3. common falloff
4. cloner / text / shape の順で適用範囲を広げる

推奨着手順は **A → B → C → D**。A の control contract が B と C の共通 API になるためである。Field engine の内部計画は並行可能だが、UI は既存 component pipeline の評価順が安定してから接続する。

### 追加の実行順

`Published Controls`、`Master Properties`、`Composition State Variants`、`Takes` は別々の機能に見えるが、実装順は次の流れにするとぶれにくい。

1. `Published Controls` で公開できる property contract を固める
2. `Master Properties` で precomp の exposed control を定義する
3. `Composition State Variants` で名前付き override set を持たせる
4. `Takes` で複数バリエーションの override set を束ねる

この順にすると、公開契約 → 露出 → 状態 → バリエーションの依存が自然に流れる。

#### 境界

- `Master Properties` は「何を公開するか」
- `Takes` は「公開済みまたは既存の override をどう束ねるか」
- `Composition State Variants` は「状態の切替をどう命名するか」
- `Published Controls` はその共通契約になる

#### 3D との関係

- 3D 側の camera / overlay 整理は別レーンだが、公開 control contract が固まると preview / state / variant の語彙を揃えやすい
- したがって、3D 系の整理より先に control contract の骨格を固めると、後続の実装がぶれにくい
- ただし 3D の orbit / pan / preview 整理自体は control contract の完了を待たずに進められる
- その場合も、状態名や公開値の命名は `Published Controls` 側の語彙に寄せると統合しやすい

## 9. 導入後の Artifact の立ち位置

この方向は Artifact を「AE の不足機能を埋めるアプリ」から、次のような製品へ進める。

- timeline で精密に作れる
- Rive のように状態とデータを持てる
- Motion / Notch のように利用者向け control を公開できる
- TouchDesigner のように外部値へ反応できる
- C4D / Unreal のように field で大量要素を動かせる
- ただし操作の中心は一貫して layer / property / timeline に置く

差別化の核は **「ライブ・データ駆動・プロシージャルな結果を、通常のキーフレームへ安全に bake できるモーショングラフィックス環境」** である。

## 10. 公式参照資料

- [Rive State Machine Overview](https://rive.app/docs/editor/state-machine/state-machine)
- [Rive Data Binding Overview](https://rive.app/docs/editor/data-binding/overview)
- [Rive Features](https://rive.app/features)
- [TouchDesigner Operator Families](https://derivative.ca/UserGuide/Operator)
- [TouchDesigner Uses of CHOPs](https://derivative.ca/UserGuide/Uses_of_CHOPs)
- [TouchDesigner Interoperability](https://derivative.ca/UserGuide/Interoperability)
- [Cinema 4D Fields](https://help.maxon.net/c4d/2026/en-us/Content/html/58091.html)
- [Unreal Engine Motion Design](https://dev.epicgames.com/documentation/unreal-engine/motion-design-in-unreal-engine)
- [Unreal Engine Motion Design Cloners and Effectors](https://dev.epicgames.com/documentation/unreal-engine/motion-design-cloners-and-effectors-in-unreal-engine)
- [Apple Motion: Publish rigs to Final Cut Pro](https://support.apple.com/guide/motion/publish-rigs-to-final-cut-pro-motn13f21017/mac)
- [Blender Geometry Nodes](https://docs.blender.org/manual/en/latest/modeling/geometry_nodes/index.html)
- [Notch OSC](https://manual.notch.one/2026.1/en/docs/reference/devices-protocols/osc/)
- [Notch Blocks](https://manual.notch.one/2026.1/en/docs/workflows/working-with-media-servers/blocks/)
- [Notch Audio Reactive Workflow](https://manual.notch.one/1.0/en/docs/learning/working-with-audio/)

## 11. 確認範囲

- ソースおよび文書の静的検索のみ
- 子 repo は読み取りのみで、変更していない
- ビルド、テスト、CMake、runtime 操作は実施していない
- 製品機能は 2026-07-04 時点で取得できた各社公式資料を参照
