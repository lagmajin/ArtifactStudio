# マイルストーン: Motion Tracking Phase 1 Execution

> 2026-03-28 作成

## 目的

`Motion Tracking Workflow` の Phase 1 を、実装順がぶれない粒度に落とす。

この文書は `MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md` の Phase 1 を実行レベルまで分解したもの。

---

## 現状認識

`ArtifactCore` にはすでに `Tracking.MotionTracker` がある。

持っているもの:

- `MotionTracker`
- `TrackerManager`
- `TrackerSettings`
- `TrackPoint` / `TrackFrame` / `TrackRegion`
- forward / backward / range tracking
- result interpolation
- smoothing / outlier removal
- JSON / file save / load

残っているのは、制作機能として使える形への接続。

- 保存形式の安定化
- 非同期トラッキング
- cancel / progress / partial commit
- property / keyframe bridge
- UI 導線

---

## Phase 1 の範囲

### 1-1. Result Schema Freeze

追跡結果の形を固定する。

対象:

- track metadata
- frame list
- confidence
- failure frames
- correction history

完了条件:

- 結果を保存して再読込しても崩れない
- 補間・再利用の前提が明確になる

### 1-2. Session Save / Restore

追跡セッションとして保存・復元できるようにする。

対象:

- settings
- track points
- track regions
- result
- tracker name / type

完了条件:

- 追跡セッションを project asset として扱える
- リロード後に同じ状態へ戻せる

### 1-3. Async Tracking Orchestration

長時間追跡を UI から切り離す。

対象:

- worker thread
- progress callback
- cancel token
- partial result commit

完了条件:

- UI を止めずに追跡できる
- 途中キャンセルしても途中結果を破棄しない

### 1-4. Property Bridge

追跡結果を property / animation に変換する。

対象:

- position
- rotation
- scale
- camera transform
- mask anchor

完了条件:

- 追跡結果を keyframe に焼ける
- manual keyframe と共存できる

---

## 実装順

### Step 1

`TrackResult` の保存形式と正規化ルールを固定する。

### Step 2

`MotionTracker` を非同期実行できるサービス化の下地に載せる。

### Step 3

結果を `AbstractProperty` / `AnimatableTransform3D` に渡す橋を作る。

### Step 4

Tracker Editor / Overlay の UI 導線に接続する。

---

## 既存資産

- `ArtifactCore/include/Tracking/MotionTracker.ixx`
- `ArtifactCore/src/Tracking/MotionTracker.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`

---

## リスク

- 手動キーフレームと追跡補正の優先順位を曖昧にすると再生が壊れる
- UI スレッドで追跡を回すと固まりやすい
- 追跡データの schema が後で揺れると project 保存が不安定になる

---

## 完了条件

- Motion Tracking の保存・復元が安定している
- 非同期追跡が UI を止めない
- 追跡結果を property / keyframe に流せる
- 後続の UI 追加に必要な契約が固まっている

---

## 2026-05-08 接続方針メモ

Core 側の `Tracking.MotionTracker` はすでに広めの API を持っているが、
`QImage` ベースの入力と UI/Property への接続がまだ直結していない。
そのため、いきなり Tracker Editor を作るより、以下の順で小さく接続する。

1. `MotionTracker` の結果を読むだけの adapter を `Artifact` 側に置く
   - `TrackResult`
   - `TrackFrame`
   - `TrackPoint`
   - confidence / failure frame

2. Composition Debug / App Debugger に tracker summary を表示する
   - tracker count
   - selected tracker name
   - result frame count
   - average confidence
   - problem frame count

3. 選択レイヤーへ「tracker を作る」だけの薄い導線を追加する
   - 実行はまだしない
   - source layer / tracker id / point or region だけ保持する

4. Property bridge は最後に接続する
   - tracked position を直接 transform に流さない
   - まず preview / debug overlay で確認する
   - keyframe bake は手動 keyframe との優先順位を決めてから行う

禁止事項:

- UI スレッドで長時間 tracking を回さない
- `QImage` 変換を描画ホットパスへ混ぜない
- tracker 結果を即座に transform property へ自動適用しない
