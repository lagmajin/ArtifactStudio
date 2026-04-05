# Milestone: Property Widget Row Alignment / Inspector Layout (2026-04-03)

**Status:** Draft
**Goal:** `ArtifactPropertyWidget` の各行を、インスペクタらしく整列したレイアウトへ段階的に寄せる。  
見た目の好みだけでなく、`PropertyEditor` の row-level 責務を揃えて、どの行も同じ規則で読めるようにする。

---

## Why This Now

`ArtifactPropertyWidget` と `ArtifactInspectorWidget` は、すでに theme / `QSS` / reusable row widget の整理に入っている。  
次のボトルネックは「機能」そのものよりも、**1 行ごとの揃い方** と **操作 affordance の位置** にある。

この milestone は以下をまとめる。

- ラベル列と値列の境界を揃える
- keyframe / reset / navigation / classification badge の配置を統一する
- 数値、色、チェックボックス、列挙、式の各 row を同じ基準で読む
- `ArtifactPropertyWidget` の ad-hoc layout を `PropertyEditor` row widget へ寄せる

---

## Scope

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/*`
- `Artifact/docs/PROPERTY_EDITOR_AUDIT_2026-03-11.md`
- `docs/planned/MILESTONE_PROPERTY_WIDGET_UPDATE_CLEANUP_THEME_2026-04-02.md`
- `docs/planned/MILESTONE_QSS_EXORCISM_PROPERTY_THEME_2026-04-02.md`

---

## Non-Goals

- property / effect API の破壊的変更
- 既存 row editor の全面的な書き直し
- QSS を一気にゼロへすること
- inspector と property の役割を入れ替えること

---

## Phases

### Phase 1: Row Geometry Contract

**Goal:** property row の寸法ルールを先に固定する。

- label column / control column / action column の幅感を決める
- keyframe triangle / reset / navigation の位置を固定する
- checkbox / combobox / slider / color bar の高さ基準を揃える
- badge の配置ルールを決める

**Done when:**

- 主要 row が同じ高さ・同じ余白規則で並ぶ
- どの row でも操作の位置が読める

---

### Phase 2: Section Header Alignment

**Goal:** `Transform` / `Effect` / `Expression` などの section header を揃える。

- header 左の開閉 affordance を統一する
- header 右の分類 badge を共通化する
- enable / disable / lock / active の表示位置を整理する
- group header の baseline と padding を揃える

**Done when:**

- section header の見た目が widget ごとにぶれない
- group の意味がすぐ読める

---

### Phase 3: Row-Level Interaction Surfaces

**Goal:** 1 行ごとの入力・補助操作を、見た目と動きの両面で揃える。

- numeric row に slider / drag / spin を同じ順序で並べる
- color row の click / drag を明示する
- keyframe navigation triangles を行内の共通 affordance にする
- reset button の位置とサイズを共通化する

**Done when:**

- row の操作方法が種類ごとに予測しやすい
- クリックとドラッグの責務がぶれない

---

### Phase 4: PropertyEditor Composition

**Goal:** `ArtifactPropertyWidget` の layout logic を `PropertyEditor` row widget に寄せる。

- row widget が label / value / action の組み立てを担当する
- `ArtifactPropertyWidget` は group と selection の orchestration に集中する
- effect / transform / expression の row 実装差を縮める

**Done when:**

- `ArtifactPropertyWidget` の ad-hoc row construction が減る
- 新しい row 追加が同じ型でできる

---

### Phase 5: Visual Polish Pass

**Goal:** 余白、baseline、hover、selection、disabled の統一感を最後に詰める。

- ラベルの left inset を揃える
- action glyph のサイズを揃える
- selection / hover の色味を theme に合わせる
- 右端の reset / status glyph を必要最小限にする

**Done when:**

- property pane が「整って見える」状態になる
- 行の密度が揃って見える

---

## Suggested Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## Related

- `Artifact/docs/PROPERTY_EDITOR_AUDIT_2026-03-11.md`
- `docs/planned/MILESTONE_PROPERTY_WIDGET_UPDATE_CLEANUP_THEME_2026-04-02.md`
- `docs/planned/MILESTONE_QSS_EXORCISM_PROPERTY_THEME_2026-04-02.md`
- `docs/planned/MILESTONE_QSS_DECOMMISSION_COMMONSTYLE_2026-04-03.md`

## First Targets

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorRowWidget.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/AppMain.cppm`

## Current Status

Property UI の課題は、単なる配色ではなく「どの要素がどこに並ぶか」まで含めた整列に移っている。  
この milestone は、見た目の揃いを責務整理の出口として扱う。
