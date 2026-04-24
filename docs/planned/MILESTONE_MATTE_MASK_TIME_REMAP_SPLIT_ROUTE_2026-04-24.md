# Matte / Mask / Time Remap Split Route

`LayerMatte`、`MaskCutout`、`TimeRemap` を 1 本の太い作業にまとめず、責務ごとに分けて進めるための整理メモ。

このドキュメントは、今回の精査で「一気に混ぜると危ない」と判断した領域を、今の `main` に合わせて切り分けるためのものです。

## Why Split

- `Matte` は `Layer2D` の依存モデルと serialization が主戦場
- `MaskCutout` は render path と compute shader が主戦場
- `TimeRemap` は timeline / playback / layer effect の時間変換が主戦場
- 3 つを同じ変更群にすると、Core / render / UI の責務が混ざりやすい

## Current State

- `ArtifactCore/include/Layer/LayerMatte.ixx`
  - matte stack と serialization の基礎は既にある
  - ただし render 側の適用順と diagnostics はまだ磨き込み余地がある
- `ArtifactCore/include/Time/TimeRemap.ixx`
  - time remap skeleton は既にある
  - こちらは `AE Feature Enhancement Roadmap` 側で段階的に育てるのが自然
- `docs/planned/MILESTONE_GPU_MASK_COMPUTE_PIPELINE_2026-04-03.md`
  - mask image を GPU に寄せる別ルートとして独立している
  - `LayerMatte` と同じ変更群にしない方が安全
- `docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`
  - mask / roto の入力 UI は editor 側の別責務

## Recommended Order

1. `Matte` の data model と diagnostics を固める
2. `GPU Mask Cutout` を preview / playback へ段階導入する
3. `TimeRemap` は roadmap 依存で UI / evaluation を育てる

## Guardrails

- `LayerMatte` の意味論を `MaskCutout` の compute 実装に流用しない
- `TimeRemap` の改善を matte / mask の変更に同梱しない
- render backend の低レベル変更は各 milestone の scope 内に閉じる
- 1 回の push で 3 系統を同時に動かさない

## Related

- [`ArtifactCore/docs/MILESTONE_TRACK_MATTE_CORE_2026-03-26.md`](../../ArtifactCore/docs/MILESTONE_TRACK_MATTE_CORE_2026-03-26.md)
- [`docs/planned/MILESTONE_GPU_MASK_COMPUTE_PIPELINE_2026-04-03.md`](./MILESTONE_GPU_MASK_COMPUTE_PIPELINE_2026-04-03.md)
- [`docs/planned/MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md`](./MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md)
- [`docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](./MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)

## Notes

- もし次に実装へ進むなら、最初の一手は `Matte` の ownership / cycle / serialization を安定させるところから入るのが安全。
- `TimeRemap` は既存の timeline / playback の現在の作法に寄せて育てる。
- `MaskCutout` は compute 版を別ルートで検証し、CPU fallback を残す。
