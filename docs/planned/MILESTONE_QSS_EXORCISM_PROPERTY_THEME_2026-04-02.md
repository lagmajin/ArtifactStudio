# Milestone: QSS Exorcism / Property Theme Ownership (2026-04-02)

**Status:** Draft
**Goal:** `QSS` 依存を property / inspector / core surface から段階的に追い出し、theme token と owner-draw に寄せる。

---

## Why This Now

`QSS` を完全にゼロにするのは一気にやると危ない。  
なので、まずは **property 系 UI と core surface から外す** のが現実的。

この milestone は、以下 2 つをつなぐ。

- `M-UI-14 QSS Reduction / Style Ownership`
- `M-UI-18 Property Widget Update / Cleanup / Theme Ownership`

---

## Scope

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`
- `Artifact/src/Widgets/Dock/*`
- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/PropertyEditor/*`

---

## Non-Goals

- 全 widget の `QSS` を一括削除すること
- OS 標準 look に戻すこと
- theme system を一新すること
- サブモジュールの広範囲改変

---

## Phases

### Phase 1: QSS Surface Inventory

- `setStyleSheet()` の使用箇所を property / inspector / dock / queue 周辺で確認する
- inline style と global stylesheet を分ける
- style ownership がどこにあるかを整理する

**Done when:**

- どの UI が `QSS` を握っているか分かる

---

### Phase 2: Property Pane Theme Ownership

- property row の色、枠、hover、selection を theme token に寄せる
- `ArtifactPropertyWidget` の見た目を widget-local `QSS` に依存させない
- `PropertyEditor` の共通スタイルを確立する

**Done when:**

- property pane が theme で再現できる

---

### Phase 3: Inspector / Queue / Dock Cleanup

- inspector と render queue の style ownership を整理する
- dock / tab / header の residual CSS を減らす
- `ApplicationSettingDialog` の theme selector を基準 UI に寄せる

**Done when:**

- surface widget 群の色と hover が theme で揃う

---

### Phase 4: Residual CSS Removal

- 残った `QSS` を順に owner-draw か theme token に置き換える
- レガシー style を削る
- theme スイッチ時の再描画を確認する

**Done when:**

- property / inspector / core surface の主要 path で `QSS` 依存が大幅に減る

---

## Suggested Order

1. `M-UI-14 QSS Reduction / Style Ownership`
2. `M-UI-18 Property Widget Update / Cleanup / Theme Ownership`
3. This milestone phases 1-4

---

## Related

- `docs/planned/MILESTONE_QSS_REDUCTION_2026-03-31.md`
- `docs/planned/MILESTONE_UI_THEME_SYSTEM_2026-03-30.md`
- `docs/planned/MILESTONE_PROPERTY_WIDGET_UPDATE_CLEANUP_THEME_2026-04-02.md`

## Current Status

`QSS` はまだ多い。  
ただし property / inspector / dock 周辺から攻めると、見た目の統一と責務整理を同時に進めやすい。
