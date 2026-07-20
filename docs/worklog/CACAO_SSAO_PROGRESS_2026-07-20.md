# CACAO / SSAO Progress

> 2026-07-20

FidelityFX CACAO を参考に、depth＋normal 入力の軽量 screen-space AO pass を追加した。

- `cacaoLiteCS.hlsl`
- 8 サンプルの半径ベース occlusion
- depth bias / radius / power を constant buffer で指定
- normal の facing weight を適用
- AO scalar を出力し、合成は host 側へ残す

これは CACAO の完全移植ではなく、ArtifactStudio の render contract に合わせた初期 pass である。

未対応:

- view-space position reconstruction
- multi-scale sampling
- deinterleaved pass
- bilateral / edge-aware denoise
- AO 合成と preview UI
- 実 GPU 検証
