# Bug Analysis: 3Dギズモとハンドル（当たり判定）が一致しない問題 (2026-07-22)

> 状態: 分析完了（修正・ビルド・実機確認は未実施）
> 対象症状: 3Dギズモの見た目のハンドル位置と、ホバー／クリック判定の位置が一致しない
> 関連: `docs/technical/GIZMO_2D_3D_CAMERA_COORDINATE_CONTRACT_2026-07-16.md`、`docs/bugs/GIZMO3D_COUNTERMEASURES_2026-03-26.md`

## 1. 問題の構造

契約（GIZMO_2D_3D_CAMERA_COORDINATE_CONTRACT）の不変条件:
**① 描画 ② ピッキングレイ生成 ③ ヒット判定/ドラッグ は全て同一の view/proj ペアを使うこと。**
どれか1つでも別座標系になると「見えているハンドルと当たり判定が一致しない」が発生する。

## 2. 直近の修正で解消済み（2026-07-21）

- `7ca8bba6 fix_align_3d_scale_gizmo_y_axis`:
  Scale モードで描画は Y 下向き（`scaleAxisDirectionFor`）、hitTest は Y 上向き
  （`axisDirectionFor`）を使用 → **Y ハンドルの判定が描画位置と逆側**にあった。
  hitTest 側を `scaleAxisDirectionFor` / `scaleAxisHandleEndFor` に統一し解消。
- `b693bc64 fix_match_3d_scale_handle_hit_positions`:
  描画の cube チップが `s*0.92` 固定（Z は `s*1.0`）なのに hitTest は `s*1.16/1.12`
  → **チップをクリックしても判定線分の外**だった。描画側も
  `scaleAxisHandleEndFor`（`kScaleHandleLength = 0.92f`）に統一し解消。

## 3. 現行コードに残っている不一致ポイント（優先度順）

### A. Scale 中央ユニフォームハンドル: 描画と判定が約9倍違う

- 描画: 白リング半径 `s*0.60`（+薄いリング `s*0.18`）
  — `Artifact3DGizmo.cppm:838-839`
- 判定: 中心点からの球 `axisHandleTipRadius(s)*1.25 ≈ s*0.069` — `:343-346`
- **見えているリングを掴んでも反応しない**。判定は中心の微小球のみ。

### B. Rotate 自由回転リング（外側グレー）: 描画されるが永久に掴めない

- 描画: `s*1.18` の自由回転トーラス — `:802-803`
- hitTest の Rotate 分岐: X/Y/Z の `checkRing` のみで `GizmoAxis::Screen` の
  ケースなし — `:348-365`。リング帯 `|dist - 1.0s| < 0.12s` でも 1.18s に届かない
- **装飾だけのデッドハンドル**。

### C. カメラ無しフォールバック時: picking ray が「最後に設定された行列」に依存

- 3Dカメラ無し（`gizmo3DCameraMatricesValid_ == false`）の場合、
  描画側はローカルに新規構築した pan/zoom + `ortho(0, hostW, hostH)` で描く
  — `ArtifactCompositionRenderController.cppm:24259-24269`
- `createPickingRay` のフォールバックは `renderer_->getViewMatrix()` を読む
  — `:17140-17146`。実体は `PrimitiveRenderer2D::externalViewMatrix_`
  （**最後に `setViewMatrix()` した誰かの行列**）
- ギズモ描画（`Artifact3DGizmo.cppm:581-582`）が設定するので直後は一致するが、
  同フレーム内で後続の描画が `setViewMatrix` を呼ぶと、次のマウスイベントで
  **別座標系のレイ**が生成される → ハンドルがずれる。
  pan/zoom 直後やフォールバック経路で再現しやすい。
- 正攻法: フォールバック時も描画に使ったペアを `gizmo3DViewMatrix_` /
  `gizmo3DProjectionMatrix_` にキャッシュし、renderer 状態の読み戻しに依存しない。

### D. `currentScale` は `draw()` の副産物 → 1フレーム遅れ

- `hitTest` のスケール・閾値は全て `impl_->currentScale` 依存だが、
  これは `draw()` 内でのみ更新される — `Artifact3DGizmo.cppm:575`。
  カメラ移動直後の hover は**前フレームのスケール**で判定される。

### E. 描画サイトが2箇所ある

- `ArtifactCompositionRenderController.cppm:24229`（`selectedLayer->is3D()` のみ）と
  `:27743`（`is3D() || viewportOrientationActive_`）。
  両方が `currentScale` を更新し、**最後に走った側のカメラでヒット判定のスケールが
  決まる**。片方が3Dカメラ・片方が2Dフォールバックで描いた場合、
  見えているギズモと判定スケールが別カメラ由来になりうる。

### F. viewport-orientation モードの2Dレイヤー: 描画されるが判定されない

- 描画サイトB は `viewportOrientationActive_` でも描くが、press/hover の
  hitTest は `selectedLayer->is3D()` ガード（`:17910`、`:19149`）で弾かれる
  → **表示だけ出て操作できない**。

### G. 小さいもの

- Rotate リングのヒット帯 ±0.12s は描画チューブ ±0.034s の3倍以上太い
  （寛容だが「リングの外をクリックしても掴める」）— `Artifact3DGizmo.cppm:272,353`
- Move Y は上向き・Scale Y は下向きとモードで軸が反転する設計
  （コメント上は意図的だが視覚的に紛らわしい）— `:93-120`
- DPR まわりは問題なし（`hostWidth_` は物理px、マウス座標も DPR 乗算済みで一致）
  — `:12381-12389, :17190`
- `:27774-27781` の `viewportOrientationMatricesValid_` / `has3DCamera` 分岐は
  `!gizmo3DCameraMatricesValid_` 内では到達不能なデッドコード（無害だが紛らわしい）。

## 4. 切り分けの視点

| 再現条件 | 疑うべき箇所 |
|---|---|
| 3Dカメラありで一致しない | A / B / D / E（ギズモ内部の描画 vs 判定の不一致） |
| カメラ無し・pan/zoom 直後に一致しない | C（picking ray の行列読み戻し問題）が最有力 |
| リング/中央ハンドルだけ反応しない | A / B |
| カメラ操作中だけ一瞬ずれる | D |

## 5. 関連ファイル

- `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`
  （draw :566 / hitTest :270 / scaleAxisHandleEndFor :227 / currentScale 更新 :575）
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  （createPickingRay :17130 / press hitTest :17910 / hover hitTest :19217 /
  描画サイト :24229, :27743 / カメラペアキャッシュ :21724）
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`（externalViewMatrix_ :217）

## 6. 未確認事項

- ビルド・実機確認は未実施（AGENTS.md ルールによりユーザー指示まで禁止）。
- A/B/C のどれがユーザー報告の症状に該当するかは再現条件の確認が必要。
