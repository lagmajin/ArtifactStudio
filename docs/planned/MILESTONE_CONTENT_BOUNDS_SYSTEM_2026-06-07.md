# MILESTONE: Content Bounds System

日付: 2026-06-07

**最終更新:** 2026-08-15
**Status:** LayerBounds／5種の query API／summary は実装済み、effect／mask の厳密計算と外部 consumer 統合は未完了

見た目の外接矩形を安定して計算し、テキスト背景追従・見た目中央アンカー・自動クロップ・整列・セーフエリア判定を支える基盤を作る。

## 2026-07-25 実装監査

`LayerBounds`／`LayerBoundsKind` と `contentBounds()`、source／visible／effect／mask／layout の query API、summary 表示は実装されている。ただし基底実装では `effectBounds` と `maskBounds` が visible bounds の代用で、`layoutBounds` も source を基本とするため、effect／mask を実測した共通計算ではない。外部 automation／UI consumer が5種類を一貫して利用する経路や safe-area warning、auto-crop／visual-center の統合も確認できない。したがって Phase 1〜2 は API の骨格、Phase 3 は未完了とする。

## Goal

`visibleBounds`, `sourceBounds`, `effectBounds`, `maskBounds`, `layoutBounds` を一貫したルールで扱えるようにする。

## Non-Goals

- 低レベル render backend の全面改修はしない
- Diligent / D3D12 path に推測ベースで広く触らない
- 既存レイヤーの意味を壊すような大規模リネームはしない
- `QImage` の新規採用は増やさない

## Core Concepts

- `sourceBounds`
  - 元素材の自然な範囲
- `visibleBounds`
  - 現在の見た目で実際に見えている範囲
- `effectBounds`
  - blur / glow / shadow などの effect を含む範囲
- `maskBounds`
  - mask で切り抜かれた後の範囲
- `layoutBounds`
  - 整列やアンカー判定に使う論理的な範囲

## Why It Matters

- テキスト背景が見た目に追従しやすくなる
- アンカーを見た目中央へ寄せやすくなる
- 自動クロップの基準を統一できる
- 整列やセーフエリア判定のロジックを共有しやすくなる
- effect や mask を含むレイヤーでも同じ語彙で扱える

## Phase 1: Bounds Definition

目的: 各 bounds の意味をコードと UI で共通化する。

- bounds の種類を明示する
- レイヤー種別ごとに返す基準を分ける
- まずは計算結果の表現を安定させる
- レンダリング実装の詳細は隠蔽する

確認観点:

- 同じレイヤーで bounds 種別ごとの差が追える
- 空レイヤーや極小レイヤーでも壊れない
- text / image / media / effect 系で共通の扱いができる

## Phase 2: Query API

目的: 外部から bounds を問い合わせできるようにする。

- `getVisibleBounds`
- `getSourceBounds`
- `getEffectBounds`
- `getMaskBounds`
- `getLayoutBounds`

確認観点:

- UI と automation の両方から読める
- 取得値が毎回同じルールで返る
- 必要なら近似値と厳密値を分けられる

## Phase 3: Layout Consumers

目的: bounds を使う側の機能をつなぐ。

- text background follow
- anchor to visible center
- auto crop
- align to visual bounds
- safe area warning

確認観点:

- 整列結果が見た目とずれにくい
- セーフエリア判定が source 基準と混ざらない
- effect や mask を足しても挙動が予測しやすい

## Integration Notes

- `Sandbox Edits` の比較表示と相性が良い
- motion tokens と組み合わせると、サイズ変化や動きの影響を比較しやすい
- まずは bounds の意味を揃え、UI は後から利用する

## 2026-08-15 現行実装監査

- `ArtifactAbstractLayer::contentBounds()` は Source／Visible／Effect／Mask／Layout を `LayerBounds` として返し、各 query と summary API が公開されている。
- `visualLocalBounds()` は clone／effect／modifier 等を含む表示寄りの bounds を計算するため、単純な `localBounds()` より進んだ実装が存在する。
- 一方、基底 `contentBounds()` では effect／mask が visible bounds を代用するケースが残り、5種類の意味が常に厳密に分離されているとは言えない。
- text background、visual-center、auto-crop、safe-area warning、automation からの共通利用は未確認。runtime の bounds 受入も未実施。

## Update 2026-08-15

- 現行コードでは `LayerBounds`／5種 query／summary、`visualLocalBounds()`、Resolution Remap／Layer Alignment からの bounds 利用を確認できる。
- 基底 query では effect／mask が visible の代用になる場合があり、text background／visual-center／auto-crop／safe-area／automation の共通 consumer と runtime 受入は未完了または未確認。
