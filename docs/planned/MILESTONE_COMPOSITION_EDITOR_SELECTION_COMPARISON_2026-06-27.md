# Milestone: Composition Editor Selection / Comparison Upgrade (2026-06-27)

**Status:** Planning
**Goal:** コンポジットエディタのビューポート上で、複数選択と A/B 比較を素早く行えるようにする。

---

## ねらい

コンポジット編集では、対象を「選ぶ」操作と、結果を「見比べる」操作の往復が多い。
この往復が遅いと、編集そのものより確認作業が重く感じられる。

このマイルストーンでは、ビューポートを単なる表示面ではなく、
選択と比較の中心導線として使えるようにする。

---

## 現状の課題

- 単体クリック中心だと、複数レイヤーの選択に手数がかかる
- 選択モードが見えにくいと、追加選択や除外選択で迷いやすい
- A/B 比較の導線が弱いと、変更前後の確認に時間がかかる
- 参照フレーム固定がないと、基準の見失いが起きやすい
- 差分確認が別画面寄りだと、編集の流れが切れやすい

---

## 改善方針

### Phase 1: 範囲選択基盤

- 矩形選択を追加する
- `Shift` で追加選択を行えるようにする
- `Alt` で除外選択を行えるようにする
- 現在の選択モードを HUD で明示する

### Phase 2: 比較導線

- A/B 切替をビューポート内で行えるようにする
- 参照フレームを固定できるようにする
- 差分オーバーレイを HUD から切り替えられるようにする

### Phase 3: 選択方式拡張

- ラッソ選択を追加する
- 矩形選択と同じ選択モード体系に載せる
- 選択結果のハイライトをビューポートで分かりやすくする

### Phase 4: 操作の読みやすさ

- 選択数、比較モード、参照フレームの状態を簡潔に表示する
- 解除操作やモード終了を迷いにくくする
- コンテキストメニューから比較関連のクイック操作へ入れるようにする

---

## 実装の着手候補

1. 矩形選択と追加 / 除外選択を実装する
2. A/B 切替の HUD 表示を追加する
3. 参照フレーム固定の状態を持たせる
4. 差分オーバーレイの切替を追加する
5. ラッソ選択を追加する

---

## 成功条件

- ビューポートだけで複数選択の主要操作が完結する
- A/B 切替と参照固定が少ない操作数で行える
- 比較状態が画面上で迷わず読める
- 選択と比較の往復が速くなる

---

## リスク

- 選択モードが増えすぎると、かえって迷いやすくなる
- 比較系の HUD を増やしすぎると、編集対象の視認性が落ちる
- 選択ロジックと比較ロジックを混ぜると、責務境界が崩れやすい

---

## 参照

- [`COMPOSITION_EDITOR_CONTRACT.md`](x:/Dev/ArtifactStudio/docs/COMPOSITION_EDITOR_CONTRACT.md)
- [`WIDGET_MAP.md`](x:/Dev/ArtifactStudio/docs/WIDGET_MAP.md)
- [`MILESTONE_COMPOSITION_EDITOR_RUBBER_BAND_MULTI_SELECTION_2026-03-26.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_RUBBER_BAND_MULTI_SELECTION_2026-03-26.md)
- [`MILESTONE_COMPOSITION_EDITOR_PLAYBACK_FEEL_REFINEMENT_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_PLAYBACK_FEEL_REFINEMENT_2026-04-23.md)

---

## 次の一手

1. 選択モードの UI 文言を確定する
2. A/B 比較の状態表現を決める
3. 参照フレーム固定の操作導線を決める
