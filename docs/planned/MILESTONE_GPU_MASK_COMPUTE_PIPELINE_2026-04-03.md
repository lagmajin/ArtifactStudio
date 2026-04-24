# Milestone: GPU Mask Cutout Compute Pipeline (2026-04-03)

**Status:** Draft
**Goal:** レイヤーマスクの適用を compute shader 経由に寄せ、`cv::Mat` 前提の CPU 切り抜きを段階的に減らす。

---

## 目的

現状の mask は、レイヤーの `MaskPath` 群を CPU 側で rasterize して `cv::Mat` の alpha に乗算している。
この構成でも正しくは動くが、以下の点で拡張余地が小さい。

- mask 適用が render path の途中で CPU 依存になる
- preview / playback / export で同じ切り抜き処理を共有しづらい
- mask image を GPU 側の中間資源として再利用しにくい

この milestone では、まず **mask image を GPU に渡して alpha cutout を compute shader で行う**。
その後、必要に応じて path rasterization 自体も GPU 化する。

---

## 現状

今の実装は以下の流れ。

1. `LayerMask::compositeAlphaMask()` で `MaskPath` 群を合成
2. `LayerMask::applyToImage()` で `cv::Mat` の alpha に乗算
3. `ArtifactCompositionRenderController` 側で surface を描画へ戻す

このため、mask は機能していても、render の中核はまだ CPU 切り抜きに寄っている。

---

## 方針

### 原則

1. 既存の CPU fallback は残す
2. mask の見た目と cutout 結果を先に GPU 側へ寄せる
3. `DiligentEngine` / backend の低レベル改変は避ける
4. path rasterization の全面 GPU 化は後段に回す
5. preview / playback / export のいずれでも同じ mask contract を使う

### まず扱う単位

- `MaskImage`
  - 1 ch でも 4 ch でも良いが、まずは alpha mask として扱う
- `MaskCutoutInput`
  - source layer texture / mask texture / opacity / bounds
- `MaskCutoutOutput`
  - cutout 済み texture / surface

---

## Scope

- `Artifact/src/Mask/*`
- `Artifact/src/Widgets/Render/*`
- `Artifact/src/Render/*`
- `Artifact/include/Mask/*`
- `Artifact/include/Render/*`
- `ArtifactCore/include/Graphics/Shader/Compute/*`
- `ArtifactCore/src/Graphics/*`

---

## Non-Goals

- `MaskPath` の完全 GPU rasterizer 化を最初からやること
- `DiligentEngine` を広範囲に触ること
- mask editor UI の全面改修
- flat compositing 以外の deep compositing と同時に解くこと

---

## Phases

### Phase 1: Mask Texture Contract

**Goal:** CPU / GPU で共通に扱える mask texture contract を定義する。

- `MaskPath` から 1 枚の alpha mask を作る contract を定義する
- `MaskImage` のサイズと座標系を layer surface に合わせる
- CPU fallback を維持する
- cache key に mask 変更を反映する

**Done when:**

- mask 画像を texture として扱う入口ができる
- CPU / GPU どちらでも同じ結果を比較できる

---

### Phase 2: Compute Mask Apply

**Goal:** source texture + mask texture を compute shader で切り抜く。

- `SrcTex` / `MaskTex` / `OutTex` を受ける compute shader を作る
- alpha 乗算を GPU 側に移す
- layer surface cache と texture cache を接続する
- fallback として CPU apply を残す

**Done when:**

- mask が付いた layer の cutout を compute shader で実行できる
- preview で CPU 版と比較できる

---

### Phase 3: Composition Integration

**Goal:** composition render path に compute mask cutout を接続する。

- `ArtifactCompositionRenderController` の layer surface 生成に接続する
- `Rasterizer effect + mask` の順序を整理する
- `preview`, `playback`, `export` で同じ cutout contract を使う
- 失敗時に CPU fallback へ落とす

**Done when:**

- render path の中で mask cutout が自然に動く
- 既存の layer edit / undo / cache invalidation が壊れない

---

### Phase 4: Path Rasterization Upgrade

**Goal:** 必要なら mask path の rasterization 自体も GPU 化する。

- vertex list / packed path data を shader に渡す
- fill rule / hole / subtract / intersect を整理する
- AA の品質を CPU 実装と比較する

**Done when:**

- GPU 側で mask image を生成できる
- CPU rasterizer との差分を診断できる

---

## Related

- `docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`
- `docs/planned/MILESTONE_DEEP_COMPOSITING_2026-03-31.md`
- `docs/planned/MILESTONE_RENDER_PATH_DECOMPOSITION_2026-03-31.md`
- `docs/planned/MILESTONE_MATTE_MASK_TIME_REMAP_SPLIT_ROUTE_2026-04-24.md`

## Notes

- 現状の `LayerMask::applyToImage()` は壊さない。
- 最初は GPU 版を追加し、切り替え可能にする。
- path rasterization の GPU 化は「できるか」ではなく「必要になったらやる」段階に留める。
