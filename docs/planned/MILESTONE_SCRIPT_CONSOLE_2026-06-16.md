# M-SCRIPT-1 Script Console (REPL) Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm`,
      `Artifact/src/Widgets/Menu/ArtifactWindowMenu.cppm`,
      `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`,
      `Artifact/src/Script/*`,
      `Artifact/src/Service/ArtifactPythonHookManager.cppm`,
      `ArtifactCore/src/Script/*`,
      `Artifact/include/Export/Python.ArtifactPythonAPI*`
位置づけ: AE の `Window > Expression Editor` / Blender の `Scripting` workspace 互換の foundation。ユーザが REPL で expression / script を実行できる。
参照:
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2
- `docs/planned/MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md`
- `docs/planned/MILESTONE_EXPRESSION_STDLIB_COMPATIBILITY_2026-04-18.md`
- `docs/planned/MILESTONE_EXPRESSION_LOOPOUT_RUNTIME_2026-06-02.md`
- `docs/planned/MILESTONE_EXPRESSION_QUICK_INPUT_2026-04-10.md`
- `docs/planned/MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md`
- `docs/planned/MILESTONE_SCRIPT_MENU_MACRO_ENTRY_2026-05-31.md`
- `docs/planned/MILESTONE_PYTHON_API_SCRIPTING_2026-03-30.md`
- `docs/planned/MILESTONE_TERMINAL_SHELL_2026-04-06.md`

---

## 1. 目的

`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2:

> - Script console: 0 hit

現状は `ArtifactScriptMenu.cppm` の menu のみ。**REPL がない**ため、expression の評価や macro 作成が menu 経由に限定される。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理

### 2.1 既存資産

- `ArtifactScriptMenu.cppm` — script menu
- `ArtifactPythonHookManager.cppm` — Python hook
- `MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md` — Expression engine
- `MILESTONE_EXPRESSION_STDLIB_COMPATIBILITY_2026-04-18.md` — stdlib
- `MILESTONE_EXPRESSION_LOOPOUT_RUNTIME_2026-06-02.md` — loopOut / loopIn
- `MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md` — Python loader
- `MILESTONE_SCRIPT_MENU_MACRO_ENTRY_2026-05-31.md` — macro
- `MILESTONE_PYTHON_API_SCRIPTING_2026-03-30.md` — Python API

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| REPL UI | 0 hit | 入力 / 評価 / 結果表示が不可能 |
| Expression eval on-the-fly | 部分的 | `ExpressionEvaluator` 経由のみ |
| Macro recording | 部分的 | menu のみ |
| Python REPL | 0 hit | Python 経由の評価が menu 限定 |

---

## 3. 設計の柱

### 3.1 Script Console Widget

`Artifact/src/Widgets/Script/ArtifactScriptConsole.cppm` を新規追加:

```cpp
class ArtifactScriptConsole : public QWidget {
public:
    explicit ArtifactScriptConsole(QWidget* parent = nullptr);

    // 入力
    void setLanguage(ScriptLanguage lang);  // Expression / Python / JavaScript (将来)

    // 評価
    void evaluate(const QString& code);

    // 履歴
    QStringList history() const;
    void clearHistory();

signals:
    void outputProduced(const QString& text, OutputKind kind);
    void errorProduced(const QString& text);
    void expressionApplied(const QString& expr);
};
```

- 上部: コード入力 (`QPlainTextEdit`)
- 下部: 出力 / エラー表示
- `Shift+Enter` で複数行入力
- `Enter` で評価
- `↑/↓` で履歴

### 3.2 Expression REPL

`ArtifactCore/src/Script/ExpressionRepl.ixx`:

```cpp
class ExpressionRepl {
public:
    static ExpressionRepl& instance();

    // 評価 (1 行)
    QVariant evaluate(const QString& expr, const ReplContext& ctx);

    // 履歴
    QStringList history() const;

    // 状態
    void setContext(const ReplContext& ctx);
    ReplContext context() const;

signals:
    void resultReady(const QVariant& result);
    void errorRaised(const QString& msg);
};
```

- 既存 `ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm` 経由
- 既存 stdlib (`thisComp / thisLayer / wiggle` 等) すべて使用可能
- 結果は `QVariant` で表現 (int / float / vec / color / string)

### 3.3 Python REPL (Optional)

`Artifact/src/Script/PythonRepl.cppm`:

```cpp
class PythonRepl {
public:
    static PythonRepl& instance();

    bool isAvailable() const;  // Python ランタイム検出
    QString execute(const QString& code);

    // サンドボックス
    void setSandbox(bool enabled);
};
```

- 既存 `ArtifactPythonHookManager` 経由
- Python ランタイム不在時は silent fallback
- sandbox で `time` / `os` / `subprocess` 等の危険 module を block

### 3.4 Macro Recording

`ArtifactCore/src/Script/MacroRecorder.ixx`:

```cpp
class MacroRecorder {
public:
    static MacroRecorder& instance();

    void startRecording(const QString& name);
    void stopRecording();
    bool isRecording() const;

    void recordAction(const QString& action, const QVariantMap& args);

    // 保存
    QString save();  // JSON
    void play(const QString& json);
};
```

- `QUndoStack` の action を hook
- replay で同じ操作を実行

### 3.5 Console Output Routing

- **stdout**: 評価結果
- **stderr**: エラー
- **info**: macro / history 操作
- **warn**: 推奨されない構文

色分け (`theme token` 経由):

- stdout: 通常色
- stderr: 赤
- info: 青
- warn: 黄

### 3.6 Auto-completion

`QCompleter` ベース:

- `thisComp` / `thisLayer` / `thisProperty` / layer 名 / property 名
- `wiggle / seedRandom / gaussRandom / posterizeTime` 等の関数名
- 履歴ベースの補完

### 3.7 Welcome / Tool 統合

- `ArtifactWindowMenu` に `Script Console` 項目追加
- shortcut: `Ctrl+Shift+J` (Python 系) / `Alt+E` (Expression 系)
- 既存 Script menu からも呼び出し可能

### 3.8 Project 保存

- `MacroRecorder` で記録した macro は `FastSettingsStore` に保存
- 個別 project には保存しない (ユーザグローバル)

### 3.9 不変条件 (Guardrails)

- 既存 `ExpressionEvaluator` / `PythonHookManager` は温存
- 新規 signal-slot 接続は `outputProduced / errorRaised / macroRecorded` の 3 個に限定
- `QImage` / `setStyleSheet` 流入禁止
- Python REPL は **sandbox 必須**
- macro replay は **冪等** で **Undo 経由**
- 既存 `ArtifactScriptMenu` の API は温存

### 3.10 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `script.console.python-missing` (severity=info, Python runtime 不在)
- `script.macro.invalid` (severity=warning, macro 再生失敗)
- `script.expression.eval-error` (severity=info, expression 評価エラー)

---

## 4. フェーズ計画

### Phase 1: Script Console Widget (P0, 1〜2 セッション)

- `ArtifactScriptConsole` 実装
- 入力 / 出力 UI
- 履歴

**Done criteria:**
- 上部入力 / 下部出力
- `Enter` で評価
- `↑/↓` で履歴

### Phase 2: Expression REPL (P0, 1 セッション)

- `ExpressionRepl` 実装
- 既存 `ExpressionEvaluator` 経由
- 結果は `QVariant`

**Done criteria:**
- `wiggle(1, 0.5)` 等の式が評価可能
- 既存 stdlib すべて使用可能
- エラーが stderr に表示

### Phase 3: Macro Recording (P0, 1〜2 セッション)

- `MacroRecorder` 実装
- `QUndoStack` hook
- save / play

**Done criteria:**
- 操作を記録
- 同じ macro で再現
- Undo 経由

### Phase 4: Python REPL (Optional, P0, 1 セッション)

- `PythonRepl` 実装
- `ArtifactPythonHookManager` 経由
- sandbox

**Done criteria:**
- Python runtime 検出
- 利用可能時に REPL 動作
- sandbox で危険 module block

### Phase 5: Auto-completion (P1, 1 セッション)

- `QCompleter` ベース
- stdlib / layer / property / history

**Done criteria:**
- Tab で補完
- `thisComp.` で補完候補表示

### Phase 6: Window Menu 統合 (P1, 1 セッション)

- `ArtifactWindowMenu` に Script Console 項目
- shortcut 登録

**Done criteria:**
- `Ctrl+Shift+J` で表示
- menu から表示

### Phase 7: Project 保存 + Diagnostics (P1, 1 セッション)

- macro を `FastSettingsStore` に保存
- Problem View への `script.*` 健全性 contribution

**Done criteria:**
- macro 永続化
- Problem View 表示

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md` | 下位 Expression engine。本 milestone は REPL UI。 |
| `MILESTONE_EXPRESSION_STDLIB_COMPATIBILITY_2026-04-18.md` | stdlib。本 milestone は REPL から呼び出す。 |
| `MILESTONE_EXPRESSION_LOOPOUT_RUNTIME_2026-06-02.md` | loopOut / loopIn。並走。 |
| `MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md` | Python loader。本 milestone は REPL。 |
| `MILESTONE_SCRIPT_MENU_MACRO_ENTRY_2026-05-31.md` | macro。本 milestone は recorder。 |
| `MILESTONE_TERMINAL_SHELL_2026-04-06.md` | terminal とは別。Console は script 専用。 |

---

## 6. リスクと未解決論点

### 6.1 実装リスク

1. **Python sandbox**。危険 module の完全 block は困難
2. **expression 評価の無限ループ**。`while(true)` 等
3. **macro replay の冪等性**。stateful な操作は replay 不可
4. **履歴の disk 容量**。無限に増えていく

### 6.2 設計未解決

- **JavaScript REPL**。将来 (Node.js integration)
- **node editor 統合**。REPL を node graph の一部に
- **macro export / import**。JSON で export
- **AI 補助**。自然言語から script 自動生成

### 6.3 サブモジュール境界

- `ArtifactCore/src/Script/ExpressionRepl.ixx` を新規追加
- `ArtifactCore/src/Script/MacroRecorder.ixx` を新規追加
- `Artifact/src/Widgets/Script/ArtifactScriptConsole.cppm` を新規追加
- `ArtifactCore/CMakeLists.txt` に登録
- `ArtifactWidgets` 触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- Script Console で Expression 評価
- 既存 stdlib (`thisComp / wiggle` 等) が動作
- Python REPL (optional) が動作
- Macro 記録 / 再生
- Auto-completion (Tab)
- `Ctrl+Shift+J` で表示
- 履歴永続化
- Problem View に `script.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2 を正式 milestone に起こした。
