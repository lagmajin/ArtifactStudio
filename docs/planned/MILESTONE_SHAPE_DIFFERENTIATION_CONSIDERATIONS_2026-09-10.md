# Shape Differentiation: 検討案 (AE弱点の残り)

**最終更新:** 2026-09-10
**Status:** 検討 (未着手) — 実装済みの Parametric-alive / Mask live link の次段候補
**Goal:** AEシェイプ層の弱点のうち、今回見送った4件の製品化順序と前提条件を固定する。

## 前提 (実装済み)

- Parametric-alive revert (`ArtifactShapeLayer::revertToParametric`, VP右クリック `Revert to Parametric`)
- Shape->Mask live link (`setShapeMaskLiveLink` / `syncLiveLinkedMask`, Layer menu `シェイプをマスクにリンク（live）`)

## C1. Style Ramp Repeater + 単線連結モード

- **AE gap:** Repeaterは変形+opacityのみ。幅・色・グラデのper-copy変化不可。単線連結リピートは有料expression頼み。
- **Artifact route:** `ArtifactCore/include/Shape/Repeater.ixx:153-184 process()` は行列+opacity乗算のみ。ramp追加はここ。
- **Scope:** 中。まずCPUで幅・色・taper・dash offsetのramp + Concatモード。GPU instancingはC4と一体で後段。
- **Precondition:** なし (いつでも可)。

## C2. Fill対応Trim + per-copy stagger

- **AE gap:** Trimはstroke専用でfillに使うと崩れる。per-copyずらしは式頼み。
- **Artifact route:** `TrimPaths.ixx:139-170` はstroke前提 (`setClosed(false)`)。fill版はsoft-wipe (alpha勾配) を新設。`ShapeStackNode` (`ArtifactShapeLayer.ixx:290-301`) の順序が明示的なのでAE式の配置混乱が起きにくい。
- **Scope:** 中。ただしstack操作UI自体が未完了 (`MILESTONE_SHAPE_PATH_NATIVE_RENDER_PIPELINE` 残作業) のため、UIは順序ドラッグ最小から。
- **Precondition:** stack list UIの最小実装。

## C3. 可変幅ストローク (Widthツール)

- **AE gap:** Taperは後付けで始終端のみ。頂点別筆圧なし。
- **Artifact route:** `taperStart/End` + `drawStyledPolyline` 移行済み。頂点別widthはstroke geometry packet化の完了が前提。
- **Scope:** 中。まずtaper curve化 (始終端→カーブ) で止めるのが現実的。
- **Precondition:** 特殊strokeのgeometry化完了。

## C4. RepeaterのGPU instancing

- **AE gap:** 大量RepeaterがCPU律速。
- **Artifact route:** `triangulate()`/`flattenSubpaths()` のGPU三角形渡しは済み。RepeaterはCPU複製のまま。
- **Scope:** 大。Diligent境界・cache無効化と一体の建築案件。
- **Precondition:** C1のCPU rampが先。Diligent低レベル変更は避ける方針を維持。

## 順序案

C1 → C2 → C3 → C4。いずれもビルド・描画比較はユーザー許可後に実施。
