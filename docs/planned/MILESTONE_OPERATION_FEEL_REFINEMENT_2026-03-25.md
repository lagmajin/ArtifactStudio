# マイルストーン: 操作感改善 / Responsiveness Refinement

> 2026-03-25 作成

## 目的

機能を増やすのではなく、既存機能の「触ったときの気持ちよさ」を上げる。

このマイルストーンは、次のような症状をまとめて扱う。

- クリックやドラッグの初動が重い
- UI の更新が一拍遅れる
- 連続操作で見た目が詰まる
- 入力と描画の責務がずれている
- 画面ごとに操作感がばらつく

---

## 対象範囲

### 高頻度 UI

- `ArtifactCompositionEditor`
- `ArtifactTimelineWidget`
- `ArtifactLayerPanelWidget`
- `ArtifactProjectManagerWidget`
- `ArtifactAssetBrowser`
- `ArtifactInspectorWidget`
- `ArtifactPlaybackControlWidget`
- `ArtifactPropertyWidget`
- `ArtifactConsoleWidget`

### 影響の大きい Core 領域

- `ArtifactCore` の event / property / animation 系
- `ArtifactCore` の audio / tracking / rendering data flow
- `Undo/Redo`
- `PlaybackEngine`

---

## 現状の問題

1. UI が「入力を受けた瞬間」に重い処理をしやすい
2. 同じ状態更新が複数ウィジェットへ広がりやすい
3. repaint / relayout / refresh が過剰になりやすい
4. drag / scrub / seek / selection の初動が不自然になりやすい
5. レンダラや service の責務が混ざり、最適化しにくい

---

## 方針

### 原則

1. 入力と描画を分離する
2. 変更範囲を最小化する
3. 高頻度処理は coalesce / throttle 前提にする
4. 画面の見た目は揃えても、内部更新は必要最小限にする
5. 1 回の操作で複数 widget を全面更新しない

### 設計上の前提

- Qt signal/slot は当面残してよい
- ただし fan-out が大きい経路は Core event system に寄せる
- `property` / `undo` / `playback` / `tracking` は順に土台を固める

---

## Phase 1: 触り始めの遅さを消す

### 1-1. Drag / Seek / Scrub の初動改善

対象:

- timeline drag
- composition pan / zoom
- property slider drag
- playback scrub

やること:

- mouse press 直後の重い refresh を避ける
- drag 中は軽い描画に落とす
- release 時にだけ重い確定処理を走らせる
- capture / release の取りこぼしをなくす

### 1-2. Selection 変更の軽量化

対象:

- layer selection
- asset selection
- project tree selection

やること:

- 変化がないときは再構築しない
- 表示更新は差分化する
- hover と selection を分離する

---

## Phase 2: Repaint / Relayout の抑制

### 2-1. 更新の coalesce

同じフレーム内で複数回飛ぶ更新をまとめる。

対象:

- projectChanged
- selectionChanged
- frameChanged
- propertyChanged
- asset refresh

### 2-2. visible widget のみ更新

非表示の widget に対しては重い refresh を止める。

### 2-3. 再レイアウトの抑制

サイズ変化がない場合は `updateGeometry()` / `repaint()` を打たない。

---

## Phase 3: イベント伝播の整流化

### 3-1. Core Event System への移行

高頻度イベントを Qt signal の多重 fan-out から分離する。

候補:

- `FrameChanged`
- `SelectionChanged`
- `CurrentCompositionChanged`
- `PlaybackStateChanged`
- `PropertyChanged`

### 3-2. UI は購読者にする

Widget は「状態の正」ではなく「結果の表示」に寄せる。

---

## Phase 4: レンダリング応答性

### 4-1. Composition Viewer

- pan / zoom / fit の応答性改善
- fallback と GPU path の責務整理
- gizmo と layer draw を別々に更新しすぎない

### 4-2. Timeline

- スクロールとズームの追従改善
- 行仮想化
- 長いタイムラインでの再描画抑制

### 4-3. Console / Diagnostics

- ログ flood を抑える
- 重要ログを見失わない導線を作る
- コピー・保存の操作を軽くする

---

## Phase 5: コマンド / Undo の感触改善

### 5-1. 連続操作の安定化

- undo/redo の即応性
- command の粒度調整
- 不要な state 再計算を避ける

### 5-2. 高頻度編集の smooth 化

対象:

- layer move
- property drag
- keyframe edit
- audio mixer 操作

---

## Phase 6: 応答性の計測

### 計測したい項目

- mouse press から反応開始までの時間
- drag 中の frame time
- selection 変更 1 回あたりの更新回数
- repaint / relayout の回数
- hidden widget の refresh 回数

### 目標

- 触った瞬間に反応が見える
- 1 回の操作で無駄な全面更新が起きない
- 非表示 UI が裏で重くならない

---

## 優先順位

### 最優先

1. drag / scrub / seek の初動
2. selection 変更の差分化
3. hidden widget の refresh 抑制

### 高

1. Core event 化の開始
2. Timeline / Composition Viewer の応答性
3. Console の軽量化

### 中

1. Undo の感触調整
2. Property editor の連続操作改善
3. Tracking / Audio の長時間処理の UI 応答

---

## 関連文書

- `docs/planned/MILESTONE_EVENT_SYSTEM_MIGRATION_2026-03-25.md`
- `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md`
- `docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md`
- `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- `docs/planned/MILESTONE_M12_POLISH_AND_STABILITY_2026-03-17.md`
- `docs/planned/MILESTONES_BACKLOG.md`

---

## Next Step

最初にやるべきことは 3 つ。

1. 高頻度 UI の更新ログを入れて、どこで重いかを測る
2. drag / scrub / seek の入力経路を軽量化する
3. hidden widget の refresh と relayout を止める

