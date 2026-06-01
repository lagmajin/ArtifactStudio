# M-FE-5a Preset Browser / Starter Flow Execution

`Templates / Presets / Starter Kits` を、抽象的な資産構想ではなく、ユーザーが最初に触る再利用導線として再開するための execution メモ。

## Why Now

- repo 全体を見ると `preset` は effect / text / render / shortcut / workspace など各所に散在している
- 一方で、ユーザー視点では「どこから preset を選ぶのか」「どう再利用するのか」が弱い
- `custom shape` や `starter project` を軽視しないためにも、まずは `Preset Browser` と `Starter Flow` の入口を固定したい

## Current Ground Truth

- `M-FE-5 Templates / Presets / Starter Kits` 自体は backlog にある
- effect 側には個別 preset の概念がすでにある
- text animator 側にも preset 再利用の必要性が強く残っている
- 広告動画向けには `template slot / variation import / output preset` の別ラインが走っている

## Core Decision

この段階では「全 preset を統合管理する巨大ブラウザ」は作らない。  
まずは `選ぶ`, `適用する`, `再利用する` の最短導線を app surface に作る。

## First Slice

### 1. Preset Taxonomy Lock

- `project preset`
- `composition preset`
- `layer preset`
- `effect preset`
- `text animator preset`
- `starter project`

この 6 種を first-class の再利用単位として固定する。

### 2. Browser Entry Lock

- `Project` から開く starter / template 導線
- `Inspector / Property Editor` から開く layer / effect / text preset 導線
- `Render Queue` の output preset は別文脈として扱い、今回は混ぜない

### 3. Shared Card Grammar

- name
- type
- short description
- preview hint
- source scope
- apply target

各 preset を同じ card grammar で見せる。

### 4. Starter Flow

- app 起動直後または新規作成時に `Blank / Starter / Recent` の 3 択を出せる形を目標にする
- 最初の slice では modal でもよい

## Non-Goals

- `.mogrt` 完全互換
- cloud marketplace
- preset sync
- すべての preset を Phase 1 で保存可能にすること

## Phase Proposal

### Phase 1: Taxonomy / Entry Audit

- preset の種類と owner surface を固定する
- `effect / text / starter` の 3 系統を最初の採用対象に絞る

### Phase 2: Minimal Preset Browser

- card/list の最小 UI
- apply action を 1 箇所で持つ
- 保存より先に browse/apply を成立させる

### Phase 3: Starter Flow

- blank / starter / recent の起動導線
- `starter project` を project asset として見せる準備

## Recommended First Connections

- `ArtifactProjectManagerWidget`
- `ArtifactInspectorWidget`
- `ArtifactPropertyWidget` / `PropertyEditor`
- `ArtifactMainWindow`

## Success Criteria

- ユーザーが「preset がある」ではなく「選んで使える」と感じられる
- effect と text と starter が別々の見た目ではなく、同じ再利用文法で見え始める
- custom shape や template を将来 asset 化する入口として使い回せる

## Related

- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `docs/planned/MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md`
- `docs/planned/MILESTONE_TEXT_ANIMATOR_NEXT_GEN_PARITY_EXECUTION_2026-05-31.md`
- `docs/planned/MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md`
