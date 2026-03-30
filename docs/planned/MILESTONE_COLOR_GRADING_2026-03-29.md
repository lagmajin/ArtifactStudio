# Milestone: Color Grading Suite (2026-03-29)

**Status:** Not Started
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
