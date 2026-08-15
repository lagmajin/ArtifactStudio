# MILESTONE: Configuration Layering System

**最終更新:** 2026-08-05
**最終監査:** 2026-08-15
**ステータス:** 基盤実装済み、QSettings 移行と hot reload は未完了
**日付**: 2026-08-04
**現状**: `FastSettingsStore`（CBOR, 単一フラットファイル）と `QSettings` が混在。階層化（システム→ユーザー→プロジェクト）不在。スキーマ検証は setter 毎の場当たり的クランプのみ。設定ホットリロード不在。 
**目標**: 3層設定システム、設定スキーマ宣言、ホットリロード、QSettings の FastSettingsStore への統一。

### 実装進捗（2026-08-05）

- `ConfigLayer` と `LayeredConfigStore` を追加し、System/User/Project/Session の優先解決・保存・再読込・import/export を実装。
- `ArtifactAppSettings` の97キーをスキーマ登録し、型・既定値・範囲・許可値の検証を接続。
- Project 作成／オープン経路で `.artifact/settings.cbor` を Project レイヤーとしてロード。
- 設定ダイアログに検索、Project override 件数表示、override の太字表示、全解除を追加。
- API key と Asset Integration 設定を User レイヤーへ移行。
- 未完了: Artifact 側に残る個別 `QSettings` の移行、行単位の override 解除、実ビルド・実行検証。

## 2026-08-15 現行コード照合

`ConfigLayer`／`LayeredConfigStore`、Project の `.artifact/settings.cbor` load／unload、`ArtifactAppSettings` のスキーマ接続、Asset／AI 系の LayeredConfigStore 利用、設定画面の検索・Project override 表示を確認した。System／User／Project／Session の解決基盤は、当初の「階層化不在」より進んでいる。

一方、`AppMain.cppm` には legacy layout 移行用の `QSettings` が残り、コードベース全体の QSettings ゼロ化は未達。ファイル変更の hot reload と valueChanged／layerChanged の全 UI 反映、行単位 override 解除、設定スキーマの全キー網羅、プロジェクト切替後の値分離は static evidence だけでは完了保証できない。今回はビルド・テストを実行していない。

## 現状の問題

| 問題 | 影響 |
|------|------|
| 単一フラット設定 | プロジェクト固有設定がアプリ全体に漏れる。プロジェクトを切り替えても前の設定が残る |
| QSettings 混在 | `LayoutState`, `APIKeyManager` 等が別ストア。設定の保存場所が統一されていない |
| スキーマ不在 | 設定名のタイプミスが実行時まで検出されない。バリデーションが分散 |
| ホットリロード不在 | 設定ファイルを手動編集しても反映されない。外部ツールとの連携が困難 |
| 設定エクスポート不在 | 設定の共有・バックアップが手動コピーのみ |

---

## Phase 1: 3層設定アーキテクチャ

### 1.1 レイヤー定義

```cpp
// ArtifactCore/include/Configuration/ConfigLayer.ixx
enum class ConfigLayer : int {
    System   = 0,  // アプリケーション出荷時デフォルト（読み取り専用）
    User     = 1,  // ユーザー設定（~/.artifactstudio/settings.cbor）
    Project  = 2,  // プロジェクト固有上書き（{projectDir}/.artifact/settings.cbor）
    Session  = 3   // 現在のセッション限定（メモリのみ、保存されない）
};

// 優先度: System < User < Project < Session（数値が大きいほど優先）
```

### 1.2 LayeredConfigStore

```cpp
// ArtifactCore/include/Configuration/LayeredConfigStore.ixx
class LayeredConfigStore {
public:
    static LayeredConfigStore& instance();
    
    // レイヤー管理
    void loadLayer(ConfigLayer layer, const QString& path);
    void saveLayer(ConfigLayer layer);
    void unloadLayer(ConfigLayer layer);     // プロジェクトクローズ時
    
    // 値の読み取り（全レイヤーを優先度順に検索）
    QVariant value(std::string_view key) const;
    QVariant value(std::string_view key, const QVariant& defaultValue) const;
    
    // 型付きアクセサ
    bool valueBool(std::string_view key, bool defaultValue = false) const;
    int64_t valueInt64(std::string_view key, int64_t defaultValue = 0) const;
    double valueDouble(std::string_view key, double defaultValue = 0.0) const;
    QString valueString(std::string_view key, const QString& defaultValue = {}) const;
    
    // 値の書き込み（常に最上位の書き込み可能レイヤーに）
    void setValue(std::string_view key, const QVariant& value);
    
    // 特定レイヤーへの書き込み
    void setValue(ConfigLayer layer, std::string_view key, const QVariant& value);
    
    // キーがどのレイヤーで定義されているか
    ConfigLayer sourceLayer(std::string_view key) const;
    
    // 全キーの列挙（重複除去、最優先値）
    QStringList allKeys() const;
    
    // ホットリロード
    void watchForChanges(bool enable = true);
    
    // 変更シグナル
    W_SIGNAL(valueChanged, std::string_view key, QVariant newValue)
    W_SIGNAL(layerChanged, ConfigLayer layer)
    
    // インポート/エクスポート
    bool exportLayer(ConfigLayer layer, const QString& path);
    bool importLayer(const QString& path, ConfigLayer targetLayer);

private:
    struct LayerData {
        ConfigLayer layer;
        QString filePath;
        std::unique_ptr<FastSettingsStore> store;  // 既存の CBOR ストア再利用
        bool writable;
        bool dirty;
        
        std::optional<QVariant> get(std::string_view key) const;
        void set(std::string_view key, const QVariant& value);
    };
    
    std::vector<LayerData> layers_;  // System=0, User=1, Project=2, Session=3
    QFileSystemWatcher* watcher_ = nullptr;
};
```

### 1.3 使用例

```cpp
// アプリ起動時
auto& config = LayeredConfigStore::instance();

// System レイヤー（読み取り専用、バイナリ埋め込み）
config.loadLayer(ConfigLayer::System, ":/defaults/system_settings.cbor");

// User レイヤー
config.loadLayer(ConfigLayer::User, QStandardPaths::writableLocation(
    QStandardPaths::AppDataLocation) + "/settings.cbor");

// 値の読み取り
int undoLevels = config.valueInt64("undo.maxLevels", 100);
QString theme = config.valueString("ui.theme", "dark");

// 値の書き込み（User レイヤーに）
config.setValue("ui.theme", "light");

// プロジェクトを開いた時
config.loadLayer(ConfigLayer::Project, projectDir + "/.artifact/settings.cbor");

// プロジェクト固有設定
config.setValue("timeline.frameRate", 30.0);

// プロジェクトクローズ時
config.saveLayer(ConfigLayer::Project);
config.unloadLayer(ConfigLayer::Project);
```

### 1.4 完了条件

- [ ] System < User < Project < Session の優先順位で値が解決される
- [ ] プロジェクトオープン時に Project レイヤーがロードされ、クローズ時にアンロードされる
- [ ] `sourceLayer()` が各キーの出所を正しく報告する
- [ ] 既存の `FastSettingsStore` をバックエンドとして再利用（移行コスト最小）

---

## Phase 2: 設定スキーマ

### 2.1 スキーマ定義

```cpp
// ArtifactCore/include/Configuration/ConfigSchema.ixx
struct ConfigProperty {
    std::string key;            // "undo.maxLevels"
    std::string description;    // "Maximum number of undo levels"
    QVariant::Type type;        // Int, Double, Bool, String
    QVariant defaultValue;
    std::optional<QVariant> minValue;
    std::optional<QVariant> maxValue;
    std::vector<QVariant> allowedValues;  // 空=任意値許可
    bool requiresRestart = false;         // 変更時に再起動必要
    bool projectOverrideable = true;      // プロジェクトレイヤーでの上書き許可
};

class ConfigSchema {
public:
    static ConfigSchema& instance();
    
    // スキーマ登録（起動時に全設定を登録）
    void registerProperty(const ConfigProperty& prop);
    
    // バリデーション
    struct ValidationResult {
        bool valid;
        std::string errorMessage;
    };
    ValidationResult validate(std::string_view key, const QVariant& value) const;
    
    // メタデータ
    const ConfigProperty* find(std::string_view key) const;
    std::vector<const ConfigProperty*> allProperties() const;
    
    // スキーマからのデフォルト値生成
    void applyDefaultsToLayer(ConfigLayer layer);

private:
    std::map<std::string, ConfigProperty, std::less<>> properties_;
};
```

### 2.2 登録例

```cpp
// 起動時のスキーマ登録
void registerAllConfigProperties() {
    auto& schema = ConfigSchema::instance();
    
    // Undo
    schema.registerProperty({
        "undo.maxLevels", "Maximum undo history entries",
        QVariant::Int, 100, 10, 1000, {}, false, true
    });
    schema.registerProperty({
        "undo.memoryBudgetMB", "Maximum undo memory (MB)",
        QVariant::Int, 256, 64, 4096, {}, false, false
    });
    
    // Timeline
    schema.registerProperty({
        "timeline.autoKeyEnabled", "Auto-keyframe mode",
        QVariant::Bool, false, {}, {}, {}, false, true
    });
    schema.registerProperty({
        "timeline.autoKeyScope", "Auto-key scope",
        QVariant::String, "Global", {}, {},
        {"Global", "Selected Layers", "Current Layer"}, false, true
    });
    schema.registerProperty({
        "timeline.ghostingFrameCount", "Ghosting frame count (0=off)",
        QVariant::Int, 0, 0, 20, {}, false, true
    });
    
    // UI
    schema.registerProperty({
        "ui.theme", "UI theme name",
        QVariant::String, "dark", {}, {},
        {"dark", "light", "high-contrast"}, true, false  // 要再起動
    });
    schema.registerProperty({
        "ui.fontScalePercent", "UI font scale (%)",
        QVariant::Int, 100, 50, 200, {}, true, false
    });
    
    // Preview
    schema.registerProperty({
        "preview.quality", "Preview quality",
        QVariant::String, "Draft", {}, {},
        {"Draft", "Preview", "Final"}, false, true
    });
    schema.registerProperty({
        "preview.resolutionPercent", "Preview resolution (%)",
        QVariant::Int, 100, 25, 100, {}, false, true
    });
    
    // 全 Schema → System レイヤーにデフォルト値適用
    schema.applyDefaultsToLayer(ConfigLayer::System);
}
```

### 2.3 バリデーション統合

```cpp
void LayeredConfigStore::setValue(std::string_view key, const QVariant& value) {
    // スキーマバリデーション
    auto result = ConfigSchema::instance().validate(key, value);
    if (!result.valid) {
        qWarning() << "Config validation failed for" << key.c_str()
                   << ":" << result.errorMessage.c_str();
        return;  // 無効な値を拒否
    }
    
    // 最上位の書き込み可能レイヤーに書き込み
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        if (it->writable) {
            it->set(key, value);
            it->dirty = true;
            break;
        }
    }
}
```

### 2.4 完了条件

- [ ] 既存の `ArtifactAppSettings` の全キー（~90個）がスキーマに登録される
- [ ] 無効な値（範囲外、許可リスト外）が拒否され、警告ログが出る
- [ ] `registerAllConfigProperties()` がアプリ起動時に呼ばれる
- [ ] 設定値の型不一致がスキーマで検出される

---

## Phase 3: ホットリロード + QSettings 統一

### 3.1 ホットリロード

```cpp
void LayeredConfigStore::watchForChanges(bool enable) {
    if (enable && !watcher_) {
        watcher_ = new QFileSystemWatcher(this);
        
        // 全レイヤーのファイルを監視
        for (const auto& layer : layers_) {
            if (!layer.filePath.isEmpty() && layer.writable) {
                watcher_->addPath(layer.filePath);
            }
        }
        
        QObject::connect(watcher_, &QFileSystemWatcher::fileChanged,
            [this](const QString& path) {
                // 変更があったレイヤーを再読み込み
                for (auto& layer : layers_) {
                    if (layer.filePath == path) {
                        layer.store->reload();
                        layerChanged(layer.layer);
                    }
                }
                // 監視を再登録（一部のエディタはファイルを削除→再作成する）
                watcher_->addPath(path);
            });
    }
}
```

### 3.2 QSettings からの移行

現在 QSettings を使っている箇所を `LayeredConfigStore` に移行:

| 現状 | 移行先 |
|------|--------|
| `LayoutState` → `QSettings` | `LayeredConfigStore::setValue("layout.windowState", ...)` |
| `APIKeyManager` → `QSettings` | `LayeredConfigStore::setValue("api.openrouter.key", ...)` |
| `ArtifactAssetIntegrationSettings` → `QSettings` | `LayeredConfigStore::setValue("asset.openassetio.enabled", ...)` |
| `EnvironmentVariable` → `QSettings` | `LayeredConfigStore::setValue("env.overrides", ...)` |

移行後、`QSettings` の使用箇所をゼロにする。

### 3.3 完了条件

- [ ] User レイヤーの設定ファイルを手動編集 → アプリに自動反映
- [ ] ホットリロードで menu/timeline の挙動に即座に反映される設定と、再起動が必要な設定が区別される
- [ ] コードベースから `QSettings` の直接使用がなくなる

---

## Phase 4: 設定 UI 改善

### 4.1 設定検索

`ApplicationSettingDialog` に設定検索を追加:

```cpp
// 設定ダイアログに検索バーを追加
QLineEdit* searchBox;
connect(searchBox, &QLineEdit::textChanged, [this](const QString& query) {
    for (auto& page : settingPages_) {
        for (auto& row : page.rows) {
            bool matches = row.label.contains(query, Qt::CaseInsensitive) ||
                          row.description.contains(query, Qt::CaseInsensitive) ||
                          row.key.contains(query, Qt::CaseInsensitive);
            row.widget->setVisible(matches);
        }
    }
});
```

### 4.2 プロジェクト設定の視覚的区別

プロジェクトレイヤーで上書きされた設定を視覚的に区別:

```cpp
// プロジェクト上書き設定は青い左ボーダーで表示
if (config.sourceLayer(row.key) == ConfigLayer::Project) {
    row.setStyle(ProjectOverrideStyle);
}

// 上書きを解除するボタン（プロジェクト値を削除し、User/System 値に戻す）
QPushButton* resetButton = new QPushButton("↩");
connect(resetButton, &QPushButton::clicked, [key = row.key]() {
    LayeredConfigStore::instance().setValue(ConfigLayer::Project, key, QVariant());
    // QVariant() (null) を書き込むと、そのレイヤーのエントリが削除される
});
```

### 4.3 完了条件

- [ ] 設定検索が全ページの設定をフィルタできる
- [ ] プロジェクト上書き設定が視覚的に区別される
- [ ] 上書き解除ボタンが動作する

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `ArtifactCore/include/Configuration/LayeredConfigStore.ixx` | 新規: 3層設定ストア |
| P1 | `ArtifactCore/src/Configuration/LayeredConfigStore.cppm` | 新規: レイヤー解決・永続化 |
| P1 | `ArtifactCore/include/Configuration/ConfigLayer.ixx` | 新規: ConfigLayer enum |
| P2 | `ArtifactCore/include/Configuration/ConfigSchema.ixx` | 新規: スキーマ定義・バリデーション |
| P2 | `ArtifactCore/src/Configuration/ConfigSchema.cppm` | 新規: スキーマ登録・バリデーション実装 |
| P2 | `ArtifactCore/src/Application/ArtifactAppSettings.cppm` | スキーマ登録呼出追加 |
| P3 | `ArtifactCore/src/Configuration/LayeredConfigStore.cppm` | QFileSystemWatcher 追加 |
| P3 | `Artifact/src/Widgets/Layout/LayoutState.cppm` | QSettings → LayeredConfigStore |
| P3 | `Artifact/src/Service/APIKeyManager.cppm` | QSettings → LayeredConfigStore |
| P4 | `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm` | 検索バー、override 表示 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: 3層アーキテクチャ | **P0** | 中 | 設定の全基盤。`FastSettingsStore` をバックエンドに再利用 |
| P2: スキーマ | **P1** | 中 | 既存 ~90 キーのスキーマ登録が主作業 |
| P3: ホットリロード+統一 | **P1** | 小 | QFileSystemWatcher 追加 + QSettings 移行（置換のみ） |
| P4: UI | **P2** | 小 | 検索と override 表示 |
