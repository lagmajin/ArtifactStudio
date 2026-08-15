# Milestone: Behavior-Driven Property Motion (2026-07-05)

**最終更新:** 2026-08-15
**ステータス:** 部分実装（Property Link／Audio Reactive の個別経路あり、共通 Behavior 契約は未整備）

## 2026-08-15 現行コード監査

`PropertyLinkManager` は source／target property と link type を保持し、`ArtifactAbstractComposition` の Audio Reactive Binding は RMS／Peak／Low／Mid／High、gain／offset／invert／clamp、attack／release、preview／bake、JSON／Undo を実装している。ExpressionEvaluator と particle force field も関連する個別基盤として存在する。

一方、Behavior ID／type／stable target／共通評価順、scalar／vec 互換検証、dependency graph／循環検出、preview／render の共通 deterministic contract、Throw／Gravity／Orbit／Attractor 等を一つの非破壊 Behavior 層で評価する実装は確認できない。既存の Property Link と Audio Reactive はこの milestone の全体契約を満たすものではない。

判定: **Phase 1〜2 は個別機能として部分実装、Phase 0 の共通契約と Phase 3 以降の motion behavior／統合 UI は pending。**

## Goal

プロパティをキーフレームだけでなく、別プロパティ、音声解析値、解析的な運動式、空間フィールドから駆動できる共通 Behavior 基盤を整備する。

初期対象は次の 4 系統とする。

1. Parameter Link
2. Audio Behavior
3. Throw / Gravity / Orbit
4. Attractor / Repel / Vortex

Random Motion と MIDI Behavior は共通基盤を再利用できる後続候補とし、本 milestone の必須範囲には含めない。

## Product Model

Behavior は「入力 source を読み、transform を適用し、対象 property の評価値を返す」非破壊の駆動レイヤーとして扱う。

```text
Source
  -> Value Transform
  -> Behavior Evaluation
  -> Target Property Preview
  -> Optional Bake to Keyframes
```

Behavior の適用結果を通常値へ直接上書きし続けない。既存値、キーフレーム値、Behavior 出力の責務と優先順位を明示する。

推奨評価順:

1. base property value
2. keyframe interpolation
3. Parameter Link / Audio source resolution
4. Motion Behavior
5. final clamp / type conversion

## Existing Foundations

| Foundation | Current state | Reuse |
|---|---|---|
| `PropertyLinkManager` | Direct / Inverse / Scale / Offset / Custom が存在 | Parameter Link のコア候補 |
| Property reference milestones | target catalog / resolver / Pick-Whip が計画済み | authoring UI と stable target 解決 |
| `AudioAnalyzer` | RMS / Peak / spectrum / Low / Mid / High を解析 | Audio Behavior source |
| Audio property path | `audio.amplitude / peak / low / mid / high` を既存 property mutation 経路へ適用可能 | preview evaluation |
| `ExpressionEvaluator` | time、wiggle、audio関数、subframe、adaptive step が存在 | 解析式と時間評価の参考・補助 |
| Particle force fields | gravity / vortex / attractor 等が存在 | 数式・パラメータ語彙のみ参考 |

Particle force field と layer transform Behavior は同一実体にしない。前者は粒子集合、後者は編集可能なオブジェクトプロパティを対象とする。

## Scope Boundaries

- 初期対象は float / vec2 / vec3 系のアニメーション可能 property
- authoring surface は `ArtifactPropertyWidget` / Property Editor に寄せる
- Inspector は選択と要約の窓口に留め、別の重複 property panel を作らない
- preview と deterministic render で同じ時刻から同じ値を得られること
- Bake to Keyframes は後半 phase で追加する
- Diligent / DX12 / render backend は変更しない
- 新しいグローバル signal / slot 経路を作らない
- QtCSS、`QColorDialog`、Qt composition による描画を追加しない

## Non-Goals

- Box2D 等による汎用剛体シミュレーション
- 衝突、回転慣性、摩擦を含む完全な物理エンジン
- 流体画像変形としての `DynamicFluidVortex`
- 粒子 force field の置換
- MIDI / OSC の実入力バックエンド
- Behavior node graph editor

## Phase 0: Behavior Contract and Evaluation Order

### Purpose

各機能を個別の場当たり的な property 更新として実装せず、共通の保存・評価契約に載せる。

### Work

- Behavior ID、type、enabled、target property path を定義
- source reference を raw pointer ではなく stable ID / property path で保存
- scalar / vec2 / vec3 の型互換表を定義
- base / keyframe / link / motion の評価順を固定
- seek、scrub、reverse playback、frame-rate変更時の契約を定義
- dependency graph と循環検出を用意
- preview-only と bakeable の capability を分ける
- JSON serialize / deserialize の version を持つ

### Done

- 同じ Behavior が save/load 後も同じ target を解決する
- 無効な参照は安全に unresolved 状態となり、値を破壊しない
- self-link と循環 link を拒否できる
- frame と time の意味が preview / render で一致する

## Phase 1: Parameter Link

### Initial Slice

- source property と target property の stable resolver
- Direct / Inverse / Scale / Offset
- `output = clamp(source * gain + offset, min, max)` の共通変換
- type compatibility validation
- link enable / disable / remove
- missing source の明示
- save/load と undo/redo

### Expression Transform

初期 Slice の後で、source 値を `x` とする制限付き式変換を追加する。

```text
x
x * 2 + 10
clamp(x, 0, 100)
```

- 任意の property mutation や副作用を式から許可しない
- expression error 時は last valid value または base value にフォールバック
- Custom callback を永続化形式として露出しない

### UI

- Property row から target を選択
- link target、変換、状態を同じ row で確認
- Pick-Whip は既存 Property Reference Linking 計画に従い、後続 Slice とする
- 初期実装では target selector だけでも完結可能にする

### Done

- 互換 property 間をリンクできる
- scale / offset / inverse が preview と render で一致する
- source rename / missing / delete を安全に扱える
- link chain を評価でき、循環は拒否される

## Phase 2: Audio Behavior

### Sources

- RMS amplitude
- Peak
- Low / Mid / High
- channel: Left / Right / Both は AudioAnalyzer 拡張後に追加
- selected frequency range と beat / transient は後続 Slice

### Transforms

- Gain
- Offset
- Normalize
- Clamp
- Invert
- Gate
- Attack
- Release
- Smoothing

### Evaluation Modes

1. Preview: 現在時刻の解析値で非破壊駆動
2. Render: 音声 source と時刻から決定的に再評価
3. Bake: 指定範囲を sampling し keyframe 化

リアルタイム live input は本 phase に含めず、composition 内の音声 source を対象とする。

### UI

- audio layer / bus source selector
- analysis source と band selector
- input/output range preview
- target property selector
- smoothing / attack / release
- Bake to Keyframes

### Done

- 音量と Low / Mid / High から property を駆動できる
- seek 後も現在時刻の正しい値になる
- preview と offline render の結果が一致する
- Bake 後は Behavior を無効化しても同等の motion を再生できる

## Phase 3: Analytic Motion Behaviors

状態保持シミュレーションより先に、任意時刻から再計算できる解析式として実装する。

### Throw

```text
position(t) = origin + initialVelocity * elapsedTime
```

Parameters:

- start time
- origin: current / explicit
- initial velocity
- direction
- speed

### Gravity

```text
position(t) =
  origin
  + initialVelocity * elapsedTime
  + 0.5 * gravity * elapsedTime^2
```

Parameters:

- gravity vector
- initial velocity
- start time
- optional terminal speed

衝突と bounce は初期範囲外とする。

### Orbit

```text
position(t) =
  center
  + basisU * radiusX * cos(angle)
  + basisV * radiusY * sin(angle)
```

Parameters:

- center: explicit point / referenced object
- radius X / Y
- angular speed
- phase
- clockwise
- plane / axis

### Done

- scrub、reverse、random access で履歴に依存せず同じ位置になる
- Throw と Gravity を同じ ballistic parameter model で扱える
- Orbit center に固定点または property reference を使用できる
- fps変更で軌道が変わらない

## Phase 4: Spatial Field Behaviors

### Slice A: Fixed-Point Field

最初は対象ごとに独立評価できる固定点または参照点 source に限定する。

#### Attractor / Repel

```text
direction = normalize(fieldPosition - objectPosition)
strength = falloff(distance)
offset = direction * strength
```

Parameters:

- field position / referenced transform
- attract / repel
- strength
- radius
- falloff: constant / linear / inverse / inverse-square
- min distance clamp
- axis mask

#### Vortex

```text
radial = objectPosition - fieldPosition
tangent = perpendicular(normalize(radial))
offset = tangent * swirlStrength + radialDirection * radialStrength
```

Parameters:

- center
- radius
- swirl strength
- inward / outward strength
- falloff
- angular bias
- axis / plane

### Slice B: Multi-Object Influence

- 1 field source -> multiple target objects
- multiple fields -> 1 target object
- deterministic field accumulation order
- source and target filters
- per-field weight
- evaluation diagnostics

### Slice C: Mutual Interaction

複数オブジェクトが互いを source / target とする相互作用は最後に扱う。

- immutable frame snapshot から全 force を計算
- evaluation中の値を別 target の入力に即時反映しない
- pair count と対象数の上限
- symmetric interaction option
- dependency cycle とは別の simulation group として管理
- bake 推奨と cost warning

### Done

- 固定点 Attractor / Repel / Vortex が random access で安定する
- 複数 field の合成順による非決定性がない
- mutual interaction が対象列挙順に依存しない
- singularity、NaN、過大速度を防止できる

## Phase 5: Authoring, Presets, and Bake

### Presets

- Parameter Follow
- Audio Pulse
- Audio Bass Scale
- Throw
- Drop with Gravity
- Circular Orbit
- Spiral In / Spiral Out
- Attractor
- Repel
- Vortex

### Authoring

- Behavior add / remove / reorder / enable
- source / target picker
- property row status indicator
- unresolved / cycle / expensive evaluation diagnostics
- duplicate layer 時の reference policy
- preset save/load

### Bake

- frame range
- sample rate / subframe rate
- key reduction tolerance
- overwrite / additive policy
- cancel で元状態へ戻す
- bake後のBehavior disable / retain 選択

### Done

- UIだけで各初期Behaviorを構成できる
- bake結果を undo/redo できる
- error と高コスト状態をユーザーが特定できる

## Suggested Implementation Order

| Order | Slice | Reason |
|---|---|---|
| 1 | Phase 0: Contract | 保存、評価順、循環検出を共通化 |
| 2 | Phase 1: Parameter Link | 最小の source -> target 経路を確立 |
| 3 | Phase 2: Audio Behavior | 既存 AudioAnalyzer と property path を活用 |
| 4 | Phase 3: Throw / Gravity / Orbit | 決定的な解析式で motion を追加 |
| 5 | Phase 4A: Fixed-Point Fields | 安全な Attractor / Repel / Vortex |
| 6 | Phase 5: Authoring / Bake | 共通UXと確定ワークフロー |
| 7 | Phase 4B: Multi-Field | 複数 source / target へ拡張 |
| 8 | Phase 4C: Mutual Interaction | snapshot方式で相互作用を追加 |

## Verification Plan

ビルド・テストの実行はユーザーの明示指示後に行う。

### Contract

- save/load round trip
- missing / renamed / deleted reference
- self-link / two-node cycle / longer cycle
- duplicate layer / duplicate composition

### Time

- sequential playback
- random seek
- reverse playback
- 30 / 60 fps comparison
- subframe evaluation

### Audio

- silence / clipping / short source / mismatched sample rate
- seek and loop boundary
- preview / render / bake comparison

### Motion and Fields

- zero duration / zero radius / zero distance
- negative time before start
- extreme strength and speed
- multiple fields with stable accumulation
- mutual interaction independent of object iteration order

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Property Link が raw pointer 永続化に依存 | stable ID / path resolver を Phase 0 で導入 |
| keyframe とBehaviorが値を奪い合う | 評価順と additive / replace policy を固定 |
| Audio preview とrenderが不一致 | live meterではなく時刻指定の音声解析を正規経路にする |
| physicsがseekで飛ぶ | 初期 motion は解析式、状態依存は bake / simulation group に限定 |
| Attractorのゼロ距離発散 | min distance、strength clamp、finite check |
| 相互作用が順序依存 | immutable frame snapshot から一括評価 |
| 大量targetで重い | target上限、診断、bake、空間indexは必要時のみ追加 |
| Expressionと責務が重複 | Behaviorは構造化preset、Expressionは高度な任意変換として分離 |

## Cross References

- `docs/planned/MILESTONE_PROPERTY_REFERENCE_LINKING_2026-05-11.md`
- `docs/planned/MILESTONE_PICK_WHIP_UI_2026-06-02.md`
- `docs/planned/MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md`
- `docs/planned/MILESTONE_EXPRESSION_SUBFRAME_TIMESTEP_POLICY_2026-06-07.md`
- `docs/planned/IMPLEMENTATION_ANIMATION_PHYSICS_2026-03-22.md`
- `docs/planned/MILESTONE_MOTION_EXTENDED_2026-06-02.md`
- `docs/analysis/REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md`

## Repository Boundary

この文書は親リポジトリ側の計画のみを変更する。実装時に `Artifact` / `ArtifactCore` の変更が必要になった場合は、対象 phase ごとに明示確認を取り、parent-child Git workflow に従う。
