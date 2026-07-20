# Dual Kawase Blur Progress

> 2026-07-20

`Dual Kawase Demo` / `Unified-Universal-Blur` を参考に、GPU compute 用の downsample / upsample pass を追加した。

- `dualKawaseDownsampleCS.hlsl`
- `dualKawaseUpsampleCS.hlsl`
- 固定 4-tap の小ブラー
- mip chain / ping-pong texture を前提にした構成
- radius ではなく反復回数で大半径 blur を作る設計

未対応:

- mip chain の生成・管理
- ping-pong render target の接続
- Bloom 合成と UI
- 実 GPU 検証
