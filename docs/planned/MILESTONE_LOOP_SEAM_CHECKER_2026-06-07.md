# MILESTONE: Loop Seam Checker

日付: 2026-06-07

ループアニメーションの最初と最後が自然につながっているかを検査し、継ぎ目を見つけやすくする。

## Goal

`Position mismatch`、`Rotation mismatch`、`Opacity mismatch`、`Color mismatch`、`TimeRemap mismatch` を検出して、ループの継ぎ目を事前に確認できるようにする。

## Non-Goals

- ループを自動で無理やり修正することを主目的にしない
- 低レベル render backend を広く変更しない
- 新規の global signal-slot 経路を増やさない
- `QImage` の新規採用を増やさない

## Core Concept

- 先頭フレームと末尾フレームを比較する
- 何が一致していないかを種類ごとに見せる
- 必要なら修正候補を出すが、まずは検査を優先する
- 背景ループ、GIF、ゲーム素材で使いやすくする

## Typical Checks

- Position mismatch
- Rotation mismatch
- Opacity mismatch
- Color mismatch
- TimeRemap mismatch

## Why It Matters

- ループ素材は継ぎ目の違和感が品質に直結する
- 背景や GIF は small mismatch でも目立ちやすい
- ゲーム素材やモーション背景の確認に効く
- `Multi-Format Preview` と組み合わせると比率ごとの継ぎ目も見やすい

## Phase 1: Seam Metrics

目的: 継ぎ目の差分項目を定義する。

- 位置差
- 回転差
- 不透明度差
- 色差
- 時間リマップ差

確認観点:

- 何がずれているかを分類できる
- 数値化できる
- レイヤーごとに比較できる

## Phase 2: Inspection View

目的: 継ぎ目を UI で見えるようにする。

- first / last の比較
- 差分ハイライト
- warning の表示
- ループ適合度の表示

確認観点:

- 目で見て違和感の原因が分かる
- 修正候補を追える
- ループ継ぎ目を素早く判断できる

## Phase 3: Export Feedback

目的: export 前にループの継ぎ目を警告する。

- GIF / 背景ループ / ゲーム素材向けの warning
- mismatch が大きい場合は警告を強める
- 必要なら export 前の確認を促す

確認観点:

- export 前に不自然なループを見つけられる
- 重大な mismatch を見逃しにくい
- ループ素材の品質が安定する

## Integration Notes

- `Motion Tokens` と相性がよい
- `Content Bounds System` を使うと位置差の解釈が安定する
- `Multi-Format Preview` で複数比率のループを同時確認しやすい

---

## Static audit follow-up (2026-07-25)

Loop Seam Checker 固有の model／checker／UI／export warning を現行ソースで検索した。ビルド・実データ検査は未実施。

| Phase | 現状 | 判定 |
|---|---|---|
| 1. Seam Metrics | position／rotation／opacity／color／time-remap の first/last 比較を担う `LoopSeamChecker` 相当の実装は確認できない。 | 未実装 |
| 2. Inspection View | first/last 差分、mismatch分類、warning、loop適合度を表示する専用UIは確認できない。 | 未実装 |
| 3. Export Feedback | GIF／loop export 前に seam mismatch を判定・警告する接続は確認できない。 | 未実装 |
| 関連基盤 | loop処理、procedural texture の seamless設定、export／previewの個別機能はあるが、本 checker の判定契約とは別。 | 部分的な関連基盤 |

### 現在の判定

本マイルストーン固有の検査モデル、可視化、export feedback は未着手。既存の loop／seamless 機能だけでは完了条件を満たさないため、「計画段階」とする。
