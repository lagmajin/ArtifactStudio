# マイルストーン: Project / Asset Workflow

> 2026-03-27 作成

## 目的

Project View と Asset Browser を別の UI ではなく、同じ制作ワークフローの入口として揃える。

このマイルストーンは、import / relink / missing / unused / recent / favorite / dependency を横断して、
「素材がどこにあるか」「今どういう状態か」「どう次へ進めるか」を一貫して読めるようにする。

---

## 背景

現状でも project / asset の基盤はあるが、分散している。

- Project View は project 構造の把握に強い
- Asset Browser は素材探索に強い
- Contents Viewer は個別ファイルの確認に強い
- Source Abstraction は missing / relink の基盤を持つ

ただし、これらが workflow としてまとまっていないため、次の操作が途切れやすい。

- import した asset が project にどう反映されたか分かりにくい
- missing / unused / relink state の見え方が UI ごとに揃っていない
- browser から timeline / viewer / render への導線が弱い
- save / load 後の asset 状態確認が面倒

---

## 方針

### 原則

1. Project View は構造の正、Asset Browser は探索の正として扱う
2. source state は Core の source / relink 基盤に寄せる
3. UI は状態の表示と次の操作の導線に徹する
4. import / relink / missing / unused の意味を揃える
5. 迷いやすい操作は context menu ではなく明示 action で用意する

### 想定対象

- project / composition / folder / bin
- image / video / audio / vector / source file
- missing / unused / relinked / imported
- recent / favorite / dependency

---

## 既存資産

- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Project/ArtifactProjectModel.cppm`
- `Artifact/src/Service/ArtifactProjectService.cpp`
- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`
- `Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md`
- `docs/done/MILESTONE_SOURCE_ABSTRACTION_CORE_2026-03-25_DONE.md`
- `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`

---

## Phase 1: Selection / State Sync Foundation

### 目的

Project View と Asset Browser の selection と state を一致させる。

### 作業項目

- current composition と selected asset の同期
- browser / project で同じ item を指せるようにする
- missing / unused / relinked / imported の state 表示を揃える
- selection change 時の preview / inspector 更新を明示化する

### 完了条件

- どちらの view から選んでも同じ item が追える
- selection を切り替えた時に state がズレない

---

## Phase 2: Import / Relink / Missing Flow

### 目的

素材の取り込みと復帰を、project workflow として完結させる。

### 作業項目

- import 結果の即時反映
- relink candidate の列挙
- bulk relink
- missing asset search root
- drag & drop import の整理

### 完了条件

- import と relink が別の操作に見えない
- missing から復帰までの導線が短い

---

## Phase 3: Asset Presentation / Metadata

### 目的

project item の状態を、一覧だけで読めるようにする。

### 作業項目

- thumbnail / type icon / size / duration / fps / resolution 表示
- dependency badge
- source path / relink state 表示
- empty / unsupported / load failure の区別

### 完了条件

- asset の種類と重要状態が見える
- Project View と Asset Browser の見え方が大きく乖離しない

---

## Phase 4: Organization / Collections

### 目的

素材のまとまりを、project と browser の両方で扱いやすくする。

### 作業項目

- folder / bin / tag / favorite / recent
- virtual collections
- unused / duplicate / missing の整理
- smart bin の入口

### Future Extension Notes

- `Metadata Tagging`
  - クリップやアセットにキャラ名・シーン番号・テイク番号・評価を持たせる。
  - まずは検索や一覧表示に効く軽量メタデータとして扱い、後からフィルタ条件へ拡張する。
- `Smart Collections / Bins`
  - 条件ベースでアセットを動的に集約する仮想コレクションを設ける。
  - 例: `★3以上の未使用素材`, `このシーンに関連する素材`, `最近触った素材`
  - folder は物理整理、collection は条件整理として責務を分ける。

### 完了条件

- 素材を探す経路と整理する経路が両方ある
- folder と collection の責務が混ざらない

---

## Phase 5: Workflow Bridges

### 目的

Project / Asset から次の作業へすばやく飛べるようにする。

### 作業項目

- browser から timeline への追加
- browser から contents viewer への open
- project から render queue への投入
- recent / favorite / missing asset の action surface
- project view の double-click から footage review を Contents Viewer へ送る

### 完了条件

- 素材を見つけてから使うまでが短い
- viewer / timeline / render への導線が自然
- project と asset で review 導線が違いすぎない

---

## Phase 6: Save / Restore Integrity

### 目的

再起動や再読込後も asset workflow が壊れないようにする。

### 作業項目

- imported / relinked path の復元
- missing state の保持と再評価
- selected / active composition の整合
- project validation との接続

### 完了条件

- 再読込後に state が崩れない
- missing / relink / imported の差分が維持される

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5
6. Phase 6

---

## Current Status

2026-03-27 時点で、Project View / Asset Browser / Contents Viewer の個別改善は進んでいるが、
それらを一枚の workflow として束ねる専用 milestone はまだ弱い。

この文書はその結節点として扱う。

---

## Next Execution Slice

Phase 1 は、Project View と Asset Browser が同じ item を指していると読める状態を先に作る。

### Phase 1A の着手点

1. current composition と selected asset の同期先を 1 本に寄せる
2. browser / project で同じ item を追える selection contract を決める
3. missing / unused / relinked / imported の state 表示を揃える
4. selection change 時に preview / inspector が追従する前提を作る

### Phase 1 完了条件

- どちらの view から選んでも同じ item が追える
- selection を切り替えた時に state がズレない
- preview / inspector の更新責務が読める

### Phase 2A の着手点

1. import 結果の即時反映を workflow に組み込む
2. relink candidate の列挙を missing state から直接辿れるようにする
3. bulk relink と drag & drop import を別操作にしすぎない
4. missing asset search root を selection contract の延長として扱う

### Phase 2 完了条件

- import と relink が workflow としてつながる
- missing から復帰までの導線が短い
- state 表示と操作導線が食い違わない

### Phase 5 への前提

- browser から timeline / viewer / render への橋渡しは selection state が揃ってから詰める
- save / restore は import/relink が安定してから別途固める
## 2026-07-25 実装監査

### 判定

Phase 1〜6 の基盤はかなり実装済みだが、Project View と Asset Browser をまたぐ一貫した workflow としては未完了。状態表示・import/relink・organization・workflow bridge の個別機能は存在する一方、実行時の cross-view 同期と save/restore 後の通し確認は未検証として扱う。

### 実装確認

- Project Model は composition / footage を project item として構築し、footage の path、type、duration / frame rate / resolution、missing 表示と missing 色を提供している。
- Project Manager 側には importable path の収集、footage の参照影響確認、composition / footage の利用先判定、missing 判定、proxy 状態表示など、project item から利用先へ進む基盤がある。
- Asset Browser 側には selection、recent / favorite、Imported / Favorite / Missing / Unused の status filter、thumbnail / type / status marker、relink undo command、directory selection 同期がある。
- Asset / project service 側の source registry・relink・状態保存基盤は別マイルストーンで整備されているため、ここでは UI 間の共通導線を重複実装しない。

### 未完了・要確認

- Project View で選択した footage と Asset Browser の選択を同一 item として相互追跡する明示的な runtime bridge は、コード上の個別 selection 処理だけでは完了と断定できない。
- import 結果を Project View と Asset Browser の両方へ即時反映する一連の操作、bulk relink / search root、drag & drop import の通し挙動は未確認。
- dependency badge、unsupported / load failure の統一表示、virtual collection / smart bin、timeline / Contents Viewer / render queue への導線は部分実装または別 milestone 側に分散している。
- save / load 後の selected item、active composition、missing / relinked state の整合と再評価は、実行時検証が必要。

### 次の判定

この milestone は「基盤実装済み・統合確認待ち」。Phase 1 の cross-view selection / state sync を最優先の runtime 確認対象とし、その後 Phase 2 の import / relink 通し確認へ進む。
