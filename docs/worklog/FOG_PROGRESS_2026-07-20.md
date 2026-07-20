# Fog Progress

> 2026-07-20

既存の `Render.AtmosphereFog` CPU renderer と volumetric cloud 用 `fogHF.hlsli` を確認した上で、一般的な composition/postprocess 用の GPU height fog pass を追加した。

- `heightFogCS.hlsl`
- color + normalized depth 入力
- density / height falloff / ground level
- start distance / near-far range
- fog color と alpha を保持

この pass は host-specific な world-position reconstruction を行わず、camera height を安定した共通入力として使う初期版である。既存 AtmosphereFog や cloud shader の置換はしない。

未対応:

- world-space position reconstruction
- height volume / local fog volume
- volumetric shadow / temporal reprojection
- render pass 登録と UI
- 実 GPU 検証
