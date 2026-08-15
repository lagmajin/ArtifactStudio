# Milestone: オーディオ波形ビジュアライザー (2026-03-29)

**最終更新:** 2026-08-15

## Update 2026-08-15 — sample-rate 契約補正

Contents Viewer の waveform／spectrum 解析が固定 44.1kHz を使っていたため、既存 audio metadata の audio stream sample rate を参照するよう補正した。metadata が取得できない場合は従来どおり 44.1kHz を fallback とする。実ファイルでの再生・表示 parity は未検証。
**Status:** 部分実装 — 波形抽出、タイムライン表示、キャッシュ、viewport／spectrum overlay は実装済み。実機検証と更新経路の確認待ち
**Goal:** タイムラインとコンポジションビューポートにオーディオ波形を表示。
アニメーションのタイミング合わせに必須。

## 2026-08-15 現行コード監査

`AudioWaveformGenerator`／`ArtifactAudioLayer::buildWaveformData()`、タイムラインの peak／RMS 描画と render cache、`AudioPreviewWidget`／Contents Viewer の波形表示、Asset Browser の非同期 waveform thumbnail、viewport の audio waveform／spectrum overlay と QSettings の表示設定を確認した。ソース signature によるキャッシュ再利用・更新時の無効化も実装されている。

ただし、タイムライン、viewport、asset thumbnail、viewer の各表示で同一のライフサイクル／サンプルレート契約が保たれるか、長尺・無音・未ロード・差し替え素材での再生成、スクラブ／再生中の遅延、スペクトラム overlay の実機安定性は静的確認だけでは保証できない。GPU 本流の波形表示ではなく、現状は CPU／Qt ウィジェット描画経路が中心である。

判定: **波形基盤と主要表示は実装済み、runtime 検証・表示経路の統一は pending。**

---

## 現状

| 機能 | 状態 |
|------|------|
| オーディオ再生 | ✅ 完成 |
| 波形データ抽出 | ✅ 実装済み |
| タイムライン波形表示 | ✅ 実装済み |
| コンポジション波形表示 | 🟡 初期Overlay実装済み |
| スペクトラム表示 | 🟡 初期Overlay実装済み |

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

初期実装では選択中Audio Layerの波形をViewport下部へ表示し、AudioSpectrumの結果を右上へ表示する。波形とスペクトラムは `setShowAudioWaveformOverlay()` / `setShowAudioSpectrumOverlay()` で個別に切り替えられ、状態は `QSettings` に永続化する。同一ソースパスの再描画ではピーク／RMSキャッシュを再利用し、ファイルサイズまたは更新時刻が変わった場合は両キャッシュを再生成する。スペクトラムの実機ライフサイクル検証は後続作業とする。

## Update 2026-08-15

現行コードを追加確認した。波形抽出と peak／RMS cache は `AudioWaveformGenerator`／`ArtifactAudioLayer::buildWaveformData()` にあり、Timeline track、Composition viewport、`AudioPreviewWidget`／Contents Viewer、Asset Browser thumbnail がそれぞれ表示経路を持つ。viewport の waveform／spectrum overlay は表示設定を QSettings に保存し、source signature の変化で cache を更新する。

未完了・未検証なのは、各表示面の sample-rate／lifecycle 契約、長尺・無音・未ロード・差し替え素材の再生成、scrub／再生中の遅延、大量アセット負荷、spectrum overlay の実機安定性である。波形基盤と主要表示は実装済み、runtime parity と経路統一は pending とする。

`M-AU-3 Audio Visualization` として、追加の現行コード監査を行った。`AudioWaveformGenerator` の peak／RMS、`AudioLevelMeter` の attack／release／peak hold／clip 検出、Timeline／Composition viewport／Asset Browser／Contents Viewer の波形表示、viewport spectrum overlay を確認した。静的な表示基盤は実装済み相当で、実機の更新遅延・長尺負荷・source 差し替え時の再生成・各表示面の sample-rate 契約は未検証とする。
