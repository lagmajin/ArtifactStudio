# Professional Monitoring Scopes Expansion (2026-08-08)

**最終更新:** 2026-08-10
**状態:** 一部実装（Phase 1a Histogram 統合済み、残作業あり）

## 概要

ArtifactStudio のカラーグレーディング・ポストプロダクション向けモニタリングツールを DaVinci Resolve / Scratch / Nuke 相当に拡充する。既存のスコープ基盤（WaveformScope / VectorScope / ParadeScope / HDRMonitor / Histogram widget）の統合と新規追加。

> **命名注記:** 公開モジュール／ファイル名は互換性のため `HistgramWidget` のまま維持しているが、正式な公開型名は `HistogramWidget` へ移行した。`HistgramWidget` は alias、`setHistgram()` は互換 API として残している。モジュール名・ファイル名の改名は import 元全体に影響するため、別の互換性移行として扱う。

## 現状スコアカード

### 実装済み ✅

| ツール | ファイル | 状態 |
|--------|---------|------|
| WaveformScopeWidget | `ArtifactWidgets/src/Color/WaveformScopeWidget.cppm` | ✅ Luma/RGB/YCbCr。非同期コンピュート (`QtConcurrent::run`)。IRE graticule |
| VectorScopeWidget | `ArtifactWidgets/src/Color/VectorScopeWidget.cppm` | ✅ Standard/HLS/Skin。スキントーンインジケーター |
| ParadeScopeWidget | `ArtifactWidgets/src/Color/ParadeScopeWidget.cppm` | ✅ RGB/YCbCr/YRGB |
| Histogram widget (`HistogramWidget`, legacy alias: `HistgramWidget`) | `ArtifactWidgets/src/Color/HistgramWidget.cppm` | ✅ Luma/RGB/Parade/Combined。対数スケール。Composition Editor のスコープダイアログに統合済み |
| ArtifactHDRMonitor | `Artifact/src/Render/ArtifactHDRMonitor.cppm` | ✅ FalseColor / Waveform / Vectorscope / GamutWarning。`analyzeFrame()` が clip/gamut stats を計算 |
| スコープダイアログ | `ArtifactCompositionEditor.cppm` | ⚠️ 非ドックウィンドウ。VectorScope+Waveform+Parade の3タブのみ。150ms タイマー更新 |
| QCデータ (内部) | `HDRMonitor::analyzeFrame()` | ⚠️ clip, gamut violation の数値あり。UI 表示なし |

### 未実装 ❌

| ツール | Resolve | Scratch | Nuke | 優先度 |
|--------|---------|---------|------|--------|
| **CIE 色度図** | ✅ | ✅ | ✅ | 🔴 |
| **スコープレイアウトプリセット** | ✅ | ✅ | ✅ | 🔴 |
| **ドッキング常駐スコープパネル** | ✅ | ✅ | ✅ | 🔴 |
| **Line Scope（単線波形）** | ✅ | ❌ | ✅ | 🟡 |
| **QC ステータスバー** | ✅ | ✅ | ✅ | 🟡 |
| **Alpha channel scope** | ✅ | ❌ | ✅ | 🟡 |
| **Time Scope（時間方向）** | ❌ | ✅ | ❌ | 🟢 |

---

## Phase 1: Histogram 統合 + CIE 色度図（即時、高インパクト）

### 1-1. Histogram をスコープダイアログに追加 ✅

**結果**: Histogram widget（API 名: `HistgramWidget`）を Composition Editor の既存 Preview Vectorscope ダイアログへ追加した。Combined + 対数スケールを既定値とし、既存のタイマー更新および初回更新で同じフレームを渡す。

**変更**:
- `ArtifactCompositionEditor.cppm`: global module fragment の include 群を変えず、既存 import 群に `import HistgramWidget;` を追加
- スコープダイアログに `tabs->addTab(histogramWidget, "Histogram")` 追加
- タイマーコールバックと初回更新に `histogramWidget->updateFrame(frame)` 追加

**残課題**: Phase 0 の非同期 readback 化と既存スコープ更新不具合の修正は未着手。Histogram は既存の更新周期に統合しただけであり、これらの安定化を置き換えない。

---

### 1-2. CIE 1931 色度図スコープ

**目的**: プロフェッショナルな色彩管理の必須ツール。Rec.709 / DCI-P3 / Rec.2020 の gamut 境界を表示し、現在フレームの色分布を可視化。

**仕様**:
- 馬蹄形の CIE 1931 xy 色度図を背景に描画
- Rec.709 / DCI-P3 / Rec.2020 / ACES AP1 の gamut 三角形オーバーレイ（設定で切替可能）
- フレーム全画素を xy 空間にプロット（半透明ドット、Intensity 調整付き）
- White point (D65) 十字線
- スキントーンライン（I-line）の角度インジケーター
- マウスホバーで xy 値表示

**実装**:
- `CIEChromaticityWidget` を新規作成（`ArtifactWidgets/src/Color/CIEChromaticityWidget.cppm`）
- sRGB → XYZ → xy 変換（既存の `Color` モジュールに RGB→XYZ 変換あり）
- Gamut 三角形は頂点の xy 座標から描画
- CIE 曲線は参照テーブル（PolygonalChain で高速描画）

**コスト**: 中（新規ウィジェット作成 + カラーマトリクス計算）。既存 `Color.ColorSpace` / `Color.ColorGamutConversion` の色域定義を利用し、入力フレームの色空間・transfer function を明示してから xy に変換する。表示用に sRGB を仮定して変換する実装は不可。

---

## Phase 0: 既存スコープ更新経路の安定化（Phase 1 の先行条件）

**理由**: Composition Editor は 150ms タイマーで `captureCurrentFrameImage()` を呼び、同期 GPU readback を UI スレッドで待機する。さらに Waveform / Parade の非同期完了後更新には既知の不具合がある。この状態で Histogram / CIE を同じ入力に追加すると、機能は表示できてもプレビューの停止と再描画不良を増やす。

**変更**:
- `captureCurrentFrameImage()` の同期呼び出しを、既存の非同期 readback 経路とフレーム cache key に置き換える
- Waveform / Parade の watcher 接続を生成時に一度だけ設定し、完了時に確実に widget を更新する
- スコープパネル非表示時は更新を停止し、同一フレームを再解析しない

**完了条件**: スコープを開いたまま再生・停止・シークしても UI スレッドで fence 待機を行わず、全既存スコープが新フレームで再描画される。

---

## Phase 2: スコープパネルのドック化 + レイアウトプリセット

### 2-1. スコープパネルを QDockWidget 化

**現状**: `QDialog` ベースのフローティングウィンドウ。閉じると状態消失。メインウィンドウにドック不可。

**変更**:
- `ScopePanelWidget` を `QDockWidget` ベースで作成
- 全スコープを `QSplitter` で分割レイアウト可能に
- 各スコープの visible/toggle を右クリックコンテキストメニューで管理
- `ArtifactMainWindow` の `View` メニューに `Scopes` チェック項目追加
- ショートカット（デフォルト: `Ctrl+Shift+S`）でトグル

**コスト**: 中

---

### 2-2. スコープレイアウトプリセット

**仕様**:
- プリセット:
  - "Single Large": 1スコープ全画面（モード切替で表示切替）
  - "Dual Vertical": Waveform + Vectorscope 左右
  - "Quad": Waveform / Vectorscope / Parade / Histogram 2×2
  - "Colorist": Parade (大) + Vectorscope (小) + Histogram (小)
  - "Broadcast": Waveform + Vectorscope + Gamut Warning
- プリセット切替: スコープパネル上部のドロップダウン
- `LayeredConfigStore` に保存（ユーザーカスタム対応）

**コスト**: 低〜中（QSplitter state + 設定永続化）

---

## Phase 3: QC 情報の可視化 + Line Scope

### 3-1. QC ステータスバー

**現状**: `ArtifactHDRMonitor::analyzeFrame()` が `hasClipping`, `clippedHighlights`, `clippedShadows`, `broadcastSafeViolations` を内部で計算済み。

**仕様**:
- スコープパネル下部に常時 QC バー表示:
  - クリップ警告: 「⚠ HL: 342 px  SD: 18 px」形式。閾値超えで赤色
  - Gamut 警告: 「⚠ Out of Gamut: rec709=12px, P3=47px」
  - マウスホバーピクセルの RGB/YUV 値をツールチップ表示
- 設定で警告閾値（IRE単位）を調整可能
- broadcast legal check: 100 IRE 上限 / 0 IRE 下限 + 色差制限

**コスト**: 低（既存データの表示。新規計算はほぼ不要）

---

### 3-2. Line Scope（単線波形）

**仕様**:
- マウスホバー位置の水平1ラインの波形を表示
- Waveform のサブモードとして実装（`WaveformMode::Line`）
- または独立した小スコープとして、スコープパネル右上に固定表示
- ガンマ／リニア切替可能

**実装**: 既存の `WaveformScopeWidget::rebuildScopeImageAsync()` を拡張し、1行のみのデータをルックアップテーブル描画

**コスト**: 低（既存コードの分岐追加）

---

### 3-3. Alpha channel scope

**仕様**:
- Waveform の追加モード（`WaveformMode::Alpha`）
- アルファチャンネルのみのグレースケール波形
- マスクのエッジ品質確認に必須

**コスト**: 低（既存の Luma モードをコピーして alpha チャンネル読み取りに変更）

---

## Phase 4: パフォーマンス最適化

### 4-1. スコープデータソースの GPU 化

**現状**: スコープの input は `captureCurrentFrameImage()` → `QImage`。スワップチェーン readback が毎フレーム発生し、GPUパイプラインをストールさせる。

**改善**:
- `HDRMonitor` の `analyzeFrame()` を拡張し、スコープ計算に必要なデータ（輝度分布、クロマ分布、ヒストグラムビン）を GPU コンピュートシェーダーで計算
- `ChannelType::Emission` と同様のパターンで `ChannelType::ScopeData` を作成
- Waveform: 横方向 → 輝度ヒストグラムへの CS リダクション
- Vectorscope: RGB → UV マッピングへの CS
- Histogram: 256-bin counting CS（`InterlockedAdd`）

**コスト**: 中（コンピュートシェーダー 2〜3本 + カスタム UAV readback）

---

## Phase 一覧

| Phase | 内容 | コスト | 既存依存 |
|-------|------|--------|---------|
| 0 | 既存スコープ更新経路の安定化 | 中 | 既存非同期 readback |
| 1 | Histogram統合 + CIE色度図 | 低〜中 | Phase 0 |
| 2 | ドック化 + レイアウトプリセット | 中 | Phase 1 |
| 3 | QCバー + Line Scope + Alpha scope | 低 | Phase 2 |
| 4 | GPU スコープ計算 | 中 | Phase 2（ドック化後） |

Phase 1〜2 で DaVinci Resolve のスコープセクションにほぼ匹敵する。
Phase 1 だけで Histogram + CIE 色度図が追加され、即座に価値が出る。

## 変更対象ファイル一覧

| ファイル | Phase |
|----------|-------|
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | 1, 2 |
| `ArtifactWidgets/include/Color/CIEChromaticityWidget.ixx` (新規) | 1 |
| `ArtifactWidgets/src/Color/CIEChromaticityWidget.cppm` (新規) | 1 |
| `Artifact/include/Widgets/ScopePanelWidget.ixx` (新規) | 2 |
| `Artifact/src/Widgets/ScopePanelWidget.cppm` (新規) | 2, 3 |
| `Artifact/src/Widgets/ArtifactMainWindow.cppm` | 2 |
| `ArtifactWidgets/src/Color/WaveformScopeWidget.cppm` | 3 |
| `Artifact/src/Render/ArtifactHDRMonitor.cppm` | 4 |
| `Artifact/App/shaders/scope_*.hlsl`（3本・新規） | 4 |
