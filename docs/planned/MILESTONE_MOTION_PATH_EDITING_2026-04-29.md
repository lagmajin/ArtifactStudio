# MILESTONE: Motion Path Editing

**Date**: 2026-04-29
**Status**: In Progress
**Priority**: Medium
**Related**: `docs/worklog/MOTION_PATH_EDITING_WORKLOG_2026-04-29.md`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

---

## 概要

Composition Editor のモーションパスを、表示用の補助線ではなく実用的な編集対象として整備するマイルストーン。
After Effects 風の編集感を出しつつ、既存のレイヤー移動・undo・overlay 系の設計に馴染む形で拡張する。

---

## 現状

| 項目 | 状態 |
|---|---|
| モーションパス表示 | 実装済み |
| キーフレーム点ドラッグ | 実装済み |
| キーフレーム追加 | 実装済み |
| キーフレーム削除 | 実装済み |
| 補間タイプ変更 | 実装済み |
| hover 強調 | 実装済み |
| ハンドル編集 | 未実装 |
| ビルド確認 | 未実施 |

---

## 目標

- モーションパスのキーを直接操作できる
- 追加・削除・移動・補間変更が一通り揃う
- 現在どのキーを触っているかが分かりやすい
- 既存の undo / redo と整合する
- editor の右クリックメニューとショートカットの両方から触れる

---

## 実施済み作業

### 1. 直接編集

- モーションパスのキー点をドラッグして移動できる
- ドラッグ終了時に undo を push する
- 親レイヤーがある場合は親空間に戻して位置を更新する

### 2. 追加 / 削除

- `Shift + クリック` で現在フレームにキーを追加
- `Alt + クリック` で現在ヒットしたキーを削除
- 右クリックメニューからも追加 / 削除できる

### 3. 補間変更

- `Hold / Linear / Ease In / Ease Out / Ease In-Out / Bezier / Back / Expo` を選択可能
- 現在の補間と同じ項目は無効化
- 補間変更も undo 対応

### 4. 表示強化

- キー点を補間タイプごとに色分け
- hover 中のキーをリングで強調
- 現在フレームのキーを見分けやすくした

### 5. Core 補助 API

- `AnimatableValueT` に keyframe value 上書き API を追加
- `AnimatableTransform3D` に位置キー value / interpolation の API を追加

---

## フェーズ

### Phase 1: Motion Path Direct Editing
**目標**: キー点の移動・追加・削除を実用レベルにする。

- [x] キー点ドラッグで位置を変更
- [x] `Shift + クリック` でキー追加
- [x] `Alt + クリック` でキー削除
- [x] undo / redo 対応

### Phase 2: Interpolation Control
**目標**: キーの補間を編集できるようにする。

- [x] 補間タイプの選択メニュー追加
- [x] 現在値と同じ補間を無効化
- [x] `Bezier` など AE っぽい候補を追加

### Phase 3: Visual Feedback
**目標**: どのキーを触っているか直感的に分かるようにする。

- [x] hover 強調
- [x] 補間タイプ別の色分け
- [ ] 選択中キーの更なる強調
- [ ] フレーム番号ラベル表示

### Phase 4: Handle Editing
**目標**: Bezier ハンドルを直接編集できるようにする。

- [ ] in/out ハンドルの表示
- [ ] ハンドルドラッグで補間曲線を調整
- [ ] ハンドル編集の undo 対応

---

## 成功条件

1. モーションパスのキーを移動できる
2. キーの追加・削除がショートカットとメニューの両方でできる
3. 補間変更が undo / redo で戻せる
4. hover / current key の状態が視覚的に分かる
5. 既存のレイヤー操作や gizmo と競合しない

