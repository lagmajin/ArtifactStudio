> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md](MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md)

# マイルストーン: オーディオミキサー 機能監査 (2026-07-04)

**最終更新:** 2026-08-15

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

## 2026-07-25 現状確認

設計監査時点から実装が進み、P0 の主要項目は次の経路で存在する。`ArtifactAudioMixer` は Audio Layer と Core `AudioMixer` の bus を同期し、channel strip に volume / pan / mute / solo を保持する。`ArtifactCompositionAudioMixerWidget` は channel/master 行、パン操作、ミュート／ソロ、左右レベル／ピークメーター、マスター操作を提供する。ソロ時の他トラック自動ミュートも `updateSoloStates()` で処理される。

一方、監査表の P1/P2（send/return、bus routing UI、VU/RMS 切替、clip hold／明示的 clip indicator、track name 編集、EQ／compressor／automation mode／VST／sidechain／normalize 等）は未実装、またはこの静的確認だけでは動作保証できない。したがって本マイルストーンは「P0 基盤実装済み、DAW 高度機能は未着手」の状態と判定する。実機再生時のメーター追従、設定の永続化、複数レイヤーの solo/mute と書き出し経路の一致は未検証。

## Update 2026-08-15 — 現行コード照合

`ArtifactCompositionAudioMixerWidget` の owner-draw メーターに 0 dBFS 以上を示す赤い clip インジケーターを確認した。P0 の静的 UI 基盤は、旧監査表の Clip Indicator ❌ から実装済み相当へ更新する。

send/return、bus topology の永続化、VU/RMS 切替、track name 編集、実機再生時のメーター追従、複数 layer の solo/mute と書き出し経路の一致は未完了または未検証である。
