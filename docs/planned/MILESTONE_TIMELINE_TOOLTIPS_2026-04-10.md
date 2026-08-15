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

---

## 2026-07-25 現状確認

キーフレーム／編集面の tooltip は実装済み。`ArtifactTimelineTrackPainterView.cppm` では marker hover 時にプロパティ名、値、フレーム、lane、incoming/outgoing easing、補間、anchor、roving、color label、選択状態、current/nearest 状態を複数行で表示する。Bezier handle、選択キーのショートカット、proportional editing の半径、snap 情報も tooltip に含まれる。keyframe area、clip、drag 中の tooltip も別形式で実装され、hover 位置の変化に応じて更新・非表示される。

未完了・未確認:

- レイヤーバーの解像度、blend mode、effect 一覧などの詳細 tooltip
- enhanced tooltip の設定 ON/OFF と 500ms 遅延の明示制御
- リッチテキスト、tooltip キャッシュ、スクリーンリーダー対応
- 大量レイヤー、テーマ切替、実運用での表示負荷と情報量の検証

したがって「marker / area / clip / drag の実用 tooltip は実装済み、レイヤー詳細と設定・品質検証は未完了」と整理する。

## Implementation Update 2026-08-15

- `ArtifactLayerPanelWidget` のレイヤー行 tooltip に、レイヤー種別、in／out frame、opacity、blend mode、effect 数を追加した。
- 既存の Track Matte 情報と Alt-drag の案内は維持している。
- 解像度、設定による ON/OFF、500ms 遅延、tooltip cache、テーマ／大量レイヤーでの runtime 検証は未実施。
