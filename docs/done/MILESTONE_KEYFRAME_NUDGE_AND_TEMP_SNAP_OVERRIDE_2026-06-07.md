# Keyframe Nudge / Temp Snap Override Milestone

**作成日:** 2026-06-07  
**ステータス:** ✅ 完了 (2026-06-15 確認・補完)  
**関連コンポーネント:** Timeline, Keyframe Editing, Snap System, Shortcut Handling

---

## 概要

選択したキーフレームを左右矢印で nudge できるようにし、必要に応じて一時的な snap 無効化もできるようにするためのマイルストーンです。

目的は、ドラッグで狙いにくい微調整をキーボードで正確に行えるようにすることです。

---

## 背景

現状はキーフレームを少し動かしたいだけでも、ドラッグと吸着の影響で操作しづらいことがあります。

- 1 フレームだけ右にずらしたい
- 10 フレーム単位でまとめて動かしたい
- いったん snap を無効にして正確に置きたい
- スクリプトや外部補助に頼らず標準操作にしたい

---

## 目標

- Left / Right Arrow で 1 frame nudge
- Shift+Arrow で 10 frame nudge
- Alt 押下中は snap を一時無効化
- 選択 keyframe のみを対象にする
- timeline の操作感と矛盾しないようにする

---

## 操作仕様

### 1. Arrow Nudge

- `Left` = 1 frame 左
- `Right` = 1 frame 右

### 2. Shift Nudge

- `Shift + Left` = 10 frame 左
- `Shift + Right` = 10 frame 右

### 3. Temporary Snap Override

- `Alt` 押下中は snap を一時的に無効化する
- drag と keyboard nudge の両方で同じ override を使う
- permanent setting は変更しない

---

## Phase 構成

### Phase 1: Keyframe Move Command

- keyframe selection を相対移動する command を作る
- 1 frame / 10 frame の step を受け取れるようにする
- undoable にする

完了条件:

- 選択 keyframe を相対移動できる

### Phase 2: Shortcut Binding

- Left / Right Arrow を nudge に割り当てる
- Shift 修飾で 10 frame に切り替える
- 他の shortcut と衝突しないようにする

完了条件:

- keyboard だけで微調整できる

### Phase 3: Snap Override

- Alt 押下中の snap 無効化を実装する
- drag / nudge の両方で同じ挙動にする
- override 中であることを UI に伝える

完了条件:

- 一時的に snap を外せる

### Phase 4: Selection and Bounds Safety

- 選択範囲が複数でも破綻しない
- frame bounds を越える移動をどう扱うかを決める
- locked / read-only keyframe はスキップする

完了条件:

- 端や複数選択でも安全に動く

実装メモ (2026-06-15):
- 複数選択対応・frame bounds clamp（`[0, durationFrames-1]`）は既存実装にあり。
- locked layer の keyframe スキップを追加（`ArtifactTimelineTrackPainterView.cpp` の nudge ループ内で `layer->isLocked()` をチェック）。

### Phase 5: Timeline Feedback

- nudge 時に現在の移動量を軽く表示する
- snap override の状態を表示する
- 操作結果が分かりやすいようにする

完了条件:

- 何フレーム動いたかが分かる

実装メモ (2026-06-15):
- 移動量表示は既存 `timelineDebugMessage` で対応済み。
- Alt 押下時（snap override 中）にメッセージへ "(snap override)" を追記するよう改善。nudge 自体は整数フレーム移動で snap が効かないため、Alt は実質的にドラッグ時のみ意味を持つが、状態の一貫性表示として追加。

---

## 実装順

1. relative move command
2. shortcut binding
3. snap override
4. safety handling
5. feedback

---

## 対象範囲

- `ArtifactTimelineWidget`
- keyframe editing surface
- snap handling
- shortcut routing

---

## リスクと留意点

- arrow key が他の編集操作と衝突する可能性がある
- snap override を恒常設定と混同しない必要がある
- 複数 keyframe の相対移動は順序依存の問題が出る可能性がある

---

## 成功条件

- 選択 keyframe を矢印キーで 1 frame / 10 frame 移動できる
- Alt 押下中は snap を外せる
- スクリプトに頼らず標準操作で微調整できる

---

## 関連

- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`
- `docs/planned/MILESTONE_SNAP_ADVANCED_2026-04-10.md`
- `docs/planned/MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md`
