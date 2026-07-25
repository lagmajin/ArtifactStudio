# M-FIGMA-3 Boolean Path Operations + Advanced Fill/Stroke Milestone

作成日: 2026-07-07
ステータス: Draft
対象: `ArtifactCore/include/Shape/ShapeGroup.ixx`,
      `ArtifactCore/src/Shape/ShapeGroup.cppm`,
      `ArtifactCore/include/Shape/ShapeOperator.ixx`,
      `ArtifactCore/src/Shape/MergePaths.cppm` (新規),
      `Artifact/include/Layer/ArtifactShapeLayer.ixx`,
      `Artifact/src/Layer/ArtifactShapeLayer.cppm`
位置づけ: Figma の Boolean Operations（Union/Subtract/Intersect/Exclude）と
          Fill/Stroke の多段レイヤリングを ArtifactStudio に導入。
          既存の `ShapeOperator` インターフェースを活用し、
          `MergePaths` の未実装を完了させる。
参照:
- `ArtifactCore/include/Shape/ShapeGroup.ixx`（addOperator, ShapeOperator）
- `ArtifactCore/include/Shape/ShapeOperator.ixx`（TrimPaths, Repeater, OffsetPaths 等）
- `docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md` (M-LE-3)
- `docs/done/COMPOSITION_EDITOR_GAP_ANALYSIS_2026-06-03.md`（ShapeOperator 6種未実装）
- `docs/planned/MILESTONE_BEYOND_AE_DIFFERENTIATION_2026-06-02.md` (Z-D1: Boolean)

---

## 1. 目的

Figma の 2D 編集における決定的な差別化要素 2 つを実装する。

### Boolean Path Operations
複数のシェイプを組み合わせて新しい形状を作る:
- **Union（和）**: 2つのシェイプを結合
- **Subtract（差）**: 前面シェイプで背面を切り抜く
- **Intersect（積）**: 重なった部分だけ残す
- **Exclude（排他的和）**: 重なった部分だけ取り除く

現在の `ShapeOperator` インターフェースに `MergePaths` が定義されているが、実装が存在しない。

### Advanced Fill/Stroke Layering
Figma では 1 つのシェイプに複数の Fill と複数の Stroke を重ねられる:
- Fill: Solid, Gradient, Image, それぞれ opacity + blend mode
- Stroke: Inside/Center/Outside alignment, dash, cap/join style
- 複数 Fill/Stroke の重ね順編集

現在の `PathShape` は fill/stroke が各 1 つずつの単純モデル。

> 重要: これは既存の `ShapeOperator` / `MergePaths` の未実装を完了させる形で進める。
> ゼロから作るのではなく、**定義済みで実装待ちの資産を仕上げる** milestone。

---

## 2. 現状整理

### 2.1 既存資産

| 資産 | 状態 |
|---|---|
| `ShapeOperator` 基底 | インターフェース完備。`process(paths) -> vector<ShapePath>` |
| `MergePaths` 宣言 | `ShapeOperatorType::MergePaths` 定義済み |
| `TrimPaths`, `Repeater`, `OffsetPaths`, `PuckerBloat`, `Twist` | 実装済み |
| `PathShape::fill()` / `PathShape::stroke()` | 1 fill + 1 stroke のみ |
| `FillSettings` | Solid color + opacity |
| `StrokeSettings` | width, cap, join, dash, alignment |

### 2.2 不足

| 軸 | 状況 |
|---|---|
| `MergePaths` 実装 | インターフェースのみ。実装ゼロ（GAP_ANALYSIS で確認済み） |
| Boolean path 演算（Union/Subtract/Intersect/Exclude） | 0 hit |
| 複数 Fill / 複数 Stroke | なし（1 fill + 1 stroke のみ） |
| Fill の種類（Gradient, Image fill） | なし |
| Fill/Stroke の重ね順編集 UI | なし |
| Individual corner radius（角丸の個別設定） | なし（全角同一） |
| Smooth corners（Figma の角丸スムージング） | なし |

### 2.3 コード検索
- `MergePaths` → `ShapeOperatorType` enum に定義あり。実装なし
- `booleanOp` / `BooleanOp` → 0 hit
- `multiple.*fill` / `fillStack` → 0 hit


## 3. Scope / Non-Goals

### Scope
- `MergePaths` 実装（Union, Subtract, Intersect, Exclude）
- 複数 Fill レイヤー（Solid/Gradient, color, opacity, blendMode）
- 複数 Stroke レイヤー（width, alignment, dash, color, opacity）
- Fill/Stroke 重ね順編集（Inspector リスト）
- Individual corner radius（四隅個別）

### Non-Goals
- Figma の完全な vector network editing → 将来
- Gradient/Image fill フルエディタ → 別 milestone
- Smooth corners → 将来

---

## 4. Phases

### Phase 1: MergePaths Boolean Operations (P0, 2 セッション)
- `MergePaths.cppm` 新規実装
- `BooleanOp` enum: Union/Subtract/Intersect/Exclude
- `QPainterPath` の boolean 演算を利用
- `ShapeGroup` operator stack に統合

**Done criteria:** 2 矩形に Union→結合 / Subtract→切り抜き / 全4演算動作

### Phase 2: Multi Fill/Stroke レイヤー (P0, 2 セッション)
- `PathShape` の fill を `vector<FillLayer>` に、stroke を `vector<StrokeLayer>` に拡張
- 複数 Fill の順次描画 / 複数 Stroke の center→inside→outside 優先描画
- 既存単一 fill/stroke API を互換ラッパーとして維持

**Done criteria:** solid + gradient の2段 fill / inside + outside の2段 stroke / 既存コード非破壊

### Phase 3: Individual Corner Radius (P1, 1 セッション)
- `RectanglePathShape` の四隅個別角丸: topLeft/topRight/bottomRight/bottomLeft

### Phase 4: Inspector Fill/Stroke リスト UI (P1, 2 セッション)
- Fill/Stroke リスト表示 + ドラッグ並び替え + 追加/削除/複製
- 各レイヤーのプロパティ編集

### Phase 5: 永続化 (P2, 1 セッション)
- project JSON に `fills[]`/`strokes[]`/`booleanOps[]` 追加 / 旧プロジェクト互換

---

## 5. Done Criteria (全体)
- MergePaths 4演算 / 複数 Fill/Stroke 多段重ね + 順序編集 / 四隅個別角丸
- Inspector リスト UI / 保存復元 / 新規 signal-slot なし

---

## 6. 更新履歴
- 2026-07-07: 初版作成。既存 MergePaths 未実装完了 + Figma Fill/Stroke 多段レイヤリング移植設計。


---

## Static audit follow-up (2026-07-25)

ShapeOperatorType::MergePaths と MergePaths 宣言、ShapeLayer 側の operator factory は存在するが、MergePaths の boolean 演算実装（Union／Subtract／Intersect／Exclude）は確認できない。

一方、ShapeLayer には単一 fill／stroke の gradient、stroke alignment／cap／join／taper、stroke gradient と persistence／Inspector property の基盤がある。複数 fill／stroke stack、重ね順 UI、individual corner radius、boolean の永続化は未確認である。Phase 1 は未完了、Phase 2 は単一レイヤー拡張まで部分実装、Phase 3〜5 は未完了または未検証として記録する。
