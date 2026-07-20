# FidelityFX SPD Progress

> 2026-07-20

FidelityFX SPD の導入準備として、明示的な source mip から destination mip へ 2x2 reduction を行う compute pass を追加した。

- `spdSinglePassDownsampleCS.hlsl`
- 1 dispatch / 1 bound destination level
- RGBA の平均 reduction
- source mip を constant buffer で指定
- clamp-to-edge

これは FidelityFX SPD の 12 mip 完全実装ではない。まず ArtifactStudio の texture / UAV mip binding 契約へ安全に接続するための building block として扱う。

未対応:

- 1 dispatch での 12 mip 同時生成
- wave / LDS 最適化
- bloom pyramid への接続
- 実 GPU 検証
