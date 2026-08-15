# Matte / Mask / Time Remap Split Route

**最終更新:** 2026-08-15
**Status:** Matte／TimeRemap 基盤と GPU mask／track matte API は実装済み、composition cross-route parity は未検証

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

## Static audit follow-up (2026-07-29)

`Layer.Matte`、`Time.TimeRemap`、Roto/Mask の評価経路と GPU Mask milestone を照合した。Matte evaluator/stack、diagnostic validation、TimeRemap processor、RotoMask keyframe/rasterization は実装基盤として存在する。GPU mask は初期化・pipeline API までで、composition の標準経路統合は未完了。ビルド・runtime検証は未実施。

### 判定

責務分離の方針は維持し、Matte/TimeRemap の基盤実装済み、GPU Mask の composition integration pending として扱う。3系統を一括変更する状態ではない。

---

## Static audit follow-up (2026-07-25)

`LayerMatte`／matte reference、mask compute pipeline、`TimeRemap` の model／layer／render接続を現行ソースで照合した。ビルド・GPU実機・複雑な評価順序は未実施。

| 分野 | 現状 | 判定 |
|---|---|---|
| Matte | `LayerMatte` の stack/evaluator、layer JSON保存・復元、参照削除時の dangling cleanup、project health rule、CPU render適用を確認した。適用順・diagnosticsの網羅は未確認。 | 実装済み基盤／確認待ち |
| MaskCutout | GPU compute用の `MaskCutoutPipeline`／`MaskPathRasterizerPipeline` と mask関連型が存在する。preview/playbackへの全経路とCPU fallback parityは未確認。 | 部分実装／統合確認待ち |
| TimeRemap | keyframe processor、easing／frame blend、layer enable／key設定／source frame評価、video decode接続、保存経路を確認した。Timeline UIと全layer種別の統合は未確認。 | 実装済み基盤／確認待ち |
| 責務分離 | Matte／Mask／TimeRemap は別module・別評価経路に分離されている。 | 方針整合 |

### 現在の判定

3系統の基盤と責務分離は現行コードに存在するが、GPU mask parity、matte適用順、TimeRemapのUI／全layer統合は未検証。全体は「基盤実装済み／統合・runtime確認待ち」とする。

## 現行コード監査 (2026-08-15)

- Matte は `LayerMatte`／参照検証／循環検出／削除時 cleanup／RenderQueue の preflight と、Composition 側の GPU track matte 適用経路まで進展している。GPU 側は最大 3 source の制約と失敗時の診断・未適用分岐も持つ。
- Mask は `MaskCutoutPipeline` と `MaskPathRasterizerPipeline` の texture／compute 経路があり、別マイルストーンで扱う責務分離は維持できている。通常 composition の全 preview／playback／export 経路での採用と CPU rasterizer との parity は未検証。
- TimeRemap は processor の keyframe／easing／reverse／hold／frame blend、layer 保存・復元、VideoLayer の source frame 評価、Curve Editor／Animation Menu、footage impact 解析まで接続されている。全 layer 種別、audio sync、scrub、export の一致は未検証。
- したがって 3 系統を一括統合する段階ではなく、Matte の適用順と diagnostics、Mask の GPU/CPU parity、TimeRemap の timeline／playback／export parity を別々に受け入れる状態である。

判定: **責務分離と各基盤は実装済み。Matte／Mask／TimeRemap をまたぐ composition runtime の順序・fallback・フレーム整合性は pending。**
