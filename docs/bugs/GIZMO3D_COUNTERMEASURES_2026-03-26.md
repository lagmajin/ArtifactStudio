# 3D Gizmo 対策メモ (2026-03-26)

## 目的

3D gizmo が表示されない問題について、これまでに試した対策と、原因切り分けの結果をまとめる。

---

## 根本原因

`ArtifactIRenderer` には 2 つの独立したレンダラーがある。

| レンダラー | 用途 | マトリクス |
|---|---|---|
| `primitiveRenderer_` | 2D 描画 | `externalViewMatrix_` / `externalProjMatrix_` |
| `primitiveRenderer3D_` | 3D 描画 | `viewMatrix_` / `projMatrix_` |

`Artifact3DGizmo::draw()` は `renderer->draw3DArrow()` / `renderer->draw3DCircle()` を呼ぶが、これは `primitiveRenderer3D_` 経由で描かれる。

一方で、以前の `ArtifactIRenderer::setViewMatrix()` / `setProjectionMatrix()` は 2D 側にしか行列を入れていなかった。

結果として:

1. `Artifact3DGizmo::draw(renderer, view, proj)` で view / proj を渡す
2. `ArtifactIRenderer::setViewMatrix()` / `setProjectionMatrix()` に入る
3. 2D renderer にしか反映されない
4. `draw3DArrow()` は 3D renderer で描く
5. 3D renderer の行列は Identity のまま
6. 座標が NDC 外になり、gizmo が表示されない

---

## 実施した対策

### 1. 3D 用 camera matrix API を追加

`ArtifactIRenderer` に以下を追加した。

- `set3DCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj)`
- `reset3DCameraMatrices()`

これは `PrimitiveRenderer3D::setCameraMatrices()` / `resetMatrices()` に直接 forward する。

関連ファイル:

- [`Artifact/include/Render/ArtifactIRenderer.ixx`](x:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactIRenderer.ixx)
- [`Artifact/src/Render/ArtifactIRenderer.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactIRenderer.cppm)

### 2. `setViewMatrix()` / `setProjectionMatrix()` を 3D 側にも forward

`ArtifactIRenderer::setViewMatrix()` と `setProjectionMatrix()` を、2D renderer だけでなく 3D renderer にも反映するように変更した。

これで `Artifact3DGizmo::draw()` が既存の API を呼ぶだけでも、`primitiveRenderer3D_` に行列が入る。

関連ファイル:

- [`Artifact/src/Render/ArtifactIRenderer.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactIRenderer.cppm)

### 3. `Artifact3DGizmo::draw()` 側でも 3D camera matrices をセット

`Artifact3DGizmo::draw()` 内で、描画前に以下を呼ぶようにした。

- `renderer->setViewMatrix(view)`
- `renderer->setProjectionMatrix(proj)`
- `renderer->set3DCameraMatrices(view, proj)`

描画後は `reset3DCameraMatrices()` を呼ぶようにした。

関連ファイル:

- [`Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/Artifact3DGizmo.cppm)

### 4. 3D gizmo の見た目を少し太くした

3D gizmo の矢印とスクリーン円のサイズ係数を少し増やした。

- move axis arrow: `0.10f` -> `0.12f`
- scale axis arrow: `0.08f` -> `0.10f`
- screen circle: `0.35f` -> `0.42f`

関連ファイル:

- [`Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/Artifact3DGizmo.cppm)

---

## 追加確認

この問題は `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` 側で作っている view / proj とも関係する。

現在は gizmo 描画前に viewport サイズから以下を組んでいる。

- view: `translate(panX, panY, 0)` + `scale(zoom, zoom, 1)`
- proj: `ortho(0, viewportW, viewportH, 0, -1000, 1000)`

ただし、3D renderer 側に確実に入るようになっていないと、ここで作った行列は意味を持たない。

関連ファイル:

- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

---

## 現在の結論

表示されない本命の理由は、`ArtifactIRenderer` の 2D / 3D 行列が分離していて、`Artifact3DGizmo` の行列が 3D renderer に届いていなかったこと。

次に詰めるなら、以下を確認する。

1. `PrimitiveRenderer3D` が本当に `setCameraMatrices()` 済みの値を使っているか
2. `renderOneFrameImpl()` の view / proj が gizmo の座標系と一致しているか
3. `reset3DCameraMatrices()` が描画後に意図せず上書きしていないか
