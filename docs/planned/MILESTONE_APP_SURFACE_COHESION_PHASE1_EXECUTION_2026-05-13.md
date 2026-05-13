# App Surface Cohesion - Phase 1 Execution

**Date**: 2026-05-13

**Source**: [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)

---

## Phase 1 Goal

Project / Asset / Timeline / Composition / Debugger の header と summary の読み方を、まずは `current / recent / selection / status` の語彙で揃える。

この段階では全面リデザインよりも、各 surface の先頭 1 画面を見た時に「今どこにいるか」がすぐ分かることを優先する。

---

## Scope

### In

- current context line
- recent / selection summary
- status chip / note の役割整理
- empty state の入口文
- Asset Browser の hub-style summary

### Out

- 全 surface の完全統一
- QtCSS ベースの新テーマ
- 新しい global signal/slot
- backend / core 再設計

---

## Current Focus Surfaces

Phase 1 では、まず次の surface を優先する。

1. `ArtifactAssetBrowser`
2. `ArtifactProjectManagerWidget`
3. `ArtifactCompositionEditor`
4. `ArtifactTimelineWidget`
5. `AppDebuggerWidget`

この順番で、「current の見え方」と「次の行動」が読めるかを揃える。

---

## Working Rules

1. current は 1 行で短く出す
2. recent は 3 件前後に抑える
3. selection は件数と要点だけを先に出す
4. status は chip / note で先に見せる
5. empty state は案内として扱う

---

## Tasks

### 1. Header Vocabulary

- current / recent / selection / status の文言を surface ごとに見比べる
- 同じ意味に別の言い方を使わないようにする
- tooltip も同じ語彙で寄せる

### 2. Asset Browser Hub

- 左ペインの `Library Hub` を phase 1 の基準面として扱う
- recent / favorites / sources の短い提示を固定する
- folder navigation の入口を同じ文脈で見せる

### 3. Selection Summary

- 選択件数を短く見せる
- current selection がない時の文言を統一する
- 何も選ばれていない時に不安になりにくい文法へ寄せる

### 4. Empty State

- 空時の案内を surface ごとに作る
- 何もない時でも「次に何をすればいいか」が読めるようにする

---

## First Files

1. `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
2. `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
3. `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
4. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
5. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

---

## Done Criteria

- 主要 surface の先頭を見た時に、今の位置が短く読める
- recent / selection / status の文言が surface ごとにバラけない
- Asset Browser の hub-style summary が横断文法の基準になる
- empty state が単なる余白ではなく、案内として機能する

---

## Next Step

Phase 2 では、summary strip の高さと情報量をさらに揃える。
