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

### Dedicated Envelope Insert Dialog

通常のレイヤー作成ダイアログには混ぜず、`Envelope Effect` 専用の挿入ダイアログを用意する。

理由:

- 平面 / テキスト / 画像などの基本作成フローを重くしない
- 「In / Out 付近に短い効果を足す」という目的を明確にする
- effect preset、duration、strength、target property の選択を専用 UI として育てられる
- 将来、選択済みレイヤーへの後付け、複数レイヤーへの一括適用、bake へ広げやすい

候補名:

- `Envelope Effect`
- `Entrance / Exit Effect`
- `Layer In/Out Effect`
- `Quick In/Out FX`

Phase 1 では `Envelope Effect` を内部名、UI 表示は `In/Out Effect` 寄りにする。

### Starter Flow

最初の入口は次の 2 種類に分ける。

1. `Layer > Add In/Out Envelope...`
   - 選択中レイヤーへ後付けする
   - レイヤーが未選択なら disabled
   - layer in/out を読んで envelope range を自動提案する

2. `Layer > New > Envelope Layer...` は Phase 2 以降
   - 新規レイヤー作成と envelope 適用をまとめる高度な入口
   - 既存の `CreateSolidLayerSettingDialog` とは分ける
   - 「素材を作る」より「演出付きで置く」ための導線として扱う

通常の `CreateSolidLayerSettingDialog` には、Phase 1 では envelope UI を入れない。
必要になった場合も、チェックボックス 1 つで専用ダイアログへ遷移する程度に留める。

### Dialog MVP

`Envelope Effect` ダイアログの MVP は、複雑な curve editor ではなく、よく使う選択肢だけを置く。

必須項目:

- preset: `Blur In/Out`, `Opacity Fade In/Out`, `Glow Pop In`, `Chromatic Exit`
- target: 選択中レイヤー / 選択中 effect property
- apply side: `In`, `Out`, `In + Out`
- duration: frames
- strength: 0..1
- easing: Phase 1 は固定、Phase 2 で選択式
- replace policy: existing envelope を置換 / 追加

初期値:

- target layer: current selected layer
- duration: 12 frames
- strength: 1.0
- side: `In + Out`
- replace policy: same target preset は置換

### Preset Semantics

ユーザーが期待する挙動は「効果の量」ではなく「入退場の見た目」なので、preset 名は effect property 名より演出名を優先する。

例:

- `Soft Blur In/Out`
- `Fast Blur In`
- `Fade Out`
- `Glow Pop In`
- `Chromatic Exit`
- `Exposure Flash In`

内部的には既存 effect property の envelope として保存する。
必要な effect がレイヤーに無い場合は、Phase 1 では自動追加できるものだけ対応する。
対応できない preset は disabled にする。

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

### Preset Browser / Starter Flow

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

### Phase 2: Property-Scoped Insert MVP

完了条件:

- effect property row から `前に挿入` / `後ろに挿入` できる
- 挿入対象は preset 名ではなく、選択中 property 自体として扱う
- 挿入時に通常 keyframe を作らない
- layer in/out を動かしても envelope が追従する
- 最初の UI は `duration` / `strength` / `opacity追従` に絞る
- 複雑な curve editor や preset browser を先に作らない

最初の対象:

- `blur.radius`
- `opacity`
- `exposure`
- `glow.intensity`

最初の形状:

- `前`: layer 左端に追従する `/` 形
- `後ろ`: layer 右端に追従する `\` 形
- Phase 1/2 では複雑な easing 種別を持たず、直線 envelope を優先する

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

- Inspector / property row から envelope の有無と基本設定を触れる
- Timeline 上で entry / exit envelope の存在が読める
- keyframe lane と envelope band が視覚的に混ざらない
- UI 名称が `docs/WIDGET_MAP.md` の責務分担に沿っている
- property row、Inspector row、Timeline band の責務が分かれている

主な作業:

- `ArtifactPropertyWidget` に `前に挿入` / `後ろに挿入` affordance を追加する
- `ArtifactTimelineTrackPainterView` に envelope band 表示を追加する
- `ArtifactInspectorWidget` の effect stack から対象 property の envelope 状態を要約する
- keyframe edit modal と envelope drag 編集を混ぜない

### Phase 4.25: Direct Manipulation Editing

完了条件:

- Timeline の envelope band を直接ドラッグして `duration` を変更できる
- Timeline の envelope band を上下ドラッグして `strength` を変更できる
- `opacity追従` などの補助モードを小さい popover / context menu から切り替えられる
- keyframe diamond と envelope band の hit target が衝突しにくい

主な作業:

- band 端ドラッグで長さ変更する hit test を追加する
- band 本体の上下ドラッグで strength を変える interaction を追加する
- double click または context menu で補助モード UI を開く
- envelope 選択時の強調表示と delete 操作を整理する

### Phase 4.5: Creation Flow Integration

完了条件:

- 新規レイヤー作成時に `前` / `後ろ` の envelope 挿入を軽量オプションとして選べる
- 既存の `CreateSolidLayerSettingDialog` の基本作成 UX を壊さない
- レイヤー作成直後の effect property に対して envelope を初期挿入できる

主な作業:

- 作成ダイアログに重い preset browser を持ち込まず、`なし / 前 / 後ろ / 前後` 程度の軽い入口を検討する
- effect 選択後に対象 property と side を解決する service を用意する
- apply 後に selection / timeline refresh だけを明示的に行う
- 新規 global signal-slot 経路を増やさない

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
- preset 名中心の UI に寄せると、ユーザーが「effect property に前後を挿す」感覚とずれやすい
- drag 操作を詰めずにダイアログ項目だけ増やすと、結局 keyframe workflow より遅くなる

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
- `docs/planned/MILESTONES_BACKLOG.md`
