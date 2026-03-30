# Composition Notes / Scratchpad Milestone (2026-03-30)

`Composition` や `Layer` に紐づく軽量なメモを残せるようにするマイルストーン。
レビューコメントや正式ドキュメントより前段の、制作中の「書きなぐり」を保持するための導線を作る。

## Goal

- コンポジションに自由記述のメモを残せる
- レイヤー単位でも短い注釈を残せる
- 必要なら特定フレームにノートを残せる
- 書式付きドキュメントではなく、気軽に書いてすぐ閉じられる
- 保存・復元・検索の最低限を担保する

## Scope

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Layer`
- `Artifact/src/Composition`
- `Artifact/src/Project`

## Non-Goals

- 完全なノートアプリ化
- リッチテキスト前提の長文編集機能
- コメントスレッドや共同編集の全面実装
- レビューツールの置き換え

## Phases

### Phase 1: Composition Note

- コンポジション本体にプレーンテキストの note を持たせる
- `composition note` を inspector か editor の小さなパネルで編集できるようにする
- 保存・復元を project snapshot に含める

### Phase 2: Layer Note

- 各 layer に短い note を持たせる
- `layer note` は property panel / inspector から即編集できるようにする
- 「差し替え予定」「仮素材」などの簡単な印を残せるようにする

### Phase 3: Frame Note / Marker Note

- 特定フレームに短い note を紐づけられるようにする
- timeline 上で note の存在を軽く示す
- hover か click で内容を確認できるようにする

### Phase 4: Scratchpad And Search

- コンポジション横に、いつでも開ける scratchpad を用意する
- note の全文検索を簡易に追加する
- note から composition / layer / frame へジャンプできる導線を作る

## Recommended Order

1. `Phase 1: Composition Note`
2. `Phase 2: Layer Note`
3. `Phase 3: Frame Note / Marker Note`
4. `Phase 4: Scratchpad And Search`

## Current Status

- いまのプロジェクトには review / annotation 系の入口はあるが、制作中の雑多なメモを溜める軽い受け皿は薄い
- `behavior.note` など局所的な note はあるが、コンポジション横断のメモとしてはまだ足りない
- まずは `Composition Note` を先に作るのが最も自然

## Validation Checklist

- コンポに短いメモを残して保存・再起動しても復元される
- layer note を inspector から即編集できる
- timeline 上で frame note の存在が分かる
- 重い編集 UI にならず、思いついた時にすぐ書ける
- review / annotation と用途が混ざらない
