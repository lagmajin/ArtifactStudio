# RGB / HSV Keyer Progress

> 2026-07-20

OpenToonz の RGB/HSV Key の考え方を参考に、GPU key mask pass を追加した。

- `rgbHsvKeyerCS.hlsl`
- key color の HSV 変換
- hue circular distance
- saturation / value minimum
- softness
- spill suppression の初期処理
- source RGB を維持し、key matte を alpha に出力

未対応:

- UI / property schema
- RGB distance mode と HSV mode の切替
- edge erosion / choke / feather
- dedicated matte output
- existing track matte / mask pipeline への接続
- 実 GPU 検証
