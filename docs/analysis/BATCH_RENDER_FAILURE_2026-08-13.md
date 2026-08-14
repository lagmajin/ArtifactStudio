# バッチレンダリング 失敗原因の検証

**最終更新:** 2026-08-13

バッチレンダリング（複数コンポジション一括 / MFR マルチフレームレンダリング）が期待どおり動かない原因を、ソースコード（`.ixx` / `.cppm`）を一次情報として検証した。

## 結論（要約）

並列化の骨組み（MFR Dispatcher / Farm / 出力バッファリング / リトライ）は**実装済み**だが、実行経路で**意図的に無効化**され、さらに共有状態のミューテックスで**実質 1 フレームずつ直列化**されている。加えて `ArtifactBatchRenderer` 側に未初期化変数・空実装のバグが重なっている。

---

## 1. 決定的: MFR が `useMfr = false` で無効化

`Artifact/src/Render/ArtifactRenderQueueService.cppm:6531-6537`:

```cpp
// A frame render mutates the shared composition and layer state. Do not
// dispatch this queue path through MFR/Farm until each worker owns an
// isolated composition snapshot and renderer.
const bool useMfr = false;
const int numWorkers = useMfr
    ? std::max(1, std::min(maxInFlightFrames_, totalFrames))
    : 1;
```

- `ArtifactCore::Render::MFR::MFRDispatcher`（`MFRDispatcher.ixx`）は `executeBlocking` でフレーム並列・リトライ・キャンセル・メモリ制限・進捗集計まで実装済み。
- `ArtifactCore::src::Render::RendererQueueManager.cppm:117-143` には MFRDispatcher を呼ぶコードが別に存在する。
- しかし `ArtifactRenderQueueService` の本線（EncodeSession / フレーム描画）では `useMfr = false` 固定で、**MFR も Farm もバイパスされ、`numWorkers = 1` の単一ワーカー**で回る。

---

## 2. `compositionFrameStateMutex_` がフレーム全体を直列化

`ArtifactRenderQueueService.cppm:6316-6326`:

```cpp
bool ...::renderSingleFrame(const FrameRenderSnapshot& snap, ...) {
    ...
    // goToFrame() updates shared mutable state (FramePosition, layers, and
    // PhysicsSystem). A single job snapshot is intentionally shared by the
    // scheduler, so serialise its render-state transition and consumption.
    std::lock_guard<std::mutex> frameStateLock(compositionFrameStateMutex_);
```

- このロックは `renderSingleFrame` の**冒頭で取得され、関数全体を覆う**。
- 内部では `goToFrame()` → `evaluateLayerComponentSimulation()` → レイヤー描画 → `readbackToImage()` / `readbackToMultiChannelImage()` まで**すべてがクリティカルセクション内**。
- コメントは「出力バッファリング/エンコードはこの外」と書くが、実際のレンダリング本体は全てロック内。
- 仮にワーカー数を増やしても、このミューテックスで **1 フレームずつしか進まない**。

---

## 3. 全ワーカーが共有コンポジション/レイヤー状態を共有

- `baseSnap.composition = compositionForRender` を全フレームで共有（`:6541`）。
- 各フレームの `renderSingleFrame` が同一コンポジションに対して `goToFrame(snap.frameNumber)` を呼び、レイヤー状態・物理状態を書き換える。
- これが「各ワーカーが分離されたコンポジションスナップショットとレンダラーを所有するまで MFR を呼べない」というコメントの根本理由。

### 関連する既存資産（未活用）
- `cloneCompositionSnapshot`（`ArtifactRenderQueueService.cppm:3754` 付近）は存在するが、**JSON 経由で重い**。
- `ArtifactRenderScheduler`（`Artifact/src/Render/ArtifactRenderScheduler.cppm`）は優先度・キャンセル・進捗・重複排除・スレッド数適応を実装済みだが、レンダーキューから**未使用**（`PERFORMANCE_ASYNC_GPU_OPTIMIZATION_2026-08-06.md` の AM 項目と一致）。

---

## 4. `ArtifactBatchRenderer` 自体のバグ（付随）

`Artifact/src/Render/ArtifactBatchRenderer.cppm`:

### 4.1 フレーム範囲取得が no-op
```cpp
// :132-135
int startF = 0, endF = 1;
if (queue->jobFrameRangeAt(compIndex, &startF, &endF)) {
    // Already set by addRenderQueueForComposition
}
```
空の if 文。フレーム範囲を取得しても何もしていない。

### 4.2 未初期化変数の参照
```cpp
// :217-223
QString outFmt, codec, codecProfile;   // 未初期化
int w, h, bitrate;                      // 未初期化
double fps;                             // 未初期化
QString useFormat = tmpl.format.isEmpty() ? outFmt : tmpl.format;
...
int useBitrate = tmpl.overrideBitrate > 0 ? tmpl.overrideBitrate : bitrate;
```
`jobOutputSettingsAt` が失敗すると `useFormat` / `useCodec` / `useProfile` / `useBitrate` に未初期化値が入る。

### 4.3 シグナルが空実装
```cpp
// :245-247
void ArtifactBatchRenderer::batchJobsAdded(int) { }
```
`batchJobsAdded` を発火しても何も起きない。

### 4.4 プリセットがハードコード
- `addCompositions` は `h264_mp4_standard` に固定（`:120`、`:127`）。
- `addAllCompositions` → `addCompositions` の流れで、テンプレートの柔軟性が活かされていない。

---

## 失敗の構造（まとめ）

| 層 | 状態 | 影響 |
|---|---|---|
| MFR Dispatcher | 実装済みだが `useMfr=false` で無効 | フレーム並列が全く効かない |
| Farm | 実装済みだが `useMfr=false` で無効 | ファーム配信が実行されない |
| `compositionFrameStateMutex_` | フレーム全体を直列化 | ワーカーが増えても 1 フレームずつ |
| 共有コンポジション状態 | スナップショット分離未完成 | 並列化の前提が崩れている |
| `ArtifactBatchRenderer` | 未初期化変数・空実装・ハードコード | バッチ追加ロジックが不安定 |

**主因**: 「MFR/Farm の並列基盤は実装済みなのに、共有状態の分離が未完成で `useMfr=false` に固定され、さらにミューテックスで 1 フレームずつ直列化」されている。バッチはキュー追加の骨組みだけで、実行が単一ワーカー + 全体ロックで直列になり、`ArtifactBatchRenderer` 側のバグも重なる。

---

## 改善の方向（未実装）

1. **コンポジションスナップショットの分離**: `cloneCompositionSnapshot` をワーカー数ぶん用意し、per-worker の独立したコンポジション/レイヤー/レンダラー状態にする。JSON 経由の重さを解消するため、メモリ上コピー（COW）または render-state のみの軽量スナップショットを検討。
2. **`useMfr` の再有効化**: 分離が完了したら `useMfr = true` に戻し、`MFRDispatcher` に `frameTask` を渡してフレーム並列を有効化。
3. **ミューテックスの縮小**: `compositionFrameStateMutex_` を「状態遷移（goToFrame / evaluate）のみ」に限定し、レンダリング・readback をロック外へ。per-worker 分離後はミューテックス自体が不要になる。
4. **`ArtifactBatchRenderer` の修正**: 未初期化変数の初期化、`batchJobsAdded` の実体化、フレーム範囲の適用、プリセットのハードコード解消。
5. **`ArtifactRenderScheduler` の接続**: 実装済みの優先度ジョブキューをレンダーキューへ接続し、既存機能を活用。

---

## 主要ファイル

| ファイル | 役割 |
|---|---|
| `Artifact/src/Render/ArtifactRenderQueueService.cppm:6316-6326` | フレーム全体を覆うミューテックス |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm:6531-6537` | `useMfr=false` 固定 |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm:6539-6552` | 共有スナップショット |
| `Artifact/src/Render/ArtifactBatchRenderer.cppm` | バッチ追加ロジック（バグあり） |
| `ArtifactCore/include/Render/MFR/MFRDispatcher.ixx` | MFR ディスパッチャ（実装済み・未接続） |
| `ArtifactCore/src/Render/RendererQueueManager.cppm:117-143` | MFR 呼び出し（別経路、本線で未使用） |
| `Artifact/src/Render/ArtifactRenderScheduler.cppm` | 優先度ジョブキュー（実装済み・未使用） |
