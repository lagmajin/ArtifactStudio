# マイルストーン: Review / Compare / Annotation

> 2026-03-28 作成

## 目的

`Contents Viewer` を、単体プレビューだけでなく、差分確認・レビュー・注釈のための inspection surface にする。

このマイルストーンは、`source / final / compare` の切り替えだけでなく、確認結果を次の操作に繋げることを狙う。

---

## 背景

Contents Viewer は image / video / 3D model を見られるようになってきたが、運用上はまだ「見る」までで止まりやすい。

- before / after の比較が弱い
- 変更点をメモする導線がない
- レビュー結果を project / asset / render に戻しにくい
- source と final の差が UI 上で埋もれやすい

---

## 方針

### 原則

1. Viewer は編集より確認を優先する
2. Compare は見た目の差を見せ、Annotation は判断を残す
3. 1 回のレビューを project / asset / render の次の操作に戻す
4. `source / final / compare` の意味を固定する

### 想定対象

- image
- video
- 3D model
- generated asset
- export result

---

## 既存資産

- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
- `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`

---

## Phase 1: Compare View Baseline

### 目的

2 つの状態を比較する基本経路を作る。

### 作業項目

- `Source` と `Final` の切替
- `Compare` の分割表示
- playback / scrub と compare の同期
- 3D model の view state と conflict しない設計

### 完了条件

- 比較対象が分かる
- compare 中の操作が迷わない

---

## Phase 2: Review Surface

### 目的

比較結果をレビューとして扱えるようにする。

### 作業項目

- review note
- bookmark
- status chip
- approved / needs work / rejected の簡易状態

### 完了条件

- review の結果を残せる
- 何を確認したかが追える

---

## Phase 3: Annotation Hooks

### 目的

レビュー結果を後で見返せる形にする。

### 作業項目

- note / marker
- timestamp / frame への紐付け
- asset / render result への関連付け
- copyable summary

### 完了条件

- 記録した内容が再参照できる
- viewer が単なる再生窓に留まらない

---

## Phase 4: Workflow Return

### 目的

レビューから次の作業に戻す。

### 作業項目

- project へ戻す導線
- asset browser へ戻す導線
- render queue への retry / re-export 導線
- compare 結果からの open source / open output

### 完了条件

- review が workflow の末端で終わらない
- viewer が project / asset / render と往復できる

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

---

## Current Status

2026-03-28 時点で、`Contents Viewer` は preview / metadata / open flow を持っているが、review と annotation の層はまだ薄い。

この文書は `Contents Viewer Expansion` の Phase 3 以降を、レビュー用の独立した workstream として切り出す役割を持つ。

---

## Next Execution Slice

Phase 1 は、比較の意味を先に固定してから UI を広げる。

### Phase 1A の着手点

1. `Source` と `Final` の切替位置を viewer 内で固定する
2. `Compare` は分割表示として扱い、差分を見せることを主目的にする
3. playback / scrub と compare の同期方針を先に決める
4. 3D model の view state と compare state が衝突しないようにする

### Phase 1 完了条件

- 比較対象が分かる
- compare 中の操作が迷わない
- source / final / compare の意味が固定される

### Phase 2A の着手点

1. review note / bookmark / status chip の最小表示を用意する
2. approved / needs work / rejected の 3 状態に絞る
3. review note は compare state と独立に残せるようにする
4. 何を確認したかが短く追えるようにする

### Phase 2 完了条件

- review の結果を残せる
- 何を確認したかが追える
- status 表示が compare UI を邪魔しない

### Phase 3 への前提

- annotation は compare と review note が安定してから足す
- timestamp / frame の紐付けは state 契約が固まってからにする

---

## Related

- `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
- `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`
- `docs/planned/MILESTONE_EXPORT_REVIEW_SHARE_2026-03-27.md`

## Static Audit (2026-07-25)

現行ソースでは、`ArtifactContentsViewer` に compare page、source A/B path、wipe/swap 設定、画像・動画の比較表示と再生／scrub の基盤があり、shortcut binding も compare 操作を持つ。3D viewer、metadata、open flow、timeline marker／bookmark 系の既存基盤も確認できる。

ただし、レビュー用の統一 `Review` / `Annotation` データ契約、approved/needs-work/rejected 状態、note の永続化、timestamp/frame と asset/render result の関連付け、copyable summary、project/asset/render への workflow return は専用経路として確認できない。比較は主に viewer 内の表示状態であり、複数対象を同時に扱うレビュー記録や再起動後の再参照まで実装済みとは判定しない。

判定: Phase 1 の compare surface は partial-to-substantial、Phase 2〜4 の review/annotation/workflow return は未完了または未検証。
