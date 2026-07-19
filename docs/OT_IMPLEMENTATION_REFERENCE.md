# OpenToonz → ArtifactStudio 実装参照ガイド

> 参照元: OpenToonz `stdfx/` (BSD 3-Clause)
> ArtifactStudio 側ライセンス: `docs/THIRD_PARTY_NOTICES.md` に記載済

---

## 全体像：OpenToonz と ArtifactStudio の決定的な違い

| レイヤー | OpenToonz | ArtifactStudio |
|---|---|---|
| レンダリング | CPU タイル (`doCompute(TTile)`) | GPU (DiligentEngine / DX12) |
| 色空間 | sRGB 前提、float ラスター時に線形変換 | GPU ネイティブ線形、シェーダー内 sRGB 変換 |
| ピクセル処理 | `for (y) for (x)` の二重ループ | フラグメントシェーダー / Compute Shader |
| ブラー | 可分離ボケ (CPU O(N)) | GPU ガウシアン (2-pass separable) |
| FFT | kiss_fft (CPU) | 不要。畳み込みは GPU ブラーで代用 |
| パラメータ | `TDoubleParamP` (キーフレーム補間内蔵) | 既存の Property システム経由 |
| エフェクト登録 | `FX_PLUGIN_DECLARATION` + `TLIBMAIN` DLL | C++20 module の Effect 登録パターン |

**基本方針**: アルゴリズムの「考え方」は OpenToonz を参考にし、実装はすべて GPU ネイティブで書き直す。



---

## 1. クロマキー (RGB Key / HSV Key)

### 参照元
- `rgbkeyfx.cpp` (145行, 非常にコンパクト)
- `hsvkeyfx.cpp` (129行)

### OpenToonz の処理フロー

```
入力画像 → doCompute(TTile) → for(y)for(x) 全ピクセル走査 → 条件判定 → Transparent or そのまま
```

**アルゴリズムコア (RGB Key):**
```cpp
bool condition = pix->r >= lowColor.r && pix->r <= highColor.r &&
                 pix->g >= lowColor.g && pix->g <= highColor.g &&
                 pix->b >= lowColor.b && pix->b <= highColor.b;
if (condition != gender) *pix = Transparent;
```

**アルゴリズムコア (HSV Key):**
```cpp
OLDRGB2HSV(pix->r, pix->g, pix->b, &h, &s, &v);
bool condition = h>=lowH && h<=highH && s>=lowS && s<=highS && v>=lowV && v<=highV;
if (condition != gender) *pix = Transparent;
```

### ArtifactStudio 向け変換

| OpenToonz | ArtifactStudio (GPU) |
|---|---|
| ピクセルループ | フラグメントシェーダー |
| `TPixel32` 固定 | `float4` RGBA (線形空間) |
| 0/1 硬判定 | ソフトキー: 範囲端で `smoothstep` によるグラデーションアルファ |
| `gender` (invert) | シェーダー内で `condition = !condition` |

疑似 GLSL (HSV Key + ソフトエッジ):
```glsl
float4 ChromaKey_HSV(float4 src, float3 keyHSV, float3 range, float soft, bool inv) {
    float3 hsv = RGBtoHSV(src.rgb);
    float3 dist = abs(hsv - keyHSV);
    dist.x = min(dist.x, 1.0 - dist.x); // 色相循環
    float a = 1.0 - smoothstep(range - soft, range + soft, dist);
    a = min(a.x, min(a.y, a.z));
    if (inv) a = 1.0 - a;
    return float4(src.rgb, src.a * a);
}
```

### 拡張ポイント
1. スピル抑制: `src.rgb -= keyColor * spill * (1-alpha)`
2. デスピル: エッジのにじみを周辺平均色相で補正
3. 複数キー色: 複数回キーイングを合成


---

## 2. 照明エフェクト群

### 2-1. TargetSpot（スポットライト）

参照元: `targetspotfx.cpp` (131行)

OpenToonz の数学モデル（円錐形スポットライト）:
```cpp
double zz = (-tan(angle) * pos.x + z)^2;
double distxy = pos.x^2/sizex + pos.y^2/sizey;  // 楕円スポット
double dist = distxy - zz;
double norm = sqrt(distxy + zz);

if (dist < 0 && lightAxis > 0)
    pixel = blend(color, black, clamp(norm*decay));
else if (dist < reference && lightAxis > 0)
    pixel = blend(blend(color,black,norm*decay), black, dist/reference);
else
    pixel = black;
```

純粋な数式なのでシェーダーにそのまま翻訳可能。

### 2-2. Raylit（光線）

参照元: `raylitfx.cpp` (257行)

入力画像のアルファを深度マップとみなし、点光源(p, z)から光線を飛ばす。
`TRop::raylit()` / `TRop::glassRaylit()` でライティング計算。

パラメータ: p(光源2D位置), z(光源高さ), intensity, decay, smoothness, radius, includeInput

疑似 GLSL:
```glsl
float3 Raylit(sampler2D src, float2 uv, float3 lightPos, float intensity, float decay) {
    float4 c = texture(src, uv);
    float depth = c.a;
    float2 toLight = lightPos.xy - uv;
    float dist = length(toLight);
    float attenuation = intensity / (1.0 + decay * dist);
    float shadow = smoothstep(0.0, 0.1, depth - lightPos.z / dist);
    return c.rgb * attenuation * shadow;
}
```

### 2-3. BodyHighlight（擬似3Dハイライト）

参照元: `bodyhighlightfx.cpp` (536行)

処理: 入力画像 → アルファ抽出 → 可分離ボケ → 法線マップ化 → Phong ハイライト → ブレンド合成

特筆: 可分離ボケが O(rows*cols) のスライディングウィンドウで実装されている。

ArtifactStudio 変換案:
```glsl
// Pass 1: アルファ → 2-pass ガウシアンブラー
// Pass 2: Sobel 勾配 → 法線 → ハイライト
float2 grad;
grad.x = texelFetchOffset(blur, uv, 0, ivec2( 1,0)).r
       - texelFetchOffset(blur, uv, 0, ivec2(-1,0)).r;


---

## 3. レンズグレア（分光グレア）

参照元: `iwa_glarefx.cpp` (689行), `iwa_glarefx.h`, `iwa_cie_d65.h`, `iwa_xyz.h`

### OpenToonz の処理フロー

```
Source 画像 ──→ FFT ─┐
                      ├─→ 周波数領域で乗算 → 逆FFT → 出力
Iris 虹彩画像 → FFT ─┘
                      ↑
           34波長でスケーリング & XYZ 累積
```

### 核心：分光計算

```cpp
for (int ram = 0; ram < 34; ram++) {
    double rambda = 0.38 + 0.01 * ram;  // 0.38～0.71μm
    double scale = std::pow(0.55 / rambda, aberration);
    for (各ピクセル) {
        double gl = lerp(glarePattern, i_scaled, j_scaled) * intensity_scale;
        g_xyz->x += gl * cie_d65[ram] * xyz[ram*3+0];
        g_xyz->y += gl * cie_d65[ram] * xyz[ram*3+1];
        g_xyz->z += gl * cie_d65[ram] * xyz[ram*3+2];
    }
}
// XYZ → sRGB
R =  3.240479*X - 1.537150*Y - 0.498535*Z
G = -0.969256*X + 1.875992*Y + 0.041556*Z
B =  0.055648*X - 0.204043*Y + 1.057311*Z
```

### ArtifactStudio 変換：FFT → GPU 畳み込み

OpenToonz は FFT で畳み込み定理を使っているが、空間領域の直接畳み込みと等価。

```
OpenToonz: Source FFT × Iris FFT → 逆FFT
ArtifactStudio: Source ⊛ Iris_kernel（空間領域畳み込み）
```

パイプライン設計案:
1. Iris → 34波長分のスケーリング済みカーネル群を初期化時1回計算
2. Source × カーネル群 → 波長別畳み込み
3. XYZ 累積 → sRGB 出力

必要なデータ（シェーダー定数として焼き込み）:
- `cie_d65[34]` — CIE D65 光源スペクトル (380-710nm)
- `xyz[34*3]` — CIE 1931 等色関数
- ともに `iwa_cie_d65.h` / `iwa_xyz.h` から持ってこれる

### 虹彩形状

OpenToonz は QPainter で毎フレーム描画 → ArtifactStudio ではプリセットテクスチャ化:
| 形状 | 実装 |
|---|---|
| 正方形 (4 streaks) | テクスチャ |
| 六角形 (6 streaks) | テクスチャ |
| 八角形 (8 streaks) | テクスチャ |
| ギア形状 (可変) | ランタイム生成 or 事前計算（要検討） |
| 入力画像モード | 任意テクスチャを Iris に |

ノイズ分散は Fractal Noise を先に実装すれば再利用可。

### 2-4. Backlit（逆光）

参照元: `backlit.cpp` (178行), `backlitfx.cpp` (300行)

2バリエーション:

| ファイル | アルゴリズム |
|---|---|
| `backlit.cpp` | 光源中心から放射状にアルファ蓄積 → エッジ発光 |
| `backlitfx.cpp` | light+lighted の2入力 → 光源ぼかし → 被写体にオーバーレイ |

`backlitfx.cpp` の2入力モデルは ArtifactStudio の多入力エフェクト設計の参考になる。



---

## 4. Fractal Noise（補足）

参照元: `iwa_fractalnoisefx.cpp`, `iwa_noise1234.cpp`, `iwa_simplexnoise.cpp`

AE の Fractal Noise と完全互換のパラメータ設計:

| パラメータ | 値 |
|---|---|
| Fractal Type | Basic, Turbulent Smooth/Basic/Sharp, Dynamic, Dynamic Twist, Max, Rocky |
| Noise Type | Block, Smooth |
| Scale / Scale W/H | 均一 or 幅/高さ別 |
| Complexity | オクターブ数 (1-10) |
| Sub Influence | サブオクターブの影響度 (0-100%) |
| Sub Scaling | サブスケーリング (0-100%) |
| Evolution | 時間進化 |
| Cycle Evolution | ループ進化 |

シェーダーでの Simplex/Perlin ノイズは成熟手法のため移植は比較的容易。

---

## 実装優先順位

| 順位 | エフェクト | 難度 | 理由 |
|---|---|---|---|
| 1 | HSV Key (+ソフトキー) | 低 | シェーダー1パス・即実装可 |
| 2 | TargetSpot | 低 | 純粋な数式・シェーダー翻訳のみ |
| 3 | Raylit | 中 | 深度マップライティング・要デザイン |
| 4 | Fractal Noise | 中 | パラメータ多いがノイズ関数は確立済 |
| 5 | BodyHighlight | 中 | 2-pass ブラー+Sobel で実現可 |
| 6 | Backlit | 低-中 | 2入力モデル・設計参考にも |
| 7 | Glare (分光) | 高 | Iris テクスチャ生成+波長別畳み込み+XYZ変換。Fractal Noise が先にあるとノイズ分散部分を再利用可 |


---

## 追加参照: アーキテクチャパターン

### 5. ShaderFx — ユーザーカスタム GLSL シェーダー枠組み

参照元: `shaderinterface.cpp`/`.h`, `shaderfx.cpp`, `shadingcontext.cpp`

**これは何か**: GLSL シェーダーを XML で定義し、パラメータを型安全にバインドして
Toonz のエフェクトとして動作させる枠組み。

**パラメータ型マッピング** (XML → GLSL uniform):
| XML 型 | GLSL 型 | UI 概念 |
|---|---|---|
| `bool` | `bool` | — |
| `float` | `float` | percent, length, angle |
| `vec2/3/4` | `vec2/3/4` | point, size, vector |
| `int` | `int` | — |
| `rgba` | `vec4` | カラーピッカー |
| `rgb` | `vec3` | カラーピッカー |

**UI コンセプト** (パラメータの視覚的意味):
```
PERCENT, LENGTH, ANGLE, POINT, RADIUS_UI, WIDTH_UI, ANGLE_UI,
POINT_UI, XY_UI, VECTOR_UI, POLAR_UI, SIZE_UI, QUAD_UI, RECT_UI, COMPASS_UI
```

**ArtifactStudio への示唆**:
- 「カスタムシェーダーエフェクト」機能の設計参考
- パラメータ型 → UI ウィジェットのマッピング表がそのまま使える
- XML ではなく ArtifactStudio の Property システム上に乗せる設計が良い

---

### 6. TangentFlow — エッジ方向ベクトル場計算

参照元: `iwa_tangentflowfx.cpp` (485行)

**これは何か**: 入力画像からエッジの接線方向ベクトル場を計算するエフェクト。
後段の FlowBlur / FlowPaintBrush に渡す中間データとして機能。

**アルゴリズム**:
```
入力画像 → 5x5 Sobel フィルタ → 勾配ベクトル場 → 90度回転 → 接線ベクトル場
         → 空領域を Fast Sweeping Method で補完
         → 反復平滑化 (kernelRadius 回)
         → 出力 (R=方向X, G=方向Y, B=勾配強度)
```

**5x5 Sobel カーネル** (90度回転済み = 接線方向):
```
kernel_x = [[ 0.25, 0.4, 0.5, 0.4, 0.25],    kernel_y = [[-0.25,-0.2, 0, 0.2, 0.25],
            [ 0.2,  0.5, 1.0, 0.5, 0.2 ],                [-0.4, -0.5, 0, 0.5, 0.4 ],
            [ 0,    0,   0,   0,   0   ],                [-0.5, -1.0, 0, 1.0, 0.5 ],
            [-0.2, -0.5,-1.0,-0.5,-0.2 ],                [-0.4, -0.5, 0, 0.5, 0.4 ],
            [-0.25,-0.4,-0.5,-0.4,-0.25]]                [-0.25,-0.2, 0, 0.2, 0.25]]
```

**ArtifactStudio への示唆**:
- シェーダー化は容易 (Sobel は定番)
- 出力を中間テクスチャとして FlowBlur / FlowPaint に流すパイプライン設計の参考
- 「方向ブラー」や「ストローク風エフェクト」の基盤

---

### 7. FX 階層とキャッシュキー設計

参照元: `unaryFx.cpp`, `binaryFx.cpp`, `zeraryFx.cpp`, `trasterfx.h`

**クラス階層**:
```
TSmartObject + TPersist + TParamObserver
  └── TFx                          ← 全エフェクトの基底
        ├── TRasterFx              ← ラスターエフェクト (doCompute 定義)
        │     ├── TStandardRasterFx ← 標準ラスターエフェクト
        │     ├── TBaseRasterFx
        │     └── TZeraryFx        ← 入力なしの生成エフェクト
        └── TGeometryFx            ← アフィン変換を子に伝播する特殊FX
```

**キャッシュキー生成 (`getAlias`)**:
```cpp
std::string getAlias(frame, info) const {
    std::string alias = getFxType() + "[";
    for (各入力ポート) {
        if (接続あり) alias += 子のgetAlias();
        alias += ",";
    }
    // + 全パラメータ値 + アフィン係数
    return alias + param1 + "," + param2 + ... + "]";
}
```

**ArtifactStudio への示唆**:
- **キャッシュ無効化の決定論的キー設計**。全パラメータ値 + 全入力の再帰的キー結合
- `TGeometryFx` のパターン: アフィン変換を `TRenderSettings` に乗せて子に渡すだけ。
  ArtifactStudio の Transform エフェクトの設計参考。

---

### 8. 画像 I/O プラグイン一覧

参照元: `image/` ディレクトリ (20フォーマット)

| フォーマット | ディレクトリ | 備考 |
|---|---|---|
| 3GP (動画) | `3gp/` | |
| AVI | `avi/` | |
| BMP | `bmp/` | |
| EXR | `exr/` | HDR |
| FFmpeg | `ffmpeg/` | 汎用動画 |
| MOV | `mov/` | QuickTime |
| PNG | `png/` | |
| PSD | `psd/` | Photoshop |
| SVG | `svg/` | ベクター |
| TGA | `tga/` | |
| TIFF | `tif/` | |
| TZL/TZM/TZP | `tzl/`,`tzm/`,`tzp/` | Toonz 独自 (レベル/メッシュ/パレット) |
| PLI | `pli/` | Toonz ベクター |
| Sprite | `sprite/` | |
| Quantel | `quantel/` | 放送用 |
| SGI | `sgi/` | Silicon Graphics |
| ZCC | `zcc/` | |

**ArtifactStudio への示唆**: FFmpeg 経由でほぼ全フォーマットをカバーできるため、
個別プラグインより FFmpeg 統合が現実的。参考としての一覧。

---

### 9. ColorFX スタイルエンジン

参照元: `colorfx/` (色スタイル、線スタイル、ジグザグスタイル)

**RubberDeform**: 物理ベースの有機的変形
```cpp
class RubberDeform {
    vector<T3DPointD> *m_pPolyOri;  // 元の制御点
    vector<T3DPointD> m_polyLoc;    // 変形後の制御点
    void deformStep();               // 内部エネルギー最小化
    double avgLength();              // 平均エッジ長
    void refinePoly(double rf);      // ポリゴン精細化
};
```

**ArtifactStudio への示唆**: 2D の物理ベース変形（布、髪、液体風）のシェーダー実装の参考。
頂点バッファ操作に置き換えれば GPU で高速化可能。

---

## 追加実装優先順位 (更新)

| 順位 | 項目 | 難度 | 備考 |
|---|---|---|---|
| 1 | HSV Key + ソフトキー | 低 | 前回と変わらず最優先 |
| 2 | TargetSpot | 低 | 数式→シェーダー |
| 3 | TangentFlow (Sobel ベクトル場) | 中 | Flow系エフェクトの基盤として価値大 |
| 4 | Raylit | 中 | 深度マップライティング |
| 5 | Fractal Noise | 中 | ノイズ基盤。Glare のノイズ分散部分と共用可 |
| 6 | BodyHighlight | 中 | 2-pass ブラー+Sobel |
| 7 | ShaderFx 風カスタムシェーダー | 中-高 | 設計フェーズ。XML→Property システムへの翻訳が必要 |
| 8 | Backlit | 低-中 | 2入力モデル |
| 9 | Glare (分光) | 高 | TangentFlow(Sobel)+FractalNoise が先 |


