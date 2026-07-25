# Phase 7 Execution: App Debugger Report / Share Bundle

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の改善を、診断内容をそのまま共有できる `report / share bundle` に落とし込むための実行メモ。

この段階では、見やすくするだけでなく、見えた内容をそのまま他人へ渡せるようにする。

---

## 方針

1. report は summary first にする
2. share bundle は frame / trace / resource / note を分ける
3. copy 用 text と保存用 bundle を分離する
4. support に渡す最小情報を固定する

---

## 現状の課題

- 診断は見えても、そのまま共有しづらい
- text dump が長くなりやすい
- frame bundle と crash bundle の境界が見えづらい
- 何を貼ればサポートに伝わるかが曖昧
- 保存先や timestamp が画面から追いにくい

---

## 実装タスク

### 1. Report を summary-first にする

やること:

- 先頭に state / warning / main cause を置く
- 長い raw text は後ろに回す
- 1 行で状況が分かる要約を作る

完了条件:

- 共有前に読むべき情報が前に出る

### 2. Share Bundle を分離する

やること:

- frame bundle
- crash bundle
- playback bundle
- support note

を分けて扱う。

完了条件:

- 何を共有しているかが明確になる

### 3. Copy / Export / Save を整理する

やること:

- copy は text clipboard
- export は file save
- share は support package

として意味を分ける。

完了条件:

- ボタンの意味が混ざらない

### 4. Support Minimum を固定する

やること:

- frame index
- render backend
- playback state
- failed pass / missing resource
- timestamp

を最低限含める。

完了条件:

- サポートに渡す最小セットが毎回同じになる

---

## レイアウト方針

- 上部: report state / save target / timestamp
- 中央: current issue summary
- 右側: copyable report / raw bundle text
- 下部: share / support notes

補助ルール:

- report は要約から入る
- bundle は種類ごとに分ける
- raw text は補助に落とす

---

## 表示ルール

- `report` は読むためのもの
- `export` は保存するためのもの
- `share` は渡すためのもの
- `bundle` は再現に必要な材料だけを束ねる

---

## 実装メモ

- `AppDebuggerWidget` に report copy 入口を寄せる
- `Frame Debug View` から bundle を参照できるようにする
- `ArtifactDebugConsoleWidget` を text-first の共有面に寄せる
- 保存した bundle の path を UI で追えるようにする

---

## 完了条件

- report を 1 回でコピーできる
- share bundle を種類ごとに分けられる
- support に渡す最小情報が固定される
- saved path が UI から追える

---

## リスク

- bundle を盛り込みすぎると重くなる
- report が長くなりすぎると貼りづらくなる
- share / export / copy の意味を混ぜると操作が分かりにくくなる

---

## 参照

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md)
- App Debugger 系の phase 実行メモは親文書へ統合済み

---

## Static audit follow-up (2026-07-25)

現行の FrameDebugBundle、AppDebugger の保存、Harness の copy/save report を確認した。

| 要件 | 現状 | 判定 |
|---|---|---|
| Summary-first Report | FrameDebug/Harness に summary、warning/failure、report details の分離がある。 | 実装済み／表示確認待ち |
| Bundle Separation | frame bundle と crash report の別導線、Harness report、FrameDebug JSON bundle がある。playback bundle/support note との正式な型分離は未確認。 | 部分実装 |
| Copy / Export / Save | Copy Report、Save Report、JSON bundle persist/restore がある。各操作の surface 横断の意味統一は未確認。 | 部分実装 |
| Support Minimum | frame/reportId、createdAt、render/playback、resource/pass/failure 情報の基盤がある。saved path の一貫表示と最小セット固定は未確認。 | 部分実装／確認待ち |

### 判定

共有レポートと frame bundle は実装が進んでいるが、bundle 種別の契約、support minimum、保存先表示の完全統一が残る。Phase 7 は「部分実装／統合確認待ち」とする。

