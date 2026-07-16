# M-EDIT-1 Edit Menu Foundation Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`,
      `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`,
      `Artifact/src/Service/ArtifactClipboardService.cppm`,
      `Artifact/src/Service/ArtifactLayerService.cppm`,
      `Artifact/src/Service/ArtifactApplicationService.cppm`,
      `Artifact/src/Widgets/ArtifactMainWindow.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`,
      `Artifact/src/Widgets/Inspector/ArtifactInspectorWidget.cppm`,
      `Artifact/src/Undo/*`
位置づけ: `REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` で確認した `EditMenu 1 hit / Clipboard 18 hit / Undo redo 27 hit` を統合し、Edit / Selection / Find / Replace / Multi-selection の foundation を整える。
参照:
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- `docs/done/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md`
- `docs/planned/MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md`
- `docs/planned/MILESTONE_EASING_LAB_2026-04-21.md`
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`

---

## 1. 目的

`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` で:

- Edit menu integration: **1 hit** (`ArtifactEditMenu.cppm`)
- Clipboard support: 18 hit
- Undo / redo history: 27 hit

EditMenu は **1 hit のみ**で、layer / property / asset の統合的な **編集 UI** が弱い。本 milestone は Edit menu 全体を再設計し、Undo / Clipboard / Selection / Find & Replace を統合する。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/Menu/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理

### 2.1 既存資産

- `ArtifactEditMenu.cppm` — 最小限の edit アクション
- `ArtifactClipboardService.cppm` — 既存 clipboard 機構
- `ArtifactLayerService.cppm` — layer 操作 service
- `ArtifactTimelineWidget.cpp` — undo 経路あり
- `MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md` — Keyframe copy/paste
- `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` — 親構想

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Undo / Redo | 27 hit | 部分的。Edit menu の標準 undo/redo 表示なし |
| Clipboard | 18 hit | keyframe のみ。layer / effect / property 不在 |
| Find / Replace | 0 hit | layer 検索 / property 検索なし |
| Multi-selection | 46 hit (前回 scan) | 部分的。複数 layer に対する一括 edit 弱 |
| Group selection | 6 hit | 部分的 |
| Edit menu 統合 | 1 hit | 弱い |

---

## 3. 設計の柱

### 3.1 ArtifactEditMenu 再設計

`Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` を再設計:

```cpp
class ArtifactEditMenu : public QMenu {
public:
    void setupMenu();  // 6 セクション

    // Undo / Redo
    void setUndoStack(QUndoStack* stack);

    // Clipboard
    void setClipboardService(ArtifactClipboardService* svc);

    // Selection
    void setSelectionProvider(std::function<QList<LayerID>()> provider);
};
```

**セクション構成**:
1. **Undo / Redo**: `Ctrl+Z` / `Ctrl+Shift+Z`
2. **Clipboard**: Cut / Copy / Paste / Paste at Original / Paste at Playhead / Duplicate / Delete
3. **Selection**: Select All / Deselect / Invert Selection / Select Same Type
4. **Find**: Find Layer / Find Property / Find Effect
5. **Layer**: Group / Ungroup / Pre-compose / Enable / Disable
6. **Timeline**: Ripple Trim In / Ripple Trim Out / Ripple Delete

### 3.2 Clipboard 統合

`ArtifactClipboardService` を拡張:

```cpp
class ArtifactClipboardService {
public:
    // 既存
    void copyKeyframes(...);
    void pasteKeyframes(...);

    // 新規 (M-CLIP-1 の分担と整合)
    void copyLayers(const QList<LayerID>& layerIds);
    void pasteLayers(ArtifactComposition* target, FramePosition atFrame);

    void copyEffects(const QString& layerId, const QStringList& effectIds);
    void pasteEffects(const QString& layerId);

    void copyPropertyValue(const QString& layerId, const QString& propPath);
    void pastePropertyValue(const QString& layerId, const QString& propPath);

    // system clipboard 同期
    void setMimeData(QMimeData* mime);
    QMimeData* mimeData() const;
};
```

- `QMimeData` 経由で `application/x-artifact-*` を設定
- `M-CLIP-1 Keyframe Copy/Paste` の `ArtifactClipboardManager` と統合

### 3.3 Multi-selection

`ArtifactSelectionManager` を追加 (`Artifact/src/Service/`):

```cpp
class ArtifactSelectionManager {
public:
    void setSelection(const QList<LayerID>& ids);
    QList<LayerID> selection() const;
    void addToSelection(LayerID id);
    void removeFromSelection(LayerID id);
    void toggleSelection(LayerID id);
    void clearSelection();
    void selectAll(ArtifactComposition* comp);
    void selectSameType(ArtifactComposition* comp, const QString& type);
    void invertSelection(ArtifactComposition* comp);

signals:
    void selectionChanged(const QList<LayerID>& selection);
};
```

- 既存 `ArtifactLayerSelectionManager` の上に薄い抽象
- Edit menu から参照

### 3.4 Find & Replace

`Artifact/src/Service/ArtifactFindService.cppm`:

```cpp
class ArtifactFindService {
public:
    // Layer search
    QList<LayerID> findLayers(const QString& query, ArtifactComposition* comp);

    // Property search
    QList<QPair<LayerID, QString>> findProperties(
        const QString& query, ArtifactComposition* comp);

    // Effect search
    QList<QPair<LayerID, QString>> findEffects(
        const QString& query, ArtifactComposition* comp);

    // Replace
    bool replaceProperty(const LayerID& layer,
                        const QString& propPath,
                        const QString& fromValue,
                        const QString& toValue);
};
```

- `QRegularExpression` ベース
- Layer / Property / Effect の 3 軸
- Timeline 上に検索結果 highlight

### 3.5 Group / Ungroup

`Artifact/src/Service/ArtifactGroupService.cppm`:

```cpp
class ArtifactGroupService {
public:
    LayerID groupLayers(const QList<LayerID>& layerIds, const QString& name);
    bool ungroupLayer(LayerID groupId);
    QList<LayerID> groupMembers(LayerID groupId) const;
};
```

- 既存 `ArtifactGroupLayer` の上に薄い service
- 親子の整合性を `MILESTONE_LAYER_GROUP_SYSTEM_2026-03-27.md` の Phase 1〜2 と並走

### 3.6 Undo Manager 統合

`Artifact/src/Undo/ArtifactEditManager.cppm`:

```cpp
class ArtifactEditManager : public QObject {
public:
    static ArtifactEditManager& instance();

    // QUndoStack ラッパ
    QUndoStack* undoStack();

    // カスタム command
    void push(QUndoCommand* cmd);

    // アクション
    QAction* undoAction(QObject* parent);
    QAction* redoAction(QObject* parent);
};
```

- 既存 `UndoManager` シングルトンと並走
- Edit menu に undo / redo action を供給

### 3.7 Shortcut 統合

`MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` の `Global` context に登録:

| Action | Shortcut |
|---|---|
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Shift+Z` / `Ctrl+Y` |
| Cut | `Ctrl+X` |
| Copy | `Ctrl+C` |
| Paste | `Ctrl+V` |
| Paste at Original | `Ctrl+Alt+V` |
| Duplicate | `Ctrl+D` |
| Delete | `Delete` |
| Select All | `Ctrl+A` |
| Deselect | `Ctrl+Shift+A` |
| Find | `Ctrl+F` |
| Find Next | `F3` / `Ctrl+G` |
| Replace | `Ctrl+H` |
| Group | `Ctrl+G` |
| Ungroup | `Ctrl+Shift+G` |

### 3.8 Project 保存

- `ArtifactProjectManager` の project JSON に `selection` セクション追加 (一時保存用)
- 旧プロジェクトは selection 欠落を許容

---

## 4. フェーズ計画

### Phase 1: ArtifactEditMenu 再設計 (P0, 1 セッション)

- 6 セクション構成
- Undo / Redo / Clipboard / Selection / Find / Layer / Timeline
- 既存 edit アクションの統合

**Done criteria:**
- Edit menu に 6 セクション表示
- 全項目に keyboard shortcut

### Phase 2: ClipboardService 拡張 (P0, 2 セッション)

- `copyLayers / pasteLayers / copyEffects / pasteEffects / copyPropertyValue / pastePropertyValue` 実装
- `QMimeData` 経由の system clipboard 同期
- `M-CLIP-1` の `ArtifactClipboardManager` と統合

**Done criteria:**
- layer / effect / property の copy / paste が動作
- system clipboard に `application/x-artifact-*` を設定
- 1 undo で完全復元

### Phase 3: SelectionManager (P0, 1 セッション)

- `ArtifactSelectionManager` 実装
- Select All / Deselect / Invert / Select Same Type
- `selectionChanged` signal

**Done criteria:**
- multi-selection が service 経由で管理
- Edit menu に反映

### Phase 4: Find & Replace (P0, 1〜2 セッション)

- `ArtifactFindService` 実装
- Layer / Property / Effect の 3 軸検索
- Timeline 上に検索結果 highlight

**Done criteria:**
- `Ctrl+F` で find dialog 表示
- 検索結果クリックで該当 layer / property にジャンプ
- Replace 機能動作

### Phase 5: Group / Ungroup (P0, 1 セッション)

- `ArtifactGroupService` 実装
- 親子整合
- Undo 対応

**Done criteria:**
- 選択 layer を `Ctrl+G` でグループ化
- `Ctrl+Shift+G` で ungroup
- 1 undo で完全復元

### Phase 6: EditManager 統合 (P0, 1 セッション)

- `ArtifactEditManager` 実装
- undo / redo action を Edit menu に供給

**Done criteria:**
- Edit menu の Undo / Redo 表示が動的に更新
- 全 edit アクションが Undo 経由

### Phase 7: Project 保存 + Diagnostics (P1, 1 セッション)

- selection を project JSON に保存
- Problem View に `edit.*` 健全性 contribution

**Done criteria:**
- project 保存 → 再読込で selection 復元
- 旧プロジェクトが開ける

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md` | ClipboardService の keyframe 部分を分離。並走。 |
| `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` | 親構想。本 milestone は Edit menu 統合。 |
| `MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` | shortcut 登録先。 |
| `MILESTONE_LAYER_GROUP_SYSTEM_2026-03-27.md` | Group 実装。本 milestone は service 化。 |

---

## 6. 不変条件 (Guardrails)

- 既存 `ArtifactEditMenu.cppm` の API は温存 (deprecated 警告のみ)
- 新規 signal-slot 接続は `selectionChanged / clipboardChanged / findResultsChanged` の 3 個に限定
- `QImage` / `setStyleSheet` 流入禁止
- `M-CLIP-1 Keyframe Copy/Paste` の `ArtifactClipboardManager` を破壊しない
- Find は `QRegularExpression` ベース (独自 parser を追加しない)
- Group / Ungroup は `MILESTONE_LAYER_GROUP_SYSTEM` と整合

---

## 7. Done Criteria (全体)

- Edit menu に 6 セクション表示
- 全項目に keyboard shortcut
- layer / effect / property の copy / paste が動作
- system clipboard に `application/x-artifact-*` を設定
- Select All / Invert / Select Same Type が動作
- Find dialog で layer / property / effect を検索
- Group / Ungroup が Undo 対応
- Edit menu の Undo / Redo 表示が動的更新
- 1 undo で完全復元
- project 保存 → 再読込で selection 復元
- 旧プロジェクトが開ける
- 新規 `QImage` / `setStyleSheet` / signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2 を正式 milestone に起こした。
# 2026-07-10 Find Similar Progress

- Composition Editor Command Palette に `Find Similar / Select Related` を追加
- Same Layer Type / Source Media / Parent / Effect Set / Font を基準に動的選択できる
- 結果は既存Selection Managerへ返し、Batch / Recipe / QAへそのまま渡せる

## 2026-07-10 Safe Delete Progress

- selected layer削除前に外部parent / matte source / expression文字列参照を監査する
- 選択内effect数と最大16件の依存関係を確認表示してから既存削除経路へ渡す
- Parametric Composition input bindingのsourceLayerId参照も監査対象へ追加
- `MacroUndoCommand` + `RemoveLayerCommand` により複数layer削除を1回でUndo/Redo可能にした
- Render QueueはComposition単位でありlayer参照を保持しないため、layer Safe Deleteの監査対象外
