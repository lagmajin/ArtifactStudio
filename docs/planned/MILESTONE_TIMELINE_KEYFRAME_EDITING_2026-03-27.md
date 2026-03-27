# Timeline Keyframe Editing Milestone

`ArtifactTimelineWidget` の右ペインと inspector 周辺をつなぎ、property keyframe を timeline 上で見て・触って・移動できるようにするためのマイルストーン。

この milestone は `AbstractProperty` を keyframe の source of truth とする前提に立ち、`Timeline` 側の編集面だけを切り出して整理する。

## Goal

- Timeline 上で keyframe の位置と対象 property を確認できる
- selected layer の keyframe を timeline から追加 / 削除 / 移動できる
- inspector の keyframe 操作と timeline の操作が同じ source を見る
- いま見ている frame と keyframe の関係がすぐ分かる

## Scope

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScene.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `ArtifactCore/src/Property/*`
- 必要に応じて `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## Non-Goals

- Graph Editor の全面実装
- easing / tangent の完全再現
- animation curve editing の全機能化
- `AnimatableTransform3D` の全面置換

## Background

既に `ArtifactCore::AbstractProperty` は keyframe の正として扱う方向が固まりつつある。
Inspector 側の keyframe 周辺は `Property / Keyframe 統合 実行計画` に整理されているが、timeline 側はまだ lane と編集導線が弱い。

この milestone は、`Timeline` に keyframe lane を載せる前段として、選択中 layer の keyframe を見える状態にし、最小の編集操作を timeline から通せるようにする。

## Phases

### Phase 1: Keyframe Visibility

- 目的:
  - selected layer の keyframe が timeline 上で見えるようにする
  - current frame と keyframe の相対位置が分かるようにする

- 作業項目:
  - property path ごとの keyframe indicator を描く
  - current frame marker と keyframe marker の関係を見える化する
  - selected layer の lane を強調する
  - lane が空のときの状態表示を整理する

- 完了条件:
  - timeline 上で keyframe が存在することを確認できる
  - selected layer の keyframe と current frame の位置関係が分かる

### Phase 2: Timeline Edit Actions

- 目的:
  - timeline から keyframe を直接追加 / 削除 / 選択できるようにする

- 作業項目:
  - lane 上の click / context menu から add / remove を通す
  - keyframe の前後ジャンプを入れる
  - current frame に keyframe を打つ最小導線を作る
  - inspector の keyframe action と同じ core API を使う

- 完了条件:
  - timeline 上だけで keyframe を 1 点追加・削除できる
  - inspector と timeline の結果が一致する

### Phase 3: Move / Range Editing

- 目的:
  - keyframe の位置を timeline 上で調整できるようにする

- 作業項目:
  - drag で keyframe を移動する
  - 複数 keyframe をまとめて扱う導線を作る
  - work area / in-out と競合しない入力分離を作る
  - lock / solo / shy との関係を整理する

- 完了条件:
  - timeline 上で keyframe の時刻を変更できる
  - selection / playback / scrub と矛盾しない

### Phase 4: Property Coverage

- 目的:
  - 主要 property を timeline lane に展開する

- 作業項目:
  - transform
  - opacity
  - effect parameter
  - mask / roto / text への接続

- 完了条件:
  - 代表的な property が同じ lane UI で扱える

### Phase 5: Copy / Paste / Easing

- 目的:
  - 編集作業として完結するところまで持っていく

- 作業項目:
  - keyframe copy / paste
  - easing / interpolation mode
  - selection 範囲の複製
  - timeline と property widget の同期強化

- 完了条件:
  - keyframe 編集が制作フローに乗る

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Current Status

- `Property / Keyframe 統合 実行計画` の前提は既にある
- timeline 側の lane と編集面はまだ未完成
- まずは `ArtifactTimelineTrackPainterView` 側で visibility を固めるのが低コスト

