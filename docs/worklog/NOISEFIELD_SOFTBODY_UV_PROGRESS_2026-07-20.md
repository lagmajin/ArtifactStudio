# NoiseField / SoftBody UV Progress

> 2026-07-20

## NoiseField

`Math.Noise` に値型 `NoiseField` を追加した。

- permutation table を instance 内に保持
- `setSeed()` は自身の table だけを更新
- `perlin()` / `fractal()` は共有 mutable state を参照しない
- 既存 `NoiseGenerator` の static API は互換維持

## SoftBody UV grid

`Physics.SoftBody::SoftBodySolver` に `SoftBodyUVVertex` と `getUVVertices()` を追加した。

- 既存 grid point の順序を維持
- normalized UV `(0..1)` を列・行から決定
- solver の評価、snapshot、既存 CPU 描画経路は変更しない
- 画像/動画の GPU deformation grid は次段でこの packet を利用する

未対応:

- GPU vertex buffer upload
- Image/Video layer の UV deformation
- preview/render 共通の dynamic grid draw path
- 並列 smoke test / build test
