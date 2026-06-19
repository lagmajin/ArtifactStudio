# Layer Effect Envelope Milestone

**作成日:** 2026-06-19  
**ステータス:** 計画中  
**関連コンポーネント:** AbstractProperty, ArtifactPropertyWidget, ArtifactTimelineTrackPainterView, ArtifactInspectorWidget, Effect Stack

---

## 概要

レイヤーの登場 / 退場に合わせて、ブラー・色調・グローなどのありきたりだが頻出するエフェクト変化を、通常のキーフレームを増やさず扱えるようにする。

この milestone では、エンベロープ専用の新規エフェクト群を作らない。
既存エフェクトの property に適用する **Envelope Preset 群** と、既存 property の時間評価に重ねる **Layer Effect Envelope** として設計する。

---

## 背景

現在の property animation はキーフレーム中心で、細かい演出には強い。
一方で、実制作では次のような短い効果を頻繁に使う。

- レイヤー登場時に blur radius を 24 から 0 へ戻す
- 退場時に saturation / exposure / opacity を落とす
- in/out の数フレームだけ glow や chromatic aberration を足す
- レイヤー長を変えても、入退場効果だけは端に追従してほしい

これらを毎回キーフレームで作ると、timeline の keyframe lane が散らかりやすく、後から layer in/out を動かした時の修正も多い。

Layer Effect Envelope の狙いは、**短い入退場演出を layer boundary に追従する modulation として扱う** こと。

---

## 目標

- 既存のキーフレーム型エフェクト / property animation を壊さない
- keyframe とは別の track として envelope を保持する
- layer in/out に相対追従する entry / exit envelope を作る
- 最初は float 系 effect property に絞る
- preset からすぐ使える実制作向けの entry / exit envelope を用意する
- envelope preset は既存エフェクト property へ適用する
- 必要なら envelope を通常 keyframe に bake できる逃げ道を持つ

---

## 非目標

- Graph Editor の代替を作ること
- すべての property type に最初から対応すること
- 既存 `KeyFrame` 構造を envelope 用に拡張すること
- effect algorithm 自体を envelope 専用に作り替えること
- envelope 専用の blur / color / glow effect 群を新設すること
- レイヤー全体の transition system を一度に刷新すること

---

## 基本方針

### Keyframe とは別系統にする

既存の `AbstractProperty::getKeyFrames()` / `addKeyFrame()` / `clearKeyFrames()` は従来通り keyframe だけを扱う。
Envelope は別ストレージ、別 UI、別 serialization field として追加する。

これにより、既存の keyframe editing、copy/paste、timeline lane、expression bake の挙動を保つ。

### Property evaluation に重ねる

評価順の基本形:

```text
base value
  -> keyframe interpolation
  -> layer-relative envelope modulation
  -> expression / external override policy
  -> final value
```

Expression と envelope の順序は Phase 1 で固定する。
MVP では `value` が既存キーフレーム結果を指すことを維持し、envelope は expression 前後どちらに置くかを明示する。

### Layer boundary に追従する

Envelope の主時間軸は composition absolute time ではなく、次の relative time とする。

- `Entry`: `time - layer.inPoint`
- `Exit`: `layer.outPoint - time`
- `Full`: layer span normalized 0..1

最初は `Entry` / `Exit` のみを対象にする。

---

## データ契約案

### EnvelopeTrack

最小項目:

- `targetPropertyPath`
- `scope`: `Entry` / `Exit`
- `durationFrames`
- `strength`
- `mode`: `Add` / `Multiply` / `Override`
- `easing`
- `enabled`

### EnvelopePreset

Preset は effect property への envelope 定義の集合として扱う。
新しい effect class ではなく、既存 effect catalog の property を target とする。

例:

- `Blur In`
  - target: `effect.blur.radius`
  - scope: `Entry`
  - mode: `Override`
  - value: `24 -> 0`
- `Fade Saturation Out`
  - target: `effect.color.saturation`
  - scope: `Exit`
  - mode: `Multiply`
  - value: `1 -> 0`
- `Glow Pop In`
  - target: `effect.glow.intensity`
  - scope: `Entry`
  - mode: `Add`
  - value: `high -> 0`

既存 effect catalog に必要な target property が無い場合だけ、軽量な標準エフェクトを穴埋めとして追加する。
ただしその場合も envelope 専用 effect ではなく、通常 keyframe / expression / preset からも使える汎用 effect として扱う。

### Compatibility Rule

- Envelope が無い property は現状通り評価する
- Envelope を読めない古い保存形式では、envelope field を無視できる構造にする
- Envelope が存在しても keyframe list は変更しない
- Bake するときだけ通常 keyframe を生成する

---

## UI 方針

### Inspector

Effect property row に envelope の小さな状態表示を持たせる。
ただし property row に複雑な curve editor を埋め込まない。

最初に必要な操作:

- envelope enabled toggle
- preset selection
- duration
- strength
- easing
- entry / exit 切り替え

### Timeline

`ArtifactTimelineTrackPainterView` では keyframe diamond とは別に、薄い envelope band として表示する。

表示方針:

- entry envelope は layer bar の左端に寄せる
- exit envelope は layer bar の右端に寄せる
- keyframe marker と同じ形にしない
- drag editing は Phase 2 以降に回す

### Preset Browser

Effect preset と同じ文法に寄せるが、用途は `Entry / Exit Motion` として見せる。
ユーザー向け名称は `Layer Entrance / Exit Effects` または `Effect Envelope` を候補にする。

---

## Phase 構成

### Phase 1: Contract Freeze

完了条件:

- `EnvelopeTrack` の最小データ契約を固定する
- keyframe と envelope の保存先を分ける方針を明文化する
- evaluation order を固定する
- 対象 property type を `Float` / `Integer` に限定する
- layer in/out relative time の扱いを決める

主な作業:

- `AbstractProperty` に envelope を足す場合の API 案を作る
- `LayerID + propertyPath` に紐づく外部 envelope store 案も比較する
- expression / external override との優先順位を決める
- serialization field 名を決める

### Phase 2: Envelope Preset MVP

完了条件:

- blur / color / glow 系の代表 preset を 5 種類以上定義する
- preset は既存 effect property を target にする
- preset 適用時に通常 keyframe を作らない
- layer in/out を動かしても envelope が追従する

候補 preset:

- `Blur In`
- `Blur Out`
- `Soft Focus Entrance`
- `Fade Saturation Out`
- `Exposure Flash In`
- `Glow Pop In`
- `Chromatic Exit`

### Phase 3: Evaluation Integration

完了条件:

- 既存 keyframe property は挙動を変えず評価できる
- envelope 付き property は keyframe 結果に modulation を重ねられる
- `getKeyFrames()` は envelope を返さない
- envelope 無しプロジェクトの評価コストを増やしすぎない

主な作業:

- envelope evaluator を property evaluation から呼べる形にする
- existing effect property を target として解決する bridge を作る
- hard/soft range clamp の扱いを決める
- `Add` / `Multiply` / `Override` の type policy を実装できる形にする
- bake 用に sampled value を取れる API を用意する

### Phase 4: Inspector / Timeline UI

完了条件:

- Inspector から envelope preset / duration / strength を設定できる
- Timeline 上で entry / exit envelope の存在が読める
- keyframe lane と envelope band が視覚的に混ざらない
- UI 名称が `docs/WIDGET_MAP.md` の責務分担に沿っている

主な作業:

- `ArtifactPropertyWidget` に envelope affordance を追加する
- `ArtifactTimelineTrackPainterView` に envelope band 表示を追加する
- `ArtifactInspectorWidget` の effect stack から preset 適用導線を作る
- keyframe edit modal と envelope edit modal を混ぜない

### Phase 5: Bake / Convert

完了条件:

- envelope を通常 keyframe に bake できる
- bake 後は envelope を無効化または削除できる
- bake 結果が既存 timeline keyframe editing で編集できる

主な作業:

- frame range sampling
- existing keyframe との merge policy
- undo snapshot
- bake preview / confirmation

---

## 実装候補ファイル

設計確認時にまず読む場所:

- `ArtifactCore/include/Property/AbstractProperty.ixx`
- `ArtifactCore/src/Property/AbstractProperty.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/include/Widgets/Timeline/ArtifactTimelineKeyframeModel.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`
- `Artifact/include/Effects/ArtifactAbstractEffect.ixx`
- `Artifact/include/Effects/EffectHostContract.ixx`

最初に target 候補として見る既存エフェクト:

- `Artifact/include/Effects/Blur/BlurEffect.ixx`
- `Artifact/include/Effects/Glow/GlowEffect.ixx`
- `Artifact/include/Effects/Glow/ChromaticGlowEffect.ixx`
- `Artifact/include/Effects/ColorCorrection/ExposureEffect.ixx`
- `Artifact/include/Effects/ColorCorrection/ColorBalanceEffect.ixx`
- `Artifact/include/Effects/ColorCorrection/GrayscaleEffect.ixx`
- `Artifact/include/Effects/ColorCorrection/LevelsEffect.ixx`

注意:

- `ArtifactCore` は子 repo なので、実装時は親子 repo workflow を守る
- user が明示しない限り submodule / child repo を変更しない
- C++20 modules の include / import hygiene を優先する

---

## リスク

- Envelope と keyframe の優先順位が曖昧だと、値がどこで決まったか追いにくくなる
- `Override` を強くしすぎると既存 keyframe animation を隠してしまう
- expression の `value` と envelope の順序を後から変えると互換性が崩れる
- layer outPoint に追従する exit envelope は trim / stretch / slip と衝突しやすい
- UI で keyframe と同じ見た目にすると、編集対象を誤認しやすい

---

## 成功条件

- ユーザーが keyframe を打たずに、layer in/out に追従する blur / color / glow 変化を作れる
- 既存 keyframe animation は envelope 追加前と同じように編集できる
- Timeline 上で keyframe と envelope の違いが一目で分かる
- Layer length を変えても entry / exit effect の意図が保たれる
- 必要なときは通常 keyframe へ bake して既存 workflow に戻せる

---

## 関連

- `docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md`
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`
- `docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`
- `docs/planned/MILESTONE_EFFECT_UI_STANDARDIZATION_2026-06-07.md`
- `docs/planned/MILESTONE_PRESET_BROWSER_STARTER_FLOW_2026-05-31.md`
