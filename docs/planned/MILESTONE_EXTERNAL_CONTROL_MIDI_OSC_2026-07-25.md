# MILESTONE_EXTERNAL_CONTROL_MIDI_OSC_2026-07-25

**ステータス:** ✅ Complete (3/3)
**対象:** `ArtifactCore/include/Control/MidiInput.ixx`, `ArtifactCore/src/Control/MidiInput.cppm`, `ArtifactCore/include/Control/OscInput.ixx`, `ArtifactCore/src/Control/OscInput.cppm`
**位置づけ:** ExternalControlManager の MIDI/OSC 入力バックエンドを実装。
**作成日:** 2026-07-25

## 1. 目的

ExternalControlManager はアドレスベースのマッピングと値変換パイプラインは完備しているが、MIDI デバイス入力と OSC ネットワーク入力のバックエンドが欠落していた。これらを追加し、実際のハードウェア/ソフトウェアから値を受信できるようにする。

## 2. 現状 (2026-07-25)

| 要素 | 状態 |
|------|------|
| ControlTarget / InputValueTransform / ExternalControlMapping | ✅ 完備 |
| ExternalControlManager シングルトン | ✅ 完備 (observeInput, processIncomingValue, getMappingDefinition) |
| ArtifactAbstractComposition::applyExternalControlValue() | ✅ 完備 (property set + live recording) |
| applyAudioAnalysis() による合成アドレス | ✅ 完備 (audio.amplitude, audio.low, etc.) |
| MIDI デバイス入力 | ❌ "midi:1:1" はコメントのみ、実装ゼロ |
| OSC ネットワーク入力 | ❌ "osc:/path/to/param" はコメントのみ、実装ゼロ |

## 3. 実装内容

### MidiInput (ArtifactCore)

`Control.Midi.Input` module:
- WinMM API (`midiInOpen`/`midiInStart`/`midiInClose`) 使用 — 追加パッケージ不要
- デバイス列挙: `enumerateDevices()` → `midiInGetNumDevs()` + `midiInGetDevCapsW()`
- コールバック: `MidiInProc()` で CC (0xB0) / Note On (0x90) を検出
- スレッドセーフなキュー + QTimer (16ms) でコールバックスレッド→メインスレッドにディスパッチ
- シグナル: `ccReceived(channel, controller, value)`, `noteOnReceived(channel, note, velocity)`

### OscInput (ArtifactCore)

`Control.OSC.Input` module:
- QUdpSocket 使用 — 追加パッケージ不要
- `startServer(port)` / `stopServer()` / `isRunning()` / `port()`
- 最小限の OSC 1.0 パーサー:
  - アドレス文字列 (`/` で開始)
  - タイプタグ (`,` で開始、`f`/`i` 対応)
  - ビッグエンディアン float/int32 値読み取り
- シグナル: `messageReceived(address, value)`

## 4. 変更ファイル

| ファイル | 変更 |
|----------|------|
| `ArtifactCore/include/Control/MidiInput.ixx` | 新規 (~60行) |
| `ArtifactCore/src/Control/MidiInput.cppm` | 新規 (~170行) |
| `ArtifactCore/include/Control/OscInput.ixx` | 新規 (~50行) |
| `ArtifactCore/src/Control/OscInput.cppm` | 新規 (~150行) |

## 5. 統合パス

両方のシグナルを Artifact レイヤーで `ExternalControlManager::observeInput()` に接続する例:

```cpp
// MIDI → ExternalControl
QObject::connect(midiInput, &MidiInput::ccReceived, [](int ch, int cc, int val) {
    QString addr = QString("midi:%1:%2").arg(ch).arg(cc);
    double normalized = val / 127.0;
    ExternalControlManager::instance().observeInput(addr, normalized);
});

// OSC → ExternalControl
QObject::connect(oscInput, &OscInput::messageReceived, [](const QString& path, float val) {
    ExternalControlManager::instance().observeInput("osc:" + path, val);
});
```

## 6. 残タスク

- [ ] Artifact レイヤーでの統合 (Widgets/Services からの MidiInput/OscInput 生成)
- [ ] Note Off メッセージのハンドリング追加 (必要なら)
- [ ] OSC バンドル (タイムタグ付き複数メッセージ) 対応
- [ ] OSC 文字列/Blob タイプ対応 (現状は float/int のみ)
- [ ] macOS CoreMIDI / Linux ALSA バックエンド (現状は WinMM のみ)
