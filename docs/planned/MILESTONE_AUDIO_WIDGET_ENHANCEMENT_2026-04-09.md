# マイルストーン: Audio Widget Enhancement / Mixer Surface

作成日: 2026-04-09
対象: `ArtifactCompositionAudioMixerWidget` / `ArtifactInspectorWidget` / `ArtifactTimelineWidget`
状態: ✅ 企画追加

---

## ■ 目的

音声の基盤実装とは別に、ユーザーが音を「見て、触って、迷わず確認できる」UI surface を整える。

このマイルストーンは `Audio Pipeline` の再設計ではなく、
`ArtifactCompositionAudioMixerWidget` を中心にした mixer / waveform / state presentation の強化に絞る。

---

## ■ 何を強くするか

- mute / solo / volume / pan の見え方
- layer / bus / playback state の一貫表示
- waveform と meter の視認性
- missing / unloaded / muted / clipped の区別
- timeline と inspector からの audio 操作導線

---

## ■ Phase 1: 状態の可視化

1. `ArtifactCompositionAudioMixerWidget` に現在状態の要約を出す
2. `ArtifactInspectorWidget` から audio layer の状態を自然に読めるようにする
3. `ArtifactTimelineWidget` の layer row へ audio state badge を揃える
4. missing / unloaded / muted / solo の色と文言を統一する

### 期待効果

- 音が出ているか、止まっているか、素材が欠けているかが一目で分かる
- どの UI から触っても同じ意味で見える

---

## ■ Phase 2: 可視化の強化

1. waveform preview を mixer surface に埋め込む
2. level meter / clip indicator を追加する
3. playback head と waveform の同期表示を整える
4. hover / selection / solo state に応じて表示密度を切り替える

### 期待効果

- 音声の確認が、別ビューを開かなくてもできる
- 長い素材でも状態把握が速くなる

---

## ■ Phase 3: 操作導線の整理

1. mute / solo / volume / pan の操作を短い導線へまとめる
2. timeline から mixer への jump / reveal を揃える
3. audio layer の source 差し替えを迷わない導線にする
4. keyboard / context menu / drag 操作を整理する

### 期待効果

- 音声まわりの操作が「探す UI」ではなく「その場で完結する UI」になる

---

## ■ Non-goals

- audio backend の全面再設計
- sample rate conversion の大規模実装
- mux / export の工程変更
- `Audio Mixer` の DSP 中核の書き換え

---

## ■ 関連ドキュメント

- [`docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md)
- [`docs/planned/MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md)
- [`docs/planned/MILESTONE_AUDIO_WAVEFORM_2026-03-29.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_AUDIO_WAVEFORM_2026-03-29.md)
- [`docs/planned/MILESTONE_AUDIO_WAVEFORM_THUMBNAIL_PREVIEW_2026-03-31.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_AUDIO_WAVEFORM_THUMBNAIL_PREVIEW_2026-03-31.md)

