# MILESTONE: Multi-Format Preview

日付: 2026-06-07

**最終更新:** 2026-08-15

同じ編集内容を複数アスペクト比や出力サイズで同時にプレビューし、1 箇所の変更が全フォーマットにどう見えるかを即座に確認できるようにする。

## 2026-07-25 実装監査

現行コードでは単一 Composition Editor の Safe Area 表示、各種 composition preset、layout variant 選択、Adaptive Text Fit などの部品は確認できる。しかし、16:9／9:16／1:1／4:5 を同時に並べる View Matrix、shared source から複数 view を再評価する専用 UI、view ごとの crop／safe-area／bounds 差分警告は確認できない。したがって Phase 1〜3 は未実装または別機能の部品段階であり、runtime の複数フォーマット比較は未検証とする。

## 2026-08-15 現行コード監査

- `PreviewQuality`、Draft／Preview／Final の解像度プリセット、Composition Editor の Safe Area／layout preset は実装されているが、これらは単一ビューの品質・レイアウト制御であり、複数フォーマットを同時表示する View Matrix ではない。
- `CompositionRenderController` には preview quality の設定反映と alpha／resource preview があるものの、16:9／9:16／1:1／4:5 を共有ソースから並列再評価する専用コンテナや、view 別 crop／bounds 差分警告は確認できない。
- したがって本マイルストーンの Phase 1〜3 は未着手に近く、既存の PreviewQuality や Safe Area を完了証拠として扱えない。ビルド・ランタイム検証は実施していない。

判定: **単一ビューのpreview品質基盤は実装済み。Multi-Format View Matrix、shared editing、cross-format feedback は pending。**

## Update 2026-08-15

現行コードを追加確認した。`PreviewQuality` の Draft／Preview／Final、alpha／resource preview、Composition Editor の Safe Area／layout preset は存在するが、いずれも単一ビューの品質・レイアウト制御である。16:9／9:16／1:1／4:5を同一編集ソースから並列評価するView Matrix、view別crop／bounds差分、cross-format警告の専用containerは確認できない。

したがって既存のPreviewQualityやSafe Areaを本マイルストーンの完了証拠にはせず、Phase 1〜3は未着手に近い状態、単一preview基盤のみ実装済みと判定する。ビルド・runtime検証は未実施。

## Goal

`16:9`、`9:16`、`1:1`、`4:5`、`Unity 1920x1080` のような複数ビューを並べて、編集の影響を横断的に確認できるようにする。

## Non-Goals

- 既存の単一プレビューを廃止しない
- 低レベル render backend に広く手を入れない
- 新規の global signal-slot 経路を増やさない
- `QImage` の新規採用を増やさない

## Core Concept

- 1 つの編集を複数フォーマットにマッピングする
- 各ビューは同じ source から別の layout / crop / safe area を持つ
- `Content Bounds System` を使って見た目のズレを追いやすくする
- `Coordinate Profiles` を使って単位差を吸収する

## Typical Views

- `16:9`
- `9:16`
- `1:1`
- `4:5`
- `Unity 1920x1080`

## Why It Matters

- Figma の responsive preview のように全体を見やすい
- AE の単一プレビューより展開確認に強い
- SNS / 広告 / Unity 向けの同時確認がしやすい
- 1 か所の修正が他の比率にどう効くかをすぐ見られる

## Phase 1: View Matrix

目的: 複数の preview view を並べる。

- aspect ratio ごとの view を持つ
- 各 view のサイズを変えられるようにする
- 同じ composition から複数 view を生成する

確認観点:

- 並列で見える
- view ごとの設定を保てる
- 単一プレビューも残せる

## Phase 2: Shared Editing

目的: 1 つの編集が全ビューに反映されるようにする。

- source を共有する
- 各ビューは独立した見え方を持つ
- 変更時に全ビューを再評価する

確認観点:

- 1 回の編集で全ビューが更新される
- 表示差が追いやすい
- 特定ビューだけ破綻したときに分かる

## Phase 3: Cross-Format Feedback

目的: 比率違いでの崩れや不足を見える化する。

- crop 差分
- safe area 差分
- bounds 差分
- collision / fallback の警告

確認観点:

- どの比率で問題が出たか分かる
- 対処すべき view が見つけやすい
- preview が重くなりすぎない

## Integration Notes

- `Content Bounds System` があると view 間比較がしやすい
- `Coordinate Profiles` があると単位の違いを吸収しやすい
- `Collision-Aware Layout` と組み合わせると自動回避の効果が見える
- `Sandbox Edits` と組み合わせると差分比較がさらに強くなる
