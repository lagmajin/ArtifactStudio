# MILESTONE: EasingLab — Easing Comparison Tool

**日付**: 2026-04-21（統合+実装詳細化: 2026-08-04）
**統合元**: `MILESTONE_EASING_LAB_*` 7ファイル
**静的監査**: 2026-07-25 — 全 Phase 実装済み、runtime 検証のみ残

## 現状

全コードが実装済み。静的監査で確認済みの主要コンポーネント:

| コンポーネント | ファイル | 状態 |
|---------------|---------|------|
| `EasingCurveUtil` | `ArtifactCore/src/Animation/EasingCurveUtil.cppm` | ✅ 実装済み |
| `EasingCandidate` カタログ | `defaultEasingCandidates()` | ✅ 6 preset 実装済み |
| `EasingPreviewWidget` | `Artifact/src/Widgets/Timeline/EasingLabWidget.cppm` | ✅ 曲線+marker 描画 |
| `EasingLabDialog` | 同上 | ✅ grid 表示、scrub 同期 |
| Apply 導線 | `ArtifactTimelineWidget.cppm` + `ArtifactAnimationMenu.cppm` | ✅ `Ease+` button、Animation menu |
| Undo/Redo | `ApplyInterpolationCommand` | ✅ before/after state 保存 |

## 残タスク: Runtime 検証

実装は完了しているため、残るのは実行時検証のみ。以下の順で実施する。

### Step 1: ビルド確認

```
cmake --build build_j_vs18 --target Artifact
```

- [ ] `EasingCurveUtil` を含む Core モジュールがリンクされる
- [ ] `EasingLabWidget` が Timeline module の一部としてコンパイルされる
- [ ] ビルドエラー・ワーニングなし

### Step 2: Easing preset の数値境界検証

`EasingCurveUtil` の各 easing 関数を t=0.0, 0.5, 1.0 で評価し、期待値と比較する。

テスト対象関数（`ArtifactCore/src/Animation/EasingCurveUtil.cppm` の `evaluateEasing()` 等）:

| Preset | t=0.0 | t=0.5 期待値 | t=1.0 |
|--------|-------|-------------|-------|
| Linear | 0.0 | 0.5 | 1.0 |
| EaseIn | 0.0 | <0.5 (加速中) | 1.0 |
| EaseOut | 0.0 | >0.5 (減速中) | 1.0 |
| EaseInOut | 0.0 | 0.5 (変曲点) | 1.0 |
| Back | 0.0 | オーバーシュートあり | 1.0 |
| Expo | 0.0 | 指数関数的 | 1.0 |

確認方法:
1. Artifact を起動し、任意の keyframe を持つ composition を開く
2. Timeline で keyframe を選択 → `Ease+` button をクリック
3. EasingLab ダイアログが表示されることを確認
4. 全 6 preset のスクラブ動作を確認（t=0.0〜1.0 の進行が滑らかか）

### Step 3: Apply / Undo / Redo の実行確認

1. keyframe を選択 → EasingLab で `EaseIn` を選択 → 「Apply」をクリック
2. keyframe の interpolation が EaseIn に変わることを確認（視覚的または `debug.getProperty` で）
3. Undo（Ctrl+Z）→ interpolation が元に戻ることを確認
4. Redo（Ctrl+Y）→ EaseIn に戻ることを確認
5. 別の easing に Apply → Undo/Redo が一貫していることを確認

### Step 4: Selection 安定性の確認

1. EasingLab を開いた状態で、別の keyframe を選択
2. dialog が元の keyframe に対する選択を維持していることを確認
3. dialog を閉じ、再度別の keyframe で開く → その keyframe に対して正しく動作するか

### Step 5: エッジケース

- [ ] keyframe が未選択状態で `Ease+` button が無効化される（または適切に処理される）
- [ ] easing 適用後に composition を閉じてもクラッシュしない
- [ ] 複数の composition を開いている状態で、各 composition の keyframe に正しく適用される

## 判定基準

全 Step がパスしたら本マイルストーンを完了とする。問題があれば該当コードを修正し、再検証する。
