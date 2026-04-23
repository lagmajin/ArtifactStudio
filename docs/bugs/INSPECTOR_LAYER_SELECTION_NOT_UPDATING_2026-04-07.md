# 調査報告書: インスペクターがレイヤー選択に反応しない問題

**調査日**: 2026-04-07  
**対象ファイル**: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`  
**報告者**: Copilot

---

## 問題の概要

コンポジションエディタでレイヤーをピッキング（クリック）しても、  
インスペクター（プロパティウィジェット）に選択レイヤーの情報が表示されないことがある。

- 新規コンポジション作成直後は特に発生しやすい
- 一度選択に成功すると、2回目以降は成功することもある
- イベントの伝達自体は行われているが、インスペクター側で無視されていた

---

## 調査結果: 根本原因

### 根本原因 — `LayerSelectionChangedEvent` の `compositionId` が空になるケース

`ArtifactProjectService::selectLayer()` が `LayerSelectionChangedEvent` を publish する際、  
`compositionId` を次のように設定する:

```cpp
// ArtifactProjectService.cpp (selectLayer 内)
event.compositionId = currentComposition().lock()
    ? currentComposition().lock()->id().toString()
    : QString();  // ← weak_ptr が期限切れ/未設定なら空文字
```

`currentComposition()` の weak_ptr が期限切れ（またはまだ設定前）の場合、  
`compositionId = ""` となり `CompositionID("").isNil() == true` になる。

インスペクターの `LayerSelectionChangedEvent` サブスクライバーは **無条件に**  
`impl_->currentCompositionId_ = cid` を代入していた:

```cpp
// 修正前（不具合あり）
const CompositionID cid(event.compositionId);
impl_->currentCompositionId_ = cid;           // ← nil を代入してしまう！
impl_->handleLayerSelected(lid);
```

その結果 `updateLayerInfo()` の nil チェック:

```cpp
if (currentCompositionId_.isNil()) {
    setNoLayerState();
    return;                // ← ここで即 return
}
```

が発動し、レイヤー情報が全く表示されない。

---

## 修正内容

### 修正1: `LayerSelectionChangedEvent` サブスクライバーでの nil 代入防止

**ファイル**: `ArtifactInspectorWidget.cppm` (L1466-1486)

```cpp
// 修正後
const CompositionID cid(event.compositionId);
const LayerID lid(event.layerId);
// nil の場合は既存の currentCompositionId_ を上書きしない
if (!cid.isNil()) {
    impl_->currentCompositionId_ = cid;
} else if (impl_->currentCompositionId_.isNil()) {
    // フォールバック: サービスから直接取得
    if (auto* svc = ArtifactProjectService::instance()) {
        if (auto comp = svc->currentComposition().lock()) {
            impl_->currentCompositionId_ = comp->id();
        }
    }
}
impl_->handleLayerSelected(lid);
```

### 修正2: `updateLayerInfo()` でのフォールバック

**ファイル**: `ArtifactInspectorWidget.cppm` (L780-789)

```cpp
// 修正後
if (currentCompositionId_.isNil()) {
    // イベントで compositionId が届かなかった場合のフォールバック
    if (auto comp = projectService->currentComposition().lock()) {
        currentCompositionId_ = comp->id();
    } else {
        setNoLayerState();
        return;
    }
}
```

---

## 補足: EventBus と compositionId の設計上の注意

- `EventBus` は `shared_ptr<Impl>` を共有するため、`globalEventBus()` のコピーからも subscribe 可能
- `LayerSelectionChangedEvent.compositionId` は `QString` 型であり、空文字 `""` が nil ID を意味する
- インスペクター側は `currentCompositionId_` を信頼できる状態に保つ責任がある  
  （イベントが不完全でも動作を継続できるよう防衛的に実装すべき）

---

## 修正適用済み

| 修正 | ファイル | 行 |
|------|----------|----|
| ✅ nil 代入防止 + フォールバック | `ArtifactInspectorWidget.cppm` | L1466-1486 |
| ✅ `updateLayerInfo` フォールバック | `ArtifactInspectorWidget.cppm` | L780-789 |
