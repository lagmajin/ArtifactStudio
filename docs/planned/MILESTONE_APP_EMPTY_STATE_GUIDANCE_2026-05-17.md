# Milestone: App Empty State Guidance

> 2026-05-17 作成

ArtifactStudio 全体の empty state を、ただの「何もない場所」ではなく次の行動が読める案内として揃えるマイルストーン。

この文書は、Project / Asset / Timeline / Composition / Contents Viewer / Inspector / Debugger にまたがる、空選択・未読み込み・未接続・未発見時の文言と導線をまとめる上位枠とする。

---

## Goal

- 何も選ばれていない時でも、次に何をすればよいかが 1 行で分かるようにする
- `open / select / inspect / import / search / navigate` の語彙を画面横断で揃える
- empty state を「待機表示」ではなく「行動の入口」として扱う
- 空状態でも、画面の責務が薄く見えないようにする

---

## Scope

### In

- empty selection の案内
- empty project / no composition / no layer の案内
- no file / missing file / no recent source の案内
- inspector / timeline / contents viewer / asset browser の空状態文法
- status text と helper text の役割分担
- empty state の tooltip / placeholder / chip の整理

### Out

- UI の全面リデザイン
- QtCSS 前提の見た目調整
- 新しい中央集権 signal/slot の導入
- render backend の変更

---

## Design Rules

1. empty state は短く、行動が先に来ること
2. 説明文より先に `open / select / inspect` を置くこと
3. `No ...` より `Open ...` / `Select ...` を優先すること
4. 画面ごとの言い回しを変えすぎず、役割だけを変えること
5. 長文ヘルプは避け、必要なら tooltip に逃がすこと

---

## Phases

### Phase 1: Empty Selection Unification

- `No selection` 系の文言を action-first に寄せる
- Project / Timeline / Inspector の未選択表示をそろえる
- 選択なし状態でも、どの対象を選ぶべきかが分かるようにする

### Phase 2: No Project / No Composition Guidance

- 未読み込み時の案内を統一する
- `open a project` と `open a composition` の使い分けを整える
- Inspector / Effects / Timeline で前提不足の案内をそろえる

### Phase 3: Viewer and Asset Guidance

- Contents Viewer の `no file` 案内を action-first にする
- Asset Browser の未選択・未接続・未発見の案内を短くまとめる
- recent / folder / selection の導線を揃える

### Phase 4: Finish and Polish

- tooltips と placeholder を見直す
- status chip と helper text の重複を減らす
- 空状態の密度と余白のバランスを整える

---

## Execution Checklist

### Phase 1 Checklist

- [ ] `No selection` 系の表現を surface ごとに洗い出す
- [ ] Project / Timeline / Inspector の未選択文言を action-first に寄せる
- [ ] empty selection で次の対象が分かるかを確認する

### Phase 2 Checklist

- [ ] `Open a project` / `Open a composition` の基準を決める
- [ ] Inspector の未前提状態メッセージをそろえる
- [ ] Effects / Layers / Timeline の helper text を揃える

### Phase 3 Checklist

- [ ] Contents Viewer の `no file` 表現を統一する
- [x] Asset Browser の selection / recent / sync 文言を短くする
- [ ] `open / select / inspect` の導線が各面で一致しているか確認する

### Phase 4 Checklist

- [ ] tooltip と placeholder を整理する
- [ ] 画面ごとに案内の長さがばらつきすぎていないか確認する
- [ ] empty state が「ただの空白」に見えないか確認する

---

## Success Criteria

- どの画面でも、空状態が案内として読める
- `No selection` より `Open ...` / `Select ...` が自然に見える
- 迷った時に次の行動が 1 つ見つかる
- empty state の語彙が surface ごとにバラけない
- アプリ全体の待機状態が、静かだけど親切に見える

---

## Related Docs

- [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)
- [`MILESTONE_ONBOARDING_EMPTY_STATES_2026-03-27.md`](./MILESTONE_ONBOARDING_EMPTY_STATES_2026-03-27.md)
- [`MILESTONE_ASSET_BROWSER_LEFT_PANE_HUB_2026-04-23.md`](./MILESTONE_ASSET_BROWSER_LEFT_PANE_HUB_2026-04-23.md)
- [`MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`](./MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md)
- [`MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md`](./MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md)

---

## Next Step

Phase 1 の対象 surface を確定し、`No selection` / `No file selected` / `Create or open a project` を優先的に洗い出す。
そのあと Project / Timeline / Inspector を先にそろえ、次に Asset Browser と Contents Viewer に広げる。

## 2026-07-30 実装監査

- Asset Browser の file list に空状態メッセージを追加した。フォルダ未選択、検索／filter 結果なし、空フォルダをそれぞれ `Open a folder...`、`No assets match...`、`Import or drop files here...` と action-first に表示する。
- selection／recent／sync の既存案内と同じ surface 上で表示し、一覧が空白だけに見えないよう owner-draw で描画する。
- Contents Viewer には `Open a file to inspect it, or choose a recent source` の action-first な未読み込み案内が既に存在することを確認した。複数 surface 間の語彙完全統一と runtime 確認は未完了として残す。
