# マイルストーン: Performance Bottleneck Triage

> 2026-06-17 作成

## 目的

`CompositionEditor` と周辺 render / UI path にある perf ボトルネックを、まず「止血対象」と「次スプリント対象」に分けて、改善の順番を固定する。

この milestone は最適化そのものよりも、

- どの hot path を先に潰すか
- どれが設計負債で、どれが単純な再描画 churn か
- どこまでを `QImage` / readback / alloc の境界に閉じ込めるか

を明確にすることを目的とする。

---

## 背景

直近の調査では、perf 問題は単発ではなく複数系統が重なっている。

特に次の 3 系統は優先度が高い:

- `paintEvent()` 中の `update()` 再帰
- `QImage` hot path 流入
- `GpuContext` の毎フレーム確保

これらは UI 応答と render cost の両方に効きやすく、他の改善の前に止血したほうがよい。

---

## Scope

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm`
- `Artifact/src/Widgets/Control/ArtifactPlaybackControlTestWidget.cppm`
- `Artifact/src/Render/*`
- `Artifact/src/Layer/*`
- `Artifact/src/Effect/*`

---

## Non-Goals

- いきなり GPU パイプライン全体を再設計しない
- `QImage` を一括廃止しない
- `paintEvent()` を無闇に増やさない
- perf の観測面を壊さない

---

## Priority List

### S: 即修正

1. `update()` in `paintEvent()`
   - 対象例: `ArtifactProjectManagerWidget`, `ArtifactPlaybackControlWidget`, `ArtifactPlaybackControlTestWidget`
   - 目的: 再描画要求の再帰と無限 invalidation を止める
   - 完了条件: `paintEvent()` から再描画要求を出さない

2. `QImage` hot path 流入
   - 目的: `ImageF32x4_RGBA` 系の内部表現を優先し、CPU round-trip を減らす
   - 完了条件: hot path の `QImage` 変換を boundary に追い出す

3. `GpuContext` の毎 frame 確保
   - 対象例: Blur / Brightness / ChannelMixer / Colorama / ColorBalance / Curves / Exposure / Fill / Hue / Levels / PhotoFilter / Selective / Tritone / EdgeBloom / Glow / ReactiveGlow / Mosaic / Sharpen
   - 完了条件: frame loop での `make_unique` をやめ、再利用または遅延初期化へ寄せる

### A: 次回 sprint

1. `QFile::exists()` in render path
2. GPU→CPU readback
3. JSON parse in render path
4. Manual `delete`
5. `QSettings` in render path

### B: 継続改善

1. `QTimer` 重複
2. `QWaitCondition` の使い方
3. 不要 copy
4. String allocation

---

## Plan

### Phase 1: Triage

- `paintEvent()` 中の `update()` を止める
- `QImage` hot path の実体を列挙する
- `GpuContext` の alloc point を局所化する

### Phase 2: Stop the Bleed

- `paintEvent` 再帰を解消する
- `QImage` 境界を明示する
- frame-local alloc を減らす

### Phase 3: Next Sprint Candidates

- `QFile::exists()` / JSON / `QSettings` の混入点を洗う
- readback と cache invalidation を分ける
- 残った copy / timer churn をまとめる

---

## Success Criteria

- `paintEvent()` 由来の再描画ループがなくなる
- `QImage` hot path の入口が特定される
- `GpuContext` の毎 frame 確保が消える
- 次スプリントに回す項目が明確になる

## 2026-07-25 実装監査

PerformanceProfiler／PerformanceMonitor／DiagnosticEngine、layer／GPU／frame cache、audio の RT 向け計測・ring buffer など観測とキャッシュの基盤は確認した。ただし `paintEvent()` 内の再描画、QImage／readback 境界、毎フレーム GPU context、調整レイヤー readback、cache-key 生成などの具体的な止血完了は確認できず、実機プロファイルによる優先順位確定も未実施である。したがって triage の観測基盤は部分実装、S/A 項目の解消と success criteria は未達・未検証とする。
