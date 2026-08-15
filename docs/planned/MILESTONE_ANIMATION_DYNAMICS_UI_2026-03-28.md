# マイルストーン: Animation Dynamics UI Surface

> 2026-03-28 作成
> 2026-08-15 現行コード監査

**最終更新:** 2026-08-15

## 目的

`Animation Dynamics Core` で導入する spring / damping / follow-through を、`Artifact` の UI から扱える形にする。

この milestone はダイナミクスの solver を実装しない。Core 側の値を、Inspector / Property / Layer UI で編集・確認・再利用できるようにする受け皿を作る。

**現状:** Layer Property の Motion Dynamics group と JSON／評価接続、Layer Panel の専用 preset menu は実装済み。Timeline／Layer Panel の状態表示、runtime 検証は未完了。

### 2026-08-15 実装確認

- `ArtifactAbstractLayer::getLayerPropertyGroups()` に Motion Dynamics の enabled、mode、stiffness、damping、mass、lag tau、overshoot clamp／limit が登録され、ラベル・tooltip・編集範囲を持つ。
- 値の変更は layer の設定・JSON 永続化と `Animation.Dynamics` の transform 評価へ接続されている。既存の Physics group とは別に Motion group として扱われる。
- `ArtifactLayerPanelWidget` のレイヤーコンテキストメニューに Smooth／Bouncy／Heavy／Floaty／Rigid の preset と Reset 導線を追加し、既存の `setLayerPropertyValue()` 経由で Motion Dynamics の値を一括適用する。
- Timeline／Layer Panel の enabled／preset badge、viewport motion path、専用 reset／copy UI、実機操作確認は未実施。

### Update 2026-08-15 — Layer Panel feedback

- Layer Panel の通常レイヤー行に `Dynamics` 状態を既存の state badge へ統合表示するようにした。既存の Hidden／Locked／Solo 等がある場合は併記し、Motion tone で表示する。
- `performUpdateLayout()` の row signature に state text／tone を含め、Motion Dynamics の有効／無効変更時に表示が stale にならないようにした。
- preset 名の完全な逆算表示、Timeline 側の badge、Undo 付き preset 適用、runtime 検証は未完了または未検証。

判定: **Inspector／Property の編集面と Core 接続、Layer Panel の preset surface は実装済み。Timeline／Layer feedback、Undo 付き workflow 統合、runtime 検証は pending。**

---

## 背景

現在の UI は、keyframe や layer transform は扱えるが、アニメーション向けのダイナミクス設定を独立した編集単位として見せる導線が弱い。

欲しいのは次のような編集体験。

- `Physics2D` ではなく animation 用の挙動として見える
- layer ごとの `physics.*` / `behavior.*` を Inspector で触れる
- preset を 1 回選ぶだけで見た目の変化が分かる
- timeline と viewport からも状態を追える

---

## 方針

### 原則

1. UI は Core の solver を作らない
2. `physics.*` と `behavior.*` の見出しを固定する
3. 表示名は layer panel / inspector / property で揃える
4. 空状態でも意味が読めるようにする
5. 先に compact な UI を作り、詳細編集は後から足す

### 想定対象

- `ArtifactInspectorWidget`
- `ArtifactPropertyWidget`
- `ArtifactTimelineWidget`
- `ArtifactLayerPanelWidget`
- `ArtifactCompositionEditor`

---

## Phase 1: Inspector Surface

### 目的

layer のダイナミクス設定を Inspector から見えるようにする。

### 作業項目

- `physics.enabled` の on/off を出す
- `physics.followThroughGain` / `physics.damping` / `physics.stiffness` を編集できるようにする
- `behavior.enabled` / `behavior.mode` / `behavior.note` を出す
- null / disabled 状態の表示を定義する

### 完了条件

- layer を選ぶと dynamics group が見える
- 既存 property と衝突しない
- 値の意味が読めるラベルになっている

---

## Phase 2: Preset Surface

### 目的

調整の入口を preset ベースで短くする。

### 作業項目

- `Smooth` / `Bouncy` / `Heavy` / `Floaty` / `Rigid` の preset ボタン
- preset から個別値へ展開できる UI
- reset / revert / copy の導線

### 完了条件

- 1 クリックで変化が分かる
- inspector で preset と個別値が両立する

---

## Phase 3: Layer / Timeline Feedback

### 目的

設定した dynamics が、layer panel と timeline からも見えるようにする。

### 作業項目

- layer panel に enabled / preset badge を表示する
- timeline selection と inspector の同期を崩さない
- viewport 側で path や motion feedback を載せる前提を作る

### 完了条件

- dynamics 設定が選択状態と一緒に追える
- どの layer に設定があるか一目で分かる

---

## Phase 4: Workflow Integration

### 目的

設定編集を、普段の制作導線に組み込む。

### 作業項目

- layer context menu から preset を適用する
- property editor から animation dynamics section に飛べるようにする
- 将来の `Animation.Dynamics` core API に対応しやすい形を保つ

### 完了条件

- dynamics 設定を探す手間が短い
- UI 側の変更が Core の solver 設計に引きずられない

---

## Non-Goals

- solver の数値計算
- rigid body / collision UI
- ノードベースの物理エディタ
- graph editor の全面実装

---

## Related

- `docs/planned/MILESTONE_ANIMATION_DYNAMICS_CORE_2026-03-28.md`
- `docs/planned/MILESTONE_LAYER_COMPONENTS_PHYSICS_BEHAVIOR_2026-03-28.md`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## Current Status

2026-03-28 時点では、layer の dynamics は UI 上の独立 surface としてはまだ弱い。
Core の `Animation Dynamics` と並行して、この UI milestone で Inspector / layer panel / timeline の受け皿を先に固定する。
