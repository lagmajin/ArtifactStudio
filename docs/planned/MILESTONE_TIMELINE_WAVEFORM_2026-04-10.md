# タイムラインオーディオ波形表示
**マイルストーン**: M-TL-13 Timeline Audio Waveform Display
**作成日**: 2026-04-10
**見積もり**: 12-15h
**優先度**: Low (細かいUX改善)

## 概要

After Effects のタイムラインにオーディオレイヤーの波形を表示し、視覚的に音声の内容を確認できるようにする。
音声同期作業の効率を大幅に向上。

## 機能仕様

### 波形生成
**リアルタイム処理:**
- オーディオファイルの波形データを生成
- RMS (音量) と Peak (ピーク) の両方を表示
- ズームレベルに応じた解像度調整

### 視覚化
**表示オプション:**
- 波形の高さ調整可能
- 色分け (RMS: 青, Peak: 赤)
- 透明度調整
- グリッドとの同期

### インタラクション
**操作支援:**
- 波形上での再生位置表示
- クリックでのシーク
- 波形選択でのトリミング
- ズーム時の詳細表示

### 実装要件
- オーディオ解析ライブラリ統合 (FFmpeg/QtMultimedia)
- キャッシュシステムによる高速表示
- 設定保存
- 大量オーディオファイル対応

### 実装場所
- `Artifact/src/Widgets/Timeline/ArtifactTimelineAudioWidget.cppm` (拡張)
- 波形生成: `Artifact/src/Core/Audio/ArtifactAudioWaveformGenerator.cppm` (新規)

## 技術的考慮
- メモリ使用量の最適化
- 波形生成のパフォーマンス
- ズーム時の再計算

## AEとの差別化
- より高品質な波形表示
- インタラクティブな操作
- パフォーマンスの最適化

## テストケース
- 各種オーディオファイルの波形生成
- ズーム時の表示品質
- インタラクションの正確性
- パフォーマンス劣化の確認

## 2026-07-25 整理

この文書の中核表示要件は、`MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md` と `docs/done/MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md` の正規経路へ統合済み。現行実装は `ArtifactTimelineWidget` が波形データを準備・キャッシュし、`ArtifactTimelineTrackPainterView` が Audio clip の peak／RMS を描画する。

したがって本書は独立した未着手マイルストーンではなく、重複する歴史的設計メモとして扱う。クリックseek、trim／gain直接編集、ズーム品質、設定保存、大量音声時の性能は後続のAudio専用編集・検証項目として残る。
