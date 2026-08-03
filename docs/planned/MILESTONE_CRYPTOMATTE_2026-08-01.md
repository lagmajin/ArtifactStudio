# Cryptomatte 実装マイルストーン

**日付**: 2026-08-01
**ベース**: Cryptomatte Specification 1.3 by Psyop
**現状**: `ArtifactIRenderer::ChannelType::ObjectId` + `MaterialId` + `readbackToMultiChannelImage()` が既に実装。IDベースのマルチAOVレンダリング基盤あり。
**不足**: Cryptomatte の標準フォーマット（ID + coverage マニフェスト付き EXR）への変換が未実装。

---

## Cryptomatte とは

Psyop 社が開発したオープンスタンダードの ID マットシステム。

```
通常の Object ID:  ピクセル = 1つのID整数値（エイリアシングが発生する）
Cryptomatte:       ピクセル = { (ID値, coverage率) × Nサンプル }（アンチエイリアス対応）
```

**特徴**:
- ピクセルあたり複数のID + coverage ペアを持つ（部分的な遮蔽や半透明エッジに対応）
- EXR のマニフェストに ID → 名前のマッピングを埋め込む
- Nuke の Cryptomatte ノードで階層選択 → 即座にマスク生成
- Blender, Houdini, V-Ray, Arnold, Redshift など業界標準対応

---

## Phase 1: Cryptomatte データ構造

### Step 1.1 — CryptoPixel

**新規**: `ArtifactCore/include/Image/Cryptomatte/CryptoPixel.ixx`

```cpp
export module Core.Image.Cryptomatte.Pixel;

export namespace ArtifactCore::Image::Cryptomatte {

/// Float エンコードされた Cryptomatte ID
/// Cryptomatte は 32bit float の指数部 + 仮数部に ID + coverage をエンコードする
struct CryptoSample {
    float id;       // 32bit: [7bit exponent] [25bit id_lo]
    float coverage; // 通常は 0.0-1.0
    
    /// IDをマニフェスト名でルックアップできる 64bit ハッシュに変換
    static uint64_t idToHash(const QString& objectName);
};

/// 1ピクセルあたりの Cryptomatte データ（max 16サンプル/px）
class CryptoPixel {
public:
    CryptoPixel() = default;

    void addSample(float id, float coverage);
    void addSample(const QString& objectName, float coverage);

    void clear();
    int sampleCount() const;

    /// EXR 書き込み用：ランクごとに最大2サンプル
    /// Rank 0: 上位2サンプル (max coverage順)
    /// Rank 1: 3-4番目
    /// ... 
    void getSamplesForRank(int rank, std::vector<CryptoSample>& out) const;

    /// coverage 降順でソート
    void sort();

    /// 空かどうか
    bool isEmpty() const;

private:
    static constexpr int kMaxSamples = 16;
    CryptoSample samples_[kMaxSamples];
    int count_ = 0;
};

} // namespace
```

### Step 1.2 — CryptoImage

**新規**: `ArtifactCore/include/Image/Cryptomatte/CryptoImage.ixx`

```cpp
export module Core.Image.Cryptomatte.Image;

export namespace ArtifactCore::Image::Cryptomatte {

enum class CryptoLayer {
    Object,     // crypto_object
    Material,   // crypto_material
    Asset       // crypto_asset
};

struct CryptoManifestEntry {
    QString originalName;  // 元のオブジェクト名
    uint64_t hash;         // MurmurHash3_64(name) または UUID
};

class CryptoManifest {
public:
    void addEntry(const QString& name);
    QString findName(uint64_t hash) const;
    uint64_t findHash(const QString& name) const;
    int entryCount() const;
    void merge(const CryptoManifest& other);
    QString toMetadata() const;    // EXR metadata 文字列
};

class CryptoImage {
public:
    CryptoImage(int width, int height);

    /// 既存の ArtifactIRenderer ObjectId/MaterialId AOV から変換
    void fromObjectIdBuffer(const std::vector<float>& objectIdBuffer,
                            int width, int height);

    void fromMaterialIdBuffer(const std::vector<float>& materialIdBuffer,
                               int width, int height);

    /// ランクごとに Flat 画像を生成（EXR レイヤーとして書き出し用）
    /// rank 0: crypto_object00.R, crypto_object00.G (2ch = 上位2ID)
    /// rank 1: crypto_object01.R, crypto_object01.G (次の2ID)
    /// ...
    void generateRankLayers(CryptoLayer layer,
                             int rank,
                             std::vector<float>& outR,
                             std::vector<float>& outG) const;

    /// 全ランクの層数を計算
    int maxRank() const;

    int width() const;
    int height() const;
    CryptoManifest& manifest();

private:
    int width_, height_;
    std::unique_ptr<CryptoPixel[]> pixels_;
    CryptoManifest manifest_;
};

} // namespace
```

---

## Phase 2: Cryptomatte Shader（GPUレンダリング統合）

### Step 2.1 — フラグメントシェーダー修正

既存の Diligent シェーダーに Cryptomatte 出力パスを追加。

```hlsl
// ps_cryptomatte.hlsl
struct PSOutput {
    float4 crypto00 : SV_Target0; // 上位2ID (x,y = id1, coverage1; z,w = id2, coverage2)
    float4 crypto01 : SV_Target1; // 次の2ID
    float4 crypto02 : SV_Target2; // さらに2ID
    float4 crypto03 : SV_Target3; // さらに2ID
};

// 注: 4 RTV で最大 8 ID/pixel をカバー

PSOutput main(VSOutput input) {
    PSOutput output;
    
    uint objectId = material.objectId;
    float coverage = computeCoverage(input.uv); // サブピクセルカバレッジ

    // id を float エンコード
    float encodedId = encodeIdToFloat(objectId);

    output.crypto00 = float4(encodedId, coverage, 0, 0);
    output.crypto01 = float4(0, 0, 0, 0);
    output.crypto02 = float4(0, 0, 0, 0);
    output.crypto03 = float4(0, 0, 0, 0);

    return output;
}
```

### Step 2.2 — マルチサンプリング（MSAA）

真のアンチエイリアスを実現するために MSAA を使用。

```
各ピクセル:
  sample[0] = { id=0x12, coverage=0.7 }
  sample[1] = { id=0x34, coverage=0.3 }
  → crypto00 = (0x12_as_float, 0.7, 0x34_as_float, 0.3)
```

---

## Phase 3: ID エンコード

### Cryptomatte の ID エンコード方式

```
32bit float:
  [s:1] [e:8] [m:23]
  
Cryptomatte 方式:
  [s:1] [e:8] [id_lo:7] [unused:6] [id_hi:10]
                     ↑ rank番号に使われることも
  
実際の方式 (仕様1.3):
  float = (id >> 2) & 0x00FFFFFF を float として再解釈
  coverage = 元のcoverage (0-1)
```

```cpp
namespace CryptoEncoding {

inline float encodeId(uint32_t id) {
    // 下位 24bit を float の仮数部に埋め込む
    uint32_t encoded = id & 0x00FFFFFF;
    return *reinterpret_cast<float*>(&encoded);
}

inline uint32_t decodeId(float encoded) {
    uint32_t bits = *reinterpret_cast<uint32_t*>(&encoded);
    return bits & 0x00FFFFFF;
}

// 文字列名 → ハッシュ → ID
// Cryptomatte では MurmurHash3_x64_128 の下位64bit
// or SHA-1 の先頭7バイト
inline uint32_t nameToId(const QString& name) {
    uint64_t hash = murmurHash64(name.toUtf8().data(), name.size());
    return static_cast<uint32_t>(hash & 0x00FFFFFF);
}

} // namespace CryptoEncoding
```

---

## Phase 4: EXR マルチパート出力

### Step 4.1 — Cryptomatte EXR メタデータ

```cpp
// EXR ヘッダに Cryptomatte メタデータを書き込む
struct CryptomatteExrMetadata {
    QString name;           // "crypto_object"
    CryptoManifest manifest;
    int ranks;              // ランク数（通常 1-6）
    int width, height;
    
    // EXR ヘッダに追加するメタデータ文字列
    QString toExrHeader() const;
};

// 出力例:
// crypto_object/00/R, crypto_object/00/G
// crypto_object/01/R, crypto_object/01/G
// crypto_object/02/R, crypto_object/02/G
// crypto_object/03/R, crypto_object/03/G
//
// メタデータ: cryptomatte/17A5CF54/name = "crypto_object"
// メタデータ: cryptomatte/17A5CF54/hash = MurmurHash3
// メタデータ: cryptomatte/17A5CF54/manifest = "{ 'obj1': 'Cube_001', ... }"
```

### Step 4.2 — readbackToCryptomatte

**変更**: `ArtifactIRenderer.cppm`

```cpp
struct CryptomatteOutput {
    CryptoImage objectLayer;
    CryptoImage materialLayer;
    CryptoImage assetLayer;
};

CryptomatteOutput ArtifactIRenderer::readbackToCryptomatte() const {
    CryptomatteOutput output;

    auto objectBuf = readbackChannelToBuffer(ChannelType::ObjectId);
    if (!objectBuf.empty()) {
        output.objectLayer.fromObjectIdBuffer(objectBuf, viewportW_, viewportH_);
    }
    auto materialBuf = readbackChannelToBuffer(ChannelType::MaterialId);
    if (!materialBuf.empty()) {
        output.materialLayer.fromMaterialIdBuffer(materialBuf, viewportW_, viewportH_);
    }

    return output;
}
```

---

## Phase 5: Nuke / AE 互換性

### Step 5.1 — Nuke Cryptomatte ノードとの互換検証

1. Artifact → Cryptomatte EXR 書き出し
2. Nuke で read → Cryptomatte ノード適用
3. ドロップダウンでオブジェクト名が表示されるか
4. マスクがエイリアシングなくキレイに抽出できるか

### Step 5.2 — AE での使用

AE の Extract エフェクト + Multi-channel EXR で読み込み可能。

---

## ファイル一覧

| フェーズ | ファイル | 新規/変更 | 内容 |
|---------|----------|----------|------|
| P1 | `ArtifactCore/include/Image/Cryptomatte/CryptoPixel.ixx` | 新規 | CryptoPixel + CryptoSample |
| P1 | `ArtifactCore/src/Image/Cryptomatte/CryptoPixel.cppm` | 新規 | 実装 |
| P1 | `ArtifactCore/include/Image/Cryptomatte/CryptoImage.ixx` | 新規 | CryptoImage + CryptoManifest |
| P1 | `ArtifactCore/src/Image/Cryptomatte/CryptoImage.cppm` | 新規 | 実装 |
| P2 | `shaders/Cryptomatte.hlsl` | 新規 | GPU シェーダー |
| P2 | `ArtifactIRenderer.cppm` | 変更 | Cryptomatte シェーダーパス追加 |
| P3 | `ArtifactCore/include/Image/Cryptomatte/CryptoEncoding.ixx` | 新規 | ID float エンコード |
| P4 | `ArtifactCore/src/Image/Cryptomatte/CryptoExrWriter.cppm` | 新規 | EXR マルチパート書き出し |
| P4 | `ArtifactCore/include/Image/OpenEXR.ixx` | 変更 | Cryptomatte EXR メタデータ |
| P4 | `ArtifactIRenderer.cppm` | 変更 | readbackToCryptomatte() |

---

## 検証チェックリスト

- [ ] ObjectId AOV が正しくレンダリングされる（確認済み）
- [ ] float ID エンコードが decode で元の ID に復元できる
- [ ] CryptoPixel の多サンプル蓄積が正しい（coverage 降順）
- [ ] ランク分割で全サンプルが失われずに EXR に保存される
- [ ] マニフェストに全オブジェクト名が正しく記録される
- [ ] EXR メタデータが Cryptomatte 1.3 仕様に準拠
- [ ] Nuke で Cryptomatte ノードが全オブジェクトを認識
- [ ] 半透明エッジでアンチエイリアスマスク抽出可能
- [ ] シンプルなシーンで >5fps でレンダリング完了
