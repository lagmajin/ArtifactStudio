# マイルストーン: オーディオレイヤー統合

**最終更新:** 2026-08-15
ステータス: Phase 1〜4 主要経路実装済み（runtime parity・異常系 UX 検証待ち、静的確認 2026-07-29）

> 2026-03-27 作成

## 現状サマリー

`ArtifactAudioLayer` は既に存在し、`loadFromPath()` / `volume` / `mute` / `hasAudio()` / `getAudio()` まで持っている。
つまり「音声を layer として保持し、composition 再生へ流す」ための土台はある。

現行コードでは AudioService の layer bus、Timeline／Inspector／Composition Audio Mixer、再生 engine、waveform 基盤まで接続されている。したがって、作成時点の「UI / timeline / project presentation との結びつきが弱い」という説明は更新が必要である。残るのは missing／decode failure の一体的な表示、source 差し替え・relink の受入、形式別 waveform／clip 診断、runtime parity である。

このマイルストーンは、`MILESTONE_AUDIO_ENGINE_2026-03.md` が扱う再生基盤とは分けて、**Audio Layer を composition / timeline / inspector に自然に載せること** に絞る。

`MILESTONE_FEATURE_EXPANSION_2026-03-25.md` では Phase 2 の Audio Production に対応する詳細ワークストリームとして扱う。

## Update 2026-08-15

`ArtifactAudioLayer` の source／volume／mute／audio payload、`ArtifactAudioService` の layer bus、composition の audio evaluation、Playback Engine の master volume／mute、Timeline／Inspector／Audio Mixer の表示・操作、`ArtifactAudioWaveform` の waveform summary を現行コードで確認した。基本的な Audio Layer の保持・編集・再生・可視化経路は実装済みとする。

未完了または未確認なのは、missing／unloaded／decode failure の UI 統一、source 差し替え／relink の全導線、波形と clip／duration／sample rate／channels 診断の接続、mute／solo／active state の runtime parity、Undo／保存再読込を含む実ファイル受入である。
Feature Expansion 側で「音声を制作能力として増やす」と定義し、本書では layer presentation と workflow 接続を詰める。

---

## Scope

- `Artifact/src/Layer/ArtifactAudioLayer.cppm`
- `Artifact/include/Layer/ArtifactAudioLayer.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- 必要に応じて `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 必要に応じて `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm`

## Non-Goals

- Audio mixer 本体の全面実装
- system audio device の下位実装刷新
- video/audio 同期エンジンの全面再設計
- waveform の DSP 実装をここで完結させること

## Background

今の `ArtifactAudioLayer` は、`sourcePath` と `volume` と `mute` を持つ軽量な layer になっている。
`getAudio()` も実装されており、少なくとも playback engine 側から PCM を供給できる状態に近い。

ただし、UI から見ると audio layer はまだ「触れるが、見え方が弱い」。
レイヤーヘッダでの可視化、プロパティ面での source / volume / mute の分かりやすさ、再生中の状態表示、missing / unloaded / muted の区別が薄い。

---

## Phase 1: Property / Presentation Sync

- 目的:
  - Audio Layer の基本状態を inspector と timeline で一貫表示する

- 作業項目:
  - `sourcePath` / `volume` / `muted` の表示整理
  - `isLoaded` / `hasAudio` / `missing source` の区別を UI に出す
  - layer header で audio state が一目で分かるようにする
  - project view 側で audio asset と layer の関係を追えるようにする

- 完了条件:
  - Audio Layer の source / mute / volume が明確に見える
  - 未ロード / ミッシング / ミュートの違いが追える

- 進捗メモ:
  - 2026-03-27: layer panel に audio state chip を追加し、Muted / Volume を見える化
  - 2026-03-27: 既存の audio icon 表示とあわせて、layer row 上で audio layer を識別しやすくした

## Phase 2: Timeline / Playback Integration

- 目的:
  - Audio Layer を timeline / playback の状態表示へ接続する

- 作業項目:
  - layer panel の audio toggle と実動作の同期
  - playback control との state sync
  - solo / mute / current layer の見え方を audio layer でも統一
  - 再生中の audio active 状態を UI へ反映

- 完了条件:
  - Audio Layer を再生対象として把握できる
  - mute / solo / active state の UX が他 layer と揃う

- 進捗メモ:
  - 2026-03-27: playback engine 側で audio を先読みバッファに積むようにして、再生時の underrun を減らす方向に入った
  - 2026-03-27: renderer の buffer 水位を見て供給を継続する形に寄せた
  - 2026-03-27: timeline 左ペインの layer row に再生中の audio output indicator を追加し、音が出る layer を green blink で示すようにした
  - 2026-03-27: indicator は icon 列と重ならないよう左端の color bar 内に収めた

## Phase 3: Visualization / Diagnostics

- 目的:
  - 音声 layer を見た目で把握しやすくする

- 作業項目:
  - wave / peak / clip の簡易表示
  - mixer strip で 0dBFS 超過時に赤い `CLIP` 警告を表示
  - missing source / decode failure / empty source の表示
  - duration / sample rate / channels の表示整理
  - 音声レイヤーの種類アイコンや色分け

- 完了条件:
  - Audio Layer の中身が最低限見える
  - 異常系の切り分けがしやすい

- 進捗メモ:
  - 2026-03-27: mixer strip に 0dBFS 超過時の赤い `CLIP` 警告を追加

## Phase 4: Import / Relink / Workflow

- 目的:
  - audio source の追加・差し替えを実運用しやすくする

- 作業項目:
  - import から Audio Layer を作る導線
  - source 差し替え / relink の整理
  - clipboard / drag&drop / project browser 連携
  - undo / redo と組み合わせた安全な編集

- 完了条件:
  - Audio Layer の source 更新が迷わない
  - 参照切れ時の再接続ができる

---

## Recommended Order

1. Phase 1: Property / Presentation Sync
2. Phase 2: Timeline / Playback Integration
3. Phase 3: Visualization / Diagnostics
4. Phase 4: Import / Relink / Workflow

---

## Validation Checklist

- [ ] Audio Layer の source / volume / mute が inspector で見える
- [ ] layer header で audio state が分かる
- [ ] mute / solo / playback state が timeline と一致する
- [ ] missing / unloaded / muted の違いが表示される
- [ ] source 差し替えが project workflow に自然に繋がる

---

## Recommended Execution Order

1. Property / Presentation Sync
2. Timeline / Playback Integration
3. Visualization / Diagnostics
4. Import / Relink / Workflow

## 2026-07-25 実装監査

判定: Phase 1〜4 の主要な layer／timeline／playback／visualization／workflow 経路は実装済み。runtime の音声同期・異常系・全 UI 表示は未検証。

## 2026-08-15 現行コード監査

- `ArtifactAudioLayer` は volume / pan / mute、source asset identity、AudioCache、sample rate / channel / duration、waveform data/summary、PCM shared payload、JSON 保存復元を実装している。
- Timeline/Layer Panel 側には audio state、mute/volume、waveform／output indicator、mask 等と混同しない audio 表示経路があり、Playback Service は audio diagnostics と RAM/preview 経路を持つ。
- Localize / Relink Shared と source version drift 後の PCM／waveform／resample 切替も Asset Instance Sharing の共通経路に接続されている。
- ただし音声の実機同期、scrub 時の audio clock、decode failure／missing／empty source の UI 表現、複数 audio layer の solo/mute 混在は runtime 未検証。
- よって Phase 1〜4 の静的実装は進んでいるが、全体ステータスは runtime parity 待ちのままとする。

- `ArtifactAudioLayer` は source、volume、mute、audio payload、waveform summary、JSON 保存/復元を持ち、Timeline は audio icon、state、volume/mute、再生中 indicator を表示する。
- `ArtifactAudioService`、`ArtifactAudioMixer`、`ArtifactPlaybackEngine`、composition の `hasAudio`/active layer 集計が音声レイヤーを playback 経路へ接続している。
- Timeline track painter、Composition Render Controller、Contents Viewer、Asset Browser に waveform/preview 経路があり、clip／peak／RMS の表示基盤も存在する。Audio layer の追加、source 置換、relink、WorkspaceAutomation、Undo 経路も確認できる。
- missing / decode failure / empty source の全状態を inspector・timeline・project view で一貫表示すること、solo/mute の実音声挙動、複数 layer の mix、sample rate/channel 変換、再生中 indicator の実機同期は未確認。
- よって Audio Layer の workflow 接続は大きく進行済みだが、受け入れ条件の runtime parity と異常系 UX は検証待ち。

ビルド・実行確認はリポジトリ方針により未実施。

## Update 2026-08-15

現行コードを追加確認した。`ArtifactAudioLayer` は source／asset identity、volume／pan／mute、AudioCache、PCM payload、waveform summary、sample rate／channel／duration、JSON 保存復元を持つ。`ArtifactAudioService`／`ArtifactAudioMixer`／`ArtifactPlaybackEngine`、Timeline／Layer Panel、Composition の audio 集計、Asset Browser／Contents Viewer の waveform／preview、source 置換・relink・Undo・WorkspaceAutomation まで接続されている。

未完了・未検証なのは、音声 clock と scrub の同期、solo／mute の実音声挙動、複数 audio layer の mix、sample rate／channel 変換、missing／decode failure／empty source の全 UI 状態、再生中 indicator の実機一致である。Phase 1〜4 の静的基盤は実装済み、runtime parity と異常系 UX は pending とする。

Composition Audio Mixer の現行 owner-draw メーターには、左右レベル・ピーク線に加えて、左右いずれかのピークが 0 dBFS 以上になったとき赤いクリップインジケーターを表示する経路を追加した。既存の volume／pan／mute／solo／routing 同期と master メーターを壊さない範囲の診断表示であり、実機でのピーク保持時間や runtime parity は未検証とする。
