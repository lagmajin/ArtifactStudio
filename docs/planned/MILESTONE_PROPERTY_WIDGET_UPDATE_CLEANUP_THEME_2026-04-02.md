# Milestone: Property Widget Update / Cleanup / Theme Ownership (2026-04-02)

**Status:** Draft
**Goal:** `ArtifactPropertyWidget` 周辺の残骸整理、`PropertyEditor` の責務分離、そして property UI のテーマ所有権を整理して、見た目と構造を長期保守しやすい形へ寄せる。

---

## Background

現在の property 系 UI は、次のものが混在しやすい。

- `ArtifactPropertyWidget` の動的組み立て
- `Artifact.Widgets.PropertyEditor` の再利用部品
- `ArtifactInspectorWidget` 側の effect stack 表示
- `QSS` / inline style / theme preset の混在
- old `Knob` 残骸や、責務が薄い wrapper

この milestone は、property 表示を「今のまま少し見た目を直す」だけでなく、**どの widget が何を所有するか** を明確にするのが目的。

---

## Scope

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/*`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`
- `Artifact/src/Widgets/Dock/*`
- `Artifact/src/AppMain.cppm`
- `ArtifactWidgets/include/Knob/*` は原則 legacy 扱い

---

## Non-Goals

- property system の完全な再設計
- 既存 layer / effect property API の破壊的変更
- theme system の全面置換を一気にやること
- `ArtifactWidgets` 側の legacy を即削除すること

---

## Phases

### Phase 1: Property UI Audit

**Goal:** どの widget がどの責務を持っているかを整理する。

- `ArtifactPropertyWidget` の rebuild / update / filter / lock 役割を棚卸しする
- `PropertyEditor` に寄せるべき row レベル責務を分ける
- Inspector と Property の境界を再確認する
- `Knob` 系 legacy の現状を固定する

**Done when:**

- property UI の責務マップが言語化される
- どこを触ると何が壊れるかが追える

---

### Phase 2: Layout and Rebuild Cleanup

**Goal:** property panel の再構築コストと重複実装を減らす。

- rebuild / update の二重処理を減らす
- property row の生成経路を一本化する
- summary row / group row / effect row の役割を整理する
- filter / selection / focus の更新経路を明確にする

**Done when:**

- property widget の再構築が追いやすくなる
- 無駄な rebuild が減る

---

### Phase 3: Theme Ownership

**Goal:** property UI の見た目を、widget 個別の `QSS` ではなく theme 側へ寄せる。

- surface / border / accent / selection の由来を整理する
- property rows が theme token から色を受け取れるようにする
- `QSS` 依存を減らす
- dark / light / high-contrast の見え方差を整理する

**Done when:**

- property pane の配色が theme で揃う
- widget-local style の散発が減る

---

### Phase 4: Row-Level UX Polish

**Goal:** 1 行ごとの操作性を上げる。

- reset / keyframe / expression / copy-paste の導線を整理する
- numeric rows に slider / spin / drag の責務を分離する
- labels / value column / inline affordance の見た目を揃える
- state indication を明確にする

**Done when:**

- property row の操作が一貫する
- どの row が編集可能か見て分かる

---

### Phase 5: Legacy Remnant Cleanup

**Goal:** old `Knob` 断片や、今の UI に寄与しない残骸を減らす。

- legacy wrapper を段階的に隔離する
- 使っていない style / widget を整理する
- 必要なら deprecated 扱いを明示する

**Done when:**

- property 系 UI の主要な流れが現行 `PropertyEditor` に寄る
- legacy が「残っている理由」を説明できる

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
- `docs/planned/MILESTONE_QSS_REDUCTION_2026-03-31.md`
- `docs/planned/MILESTONE_UI_THEME_SYSTEM_2026-03-30.md`
- `docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md`
- `docs/planned/MILESTONE_CONSOLE_WIDGET_ENHANCEMENT_2026-03-31.md`

## First Targets

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorRowWidget.cppm`
- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`
- `Artifact/src/Widgets/Dock/DockStyleManager.cppm`

## Current Status

Property UI は動いているが、責務が散りやすい。  
この milestone では、見た目の更新と同時に「どこが theme を所有するか」を明確にする。
