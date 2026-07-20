# NPR / FILM Progress

> 2026-07-20

## NPR Toon

追加: `toonLightingPS.hlsl`

- normal と base color を入力
- quantized diffuse bands
- shadow color
- rim light
- existing raster / postprocess path から独立

## FILM-inspired frame interpolation

追加: `filmFrameInterpolationCS.hlsl`

- previous / next frame
- previous / next motion vector
- blend factor
- motion-aware sampling と confidence blend

これは Google FILM のニューラルネットワーク本体ではなく、既存の motion-vector / frame-blend 受け皿へ接続する初期 shader contract である。

未対応:

- optical-flow 推定
- FILM model inference
- occlusion / disocclusion mask
- renderer / playback への登録
- 実 GPU 検証
