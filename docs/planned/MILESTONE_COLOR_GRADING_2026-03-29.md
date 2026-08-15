# Milestone: Color Grading Suite (2026-03-29)

**最終更新:** 2026-08-15
**Status:** 色補正／curves／scope／LUT 基盤は実装済み、統合 UI と runtime 受入れ待ち
**Goal:** DaVinci Resolve / Lumetri 風のカラーグレーディングツール。
カーブ、ホイール、スコープをビューポートに統合。

---

## 現状

| 機能 | 状態 |
|------|------|
| 基本カラーコレクション | ✅ 完成 (Brightness, Hue/Sat) |
| カラーホイール (Lift/Gamma/Gain) | ✅ 完成 |
| カラースペース管理 | ✅ 完成 (sRGB/Rec709/Rec2020/P3/ACES) |
| カラーカーブ | ❌ 未実装 |
| スコープ (Waveform/Vectorscope) | ❌ 未実装 |
| ルックアップテーブル (LUT) 適用 | ⚠️ 基本のみ |

## 2026-07-25 実装監査

現状表を更新するための監査結果。ColorSpace／OCIO の管理基盤、ColorWheels／Lift-Gamma-Gain、ColorCurves、Waveform／Vectorscope／Parade／Histogram の renderer、LUT の読み込み・適用 API は実装を確認した。ただし、カーブの編集 UI、スコープのビューポート統合と継続更新、LUT プリセット／ブラウザ、grading suite としての一体型 UI、runtimeでの表示・適用確認は未確認である。したがって本マイルストーンは「基盤部分実装、UI統合と受け入れ未完了」とする。

## 2026-08-15 現行コード監査

- `ColorWheelsProcessor`／`ColorCurves`／`ColorGrader`、ColorSpace／OCIO、LUT load／apply／write の基盤を確認した。
- `ColorScopeRenderer` に Waveform／Vectorscope／Parade／Histogram の描画経路があり、GPU bin／HDR monitor 側の解析基盤も存在する。
- ただし、curves の編集面、scope の viewport／workspace 統合と継続更新、LUT browser／preset UI、grading suite の一体型レイアウト、選択同期と runtime の表示・適用結果は未検証。

判定: **カラーグレーディングの処理基盤は実装済み。専用 suite UI、scope／LUT workflow 統合、runtime 受入れは pending。**

## Update 2026-08-15 — M-FX-6 実装確認

- `ColorWheelsEffect` と `CurvesEffect` は CPU／GPU の両実装を保持し、Inspector の effect catalog／factory から追加できる。
- ColorGradingEngine は Color Wheels、RGB curves、Hue/Sat/Luma、LUT、preset 保存・読込、サンプル解析を持つ。
- `ArtifactColorScienceManager` は LUT の load／builtin／intensity／GPU upload 用参照を持つ。
- したがって M-FX-6 は「処理基盤と effect 導線は実装済み」と更新する。ただし、GPU effect chain の恒常利用、ColorGradingEngine と通常 effect stack の単一状態化、専用 grading UI／scope／LUT browser、CPU／GPU parity の runtime 受入れは未完了または未検証。

---

## Implementation

### 1. カラーカーブエディタ
- RGB 各チャンネルのカーブ
- マスターカーブ（輝度）
- ベジェカーブでポイントを追加/編集
- ヒストグラムオーバーレイ

### 2. スコープ
- **Waveform** — 輝度の時間変化
- **Vectorscope** — 色相/彩度の極座標表示
- **Parade** — RGB チャンネル並列表示
- **Histogram** — 輝度分布

### 3. LUT 適用
- .cube / .3dl / .lut ファイル読み込み
- 3D LUT テーブル補間
- LUT プリセット管理

---

## 見積

| タスク | 見積 |
|--------|------|
| カラーカーブエディタ | 4h |
| スコープ (Waveform/Vectorscope) | 4h |
| LUT 読み込み/適用 | 3h |
| UI (ビューポートオーバーレイ) | 2h |

**総見積: ~13h**
