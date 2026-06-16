# Random / Noise Helper Proposal (ArtifactCore) — 2026-06-16

**Author**: CommandCode (dev/commandcode-2026-06-16 worktree)
**Status**: Proposal (未着手)
**Target**: `ArtifactCore/include/Math/**`, `ArtifactCore/include/Particle/**`, `ArtifactCore/include/Script/Expression/**`

---

## 背景

### 現状の整理

| 系統 | 場所 | 用途 | 問題 |
|---|---|---|---|
| `Math.Random` (独自 splitmix64) | `ArtifactCore/include/Math/Random.ixx` | 中核 RNG | Singleton が thread-unsafe、`fork()` はあるが活用薄い |
| `std::mt19937` 直書き | `ParticleSystem.ixx`, `AddNoiseEffect.cppm`, `OpenCV/Noise.cppm`, `ExpressionEvaluator` | Particle / Noise effect / AE `random()` | 各箇所で重複、seed 戦略バラバラ |
| `std::default_random_engine` | `NoiseGenerator.cppm` | Perlin permutation | thread-unsafe (共有 `p[]` テーブル) |
| `rand()` (libc) | `ParticleSystem.cppm:74` (FluidField) | 1 箇所だけ | seedRandom 化候補 |
| `boost::uuids::random_generator` | `Id.ixx` | UUID 生成 | OK だが v4 のみ、v7 (時間ソート) 無し |
| GPU 側独自 hash | `ProceduralTexture.ixx` (SplitMix32) / `ParticleCompute.cppm` (iq hash33) | テクスチャ生成 / particle 乱流 | CPU/GPU で seed 共有なし |
| AE 互換 Expression | `ExpressionEvaluator.cppm` | `random / noise / wiggle` | `gaussRandom / seedRandom / posterizeTime` 未実装 |

### ノイズ実装の重複

| 実装 | 種類 | 場所 |
|---|---|---|
| `NoiseGenerator` | Perlin / Worley / fBm | `Math/NoiseGenerator.{ixx,cppm}` |
| `NoiseImageGenerator` | Perlin / fBm / Worley / cloud / wood / marble (ラッパ) | `ImageProcessing/NoiseImageGenerator.{ixx,cppm}` |
| `ProceduralTextureGenerator` | Perlin / Simplex / FBM / Voronoi / White / Value / Gradient + domain warp | `ImageProcessing/ProceduralTexture.{ixx,cppm}` (+ HLSL) |
| `OpenCV::Noise` | Gaussian / Uniform / Salt&Pepper / Perlin近似 / FilmGrain | `ImageProcessing/OpenCV/Noise.{ixx,cppm}` |

**4 つの重複**。`ProceduralTextureGenerator` が最もリッチなので、single source of truth 化候補。

---

## 方針

- AGENTS.md 準拠: 新規 signal/slot 追加なし、`QImage` 新規追加なし、QtCSS なし
- 既存 `Math.Random` を **強化**、分散 (Distribution) ヘルパを新設
- ノイズは `ProceduralTextureGenerator` をベースに統合、4 系列を薄くする
- AE 互換は `ExpressionEvaluator` に `gaussRandom / seedRandom / posterizeTime` 追加
- テストは `tests/ArtifactCore/MathTest.cpp` 新設 (乱数・ノイズ・Distribution・Fork)

---

## Module A: `Math.Random` 強化

### A-1: 既存 API の thread-safety 改善
- `RandomStream` を **値型** として完全保持 (既に `std::uint64_t state_` のみなので低コスト)
- Singleton `Random::instance()` は **thread_local 化** または **mutex 化** を選択制に
- `fork(salt)` とは別に `split()` API を追加 (xoroshiro の "splitmix" 的な、独立な子ストリーム生成)

### A-2: エンジン選択肢
```cpp
enum class RandomEngine {
    SplitMix64,            // 既存 (軽量、64bit 状態)
    Xoroshiro128Plus,      // 高速、128bit 状態 (新)
    Pcg64,                 // PCG-XSH-RR 系、128bit 状態 (新)
    Wyrand,                // Lemire 系、最速 (新)
    Mt19937_64,            // 標準 MT、2.5KB 状態 (新、互換性用)
};

class LIBRARY_DLL_API RandomStream {
public:
    explicit RandomStream(uint64_t seed = Random::defaultSeed(),
                          RandomEngine engine = RandomEngine::SplitMix64);

    void reseed(uint64_t seed);
    void reseedWithSalt(uint64_t base, uint64_t salt);

    // 基本乱数
    uint32_t nextU32();
    uint64_t nextU64();
    float nextUnitFloat();                    // [0, 1)
    double nextUnitDouble();                  // [0, 1)
    int nextInt(int minInclusive, int maxExclusive);
    int64_t nextInt64(int64_t minInclusive, int64_t maxExclusive);
    float nextFloat(float minInclusive, float maxExclusive);
    double nextDouble(double minInclusive, double maxExclusive);

    // 分布 (新)
    bool chance(float probability);          // Bernoulli
    bool chance(double probability);
    float gaussian(float mean = 0.0f, float stddev = 1.0f);       // Box-Muller
    double gaussian(double mean = 0.0, double stddev = 1.0);
    int poisson(double lambda);              // Knuth
    float exponential(float lambda = 1.0f);  // -ln(U)/lambda
    float triangular(float low = 0.0f, float high = 1.0f, float mode = 0.5f);
    float gamma(float shape, float scale);   // Marsaglia & Tsang
    int discrete(std::span<const double> weights);  // 別名 weighted pick

    // ベクトル
    QVector2D nextVec2(float min = -1.0f, float max = 1.0f);
    QVector3D nextVec3(float min = -1.0f, float max = 1.0f);
    QVector4D nextVec4(float min = -1.0f, float max = 1.0f);
    QVector2D unitVec2();                    // 単位円上
    QVector3D unitVec3();                    // 単位球上 (Marsaglia)
    QVector3D hemisphereVec3(const QVector3D& normal);  // コサイン重み
    QVector2D diskVec2(float radius = 1.0f);            // 単位ディスク上

    // 配列
    template<typename T> void shuffle(std::vector<T>& v);
    template<typename T> T pick(const std::vector<T>& v);
    template<typename T, size_t N> T pick(const std::array<T, N>& v);
    template<typename T, typename WeightFn>
    T weightedPick(const std::vector<T>& v, WeightFn weightFn);

    // 派生ストリーム
    RandomStream fork(uint64_t salt) const;
    RandomStream child() const;              // 親の状態から独立な子

    // 状態
    uint64_t state() const;                  // snapshot / restore
    void restore(uint64_t state);
    RandomEngine engine() const;
};
```

### A-3: 全 engine 同一 API
`Xoroshiro128Plus` / `Pcg64` / `Wyrand` を 1 つの `RandomStream` 抽象に統合。`engine()` で選択。

### A-4: Static な `Random` Singleton (thread_local 化)
```cpp
class LIBRARY_DLL_API Random {
public:
    static thread_local Random& instance();
    // ... 既存 API は thread_local 化 ...
};
```

### A-5: 暗号学的乱数
```cpp
class LIBRARY_DLL_API SecureRandom {
public:
    static std::optional<QByteArray> bytes(int size);
    static uint32_t u32();
    static uint64_t u64();
    static QString hexString(int byteCount);
};
```
- 内部: Win `BCryptGenRandom` / Linux `getrandom` / macOS `SecRandomCopyBytes` / `std::random_device` のフォールバック

---

## Module B: `Math.Noise` 強化

### B-1: エンジン API の統合
- `NoiseGenerator` を **統合エントリ** として残し、内部で `ProceduralTextureGenerator` を呼び出す薄いファサードに
- `setSeed` を **stream ベース** に変更 (内部で `RandomStream` を保持)

### B-2: 既存 API 拡張
```cpp
class LIBRARY_DLL_API NoiseGenerator {
public:
    // 既存
    static float perlin(float x);
    static float perlin(float x, float y);
    static float perlin(float x, float y, float z);
    static float fractal(float x, float y, float z,
                         int octaves = 4, float persistence = 0.5f,
                         float lacunarity = 2.0f);
    static float worley(float x, float y, float z);

    // 新規
    static float simplex(float x);                      // 1D
    static float simplex(float x, float y);             // 2D
    static float simplex(float x, float y, float z);    // 3D
    static float simplex(float x, float y, float z, float w);  // 4D
    static float value(float x, float y, float z);      // value noise
    static float ridge(float x, float y, float z,      // ridge multifractal
                       int octaves = 4, float gain = 2.0f, float lacunarity = 2.0f);
    static float billow(float x, float y, float z,      // billow
                        int octaves = 4, float gain = 2.0f, float lacunarity = 2.0f);
    static float turbulence(float x, float y, float z,  // |perlin| の fBm
                            int octaves = 4, float persistence = 0.5f,
                            float lacunarity = 2.0f);
    static float pingpong(float x, float y, float z,    // sin(perlin*pi)
                          int octaves = 4, float frequency = 1.0f);

    // Curl noise (発散なしベクトル場)
    static QVector3D curl2D(float x, float y, float eps = 0.0001f);
    static QVector3D curl3D(float x, float y, float z, float eps = 0.0001f);

    // 派生 (Domain warp)
    static float warpedPerlin(float x, float y, float z, float warpStrength = 1.0f);
    static QVector3D warpedFBM3D(float x, float y, float z, float warpStrength = 1.0f);

    // Seed 戦略
    static void setSeed(unsigned int seed);              // 既存
    static void setSeedStream(const RandomStream& s);   // 新
    static RandomStream& stream();                      // 共有 stream
};
```

### B-3: インスタンス化可能版
```cpp
class LIBRARY_DLL_API NoiseField {
public:
    explicit NoiseField(const RandomStream& stream);
    float perlin(float x, float y, float z) const;
    float fractal(float x, float y, float z,
                  int octaves, float persistence, float lacunarity) const;
    // ... 同じ API 群 ...
private:
    std::array<int, 512> p_;                            // thread-safe
};
```
- 既存の static 共有テーブル問題を解消
- `NoiseField` インスタンスを複数作って並列利用可能

### B-4: 統合リファクタ
- `NoiseImageGenerator` は `NoiseGenerator` / `ProceduralTextureGenerator` のラッパに縮約
- `OpenCV::Noise` の `Perlin` 近似は `ProceduralTextureGenerator` の `Perlin` を呼ぶように置換
- `OpenCV::Noise` の `Gaussian / Uniform / SaltAndPepper / FilmGrain` は OpenCV の wrap のまま残す (Image ドメイン特化)

---

## Module C: `Math.RandomDistribution` (新規, 分布ヘルパ)

`RandomStream::gaussian()` 等を独立利用したいケースのため

```cpp
namespace RandomDistribution {
    // 1D
    float uniform(float minV, float maxV, RandomStream& s);
    float gaussian(float mean, float stddev, RandomStream& s);
    float exponential(float lambda, RandomStream& s);
    float triangular(float low, float high, float mode, RandomStream& s);
    float gamma(float shape, float scale, RandomStream& s);
    float beta(float alpha, float beta, RandomStream& s);
    float chiSquared(float k, RandomStream& s);
    float studentT(float nu, RandomStream& s);
    float cauchy(float location, float scale, RandomStream& s);
    float lognormal(float mu, float sigma, RandomStream& s);
    float weibull(float shape, float scale, RandomStream& s);
    int   poisson(double lambda, RandomStream& s);
    int   binomial(int trials, float p, RandomStream& s);
    int   geometric(float p, RandomStream& s);

    // ベクトル
    QVector2D uniformInDisk(float radius, RandomStream& s);
    QVector3D uniformInSphere(float radius, RandomStream& s);
    QVector3D uniformInBox(const QVector3D& extent, RandomStream& s);
    QVector3D cosineHemisphere(const QVector3D& normal, RandomStream& s);
    QVector2D uniformInTriangle(const QVector2D& a, const QVector2D& b, const QVector2D& c, RandomStream& s);

    // 重み付きサンプリング
    int weightedIndex(std::span<const double> weights, RandomStream& s);
    int weightedIndex(std::span<const float> weights, RandomStream& s);
}
```

---

## Module D: `Script.Expression.Random` (AE 互換 Expression 拡張)

`ExpressionEvaluator.cppm` の `BuiltinFunctions` に追加

### D-1: 新規ビルトイン
```cpp
ExpressionValue gaussRandom();                            // AE 互換 (mean=0, stddev=1)
ExpressionValue seedRandom(int seed);                     // static seed 設定
ExpressionValue seedRandom(int seed, bool timeless);      // timeless=true でフレーム seed を抑止
ExpressionValue posterizeTime(double fps);                // 整数フレームに量子化
ExpressionValue posterizeTime(double duration, double fps);

// 既存強化
ExpressionValue random(double max);                       // static seed 切替対応
ExpressionValue random(double min, double max);
ExpressionValue noise(double x);
ExpressionValue noise(double x, double y);
ExpressionValue noise(double x, double y, double z);
ExpressionValue wiggle(double freq, double amp,
                       double octaves = 4.0,
                       double persistence = 0.5,
                       double lacunarity = 2.0);
```
- `seedRandom(seed, timeless=true)` フラグ対応 (AE 仕様)
- `posterizeTime(fps)` で `floor(time * fps) / fps` の量子化

### D-2: 内部の `static std::mt19937` を `RandomStream` 化
- AE 式評価コンテキストごとに `RandomStream` を持つように
- グローバル Singleton 削除 (thread-safety)

### D-3: 評価コンテキスト API
```cpp
class ExpressionEvaluator {
public:
    void setRandomSeed(uint64_t seed);
    void setRandomStream(std::unique_ptr<RandomStream> s);
    RandomStream& randomStream();
    // ...
};
```

---

## Module E: `Math.UUID` (v4 / v7 対応)

`Id.ixx` を `Math.UUID` モジュールに移し、v7 を追加

```cpp
enum class UUIDVersion { v1, v4, v7, nil };

struct LIBRARY_DLL_API UUID {
    std::array<uint8_t, 16> bytes;

    static UUID v4();
    static UUID v7();                              // 時間ソート可能、RFC 9562
    static UUID v7(uint64_t unixMs, RandomStream& s);
    static UUID v5(const UUID& ns, std::string_view name);  // SHA-1
    static UUID nil();

    bool isNil() const;
    UUIDVersion version() const;
    uint64_t unixMs() const;                       // v7 用
    QString toString(bool upper = true) const;
    QString toString(QUuid::StringFormat fmt) const;  // Qt 互換 format
    static std::optional<UUID> fromString(QStringView s);

    bool operator==(const UUID& other) const;
    bool operator<(const UUID& other) const;       // v7 ソート用
    uint qHash() const;
};
```

### E-1: 既存 `Id` の互換
- `Id` の内部実装を `UUID` に置換
- `CompositionID` / `LayerID` 等の typed alias は維持
- boost::uuids 依存は **残す** (外部 API 互換のため、内部実装は `Math.UUID` に置換する選択肢)

### E-2: Layer/Composition/Asset ID 生成ポリシー
- `LayerID`: `UUID::v7()` (作成時刻でソート可能、衝突極小)
- `CompositionID`: `UUID::v7()`
- `EffectInstanceID`: `UUID::v4()` (衝突回避のみ)
- `ParticleSeed`: `RandomStream::nextU64()`

---

## Module F: `Math.Hash` (ハッシュベース乱数, 文字列→乱数)

`StringLike` コンセプトと組み合わせる

```cpp
namespace HashRandom {
    uint32_t u32(QStringView s, uint32_t seed = 0);     // FNV-1a / xxHash 系
    uint64_t u64(QStringView s, uint64_t seed = 0);
    uint32_t u32(const void* data, size_t size, uint32_t seed = 0);
    QVector2D vec2(QStringView s, uint32_t seed = 0);
    QVector3D vec3(QStringView s, uint32_t seed = 0);
    QVector4D vec4(QStringView s, uint32_t seed = 0);
    QColor color(QStringView s, uint32_t seed = 0);    // 決定論カラー (ID 用)

    // xxHash3 (高速)
    uint64_t xxhash3(QStringView s, uint64_t seed = 0);
    uint64_t xxhash3(const void* data, size_t size, uint64_t seed = 0);
}
```
- 用途: layer name → 決定論カラー、layer id → 決定論シード

---

## Module G: `Math.NoiseGpu` (HLSL 側, GPU 乱数)

`ProceduralTexture.ixx` の splitmix32 + iq hash33 を **公開ヘッダ** に切り出し、particle / procedural texture / post-effect で再利用可能に

### G-1: 新規 HLSL ヘッダ
`ArtifactCore/include/Graphics/Shader/Compute/HLSL/Random.ixx` (module `Graphics.Shader.Compute.HLSL.Random`)

```hlsl
// HLSL 関数群 (snippet)
uint HashU32(uint x);
uint HashU32(uint2 p, uint seed);
uint HashU32(uint3 p, uint seed);
uint HashU32(uint4 p, uint seed);
float Hash01(uint h);
float2 Hash01_2(uint h);
float3 Hash01_3(uint h);
float4 Hash01_4(uint h);

uint Wyrand(uint2 p, uint seed);                  // Lemire wyrand
uint WangHash(uint seed);
float InterleavedGradientNoise(uint2 p);          // Jorge Jimenez IGN
float2 Hash22(float2 p);                          // Inigo Quilez
float3 Hash33(float3 p);

uint3 PCG3D(uint3 p);                             // Mark Jarzynski / Marc Olano
```
- 既存 `ProceduralTexture.ixx` の `HashU32` は `Graphics.Shader.Compute.HLSL.Random` を import する形に置換
- 既存 `ParticleCompute.cppm` の `hash33` も置換

### G-2: CPU/GPU 共有 Seed
- `ProceduralTextureSettings::seed` + `Math.Random` の `RandomStream` をシームレスに共有
- `RandomStream seedForGpu() const;` で GPU 互換 seed (`uint32_t`) を渡す
- シェーダーの引数 `uint seed` は `RandomStream::nextU32()` で初期化

---

## Module H: `Math.Particle` の乱数整理

`ParticleSystem.ixx` / `ParticleSystem.cppm` の乱数を整理

### H-1: `ParticleEmitter::rng_` を `RandomStream` に置換
```cpp
class ParticleEmitter {
public:
    void setSeed(uint64_t seed);
    void setRandomStream(RandomStream s);
    const RandomStream& randomStream() const;
    void reseed();
private:
    RandomStream rng_;                            // std::mt19937 から置換
};
```

### H-2: `TurbulenceForce::rng_` も同じ
- `ForceField` 基底に `RandomStream rng_;` を持たせる

### H-3: `FluidField::injectAudioFrequency` の `rand()` 削除
```cpp
// 修正前
int rx = rand() % solver_.width();
int ry = rand() % solver_.height();

// 修正後
auto& s = stream();
int rx = static_cast<int>(s.nextU32() % static_cast<uint32_t>(solver_.width()));
int ry = static_cast<int>(s.nextU32() % static_cast<uint32_t>(solver_.height()));
```

---

## Module I: `Math.Random` の Utility

### I-1: Frame → Seed ヘルパ
```cpp
namespace RandomUtil {
    // フレーム番号 + layer id から決定論 seed
    uint64_t frameSeed(uint64_t baseSeed, int64_t frame, uint32_t layerId = 0);

    // timecode (e.g. "01:23:45:12") → seed
    uint64_t timecodeSeed(QStringView timecode);

    // 文字列を hash して seed
    uint64_t nameSeed(QStringView name, uint64_t salt = 0);

    // 32bit hash を 64bit splitmix seed に昇格
    uint64_t promoteSeed(uint32_t seed32);
}
```

### I-2: Stream Pool
```cpp
class RandomStreamPool {
public:
    static RandomStreamPool& instance();
    RandomStream& acquire(uint64_t key);
    void release(uint64_t key);
    void clear();
};
```
- 用途: マルチスレッド Particle シミュレーションでスレッド毎の独立 stream

---

## 実装優先度

| 優先度 | Module | 根拠 |
|---|---|---|
| ★★★ | A-1〜A-3 (RandomStream 強化) | 中核、乱数 API の single source of truth |
| ★★★ | D-1 (AE 互換 Expression 拡張) | 既存 random() のバグ修正 + gaussRandom/seedRandom 追加でユーザ要望多 |
| ★★★ | H-1〜H-3 (Particle 乱数整理) | rand() 撲滅、std::mt19937 → RandomStream 統一 |
| ★★ | B-3 (NoiseField インスタンス化) | thread-safety 解消、並列対応 |
| ★★ | E (UUID v7) | layer id ソート、衝突極小 |
| ★★ | G-1 (HLSL Random 切り出し) | shader 間の重複解消、CPU/GPU 共有 |
| ★★ | C (RandomDistribution) | アプリ側の分布ニーズ一般化 |
| ★ | A-4 (thread_local Singleton) | 既存コードへの影響小、mutex 化より低コスト |
| ★ | A-5 (SecureRandom) | セキュリティ関連 |
| ★ | B-4 (ノイズ統合リファクタ) | 4 系統を整理、影響大 |
| ★ | F (HashRandom) | ニッチだが便利 |
| ★ | I (RandomUtil) | 補助 |
| ○ | I-2 (StreamPool) | 並列 Particle 専用 |

## テスト方針

`tests/ArtifactCore/MathTest.cpp` 新設 (GoogleTest):

- A: 各 Engine の `nextU32` の分布 (chi-squared test)、`fork/child` の独立性、Poisson の期待値、Gussian の mean/stddev、triangular の mode、unit disk/sphere の方向均一性
- B: perlin 1D/2D/3D の [-1,1] 範囲、simplex の [-1,1] 範囲、fBm の周期性、curl noise の発散 ≈ 0
- C: 各分布の mean/stddev (許容誤差 1% 以内)
- D: `seedRandom(42)` → `random()` シーケンス固定、`posterizeTime(24)` でフレーム量子化
- E: `v7()` で連続生成時に UnixMs 単調増加、`v5(ns, name)` の決定論性、`<` での v7 ソート
- F: 同じ name → 同じ color、name 変更 → 異なる color
- G: HLSL 側は HLSL unit test 環境が無ければ省略 (compute shader smoke test で代替)

## 関連ファイル

- 既存: `ArtifactCore/include/Math/Random.ixx` (splitmix64 + Random singleton)
- 既存: `ArtifactCore/include/Math/NoiseGenerator.ixx` (Perlin/Worley/fBm, thread-unsafe)
- 既存: `ArtifactCore/include/ImageProcessing/NoiseImageGenerator.ixx` (ラッパ)
- 既存: `ArtifactCore/include/ImageProcessing/ProceduralTexture.ixx` (Perlin/Simplex/FBM/Voronoi)
- 既存: `ArtifactCore/include/ImageProcessing/OpenCV/Noise.ixx` (Gaussian/Uniform/Salt&Pepper/Perlin/FilmGrain)
- 既存: `ArtifactCore/include/Utils/Id.ixx` (boost::uuids v4 のみ)
- 既存: `ArtifactCore/include/Particle/ParticleSystem.ixx` (mt19937 + rand() 混在)
- 既存: `ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm` (random/noise/wiggle のみ、gaussRandom なし)
- 既存 GPU: `ArtifactCore/include/Graphics/Shader/Compute/HLSL/ProceduralTexture.ixx` (splitmix32)
- 既存 GPU: `ArtifactCore/src/Graphics/ParticleCompute.cppm` (iq hash33)
- 既存テスト: `tests/ArtifactCore/UtilsTest.cpp` (乱数テストなし)
- 既存 CMake: `ArtifactCore/CMakeLists.txt` (GLOB_RECURSE 自動収集)
