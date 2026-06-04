# マイルストーン: Project Interprocess Copy

> 2026-06-05 作成

## 目的

別の Artifact プロジェクト、あるいは別の Artifact インスタンスから、1つのレイヤーやアセットを安全に取り出して再利用できるようにする。

After Effects の「別プロジェクトから素材だけ持ってくる」感覚を、プロジェクト全体の import に頼らずに実現する。

---

## 判断

- いける
- ただし「別プロセスの live object を共有する」のではなく、「再構成可能な bundle を受け渡す」設計が前提
- まずは clipboard / drag / paste の延長で始め、必要なら同一マシン内 IPC を足すのが安全

結論としては、インタープロセスのコピーは実現可能だが、実装単位は `shared object` ではなく `serialized bundle` にするべき。

---

## Goal

- 選択した layer / asset を別プロジェクトへ持っていける
- プロジェクト全体を import しなくても、必要な単位だけ抜き出せる
- コピー元とコピー先が別インスタンスでも、貼り付けや再利用ができる
- 依存アセットを含めて、壊れにくい形で受け渡せる

---

## Why Now

- すでに clipboard / mime / drop / import の導線がある
- `Project View` と `Asset Browser`、`Composition Editor` には既存の選択・貼り付け経路がある
- 別プロジェクト間の再利用が増えるほど、全体 import は重くなりやすい

---

## Scope

### In Scope

- `Artifact/src/Service/ArtifactClipboardService.cpp`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`
- 必要に応じて `Artifact/src/Project/ArtifactProjectManager.cppm`
- 必要に応じて `Artifact/src/Service/ArtifactProjectService.cpp`

### Out Of Scope

- live shared editing
- remote network sync
- Diligent / DX12 backend 変更
- QtCSS の追加
- `QColorDialog` の新規採用

---

## Current Ground Truth

- クリップボード経路はすでにある
- `QMimeData` ベースの drag / drop は使われている
- `importAssetsFromPaths` による外部資産取り込みはある
- selection / current composition / project item の概念は既に存在する

つまり、必要なのは新しい UI というより「交換フォーマット」と「受け口」の整備。

---

## Problem Statement

AE 的な作業では、別プロジェクトの一部だけを流用したい場面がある。

現状の全体 import 型だと次の問題が出やすい。

- 必要以上に重い
- 参照アセットの整理が難しい
- 選択した 1 レイヤーだけを安全に抜き出しにくい
- 別インスタンス間のコピーが標準化されていない

---

## Design Rules

- live object を共有しない
- まずは「再構成可能な bundle」を作る
- コピー先で壊れないことを最優先にする
- clipboard は text / mime の両方を使えるようにする
- 依存関係はできるだけ bundle に含める
- 不足依存は明示的に unresolved として扱う

---

## Bundle Model

### Bundle に含めたいもの

- item kind
- 元プロジェクト内の識別情報
- 表示名
- layer / asset の主要メタデータ
- 依存する file path / asset reference
- 必要なら簡易 preview / thumbnail
- 互換性のための version tag

### Bundle に含めたくないもの

- live pointer
- UI state の一時情報
- selection の瞬間状態そのもの
- プロセス固有の resource handle

### 受け渡し形式の候補

- clipboard text 内の JSON
- `application/x-artifact-project-bundle` の MIME
- 一時ファイルを使う bundle URI
- 同一マシン内 IPC 用の local socket メッセージ

---

## Phases

### Phase 1: Bundle Format

目的:
コピー可能な最小単位を定義する。

作業項目:

- layer / asset / composition の bundle スキーマを決める
- version を付ける
- 依存アセットの表現を決める
- 複数選択時の bundle 配列を定義する

Done when:

- bundle を JSON として説明できる
- コピー先で復元するための情報が揃っている

### Phase 2: Copy Surface

目的:
選択項目を bundle にして clipboard へ載せる。

作業項目:

- Project View / Asset Browser / Composition Editor から copy を始められるようにする
- system clipboard に bundle を入れる
- 必要なら text fallback も入れる
- 同一インスタンス内の paste と互換を保つ

Done when:

- 1 item をコピーできる
- 別インスタンスでも clipboard 経由で読める

### Phase 3: Paste And Rebuild

目的:
受け取った bundle を現在の project に復元する。

作業項目:

- asset / layer / composition ごとの復元経路を作る
- 依存ファイルがある場合は取り込みを行う
- name collision の解決ルールを決める
- 不足依存は明示表示する

Done when:

- 別 project へ貼り付けられる
- 参照が壊れても reason が追える

### Phase 4: IPC Fast Path

目的:
同一マシン内の複数 Artifact インスタンス間で即時コピーを可能にする。

作業項目:

- `QLocalSocket` / `QLocalServer` の採用可否を決める
- bundle URI または payload を送る
- clipboard を経由せずに直接渡す経路を用意する
- 失敗時は clipboard 経路にフォールバックする

Done when:

- 別インスタンス間でも即時貼り付けができる
- IPC が壊れても clipboard で退避できる

### Phase 5: Dependency Packaging

目的:
コピー先で不足しやすい依存を、できるだけ同梱する。

作業項目:

- footage / texture / preset / effect などの依存列挙
- bundle の sidecar 取り扱い
- missing asset の表示と再リンク導線

Done when:

- 1 レイヤーだけでも実用的に持ち出せる
- 外部依存がどこで欠けたか分かる

### Phase 6: UX Polish

目的:
コピーが「便利な導線」として読めるようにする。

作業項目:

- copy / paste の説明文を整える
- 成功 / 部分成功 / 失敗の通知を揃える
- Project View / Asset Browser からの導線を揃える

Done when:

- どこからコピーしても体験が近い
- 利用者が bundle / IPC の存在を意識しなくて済む

---

## Implementation Notes

- まずは clipboard ベースの bundle から始める
- IPC は後付けの fast path にする
- 交換フォーマットは拡張しやすい JSON 系がよい
- live sharing より、コピー時点の静的再構成に寄せる
- 失敗しても元プロジェクトを壊さない

### Suggested First Pass

1. bundle schema を定義する
2. system clipboard に `application/x-artifact-project-bundle` を載せる
3. `Paste` 側で bundle を読み、現プロジェクトへ import する
4. その後に `QLocalSocket` fast path を足す

---

## First Files

1. `Artifact/src/Service/ArtifactClipboardService.cpp`
2. `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`
3. `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
4. `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
5. `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
6. `Artifact/src/Project/ArtifactProjectManager.cppm`

---

## Done Criteria

- 別プロジェクトから 1 item を持ってこれる
- プロジェクト全体 import を避けられる
- 別インスタンス間でもコピー・ペーストできる
- bundle が壊れても原因を追える

---

## Related Docs

- [MILESTONE_PROJECT_VIEW_TILE_MODE_2026-06-05.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROJECT_VIEW_TILE_MODE_2026-06-05.md)
- [MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md)
- [MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md)
