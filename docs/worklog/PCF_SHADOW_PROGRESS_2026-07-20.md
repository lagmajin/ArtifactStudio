# PCF Shadow Progress

> 2026-07-20

TheRealMJP/Shadows を参考に、独立した 3x3 PCF shadow resolve pass を追加した。

- `pcfShadowResolveCS.hlsl`
- shadow map と receiver data を分離入力
- depth bias / normal bias
- 9 tap の percentage-closer filtering
- visibility scalar を出力

未対応:

- CSM の cascade selection
- stable cascade fitting
- VSM / EVSM / MSM
- shadow atlas
- 実 render path への登録
- 実 GPU 検証
