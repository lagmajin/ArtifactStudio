# DAW-Style Input Surface Milestone

Date: 2026-04-08

## Goal

タイムライン編集を、DAW のような「リアルタイム入力」と「ステップ入力」の両方で扱えるようにする。

狙いは次の 3 つ。

- 再生しながら入力しても、あとで小さく修正しやすい
- 停止中でも、フレーム単位で確実に入力できる
- 入力モードの違いを UI 上で分かりやすく保つ

## Scope

- `ArtifactTimelineWidget`
- `ArtifactLayerPanelWidget`
- `ArtifactCompositionEditor`
- `ArtifactPlaybackEngine`
- `ArtifactAudioMixerWidget`
- `ArtifactPropertyWidget`
- `ArtifactInspectorWidget`
- input / transport / step entry / live capture に関わる service 層

## Current Status

- Core 側の入力モード基盤として `InputSurfaceManager` を実装済み
- `InputSurfaceStateChangedEvent` を `EventBus` に流せるようにした
- リアルタイム入力 / ステップ入力 / armed / pending の状態を一元化した

## Non-Goals

- MIDI / OSC / 外部コントローラの完全対応
- 本格的なシーケンサー化
- グリッド / quantize の高度な音楽制作機能を先に作ること
- 低レベルの render backend や Diligent / DX12 の変更

## Canonical Model

この milestone では、入力を 2 系統に分ける。

- `Real-time Input`
  再生やライブ操作に追従して、その場で状態を変える入力。
- `Step Input`
  再生を止めた状態でも、1 フレームずつ確定して積み上げる入力。

どちらも「同じ property / layer / keyframe model に書く」が、入力の確定タイミングだけが違う。

## Milestones

### M-DAW-1 Input Mode Contract

目的:
リアルタイム入力とステップ入力の違いを、UI と service の両方で明確にする。

実装方針:

- input mode を明示的な state として持つ
- 現在の mode を timeline / inspector / transport から同じ値で参照する
- mode ごとの disabled / warning / confirm を整理する
- `ArtifactTimelineWidget` と `ArtifactInspectorWidget` の表示を揃える

Done:

- いま何モードなのかが見える
- mode ごとの操作可否が曖昧にならない
- 既存の property 編集と競合しない

### M-DAW-2 Real-time Capture Path

目的:
再生しながら入力した値を、そのままライブで反映できる導線を作る。

実装方針:

- transport 再生中に入力された変更を live preview に流す
- 必要なら記録前の `pending` 状態を挟む
- on-the-fly の変更が playback / preview / inspector に同時反映されるようにする
- 入力が重い場合は throttling / debounce を設ける

Done:

- 再生しながら触っても追従が分かりやすい
- ライブ操作が UI を壊しにくい
- 入力の遅延や引っかかりが可視化できる

### M-DAW-3 Step Input Path

目的:
再生を止めていても、フレーム単位で確実に値を置ける入力経路を作る。

実装方針:

- 現在フレームを基準に value を確定する
- 1 step ごとの確定と複数 step の連続入力を分ける
- keyframe / property / layer のどこに書いたかを明示する
- `Shift` / `Ctrl` などの modifier で挙動差を作る

Done:

- 停止状態でも制作が進む
- 1 フレーム単位の修正がしやすい
- 途中入力のやり直しが分かりやすい

### M-DAW-4 Transport And Feedback Integration

目的:
再生・停止・シーク・入力確定の関係を、DAW っぽく一貫させる。

実装方針:

- transport 状態に応じて入力 UI を切り替える
- playhead と入力中の値がズレないようにする
- live capture 時の feedback を timeline / inspector に返す
- 必要なら recording / armed 状態の表示を追加する

Done:

- 再生中と停止中の UI 差が自然になる
- 入力の成功 / 保留 / 未確定が見える
- playhead と入力結果の整合が取れる

### M-DAW-5 Validation And Regression Checklist

目的:
入力モード導入で壊れやすい箇所を先に洗い出す。

確認項目:

- 再生中の入力が UI を固めない
- 停止中の step input が 1 フレーム単位で確定する
- mode 切り替えで既存の keyframe が壊れない
- inspector と timeline で同じ値を見ている
- playback / scrub / input の競合がない

## Suggested Order

1. `M-DAW-1 Input Mode Contract`
2. `M-DAW-3 Step Input Path`
3. `M-DAW-2 Real-time Capture Path`
4. `M-DAW-4 Transport And Feedback Integration`
5. `M-DAW-5 Validation And Regression Checklist`

## Risks

- リアルタイム入力を急ぎすぎると、既存の playback / inspector / undo と競合しやすい
- step input を曖昧にすると、単なる別ショートカット入力になってしまう
- まず入力モードを明確にしないと、UI の見た目だけ DAW 化して中身が混ざる

## Validation Checklist

- 再生中に入力してもフリーズしない
- 停止中に step input で frame 単位の値が入る
- mode の切り替えで入力対象が変わることが分かる
- playback 開始時に live input の pending 状態が残らない
- undo / redo で step input の結果が戻せる
