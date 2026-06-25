# マイルストーン: VideoLayer Playback Stability

> 2026-06-06 作成
> **コードレビュー元:** `ArtifactVideoLayer.cppm` (Artifact 子リポジトリ)

## 概要

`ArtifactVideoLayer` の再生（play → pause → stop）が不安定で、特に停止後の再開やシーク後のフレーム表示にゴーストフレーム・フリーズが発生する。  
これは **非同期デコードパイプラインに stop / cancel / reset の概念が存在しない** ことが根本原因。

**優先度:** 🔴 高 — ユーザーが触ってすぐ気づく操作感の欠陥

**関連ファイル:**
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- `Artifact/include/Layer/ArtifactVideoLayer.ixx`

**依存:**
- なし（Artifact 子リポジトリ内で完結）

---

## 完了条件

1. `ArtifactVideoLayer::stop()` を新設し、非同期デコードのキャンセル・フレームバッファクリア・状態リセットが行われる
2. デコード世代管理（generation counter）を導入し、古い非同期デコード結果が後から反映されない
3. `currentFrameImageBuffer()` のフレームアドプション・デコード発火条件が明快で、描画スレッドのブロッキングがない
4. `seekToFrame()` と `decodeCurrentFrame()` の間でフレーム不整合が発生しない
5. 上記 4 点が単体テスト（モックコントローラ）で検証可能

---

## Phase 構成

### Phase 1: stop() の実装（2h）

**目的:** 停止操作でデコード状態を完全にリセットする

- `void ArtifactVideoLayer::stop()` の追加
  - `decodeFuture_.cancel()` — 未実行の非同期タスクを破棄
  - `decoding_ = false`
  - `decodeTargetFrame_ = -1`
  - `currentFrameBuffer_` を空に（`ImageF32x4_RGBA()`）
  - `hasCurrentFrameBuffer_ = false`
  - `lastDecodedFrame_ = -1`
  - `frameCache_.clear()`
  - 音声バッファのクリア（`audioBufferL_`, `audioBufferR_`）
  - `impl_->playbackController_->stop()` を呼ぶ（存在すれば）
  - `lastDecodeState_ = "stopped"`

**影響範囲:** `ArtifactVideoLayer.cppm` の Impl クラス + public メソッド 1 つ

---

### Phase 2: デコード世代管理（2h）

**目的:** `seekToFrame()` 後に完了した古い非同期デコードが最新フレームを上書きするのを防ぐ

```cpp
// Impl に追加
std::atomic<uint32_t> decodeGeneration_{0};
```

- `decodeCurrentFrame()` 開始時に世代をインクリメント
- 非同期ラムダ内で世代をキャプチャし、完了時に現在の世代と一致する場合のみ `currentFrameBuffer_` を更新
- `stop()` でも世代をインクリメントし、進行中のデコードを無効化

```cpp
// 非同期ラムダ内
const uint32_t capturedGen = impl_->decodeGeneration_.load();
// ...
// 完了処理
if (capturedGen == impl_->decodeGeneration_.load()) {
    // currentFrameBuffer_ を更新
}
```

---

### Phase 3: `currentFrameImageBuffer()` のリファクタ（1.5h）

**目的:** 描画パスでの不要なブロッキングとコピーを削減

- `decodeFuture_.result()` への同期待ちを削除（代わりに `QFuture::isFinished()` + ポーリングで結果を取得）
- `const&` 返しにしてコピーを回避（mutex 保護下での参照返し）
- デコード発火条件を簡略化: 「lastDecodedFrame != sourceFrame && !inFlight」だけにする

---

### Phase 4: `seekToFrame()` と `decodeCurrentFrame()` の同期（1.5h）

**目的:** seek → decode の 2 段階操作を不可分にする

- `seekToFrame()` 内で `decodeCurrentFrame()` を呼ぶ前に `cancelPendingDecode()` を実行
  - `decoding_`, `decodeFuture_`, `decodeTargetFrame_` をリセット
  - 世代カウンタをインクリメント
- これにより、seek → decode の間に古いデコードが割り込む余地をなくす

---

### Phase 5: テスト（1h）

**目的:** 修正の継続的検証を確保

- モック `MediaPlaybackController` を使って:
  - `stop()` → フレームバッファが空になる
  - `seekToFrame(A)` → `seekToFrame(B)` の直後に A のフレームが表示されない
  - `stop()` → `play()` で新しいデコードが始まる
  - 世代管理下で古い非同期デコードが無視される
- テストファイル: `Artifact/test/VideoLayerPlaybackStabilityTest.cppm`

---

## 見積サマリー

| Phase | 内容 | 工数 | 優先度 |
|-------|------|------|--------|
| 1 | `stop()` 実装 | 2h | 🔴 最優先 |
| 2 | デコード世代管理 | 2h | 🔴 最優先 |
| 3 | `currentFrameImageBuffer()` リファクタ | 1.5h | 🟡 中 |
| 4 | seek と decode の同期 | 1.5h | 🔴 高 |
| 5 | テスト | 1h | 🟢 低（ただし品質保証に必要） |
| **計** | | **8h** | |

---

## 関連ドキュメント

- `docs/bugs/VIDEO_LAYER_NOT_DISPLAYING_HYPOTHESES_2026-03-27.md` — 表示不具合の仮説レポート
- `docs/bugs/VIDEO_DECODE_FAILURE_HYPOTHESES_2026-03-23.md` — デコード失敗の仮説レポート
- `docs/bugs/BUG_INVESTIGATION_DILIGENT_VISIBILITY_OPACITY.md` — opacity 未使用バグ
- `docs/planned/MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md` — プロキシ改善（別マイルストーン）
- `docs/planned/MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md` — オーディオ再生安定化（別マイルストーン）
- `docs/planned/MILESTONES_BACKLOG.md` — 全体バックログ

## Completion Note

Core playback stability behavior is documented as complete in `docs/done/MILESTONE_VIDEO_LAYER_PLAYBACK_STABILITY_2026-06-25.md`.
