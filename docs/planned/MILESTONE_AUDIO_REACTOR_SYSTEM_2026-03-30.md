# Milestone: Audio Reactor System (M-AU-6)

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
