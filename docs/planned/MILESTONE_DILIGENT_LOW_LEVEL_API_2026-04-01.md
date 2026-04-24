# Milestone: Diligent Low-Level Rendering API Expansion (2026-04-01)

**Status:** Phase 1 ✅ / Phase 2 ✅ / Phase 3 ✅ / Phase 5 ✅ / Phase 7a ✅ / Phase 4・6・7b/c 未着手
**Goal:** `DiligentImmediateSubmitter` の描画 API を拡充し、レンダーステートの最適化を積み重ねながら、長期的に Immediate Context 依存を段階的に削減する

> **更新: 2026-04-25** — Phase 1・2 完了確認、Phase 5 (H1-H5 最適化) 完了反映、Phase 6・7 を新規追加；Phase 7a 実装完了
> **更新: 2026-04-28** — Phase 3 (`SolidRectPkt` バッチ描画) 実装完了

---

## 現状 (2026-04-25 時点)

| カテゴリ | 状態 |
|---------|------|
| **Solid Rect / Sprite / MaskedSprite** | ✅ 実装済み |
| **Line / ThickLine / DotLine / Quad** | ✅ 実装済み |
| **Bezier (2次/3次)** | ✅ 実装済み (CPU セグメント分割) |
| **SolidCircle / SolidTri / Crosshair** | ✅ 実装済み |
| **Checkerboard / Grid** | ✅ 実装済み |
| **RectOutline PSO** | ✅ 実装済み (`sm.outlinePsoAndSrb()` で作成) |
| **テキスト描画 (GlyphText / GlyphTextXform)** | ✅ 実装済み (glyph atlas + 8方向アウトライン) |
| **SpriteXform (QMatrix4x4 変換付き)** | ✅ 実装済み |
| **PSO 管理 (15ペア)** | ✅ 実装済み (TBB並列作成) |
| **静的CB/サンプラー事前バインド (H1)** | ✅ 実装済み |
| **SetRenderTargets 1回化 (H2)** | ✅ 実装済み |
| **PSO 切替デdup (H3)** | ✅ 実装済み |
| **Unit Quad VB (H5)** | ✅ 実装済み |
| **バッチ描画 (SolidRect Phase 3)** | ✅ 実装済み (AA付き、最大512矩形/call) |
| **アップスケール設定 (FSR/RSR)** | ❌ no-op |
| **3D Line thickness** | ❌ 無視されている (`(void)thickness`) |
| **パケットのPSO別ソート (H6)** | ❌ 未実装 |
| **Deferred Context 移行 (Phase 7a)** | ✅ 実装済み |
| **Deferred Context 並列記録 (Phase 7b/c)** | ❌ 未着手 |

---

## Phase 1: Bug Fixes & API Parity ✅ 完了

### 実施内容
1. **`outlinePsoAndSrb_` の PSO 作成** — `createPSOs()` で修正済み、`drawRectOutlineLocal()` 機能
2. **`drawLineLocal()` を公開 API に追加** — `PrimitiveRenderer2D` で公開済み
3. **RectOutline draw path** — `submitRectOutline()` が `submitSolidRect` 4回呼び出しで実装 (outline PSO は用意、H7 スキップ済)

---

## Phase 2: Text Rendering API ✅ 完了

### 実施内容
1. **`drawText()` / `drawTextTransformed()`** — `PrimitiveRenderer2D` に実装済み
2. **Glyph PSO 2種** — `m_draw_glyph_pso_and_srb` (通常) + `m_draw_glyph_transform_pso_and_srb` (行列変換)
3. **GlyphAtlas** — フォント・サイズ・スタイルキーで USAGE_IMMUTABLE テクスチャアトラスをキャッシュ
4. **アウトラインテキスト** — `outlineThickness > 0` のとき 8方向オフセットで outline pass を先行描画
5. **submit 側の実装** — `submitGlyphText()` / `submitGlyphTextTransformed()` 実装済み

### 残留課題
- Glyph atlas が dirty になるたびに `USAGE_IMMUTABLE` テクスチャを全再生成 (頻繁な再生成コスト)
- Subpixel AA / hinting 非対応

---

## Phase 3: Batch Rendering System (SolidRect) ✅ 完了

### 実施内容
1. **`batchSolidRectAAPsoAndSrb_`** — AA付き専用 PSO を `ShaderManager` に追加 (UV + fwidth AA)
2. **動的 VB / 静的 IB** — `DiligentImmediateSubmitter::createBuffers()` で確保 (512矩形分)
3. **CPU 頂点事前計算** — `SolidRectPkt.xform` から NDC 頂点を CPU で積み上げ
4. **バッチフラッシュ戦略** — 他パケット型が来るか、512件超えたら `flushSolidRectBatch()` で1回の `DrawIndexed` 発行
5. **フォールバック** — バッファ未準備時は既存 `submitSolidRect()` を呼ぶ安全パス維持

### 残留課題 (Line / Glyph バッチ)
- Line バッチ (Bezier セグメント一括) は未着手
- Glyph バッチ (同一 atlas のグリフ一括) は未着手

---

## Phase 4: Advanced Features (P2) ❌ 未着手

### 対象
1. **3D Line thickness 対応**
   - `PrimitiveRenderer3D::draw3DLine()` で `(void)thickness` を解消
   - ビルボードクワッドでジオメトリ展開

2. **`setUpscaleConfig()` の実装**
   - FSR / RSR 統合
   - 4K レンダリング時の性能向上

3. **3D ギズモ拡張**
   - Sphere / Cube 基本メッシュ描画
   - 選択ハイライト

4. **Glyph Atlas 差分更新**
   - atlas dirty 時に全テクスチャ再生成ではなく更新領域だけ転送
   - `USAGE_DEFAULT + UpdateTexture` または `USAGE_DYNAMIC` に変更

### 見積: 12-18h

---

## Phase 5: Render State Optimization (H1–H5) ✅ 完了 (2026-04-25)

`DiligentImmediateSubmitter` に対して実施した低レベル最適化。

| 最適化 | 内容 | 効果 |
|--------|------|------|
| **H1** | `setPSOs()` 時に静的CB・サンプラーを SRB に事前バインド。テクスチャ SRV 用 `IShaderResourceVariable*` をキャッシュ | `GetVariableByName()` (ハッシュ検索) を draw 毎に排除 |
| **H2** | `SetRenderTargets` を `submit()` 先頭で1回のみ実施 | 全 submit* 関数から `SetRenderTargets` を削除 |
| **H3** | `m_currentPSO_` で PSO 追跡し、未変化時は `SetPipelineState` をスキップ | submit 開始時にリセット、同 PSO 連続呼び出しは2回目から無コスト |
| **H5** | `m_sprite_unit_quad_vb_` (USAGE_IMMUTABLE) をopacity==1.0f スプライトに使用 | `mapWriteDiscard` による VB 書き換えをスキップ |

### 実装ファイル
- `Artifact/include/Render/DiligentImmediateSubmitter.ixx` (新規フィールド)
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm` (全 submit* 関数)

---

## Phase 6: Draw Order Optimization (P1) ❌ 未着手

### 目的
`submit()` 内でパケットを PSO ごとにソートしてから処理することで、H3 (PSO dedup) の実効ヒット率を最大化する。

### 背景
現在の `submit()` は `packets_` を追加順（描画要求の生成順）に処理する。  
例えば「SolidRect → Sprite → SolidRect → Sprite」と交互に来ると PSO が毎回切り替わる。  
呼び出し元の描画要求順を変えずに submitter 側でソートすれば、PSO スイッチをフレームあたり最小化できる。

### 作業項目
1. `DrawPacket` の PSO インデックスを高速に決定する `psoKey(DrawPacket)` ヘルパー
2. `submit()` 内でパケット列を `psoKey` でソート (安定ソート)
3. ただし **描画順が重要なパケット** (透明/アルファブレンド依存) の保護が必要 — alpha 描画パスを分離するか、Zオーダーキーを組み合わせる

### 注意
- この最適化は **完全に透明度ブレンドが描画順に依存しないパス** (例: ギズモ上書き) には直接適用できる
- コンポジションのレイヤーブレンドが絡む場合は描画順を崩せないため適用範囲を慎重に決める

### 見積: 4-6h

---

## Phase 7: Deferred Context Migration (P2) ✅ Phase 7a 完了 (2026-04-25)

### 目的
`DiligentImmediateSubmitter` が直接 `IDeviceContext*` (Immediate Context) を使っている構造を段階的に `Deferred Context` 経由へ移行し、将来のマルチスレッドコマンド記録を可能にする。

### 背景
- `DiligentDeviceManager` はすでに `deferredContext()` を公開している
- Diligent Engine の Deferred Context は `IDeviceContext` と同一インターフェースだが、コマンドリストに記録されてから後で Immediate Context に flush される
- 現状の `DiligentImmediateSubmitter` はフレームごとに GPU へ直接呼び出しをしているため、将来の並列記録・マルチスレッド対応が難しい

### Phase 7a の実装内容 ✅
1. `m_deferredCtx_` フィールド追加 (`RefCntAutoPtr<IDeviceContext>`)
2. `setDeferredContext()` メソッド追加 — 3 つの init パス (initialize/initializeHeadless/createSwapChain) で呼び出し
3. `submit()` を修正: `Begin(immCtxId)` → コマンド記録 → `FinishCommandList()` → `ExecuteCommandLists()` → `FinishFrame()`
4. `destroy()` で `m_deferredCtx_ = nullptr`
5. `m_deferredCtx_` が null の場合は Immediate Context フォールバック (後方互換)
6. `CommandList.h` を GMF に追加

### 移行ロードマップ

```
現在 (Phase 7a 完了):
  RenderCommandBuffer → DiligentImmediateSubmitter (Deferred Context 記録) → CommandList → Immediate.ExecuteCommandLists()

Phase 7b: Parallel Recording
  複数 RenderCommandBuffer → 複数 DeferredContext (並列スレッド) → Immediate.ExecuteCommandLists()
  ※ 共有状態 (glyph atlas, SRB variable pointers, m_currentPSO_) のスレッド安全化が必要

Phase 7c: Rename & Interface Cleanup
  DiligentImmediateSubmitter → DiligentCommandRecorder に改名
  IRenderSubmitter の submit() シグネチャを deferred-first に変更
```

### 見積: Phase 7b 12-16h / 7c 4h

---

## Recommended Order

| 順序 | フェーズ | 見積 | 優先度 | 状態 |
|---|---|---|--------|------|
| 1 | **Phase 1: Bug Fixes & API Parity** | 2h | P0 | ✅ 完了 |
| 2 | **Phase 2: Text Rendering API** | 8h | P0 | ✅ 完了 |
| 3 | **Phase 5: Render State Optimization (H1–H5)** | — | P0 | ✅ 完了 |
| 4 | **Phase 3: Batch Rendering System** | 10-14h | P1 | ❌ 未着手 |
| 5 | **Phase 6: Draw Order Optimization** | 4-6h | P1 | ❌ 未着手 |
| 6 | **Phase 4: Advanced Features** | 12-18h | P2 | ❌ 未着手 |
| 7 | **Phase 7a: Deferred Context Recording** | 6-8h | P2 | ✅ 完了 |
| 8 | **Phase 7b/7c: Parallel Recording & Rename** | 16-20h | P3 | ❌ 未着手 |

**残見積: ~50-66h**

---

## 関連ファイル

| ファイル | 内容 |
|---------|------|
| `Artifact/include/Render/DiligentImmediateSubmitter.ixx` | submit クラス定義 |
| `Artifact/src/Render/DiligentImmediateSubmitter.cppm` | 全描画実装 |
| `Artifact/include/Render/RenderCommandBuffer.ixx` | DrawPacket variant 定義 |
| `Artifact/include/Render/IRenderSubmitter.ixx` | submit インターフェース |
| `Artifact/include/Render/PrimitiveRenderer2D.ixx` | 2D プリミティブ API |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | CommandBuffer への記録 |
| `Artifact/src/Render/PrimitiveRenderer3D.cppm` | 3D プリミティブ API |
| `Artifact/src/Render/ShaderManager.cppm` | PSO/SRB 管理 |
| `Artifact/include/Render/DiligentDeviceManager.ixx` | `immediateContext()` / `deferredContext()` |
