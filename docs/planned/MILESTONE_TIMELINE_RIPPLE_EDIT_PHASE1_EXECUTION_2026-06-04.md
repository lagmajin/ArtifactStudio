# Timeline Ripple Edit - Phase 1 Execution

> 2026-06-04 作成

## 目的

`M-TL-15 Timeline Ripple Edit / Downstream Shift` の最初の実行スライスを固定する。

この Phase 1 は、timeline の後続全移動を AE / NLE っぽい編集動作として安定させるための最小導入であり、まずは `Trim Out` を起点に後続レイヤーを押し出せる状態を作る。

---

## Phase 1 の範囲

### In

- `Trim Out` で生じた差分を後続レイヤーへ伝播する
- 後続レイヤーの `inPoint / outPoint / startTime` をまとめてずらす
- 後続レイヤー内の animatable keyframe を同じ delta で移動する
- Undo/Redo を 1 操作として維持する
- locked layer は ripple 対象から除外する

### Out

- `Trim In` / `Delete` の ripple 対応
- 複数選択への一括 ripple
- overlap / collision の自動解決
- parent/child 階層をまたぐ特殊ルール

---

## Current Boundary Note

- まずは `Trim Out` を起点にした後続全移動だけを完成させる
- `Delete` ripple は同じ仕組みで拡張できるが、Phase 1 では実装範囲に入れない
- 後続レイヤーの選択順に依存せず、`current layer` を優先して 1 つだけ動かす
- ripple が起きた後も keyframe selection / lane state / timeline refresh が破綻しないことを優先する

---

## First Files

1. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
2. `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
3. `Artifact/src/Undo/UndoManager.cppm`
4. `Artifact/src/Service/ArtifactProjectService.cpp`

---

## First Move

1. `ArtifactTimelineWidget.cpp` に ripple 用の共通 snapshot / restore をまとめる
2. `ArtifactTimelineTrackPainterView.cpp` に `Trim Out` 起点の実行入口を固定する
3. Undo コマンドの redo/undo が target + followers を同じ状態へ戻すことを確認する
4. 後続レイヤーの keyframe shift が layer range と一致するか確認する

---

## Tasks

### 1. Downstream Layer Shift

- target layer の `outPoint` 変化量を delta として扱う
- boundary より後ろの layer をまとめて移動する
- locked layer はスキップする

### 2. Keyframe Carry

- follower layer の animatable property を同じ delta でシフトする
- keyframe の相対関係を崩さない
- 0 未満に落ちる場合は 0 に clamp する

### 3. Undo Snapshot

- target と follower の state を 1 回の snapshot にまとめる
- redo で ripple を再現し、undo で元へ戻す

### 4. Timeline Feedback

- ripple が起きたことを debug message で読めるようにする
- selection / refresh の再同期が必要なら local path に寄せる

---

## Recommended Order

1. target layer の trim-out delta を確定する
2. follower layer の shift 対象を決める
3. Undo snapshot を target + followers で固定する
4. keyframe shift の一致を確認する
5. `Delete` ripple へ広げる前の boundary を固める

---

## Done Criteria

- `Trim Out` のあと、後続レイヤーが意図通り詰まる
- ripple の undo / redo が安定する
- keyframe と layer range のズレが見えない
- 次の `Delete` / `Trim In` ripple にそのまま拡張できる

