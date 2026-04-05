# パフォーマンス調査報告書 - EventBus 過剰再描画問題

**日付**: 2026-04-05  
**対象**: コンポジション作成・新規レイヤー追加・ブレンドモード変更の遅延  
**結論**: EventBus 移行後に Qt シグナル接続が残存し、**二重通知＋過広域イベントの組み合わせ**により全ウィジェットが毎回フルリビルドされている

---

## 1. 根本原因の整理

### 原因 A：二重発火（Qt シグナル + EventBus が両方生きている）

`ArtifactProjectService.cpp` lines 526–531:

```cpp
connect(&impl_->projectManager(), &ArtifactProjectManager::projectChanged, this, [this]() {
    globalEventBus().publish<ProjectChangedEvent>(...); // EventBus
    projectChanged();                                    // Qt シグナルも emit
});
```

**問題**: `notifyProjectMutation()` → `project->projectChanged()` → Qt signal が EventBus publish **と** Qt signal 両方を発火する。  
`ArtifactLayerPanelWidget` は Qt signal `projectChanged` に直接 connect されているため、EventBus 購読とは別に **updateLayout() がもう 1 回呼ばれる**。

```
notifyProjectMutation()
  └─ project->projectChanged()
       └─ ArtifactProjectManager::projectChanged (Qt signal)
            ├─ [EventBus] publish<ProjectChangedEvent>   → 購読者 10+ 箇所が全リビルド
            └─ [Qt signal] projectChanged()              → ArtifactLayerPanelWidget::updateLayout() (追加の1回)
```

---

### 原因 B：ブレンドモード変更がプロジェクト全体変更扱いになっている

`ArtifactLayerPanelWidget.cpp` line 1237–1242（コンボボックス選択時）:

```cpp
layer->setBlendMode(mode);
if (service) {
    if (auto project = service->getCurrentProjectSharedPtr()) {
        project->projectChanged(); // ← 問題の行
    }
}
```

**ブレンドモードはレイヤー 1 枚のレンダリング属性の変更に過ぎないのに、`ProjectChangedEvent` を全体に broadcast している。**

※ ホイールによるブレンドモード変更（lines 1813–1817）はすでに `layer->changed()` のみ使うよう修正済み。  
コンボボックス選択側が修正されていないため、クリック操作だけ重い。

---

### 原因 C：レイヤー追加で LayerChangedEvent ＋ ProjectChangedEvent が二重発火

`ArtifactProjectService.cpp` line 215:

```cpp
notifyProjectMutation(manager); // ← LayerCreated の後にも ProjectChanged を追加発火
```

レイヤー追加では以下が全て連続発火する：

| イベント | 経路 |
|---------|------|
| `LayerChangedEvent{Created}` | `projectManager::layerCreated` signal → EventBus |
| `ProjectChangedEvent` | `notifyProjectMutation` → `project->projectChanged()` → EventBus |
| Qt signal `layerCreated` | 直接 emit |
| Qt signal `projectChanged` | `notifyProjectMutation` 経由 |
| `LayerSelectionChangedEvent` | `selectLayer()` から追加発火 |

レイヤー 1 枚追加で **5 種類のイベント**が出る。

---

### 原因 D：コンポジション作成で CurrentCompositionChangedEvent が ProjectChangedEvent を誘発

`createComposition()` の流れ：

```
createComposition()
  └─ manager.createComposition()
       └─ compositionCreated signal → changeCurrentComposition(id)
            ├─ publish<CurrentCompositionChangedEvent>
            └─ (changeCurrentComposition内) → project->projectChanged() が再度発火
                 └─ publish<ProjectChangedEvent>   ← 重複
```

---

## 2. 各アクションの発火イベント一覧と影響ウィジェット

### アクション①：コンポジション作成

| 発火イベント | 購読ウィジェット | 処理 |
|------------|---------------|------|
| `CompositionCreatedEvent` | ArtifactLayerPanelWidget | updateLayout() |
| `CurrentCompositionChangedEvent` | ArtifactTimelineWidget | setComposition() → refreshTracks() |
| `CurrentCompositionChangedEvent` | ArtifactCompositionRenderController | setComposition() |
| `CurrentCompositionChangedEvent` | ArtifactCompositionEditor | setComposition() |
| `ProjectChangedEvent` | **10+ ウィジェット** | 全員フルリビルド |

### アクション②：新規レイヤー追加

| 発火イベント | 購読ウィジェット | 処理 |
|------------|---------------|------|
| `LayerChangedEvent{Created}` | ArtifactTimelineWidget | onLayerCreated() → refreshTracks() |
| `LayerChangedEvent{Created}` | ArtifactCompositionRenderController | layer.changed 購読を追加 |
| `ProjectChangedEvent` | ArtifactTimelineWidget | refreshTracks() **← 2回目** |
| `ProjectChangedEvent` | ArtifactLayerPanelWidget (Qt signal) | updateLayout() |
| `ProjectChangedEvent` | **その他 8+ ウィジェット** | フルリビルド |
| `LayerSelectionChangedEvent` | ArtifactTimelineWidget | updateSelectionState() ×3 |

**ArtifactTimelineWidget は refreshTracks() が 2 回キューに積まれる（LayerChangedEvent と ProjectChangedEvent それぞれから）。**

### アクション③：ブレンドモード変更（コンボ選択）

| 発火イベント | 購読ウィジェット | 処理 |
|------------|---------------|------|
| `ProjectChangedEvent` | ArtifactTimelineWidget | refreshTracks() ← **全トラック再構築** |
| `ProjectChangedEvent` | ArtifactLayerPanelWidget | updateLayout() ← **全レイヤーリスト再構築** |
| `ProjectChangedEvent` | ArtifactCompositionRenderController | invalidateBaseComposite() |
| `ProjectChangedEvent` | **その他 7+ ウィジェット** | フルリビルド |

→ **ブレンドモード 1 プロパティの変更で、全タイムライン・全レイヤーパネルが再描画される**

---

## 3. 実施した修正

### 修正 1（完了）：ブレンドモード変更のコンボボックス側を layer->changed() に統一

**ファイル**: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

```cpp
// Before（問題のコード）
layer->setBlendMode(mode);
if (service) {
    if (auto project = service->getCurrentProjectSharedPtr()) {
        project->projectChanged();  // ← 削除
    }
}

// After（修正後）
layer->setBlendMode(mode);
emit layer->changed();  // レンダラーが受け取り再描画するだけ
update();               // パネル自身の再描画
```

### 修正 2（完了）：addLayerToCurrentComposition で notifyProjectMutation を削除

**ファイル**: `Artifact/src/Service/ArtifactProjectService.cpp` line 215

```cpp
// Before
notifyProjectMutation(manager);  // ← 削除

// After
// LayerChangedEvent{Created} が既に発火されているため不要
```

### 修正 3（完了）：ArtifactLayerPanelWidget の Qt signal 接続を全て EventBus 購読に置き換え

**ファイル**: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

- `layerCreated` → `LayerChangedEvent{Created}` 購読
- `layerRemoved` → `LayerChangedEvent{Removed}` 購読
- `layerSelected` → `LayerSelectionChangedEvent` 購読
- `compositionCreated` → `CompositionCreatedEvent` 購読
- `projectChanged` → `ProjectChangedEvent` 購読（EventBus 経由のみ）
- コメント追加: `// Qt シグナル接続は廃止。全サービスイベントは EventBus 経由で受け取る。`

### 修正 4（完了）：ArtifactTimelineWidget の refreshTracks デバウンス追加

**ファイル**: `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

- `bool pendingRefresh_ = false;` を Impl に追加
- `ProjectChangedEvent` と `LayerChangedEvent` の両方で `scheduleRefresh()` ラムダを共有
- 同一フレームで両方発火しても `refreshTracks()` は 1 回のみ実行される

### 修正 5（完了）：applyTimelineLayerRangeEdit の svc->projectChanged() を layer->changed() に統一

**ファイル**: `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

レイヤー移動・トリムで全体 projectChanged が発火していたのを layer 個別通知に限定。

---

## 4. 優先度別まとめ（実施済み含む）

| 優先度 | 修正内容 | 状態 |
|--------|---------|------|
| ★★★ | ブレンドモードコンボで `project->projectChanged()` を `layer->changed()` に変更 | ✅ 完了 |
| ★★★ | `addLayer` の `notifyProjectMutation` 削除 | ✅ 完了 |
| ★★ | ArtifactLayerPanelWidget の Qt signal 二重購読を解除 | ✅ 完了 |
| ★★ | ArtifactTimelineWidget の refreshTracks デバウンス | ✅ 完了 |
| ★ | ProjectChangedEvent の細分化（LayerPropertyChangedEvent 等） | 中期課題 |

---

## 5. 修正後の期待される動作

| アクション | 修正前の発火数 | 修正後の発火数 |
|------------|-------------|-------------|
| ブレンドモード変更 | ProjectChangedEvent → 全ウィジェット再描画 | layer.changed() → レンダラーのみ再描画 |
| レイヤー追加 | LayerChanged + ProjectChanged + 各Qt signal | LayerChanged{Created} + SelectionChanged のみ |
| コンポジション作成 | CurrentCompositionChanged + ProjectChanged × 2 | CurrentCompositionChanged のみ |
