# MILESTONE: Undo Framework Hardening

**日付**: 2026-08-04
**最終更新:** 2026-08-31
**実装状況:** `UndoCommand::estimatedMemoryBytes()`、UndoBudget（件数・総メモリ・単一エントリ上限）、古いエントリからの予算エビクション、全コマンドカテゴリ（JSON／KeyFrame／LayerMask／画像／Composition／コールバック）のサイズ見積もり、optional serialization 契約、version／サイズ／JSON形状を検証する履歴 JSON 保存／Factory ロード API、layer／project item／playback／resolution remap／modulationを含む主要なserializable command群、Effect／Layer／Composition／InOutPoints resolver、対応コマンドの `OffloadedUndoCommand` によるディスク退避・Undo/Redo 時復元、履歴クリア時の生成ファイル cleanup を追加した。現在、`commandType()` とfactoryの静的対応は42組である。callbackやselection snapshotなどsession境界へ持ち込むと外部状態を欠落させるコマンドは明示的に永続化対象外とし、runtime／session受入は未完了である。
**現状**: `UndoManager`（アプリ層, 主要serializable command 42組, 100 エントリ制限）と `Command` モジュール（Core 層, `QUndoCommand`, コラボ向け）の2系統が並行。アプリ層のメモリ予算・ディスクオフロード・履歴永続化は実装済み。`SerializableCommand` は `ISerializable` を継承し、`commandType()` を typeName、schemaVersion=1 として共通契約へ接続した。`type`／`schemaVersion`／`data` の typed envelope、Factory の入力／deserialize 失敗検証、creator map の mutex 保護も追加済み。両系統の実運用統合は未完了。
**目標**: エントリ数制限→バイト予算制限移行、大規模スナップショットのディスクオフロード、セッション間 Undo 履歴永続化、Command モジュールの統合または廃止。

## Update 2026-08-15

- `UndoManager` の現行実装には `UndoBudget`（件数・総メモリ・単一エントリ上限）、`estimatedMemoryBytes()`、古い履歴の予算エビクション、`memoryPressure()`、`OffloadPolicy`、`OffloadedUndoCommand` がある。少なくとも主要な画像・レイヤー・マスク・キーフレーム・Composition 系コマンドには個別のサイズ見積もりが実装されている。
- typed envelope の serialization／Factory／resolver と、`saveSessionHistory()` / `loadSessionHistory()` の履歴 JSON API が存在する。Macro、InOutPoints、layer/property 系など複数コマンドの復元経路も確認できる。
- ただしヘッダ既定の `canSerialize() == false` を持つコマンドが残り、callbackやselection snapshotなど外部状態に依存する操作は意図的にsession境界から除外している。全操作をセッション間で復元できる状態とはみなさない。
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

## Update 2026-08-30

- `SetLayerPropertyKeyframesCommand` の constructor layer ID初期化と、`SetLayerPropertyValueCommand` の layer ID／JSON codec／factoryを追加した。
- `SetTextAnimatorStackCommand` の factory登録漏れを補完し、`commandType()` を持つ全 command type と factory名の静的照合を一致させた。
- Undo／Redo の履歴位置を state ID で保持し、保存済み位置との dirty 判定が操作回数ではなく履歴位置の同一性を比較するようにした。履歴 JSONには `currentVersion` を追加し、旧形式は `savedVersion` を fallback とする。
- これらは source／diff の静的確認のみで、実際の offload、save/load、Text Animator／Particle の復元、runtime dirty 表示は未検証である。

## Update 2026-08-30 (continued)

- `ArtifactPropertyWidget` の通常レイヤープロパティ行で、初回 preview／commit 前の各選択レイヤー値を snapshot し、確定時に `SetLayerPropertyValueCommand` を `MacroUndoCommand` として記録するようにした。複数選択でも一回の編集を一回の Undo 操作として扱う。

- AI／automation の代表的な layer・composition setter を既存の Undo command へ接続した。Layer state、Blend／Opacity、2D Transform、Parent、Layer／Composition Note は no-op を除外して before／after を保持し、複数チャンネルや Template Variation は macro 境界を持つ。Effect scalar parameter は `SetPropertyCommand`、Effect parameter の keyframe／expression は専用 snapshot command を利用する。Property Widget の Effect-owned 編集全体の session history は引き続き未検証である。

- AI の group／solid／noise layer 作成を `AddLayerCommand` に、split／ripple delete／sequential align を timing property と layer add／remove の macro に接続した。遅延 macro でも同一 layer の連続 slot／timing更新を失わないよう、pending state と更新後の end time を保持する。group 階層をまたぐ move／ungroup の membership・順序 snapshot は未対応である。

## Update 2026-08-30 (continued 4)

- group階層のAI操作も、moveは親ID変更のmacro、ungroupは子の親解除と`RemoveLayerCommand`のmacroへ接続した。既存のcomposition orderを使うため、Undo／Redoで子の順序とグループ位置を保持できる。選択状態、runtime cache、session reloadは未検証である。

- 一般レイヤーのmove／remove／duplicateも共通ProjectServiceでUndo化した。duplicateは既存サービスが作成した完全な複製layerを`AddLayerCommand`とindex移動macroへ再接続し、AIと通常サービスの境界を揃えた。選択状態、runtime cache、session reloadは未検証である。

- AIの一般 keyframe API (`setKeyframe`／`batchSetKeyframes`／`deleteKeyframe`) も、property単位のbefore／after keyframe列とanimatable flagを`SetLayerPropertyKeyframesCommand`へ渡すようにした。単発操作はcompositionのFPSを維持し、一括操作はpropertyごとに一つのmacro childへまとめる。runtime／session reloadは未検証である。
- Effect editor の既存 callback と新規 signal／slot は変更していない。スクラブの Esc／focus-out cancel では snapshot を破棄する既存行の cancel callback を利用する。keyframe mode、runtime／session reload は未検証である。
- Expression の Clear／Convert／Bake について、expression before／after を保持する `SetLayerPropertyExpressionCommand` を追加し、Convert は keyframe snapshot と macro 化した。command factory／JSON codec も追加したが、sampled value の全型と runtime／session reload は未検証である。

- CommandIR経由の `set_keyframes`／`batch_set_keyframes` も独自の直接snapshot commandから `SetLayerPropertyKeyframesCommand` へ移行した。モーションスケッチ／自動向き生成は `batchSetKeyframes` 一回へ集約し、EaseInOut／Bezierの入力名も扱う。旧payloadのtimeScale省略は30fps fallbackを維持し、runtime／session reloadは未検証である。

- AIのeffect複製を既存の `addEffectToLayerWithUndo()` 経路へ接続した。複製元の全keyframe／expressionコピーは別仕様として残り、runtime／session reloadは未検証である。

- 外部 `CommandIRExecutor` のkeyframe実行を `WorkspaceAutomation::batchSetKeyframes()` 一回へ集約し、全件preflightと `CommandResult.valid` のdispatch補正を追加した。property欠落・不正payloadによるpartial mutationをmutation前に拒否する。runtime／session reloadは未検証である。

- AIの音声編集もUndo境界へ接続した。trim in／outとde-click設定は`SetLayerPropertyValueCommand`のmacro、playback rateは同command、de-click範囲はbefore／afterを保持する`SetAudioDeClickRangesCommand`を利用する。範囲の正規化・マージ、レイヤーのresampled cache無効化、session JSON factoryを追加したが、runtime／session reloadは未検証である。

- 共通ProjectServiceのlayer renameも`RenameLayerCommand`へ接続した。同値名はno-opとし、AIと通常サービスでUndo境界を共有する。selection、project item表示、session reloadは未検証である。

- composition renameも、composition本体と対応するProject View `CompositionItem`名をシリアライズ可能な`RenameCompositionCommand`で更新するようにした。別project切替後の履歴復元、current composition、session reloadは未検証である。

- 通常のProjectServiceの表示・ロック・Solo・Shy変更も既存の状態commandへ接続した。Solo Only／Smart Soloは各レイヤーのbefore／afterをmacroにまとめ、親付け／親解除は`ChangeLayerParentCommand`で親IDを保持する。親レイヤー削除と同時の復元、selection、runtime cache、session reloadは未検証である。

- Undo付きsplitは、フレーム範囲・timing lock・project存在・duplicate結果を確認してから元layerを短縮するようにし、複製失敗時の部分変更を避ける。失敗コマンドの履歴登録可否、selection、runtime cache、session reloadは未検証である。

- Project Viewの非Composition項目の改名・フォルダ移動も、IDベースの`RenameProjectItemCommand`／`MoveProjectItemCommand`へ接続した。履歴再適用時に現在projectのtreeから対象を再解決するため、対象消失時の誤適用を避ける。Project item削除とComposition削除のsafe-write表示は、現状Undoがないため`undoAvailable=false`へ補正した。

- AIのProject Viewフォルダ作成は、作成前後の項目ID集合を比較して新規Folderの実体・名前・親を確認してから成功を返すようにした。フォルダ生成後のUndo、selection、session reloadは未検証である。

- Project Viewの一括改名・フォルダ移動は、全対象を先に検証し、失敗時のpartial mutationを避けるようにした。成功時は`MacroUndoCommand`一つへまとめ、各項目のUndoを分裂させない。同値項目はno-opとして履歴へ積まない。selection、別project切替、session reload、runtimeは未検証である。

- Timelineのplayhead分割をAI／ActiveContextとも`splitLayerWithUndo()`へ接続し、ActiveContextのLayer In／Out／Trimも既存のtiming property commandへ接続した。Trim InのIn PointとSource Startは一つのmacroにまとめ、境界外・timing lock・同値入力は履歴へ積まない。UndoManager不在時もlayer ID差分で成功判定する。selection、current frame、cache、session reloadは未検証である。

- AIのIn／Out point、marker、chapter、marker clearは、既存PlaybackServiceのbefore／after JSON snapshotと`InOutPointsSnapshotCommand`へ委譲した。UndoManager不在時の無条件pushを避け、同値操作は履歴へ積まない。Work Area本体、current frame、cache、session reloadは未検証である。

- 共通ProjectServiceのlayer作成も、ProjectManagerが生成した完全なlayerをdetachして既存`AddLayerCommand`、index移動、親変更を`Create Layer` macroへ再接続するようにした。AI別名と通常UIで初期配置込みの一操作Undo境界を共有する。selection、creation event、runtime cache、session reloadは未検証である。

- Work Areaの開始・終了・現在frameへの移動も、before／after frameを保持する`SetCompositionWorkAreaCommand`へ接続した。Undo／Redo時は既存`WorkAreaChangedEvent`とPlayback Engineの範囲同期をcallbackで実行する。callbackを含むためsession history／別composition復元は未対応で、runtime受入も未検証である。

- Group／UngroupのUndo wrapperも、Undo中にUndo対応済みserviceを再入呼出ししないよう修正した。groupはAdd／親変更、ungroupは親解除／Removeを一つのmacroへ直接構成し、適用後のcomposition実体を検証する。selection、order、予算超過時の復元、runtime／session reloadは未検証である。

- Group／Ungroup macroのselectionも`LayerSelectionSnapshotCommand`でbefore／afterを保持するようにした。Undo／Redoの構造変更順序に合わせ、作成group／解除後の子layer選択とcurrent layerを復元する。別composition時のbridge、runtime、session reloadは未検証である。

- Project Viewのフォルダ作成を、作成前後のID差分で特定した新規Folderと`CreateProjectFolderCommand`へ接続した。Undoは空フォルダだけを削除し、Redoは同じID・名前・親・tagを復元する。同名既存フォルダの誤認を避けるが、子項目を持つ場合のUndo、selection、session reload、runtimeは未検証である。

- Project item削除は、CompositionまたはCompositionを含むFolderを対象外とし、Folder／Footage／SolidのサブツリーをJSON snapshot・親ID・兄弟index付きの`RemoveProjectItemCommand`へ接続した。Undo／Redoで同じIDのサブツリーを元位置へ復元・削除し、snapshotが単一エントリ上限を超える場合は削除前に拒否する。safe-writeの`undoAvailable`も実際の対象型に合わせる。Composition／render queue復元、selection、session reload、runtimeは未検証である。

- `MoveMaskCommand`もlayer ID・前後indexのJSON serializationとfactoryを持つようにし、Undo／Redo後にlayer changed通知を通す。maskの同一性が別操作・session境界で変わる場合、selection、runtime、session reloadは未検証である。

- Composition解像度Remapも、composition ID・旧／新サイズ・RemapPolicyと、既存のmask／transform property／keyframe snapshotをJSON化する`ChangeCompositionResolutionCommand`のcodec・factoryを追加した。keyframe valueが許可codec外の場合はserializable対象から除外し、runtime remap、session reload、別composition復元は未検証である。

## Update 2026-08-30 (continued 2)

- Layer-owned property の Expression Copilot apply callback も `SetLayerPropertyExpressionCommand` を通るようにし、launcher 前段の直接 expression mutation を除去した。同値入力は no-op とした。Effect-owned property は既存の直接更新を維持した。
- `SetLayerPropertyKeyframesCommand` の復元／JSON codec が Anchor、Color Label、roving を保持するようにした。旧 JSON の欠落 metadata は既定値へ戻す。
- 複数選択時の keyframe mirror は target ごとの keyframe snapshot を `MacroUndoCommand` に記録するようにした。primary／target の完全な一操作境界と animatable flag は引き続き runtime 確認対象である。
- `SetLayerPropertyKeyframesCommand` に optional な before／after `animatable` snapshotを追加し、Property／Channel Box側の新規 keyframe commandから渡すようにした。旧JSONにfieldがない場合は従来挙動を保つ。

Timeline のローカル `KeyframePropertySnapshot` も `animatable`、Anchor、Color Label を復元対象に含め、復元時の Property dirty 通知を追加した。これにより Timeline keyframe 操作の lambda command でも、keyframe 列だけでなく表示・cache に関係する状態を同じ snapshot から戻せる。

`SetLayerPropertyKeyframesCommand` の JSON codec は、従来の互換用 `frame` に加えて RationalTime の `timeValue`／`timeScale` を保存し、読み込み時は新形式を優先するようにした。30fps固定の丸めで24／60fps等の keyframe 位置が変わる余地を減らす。

Expression action／Copilot の layer command生成も pointer ownership を確認するようにし、Effect-owned propertyへ layer commandを誤適用しないようにした。Effect-owned expression は既存の直接更新を維持し、専用 effect command は未導入である。

## Update 2026-08-30 (continued 3)

- 上記のEffect-owned expressionに関する記述は、その後のAI automation対応で更新された。AIのEffect parameter keyframe／expressionは、それぞれ `SetEffectPropertyKeyframesCommand`／`SetEffectPropertyExpressionCommand` を使う。Property WidgetのEffect-owned全編集は引き続き別経路であり、専用Undo境界のruntime／session reload確認は未実施である。

## Update 2026-08-31 (continued 4)

- `EffectModulationSnapshotCommand`／`LayerModulationSnapshotCommand` に source definition、assignment、smoothing time のJSON codecとfactoryを追加した。assignmentの64bit `targetId`は文字列で保持し、非有限値、重複ID、未解決source参照、範囲外enumを履歴保存前に拒否する。音響runtime、session reload、別composition復元は未検証である。
- Effect modulationのsnapshot setterではbefore／afterを比較し、同一値の操作をUndo履歴や変更通知へ積まないようにした。
- AIのgroup移動／ungroupでは、対象の全件preflightと適用後postconditionを追加した。Undo pushが予算で拒否された場合や親変更／group削除が成立しない場合は、成功結果を返さず、記録済み履歴または直接復元経路で部分変更を戻す。
- AIの位置・scale・rotation・opacity、note、keyframe、Project View一括rename／moveにもpush後の実状態確認を追加した。commandが予算で保持されず値が変わらない場合は成功を返さず、batch macroが部分適用された場合は履歴Undoまたは保存済み旧状態で復元する。
- 共通ProjectService／EffectServiceのlayer・effect scalar、visibility／lock／solo／shy／parent、audio trim／de-click、effect keyframe／expression、modulationにも同じpush後検証を適用した。失敗時は履歴Undoしてから変更通知を出し、作成・複製・フォルダ生成でUndo予算により実体だけが残るケースも復元する。runtime、selection、session reloadは未検証である。

## Update 2026-08-31 (continued 5)

- session historyの保存では、永続化対象として宣言されたcommandの空payloadやtype欠落を成功扱いでスキップしないようにした。読み込みも不正・未知・復元不能なentryを黙って捨てず、履歴全体を失敗として扱う。
- session historyの`estimatedBytes`も復元後commandの実測値と照合し、metadataだけ改変されたentryを受け入れないようにした。旧形式でfieldがないentryは従来どおり許容する。
- `OffloadedUndoCommand` の復元では、保存されたtype／label／estimatedBytesをwrapper自身と照合し、factoryが別typeのcommandを返した場合も拒否する。破損・差し替えられた退避ファイルによる誤Undo／Redoを避けるための境界検証である。実際のファイル破損・runtime復元は未検証である。
- `OffloadedUndoCommand` が履歴から削除・置換・破棄される際は、自身の退避JSONをwrapperのデストラクタで削除するようにした。予算エビクションやsession load、アプリ終了後に不要な`undo_*.json`が残る可能性を抑える。ファイル権限エラー時のcleanup結果は未検証である。
- 履歴envelopeのversion／estimatedBytes／state versionは有限・非負の整数として検証し、小数や文字列の暗黙変換を拒否する。旧形式の`currentVersion`欠落だけは従来どおり`savedVersion`へfallbackする。
- keyframe／mask／matte／text animator／effect mask のsession codecでは、必須配列・オブジェクト・数値・boolを暗黙変換せず、壊れた要素を復元成功として欠落させないようにした。maskのpath数値・enum・Bezier頂点も有限値と要素数を検証する。旧keyframeのframe-only形式は30fps fallbackとして維持する。runtimeのcodec受入は未検証である。
- Align／opacity／composition resolutionのcodecも、snapshot配列、レイヤーID、有限値、サイズ、policyを厳密に検証するようにした。欠落・小数・文字列の暗黙変換で不正な座標や解像度を履歴へ戻さない。旧Align payloadのscale欠落だけは既定値1.0を維持する。runtimeのcodec受入は未検証である。
- `UndoCommand::lastOperationSucceeded()` を任意の失敗し得る外部操作向けに追加し、`MoveAssetFileCommand` のrename失敗時はpush／undo／redoの履歴を進めないようにした。`MacroUndoCommand` とoffload wrapperも子commandの失敗を伝播し、部分適用時は既に適用した子を逆方向へ戻す。UIからのエラー表示と、ファイル以外の既存void commandの実適用結果は未検証である。
- Asset Browserのrelink／delete／import登録も同じ境界へ接続した。relinkの履歴登録失敗時は変更を戻し、deleteのbackupはredo間で再利用し、undo command破棄時に削除する。batch relinkはfile／layerの部分適用をrollbackしてから失敗を返す。runtimeの権限拒否・同名衝突・current project切替は未検証である。
- 共通の`pushUndoCommandAndVerify()`とAI automationの直接push経路も`UndoManager::push()`の戻り値を先に確認するようにした。budget拒否・初回redo失敗を、偶然成立しているpostconditionや前回のoperation outcomeで成功扱いせず、folder作成・layer／effect操作・precompose／ungroupを失敗として返す。runtime、通知順序、session reloadは未検証である。
- Property／Channel Boxのpreview変更後にkeyframe、Anchor、Color Label、Expression、Text Animator、opacityのUndo pushが拒否された場合、保持したbefore snapshotへ戻してから通知・再描画を行うようにした。Timelineのkeyframe／curve／trajectory／fringe／ripple／paste／area valueも、先行変更をkeyframe snapshotへ戻すか、command-only操作の成功結果だけをUIへ反映する。runtime、極小Undo予算、selection／cacheの受入は未検証である。
- TimelineTrackPainterViewの選択keyframe編集、範囲変換、ドラッグ移動、接線、値／Anchor／Color Label、重複整理、ソーステキストも同じrollback helperへ接続した。push拒否時はbefore snapshotとselectionを戻し、補間・roving・ripple・slideのcommand-only操作はpush結果を件数・成功表示へ伝播する。runtime、極小Undo予算、selection／cacheの受入は未検証である。
- Playbackのin/out point・marker操作とActiveContextのlayer in/out／trimも、先行変更またはcommand-only pushの失敗を成功扱いしないようにした。push拒否時は`fromJson(before)`または処理中断で実状態と通知を一致させ、Motion Sketchも位置keyframeをbefore snapshotへ戻す。runtime、極小Undo予算、playback engine同期は未検証である。
- Project Viewのcomposition resolution remap、ProjectServiceのsource relink／localize／composition effect追加もpush結果を確認し、履歴へ登録できない場合は後続の成功通知・dirty処理へ進まないようにした。runtime、極小Undo予算、composition／render cache同期は未検証である。
- Render Layer Widget v2のmask／polygon／corner radius／star inner radiusのdrag commitも、push拒否時に編集前スナップショットへ戻すようにした。runtime、複数mask構造変更、render cache同期は未検証である。
- Inspectorのmatte変更、mask preset／一括mask操作、effect mask画像、Surface FX element操作も、Undo push拒否を成功扱いしない境界へ揃えた。mask presetは既存の`MaskEditCommand`で置換／追加を記録し、Surface FXは変更前の選択indexを拒否時に復元する。runtime、極小Undo予算、selection／cache同期は未検証である。
- Timeline左ペインのvariant、matte、visibility／lock／solo／shy、mask削除、layer移動と複数選択macroも、push結果に応じて再描画・選択更新を進めるようにした。mask削除は先行変更後のbefore snapshotを保持し、push拒否時に全maskを復元する。runtime、selection、dirty／cache同期は未検証である。
- Layer Menuの整列・分布・spacing・衝突解消も、実値を先に適用した後のUndo push拒否で`AlignLayerSnapshot`のbefore位置／scaleへ戻すようにした。ProjectServiceのlayer作成macroもpush結果を明示的にpostconditionへ反映する。runtime、selection、dirty／cache同期は未検証である。
- Composition Render Controller／Transform Gizmoの3D・2D transform、anchor、複数選択transform、motion path key／tangent、shape path、live field、camera POI、corner radiusも、既存before snapshotへ戻してから失敗を返すようにした。push拒否時は成功通知・selection依存の再描画を先へ進めない。runtime、極小Undo予算、selection／cache同期は未検証である。
- Composition Render ControllerのRig骨／制御点／ウェイト／ポーズ、Puppet pin、line endpointとmask一括操作も、Undo push拒否時にbefore snapshotへ復元するようにした。mask色変更は`MaskEditCommand`のtransactionへ接続し、複数レイヤーmacroは全対象をrollbackする。runtime、極小Undo予算、selection／cache、Puppet deform同期は未検証である。

- mask／matte復元、Text Animator、layer／effect propertyのkeyframe・value・expressionは、適用後の要素数・値・metadata・snapshotを確認し、不一致時に直前状態へ戻して失敗を返す。Add／Remove Layerの参照解除・復元とselection復元結果も成功状態へ伝播する。Layout snapshotはrestore callback成功時だけ変更通知を発行する。setter内部の副作用、runtime、session reloadは未検証である。
- Add／Remove Layerはmatte／parent／selectionの復元・解除結果を集約し、依存関係の失敗を本体成功として通知しない。失敗時は事前に取った関係snapshotから本体・関係状態を補償する。`SetPropertyCommand`は既存editable propertyの適用後値を比較し、不一致時に旧値へ補償する。custom Effect固有setterの内部結果、runtime、session reloadは未検証である。
- `CreateVariantCommand`は抽出済み`LayerVariant`をメンバーに保持するが、既存JSONにVariant本体の完全な復元情報がないため、`canSerialize()`をfalseにしてoffload／session history対象外とした。完全なVariant snapshot APIが設計・実装されるまで、誤ったRedo再生成を避ける境界とする。
- session historyのloadでは、`savedVersion`が`currentVersion`を超える逆転したversion envelopeを拒否し、dirty判定と履歴位置の順序前提を守る。
- session history保存とdisk offloadでも、load側と同じtype／label長およびestimatedBytesのqint64範囲を事前検証し、保存後にload不能となるenvelopeを生成しにくくした。
- session historyのloadではtype／labelのJSON型と再構成後labelを照合し、estimatedBytes比較前にqint64変換範囲を検証する。
- 共通JSON整数decoderもqint64の下限以上・上限未満を検証してから変換し、version／index／estimatedBytesの範囲外値を暗黙変換しない。
- Text Gizmo内のAnimator単一値commandもsetter結果・適用後value・失敗時補償を`lastOperationSucceeded()`へ接続し、成功時だけ変更通知を発行する。
- ProjectService内のprecompose／unprecompose、layer／composition effect、group／ungroup、split commandも成功状態を返し、service bool結果・適用後存在確認をUndoManagerへ伝播する。effect remove APIは対象存在と削除後状態も確認する。
- Timelineのripple trim／delete、slide、interpolation commandもsnapshot restore・適用関数結果を成功状態へ伝播し、interpolationは全対象をpreflightしてから適用する。
- Render Controllerの3D／2D Gizmo、複数選択Gizmo、Rig、Puppet、Anchor、Shape corner radius、Motion Path、Live Field commandも、対象・型・主要getterの検証結果を`lastOperationSucceeded()`へ伝播し、成功時だけ変更通知を発行する。
- Command Paletteのマスクsnapshot commandも対象不在・復元後mask数不一致を`lastOperationSucceeded()`へ伝播する。
- `UndoManager::push()`も初回redoの成功状態がfalseの場合に`undo()`を一度実行してからcommandを破棄し、履歴へ入らない部分適用を共通境界で補償する。
- Macroは失敗した子command自身を補償してから既適用子を戻し、UndoManagerのpush／undo／redo境界はMacroへの二重補償を避けつつ非Macroの部分適用を逆操作で戻す。
- Asset dropはMoveAssetFileCommandのpush結果を確認してからモデル更新し、Audio Reactive bindingの追加／削除もUndoManagerの結果を呼び出し側へ返す。
- Audio Reactive bake／record commitもpush結果を確認し、履歴登録失敗時に成功メッセージやtrue結果へ進まない。
- Audio Mixerのadvanced routingもsnapshot push結果を確認し、失敗時はgraphを再表示するだけでcomposition changedを確定しない。
- Template importは生成layerを単一MacroUndoCommandへまとめ、複数layerの途中失敗で部分挿入を残しにくくする。
- Timeline Track Painter側のripple／slide／interpolation／roving commandも、snapshot復元・対象存在・適用件数を成功状態へ伝播する。
- Template Libraryの複数layer importも、各layerを個別pushせず単一`MacroUndoCommand`へまとめ、途中失敗時の部分挿入と追加件数の誤表示を避ける。layer順序・selection・参照復元はruntime確認対象である。
- Nested `MacroUndoCommand`は、内側Macroの失敗補償を外側Macroが二重実行しないよう、失敗操作の補償責務を明示する。nested構造のruntime確認は未実施である。
- Composition EditorのAnimation Layer操作はsnapshot commit helperへ揃え、UndoManager不在時もafter snapshotを直接適用し、push拒否時はbeforeへ戻す。Animation Layer内容・selection・cacheはruntime確認対象である。
- Composition Editorのlayer visibility／lock／solo／shy／center command呼び出しもUndoManagerをnull-safe化し、履歴基盤未初期化時のクラッシュを避ける。manager不在時は編集を適用しない。
- Paste Layers／Composition CleanupもUndoManagerをnull-safe化し、Render Widgetのドラッグ確定はmanager不在時に確定差分を直接再適用する。Pasteのselection／indexとCleanupのlock状態はruntime確認対象である。
- Asset Browserのimport／relinkとPlayback marker fallbackもpush前のUndoManager存在確認を行い、manager不在・push拒否時は既存rollbackへ入る。filesystem・marker復元はruntime確認対象である。
- Layer Menuのvisibility／lock／solo／shy／mask-to-shape command入口もnull-safe化し、履歴基盤未初期化時のクラッシュを避ける。manager不在時のfallback方針はruntime確認対象である。
- Render Widgetのparticle emitter／direction／effector／radius dragとkeyboard nudgeもnull-safe化し、Effector rollbackの開始値を正す。particle setter・cache同期はruntime確認対象である。
- Composition EditorのText／Center command入口もnull-safe化し、先行変更型のSequence／Match Durationはpush拒否時のbefore復元を維持する。timeline cache・selectionはruntime確認対象である。
- Safe Delete Layersのmacro pushもmanager存在を確認し、拒否時にselection変更や成功表示へ進まないようにする。
- Timelineのsnapshot適用をpreflight付きboolへ変更し、TimelineKeyframeSnapshotCommandにbool callbackと成功状態を追加した。既存void callbackと未接続箇所はruntime確認対象である。
- Curve Editorのtangent snapshot callbackもbool化し、Auto／Flat／Linear／Broken／Unified操作で復元失敗を履歴へ伝播する。
- PlayheadのAdd／Remove Keyframe snapshot callbackもbool化し、対象消失・property欠落を履歴失敗へ伝播する。
- Keyframe Area Valueもafter適用とsnapshot callbackをbool化し、適用不能時に成功通知へ進まない。
- Layer Menuのquick layer creation、cache policy、proxy qualityもUndoManager null-safe境界へ揃え、quick layerのdetach後失敗では既存復元を使う。

- Composition EditorのPaste Layersは、一時detachした複数レイヤーをpush拒否時に元indexへ戻すようにした。Sequence Layers／Match Layer Durationは`LayoutSnapshotCommand`へ接続し、Center Layerは`MoveLayerCommand`へ統一した。マスク単発編集の変更通知もUndo commit成功後へ移動した。Paste後のselection・親／matte参照、timeline cache、runtime、極小Undo予算は未検証である。

- Pen／Shape Pathの確定も、既存`ShapePathVertexEditCommand`へ接続し、push拒否時にcustom pathをbeforeへ戻すようにした。Pending Maskはcommit成功後にだけpending stateを消去し、矩形・閉じパス・segment挿入の通知もcommit成功後へ揃えた。path／mask cache、selection、runtime、極小Undo予算は未検証である。
- Composition EditorのAuto Stagger／Adaptive Text Fit／Quick Replace Sourcesは、command-only操作のpush拒否時に成功後の案内を出さず、失敗を表示して中断するようにした。実体の先行変更はないためrollbackは不要だが、UI結果とUndo履歴の不一致を抑える。runtime、極小Undo予算、ダイアログ連続操作は未検証である。
- Layer Menuの複数選択Bring／Send操作は、実行時点の旧indexをシミュレーションし、相対順序を維持する既存`MoveLayerIndexCommand`のmacroへまとめた。最上段・最下段の無効移動は除外し、command自身も移動後indexを検証してmacroへ失敗を伝播する。runtime、selection、部分移動失敗、保存／再読込後の順序は未検証である。
- `LayoutSnapshotCommand`はrestore callbackの結果を、`AnimationLayerStackSnapshotCommand`は復元後snapshotの一致結果を`lastOperationSucceeded()`へ返し、初回redo／macro内の復元失敗をUndoManagerへ伝播するようにした。復元途中で失敗した場合は逆snapshotを補償適用する。runtime、壊れた対象の復旧、session reloadは未検証である。
- layer／effect propertyのvalue、keyframe、expression commandも、既存setterのbool結果または対象propertyの存在を`lastOperationSucceeded()`へ伝播するようにした。対象消失や不正propertyを履歴登録成功として扱わない。keyframe要素単位の内部失敗、runtime、session reloadは未検証である。
- Text Layerの`text.value`とText Animator stack commandも、既存setterのbool結果または`ArtifactTextLayer`への型判定を`lastOperationSucceeded()`へ伝播するようにした。対象消失・対象型不一致を履歴登録成功として扱わない。Animator内部JSONの妥当性、runtime、session reloadは未検証である。
- mask、matte、de-click、effect mask、source置換、modulation、layer追加／削除、visibility／lock／solo／shy／blend、rename／parentのcommandも、既存のbool戻り値・getter・composition存在確認を`lastOperationSucceeded()`へ伝播するようにした。失敗時の変更通知を抑制し、追加／削除は操作後の存在状態を検証する。要素内容、setter内部の副作用、runtime、session reloadは未検証である。
- 整列は全対象の存在を先に検証し、各layerの位置・scale適用後にgetterで確認するようにした。途中失敗時は適用済みsnapshotを逆方向へ補償し、opacity／Variant／Project item／in-out／work areaも対象・親・存在・JSON・getter結果を`lastOperationSucceeded()`へ返す。opacityのanimation／variant／modulation評価、runtime、session reloadは未検証である。
- ChangeCompositionResolutionCommandも全snapshot layerをpreflightし、サイズ・mask数・復元対象property／keyframe数を確認する。redoのサイズ適用失敗時は旧サイズとbefore snapshotへ戻して失敗を返す。mask要素内容、keyframe値の完全比較、runtime、session reloadは未検証である。
- Source localization／shared relinkのcallbackもasset APIのbool結果を`lastOperationSucceeded()`へ返し、対象消失・asset拒否時の通知を抑制する。filesystem権限、source cache、runtime、session reloadは未検証である。
- TimelineのMotion Trajectory／Keyframe Fringe／Move／Paste snapshot callbackとTrack PainterのReverse／Set Value callbackもbool化し、snapshot target preflight失敗をUndo状態へ反映する。selection・setter完全検証・runtimeは未検証である。
- Track PainterのDelete／Reverse／Set Value／Set Anchor／Set Color Labelもbool snapshot callbackへ接続し、keyframe metadata操作のpreflight失敗をUndo状態へ反映する。metadata setter完全検証・selection・runtimeは未検証である。
- Track PainterのDuplicate／Distribute／Repeat Selected Keyframesもbool snapshot callbackへ接続し、batch操作の対象消失・preflight失敗をUndo状態へ反映する。複数対象途中失敗・selection・runtimeは未検証である。
- Track Painter context menuのSet Keyframe Area Valueもbool snapshot callbackへ接続し、area valueの対象消失・preflight失敗をUndo状態へ反映する。setter完全検証・selection・runtimeは未検証である。
- Track Painter context menuのBreak／Unify TangentsとTransform Selected Keyframesもbool snapshot callbackへ接続し、対象消失・preflight失敗をUndo状態へ反映する。複数対象途中失敗・selection・runtimeは未検証である。
- Track PainterのReverse All／Clean Keyframesなど残存するsnapshot callbackもbool `Operation`へ接続し、主要操作の対象消失・preflight失敗をUndo状態へ反映する。setter完全検証・selection・runtimeは未検証である。
- Timeline Widget／Track Painterのsnapshot command利用箇所を全てbool `Operation`へ移行し、未使用のvoid互換コンストラクタを削除した。ABI・setter内部失敗・runtimeは未検証である。
- Layer Panelのinline rename command生成をraw pointerから`std::make_unique`へ変更し、UndoManager不在時のcommandリークを防いだ。push拒否・selection・runtimeは未検証である。
- Inspector／Layer Panelのmatte reference、variant、layer move commandも`std::make_unique`へ統一し、UndoManager不在時のraw pointerリークを防いだ。push拒否・selection・runtimeは未検証である。
- Artifact source内のraw `new *Command`を再検索し、対象Inspector／Layer Panel経路に残存しないことを確認した。別形式のallocationとruntimeは未検証である。
- Layer PanelのVariant切替／作成とinline renameは、UndoManager不在時にcommandの`redo()`を直接実行するfallbackを追加した。manager不在時の操作消失を抑えたが、dirty・selection・runtimeは未検証である。
- Layer Panelのblend mode、visibility／lock／solo／shyの単一・複数選択経路も、UndoManager不在時にcommand／macroの`redo()`を直接実行するfallbackへ揃えた。dirty・selection・runtimeは未検証である。
- Layer Panelのmatte type／enabled／opacity／blend／fit helperも、UndoManager不在時に`ChangeLayerMatteReferencesCommand::redo()`を実行して成功状態を返すfallbackへ揃えた。dirty・selection・runtimeは未検証である。
- `MacroUndoCommand`は子commandが空の場合にredo／undo／serializeを失敗扱いとし、状態を変更しない空履歴の登録を防いだ。既存session復旧とruntimeは未検証である。
- `UndoManager::push()`／`createCommand()`は、単一commandまたは全体memory budgetで保持不能なcommandを初回redo／session復元前に拒否するようにした。budget縮小・offload・runtimeは未検証である。
- `saveSessionHistory()`は非シリアライズ commandを黙って省略せず、履歴全体を保存できない場合に失敗するようにした。部分保存とversion不一致を防ぐが、既存保存UX・runtimeは未検証である。
- `UndoManager::Impl::stackBytes()`は推定サイズを飽和加算し、`size_t` wrapによるmemory budget超過の見逃しを防ぐようにした。異常値・runtimeは未検証である。
- offload file名とcleanup globにmanager単位のUUIDを付け、共有directory上の別managerの`undo_*.json`を削除しないようにした。既存孤児ファイルと複数プロセスのruntimeは未検証である。
- state ID割り当ては既存undo／redo IDとsaved/current versionを避けるようにし、`INT64_MAX`後のwrapによるversion衝突を抑えた。ID空間枯渇とruntimeは未検証である。
- Undo／Redo両stackをentry／memory budgetの対象に統合し、`currentMemoryBytes()`とmemory pressureも全履歴を数えるようにした。budget変更・淘汰順・runtimeは未検証である。
