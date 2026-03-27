# Milestone: レイトレ系エフェクト段階導入 (2026-03-25)

**Status:** Not Started
**Goal:** AE風コンポジタに「軽量で実用的なレイトレ系表現」を段階導入。
常時はラスタ（高速）、必要時のみ RT/近似で品質を上げる。

---

## Architecture（共通方針）

- **Default:** Raster（高速ラスタパス維持）
- **Quality Tier:**
  - `Preview` — 軽量（Fast/Soft/SSAO低品質/SSR低品質）
  - `High` — 中品質（SSAO高品質/SSR高品質）
  - `Final` — 重い処理 OK（低サンプル RT 有効）

```cpp
enum class Quality { Preview, High, Final };
```

- すべての効果は**独立パス**として実装し、最後に合成
- 解像度は効果ごとにダウンサンプル可（1/2, 1/4）
- CPU RT は禁止、常時 RT は禁止、すべて GPU（Diligent）で処理
- 既存ラスタパスを壊さない

---

## Rendering Pipeline（全体フロー）

```cpp
RenderScene();                          // 既存ラスタパス
if (enableShadow)   RenderShadowPass(); // M1
if (enableSSAO)     RenderSSAOPass();   // M2
if (enableSSR)      RenderSSRPass();    // M3
if (enableContact)  RenderContactShadow(); // M4
if (enableRTShadow && quality == Final) RenderRTShadow(); // M5
Composite();                            // 既存コンポジット
```

---

## Existing Infrastructure

| コンポーネント | 状態 | 場所 |
|---|---|---|
| Wicked Engine の BVH 構造体 | ✅ 継承済み | `ShaderInterop_BVH.h` |
| Wicked Engine の raytraceCS.hlsl | ✅ 継承済み | `shaders/raytraceCS.hlsl` |
| Wicked Engine の raytraceCS_rtapi.hlsl | ✅ 継承済み | `shaders/raytraceCS_rtapi.hlsl` |
| Wicked Engine の ssr_raytraceCS.hlsl | ✅ 継承済み | `shaders/ssr_raytraceCS.hlsl` |
| Wicked Engine の ssr_raytraceCS_cheap.hlsl | ✅ 継承済み | `shaders/ssr_raytraceCS_cheap.hlsl` |
| Wicked Engine の raytracingHF.hlsli | ✅ 継承済み | `shaders/raytracingHF.hlsli` |
| Diligent ComputePipeline | ✅ 利用可能 | `ArtifactCore/src/Graphics/Compute.cppm` |
| ComputeExecutor | ✅ 完成 | `ArtifactCore/include/Graphics/Shader/Compute/Compute.ixx` |
| LayerBlendPipeline (Compute パターン) | ✅ 完成 | `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm` |
| GpuContext | ✅ 完成 | `ArtifactCore/src/Graphics/GPUComputeContext.cppm` |
| ArtifactIRenderer | ✅ 完成 | `Artifact/include/Render/ArtifactIRenderer.ixx` |
| PrimitiveRenderer2D | ✅ 完成 | `Artifact/src/Render/PrimitiveRenderer2D.cppm` |
| TextureBundle (RTV/SRV/UAV) | ✅ 完成 | `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` |

---

## Milestone 1: Soft Shadow（必須）

**目的:** 軽くて見た目の良い影。RT なしで "それっぽさ" を出す。

### Inputs
- `lightDir` (vec2/vec3)
- `distance`
- `softness`
- `opacity`

### Implementation
1. オブジェクトシルエットをオフセット描画（シャドウテクスチャ生成）
2. ガウシアン or Kawase blur（ping-pong 2パス）
3. 距離でブラー量変化: `float blur = baseBlur + distance * k`

### Pass Structure
| パス | 入力 | 出力 | 解像度 |
|---|---|---|---|
| Shadow RT | scene geometry | shadow mask (1ch) | 1/2 or 1/4 |
| Blur Pass H | shadow mask | blurH | 1/2 or 1/4 |
| Blur Pass V | blurH | blurV | 1/2 or 1/4 |
| Composite | blurV + scene | final | full |

### 見積
| タスク | 見積 |
|---|---|
| Shadow テクスチャ生成 Compute シェーダー | 3h |
| Kawase Blur Compute シェーダー | 2h |
| ShadowPass クラス（GpuContext 使用） | 3h |
| Composite への統合 | 2h |
| Inspector UI (lightDir/distance/softness/opacity) | 2h |
| Quality 切替 (Preview=1/4解像度, High=1/2) | 1h |

### Acceptance Criteria
- Preview で 60fps 維持
- AE の Drop Shadow より自然
- ON/OFF で視覚的差異が明確

---

## Milestone 2: SSAO（Screen Space AO）

**目的:** 接触部の陰影を強化（コスパ最強）

### Inputs
- depth buffer（既存のビューポート深度 or 擬似深度）
- normal（無ければピクセル差分で擬似生成）

### Implementation
- ランダムサンプル半球（8〜16 samples）
- depth 差で遮蔽判定
- ブラーでノイズ除去

```hlsl
float occlusion = 0;
for (int i = 0; i < sampleCount; ++i) {
    float3 samplePos = pos + hemisphere[i] * radius;
    float sampleDepth = depthTexture.Sample(samplePos);
    occlusion += step(sampleDepth, currentDepth - bias);
}
occlusion = 1.0 - (occlusion / sampleCount) * intensity;
```

### Pass Structure
| パス | 入力 | 出力 | 解像度 |
|---|---|---|---|
| SSAO | depth + normal | ao raw (1ch) | 1/2 |
| Blur | ao raw | ao smooth | 1/2 |
| Composite | ao smooth + scene | final | full |

### 見積
| タスク | 見積 |
|---|---|
| SSAO Compute シェーダー | 3h |
| ランダムノイズテクスチャ生成 | 1h |
| 擬似深度/法線生成（未実装の場合） | 3h |
| SSAOPass クラス | 2h |
| Blur パス（共通 Kawase/Gaussian を再利用） | 1h |
| Composite 統合 | 1h |
| Inspector UI (intensity/radius/samples) | 1h |

### Acceptance Criteria
- エッジ/接触部が締まる
- コスト低（Preview で 60fps）
- 1/2 解像度でノイズが目立たない

---

## Milestone 3: SSR（Screen Space Reflection）

**目的:** 軽量な反射表現

### Inputs
- color buffer
- depth buffer
- normal（あれば）

### Implementation
- レイをスクリーンスペースでレイマーチ
- depth 一致でヒット判定

```hlsl
float3 rayDir = reflect(viewDir, normal);
float3 pos = screenPos;
for (int i = 0; i < maxSteps; ++i) {
    pos += rayDir * stepSize;
    float hitDepth = depthTexture.Sample(pos);
    if (hitDepth >= pos.z - thickness) {
        // ヒット
        return colorTexture.Sample(pos);
    }
}
return float4(0,0,0,0); // ヒットなし
```

### Limitations
- 画面外は反射しない（OK、AE でも同様）
- 粗面マテリアルは stepSize を大きくしてぼかす

### Pass Structure
| パス | 入力 | 出力 | 解像度 |
|---|---|---|---|
| SSR | color + depth + normal | reflection mask | 1/2 |
| Blur (roughness) | reflection mask | reflection smooth | 1/2 |
| Composite | reflection + scene | final | full |

### 見積
| タスク | 見積 |
|---|---|
| SSR Compute シェーダー（Wicked Engine をベース改修） | 4h |
| Roughness ブラー | 2h |
| SSRPass クラス | 2h |
| Composite 統合 | 1h |
| Inspector UI (intensity/steps/roughness) | 1h |

### Acceptance Criteria
- 簡易リフレクション表現が可能
- Preview で軽い
- 画面外クリッピングが自然

---

## Milestone 4: Contact Shadow（軽 RT or Screen）

**目的:** 接触影を強化

### Implementation（2択）

**A. Screen space ray march（Preview/High）**
- SSAO の逆、光線をレイマーチして接触判定
- SSAO パスと同じ深度/法線を使用

**B. DXR/compute で 1 サンプル RT（Final）**
- Wicked Engine の `raytraceCS_rtapi.hlsl` をベースに 1spp
- BVH は Wicked Engine 継承構造体を利用

### Quality 切替
| Quality | モード | コスト |
|---|---|---|
| Preview | Screen space (1/4 解像度) | 低 |
| High | Screen space (1/2 解像度) | 中 |
| Final | 1spp RT | 高 |

### 見積
| タスク | 見積 |
|---|---|
| Screen space contact shadow シェーダー | 3h |
| 1spp RT シェーダー（既存ベース改修） | 4h |
| ContactShadowPass クラス | 2h |
| Quality 切替ロジック | 1h |
| Inspector UI | 1h |

### Acceptance Criteria
- 接触影が自然に強化される
- Quality で見た目/パフォーマンスの差が明確

---

## Milestone 5: RT Shadow（低サンプル）

**目的:** 最終品質用の影（Final モードのみ）

### Implementation
- 1〜4 samples per pixel
- ノイズ許容（後段で軽くブラー）
- Wicked Engine の `raytraceCS_rtapi.hlsl` + BVH を利用

```hlsl
float3 shadow = 0;
for (int i = 0; i < samples; ++i) {
    float3 lightDir = jitteredLightDir(i);
    shadow += traceShadowRay(hitPoint, lightDir);
}
shadow /= samples;
```

### Constraints
- **Final モードのみ有効**
- 解像度 1/2
- Preview/High では無効化して Soft Shadow にフォールバック

### 見積
| タスク | 見積 |
|---|---|
| RT Shadow シェーダー（既存ベース改修） | 4h |
| ノイズ除去ブラー | 2h |
| RTShadowPass クラス | 3h |
| BVH 構築との連携 | 4h |
| Quality 切替 + フォールバック | 2h |
| Inspector UI | 1h |

### Acceptance Criteria
- Final モードで高品質な接触影が生成される
- Preview/High ではパフォーマンスに影響しない
- Soft Shadow へのフォールバックがスムーズ

---

## Milestone 6: Quality Switch

**目的:** 状況に応じた品質自動切替

```cpp
enum class Quality { Preview, High, Final };

Quality autoQuality() {
    if (isFinalRender)  return Quality::Final;
    if (isPlaying)      return Quality::Preview;
    if (isIdle)         return Quality::High;
    return Quality::Preview;
}
```

### Quality ごとの有効エフェクト
| エフェクト | Preview | High | Final |
|---|---|---|---|
| Soft Shadow | ✅ 1/4 解像度 | ✅ 1/2 解像度 | ✅ 1/2 解像度 |
| SSAO | ✅ 8 samples, 1/4 | ✅ 16 samples, 1/2 | ✅ 32 samples, 1/2 |
| SSR | ❌ | ✅ 16 steps, 1/2 | ✅ 32 steps, 1/2 |
| Contact Shadow | ❌ | ✅ Screen, 1/2 | ✅ 1spp RT |
| RT Shadow | ❌ | ❌ | ✅ 1-4spp |

### 見積
| タスク | 見積 |
|---|---|
| QualityController クラス | 2h |
| 自動切替ロジック | 1h |
| 再生状態との連携 | 1h |
| レンダーキュー Final 品質フラグ | 1h |

---

## Milestone 7: Inspector UI

**目的:** 各エフェクトのパラメータを Inspector で編集

### Shadow
- Mode: Fast / Soft / RT
- Distance / Softness / Opacity / Light Direction

### AO
- Intensity / Radius / Samples

### Reflection
- Intensity / Steps / Roughness

### Contact Shadow
- Mode: Screen / RT
- Distance / Intensity

### 見積
| タスク | 見積 |
|---|---|
| Shadow プロパティグループ | 2h |
| AO プロパティグループ | 1h |
| SSR プロパティグループ | 1h |
| Contact Shadow プロパティグループ | 1h |
| Quality セレクタ | 1h |
| プリセット (Low/Medium/High/Ultra) | 1h |

---

## Deliverables

| ファイル | 内容 |
|---|---|
| `ArtifactCore/include/Graphics/Shader/Compute/SoftShadowPass.ixx` (新規) | シャドウパス |
| `ArtifactCore/src/Graphics/SoftShadowPass.cppm` (新規) | シャドウ実装 |
| `ArtifactCore/include/Graphics/Shader/Compute/SSAOPass.ixx` (新規) | SSAO パス |
| `ArtifactCore/src/Graphics/SSAOPass.cppm` (新規) | SSAO 実装 |
| `ArtifactCore/include/Graphics/Shader/Compute/SSRPass.ixx` (新規) | SSR パス |
| `ArtifactCore/src/Graphics/SSRPass.cppm` (新規) | SSR 実装 |
| `ArtifactCore/include/Graphics/Shader/Compute/ContactShadowPass.ixx` (新規) | 接触影パス |
| `ArtifactCore/src/Graphics/ContactShadowPass.cppm` (新規) | 接触影実装 |
| `ArtifactCore/include/Graphics/Shader/Compute/RTShadowPass.ixx` (新規) | RT シャドウパス |
| `ArtifactCore/src/Graphics/RTShadowPass.cppm` (新規) | RT シャドウ実装 |
| `ArtifactCore/include/Graphics/QualityController.ixx` (新規) | 品質切替 |
| `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` (拡張) | エフェクト UI |

---

## Recommended Order

| 順序 | マイルストーン | 見積 | 理由 |
|---|---|---|---|
| 1 | **M1 Soft Shadow** | 13h | 最も視覚的インパクト大、既存 blur パターンを再利用 |
| 2 | **M2 SSAO** | 12h | コスパ最強、接触影が劇的に改善 |
| 3 | **M3 SSR** | 10h | Wicked Engine の SSR シェーダーをベースに改修 |
| 4 | **M6 Quality Switch** | 5h | M4/M5 の前提 |
| 5 | **M4 Contact Shadow** | 11h | SSAO と組み合わせで高品質 |
| 6 | **M5 RT Shadow** | 16h | 最終品質用、DXR 対応 GPU 必須 |
| 7 | **M7 Inspector UI** | 7h | 各エフェクト完成後に UI 統合 |

**総見積: ~74h**

---

## Related Files

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/shaders/ShaderInterop_BVH.h` | 1-57 | BVH 構造体（Wicked Engine 継承） |
| `Artifact/shaders/raytraceCS.hlsl` | - | レイトレーシング CS（Wicked Engine 継承） |
| `Artifact/shaders/raytraceCS_rtapi.hlsl` | - | RT API 版 CS（Wicked Engine 継承） |
| `Artifact/shaders/raytracingHF.hlsli` | - | レイトレヘルパー（Wicked Engine 継承） |
| `Artifact/shaders/ssr_raytraceCS.hlsl` | - | SSR CS（Wicked Engine 継承） |
| `Artifact/shaders/ssr_raytraceCS_cheap.hlsl` | - | SSR 低品質版（Wicked Engine 継承） |
| `Artifact/shaders/ssr_raytraceCS_earlyexit.hlsl` | - | SSR 早期終了版（Wicked Engine 継承） |
| `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm` | 1-177 | Compute パス実装パターン（参考） |
| `ArtifactCore/src/Graphics/Compute.cppm` | 1-131 | ComputeExecutor（参考） |
| `ArtifactCore/src/Graphics/GPUComputeContext.cppm` | 1-162 | GpuContext（参考） |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` | 1-247 | TextureBundle（参考） |
