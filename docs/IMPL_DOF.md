# DOF (被写界深度) 実装詳細参照書

> 参照元: **Filament `PostProcessManager.cpp`** (Apache 2.0)
> 対応マイルストーン: `docs/GAME_ENGINE_RENDERING_REFERENCE.md`

---

## パイプライン全景

```
HDR入力画像 + 深度バッファ
    │
    ├─→ [CoC計算] 深度 → Circle of Confusion 半径 (RGBA8: R=手前CoC, G=奥CoC)
    │       │
    │       ├─→ [手前ボケ] CoCに応じた可変カーネルブラー (リング方式)
    │       │
    │       └─→ [奥ボケ] 同上、奥側
    │
    └─→ [合成] 元画像 + 手前ボケ + 奥ボケ を CoC でアルファブレンド
```

---

## CoC (Circle of Confusion) 計算

```glsl
// 深度 → CoC半径
float linearDepth = linearizeDepth(depthSample, cameraNear, cameraFar);
float coc = (linearDepth - focusDistance) * cocScale;
// coc > 0 → 奥ボケ, coc < 0 → 手前ボケ
// 最大値でクランプ
coc = clamp(coc, -maxCoc, maxCoc);
```

**ArtifactStudio 向けパラメータ:**
```cpp
struct DofParams {
    float focusDistance;   // ピント位置
    float cocScale;        // CoC の大きさ (絞り値に相当)
    float maxCoc;          // 最大CoC半径 [px] (Filament: 24-32)
    float cameraNear;      // カメラのnear平面
    float cameraFar;       // カメラのfar平面
};
```

---

## Filament 方式: リングブラー (Ring Blur)

```
リング数 = デスクトップ5, モバイル3
リングあたりのサンプル数 = 解像度に応じて可変

各ピクセル:
  coc = texture(cocTex, uv).r  (手前), .g (奥)
  for ring in 0..ringCount:
    サンプル半径 = (ring + 0.5) / ringCount * coc
    for sample in 0..samplesPerRing:
      角度 = (sample + halton(ring)) * 2π / samplesPerRing
      color += texture(input, uv + 半径 * vec2(cos, sin))
  color /= totalSamples
```

**Halton 系列**による回転オフセットでリング間のエイリアス低減:
```cpp
float halton(i, base) {
    i += 409;  // スキップで平均0.5に近づける
    float f = 1.0, r = 0.0;
    while (i > 0) { f /= base; r += f * (i % base); i /= base; }
    return r;
}
```

---

## シェーダー構造

### Pass 1: CoC生成
```glsl
// 入力: 深度テクスチャ
// 出力: RGBA8 (R=手前CoC, G=奥CoC, BA=未使用)

void main() {
    float depth = texture(s_depth, uv).r;
    float linearDepth = cameraNear * cameraFar /
        (cameraFar - depth * (cameraFar - cameraNear));
    float coc = (linearDepth - focusDistance) * cocScale;
    // 手前/奥を別チャンネルに分離
    gl_FragColor = vec4(max(0.0, -coc),  // 手前ボケのCoC半径
                        max(0.0,  coc),  // 奥ボケのCoC半径
                        0.0, 0.0);
}
```

### Pass 2: 手前ボケ
```glsl
// 入力: HDR画像 + CoCテクスチャ
// 出力: 手前ボケのみの画像
// CoC半径に応じた可変カーネル

void main() {
    float coc = texture(s_coc, uv).r;
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;

    for (int ring = 0; ring < RING_COUNT; ring++) {
        float radius = (float(ring) + 0.5) / float(RING_COUNT) * coc;
        int samples = max(1, int(radius * 2.0 * PI) / RING_COUNT);
        for (int s = 0; s < samples; s++) {
            float angle = (float(s) + halton(ring, 2)) / float(samples) * 2.0 * PI;
            vec2 offset = radius * vec2(cos(angle), sin(angle));
            float w = 1.0;  // 均等重み (またはガウシアン)
            color += texture(s_input, uv + offset).rgb * w;
            totalWeight += w;
        }
    }
    // 背景色でfillがない場合の補完
    vec4 center = texture(s_input, uv);
    color /= max(totalWeight, 0.0001);
    gl_FragColor = vec4(color, 1.0);
}
```

### Pass 3: 奥ボケ
- Pass 2 と同一ロジック、入力 CoC チャンネルが G になるだけ

### Pass 4: 合成
```glsl
// 入力: 元画像 + 手前ボケ + 奥ボケ + CoC
// CoC半径に応じて alpha = smoothstep(0, maxCoc, abs(coc)) でブレンド

void main() {
    vec4 original = texture(s_input, uv);
    vec4 nearBlur = texture(s_near, uv);
    vec4 farBlur  = texture(s_far, uv);
    float coc = texture(s_coc, uv).r - texture(s_coc, uv).g; // 手前-奥

    float alpha = smoothstep(0.0, maxCoc, abs(coc));
    vec4 blurred = (coc < 0.0) ? nearBlur : farBlur;
    gl_FragColor = mix(original, blurred, alpha);
}
```

---

## ArtifactStudio DiligentEngine 実装方針

| Pass | シェーダー | 入力 | 出力 | 備考 |
|---|---|---|---|---|
| CoC計算 | 1 FS | 深度テクスチャ | RG8 (near/far CoC) | linearizeDepth必須 |
| 手前ボケ | 1 FS (ループ) | HDR + CoC | RGBA16F (1/4サイズ) | リング数3-5、ダウンスケールで高速化 |
| 奥ボケ | 同上 | HDR + CoC | RGBA16F (1/4サイズ) | |
| アップスケール+合成 | 1 FS | 元画像 + Near + Far + CoC | バックバッファ | bilinear アップスケール + alphaブレンド |

**最適化ポイント:**
- ボケパスは 1/2 または 1/4 解像度で計算 → アップスケール時に bilinear 補間
- リングサンプル数は CoC 半径に比例（小さいボケは少ないサンプル）
- Halton 系列はシェーダー定数テーブルとしてプリ計算
- maxCoc = 24-32px (Filament 基準)
