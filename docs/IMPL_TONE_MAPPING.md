# トーンマッピング 実装詳細参照書

> 参照元: **bgfx `09-hdr/fs_hdr_tonemap.sc`** (BSD-2) + **Filament** (Apache 2.0)

---

## 主要3方式

### 1. Reinhard (bgfx 版) — 最もシンプル

```glsl
float reinhard2(float lp, float whiteSqr) {
    return (lp * (1.0 + lp / whiteSqr)) / (1.0 + lp);
}
// 使用時:
//   lp = pixelLuminance * middleGray / avgSceneLum
//   result = reinhard2(lp, whiteSqr)
//
// パラメータ:
//   middleGray = 0.18 (標準), 調整可
//   white = 1.0～4.0 (白の飽和点)
```

**Yxy 色空間での適用 (色度保存):**
```glsl
vec3 rgb = texture(hdrInput, uv).rgb;
float lum = texture(avgLum, vec2(0.5)).r;

// RGB → Yxy
vec3 Yxy;
Yxy.x = dot(rgb, vec3(0.2126, 0.7152, 0.0722));  // BT.709 輝度
Yxy.y = rgb.r / (rgb.r + rgb.g + rgb.b);           // 色度x
Yxy.z = rgb.g / (rgb.r + rgb.g + rgb.b);           // 色度y

// トーンマッピングは輝度のみ
Yxy.x = reinhard2(Yxy.x * middleGray / lum, white * white);

// Yxy → RGB
float x = Yxy.x / Yxy.y * Yxy.z;
rgb.r = Yxy.y * x;
rgb.b = x - rgb.r - Yxy.z * x;
rgb.g = Yxy.x - rgb.r - rgb.b;
```

### 2. ACES (Filament 版) — 映画品質

```glsl
// ACES RRT + ODT 近似 (Krzysztof Narkowicz 版)
vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
```

### 3. Filmic (Unreal 系)

```glsl
// Jim Hejl & Richard Burgess-Dawson 版
vec3 filmic(vec3 x) {
    vec3 X = max(vec3(0.0), x - 0.004);
    return (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
}
```

---

## 実装選択

| 方式 | 品質 | コスト | おすすめ用途 |
|---|---|---|---|
| Reinhard | ★★☆ | 最小 | リアルタイムプレビュー |
| ACES | ★★★ | 低 | 本番出力のデフォルト |
| Filmic | ★★☆ | 低 | シネマティックルック |

---

## ArtifactStudio 実装方針

```glsl
// シェーダー定数
uniform float u_tonemapMode;   // 0=Reinhard, 1=ACES, 2=Filmic
uniform float u_exposure;      // 露出補正
uniform float u_middleGray;    // 0.18 標準
uniform float u_whitePoint;    // 白レベル

// HDR → LDR
vec3 toneMap(vec3 hdr) {
    hdr *= u_exposure;

    vec3 ldr;
    switch (int(u_tonemapMode)) {
    case 0: ldr = reinhard2(hdr, u_whitePoint * u_whitePoint); break;
    case 1: ldr = aces(hdr); break;
    case 2: ldr = filmic(hdr); break;
    }

    // 最終ガンマ補正 (sRGB 出力時)
    return pow(ldr, vec3(1.0 / 2.2));
}
```
