# グリッジ系動画像エフェクト提案

> 2026-06-13 作成  
> 既存: `ArtifactGlitchEffect`（チャンネルオフセット）、`TurbulentDisplaceEffect`、`WaveEffect`

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

- `VectorFlowGlitch` の Core processor と `VectorFlowGlitchEffect`、`ChromaticReliefEffect` は実装され、`ArtifactEffectService` の catalog／Inspector 入口へ登録されている。
- Glitch は rasterizer の CPU implementation に加え、既存 `ArtifactGlitchEffect` の GPU compute／Transition 系の経路がある。Mosaic／Auto Mosaic／Turbulent Displace も利用可能な基盤として確認できる。
- `TemporalFossilEffect` は Core の history／decay 処理として存在するが、通常の Layer effect catalog・GPU composition path への完全接続は未確認。
- `QuantumGlitch`、`SignalCollapse`、`InkDelay／DitherBleed` の提案名に対応する完成 effect は現行ソースで確認できない。
- 判定: **提案の一部（VectorFlow／ChromaticRelief／Glitch 系）は実装済み。残りの提案 effect と runtime／CPU-GPU parity は未完了または未検証。** ビルド・テストは未実施。

---

## 既存エフェクト分析

### ArtifactGlitchEffect
- チャンネルRGBずらし（ランダムオフセット）
- 行ベースのグリッジノイズ
- 極シンプル実装

### TurbulentDisplaceEffect
- ノイズベースの歪み変形
- オクターブ/サイズ/シード制御
- OpenCV remap使用

### WaveEffect
- サイン波形でピクセル変位
- 振幅/周波数/位相制御

---

## NEW グリッジ系エフェクト提案

### 1. VectorFlowGlitch

**コンセプト**: 画像のエッジ・構造テンザイトを検出し、流れの方向に沿って引き裂くグリッジ

```
struct VectorFlowGlitch {
    // テクスチャ解析
    float edgeThreshold = 0.2f;      // エッジ検出の感度
    float flowStrength = 1.0f;       // グリッジ強度
    
    // 引き裂き効果
    float tearAmount = 0.3f;         // 0.0..1.0
    float tearLength = 30.0f;        // 最大引き裂き長さ(px)
    
    // カラーグリッジ
    bool colorBleed = true;          // 色滲み
    float colorShift = 0.1f;         // RGBずらし強度
};
```

**ベース**: `EdgeFinder` + `StructureTensor` → 歪みマップ生成 → `cv::remap`

### 2. QuantumGlitch (WF Collapse)

**コンセプト**: タイル状に画像を分割し、隣接ルールでランダムに再配置（量子状態崩壊のメタファー）

```
struct QuantumGlitch {
    int tileSize = 16;              // タイルブロックサイズ
    float collapseProb = 0.3f;       // タイル崩壊確率
    float evolution = 0.0f;        // 崩壊進行度（アニメ化）
    int seed = 0;
};
```

**アルゴリズム**:
1. 画像を `tileSize`×`tileSize` ブロックに分割
2. 各ブロックのハッシュ値でグループ化
3. 同一グループ内でランダムシャッフル or 隣接ルール配置
4. `collapseProb` で崩壊の進行を制御

### 3. SignalCollapse

**コンセプト**: デジタル信号断ちょうどいいグリッジ（ブロックノイズ + 色深度崩壊）

```
struct SignalCollapseGlitch {
    float collapseAmount = 0.5f;      // 0..1
    float blockNoise = 0.3f;         // 8x8ブロックノイズ
    float bitDepthLoss = 0.2f;       // 色深度低下（8bit→4bit）
    float chromaShift = 0.1f;        // 色差ずらし
};
```

### 4. InkDelay / DitherBleed

**コンセプト**: インクや染料がゆっくり流れて滲むような効果

```
struct InkDelayGlitch {
    float bleedAmount = 0.3f;         // 滲み量
    float delayFrames = 5.0f;         // 遅延フレーム
    float viscosity = 0.5f;          // 粘性（滲む遅さ）
    float turbulence = 0.1f;         // 渦巻きノイズ
};
```

**ベース**: FlowFieldベースの `ReactionDiffusion` + `TemporalAccumulation`

### 5. ChromaticRelief

**コンセプト**: 色差成分だけを深度マップとして扱い、カリotypeで浮き出し効果

```
struct ChromaticReliefGlitch {
    float reliefDepth = 0.5f;         // 浮き出し深度
    float edgeThreshold = 0.3f;       // エッジ抽出
    float chromaScale = 10.0f;        // 色差スケール
};
```

### 6. TemporalFossil

**コンセプタ**: 過去フレームの残像を堆積させ、時間の層を視覚化

```
struct TemporalFossilGlitch {
    int fossilFrames = 8;              // 保存フレーム数
    float decayPerFrame = 0.85f;       // フレーム毎減衰率
    float blendMode = 0;               // 0=Add, 1=Screen, 2=Overlay
    bool tintByAge = true;            // 年老による色付け
};
```

---

## 実装優先度

| 優先度 | エフェクト | 理由 |
|--------|----------|------|
| ★★★ | VectorFlowGlitch | TurbulentDisplace + Edge の組み合わせで実装しやすい |
| ★★☆ | QuantumGlitch | タイルシステムは既存Mosaicと共有 |
| ★★☆ | SignalCollapse | 8-bit処理は単純 |
| ★☆☆ | InkDelay | ReactionDiffusion + FluidSolver2D活用 |
| ★☆☆ | TemporalFossil | フレーム履歴システムを流用 |
| ☆☆☆ | ChromaticRelief | 色差抽出だけなので後回し |

---

## コード参照

- `Artifact/src/Effect/ArtifactCreativeEffects.cppm` - 既存Glitch
- `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm` - ノイズ変形
- `Artifact/src/Effects/Mosaic/AutoMosaicEffect.cppm` - タイリングベース
- `ArtifactCore/include/Physics/FluidSolver2D.ixx` - ReactionDiffusionベース
- `ArtifactCore/include/Math/Noise.ixx` - ノイズ関数
