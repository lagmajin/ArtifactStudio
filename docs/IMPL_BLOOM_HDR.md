# Bloom 実装詳細参照書

> 参照元: **bgfx `examples/09-hdr`** (BSD-2) + **Filament `PostProcessManager.cpp`** (Apache 2.0)
> 対応マイルストーン: `docs/GAME_ENGINE_RENDERING_REFERENCE.md`

---

## パイプライン全景

```
HDR入力画像 (RGBE8 / RGBA16F)
    │
    ├─→ [輝度抽出]  3x3 ボックスフィルタ → 平均輝度テクスチャ (R8)
    │       │
    │       └─→ [輝度ダウンスケール] 4x4 → 4x4 → ... → 1x1 (平均シーン輝度)
    │
    ├─→ [ブライトパス] 元画像を3x3ボックス → 閾値減算 → 輝度正規化 → Reinhard
    │       │
    │       └─→ [ブラー] 9tap ガウシアン (水平+垂直 2-pass または単一)
    │
    └─→ [最終合成] 元画像(Yxy変換) → Reinhardトーンマップ → +0.6*ぼかしBloom → ガンマ補正
```

---

## シェーダー実装

### Pass 1: 輝度抽出 (`fs_hdr_lum`)

```glsl
// 入力: HDRテクスチャ (RGBE8)
// 出力: 輝度 (R8, encodeRE8)
// 3x3 ボックスフィルタで平均輝度を1/9に圧縮

uniform vec4 u_offset[16];  // 3x3 サンプルオフセット (du, dv)

void main() {
    vec3 rgb0 = decodeRGBE8(texture(s_texColor, uv + u_offset[0].xy));
    vec3 rgb1 = decodeRGBE8(texture(s_texColor, uv + u_offset[1].xy));
    // ... 9サンプル
    float avg = (luma(rgb0) + luma(rgb1) + ... + luma(rgb8)) / 9.0;
    gl_FragColor = encodeRE8(avg);
}
```

**補助関数:**
```glsl
// RGBE8 → float3
vec3 decodeRGBE8(vec4 rgbe) {
    return rgbe.rgb * exp2(rgbe.a * 255.0 - 128.0);
}

// float → R8E (輝度のみ)
vec4 encodeRE8(float f) {
    float e = ceil(log2(f));
    return vec4(f / exp2(e), 0, 0, (e + 128.0) / 255.0);
}

// 輝度計算 (BT.709)
vec4 luma(vec3 rgb) {
    return vec4(dot(rgb, vec3(0.2126, 0.7152, 0.0722)), 1.0);
}
```

### Pass 2: 輝度ダウンスケール (`fs_hdr_lumavg`)

```glsl
// 4x4 → 次のレベルへ再帰的にダウンスケール
// bgfx の実装: 256→64 (level0), 64→16 (level1), 16→4 (level2), 4→1 (level3)
// 各段階で4x4=16サンプルの平均を取る

uniform vec4 u_offset[16];  // 4x4 サンプルオフセット

void main() {
    float avg = 0.0;
    for (int i = 0; i < 16; i++) {
        avg += decodeRE8(texture(s_texColor, uv + u_offset[i].xy));
    }
    avg /= 16.0;
    gl_FragColor = encodeRE8(avg);
}
```

### Pass 3: ブライトパス (`fs_hdr_bright`)

```glsl
// 入力: 元HDR画像 + 平均輝度テクスチャ
// 出力: 閾値以上の明るい部分のみ (gamma補正済)

uniform vec4 u_tonemap; // x=middleGray, y=whiteSqr, z=threshold, w=offset

void main() {
    float lum = clamp(decodeRE8(texture(s_texLum, uv)), 0.1, 0.7);

    vec3 rgb = vec3(0.0);
    for (int i = 0; i < 9; i++)  // 3x3 ボックス
        rgb += decodeRGBE8(texture(s_texColor, uv + u_offset[i].xy));
    rgb /= 9.0;

    float middleGray = u_tonemap.x;
    float whiteSqr   = u_tonemap.y;
    float threshold  = u_tonemap.z;

    // 閾値減算 → 輝度で正規化 → Reinhard → gamma
    rgb = max(vec3(0.0), rgb - threshold) * middleGray / (lum + 0.0001);
    rgb = reinhard2(rgb, whiteSqr);
    gl_FragColor = toGamma(vec4(rgb, 1.0));
}
```

**Reinhard トーンマッピング:**
```glsl
float reinhard2(float lp, float whiteSqr) {
    return (lp * (1.0 + lp / whiteSqr)) / (1.0 + lp);
}
```

### Pass 4: ブラー (`fs_hdr_blur`)

```glsl
// 入力: ブライトパス出力
// 出力: ぼかし済み
// bgfx 方式: 頂点シェーダーが5つのtexcoord (中心+4オフセット) を出力
// blur9() は9tapガウシアン (v_texcoord0中心, v_texcoord1-4が左右/上下対称)

vec4 blur9(sampler2D tex, vec2 uv0, vec2 uv1, vec2 uv2, vec2 uv3, vec2 uv4) {
    vec4 color = texture(tex, uv0) * 0.2270270270;    // 中心
    color += texture(tex, uv1) * 0.3162162162;        // ±1
    color += texture(tex, uv2) * 0.3162162162;
    color += texture(tex, uv3) * 0.0702702703;        // ±2
    color += texture(tex, uv4) * 0.0702702703;
    return color;
}
```

**ArtifactStudio 最適化**: 2-pass separable (水平→垂直) に分離してタップ数を O(N²)→O(2N) に削減。

### Pass 5: 最終合成 (`fs_hdr_tonemap`)

```glsl
// 入力: 元HDR画像 + 平均輝度 + ぼかしBloom
// 出力: 最終画像 (gamma補正済)

void main() {
    vec3 rgb = decodeRGBE8(texture(s_texColor, uv));
    float lum = clamp(decodeRE8(texture(s_texLum, uv)), 0.1, 0.7);

    // Yxy 色空間でトーンマッピング (輝度のみ操作、色度保存)
    vec3 Yxy = convertRGB2Yxy(rgb);
    float lp = Yxy.x * middleGray / (lum + 0.0001);
    Yxy.x = reinhard2(lp, whiteSqr);
    rgb = convertYxy2RGB(Yxy);

    // ぼかしBloomを加算
    vec4 blur = blur9(s_texBlur, uv0, uv1, uv2, uv3, uv4);
    rgb += 0.6 * blur.xyz;

    gl_FragColor = toGamma(vec4(rgb, 1.0));
}
```

---

## CPU 側のセットアップ (bgfx C++)

```cpp
// レンダーターゲットチェーン
m_fbtextures[2];      // HDR カラー (2枚で ping-pong)
m_lum[5];             // 輝度ピラミッド [0]=256, [1]=64, [2]=16, [3]=4, [4]=1
m_bright;             // ブライトパス出力
m_blur;               // ブラー出力

// パイプライン
// 1. シーンを m_fbtextures[0] にHDR描画
// 2. m_fbtextures[0] → m_lum[0] (輝度抽出 3x3)
// 3. m_lum[0] → m_lum[1] → m_lum[2] → m_lum[3] → m_lum[4] (4x4 ダウンスケール)
// 4. m_fbtextures[0] + m_lum[4] → m_bright (ブライトパス)
// 5. m_bright → m_blur (ブラー 9tap)
// 6. m_fbtextures[0] + m_lum[4] + m_blur → バックバッファ (最終合成)
```

---

## Filament 方式 (高品質版の参考)

```
kMaxBloomLevels = 12 (最大12段のダウンスケールピラミッド)

ダウンサンプル:
  - Karis 平均 (13-tap) で各レベルを生成
  - 各レベルで前レベルの 1/2 解像度

アップサンプル (Kawase フィルタ):
  - 小さい解像度から順にアップサンプル + 加算合成
  - 各段で双線形フィルタ

最終合成:
  - 全レベルのアップサンプル結果を元画像に加算
```

---

## ArtifactStudio DiligentEngine 実装方針

| Pass | シェーダー | 入力 | 出力 | 備考 |
|---|---|---|---|---|
| Lum Extract | 1つのフラグメントシェーダー | HDR入力 | R16F 1/9サイズ | 3x3 box |
| Lum Downscale | 同上ループ | R16F | R16F 1/16ずつ | 4x4 box, 計4段 |
| Bright Pass | 同上 | HDR + Lum1x1 | RGBA8 1/2サイズ | 閾値+Reinhard |
| Blur H | separableシェーダー | Bright | RGBA8 1/2サイズ | 9tap horizontal |
| Blur V | separableシェーダー | Blur H | RGBA8 1/2サイズ | 9tap vertical |
| Composite | 同上 | HDR + Lum1x1 + Blur | バックバッファ | Yxy TM + Bloom加算 + Gamma |
