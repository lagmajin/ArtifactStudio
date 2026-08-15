# 簡易カラーマネジメント設定の実装
**マイルストーン**: M-CM-1 Quick Color Management Settings
**作成日**: 2026-04-10
**最終更新**: 2026-08-15
**見積もり**: 10-15h
**優先度**: Low (細かいUX改善)

## 2026-07-25 実装監査

OCIOConfig／ArtifactOCIOManager と Color Science Panel の preset・display・view・config load・LUT export の導線、ColorPalette の保存／読込は実装を確認した。一方、本書が想定する StatusBar の Color Space／Gamma／LUT quick control、全 composition への preview-only 適用、最終 render との明確な分離は確認できない。したがって Core／専用 panel は実装済みだが、本マイルストーンの quick status-bar UX と受け入れテストは未完了・runtime未検証とする。

## 概要

After Effects のようなプロフェッショナルツールでは、色空間やガンマ設定を素早く切り替えられるUIが重要。
複雑なカラーマネジメントダイアログを開かずに、基本的な設定を切り替えられるようにする。

## 機能仕様

### ステータスバーへの簡易コントロール
**新規ステータスバー項目:**
- `Color Space: sRGB` (クリックでドロップダウン)
- `Gamma: 2.2` (クリックでドロップダウン)
- `LUT: None` (クリックでブラウザ)

### ドロップダウンメニュー
**Color Space:**
- `sRGB` (ウェブ/モニター用)
- `Adobe RGB` (写真/印刷用)
- `DCI-P3` (デジタルシネマ用)
- `Rec.709` (HDTV用)
- `Rec.2020` (UHDTV用)
- `ACEScg` (プロダクション用)
- `Linear` (リニアワークフロー用)

**Gamma/Power:**
- `1.0` (リニア)
- `2.2` (sRGBガンマ)
- `2.4` (Rec.709ガンマ)

**LUT プリセット:**
- `None`
- `Filmic`
- `AgX`
- `Custom...`

### 実装要件
- `ArtifactStatusBar` に新規コントロール追加
- 設定変更時は全コンポジションに即座適用
- プロジェクト設定として保存
- プレビューでのみ適用 (最終レンダー時は別設定)

### 実装場所
- `Artifact/src/Widgets/ArtifactStatusBar.cpp`
- 新規: `ArtifactColorManagementWidget`
- 設定保存: プロジェクトファイルに追加

## 技術的考慮
- GPU シェーダーでの色変換実装
- OCIO 統合 (将来拡張用)
- プレビューレンダリングのみ適用
- パフォーマンス最適化 (LUT テクスチャキャッシュ)

## AEとの差別化
- より直感的なUI (ボタン式ではなくドロップダウン)
- 一般的な色空間の網羅
- 簡易LUT プリセットの同梱

## テストケース
- 各色空間の正確な変換
- ガンマ設定の適用
- LUT の読み込み/適用
- プレビューレンダリングへの反映
- 最終出力への非適用確認

## 2026-08-15 現行コード監査

- `ArtifactOCIOManager` は preset／config file、working space、display、view、looks、viewer exposure／gamma、CPU view transform、GPU shader descriptor を保持している。
- `Color Science Panel` には専用の設定・LUT／表示変換導線があるため、単純な色空間基盤を未実装とする旧判定は現状に適用しない。
- 一方、文書が想定する StatusBar の Color Space／Gamma／LUT quick control、preview-only と最終 render の明確な分離、全 composition への即時反映は確認できない。
- OCIO／色変換の実機表示差、GPU／CPU parity、プロジェクト保存の受け入れ確認は未検証。

判定: **色管理の Core／専用 panel 基盤は実装済み。StatusBar quick UX、適用境界の明示、runtime／parity 検証は pending。**
