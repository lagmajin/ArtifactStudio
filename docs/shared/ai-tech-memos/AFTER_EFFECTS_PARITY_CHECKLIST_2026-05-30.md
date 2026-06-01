# After Effects Parity Checklist

> 2026-05-30

## P0 - Stability

- [ ] RAM preview / cache の状態契約を `requested / ready / failed` で統一する
- [ ] Track matte / alpha compositing の評価順を確認する
- [ ] Blend mode coverage の穴を洗い出す

## P1 - Productivity

- [ ] Keyframe interpolation / graph editor の完成度を確認する
- [ ] Text animator UX の timeline 統合を詰める
- [ ] Motion blur の UI 配線と一貫性を確認する
- [ ] Adjustment layer の render path 統合を確認する
- [ ] Parent / transform propagation の連鎖を確認する

## P2 - Standard Pro Features

- [ ] Marker system の有無と運用導線を確認する
- [ ] Shape operators の実装範囲を確認する
- [ ] Precompose workflow の実務完成度を確認する
- [ ] Layer styles の未実装項目を洗い出す
- [ ] Time remap / frame blend の現状を確認する
- [ ] Expression engine の AE 互換 gap を確認する

## P3 - Advanced / Ecosystem

- [ ] Effects 数の不足を機能カテゴリ別に分ける
- [ ] OCIO / ACES の production pipeline を確認する
- [ ] 3D camera tracker の要否を確認する
- [ ] Plugin SDK / AEX compatibility の方向性を確認する
- [ ] Mogrt-like templates の必要性を確認する
- [ ] Python API coverage の範囲を確認する

## Notes

- これは実装完了表ではなく、次の調査や実装で抜けを追いやすくするためのチェック表
- まず全体像は [After Effects Parity Master Summary](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md) を読む
- 詳細は [After Effects Parity Comparison Notes](AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md) を参照
- 現行の優先度判断は [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md) を基準にする
- 原本メモは [AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md](/x:/Dev/ArtifactStudio/docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md) を参照
