# M-FIGMA-1 Auto Layout + Constraints System Milestone

作成日: 2026-07-07
ステータス: Draft
対象: `ArtifactCore/include/Shape/ShapeGroup.ixx`,
      `ArtifactCore/src/Shape/ShapeGroup.cppm`,
      `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Layer/ArtifactShapeLayer.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Widgets/Inspector/ArtifactInspectorWidget.cppm`
位置づけ: Figma の Auto Layout + Constraints を ArtifactStudio に移植。テキスト変更時の自動リサイズ、
          レスポンシブなレイアウト、制約ベースのアンカーを実現。
          既存の `ShapeGroup` 階層 + `ShapeTransform` に上積みする。
参照:
- `docs/planned/MILESTONE_BEYOND_AE_DIFFERENTIATION_2026-06-02.md` (Z-C3)
- `ae_maturity_additional_analysis.md` #44 (Responsive Layout)
- `ArtifactCore/include/Shape/ShapeGroup.ixx`
- `docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md`

---

## 1. 目的

Figma の最も差別化された 2D 編集機能「Auto Layout」と「Constraints」を導入する。

### Auto Layout（Figma コア）
子要素のサイズ変更に応じて親コンテナが自動リサイズ。
テキストが増えてもボタンが伸びる、カードがスタックする。
AE には完全に存在しない機能。

### Constraints（Figma アンカー制約）
親コンテナのリサイズ時に子要素の位置/サイズを
「左端固定」「右端固定」「中央」「スケール」で制御。

> 重要: 現在の ArtifactStudio にはレイアウト自動調整機能が一切ない。
> コンポジションサイズを変更しても、すべてのレイヤーは絶対座標のまま。


---

## 2. 現状整理 (2026-07-07 基準)

### 2.1 既存資産

| 資産 | ファイル | 内容 |
|---|---|---|
| `ShapeGroup` | `ShapeGroup.ixx` | 子要素管理（addChild/removeChild）、transform、operator stack |
| `ShapeTransform` | `Shape.Types` | position, rotation, scale の2D変換 |
| `RectanglePathShape` | `ShapeGroup.ixx:207` | cornerRadius プロパティ完備 |
| `ArtifactShapeLayer` | `ArtifactShapeLayer.cppm` | customPolygonPoints, operator stack |
| `ResponsiveLayout` 構想 | `ae_maturity_additional_analysis.md` #44 | LayoutVariant データ構造案 |

### 2.2 不足

| 軸 | 状況 |
|---|---|
| Auto Layout データモデル | なし |
| Padding / Gap / Alignment | なし |
| SizeMode（Fixed/Fill/Hug） | なし |
| Constraints（Left/Right/Top/Bottom/Center/Scale） | なし |
| 親リサイズ時の子要素追従 | なし |
| Auto Layout ビューポート UI | なし |

### 2.3 コード検索: AutoLayout → 0 hit, LayoutConstraint → 0 hit, HugContents → 0 hit

---

## 3. Scope / Non-Goals

### Scope
- `ShapeAutoLayout`: 水平/垂直 Auto Layout、Padding/Gap/Alignment、SizeMode（Fixed/Fill/Hug）
- `ShapeConstraints`: 親リサイズ時の子要素位置/サイズ追従
- ビューポート UI（パディング境界、Gapライン、アンカー表示）
- Inspector 設定パネル / 永続化

### Non-Goals
- Layer 間 Auto Layout → Phase 2 以降 / Figma API 連携 → Z-C1
- コンポーネント/インスタンス → Z-C2 / ラップレイアウト → 将来

---

## 4. Phases

### Phase 1: ShapeAutoLayout コア (P0, 2 セッション)
- `ShapeAutoLayout.ixx` / `.cppm` 新規追加
- `LayoutDirection` (Horizontal/Vertical), `SizeMode` (Fixed/Fill/Hug), `Alignment` (Start/Center/End/SpaceBetween)
- `layout()` メソッド: 子要素再配置 + 親サイズ更新
- `ShapeGroup` に `autoLayout()` アクセサ追加

**Done criteria:** 子要素が direction に従って整列 / Padding+Gap 正しい / HugContents 動作 / 単体テスト

### Phase 2: Constraints (P0, 1 セッション)
- `ShapeConstraints` クラス: Left/Right/Top/Bottom/Center/Scale
- 親リサイズ時に constraint に従って子要素の位置/サイズ再計算

**Done criteria:** Left+Right で幅追従 / Center で中央維持 / Scale で等比拡縮

### Phase 3: ビューポート UI (P0, 2 セッション)
- パディング境界の点線表示 + Gap ライン + ドラッグハンドル
- Constraints のアンカーライン表示
- `CompositionRenderOverlay` に統合

### Phase 4: Inspector + 永続化 (P1, 2 セッション)
- Inspector に Auto Layout セクション + Constraints 設定
- project JSON 保存復元 / 旧プロジェクト互換

---

## 5. Done Criteria (全体)
- Auto Layout で子要素自動整列 + Constraints で親リサイズ追従
- ビューポートでパディング/Gap/アンカー表示
- Inspector から全パラメータ編集 / 保存復元
- 新規 signal-slot / QImage / setStyleSheet なし

---

## 6. 更新履歴
- 2026-07-07: 初版作成。Figma Auto Layout + Constraints 移植設計。

> Auto Layout は **ShapeGroup 内の子要素間** から始め、Layer 間へ拡張する。


---

## Static audit follow-up (2026-07-25)

文書の「レイアウト自動調整機能が一切ない」「AutoLayout 0 hit」は現行ソースと一致しない。ArtifactAbstractLayer に component.layout.*（enabled／mode／alignment／direction／gap）と親レイヤー由来の offset 計算があり、Composition render controller に Auto Layout の並べ替え・order 更新、responsive layout／overlay の参照も確認できる。

一方、ShapeGroup 所有の ShapeAutoLayout／ShapeConstraints データモデル、Fixed／Fill／Hug の完全契約、padding／gap／anchor の専用 Inspector／永続化、viewport の専用ガイド表示は確認できない。よって既存の Layer component layout は部分的な前身として扱い、Phase 1〜2 は未完了、Phase 3〜4 は未着手または未検証として記録する。
