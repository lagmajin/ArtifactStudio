**最終更新:** 2026-09-09

# Color Grading UI Concept

`artifactstudio-color-grading-wheels-concept-2026-09-09.png` は、ArtifactStudio の既存テーマに合わせたカラーコレクション UI の検討用モックアップである。

## 採用候補

- Lift / Gamma / Gain / Offset の4ホイール構成
- ホイール下の master luminance と RGB 数値入力
- Temperature / Tint / Contrast / Pivot / Saturation / Mix の一次調整
- Before / After wipe、RGB Parade、Vectorscope の近接配置
- Wheels / Curves / Qualifiers / LUTs の段階的な編集導線

## 注意

- 本画像はUI検討用であり、表示内容や写真素材を製品アセットとして採用しない。
- DaVinci Resolve の商標、ロゴ、固有アセット、画面構成を複製しない。
- 実装では既存 `ColorWheelsEffect`、`LiftGammaGainEffect`、ColorGradingEngine、scope renderer を再利用し、通常の Effect stack と状態を二重化しない。
- 新規シグナル＆スロットを追加せず、既存イベント経路とサービスを利用する。
- QtCSS、`QColorDialog`、ホットパス上の `QImage` / `QPainter` 合成を追加しない。
