# タイムラインツールチップ拡張の実装
**マイルストーン**: M-TL-11 Timeline Enhanced Tooltips
**作成日**: 2026-04-10
**見積もり**: 6-8h
**優先度**: Low (細かいUX改善)

## 概要

After Effects のタイムラインでは、マウスホバー時にレイヤー情報やキーフレーム情報をツールチップで素早く確認できる。
これを実装することで、クリックせずに情報を素早く把握できるようになり、作業効率が向上する。

## 機能仕様

### レイヤーバーのツールチップ
**ホバー表示情報:**
- レイヤー名
- レイヤー種別 (Video, Audio, Text, Shape, etc.)
- サイズ/解像度 (動画の場合)
- デュレーション
- 現在の不透明度
- ブレンドモード
- エフェクト数

**表示例:**
```
Background Footage
Video Layer | 1920x1080 | 00:00:05:00 | Opacity: 100% | Normal
Effects: 2 (Drop Shadow, Gaussian Blur)
```

### キーフレームのツールチップ
**ホバー表示情報:**
- プロパティ名
- 現在の値
- 補間方法 (Linear, Ease In/Out, etc.)
- 次/前のキーフレーム情報
- 時間位置

**表示例:**
```
Position
X: 960, Y: 540
Linear interpolation
Next keyframe: 00:00:02:15 (X: 1200, Y: 600)
```

### コンポジション領域のツールチップ
- コンポジション名
- 解像度とフレームレート
- カレントタイムインジケーター位置
- ズームレベル

## 実装要件
- `ArtifactTimelineWidget` の mouseMoveEvent でツールチップ生成
- リッチテキスト対応 (Qt::RichText)
- 設定でON/OFF可能
- ツールチップ表示遅延: 500ms (AE標準)
- 複数行表示対応

### 実装場所
- `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cppm`
- 新規設定項目: `Preferences > Timeline > Enhanced tooltips`

## 技術的考慮
- パフォーマンス: 頻繁なマウス移動で重くならないようキャッシュ
- アクセシビリティ: スクリーンリーダー対応
- テーマ対応: ダーク/ライトテーマで色調整

## テストケース
- レイヤーバー各種の情報表示
- キーフレーム情報の正確性
- ツールチップの表示/非表示タイミング
- パフォーマンス劣化の確認