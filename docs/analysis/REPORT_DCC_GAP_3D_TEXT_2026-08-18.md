# DCC ギャップ分析：3D レイヤー & テキスト機能 — 2026-08-18

**最終更新:** 2026-08-18
**調査対象:** ArtifactStudio 親リポジトリ、`Artifact`、`ArtifactCore` の現行ソース
**調査方法:** ソースコード直接検証（`.cppm`/`.ixx`/`.cpp`）。`docs/analysis/` の既存レポートは参考のみ。
**比較対象 DCC:**
- 3D: Cinema 4D, Maya, 3ds Max, Blender, Element 3D
- テキスト: Adobe After Effects (主要)、Nuke (補助)

---

## 1. 3D レイヤー

### 実装済み ✅（ソースで確認）

| 機能 | 実装ファイル | 実装内容 |
|---|---|---|
| **基本ジオメトリ** | `Artifact3DModelLayer.cppm` | Plane, Cube, Sphere, Cylinder, Cone |
| **外部モデル読み込み** | `MeshImporter.cppm` | glTF/GLB, FBX, OBJ, PLY, STL, USDA (ufbx バックエンド) |
| **PBR マテリアル** | `Material.ixx`, `MeshRenderer.cppm` | BaseColor, Metallic, Roughness, Opacity, AO, Emission, EmissionStrength, EmissionTexture |
| **PBR テクスチャ** | `MeshRenderer.cppm` | 6種 (baseColor, metallicRoughness, normal, emission, occlusion, opacity) + auto-detect |
| **マテリアルプリセット** | `Material.ixx` | makeDefault, makeMetal, makePlastic, makeGlass, makeEmissive |
| **GPU レンダリング** | `MeshRenderer.cppm`, `ArtifactIRenderer.cppm` | DiligentEngine DX12/Vulkan |
| **シーンライト** | `Light.ixx`, `ArtifactLightLayer.cppm` | Directional, Point, Spot, Ambient (4種) + GOBO |
| **ライトリンク** | `Light.ixx` | per-layer light filtering (`lightAppliesToLayer()`) |
| **3D ギズモ** | `Artifact3DGizmo.cppm` | Move/Rotate/Scale ハンドル (ボリューム型) |
| **インスタンシング** ✅ | `MeshRenderer.cppm:1381,1596`, `ArtifactCloneLayer.cppm:1160` | `InstanceData` 構造体, `updateInstanceData()`, `draw(instanceCount)` で GPU インスタンシング対応済み |
| **シャドウマッピング** ✅ | `MeshRenderer.cppm:1681-1744` | `prepareShadow()`, `drawShadow()`, `setShadowMap()` — Directional + Spot ライトのハードシャドウ。`ArtifactIRenderer.cppm:3624` `renderShadowMapFrame()` |
| **レンダーモード** | `Artifact3DModelLayer.cppm` | Wireframe, Solid + WireOverlay |
| **Transform** | `AnimatableTransform3D.cppm` | position/rotation/scale/anchor + キーフレーム |

### 未実装 ❌ / 部分実装 🟡（ソースで確認）

| 機能 | 状態 | ソース根拠 |
|---|---|---|
| **IBL / 環境マップ** | 🟡 実装済み・runtime未確認 | `MeshRenderer.cppm` の HDR/EXR→cubemap、CPU irradiance、BRDF LUT、PBR binding、`ArtifactIRenderer` の環境伝播、Skybox 接続まで実装。GPU prefilter と runtime受入れは未確認。 |
| **3軸回転** | ❌ 未実装 | `AnimatableTransform3D.cppm:60` — `rotation_` は **1つの float** (degrees)。`setRotation(time, degrees)` (line 322) 唯一の回転 API。`rotationX/Y/Z`、クォータニオン、Euler 変換 **なし**。`StaticTransform3D.cppm:11` では `rotationX/Y/Z` が**ローカル変数**として宣言されているが実装は不明。 |
| **デフォーマ/モディファイア** | ❌ 未実装 | `Bend`/`Twist`/`Taper`/`FFD`/`vertexDeform`/`Deformable` など **一切ヒットなし** (QColor の `#FFD700` 色定数以外)。`VolumeModifier.cppm` は体積レンダリングの modifier、3D メッシュ変形ではない。 |
| **スキニング/アニメーション** | 🟡 部分実装 | `MeshImporter.cppm:853-886` — PMD フォーマットのみ boneIndices/boneWeights をパース。glTF/ufbx 経路 (`ufbx_mesh` の vertex_position/normal/uv) ではスキニングデータ **抽出なし**。`ufbx_scene` に `skin` / `joint` / `animation` / `sampler` **未検索**. Skeleton / joint hierarchy **未実装**。 |
| **ソフトシャドウ** | 🟡 基盤実装・runtime未確認 | `MeshRenderer.cppm` に 3×3 PCF と softness パラメータを実装。Variance Shadows、Point/Area shadow、runtime品質確認は未完了。 |
| **Point/Area ライトシャドウ** | ❌ 未実装 | Directional/Spot のハードシャドウのみ。Point ライトキャスター非対応 (AE_PAIN_POINT 確認済み)。 |
| **CSM (Cascaded Shadow Maps)** | ❌ 未実装 | `lightViewProjection` は単一行列 (single light space)。Cascade 制御 **なし**。 |
| **Cube Shadow Maps (Point)** | ❌ 未実装 | **未発見**。 |
| **リフレクションプローブ** | ❌ 未実装 | `reflectionProbe` / `ReflectionProbe` **未発見**。 |
| **テッセレーション/サブディビジョン** | ❌ 未実装 | Hull/Domain シェーダー, Catmull-Clark **未発見**。 |
| **法線マップベイク** | ❌ 未実装 | **未発見**。 |
| **頂点カラー描画** | 🟡 データのみ | `MeshImporter.cppm:308` で頂点カラーをインポードするコードありが、描画/シェーダーへの反映 **未確認**。 |

### DCC 別比較

| 機能 | C4D | Maya | 3ds Max | Blender | Element 3D | Artifact | ギャップ |
|---|---|---|---|---|---|---|---|
| IBL / 環境マップ | ✅ | ✅ | ✅ | ✅ | ✅ (簡易) | ❌ | **🔴 最大のギャップ** |
| シャドウマッピング | ✅ | ✅ | ✅ | ✅ | ✅ (AO) | 🟡 (ハードのみ) | ソフト影・Point/AREA 影 |
| 3軸回転 | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ (1軸のみ) | **🟡→❌ 根本欠落** |
| デフォーマ | ✅ (20種) | ✅ | ✅ | ✅ | ❌ | ❌ | **🔴** |
| スキニング | ✅ | ✅ | ✅ | ✅ | ❌ | 🟡 (PMDのみ) | **🟡→❌** |
| パーティクルインスタンス | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | 🟡 (Cloner→Instancing 基盤はある) |
| アニメーション再生 | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | **🔴** |
| SSAO | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ | 🟢 |
| 反射プローブ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | 🟡 |
| テクスチャトランスフォーム | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | 🟡 |

---

## 2. テキスト機能

### 実装済み ✅（ソースで確認）

| 機能 | 実装ファイル | 実装内容 |
|---|---|---|
| **TextLayer データモデル** | `ArtifactTextLayer.cppm` | 4500行超の実装。JSON セーブ/ロード (`obj["text.*"]` プロパティ群) |
| **TextAnimator エンジン** | `TextAnimator.ixx` (173行), `TextAnimator.cppm` | **実装完備**: `RangeSelector` (start/end/offset, `SelectorUnits`: Percentage/Index/Cluster/Line/Tag, `SelectorShape`: Square/RampUp/RampDown/Triangle/Round/Smooth, `SelectorOrder`: 7種, easeHigh/easeLow, regex, anchorGrouping), `WigglySelector` (wigglesPerSecond/correlation/phase/seed), `AnimatorProperties` (position/scale/rotation/opacity/skew/tracking/z/**fillColor/strokeColor/strokeWidth/blur**) |
| **TextAnimator スタック** | `TextAnimator.cppm:699-753` | `applyAnimatorStack()` — 複数アニメーターの合成 (`std::span<const std::tuple<RangeSelector, WigglySelector, AnimatorProperties>>`) |
| **TextAnimator カラー** | `TextAnimator.cppm:678-693` | `props.colorEnabled` → `glyphs[i].hasColorOverride / fillColorOverride / fillColorOverrideWeight`, `strokeColorOverride`, `offsetStrokeWidth`, `offsetBlur` — セレクター重みで per-glyph カラーオーバーライド適用 |
| **TextAnimator シリアライズ** | `ArtifactTextLayer.cppm:1447,2156` | `serializedProperties`, `serializedAnimatorProperties` — JSON 保存/復元 |
| **TextAnimator カラープロパティエディタ** | `ArtifactPropertyEditorTextAnimatorColor.cppm` | カラーアニメータープロパティの専用編集 UI |
| **Glyph Atlas** | `GlyphAtlas.cppm` | `QRawFont` + `QFontDatabase` ベース、GPU glyph レンダリング、絵文字対応 |
| **Font management** | `FreeFont.ixx`, `FontPickerWidget.cppm`, `TextStyle.ixx` | `FontManager::makeFont()`, `availableFamilies()`, `containsCjkCharacters()`, `resolvedFamily()` |
| **Font stretch** ✅ | `TextStyle.ixx:38`, `FreeFont.ixx:232` | `fontStretch` (50-200, animatable) — `font.setStretch()` 実装済み |
| **Tracking** ✅ | `TextStyle.ixx:37` | `tracking` (letter spacing) — `font.setLetterSpacing()` 実装済み |
| **Leading** ✅ | `TextStyle.ixx:39` | `leading` (auto when -1) |
| **Paragraph styles** ✅ | `TextStyle.ixx:62-71` | HorizontalAlignment, VerticalAlignment, WrapMode, box dimensions, paragraph spacing, path binding |
| **Text style** ✅ | `TextStyle.ixx:33-60` | fontFamily, fontSize, pixelSize, tracking, fontStretch, leading, fontWeight(Normal/Bold), fontStyle(Normal/Italic), allCaps, underline, strikethrough, **fillColor**, stroke(strokeEnabled/strokeColor/strokeWidth), shadow(shadowEnabled/shadowColor/offset/blur) |

### 未実装 ❌ / 部分実装 🟡（ソースで確認）

| 機能 | 状態 | ソース根拠 |
|---|---|---|
| **Viewport inline editing** | ❌ 未実装 | `ArtifactTextLayer.cppm` に `TextTool`/`inlineEdit`/`setEditingText`/`textEditMode`/`editText` **未発見**。`ArtifactToolManager.cppm:43` に `case ToolType::Text: return "TextTool";` の文字列のみ。modal 編集のみ (AE_PAIN_POINT 確認)。 |
| **Expression Selector** | ❌ 未実装 | `TextAnimator.ixx`/`.cppm` に `ExpressionSelector`/`textIndex`/`textTotal` **未発見**。RangeSelector と WigglySelector のみ。 |
| **Variable fonts** | ❌ 未実装 | `FontDescriptor.ixx` に `fontVariation`/`fontAxis` **未発見**。`FontWeight` は `Normal=0`/`Bold=1` の2値。`FontStyle` は `Normal=0`/`Italic=1` の2値。 |
| **数値フォントウェイト** | ❌ 未実装 | `FreeFont.ixx:226` — `font.setBold(style.fontWeight == FontWeight::Bold)` の **2値**。QFont::Weight (Thin～Black, 100-900) の数値対応 **なし**。 |
| **ベースラインシフト** | ❌ 未実装 | `TextStyle.ixx` に `baselineShift` **未発見**。 |
| **個別 Fill/Hue/Sat/Brightness アニメータープロパティ** | 🟡 部分実装 | `AnimatorProperties` に `fillColor` (FloatRGBA) はあるが、AE の Fill RGB / Hue / Sat / Brightness の**個別プロパティ**はなし。単一の `fillColor` オーバーライドのみ。 |
| **HarfBuzz シェーピング** | ❌ スタブ | `TextShapingBackend.cppm:1199-1202` — `HarfBuzzShapingBackend::shape()` が `QtShapingBackend{}.shape(request)` に**フォールバック** (コメント: "Temporary fallback until the HarfBuzz adapter is wired in") |
| **Text animator presets (.ffx 相当)** | ❌ 未実装 | `TextAnimator.cppm` に preset save/load **未発見**。 |
| **Per-character range styling** | ❌ 未実装 | アニメーター経由のカラーオーバーライドはあるが、**独立した range スタイルエディタ** (AE の Character panel) **未発見**。 |
| **Text on path** | 🟡 データのみ | `ParagraphStyle::PathBinding` があるが、パス作成/編集 UI **未確認**。 |
| **Text layer creation UI** | ❌ 未実装 | `ArtifactTextLayer.cppm` はデータモデルのみ。新規テキストレイヤー作成ダイアログ **未確認**。 |

---

## 3. 優先ギャップサマリ

### P0 (最重要 — 根本的な未実装)

1. **3D IBL / 環境マップの受入れ・GPU最適化** — CPU irradiance／PBR／Skybox は実装済みだが、GPU prefilter と runtime 検証が未完了
2. **3D 3軸回転** — `AnimatableTransform3D` が 1 軸のみ、これは 3D ワークフローの根本欠落
3. **3D デフォーマ** — AE 3D 変形の必須機能、1つも実装なし

### P1 (重要 — 実用性に直結)

4. **Text Viewport inline editing** — AE 最大の UX 差
5. **3D スキニング/アニメーション** — glTF アニメーション再生未対応 (ufbx でスキニング抽出未実装)
6. **Text Animator Expression Selector** — `textIndex`/`textTotal` 未実装

### P2 (中優先 — 機能拡充)

7. **3D ソフトシャドウ / Point ライトキャスター / CSM** — 影品質の大幅不足
8. **Variable fonts / 数値フォントウェイト** — フォント表現の限界
9. **3D パーティクルインスタンス** — Cloner コンポーネントの 3D 拡張 (InstanceData 基盤はある)

---

## 4. 修正対象ドキュメント

- `docs/analysis/THREED_LAYER_FEATURE_GAP_DCC_COMPARISON_2026-08-08.md` — 3D ギャップ分析（このレポートで補足）
- `docs/analysis/GAP_AE_NUKE_2026-08-01.md` — AE/Nuke ギャップ分析
- `docs/analysis/REPORT_DCC_GAP_UPDATE_2026-08-15.md` — DCC ギャップ最新レポート

## 5. 調査上の注意

- インスタンシングは `MeshRenderer::draw(instanceCount)` + `updateInstanceData()` で **実装完了** — 3D Cloner/パーティクルインスタンスの基盤は確立済み
- シャドウマッピングの `prepareShadow`/`drawShadow`/`setShadowMap` は **実装完了** — ただしソフトシャドウ、Point ライトキャスター、CSM は未対応
- HarfBuzz シェーピングエンジンは **スタブ** (Qt フォールバック) — 実装の手がかりは `TextShapingBackend.cppm:1199-1202`
- `fontStretch` は実装済み (50-200) — 既存 docs が見落していた可能性あり
- `tracking` (letter spacing) は実装済み — `TextStyle.tracking` + `font.setLetterSpacing()`
