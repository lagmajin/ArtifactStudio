# MILESTONE: Text Animator (ArtifactCore) → Application Layer Integration

**Date**: 2026-04-27 (updated 2026-06-25)
**Status**: ✅ Completed — Phase 1-4 done, Phases 5-8 deferred as separate milestones
**Priority**: High
**Related**: MILESTONE_TEXT_SYSTEM_2026-03-12 (C-TXT-5), MILESTONE_TEXT_ANIMATOR_SYSTEM_2026-03-25

---

## 概要

ArtifactCore で実装済みの `TextAnimatorEngine` を `ArtifactTextLayer` に完全統合し、After Effects ライクなテキストアニメーター UI を構築する。
Phase 1-4 の完了により、property panel 編集・timeline keyframe アニメーション・per-glyph rendering の全経路が接続された。

---

## 最終ステータス (2026-06-25)

### ArtifactCore 側（完了済み）
| コンポーネント | ファイル | 状態 |
|---|---|---|
| `TextAnimatorEngine` | `ArtifactCore/include/Text/TextAnimator.ixx` | ✅ 実装済み |
| `GlyphItem` (per-glyph data) | `ArtifactCore/include/Text/GlyphLayout.ixx` | ✅ 実装済み |
| `RangeSelector` / `WigglySelector` | `TextAnimator.ixx` | ✅ 実装済み |
| `AnimatorProperties` | `TextAnimator.ixx` | ✅ 実装済み |

### Artifact 側（全フェーズ完了）
| コンポーネント | ファイル | 状態 |
|---|---|---|
| `ArtifactTextLayer` animator state | `Artifact/include/Layer/ArtifactTextLayer.ixx` | ✅ 追加/削除/個数管理 API 接続済み |
| animator state / serialization | `Artifact/src/Layer/ArtifactTextLayer.cppm` | ✅ JSON 保存復元・property path 接続済み |
| glyph-aware fallback rendering | `Artifact/src/Layer/ArtifactTextLayer.cppm` | ✅ animator 設定が raster fallback 描画に反映 |
| プロパティパネル | Inspector | ✅ `text.animatorCount` + `text.animators.N.*` 全プロパティ公開済み |
| アニメーター追加 UI | Inspector | ✅ Add/Remove 直接操作可能 |
| セレクター範囲 UI | Inspector | ✅ Start/End/Offset/Shape/Units を property 経由で編集可能 |
| タイムラインキーフレーム | Timeline | ✅ animatable prop として露出、キーフレーム編集可能 |

---

## 実装詳細

### Phase 1: アニメーター追加・削除 UI ✅
- `addAnimator()`, `removeAnimator(int index)` 実装済み
- Inspector property group ベースのリスト表示
- アニメーター名のカスタマイズ
- `text.animatorCount` Add/Remove editor

### Phase 2: セレクター設定 UI ✅
- RangeSelector: Start/End/Offset/Shape/Units/AnchorPointGrouping/Order
- WigglySelector: Frequency/Amplitude/Phase/RandomSeed

### Phase 3: アニメータープロパティ UI ✅
- Transform: Position(X,Y,Z) / Scale(X,Y,Z) / Rotation(X,Y,Z) / Opacity / Skew / Tracking
- Color: FillColor / StrokeColor / StrokeWidth / Blur
- 全プロパティ `setAnimatable(true)` + `persistentLayerProperty` 経由でキーフレーム対応

### Phase 4: タイムライン連携 ✅
- `AbstractProperty` としての公開 → `getLayerPropertyGroups()` で `text.animators.N.*` を登録
- タイムライントラック表示 → `collectAnimatablePropertyRefs()` が全 animatable prop を収集 → `displayLabelForPropertyPath()` が表示ラベル解決
- キーフレーム編集 → `setLayerPropertyValue()` + `parseAnimatorPropertyPath()` で書き込み
- 再生中プレビュー → `updateImage()` → `applyAllAnimators()` が timeline time で評価

### コードパス（検証済み）
```
collectAnimatablePropertyRefs() → getLayerPropertyGroups()
  → text.animators.N.positionX (persistentLayerProperty, setAnimatable(true))
  → displayLabelForPropertyPath("text.animators.0.positionX") = "Text Animator 1 / Positionx"
setLayerPropertyValue("text.animators.0.positionX", val) → parseAnimatorPropertyPath → animator.properties.position.setX()
updateImage() → applyAllAnimators() → perGlyphMode_ rendering
```

---

## 成功条件（達成状況）

| # | 条件 | 状態 |
|---|------|------|
| 1 | `ArtifactTextLayer` に複数のアニメーターを追加・削除できる | ✅ |
| 2 | 各アニメーターの Range Selector が UI から操作できる | ✅ |
| 3 | プロパティ（Position/Scale/Rotation/Opacity/Color）にキーフレームを打てる | ✅ |
| 4 | タイムライン再生で文字ごとのアニメーションが反映される | ✅ |
| 5 | プリセットからワンクリックでアニメーターを適用できる | ⏳ Phase 5 (将来) |
| 6 | 1000 文字のテキストで 60fps を維持できる | ⏳ Phase 6 (将来) |

---

## 残りのフェーズ（別マイルストーンとして deferred）

- **Phase 5**: プリセットシステム (`MILESTONE_MOTION_GRAPHICS_TEMPLATE` と統合予定)
- **Phase 6**: パフォーマンス最適化 (GlyphAtlas / GPU 経路依存)
- **Phase 7**: Text on Path (将来)
- **Phase 8**: 3D Text / Per-Character 3D (将来)