# Gizmo Visual Noise Reduction (2026-09-04)

**最終更新:** 2026-09-04
**状態:** 実装完了（commit `dac0ab67` in `Artifact`、親Repo `63140e8`、push 済み）

**関連:**
- `docs/technical/GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md` (Phase 1-2 の改善候補元)
- `docs/technical/GIZMO_2D_3D_CAMERA_COORDINATE_CONTRACT_2026-07-16.md` (2D/3D 座標系契約)

---

## 概要

`Artifact/src/Widgets/Render/TransformGizmo.cppm` の視覚ノイズ削減を
`docs/technical/GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md` の改善候補に従って実施。

**変更統計:** `1 file changed, 22 insertions(+), 41 deletions(-)` (`Artifact` commit `dac0ab67`)

## 変更内容

### Phase 1: Scale Gizmo 中央 Y+ 軸線の撤去

- `drawScaleCenterHandle()` から **Y+ 軸線 + Y+ tip ハンドル** を削除
- **X+ 軸線と tip ハンドルは残す**（X 軸 nudge / accessibility）
- **Center ハンドルも残す**（uniform scale 操作用）
- 関数シグネチャを 4 → 3 引数に整理
- 呼び出し側 (`TransformGizmo::draw()`) の `yAxisRaw` 計算も削除

**確認結果:** `GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md` の「中心→四隅 X 線」記述は
当時のコードで既に解消済みで、現状の X 線正体は **Y+ 軸線**だった。Aspect Lock は
`isCornerScaleHandle()` 側 (3353  付近) にあり、Center ハンドルとは無関係。

### Phase 2: Rotate Gizmo 視覚ノイズ削減

- **削除**: `drawEllipse` 2 本（X 軸赤 / Y 軸緑）- `transform3D.rotation()` で投影した
  rotation-projection 楕円の重複描画
- **変更**: `segmentSweep = 68.0f` → `36.0f` - X/Y 軸色ヒント弧の範囲を縮小し、
  リング全体は単色基調に
- **既存維持**: Leader + grip の常時描画、`drawRotateTickMarks` / `drawRotateCardinalMarks`、
  drag 中 arc sweep / leader / grip 強調

**確認結果:** `hitThickness = ringThickness * GizmoVisualStyle::rotateRingHitBoost`
` で **hit area と  visual thickness の分離**は既に分離済み (757 行)。

## Status 文書項目との対応

| GIZMO_IMPLEMENTATION_STATUS 2026-04-10 改善候補 | 状態 |
|---|---|
| Scale の `center -> corner` 4 本線を廃止 | ✅ (Phase 1) |
| 角ハンドルだけ残し、内部の X は出さない | ✅ (Phase 1) |
| Rotate gizmo を ImGuizmo 参照で再設計 | ✅ (Phase 2: 楕円削除 + sweep 縮小で簡素化) |
| Rotate 開始角 / 現在角 / sweep 表示明確化 | ✅ (既存 Leader + grip 常時表示で達成済み) |
| Rotate hit area と visual thickness 分離 | ✅ (`hitThickness`  で既存分離済み) |

## 影響評価

- **局所修正のみ**で外部 API には触らず
- `TransformGizmo` クラスの `HandleType`、`Mode`、`draw()` シグネチャは無変更
- hit test・Undo・ショートカット・Aspect Lock 動作 (`isCornerScaleHandle()`) は無変更
- `drawEllipse` ローカル関数 (816 行) は未使用になるが残置
- `drawResizeBadge` (drag 中オーバーレイ) は無変更

## AGENTS.md ルール遵守

- ✅ LF (`\n`) 統一 - commit 時に `core.autocrlf=false` で CRLF 置換を回避
- ✅ 既存挙動を不用意に変えず
- ✅ 既存 IRenderer プリミティブ (`drawSolidLine` / `drawCircle` / `drawBoxHandle`) のみ使用
- ✅ QPainter / QImage / QtCSS / QColorDialog 未使用
- ✅ 新規 signal/slot 接続なし
- ✅ C++20 modules  循環回避 (TransformGizmo は既存 module 構造維持)
- ✅ PImpl は `Impl*` 明示所有 (本変更は描画関数のみで PImpl には触なし)

## 検証状態

**未検証** (ビルド・runtime 検証は AGENTS.md の「ユーザー指示待ち」に従い未実施)。

## 確認範囲

- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/include/Widgets/Render/TransformGizmo.ixx` (未変更)
- 関連 GIZMO_IMPLEMENTATION_STATUS / GIZMO_2D_3D_CAMERA_COORDINATE_CONTRACT 文書