# MILESTONE: Named Guides

日付: 2026-06-07

**最終更新:** 2026-08-15

## Update 2026-08-15

- `GuideDefinition`／`GuideSet`／`GuideBinding` の name・purpose 相当の semantic tag、orientation／position／enabled／priority、JSON 保存復元、semantic／enabled filtering を再確認した。Project と Construction Layer の guide set に保持され、Smart Guides が Construction Layer の enabled guide を snap 候補へ取り込む。
- View の guide 表示／snap toggle と toolbar／shortcut の導線も存在する。ただし `GuideBinding` を使った layer の follow／relative offset 解決、優先順位を考慮した実際の追従、用途別自動配置、Content Bounds／Collision-Aware Layout 連携は未確認。
- 現状判定は Phase 1 実装済み、Phase 2 表示・snap 部分実装、Phase 3 未完了で、前回監査から大きな変更は確認できない。

ガイドを単なる線ではなく、名前付き・用途付きの永続的な参照として扱う。

## 2026-07-25 実装監査

`GuideDefinition`／`GuideSet`／`GuideBinding` は name・purpose・position・enabled・priority・semantic tag を持ち、Project／Construction Layer の JSON 保存・復元と enabled／semantic filtering が実装されている。Smart Guides は Construction Layer の enabled guide を縦横の snap guide として取り込む。一方、binding に基づく layer の follow／relative offset／優先順位付き解決、`Title Baseline` 等の用途別自動配置、Content Bounds／Collision-Aware Layout との接続は確認できない。したがって Phase 1 は実装済み、Phase 2 は表示・snap の部分実装、Phase 3 は未完了とする。

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
