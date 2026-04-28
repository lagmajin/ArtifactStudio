# MILESTONE: Shape Layer Enhancement

**Date**: 2026-04-28
**Status**: In Progress
**Priority**: High
**Related**: `docs\FEATURE_DICTIONARY_2026-04-17.md`, `docs\arch\ARTIFACTCORE_MODULE_CONNECTION_MAP.md`

---

## 概要

Shape layer は基本描画と一部の on-canvas 編集は存在するが、編集体験がまだ分散しており、
active composition editor / tool options / core shape operators の接続が弱い。
このマイルストーンでは、AE/Illustrator ライクに「作成 → 形状編集 → スタイル編集 → オペレータ適用」までを
一貫して扱える shape workflow を整備する。

---

## 現状（2026-04-28）

| 項目 | 現状 |
|---|---|
| shape layer 本体 | `Artifact\include\Layer\ArtifactShapeLayer.ixx`, `Artifact\src\Layer\ArtifactShapeLayer.cppm` |
| 基本 shape type | Rect / Ellipse / Star / Polygon / Line / Triangle / Square |
| style | Fill / Stroke / StrokeCap / StrokeJoin / StrokeAlign / DashPattern |
| editable geometry | custom polygon, custom bezier path |
| on-canvas edit 実装 | `Artifact\src\Widgets\Render\ArtifactRenderLayerWidgetv2.cppm` |
| 既存 param handle | Rect の corner radius、Star の inner radius |
| 既存 path edit | bezier vertex / tangent drag, undo transaction |
| active composition editor | shape-layer 専用 live editor 統合は未整備 |
| core 連携候補 | `ShapePath`, `ShapeGroup`, `ShapeOperator`, `TrimPaths` |
| 既知の課題 | shape cache と actual draw の連携が弱い |

---

## 問題整理

1. shape-specific editing が `ArtifactLayerEditorWidgetV2` 側にあり、通常の composition editor 操作系と分離している
2. shape tool / tool options bar に shape-layer の live parameter control がほぼ無い
3. path editing は存在するが、corner/smooth 切替や open/close など実務的な編集操作が不足している
4. core の shape operator / trim path 系が app-layer workflow に十分出てきていない
5. shape cache は存在するが、描画直前に無効化される構造が残っている

---

## フェーズ

### Phase 1: Active Composition Editor Integration
**目標**: shape layer 編集を通常の composition editor から違和感なく扱えるようにする。

- [ ] shape-layer 専用 handle / overlay を active composition editor に統合
- [ ] transform gizmo と shape param handle の責務分離
- [ ] selection / hover / cursor / undo の統一
- [ ] shape layer 選択時の info / affordance 強化

### Phase 2: Shape Tool / Parameter UX
**目標**: shape 作成・基本パラメータ変更を toolbar / tool options から直接行えるようにする。

- [x] shape tool row を tool options bar に追加
- [x] shape type 切替（Rect / Ellipse / Star / Polygon / Line など）
- [x] corner radius / star points / inner radius / polygon sides の live 編集
- [ ] fill / stroke / stroke width / stroke align / dash の quick control

### Phase 2 進捗メモ（2026-04-28）
- `ArtifactToolOptionsBar` に shape row を追加し、`シェイプ` ツール選択時に live 表示されるようにした
- `ArtifactMainWindow` から選択中 `ArtifactShapeLayer` と双方向同期し、shape type / size / fill / stroke / stroke width / rect-star-polygon parameter を直接反映できるようにした
- 今回の quick control はまず `fill enabled` / `stroke enabled` / `stroke width` まで。`stroke align` / `dash` / color surface は次段で追加する

### Phase 3: Path Editing Feel
**目標**: custom polygon / bezier path 編集を実制作向けの操作感にする。

- [ ] vertex add / delete
- [ ] smooth / corner 切替
- [ ] open path / closed path 切替
- [ ] tangent handle の一貫した UX
- [ ] polygon ⇄ custom path の変換導線

### Phase 4: Shape Operators (Trim Paths / Repeater / Merge Paths / Offset / Pucker&Bloat / Twist)
**目標**: AE ライクな shape オペレータを layer workflow に接続する。

- [ ] `TrimPaths` — start/end/offset のパスアニメーション
  - layer / property surface 連携
  - 複数パス同時トリム
  - キーフレームアニメーション対応
- [ ] `Repeater` — コピー＋トランスフォームオフセット
  - copies / offset (position/scale/rotation) / opacity の property surface
  - インスタンスの合成モード指定
  - timeline / animation 対応
- [ ] `MergePaths` — パスブール演算
  - Merge / Add / Subtract / Intersect / Exclude 各モード
  - 複数 shape パスの統合処理
- [ ] `OffsetPaths` — パスの拡大・縮小
  - amount / line join / miter limit の property
- [ ] `Pucker&Bloat` — パス膨張・収縮
  - amount のアニメーション対応
- [ ] `Twist` — パスねじれ変形
  - angle / center の property
- [ ] operator stack の最小 UI（追加・削除・並び替え）
- [ ] 各オペレータのキャッシュ無効化設計

### Phase 5: Rendering / Cache / Performance
**目標**: shape layer 編集中でも軽く、再構築コストを抑える。

- [ ] shape cache と actual draw の再接続方針を整理
- [ ] dirty flag 粒度を shape parameter / path edit / style edit で分離
- [ ] renderer 向け path / triangulation cache の整理
- [ ] edit overlay と final draw の二重計算削減

---

## 最初に着手すべき実装候補

1. **shape tool options row**
   - shape type
   - corner radius / star / polygon parameter
   - fill / stroke の最小 live control
2. **active composition editor への shape overlay 移植**
   - 既存 `ArtifactLayerEditorWidgetV2` の param handle / path edit を参考にする
3. **cache 再接続の調査**
   - shape cache が draw 直前に無効化される箇所を整理する

---

## 成功条件

1. shape layer を composition editor 上で直接編集できる
2. shape type / parameter / style を tool options から即時変更できる
3. path editing に必要な頂点・接線操作が一通り揃う
4. core shape operator を app-layer から使える
5. shape layer 編集中の再描画が不必要に重くならない
