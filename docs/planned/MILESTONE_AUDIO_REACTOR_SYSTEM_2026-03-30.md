# Milestone: Audio Reactor System (M-AU-6)

**最終更新:** 2026-08-15
ステータス: FFT／Audio Reactive binding 基盤実装済み（専用サービス化・実運用検証待ち、静的確認 2026-07-29）

## 🎯 目的
`ArtifactAudioMixer` と `AudioRenderer` から提供されるオーディオデータを解析し、レイヤーのプロパティ（位置・スケール・色・エフェクト強度）を音楽や音声に同期して自動アニメーションさせるシステムを構築する。

## 🏗️ アーキテクチャ構成
1. **`AudioFFTService`**: 
   - 再生中のオーディオをリアルタイムで解析（FFT）し、周波数帯域ごとの強度を計算するサービス。
2. **`AudioLinkValue`**:
   - 任意のプロパティが「オーディオのどの周波数（Bass/Mid/High）に反応するか」を定義するマッピング。
3. **`AudioTimelineView`**:
   - タイムライン上にオーディオの波形を描画し、同期のタイミングを視認できる UI。

## 📅 実装フェーズ

### Phase 1: 解析エンジンの構築 (2026-04-05 - 2026-04-15)
- [ ] `FFT` (Fast Fourier Transform) の実装（または既存ライブラリの利用）。
- [ ] 指定した周波数範囲 (Hz) の強さを 0.0 - 1.0 の数値に変換するロジック。
- [ ] 「スムージング・アタック・リリース」パラメータ（オーディオの変化に緩急をつける）の実装。

### Phase 2: プロパティ・バインディング UI (2026-04-16 - 2026-04-25)
- [ ] `Audio Link` ボタンを Inspector の各プロパティに追加。
- [ ] 周波数帯域（Low/Mid/High）と 振幅 (Amplitude) の選択 UI。
- [ ] 既存の `Expression Engine` と連携し、オーディオの値を数式でさらに加工できる仕組み。

### Phase 3: ビジュアライゼーション & UX (2026-04-26 - 2026-05-10)
- [ ] `Timeline Waveform Renderer`: タイムライン上に波形をマルチスレッド（TBB）で生成・再利用。
- [ ] オーディオ駆動のパーティクルやエフェクト・プリセットの作成。
- [ ] レンダリング時のオーディオ・ビデオ同期精度の最終調整。

## 🚀 期待される成果
- モーショングラフィックスにおける「音に合わせた動き」の制作時間を大幅に短縮できる。
- 複雑なキーフレームを打たずに、ダイナミックで音楽的なアニメーションが自動生成される。

## 🔗 関連マイルストーン
- [M-AU-3 Audio Visualization](MILESTONE_AUDIO_WAVEFORM_2026-03-29.md)
- [M-AU-5 Audio Playback Stabilization](MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md)

## 2026-07-25 現状確認

解析エンジンは `ArtifactCore::AudioAnalyzer` に実装済みで、Hamming 窓付き Radix-2 FFT、RMS／Peak、Low／Mid／High 強度を返す。`ArtifactAbstractComposition::applyAudioAnalysis()` は amplitude／peak／帯域値を外部制御と Audio Reactive Binding に渡し、binding ごとの attack／release smoothing、gain／offset／invert／clamp を適用する。Animation メニューには binding の設定・削除・Preview・Bake・録音操作があり、binding は JSON 化対象になっている。Particle 側の audio spectrum 入力と Viewer の解析表示も存在する。

ただし、再生サービスから解析を継続供給する専用 `AudioFFTService`、Inspector の常設 Audio Link UI、明示的なプリセットライブラリ、実時間更新負荷、書き出し時の音画同期は静的確認だけでは完了を保証できない。したがって「FFT／バインディング基盤と操作導線は実装済み、専用サービス化と実運用検証は未完了」と判定する。

## 2026-08-15 現行コード監査

- `AudioAnalyzer` の Hamming window / Radix-2 FFT、RMS/Peak、Low/Mid/High を現行コードで確認した。
- `ArtifactAbstractComposition::applyAudioAnalysis()` は binding ごとの attack/release、gain/offset/invert/clamp を適用し、Animation メニューから設定・削除・Preview・Bake・録音を行える。JSON 保存復元も存在する。
- Particle の audio spectrum input、Expression Evaluator の audio data、Viewer の解析表示も確認できるため、当初の「解析・binding 未実装」という Phase 1/2 の記述は更新が必要である。
- 一方、再生サービスから継続供給する専用 `AudioFFTService`、Inspector の常設 Audio Link UI、共通プリセットライブラリ、長時間更新負荷、書き出し時の音画同期は未確認。
- よって FFT／binding の実装基盤は完了扱い、専用サービス化と実運用検証は未完了とする。

## Update 2026-08-15

`AudioAnalyzer` の Hamming window／Radix-2 FFT、RMS／Peak、Low／Mid／High解析、`applyAudioAnalysis()` の attack／release・gain／offset／invert／clamp、Animation menu の設定／Preview／Bake／録音、JSON保存復元、Particle／Expression／Viewer接続を現行コードで確認した。FFTとAudio Reactive bindingの基盤は実装済みとして扱う。

未完了・未確認なのは、再生サービスから継続供給する専用 `AudioFFTService`、Inspectorの常設Audio Link UI、共通プリセット、長時間更新負荷、書き出し時の音画同期、実時間runtime受入である。

`M-AU-6` の次スライスを追加確認した。`AudioAnalyzer` と `ArtifactAbstractComposition::applyAudioAnalysis()` は個別解析・binding 適用 API として存在する一方、現行コードには再生ループから解析結果を継続供給する `AudioFFTService` がなく、`applyAudioAnalysis()` の runtime 呼び出しも確認できない。再生スレッドから直接 composition／UI を更新する実装は責務混在と競合リスクがあるため、専用サービス境界、フレーム／sample 時刻付き解析スナップショット、main-thread 側の binding 適用経路を次の実装単位とする。

順序監査では、`ArtifactPlaybackEngine` から現状提供されるのは level callback（RMS／peak）で、band analysis の snapshot 契約はまだ存在しないことも確認した。したがってこの段階で `applyAudioAnalysis()` を playback callback に直接接続する変更は行わず、FFT snapshot の所有者と適用タイミングを先に定義する。
