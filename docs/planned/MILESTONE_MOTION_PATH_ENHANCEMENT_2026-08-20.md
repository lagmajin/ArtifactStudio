# MILESTONE: モーションパス機能の充実（Broken Tangent / Seek 連動 / HUD フィードバック / 高度編集）

> 2026-08-20 作成

**最終更新:** 2026-08-20
**Status:** In Progress (Phase 1〜3 実装中)
**Priority:** High
**Related:** `docs/planned/MILESTONE_MOTION_PATH_EDITING_2026-04-29.md`, `docs/planned/MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md`, `docs/planned/MILESTONE_AUTO_ORIENT_2026-06-16.md`

---

## 1. 目的

コンポジションエディタ（Viewport）上のモーションパス機能を、単なる表示・基本ドラッグから **After Effects 相当以上の本格的かつ直感的な空間アニメーション編集ツール** へと引き上げる。

特に以下の課題を解決する：
1. ベジェハンドルの角度を折る（Cusp / Broken Tangent）操作ができないため、急激な方向転換（バウンドや鋭角ターン）のパスが作りにくい。
2. パス上のキーフレームをクリックしても、再生ヘッド（CTI）がそのフレームに移動せず、タイムラインとの往復が必要になる。
3. どのフレームのキーを操作・ホバーしているか、視覚的・数値的なフィードバックが不足している。

---

## 2. フェーズ構成

### Phase 1: ハンドルの屈折・独立操作（Broken / Cusp Tangents）
- `Alt + タンジェントドラッグ` で In / Out ハンドルの連動（`linked`）を解除。
- 角度・長さを独立して調整できるようにし、鋭角な空間パス（シャープな方向転換）を可能にする。
- 通常ドラッグ時は連動状態を維持。
- Undo / Redo で `linked` 状態および各タンジェント値の正確な復元。

### Phase 2: キーフレームへのシーク・CTI 同期（Double-Click to Seek）
- モーションパス上のキーフレーム点をダブルクリック（またはクリック操作）した際に、コンポジションの再生ヘッドをそのフレーム番号へシーク。
- Viewport とタイムラインの作業文析をシームレスに結合する。

### Phase 3: 操作・ホバー時の HUD / フレーム番号フィードバック
- キーフレームやタンジェントのホバー時に、対象のフレーム番号（例: `Frame 45`）や補間タイプ、座標を Viewport HUD にリアルタイム表示。
- ドラッグ操作中も移動先・変形モード（Move / Rotate / Scale / Tangent）の直感的な案内を提供。

### Phase 4: 後続ロードマップ（将来拡張）
- **Auto-Orient Along Path**: パスの進行方向（接線）に沿ってレイヤーの回転角を自動追従（`MILESTONE_AUTO_ORIENT_2026-06-16.md` 連携）。
- **Roving Keyframes（等速ロービング）**: パス形状を保持したまま時間タイミングを空間長さに応じて自動配分。
- **Lasso / 矩形ラバーバンド選択**: パス上の複数キーフレームの一括範囲選択。

---

## 3. 実装対象ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`

---

## 4. 成功条件 (Done Criteria)

1. `Alt + タンジェントドラッグ` で片方のハンドルのみが回転・伸縮し、折れたハンドル（Broken Tangent）が作成できること。
2. 通常のタンジェントドラッグでは従来通り対称に連動すること。
3. パス上のキーフレームをダブルクリックすると、タイムラインの再生位置が該当フレームに移動すること。
4. ホバー中に対象フレーム情報が HUD に分かりやすく表示されること。
5. 操作が既存の Undo / Redo と完全に整合すること。
