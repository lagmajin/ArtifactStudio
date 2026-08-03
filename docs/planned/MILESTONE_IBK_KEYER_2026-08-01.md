# IBKキーヤー 実装マイルストーン

**日付**: 2026-08-01
**ベース**: Nuke IBK (Image-Based Keyer) の基本アルゴリズム
**現状**: `ChromaKeyEffect` が単純なクロマキー実装（色距離ベース）として存在

---

## IBKとは

**IBK (Image-Based Keyer)** = Nuke に搭載されている差分ベースのキーヤー。
- クリーンプレート（背景のみの画像）と前景画像の差分からアルファマットを生成
- 照明変動・ムラのある背景でも高品質なキーイングが可能
- クロマキー（色ベース）より汎用的。グリーンバックがなくても使える

---

## Phase 1: コアアルゴリズム（ファイル変更のみ）

### Step 1.1 — IBK コア構造体（新規ヘッダ、最小限）
**新規**: `ArtifactCore/include/ImageProcessing/Keying/IBKKeyer.ixx`

```cpp
export module Core.ImageProcessing.Keying.IBKKeyer;

export namespace ArtifactCore::Keying {

struct IBKParams {
    float screenCorrection = 1.0f;   // スクリーン補正量
    float coreMatteClip = 0.5f;      // コアマットクリップ
    float edgeMatteSoftness = 0.2f;  // エッジマットの柔らかさ
    float despillStrength = 0.5f;    // スピル除去強度
    float garbageMatteGamma = 1.0f;  // ガベージマットのガンマ
    float detailRecovery = 0.3f;     // ディテール復元量
    int erodePixels = 1;             // エロードピクセル数
    int dilatePixels = 3;            // ダイレートピクセル数
};

struct IBKBuffers {
    const float* foreground = nullptr; // RGBA float32
    const float* cleanPlate = nullptr; // RGBA float32
    float* outputRGBA = nullptr;       // RGBA float32 出力
    int width = 0, height = 0;
};

} // namespace ArtifactCore::Keying
```

### Step 1.2 — コアアルゴリズム実装
**新規**: `ArtifactCore/src/ImageProcessing/Keying/IBKKeyer.cppm`

```
1. スクリーン補正
   screenCorrected = foreground * screenCorrection

2. 差分マット計算
   per-pixel: プレートとの差分 (RGB distance)
   rawMatte = 1.0 - exp(-distance / sigma)

3. コアマット
   coreMatte = clamp(rawMatte - coreMatteClip, 0, 1)

4. エッジマット
   edgeMatte = rawMatte * (1.0 - coreMatte)
   edgeMatte = smoothstep(0, edgeMatteSoftness, edgeMatte)

5. 合成マット
   alpha = coreMatte + edgeMatte * detailRecovery

6. スピル除去
   despill = screenCorrected - cleanPlate * despillStrength
   despill = max(despill, 0)

7. ガベージマット（任意）
   alpha = pow(alpha, garbageMatteGamma)

8. モルフォロジー
   alpha = erode(alpha, erodePixels)
   alpha = dilate(alpha, dilatePixels)

9. 出力
   output.rgb = despill.rgb * alpha
   output.a = alpha
```

### Step 1.3 — エフェクトラッパー
**新規**: `Artifact/include/Effects/Keying/IBKKeyerEffect.ixx` + `Artifact/src/Effects/Keying/IBKKeyerEffect.cppm`

既存の `ChromaKeyEffect` の隣に配置。`ArtifactAbstractEffect` を継承し、プロパティとして `IBKParams` の各パラメータを露出。

---

## Phase 2: GPU 高速化

### Step 2.1 — HLSL コンピュートシェーダー
**新規**: `ArtifactCore/include/Graphics/Shader/Compute/HLSL/IBKKeyer.ixx` + 対応 `.hlsl`

```
[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
    // Phase 1 と同じアルゴリズムをGPUで実行
}
```

### Step 2.2 — GPUパイプライン統合
`Graphics/Shader/Compute/IBKKeyerPipeline.ixx` を作成し、`ArtifactEffectImplBase` のGPUパスに配線。

---

## Phase 3: クリーンプレート自動生成

### Step 3.1 — 時間中央値フィルタ
静止背景を仮定し、複数フレームの中央値からクリーンプレートを自動生成。

```cpp
void autoGenerateCleanPlate(
    const std::vector<ImageF32x4_RGBA>& frames,
    ImageF32x4_RGBA& outCleanPlate);
```

---

## Phase 4: UI

### Step 4.1 — IBK プロパティパネル
`ArtifactInspectorWidget` に IBK パラメータの UI 追加。

### Step 4.2 — VP プレビュー
- マット表示モード（白黒アルファ）
- スクリーン補正前/後 切り替え
- クリーンプレート指定 UI（レイヤーピッカー）

---

## ファイル一覧（全フェーズ）

| ファイル | 新規/変更 | 内容 |
|----------|----------|------|
| `ArtifactCore/include/ImageProcessing/Keying/IBKKeyer.ixx` | 新規 | IBK 構造体・インターフェース |
| `ArtifactCore/src/ImageProcessing/Keying/IBKKeyer.cppm` | 新規 | CPU 実装 |
| `Artifact/include/Effects/Keying/IBKKeyerEffect.ixx` | 新規 | エフェクトラッパー |
| `Artifact/src/Effects/Keying/IBKKeyerEffect.cppm` | 新規 | エフェクトラッパー実装 |
| `ArtifactCore/include/Graphics/Shader/Compute/HLSL/IBKKeyer.ixx` | 新規 | HLSL 宣言 |
| `shaders/IBKKeyer.hlsl` | 新規 | GPU シェーダー |
| `ArtifactCore/include/Graphics/Shader/Compute/IBKKeyerPipeline.ixx` | 新規 | GPU パイプライン |
| `ArtifactCore/src/Graphics/Shader/Compute/IBKKeyerPipeline.cppm` | 新規 | GPU パイプライン実装 |
| `ArtifactService/ArtifactEffectService.cppm` | 変更 | IBK 登録 |
| `ArtifactInspectorWidget` | 変更 | プロパティ UI |

---

## 検証チェックリスト

- [ ] グリーンバック素材で単純クロマキーより高品質なアルファマット
- [ ] 照明ムラのある背景でもキレイに抜ける
- [ ] クリーンプレート自動生成が動作する（静止背景）
- [ ] スピル除去で緑カブリが消える
- [ ] GPU パスが CPU パスと同一結果
- [ ] プロパティ変更がリアルタイムにVPプレビュー反映
