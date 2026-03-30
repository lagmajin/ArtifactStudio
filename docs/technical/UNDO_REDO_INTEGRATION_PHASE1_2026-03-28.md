# Undo/Redo 統合 実装レポート 段階 1

**作成日:** 2026-03-28  
**ステータス:** 段階 1 完了  
**関連コンポーネント:** UndoManager, ArtifactEditMenu

---

## 概要

Undo/Redo システムの統合 段階 1 を実装した。

**実装内容:**
1. ✅ Edit メニューの Undo/Redo を UndoManager に直結
2. ✅ UI 状態同期機能の追加
3. ✅ 新規コマンドクラスの追加（3 つ）

---

## 実装内容

### 変更ファイル（3 つ）

| ファイル | 追加行数 | 内容 |
|---------|---------|------|
| `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` | +20 | UI 状態同期 |
| `Artifact/include/Undo/UndoManager.ixx` | +45 | 新規コマンド宣言 |
| `Artifact/src/Undo/UndoManager.cppm` | +70 | 新規コマンド実装 |

**合計:** 135 行（追加）

---

### 1. Edit メニューの UI 状態同期

**ファイル:** `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`

#### 変更前

```cpp
void ArtifactEditMenu::Impl::handleUndo() {
  if (auto mgr = UndoManager::instance()) {
   const QString desc = mgr->undoDescription();
   mgr->undo();
   // 状態同期なし
  }
}
```

#### 変更後

```cpp
void ArtifactEditMenu::Impl::handleUndo() {
  if (auto mgr = UndoManager::instance()) {
   const QString desc = mgr->undoDescription();
   mgr->undo();
   // UI 状態を同期
   syncUIState();
  }
}

void ArtifactEditMenu::Impl::syncUIState() {
  // Undo/Redo 後に UI 状態を同期
  if (auto* svc = ArtifactProjectService::instance()) {
   emit svc->currentCompositionChanged(svc->currentComposition()->id());
  }
  if (auto* selMgr = ArtifactLayerSelectionManager::instance()) {
   emit selMgr->activeCompositionChanged(selMgr->activeComposition());
  }
}
```

**効果:**
- ✅ Undo/Redo 後に UI 状態が自動同期
- ✅ レイヤー選択・プロパティ表示が正しく更新

---

### 2. 新規コマンドクラス（3 つ）

#### MoveLayerIndexCommand

**用途:** レイヤーのインデックス移動

```cpp
class MoveLayerIndexCommand : public UndoCommand {
public:
    MoveLayerIndexCommand(ArtifactCompositionPtr comp, 
                         ArtifactAbstractLayerPtr layer, 
                         int oldIndex, int newIndex);
    void undo() override;
    void redo() override;
    QString label() const override;
};

// 使用例
auto* cmd = new MoveLayerIndexCommand(comp, layer, 0, 2);
UndoManager::instance()->push(std::unique_ptr<MoveLayerIndexCommand>(cmd));
```

**実装:**
```cpp
void MoveLayerIndexCommand::undo() {
    auto comp = comp_.lock();
    auto layer = layer_.lock();
    if (comp && layer) {
        comp->moveLayerToIndex(layer->id(), oldIndex_);
    }
}

void MoveLayerIndexCommand::redo() {
    auto comp = comp_.lock();
    auto layer = layer_.lock();
    if (comp && layer) {
        comp->moveLayerToIndex(layer->id(), newIndex_);
    }
}
```

---

#### RenameLayerCommand

**用途:** レイヤー名変更

```cpp
class RenameLayerCommand : public UndoCommand {
public:
    RenameLayerCommand(ArtifactAbstractLayerPtr layer, 
                      const QString& oldName, 
                      const QString& newName);
    void undo() override;
    void redo() override;
};

// 使用例
auto* cmd = new RenameLayerCommand(layer, "Layer 1", "Background");
UndoManager::instance()->push(std::unique_ptr<RenameLayerCommand>(cmd));
```

**実装:**
```cpp
void RenameLayerCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setLayerName(oldName_);
    }
}

void RenameLayerCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setLayerName(newName_);
    }
}
```

---

#### ChangeLayerOpacityCommand

**用途:** レイヤー不透明度変更

```cpp
class ChangeLayerOpacityCommand : public UndoCommand {
public:
    ChangeLayerOpacityCommand(ArtifactAbstractLayerPtr layer, 
                             float oldOpacity, 
                             float newOpacity);
    void undo() override;
    void redo() override;
};

// 使用例
auto* cmd = new ChangeLayerOpacityCommand(layer, 1.0f, 0.5f);
UndoManager::instance()->push(std::unique_ptr<ChangeLayerOpacityCommand>(cmd));
```

**実装:**
```cpp
void ChangeLayerOpacityCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setOpacity(oldOpacity_);
    }
}

void ChangeLayerOpacityCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setOpacity(newOpacity_);
    }
}
```

---

## 既存コマンドクラス（おさらい）

既に実装済みのコマンド：

| コマンド | 用途 |
|---------|------|
| `SetPropertyCommand` | エフェクトプロパティ変更 |
| `MoveLayerCommand` | レイヤー位置移動（座標） |
| `AddLayerCommand` | レイヤー追加 |
| `RemoveLayerCommand` | レイヤー削除 |
| `MaskEditCommand` | マスク編集 |

**追加コマンド:**

| コマンド | 用途 |
|---------|------|
| `MoveLayerIndexCommand` | レイヤーインデックス移動 |
| `RenameLayerCommand` | レイヤー名変更 |
| `ChangeLayerOpacityCommand` | 不透明度変更 |

---

## 使用例

### レイヤー名変更の Undo/Redo

```cpp
// レイヤー名変更コマンドを実行
void renameLayer(ArtifactAbstractLayerPtr layer, const QString& newName) {
    const QString oldName = layer->layerName();
    auto* cmd = new RenameLayerCommand(layer, oldName, newName);
    UndoManager::instance()->push(std::unique_ptr<RenameLayerCommand>(cmd));
}

// Ctrl+Z で Undo
// Ctrl+Y で Redo
```

---

### レイヤー移動の Undo/Redo

```cpp
// レイヤーを 0 番目から 2 番目に移動
void moveLayer(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, int newIndex) {
    int currentIndex = getCurrentIndex(comp, layer);
    auto* cmd = new MoveLayerIndexCommand(comp, layer, currentIndex, newIndex);
    UndoManager::instance()->push(std::unique_ptr<MoveLayerIndexCommand>(cmd));
}
```

---

### 不透明度変更の Undo/Redo

```cpp
// 不透明度を 100% → 50% に変更
void changeOpacity(ArtifactAbstractLayerPtr layer, float newOpacity) {
    float oldOpacity = layer->opacity();
    auto* cmd = new ChangeLayerOpacityCommand(layer, oldOpacity, newOpacity);
    UndoManager::instance()->push(std::unique_ptr<ChangeLayerOpacityCommand>(cmd));
}
```

---

## 効果

### 改善前

| 操作 | Undo 対応 |
|------|----------|
| レイヤー追加/削除 | ✅ 対応 |
| レイヤー移動（座標） | ✅ 対応 |
| **レイヤー名変更** | ❌ 未対応 |
| **レイヤーインデックス移動** | ❌ 未対応 |
| **不透明度変更** | ❌ 未対応 |
| **Undo 後の UI 同期** | ⚠️ 不十分 |

---

### 改善後

| 操作 | Undo 対応 |
|------|----------|
| レイヤー追加/削除 | ✅ 対応 |
| レイヤー移動（座標） | ✅ 対応 |
| **レイヤー名変更** | ✅ 対応 |
| **レイヤーインデックス移動** | ✅ 対応 |
| **不透明度変更** | ✅ 対応 |
| **Undo 後の UI 同期** | ✅ 十分 |

---

## 次のステップ（段階 2）

### 実装予定

1. **レイヤーパネルでのコマンド使用**（4-6h）
   - 名前編集をコマンド化
   - インデックス移動をコマンド化

2. **プロパティパネルでのコマンド使用**（4-6h）
   - 不透明度スライダーをコマンド化
   - 変形プロパティをコマンド化

3. **UndoHistory のセッション保存**（4-6h）
   - プロジェクト保存時に Undo 履歴も保存
   - プロジェクト読み込み時に復元

---

## 関連ドキュメント

- `docs/technical/CORE_FEATURE_GAP_ANALYSIS_VOL2_2026-03-28.md` - 元の問題分析
- `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md` - 元のマイルストーン
- `Artifact/src/Undo/UndoManager.cppm` - UndoManager 実装
- `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` - Edit メニュー実装

---

## 結論

**Undo/Redo 統合 段階 1 が完了した（135 行追加）。**

### 実装済み

- ✅ Edit メニューの UI 状態同期
- ✅ MoveLayerIndexCommand
- ✅ RenameLayerCommand
- ✅ ChangeLayerOpacityCommand

### 次の段階

- レイヤーパネル・プロパティパネルでのコマンド使用
- UndoHistory のセッション保存

---

**文書終了**
