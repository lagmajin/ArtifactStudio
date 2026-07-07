# M-TWC-1 Time Warp Curve on Clips Milestone

作成日: 2026-07-07
ステータス: Draft
対象: `ArtifactCore/include/Time/TimeRemap.ixx`,
      `ArtifactCore/src/Time/TimeRemap.cppm`,
      `ArtifactCore/include/Animation/AnimatableValue.ixx`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`,
      `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`,
      `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`
位置づけ: Maya の Time Warp Curve / MotionBuilder の Retime を Artifact タイムラインに移植。
          クリップ単位で非破壊の時間歪曲カーブを適用し、Speed Ramp / 逆再生 / Freeze を実現。
参照:
- `ArtifactCore/include/Time/TimeRemap.ixx`（実装済み: `sourceTimeAt(outputTime)`）
- `ArtifactCore/src/Time/TimeRemap.cppm`（実装済み）
- `docs/planned/MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md`（Time Warp Curve ❌, Speed Point ❌）
- `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`（speed/value graph 表示）

---

## 1. 目的

`MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` の Maya アニメ特化カテゴリ:

> **Time Warp Curve** — Maya。クリップに適用する時間歪曲カーブ。コマ撮り/スロー/逆再生を非破壊で。
> **Speed Point 編集** — Resolve。クリップ上に速度変化点を追加し、区間ごとに変速。

現在の ArtifactStudio では `TimeRemap` がレイヤー単位のリマップを実装しているが、
タイムライン上のクリップに **視覚的なカーブ編集で** 非破壊の変速を適用する UI がない。

本 milestone は、`TimeRemap` の既存評価パイプラインを拡張し、
クリップ上に Time Warp Curve を重ねて編集可能にする:

- 水平線 = 等速再生（元の速度）
- 傾斜 = 加速/減速
- 頂点追加 = Speed Ramp ポイント
- 負の傾斜 = 逆再生
- 水平区間 = Freeze Frame

> 重要: AE の Time Remap はレイヤー全体に 0〜100% のキーフレームを打つ方式だが、
> 本 milestone は **クリップに重ねるカーブとして** 視覚化する Maya/MotionBuilder 方式を採用。
> AE 互換の Time Remap は既存のままで、本 curve 方式は追加の選択肢。

---

## 2. 現状整理 (2026-07-07 基準)

### 2.1 既存資産

| 資産 | ファイル | 内容 |
|---|---|---|
| `TimeRemap` | `ArtifactCore/src/Time/TimeRemap.cppm` | `sourceTimeAt(outputTime)` 評価。実装済み |
| `AnimatableValue<float>` | `ArtifactCore/include/Animation/AnimatableValue.ixx` | Time Warp Curve のキーフレームとして流用可能 |
| `ArtifactCurveEditorWidget` | `ArtifactCurveEditorWidget.cppm` | speed/value graph 表示。`sampleSpeedGraph()` 実装済み |
| `ArtifactTimelineTrackPainterView` | `ArtifactTimelineTrackPainterView.cppm` | owner-draw クリップ描画。クリップ上への追加描画可能 |

### 2.2 不足

| 軸 | 状況 |
|---|---|
| Time Warp Curve データモデル（クリップ単位） | なし |
| クリップ上へのカーブ重畳描画 | なし |
| Speed Point の追加/削除/ドラッグ UI | なし |
| 逆再生 / Freeze 設定 UI | なし |
| Beat Sync（BPM 指定の自動変速） | なし |
| カーブ評価を TimeRemap に統合 | 未接続 |

---

## 3. Scope / Non-Goals

### Scope

- `TimeWarpCurve` データモデル（`AnimatableValue<float>` でカーブを保持）
- `ArtifactAbstractLayer` への per-clip TimeWarpCurve 保持
- クリップ上へのカーブ重畳描画（`ArtifactTimelineTrackPainterView`）
- Speed Point の追加（クリック）/ 削除 / ドラッグ編集
- 逆再生（カーブ負勾配）/ Freeze（水平区間）の設定
- カーブ評価を `TimeRemap::sourceTimeAt()` に統合

### Non-Goals

- Beat Sync（BPM 連携）→ 別 milestone
- タイムラインクリップの概念が存在しない場合の下位互換 → クリップ編集基盤に依存
- 複数クリップの一括 Time Warp 編集 → 将来拡張

---

## 4. Phases

### Phase 1: TimeWarpCurve データモデル (P0, 1 セッション)

- `TimeWarpCurve` クラスを `ArtifactCore` に新規追加
  - `AnimatableValue<float>` で source-time→output-time マッピングを保持
  - `evaluate(outputTime) -> sourceTime` の評価
  - デフォルトは Identity（output == source）
- `ArtifactAbstractLayer` に `TimeWarpCurve` 保持メンバ追加

**Done criteria:**
- Identity 設定時は TimeRemap なしと等価
- カーブにキーフレーム追加後、evaluate 結果が正しい source time を返す
- 負勾配で source time が逆方向に進む

### Phase 2: クリップ上カーブ描画 (P0, 2 セッション)

- `ArtifactTimelineTrackPainterView` に Time Warp Curve overlay 描画追加
  - クリップの高さ領域に半透明のカーブライン
  - キーフレーム点 = Speed Point（小さなダイヤモンド形）
  - X 軸 = output time（クリップ上の時間）/ Y 軸 = source time（上=終了、下=開始）
- Speed Point のクリック追加 / ドラッグ移動 / Delete 削除

**Done criteria:**
- クリップ上に Time Warp Curve が重畳表示
- Speed Point をクリック追加→ドラッグ移動→タイムライン反映
- 水平カーブでクリップがフリーズ / 負勾配で逆再生

### Phase 3: TimeRemap 統合 + プリセット (P1, 1 セッション)

- `TimeRemap::sourceTimeAt()` に `TimeWarpCurve` 経由の評価パス追加
- プリセット: Ease In / Ease Out / Ease Both / Reverse / Freeze Frame
- 右クリックメニューからプリセットを一発適用

**Done criteria:**
- Time Warp Curve の評価結果がビューポート再生に反映
- プリセット選択→即反映→カーブの手動微調整

### Phase 4: 永続化 + Diagnostics (P2, 1 セッション)

- project JSON に `layer.timeWarpCurve` 追加
- 旧プロジェクト後方互換（カーブなし=Identity）
- Problem View 診断（カーブが単調性を破った場合の警告 等）

**Done criteria:**
- 保存→再読込で Time Warp Curve 完全復元 / 旧プロジェクト互換

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` | 🔵 Maya アニメ特化（Time Warp）。本 milestone で実装 |
| `MILESTONE_ANIMATION_LAYERS_2026-07-07.md` | Anim Layer と併用可能（TimeWarp は全 Layer 共通 or Layer 別） |
| `MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md` | Curve Editor を Speed Point 編集に流用 |

---

## 6. リスク

1. **クリップ単位の扱い**。現状のタイムラインがクリップベースかレイヤーベースかの確認。クリップ概念がない場合、まずクリップ基盤が必要
2. **カーブの単調性**。source time が後退する場合の逆再生。タイムラインの再生方向との整合
3. **サブモジュール境界**: `ArtifactCore/include/Time/TimeWarpCurve.ixx` 新規。`ArtifactWidgets` は触らない

---

## 7. Done Criteria (全体)

- Time Warp Curve がクリップ上に重畳表示 / Speed Point のドラッグ編集
- 逆再生 / Freeze / Speed Ramp がビューポート再生に反映
- プリセット一発適用 → 手動微調整のワークフローが成立
- 保存→再読込で全復元 / 旧プロジェクト互換
- 新規 signal-slot / QImage / setStyleSheet なし

---

## 8. 更新履歴

- 2026-07-07: 初版作成。Maya Time Warp Curve の ArtifactStudio 移植設計。
