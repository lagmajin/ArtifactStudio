# マイルストーン: Text / Effect Workflow Bridge

**作成日:** 2026-05-25  
**ステータス:** 計画中  
**優先度:** 中  
**関連:** `docs/planned/MILESTONE_TEXT_ANIMATOR_SYSTEM_2026-03-25.md`, `docs/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`, `docs/MILESTONE_EFFECT_SYSTEM_BRIDGE_2026-05-25.md`

---

## 概要

Text と Effect は、どちらも property / animation / preset を強く使う領域だが、UI の見せ方がまだ揃っていない。
このフェーズでは、text animator と effect parameter の見え方を近づけ、選択中の対象に対して何ができるかを迷わず分かるようにする。

---

## Phase 1: Text Animator UX Coherence

**目標:** text animator の編集導線を property panel と timeline で揃える。

- [ ] animator の追加 / 削除 / 有効無効を自然に見せる
- [ ] range selector / wiggly selector の操作を整理する
- [ ] property path 表示を text layer 側と合わせる
- [ ] text tool / inspector / timeline の役割を分ける

## Phase 2: Effect Parameter Editing Bridge

**目標:** effect stack と text animator の parameter editing を同じ文法で扱う。

- [ ] active selection に対する編集対象を明確にする
- [ ] effect parameter / text animator property / expression の見た目を揃える
- [ ] 値の変更・キーフレーム化・expression 化の導線を近づける
- [ ] 使えない項目は無効化して誤操作を防ぐ

## Phase 3: Preset / Reuse Workflow

**目標:** text と effect の再利用を、同じ preset 文法で扱う。

- [ ] preset 保存 / 読込の入り口を揃える
- [ ] text animator preset と effect preset の見た目を統一する
- [ ] よく使う設定を再適用しやすくする
- [ ] JSON / file / current selection の責務を分ける

## Phase 4: Timeline Affordance

**目標:** text / effect が timeline 上で何をしているか分かるようにする。

- [ ] animator / effect の存在を track 上で見える化する
- [ ] keyframe の意味を読み取りやすくする
- [ ] selection change で timeline と inspector を同期する
- [ ] text 専用 UX と effect 専用 UX を混ぜすぎない

---

## 完了条件

1. Text Animator と Effect の編集導線が迷いにくい
2. property / expression / keyframe の文法が揃う
3. preset の再利用が自然にできる
4. timeline で編集対象が読める
5. それぞれの責務が混ざらない
