# マイルストーン: オーディオミキサー 機能監査 (2026-07-04)

> 2,291行。Logic Pro / Ableton / Pro Tools / Resolve Fairlight 比較。

## 監査サマリー

`ArtifactCompositionAudioMixerWidget` はコンポジションの音声レイヤー用ミキサー。

---

## 🔴 P0: 基本ミキサー機能

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Volume Fader（dB スライダー）** | DAW | ⚠️ |
| **Pan Knob（左右パン）** | DAW | ⚠️ |
| **Mute ボタン（トラック単位）** | DAW | ❌ |
| **Solo ボタン（トラック単位）** | DAW | ❌ |
| **Peak Meter（レベルメーター）** | DAW | ⚠️ |
| **Clip Indicator（赤警告）** | DAW | ❌ |
| **Track Name 編集** | DAW | ❌ |

---

## 🟡 P1: DAW 品質

| 機能 | 参照元 | 状態 |
|---|---|---|
| **VU Meter / RMS Meter 切替** | DAW | ❌ |
| **Stereo Balance vs Pan 切替** | Pro Tools | ❌ |
| **Send/Return（エフェクトセンド）** | DAW | ❌ |
| **Bus Routing（グループバス）** | DAW | ❌ |
| **Track Color 割り当て** | Logic/Ableton | ❌ |
| **Gain Staging 表示** | DAW | ❌ |
| **Phase Invert ボタン** | DAW | ❌ |
| **Track Freeze** | DAW | ❌ |

---

## 🔵 P2: 高度機能

| 機能 | 参照元 | 状態 |
|---|---|---|
| **EQ カーブ表示（インライン）** | Logic | ❌ |
| **Compressor Gain Reduction Meter** | DAW | ❌ |
| **Automation Read/Touch/Latch/Write モード** | DAW | ❌ |
| **Spectrum Analyzer 埋め込み** | Ableton | ⚠️ SpectrumAnalyzerWidget あり |
| **VST プラグインスロット** | DAW | ❌ |
| **Sidechain 入力選択** | DAW | ❌ |
| **DC Offset 除去** | Audacity | ❌ |
| **Normalize** | Audacity | ❌ |