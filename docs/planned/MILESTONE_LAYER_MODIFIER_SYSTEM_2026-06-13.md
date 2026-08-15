# 実装案: M-LYR-MOD Layer Modifier System (Blender-style Deformers)

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

「未実装」という冒頭の旧判定は現状と一致しません。`ArtifactLayerModifier`／`TransformLayerModifier`／`LayerModifierStack`、追加・削除・順序適用、JSON serialize／deserialize、`ArtifactAbstractLayer` の transform 評価への適用が実装されています。別系統では clone modifier descriptor の time-offset／sequence／random／formula／spline／step と field influence も `cloneRenderInstances()` に接続されています。

ただし、提案されていた Taper／Bend／Twist、Noise Displace、Spline Warp、Lattice、Mesh／Wave deform の専用 modifier 実装と、Tool Options／Inspector の動的追加 UI、複雑な geometry deformation、各 modifier の runtime parity は未確認です。現状は **modifier stack の基盤＋Transform／clone 系部分実装**と判定します。

> 2026-06-13 作成  
> 状態: 未実装  
> 優先度: 中（CloneEffector 基盤があるため、段階的拡張可能）

## 目標

AEスタイルのレイヤーに、Blender/C4Dライクなモディファイアスタックを導入。  
変形（Deformer）はエフェクトスタックの一種として扱い、レイヤーの描画前に適用される。

---

## 既存実装（参考）

### CloneEffector System (C4Dライク)
- `Artifact.Effect.Clone.Core` - CloneData / AbstractCloneField / AbstractCloneEffector
- `Artifact.Effect.Clone.Basic` - TransformCloneEffector, SphericalCloneField
- `Artifact.Effect.Clone.Advanced` - Step/Random/Noise CloneEffector
- `Artifact.Effect.Generator.Cloner` - ClonerGenerator（クローン生成）

### Layer Effect Stack
- `ArtifactAbstractLayer::addEffect()` / `getEffects()` でエフェクトスタック管理
- `EffectPipelineStage`: Generator → PreProcess → Shader → PostProcess

---

## 提案: LayerModifier ABSTRACT

```
module;
#include <QMatrix4x4>
#include <QVector3D>
#include <memory>

export module Artifact.Layer.Modifier;

import Artifact.Layer.Abstract;
import Property.Abstract;

export namespace Artifact {

// モディファイアタイプ
enum class LayerModifierType {
    Basic_Deform,     // ベース変形 (Taper, Bend, Twist)
    Noise_Displace,   // ノイズ変位
    Spline_Warp,      // スプラインワープ
    Lattice_Deform,   // ラティスデフォーマ
    Mesh_Distort,     // メッシュ引っ掛け
    Wave_Deform,      // 波形変形
    Custom            // プラグイン/スクリプト対応
};

// モディファイア基底
class ArtifactLayerModifier : public ArtifactAbstractEffect {
public:
    ArtifactLayerModifier() {
        setPipelineStage(EffectPipelineStage::PreProcess);
    }

    // レイヤーのローカル座標を変形
    virtual QMatrix4x4 applyToLayerTransform(const QMatrix4x4& baseTransform,
                                           const QRectF& localBounds,
                                           double frameTime) const = 0;

    // モディファイア専用のプロパティオーバーライド
    virtual std::vector<AbstractProperty> getModifierProperties() const = 0;
};

} // namespace Artifact
```

---

## 基本モディファイア実装

### 1. BasicDeformModifier (Taper/Bend/Twist)

```cpp
class BasicDeformModifier : public ArtifactLayerModifier {
    // Taper: 一方向に縮小
    float taperAmount = 0.0f;     // -1.0 (逆) .. 1.0 (縮小)
    QVector3D taperAxis{1, 0, 0}; // 縮小方向

    // Bend: 円弧状に曲げる
    float bendAngle = 0.0f;
    QVector3D bendAxis{0, 1, 0};
    float bendRadius = 200.0f;

    // Twist: 回転シェア
    float twistAngle = 0.0f;
    QVector3D twistAxis{0, 1, 0};
};
```

### 2. NoiseDisplaceModifier

```cpp
class NoiseDisplaceModifier : public ArtifactLayerModifier {
    float intensity = 10.0f;
    float frequency = 1.0f;
    float evolution = 0.0f; // 時間ベース
    int seed = 12345;
};
```

### 3. WaveDeformModifier

```cpp
class WaveDeformModifier : public ArtifactLayerModifier {
    float amplitude = 10.0f;
    float wavelength = 100.0f;
    float speed = 1.0f;
    int waveCount = 1;
};
```

---

## Layer側の拡張

### ArtifactAbstractLayer.ixx への追加

```cpp
class ArtifactAbstractLayer {
    // ... 既存コード ...

    // Layer Modifier Stack
    void addModifier(std::shared_ptr<ArtifactLayerModifier> modifier);
    void removeModifier(const UniString& modifierId);
    std::vector<std::shared_ptr<ArtifactLayerModifier>> modifiers() const;
    
    // レイヤー座標変換にモディファイアを適用
    QMatrix4x4 effectiveTransform() const; // transform2D/3D + modifiers

protected:
    std::vector<std::shared_ptr<ArtifactLayerModifier>> modifiers_;
};
```

### 実装案

```cpp
// ArtifactAbstractLayer::draw() または effectiveTransform()
QMatrix4x4 ArtifactAbstractLayer::effectiveTransform() const {
    QMatrix4x4 result = getLocalTransform(); // 基本変形
    
    for (const auto& mod : modifiers_) {
        if (mod->isEnabled()) {
            result = mod->applyToLayerTransform(result, localBounds(), currentFrame());
        }
    }
    return result;
}
```

---

## UI 接続（ArtifactToolOptionsBar）

```
[モディファイア追加]
↓
- Basic Deform (Taper/Bend/Twist)
- Noise Displace
- Wave Deform
- ---
- スプリクトエディター...
```

各モディファイア選択で、以下のプロパティUIを動的生成:

```
Taper Amount: [slider -100..100]
Bend Angle: [slider 0..360]
Twist Angle: [slider 0..360]
```

---

## 作業順

| フェーズ | 内容 | 作業時間 |
|---------|------|--------|
| Phase 1 | `Artifact.Layer.Modifier` モジュール作成、抽象クラス定義 | 2h |
| Phase 2 | `BasicDeformModifier` 実装（Taper/Bend/Twist） | 3h |
| Phase 3 | `NoiseDisplaceModifier` 実装 | 2h |
| Phase 4 | `WaveDeformModifier` 実装 | 2h |
| Phase 5 | Layer へのモディファイアスタック統合 | 2h |
| Phase 6 | ToolOptionsBar への UI 追加、アイコン作成 | 3h |
| **合計** | | 14h |

---

## 依存関係

- **前提**: EffectPipelineStage::PreProcess が動作すること
- **依存なし**: レイヤー描画前に変形を適用する単独機能
- **後続**: Modifier UI を Inspector に統合（M-APP-7 後）

---

## アイコン

| ファイル | 用途 |
|----------|------|
| `Artifact/App/Icon/Studio/modifier_deform.svg` | ベーシック変形アイコン |
| `Artifact/App/Icon/Studio/modifier_noise.svg` | ノイズ変位アイコン |
| `Artifact/App/Icon/Studio/modifier_wave.svg` | 波形変形アイコン |
| `Artifact/App/Icon/Studio/modifier_lattice.svg` | ラティスデフォーマアイコン |

---

## 関連マイルストーン

- `MILESTONE_APP_LAYER_COMPLETENESS.md` - M-APP-7 EditMode UI 接続（ツール側）
- `MILESTONE_MESH_INSTANCING_2026-04-26.md` - インスタンシングとの相関
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md` - C-GEOM 幾何変形候補
