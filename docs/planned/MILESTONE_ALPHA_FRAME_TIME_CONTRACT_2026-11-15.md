# Alpha Association & Frame-Time Contract R&D

**最終更新:** 2026-11-15
**ステータス:** 計画（コード変更・ビルドなし）

## 目的

画像エフェクト R&D で繰り返し未解決として残っている「alpha association（straight/premultiplied）」と「フレーム時間での振る舞い（flicker / shimmer / time-uniformity）」を、エフェクト横断の契約として定義・検証する。Temporal / Echo / Time Displacement 系の新機能を追加する前の前提にもなる。

## 未解決事項の出典（読解で確認）

| 出典 | 未解決事項 |
|---|---|
| `MILESTONE_DETAIL_SHARPENING_LAB_2026-11-14.md` Phase 2/3 | alpha 加重で黒背景合成誤差 26.06→10.68 code、クランプ併用で 0.00 code。premultiplied 契約未確認。クランプの時間一様性未解決（high-pass std 比 1.196 だが frame 差分比 1.583）。 |
| `MILESTONE_BLUR_EFFECTS_RD_2026-11-12.md` | hidden RGB invariant は式レベルで zero 差を確認。実行時の descriptor/value association は未確認。`GlowEffectCPUImpl` は入力 descriptor を読まず BGRA float RGB を前提に luma 計算。 |
| `MILESTONE_ATMOSPHERIC_GLOW_BLUR_RD_2026-11-12.md` | alpha edge fixture を straight RGB から premultiply/unassociate した対で入力可視 RGB 最大差 1.19e-7、候補可視 RGB 最大差 4.77e-7。fully transparent 画素へ hidden RGB (5,4,2) を置く stress で alpha は維持されたが隣接半透明物体の合成後 RGB RMSE 4.27e-6 / 最大差 6.89e-5。候補が alpha-aware でなく hidden RGB を source selection に使う感度。 |
| `MILESTONE_OUTLINE_LAB_2026-11-12.md` | straight/premultiplied を別条件として扱うと明記したが、実評価は未実施。時間安定性（flicker / 線の太さ・位置の揺れ）は Phase 2。 |
| `MILESTONE_TEMPORAL_EFFECTS_RD_2026-11-12.md` | Phase 1 sampler 契約テストをソースに追加したが target 登録・build・実行は未実施。 |
| `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` | 必須項目「色と alpha」「時間安定性」を、適用/対象外/未検証のいずれかで明記する規則。 |

## 契約として定義すべき項目

1. **入力 association の宣言**: 各エフェクトが straight / premultiplied のどちらを前提とするか、入力 descriptor を読むか読まないかを明示。
2. **transparent 画素の RGB 扱い**: hidden RGB を source selection / luma / threshold に使うか否か。Artifact の `toFrameBuffer(QImage)`（`ArtifactImageLayer.cpp` / `ArtifactVideoLayer.cpp`）が straight として読む契約との整合。
3. **出力 association**: エフェクト出力が straight / premultiplied のどちらで、alpha を変更するか否か。
4. **背景合成後の縁**: 黒/灰/白背景への合成後の境界帯差の許容。
5. **時間一様性**: 静止入力でフレームごとに結果が変わらないこと。motion compensation なしの frame 差分比の扱い。
6. **HDR headroom**: 0–1 丸めによるクリップの扱い、非有限値の伝播。

## 検証手順（R&D）

- 各エフェクトについて、straight/premultiplied の対応ペア入力で出力 association と背景合成後の縁を比較。
- fully transparent 画素に hidden RGB を置いた stress で、alpha 保持と隣接半透明物体への影響を測定。
- 静止入力の反復実行で frame 差分が 0 であること（または許容差を明記）。
- `MILESTONE_TEMPORAL_EFFECTS_RD` の Phase 1 sampler 契約テストを登録・実行（ビルド許可後）。

## 非対象

- 製品コードの変更、新エフェクト追加、UI、GPU render path の変更
- ビルド・テスト・CMake 実行（ユーザーの明示指示を待つ）

## 確認済みソース証拠（2026-11-15、読解のみ）

- `Artifact/src/Layer/ArtifactImageLayer.cpp` L125-140: `toFrameBuffer(const QImage&)` は `QImage::Format_ARGB32_Premultiplied` へ変換してから float 化（`1.0/255.0`）。Artifact の内部契約は **premultiplied RGBA float**。
- `Artifact/src/Layer/ArtifactVideoLayer.cpp` L176: 同名 `toFrameBuffer(const QImage&)` が存在（video 経路も同契約を前提）。
- `Artifact/src/Effects/Glow/GlowEffect.cpp`（R&D 文書より）: `GlowEffectCPUImpl::applyCPU` は入力 descriptor を読まず BGRA float RGB の luma (`.114B+.587G+.299R`) を threshold `.6` で計算 → Glow は premultiplied 前提で hidden RGB を source selection に使う感度が残る。
- `Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cpp` L100: RT format は `TEX_FORMAT_RGBA8_UNORM_SRGB` → offscreen capture は sRGB-encoded 8-bit 由来。linear/scene-linear での alpha 契約検証には別経路が必要。

## 関連

- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `docs/planned/MILESTONE_DETAIL_SHARPENING_LAB_2026-11-14.md`
- `docs/planned/MILESTONE_BLUR_EFFECTS_RD_2026-11-12.md`
- `docs/planned/MILESTONE_ATMOSPHERIC_GLOW_BLUR_RD_2026-11-12.md`
- `docs/planned/MILESTONE_OUTLINE_LAB_2026-11-12.md`
- `docs/planned/MILESTONE_TEMPORAL_EFFECTS_RD_2026-11-12.md`
- `Artifact/src/Layer/ArtifactImageLayer.cpp` (`toFrameBuffer`)
- `Artifact/src/Layer/ArtifactVideoLayer.cpp` (`toFrameBuffer`)
