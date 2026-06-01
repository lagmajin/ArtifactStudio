# MILESTONE: Ad Production Accelerator

日付: 2026-05-28

AE 互換や高機能編集だけを追うのではなく、広告動画・SNS 動画の量産で発生する反復作業を短縮するための実装候補。

元メモ: `docs/analysis/MOTION_GRAPHICS_AD_PRODUCTION_THINKING_MEMO_2026-05-28.md`

## Goal

1 つのモーショングラフィックテンプレートから、文言・素材・画角違いの広告動画を安全に量産できる状態を目指す。

## Non-Goals

- 完全自動生成ツールに寄せすぎない
- 既存の編集 UI を大きく置き換えない
- Diligent / D3D12 backend や低レベル render path にはこの段階では触らない
- `QColorDialog` / QtCSS / 新規グローバル signal-slot 経路を増やさない

## Primary Users

- 広告運用者: 訴求・CTA・尺違いを大量に回したい
- EC 担当: 商品名、価格、在庫、キャンペーン日付を頻繁に差し替えたい
- SNS 運用者: 9:16 / 1:1 / 4:5 の展開を短時間で作りたい
- デザイナー: テンプレを作り、量産作業だけを圧縮したい

## Core Workflow

1. ユーザーが通常の編集 UI で広告テンプレートを作る
2. 差し替え可能な項目を `slot` として定義する
3. CSV / JSON / 手入力フォームから複数 variation を流し込む
4. 各 variation を composition / render preset / export job に展開する
5. 画角違いに対して text fit / safe area / anchor rule を適用する
6. 書き出し前に文字切れ・はみ出し・素材欠落を警告する

## Phase 1: Template Slots

目的: 既存 composition の上に、差し替え項目を明示できる最小構造を作る。

- text layer を `slot` として名前付けできる
- image / media layer を `slot` として名前付けできる
- slot には display name、default value、required flag を持たせる
- project 保存時に slot 定義が保持される
- 既存 layer name と競合しないよう、内部 id と表示名を分ける

確認観点:

- slot がない既存プロジェクトは挙動が変わらない
- slot 名変更後も既存 layer 参照が壊れない
- 未入力 required slot を検出できる

## Phase 2: Variation Data Import

目的: CSV / JSON から複数の広告 variation を読み込めるようにする。

- 1 行を 1 variation として扱う
- column name と slot name を対応させる
- preview 用に variation list を表示する
- invalid row を警告として扱い、全体を即失敗させない
- ファイル import だけでなく、後で手入力フォームにも拡張できる形にする

確認観点:

- 日本語テキスト、価格表記、改行を扱える
- 空欄 required slot を警告できる
- 存在しない slot の column を無視または警告できる

## Phase 3: Responsive Layout Rules

目的: 文量や画角違いで広告が崩れる箇所を、最小限のルールで吸収する。

- text fit: shrink / wrap / truncate warning
- safe area: 媒体別の内側余白を持つ
- anchor rule: left / center / right / top / bottom を保持する
- media fit: contain / cover / crop focus を選べる
- まずは自動補正より、崩れ検出と警告を優先する

確認観点:

- 9:16、1:1、16:9 の preview で文字切れを検出できる
- 自動縮小が読みやすさを壊す場合は warning にできる
- layout rule が既存の transform / animation と衝突しない

## Phase 4: Batch Export Bridge

目的: variation と画角 preset を組み合わせて、書き出し job を作れるようにする。

- variation x output preset の job list を作る
- job name に variation id / output preset を含める
- missing asset / text overflow / required slot missing を export 前に集約する
- 既存 render / export 経路を再利用し、専用 renderer を作らない

確認観点:

- 1 variation だけの通常出力と batch 出力の結果が一致する
- error / warning が job 単位で追える
- 書き出し失敗時に残り job の扱いを選べる

## UI Candidates

- Project View: template / variation data file を asset として見せる
- Property Editor: selected layer の slot 設定を表示する
- Export / Render Center: variation batch の job preview を表示する
- Composition Editor: safe area / overflow warning overlay を表示する

UI 名称や責務で迷った場合は `docs/WIDGET_MAP.md` を確認する。

## Data Model Candidates

- `TemplateSlot`
  - stable id
  - display name
  - target layer id
  - value type: text / image / media / color / number
  - default value
  - required flag

- `TemplateVariation`
  - variation id
  - display name
  - slot value map
  - source row index
  - validation state

- `OutputVariant`
  - composition size
  - safe area preset
  - render preset
  - naming rule

## Open Questions

- slot 定義は composition 側に持つか、project asset として独立させるか
- variation を実体 composition として複製するか、export 時に一時適用するか
- text fit は Core 側の text layout 責務に寄せるか、UI 側 preview 補助に留めるか
- CSV import は最初から UTF-8 / BOM / Shift_JIS を考慮するか
- batch export の進捗とエラー表示を既存 Render Center に寄せられるか

## Suggested MVP

最初は以下だけでよい。

- text slot 定義
- CSV 1 行 = 1 variation
- variation preview
- 9:16 / 1:1 / 16:9 の output preset
- text overflow warning
- batch export job list の生成

これで「広告動画の文言差し替えと画角展開をまとめて処理する」価値を最短で検証できる。
