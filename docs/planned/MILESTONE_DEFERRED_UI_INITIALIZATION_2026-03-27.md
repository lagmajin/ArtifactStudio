# Deferred UI Initialization / Lazy Load (2026-03-27)

## Goal

アプリ起動直後の重い初期化を減らし、最初の表示と初回操作の待ち時間を短くする。

特に以下のような「数が多い」「素材が重い」「見えないのに全部作っている」経路を後回しにする。

- SVG / icon の同期読み込み
- thumbnail の同期生成
- dock / viewer / inspector の初回全件構築
- large list / tree の初期展開
- playback / render / diagnostic の重い初期接続

## Motivation

現在のアプリは、機能が増えるほど起動時の eager load が効いてくる。
実際に、

- `Asset Browser` の thumbnail
- `Contents Viewer` のメディア種別切替
- `Timeline` の各種 icon
- `Console` / `Diagnostics` の初期表示

は、ユーザーが最初に触る瞬間より前に全部準備しなくても成立する。

## Definition Of Done

- 起動直後に必須でない UI が後から構築される
- icon / thumbnail / preview の重い処理が段階化される
- first paint / first interaction が軽くなる
- 表示順序と状態同期が崩れない
- lazy load しても項目の見え方が不自然にならない

## Work Packages

### 1. Icon Warmup Split

対象:

- toolbar
- playback control
- timeline / layer panel
- asset browser

内容:

- 共有 icon cache を持つ
- 起動時は最低限の icon のみ先に読む
- それ以外は first show / first use で warmup する

状態:

- 2026-03-27 時点で、いくつかの widget で delayed icon load の方向に寄せ始めた
- 2026-03-27 時点で、`ArtifactContentsViewer` の video / 3D model page を first use まで遅延生成するようにした
- 2026-03-27 時点で、`ArtifactPropertyWidget` の初回 rebuild を show 後の event loop に遅延するようにした

### 2. Thumbnail / Preview Deferral

対象:

- asset browser
- contents viewer

内容:

- 一覧表示時は placeholder を出す
- thumbnail は queue / timer / background task で少しずつ埋める
- visible 範囲だけ優先して読む

状態:

- 2026-03-27 時点で、`Asset Browser` の thumbnail warmup queue を入れ始めた

### 3. Dock / Panel First Show Load

対象:

- console
- diagnostics
- properties
- contents viewer

内容:

- panel 初回表示まで heavy widget を作らない
- hidden panel の data refresh を遅延する
- パネル切替時だけ必要な state を構築する

### 4. Deferred Playback / Render Bootstrap

対象:

- playback service
- render queue

内容:

- 起動時に全部の backend を初期化しない
- 最初に必要な backend だけ作る
- diagnostic / warmup を初回再生後に回す

### 5. State Sync Safety

対象:

- project / composition / selection sync
- layer panel
- timeline

内容:

- lazy load しても current composition / current layer の追従が壊れないようにする
- 初回 load 後に state を再同期する

## Recommended Order

1. Icon Warmup Split
2. Thumbnail / Preview Deferral
3. Dock / Panel First Show Load
4. Deferred Playback / Render Bootstrap
5. State Sync Safety

## Notes

このマイルストーンは「見た目を変える」より「最初の体感を軽くする」ためのもの。
`Asset Browser`、`Contents Viewer`、`Timeline`、`Console` のような高頻度 UI から効く。
