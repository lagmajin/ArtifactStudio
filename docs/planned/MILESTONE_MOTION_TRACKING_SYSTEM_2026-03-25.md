# マイルストーン: モーショントラッキングシステムの段階導入

> 2026-03-25 作成

**最終更新:** 2026-10-11

**ステータス:** In Progress

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
- 結果フレームの有限値検証、重複ポイントID・不正ホモグラフィの正規化
- 失敗フレームと問題フレームの統合、キャンセル時の部分結果保持

### 1-3. 追跡アルゴリズムの実装強化

現在の簡易実装を、実運用に耐える back-end に差し替える。

候補:

- OpenCV ベースの optical flow
- feature based tracking
- template matching
- planar tracking
- hybrid mode

#### Implemented

- Optical Flow / Feature Based / Template Matching / Hybrid の方式別処理
- Planar homography の適用と信頼度評価
- `cv::Mat` フレーム入力、進捗コールバック、キャンセル対応
- キャリブレーション済みカメラ姿勢 solve、再投影誤差、姿勢系列JSON、非同期ジョブ

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

---

## Static audit follow-up (2026-07-25)

Phase 1 実行文書と現行ソースを突き合わせた。`MotionTracker` は optical flow / feature / hybrid / NCC 系の処理、結果の正規化、失敗・問題フレーム、補正、JSON / file save-load、progress / stop を持つ。Artifact 側にも Point Tracker Gizmo、overlay、Composition Render Controller の trackerApplyToPosition / trackerApplyToAnchor があり、Core からアプリへの最小接続は進んでいる。

未完了または未確認なのは、独立した TrackingService による worker thread orchestration、project asset としての session 管理、mask / camera / stabilization の統一 bridge、manual keyframe との優先順位、Tracker Editor の一貫した編集 workflow、runtime の長時間処理と cancel 後の部分結果確認である。

### Audit status

- Phase 1 Core model / session: 実装済み相当 — schema 正規化・保存復元・部分結果保持を確認
- Phase 2 UI / overlay: 部分実装 — Gizmo / overlay はあるが、専用 Editor workflow は未確認
- Phase 3 property / layer bridge: 部分実装 — position / anchor を確認。mask / camera / stabilization は未統合
- Phase 4 orchestration: 部分実装 — progress / stop API はあるが、専用 service / worker 契約は未確認
- Status: `In Progress` を維持

## Update 2026-08-15

- `MotionTracker` は point／region／planar、前後・範囲追跡、Optical Flow／Feature／Hybrid／NCC、信頼度・問題フレーム、補正・平滑化・外れ値除去、JSON／file save-load、progress／stop を引き続き提供している。
- Artifact 側では `ArtifactPointTrackerTool`、`ArtifactPointTrackerGizmo`、Composition Render Controller の tracker apply、Corner Pin、Video Layer の tracker ID、Diagnostics 表示まで接続されている。2D tracker の主要導線は別実行文書と整合する。
- ただし、独立 TrackingService／worker orchestration、project asset としての session lifecycle、mask／camera／stabilization の統一 bridge、manual keyframe との優先順位、長時間処理の cancel 後部分結果は未確認または未完了である。
- 判定は **Core／2D UI／position・anchor 適用は実装済み相当、アプリ横断統合と runtime 受入れは未完了** を維持する。ビルド・テスト・runtime 確認は未実施。

## Update 2026-10-11 — Tracker panel integration

- `ArtifactCompositionEditor::trackerPanelWidget()` は `AppMain.cppm` から右側の独立 Tracker ドックとして登録されている。Point／Planar の切替、Viewport 上での追跡形状編集、前後の範囲追跡と1フレーム送り（成功時は Playback Service 経由で再生ヘッドを1フレーム進める／戻す）、全範囲の解析、停止・進捗、問題フレーム確認、平滑化・外れ値除去・信頼度フィルターをドックから操作できる。Point モードではドックから追加配置を有効にし、Viewport クリックで追跡点を追加、最後の点を削除できる。解析後は登録した全ポイントの軌跡と信頼度マーカーを表示し、各ポイントの軌跡点を個別にドラッグ補正できる。点の追加・削除と解析方式／信頼度しきい値（5%刻み）の変更は Tracker の Undo 履歴へ記録し、Redo で再適用される。Viewport 上の軌跡点の手動補正も、ドラッグ単位で Undo／Redo に記録する。進捗は入力フレームのキャプチャとバックグラウンド解析を各50%で合算して表示する。Point／Planar 切替、解析、適用、Tracker 削除は専用ドックへ集約し、Viewport の右クリックメニューから重複する操作を除いた。Viewport は追跡形状と軌跡点の直接編集を担当する。
- 適用操作は Point の Position／Anchor／複数 Null と Planar の Corner Pin／Mask を提供する。UI は解析元と適用対象を別に表示する。追跡元は静止画／連番画像レイヤーをドックから巡回選択でき、結果の適用先は Composition の選択レイヤーとする。追跡元レイヤー ID はプロジェクト `extension_data` にコンポジション単位で保存し、sessionごとの上書きも保存する。Tracker session の一覧・切替・削除は Composition 単位の manifest で管理し、sessionごとの TrackerBoxState を保存・復元する。解析中は入力元を固定し、解析結果がある間は入力元の切替を止める。未解析・解析中・モード違い・マスクパス不在では適用操作を無効化する。解析操作は静止画／連番画像レイヤー選択時に限定される。ドックの状態行は、ソース選択・セッション作成・追跡形状設定・解析・完了・失敗・キャンセルの各段階で次の操作または結果を示す。解析中はドックが非表示でも進捗状態を更新する。
- Tracker ドックの文言・ツールチップは `TranslationManager` 経由にし、設定で選択できる `en`／`ja`／`zh`／`zh-TW` の翻訳キーを追加した。言語別の実画面レイアウトとキャプチャ確認は未実施。
- **判定を更新:** Tracker Editor の基本操作面と Overlay／Bake の UI 接続はコード上で実装済み相当。ビルド、言語別レイアウト、実機でのポインター編集、解析／キャンセル、Undo と適用結果の runtime 受入れは未確認。
- session 作成・削除は TrackerSessionUndoCommand で tracker JSON、同じ tracker ID、source override、TrackerBoxState を Undo/Redo できる。UndoManagerChangedEvent 後に Controller が active manifest と現在の binding を照合して再bindする。追跡元レイヤーの削除では RemoveLayerCommand が該当するComposition既定sourceとsession別source参照を整理し、Undoで元レイヤーが戻った場合だけ、まだ別sourceへ変更されていない空き参照を復元する。Controller の source 解決は存在しない layer ID を拒否して現在の選択へ fallback する。未完了は、旧プロジェクトで Manager 内に残る未登録trackerの復元保証、長時間処理の共通 service 化とキャンセル後の部分結果方針、manual keyframe との優先順位、カメラ／安定化を含む横断適用である。旧データの孤立 tracker は所有先を安全に推定できない。ビルド・テスト・runtime 受入れは未実施。
- Tracker ドックの「追加」は既存セッションがある場合も有効にし、Composition 内に別セッションを作成・選択できるようにした。新規セッション作成前に現在のセッションの TrackerBoxState を保存する。作成をUndoすると作成前のactive session IDへ戻り、Redoで新規セッションを再選択する。Undo登録失敗時も直前のController bindingを復元する。UI上の多セッション作成・切替はruntime未確認。
- 「全ポイントをNull化」は各ポイントのレイヤー追加と位置キーフレームを単一の `Apply All Tracking Points` Undo macro にまとめ、一回のUndoで一括適用を戻せるようにした。Undo履歴の容量や実際の一括適用結果はruntime未確認。
- Pointの「Position適用」は位置キーフレームだけを書き、Anchor操作とは分離した。個別の「Anchor適用」はAnchorプロパティのみを書き換える。
- 問題フレームのレビュー移動は `ArtifactPlaybackService` に委譲する。Playback Service が対象Compositionを所有していない場合はCompositionへ直接seekせず、レビュー操作を無効化し、ドックのツールチップでPlaybackの対象にする必要を表示する。
- 1フレーム追跡もPlayback Serviceが対象Compositionを所有し、再生中でない場合のみ実行可能とし、開始フレームは同Serviceのcurrent frameから解決する。別Compositionが再生対象の場合や再生中は両方向のstep操作を無効化してツールチップに理由を表示する。
- 既存の `PlaybackStateChangedEvent` 購読から Tracker ドックを更新し、再生／一時停止／停止の遷移に合わせてフレーム送りの有効状態とツールチップを同期する。
- 問題フレームのレビューは前後両方向へ移動可能にし、Playback Service が対象Compositionを所有する条件を両操作で共有する。双方向巡回のruntime確認は未実施。
- 停止中／スクラブ中の再生ヘッド位置について、最寄りの結果フレーム番号・信頼度を品質欄に表示し、明示的な失敗フレームと信頼度しきい値未満を区別する。再生中は表示をLive playbackに切り替え、FrameChangedイベント毎の文字列生成を避ける。FrameChangedとTracker resultの対応runtime確認は未実施。
- problem frameが1件以上あるときは、全5種類のBake操作前に件数を示し、失敗フレームにはキーがなくなる可能性と低信頼度値が使われる可能性を説明して明示確認する。既定はCancel。問題フレームがない場合は確認を挟まない。ダイアログ操作・Undo結果はruntime未確認。
- 品質欄に結果フレームの信頼度推移を固定48サンプルの owner-draw グラフで表示し、設定中の信頼度しきい値を破線で重ねる。横軸は結果時刻に揃え、サンプル間の欠落区間は線を切る。データはcaller-owned fixed arraysへbounded samplingし、再生中と解析中はグラフ値を更新しない。視覚品質と低幅ドックでのレイアウトはruntime未確認。
- Planar のマスク適用対象を選択レイヤー内のマスク／パス間で巡回できるようにし、選択中の番号をドックに表示する。Apply は選択パスだけへ書き込む。UI文言は `en`／`ja`／`zh`／`zh-TW` に追加した。実機操作とキーフレーム適用結果は未確認。
