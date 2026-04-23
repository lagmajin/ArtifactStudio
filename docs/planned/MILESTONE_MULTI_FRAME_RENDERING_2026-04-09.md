# マイルストーン: Multi-Frame Rendering (MFR) for Render Queue

> 2026-04-09 作成

## 目的

Render Queue の書き出しを、1 フレームずつ直列に処理する方式から、複数フレームを同時に進められる Multi-Frame Rendering (MFR) へ段階的に移行する。

この milestone は、ライブプレビューを速くするためではなく、まずは export / render queue のスループット向上を狙う。

---

## 背景

現状の render queue は、1 job 内で 1 frame を処理してから次へ進む前提が強い。

そのため、GPU も CPU も待ち時間が発生しやすく、特に以下のケースで効率が落ちる。

- 高解像度の動画書き出し
- 重いレイヤー / effect が多いショット
- readback や encode 待ちが混ざるケース
- 連番出力で単純な直列化がボトルネックになるケース

MFR はこの待ち時間を複数フレームの in-flight 化で埋める。

---

## 方針

### 原則

1. まずは export-only で導入する
2. live preview には入れない
3. 既存の 1-frame render 経路を残す
4. job state は frame ごとに snapshot する
5. cache / temp buffer / readback は上限付きで管理する
6. backend ごとに最適な並列度を変えられるようにする

### 想定する効果

- 書き出し時間の短縮
- GPU / CPU の idle time 削減
- encode 待ち中に次フレームの準備を進められる
- レンダーキューの長いジョブで体感が改善する

---

## Phase 1: Frame Snapshot Contract

### 目的

各フレームを独立した snapshot として扱えるようにする。

### 作業項目

- composition / job / playback state の frame snapshot を定義する
- frame ごとの render input を immutable にする
- layer mutation と render execution の境界を切る
- cache key に frame dependency を明示する

### 完了条件

- 1 フレームの描画に必要な state が明示できる
- render worker が shared mutable state に依存しない

---

## Phase 2: Multi-Frame Scheduler

### 目的

複数フレームを同時に queue へ流せるようにする。

### 作業項目

- render queue に in-flight frame 数の上限を導入する
- CPU / GPU / encode の各段階で backpressure を扱う
- 進捗表示が frame 単位でも破綻しないようにする
- cancel / resume の扱いを frame-safe にする

### 完了条件

- 少なくとも 2 フレーム以上が同時進行できる
- キャンセル時に中途半端な frame state が残らない

---

## Phase 3: Cache and Memory Boundaries

### 目的

MFR で増えやすいメモリ使用量を抑える。

### 作業項目

- frame-local temp buffer の lifetime を管理する
- surface / texture cache の eviction ルールを明確にする
- readback 後の buffer を早めに解放できるようにする
- backend ごとの同時実行数を制限する

### 完了条件

- 同時フレーム数が増えてもメモリが暴れない
- cache hit と memory limit の両立ができる

---

## Phase 4: Diagnostics and Opt-in Rollout

### 目的

MFR を安全に使えるようにする。

### 作業項目

- render queue UI に MFR 有効化スイッチを追加する
- in-flight frame 数と backlog を表示する
- backend 別の推奨並列度を表示する
- 問題が出たら 1-frame mode に落とせるようにする

### 完了条件

- MFR の有効 / 無効を UI で切り替えられる
- 不具合時に直列モードへ戻せる

---

## Non-Goals

- live preview の全面並列化
- frame 間依存を持つ特殊 effect の自動最適化
- 分散レンダリング基盤
- レンダリング結果の非決定性を許容する設計

---

## Related

- `docs/planned/MILESTONE_RENDER_QUEUE_GPU_BACKEND_2026-04-03.md`
- `docs/planned/MILESTONE_INTEGRATED_RENDERING_ENGINE_2026-03-28.md`
- `docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md`
- `docs/planned/MILESTONE_RENDERING_PERFORMANCE_2026-03-28.md`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/Image/FFmpegEncoder.cppm`

## Current Status

2026-04-09 時点では未着手。  
この milestone は、Render Queue の export スループットを上げるための段階導入計画として扱う。
