# Timeline Flat Keyframe View / U-Key Style Filter

`ArtifactTimelineWidget` の keyframe 表示を、After Effects の `U` キーに近い思想で再編する milestone です。

現在の timeline は `Layer` / `Transform` / `Physics` のような property group 階層をそのまま見せているため、編集対象が増えるとノイズが大きくなります。  
この milestone では、**デフォルトを「キーフレームのみ」ビューに寄せて、必要なときだけ全プロパティへ戻せる** ようにします。

## Progress

- Timeline header now exposes `All Properties` / `Keyframes Only`.
- `ArtifactLayerPanelWidget` now defaults to `Keyframes Only`, flattens visible property labels to keyframed properties, and prefers humanized/display labels.
- Phase 1 and Phase 2 are started in code; Phase 3+ remain for deeper expand/collapse behavior and broader property coverage.
## Goal

- キーフレームが存在する property だけを timeline にフラット表示する
- `Transform` / `Physics` のような group 階層を、編集の邪魔にならない形に吸収する
- `U` キー相当で「キーフレームありのプロパティだけ」へ即切り替えできる
- `[全プロパティ表示] [キーフレームのみ]` の切り替えを持つ
- デフォルト表示を `キーフレームのみ` にして、編集密度を下げる

## Scope

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScene.cppm`
- `ArtifactCore/src/Property/*`
- 必要なら `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

## Non-Goals

- Graph Editor の全面改修
- easing / tangent の完全再設計
- property tree の完全廃止
- すべての layer type の lane 専用 UI 化

## Background

今の timeline は keyframe marker を描ける一方で、表示単位はまだ layer / group 寄りです。  
そのため、`Transform` のような大分類は見えても、実際に編集したい `position.x` や `rotation` が埋もれやすくなっています。

AE で `U` キーが効くのは、**「動いているものだけ見たい」** という編集中の欲求に直接応えているからです。  
Artifact でも同じく、`キーフレームだけを先に見せる` 方が、タイムラインの実用感が上がります。

## Phases

### Phase 1: Flat Filter

- 目的
  - キーフレームを持つ property だけを timeline に出す

- 実装の要点
  - property group を走査して、animatable かつ keyframe を持つものだけ残す
  - lane 表示は `displayLabel` 優先、なければ humanized name
  - `Transform` などの group 名は補助表示に回す

- DoD
  - 何も動いていない property は初期表示で邪魔しない
  - selected layer の keyframe が一目で追える

### Phase 2: Toggle Surface

- 目的
  - `[全プロパティ表示] [キーフレームのみ]` を UI に置く

- 実装の要点
  - toolbar / header / menu のいずれかに切り替え口を置く
  - `U` キーで `キーフレームのみ` をトグル
  - 表示モードの状態を timeline header に出せるようにする

- DoD
  - 1 操作で `全表示` と `キーフレームのみ` を切り替えられる
  - どちらの状態か迷わない

### Phase 3: Expand / Collapse Behavior

- 目的
  - AE っぽく「必要なら展開、普段は折りたたみ」にする

- 実装の要点
  - layer 単位の展開 / 折りたたみ
  - selected layer を優先して自動展開
  - keyframe のある property だけを見せる簡易モードを維持

- DoD
  - 全プロパティ表示でも UI が破綻しない
  - keyframe only へすぐ戻れる

### Phase 4: Property Coverage

- 目的
  - 表示対象を主要 property に広げる

- 対象例
  - `transform.position.x`
  - `transform.position.y`
  - `transform.scale.x`
  - `transform.scale.y`
  - `transform.rotation`
  - `layer.opacity`
  - `effect parameter`

- DoD
  - 表示が group 単位ではなく、実際の編集対象単位に寄る

## Risks

- `property group` をそのままフラット化するとラベルが長くなりすぎる
- 全プロパティ表示と keyframe only の責務が混ざると実装が散らかる
- keyframe なし property を隠しすぎると、初見で何が編集できるか分かりにくくなる

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4


