# MILESTONE: Undo Framework Hardening

**日付**: 2026-08-04
**最終更新:** 2026-08-05
**実装状況:** `UndoCommand::estimatedMemoryBytes()`、UndoBudget（件数・総メモリ・単一エントリ上限）、古いエントリからの予算エビクション、全コマンドカテゴリ（JSON／KeyFrame／LayerMask／画像／Composition／コールバック）のサイズ見積もり、optional serialization 契約、version／サイズ／JSON形状を検証する履歴 JSON 保存／Factory ロード API、`AnimationLayerStackSnapshotCommand`・`AlignLayersUndoCommand`・`InOutPointsSnapshotCommand`・`MaskEditCommand`（LayerMask Codec）・`SetEffectMaskImagesCommand`（RGBA32F Codec）・`MoveAssetFileCommand`・`MacroUndoCommand`・`SetPropertyCommand`・`AddLayerCommand`・`RemoveLayerCommand`・`MoveLayerCommand`・`ChangeLayerOpacityCommand`・`RenameLayerCommand`・表示／ロック／Solo／Shy／Active Variant／Blend Mode 切替・`MoveLayerIndexCommand`・`SetTextLayerTextCommand`・`ReplaceLayerSourceCommand`・限定許可型 Codec 付き `SetLayerPropertyKeyframesCommand`・`ChangeLayerMatteReferencesCommand` の serialization、Effect／Layer／Composition／InOutPoints resolver、対応コマンドの `OffloadedUndoCommand` によるディスク退避・Undo/Redo 時復元、履歴クリア時の生成ファイル cleanup を追加。既存コマンドは安全のため明示 opt-in なしではメモリ保持。既存34コマンド全体の個別 serialization は未完了。
**現状**: `UndoManager`（アプリ層, 34 コマンド, 100 エントリ制限）と `Command` モジュール（Core 層, `QUndoCommand`, コラボ向け）の2系統が並行。アプリ層のメモリ予算・ディスクオフロード・履歴永続化は実装済み。`SerializableCommand` は `ISerializable` を継承し、`commandType()` を typeName、schemaVersion=1 として共通契約へ接続した。`type`／`schemaVersion`／`data` の typed envelope、Factory の入力／deserialize 失敗検証、creator map の mutex 保護も追加済み。両系統の実運用統合は未完了。
**目標**: エントリ数制限→バイト予算制限移行、大規模スナップショットのディスクオフロード、セッション間 Undo 履歴永続化、Command モジュールの統合または廃止。

## Update 2026-08-15

- `UndoManager` の現行実装には `UndoBudget`（件数・総メモリ・単一エントリ上限）、`estimatedMemoryBytes()`、古い履歴の予算エビクション、`memoryPressure()`、`OffloadPolicy`、`OffloadedUndoCommand` がある。少なくとも主要な画像・レイヤー・マスク・キーフレーム・Composition 系コマンドには個別のサイズ見積もりが実装されている。
- typed envelope の serialization／Factory／resolver と、`saveSessionHistory()` / `loadSessionHistory()` の履歴 JSON API が存在する。Macro、InOutPoints、layer/property 系など複数コマンドの復元経路も確認できる。
- ただしヘッダ既定の `canSerialize() == false` を持つコマンドが残り、現行文書に記載された「34コマンド全体の個別 serialization 未完了」と整合する。全操作をセッション間で復元できる状態とはみなさない。
- `OffloadPolicy` の既定値は `Never` であり、OnPressure／Always の動作は API とコード経路の存在までは確認できるが、OOM 回避・Undo/Redo 復元・cleanup の実運用結果は未検証である。`memoryPressureChanged` の UI 表示も確認できない。
- Core の `Command` モジュールと Artifact の `UndoManager` は依然として並行している。よって現状は `budget / offload / history foundation implemented; command coverage, default policy, UI diagnostics, cross-session runtime validation pending` と整理する。

## 現状のボトルネック

`UndoManager::Impl`:
```cpp
std::vector<std::unique_ptr<UndoCommand>> undoStack;  // 全コマンドがメモリ常駐
size_t maxHistorySize_ = 100;                          // カウント制限のみ
// バイト予算: なし
// ディスクオフロード: なし
// 終了時の履歴消滅: あり
```

100 エントリ制限の問題: `SetEffectMaskImagesCommand` が `vector<SharedPtr<ImageF32x4_RGBA>>` を保持すると、4K 画像2枚×100エントリで ~6.4GB のメモリを消費しうる。

---

## Phase 1: バイト予算 + エントリ制限

### 1.1 メモリ予算の導入

```cpp
// UndoManager に追加
struct UndoBudget {
    size_t maxEntryCount = 100;          // 既存のカウント制限（維持）
    size_t maxMemoryBytes = 256 * 1024 * 1024;  // 新規: 256MB
    size_t maxSingleEntryBytes = 64 * 1024 * 1024; // 1エントリ最大 64MB
};

class UndoManager {
public:
    void setBudget(const UndoBudget& budget);
    const UndoBudget& budget() const;
    
    // 現在の使用量
    size_t currentMemoryBytes() const;
    size_t currentEntryCount() const;
    float memoryPressure() const;  // 0.0-1.0
    
    W_SIGNAL(memoryPressureChanged, float pressure)

private:
    void enforceBudget();  // push 後に超過分を切り詰め
    void estimateEntrySize(UndoCommand& cmd);  // コマンドサイズの見積もり
};
```

### 1.2 コマンドサイズ見積もり

`UndoCommand` 基底にサイズ見積もりメソッドを追加:

```cpp
class UndoCommand {
public:
    // 既存 ...
    
    // このコマンドが占有するおおよそのメモリ量（バイト）
    virtual size_t estimatedMemoryBytes() const {
        // デフォルト: 1KB（文字列や数値の小さいコマンド用）
        return 1024;
    }
};

// 具体例
size_t SetEffectMaskImagesCommand::estimatedMemoryBytes() const {
    size_t total = 0;
    for (auto& img : oldImages_) {
        if (img) total += img->width() * img->height() * 4 * sizeof(float);
    }
    for (auto& img : newImages_) {
        if (img) total += img->width() * img->height() * 4 * sizeof(float);
    }
    return total;
}

size_t SetLayerPropertyKeyframesCommand::estimatedMemoryBytes() const {
    // キーフレームベクター × 2（before + after）
    return (oldKeyframes_.size() + newKeyframes_.size()) * sizeof(KeyFrame);
}
```

### 1.3 予算超過時のエビクション

```cpp
void UndoManager::enforceBudget() {
    // カウント制限
    while (impl_->undoStack.size() > budget_.maxEntryCount) {
        impl_->undoStack.erase(impl_->undoStack.begin());
    }
    
    // バイト制限
    size_t totalBytes = currentMemoryBytes();
    while (totalBytes > budget_.maxMemoryBytes && !impl_->undoStack.empty()) {
        auto& oldest = impl_->undoStack.front();
        totalBytes -= oldest->estimatedMemoryBytes();
        impl_->undoStack.erase(impl_->undoStack.begin());
    }
    
    // シグナル発行
    float pressure = static_cast<float>(totalBytes) / budget_.maxMemoryBytes;
    memoryPressureChanged(pressure);
}
```

### 1.4 完了条件

- [ ] `estimatedMemoryBytes()` が全 34 コマンドで実装される
- [ ] 256MB 制限超過時に最も古いエントリから削除される
- [ ] 単一エントリが 64MB を超える場合、警告 + 記録後に削除
- [ ] `memoryPressure` シグナルが UI に表示される（ステータスバー等）

---

## Phase 2: ディスクオフロード

### 2.1 OffloadPolicy

```cpp
enum class OffloadPolicy {
    Never,          // 常にメモリ（デフォルト）
    OnPressure,     // メモリ圧力が高い時のみ
    Always          // 常にディスク
};

class UndoManager {
public:
    void setOffloadPolicy(OffloadPolicy policy);
    void setOffloadDirectory(const QString& path);  // デフォルト: {projectDir}/.undo/
    
private:
    // オフロード（メモリ→ディスク）
    bool offloadEntry(size_t stackIndex);
    // リストア（ディスク→メモリ、undo/redo 実行時）
    bool restoreEntry(size_t stackIndex);
};
```

### 2.2 オフロード実装

```cpp
bool UndoManager::offloadEntry(size_t index) {
    auto& cmd = impl_->undoStack[index];
    
    // コマンドを JSON にシリアライズ
    QJsonObject json;
    json["type"] = QString::fromStdString(cmd->commandType());
    json["data"] = cmd->toJson();
    json["estimatedBytes"] = static_cast<qint64>(cmd->estimatedMemoryBytes());
    
    // ディスクに書き出し
    QString path = offloadDir_ + "/undo_" + QString::number(impl_->nextOffloadId_++) + ".json";
    QSaveFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
    file.commit();
    
    // UndoCommand の実体はディスクに移動、メモリにはスタブを残す
    impl_->undoStack[index] = std::make_unique<OffloadedUndoCommand>(
        path, cmd->estimatedMemoryBytes(), cmd->commandType()
    );
    
    return true;
}

// OffloadedUndoCommand: 実行時にディスクから再構築
class OffloadedUndoCommand : public UndoCommand {
public:
    void redo() override {
        auto real = restore();
        real->redo();
    }
    
    void undo() override {
        auto real = restore();
        real->undo();
    }
    
private:
    std::unique_ptr<UndoCommand> restore() {
        // ディスクから JSON 読み込み → CommandFactory で再構築
        QFile file(offloadPath_);
        file.open(QIODevice::ReadOnly);
        QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        return UndoCommandFactory::createFromJson(json["type"].toString(), json["data"].toObject());
    }
    
    QString offloadPath_;
    std::string commandType_;
};
```

### 2.3 自動オフロード戦略

`push()` 後の処理:

```cpp
void UndoManager::push(std::unique_ptr<UndoCommand> cmd) {
    cmd->redo();
    impl_->undoStack.push_back(std::move(cmd));
    impl_->redoStack.clear();
    impl_->version_++;
    
    // 予算超過分を削除
    enforceBudget();
    
    // オフロードポリシーに従ってディスクに退避
    if (offloadPolicy_ == OffloadPolicy::Always) {
        // 新しく追加されたエントリ以外をオフロード
        for (size_t i = 0; i < impl_->undoStack.size() - 1; ++i) {
            offloadEntry(i);
        }
    } else if (offloadPolicy_ == OffloadPolicy::OnPressure && memoryPressure() > 0.8f) {
        // 圧力が 80% を超えたら古いエントリからオフロード
        for (size_t i = 0; i < impl_->undoStack.size() / 2; ++i) {
            offloadEntry(i);
        }
    }
}
```

### 2.4 完了条件

- [ ] 256MB のマスク画像コマンドを含む Undo スタックが 100 エントリを超えても OOM しない
- [ ] オフロードされたエントリの undo/redo が正しく動作
- [ ] オフロードディレクトリがプロジェクトクローズ時にクリーンアップされる
- [ ] `OnPressure` モードでメモリ圧力に応じて動的にオフロード

---

## Phase 3: セッション間 Undo 履歴の永続化

### 3.1 プロジェクトクローズ時の保存

```cpp
void UndoManager::saveSessionHistory(const QString& projectPath) {
    QString undoPath = projectPath + ".undohist";
    QSaveFile file(undoPath);
    file.open(QIODevice::WriteOnly);
    
    QJsonObject root;
    root["version"] = 1;
    root["entryCount"] = static_cast<int>(impl_->undoStack.size());
    root["savedVersion"] = static_cast<qint64>(impl_->savedVersion_);
    
    QJsonArray entries;
    size_t savedCount = 0;
    for (size_t i = 0; i < impl_->undoStack.size() && savedCount < kMaxPersistedEntries; ++i) {
        auto& cmd = impl_->undoStack[i];
        // スナップショット系コマンドのみ保存（軽量なものはスキップ）
        if (cmd->estimatedMemoryBytes() < kMaxPersistEntryBytes) {
            QJsonObject entry;
            entry["type"] = QString::fromStdString(cmd->commandType());
            entry["data"] = cmd->toJson();
            entries.append(entry);
            ++savedCount;
        }
    }
    root["entries"] = entries;
    
    file.write(QJsonDocument(root).toJson());
    file.commit();
}

bool UndoManager::loadSessionHistory(const QString& projectPath) {
    QString undoPath = projectPath + ".undohist";
    QFile file(undoPath);
    if (!file.exists()) return false;
    
    file.open(QIODevice::ReadOnly);
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    
    // 保存時と同じプロジェクト状態か検証
    if (root["savedVersion"].toInteger() != impl_->savedVersion_) {
        return false;  // プロジェクトが変更されている → 履歴無効
    }
    
    // エントリを復元
    for (auto& entryVal : root["entries"].toArray()) {
        QJsonObject entry = entryVal.toObject();
        auto cmd = UndoCommandFactory::createFromJson(
            entry["type"].toString(), entry["data"].toObject()
        );
        if (cmd) {
            impl_->undoStack.push_back(std::move(cmd));
        }
    }
    
    return true;
}
```

### 3.2 完了条件

- [ ] プロジェクトを閉じて再度開いた後、Ctrl+Z で前回セッションの変更が元に戻せる
- [ ] `.undohist` ファイルのサイズが妥当（最大 50MB 程度）
- [ ] プロジェクトが変更されている場合、古い履歴が読み込まれない

---

## Phase 4: Command モジュールの整理

### 4.1 統合または廃止

2系統ある Command システムの整理:

| モジュール | 現在の使用 | 方針 |
|-----------|----------|------|
| `UndoManager` (Artifact) | **実際に使われている** | **主システムとして維持・強化** |
| `Command` (ArtifactCore) | `SerializableCommand` の基底契約／Factory は存在するが、`EditSessionManager` ブリッジとコラボ用 `LayerLockManager` が未接続 | コラボ用に分離・再定義 |

`Command` モジュールを UndoManager と統合せず、**コラボレーション専用のプロトコルレイヤー**として再定義する:

```cpp
// Command モジュールの新しい責務
// - コラボ編集のための SerializableCommand（ネットワーク転送可能）
// - EditSession: 複数ユーザーの同時編集セッション管理
// - LayerLockManager: コラボ編集時のレイヤーロック（既存維持）
// - CommandFactory: SerializableCommand の JSON → インスタンス復元
//
// 削除するもの:
// - LambdaCommand（UndoManager 側の SetPropertyCommand 等で十分）
// - EditSessionManager（UndoManager::instance() で代用）
```

### 4.2 SerializableCommand の実装

既存の `SerializableCommand` 基底契約を `ISerializable`／コラボ編集プロトコルへ接続する:

```cpp
class SerializableCommand : public QUndoCommand, public ISerializable {
public:
    // JSON payload と typed envelope（type/schemaVersion/data）を使用
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    QString commandType() const override;
    QString typeName() const override;
    int schemaVersion() const override;
    QJsonObject toJson() const;
};
```

### 4.3 完了条件

- [ ] `Command` モジュールの責務が明確に文書化される（コラボ専用）
- [x] `SerializableCommand` の基底契約と Factory の不正入力／deserialize 失敗拒否
- [x] `SerializableCommand` を `ISerializable` の typeName／schemaVersion 契約へ接続
- [ ] `SerializableCommand` をコラボ編集プロトコルへ接続
- [ ] `EditSessionManager` が UndoManager を参照せず独立
- [ ] 重複機能（LambdaCommand 等）が削除される

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `Artifact/include/Undo/UndoManager.ixx` | `UndoBudget`, `estimatedMemoryBytes()` 追加 |
| P1 | `Artifact/src/Undo/UndoManager.cppm` | 予算強制、サイズ見積もり実装 |
| P2 | `Artifact/src/Undo/UndoOffload.cppm` | 新規: ディスクオフロード実装 |
| P2 | `Artifact/include/Undo/OffloadedUndoCommand.ixx` | 新規: オフロードスタブ |
| P3 | `Artifact/src/Undo/UndoManager.cppm` | `saveSessionHistory()` / `loadSessionHistory()` |
| P4 | `ArtifactCore/include/Command/SerializableCommand.ixx` | `ISerializable`／コラボ用契約への接続 |
| P4 | `ArtifactCore/include/Command/EditSessionManager.ixx` | 責務分離・文書化 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: バイト予算 | **P0** | 中 | OOM リスクの直接解決。34 コマンドすべての見積もり実装が主作業 |
| P2: ディスクオフロード | **P0** | 中 | JSON シリアライズ基盤（Serialization Framework M5 完了後）に依存 |
| P3: 履歴永続化 | **P1** | 小 | Phase 2 の JSON 出力を `.undohist` に保存するだけ |
| P4: Command 統合 | **P2** | 小 | 削除・文書化が主。新規コードは最小限 |
