# Phase 2 実行メモ: Universal Property Registry

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md) の Phase 2 を、既存の property UI を壊さずに進めるための実行メモ。

この段階では property UI を再設計しない。先に `UI なしでも読める property index` を作る。

---

## 方針

1. `AbstractProperty` / `PropertyGroup` をそのまま残す
2. `PropertyRegistry` は read-only index として導入する
3. path / ID / owner の対応を先に固定する
4. 既存の PropertyWidget は registry の consumer に徐々に寄せる

---

## 現状の土台

- [`ArtifactCore/include/Property/AbstractProperty.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/AbstractProperty.ixx)
- [`ArtifactCore/include/Property/PropertyGroup.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/PropertyGroup.ixx)
- [`ArtifactCore/src/Property/AbstractProperty.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Property/AbstractProperty.cppm)
- [`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactPropertyWidget.cppm)
- [`Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm)
- [`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactInspectorWidget.cppm)

---

## 実装タスク

### 1. Registry 型を追加する

追加候補:

- `PropertyPath`
- `PropertyRegistry`
- `PropertyHandle`
- `PropertyOwnerDescriptor`
- `PropertySerializationBridge`

責務:

- property の所在を path で表現する
- layer / effect / composition を同じ検索経路で引けるようにする
- serialize / deserialize の入口を 1 か所に寄せる

### 2. read-only 列挙 API を作る

やること:

- `AbstractProperty` と `PropertyGroup` を列挙する
- owner ごとに property を walk できるようにする
- current value / keyframe / expression の読取を registry 経由にする

### 3. path lookup を固定する

例:

- `layer.<LayerID>.transform.position.x`
- `layer.<LayerID>.effect.<EffectID>.mix`
- `composition.<CompositionID>.settings.frameRate`

やること:

- path 文字列から owner と property を解決する
- property ID と path の対応を安定させる
- UI から見た名前と内部 ID を分ける

### 4. 既存 UI の参照先を registry に寄せ始める

やること:

- PropertyWidget の表示用データを registry から取れるようにする
- Inspector の selection 変更と property lookup をつなぐ
- UI は引き続き編集を担うが、source of truth は registry に寄せる

---

## 実装順

1. `PropertyPath` / `PropertyRegistry` / `PropertyHandle`
2. read-only 列挙 API
3. path lookup
4. UI の参照先切り替え

---

## 完了条件

- UI なしで property を検索できる
- expression / keyframe / current value が path で読める
- 既存の property UI の見た目と操作感が壊れない

---

## 変更しないこと

- PropertyWidget の全面再設計
- 既存 keyframe 編集の操作感
- 既存の property row の見た目
- 既存 serializer のフォーマットそのもの

---

## リスク

- path と internal ID を混同すると後で search / RPC が壊れる
- UI の編集責務まで一気に移すと、既存の row editing が不安定になる
- registry を read-only で始めないと、早い段階で挙動変更が混ざる

---

## 次の Phase への橋渡し

Phase 2 が終わると、Phase 3 で host-adapted effect contract を property と同じく read-only に繋ぎやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE1_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE1_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
