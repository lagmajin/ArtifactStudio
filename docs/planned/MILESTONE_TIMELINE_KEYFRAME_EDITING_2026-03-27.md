# Timeline Keyframe Editing Milestone

`ArtifactTimelineWidget` の右ペインと inspector 周辺をつなぎ、property keyframe を timeline 上で見て・触って・移動できるようにするためのマイルストーン。

この milestone は `AbstractProperty` を keyframe の source of truth とする前提に立ち、`Timeline` 側の編集面だけを切り出して整理する。

## Goal

- Timeline 上で keyframe の位置と対象 property を確認できる
- selected layer の keyframe を timeline から追加 / 削除 / 移動できる
- inspector の keyframe 操作と timeline の操作が同じ source を見る
- いま見ている frame と keyframe の関係がすぐ分かる
- keyframe ドラッグ中に AE 風の時間スナップと距離表示が扱える

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

---

## Current Status

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
  - selected layer の keyframe marker を描画し、click modifier と rectangle selection を扱える
  - 左クリックで keyframe を seek し、context menu から add / remove も通せる
  - selected marker の batch move と move requested event がある
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
  - `keyframeStatusLabel` と `selectionSummaryLabel` があり、frame / selection / keyframe 状態をヘッダーで読める
  - `Ctrl+PageUp` / `Ctrl+PageDown` で keyframe navigation を行える
  - selection と keyframe state の同期導線が既に入っている
  - header 側に search / mode / count を寄せる土台もあるので、診断表示を同じ surface にまとめやすい
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
  - property row 側の keyframe toggle が source of truth に繋がっている
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
  - `Keyframes Only` / presentation 寄りの整理と相性がよく、flat view と組み合わせやすい
  - layer display mode が実際にフィルタへつながり、keyframed layer を絞り込める

この状態を踏まえると、今の中心は「keyframe 操作を増やすこと」よりも「迷わず触れる surface に整えること」にある。

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
  - 既存の selection / seek / context menu を壊さずに visibility を磨く

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
  - `timeline keyframe status` と `selection summary` が一致するようにする

- 完了条件:
  - timeline 上だけで keyframe を 1 点追加・削除できる
  - inspector と timeline の結果が一致する

### Phase 3: Move / Range Editing

- 目的:
  - keyframe の位置を timeline 上で調整できるようにする

- 作業項目:
  - drag で keyframe を移動する
  - 複数 keyframe をまとめて扱う導線を作る
  - 他レイヤー / 他 property の keyframe との距離表示と等間隔スナップを入れる
  - current frame、他 keyframe、grid への時間スナップを整理する
  - work area / in-out と競合しない入力分離を作る
  - lock / solo / shy との関係を整理する
  - move requested event の受け口を timeline / property の両側で揃える

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

### Phase 6: Navigation / Diagnostics

- 目的:
  - keyframe が増えても追跡しやすい状態を保つ
  - timeline 上の現在位置と keyframe 群の関係をすぐ辿れるようにする

- 作業項目:
  - selected layer / current clip の keyframe を前後にジャンプする
  - hit count と空状態を timeline header に出す
  - current frame から最寄りの keyframe を強調する
  - search / filter 状態と keyframe 表示を矛盾させない
  - keyframe へのスナップ候補を視覚的に出す
  - `Operation Feel Refinement` と `Flat Keyframe View` を診断面から支える

- 完了条件:
  - keyframe が多い layer でも目的の点まで素早く移動できる
  - timeline 上で見えている keyframe と操作対象が一致する

## Implementation Tasks

### Phase 1 Task Breakdown

1. `ArtifactTimelineTrackPainterView` に keyframe marker 用の描画情報を集約する
   - property path ごとの marker 情報を 1 回のスキャンで作る
   - current frame と marker の重なりを hit test できる形にする
2. selected layer の lane state を timeline 側へ渡す
   - 選択中 layer の keyframe 群だけを強調する
   - lane が空のときの表示と非表示を分ける
3. current frame と keyframe の相対関係をヘッダ側に出す
   - “今の frame に keyframe があるか” を即座に分かるようにする
   - marker 密度が高い場合の省略表示を決める

### Phase 2 Task Breakdown

1. keyframe add / remove の共通入口を timeline widget 側に揃える
   - context menu と mouse action の両方から同じ core API を呼ぶ
   - inspector 側の action と差分が出ないようにする
2. 前後ジャンプを実装する
   - selected layer / current clip の候補だけに絞る
   - `Enter` / `Shift+Enter` / `F3` の導線は Phase 6 の diagnostics と共有する
3. playhead 位置への打鍵導線を安定化する
   - current frame に最小操作で keyframe を追加する
   - 既存 keyframe との衝突時の扱いを決める

### Phase 3 Task Breakdown

1. keyframe drag を timeline 上で扱えるようにする
   - marker の draggable region を定義する
   - snap 単位と free move の切り替えを分ける
2. range selection の最小導線を作る
   - 複数 keyframe の同時選択
   - selection の解除と再選択
3. scrub / work area / in-out と衝突しない入力ルールを決める
   - 左ドラッグ / 修飾キー / right-click の責務を切り分ける
   - スナップガイドと距離表示のレイヤー優先順位を決める

### Phase 4 Task Breakdown

1. 代表 property を timeline lane に展開する
   - `transform`
   - `opacity`
   - `effect parameter`
2. layer 種別ごとの接続点を整理する
   - `mask`
   - `roto`
   - `text`
3. property editor と lane 表示の名前を合わせる
   - path 表示と human-readable label を揃える
   - keyframe の source を見失わないようにする

### Phase 5 Task Breakdown

1. keyframe copy / paste の最小単位を決める
   - 1 点コピー
   - 範囲コピー
2. easing / interpolation の表示だけ先に揃える
   - full editor を先に作らず、既存値の見える化から入る
3. property widget と timeline の selection を同期する
   - どちらで選んでも同じ keyframe 群に入るようにする

### Phase 6 Task Breakdown

1. hit count と current hit index を timeline header に出す
   - 絞り込み後の keyframe 数を見せる
   - 空検索時の案内を入れる
2. nearest keyframe の強調表示を入れる
   - current frame に最も近い keyframe を目立たせる
   - 近接ハイライトと selection ハイライトを区別する
3. search / filter 状態と keyframe 表示を連動させる
   - layer search が active のときに keyframe 表示が壊れないようにする
   - filter-only モードでも操作対象が分かるようにする

## File Checklist

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
  - timeline header の検索 / 件数 / モードと keyframe 操作の連携点
  - jump / selection / playhead 同期
- `Artifact/include/Widgets/ArtifactTimelineWidget.ixx`
  - keyframe navigation と search state の公開 API
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
  - keyframe marker の描画と hit test
  - highlight / nearest / selected state の見え方
- `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`
  - marker visual / hit result の公開型
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScene.cppm`
  - timeline 内の入力イベントと scene 側の選択状態
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
  - layer search と keyframe visibility の干渉整理
- `Artifact/include/Widgets/Timeline/ArtifactLayerPanelWidget.ixx`
  - search mode / match result / selection 状態の公開
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
  - inspector 側の keyframe action と timeline 側の差分確認
- `ArtifactCore/src/Property/*`
  - keyframe source of truth と action API
- `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`
  - search input / jump shortcut / header controls の接続

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5
6. Phase 6

---

## Related Execution Memos

- [`MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03_EXECUTION.md`](./MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03_EXECUTION.md)
- [`MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03_EXECUTION.md`](./MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03_EXECUTION.md)
