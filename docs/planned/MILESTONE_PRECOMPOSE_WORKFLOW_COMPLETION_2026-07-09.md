**ステータス:** Not Started

# M-PRECOMP-2: Precompose Workflow Completion (2026-07-09)

`M-PRECOMP-1` で `PreCompose.ixx/cppm` の実装が前進したあとに残っている、**実務上の finish line** をまとめた専用マイルストーン。
ここでは「プリコンポーズ自体を呼べる」ことではなく、**戻せる・壊れない・後続機能の前提になる**ことを完了条件に置く。

## 1. 目的

- `unprecompose()` を実務レベルで完了させる
- nested workflow の undo/redo とレイヤー復元を破綻させない
- `Master Properties` などの後続マイルストーンが依存できる土台にする

## 2. なぜ今これが必要か

- `docs/analysis/REPORT_AE_GAP_UPDATE_2026-07-03.md` で **Precompose 完成 (unprecompose)** が未完了として残っている
- `docs/planned/MILESTONE_MASTER_PROPERTIES_2026-07-08.md` が、`Precompose` 完了をブロック依存として持っている
- `docs/planned/COMPOSITION_PRECOMPOSE_ANALYSIS_2026-04-17.md` でも、UI と command はあるが内部の composition 操作が未完成と整理されている

## 3. 現状整理

### できていること

- `PrecomposeDialog` とメニュー導線は存在する
- `PreComposeManager` / `PreComposeCommand` / `UnprecomposeCommand` の枠組みはある
- 2026-07-03 セッション完了レポートでは `M-PRECOMP-1 Precompose` が実装対象として扱われている

### 足りていないこと

- `unprecompose()` の実務完成度が不明確で、分析文書上は未完了扱い
- 親コンポジションへのレイヤー復元順、時間、親子参照の整理が完了条件として固定されていない
- 後続の `Master Properties` や nested workflow と責務境界がまだ弱い

## 4. スコープ

### Phase 1 - Unprecompose Core Closeout

- `unprecompose()` が parent composition へレイヤーを正しく戻す
- precompose layer の置換/削除と復元対象の再配置順を固定する
- self-reference / broken parent-child relation を拒否する

### Phase 2 - Time / Transform / Range Integrity

- 親子の時間変換で破綻しない
- in/out range、start offset、transform の復元方針を固定する
- 「見た目は戻ったが timing がずれる」状態を未完了として扱う

### Phase 3 - Undo/Redo Completion

- precompose → unprecompose → redo が安定する
- command 層と manager 層で責務を分け、片側だけが状態を持たないようにする
- nested composition を跨いでも履歴が壊れない

### Phase 4 - Workflow Contract for Follow-up Features

- `Master Properties` から見た precomp 境界を明文化する
- exposed property / internal property / restored layer の責務境界を固定する
- 後続機能が「precomp はあるが戻せない」前提を持たなくてよい状態にする

## 5. 完了条件

- `unprecompose()` が layer restore を最後まで実行できる
- 親コンポジションへ戻したレイヤーの順序、時間、基本 transform が期待どおりである
- undo / redo で precompose と unprecompose を往復しても状態が壊れない
- `Master Properties` 側で「Precompose 完了待ち」としていたブロッカーを外せる

## 6. 非スコープ

- `Track Matte Drag-Link UX`
- `Adjustment Layer` の描画拡張
- precompose を使った新規 UI 演出や大量の見た目変更
- timeline 左ペインの新規常時バッジ追加

## 7. リスク / 注意点

- composition 階層と layer ownership が曖昧なまま広く触ると、undo/redo と save/load の両方に波及しやすい
- `ArtifactCore/src/Composition/PreCompose.cppm` は後続機能の依存点になっているため、finish line の定義なしに広く変更しない
- まずは `unprecompose()` の責務を閉じ、その後に `Master Properties` へ進む

## 8. 関連文書

- `docs/planned/COMPOSITION_PRECOMPOSE_ANALYSIS_2026-04-17.md`
- `docs/analysis/REPORT_AE_GAP_UPDATE_2026-07-03.md`
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`
- `docs/planned/MILESTONE_MASTER_PROPERTIES_2026-07-08.md`
- `docs/done/MILESTONE_2026-07-03_SESSION_COMPLETION.md`

## 9. 推奨着手順

1. `unprecompose()` の restore contract を先に固定する
2. parent/child の時間と layer order の復元ルールを文書化する
3. undo/redo を安定させる
4. その後に `Master Properties` の前提として参照させる

最初の実作業は、`PreCompose.cppm` の `unprecompose()` がどこまで restore しているかを棚卸しし、restore 不足を埋めることから入る。
