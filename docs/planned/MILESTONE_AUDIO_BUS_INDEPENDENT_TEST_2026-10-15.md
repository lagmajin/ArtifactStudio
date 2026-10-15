# Artifact Audio Bus 独立テストビューア

**最終更新:** 2026-10-15
**ステータス:** In Progress

## 目的

FBX アニメーション独立 Viewer と同じ流れで、Artifact 本体を起動せずに本番の `Artifact::AudioMixerWidget` と `ArtifactCore::AudioMixer` の bus UI、routing、meter 表示を確認できる専用実行ファイルを用意する。読み取り専用の graph summary でprimary route、send、VCA membershipを表示し、routing編集後も追従させる。合成入力は offline で処理し、オーディオデバイスや再生サービスは起動しない。

## テスト内容

- Master、Dialogue／Music Group、ネストしたStem Bus、FX Bed→Compressor付きFX Return、重ねて適用するDialogue VCA、4つのlayer busを持つ固定グラフを表示する。Dialogue／MusicのGroupはStem Busへ集約し、Masterへ送る。
- DialogueのPre-Fader sendとMusicのPost-Fader sendをFX Returnのcompressor detectorへ合流させ、FX Bedのmain signalをduckする。合成sidechain値、FX Returnのgain reduction、Masterまでの出力寄与を48 kHz stereoのofflineブロックで測る。
- `audio.runContract` が nested primary routing、cycle rejection、削除したReturnを参照するroute／sendのcleanup、複数VCAのmember削除、Pre/Post-Fader sidechain、compressorのsidechain detector選択とbypass、複数VCAのゲイン乗算、Group pan、bus／Master mute、Master gain、solo、JSON保存・復元後の信号を確認する。
- `audio.setBus`、`audio.setRoute`、`audio.addSidechainSend`、`audio.removeSidechainSend`、`audio.setVcaMember` はグラフを変更し、production mixer widgetとgraph summaryをrefreshする。`audio.state` からbus種別、route、send、VCA membership、control、meter値を読める。
- `tools/test_audio_bus_mcp.py` は独立プロセスを起動し、固定ケースのRMS、send追加／更新／解除後のsidechain入力変化と設定復元、VCA付け替え後のmain／Post-Fader sendゲイン、route summary、ミュート、cycle拒否、ツール一覧を確認してfixtureを元に戻す。

既定入力ではFX Bedを0にしてprimary mix `(0.35, 0.35)` を確認する。FX Bedを0.20にすると、Return compressorは合成sidechain `(0.20 × 0.25) + (0.10 × 0.5) = 0.10` をdetectorに使い、FX Return RMSは0と0.20の間になる。そのReturn出力がMaster mixへ加算されること、compressor bypass時にReturn RMSが0.20へ戻ることを確認する。Pre-Fader／Post-Faderの個別期待値は、Dialogue fader変更後も合算0.10、Music faderを約-6.0206 dBにした場合は合算0.075。

FX Bedを除くrouting controlの期待値は、Dialogue Groupを右パンにした状態で `(0.15, 0.35)`、Dialogue VCAを約-6.0206 dBにした状態で左0.25、DialogueのVCAを2つとも同値にした状態で左0.20、Dialogue muteで左0.15、Music soloで左0.10、Masterを約-6.0206 dBにした状態で左0.175、Master muteで `(0, 0)`。

## ビルドと起動

```powershell
cmake -S . -B out/build/audio-bus -DARTIFACT_BUILD_AUDIO_BUS_VIEWER=ON
cmake --build out/build/audio-bus --target ArtifactAudioBusViewer
python tools/test_audio_bus_mcp.py --exe out/build/audio-bus/bin/Debug/ArtifactAudioBusViewer.exe
```

上記のパスは Debug multi-config generator の例。Windows では post-build で Qt platform plugin と vcpkg runtime DLL を実行ファイル隣へ配置する。目視用には `ArtifactAudioBusViewer.exe` を直接起動する。MCP接続を使う場合は `--mcp-stdio` を付ける。ヘッドレス環境では runner に `--offscreen` を追加する。

## 範囲外・未確認

- `ArtifactCompositionAudioMixerWidget` の composition切替、Undo、プロジェクト保存・再読込までは対象にしない。Viewerは Core mixer の routing graphとproduction `AudioMixerWidget` を直接確認する。
- OSオーディオデバイス、Playback Service、実時間再生、外部プラグインの音声出力は使わない。
- ビルド、MCP実行、画面キャプチャによる確認はまだ行っていない。
