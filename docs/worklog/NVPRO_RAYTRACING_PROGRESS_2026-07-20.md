# nvpro Ray Tracing Progress

> 2026-07-20

nvpro-samples の最小 ray tracing tutorial 構成を参考に、既存 Diligent backend から独立した shader 契約を追加した。

- `rayTracingMinimalPayload.hlsli`
- `rayTracingMiss.hlsl`
- `rayTracingClosestHit.hlsl`
- miss payload と closest-hit payload の最小構造
- barycentric hit attribute と hit distance を保持

未対応:

- BLAS / TLAS 作成
- shader binding table
- Diligent ray tracing pipeline 登録
- camera ray generation
- 実 GPU 検証

低レベル backend の挙動を推測で変更しないため、今回は shader 契約までに限定した。
