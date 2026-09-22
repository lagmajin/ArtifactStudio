# M-VP-PFRAME-1: Projected Frame Gizmo 仕上げ（分離・残項目・runtime受入）

**最終更新:** 2026-09-21

**ステータス:** In Progress（2026-09-21 にリサイズガイドを実装。クラス分離と runtime 受入は未着手）

## 目的

Composition Viewport の 3D投影フレーム（projected frame）を、追加の仕様拡張なしで受入可能な状態まで閉じる。対象は (1) runtime 受入、(2) `ArtifactProjectedFrameGizmo` のクラス分離、(3) 残項目（回転マーク、3D軸ギズモとの Z オーダー、常設対角線の切替）である。

## 前提（2026-09-21 実コード照合の確定事実）

- ヒットテスト（コーナー／エッジ）、フレーム内移動、バッジ、ドラッグHUD、スナップ、Undo、ダブルクリックリセット、最小サイズクランプ、near/far クリップ、数値入力（`w`/`h`）、モード別フィルタ、複数選択の包絡フレーム＋各レイヤー細枠は実装済み。詳細は `docs/spec/SPEC_3D_FRAME_GIZMO_REQUIREMENTS_2026-07-31.md` の 7章を参照。
- 2026-09-21 にリサイズガイドを追加した（`projectedFrameGuidePoints`、`projectedFrameScaleStartHandlePoint_`、`CompositionRenderController::Impl::drawViewportInteractionOverlay` の描画ブロック）。
- 投影フレームのロジックは `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の無名名前空間ヘルパーと `Impl` メンバに分散しており、専用クラスは無い。
- Z回転はフレームではなく 3D 回転リングが担う（`showProjectedRotationHandle = !projectedFrame`）。
- `Viewport/ProjectedFrame/ShowDiagonals` は関数内 `static const` として起動時に1回だけ読み、UI からは切替できない（既定は無効）。

## Phase 計画

| Phase | 内容 | 状態 | 主な対象 |
| --- | --- | --- | --- |
| P0 | runtime 受入（ビルド・実機） | 未着手（ビルドはユーザー許可待ち） | ビルド手順、受入観点 |
| P1 | 投影フレームの入出力棚卸し | 未着手 | `ArtifactCompositionRenderController.cppm` |
| P2 | `ArtifactProjectedFrameGizmo` 分離 | 未着手 | 新規 `.ixx` / `.cppm` + 同 controller |
| P3 | 残項目（回転マーク・Zオーダー・対角線切替） | 未着手 | Overlay / Controller / 設定 |

### P0 — runtime 受入

ビルド・テスト・CMake 実行は AGENTS.md により**ユーザーの明示指示が必要**。指示を受けたら次を確認する。

- `check_module_hygiene` とモジュール再スキャン（今回の変更に `.ixx` の増減は無い想定）。
- 単一レイヤー: コーナー／エッジのドラッグ、Shift（アスペクト維持）、Ctrl（中心固定）、ダブルクリックリセット、Esc／右クリックのキャンセル、Undo／Redo。
- ガイド: ドラッグ中だけ固定点マーク・開始位置マーク・固定点と駆動点を結ぶ線が出ること、Move モードと包絡フレームでは出ないこと。
- スナップ（ON/OFF、Alt 一時解除）、バッジクリック → `w`/`h` 入力 → Enter 確定。
- 表示倍率 100／150／200%、斜めカメラ、near/far 跨ぎ、D3D12 と Vulkan の両方。

### P1 — 棚卸し

分離前に、投影フレームの状態と関数を一覧化して所有者を決める。

- 状態: `projectedFrameHandle_`、`projectedFrameMove_`、`projectedFrameScaleStartPointer_` / `projectedFrameScaleFixedPointer_` / `projectedFrameScaleStartHandlePoint_`、`projectedFrameScalePointerBasisValid_`、スナップ状態、`projectedFrameLastPointer_`、`projectedFrameCorrectedLocalPosition_`、`projectedFrameForcedLocal_` / `projectedFrameForcedMode_`。
- 関数: ヒットテスト、ガイド点算出、スナップ、スケール解決、リセット、描画補助。
- 既存の `Artifact3DGizmo`（`GizmoMode` / `GizmoSpace`）との境界を明文化し、3D軸操作を移さないことを確認する。

### P2 — `ArtifactProjectedFrameGizmo` 分離

挙動を変えずに段階的に移す。

1. 新規 `Artifact/include/Widgets/Render/ArtifactProjectedFrameGizmo.ixx` と実装 `.cppm` を追加し、既存フリー関数をラップして委譲する（呼び出し結果は同一）。
2. 状態（`projectedFrame*_`）をクラスへ移し、`Impl` からは所有ポインタだけを持つ。
3. controller は press／move／release のルーティングと overlay 呼び出しだけにする。
4. 旧ヘルパーと旧メンバを削除し、`check_module_hygiene` の対象（自己 import、前方宣言、purview への include）を確認する。

新規 `.ixx` / `.cppm` は `Artifact/CMakeLists.txt` の登録（`GLOB` と force list）を伴うため、全体再スキャン要因になる。登録漏れと import 漏れを分けて切り分ける。

### P3 — 残項目

- 回転 0／90／180／270 のマーク: 3D回転リングのリーダーHUDに対する追加表示として検討する。フレームの回転ハンドル復活は行わない。
- 3D軸ギズモとの Z オーダー: コーナー／エッジ優先は press 経路で解決済み。軸がフレームより手前にあるケースのみ追加の除外を検討する。
- `ShowDiagonals` の runtime 切替: 関数内 `static` を外し、既存の設定値経路で読み直せるようにする（UI 追加は別判断。枠内のXがハンドルと誤認される懸念を再確認する）。

## 受入条件

- 投影フレームの操作結果が、状態・Composition フレーム・Timeline・Viewport 表示で一致する。
- 1回のドラッグが1回の Undo／Redo で往復し、キャンセルで元に戻る。
- ガイドはドラッグ中だけ表示され、非ドラッグ時・Move モード・包絡フレームでは表示されない。
- D3D12 と Vulkan で同じ入力・表示結果になる。
- 分離後も既存の公開 API（controller の public メソッド）と描画経路が変わらない。

## 非対象・禁止事項

- 新規 signal／slot、QtCSS（`setStyleSheet`）、`QColorDialog`、`QImage` 化、`QPainter` 合成の追加。
- `ReactiveEvents` とサブモジュール（`ArtifactWidgets` / `libs/DiligentEngine` / `third_party/*`）の変更。
- ホットパスでの新規確保の追加。今回のガイドは線1本＋矩形3個のみで、確保は既存フィールドの読み出しに限定している。
- Composition Viewport の採用モックを根拠としたキャンバス内要素（ギズモ・選択枠・HUD）の追加変更。

## 検証状況

- 2026-09-21: リサイズガイドを実装。静的確認のみ（宣言順序、シグネチャ、名前衝突、ブレース対応、改行維持）。ビルドと実機確認は未実施。
- `python tools/docs/audit_document_dates.py docs --file ...` は対象文書で問題なし。
- 変更文書に絶対パスリンクと壊れた相対リンクは追加していない。
- この環境では git が起動しないため、差分・コミット・サブモジュール gitlink の確認は未実施。
- `tools/generate_doc_inventory.py` は git の履歴日付を使うため、git が使えない環境では実行しない（`docs/INDEX_GENERATED.md` の更新日が失われる）。

## 関連文書

- `docs/spec/SPEC_3D_FRAME_GIZMO_REQUIREMENTS_2026-07-31.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`
- `docs/design/composition-viewport/README.md`
- `docs/memo/BUG_3D_FRAME_GIZMO_DRAG_2026-07-31.md`
- `Insight.md`（2026-09-21 の項目）
