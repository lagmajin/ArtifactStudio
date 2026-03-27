# Timeline Search / Keyframe Integration Milestone

`Timeline Layer Search` と `Timeline Keyframe Editing` を一体で扱い、検索した layer から keyframe まで迷わず辿れるようにするためのマイルストーン。

この milestone は検索を単独機能として閉じず、keyframe の可視化・ジャンプ・診断と結びつけて、`ArtifactTimelineWidget` の上部操作系をひとまとめに整理する。

## Goal

- layer search と keyframe navigation が同じ header / status area で扱える
- 検索結果から keyframe の位置へ素早く移動できる
- filter-only / highlight-only でも keyframe の文脈を失わない
- 検索と keyframe の現在位置表示が矛盾しない

## Scope

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScene.cppm`
- 必要に応じて `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

## Non-Goals

- 全文検索エンジンの導入
- keyframe graph editor の本格実装
- property editor 側の完全な再設計
- asset browser との横断検索統合

## Background

`Timeline Layer Search` は layer / effect / tag / state のインクリメンタル絞り込みとして既に前進している。
一方で keyframe 側は、可視化と追加 / 削除 / ジャンプの基礎が整ってきた段階で、検索結果と操作対象の結びつけがまだ弱い。

この milestone は、検索と keyframe を別物として扱わず、`artifactTimelineWidget` 上で「見つける」「飛ぶ」「確認する」を同じ操作面に載せることを狙う。

## Phases

### Phase 1: Shared Status Model

- 目的:
  - search と keyframe の状態を同じ status model で扱う

- 作業項目:
  - search hit count と keyframe count を分けて保持する
  - current match / current keyframe の位置を分離する
  - empty / no-hit / no-keyframe の表示を区別する

- 完了条件:
  - header 上で search と keyframe の状態が混ざらない

### Phase 2: Search-to-Keyframe Navigation

- 目的:
  - 検索結果から keyframe へ素早く移動できるようにする

- 作業項目:
  - search hit から対象 layer の keyframe 群へ飛ぶ
  - next / previous hit と next / previous keyframe の導線を分ける
  - `Enter` / `Shift+Enter` / `F3` の役割を整理する

- 完了条件:
  - 検索結果と keyframe の両方を迷わず辿れる

### Phase 3: Filter / Highlight Coherence

- 目的:
  - 検索で非表示になっても keyframe の文脈を失わない

- 作業項目:
  - highlight-only で keyframe marker を残す
  - filter-only で操作対象を status に残す
  - nearest keyframe の表示と search result highlight を分ける

- 完了条件:
  - 大量 layer でも文脈を失わずに keyframe を追える

### Phase 4: Header Diagnostics

- 目的:
  - いま見ている場所の意味を header から把握できるようにする

- 作業項目:
  - hit count / current hit / nearest keyframe を表示する
  - 空検索時と空 keyframe 時のメッセージを分ける
  - mode 切替時に表示が一瞬壊れないようにする

- 完了条件:
  - search と keyframe の状態が header だけで把握できる

### Phase 5: Polish / Safety

- 目的:
  - 操作の誤認を減らす

- 作業項目:
  - search clear と keyframe selection clear の分離
  - view focus が外れたときの state reset を整理する
  - current layer 変更時の fallback を決める

- 完了条件:
  - search / keyframe / selection の切替で迷いにくい

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Current Status

- `Timeline Layer Search` と `Timeline Keyframe Editing` は別々に進められる段階にある
- ただし実運用では、検索結果から keyframe へ飛びたい要求が強く、統合の導線が必要
- `ArtifactTimelineWidget` の header / status area は、統合用の土台として拡張しやすい位置にある
- まずは search state と keyframe state を同じ表示系で扱うのが低コスト

## Implementation Tasks

### Task 1: Shared Status State

- search hit count / current hit index / keyframe count / nearest keyframe を別々に持つ
- `ArtifactTimelineWidget` の header へ渡す state を 1 箇所にまとめる
- empty / no-hit / no-keyframe の文言を分ける

### Task 2: Navigation Wiring

- `Enter` / `Shift+Enter` / `F3` の対象を search hit と keyframe で整理する
- search hit から target layer の keyframe へジャンプする補助関数を作る
- `Timeline Layer Search` 側の選択移動と keyframe 移動を干渉させない

### Task 3: Highlight Coherence

- highlight-only では keyframe marker を残す
- filter-only では操作対象の layer / keyframe を status に残す
- nearest keyframe と current search result の色と役割を分ける

### Task 4: Diagnostics / Safety

- mode 切替時の一瞬の blank state を防ぐ
- current layer 変更時の fallback を決める
- search clear と keyframe selection clear を別操作として扱う
