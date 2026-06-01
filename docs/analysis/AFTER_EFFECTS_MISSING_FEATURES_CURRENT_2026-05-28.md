# After Effects 観点の現行不足機能メモ

作成日: 2026-05-28
対象: ArtifactStudio 現行リポジトリ
目的: 「AE 風ツールとして見たとき、今なにが本当に不足しているか」を現行コード基準で短く整理する

---

## 前提

この文書は「未実装一覧」を大量に並べるものではない。

現行 repo にはすでに次の土台がある。

- Text animator の core 実装
- expression parser / evaluator
- track matte / mask の描画経路
- marker 基礎
- parent layer 基礎
- adjustment layer 基礎
- RAM preview / cache の導線

したがって、After Effects parity の観点で本当に不足しているのは、
「機能の存在」よりも「制作ワークフローとしての完成度」が低い領域である。

関連:

- [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md:1)
- [MILESTONE_AE_PARITY_EXECUTION_2026-04-29.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE_PARITY_EXECUTION_2026-04-29.md:1)
- 共有メモ: [AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md:1)
- チェック表: [AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md:1)
- 再開点: [AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md:1)
- 未解決: [AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md:1)

---

## 結論

現行 repo を AE として見たときの主要不足は次の 5 本。

1. Graph Editor / 補間編集
2. Precompose の実務完成度
3. Text Animator の timeline 統合
4. Track Matte / Mask / Blend の正確性
5. Proxy workflow

この 5 つは「あるように見えるが制作ではまだ弱い」か、「導線はあるが中身が未完成」のどちらかに入る。

---

## 1. Graph Editor / 補間編集

### なぜ不足扱いか

AE らしさの中心にある `easy ease`、`value graph`、`speed graph`、
`bezier handle`、`roving`、`hold` がまだ実務レベルで揃っていない。

### いまあるもの

- Timeline 右パネルの keyframe 可視化と marker selection の土台
- current frame / selected keyframe / lane visibility の改善

コード上の足場:

- [ArtifactTimelineTrackPainterView.cpp](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp:670)

### まだ足りないもの

- curve editor surface の本実装
- speed graph / value graph の切替
- easy ease / bezier handle
- roving / hold / tangent 編集

根拠:

- [MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md:5)
- [MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md:50)
- [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md:58)

---

## 2. Precompose の実務完成度

### なぜ不足扱いか

UI と呼び出し経路はあるが、中核の composition 操作がまだ薄い。
ユーザーから見ると「プリコンポーズできそう」に見える一方、
内部は未完成箇所を残している。

### いまあるもの

- Layer menu からの precompose 導線
- dialog
- manager / command の骨組み

コード:

- [ArtifactLayerMenu.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm:314)
- [PrecomposeDialog.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Dialog/PrecomposeDialog.cppm:280)

### まだ足りないもの

- `unprecompose()` 実装完成
- 実レイヤー移動
- nested workflow の信頼性
- time remap / transform 変換の整合

コード上の未完了箇所:

- [PreCompose.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Composition/PreCompose.cppm:184)
- [PreCompose.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Composition/PreCompose.cppm:266)
- [PreCompose.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Composition/PreCompose.cppm:286)
- [PreCompose.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Composition/PreCompose.cppm:294)

補助分析:

- [COMPOSITION_PRECOMPOSE_ANALYSIS_2026-04-17.md](X:/Dev/ArtifactStudio/docs/planned/COMPOSITION_PRECOMPOSE_ANALYSIS_2026-04-17.md:672)

---

## 3. Text Animator の timeline 統合

### なぜ不足扱いか

Text animator engine 自体はかなり育っているが、
AE 的な「timeline 上で追える」「preset を再利用できる」
という編集体験はまだ途中。

### いまあるもの

- `TextAnimatorEngine`
- `ArtifactTextLayer` への統合
- wiggly / range selector データ
- property editor 側の一部 preset 導線

コード:

- [TextAnimator.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Text/TextAnimator.cppm:15)
- [ArtifactTextLayer.cppm](X:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactTextLayer.cppm:2253)
- [ArtifactPropertyEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm:1520)

### まだ足りないもの

- animator property track の timeline 表示
- range / wiggly selector の操作感整理
- preset browser / reuse flow
- effect parameter と同じ編集文法

根拠:

- [MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md](X:/Dev/ArtifactStudio/docs/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md:36)
- [MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md:19)
- [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md:57)

---

## 4. Track Matte / Mask / Blend の正確性

### なぜ不足扱いか

未実装ではないが、制作ソフトとして最も信用を落としやすい領域。
AE parity では「できる」より「破綻しない」が重要。

### いまあるもの

- matte 適用経路
- premultiplied alpha を意識した mask path
- blend / render contract の文書化

コード:

- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:1509)
- [LayerMask.cppm](X:/Dev/ArtifactStudio/Artifact/src/Mask/LayerMask.cppm:179)

### まだ足りないもの

- matte order の完全整理
- premultiplied / straight alpha の境界統一
- mask と blend の組み合わせ検証
- UI 上の reason 表示

関連:

- [MILESTONE_AE_PARITY_EXECUTION_2026-04-29.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE_PARITY_EXECUTION_2026-04-29.md:17)
- [BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md](X:/Dev/ArtifactStudio/docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md:1)
- [RENDER_FORMAT_CONTRACT_2026-05-16.md](X:/Dev/ArtifactStudio/docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md:1)

---

## 5. Proxy workflow

### なぜ不足扱いか

素材が重くなったときの逃げ道として、AE 系ツールでは proxy はかなり重要。
この repo では UI 側に導線があるぶん、未完成さが逆に目立つ。

### いまあるもの

- Project 側の generate proxy 導線
- layer 側の proxy path 保持

コード:

- [ArtifactProjectManagerWidget.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm:3595)
- [ArtifactVideoLayer.cppm](X:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactVideoLayer.cppm:1060)

### まだ足りないもの

- 実 proxy 生成の完成
- workflow の service 一本化
- preview / cache / proxy の使い分け整理

現状の明示的な不足:

- [ArtifactVideoLayer.cppm](X:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactVideoLayer.cppm:1072)

関連:

- [MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md:15)

---

## 不足ではないが、まだ弱い領域

次は「無い」とは言いにくいが、AE としてはまだ磨きたいもの。

- parent layer 基礎
  - [ArtifactAbstractLayer.cppm](X:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactAbstractLayer.cppm:1019)
- adjustment layer 基礎
  - [ArtifactTestAdjustmentLayer.cppm](X:/Dev/ArtifactStudio/Artifact/src/Test/ArtifactTestAdjustmentLayer.cppm:41)
- expression 基礎
  - [ExpressionEvaluator.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm:379)
- marker 基礎
  - [ArtifactInOutPoints.cppm](X:/Dev/ArtifactStudio/Artifact/src/Composition/ArtifactInOutPoints.cppm:317)

これらは「不存在」ではなく、「使い込みのための UI / 契約 / 一貫性」がまだ弱い。

---

## 優先順の提案

AE としての不足を埋める順番は、現時点では次が自然。

1. Graph Editor / 補間編集
2. Track Matte / Mask / Blend の正確性
3. Precompose の実務完成度
4. Text Animator の timeline 統合
5. Proxy workflow

理由:

- 1 と 2 は制作体験の「気持ちよさ」と「信用」を直接決める
- 3 は AE ワークフローの中核だが、土台は既にある
- 4 は価値が高いが、既存 engine を前提に UI 統合中心で進められる
- 5 は重素材の運用で重要だが、まず editor の中心導線を固めたい

---

## メモ

- この文書は「現行 repo を読んだうえでの不足メモ」であり、ビルド・実機検証は行っていない
- 既存の parity 文書を否定するものではなく、実装済み土台と未完成ワークフローを切り分けるための補助メモ
- 共有版の入口は [docs/shared/ai-tech-memos/INDEX.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/INDEX.md:1)
- まず総括だけ読むなら [AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md:1)
- ざっと確認するなら [AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md:1)
- 迷ったら [AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md:1)
- 答えが揃っていない論点は [AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md:1)

---

## 補足: 外部AI 調査メモ

別のAIに AE ライク機能の不足を整理してもらった要約を、比較用のメモとして残す。
この一覧は「どこから手を付けると制作機としての信用が上がるか」という感覚をつかむためのもので、現行コードの厳密な再検証までは含まない。

### P0 - 即座に対処が必要

- RAM プレビュー / キャッシュの安定性
  - `requested` と `ready` の状態契約が未整備
- トラックマット / アルファ合成の正確性
  - マット連鎖の評価順にバグの疑い
- ブレンドモード
  - 18/38 程度しか埋まっておらず、Dissolve / Linear Burn / Hard Mix / Stencil 系が不足

### P1 - コア生産性

- キーフレーム補間 / グラフエディタ
  - Linear が中心で、Bezier / Hold / Roving / Speed Graph が未完成
- テキストアニメーター UX
  - Range Selector / CJK / timeline 表示が未完成
- モーションブラー
  - 実装の骨はあるが UI 配線と一貫性が弱い
- アジャスタメントレイヤー
  - スタブ段階で、レンダーパス統合が未完成
- 親子階層 / トランスフォーム伝播
  - データ構造はあるが伝播チェーンがまだ不完全

### P2 - 標準的なプロ機能

- マーカーシステム
  - コンポ / レイヤーともに未実装扱い
- シェイプレイヤー演算子
  - Trim Path / Repeater / Boolean ops / パスアニメーションが不足
- プリコンポーズ完全ワークフロー
  - ネスト、タブ開き、属性保持が不完全
- レイヤースタイル
  - Drop Shadow / Glow / Bevel などが不足
- タイムリマップ / フレームブレンド
  - Optical Flow 相当がない
- エクスプレッションエンジン
  - パーサーはあるが `wiggle()` / `loopIn()` / `thisComp` / pick whip が不足

### P3 - 高度な機能

- エフェクト数
  - 現在の種数から大幅に増やす余地がある
- カラーマネジメント (OCIO / ACES)
  - 基盤はあるが production pipeline は未実装
- 3D カメラトラッカー
  - 未実装
- Plugin SDK / AEX 互換
  - OFX スタブ段階
- テンプレートシステム (.mogrt 相当)
  - 未実装
- Python API 完全カバレッジ
  - 部分的

### このメモから読めること

- P0 は「機能の有無」より「制作中に壊れないか」が主題
- P1 は「実際の編集体験を AE に近づける」領域
- P2 は「標準的なプロ機能を埋める」段階
- P3 は「互換・拡張・業務連携」の長期課題

### 既存メモとの関係

- 既存の [`MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md:1) と大枠は一致する
- この補足は、外部AI の調査結果を比較参照できるように残すためのもの
- 優先順位の細部は、現行コードの再確認で詰める前提にする
