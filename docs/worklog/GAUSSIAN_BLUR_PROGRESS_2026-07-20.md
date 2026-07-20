# Gaussian Blur Progress

> 2026-07-20

`glsl-fast-gaussian-blur` を参考に、separable Gaussian Blur の GPU compute shader 資産を追加した。

- `gaussianBlurHorizontalCS.hlsl`
- `gaussianBlurVerticalCS.hlsl`
- radius は 1〜16 に制限
- sigma と radius を constant buffer から指定
- clamp-to-edge サンプリング
- Qt の `QPainter` / `CompositionMode` は使用しない

未対応:

- ShaderManager / render pass 登録
- downsample と組み合わせた大半径高速化
- bloom / effect UI への接続
- 実 GPU での検証
