# Milestone: オーディオ波形ビジュアライザー (2026-03-29)

**Status:** Partial — waveform extraction, timeline display, and initial viewport overlay implemented; spectrum and overlay lifecycle hardening pending
**Goal:** タイムラインとコンポジションビューポートにオーディオ波形を表示。
アニメーションのタイミング合わせに必須。

---

## 現状

| 機能 | 状態 |
|------|------|
| オーディオ再生 | ✅ 完成 |
| 波形データ抽出 | ✅ 実装済み |
| タイムライン波形表示 | ✅ 実装済み |
| コンポジション波形表示 | 🟡 初期Overlay実装済み |
| スペクトラム表示 | ❌ 未実装 |

---

## Implementation

### 1. 波形データ抽出
- WAV/MP3/AAC をデコード → PCM サンプル
- ダウンサンプリング（ピーク値保持）
- 各タイムライントラック用の波形データを生成

### 2. タイムライン波形表示
- タイムライントラックの背景に波形を描画
- 振幅を Y 軸、時間を X 軸
- 色: 低域=青、中域=緑、高域=赤（周波数ビジュアライゼーションオプション）

### 3. コンポジションビューポート波形
- 再生中のリアルタイム波形オーバーレイ
- 選択レイヤーの波形を画面下部に表示

---

## 見積

| タスク | 見積 |
|--------|------|
| 波形データ抽出エンジン | 3h |
| タイムライン波形描画 | 3h |
| コンポジションオーバーレイ | 2h |
| ズーム対応（レベルズーム） | 1h |

**総見積: ~9h**

---

## 関連ファイル

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/src/Audio/AudioRingBuffer.cppm` | オーディオデータ |
| `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` | タイムライン描画 |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 再生エンジン |

## 次の実装スライス: Composition Viewport Overlay

- 既存の `ArtifactCompositionRenderOverlay` の選択・情報表示責務へ波形データ生成を直接混ぜない。
- `ArtifactAudioLayer::buildWaveformData()` のピーク／RMSを、選択中Audio Layerの表示用スナップショットとして受け取る専用Overlay境界を設ける。
- 表示条件は `showAudioWaveformOverlay`、選択レイヤーがAudio Layerであること、現在フレームが有効であることに限定する。
- オーバーレイはビュー下部の固定領域に表示し、コンポジション本体の座標変換やレイヤー合成へ影響させない。
- 表示更新はフレーム変更・選択変更・ズーム変更時に限定し、音声サンプルの再デコードを描画パスから追い出す。
- 実装後に、非Audio Layer、無音、未ロード素材、選択なし、表示OFFの各ケースを確認する。

初期実装では選択中Audio Layerの波形をViewport下部へ表示する。`CompositionRenderController::setShowAudioWaveformOverlay()` で表示をOFFにできる。設定の永続化、更新キャッシュ、スペクトラム表示は後続作業とする。
