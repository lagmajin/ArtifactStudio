**ステータス:** In Progress

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

### Current Progress

- `ArtifactProjectService` の `precomposeLayersWithUndo()` / `unprecomposeLayerWithUndo()` は既に undo surface に接続されている
- `PreComposeManager` の失敗系と循環検出を固定する回帰テストを追加した
- コマンド factory と `PreComposeCommand::Type` の往復も回帰テストで固定した
- core 側の `UnprecomposeCommand` も execute / undo / redo の往復を回帰で固定した
- core 側の `UnprecomposeCommand` の undo 後に nesting hierarchy が戻ることも固定した
- `ArtifactProjectService` を使った実地の precompose / undo / redo 回帰を追加した
- `NestedTimeUtils` は layer state の `startTime` を参照する実装に寄せた
- `isPrecomposeLayer` / `getSourceCompositionId` のマッピングも回帰テストで固定した
- `getRemappedTime` も precomp layer の parent-to-child 変換に合わせて回帰テストで固定した
- redo 後も source composition mapping が維持される回帰を追加した
- undo 後の layer order を回帰テストで固定した
- redo 後の precomp layer insertion point も回帰テストで固定した
- unprecompose の戻し切りと undo 往復も回帰テストで固定した
- unprecompose の復元メタ情報記録も回帰テストで固定した
- unprecompose で source layer の position / opacity も保持される回帰を追加した
- keepComposition=false の child composition 削除と undo 往復も回帰テストで固定した
- restorePrecompose 後の child composition hierarchy も回帰で固定した

### Phase 4 - Workflow Contract for Follow-up Features

- `Master Properties` から見た precomp 境界を明文化する
- exposed property / internal property / restored layer の責務境界を固定する
- `keepComposition=true/false` の両経路で precompose / unprecompose / undo / redo が壊れない状態を維持する
- 後続機能が「precomp はあるが戻せない」前提を持たなくてよい状態にする

## 5. 完了条件

- `unprecompose()` が layer restore を最後まで実行できる
- 親コンポジションへ戻したレイヤーの順序、時間、基本 transform が期待どおりである
- undo / redo で precompose と unprecompose を往復しても状態が壊れない
- core 側の `UnprecomposeCommand::undo()` は restorePrecompose 経由で往復できる
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
- ここまでの進捗は source / diff ベースで確認済みで、runtime / build の最終確認は未実施

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
# 2026-07-10 Auto Package Progress

- Command Palette に `Auto Precompose Package` を追加
- 選択レイヤーを既存の `precomposeLayersWithUndo()` へ渡し、内容尺へ合わせる
  `MoveSelected` package を少ない入力で作成できる入口を追加
