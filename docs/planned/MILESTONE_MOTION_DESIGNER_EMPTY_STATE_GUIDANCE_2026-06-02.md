# Milestone: Motion Designer Empty State Guidance

> 2026-06-02 作成

**最終更新:** 2026-08-15

モーションデザイナー向けに、タイムラインや関連 surface の空状態を「何もない場所」ではなく「次の操作が分かる案内」として揃えるマイルストーン。

この文書は、Timeline / Property / Inspector / Preview / Composition 周辺の empty selection、未キーフレーム、未対象、未読み込み時の文言と導線をまとめる上位枠とする。

---

## Goal

- 何も選ばれていない時でも、次に何をすればよいかが 1 行で分かるようにする
- `open / select / add keyframe / inspect / scrub / preview` の語彙を surface 横断で揃える
- empty state を「停止表示」ではなく「編集の入口」として扱う
- モーション作業の文脈が薄く見えないようにする

---

## Scope

### In

- empty selection の案内
- no keyframe / no property / no mask の案内
- timeline / inspector / preview の未対象表示
- current frame に対する helper text の整理
- tooltip / placeholder / chip / inline hint の役割分担
- motion designer が「次に触る場所」を見つけやすくする文言

### Out

- UI の全面リデザイン
- QtCSS 前提の見た目調整
- 新しい中央集権 signal/slot の導入
- render backend や playback core の変更

---

## Design Rules

1. empty state は短く、行動が先に来ること
2. 説明文より先に `select / add / inspect / preview` を置くこと
3. `No ...` より `Select ...` / `Add ...` / `Open ...` を優先すること
4. 画面ごとの差は役割に限定し、語彙はできるだけ揃えること
5. 長文ヘルプは避け、必要なら tooltip に逃がすこと

---

## Phases

### Phase 1: Timeline Empty Selection Unification

- Timeline の未選択文言を action-first に寄せる
- Layer / Property / Keyframe の未選択状態をそろえる
- current frame が見えている時に、次の操作が分かるようにする

### Phase 2: No Keyframe / No Property Guidance

- no keyframe の案内を短く統一する
- property が見えているが編集対象がない時の helper text を整理する
- mask / effect / transform で文言の基準を揃える

### Phase 3: Preview and Inspector Guidance

- Preview の未対象・未再生・未読み込み表示を整理する
- Inspector の empty state を action-first にする
- `open / select / scrub / inspect` の導線を揃える

### Phase 4: Finish and Polish

- tooltip と placeholder を見直す
- status chip と helper text の重複を減らす
- 空状態の密度と余白のバランスを整える

---

## Execution Checklist

### Phase 1 Checklist

- [ ] Timeline の `No selection` 系の表現を洗い出す
- [ ] Layer / Property / Keyframe の未選択文言を action-first に寄せる
- [ ] current frame から次の操作が分かるかを確認する

### Phase 2 Checklist

- [ ] `No keyframe` / `No property` の表現基準を決める
- [ ] mask / effect / transform の helper text をそろえる
- [ ] 編集対象がない時の案内が短く読めるか確認する

### Phase 3 Checklist

- [ ] Preview の empty guidance を統一する
- [ ] Inspector の未対象状態を action-first にする
- [ ] `open / select / scrub / inspect` の導線が一致しているか確認する

### Phase 4 Checklist

- [ ] tooltip と placeholder を整理する
- [ ] 画面ごとに案内の長さがばらつきすぎていないか確認する
- [ ] empty state が「ただの空白」に見えないか確認する

---

## Success Criteria

- どの画面でも、空状態が案内として読める
- `No selection` より `Select ...` / `Add ...` が自然に見える
- 迷った時に次の行動が 1 つ見つかる
- empty state の語彙が surface ごとにバラけない
- モーション作業の流れが止まらず、軽く次の一手に進める

---

## Related Docs

- [`MILESTONE_APP_EMPTY_STATE_GUIDANCE_2026-05-17.md`](./MILESTONE_APP_EMPTY_STATE_GUIDANCE_2026-05-17.md)
- [`MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`](./MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md)
- [`MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`](./MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)
- [`MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md`](./MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md)
- [`MILESTONE_PLAYBACK_STATE_CONTRACT_AND_TRANSPORT_COHESION_2026-05-31.md`](./MILESTONE_PLAYBACK_STATE_CONTRACT_AND_TRANSPORT_COHESION_2026-05-31.md)

---

## Next Step

Phase 1 の対象 surface を Timeline / Property / Layer panel に絞り、`No selection` / `No keyframes` / `Add a keyframe at the playhead` の現状文言を先に洗い出す。
そのあと Preview と Inspector に広げ、最後に tooltip と placeholder をそろえる。

---

## Static audit follow-up (2026-07-25)

現行ソースには Composition Editor の `EmptyCompositionOverlayWidget`、テキスト編集時の `No text layer selected.`、プロパティ参照時の `This layer exposes no referenceable properties.` など、個別 surface の空状態・未対象表示は存在する。また、Composition Editor には選択・スマート選択・ProblemView 相当の導線がある。

ただし、Timeline / Property / Inspector / Preview 全体で `Select / Add / Inspect / Preview` の action-first 語彙を共有する仕組みや、no keyframe / no property / no mask の統一文言は確認できない。空状態から add keyframe、open、scrub へ直接進む共通導線も未確認で、実行時の表示密度・tooltip 重複は未検証である。

### Audit status

- Phase 1: 部分実装 — 個別の未対象表示はあるが、Timeline / Layer / Property の統一は未確認
- Phase 2: 未完了 — no keyframe / no property / no mask の共通案内は未確認
- Phase 3: 部分実装 — Composition Preview の empty overlay はあるが、Inspector / Preview 横断の導線は未確認
- Phase 4: 未着手相当 — tooltip / placeholder / helper text の全体整理と runtime 確認が残る

## Update 2026-08-15

現行コードを追加確認した。Timeline には `Select a layer to continue`、`Keys: - | Select a layer`、Property Widget には `Select a layer or composition effect to edit`、Inspector には `Select a layer to manage effects`／`Select a layer inside a composition to add ...`、Preview 系には `Select a layer to inspect the preview` など、action-first の空状態文言が複数 surface に実装されている。Composition Editor の空状態オーバーレイと、Layer Solo View の `Select a layer to inspect` も確認できる。

一方で、Dope Sheet の `No dope sheet rows yet. Select a layer with keyframes.`、Composition Editor の `No text layer selected.`、参照プロパティの `This layer exposes no referenceable properties.` など、`No ...` 系や surface 固有の表現も残っている。空状態から add keyframe／open／scrub へ進む共通 action、no keyframe／no property／no mask の統一テンプレート、tooltip／placeholder の横断管理は未確認である。

判定を更新し、Phase 1 と Phase 3 は個別 surface の action-first 表示が進んだ「部分実装」、Phase 2 は共通文言・導線が未完了、Phase 4 は全体整理と runtime の表示密度確認が残る状態とする。実装コードの確認のみで、実際の画面上の重複や読みやすさは未検証である。
