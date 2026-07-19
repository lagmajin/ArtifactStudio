# SSAO 実装詳細参照書

> 参照元: **Godot `servers/rendering/renderer_rd/effects/ssao.cpp`** (MIT)
> 補足: Filament `PostProcessManager.cpp` SSAO (Apache 2.0)

---

## パイプライン全景

```
深度バッファ + 法線バッファ (G-Buffer)
    │
    ├─→ [SSAO計算] GTAO (Ground Truth Ambient Occlusion)
    │       深度から半球サンプリング + 法線で重み付け
    │
    └─→ [Bilateral Blur]  Separable 2-pass エッジ保存ブラー
            深度の不連続をエッジとして保持しつつノイズ除去
```

---

## GTAO アルゴリズム (Godot 版)

```glsl
// 各ピクセルで深度バッファから半球サンプリング
float computeGTAO(vec2 uv, float depth, vec3 normal) {
    vec3 viewPos = reconstructViewPos(uv, depth);
    float ao = 0.0;

    // 複数方向にスライス
    for (int i = 0; i < SLICE_COUNT; i++) {
        float angle = (float(i) + interleavedOffset) * PI / float(SLICE_COUNT);
        vec2 dir = vec2(cos(angle), sin(angle));
        // 各方向で段階的に遠ざかるサンプル
        for (int step = 1; step <= STEP_COUNT; step++) {
            vec2 sampleUV = uv + dir * step * stepSize;
            float sampleDepth = texture(s_depth, sampleUV).r;
            vec3 samplePos = reconstructViewPos(sampleUV, sampleDepth);
            vec3 v = samplePos - viewPos;
            float vv = dot(v, v);
            float vn = dot(v, normal);
            // Horizon angle による遮蔽計算
            float occlusion = max(0.0, vn - viewPos.z * 0.001) / (vv + 0.0001);
            ao += occlusion;
        }
    }
    ao /= float(SLICE_COUNT * STEP_COUNT);
    return 1.0 - ao * intensity;
}
```

**パラメータ:**
| パラメータ | 範囲 | 効果 |
|---|---|---|
| intensity | 0.0-3.0 | AO強度 |
| radius | 0.1-10.0 | サンプル半径 |
| sliceCount | 2-6 | 方向数 |
| stepCount | 2-6 | 方向あたりの段数 |
| bias | 0.001 | 自己遮蔽防止 |

---

## Bilateral Blur (Separable 2-Pass)

```glsl
// Pass 1: 水平方向
void bilateralBlurH(vec2 uv) {
    float centerDepth = texture(s_depth, uv).r;
    float sum = 0.0, totalWeight = 0.0;

    for (int x = -RADIUS; x <= RADIUS; x++) {
        vec2 suv = uv + vec2(float(x) * texelSize.x, 0.0);
        float sampleDepth = texture(s_depth, suv).r;
        float depthDiff = abs(centerDepth - sampleDepth);

        // エッジ保存: 深度差が大きいと重みが急減
        float weight = exp(-depthDiff * depthDiff * edgeSharpness);
        weight *= gaussianWeight[abs(x)];

        sum += texture(s_ao, suv).r * weight;
        totalWeight += weight;
    }
    return sum / totalWeight;
}
// Pass 2: 垂直方向 (同ロジック)
```

---

## Godot SSAO パラメータ完全セット

```cpp
struct SSAOSettings {
    float intensity = 1.0;       // AO強度
    float radius = 1.0;          // サンプル半径
    float detail = 0.5;          // ディテール量
    bool halfSize = true;        // 1/2解像度で計算
    float power = 1.5;           // 結果のパワーカーブ
    // SSIL (間接照明)
    bool ssilEnabled = false;
    float ssilRadius = 5.0;
    float ssilIntensity = 1.0;
    float ssilSharpness = 0.98;
};
```

---

## ArtifactStudio 応用

3D レイヤー間の陰影による奥行き感に使用:
- 全3Dレイヤーを統合深度バッファにレンダリング後、SSAO適用
- Bilateral Blur でエッジ（レイヤー境界）を自然に保持
- 1/2 解像度で計算し Bilinear アップスケールで高速化

---

## 実装優先度

SSAO は Bloom/DOF/ToneMapping より後の優先度。
理由: 3D レイヤーの数と複雑さに依存し、2D コンポジットでは不要なため。
