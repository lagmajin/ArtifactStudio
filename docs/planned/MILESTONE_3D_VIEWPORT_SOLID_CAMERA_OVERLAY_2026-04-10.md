# 3D Viewport Stabilization: Solid / Camera / Overlay

`primitive 3D` と `3D gizmo` はもうある程度描けるが、`Artifact` の 3D 表示はまだ
「検証用の簡易 3D surface」に近い。  
この milestone は、`solid shading`、`camera`、`overlay` を分けて整理し、
3D レイヤーと 3D ビューを実用寄りに寄せるための実行計画。

## Goal

- 3D 形状を「読める」見た目で安定表示する
- camera 操作と表示更新の責務を分ける
- gizmo / bounds / selection / HUD の overlay 順を固定する
- wireframe / solid / overlay を同じ 3D surface 上で破綻なく共存させる

## Current State

- `PrimitiveRenderer3D` で line / arrow / circle / quad / torus / cube は描ける
- `Artifact3DLayer` は mesh を読み込んで wireframe 表示できる
- `Artifact3DGizmo` は move / rotate / scale の補助表示として機能している
- ただし solid は簡易で、カメラと overlay の責務分離はまだ途中

## What Still Feels Weak

- solid mesh が「面として読める」ほど安定していない
- triangulation / face filling が簡易で、複雑 mesh で崩れやすい
- camera の orbit / pan / dolly が editor ごとに散りやすい
- gizmo / bounds / HUD の重なり順が今後の拡張で崩れやすい
- 3D viewer と composition editor の見た目基準が完全にはそろっていない

## Scope

- `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- `Artifact/src/Render/PrimitiveRenderer3D.cppm`
- `Artifact/include/Render/PrimitiveRenderer3D.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cpp`
- `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 必要に応じて `Artifact/docs/MILESTONE_PRIMITIVE3D_RENDER_PATH_2026-03-21.md`
- 必要に応じて `Artifact/docs/MILESTONE_3D_GIZMO_IMPLEMENTATION_2026-03-25.md`

## Non-Goals

- PBR / real-time shadows / complex material graph
- 完全な DCC 3D viewport 再現
- low-level backend の全面再設計
- scene graph の大規模な書き換え

## Design Principles

- まずは solid を安定させ、見た目の読める 3D を優先する
- camera は viewport と renderer の責務を分けて扱う
- overlay は gizmo / selection / HUD / bounds を層として扱う
- 3D surface は editor widget ごとの個別描画に戻しにくい設計にする

## Phases

### Phase 1: Solid Shading Hardening

- mesh の face filling を安定化する
- triangulation の前提を明示する
- backface / depth / alpha の見え方を整理する
- wireframe と solid の切り替えで破綻しないようにする

**Done when:**

- 3D オブジェクトが固まりとして読める
- wireframe / solid を切り替えても大きく崩れない

### Phase 1A: Solid First Execution

`solid` を先に直す理由:

- 3D の「読める / 読めない」を最も左右する
- camera / overlay だけ先に整えても、面が崩れていると全体の印象が改善しない
- `wireframe` は比較的壊れにくいので、まず `solid` の破綻を潰したほうが効果が大きい

優先タスク:

1. `Artifact3DLayer` の solid face filling を安定化する
2. triangulation 前提を docs かコードコメントで明示する
3. face の裏表や alpha の見え方を整理する
4. 3D view 側で solid / wireframe 切り替えの破綻を確認する

### Phase 2: Camera Parity

- orbit / pan / dolly の操作責務を整理する
- perspective / orthographic を切り替えられるようにする
- camera matrix の更新と redraw の境界を固定する

**Done when:**

- 3D viewer と composition editor で camera の考え方が揃う
- zoom / rotate / pan の更新が一貫する

### Phase 3: Overlay Ordering

- gizmo / selection / bounds / HUD の順序を固定する
- 3D editor overlay と 2D overlay を混同しにくくする
- viewport 内の補助表示を共通の overlay 経路へ寄せる

**Done when:**

- overlay の重なり順が壊れない
- gizmo と HUD が競合しにくい

### Phase 4: Mesh Upload And Cache

- mesh の再 upload を毎フレーム起こしにくくする
- static geometry と dynamic transform を分ける
- simple cache hit / miss を追えるようにする

**Done when:**

- 静止 mesh で無駄な再 upload が減る
- 3D 表示が大きくなっても極端に重くならない

## Recommended Order

1. Solid Shading Hardening
2. Camera Parity
3. Overlay Ordering
4. Mesh Upload And Cache

## Implementation Tasks

### Task Group A: Solid Rendering

- `Artifact3DLayer` の face filling を安定化する
- triangulation を前提にするなら、その制約を明示する
- solid / wireframe の切り替えで大きな破綻が出ないか確認する
- 単純 mesh と複雑 mesh の両方で壊れ方を把握する

### Task Group B: Camera Ownership

- camera 操作を `viewport` と `renderer` のどちらが持つか固定する
- perspective / orthographic の切替点を明示する
- orbit / pan / zoom の更新が overlay と独立に進むようにする
- 3D viewer と composition editor の camera state を揃える

### Task Group C: Overlay Stack

- gizmo / bounds / selection / HUD の描画順を固定する
- overlay が widget ごとの手描きに戻らないようにする
- 3D viewport 上の補助表示を共通の overlay 経路に寄せる
- playhead や 2D overlay と衝突しない順序を決める

### Task Group D: Cache And Perf

- mesh の再 upload を毎回発生させない
- static geometry と dynamic transform を分ける
- 3D viewport の redraw で無駄な重複処理がないか見る

## Suggested Execution Order

1. `solid` の安定化
2. `camera` の責務固定
3. `overlay` の順序固定
4. `mesh cache / upload` の最適化

### Why This Order

- `solid` が崩れていると、camera や overlay をいくら整えても見栄えが安定しない
- `camera` が揺れると overlay の座標系が不安定になる
- `overlay` の順序が決まると gizmo / HUD / bounds の責務が明快になる
- cache は最後に回しても、まず見た目と責務が固まっていれば改善しやすい

## Responsibility Map

```text
3D Viewport
├─ Solid Geometry
│  ├─ Artifact3DLayer
│  ├─ PrimitiveRenderer3D
│  └─ ArtifactIRenderer (3D primitive entry points)
├─ Camera
│  ├─ orbit / pan / zoom / dolly state
│  ├─ perspective / orthographic projection
│  └─ viewport sync
└─ Overlay
   ├─ gizmo
   ├─ selection / bounds
   ├─ HUD / diagnostics
   └─ playhead / guide ordering
```

## Validation Checklist

- 3D mesh が wireframe だけでなく solid でも読める
- camera 操作が editor 間で大きくズレない
- gizmo / bounds / HUD の順番が崩れない
- 3D surface が widget ごとの手描きに戻りにくい

## Notes

この milestone は、`Primitive 3D Render Path` の延長として
「3D を描ける」から「3D を読める」へ寄せるためのもの。  
`ImGuizmo Direct Code Port` と相性がよく、gizmo の direct code 化と
overlay 順の固定を同時に進めやすい。
