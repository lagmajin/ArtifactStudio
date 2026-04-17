# ArtifactStudio Git内蔵機能実装計画（修正版）

## 要件分析（修正）

ユーザー要求：
- AfterEffectsのようなツールにGitそのものを内蔵
- Gitそのものを操作できるUIを提供
- 優先機能: Gitコミット履歴表示、差分表示、Git操作UI

**重要な修正**: Gitライクなシステムではなく、Gitそのものを活用

## 既存環境分析

**現在のGit環境**:
- プロジェクトはGitリポジトリとして管理済み
- サブモジュール構造: Artifact, ArtifactCore, ArtifactWidgets, DiligentEngine
- 開発ブランチ: dev/20260416 (main統合済み)

**ArtifactStudioアーキテクチャ**:
- QtベースのGUIアプリケーション
- モジュール構造: Artifact(メイン), ArtifactCore(コア), ArtifactWidgets(UI)
- プロジェクトファイル: .artifact, コンポジション, アセット等

**実装方針の変更**:
- 自作バックアップシステム → Gitコミットを利用したバージョン管理
- 自作差分表示 → Git diffコマンドの結果を表示
- Gitマネージャー → 実際のGitリポジトリ操作

## 実装範囲と優先順位

### Phase 1: 基本インフラ (必須)

#### 1.1 Git操作ラッパークラス
**ファイル**: `ArtifactCore/src/Utils/GitManager.cppm/.ixx`

```cpp
class GitManager {
public:
    // 基本操作
    bool initializeRepository(const QString& path);
    bool isRepository(const QString& path);
    GitStatus getStatus(const QString& path);

    // コミット操作
    bool commit(const QStringList& files, const QString& message);
    bool commitAll(const QString& message);

    // リモート操作
    bool push(const QString& remote = "origin");
    bool pull(const QString& remote = "origin");
    bool fetch(const QString& remote = "origin");

    // ブランチ操作
    QStringList getBranches();
    bool switchBranch(const QString& branch);
    bool createBranch(const QString& branch);

    // 履歴取得
    QList<GitCommit> getCommitHistory(int limit = 50);
    GitCommit getCurrentCommit();

    // 差分取得
    GitDiff getDiff(const QString& file = QString());
    GitDiff getStagedDiff();
    QList<GitFileStatus> getFileStatuses();
};
```

#### 1.2 データ構造定義
**ファイル**: `ArtifactCore/include/Utils/GitTypes.ixx`

```cpp
struct GitCommit {
    QString hash;
    QString message;
    QString author;
    QDateTime date;
    QStringList files;
};

struct GitFileStatus {
    QString path;
    GitStatus status; // Modified, Added, Deleted, Untracked, etc.
    QString oldPath; // for renames
};

struct GitDiff {
    QString filePath;
    QList<GitDiffHunk> hunks;
};

struct GitDiffHunk {
    int oldStart;
    int oldCount;
    int newStart;
    int newCount;
    QStringList lines; // with +/- prefixes
};

enum class GitStatus {
    Unmodified,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Ignored
};
```

### Phase 2: バックアップ/復元機能

#### 2.1 自動バックアップシステム
**ファイル**: `Artifact/src/Project/ProjectBackupManager.cppm`

```cpp
class ProjectBackupManager {
public:
    // 自動バックアップ設定
    void setAutoBackupEnabled(bool enabled);
    void setBackupInterval(int minutes);
    void setMaxBackups(int count);

    // バックアップ操作
    bool createBackup(const QString& description = QString());
    bool restoreBackup(const QString& backupId);
    QStringList listBackups();

    // セッション管理
    bool saveSession();
    bool loadSession(const QString& sessionId);
    QStringList listSessions();

    // バージョン管理統合
    bool commitBackup(const QString& description);
    bool revertToVersion(const QString& commitHash);
};
```

#### 2.2 バックアップストレージ
- 場所: `project/.artifact/backups/`
- 形式: タイムスタンプ付きディレクトリ + プロジェクトファイルコピー
- 圧縮: ZIPアーカイブ（オプション）

### Phase 3: 差分表示と履歴ビュー

#### 3.1 差分ビューアーUI
**ファイル**: `ArtifactWidgets/src/Project/GitDiffWidget.cppm`

```cpp
class GitDiffWidget : public QWidget {
    Q_OBJECT

public:
    explicit GitDiffWidget(QWidget* parent = nullptr);

    // 差分表示
    void showDiff(const GitDiff& diff);
    void showFileDiff(const QString& filePath);
    void showCommitDiff(const QString& commitHash);

    // ナビゲーション
    void nextHunk();
    void prevHunk();
    void scrollToLine(int line);

private:
    QTextEdit* diffView;
    QSplitter* splitter;
    QListWidget* fileList;

    // シンタックスハイライト
    void highlightDiff();
    void highlightLine(int line, const QColor& color);
};
```

#### 3.2 コミット履歴ブラウザ
**ファイル**: `ArtifactWidgets/src/Project/GitHistoryWidget.cppm`

```cpp
class GitHistoryWidget : public QWidget {
    Q_OBJECT

public:
    explicit GitHistoryWidget(QWidget* parent = nullptr);

    void setRepositoryPath(const QString& path);
    void refreshHistory();

signals:
    void commitSelected(const QString& hash);
    void diffRequested(const QString& hash);

private:
    QListWidget* commitList;
    QTextEdit* commitDetails;

    void populateCommitList();
    void showCommitDetails(const GitCommit& commit);
};
```

## UI統合計画

### メインウィンドウ統合

1. **メニューバー**: Project > Git > Commit/Push/Pull/History
2. **ステータスバー**: Gitステータス表示（変更ファイル数、未コミット有無）
3. **ツールバー**: クイックコミットボタン

### 専用ダイアログ

#### コミットダイアログ
```cpp
class GitCommitDialog : public QDialog {
    QLineEdit* messageEdit;
    QListWidget* fileList;
    QCheckBox* autoPush;
};
```

#### バックアップダイアログ
```cpp
class ProjectBackupDialog : public QDialog {
    QListWidget* backupList;
    QPushButton* createBackupBtn;
    QPushButton* restoreBtn;
};
```

## 実装順序

### Lv1: コア機能 (Week 1)

1. **GitManagerクラス実装** (ArtifactCore)
   - libgit2 or Qtプロセス経由でGit操作
   - 基本的なstatus/commit/push/pull

2. **GitTypesデータ構造** (ArtifactCore)
   - コミット情報、ファイルステータス、差分データ

3. **ProjectBackupManager実装** (Artifact)
   - 自動バックアップ機能
   - セッション保存/復元

### Lv2: UI機能 (Week 2)

1. **GitDiffWidget実装** (ArtifactWidgets)
   - 差分表示とシンタックスハイライト
   - ハンク単位ナビゲーション

2. **GitHistoryWidget実装** (ArtifactWidgets)
   - コミット履歴表示
   - コミット詳細表示

### Lv3: 統合機能 (Week 3)

1. **メインウィンドウ統合**
   - メニュー/ツールバー追加
   - ステータス表示

2. **バックアップUI**
   - バックアップ管理ダイアログ
   - 復元機能

3. **設定UI**
   - Git設定ダイアログ
   - バックアップ設定

## 技術的考慮事項

### Git操作の実装方法

**選択肢**:
1. **Qtプロセス経由**: `QProcess` で `git` コマンド実行
   - 利点: シンプル、既存Git利用
   - 欠点: パースが必要、非同期処理複雑

2. **libgit2ライブラリ**: Cライブラリ直接使用
   - 利点: 高性能、詳細制御可能
   - 欠点: 追加依存、複雑

3. **ハイブリッド**: 基本操作はQtプロセス、複雑操作はlibgit2

**推奨**: Qtプロセス経由から開始（シンプル）

### セキュリティとパフォーマンス

- **パスワード管理**: Git認証情報はシステムに委ねる
- **非同期処理**: UIフリーズ防止のため全Git操作を非同期化
- **エラーハンドリング**: Gitエラーをユーザーフレンドリーに表示
- **キャンセル機能**: 長時間操作の中止可能

### AfterEffectsとの類似性

After Effectsのバージョン管理に相当する機能を以下のようにマッピング：

| After Effects | ArtifactStudio Git機能 |
|---------------|------------------------|
| Save Versions | Gitコミット |
| Project Recovery | バックアップ/復元 |
| Version History | コミット履歴ブラウザ |
| File Comparison | 差分ビューアー |
| Auto-Save | 自動バックアップ |

## 成功基準

- [ ] Git操作がアプリケーション内から実行可能
- [ ] プロジェクト変更時に自動バックアップ作成
- [ ] コミット履歴をグラフィカルに閲覧可能
- [ ] ファイル差分をわかりやすく表示
- [ ] 誤操作時の復元が可能
- [ ] パフォーマンスがAfterEffects並み（操作遅延1秒以内）

## リスクと緩和策

### リスク1: Git操作の複雑さ
**緩和**: 基本操作のみ実装、詳細設定は外部Gitクライアントに委ねる

### リスク2: UI統合の複雑さ
**緩和**: 既存のダイアログパターンに従い、シンプルなUIから開始

### リスク3: パフォーマンス問題
**緩和**: 非同期処理を徹底、キャッシュを活用

---

## 実装開始準備

この計画で進めますか？ それとも特定の部分を修正/追加したいですか？