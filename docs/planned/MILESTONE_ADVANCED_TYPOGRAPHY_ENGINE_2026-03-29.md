# Milestone: Advanced Typography Engine (2026-03-29)

**Status:** Implementation Starting
**Goal:** 文字単位の高度な変形・アニメーション制御により、AEを超えるタイポグラフィ表現を実現。
**関連コンポーネント:** Text.Animator, Text.GlyphLayout, ArtifactTextLayer, Graphics.TextRenderer

---

## コンセプト

既存の `TextAnimator` 基盤を拡張し、文字一つ一つが独立したジオメトリとして振る舞い、ベクター変形、3D的な厚み、さらには物理シミュレーション（流体等）の影響を受ける仕組みを構築します。

---

## Phase 1: ベクター変形と高度なアニメータープロパティー ✅ **実装完了**

### Implementation
- ✅ `AnimatorProperties` の実実装拡張 (skew, tracking, z, color, stroke, blur)
- ✅ `GlyphItem` へオフセット項目を追加
- ✅ `TextAnimatorEngine` での適用ロジック実装
- ⏳ `ArtifactTextLayer` でのレンダリング対応

### 見積: 10h

---

## Phase 2: 3Dタイポグラフィと厚み (Extrusion)

### Implementation
- `Z-Depth` 制御:
  - 文字ごとの Z 軸移動とカメラ連携
  - 簡易的な擬似 3D 厚み (Extrusion / Drop Shadow 拡張)
- 3Dギズモとの統合:
  - 3D空間上での文字単位の操作

### 見積: 12h

---

## Phase 3: 流体文字と物理シミュレーション (Fluid & Physics)

### Implementation
- `FluidTextSolver`:
  - 文字の輪郭を `FluidSolver2D` のソースとして扱い、文字が液体のように溶けたり混ざったりする表現。
- `SpringText`:
  - 文字の各頂点に質点バネ系を適用し、揺れや弾みを表現。

### 見積: 10h

---

## Technical Architecture

```
AdvancedTypographyEngine
├── TextAnimator (extend existing)
│   ├── calculateWeight()
│   ├── applyTransformations() (Position/Scale/Rotation/Skew)
│   └── applyStyleOverrides() (Color/Stroke/Blur)
├── GlyphGeometryGenerator
│   ├── glyphToPath()
│   ├── applyVectorDeformation()
│   └── generateExtrusionMesh()
└── PhysicsBridge
    ├── textToParticles()
    └── applyFluidForceToGlyphs()
```

---

## UI Integration

### Text Animator パネル
```
[Text Animator]
├── Range Selector: [Start: 0%] [End: 100%] [Offset: 0%]
├── Properties:
│   ├── [Position] [Scale] [Rotation]
│   ├── [Skew: 0.0] ████░░░░░░
│   ├── [Fill Color] [Stroke Color]
│   └── [Blur: 5.0px] ██████░░░░
└── Advanced:
    ├── [Enable 3D] [X]
    └── [Fluidity: 50%] █████░░░░░
```

---

## Deliverables

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/Text/TextAnimator.ixx` | アニメータープロパティの拡張 |
| `ArtifactCore/src/Text/TextAnimator.cppm` | 変形ロジックの実装 |
| `Artifact/src/Layer/ArtifactTextLayer.cppm` | レンダリングパイプラインへの統合 |
| `ArtifactCore/include/Text/Text3DEngine.ixx` | 3Dテキスト基盤 (Phase 2) |
| `ArtifactCore/src/Physics/FluidTextSolver.cppm` | 流体文字シメント (Phase 3) |

---

## 見積もり総計: ~32h

| Phase | 時間 | 内容 |
|-------|------|------|
| **Phase 1** | 10h | ベクター変形・プロパティ拡張 |
| **Phase 2** | 12h | 3D・厚み表現 |
| **Phase 3** | 10h | 流体・物理統合 |
