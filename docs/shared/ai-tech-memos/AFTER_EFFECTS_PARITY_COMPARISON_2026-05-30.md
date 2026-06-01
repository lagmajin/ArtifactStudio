# After Effects Parity Comparison Notes

> 2026-05-30

## Summary

- 外部AI による AE ライク機能不足の要約を、既存の parity gap 文書と突き合わせるための共有メモ
- ここでは「どの項目が本当に P0 か」「何が機能不足で、何が cohesion 不足か」を区別する
- 現行の planning は [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md) を基準にしつつ、この共有メモで比較観点を残す

## What The External AI Said

- P0
  - RAM preview / cache stability
  - track matte / alpha compositing correctness
  - blend mode coverage
- P1
  - keyframe interpolation / graph editor
  - text animator UX
  - motion blur
  - adjustment layer
  - parent / transform propagation
- P2
  - markers
  - shape operators
  - full precompose workflow
  - layer styles
  - time remap / frame blend
  - expression engine completeness
- P3
  - more effects
  - OCIO / ACES
  - 3D camera tracker
  - plugin SDK / AEX compatibility
  - mogrt-like templates
  - Python API coverage

## Comparison To Current Repo Reading

- P0 の preview / cache / playback stability は、現行 parity gap でも最重要扱いで一致している
- track matte / mask / blend の正確性も、現行文書と同じく高優先度
- graph editor / interpolation は、現行では keyframe / graph editor として P1 扱い
- text animator は、機能の有無より timeline 統合と UX coherence が課題
- precompose / adjustment / motion blur は、土台はあるが workflow 完成度が足りない、という整理に近い
- 外部AI の P3 は、現行の backlog にもある長期テーマと整合している

## Risks

- 外部AI の「未実装率」は厳密なコード監査ではないので、数字は参考値として扱う
- `feature missing` と `workflow not cohesive` を混ぜると、優先順位を見誤りやすい
- `Fill` / `100%` のような viewport 問題は、AE parity より先に UI contract の問題として切り分ける必要がある

## Next Steps

- もし別AIに続き調査させるなら、このメモと [AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md](X:/Dev/ArtifactStudio/docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md) をセットで渡す
- 実装優先度は [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md) を基準にする
- 具体的な差分は `RAM preview / track matte / graph editor / text animator` の 4 本から再確認するのが効率的
- 全体像は [AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md) に戻る
