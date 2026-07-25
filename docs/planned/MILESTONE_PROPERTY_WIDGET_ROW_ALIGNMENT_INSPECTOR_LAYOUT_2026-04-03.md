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
- pick-whip / reference link の affordance 位置を row 末尾の共通 action に寄せる

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
- reference link / pick-whip glyph の位置を共通化する

**Done when:**

- row の操作方法が種類ごとに予測しやすい
- クリックとドラッグの責務がぶれない

---

### Phase 4: PropertyEditor Composition

**Goal:** `ArtifactPropertyWidget` の layout logic を `PropertyEditor` row widget に寄せる。

- row widget が label / value / action の組み立てを担当する
- `ArtifactPropertyWidget` は group と selection の orchestration に集中する
- effect / transform / expression の row 実装差を縮める
- row widget に link / keyframe / reset の並び順を定義する
- pick-whip の drag affordance を row action の一部として扱う

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

進捗メモ:
- `ArtifactPropertyWidget` の rebuild 時に、再利用 row を先に親から切り離して group box の破棄に巻き込まれないようにした
- `PropertyEditor` 側は icon cache が既に入っており、次の焦点は row geometry の固定と section header の整列に移っている
- row の最小高さ、ボタン寸法、label 幅の基準値を定数化して、Property / Inspector の見た目調整を一箇所へ寄せ始めた
- Inspector 側の note / rack / effect section の余白を共通定数に寄せた
- `ArtifactPropertyWidget` に rebuild signature を入れて、visible structure が変わっていない full rebuild を update-only に落とせるようにした
- `ArtifactInspectorWidget` 側も layer info / effects list の signature を入れて、同じ内容なら再構築を避けるようにした
- `ArtifactInspectorWidget` の project / composition / layer 更新を 0ms の queued refresh に寄せて、連続イベントを 1 回に束ねるようにした
- `ArtifactInspectorWidget` の refresh を dirty-bit 化して、composition note / layer note / layer info / effects list を必要なものだけ更新するようにした
- `ArtifactPropertyWidget` の filter 入力は即時 rebuild ではなく debounce rebuild に寄せた
- `ArtifactPropertyWidget` の animated value 更新は frame cache を持ち、同じフレームの再計算を飛ばせるようにした

## Static Audit (2026-07-25)

現行の Property Editor には、row 最小高さ・label 幅・action spacing・keyframe／navigation／reset／expression の寸法定数が集約され、row widget が label／value editor／aux action を共通配置する。owner-draw の row chrome、hover／selection、keyframe／reset／favorite の表示制御、section の presentation badge、`alignPropertyRowLabels()` による group ごとの整列が実装されている。Property Widget 側も effect／channel／transform／通常 group で共通 row builder を利用し、refresh の signature／dirty-bit／debounce／frame cache も確認できる。

未確認なのは、全 editor 種別（numeric／color／checkbox／combo／expression）での実画面上の baseline と高さの一致、Inspector と Property Widget 間の section header 完全統一、Phase 3 に含まれる reference link／pick-whip affordance、Phase 4 の ad-hoc row 構築の完全撤去、Phase 5 の runtime 視認性である。reset handler の全 row 接続や新規 property 追加時の共通契約も、静的検索だけでは完成を証明できない。

判定: **Phase 1〜2 と row chrome の主要実装は確認できる。** Phase 3〜5 は部分実装または runtime 検証待ちであり、文書の Status は Draft のまま維持する。
