# Export Matrix And Alpha Clarity Milestone

**最終更新:** 2026-08-15

> 2026-06-07 作成

> 2026-07-25 実装監査: `ExportMatrix`／variant／preset／cell rule の JSON schema、cell 解決、WorkspaceAutomation の resolve／job生成／現行 composition への queue 入口は実装を確認した。一方、本書の alpha clarity 要件（用途プリセットのUI、straight／premultiplied の比較、alpha edge check、export 前の警告集約）は別の出力基盤と部分的な警告に留まり、専用の一貫した preview／preflight workflow は未確認である。Matrix のデータ／automation 基盤は実装済み、UI と alpha 契約・検査は継続とする。

**ステータス:** Export Matrix／preset／automation と出力設定ガイドは実装済み、alpha preflight／比較 workflow と runtime 検証が未完了

## Update 2026-08-15

`ExportMatrix` の variant／preset／cell rule JSON schema、cell解決、WorkspaceAutomation の resolve／job生成／Render Queue入口、出力設定ガイドを現行コードで確認した。用途別の出力設定基盤とautomation接続は実装済みである。

未完了・未確認なのは、用途プリセットの全UI、straight／premultiplied比較、alpha edge／fringe preflight、警告の一体的な集約、実出力でのalpha保持、形式別runtime互換性である。alpha契約と比較workflowは pending とする。

## 目的

書き出し時の `channel` / `alpha` / `premultiplied` / `straight` の混乱をなくし、
ユーザーが「どのプリセットを選べばよいか」「透過が本当に残るか」を
迷わず判断できるようにする。

このマイルストーンでは、出力形式の細かい内部設定を前面に出しすぎず、
**用途ベースのプリセット名** と **アルファ状態の自動推定/警告** を中心に再設計する。

## 背景

現在の書き出し UI は、機能としては多くの形式を扱える一方で、
初心者にとっては `RGB` / `RGB+Alpha` / `Alpha only` や
`Straight` / `Premultiplied` の意味が分かりにくい。

その結果、以下の事故が起きやすい。

- 透過動画のつもりで出したのに、RGB のみで黒背景になる
- 他ソフトで合成したときに縁が白くなる
- どのプリセットが編集ソフト向けで、どれが web 向けか直感で分からない
- 書き出し前にアルファの健全性を確認しづらい

## Goal

- 用途ベースの直感的なプリセット名を標準化する
- Alpha の扱いを自動判定し、必要なら警告する
- `straight` と `premultiplied` の違いをプレビューで見えるようにする
- 書き出し前に `alpha edge check` を実行できるようにする

## Scope

- `Artifact/src/Widgets/Render`
- `Artifact/src/Widgets`
- `Artifact/src/Service`
- `Artifact/src/Effect`
- `ArtifactCore/include/Video`
- `ArtifactCore/include/Render`
- `ArtifactCore/src/Video`
- `docs/planned/MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md`
- `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`
- `docs/done/EXPORT_PIPELINE_INVESTIGATION_2026-03-20.md`

## Non-Goals

- 全エンコーダー設定の専門用語を完全に廃止すること
- 出力形式のサポート範囲を減らすこと
- アルファの仕様を曖昧にしたまま自動化だけを増やすこと
- 既存の render queue の基本構造を壊すこと

## Design Principles

- ユーザー向け表現は用途ベースにする
- 内部設定は詳細パネルに退避し、普段は見せすぎない
- 透過は「ある/ない」だけでなく、`straight / premultiplied / opaque` を状態として扱う
- 書き出し前に不安を消す
- 事故を未然に止める警告は、邪魔ではなく保険として扱う

## Functional Requirements

### 1. User-Friendly Presets

- 次のような用途名を標準プリセットとして用意する
  - `背景透過動画(WebM/VP9)`
  - `編集ソフト用(ProRes + アルファ推奨)`
  - `静止画連番(PNG + 透明)`
- 表示名から目的と alpha の期待値が分かるようにする
- 必要に応じて内部形式の詳細を折りたたむ

### 1-1. Beginner-Friendly Format Matrix

初心者向けには、まず「用途」で選び、その後に
`コンテナ` と `コーデック` を確認できるようにする。

| 用途 | コンテナ | コーデック | Alpha | 備考 |
|---|---|---|---|---|
| 背景透過動画 | WebM | VP9 | あり | Web 向け、透過を分かりやすく扱える |
| 編集ソフト用 | MOV | ProRes 4444 | あり | 他ソフト連携の定番、透過推奨 |
| 静止画連番 | PNG sequence | PNG | あり | 1 フレームごとに透明を維持 |
| 高画質配布 | MP4 | H.264 / H.265 | なし | 透過不要の配布向け |
| 軽量確認用 | MP4 | H.264 | なし | まず確認したいとき用 |

- `コンテナ`
  - どのファイルの「箱」に入れるか
- `コーデック`
  - 中身の圧縮方式
- `Alpha`
  - 透過が残るかどうか
- `備考`
  - 初心者向けの一言説明

### 1-2. Container Guidance

- `WebM`
  - ブラウザや web 配布で分かりやすい
- `MOV`
  - 編集ソフト向けの中間素材として扱いやすい
- `PNG sequence`
  - 静止画連番で、1 枚ずつ alpha を保ちやすい
- `MP4`
  - 最もわかりやすい一般配布用だが、透過には向かないことが多い

この整理で、ユーザーは「形式名」ではなく
**何のための出力か** と **透過が残るか** を先に判断できる。

### 1-3. Plain-Language Rule

UI 上では、コンテナとコーデックの役割を次の一文で説明できるようにする。

- `コンテナ` は、映像・音声・字幕・メタデータを入れる箱
- `コーデック` は、映像や音声の中身をどう圧縮するか
- `MP4 / MOV / MKV / WebM` は「箱の種類」
- `H.264 / H.265 / ProRes / VP9 / AV1` は「中身の種類」

このため、`MP4 だから高画質` ではなく、画質の主役はコーデックであることを明示する。
UI の標準案内は `迷ったら MP4 + H.264 + AAC` を基本にし、
透過が必要なときだけ `MOV + ProRes 4444` や `WebM + VP9` を候補として前に出す。

### 2. Alpha Mode Clarity

- 出力時の `RGB / RGB+Alpha / Alpha only` を、用途プリセットから自動提案する
- `straight` と `premultiplied` の状態を明示する
- 入出力の alpha 形式が違うときは、変換方針を UI 上で見えるようにする

### 3. Preflight Alpha Check

- 書き出し前に、透過縁・黒フリンジ・白フリンジの兆候をチェックする
- `alpha edge check` で、境界に不自然な縁が出やすい素材を検出する
- 問題がある場合は、修正候補を提示する

### 4. Preview And Compare

- `straight / premultiplied` の差を、簡易プレビューで比較できるようにする
- 背景色を切り替えて、縁問題を見つけやすくする
- export 前に最終結果の見え方を確認できるようにする

## Phases

### Phase 1: Preset Vocabulary

- 目的:
  - 出力プリセット名を用途ベースに整える

- 作業項目:
  - 既存形式を用途名にマッピングする
  - 編集ソフト向け / web 向け / 連番向けを整理する
  - 詳細設定への導線を残す

- 完了条件:
  - ユーザーが名称だけで方向性を把握できる
  - `RGB+Alpha` を読まなくても判断しやすい

### Phase 2: Alpha State Contract

- 目的:
  - alpha の意味を内部で一貫させる

- 作業項目:
  - `opaque / straight / premultiplied / alpha-only` の契約を定義する
  - encoder / renderer / preview で状態を共有する
  - 変換が必要なときの責務を明確にする

- 完了条件:
  - どこで alpha 形式が決まるか説明できる
  - 黒背景事故の原因が追える

### Phase 3: Alpha Edge Check

- 目的:
  - 縁の破綻を書き出し前に見つける

- 作業項目:
  - 境界ピクセルの検査
  - ストレート/プリマルチの不一致検出
  - 背景切替での見え方確認

- 完了条件:
  - 白フリンジ / 黒フリンジの兆候を警告できる
  - 書き出し前に確認できる

### Phase 4: Export Preview And Guidance

- 目的:
  - 迷いを減らす

- 作業項目:
  - export preview に alpha overlay を追加
  - 推奨設定を用途ごとに示す
  - 失敗時のメッセージを具体化する

- 完了条件:
  - 初心者でも透過設定を選びやすい
  - 事故後の修正ではなく事前予防ができる

## Related Milestones

- `docs/planned/MILESTONE_RENDER_QUEUE_2026-03-22.md`
- `docs/planned/MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md`
- `docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md`
- `docs/planned/MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md`
- `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`
- `docs/done/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`

## Acceptance Checklist

- 用途ベースのプリセット名で選べる
- alpha の状態が明確に表示される
- straight / premultiplied の違いを確認できる
- 書き出し前に縁問題を検出できる
- 黒背景事故の発生率を下げられる

## Next Step

最初に、既存の render preset / encoder setting の一覧を用途名へマッピングし、
その次に alpha 状態の contract と preflight check を整理する。

## Static audit follow-up (2026-08-15)

- `ExportMatrix` の schema／variant／preset／cell rule、WorkspaceAutomation の解決・job生成、現行 composition の queue 入口を確認した。
- `ArtifactRenderOutputSettingDialog` には format preset、container／codec guide、alpha／premultiplied guide、frame-rate preflight、multi-channel UI、output package guide がある。旧来の「出力形式だけで用途が分からない」という状態は部分的に改善されている。
- ただし、straight／premultiplied の実画像比較、alpha edge／fringe の専用検査、警告の一元集約、preview と最終 export の parity は確認できない。
- 実ファイルの alpha 保持、各 codec／container の組み合わせ、preflight の runtime 挙動は未検証。ビルド／テストは実行していない。

追加確認:

- `ArtifactRenderQueuePresets` には MP4/MOV/WebM、PNG/APNG/WebP、EXR などの形式別プリセットと説明があり、形式マトリクスのデータ層は旧計画より進んでいる。
- `ArtifactRenderQueueService::preflightRenderQueueAt()` はコンテナ・音声・コーデック互換性などの一般的なジョブ検査を行うが、alpha edge／黒・白フリンジを検査する処理は確認できない。
- `ImageExporter` とレンダーキューの alpha 入出力経路は存在するものの、straight／premultiplied を比較表示する専用 UI はコード検索で確認できない。

判定: **Export Matrix／preset／出力設定ガイドは実装済み。alpha clarity の実画像検査、比較／preflight の統一、runtime 検証は pending。**
