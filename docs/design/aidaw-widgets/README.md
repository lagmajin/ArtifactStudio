# AIDAW Track / Audio Mixer Widget Mockups

**最終更新:** 2026-09-09

## 画像

- `aidaw-track-widget-mockup-2026-09-09.png`
  - MIDI／オーディオ混在のArrangement画面
  - トラックごとのMute／Solo／Record／AI状態
  - MIDIクリップ、音声波形、AI提案の比較・再生成・採用導線
- `aidaw-audio-mixer-widget-mockup-2026-09-09.png`
  - MIDIトラック、音声トラック、Return、Masterのチャンネルストリップ
  - Inserts、Send、Pan、Mute／Solo／Record、Fader、Stereo meter
  - AIミックス提案のReview導線

## 採用方針

AIDAWの初期UI検討用コンセプトであり、そのまま実装仕様を確定するものではない。既存ArtifactStudioのチャコール背景、アンバーの選択色、高密度なDCC系配置を参照し、AI提案を既存編集操作から区別できるようにした。

AIによる変更は即時適用せず、原則としてPreview／Compare／Acceptを経由する。トラックの正本、Undo境界、MIDI routing、音声bus routingは実装時に既存サービスとの責務を確認する。

## 再生成

`generate_mockups.py` をPillowで実行すると、同じフォルダへ2枚のPNGを生成する。これは画像生成用スクリプトであり、アプリケーションのビルド処理ではない。
