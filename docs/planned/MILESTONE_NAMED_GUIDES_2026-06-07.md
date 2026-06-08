# MILESTONE: Named Guides

日付: 2026-06-07

ガイドを単なる線ではなく、名前付き・用途付きの永続的な参照として扱う。

## Goal

`Title Baseline`、`Logo Area`、`Caption Bottom`、`Character Center`、`Export Crop Edge` のような semantic guide を作り、各レイヤーがスナップや追従に使えるようにする。

## Non-Goals

- 既存のガイド表示を壊さない
- 新規の global signal-slot 経路を増やさない
- Diligent / D3D12 backend に触らない
- QtCSS / `QColorDialog` / `QImage` を増やさない

## Core Concept

- `Guide`
  - id
  - name
  - purpose
  - position / range
  - enabled
- `GuideSet`
  - プロジェクトやコンポジション単位のガイド群
- `GuideBinding`
  - レイヤーがどのガイドにスナップするか
  - 追従 / 固定 / 参考 の関係を持てる

## Why It Matters

- ただの線より、意図のあるレイアウト指標として扱える
- タイトル、ロゴ、字幕、キャラクターの基準を共有しやすい
- export crop や safe area と結びつけやすい
- `Collision-Aware Layout` の回避先を明示しやすい

## Phase 1: Guide Schema

目的: 名前付きガイドを保存できるようにする。

- name と purpose を持つ
- 水平 / 垂直 / 範囲ガイドを扱う
- enabled 切り替えを持つ
- プロジェクトへ永続化できるようにする

確認観点:

- ガイド名で意味を追える
- 既存の無名ガイドと共存できる
- 複製や export に耐えられる

## Phase 2: Snapping and Follow

目的: レイヤーがガイドに追従できるようにする。

- snap
- align
- follow
- relative offset

確認観点:

- ガイドに吸い付く挙動が安定する
- 必要なら手動で外せる
- 複数ガイドの優先順位を扱える

## Phase 3: Semantic Usage

目的: 用途別ガイドとして機能させる。

- `Title Baseline`
- `Logo Area`
- `Caption Bottom`
- `Character Center`
- `Export Crop Edge`

確認観点:

- 用途名だけで使い道が分かる
- `Content Bounds` と組み合わせて自動配置できる
- `Collision-Aware Layout` の回避先として使える

## Integration Notes

- `Content Bounds System` があるとガイド位置の意味が安定する
- `Coordinate Profiles` と組み合わせると単位の異なるガイドも扱いやすい
- `Sandbox Edits` でガイド変更前後を比較しやすい
