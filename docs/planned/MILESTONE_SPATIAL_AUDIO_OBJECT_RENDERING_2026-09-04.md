# Spatial Audio Object Rendering Milestone

**最終更新:** 2026-09-04
**ステータス:** M-AU-9.1 implemented; M-AU-9.2 partial
**マイルストーン ID:** M-AU-9

## 目的

Dolby Atmos / Auro-3D と同種の「音源を空間内のオブジェクトとして配置する」制作体験を、ArtifactStudio 独自のライセンス非依存な空間音響機能として提供する。公式 Dolby Atmos / AURO コーデック、商標、SDK 互換を実装目標には含めない。

## 現状と境界

- `ArtifactPlaybackEngine` は PCM channel buffer、master volume/mute、audio clock、stereo RMS/peak を既に扱う。
- `M-AU-1` の mixer/routing と `M-AU-4` の Audio Layer を再実装せず、既存の source、mute、volume、pan、保存経路を拡張する。
- 2026-09-04時点で Audio Object の stable ID、spread、gain、mute、enabled と JSON 保存/復元を実装済み。既存の `SpatialRenderer` に最小ステレオ spread 処理を接続し、mono素材も stereo preview へ拡張する。
- `ReactiveEvents`、新規 signal/slot、QImage/QPainter 合成、Diligent の低レベル変更は対象外。
- 公式 Atmos ADM BWF / DAMF、Auro-Codec、Dolby/AURO SDK の encode/decode は別途ライセンス判断が必要な将来課題とする。

## フェーズ

### M-AU-9.1 Audio Object Contract

- `AudioObject` に stable ID、source layer/asset、position (x/y/z)、size/spread、gain、mute、solo、enabled を定義する。
- listener と座標系（右手系、距離単位、前方/up）を明文化する。
- JSON 保存・読込と旧 stereo/pan データからの後方互換変換を追加する。

### M-AU-9.2 Spatial Mixer Phase 1

- 既存 Playback/Mixer の stereo 出力に対し、距離減衰、azimuth/elevation、spread を適用する。
- 2ch headphone preview は HRTF の外部依存を増やさない最小 panning から開始し、CPU callback の allocation/lock を増やさない。
- mono/stereo source、mute/solo、seek/stop/restart、underflow 時の deterministic fallback を定義する。

### M-AU-9.3 Bed/Object Routing UI

- Audio Mixer に Bed / Object の区別、object list、position/spread/gain の編集を追加する。
- Inspector と Timeline で position/elevation/spread の keyframe を既存 command/Undo 経路へ接続する。
- 既存の pan/volume UI と責務を重複させず、object 編集は専用セクションに置く。

### M-AU-9.4 Speaker Layouts and Preview

- stereo、5.1、7.1、7.1.4 の layout descriptor と downmix/fallback を定義する。
- listener/camera preview と level/meter diagnostics を追加する。
- 30/60 fps、sample-rate、device clock drift、seek後の stale audio を受入条件にする。

### M-AU-9.5 Exchange and Optional Licensed Adapters

- 独自 JSON + WAV の交換形式を文書化する。
- ADM BWF import/export は仕様調査後に独立 adapter として判断する。
- Dolby/AURO SDK 統合はライセンス契約、配布条件、商標表示、認証要件を確認してから別 milestone 化する。

## 受入条件

- 1つ以上の Audio Layer を 3D 位置へ配置し、再生中に azimuth/elevation/距離の変化が音量・左右/スピーカー配分へ反映される。
- object の位置・spread・gain・mute が保存/再読込され、既存 stereo project が壊れない。
- stereo fallback は常に利用でき、未対応 layout は明示的に downmix される。
- audio callback に毎回の heap allocation と mutex 待ちを追加しない。
- Preview、Render Queue、Audio Mixer の責務が一致し、seek/stop/restart 後に古い object 音声を再生しない。
- Dolby Atmos / Auro-3D の公式互換を謳わず、独自仕様であることを UI/文書に明記する。

## リスクと次の確認

- 現行 mixer の callback 境界と channel layout 表現を確認してから `AudioObject` の所有場所を決める。
- HRTF は品質・ライセンス・CPU負荷の影響が大きいため、Phase 1 では単純 panning、Phase 4 以降で導入可否を評価する。
- 実装開始時は M-AU-9.1 の contract と JSON fixture を先に作り、再生経路を広げる前に後方互換を確認する。

## 関連

- `docs/planned/MILESTONES_BACKLOG.md` の M-AU-1 / M-AU-4 / M-AU-5
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`
- Dolby Atmos ADM Profile specification
- AURO-3D Professional Tools and licensing
