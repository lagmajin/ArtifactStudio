# MILESTONE: After Effects Parity & Gap Analysis - 2026-05-28

作成日: 2026-05-28  
対象: ArtifactStudio 現行リポジトリ  
位置づけ: 2026-04-08 版の再整理・現行化

---

## 目的

この文書は、ArtifactStudio を After Effects 風の制作ツールとして見たときの
**現在の実装状況**と、**まだ埋まっていない制作ギャップ**を
現行リポジトリ基準で整理し直したもの。

古い gap analysis は歴史的な参照としては有用だが、今のコードベースでは
「未実装一覧」よりも「既にある土台をどうつなぐか」の方が重要になっている。

---

## 参照元

- [`docs/planned/MILESTONE_AE_PARITY_EXECUTION_2026-04-29.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE_PARITY_EXECUTION_2026-04-29.md)
- [`Artifact/docs/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md`](X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md)
- [`docs/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`](X:/Dev/ArtifactStudio/docs/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md)
- [`docs/planned/MILESTONE_APP_SURFACE_COHESION_PHASE4_EXECUTION_2026-05-17.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_SURFACE_COHESION_PHASE4_EXECUTION_2026-05-17.md)
- [`docs/planned/MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md)
- [`docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md)
- [`docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md`](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md)
- [`docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md`](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md)
- [`docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md`](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md)
- [`docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md`](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md)
- [`docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md`](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md)

---

## Executive Summary

### いまの結論

1. **AE parity は「機能が無い状態」ではない**
   - Text animator, track matte, precompose, RAM preview, expression parser/evaluator など、
     重要な土台は既にある。

2. **現在のボトルネックは cohesion**
   - 各機能の存在よりも、`requested / ready / failed` のような状態契約、
     timeline / inspector / diagnostics の文法一致、
     keyframe 編集の整合性が弱い。

3. **優先順位は古い分析より少し前倒しされている**
   - preview / cache / playback の安定化
   - text animator の timeline 統合
   - track matte / mask / blend の正確性
   - keyframe interpolation / graph editor
   - parent / precompose / motion blur の操作感

---

## Current Snapshot

| 領域 | 現在の状態 | まだ残るギャップ | 優先度 |
|---|---|---|---|
| Preview / Cache / Playback | `ArtifactPlaybackService` が RAM preview 状態を持ち、timeline 側にも cache 表示がある。`ArtifactTimelineWidget` と `ArtifactCompositionRenderController` にも関連導線がある。 | authoritative な preview build queue、final image readiness の厳密化、playback fallback policy の明文化。 | P0 |
| Text Animator | `ArtifactCore/src/Text/TextAnimator.cppm` の `TextAnimatorEngine` が存在し、`ArtifactTextLayer` 側への統合も進んでいる。 | timeline トラック UI、専用 preset / reuse flow、effect との編集文法統一。 | P0 / P1 |
| Keyframe / Graph Editor | timeline 右パネルで selection / multi-drag / summary の改善が進んでいる。 | speed graph / value graph の完全な切替、bezier handle、roving / hold / tangent の深い編集。 | P1 |
| Track Matte / Mask / Blend | render controller に track matte 評価経路があり、mask / blend の基盤もある。 | alpha / premultiplied alpha / order の例外処理、stack correctness、UI 上の説明不足。 | P0 / P1 |
| Parent / Precompose / Adjustment | precompose 呼び出し経路と adjustment layer の土台がある。 | nested workflow、transform propagation、編集導線の整理。 | P1 / P2 |
| Motion Blur / Time Remap | motion blur / time 系の基礎はある。 | サンプル政策、preview と render の一貫性、UI の見せ方。 | P1 / P2 |
| Expression | parser / evaluator / property expression の基礎がある。 | AE 風 stdlib、debug / inspection、host binding の境界整理。 | P2 |
| Color Management | 色管理と LUT 系の下地はある。 | preview/export の見え方一致、ACES/HDR の明示、実務向け UI の整備。 | P2 |
| Plugins / Interop | project / import / export の周辺は広い。 | OFX / plugin SDK、AEP/Nuke/Fusion 系 interchange はまだ大きい。 | P3 |

---

## いま本当に足りないもの

### 1. Preview / Cache / Playback Stability

現行の最重要課題。

- `requested` と `ready` を同列に扱わないこと
- final image が存在しないのに cache hit と見なさないこと
- RAM preview の都合を widget 側の見せ方だけで誤魔化さないこと
- playback / scrub / diagnostics が同じ真実を読むこと

この領域は、見た目の AE らしさよりも先に、制作ソフトとしての信用を決める。

### 2. Text Animator UX Coherence

Core のエンジンはあるが、AE っぽい編集体験はまだ途中。

- animator の追加・削除・有効無効
- range selector / wiggly selector の操作感
- property path の表記統一
- timeline 上でのキーフレーム編集
- preset / reuse workflow

### 3. Track Matte / Mask / Compositing Correctness

合成の見た目を決める領域で、正確性の要求が高い。

- matte order
- alpha / premultiplied alpha
- mask と blend の組み合わせ
- 破綻時の reason 表示

### 4. Keyframe Interpolation / Graph Editor

線形補間を超えたところが、まだ AE らしさの本丸。

- easy ease
- value graph / speed graph
- bezier handle
- roving / hold

### 5. Parent / Precompose / Motion Blur

大きめの制作物に入るための編集基盤。

- parent/child の伝搬
- precompose の nested workflow
- motion blur の UI と quality policy

---

## 既にある強み

- `ArtifactCore` と `Artifact` の分離がはっきりしている
- text / expression / track matte / RAM preview の基礎が存在する
- timeline 周りの summary / selection / cache 表示が育っている
- surface cohesion を詰めるための実装・文書が最近かなり増えている

要するに、今の状態は「土台がない」ではなく、
**土台はあるので、順序と契約を揃える段階**。

---

## 次に進める順番

1. Preview / Cache / Playback Stability
2. Text Animator UX Coherence
3. Track Matte / Mask / Compositing Correctness
4. Keyframe Interpolation / Graph Editor
5. Parent / Precompose / Motion Blur
6. Expression / Color / Interop

---

## 注意

- この文書は 2026-04-08 版の代替ではなく、現行 repo に合わせた再評価版。
- 古い gap analysis は履歴として残すが、優先順位の判断はこの版を基準にする。
- 外部AI の要約メモは [AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md](X:/Dev/ArtifactStudio/docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md) を比較参照として使う。
- 比較の入口は [AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md) にもある。
- ざっと見るなら [AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md) を使う。
- 再開点を固定したいなら [AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md) を使う。
- 未解決の論点を洗うなら [AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md) を使う。
- まず全体像を掴むなら [AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md) を使う。
- ビルドやテストは実施していない。
