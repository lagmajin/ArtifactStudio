# マイルストーン: Project View Tile Mode

> 2026-06-05 作成

## 目的

`ArtifactProjectManagerWidget` を、構造を見る `Project View` のまま、サムネイル中心の tile mode でも使えるようにする。

ユーザーが言う「メディアブラウザっぽい感じ」を、Project View の責務を壊さずに取り込む。

---

## 判断

- これは `Project View` の新規別画面ではなく、既存 view の presentation 拡張として扱う
- 既存の `list / grid` 切替の延長に置くのが自然
- `Asset Browser` と競合しないよう、Project View 側は「project 構造の確認」と「次の操作への導線」を残す

結論としては「いける」が、最初から Asset Browser そのものに寄せすぎない方が安全。

---

## Goal

- Project View を list / tile で切り替えられる
- tile mode ではサムネ、種別、尺、解像度、状態が見やすい
- project item の構造感と選択同期を壊さない
- 低密度では情報が読めて、高密度でも破綻しない

---

## Why Now

- 既存の Project View は selection / basic operation の土台がある
- `Project View Search / Filter / Presentation` で list / grid の方向性が既に整理されている
- サムネイルが並ぶ presentation は、project の中身を素早く見渡す用途に効く

---

## Scope

### In Scope

- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx`
- 必要に応じて `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 必要に応じて `Artifact/src/Widgets/Project/` 周辺の presentation helper

### Out Of Scope

- Asset Browser の全面再設計
- Contents Viewer の大改造
- 新しい global signal / slot の追加
- QtCSS の追加
- `QColorDialog` の新規採用

---

## Follow-up Direction

tile mode に加えて、Project View を「見るだけの表面」から「その場で触れる詳細面」に寄せる。

追加したいもの:

- composition の inline settings editor
- 複数 composition に対する frame rate 一括変更
- unused asset の状態確認を current health / summary から読みやすくする表示

狙い:

- `Composition Settings...` のモーダルを毎回開かなくても、軽い修正は Project View だけで済む
- 複数 comp の fps 揃えを短い導線で行える
- unused 表示を search / filter だけでなく、status chip 側でも読めるようにする

---

## Current Ground Truth

- `Project View` は project 構造の正として扱う
- `Asset Browser` は探索の正として扱う
- selection / current composition の同期は壊さない
- presentation の切り替えは、責務を増やすのでなく見せ方を変える方向で行う

---

## Problem Statement

今の Project View が list 寄りだけだと、次の情報がぱっと読みづらい。

- どの素材が何なのか
- どの素材が最近触る対象か
- どの素材にサムネがあるか
- どの素材が未使用、欠損、または注目対象か

メディアブラウザ的な tile 表示があると、素材の見た目と状態を一度に追いやすくなる。

---

## Design Rules

- Project View は「構造」と「状態」を読む場所に留める
- tile mode は派手さより読みやすさを優先する
- サムネは主役だが、種別や状態を隠しすぎない
- selection は強く見せるが、タイル全体を潰さない
- list と tile で同じ item を見ていることが分かるようにする

---

## Phases

### Phase 1: Presentation Switch

目的:
Project View に list / tile の表示切替を導入する。

作業項目:

- 現在の list 表示を保持する
- tile mode の切替 UI を追加する
- 表示モードごとの state を明確に分ける
- toggle 後も selection と current item を維持する

Done when:

- list と tile を切り替えられる
- 切り替えで選択が消えない

### Phase 2: Tile Layout

目的:
タイルとして成立する基本レイアウトを作る。

作業項目:

- thumbnail 領域
- item title
- type badge
- duration / size / fps / resolution などの要約
- unused / missing / relinked などの状態表示

Done when:

- ひと目で item の種類と状態が分かる
- 省スペースでも最低限の意味が読める

### Phase 3: Thumbnail Pipeline

目的:
タイルの見た目を素材の内容に近づける。

作業項目:

- thumbnail の優先取得ルールを決める
- 取得できない場合の fallback icon を用意する
- heavy な素材で遅延更新しても UI が固まらないようにする
- audio / video / image / comp で見せ方を分ける

Done when:

- tile が単なるアイコン一覧ではなくなる
- 初期表示の体感が壊れない

### Phase 4: Status Affordance

目的:
tile mode でも project 状態が読めるようにする。

作業項目:

- unused / missing / relinked / imported を見える化する
- selection state と project state を分けて表現する
- item count や filter state を表面に出す

Done when:

- 状態が tile だけで追える
- 選択と project status が混ざらない

### Phase 5: Density And Resize Polish

目的:
サイズが変わっても崩れないようにする。

作業項目:

- tile size の可変化
- 余白と文字量の調整
- 高密度表示時の省略ルール
- hover / selection の強さ調整

Done when:

- 小さくしても大きくしても破綻しない
- 一覧性と情報量のバランスが保てる

### Phase 6: Workflow Bridge

目的:
Project View から次の作業へ進みやすくする。

作業項目:

- double-click / context menu の導線整理
- viewer / timeline / render への次操作を分かりやすくする
- Asset Browser と見え方が完全に乖離しないようにする

Done when:

- tile view から次の行動に移りやすい
- Project View と Asset Browser の役割分担が維持される

---

## Implementation Notes

- まずは既存の Project View にモードを足す
- 新しい画面を別に作るより、既存の `ArtifactProjectManagerWidget` の presentation を拡張する
- サムネは重いので、必要なら遅延取得やキャッシュ前提で考える
- tile mode は「見て選ぶ」導線に強く、構造編集は今の list 側が担う想定でよい

### Suggested First Pass

- list / tile toggle を追加
- tile にサムネと主要バッジを出す
- selection と current item の同期を維持する
- まずは主要素材種別だけ見せ方を分ける

---

## Done Criteria

- Project View を list / tile で切り替えられる
- tile mode でも project 構造が読める
- selection / state sync が壊れない
- Asset Browser と役割が被りすぎない

---

## Related Docs

- [MILESTONE_PROJECT_VIEW_SEARCH_FILTER_PRESENTATION_2026-04-03.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROJECT_VIEW_SEARCH_FILTER_PRESENTATION_2026-04-03.md)
- [MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md)
- [MILESTONE_ASSET_BROWSER_NAVIGATOR_SEARCH_PRESENTATION_2026-04-03.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_ASSET_BROWSER_NAVIGATOR_SEARCH_PRESENTATION_2026-04-03.md)

## Static Audit (2026-07-25)

`ArtifactProjectView` は `PresentationMode::List` / `Tile` を持ち、Project Manager の view mode combo から切り替えられる。Tile 描画には thumbnail cache、fallback 表示、タイトル、種別 badge、metadata、missing／unused／status／proxy badge、hover／selection 表現があり、Ctrl+wheel による tile 密度調整も確認できる。Tree 側と同じ selection model・visible rows を利用しているため、表示切替で別の item 集合を持つ設計ではない。Project View からの open、reveal、proxy、relink 等の次操作導線も存在する。

一方、thumbnail の全素材種別での表示品質、重い素材を含む初期表示性能、Tile と Asset Browser の見え方の整合、selection/current composition の実動作、各密度での文字省略と runtime 視認性は未検証である。List／Tile の名称は元仕様の list／tile と対応するが、grid／freeform は対象外で、Tile の構造表示も flat な visible rows の描画に依存する。

判定: **Phase 1〜5 の主要実装は確認できる。** Phase 3・Phase 6 と Done Criteria の runtime／性能／責務境界検証が残っている。
