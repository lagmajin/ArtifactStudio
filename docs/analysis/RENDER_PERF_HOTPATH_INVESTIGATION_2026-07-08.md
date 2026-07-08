# Rendering Performance Hot-Path Investigation (2026-07-08)

> 状態: 調査（実機プロファイル未実施・静的ソーストレースベース）
> スコープ: インタラクティブ描画の per-frame パス `ArtifactCompositionViewDrawing.cppm::drawLayerForCompositionView`
> 関連: `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md`（grep ヒューリスティクス版）

---

## 1. 調査の動機

既存の 2026-06-16 パフォーマンスレポートは grep ベースのヒューリスティクス（"QImage 38 hit" 等）であり、実際の per-frame 制御フローを追っていない。本調査は `drawLayerForCompositionView`（`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:684`）の実体を読み、**今のコードで本当に毎フレーム重い箇所**を特定する。

## 2. 調査したホットパス

1 フレーム = N レイヤー × `drawLayerForCompositionView` 呼び出し。各レイヤーは `applySurfaceAndDraw` ラムダ（`:718`）を通り、さらに以下の型別分岐を持つ:
- Solid2D / SolidImage / Image / Svg / Video / Text / Shape / Particle / FormParticle / Adjustment / ChildComp / 汎用 `layer->draw()`。

キャッシュ基盤自体は存在する（`LayerSurfaceCacheEntry` / `StaticLayerGpuCacheEntry` / `GPUTextureCacheManager`、`GPUTextureCacheManager` 有効時は SRV バインドで再アップロード回避）。問題は「キャッシュ判定前」と「キャッシュ外パス」にある。

## 3. ボトルネック発見（file:line 付き）

### B1. キャッシュキー `buildLayerSurfaceCacheKey` の毎フレーム無条件構築【重大】

- 呼び出し: `applySurfaceAndDraw` 冒頭 `:724` — 全レイヤー型で毎フレーム呼ばれる。
- 実装: `:234-344`。`QString` を `layer->id()` + 複数 `QString::arg(...)` で結合。
  - **Text レイヤー（:304-333）**: 27 フィールドを連結。本文 `textLayer->text().toQString()`、フォント名、`rgbaKey(...)`×3 等、全て `QString::arg` 経由で毎フレーム再構築 → 多段 alloc。
  - **SolidImage（:256-273）**: 12 フィールド、`rgbaKey`×3 + 浮動小数点 `arg` 多数。
- 致命な点: キーは **`surfaceCache` が null でも `allowSurfaceCache=false` でも常に構築される**。調整レイヤー経路（`:1246`）や Static GPU キャッシュのみを使うケースでも毎フレーム作られる。
- 静的レイヤー（内容不変）は署名が同一なのに毎フレーム再構築 → 純粋な無駄。

**修正案**:
- キー構築を「実際にキャッシュを引く場合」のみに遅延（早期 return でスキップ）。
- 署名をレイヤー側に保持し、プロパティ変更時にだけ無効化（dirty フラグ）。Text/SolidImage は特に効果大。

### B2. 調整レイヤーの毎フレーム GPU→CPU 全画面 readback【重大】

- `:1240-1248`（`isAdjustmentLayer()` 分岐）: `QImage background = renderer->readbackToImage();`
- 現在の描画ターゲット（RTV）全体を CPU へ readback して「背面」を取得し、エフェクトを適用。
- `ArtifactIRenderer::readbackToImage`（`ArtifactIRenderer.cppm:1308`）は `readbackTextureViewToImage` → mutex 取得 + ステージングコピー + Map。フレームごとに**完全同期ストール**を引く。
- 既存レポートの「Readback 42 hit」のうち、ここがインタラクティブ描画で最も重い 1 箇所。

**修正案**:
- 調整レイヤー用に専用のオフスクリーン RT を用意し、背面を別テクスチャとして保持（readback 不要化）。
- どうしても CPU 側で処理する場合は非同期 readback リング（`readbackToImageAsync`、`:1726`）を使い、次フレームで適用。

### B3. `sceneLights` リフトが全面 per-pixel CPU ループ【重大】

- `:815-834`: `sceneLights` ありかつ非 3D レイヤーの場合、`surface.convertToFormat(...)` + 幅×高さのネストループでピクセルごと明度補正。
- レイヤー解除像サイズ W×H で O(W·H) の CPU 処理が**毎フレーム**発生。大きなレイヤーほど致命的。
- さらに `convertToFormat(ARGB32_Premultiplied)` で追加コピー。

**修正案**:
- このリフトは乗算シェーダ（または 1 回の全体スプライト描画で行う簡易トーンオペレーション）へ GPU 化。
- 最低限、静止レイヤーはキャッシュヒット時にこのループをスキップ。

### B4. キャッシュミス / 非 GPU キャッシュパスでの `ImageF32x4_RGBA → QImage` ラウンドトリップ【中】

- キャッシュミス時（`:750-758`）: `buildRasterizedSurfaceBuffer` で F32 バッファ構築 → `processed.toQImage()` で QImage 化。
- 非 GPU キャッシュパスでは `processedSurface` に **QImage（sRGB）** を格納し、描画は `drawSpriteTransformed(surface)`（`:871`）で毎フレーム QImage を GPU へ再アップロード（永続テクスチャなし）。
- `RENDER_FORMAT_CONTRACT` に反し、F32（linear）→ QImage（sRGB uint8）変換 + 再アップロードの 2 重コスト。

**修正案**:
- `layerUsesGpuTextureCacheForCompositionView` が false / `gpuTextureCacheManager` null の環境でも、F32 バッファから直接 GPU テクスチャを作りキャッシュ（既存 `GPUTextureCacheManager` を常時有効化）。
- キャッシュヒット時は `toQImage()` を一切通さず SRV バインドで描画。

### B5. キャッシュキー比較前の不要な `processedBuffer` 構築【軽度】

- `:750-758`: キャッシュミス分岐内で `buildRasterizedSurfaceBuffer` を毎回呼ぶが、GPU キャッシュ有効時は `processedBuffer`（F32）を作ってから `acquireOrCreate` に渡す。F32 構築自体は必要だが、QImage 化（`:756`）は GPU パスでは不要 → 分岐で除外済み（`:755`）。ここはむしろ「QImage 化を GPU パスでやらない」設計は既に入っているので軽度。

## 4. 重大度まとめ

| ID | 箇所 | 毎フレームコスト | 影響 |
|----|------|------------------|------|
| B1 | `buildLayerSurfaceCacheKey` 無条件構築（:724 / :234） | レイヤー数 × QString 多段 alloc（Text は 27 フィールド） | GC 圧・フレーム時間 |
| B2 | 調整レイヤー `readbackToImage`（:1244） | 全画面 GPU→CPU 同期 | 同期ストール・FPS 崩壊 |
| B3 | `sceneLights` 全面 CPU ループ（:818） | O(W·H) per lit layer | 大レイヤーで深刻 |
| B4 | 非 GPU パス `toQImage` ラウンドトリップ + 毎フレーム再アップロード（:756 / :871） | F32→sRGB 変換 + upload | 帯域・GPU 負荷 |

B1/B3 は「キャッシュ判定前」に発生するため、**キャッシュが効いていても常に払われる**コスト。ここが既存レポートが見落としていた最大の差。

## 5. 推奨調査順（確定の早道）

1. **B1**: Text レイヤー 1 つだけ置いた comp で、`drawLayerForCompositionView` 呼び出し時の `QString` アロケーション数を計測（Alloc トレース / `qCountAllAllocation` 等）。Text 署名構築が毎フレーム数十 alloc になるはず。
2. **B2**: 調整レイヤーを 1 つ含む comp で、フレームごとの `readbackToImage` 呼び出し回数と所要時間をログ。
3. **B3**: `sceneLights` あり + 大きなレイヤーで per-pixel ループの CPU 時間を計測。

## 6. 関連ファイル

- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` — `drawLayerForCompositionView` / `applySurfaceAndDraw` / `buildLayerSurfaceCacheKey`
- `Artifact/src/Render/ArtifactIRenderer.cppm` — `readbackToImage` / `readbackToImageAsync`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm` — オフラインレンダパス（readback 多用、別スコープ）
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` — 既存 grep ベース調査（本報告の補完）

## 7. 注意

- 本調査は静的解析。実際のフレーム落ち箇所はプロファイラ（Nsight / GPA / Tracy）での測定が最終確定に必要。
- サブモジュール境界: `Artifact/src` のみを対象。該当修正は Artifact 側で完結する見込み。
