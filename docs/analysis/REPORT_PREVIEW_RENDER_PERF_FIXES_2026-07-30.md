# 🔴 プレビューレンダリング パフォーマンス要修正レポート

**調査日:** 2026-07-30
**対象:** 画像レイヤー / シェイプレイヤー / 音声レイヤー（プレビュー表示時）
**深刻度:** 🔴 Critical — プレビュー操作の体感遅延に直結

---

## 修正優先順位サマリー

| 優先度 | ID | 問題 | 影響 | 修正難度 |
|--------|-----|------|------|----------|
| 🔴 S | B2 | 調整レイヤー full-screen readback 同期ストール | FPS崩壊 | 中 |
| 🔴 S | FX-3 | 16個のGPUエフェクトが毎フレーム `GpuContext` 再生成 | 16 alloc/frame | 低 |
| 🔴 S | #1 | エフェクト適用時の QImage→OpenCV→float→QImage CPUラウンドトリップ | 4K時64MB確保+コピー/frame | 中 |
| 🔴 S | #4 | シェイプレイヤー三角分割キャッシュなし | CPUジオメトリ再計算毎frame | 低 |
| 🟡 A | B1 | `buildLayerSurfaceCacheKey` 全レイヤー無条件実行 | 毎frame文字列連結 | 低 |
| 🟡 A | B3 | `sceneLights` ピクセル単位CPUループ | O(W×H) loop/frame | 低 |
| 🟡 A | — | レンダーパス内のディスクI/O 35箇所 | 毎frame stat呼出 | 低 |
| 🟡 A | — | ダーティフラグがバイナリ（全再描画 or 無視） | 不要な全再レンダリング | 中 |
| 🟢 B | FX-5/6 | エフェクトチェーン readback chain 未解体 | N効果 = N回 GPU drain | 高 |
| 🟢 B | FX-4 | グローバルPSOキャッシュ不在 | D3DCompile毎回 | 中 |

---

## 🔴 S-Severity（即時修正候補）

### B2: 調整レイヤー full-screen GPU→CPU readback 同期ストール

- **ファイル:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:1240-1248`
- **現象:** 調整レイヤーが毎フレーム `readbackToImage()` を発行し、GPU パイプラインが `WaitForIdle` で完全停止。1回の readback が 2-8ms の同期ストールを発生させる。
- **修正方針:** 調整レイヤーの背景をオフスクリーンテクスチャ参照にし、readback を廃止（`MILESTONE_INTERACTIVE_RENDER_PERFORMANCE` RP-2 相当）。
- **関連マイルストーン:** `Artifact/docs/MILESTONE_INTERACTIVE_RENDER_PERFORMANCE_2026-07-27.md` RP-2

---

### FX-3: GPU エフェクトの毎フレーム GpuContext 再生成

- **ファイル:** `*Effect.cppm`（Blur, Brightness, ColorBalance, Exposure, Bevel, Kaleidoscope 他 全16ファイル）
- **現象:** 各エフェクトの `apply()` 内で `auto ctx = std::make_unique<GpuContext>(...)` が毎フレーム実行され、GPU オブジェクト（PSO, SRV, UAV）が使い捨てられている。
- **修正方針:** エフェクトインスタンスごとに `GpuContext` をメンバー保持し、入力テクスチャ変更時のみ再生成。またはリングバッファで再利用。
- **関連マイルストーン:** `Artifact/docs/MILESTONE_GPU_EFFECT_PERF_FIXES_2026-07-22.md` FX-3

---

### #1: エフェクト適用時の CPU ラウンドトリップ

- **ファイル:** `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`（ラスタライザーエフェクトパス）
- **現象:**
  ```
  QImage → cv::Mat（8bitコピー）→ convertTo(CV_32FC4)（float変換）
  → ImageF32x4_RGBA（コピー）→ ImageF32x4RGBAWithCache（エフェクト毎に確保）
  → effect->applyConfigured() → toQImage()（float→8bit変換）
  → drawSpriteTransformed()（GPU再アップロード）
  ```
  4K レイヤーで約 64MB の確保+コピーがフレームごとに発生。バッファプーリングなし。
- **修正方針:**
  - 短期: `ImageF32x4_RGBA` のプーリング導入
  - 中期: エフェクトを GPU 常駐化（RP-2）し CPU パス廃止
- **関連マイルストーン:** RP-2, RP-3

---

### #4: シェイプレイヤー三角分割キャッシュ不在

- **ファイル:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- **現象:** GPU ネイティブ描画パスを使っているにも関わらず、`path.triangulate(0.25/renderScale)` と `path.flattenSubpaths(0.25/renderScale)` が毎フレーム CPU 上で実行される。複雑な Shape ほど負荷が線形増加。
- **修正方針:** 三角分割結果をレイヤーのレンダーキャッシュに保存し、パス変更時のみ再計算。
- **関連マイルストーン:** `Artifact/docs/MILESTONE_SHAPE_PATH_NATIVE_RENDER_PIPELINE_2026-07-27.md`

---

## 🟡 A-Severity（次スプリント）

### B1: `buildLayerSurfaceCacheKey` 無条件実行

- **ファイル:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:234-344, :724`
- **現象:** 全可視レイヤーに対し毎フレームキャッシュキーを構築。Text レイヤーは 27 フィールドを `QString::arg()` 連結。`surfaceCache` が null の時も実行。
- **修正方針:** `surfaceCache` 有効時のみ遅延構築。またはレイヤー側で dirty 時のみキャッシュキーを更新。

---

### B3: `sceneLights` ピクセル単位 CPU ループ

- **ファイル:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:815-834`
- **現象:** 非 3D レイヤーに対しても O(W×H) のネストループ + `convertToFormat()` が毎フレーム実行。
- **修正方針:** 3D レイヤー以外では早期 return。

---

### ディスク I/O（レンダーパス内 35 箇所）

- **該当:** `ArtifactVideoLayer`, `ArtifactFrameCache`, `ArtifactProjectService` 他
- **現象:** `QFileInfo::exists()` がレンダーパス内で呼ばれている。ファイルシステム stat は OS キャッシュヒット時でも 1-5µs のレイテンシ。
- **修正方針:** ファイル存在確認をレイヤー状態としてキャッシュし、ファイル監視で更新。

---

### ダーティフラグの粒度不足

- **ファイル:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（約 27,000 行）
- **現象:** `markRenderDirty()` がバイナリフラグ。プロパティ変更があると常に全レイヤー全再描画。`LayerDirtyFlag` は存在するが、一部の軽量変更（Transform/Property のみ）でキャッシュスキップするパスがあるのみで、真の部分更新は未実装。
- **修正方針:** ダーティ分類（Content/Effect/Mask/Transform/Opacity/OverlayOnly）に応じた部分再合成（`MILESTONE_INTERACTIVE_RENDER_PERFORMANCE` RP-3 相当）。

---

## 🟢 B-Severity（継続改善）

### FX-5/6: エフェクトチェーン readback chain 未解体

- **現象:** GPU エフェクトのチェーンが `Flush → WaitForIdle → Map → CPU image → 再アップロード` のパターンを連鎖的に繰り返す。N 個のエフェクトで N 回の GPU drain + 33MB+ のコピー往復。
- **修正方針:** ピンポンテクスチャによる GPU 常駐チェーン（RP-2）+ readback の完全廃止（FX-6）。

### FX-4: グローバル PSO キャッシュ不在

- **現象:** HLSL のランタイムコンパイル（`D3DCompile`）がエフェクト追加のたびにレンダースレッドをブロック。
- **修正方針:** アプリケーション起動時にまとめてコンパイルするか、コンパイル済みキャッシュを使用。

---

## 既存マイルストーンとの対応

| 本レポート ID | 既存マイルストーン | 状態 |
|---------------|-------------------|------|
| B2 | RP-2 GPU-Resident Effect Chain | 未着手 |
| #1 | RP-2 + RP-3 Persistent Layer Results | 未着手 |
| FX-3 | FX-3 GPU Object Reuse (31 effects) | 未着手 |
| #4 | ShapePath Native Render Pipeline | 一部完了 |
| B1 | MILESTONE_COMPOSITION_EDITOR_PERFORMANCE | キャッシュ機構は完了、構築最適化未着手 |
| ダーティ粒度 | RP-3 Persistent Layer Results & Partial Recompose | 未着手 |
| FX-5/6 | FX-5 Readback Chain Dismantle / FX-6 Eliminate Sync Readback | 未着手 |
| FX-4 | FX-4 Global PSO Cache | 未着手 |

---

## レンダーパス構成（参考）

```
User inputs
    │
    ▼
LayerChangedEvent
    │
    ▼
CompositionRenderController::onLayerChanged()
  ├─ Created/Removed → 全キャッシュクリア
  ├─ Property/Transform → 軽量パス（キャッシュ非破棄）
  └─ Effect/Mask/Source → surfaceCache 破棄
    │
    ▼
markRenderDirty() → renderTickDriver_ (~16ms)
    │
    ▼
renderOneFrameImpl():
  1. コンポジション解決
  2. プロジェクト健全性チェック
  3. 可視レイヤーイテレーション
     └─ drawLayerForCompositionView() [1400行+]
        ├─ surfaceCache ルックアップ（キー構築含む）
        ├─ エフェクト/マスク適用（CPUラウンドトリップ）
        ├─ GPUテクスチャキャッシュ → アップロード
        └─ Diligent/D3D12 描画
  4. ギズモ・オーバーレイ描画
  5. renderer_->present()
```
