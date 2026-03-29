# マイルストーン: Feature Expansion Focus Trio

> 2026-03-28 作成

## 目的

機能拡充の中でも、優先度を上げた 3 本を実装順つきで固定する。

対象は次の 3 つ。

1. `Motion Tracking Workflow`
2. `Vector / SVG Layer Import`
3. `Animated Image Export`

この 3 本は性質が異なるため、同じ「機能追加」でも順番を明示して進める。

---

## 方針

### 原則

1. まず制作の核になる追跡を固める
2. 次に素材入力の幅を広げる
3. 最後に出力形式の価値を広げる
4. 各段階で Core と UI の責務を分ける
5. 既存の render / asset / project の経路を壊さない

### 位置づけ

- `Motion Tracking Workflow` は `Feature Expansion` の中核
- `Vector / SVG Layer Import` は `Asset System` と `Composition` をまたぐ
- `Animated Image Export` は `Render` 側の拡張だが、機能拡充の成果として扱う

---

## Phase 0: Shared Prerequisites

### 目的

3 本に共通する下準備を先に整える。

### 重点

- command surface / menu routing の安定化
- asset / project / viewer の状態同期
- render preset surface の整理
- status / diagnostics の共通表示

### 完了条件

- どの機能も既存画面に無理なく入口を持てる
- 状態表示がばらつかない

---

## Phase 1: Motion Tracking Workflow

### 目的

映像内の対象を追跡し、レイヤー変形・マスク・カメラ補助・安定化へ使えるようにする。

### 先にやる理由

- 制作の核になる
- Core 側に既存資産がある
- 追跡結果は後続の transform / vector / animation にも使い回せる

### 範囲

- track forward / backward / range
- motion path 表示
- tracked transform bake
- stabilize 生成
- tracker editor と overlay

### 連携先

- `ArtifactCore::MotionTracker`
- `ArtifactCompositionEditor`
- `ArtifactPropertyWidget`
- `ArtifactTimelineWidget`

---

## Phase 2: Vector / SVG Layer Import

### 目的

SVG を単なる画像ではなく、layer として取り込んで編集できるようにする。

### 先にやる理由

- 素材入力の幅を広げる
- composition / property / project の一連の導線に乗せやすい
- motion tracking 後の変形対象としても相性がよい

### 範囲

- SVG ingest
- vector layer representation
- raster preview / rendering path
- persistence / relink

### 連携先

- `ArtifactAssetBrowser`
- `ArtifactProjectService`
- `ArtifactLayerFactory`
- `ArtifactCompositionEditor`

---

## Phase 3: Animated Image Export

### 目的

GIF / APNG / Animated WebP を、動画とは別の出力系として整理する。

### 先に回す理由

- 出力価値を早く上げられる
- render queue に比較的少ない変更で足せる
- motion tracking / vector import の成果を共有しやすい

### 範囲

- animated image preset surface
- GIF / APNG / Animated WebP
- palette / alpha / frame delay の整理
- render queue との統合

### 連携先

- `ArtifactRenderQueueService`
- `ArtifactRenderQueuePresets`
- `FFmpegEncoder`
- `ArtifactPlaybackEngine`

---

## 実装順

1. `Motion Tracking Workflow`
2. `Vector / SVG Layer Import`
3. `Animated Image Export`

---

## 完了条件

- 追跡が制作導線として使える
- SVG が layer として扱える
- animated image が render queue から選べる
- 3 本の入口と状態表示が壊れない

---

## 関連文書

- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- `docs/planned/MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md`
- `docs/planned/MILESTONE_ANIMATED_IMAGE_EXPORT_2026-03-27.md`
- `docs/planned/MILESTONES_BACKLOG.md`
