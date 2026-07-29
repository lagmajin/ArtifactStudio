# マイルストーン: Audio Mixer Surface Phase 2

**作成日:** 2026-05-25  
**ステータス:** Partial（Phase 1 mixer surface 基礎実装済み、routing／waveform／永続化と runtime 検証待ち、静的確認 2026-07-29）  
**優先度:** 高  
**関連:** `docs/planned/MILESTONE_AUDIO_WIDGET_ENHANCEMENT_2026-04-09.md`, `Artifact\docs\MILESTONE_AUDIO_BUS_ROUTING_UI_2026-04-09.md`, `Artifact\docs\MILESTONE_AUDIO_ENGINE_2026-03.md`

---

## 概要

Audio の基盤はある程度揃っているが、UI surface はまだ「状態確認」と「操作導線」が分離しすぎている。
このフェーズでは、mixer / waveform / routing / playback の見え方をまとめ、音声まわりを「探す UI」ではなく「その場で完結する UI」に寄せる。

---

## Phase 1: Mixer State Cohesion

**目標:** 音声レイヤー・バス・再生状態の見え方を統一する。

- [ ] mute / solo / volume / pan の表示を揃える
- [ ] layer state と bus state の文言を統一する
- [ ] missing / unloaded / muted / clipped の区別を明確にする
- [ ] timeline row と mixer surface で同じ意味の badge を使う

## Phase 2: Routing Surface Expansion

**目標:** bus routing を混乱なく編集できる UI を用意する。

- [ ] send / return / master の関係を見える化する
- [ ] route target の選択と解除を UI から行えるようにする
- [ ] bus の順序と選択状態を追いやすくする
- [ ] 既存 mixer layout を壊さず advanced routing を足す

## Phase 3: Waveform / Meter Presentation

**目標:** 音の状態を、再生せずとも視認しやすくする。

- [ ] waveform preview を mixer surface に寄せる
- [ ] level meter / clip indicator を追加する
- [ ] playback head と waveform の同期表示を整える
- [ ] 小さいサイズでも最低限の状態が読めるようにする

## Phase 4: Persistence and Safety

**目標:** ルーティングと mixer state を reopen / undo で失わないようにする。

- [ ] routing topology の保存/復元
- [ ] invalid bus target の検出
- [ ] playback callback に直接触れない安全な同期経路
- [ ] 編集後に state refresh が自然に追従すること

---

## 完了条件

1. Audio 状態が mixer / timeline / inspector で同じ意味で読める
2. bus routing の基本操作が UI だけで完了する
3. waveform / meter / clip state が見える
4. 保存して再読込しても routing が壊れない
5. 既存 playback 安定性を落とさない

## 2026-07-25 静的確認

現状は Phase 1 の基礎部分（channel/master の volume・pan・mute・solo、レベル／ピーク表示、Audio Layer と Core bus の同期）が実装済み。Audio Mixer Widget には FX chip と Master／Stereo Out の表示もある。

Phase 2 の send/return・route target 編集・bus topology の永続化、Phase 3 の mixer 内 waveform／専用 clip indicator、Phase 4 の routing 保存復元・invalid target 検出は未実装または未検証。したがって本マイルストーンは「基礎 mixer surface は導入済み、routing 拡張と安全性検証は未完了」と判定する。
