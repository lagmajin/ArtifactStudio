**ステータス:** Not Started

# M-VIDEO-QIR: Video QImage Retirement — Completion 設計マイルストーン

`MILESTONE_VIDEO_QIMAGE_RETIREMENT_2026-04-15.md`（In Progress）の**完遂（finish line）**に絞った設計。
元マイルストンの WP-1/2/3/6 はコード上実質完了しているため、ここでは**未完了の WP-4(CPU 系) + WP-5 + WP-7** のみを対象とする。重複を避け、追加スコープのみを定義する。

## 現状把握（コード実態、元マイルストンより進んでいる）

- `currentQImage_` はコードベースから消滅（grep 0 件）。動画レイヤーの正フレームバッファは `ImageF32x4_RGBA currentFrameBuffer_`。
- `VideoFrame.ixx` に `CpuVideoFrame` / `GpuVideoFrame` / `DecodedVideoFrame` variant + `VideoFrameColorInfo{colorSpace,colorRange,colorPrimaries,colorTransfer}` が存在（元マイルストンの `VideoFrameSurface` はこの名前で実装済み）。
- `MediaPlaybackController` は raw-first エントリ（`getVideoFrameAtFrameDirectRaw` 等）を提供。`ArtifactVideoLayer::draw()` は `ImageF32x4_RGBA` を直接 `renderer->drawSpriteTransformed(...)` に渡す。
- GPU/Vulkan アップロード経路は `GPUTextureCacheManager::acquireOrCreate(..., GpuVideoFrame)` で接続済み（`ArtifactCompositionViewDrawing.cppm:1043-1078` の GPU ブランチは `toQImage()` を通らない）。

### 未完了（本マイルストンの対象）

| 項目 | 状態 | 証拠 |
|---|---|---|
| WP-4 (CPU 系) | 🟡 部分 | `ArtifactCompositionViewDrawing.cppm:~1097` の CPU fallback が `frameBuffer.toQImage()` → `downsampleForLOD` → `applySurfaceAndDraw`。`ArtifactPreviewCompositionPipeline.cppm:371` が `videoLayer->currentFrameToQImage()` を live パスで呼ぶ |
| WP-5 Color-Managed | ❌ 未着手 | `FFMpegVideoDecoder::makeCpuVideoFrameFromFrame()` が `meta.color` を一切埋めない。`AVFrame` の `color_range`/`colorspace`/`color_primaries`/`color_trc` を読むコードなし。`VideoFrameColorInfo` が decode 時に populate されない |
| WP-7 QImage Retirement | ❌ 未達 | 上記 CPU fallback により「hot path が QImage を要しない」という検証項目が未達。`toQImage()` が active パスに残る |

注: `docs/MILESTONE_ANALYSIS_FINAL_2026-04-27.md` の「NO IMPLEMENTATION YET」や 2026-04-27 ロードマップの「blocked」は**既に陳腐**。元マイルストンの `Current State`/`Progress Notes`（最終 2026-04-30、まだ `currentQImage_` に言及）もコードと乖離。本マイルストン完了時に元ドキュメントのステータスを `✅ Complete` へ更新すること。

## アーキテクチャ決定

- **GPU ブランチは完成済みとする。** 新規実装は CPU fallback 経路のみ。
- **CPU パスでも `ImageF32x4_RGBA` を正フレーム型として維持し、`toQImage()` を排除する。** `DecodedVideoFrame` → `ImageF32x4_RGBA` 変換は既に `decodedVideoFrameToImageF32x4_RGBA()` が存在するので、それを CPU fallback でも使う。
- **カラーメタは decode 時に一度だけ `AVFrame` から `VideoFrameColorInfo` へ populate** し、以降レイヤ/レンダラ/エクスポートで保持。変換は暗黙にせず explicit ヘルパを通す（AGENTS ルール: QImage 化/ダウンロード/アップロードは明示関数）。

## 実装フェーズ

### P0: CPU fallback の QImage 排除（WP-4 CPU 系 + WP-7 の根幹）
- `ArtifactCompositionViewDrawing.cppm:~1097` の `frameBuffer.toQImage()` → `downsampleForLOD` を、`ImageF32x4_RGBA` を直接 LOD ダウンサンプルするヘルパに置換（`applySurfaceAndDraw` が `ImageF32x4_RGBA` を受けられるか確認し、受けられない場合は `applySurfaceAndDraw` 側の入力型を拡張）。
- `ArtifactPreviewCompositionPipeline.cppm:371` の `currentFrameToQImage()` を `currentFrameBuffer()`（または `decodedVideoFrameToImageF32x4_RGBA`）へ置換。
- 完了条件: コンポジション描画・プレビュー双方の live パスで `QImage` 変換が消え、`toQImage()` は debug/export 用 shim のみに残る。

### P1: カラーメタの decode 時 populate（WP-5 の土台）
- `ArtifactCore/src/Codec/FFMpegVideoDecoder.cppm` の `makeCpuVideoFrameFromFrame()` で `AVFrame` から `color_range` / `colorspace` / `color_primaries` / `color_trc` を読み `meta.color` を埋める。
- `DecodedVideoFrame` → `ImageF32x4_RGBA` 変換（`decodedVideoFrameToImageF32x4_RGBA`）時に `VideoFrameColorInfo` を伝搬し、フレームバッファに色メタを付随させる。
- 完了条件: デコード直後にソースカラータグが `VideoFrameColorInfo` として存在し、以降の変換で落ちない。

### P2: explicit input→working→output ポリシー（WP-5 本質）
- コンポジタの作業空間（working space）を定義し、ビデオ入力→working のリニア化ポリシーを決める。
- 既存の `ArtifactColorManagement` / `ArtifactOCIOManager` / `ColorSpace` 変換を、デコードソースタグを尊重するよう接続。wide-gamut（Display P3 / Rec.2020）と full/limited range を保持。
- SDR / wide-gamut の検証アセットを追加。
- 完了条件: 混在フッテージコンポジションが全ソースを sRGB 8bit と仮定しない。OCIO/ACES 系ワークフローへの橋渡しが動く。

### P3: QImage 退役完了（WP-7 クローズ）
- live パスから `QImage` を完全に排除。残る `toQImage()` / `currentFrameToQImage()` / `decodeFrameToQImage()` は debug snapshot export / 未移行 inspector / 明示的 compatibility shim のみに限定し、コード上で用途を明記。
- 元 `MILESTONE_VIDEO_QIMAGE_RETIREMENT` のステータスを `✅ Complete` へ更新し、陳腐な `Current State`/`Progress Notes` を補正。

## 検証チェックリスト（元マイルストンから引き継ぎ、ここでクローズ）

- インポート動画の最初のフレームがコンポジションに確実に表示される
- タイムラインスクラブが偶発的 repaint なしに可視フレームを更新する
- 再生/プロキシ切替でフレーム同一性とキャッシュ無効化が正しい
- 動画の opacity / transform / mask が描画に影響する
- **ソースカラーメタが decode で落ちず保持される（P1/P2 で新規追加項目）**
- 混在フッテージコンポジションが全ソースを sRGB 8bit と仮定しない
- **hot path が通常の動画描画で QImage を要しない（P0/P3 で新規追加項目）**

## リスク・未確認

- `applySurfaceAndDraw` が `ImageF32x4_RGBA` を直接受けられるか、あるいは `QImage` 型に依存しているか確認必須（P0 の作業量に直結）。
- `downsampleForLOD` の入力型。CPU パスでも `ImageF32x4_RGBA` 向けの LOD ダウンサンプルが必要（既存は `QImage` 向けかも）。
- OCIO/ACES 接続は `ArtifactColorManagement` の既存 API 依存量が高い。P2 は既存システムの調査から。
- Vulkan HW デコード（GpuVideoFrame）のカラーメタ伝搬は CPU パスとは別経路。`meta.color` populate は CPU デコード側のみで確認済み、GPU 側は別途。

## 次のステップ

1. P0 から始める（CPU fallback の QImage 排除が WP-7 達成の前提）。
2. P1 でカラーメタ populate（WP-5 の土台、最も「不足」している部分）。
3. P2 で explicit ポリシーと OCIO/ACES 接続。
4. P3 で退役完了＋元マイルストンステータス更新。
