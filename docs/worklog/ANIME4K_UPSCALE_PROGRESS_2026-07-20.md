# Anime4K Preview Upscale Progress

> 2026-07-20

## 初期実装

`Artifact/shaders/anime4k_edge_upscaleCS.hlsl` を追加した。

- compute shader の独立 pass
- 輝度勾配から水平 / 垂直エッジを推定
- bilinear interpolation と方向性補間を edge strength で blend
- alpha は bilinear 経路を維持
- 8x8 thread group

これは Anime4K のアルゴリズムをそのまま移植したものではなく、ArtifactStudio の preview path に接続するための Anime4K-inspired な初期 pass である。

## 未接続

- `ShaderManager` / render pass への登録
- preview surface の upscale toggle
- constant buffer の実バインド
- FSR との品質・速度比較
- 低解像度入力、HDR、premultiplied alpha の確認

ビルド・テストは未実行。
