# Milestone: Advanced Typography Engine (2026-03-29)

**最終更新:** 2026-08-15
**Status:** animator／shaping／GPU glyph の基盤と Text Layer の一部統合は実装済み。3D extrusion、Fluid／Physics、OpenType 高度機能、全 animator の runtime 受入れは未完了。

### 実装状況（2026-08-15 確認）

`TextAnimatorEngine` の transform／style override、GlyphItem の per-glyph 値、`Text.ShapingBackend`／Qt shaping と RTL 判定、Diligent の glyph 描画経路を確認した。残課題は Text Layer での全 animator 項目の表示反映、3D extrusion、Fluid／Physics 連携、OpenType 高度機能の runtime 検証。

`ArtifactTextLayer` は glyph evaluation 更新経路を持ち、Property Editor には Text Animator の color editor、renderer には glyph atlas／transformed glyph submitter がある。CJK／RTL／emoji 系の shaping 基盤も存在するため、旧来の「Phase 1 は Text Layer 未統合」という表現は「一部統合・全項目未確認」に改める。
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

## Static audit follow-up (2026-07-25)

- `TextAnimator`, `GlyphLayout`, `TextShapingBackend`, `TextLayoutContract`, and `GlyphAtlas` modules provide the Phase 1 animator/shaping foundation; the text layer and property-editor animator surfaces consume part of it.
- RTL/text-direction handling and a GPU glyph rendering path are present, but complete propagation of every animator property through `ArtifactTextLayer` was not confirmed.
- No dedicated Text3DEngine/extrusion mesh or FluidText/SpringText integration was found in the targeted source tree. Phase 2-3 therefore remain unimplemented, with runtime shaping/renderer verification pending.
- No build or runtime verification was performed under the repository policy.

## Update 2026-08-15

現行コードを追加確認した。`TextAnimatorEngine` は transform／style override と glyph 単位の値を扱い、`Text.ShapingBackend`／Qt shaping、RTL判定、CJK／emoji系の基盤、glyph atlas と Diligent glyph submitter が存在する。`ArtifactTextLayer` の glyph evaluation 更新と Property Editor の Text Animator color editor も確認できる。

一方、全 animator 項目の Text Layer 反映、専用 Text3DEngine／extrusion mesh、Fluid／Spring physics、OpenType高度機能の runtime 受入れは未完了または未検証。Phase 1 は基盤と部分統合済み、Phase 2〜3と全 animator parityは pending とする。
