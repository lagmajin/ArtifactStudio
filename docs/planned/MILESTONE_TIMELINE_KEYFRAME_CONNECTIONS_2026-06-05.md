# マイルストーン: Timeline Keyframe Connections

> 2026-06-05 作成

## 目的

After Effects 風に、キーフレーム同士を薄い線でつないで、どこでアニメーションが起きているかを視覚的に追えるようにする。

点が点々としているだけだと変化区間が読みにくいため、同一プロパティの連続 keyframe を背景の太めラインで結び、アニメーションの「続いている感」を出す。

---

## 判断

- いける
- 既存の `ArtifactTimelineTrackPainterView::KeyframeMarkerVisual` には、線を引くために必要な `layerId` / `propertyPath` / `frame` / `trackIndex` がすでにある
- そのため、新しいデータ層を作るより、描画側で keyframe を並べて区間を描く方が自然

ただし、線を強くしすぎると marker や playhead を邪魔するので、最初は「薄い背景ライン」として入れるのが安全。

---

## Goal

- 同じ property の keyframe 群が、点だけでなく区間として読める
- どこで変化しているか、どこが持続しているかが一目で分かる
- 密度の高い timeline でも、動きの流れが追いやすい
- 既存の selection / hover / current-frame 表現と衝突しない

---

## Why Now

- keyframe 点表示はすでにある
- 色分けの議論と相性がよく、意味のある keyframe 群を線で束ねると可読性が上がる
- 変化区間の視認性は、歩きモーション、口パク、揺れ、フェードなどの確認で特に効く

---

## Scope

### In Scope

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- 必要なら `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`

### Out Of Scope

- 新しい global signal / slot の追加
- keyframe データモデルの再設計
- `DiligentEngine` / DX12 backend の変更
- QtCSS の追加

---

## Current Ground Truth

- marker の実体は `KeyframeMarkerVisual`
- marker は `layerId` / `propertyPath` / `frame` を持っている
- 描画は `ArtifactTimelineTrackPainterView` が担当している
- `ArtifactTimelineKeyframeModel` 側で property path 名寄せができる

つまり、線を引くための順序付けと grouping は、今ある marker 配列だけで作れる。

---

## Problem Statement

点だけの keyframe 表示だと、次のことが分かりにくい。

- どの区間で値が変化しているか
- どの keyframe 群が同じ motion を構成しているか
- 間隔が詰まっているときに、どこからどこまでが一連の動きか

特に、歩きモーションや口パクのような「密な区間」が多い素材では、区間の可読性が重要になる。

---

## Design Rules

- 線は marker より控えめにする
- 線は「背景の帯」に近い見せ方を優先する
- selected / hovered / current-frame-hit を邪魔しない
- 同一 property の連続 keyframe を主対象にする
- 線の意味は「この区間にアニメがある」ことに限定し、状態表示を兼ねさせすぎない

---

## Phases

### Phase 1: Connection Semantics

目的:
どの keyframe をどの単位で結ぶかを決める。

作業項目:

- 同一 `layerId + propertyPath` の keyframe を frame 順に並べる
- 隣接 keyframe 間を segment として扱う
- segment を全区間で描くか、一定閾値以上の近接区間だけ描くかを決める
- keyframe が 1 個だけの property は線を出さない

Done when:

- 「何と何を結ぶか」が説明できる
- どの条件で線が出るかが曖昧でない

### Phase 2: Background Line Rendering

目的:
marker の背後に薄い連結線を描く。

作業項目:

- `ArtifactTimelineTrackPainterView::paintEvent()` の marker 描画前に segment を描く
- 線は太めで薄く、marker より下層に置く
- current-frame 上の marker や selection 表現を邪魔しない色と alpha にする
- track / lane の上下に収まるように描画範囲を絞る

Done when:

- timeline 上で「区間」が背景として読める
- 点の視認性が落ちない

### Phase 3: Semantic Refinement

目的:
区間の意味を少しだけ読みやすくする。

作業項目:

- ease / bezier / roving などの状態を必要なら線の見た目に反映する
- 同じ property でも密度が高すぎる場合の省略ルールを決める
- transform / motion / effect で線の色系統を分けるか検討する

Done when:

- 線がただの飾りではなく、変化区間の手がかりになる
- 見た目がうるさくなりすぎない

### Phase 4: Readability Polish

目的:
長い timeline でも邪魔にならない仕上げにする。

作業項目:

- zoom が低いときは線を簡略化する
- marker が多いときのコントラストを調整する
- selected layer の強調と連結線の強調を分離する

Done when:

- 近くで見ても遠くで見ても破綻しない
- 線が playhead や selection の代わりになっていない

### Phase 5: Regression Check

目的:
実運用で読みにくくならないことを確認する。

作業項目:

- 歩きモーションの連続 keyframe
- 口パクの高密度 keyframe
- 揺れの反復 keyframe
- フェードや effect の混在

Done when:

- 代表ケースで「どこで動いているか」が追いやすい
- 既存の編集操作が壊れない

---

## Implementation Notes

- まずは描画のみで完結させる
- 新しい保存形式は不要
- 線は「薄い背景帯」として扱う
- 色分けとの併用が前提なので、線は主役にしすぎない
- 既存の `marker.color` や selection 色と衝突しないようにする

### Suggested First Pass

- 連続する keyframe の間に、半透明の太線を描く
- 線の太さは zoom に応じて少しだけ変える
- 1 property 内の複数 keyframe を結ぶだけに絞る

---

## Done Criteria

- timeline 上で keyframe 群が区間として読める
- 変化している場所が点だけより追いやすい
- selection / hover / playhead の可読性が保たれる
- 実装が描画側中心で閉じている

---

## Next Execution Slice

Phase 1 は、どの keyframe をどの単位で結ぶかを先に固定する。

### Phase 1A の着手点

1. 同一 `layerId + propertyPath` の keyframe を frame 順に並べる
2. 隣接 keyframe 間を segment として扱う
3. segment を全区間で描くか、一定閾値以上の近接区間だけ描くかを決める
4. keyframe が 1 個だけの property は線を出さない

### Phase 1 完了条件

- 「何と何を結ぶか」が説明できる
- どの条件で線が出るかが曖昧でない
- 1 property 内の複数 keyframe を結ぶだけに絞れている

### Phase 2A の着手点

1. marker 描画前に segment を描く
2. 線は marker より下層に置く
3. current-frame 上の marker や selection 表現を邪魔しない alpha にする
4. track / lane の上下に収まるよう描画範囲を絞る

### Phase 2 完了条件

- timeline 上で「区間」が背景として読める
- 点の視認性が落ちない
- selection / hover / playhead の可読性が保たれる

### Phase 3 への前提

- ease / bezier / roving の見た目反映は segment semantics が固まってからでよい
- zoom に応じた簡略化は背景帯が読める状態になってから入れる

---

## Related Docs

- [MILESTONE_TIMELINE_COLOR_KEYFRAMES_2026-06-05.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_COLOR_KEYFRAMES_2026-06-05.md)
- [MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md)
- `M-TL-10 Timeline Feature Implementation / Interaction Surface` の Phase 4 に吸収済み

---

## 2026-07-25 現状確認

実装済み（描画フェーズ）。`ArtifactTimelineTrackPainterView.cppm` に
`KeyframeConnectionSegment` と segment 収集・描画処理があり、同一
`layerId + propertyPath + trackIndex` の marker を frame 順に並べ、隣接点を
Bezier 曲線で接続している。marker の `incomingBezier` / `outgoingBezier` の
handle、選択レイヤー／選択 marker に応じた alpha、zoom に応じた短区間の省略、
dirty rect による可視範囲判定も実装済みで、marker 描画前の背景線として動作する。

未確認・未実装扱い:

- ease / roving 等を線の意味として表現する Phase 3 の拡張
- 高密度 marker や低 zoom 時の表示品質の実機確認
- 歩き、口パク、反復揺れ、fade/effect 混在での regression 確認
- 線に対する独立した編集・保存モデル（現状は既存 marker の描画から生成）

したがって本マイルストーンは「Phase 1〜2 相当: 実装済み、Phase 3〜5 は要検証・拡張」と整理する。
