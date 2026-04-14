# マイルストーン: Project Health & Problem View System

> 2026-04-14 作成

## 目的

IDE（Visual Studio / Rider）の「Problem View」のような、プロジェクトの問題を一元表示するシステムを実装する。

**エラー・警告を「壊れてから」ではなく「壊れる前に」気づかせる。**

---

## 背景

既存の DebugConsole / PowerShellWidget は **ログ出力・コマンド実行** が目的。

Problem View は **静的解析・構造検証・依存関係チェック** が目的。

**明確に役割が異なる。**

### 現状の問題

- AEはプロジェクトが壊れても気づけない
- 無効な参照、存在しないマット、循環依存がサイレントに無視される
- レンダー失敗の原因がどこにあるか追いにくい
- 共同制作時に「誰かのミス」が後半で発覚

---

## 概念

### Problem View

**何を検出するか:**

| カテゴリ | 例 |
|---|---|
| **無効な参照** | 削除されたソース、missingファイル |
| **存在しないマット** | 参照先のマットレイヤーが存在しない |
| **循環依存** | A→B→A のような依存ループ |
| **設定ミス** | 範囲外のキーフレーム、不正なエクスプレッション |
| **パフォーマンス警告** | 高解像度未使用アセット、過剰なエフェクト |

**どこに表示するか:**
```
┌─ Problem View ──────────────────────┐
│ 🔴 Error   | Missing file: foo.png  │
│ 🔴 Error   | Circular dependency: A │
│ ⚠️ Warning | Unused asset: bar.jpg  │
│ ℹ️ Info    | Low resolution comp    │
└─────────────────────────────────────┘
```

**特徴:**
- AEはこれがない（最大の差別化ポイント）
- DaVinci Resolve / Unity / Rider と同等の品質保証
- 制作中にリアルタイム更新
- エクスポート前にブロック可能

---

## フェーズ設計

### Phase 1: Diagnostics Core

**目的:**
問題検出の基盤を実装する。

**作業項目:**
- `ProjectDiagnostic` クラス
  - 問題の種別（Error/Warning/Info）
  - カテゴリ（Reference/Matte/Circular/Performance等）
  - 発生源（レイヤーID、コンポジションID）
  - メッセージ
  - 修復アクション（提案）
- `DiagnosticEngine`
  - プロジェクト全体をスキャン
  - ルールベースで問題検出
  - 結果をキャッシュ

**完了条件:**
- プロジェクトの問題をプログラムで検出可能
- 結果をリストとして取得可能

---

### Phase 2: Validation Rules

**目的:**
具体的な検証ルールを実装する。

**作業項目:**
- **Reference Validation**
  - missingファイル検出
  - 無効なソース参照
  - 削除されたレイヤー参照
- **Matte Validation**
  - 存在しないマット参照
  - 循環マット依存
- **Circular Dependency**
  - レイヤー間循環依存
  - コンポジション間循環依存
- **Expression Validation**
  - 構文エラー
  - 存在しないプロパティ参照
- **Performance Warnings**
  - 未使用高解像度アセット
  - 過剰なエフェクト数
  - 巨大なコンポジション

**完了条件:**
- 主要な問題をすべて検出可能
- ルールを追加しやすい構造

---

### Phase 3: Problem View Widget

**目的:**
IDEライクな問題表示UIを実装する。

**作業項目:**
- `ArtifactProblemViewWidget`
  - ツリービュー（カテゴリ別にグループ化）
  - Error/Warning/Info のアイコンと色分け
  - クリックで問題箇所へジャンプ
  - 修复アクションの提案
  - フィルタ（Errorのみ/Warningのみ等）
  - リアルタイム更新
- ドッキング対応
- バッジ表示（問題数）

**完了条件:**
- UIから問題一覧を確認可能
- 問題箇所へ即座に移動可能

---

### Phase 4: Real-time Integration

**目的:**
制作中に自動的に検証する。

**作業項目:**
- リアルタイム監視
  - レイヤー追加/削除時
  - プロパティ変更時
  - ファイルリロード時
- バックグラウンド検証
  - 一定間隔で自動スキャン
  - 変更差分のみ検証（高速化）
- 通知
  - 新規問題発生時に通知
  - エクスポート前にブロック警告

**完了条件:**
- 制作中に自動的に問題が更新される
- エクスポート前に警告表示

---

## Non-Goals

- DebugConsole の代替（ログ出力は引き続きDebugConsole）
- PowerShellWidget の代替（コマンド実行は引き続きPowerShellWidget）
- 自動修復（提案のみ、自動実行はしない）
- CI/CD 統合

---

## 技術方針

### Diagnostics Result

```text
ProjectDiagnostic
├─ severity (Error/Warning/Info)
├─ category (Reference/Matte/Circular/Performance/Expression)
├─ source (layerId, compId)
├─ message
├─ description (詳細)
├─ fixAction (提案)
└─ timestamp
```

### Validation Rule

```text
ValidationRule
├─ name
├─ severity
├─ enabled (bool)
└─ validate(project) -> List<Diagnostic>
```

### Engine

```text
DiagnosticEngine
├─ rules (List<ValidationRule>)
├─ cache (DiagnosticCache)
├─ validateAll() -> List<Diagnostic>
├─ validateDelta(changes) -> List<Diagnostic>
└─ getResults() -> List<Diagnostic>
```

---

## 関連マイルストーン

- `M-DEV-1` Crash Diagnostics & Recovery
- `MILESTONES_BACKLOG.md` - M-QA-2 Manual Regression Checklist
- `Artifact/src/Widgets/PowerShellWidget.cppm` - 既存のコンソール（別物）
- `ArtifactCore/include/Event/EventBus.ixx` - リアルタイム通知

---

## 完了条件全体

- [ ] Phase 1: Diagnostics Core
- [ ] Phase 2: Validation Rules
- [ ] Phase 3: Problem View Widget
- [ ] Phase 4: Real-time Integration

---

## 差別化ポイント

| 機能 | AE | Resolve | このツール |
|---|---|---|---|
| Problem View | ❌ | ⚠️ 一部 | ✅ 完全 |
| 循環依存検出 | ❌ | ✅ | ✅ |
| Missing検出 | ⚠️ 遅い | ✅ | ✅ リアルタイム |
| 修复提案 | ❌ | ❌ | ✅ |
| リアルタイム | ❌ | ⚠️ | ✅ |

---

## 現状

2026-04-14 時点で未着手。

`ArtifactProjectHealthDashboard.cppm` が一部存在するが、
Problem View としての機能はまだ。

実装は空き時間に段階的に進める。
