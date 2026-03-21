# マイルストーン: ComputeShader (HLSL) ベース レイヤーブレンドシステム

> 2026-03-21 作成

## 概要

CompositionEditor の GPU レンダーパスに ComputeShader ベースのレイヤーブレンドを導入する。
現状は painter's algorithm でレイヤーを順描画するのみでブレンドモードが無視されている。
18種類の `BlendMode` 全てを GPU Compute で実装し、リアルタイム合成を実現する。

---

## 現状サマリー

### 実装済み

| コンポーネント | 状態 | 場所 |
|---|---|---|
| `BlendMode` enum (18種) | ✅ 定義済 | `ArtifactCore/include/Layer/LayerBlend.ixx` |
| CPU ブレンド実装 (18種全数) | ✅ 完了 | `ArtifactCore/src/Color/ColorBlendMode.cppm` |
| `ComputeExecutor` (汎用Compute実行器) | ✅ 完了 | `ArtifactCore/include/Graphics/Shader/Compute/Compute.ixx` |
| GPUブレンドシェーダ (Normal) | ✅ HLSLファイル有 | `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendNormal.hlsl` |
| GPUブレンドシェーダ (Add) | ✅ HLSLファイル有 | `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendAdd.hlsl` |
| GPUブレンドシェーダ (Screen) | ✅ HLSLファイル有 | `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendScreen.hlsl` |
| GPUブレンドシェーダ (Overlay) | ✅ HLSLファイル有 | `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendOverlay.hlsl` |
| GPUブレンドシェーダ (SoftLight) | ✅ HLSLファイル有 | `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendSoftlight.hlsl` |
| インラインシェーダ文字列 (4種) | ✅ | `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx` |
| ブレンドUI (18種コンボボックス) | ✅ | `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` |
| レイヤーの `blendMode_` フィールド | ✅ シリアライズ対応 | `Artifact/src/Layer/ArtifactAbstractLayer.cppm` |

### 未実装 / 不完全

| コンポーネント | 状態 | 場所 |
|---|---|---|
| GPUブレンドシェーダ (残り13種) | ❌ なし | - |
| `OffscreenRenderer2D` PSO作成 | ❌ コメントアウト | `Artifact/src/Render/ArtifactOffscreenRenderer2D.cppm:221-224` |
| `OffscreenRenderer2D` Dispatch | ❌ コメントアウト | `Artifact/src/Render/ArtifactOffscreenRenderer2D.cppm:300` |
| CompositionEditor への統合 | ❌ 未着手 | - |
| レイヤー毎の opacity 対応 | ❌ 未着手 | - |
| 中間テクスチャ合成パイプライン | ❌ 未着手 | - |
| `RenderPipeline` | ❌ 空スタブ | `Artifact/src/Render/ArtifactRenderLayerPipeline.cpp` |
| `LayerPreviewPipeline` | ❌ 空スタブ | `Artifact/src/Preview/ArtifactLayerPreviewPipeline.cpp` |

### 既存アーキテクチャ

現在の描画パス (CompositionEditor):
```
CompositionRenderController
  └─ PreviewCompositionPipeline::render()
       ├─ drawCheckerboard (背景)
       ├─ for layer in allLayer() (逆順 painter's algorithm)
       │    └─ drawLayerForPreviewView()  ← 描画のみ、ブレンド無し
       ├─ draw selection gizmos
       └─ flush()
```

目標の描画パス:
```
CompositionRenderController
  └─ LayerBlendPipeline::render()
       ├─ レイヤー毎に RT にオフスクリーン描画
       ├─ ComputeShader で 2枚のテクスチャをブレンド (back-to-front)
       │    ├─ SrcTex (現在レイヤー) + DstTex (累積結果)
       │    ├─ ブレンドモードに応じた PSO 選択
       │    └─ opacity パラメータを CB で渡す
       ├─ 最終結果をスクリーンに.blit
       └─ flush()
```

---

## Phase 1: HLSL シェーダ補完 (P0)

残り13種のブレンドモードを HLSL で実装する。
既存5種は `ArtifactCore/include/Graphics/Shader/HLSL/Blend/` に存在する。

### 1-1. 残り HLSL シェーダ作成

| ファイル名 | ブレンドモード | 数式 |
|---|---|---|
| `CS_BlendSubtract.hlsl` | Subtract | `saturate(dst - src)` |
| `CS_BlendDarken.hlsl` | Darken | `min(src, dst)` |
| `CS_BlendLighten.hlsl` | Lighten | `max(src, dst)` |
| `CS_BlendColorDodge.hlsl` | ColorDodge | `dst / (1 - src)` (clamp対応) |
| `CS_BlendColorBurn.hlsl` | ColorBurn | `1 - (1 - dst) / src` (clamp対応) |
| `CS_BlendHardLight.hlsl` | HardLight | Overlay の src/dst 反転 |
| `CS_BlendDifference.hlsl` | Difference | `abs(src - dst)` |
| `CS_BlendExclusion.hlsl` | Exclusion | `src + dst - 2 * src * dst` |
| `CS_BlendHue.hlsl` | Hue | RGB→HSL変換、H置換 |
| `CS_BlendSaturation.hlsl` | Saturation | RGB→HSL変換、S置換 |
| `CS_BlendColor.hlsl` | Color | RGB→HSL変換、H+S置換 |
| `CS_BlendLuminosity.hlsl` | Luminosity | RGB→HSL変換、L置換 |

**共通仕様:**
- `Texture2D<float4> SrcTex : register(t0)` (前景)
- `Texture2D<float4> DstTex : register(t1)` (背景)
- `RWTexture2D<float4> ResultTex : register(u0)` (出力)
- `ConstantBuffer<BlendParams> CB : register(b0)` — opacity, blendMode 等
- `[numthreads(8,8,1)]`
- エントリポイント: `main`

**見積:** 13ファイル × 1時間 ≈ **1.5日**

### 1-2. 共通ヘッダ (`BlendCommon.hlsli`)

RGB↔HSL 変換ヘルパー、共通 clamp/alpha 処理をヘッダ化。

**対象:** `ArtifactCore/include/Graphics/Shader/HLSL/Blend/BlendCommon.hlsli`
**見積:** 0.5日

### 1-3. インラインシェーダ文字列 (`LayerBlendComputeShader.ixx`) 更新

- 18種全てのシェーダ文字列を `BlendShaders` マップに登録
- 既存の 4種 + 新規 14種
- ファイル読み込み or インライン文字列の選択 → **インライン文字列方式を維持** (既存パターン踏襲)

**対象:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`
**見積:** 0.5日

---

## Phase 2: Compute パイプライン構築 (P0)

### 2-1. `BlendPipeline` クラス設計・実装

`ComputeExecutor` を活用してブレンド専用のパイプラインを構築する。

```
class LayerBlendPipeline {
    // 各 BlendMode に対応する PSO + SRB
    QMap<BlendMode, ComputeExecutor> executors_;

    bool initialize(IDevice* device);
    bool blend(
        IDeviceContext* ctx,
        ITextureView* srcSRV,    // 前景レイヤー
        ITextureView* dstSRV,    // 累積背景
        ITextureView* outUAV,    // 出力
        BlendMode mode,
        float opacity
    );
};
```

**対象新規ファイル:**
- `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx`
- `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`

**見積:** 2日

### 2-2. `OffscreenRenderer2D` の PSO 作成・Dispatch 完成

既存のコメントアウト箇所を復活させる。

| 行 | 作業 |
|---|---|
| 221-224 | `CreateComputePipelineState` の復活 |
| 262 | `CreateShader` の復活 |
| 300 | `DispatchCompute` の復活 |
| 247-258 | `ShaderResourceVariableDesc` 追加 (SrcTex, DstTex, OutTex) |

**対象:** `Artifact/src/Render/ArtifactOffscreenRenderer2D.cppm`
**見積:** 1日

---

## Phase 3: テクスチャ管理・合成ループ (P0)

### 3-1. 中間テクスチャの Ping-Pong 管理

2枚のテクスチャを交互に読み書きする ping-pong パターンで N 枚のレイヤーを合成する。

```
for i = 0..N-1:
    // レイヤー[i] を layerTex に描画
    renderer->renderLayer(layers[i], layerTex_RTV)

    // layerTex + accumTex → resultTex にブレンド
    blendPipeline->blend(ctx,
        layerTex_SRV,    // Src
        accumTex_SRV,    // Dst
        resultTex_UAV,   // Out
        layers[i].blendMode,
        layers[i].opacity
    )

    // resultTex → accumTex にコピー (次イテレーションの Dst に)
    ctx->CopyTexture(accumTex, resultTex)
```

**テクスチャ仕様:**
- Format: `TEX_FORMAT_RGBA8_UNORM` (コンピュート互換)
- BindFlags: `BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS`
- 2枚 (accum + temp) をコンポジションサイズで作成
- リサイズ時に再生成

**対象:** `Artifact/src/Render/ArtifactRenderLayerPipeline.cpp` (空スタブを実装化)
**見積:** 2日

### 3-2. レイヤー毎 opacity 対応

ブレンドシェーダに `ConstantBuffer` で opacity を渡し、ブレンド結果を `lerp(dst, blended, opacity)` で合成する。

**CB 定義:**
```hlsl
ConstantBuffer<BlendParams> : register(b0) {
    float opacity;       // 0.0 ~ 1.0
    uint  blendMode;     // BlendMode enum value
    float2 _pad;
};
```

**対象:** 全 HLSL シェーダ + `LayerBlendPipeline`
**見積:** 0.5日

---

## Phase 4: CompositionEditor 統合 (P0)

### 4-1. `PreviewCompositionPipeline` の描画パス置換

現状の painter's algorithm 直接描画を、`LayerBlendPipeline` 経由の Compute 合成に置き換える。

- `render()` 内のレイヤーループを合成ループに変更
- 合成結果テクスチャを最終描画で.blit
- 選択ギズモは合成後にオーバーレイ描画 (従来通り)

**対象:** `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`
**見積:** 1.5日

### 4-2. `CompositionRenderController` への統合

- `renderOneFrame()` でパイプラインを経由
- ブレンドモード変更時の PSO 再選択
- テクスチャリサイズ対サイズ対応

**対象:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
**見積:** 1日

---

## Phase 5: 品質・最適化 (P1)

### 5-1. フォールバック (CPU ブレンド)

GPU 未対応環境向けに `ColorBlendMode::blend()` (CPU) へのフォールバックパスを用意。

**対象:** `ArtifactCore/src/Color/ColorBlendMode.cppm` (既存)
**見積:** 0.5日

### 5-2. パフォーマンス最適化

- ディスパッチ回数削減: 連続する Normal+opacity=1.0 レイヤーをスキップ
- テクスチャリアロケーション回避: サイズ不変時は再利用
- Fence 待ち最適化: バッチディスパッチ

**見積:** 1日

### 5-3. デバッグ可視化

- ブレンド結果テクスチャのプレビュー表示
- パイプラインステップごとの中間結果確認

**見積:** 0.5日

---

## 現状の機能マップ

| 機能 | 状態 |
|---|---|
| BlendMode 定義 (18種) | ✅ |
| CPU ブレンド実装 (18種) | ✅ |
| GPU ブレンドシェーダ (5/18) | 🔶 |
| GPU ブレンドシェーダ (残り13/18) | ❌ |
| ComputeExecutor (汎用) | ✅ |
| ブレンド PSO 作成 | ❌ (コメントアウト) |
| ブレンド Dispatch | ❌ (コメントアウト) |
| レイヤー opacity | ❌ |
| 中間テクスチャ管理 | ❌ |
| CompositionEditor 統合 | ❌ |
| Painter's algorithm 描画 | ✅ (ブレンド無し) |
| UI ブレンドモード選択 | ✅ |

---

## 対象ファイル一覧

### 新規作成

| ファイル | 概要 |
|---|---|
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/BlendCommon.hlsli` | 共通 HLSL ヘッダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendSubtract.hlsl` | Subtract シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendDarken.hlsl` | Darken シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendLighten.hlsl` | Lighten シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendColorDodge.hlsl` | ColorDodge シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendColorBurn.hlsl` | ColorBurn シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendHardLight.hlsl` | HardLight シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendDifference.hlsl` | Difference シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendExclusion.hlsl` | Exclusion シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendHue.hlsl` | Hue シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendSaturation.hlsl` | Saturation シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendColor.hlsl` | Color シェーダ |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/CS_BlendLuminosity.hlsl` | Luminosity シェーダ |
| `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx` | ブレンドパイプライン定義 |
| `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm` | ブレンドパイプライン実装 |

### 変更

| ファイル | 変更内容 |
|---|---|
| `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx` | 18種シェーダ登録 + CB 追加 |
| `Artifact/src/Render/ArtifactOffscreenRenderer2D.cppm` | PSO/Dispatch コメント解除 |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cpp` | 空スタブを実装化 |
| `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm` | 描画パス置換 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | パイプライン統合 |

---

## Suggested Order

1. **Phase 1** — HLSL シェーダ補完 (独立作業、他に影響無し)
2. **Phase 2** — `LayerBlendPipeline` クラス構築 (`ComputeExecutor` 活用)
3. **Phase 3** — テクスチャ管理 + 合成ループ
4. **Phase 4** — CompositionEditor 統合
5. **Phase 5** — 品質・最適化

---

## 見積サマリー

| Phase | 見積 |
|---|---|
| Phase 1: HLSL シェーダ補完 | 2.5日 |
| Phase 2: Compute パイプライン | 3日 |
| Phase 3: テクスチャ管理・合成ループ | 2.5日 |
| Phase 4: CompositionEditor 統合 | 2.5日 |
| Phase 5: 品質・最適化 | 2日 |
| **合計** | **≈ 12.5日** |
