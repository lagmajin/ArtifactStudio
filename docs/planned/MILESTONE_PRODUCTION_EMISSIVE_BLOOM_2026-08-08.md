# Production Emissive + GPU Bloom Pipeline (2026-08-08)

**最終更新:** 2026-08-08
**状態:** 計画

## 概要

3D Model Layer の PBR マテリアルに実装済みのエミッシブ (`emissionColor` + `emissionStrength` + `emissionTexture`) を、GPU ベースのリアルタイム Bloom ポストプロセスで光らせる。プロダクションレベルの発光表現（Blade Runner ネオン、TRON グロー、モーショングラフィックス）を実現する。

## 現状のスコア

| 要素 | 状態 | ファイル |
|------|------|---------|
| `Material::setEmissionColor()` | ✅ | `ArtifactCore/include/Material/Material.ixx` |
| `Material::setEmissionStrength()` (float) | ✅ | 同上 |
| `Material::setEmissionTexture()` (エミッシブマップ) | ✅ | 同上 |
| `Material::makeEmissive(color, strength)` preset | ✅ | 同上 |
| `Artifact3DModelLayer::Impl::material_` | ✅ `Artifact/src/Layer/Artifact3DModelLayer.cppm:100` |
| `ArtifactIRenderer::setEmissionColor/Texture()` | ✅ | `Artifact/src/Render/ArtifactIRenderer.cppm:779-781` |
| `ArtifactIRenderer::drawMesh()` → エミッシブチャンネル出力 | ✅ | レンダラ内部で個別チャンネルに書き込み |
| `ChannelType::Emission` readback | ✅ | `ArtifactIRenderer.cppm:2987-2988` |
| `PBRMaterialEffect` (レイヤーエフェクト, emissive 0-100) | ✅ | `Artifact/include/Effects/Render/PBRMaterialEffect.ixx` |
| 2D CPU Glow エフェクト群 (OpenCV) | ✅ | `Artifact/src/Effects/Glow/*.cppm` (6種) |
| `EdgeBloomEffectCPUImpl` (GPU compute, Diligent) | ✅ | `Artifact/src/Effects/Glow/EdgeBloomEffect.cppm` |
| **GPU Bloom ポストプロセス** | ❌ | **これを作る** |
| **HDR バックバッファ (RGBA16F)** | ❌ | 既存は LDR。Bloom用に fp16 化 |
| **Tone Mapping (HDR→LDR)** | ❌ | Bloom適用後に必須 |

## アーキテクチャ: 既存レンダーパイプラインへの統合

現在のフレームレンダリングパス（`CompositionRenderController::renderFrame()`）:

```
Setup → Base → Surface → Mask → Composite → Post → Overlay
```

変更後:

```
Setup → Base → Surface (→ Emissive RT 出力) → Mask → Composite
       ↓
  HDR Composite RT (RGBA16F)
       ↓
  [新] Bloom Pass:
    1. Bright Pass: emissive RT を閾値で切り出し → half-res
    2. Downsample Blur: 1/2 → 1/4 → 1/8 → 1/16
    3. Upsample Blur: 1/16 → 1/8 → 1/4 → 1/2
    4. Combine: blur pyramid を加算合成
       → Bloom RT (RGBA16F, full-res)
       ↓
  Composite: HDR Color + Bloom → Post (Tone Map + LUT) → Overlay
```

### 必要な GPU リソース

| リソース | フォーマット | 解像度 | 用途 |
|----------|------------|--------|------|
| `g_HDRCompositeRT` | RGBA16F | full | Color + Emissive 合成結果。これを既存の LDR RT の代わりに使う |
| `g_EmissiveRT` | R11G11B10F | full | Surface pass で emissive のみをここに出力 (MECE) |
| `g_BloomRT[0..4]` | R11G11B10F | full, 1/2, 1/4, 1/8, 1/16 | Bloom ピラミッド。2つのテクスチャでピンポン |

### 統合ポイント

既存の `ArtifactFinalPostProcess` (`Artifact/src/Render/ArtifactFinalPostProcess.cppm`) がポストプロセスステージ。
ここに Bloom を追加する。`ArtifactFinalPostProcess::Impl` はすでに Diligent の `IDeviceContext` + `ITextureView*` インターフェースで動作している。

```cpp
// ArtifactFinalPostProcess に追加
bool applyBloom(IDeviceContext* pCtx,
                ITextureView* srcColorSRV,   // HDR Composite RT
                ITextureView* emissiveSRV,   // Emissive RT
                ITextureView* dstUAV,        // 出力先 (LDR swapchain)
                int width, int height,
                const BloomSettings& settings);
```

---

## Phase 1: HDR レンダーターゲット + Emissive RT 分離

**目的**: 既存の LDR フレームバッファを HDR 化し、エミッシブ成分を独立チャンネルとして取得する。

**変更**:
1. `blendPipeline_` の Composite RT を `RGBA8_UNORM` → `RGBA16_FLOAT` に変更
2. Surface pass で 3D モデルレイヤー描画時に、Emissive を別途 `g_EmissiveRT` に `Diligent::ATTACHMENT_BLEND_FACTOR_ZERO` で書き込み（Color RT には emissive を含めない MECE 分離）
3. Diligent の MRT (Multiple Render Target) を使用: Color RT (RGBA16F) + Emissive RT (R11G11B10F)

**影響範囲**:
- `LayerBlendPipeline` の RT 作成
- `ArtifactIRenderer::drawMesh()` → シェーダーで `emissiveOutput += baseColor * emissionColor * emissionStrength` を Emissive RT に出力
- 既存の PBR シェーダー (`Artifact/App/shaders/` 以下の `.hlsl`) に MRT 出力追加

**リスク**: 低。Diligent MRT は標準機能。既存の PBR シェーダーに追加出力チャンネルを加えるだけ。

---

## Phase 2: Bloom コンピュートパイプライン

**目的**: Emissive RT から多段ガウスブラーを GPU コンピュートで実行し、ブルーム画像を生成する。

**既存資産の再利用**:
- `EdgeBloomEffectCPUImpl` がすでに Diligent コンピュートシェーダーでガウスブラーを実装済み（`Artifact/src/Effects/Glow/EdgeBloomEffect.cppm`）。このパターンを踏襲
- `Graphics.Compute` モジュール / `GpuContext` がコンピュートシェーダーの管理基盤

**Bloom アルゴリズム** (Kawase / dual-filter 方式):

```
// Bright Pass (Compute Shader)
// emissiveSRV → brightUAV
// output = max(0, emissive.rgb - threshold) * intensity

// Downsample (Compute Shader) × 4
// srcUAV → dstUAV (half size each step)
// 4-tap tent filter + downsample

// Upsample (Compute Shader) × 4
// srcUAV + nextLevelSRV → dstUAV (double size each step)
// bilinear upsample + additive blend with next level

// 結果: bloomUAV (full-res, R11G11B10F)
```

**HLSL シェーダー** (新規):
- `bloom_bright_pass.hlsl` — 閾値切り出し
- `bloom_downsample.hlsl` — Kawase ダウンサンプルブラー
- `bloom_upsample.hlsl` — アップサンプル + 加算合成
- `bloom_composite.hlsl` — HDR Color + Bloom → LDR ToneMap

これらは `Artifact/App/shaders/` に配置し、既存の PBR シェーダーと同じパターンで Diligent のシェーダーコンパイルパイプラインに乗せる。

---

## Phase 3: Tone Mapping + LUT 統合

**目的**: Bloom 合成後の HDR 画像をディスプレイの SDR/LDR に変換する。

**実装**:
- ACES Filmic Tone Mapping（標準的な映画用トーンマップ）
- `bloom_composite.hlsl` 内で実装:

```hlsl
// ACES Filmic
float3 ACESFilm(float3 x) {
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x*(a*x+b)) / (x*(c*x+d)+e));
}

// HDR Color + Bloom → ToneMap → sRGB
float3 hdr = colorRT.rgb + bloom.rgb * bloomIntensity;
float3 mapped = ACESFilm(hdr);
float3 srgb = LinearToSRGB(mapped);
// 既存の LUT/OCIO もこの後に適用
```

---

## Phase 4: ユーザー設定とプリセット

**目的**: Bloom の強度・閾値・半径を作業者が調整できるようにする。

**プロパティ** (レイヤー単位 または コンポジショングローバル):

| プロパティ | デフォルト | 範囲 |
|-----------|----------|------|
| `bloomThreshold` | 1.0 | 0.0-10.0 |
| `bloomIntensity` | 1.0 | 0.0-5.0 |
| `bloomRadius` | 1.0 | 0.25-4.0 (ダウンサンプル段数制御) |
| `bloomTint` | White | 任意色 (emissive に乗算) |
| `bloomEnabled` | true | bool |

**プリセット**:
- **Subtle**: threshold=1.5, intensity=0.4 — UI要素のソフトな発光
- **Standard**: threshold=1.0, intensity=1.0 — ネオン／モーショングラフィックス
- **Cinematic**: threshold=0.7, intensity=1.8 — SF/映画的な強い発光
- **Blade Runner**: threshold=0.3, intensity=3.0, tint=warm amber — 極端な発光

---

## Phase 一覧

| Phase | 内容 | コスト | 依存 |
|-------|------|--------|------|
| 1 | HDR RT + Emissive MRT 出力 | 中 | シェーダー改変 + RT作成 |
| 2 | Bloom コンピュートパイプライン | 中 | Phase 1, HLSL 3本新規 |
| 3 | Tone Mapping + LUT 統合 | 低 | Phase 2 |
| 4 | ユーザー設定 + プリセット | 低 | Phase 3 |

合計 **中程度** の工数。Phase 1 が唯一のリスクポイント（既存シェーダー改変）で、Phase 2〜4 は純粋な追加。

---

## 変更対象ファイル

| ファイル | Phase |
|----------|-------|
| `Artifact/src/Render/ArtifactFinalPostProcess.cppm` | 1, 2, 3 |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 1 |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` | 1 |
| `Artifact/App/shaders/PBR_PS.hlsl`（既存改変） | 1 |
| `Artifact/App/shaders/bloom_bright_pass.hlsl`（新規） | 2 |
| `Artifact/App/shaders/bloom_downsample.hlsl`（新規） | 2 |
| `Artifact/App/shaders/bloom_upsample.hlsl`（新規） | 2 |
| `Artifact/App/shaders/bloom_composite.hlsl`（新規） | 3 |
| `Artifact/include/Render/ArtifactHDRMonitor.ixx` | 4 |
| `Artifact/src/Render/ArtifactHDRMonitor.cppm` | 4 |
| `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm` | 4 |
