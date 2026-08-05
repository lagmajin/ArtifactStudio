# MILESTONE: General-Purpose Database Module (SQLite)

**日付**: 2026-08-04
**最終更新:** 2026-08-05
**実装状況:** ArtifactCore に SQLite Database 基盤、PreparedQuery、トランザクション、WAL/foreign key 初期化、MigrationRunner、VACUUM backup を追加。SessionLedger・AssetDatabase 等の既存消費者移行と実ビルド検証は未実施。
**現状**: データベース抽象化レイヤー不在。`AssetDatabase` は JSON ファイル、`FastSettingsStore` は CBOR、`SessionLedger` はメモリのみ（終了時に消失）。SQL 系の依存ゼロ。
**目標**: Qt 内蔵 `QSqlDatabase` + SQLite ドライバを使った軽量永続化モジュール。マイグレーション、プリペアドクエリ、非同期クエリ。SessionLedger → AssetDatabase → RenderQueue → Tag システムの順に移行。

## 現状の永続化機構

| 機構 | バックエンド | クエリ可能？ | トランザクション | 問題点 |
|------|------------|-----------|--------------|--------|
| `AssetDatabase` | JSON (QJsonDocument) | UUID/パス/型（線形スキャン） | QSaveFile アトミック書込 | タグ検索不可、リレーショナル不在 |
| `FastSettingsStore` | CBOR (QCborMap) | 文字列キーのみ | バッチ書込＋アトミック保存 | 設定用途には十分 |
| `SessionLedger` | **メモリのみ** | 追記専用スキャン | なし | **終了時に全データ消失** |
| `CrashHandler` | .txt + .dmp ファイル | N/A | アトミックフラグ | ファイル管理のみ |
| `DataTable` / `CsvParser` | CSV/JSON 取り込み | インメモリ | なし | 外部データ取り込み用、永続化ではない |

## ターゲット消費者（全 Phase 完了時）

| 消費者 | データ形状 | アクセスパターン |
|--------|-----------|---------------|
| **SessionLedger** | プロジェクト開閉、レンダーイベント、クラッシュ、リカバリポイント | 追記多数、セッション復元時に検索 |
| **Asset Tag System** | ユーザー定義タグ、レーティング、ラベル | タグ検索（逆インデックス）、CRUD |
| **Render Queue** | ジョブ定義、パラメータ、ステータス、出力パス | CRUD、ステータス遷移、日付/プロジェクトフィルタ |
| **Edit History** | Undoスナップショット、コマンド履歴 | 追記多数、undo/redo で順次読出 |
| **Asset Catalog** | アセットメタデータ、依存グラフ | 読出多数、複数属性での検索/フィルタ |
| **Crash/Telemetry** | クラッシュレポート、セッションログ、性能トレース | 追記のみ、分析用に読出 |

**移行しないもの**: `FastSettingsStore` — CBOR はキー・バリュー設定用途に十分。`ProjectMetadata` — JSON はプロジェクトファイルの可搬性に必要。

---

## Phase 1: Database モジュール基盤

### 1.1 モジュール配置

```
ArtifactCore/
├── include/Database/
│   ├── Database.ixx           # モジュール本体: Database, MigrationRunner, PreparedQuery
│   └── DatabaseTypes.ixx      # Row, ResultSet, DatabaseError 型定義
└── src/Database/
    ├── Database.cppm           # QSqlDatabase ラッパー実装
    ├── MigrationRunner.cppm    # マイグレーション実行エンジン
    └── PreparedQuery.cppm      # プリペアドクエリ実装
```

### 1.2 Database クラス

Qt 内蔵の `QSqlDatabase` + SQLite ドライバで十分。vcpkg 依存不要。

```cpp
// Database.ixx
class ArtifactDatabase {
public:
    struct Config {
        QString filePath;           // ".db" ファイルパス
        QString connectionName;     // QSqlDatabase 接続名（複数 DB 同時使用時）
        int busyTimeoutMs = 5000;   // SQLite busy timeout
        bool walMode = true;        // WAL モード（並行読出性能向上）
        bool foreignKeys = true;    // 外部キー制約
    };

    // ライフサイクル
    static std::unique_ptr<ArtifactDatabase> open(const Config& config);
    void close();
    bool isOpen() const;
    const Config& config() const;

    // トランザクション
    void beginTransaction();
    void commit();
    void rollback();

    // クエリ
    PreparedQuery prepare(const QString& sql);
    void execute(const QString& sql);  // DDL, INSERT/UPDATE/DELETE

    // メタデータ
    QStringList tables() const;
    bool tableExists(const QString& name) const;
    QSqlDatabase& raw();  // QSqlDatabase への直接アクセス（緊急時のみ）

    // バックアップ
    bool backupTo(const QString& path);

private:
    Config config_;
    QSqlDatabase db_;
};
```

### 1.3 PreparedQuery

バインディングと行反復を型安全にする軽量ラッパー:

```cpp
class PreparedQuery {
public:
    // パラメータバインディング
    template<typename T>
    PreparedQuery& bind(const T& value);   // QString, int, double, QByteArray, QUuid...

    PreparedQuery& bindNull();
    
    // 実行
    bool exec();  // SELECT 系
    bool execInsert();  // INSERT → lastInsertId 取得
    
    // 行反復
    bool next();          // 次の行に移動（なければ false）
    QVariant value(int columnIndex) const;
    QVariant value(const QString& columnName) const;
    
    // 型付き取得
    template<typename T> T get(int columnIndex) const;
    template<typename T> T get(const QString& columnName) const;
    
    // ユーティリティ
    int columnCount() const;
    QStringList columnNames() const;
    int64_t lastInsertId() const;
    int numRowsAffected() const;

private:
    QSqlQuery query_;
};
```

使用例:
```cpp
auto& db = ArtifactDatabase::open({ .filePath = "session.db" });

// 基本クエリ
auto query = db.prepare("SELECT name, frame_count FROM compositions WHERE project_id = ?");
query.bind(projectUuid.toString());
while (query.next()) {
    QString name = query.get<QString>("name");
    int frames = query.get<int>("frame_count");
}

// INSERT
auto insert = db.prepare(
    "INSERT INTO session_events (timestamp, event_type, project_id, details) VALUES (?, ?, ?, ?)"
);
insert.bind(QDateTime::currentMSecsSinceEpoch());
insert.bind("ProjectOpened");
insert.bind(projectId);
insert.bind(details.toJson());
insert.execInsert();
```

### 1.4 MigrationRunner

スキーマバージョン管理:

```cpp
class MigrationRunner {
public:
    struct Migration {
        int version;           // スキーマバージョン番号（1, 2, 3...）
        QString description;   // 人が読める説明
        QString sql;           // 実行する SQL（複数文可）
    };

    // 全マイグレーションを登録し、未実行のものをバージョン順に実行
    static bool runAll(ArtifactDatabase& db, const std::vector<Migration>& migrations);
    
    // 現在のスキーマバージョンを取得
    static int currentVersion(ArtifactDatabase& db);
    
    // ロールバック（最後のマイグレーションを元に戻す）
    static bool rollbackLast(ArtifactDatabase& db);

private:
    static void ensureVersionTable(ArtifactDatabase& db);
};
```

マイグレーション定義例:
```cpp
std::vector<MigrationRunner::Migration> sessionMigrations = {
    {
        1, "Create session_events table",
        R"(
            CREATE TABLE session_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp_ms INTEGER NOT NULL,
                event_type TEXT NOT NULL,
                project_id TEXT,
                session_id TEXT NOT NULL,
                details_json TEXT,
                frame_index INTEGER,
                duration_us INTEGER
            );
            CREATE INDEX idx_session_events_type ON session_events(event_type);
            CREATE INDEX idx_session_events_project ON session_events(project_id);
            CREATE INDEX idx_session_events_timestamp ON session_events(timestamp_ms);
        )"
    },
    {
        2, "Add recovery_points table",
        R"(
            CREATE TABLE recovery_points (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp_ms INTEGER NOT NULL,
                project_id TEXT NOT NULL,
                label TEXT,
                snapshot_json TEXT NOT NULL,
                created_by TEXT DEFAULT 'auto'
            );
            CREATE INDEX idx_recovery_project ON recovery_points(project_id);
        )"
    }
};
```

### 1.5 CMakeLists.txt 統合

Qt6::Sql は既存のリンクに追加するだけ（SQLite ドライバは Qt ビルドに同梱）:

```cmake
# ArtifactCore/CMakeLists.txt
target_link_libraries(ArtifactCore PRIVATE Qt6::Sql)
```

新規モジュール追加:
```cmake
# .ixx は GLOB_RECURSE で自動発見される
# .cppm は content scan または force list に追加
```

### 1.6 完了条件

- [ ] `ArtifactDatabase::open()` で `.db` ファイルが作成される
- [ ] `PreparedQuery` の bind/exec/next/get が全基本型で動作
- [ ] `MigrationRunner` が未実行マイグレーションを順次適用
- [ ] WAL モード + busy timeout + 外部キーが正しく設定される
- [ ] `beginTransaction/commit/rollback` が動作
- [ ] バックアップ（`.backupTo()`）が動作

---

## Phase 2: SessionLedger SQLite 移行

### 2.1 現状の問題

`SessionLedger` はアプリ終了時に全データ消失:

```cpp
// 現状 (SessionLedger.ixx)
std::vector<SessionLedgerEntry> entries_;       // メモリのみ
std::vector<RecoveryPoint>    recoveryPoints_;  // メモリのみ
```

クラッシュ時の障害解析、セッション比較、長期トレンド分析が不可能。

### 2.2 SQLite 化

既存の `SessionLedger` API を維持しつつ、内部実装を SQLite に差し替える:

```cpp
class SessionLedger {
public:
    // 既存 API（変更なし）
    static SessionLedger* instance();
    void open(const QString& dbPath);
    void close();
    
    void recordProjectOpened(const QString& projectId);
    void recordProjectClosed(const QString& projectId);
    void recordProjectSaved(const QString& projectId);
    void recordRenderStarted(const QString& projectId);
    void recordRenderCompleted(const QString& projectId, int64_t durationUs);
    void recordRenderFailed(const QString& projectId, const QString& error);
    void recordCrash(const CrashRecord& crash);
    void recordSettingsChanged(const QString& key, const QVariant& oldVal, const QVariant& newVal);
    
    void addRecoveryPoint(const QString& projectId, const QString& label, const QJsonObject& snapshot);
    std::vector<RecoveryPoint> recoveryPoints(const QString& projectId) const;
    
    // 新規: クエリ API
    std::vector<SessionLedgerEntry> entriesForSession(const QString& sessionId) const;
    std::vector<SessionLedgerEntry> entriesForProject(const QString& projectId, int limit = 100) const;
    std::vector<SessionLedgerEntry> errorsSince(const QDateTime& since) const;
    int crashCount(const QString& projectId) const;
    
    // 新規: クリーンアップ
    void purgeOlderThan(const QDateTime& olderThan);

private:
    std::unique_ptr<ArtifactDatabase> db_;
    QString currentSessionId_;  // アプリ起動ごとに新規
};
```

### 2.3 スキーマ

```sql
CREATE TABLE session_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_ms INTEGER NOT NULL,
    session_id TEXT NOT NULL,
    event_type TEXT NOT NULL,      -- 'ProjectOpened','ProjectClosed','RenderStarted','RenderCompleted','RenderFailed','Crash','SettingsChanged'
    project_id TEXT,
    details_json TEXT,             -- イベント固有データ
    frame_index INTEGER,
    duration_us INTEGER,
    error_message TEXT
);

CREATE TABLE recovery_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_ms INTEGER NOT NULL,
    project_id TEXT NOT NULL,
    label TEXT,
    snapshot_json TEXT NOT NULL,
    created_by TEXT DEFAULT 'auto'
);

CREATE INDEX idx_sess_events_session ON session_events(session_id);
CREATE INDEX idx_sess_events_project ON session_events(project_id);
CREATE INDEX idx_sess_events_timestamp ON session_events(timestamp_ms);
CREATE INDEX idx_sess_events_type ON session_events(event_type);
CREATE INDEX idx_recov_project ON recovery_points(project_id);
```

### 2.4 完了条件

- [ ] `SessionLedger::instance()` が初回呼出時に `.db` を開く
- [ ] 既存の全 `record*()` メソッドが SQLite に永続化される
- [ ] アプリ再起動後、前回セッションのイベントが読める
- [ ] クラッシュイベントがアプリ再起動後も残っている
- [ ] `entriesForProject()` でプロジェクト別イベント履歴が取得できる
- [ ] マイグレーションで既存 `.db` が壊れずにアップグレードされる

---

## Phase 3: Asset Tag + メタデータ検索

### 3.1 タグシステム

`ArtifactAssetDatabase` を JSON → SQLite 移行せず、**タグ用に SQLite テーブルを追加**するハイブリッド方式:

```cpp
class AssetTagStore {
public:
    static AssetTagStore* instance();
    void open(ArtifactDatabase* db);  // SessionLedger と同じ .db を共有

    // タグ CRUD
    QStringList allTags() const;
    QStringList tagsForAsset(const QUuid& assetId) const;
    void addTag(const QUuid& assetId, const QString& tag);
    void removeTag(const QUuid& assetId, const QString& tag);
    void setTags(const QUuid& assetId, const QStringList& tags);
    
    // タグ検索
    QList<QUuid> assetsWithTag(const QString& tag) const;
    QList<QUuid> assetsWithAnyTag(const QStringList& tags) const;
    QList<QUuid> assetsWithAllTags(const QStringList& tags) const;
    
    // レーティング
    int rating(const QUuid& assetId) const;
    void setRating(const QUuid& assetId, int rating);  // 0-5
    
    // お気に入り
    bool isFavorite(const QUuid& assetId) const;
    void setFavorite(const QUuid& assetId, bool favorite);

    // AssetBrowser へのシグナル
    W_SIGNAL(tagsChanged, const QUuid& assetId)
    W_SIGNAL(ratingChanged, const QUuid& assetId, int rating)
};
```

### 3.2 スキーマ

```sql
CREATE TABLE asset_tags (
    asset_id TEXT NOT NULL,
    tag TEXT NOT NULL COLLATE NOCASE,
    PRIMARY KEY (asset_id, tag)
);

CREATE INDEX idx_asset_tags_tag ON asset_tags(tag);
CREATE INDEX idx_asset_tags_asset ON asset_tags(asset_id);

CREATE TABLE asset_metadata (
    asset_id TEXT PRIMARY KEY,
    rating INTEGER DEFAULT 0 CHECK(rating BETWEEN 0 AND 5),
    favorite INTEGER DEFAULT 0,
    import_count INTEGER DEFAULT 1,
    last_used_ms INTEGER,
    use_count INTEGER DEFAULT 0,
    custom_json TEXT
);
```

### 3.3 AssetBrowser 統合

1. タグ入力欄を AssetBrowser の情報パネルに追加
2. タグによるフィルタリング（既存の type filter に tag filter 追加）
3. タグサジェスト（入力中に既存タグのオートコンプリート）
4. レーティング表示（★1-5）
5. お気に入り（⭐アイコン）→ 既存の Favorites 機能と統合

### 3.4 完了条件

- [ ] タグの追加・削除・検索が動作
- [ ] AssetBrowser でタグフィルタが使える
- [ ] お気に入りがレーティングと共に永続化される
- [ ] アプリ再起動後もタグ・レーティングが保持される

---

## Phase 4: Render Queue SQLite 移行 + 非同期クエリ

### 4.1 Render Queue の SQLite 化

現在の `ArtifactRenderQueueService` のジョブ管理を SQLite に移行:

```sql
CREATE TABLE render_jobs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    job_id TEXT UNIQUE NOT NULL,
    project_id TEXT NOT NULL,
    composition_name TEXT NOT NULL,
    output_path TEXT NOT NULL,
    format TEXT NOT NULL,
    resolution_width INTEGER,
    resolution_height INTEGER,
    frame_start INTEGER,
    frame_end INTEGER,
    frame_step INTEGER DEFAULT 1,
    status TEXT NOT NULL DEFAULT 'queued',  -- queued, rendering, completed, failed, cancelled
    progress REAL DEFAULT 0.0,             -- 0.0 - 1.0
    error_message TEXT,
    created_at_ms INTEGER NOT NULL,
    started_at_ms INTEGER,
    completed_at_ms INTEGER,
    duration_us INTEGER,
    output_size_bytes INTEGER,
    render_settings_json TEXT
);

CREATE INDEX idx_render_jobs_status ON render_jobs(status);
CREATE INDEX idx_render_jobs_project ON render_jobs(project_id);
CREATE INDEX idx_render_jobs_created ON render_jobs(created_at_ms);
```

### 4.2 非同期クエリ

`BackgroundTask` 経由でメインスレッドをブロックしないクエリ:

```cpp
class AsyncDatabaseQuery {
public:
    template<typename T>
    static void execute(ArtifactDatabase& db,
                         const QString& sql,
                         std::function<T(PreparedQuery&)> resultMapper,
                         std::function<void(T)> onResult,
                         std::function<void(const QString&)> onError = nullptr)
    {
        auto* task = new BackgroundTypedTask<T>(
            [&db, sql, mapper = std::move(resultMapper)]() -> T {
                auto query = db.prepare(sql);
                query.exec();
                return mapper(query);
            }
        );
        QObject::connect(task, &BackgroundTypedTask<T>::finished,
                         [onResult = std::move(onResult)](const T& result) {
                             onResult(result);
                         });
        BackgroundWorkerPool::instance()->enqueue(task);
    }
};
```

使用例:
```cpp
// AssetBrowser のタグフィルタリング（非同期）
AsyncDatabaseQuery::execute(
    *tagStore->db(),
    "SELECT asset_id FROM asset_tags WHERE tag = ? ORDER BY asset_id",
    [&tag](auto& query) {
        query.bind(tag);
        QList<QUuid> ids;
        while (query.next()) {
            ids.append(QUuid(query.get<QString>("asset_id")));
        }
        return ids;
    },
    [this](const QList<QUuid>& ids) {
        // UI スレッドに戻って表示更新
        filteredAssetIds_ = ids;
        updateBrowserView();
    }
);
```

### 4.3 完了条件

- [ ] Render Queue ジョブが SQLite に永続化される
- [ ] アプリ再起動後、キュー内のジョブが復元される
- [ ] 非同期クエリが UI をブロックしない
- [ ] 完了ジョブの履歴検索が動作（日付・プロジェクト・ステータス）

---

## Phase 5: 長期計画（本マイルストーン範囲外・次期）

| 機能 | 優先度 | 説明 |
|------|--------|------|
| Edit History SQLite 化 | P2 | Undo stack のディスクオフロード（大きいプロジェクトでメモリ節約） |
| Asset Catalog SQLite 化 | P3 | JSON → SQLite 移行。複合検索・ソート性能向上 |
| Crash Report DB 収集 | P2 | クラッシュレポートの集約・分析用ビュー |
| コラボレーション状態 | P3 | 編集セッション共有、競合解決、プレゼンス |
| WAL チェックポイント最適化 | P2 | メモリ使用量と I/O のバランス調整 |
| SQLCipher 暗号化 | P3 | 機密プロジェクト向け透過暗号化 |

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `ArtifactCore/include/Database/Database.ixx` | 新規: Database, PreparedQuery 宣言 |
| P1 | `ArtifactCore/include/Database/DatabaseTypes.ixx` | 新規: Row, ResultSet, DatabaseError |
| P1 | `ArtifactCore/src/Database/Database.cppm` | 新規: QSqlDatabase ラッパー |
| P1 | `ArtifactCore/src/Database/PreparedQuery.cppm` | 新規: プリペアドクエリ実装 |
| P1 | `ArtifactCore/src/Database/MigrationRunner.cppm` | 新規: マイグレーションエンジン |
| P1 | `ArtifactCore/CMakeLists.txt` | Qt6::Sql 追加 |
| P2 | `ArtifactCore/src/Diagnostics/SessionLedger.cppm` | SQLite バックエンド実装 |
| P3 | `ArtifactCore/include/Database/AssetTagStore.ixx` | 新規: タグストア |
| P3 | `ArtifactCore/src/Database/AssetTagStore.cppm` | 新規: タグストア実装 |
| P3 | `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | タグ入力・フィルタ統合 |
| P4 | `Artifact/include/Service/DatabaseRenderQueueService.ixx` | 新規（または既存拡張） |
| P4 | `ArtifactCore/include/Database/AsyncDatabaseQuery.ixx` | 新規: 非同期クエリ |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: Database 基盤 | **P0** | 中 | 全後続 Phase の前提。Qt::Sql で依存追加なし |
| P2: SessionLedger 移行 | **P0** | 小 | 最も明白な改善（データ消失問題の解決）。API 変更なし |
| P3: Asset Tag System | **P1** | 中 | AssetBrowser UX の直接改善。タグ検索が即価値 |
| P4: Render Queue + Async | **P1** | 中 | ジョブ永続化＋非同期クエリパターンの確立 |
| P5: 長期計画 | **P2+** | — | 他 Phase 完了後の拡張 |

## 技術選択理由: Qt Sql + SQLite

- **Qt 内蔵** — `QT += sql` だけで追加の vcpkg 依存不要。SQLite ドライバは全 Qt ビルドに同梱
- **単一ファイル** — `.db` 一つで完結。プロジェクト単位やアプリ全体でファイル分割可能
- **WAL モード** — 並行読み取り性能向上。書き込み中も読み取りブロックなし
- **成熟度** — 全世界で最もテストされたデータベースエンジン。破損リスク極小
- **可搬性** — `.db` ファイルを他ツール（DB Browser for SQLite, Python, etc）で直接開ける
