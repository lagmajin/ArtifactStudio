# MILESTONE: Text Animator (ArtifactCore) → Application Layer Integration

**Date**: 2026-04-27
**Status**: In Progress
**Priority**: High
**Related**: MILESTONE_TEXT_SYSTEM_2026-03-12 (C-TXT-5), MILESTONE_TEXT_ANIMATOR_SYSTEM_2026-03-25

---

## 概要

ArtifactCore で実装済みの `TextAnimatorEngine` を `ArtifactTextLayer` に完全統合し、After Effects ライクなテキストアニメーター UI を構築する。
既に `ArtifactTextLayer` には `animators_` ベクターと `perGlyphMode_` フラグが存在するが、UI 側での操作・プロパティパネル・タイムライン連携が未完了。

---

## 現状（2026-04-27）

### ArtifactCore 側（完了済み）
| コンポーネント | ファイル | 状態 |
|---|---|---|
| `TextAnimatorEngine` | `ArtifactCore/include/Text/TextAnimator.ixx` | ✅ 実装済み |
| `GlyphItem` (per-glyph data) | `ArtifactCore/include/Text/GlyphLayout.ixx` | ✅ 実装済み |
| `RangeSelector` / `WigglySelector` | `TextAnimator.ixx` | ✅ 実装済み |
| `AnimatorProperties` | `TextAnimator.ixx` | ✅ 実装済み |

### Artifact 側（途中）
| コンポーネント | ファイル | 状態 |
|---|---|---|
| `ArtifactTextLayer` animator state | `Artifact/include/Layer/ArtifactTextLayer.ixx` | ✅ 追加/削除/個数管理 API 接続済み |
| animator state / serialization | `Artifact/src/Layer/ArtifactTextLayer.cppm` | ✅ JSON 保存復元・property path 接続済み |
| glyph-aware fallback rendering | `Artifact/src/Layer/ArtifactTextLayer.cppm` | ✅ animator 設定が raster fallback 描画に反映 |
| プロパティパネル | Inspector | ⚠️ `text.animatorCount` 専用 editor と `text.animators.N.*` を公開済み |
| アニメーター追加 UI | Inspector | ⚠️ Add/Remove 直接操作は可能、専用パネルは未実装 |
| セレクター範囲 UI | Inspector | ⚠️ Start/End/Offset/Shape/Units を property 経由で編集可能 |
| タイムラインキーフレーム | Timeline | ❌ 未実装 |

---

## 目標設計

### 1. アニメーター統合アーキテクチャ
```
ArtifactTextLayer
├── std::vector<TextAnimatorState> animators_
├── bool perGlyphMode_
└── applyAllAnimators(GlyphItem& glyph, int glyphIndex, int totalGlyphs)
    ├── for each animator:
    │   ├── calculateWeight() // RangeSelector / WigglySelector
    │   └── applyAnimator()  // position, scale, rotation, opacity, color...
    └── finalizeGlyphTransform()
```

### 2. アニメータープロパティ（AnimatorProperties）
```cpp
struct AnimatorProperties {
    // Transform
    QVector3D position;      // offset position
    QVector3D scale;         // 1.0 = 100%
    QVector3D rotation;       // euler angles
    float opacity;            // 0.0 - 1.0
    float skew;               // skew angle
    float tracking;           // character spacing
    float z;                  // z-depth
    
    // Color
    QColor fillColor;
    QColor strokeColor;
    float strokeWidth;
    float blur;
};
```

### 3. セレクタータイプ
- **Range Selector**: 範囲指定（Start/End/Offset）+ シェイプ（Square/RampUp/RampDown/Triangle/Round/Smooth）
- **Wiggly Selector**: ランダム揺れ（周波数・振幅・位相）

---

## 実装マイルストーン

### Phase 1: アニメーター追加・削除 UI
**目標**: `ArtifactTextLayer` に対して複数のアニメーターを追加・削除できるようにする。

- [x] `ArtifactTextLayer` に `addAnimator()`, `removeAnimator(int index)` を実装
- [ ] アニメータータイプ選択 UI（Property Panel または専用ダイアログ）
- [x] アニメーターリスト表示 UI（各アニメーターの有効/無効切り替え）※ Inspector property group ベース
- [x] アニメーター名のカスタマイズ
- [x] `text.animatorCount` を Add/Remove 直接操作 editor に置換

### Phase 2: セレクター設定 UI
**目標**: Range Selector の Start/End/Offset とシェイプを UI から操作できる。

- [x] `RangeSelectorProperties` の最小 property 接続
  - Start (% または index)
  - End (% または index)
  - Offset (シフト)
  - Shape (ComboBox: Square/RampUp/RampDown/Triangle/Round/Smooth)
  - Units (Percentage/Index)
  - Anchor Point Grouping (Character/Word/Line/Paragraph/Span/All)
  - Order (Natural/Reverse/RandomStable/CenterOut/EdgeIn/LeftToRight/RightToLeft)
- [x] `WigglySelectorProperties` の最小 property 接続
  - Frequency, Amplitude, Phase
  - Random Seed

### Phase 3: アニメータープロパティ UI
**目標**: 各アニメーターの変形プロパティをキーフレームアニメーション可能にする。

- [x] Transform プロパティグループ（最小）
  - Position (X, Y, Z)
  - Scale (X, Y, Z)
  - Rotation (X, Y, Z)
  - Opacity
  - Skew / Skew Axis
  - Tracking Amount
- [x] Color プロパティグループ（最小）
  - Fill Color / Fill Hue / Fill Saturation / Fill Brightness
  - Stroke Color / Stroke Width
  - Blur (Gaussian)
- [ ] 各プロパティのキーフレーム化ボタン（ストップウォッチアイコン）

### Phase 4: タイムライン連携
**目標**: アニメータープロパティをタイムラインでキーフレームアニメーションできる。

- [ ] `TextAnimatorEngine` のプロパティを `AbstractProperty` として公開
- [ ] タイムラインパネルにアニメーターのプロパティトラックを表示
- [ ] キーフレーム追加・編集・削除の実装
- [ ] 再生中のリアルタイムプレビュー

### Phase 5: プリセットシステム
**目標**: AE の Animator Preset のように、よく使うアニメーションを保存・読み込みできる。

- [ ] `TextAnimatorPreset` クラス（JSON シリアライズ）
- [ ] プリセットブラウザ UI
- [ ] ビルトインプリセット作成
  - Fade In (By Character)
  - Slide In (Left/Right/Top/Bottom)
  - Scale In
  - Rotation In
  - Tracking Fade
  - Wiggly Position
  - Blur Reveal

### Phase 6: パフォーマンス最適化
**目標**: 大量の文字に対しても 60fps を維持する。

- [ ] グリフキャッシュの活用（変更時のみ再レイアウト）
- [ ] アニメーター計算のクエリ最適化
- [ ] GPU アクセラレーションの検討（Glyph Atlas の活用）

---

## ファイル構成（予定）

```
Artifact/include/Layer/
├── ArtifactTextLayer.ixx          (更新) アニメーター管理メソッド追加
└── ArtifactTextAnimatorUI.ixx     (新規) アニメーターUI管理クラス

Artifact/src/Layer/
└── ArtifactTextLayer.cppm          (更新) applyAllAnimators() 実装

ArtifactWidgets/include/
├── TextAnimatorPanel.ixx          (新規) アニメータープロパティパネル
├── SelectorPropertiesPanel.ixx     (新規) セレクタープロパティパネル
└── TextPresetBrowser.ixx          (新規) プリセットブラウザ

ArtifactWidgets/src/
├── TextAnimatorPanel.cppm
├── SelectorPropertiesPanel.cppm
└── TextPresetBrowser.cppm

docs/
└── TEXT_ANIMATOR_PRESETS.md       (新規) プリセット定義書
```

### Phase 7 (将来): Text on Path
**目標**: ベジェパスに沿ったテキスト配置をサポートする。

- [ ] `PathOptions` の data model（path layer reference / reverse / perpendicular / forced alignment / first margin / last margin）
- [ ] カスタムパス＋shape layer のパスをテキストパスとして選択する UI
- [ ] パス上の文字位置アニメーション（start / end / offset）
- [ ] パス形状変更に追従するレイアウト再計算

### Phase 8 (将来): 3D Text / Per-Character 3D
**目標**: テキストレイヤーのパーキャラクター 3D 対応。

- [ ] 各グリフに個別の Z 深度・3D トランスフォームを持たせる
- [ ] 3D シーン内でのテキストレンダリング
- [ ] 3D テキスト用マテリアルオプション（ベベル・押し出し）
- [ ] ライティング／シャドウとの連携

---

## 依存関係

- `TextAnimatorEngine` (`ArtifactCore`) が正しく動作すること ✅
- `GlyphItem` レイアウトが `perGlyphMode_` で正しく機能すること ⚠️
- タイムラインシステムがキーフレームをサポートしていること ⚠️
- プロパティパネル基盤 (`ArtifactWidgets`) が動作すること ⚠️

---

## 成功条件

1. `ArtifactTextLayer` に複数のアニメーターを追加・削除できる
2. 各アニメーターの Range Selector が UI から操作できる
3. プロパティ（Position/Scale/Rotation/Opacity/Color）にキーフレームを打てる
4. タイムライン再生で文字ごとのアニメーションが反映される
5. プリセットからワンクリックでアニメーターを適用できる
6. 1000 文字のテキストで 60fps を維持できる

---

## 参考: AE のアニメーター構造

```
Text Layer
├── Text Properties
│   ├── Source Text (font, size, color...)
│   └── Path Options
└── Animator 1
    ├── Range Selector 1
    │   ├── Start, End, Offset
    │   └── Shape, Units, Anchor Point Grouping, Order
    └── Properties
        ├── Position, Scale, Rotation, Opacity
        ├── Fill Color, Stroke Color, Stroke Width
        └── Blur, Tracking, Skew...
```
