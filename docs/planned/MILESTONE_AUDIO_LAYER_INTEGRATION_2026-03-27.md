# マイルストーン: オーディオレイヤー統合

> 2026-03-27 作成

## 現状サマリー

`ArtifactAudioLayer` は既に存在し、`loadFromPath()` / `volume` / `mute` / `hasAudio()` / `getAudio()` まで持っている。
つまり「音声を layer として保持し、composition 再生へ流す」ための土台はある。

一方で、現状のオーディオレイヤーは `ArtifactVideoLayer` ほど UI / timeline / project presentation と密に結びついていない。
プロパティ編集、視覚的な状態把握、再生同期、サムネイルや waveform 表示、エラー状態の説明が弱い。

このマイルストーンは、`MILESTONE_AUDIO_ENGINE_2026-03.md` が扱う再生基盤とは分けて、**Audio Layer を composition / timeline / inspector に自然に載せること** に絞る。

`MILESTONE_FEATURE_EXPANSION_2026-03-25.md` では Phase 2 の Audio Production に対応する詳細ワークストリームとして扱う。
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
