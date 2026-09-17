# Milestone: Composition Editor Selection / Comparison Upgrade (2026-06-27)

**Status:** 部分完了（選択・A/B 切替・比較状態は存在。Diff／固定参照フレームの比較描画接続と runtime 検証は未完了）
**最終更新:** 2026-09-17

## 2026-09-17 実装予定の追記

- **優先度:** 矩形指定の部分プレビュー（Interactive ROI）の後続。ユーザー依頼により比較表示の完成を実装予定として記録する。今回の作業は文書更新のみ。
- **静的確認:** `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` に Compare: Diff／Reference Pin の導線、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` に `setCompositionCompareMode` 等の状態管理がある。一方、今回の調査では Diff 状態・固定参照フレームを消費する VP 比較描画を確認できなかった。A/B の State Variant 切替は存在するため、比較機能全体を未実装とは扱わない。
- **予定範囲:** 比較元と比較先の契約を決め、固定フレームの画像保持・更新・解除と Diff の GPU 表示を既存 VP 経路へ接続する。色空間・解像度・alpha の扱いを合わせ、編集後のキャッシュ更新と参照側の固定を区別する。
- **責務:** Contents Viewer の既存 Wipe／Split／Difference と VP の比較は別サーフェス。下絵用の参照画像 overlay と固定フレーム比較も区別する。Wipe 追加は [M-VP-F の F4](MILESTONE_VP_PLANE_IMAGE_DIRECT_EDIT_TODO_2026-09-04.md) と調整し、まず既存 Diff／Pin の表示を成立させる。
- **受入条件:** 同一画像の差分がゼロ、変更箇所のみ差分が出る、シーク後も参照フレームが変わらない、解除で通常表示に戻る、比較表示が Render Queue 出力へ混入しない。保存／再読込時の扱いは実装前に決める。
- **制約／未確認:** 新規 signal/slot、Qt 合成、暗黙の画像変換を追加しない。GPU リソース容量・寿命と D3D12／Vulkan の表示整合を確認する。実装規模は中〜大、描画接続と実機挙動は要検証。

以下の旧監査でいう「A/B／Diff・reference frame」は状態基盤を含む表現であり、比較画像の完成を保証しない。

## 2026-08-15 現行コード監査

矩形選択・selection HUD・A/B／Diff・reference frame の状態モデルと、Contents Viewer 側の compare A/B／wipe／swap 設定、FrameDebug の compare mode は現行コードで確認できる。SelectionManager による複数 layer 選択と既存の viewport 操作経路も存在する。

一方、viewport 内 lasso 選択は `Alt` で開始し、`Shift` 追加／`Ctrl` toggle の selection mode と組み合わせる実装を確認した。ただし Shift／Alt の選択契約を全ツールモードで統一すること、比較状態の一貫した HUD／context menu 導線、reference frame の project 保存、複数選択と比較の runtime E2E は未確認。Contents Viewer の compare API と Composition Editor の A/B/Diff を同一機能として数えない。

判定: **矩形／lasso選択・比較／reference 基盤は部分実装、全モード操作契約・永続化・runtime 検証は pending。**
**Goal:** コンポジットエディタのビューポート上で、複数選択と A/B 比較を素早く行えるようにする。

---

## ねらい

コンポジット編集では、対象を「選ぶ」操作と、結果を「見比べる」操作の往復が多い。
この往復が遅いと、編集そのものより確認作業が重く感じられる。

このマイルストーンでは、ビューポートを単なる表示面ではなく、
選択と比較の中心導線として使えるようにする。

---

## 現状の課題

- 単体クリック中心だと、複数レイヤーの選択に手数がかかる
- 選択モードが見えにくいと、追加選択や除外選択で迷いやすい
- A/B 比較の導線が弱いと、変更前後の確認に時間がかかる
- 参照フレーム固定がないと、基準の見失いが起きやすい
- 差分確認が別画面寄りだと、編集の流れが切れやすい

---

## 改善方針

### Phase 1: 範囲選択基盤

- 矩形選択を追加する
- `Shift` で追加選択を行えるようにする
- `Alt` で除外選択を行えるようにする
- 現在の選択モードを HUD で明示する

### Phase 2: 比較導線

- A/B 切替をビューポート内で行えるようにする
- 参照フレームを固定できるようにする
- 差分オーバーレイを HUD から切り替えられるようにする

### Phase 3: 選択方式拡張

- ラッソ選択を追加する
- 矩形選択と同じ選択モード体系に載せる
- 選択結果のハイライトをビューポートで分かりやすくする

### Phase 4: 操作の読みやすさ

- 選択数、比較モード、参照フレームの状態を簡潔に表示する
- 解除操作やモード終了を迷いにくくする
- コンテキストメニューから比較関連のクイック操作へ入れるようにする

---

## 実装の着手候補

1. 矩形選択と追加 / 除外選択を実装する
2. A/B 切替の HUD 表示を追加する
3. 参照フレーム固定の状態を持たせる
4. 差分オーバーレイの切替を追加する
5. ラッソ選択を追加する

---

## 成功条件

- ビューポートだけで複数選択の主要操作が完結する
- A/B 切替と参照固定が少ない操作数で行える
- 比較状態が画面上で迷わず読める
- 選択と比較の往復が速くなる

---

## リスク

- 選択モードが増えすぎると、かえって迷いやすくなる
- 比較系の HUD を増やしすぎると、編集対象の視認性が落ちる
- 選択ロジックと比較ロジックを混ぜると、責務境界が崩れやすい

---

## 参照

- [`COMPOSITION_EDITOR_CONTRACT.md`](x:/Dev/ArtifactStudio/docs/COMPOSITION_EDITOR_CONTRACT.md)
- [`WIDGET_MAP.md`](x:/Dev/ArtifactStudio/docs/WIDGET_MAP.md)
- [`MILESTONE_COMPOSITION_EDITOR_RUBBER_BAND_MULTI_SELECTION_2026-03-26.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_RUBBER_BAND_MULTI_SELECTION_2026-03-26.md)
- [`MILESTONE_COMPOSITION_EDITOR_PLAYBACK_FEEL_REFINEMENT_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_PLAYBACK_FEEL_REFINEMENT_2026-04-23.md)

---

## 次の一手

1. 選択モードの UI 文言を確定する
2. A/B 比較の状態表現を決める
3. 参照フレーム固定の操作導線を決める
