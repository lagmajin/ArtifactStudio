# ShapeLayer クラス分析レポート

**作成日**: 2026-04-17  
**対象クラス**: `ArtifactCore::ShapeLayer`  
**ファイル**: `ArtifactCore/include/Shape/ShapeLayer.ixx`  
**モジュール**: `Shape.Layer`

---

## 1. クラス辞典（Class Dictionary）

### 基本情報

| 項目 | 内容 |
|------|------|
| ヘッダーファイル | `ArtifactCore/include/Shape/ShapeLayer.ixx` |
| 実装ファイル | `ArtifactCore/src/Shape/ShapeLayer.cppm` **（未作成／存在せず）** |
| モジュール名 | `Shape.Layer` |
| 名前空間 | `ArtifactCore` |
| クラス型 | 具象クラス（PIMPL イディオム採用） |
| 基底クラス | （なし）——独立したレイヤーエンティティ |

### クラス定義の構造

```cpp
export namespace ArtifactCore {
    class ShapeLayer {
        class Impl;       //  forward declaration
        Impl* impl_;      //  PIMPL ポインタ
        // ...
    };
}
```

**PIMPL の意図**: 実装詳細を `.cppm` ファイルに隠蔽し、ヘッダー依存を最小化。ただし現状、対応する `.cppm` は存在しない。

### 主要メソッド一覧

| カテゴリ | メソッド | 概要 | 実装状况 |
|---------|---------|------|----------|
| **Construction** | `ShapeLayer()` | デフォルト構築 | 宣言のみ（未実装） |
| | `~ShapeLayer()` | デストラクタ | 宣言のみ（未実装） |
| | `ShapeLayer(const ShapeLayer&)` | コピーctor | 宣言のみ（未実装） |
| | `operator=(const ShapeLayer&)` | コピー代入 | 宣言のみ（未実装） |
| | `ShapeLayer(ShapeLayer&&)` | ムーブctor | 宣言のみ（未実装） |
| | `operator=(ShapeLayer&&)` | ムーブ代入 | 宣言のみ（未実装） |
| **Layer Properties** | `name() const` / `setName()` | レイヤー名 | 宣言のみ |
| | `layerId() const` | レイヤーID | 宣言のみ |
| | `isVisible() const` / `setVisible()` | 表示切替 | 宣言のみ |
| | `isLocked() const` / `setLocked()` | ロック切替 | 宣言のみ |
| | `isSolo() const` / `setSolo()` | ソロ切替 | 宣言のみ |
| **Content** | `content()` / `content() const` | ルート `ShapeGroup` 取得 | 宣言のみ |
| | `addShape(std::unique_ptr<ShapeElement>)` | シェイプ追加 | 宣言のみ |
| | `clearContent()` | 全シェイプ削除 | 宣言のみ |
| | `shapeCount() const` | 子シェイプ数 | 宣言のみ |
| **Transform** | `transform()` / `transform() const` | レイヤートランスフォーム | 宣言のみ |
| | `anchorPoint() const` / `setAnchorPoint()` | アンカーポイント | 宣言のみ |
| | `position() const` / `setPosition()` | 位置 | 宣言のみ |
| | `scale() const` / `setScale()` | スケール | 宣言のみ |
| | `rotation() const` / `setRotation()` | 回転角度 | 宣言のみ |
| | `opacity() const` / `setOpacity()` | 不透明度 | 宣言のみ |
| **Geometry** | `boundingRect() const` | バウンディングボックス | 宣言のみ |
| | `contains(const QPointF&) const` | 点の包含判定 | 宣言のみ |
| **Rendering** | `render(const QSize&, double) const` | 画像レンダリング | 宣言のみ |
| | `toPainterPath() const` | QPainterPath 変換 | 宣言のみ |
| **Blend** | `blendMode() const` / `setBlendMode()` | ブレンドモード | 宣言のみ |
| | `enum class BlendMode` | 14 моude（Normal〜Luminosity） | 宣言済 |
| **Utility** | `clone() const` | ディープコピー | 宣言のみ |
| | `toSvg() const` | SVG エクスポート | 宣言のみ |
| | `fromSvg(const QString&)` | SVG インポート（static） | 宣言のみ |
| **Factory Methods** | `createEmpty()` | 空のレイヤー作成 | 宣言のみ |
| | `createRectangle()` | 矩形レイヤー作成 | 宣言のみ |
| | `createEllipse()` | 楕円レイヤー作成 | 宣言のみ |
| | `createStar()` | 星形レイヤー作成 | 宣言のみ |
| | `createPolygon()` | 多角形レイヤー作成 | 宣言のみ |

### 叶列の主要メンバー変数

| 可視性 | メンバー | 型 | 説明 |
|--------|---------|-----|------|
| `private` | `impl_` | `Impl*` | PIMPL ポインタ。実装は `.cppm` で定義予定 |
| `private` | `Impl` | `class` | 実装詳細を隠蔽するための不完全型 |

### 関連補助クラス（同一ヘッダー内）

| クラス | 説明 |
|--------|------|
| `ShapeLayer::BlendMode` | ブレンドモード列挙型 |
| `ShapeLayerFactory` | 静的ファクトリメソッド群を提供する補助クラス |

---

## 2. 依存関係マップ（Dependency Map）

### 依存先モジュール・クラス（import リスト）

`ShapeLayer.ixx:43-45` より：

```cpp
import Shape.Types;    // → FillSettings, StrokeSettings, ShapeTransform, ShapeType, ...
import Shape.Path;     // → ShapePath, PathShape, PathCommand, ...
import Shape.Group;    // → ShapeGroup, ShapeElement, ShapeOperator, ...
```

### 型依存関係詳細

| 依存先 | 使用箇所 | 依存タイプ |
|--------|---------|-----------|
| `Shape.Types` | `ShapeTransform`（メソッド `transform()` の返し値） | 型依存 |
| | `Point2DValue`（anchor/position/scale の型） | 型依存 |
| | `FillSettings`, `StrokeSettings`（`createRectangle()` 等の引数） | 型依存 |
| | `ShapeType`（内部で使用想定） | 型依存 |
| `Shape.Path` | `ShapePath`（`processedPaths()` 等で使用） | 型依存 |
| | `PathShape`（`addShape()` で追加可能） | 型依存 |
| `Shape.Group` | `ShapeGroup`（`content()` の返し値，`addShape()` 内部で使用） | 型依存 |
| | `ShapeElement`（`addShape()` の引数型） | 型依存 |
| | `ShapeOperator`（間接的：`ShapeGroup` が保持） | 転送依存 |

### 間接依存（ transitively through imports ）

```
Shape.Layer
 ├─ Shape.Types
 │   └── (QString, QPointF, QColor, QRectF, ...)
 ├─ Shape.Path
 │   └── Shape.Types  →  PathCommand, BezierSegment
 └─ Shape.Group
     ├─ Shape.Path    →  ShapePath, PathShape
     └─ Shape.Operator → ShapeOperator, TrimPaths, ...
```

### 被依存関係（どのクラスから使われているか）

| 依存元ファイル | 依存元クラス／モジュール | 依存方法 |
|---------------|----------------------|---------|
| `ArtifactCore_Library_Reference.md` | ユーザーコード例 | `import Shape.Layer;` |
| `Artifact/src/Layer/ArtifactShapeLayer.cppm` | `ArtifactShapeLayer` 実装 | `#include` ではなく別モジュール（名前空間 `Artifact` 側で独自実装） |
| `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` | UI ウィジェット | `dynamic_pointer_cast<ArtifactShapeLayer>`（型チェック） |

**注**: `Artifact` プロジェクト側に `ArtifactShapeLayer` という別クラスが存在し、`ArtifactAbstractLayer` を継承して`ShapeLayer` とは別実装。`ShapeLayer` はコアロジックを提供するが、アプリケーション層では `ArtifactShapeLayer` がラップして利用されている可能性。

### 簡易テキスト図

```
[User Code]
     │
     │ import Shape.Layer
     ▼
┌─────────────────┐
│  ShapeLayer     │  ← ArtifactCore（インターフェース層）
│  ├─ content(): ShapeGroup   ──┐
│  ├─ addShape()               │
│  ├─ transform(): ShapeTransform
│  └─ render(): QImage         │
└─────────────────┘           │
                              ▼
                    ┌─────────────────────┐
                    │   ShapeGroup        │
                    │   ├─ children: ShapeElement*  ←┐
                    │   ├─ operators: ShapeOperator* │
                    │   └─ transform: ShapeTransform │
                    └─────────────────────┘         │
                              │                     │
                    ┌─────────┴─────────┐           │
                    ▼                   ▼           │
            ┌───────────────┐   ┌───────────────┐   │
            │  PathShape    │   │  Rectangle-   │   │
            │  ├─ path: ShapePath   │  PathShape    │   │
            │  ├─ stroke:       │   │  （他プリミティブ） │
            │  │  StrokeSettings│   └───────────────┘   │
            │  └─ fill: FillSettings│                   │
            └───────────────┘                       │
                              │                     │
                    ┌─────────▼─────────┐           │
                    │   ShapePath       │◄──────────┘
                    │   ├─ commands: std::vector<PathCommand>
                    │   └─ toPainterPath()
                    └───────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │  ShapeOperator    │
                    │  (TrimPaths, ...) │
                    └───────────────────┘

[App Layer (Artifact)]
     │
     │ （ラッパー実装）
     ▼
┌─────────────────────┐
│ ArtifactShapeLayer  │
│ ─ ArtifactAbstractLayer  ← 別モジュールで独自実装
│  ├─ シェイプ種別プロパティ（Rect/Ellipse/Star/Polygon）
│  ├─ 色・線幅設定プロパティ
│  ├─ QImage 生成（toQImage）
│  └─ JSON シリアライズ
└─────────────────────┘
```

---

## 3. 責務表（Scope of Responsibility）

### ✅ やること（Scope IN）

| 責務領域 | 説明 |
|---------|------|
| **レイヤー管理** | 単一のシェイプレイヤーを表現。名前、可視性、ロック、ソロ状態を管理 |
| **コンテナとしてのシェイプ集約** | `ShapeGroup` をルートコンテナとして保持し、複数の `ShapeElement`（`PathShape` 等）を階層管理 |
| **トランスフォーム階層** | レイヤー全体のトランスフォーム（`ShapeTransform`：位置・回転・スケール・アンカー・不透明度）を提供 |
| **ジオメトリ計算** | 全内容のバウンディングボックス統合、点の包含判定 |
| **レンダリング抽象化** | `render(QSize, time)` による画像生成、`toPainterPath()` による Qt 描画経路への変換 |
| **ブレンドモード** | 14種類のブレンドモード列挙型とアクセサ |
| **シリアライズ／デシリアライズ** | `toSvg()`（ SVG 出力 ）, `fromSvg()`（静的インポート） |
| **プリミティブ生成** | 静的ファクトリメソッドにより、矩形・楕円・星形・多角形の初期シェイプレイヤーを生成 |
| **複製** | `clone()` によるディープコピー（PIMPL 内で実装予定） |

### ❌ やらないこと（Scope OUT）

| 除外責務 | 理由 |
|----------|------|
| **実体の描画（rasterization）** | `render()` は抽象的委譲。実際のレンダリングは `ArtifactIRenderer` 等外部レンダラに依存する design 想定 |
| **アニメーションキーフレーム管理** | Transform 値の時間変化は `ShapeTransform` の各フィールドが `Point2DValue` であり、アニメーション対応はプロパティシステム側で扱う |
| **エフェクト・フィルタ** | ブレンドモードは持つが、グロー・ブラー等のEffectは別レイヤー・システム |
| **子レイヤー階層** | `ShapeLayer` は単一レイヤー。複数レイヤーの合成は親 `Composition` クラス等の責務 |
| **パスの幾何学演算（トリム／オフセットなど）** | `ShapeOperator` は `ShapeGroup` が保持。`ShapeLayer` はあくまでコンテナ・変換の責務に特化 |
| **ファイルI/O（SVGの完全パース）** | `fromSvg()` は簡単なインポートを想定。完全な SVG DOM パースは別ユーティリティ |
| **undo/redo 履歴** | 変更履歴は外部 Command システム側で管理 |
| **GPU リソース管理** | ソフトウェアレンダリング想定。DiligentEngine/DX12 などの GPU バックエンドは別レイヤー |

### 設計理由（なぜそれを担うのか）

| 観点 | 設計判断 |
|------|---------|
| **PIMPL イディオム** | `Impl* impl_` とすることで、実装変更（ `.cppm` の追加）にヘッダー再コンパイルを発生させず、`ShapeTypes` 等への依存を隠蔽。`ShapeTypes` はエクスポート済みのため、实际上は必須ではないが、将来の内部変更に備えた惯例 |
| **モジュール分割** | `Shape.Layer` を独立モジュールとして、`Shape.Group` と `Shape.Path` に依存分割。これにより、シェイプ定義（Path/Group）とレイヤー管理が明確に分離 |
| **`ShapeGroup` を root とする階層** | AE ライクな「コンテンツ」は `ShapeGroup` として保持。Group は Operator チェインも持つため、将来の拡張（Trim/Repeater 等）が可能 |
| **`ShapeLayerFactory` 分離** | 静的ファクトリを別クラスにすることで、`ShapeLayer` 本体の初期化子を複雑化させず、プリミティブ生成ロジックを集約 |
| **値セマンティクスの `Point2DValue`** | トランスフォーム値を `double` ベースの独自構造体とすることで、浮動小数点誤差への配慮（`qFuzzyCompare` 付き）とアニメーション補間用 operator を提供 |
| **`ShapeTransform` (struct)** | 値として扱うことで、コピー・代入が容易。`resetTo()` メソッドで矩形バウンドへの自動整合を支援 |
| **BlendMode 列挙** | 14モードを網羅し、GPU ブレンド関数へのマッピングを想定。`ShapeLayer` が保持することで、合成時にレンダラが参照可能 |
| **`ArtifactShapeLayer` との二重構造** | `ArtifactCore::ShapeLayer` は純粋なデータモデル（PIMPL 未実装）。`Artifact::ArtifactShapeLayer` は UI 統合・プロパティシステム・信号対応のラッパーとして実装。**責務分離**: Core はロジック、Artifact はフレームワーク統合 |

---

## 4. 補足：実装状況と今後のタスク

### 現在の実装ステータス

| 项目 | 状態 |
|------|------|
| ヘッダー定義（`.ixx`） | ✅ 完了 |
| 実装ファイル（`.cppm`） | ❌ 未作成 |
| `Impl` クラス定義 | ❌ 未作成 |
| メソッド実装（ctor/dtor 含む） | ❌ 未実装 |
| ユニットテスト | ❌ 未作成 |

### 想定実装ファイル配置

```
ArtifactCore/
├── include/Shape/ShapeLayer.ixx      ← 宣言（完了）
├── src/Shape/ShapeLayer.cppm          ← 実装（未作成）
└── src/Shape/ShapeLayer/Impl.ixx      ← PIMPL 実装クラス（未作成、 optionally 分割）
```

### 重点実装項目（優先度順）

1. **Impl クラス設計** —— `ShapeGroup* rootGroup_`, `ShapeTransform transform_`, `BlendMode blendMode_`, プロパティ文字列名等を private で保持
2. **構築／破棄の実装** —— `ShapeLayer()` で `Impl` を new, `~ShapeLayer()` で delete
3. **`content()` の実装** —— `rootGroup_` へのアクセサ（`ShapeGroup` のデフォルト構築含む）
4. **`addShape()` と `clearContent()`** —— `rootGroup_->addChild()` / `clearChildren()` への委譲
5. **Transform アクセサ** —— `ShapeTransform` 値の getter/setter（`Point2DValue` 変換を含む）
6. **幾何学メソッド** —— `boundingRect()` = `rootGroup_->boundingRect()` 委譲, `contains()` は psychiatrist 変換後判定
7. **`render()` 実装** —— `QImage` 创建 → `QPainter` → `rootGroup_->toPainterPath()` 再帰描画（トランスフォーム適用）
8. **`toSvg()` 実装** —— `rootGroup` 以下のパスを SVG path 要素に再帰変換
9. **`clone()` 実装** —— `Impl` ごと deep copy（`rootGroup_->clone()` 委譲）
10. **`fromSvg()` 実装** —— 簡易 SVG パーサ（`QPainterPath::parseSvgPath()` 等を活用）から `ShapeLayer` 構築

---

## 5. ファイルパス一覧（完全リスト）

| 種別 | パス |
|------|------|
| ヘッダー（宣言） | `ArtifactCore/include/Shape/ShapeLayer.ixx` |
| 実装（未作成） | `ArtifactCore/src/Shape/ShapeLayer.cppm` |
| 依存先 1 | `ArtifactCore/include/Shape/ShapeTypes.ixx` |
| 依存先 2 | `ArtifactCore/include/Shape/ShapePath.ixx` |
| 依存先 3 | `ArtifactCore/include/Shape/ShapeGroup.ixx` |
| 間接受依存 | `ArtifactCore/include/Shape/ShapeOperator.ixx` |
| 間接受依存 | `ArtifactCore/include/Shape/TrimPaths.ixx` |
| 间接依存 | `ArtifactCore/include/Property/PropertyTypes.ixx`（`Point2DValue`） |
| 利用例 | `ArtifactCore_Library_Reference.md` |
| ラッパー実装 | `Artifact/include/Layer/ArtifactShapeLayer.ixx` & `Artifact/src/Layer/ArtifactShapeLayer.cppm` |

---

## 6. 結論

`ShapeLayer` は **設計段階でインターフェースが確定しているが、実体の実装が追いついていない** 状態。PIMPL パターンを前提としており、`.cppm` ファイルの作成が今後の最大のタスク。

`ShapeGroup` と `PathShape` などの下位シェイプ階層は既に実装済みのため、`ShapeLayer` 実装ではこれらをコンテナ・変換の中心に据えて、以下のように実装を進めるべき：

1. `Impl` に `std::unique_ptr<ShapeGroup> rootGroup_` と `ShapeTransform transform_`, `BlendMode blendMode_` を保持
2. `addShape()` は `rootGroup_->addChild()` へ委譲
3. `render()` は `QPainter` により `rootGroup_->toPainterPath()` を描画
4. `toSvg()` は再帰的に子シェイプを `<path>` 要素に変換

同時に、`ArtifactShapeLayer` 側との役割分担を明確にし、Core 層は純粋なデータ・計算ロジックに専念。UI 統合・プロパティ公開・JSON シリアライズは `ArtifactShapeLayer` 側で担当する二層構造を維持すべき。
