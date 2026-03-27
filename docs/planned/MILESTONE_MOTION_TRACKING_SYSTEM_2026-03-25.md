# マイルストーン: モーショントラッキングシステムの段階導入

> 2026-03-25 作成

## 目的

動画・コンポジット向けのモーショントラッキングを、`ArtifactCore` から `Artifact` アプリ層まで段階的に導入する。

狙いは次の 4 つ。

1. 追跡結果を再利用可能な Core データとして持つ
2. レイヤー変形、マスク、カメラ、安定化へ接続する
3. UI と処理を分離し、長時間処理を扱いやすくする
4. 将来の自動化・解析・ロトスコープ機能へつなぐ

この文書は `MILESTONE_FEATURE_EXPANSION_2026-03-25.md` の Phase 1 に対応する詳細ワークストリームでもある。
Feature Expansion 側で「追跡を制作能力として増やす」と定義し、本書では実装順と連携先を詰める。

---

## 現状

`ArtifactCore` には既に `Tracking.MotionTracker` がある。

持っているもの:

- `MotionTracker`
- `TrackerManager`
- `TrackerSettings`
- `TrackPoint` / `TrackFrame` / `TrackRegion`
- 順方向・逆方向・範囲トラッキング
- 結果の補間、外れ値除去、スムージング
- JSON / ファイル保存

ただし現状はまだ次の問題がある。

- Core の追跡結果がアプリの各機能へ十分に接続されていない
- レイヤー変形やマスクの自動駆動に統一されていない
- UI 側の tracker editor / overlay / workflow が未整備
- 長時間処理の進捗、キャンセル、Undo、再実行の導線が弱い

---

## 方針

### 原則

1. 追跡データの正は Core に置く
2. UI は追跡結果を編集・可視化するだけにする
3. アプリ層は長時間処理の orchestration を担当する
4. レイヤー変形への適用は property / animation 系と整合させる

### 想定する対象

- レイヤーの位置追跡
- 平面トラッキング
- カメラトラッキング
- マスク追従
- 安定化
- 追跡結果からのキーフレーム生成

---

## 既存資産

### Core

- `ArtifactCore/include/Tracking/MotionTracker.ixx`
- `ArtifactCore/src/Tracking/MotionTracker.cppm`

### 関連候補

- `ArtifactCore/include/Property/AbstractProperty.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`
- `ArtifactCore/include/Time/TimeRemap.ixx`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

---

## Phase 1: Core の追跡モデルを固める

### 1-1. データモデルの完成

`MotionTracker` の結果形式を、追跡・補間・再利用に耐える形へ整える。

追加したいもの:

- 追跡ソースのメタデータ
- 追跡対象の種類
- 追跡信頼度の集約
- フレーム単位の補正履歴
- 追跡失敗区間の記録

### 1-2. Core session の保存/復元

`MotionTracker` 単体を、追跡結果だけでなく設定・追跡点・追跡領域も含めて保存/復元できるようにする。

追加したいもの:

- tracker settings のシリアライズ
- track points / regions のシリアライズ
- result の正規化
- `clearTrackingData()` のような session reset

#### Implemented

- `TrackResult::normalize()` / `setFrame()` / `frameCount()`
- `MotionTracker::clearTrackingData()`
- `MotionTracker::hasResult()`
- tracker settings / track points / track regions の JSON 保存復元

### 1-3. 追跡アルゴリズムの実装強化

現在の簡易実装を、実運用に耐える back-end に差し替える。

候補:

- OpenCV ベースの optical flow
- feature based tracking
- template matching
- planar tracking
- hybrid mode

### 1-4. 入出力

- JSON / project serialization
- 外部トラッキングデータの import / export
- 追跡セッションの保存と復元

---

## Phase 2: Core Animation / Property 連携

### 2-1. キーフレーム化

追跡結果をそのまま使えるように、property へ変換する導線を作る。

対象:

- `transform.position`
- `transform.rotation`
- `transform.scale`
- `mask path`
- `camera transform`

### 2-2. property bridge

`AbstractProperty` / `AnimatableTransform3D` へ接続し、追跡結果を再生中に反映できるようにする。

ここでやること:

- tracker result から property key を生成
- property interpolation と追跡補正の優先順位を定義
- manual keyframe と tracked keyframe の共存ルールを決める

### 2-3. 時間系の整合

- `FramePosition`
- `FrameRate`
- `TimeRemap`
- nested composition の時間変換

追跡結果はフレーム値と秒値の両方で扱えるようにする。

---

## Phase 3: Layer / Effect への接続

### 3-1. レイヤー追跡アタッチ

レイヤーに tracker を紐づける。

想定:

- `CameraLayer` にカメラ追跡をアタッチ
- `ImageLayer` / `VideoLayer` に平面追跡をアタッチ
- `TextLayer` に位置/回転追跡をアタッチ

### 3-2. マスク追従

追跡結果を mask / roto の control point に反映する。

### 3-3. 安定化

追跡結果を使って `stabilize` を生成する。

ここでは:

- 座標の平滑化
- 回転補正
- スケール補正
- 反転やジャンプの除去

を扱う。

---

## Phase 4: UI 導線

### 4-1. Tracker Editor

トラッカー編集用 UI を追加する。

必要なもの:

- 追跡点の追加 / 削除
- 追跡領域の描画
- 追跡パスの表示
- 信頼度ヒートマップ
- current frame の scrub
- track forward / backward ボタン

### 4-2. Overlay 表示

コンポジションビュー / プレビュー上に追跡オーバーレイを描く。

表示対象:

- ポイント
- 領域
- motion path
- tracked anchor
- confidence

### 4-3. ワークフロー

UI から次を実行できるようにする。

- track once
- track range
- smooth
- remove outliers
- generate keyframes
- bake to layer

---

## Phase 5: App 層サービス化

### 5-1. TrackingService

アプリ層に追跡専用サービスを追加する。

責務:

- tracker の生成 / 保持 / 削除
- 長時間処理のキュー管理
- progress / cancel / error report
- project への保存

### 5-2. Undo / Redo

追跡関連操作を command 化する。

対象:

- tracker の追加 / 削除
- 追跡点編集
- 領域編集
- keyframe bake
- stabilize 適用

### 5-3. 非同期処理

長時間トラッキングは UI を止めずに走らせる。

必要要素:

- worker thread
- progress callback
- cancel token
- partial result commit

---

## Phase 6: アプリ機能への展開

### 6-1. Motion Tracking Workflow

ユーザーが以下を自然に辿れるようにする。

1. レイヤーを選ぶ
2. トラッキング対象を指定する
3. 追跡を走らせる
4. 結果を確認する
5. キーフレームへ焼き込む
6. 必要なら安定化やマスクへ適用する

### 6-2. Camera / Roto / Text 連携

追跡結果を各機能へ横展開する。

- camera solve の補助
- roto mask の自動追従
- text / title の追従アニメーション

### 6-3. 解析・補助機能

将来の拡張候補:

- auto track suggestions
- track confidence visualization
- keyframe refinement
- shot cut aware tracking
- reference frame selection

---

## Phase 7: Project / Asset / Export Bridges

### 7-1. Project / Asset への接続

追跡セッションを project asset の一種として扱えるようにする。

対象:

- tracker preset
- tracked region template
- motion path session
- recent tracking project

### 7-2. Review / Export への接続

追跡結果を確認・共有しやすい形にする。

対象:

- tracked overlay の review
- stabilize before/after compare
- tracked keyframe の export
- session summary の共有

### 7-3. 再利用導線

追跡結果を一度作って終わりにしない。

対象:

- recent tracker session
- preset recall
- per shot tracking template
- relink / reuse / duplicate

---

## 優先順位

### 最優先

1. Core の追跡結果保存形式を安定化
2. `MotionTracker` と `AbstractProperty` の bridge
3. 非同期実行とキャンセル

### 高

1. Tracker Editor UI
2. Overlay 可視化
3. Layer への追跡適用

### 中

1. 安定化
2. マスク追従
3. Camera solve 補助

### 低

1. 高度な自動化
2. AI 補助の追跡提案
3. 外部フォーマットの広範囲サポート

---

## リスク

- 追跡データと手動キーフレームの優先順位を曖昧にすると破綻しやすい
- 長時間処理を UI スレッドに乗せると操作感が悪化する
- レイヤー種別ごとの適用ルールが曖昧だと実装が散る
- 既存の transform / property / time remap と整合しないまま進めると再生系が壊れる

---

## 次の一手

最初にやるべきは次の 3 つ。

1. `MotionTracker` の保存形式と import/export を確定する
2. `AbstractProperty` へ変換する bridge を決める
3. トラッカー編集 UI の最小版を作る

---

## Recommended Execution Order

1. Core の追跡結果保存形式を安定化する
2. `AbstractProperty` へ変換する bridge を決める
3. トラッカー編集 UI の最小版を作る
4. Layer / mask / camera への適用を整える
5. App 層の TrackingService を詰める
