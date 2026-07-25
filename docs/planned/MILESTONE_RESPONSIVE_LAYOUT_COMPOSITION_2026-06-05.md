# MILESTONE Responsive Layout Composition

## Goal

`ResponsiveComposition` を別のコンポ種別として増やすのではなく、`Composition` に `Responsive Layout` 機能を付ける。1つの composition の中で複数の layout variant を持ち、出力先の aspect ratio に応じてレイアウトを切り替えられるようにする。

## Why this is needed

- 同じ作品を `16:9` / `9:16` / `1:1` で展開したいケースが増えている
- 別コンポに分けると、レイヤー構造やタイムラインの同期が崩れやすい
- `Composition` の中に variant を持たせれば、素材・演出・編集履歴を共有したまま出力先だけ変えられる

## Scope

- `Composition` に responsive layout 定義を追加する
- 複数の layout variant をシリアライズできるようにする
- variant ごとの解像度比、ガイド、安全領域、アンカー、余白ルールを保持する
- UI から variant を切り替えてプレビューできるようにする
- 出力時に variant を明示選択できるようにする

## Current Code Status

- `ArtifactAbstractComposition` already stores `ResponsiveLayoutSet` and serializes it through `responsiveLayout`
- Project Manager UI already exposes add / duplicate / edit / activate actions for layout variants
- Render overlay and project model already surface the active layout variant in summaries
- 2026-06-29 時点の残作業は、variant switch の見え方をさらに整理しつつ、必要なら preflight / export の明示メッセージを詰めること

### 2026-07-25 実装監査

Phase 1 の `ResponsiveLayoutSet`／variant の JSON 保存・復元、Project View の追加・複製・編集・activate、Composition Editor の selector／`Responsive Preview Matrix` 入口、overlay／model の active variant summary は実装を確認した。Render Queue には active variant と output size の不一致 warning もある。一方、Preview Matrix は一覧から variant を切り替える入口で、複数ビューを同時描画するものではない。variant ごとの実レイアウト再配置、preflight の網羅的診断、4:5 等の preset／safe-area・anchor の実適用は未確認であり、Phase 2 は部分実装、Phase 3〜4 は継続とする。

## Variant examples

- `16:9` - 横長の標準出力
- `9:16` - 縦長の SNS / モバイル出力
- `1:1` - 正方形の配信用途

## Suggested implementation phases

### Phase 1: Data model and serialization
- `Composition` に responsive layout set を追加する
- `LayoutVariant` の ID、aspect ratio、safe area、layout rule を保存する
- 既存 composition からの互換読み込み方針を決める

### Phase 2: Editor surface
- `Composition` 編集 UI に variant selector を置く
- variant ごとの visible guide を表示する
- 1つの composition 内で variant を切り替えて確認できるようにする

### Phase 3: Render and export routing
- render/export 時に対象 variant を選択する
- output size と layout variant の整合性をチェックする
- variant ごとの preflight / diagnostics を出せるようにする

### Phase 4: Polish and presets
- `16:9 / 9:16 / 1:1` のプリセットを用意する
- 将来の `4:5` / `21:9` 拡張を見越して preset registry を整える
- safe area と content anchor の調整を詰める

## Success criteria

- `Responsive Layout` が `Composition` の内包機能として扱われる
- 1つの composition から複数 aspect ratio の出力を切り替えられる
- layout variant の追加が既存編集フローを壊さない
- 出力や preview で、どの variant を使っているかが明確に見える
# 2026-07-10 Progress

- Command Palette に `Responsive Preview Matrix` の初期入口を追加
- variant 名と解像度を一覧化し、active responsive variant を素早く切り替えられるようにした
- 同時サムネイル描画は後続スライス
