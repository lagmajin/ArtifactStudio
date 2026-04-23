# ArtifactStudio クラス辞典・依存マップ・責務表

作成日: 2026-04-17
対象: ShapePath, MaskPath, LayerMask, ShapeGroup
ソース: `ArtifactCore` / `Artifact` プロジェクト

---

## 📖 クラス辞典（Class Dictionary）

### 1. ShapePath（ArtifactCore）

**ファイルパス**: `ArtifactCore/include/Shape/ShapePath.ixx`（宣言）, `ArtifactCore/src/Shape/ShapePath.cppm`（実装）  
**モジュール**: `Shape.Path`（エクスポート）, `Shape.Path:Impl`（パーティション）  
**主な用途**: ベジェパス・vector形状の幾何学表現と操作  
**基底クラス**: なし（独立クラス）

#### 主要メソッド一覧

| メソッド | シグネチャ | 説明 | 実装状況 |
|---------|------------|------|----------|
| 構築 | `clear() → void` | 全てのコマンドを削除 | ✅ 完成 |
| 構築 | `moveTo(const QPointF&)` | ペンを移動（MoveTo） | ✅ |
| 構築 | `lineTo(const QPointF&)` | 直線を追加（LineTo） | ✅ |
| 構築 | `cubicTo(cp1, cp2, end)` | 3次ベジェ曲線追加 | ✅ |
| 構築 | `quadTo(control, end)` | 2次ベジェ曲線追加（3次に変換） | ✅ |
| 構築 | `close() → void` | パスを閉じる（Close） | ✅ |
| 構築 | `arcTo(rect, start°, sweep°)` | 楕円弧をlineTo近似で追加 | ⚠️ 1度刻み近似 |
| 図形 | `setRectangle(QRectF)` | 四角形を設定 | ✅ |
| 図形 | `setEllipse(QRectF)` | 楕円を4ベジェで近似設定 | ✅ |
| 図形 | `setPolygon(points, closed)` | 多角形を設定 | ✅ |
| 図形 | `setStar(center, points, rOut, rInn)` | 星形を設定 | ✅ |
| プロパティ | `name()/setName()` | パス名 | ✅ |
| プロパティ | `isClosed()/setClosed()` | 閉じているか | ✅ |
| プロパティ | `isEmpty() → bool` | 空パス判定 | ✅ |
| プロパティ | `commandCount() → int` | コマンド数 | ✅ |
| プロパティ | `commands() → vector<PathCommand>` | コマンド列取得 | ✅ |
| 幾何 | `boundingRect() → QRectF` | バウンディングボックス（キャッシュ付） | ✅ |
| 幾何 | `contains(QPointF) → bool` | 点の内含判定（QPainterPath経由） | ✅ |
| 幾何 | `length() → double` | パス全長（数値近似） | ✅ |
| 幾何 | `pointAtLength(t) → QPointF` | 長さtの点 | ✅ |
| 幾何 | `pointAtPercent(t) → QPointF` | 割合tの点 | ✅ |
| 幾何 | `toSegments() → vector<BezierSegment>` | ベジェセグメント列に変換 | ✅ |
| 変換 | `translate(offset)` | 平行移動 | ✅ |
| 変換 | `scale(center, sx, sy)` | スケール | ✅ |
| 変換 | `rotate(center, angle)` | 回転 | ✅ |
| 変換 | `transform(matrix)` | 任意行列 | ✅ |
| Qt | `toPainterPath() → QPainterPath` | Qtパスに変換 | ✅ |
| Qt | `fromPainterPath(path) → ShapePath` | Qtパスから構築（静的） | ✅ |
| 操作 | `clone() → ShapePath` | ディープコピー | ✅ |
| 操作 | `reverse() → void` | パスを反転 | ✅ |
| 操作 | `addPath(other)` | 他のパスを連結 | ✅ |
| 操作 | `simplify() → void` | QPainterPath::simplified で単純化 | ✅ |

#### プライベートメソッド（実装内部用）

| メソッド | 説明 |
|---------|------|
| `Impl::computeBounds()` | キャッシュされたバウンディングボックスを再計算 |
| `Impl::toPainterPath()` | コマンド列をQPainterPathに変換 |
| `Impl::fromPainterPath()` | QPainterPathからコマンド列を構築 |
| `Impl::transform(matrix)` | 全コマンドの座標を行列変換 |
| `getStartPoint/EndPoint(cmd)` | コマンドの開始/終了点を取得 |
| `cubic/quadApproxLength()` | ベジェ曲線の長さを数値積分で近似 |
| `cubic/quadPointAtLength()` | 指定長さの点を二分探索で求める |

---

### 2. MaskPath（Artifact）

**ファイルパス**: `Artifact/include/Mask/MaskPath.ixx`（宣言）, `Artifact/src/Mask/MaskPath.cppm`（実装）  
**モジュール**: `Artifact.Mask.Path`  
**主な用途**: マスクの形状＋属性（feather, opacity 等）を保持  
**基底クラス**: なし（Pimplパターン）

#### 主要型

| 型 | 位置 | 説明 |
|----|------|------|
| `MaskVertex` | `MaskPath.ixx:45` | ベジェ制御点付き頂点 |
| `MaskMode` | `MaskPath.ixx:52` | 列挙: Add, Subtract, Intersect, Difference |

#### 主要メソッド一覧

| メソッド | シグネチャ | 説明 | 実装状況 |
|---------|------------|------|----------|
| 構築 | `MaskPath()` / `~MaskPath()` | コンストラクタ | ✅ |
| 構築 | `addVertex(MaskVertex)` | 頂点追加 | ✅ |
| 構築 | `insertVertex(idx, v)` | 頂点挿入 | ✅ |
| 構築 | `removeVertex(idx)` | 頂点削除 | ✅ |
| 構築 | `setVertex(idx, v)` | 頂点変更 | ✅ |
| 構築 | `clearVertices()` | 全頂点削除 | ✅ |
| プロパティ | `vertex(idx) → MaskVertex` | 頂点取得 | ✅ |
| プロパティ | `vertexCount() → int` | 頂点数 | ✅ |
| プロパティ | `isClosed()/setClosed()` | 閉じているか | ✅ |
| プロパティ | `opacity()/setOpacity()` | 不透明度（0-1） | ✅ |
| プロパティ | `feather()/setFeather()` |  feather量 | ✅ |
| プロパティ | `expansion()/setExpansion()` | 拡大/縮小（morphological） | ✅ |
| プロパティ | `isInverted()/setInverted()` | 反転マスク | ✅ |
| プロパティ | `mode()/setMode()` | マスク合成モード | ✅ |
| プロパティ | `name()/setName()` | マスク名 | ✅ |
| 描画 | `rasterizeToAlpha(w, h, outMat)` | cv::Mat ラスタライズ | ✅ |

#### 内部実装（private）

| メソッド | 説明 |
|---------|------|
| `Impl::toPolygon(subdiv=16)` | ベジェ曲線をポリゴンに離散化（OpenCV用） |

---

### 3. LayerMask（Artifact）

**ファイルパス**: `Artifact/include/Mask/LayerMask.ixx`, `Artifact/src/Mask/LayerMask.cppm`  
**モジュール**: `Artifact.Mask.LayerMask`  
**主な用途**: レイヤーに属する MaskPath のコレクション管理  
**基底クラス**: なし

#### 主要メソッド

| メソッド | 説明 |
|---------|------|
| `addMask(path)` | マスク追加 |
| `removeMask(idx)` | マスク削除 |
| `mask(idx) → MaskPath` | マスク取得 |
| `maskCount() → int` | マスク数 |
| `clearMasks()` | 全削除 |

---

### 4. ShapeGroup（ArtifactCore）

**ファイルパス**: `ArtifactCore/include/Shape/ShapeGroup.ixx`（宣言）, `ArtifactCore/src/Shape/ShapeGroup.cppm`（実装）  
**モジュール**: `Shape.Group`（エクスポート）, `Shape.Group:Impl`（パーティション）  
**主な用途**: 複数 ShapeElement をグループ化し、演算子適用  
**基底クラス**: `ShapeElement`（抽象）

#### 主要クラス階層

```
ShapeElement（抽象）
├── ShapeGroup（グループ）
│   ├── 子要素: vector<unique_ptr<ShapeElement>>
│   ├── 演算子: vector<unique_ptr<ShapeOperator>>
│   └── processedPaths() → vector<ShapePath>
└── PathShape（具体）
    ├── RectanglePathShape
    ├── EllipsePathShape
    ├── PolygonPathShape
    └── StarPathShape
```

#### 主要メソッド

| クラス | メソッド | 説明 |
|--------|---------|------|
| ShapeElement | `type() → ShapeType` | 種別取得（純粋仮想） |
| ShapeElement | `name()/setName()` | 名前 |
| ShapeElement | `isVisible()/setVisible()` | 表示 |
| ShapeElement | `isLocked()/setLocked()` | ロック |
| ShapeElement | `boundingRect() → QRectF` | バウンディングボックス（純粋仮想） |
| ShapeElement | `transform() → ShapeTransform&` | トランスフォーム参照 |
| ShapeElement | `clone() → unique_ptr<ShapeElement>` | クローン（純粋仮想） |
| ShapeGroup | `addChild(child)` | 子要素追加 |
| ShapeGroup | `insertChild(idx, child)` | 子要素挿入 |
| ShapeGroup | `removeChild(child|idx)` | 削除 |
| ShapeGroup | `childCount() → int` | 子数 |
| ShapeGroup | `childAt(idx) → ShapeElement*` | 子取得 |
| ShapeGroup | `children() → vector<ShapeElement*>` | 全子取得 |
| ShapeGroup | `indexOf(child) → int` | インデックス検索 |
| ShapeGroup | `translate(offset)` | 全子と自己を移動 |
| ShapeGroup | `scale(center, sx, sy)` | 全子と自己をスケール |
| ShapeGroup | `rotate(center, angle)` | 全子と自己を回転 |
| ShapeGroup | `clone() → unique_ptr<ShapeElement>` | グループ全体のクローン |
| ShapeGroup | `addOperator(op)` | 演算子追加 |
| ShapeGroup | `operatorAt(idx) → ShapeOperator*` | 演算子取得 |
| ShapeGroup | `operators() → vector<...>` | 全演算子取得 |
| ShapeGroup | `processedPaths() → vector<ShapePath>` | 演算子適用後のパスリスト |
| PathShape | `path()/setPath()` | 保持するShapePath |
| PathShape | `stroke()/setStroke()` | ストローク設定 |
| PathShape | `fill()/setFill()` | フィル設定 |
| PathShape | `boundingRect() → QRectF` | パスのバウンディングボックス |
| PathShape | `toPainterPath() → QPainterPath` | Qtパスへ変換（トランスフォーム適用済） |
| PathShape | `clone() → unique_ptr<ShapeElement>` | クローン |
| RectanglePathShape | `rect()/setRect()` | 矩形 |
| RectanglePathShape | `cornerRadius()/setCornerRadius()` | 角丸半径 |
| EllipsePathShape | `rect()/setRect()` | 楕円の境界矩形 |
| PolygonPathShape | `points()/setPoints()` | 頂点リスト |
| PolygonPathShape | `pointAt(idx) → QPointF` | 頂点取得 |
| PolygonPathShape | `setClosed(bool)` | 閉じる／開く |
| StarPathShape | `center()/setCenter()` | 中心 |
| StarPathShape | `points()/setPoints()` | 峰の数 |
| StarPathShape | `outerRadius()/setOuterRadius()` | 外半径 |
| StarPathShape | `innerRadius()/setInnerRadius()` | 内半径 |

---

### 5. ShapeOperator（ArtifactCore）

**ファイルパス**: `ArtifactCore/include/Shape/ShapeOperator.ixx`（宣言のみ, 実装未完）  
**モジュール**: `Shape.Operator`  
**主な用途**: ShapeGroup に適用するパス演算子（結合、除外、Trim、Round 等）  
**状態**: ⚠️ 実装未完（インターフェースのみ）

#### 主要クラス

| クラス | 説明 |
|--------|------|
| `ShapeOperator`（抽象） | `process(vector<ShapePath>) → vector<ShapePath>` を定義 |
| `CombinePaths` | 複数パスの結合 |
| `SubtractPaths` | パス減算 |
| `IntersectPaths` | パス交差 |
| `DifferencePaths` | パス差集合 |
| `TrimPaths` | パストリミング（開始/終点で切り取り） |
| `OffsetPaths` | パスオフセット |
| `RoundPaths` | コーナー丸め |

---

## 🗺️ 依存関係マップ（Dependency Map）

```
[ArtifactCore]                                         [Artifact]
─────────────────────────────────────────────────────────────────────
Shape.Path (ixx + :Impl)  ────────┐
    │                               │
    │ export                        │ import
    ▼                               ▼
Shape.Group (ixx + :Impl)         Artifact.Mask.Path (ixx + cppm)
    │                               │
    │ import                        │
    ▼                               ▼
Shape.Layer (ixx)                 Artifact.Mask.LayerMask (ixx + cppm)
    │                               │
    └───────────────┬───────────────┘
                    │ import
                    ▼
        ArtifactCompositionRenderController (Artifact)
                    │
                    ├── import Artifact.Layer.Layer
                    ├── import Render.ArtifactIRenderer
                    └── import Widgets.Render.ArtifactRenderManagerWidget
                    ...
```

### 詳細依存リスト

#### ShapePath の依存
| 依存先 | 種類 | 方向 |
|--------|------|------|
| `Shape.Types` | import（モジュール） | → Shape.Types |
| `Core.Math` | import | → Core.Math |
| Qt (QPointF, QRectF, QPainterPath) | ヘッダーインクルード | → Qt6 |

#### MaskPath の依存
| 依存先 | 種類 | 方向 |
|--------|------|------|
| `Utils.String.UniString` | import | → Utils.String |
| Qt (QPointF, QPolygonF) | ヘッダーインクルード | → Qt6 |
| OpenCV (`opencv2/opencv.hpp`) | ヘッダーインクルード | → OpenCV |

#### LayerMask の依存
| 依存先 | 種類 | 方向 |
|--------|------|------|
| `Artifact.Mask.Path` | import | → MaskPath |
| `Utils.String.UniString` | import | → Utils.String |

#### ShapeGroup の依存
| 依存先 | 種類 | 方向 |
|--------|------|------|
| `Shape.Types` | import | → Shape.Types |
| `Shape.Path` | import | → Shape.Path |
| `Shape.Operator` | import | → Shape.Operator |

---

## 📋 責務表（Scope of Responsibility）

### ShapePath の責務

#### ✅ やること（Scope: IN）

| 項目 | 説明 |
|------|------|
| **幾何学モデリング** | ベジェ曲線・多角形・星形・円・矩形の形状データの保持 |
| **座標変換** | 平行移動・回転・スケール・行列変換の内部座標更新 |
| **ジオメトリ計算** | バウンディングボックス、内含判定、パス長、点位置の算出 |
| **フォーマット変換** | `toPainterPath()` / `fromPainterPath()` によるQt形式との相互変換 |
| **操作** | クローン、反転、パス連結、単純化（ Douglas-Peucker ではないが、Qtのsimplified利用） |
| **プリミティブ生成** | 矩形、楕円、多角形、星形をコマンド列として生成 |
| **シリアライズ準備** | コマンド列を `std::vector<PathCommand>` で保持（後でJSON等へ変換可能） |

#### ❌ やらないこと（Scope: OUT）

| 項目 | 説明 |
|------|------|
| **ラスタライズ** | ピクセルイメージへの変換は行わない（MaskPath 担当） |
| **レンダリング** | 描画はQPainterやGPUパイプラインに委譲 |
| **ストローク/フィル** | 線種・塗りつぶし属性は持たない（PathShape が担当） |
| **アニメーション** | キーフレームやタイムライン連携は別レイヤー |
| **UI編集** | ハンドル操作、グリッドスナップ等はウィジェット層 |
| **GPU処理** | コンピュートシェーダ等でのラスタライズはレンダラ側 |

#### 設計理由
- ShapePath は「几何データのみ」の純粋クラスとして、**関心の分離**を実現。
- レンダリングや属性は `PathShape`、`ShapeLayer`、`ArtifactIRenderer` 等に委譲。
- `MaskPath` が「ラスタライズ可能属性付きパス」として別役割を担うことで、両者の混同を防止。

---

### MaskPath の責務

#### ✅ やること（Scope: IN）

| 項目 | 説明 |
|------|------|
| **マスク形状保持** | ベジェ制御点付き頂点列（`MaskVertex`）を保持 |
| **ラスタライズ** | `rasterizeToAlpha(w, h, outMat)` でアルファマスク画像を生成（OpenCV使用） |
| **マスク属性管理** | 不透明度、フェザー、拡大縮小、反転、合成モード（Add/Subtract/Intersect/Difference） |
| **ポリゴン化** | `toPolygon(subdivisions)` でベジェ曲線を直線近似しOpenCVに渡す |
| **シリアライズ準備** | 頂点リストと属性の保存 |

#### ❌ やらないこと（Scope: OUT）

| 項目 | 説明 |
|------|------|
| **レンダリング本体** | `rasterizeToAlpha` はアルファプレーン生成のみ、色合成は別 |
| **モーションブラー** | 時間的ブラーはレンダリングパイプライン任せ |
| **3D変形** | カメラ／透視変換は GPU ラスタライザ側で適用 |
| **ストローク** | マスクは領域のみ、線の太さは概念なし |
| **エフェクト連携** | エフェクトはマスク結果を入力として消費する |

#### 注意点
- `rasterizeToAlpha()` 内で `cv::fillPoly` に整数座標を渡すため、**サブピクセル精度がLoss**（`std::round()` で丸められている）。
- 高精度ラスタライズが必要な場合、`toPolygon()` の `subdivisions` 引数を増やすか、float座標維持型のOpenCV呼び出しに変更する必要がある。

---

### ShapeGroup の責務

#### ✅ やること（Scope: IN）

| 項目 | 説明 |
|------|------|
| **子要素管理** | `ShapeElement` の追加・削除・取得・巡回 |
| **グループ変換** | 全子要素への translate/scale/rotate 伝播 |
| **バウンディングボックス統合** | 子全のバウンディングボックスを统合 |
| **演算子適用** | 保持する `ShapeOperator` を子の `PathShape` パスに適用し `processedPaths()` で結果を返す |
| **クローン** | グループ全体のディープコピー生成 |

#### ❌ やらないこと（Scope: OUT）

| 項目 | 説明 |
|------|------|
| **パス形状の保持** | 自身はパスを持たず、子の `PathShape` から取得する |
| **描画** | 描画責任はレンダラ側 |
| **アニメーション** | タイムライン連携は別レイヤー |
| **ストローク/フィル** | 属性は `PathShape` が保持、グループは集約のみ |

---

### ShapeOperator の責務

#### ✅ やること（Scope: IN）

| 項目 | 説明 |
|------|------|
| **パス加工** | `process(vector<ShapePath>) → vector<ShapePath>` でパス列を変換 |
| **非破壊編集** | 元パスを変更せず新しいパスリストを返す |

#### ❌ やらないこと（Scope: OUT）

| 項目 | 説明 |
|------|------|
| **状態保持** | 演算子自体はステートレス（設定はパラメータで渡す） |
| **描画** | あくまでジオメトリ変換のみ |

#### 実装済み演算子（予定）
- Combine / Subtract / Intersect / Difference（ブール演算、未実装）
- TrimPaths（trim start/end、未実装）
- OffsetPaths（オフセット、未実装）
- RoundPaths（丸め、未実装）

---

## 🎯 設計上の分離基準

| クラス | 担当領域 | 委譲先 |
|--------|----------|--------|
| **ShapePath** | 幾何・座標・ジオメトリ計算 | - |
| **MaskPath** | ジオメトリ＋ラスタライズ属性 | OpenCV へラスタライズ実行 |
| **PathShape** | ShapePath＋ストローク/フィル | 描画时に QPainter / Renderer へ |
| **ShapeGroup** | 子要素管理＋演算子適用 | 各 `ShapeOperator` へ加工依頼 |
| **ShapeOperator** | パス列の非破壊変換 | 返されたパスを `ShapeGroup` が受理 |

---

## 🔄 データフロー

```
ユーザー編集
    ↓
ShapePath（コマンド列を操作）
    ↓
PathShape（_shape属性付与）
    ↓
ShapeGroup（子要素をまとめ、演算子適用）
    ↓
ShapeLayer（タイムライン・キーフレーム連携）
    ↓
ArtifactCompositionRenderController
    ↓
ArtifactIRenderer（DiligentEngine 経由 GPU レンダリング）
    ↓
 Composited Image
```

---

**更新日**: 2026-04-17  
**担当**: Kilo（AI Agent）  
**次回更新予定**: ShapeOperator 実装完了後、エフェクトシステムとの連携を追記
