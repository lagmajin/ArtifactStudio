# Milestone M-LE-1: Layer Solo View 編集機能強化（平面・シェイプ）

**Status:** Phase 1・2・3・4・5 未着手  
**Goal:** レイヤーソロビュー（`ArtifactRenderLayerWidgetv2`）における平面レイヤーとシェイプレイヤーの編集体験を段階的に強化する。現状は変形ギズモと多角形の頂点編集まで実装済みだが、形状固有のビューポートハンドル・グラデーション・ストロークスタイルが未対応。

> **作成: 2026-04-25**

---

## 現状確認（2026-04-25 時点）

### ✅ 実装済み

| 機能 | 場所 |
|------|------|
| 変形ギズモ（移動・スケール・回転、8ハンドル） | `TransformGizmo.cppm` |
| シェイプ頂点編集（カスタムポリゴン、Undo/Redo付き） | `ArtifactRenderLayerWidgetv2.cppm` L113-237 |
| マスク頂点 + ベジェタンジェントハンドル | `ArtifactRenderLayerWidgetv2.cppm` L159-283 |
| 平面レイヤー：単色・サイズ・変形・不透明度 | `ArtifactSolid2DLayer.cppm/ixx` |
| シェイプレイヤー：7種類（矩形・楕円・星・多角形・ライン・三角形・正方形） | `ArtifactShapeLayer.cppm/ixx` |
| シェイプ：フィル色・ストローク色・幅・角丸（矩形）・星内半径・多角形辺数 | `ArtifactShapeLayer.ixx` L41-64 |
| プロパティエディタ連携（色・数値・アニメーションキー） | `ArtifactPropertyEditor.cppm` |
| テキストレイヤーインライン編集 | `ArtifactCompositionEditor.cppm` |

### ❌ 未実装（本マイルストーン対象）

| 機能 | 優先度 |
|------|--------|
| 矩形/正方形の角丸ビューポートハンドル | P1 |
| 星シェイプの内半径ビューポートハンドル | P1 |
| シェイプレイヤー グラデーションフィル | P2 |
| 平面レイヤー グラデーションフィル | P2 |
| プロパティエディタ グラデーションピッカー | P2 |
| ストローク破線パターン（点線・鎖線など） | P3 |
| ストローク端点/接合スタイル（miter・round・bevel） | P3 |
| ストローク配置（内側・中央・外側） | P3 |
| ギズモドラッグ中のXYWH数値HUDオーバーレイ | P4 |
| シェイプ頂点ベジェカーブ編集 | P5 |

---

## Phase 1: シェイプ固有ビューポートハンドル

### 目的
プロパティパネルを触らずに、ビューポート上のドラッグ操作だけで角丸半径・星内半径を調整できるようにする。After Effects の「シェイプツール内ハンドル」相当。

### 実装内容

#### 1-A: 矩形/正方形 角丸ハンドル

**ハンドル位置（局所座標）**  
右上コーナーから内側へ `cornerRadius` ピクセル分ずらした点に円形ハンドル 1 個を配置。
```
handle_pos = (width - cornerRadius, cornerRadius)   // 右上コーナー内側
```

**インタラクション**
- `hitTestCornerRadiusHandle()` を `ArtifactRenderLayerWidgetv2.cppm` に追加
- ドラッグ量 → `cornerRadius` 更新（0 ≤ r ≤ min(w,h)/2 にクランプ）
- Undo/Redo: `CornerRadiusEditCommand`（ShapeEditCommand 同様のパターン）
- 描画: `PrimitiveRenderer2D::drawSolidCircle` で 8px ハンドル

**対象ファイル**
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`（ヒットテスト・描画・ドラッグ処理）
- `Artifact/include/Layer/ArtifactShapeLayer.ixx`（API は既存 `setCornerRadius` を使用）

#### 1-B: 星シェイプ 内半径ハンドル

**ハンドル位置**  
第1内頂点（内半径リングの最初のポイント）にハンドルを配置。

**インタラクション**
- ドラッグで `starInnerRadius` (0.0〜1.0) を更新
- 中心からのドラッグ距離 → 外半径比で正規化
- Undo/Redo: `StarRadiusEditCommand`

**対象ファイル**
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- `Artifact/include/Layer/ArtifactShapeLayer.ixx`（`setStarInnerRadius` 使用）

### 対象ファイル一覧

| ファイル | 変更内容 |
|---------|---------|
| `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` | ヒットテスト、描画、ドラッグステート追加 |
| `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` | `Impl` に `isDraggingCornerRadius_`・`isDraggingStarInnerRadius_` フラグ追加 |

### 見積: 4-6h

---

## Phase 2: グラデーションフィル

### 目的
シェイプレイヤーと平面レイヤーに線形・放射グラデーションフィルを追加する。

### 実装内容

#### 2-A: ArtifactShapeLayer グラデーションサポート

**新規 API（`ArtifactShapeLayer.ixx` 追加）**
```cpp
enum class FillType { Solid, LinearGradient, RadialGradient };
struct GradientStop { float position; FloatColor color; };
void setFillType(FillType type);
FillType fillType() const;
void setGradientStops(const std::vector<GradientStop>& stops);
std::vector<GradientStop> gradientStops() const;
void setGradientAngle(float degrees);          // LinearGradient
float gradientAngle() const;
void setGradientCenter(QPointF relative);      // RadialGradient (0-1, 0-1)
void setGradientRadius(float ratio);           // RadialGradient
```

**描画変更（`ArtifactShapeLayer.cppm`）**
- `draw()` の塗りつぶし部分を `FillType` で分岐
- LinearGradient: `QLinearGradient` ベースで事前に CPU 側テクスチャを生成 → `drawSprite`
- RadialGradient: `QRadialGradient` ベース
- ソフトウェアパス（QImage経由）を先に実装し、後でGPUシェーダへ移行可能な設計にする

#### 2-B: ArtifactSolid2DLayer グラデーションサポート

同様に `FillType` / `GradientStop` API を追加（`ArtifactSolid2DLayer.ixx/.cppm`）。

#### 2-C: プロパティエディタ グラデーションピッカー

**新規ウィジェット: `ArtifactGradientPropertyEditor`**
- ストップ追加・削除・色変更
- ストップ位置ドラッグ
- 線形/放射 タイプ切替
- プレビューバー表示

**対象ファイル**
- `Artifact/include/Layer/ArtifactShapeLayer.ixx`
- `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- `Artifact/include/Layer/ArtifactSolid2DLayer.ixx`
- `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`（グラデーションピッカー組み込み）

### 見積: 10-16h

---

## Phase 3: ストロークスタイル強化

### 目的
直線のソリッドストロークのみ対応している現状を拡張し、破線・端点・接合スタイルをサポートする。

### 実装内容

#### 3-A: ストローク API 拡張（`ArtifactShapeLayer.ixx`）
```cpp
enum class StrokeCap  { Flat, Round, Square };
enum class StrokeJoin { Miter, Round, Bevel };
enum class StrokeAlign { Center, Inside, Outside };
struct DashPattern { float dash, gap; };

void setStrokeCap(StrokeCap cap);
void setStrokeJoin(StrokeJoin join);
void setStrokeAlign(StrokeAlign align);
void setDashPattern(const std::vector<DashPattern>& pattern);  // 空=実線
```

#### 3-B: 描画変更
- QPainterPath + QPen で各スタイルを適用
- StrokeAlign: `Inside` → パス縮小オフセット、`Outside` → 拡大オフセット
- DashPattern: `QPen::setDashPattern()` で実装

#### 3-C: プロパティ UI
- `StrokeCap` / `StrokeJoin` / `StrokeAlign` は各 3 択のアイコンボタン行
- DashPattern はプリセット（実線・点線・鎖線・一点鎖線）+ カスタム

**対象ファイル**
- `Artifact/include/Layer/ArtifactShapeLayer.ixx`
- `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`

### 見積: 6-10h

---

## Phase 4: ソロビュー UX — XYWH 数値 HUD

### 目的
ギズモでドラッグ中に現在の X/Y/W/H をビューポート上に小さく表示し、数値の確認を可能にする。

### 実装内容

**OverlayDrawing in `ArtifactRenderLayerWidgetv2`**
- `paintEvent` の QPainter レイヤーに、ギズモドラッグ中のみ追加描画
- 表示内容: `X: 120  Y: 80  W: 400  H: 300`（単位: px）
- 位置: ギズモ境界ボックスの右下 + 8px オフセット（画面内にクランプ）
- フォント: システムフォント 10pt、背景: 半透明黒ラベル

**追加ステート**
```cpp
bool isTransformDragging_ = false;
QRectF transformDragBounds_;   // 現在のレイヤー境界（ビューポート座標）
```

**TransformGizmo 連携**
- `gizmo.isDragging()` の状態をチェックして描画切替
- `gizmo.boundingRect()` から現在サイズを取得

**対象ファイル**
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`（paintEvent 追加描画）
- `Artifact/include/Widgets/Render/TransformGizmo.ixx`（boundingRect アクセサが未公開なら追加）

### 見積: 3-4h

---

## Phase 5: シェイプ頂点 ベジェカーブ編集

### 目的
カスタムポリゴンの各頂点をベジェカーブに変換し、滑らかな曲線シェイプを描けるようにする。After Effects の「頂点の変換（滑らか/コーナー）」相当。

### 実装内容（詳細は別途設計が必要）

- `customPolygonPoints` を拡張し `CustomPathVertex { QPointF pos; QPointF inTangent; QPointF outTangent; bool smooth; }` に変更
- `buildShapeEditSeedPoints` の返り値型を変更（後方互換注意）
- ソロビューに inTangent / outTangent の見えるハンドルを追加
- 既存の `MaskPath` 編集ロジック（L159-283）を参考にする

> ⚠️ データ構造変更を伴うため、既存のカスタムポリゴン JSON の後方互換を必ず維持すること。

**対象ファイル**
- `Artifact/include/Layer/ArtifactShapeLayer.ixx`（CustomPathVertex 追加）
- `Artifact/src/Layer/ArtifactShapeLayer.cppm`（JSON互換維持）
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`（ハンドル描画・ドラッグ）

### 見積: 12-18h（設計含む）

---

## ファイル対応表

| ファイル | Phase |
|---------|-------|
| `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` | 1, 4, 5 |
| `Artifact/include/Layer/ArtifactShapeLayer.ixx` | 2, 3, 5 |
| `Artifact/src/Layer/ArtifactShapeLayer.cppm` | 2, 3, 5 |
| `Artifact/include/Layer/ArtifactSolid2DLayer.ixx` | 2 |
| `Artifact/src/Layer/ArtifactSolid2DLayer.cppm` | 2 |
| `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` | 2, 3 |
| `Artifact/include/Widgets/Render/TransformGizmo.ixx` | 4 |

---

## 実装順の推奨

```
Phase 1（ビューポートハンドル）
  ↓
Phase 4（HUD）          ← Phase 1 のドラッグステートを再利用
  ↓
Phase 2（グラデーション） ← 独立して進められる
  ↓
Phase 3（ストロークスタイル）
  ↓
Phase 5（ベジェ）       ← データ構造変更を伴うので最後
```
