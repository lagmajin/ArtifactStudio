# MILESTONE: Motion Tokens

日付: 2026-06-07

Color Tokens の動き版として、イージング・時間・振幅・遅延を名前付きで管理し、レイヤーやコンポジションの演出を再利用可能な設計単位にする。

## 2026-07-25 実装監査

`MotionToken`／`MotionBinding`／`MotionProfile` の専用 schema、JSON 保存、token registry、既存レイヤーや UI からの参照・適用経路は確認できない。既存の Easing、Animation Dynamics、プリセットは個別機能として存在するが、名前付き token による共有管理とは別である。したがって Phase 1〜3 は未実装として扱い、runtime の profile 切替や export/import も未検証とする。

## Goal

`introMotion = MotionToken.PopIn` のように、動きを直接埋め込まず、名前付きトークン経由で参照できる状態を作る。

## Non-Goals

- 既存のタイムライン UI を全面刷新しない
- Diligent / D3D12 backend や低レベル render path に触らない
- 新規の global signal-slot 経路を増やさない
- QtCSS / `QColorDialog` / `QImage` の採用を増やさない

## Core Concept

- `MotionToken`
  - `id`
  - `name`
  - `duration`
  - `easing`
  - `amplitude`
  - `delay`
  - `stagger`
  - `loop` / `yoyo`
- `MotionBinding`
  - どの target に適用するか
  - `intro` / `exit` / `hover` / `attention` などの役割
- `MotionProfile`
  - トークン群のセット
  - `Brand A`、`Cinematic`、`Playful` のようなテーマ単位

## Why It Matters

- レイヤーごとに数値を埋め込むより、演出意図を共有しやすい
- `PopIn` の定義変更で複数箇所の動きを一括で揃えられる
- Color Tokens と同じ思想で、デザインと実装の境界を短くできる
- AE では弱い「意味ベースのモーション管理」を Artifact の強みにできる

## Phase 1: Token Schema

目的: モーションの最小定義を保存できるようにする。

- token の名前と id を分ける
- duration / easing / amplitude / delay を表現する
- まずは数値と easing 名の組み合わせで扱う
- 既存の animation 実装へはまだ深く入らない

確認観点:

- token 定義を JSON で保持できる
- token 名変更後も参照が壊れない
- 未定義 token を検出できる

## Phase 2: Binding Model

目的: レイヤーや UI 要素が motion token を参照できるようにする。

- `introMotion`
- `exitMotion`
- `hoverMotion`
- `attentionMotion`

確認観点:

- token を直接埋め込まず参照できる
- 同じ token を複数 target で共有できる
- override が必要な場合だけ局所上書きできる

## Phase 3: Profile Management

目的: トークン群をプロジェクトやブランド単位で切り替えられるようにする。

- profile 切り替え
- token の export / import
- 既存プロジェクトへの後方互換

確認観点:

- profile を切り替えても既存プロジェクトが壊れない
- export/import で token 群を移植できる
- preset 化しやすい粒度を維持できる

## Integration Notes

- Export Matrix と組み合わせると、出力先ごとに motion profile を変えやすい
- まずは定義層を作り、UI は後から追従させる
- 既存の演出コードは一気に置き換えず、参照経路を増やして段階移行する
