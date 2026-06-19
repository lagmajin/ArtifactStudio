# マイルストーン: Timeline Color Keyframes

> 2026-06-05 作成

## 目的

After Effects 的な「キーフレームに色の意味を持たせる」表現を、タイムライン上で読めるようにする。

歩きモーション、口パク、揺れ、フェードのような編集意図を色で見分けられるようにして、後から見返した時の探索コストを下げる。

---

## まずの判断

- 既存の `ArtifactTimelineTrackPainterView::KeyframeMarkerVisual` には `color` がある
- そのため、土台を新設するというより、既存 marker 色の割り当て規則を拡張する作業として進められる
- ただし、色だけに意味を持たせると破綻しやすいので、ラベルや形状、selection 表現と組み合わせて定義する必要がある

結論としては「いけそう」だが、色の taxonomy を先に決めた方が安全。

---

## Goal

- keyframe の種類や用途を色で判別できる
- 同じ timeline 上で、歩き、口パク、揺れ、フェードなどの意図が並んでも追いやすい
- selected / hovered / current-frame-hit の状態と、色の意味が衝突しない
- 既存の keyframe 編集導線を壊さずに導入する

---

## Why Now

- `KeyframeMarkerVisual::color` がすでに描画経路に存在する
- timeline の keyframe 表示は「見える」段階に入っており、次は「読める」段階が必要
- text / effect / transform / motion path が増えるほど、同色の keyframe 群は見分けにくくなる

---

## Scope

### In Scope

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`
- 必要なら `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`

### Out Of Scope

- Graph Editor の全面導入
- 新しい global signal / slot の追加
- `DiligentEngine` / DX12 backend の変更
- QtCSS の追加
- 色選択ダイアログの新規採用

---

## Current Ground Truth

- marker 描画は `ArtifactTimelineTrackPainterView`
- marker 色の割り当ては track / property 側で作る
- keyframe の実体は `ArtifactTimelineKeyframeModel` と property API が source of truth
- state 強調は selected / hovered / current-frame-hit の既存表現を維持する

---

## Problem Statement

現状の keyframe は、形や状態は読めても「これはどの意図の keyframe か」が一目で分かりにくい。

特に次のケースで色分けの価値が高い。

- 歩きモーションのように大量の transform keyframe が並ぶ
- 口パクのように短い間隔で密に並ぶ
- 揺れのように繰り返し系のキーが続く
- フェードや effect のように意味が別系統のキーが混在する

---

## Design Rules

- 色は「状態」ではなく「意味」に寄せる
- selected / hovered は既存の selection 表現で上書きする
- 同じ意味の keyframe は、なるべく同じ色系統にまとめる
- 読みやすさを優先し、色数は最初から増やしすぎない
- 色だけでなく label / lane / grouping でも追えるようにする

---

## Phases

### Phase 1: Color Taxonomy Definition

目的:
どの keyframe にどの色を与えるかを決める。

作業項目:

- 主要カテゴリを決める
- 例: transform / motion / facial / effect / misc
- property path から色カテゴリへ落とす最小ルールを作る
- fallback 色を固定する

Done when:

- 色の対応表が文章で説明できる
- どの keyframe がどのカテゴリに入るかが曖昧でない

### Phase 2: Painter Integration

目的:
色の割り当てを marker 描画に反映する。

作業項目:

- `KeyframeMarkerVisual::color` の生成ルールを拡張する
- selected / hovered / current-frame-hit と競合しない補色処理を決める
- 同色系 keyframe が密集しても視認できるようにする

Done when:

- timeline 上で keyframe の種類が色で判別できる
- 既存の selection 読みやすさが落ちない

### Phase 3: Source Alignment

目的:
property / layer / preset 由来の意味と色を揃える。

作業項目:

- `ArtifactTimelineKeyframeModel` 側の property path 解釈と整合させる
- text / motion / transform / effect の分類を一貫させる
- 将来的に preset や animator 系へ拡張しやすい形にする

Done when:

- keyframe の見た目が property 意味と矛盾しない
- 新しい property 系が増えてもルールを流用しやすい

### Phase 4: Readability Polish

目的:
色を増やしたことで逆に読みにくくならないようにする。

作業項目:

- selected / hovered / current-frame-hit の見え方を再調整する
- 必要なら簡易 legend や summary 表示を追加する
- 高密度場面でのコントラストを確認する

Done when:

- 色が増えても timeline がうるさく見えない
- 目印としての意味が保てる

### Phase 5: Regression Check

目的:
実運用で壊れやすい場面を先に固定する。

作業項目:

- 歩きモーションの連続 keyframe
- 口パクの高密度 keyframe
- 揺れの反復 keyframe
- フェードや effect keyframe の混在

Done when:

- 代表ケースで色が破綻しない
- selection と色の役割が衝突しない

---

## Implementation Notes

- 新しい中央集権的なイベント経路は増やさない
- `QColorDialog` は使わない
- `QtCSS` は追加しない
- 色は意味を持たせるが、意味の唯一の担保にはしない
- 既存の `marker.color` を活用し、描画側の変更を最小化する

### Initial Category Sketch

- transform: 落ち着いた暖色 or 中立系
- motion / walk: 緑〜青系
- lip sync: 近接色の別トーン
- shake / oscillation: 反復感のある別系統
- fade / opacity: 明度差のある専用色
- misc / fallback: 現状の既定色

---

## Done Criteria

- timeline 上で keyframe の意図が色で追える
- selection 表現と色分けがぶつからない
- property / preset / motion 系の keyframe を見返した時に識別しやすい
- 既存の編集操作が壊れない

---

## Related Docs

- [MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md)
- `M-TL-10 Timeline Feature Implementation / Interaction Surface` の Phase 4 に吸収済み
- [MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)
- [MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md](/x:/Dev/ArtifactStudio/docs/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md)
