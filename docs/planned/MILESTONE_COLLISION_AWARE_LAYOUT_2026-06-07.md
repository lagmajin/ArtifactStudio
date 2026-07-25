# MILESTONE: Collision-Aware Layout

日付: 2026-06-07

UI やテロップが重なったときに、自動で避けるレイアウト制御を作る。

## 2026-07-25 実装監査

本マイルストーンの semantic target role、layout collision resolver、position／scale／reflow の自動回避、衝突理由の UI 表示に対応する専用実装は確認できない。既存には timeline keyframe の衝突表示や物理／粒子の collision、layer の input priority など別責務の機能はあるが、本マイルストーンの広告レイアウト衝突とは分けて扱う。したがって Phase 1〜3 は未実装で、Content Bounds／Responsive Layout からの統合も未着手とする。

## Goal

タイトルとロゴ、字幕と顔、ボタンとセーフエリアのような衝突を検知し、必要に応じて位置調整や縮小を行えるようにする。

## Non-Goals

- 既存レイアウトを全面的に置き換えない
- 推測ベースで低レベル render path を広く変更しない
- 新規の global signal-slot 経路を増やさない
- `QImage` / QtCSS / `QColorDialog` の採用を増やさない

## Core Concept

- 衝突対象を semantic に扱う
- `face`、`logo`、`subtitle`、`button`、`safeArea` などの役割を持たせる
- 衝突したら位置・サイズ・優先度に基づいて回避する
- 回避結果を UI と export の両方で使えるようにする

## Typical Reactions

- タイトルとロゴが重なる
  - タイトルを少し下げる
  - またはロゴを縮小する
- 字幕が人物の顔にかかる
  - 字幕を上へ逃がす
  - 必要なら行数を調整する
- ボタンがセーフエリアから出る
  - 内側へ戻す
  - 必要なら縮小する

## Why It Matters

- サムネイルやショート動画生成で特に効く
- 人物、ロゴ、字幕が混在するコンテンツに強い
- 自動化しても「何を避けたか」が説明しやすい
- `Content Bounds System` と組み合わせると判定が安定する

## Phase 1: Collision Targets

目的: 回避対象の役割を定義する。

- target role を持たせる
- 優先度を設定できるようにする
- safe area も target として扱えるようにする

確認観点:

- 何を避けるかを明示できる
- 対象ごとに優先度を変えられる
- 既存レイヤーが壊れない

## Phase 2: Collision Resolution

目的: 衝突時の自動調整ルールを作る。

- position offset
- scale down
- reflow / wrap
- fallback placement

確認観点:

- 1 回の調整で破綻しない
- 過剰に揺れない
- 同じ入力で同じ結果になる

## Phase 3: Layout Feedback

目的: 何が起きたかを UI で分かるようにする。

- 衝突のハイライト
- 回避理由の表示
- 調整後の bounds 表示

確認観点:

- 自動調整の意図が追える
- ユーザーが手動で上書きできる
- 失敗時に理由が分かる

## Integration Notes

- `Content Bounds System` は前提になる
- `Named Guides` と組み合わせると回避先が明確になる
- `Sandbox Edits` と組み合わせると回避前後の比較がしやすい
