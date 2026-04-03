# Timeline Operation Feel Refinement / AE-Style Editing Flow

`ArtifactTimelineWidget` とその周辺の操作感を、After Effects っぽい「迷わない編集導線」に寄せるための milestone です。

タイムラインは表示が増えるほど、選択・スクロール・ズーム・プロパティ編集・キーフレーム編集の境界が曖昧になりやすいです。  
この milestone では、**見た目の拡張よりも操作ルールの統一** を優先し、編集の往復を減らします。

## Goal

- KF 選択 / 範囲選択を強くし、ラバーバンドで複数対象を拾いやすくする
- Shift / Ctrl 修飾キーで選択意図を明確にする
- スクロールはパン、`Ctrl+Scroll` だけズームに統一する
- 原点復帰 / フィット復帰のショートカットを用意して迷子を減らす
- property と timeline を行単位で往復しやすくする
- 同一行に property 値をインライン表示し、必要ならその場で編集できるようにする
- 複数レイヤー選択時に keyframe を相対移動でまとめて動かせるようにする

## Scope

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScene.cppm`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 必要なら `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## Non-Goals

- Graph Editor の全面改修
- property tree の全面置換
- 全ショートカットの再設計
- keyframe interpolation の再定義

## Background

現在の timeline は、keyframe 編集そのものは進みつつある一方で、操作感がまだ分散しています。

- KF 選択が弱いと、複数 keyframe の編集対象が見えにくい
- スクロールとズームの境界が曖昧だと、長い timeline で迷子になりやすい
- property panel と timeline の往復が多いと、値の確認だけで視線コストが増える
- 複数レイヤー選択時の一括編集が弱いと、AE っぽい作業フローに乗りにくい

この milestone は、こうした「制作時の手の動き」を先に固めるためのものです。

## Phases

### Phase 1: Selection Strengthening

- 目的
  - KF 選択と範囲選択を強くする

- 実装の要点
  - ラバーバンド選択を timeline に導入する
  - Shift / Ctrl 修飾キーで追加・除外・範囲補助を整理する
  - keyframe / clip / layer の hit test を選択意図に合わせる

- DoD
  - 複数対象を迷わず拾える
  - クリックとドラッグの意図がぶれない

### Phase 2: Scroll / Zoom Discipline

- 目的
  - 視点操作を一定のルールに固定する

- 実装の要点
  - plain scroll = pan
  - `Ctrl+Scroll` = zoom
  - 原点復帰または表示復帰の shortcut を用意する
  - zoom 中も playhead / keyframe / clip の見失いを減らす

- DoD
  - スクロールとズームの役割が一目で分かる
  - 長い timeline でも現在位置を戻せる

### Phase 3: Inline Property Surface

- 目的
  - property ↔ timeline の往復コストを減らす

- 実装の要点
  - 同一行に property の代表値を出す
  - 値が編集可能なものは inline editor へ落とす
  - view-only の値と edit-only の値を混ぜない

- DoD
  - 確認のためだけに inspector へ戻る回数が減る
  - その場で触れる property が分かる

### Phase 4: Multi-Layer Keyframe Batch Editing

- 目的
  - 複数レイヤーの keyframe を一括で扱いやすくする

- 実装の要点
  - multiple selection 時に keyframe を相対移動できるようにする
  - 選択群のフレーム差を保ったまま batch move する
  - snap / collision / overlap の扱いを揃える

- DoD
  - 複数レイヤーのタイミングを同時に整えられる
  - 1 点ずつ修正しなくても制作フローに乗る

### Phase 5: Property Round-Trip Polish

- 目的
  - property と timeline の行き来を自然にする

- 実装の要点
  - timeline から property へ、property から timeline へ、相互に飛べる
  - selection の強調を両側で揃える
  - 編集対象の由来を status / header に残す

- DoD
  - どこを触っているか迷わない
  - timeline が「閲覧」と「編集」の両方に使える

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Notes

- この milestone は `M-TL-5 Timeline Keyframe Editing` と重なるが、こちらは「操作感」と「行き来のしやすさ」に寄せる
- `M-TL-10 Timeline Flat Keyframe View / U-Key Style Filter` と組み合わせると、編集対象の見通しがかなり上がる
- `M-TL-4` / `M-TL-8` の owner-draw / scene elimination が進んでいる前提で、入力周りの polish を積みやすい

## Progress

- Phase 1 has started in code:
  - keyframe marker selection now supports click modifiers and rectangle selection
  - selected keyframes are highlighted and can batch-move when multiple selected markers are dragged together
  - timeline clip selection now receives modifier state from the painter view
- Remaining Phase 1 polish:
  - rubber-band behavior for layer clips and keyframes can still be tuned
  - selection clearing and additive semantics may need a final UX pass
