# 高度なスナップ機能の実装

**最終更新:** 2026-08-15
**マイルストーン**: M-SN-1 Advanced Snapping System
**作成日**: 2026-04-10
**見積もり**: 12-15h
**優先度**: Low (細かいUX改善)

## 2026-08-15 現行コード照合

- ✅ `TransformGizmo` は他レイヤーの bounds／center／edge、composition 基準、spacing guide を検出し、移動・リサイズの snap と active line／label 表示へ接続している。
- ✅ ViewMenu には grid／guide の表示・スナップ切替があり、ショートカットと `QSettings` による状態復元がある。GridSettings には snapToGrid が含まれる。
- ✅ 回転には `Viewport/RotationSnapDegrees` 設定と軸／角度スナップの既存経路がある。
- ⚠️ 旧案の専用 Snap Manager、ターゲット別 ON／OFF、許容範囲・磁石強度・色の設定ダイアログ、safe margin／custom guide の統一モデルは確認できない。
- ⏳ Alt トグル、優先順位設定、音 feedback、全操作モードの runtime 精度・性能 QA、設定 UI の完全接続は未完了。

## 概要

After Effects のような精密な位置調整をサポートする高度なスナップシステム。
ガイド、グリッド、レイヤー境界などへの正確なスナップを実現する。

## 機能仕様

### スナップターゲット
**新規スナップ先:**
- `Guides`: カスタムガイド線
- `Grid`: ピクセル/パーセントグリッド
- `Layer Bounds`: 他のレイヤーの境界線
- `Layer Centers`: レイヤー中心線
- `Composition Edges`: コンポジション境界
- `Safe Margins`: タイトル/アクションセーフ

### スナップ設定ダイアログ
**詳細設定:**
- 各ターゲットごとのON/OFF
- スナップ許容範囲 (ピクセル単位)
- スナップガイドの表示色
- 磁石効果の強度

### スマートスナップ
**インテリジェント動作:**
- 最も近いターゲットへの自動スナップ
- 複数ターゲット間の優先順位付け
- ドラッグ中のプレビュー表示
- スナップ音/視覚フィードバック

### 実装要件
- 既存のトランスフォーム操作に統合
- リアルタイムパフォーマンス
- 設定保存/読み込み
- キーボードトグル (Altキー)

### 実装場所
- `Artifact/src/Core/ArtifactSnapManager.cppm` (新規)
- UI: `View > Snapping` メニュー
- 設定: `Preferences > General > Snapping`

## 技術的考慮
- レイヤー座標計算の高速化
- メモリ使用量の最適化
- マルチスレッド対応

## AEとの差別化
- より詳細なターゲット選択
- 設定可能な許容範囲
- 視覚フィードバックの充実

## テストケース
- 各種ターゲットへの正確なスナップ
- パフォーマンス劣化の確認
- 設定の保存/読み込み
- キーボードトグルの動作
