> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_AI_CLOUD_UI_2026-04-09.md](MILESTONE_AI_CLOUD_UI_2026-04-09.md)

# Codex App パターン → Artifact AICloudWidget 応用 (2026-07-04)

> Codex CLI / Codex App (OpenAI) のアーキテクチャを分析し、Artifact の AICloudWidget に転用可能なパターンを抽出。

## Codex App の主要パターン

### 1. Agent Loop（観察→計画→実行→レビュー）
```
Observe → Plan → Act → Review → (repeat)
```
Codex はコードを読んで理解し、計画を立て、編集し、自分でレビューする自律ループ。Subagents で並列タスクも可能。

**→ Artifact 応用**: AI が comp 構造を分析 → 編集計画を提案 → 実行 → プレビュー → 自動レビュー

### 2. Apply Model（差分提案→確認→適用/却下）
変更を side-by-side diff で表示。ユーザーが Accept/Reject を判断。Worktrees で分離された状態で作業。

**→ Artifact 応用**: AI がプロパティ変更を提案 → Inspector 上で変更前/変更後をプレビュー → 一括適用/却下

### 3. Sandbox（隔離実行環境）
生成されたコードを安全なサンドボックスで実行し、結果を確認してから本体に反映。

**→ Artifact 応用**: AI の提案を仮想 comp に適用 → 結果を Contents Viewer で確認 → 承認したら本番 comp にマージ

### 4. Slash Commands（/fix, /test, /explain）
よく使うプロンプトをスラッシュコマンド化。文脈に応じてパラメータが自動補完。

**→ Artifact 応用**: `/fix-flicker`, `/optimize-render`, `/create-transition`, `/analyze-comp`, `/suggest-colors`

### 5. Memories + Chronicle（学習と記録）
プロジェクトの規約やユーザーの好みを学習。作業履歴を Chronicle として記録。

**→ Artifact 応用**: プロジェクト固有の命名規則・カラーパレット・エフェクト設定を学習。AI 会話履歴の永続化。

### 6. MCP / Tools 拡張
外部ツールを MCP プロトコルで接続。Codex Security でコードスキャンも。

**→ Artifact 応用**: 既に ToolBridge + MCP 対応済み。追加で RenderQueue 自動キューイング、FFmpeg エンコード最適化提案など。

### 7. Subagents（専門エージェントの委譲）
大規模タスクを専門サブエージェントに分割委譲。並列実行で高速化。

**→ Artifact 応用**: Comp分析エージェント、Render最適化エージェント、ColorGrade提案エージェント、Animationチェックエージェント

---

## Artifact に今すぐ実装可能なもの

| 優先 | パターン | 工数 | 実装内容 |
|---|---|---|---|
| 🔴 | **Slash Commands** | 小 | プロンプト入力欄で `/` 入力時にコマンド候補表示。既存機能のショートカット化 |
| 🔴 | **Apply/Reject Diff** | 中 | AI 提案をプレビュー可能な `ArtifactCompositionSnapshot` 作成 → Accept/Reject |
| 🟡 | **Memories/Chronicle** | 中 | プロジェクト固有の指示を永続化。`AGENTS.md` 相当のファイル自動生成 |
| 🟡 | **Sandbox Preview** | 大 | 仮想 comp に AI 変更を適用 → プレビュー → 本番反映 |
| 🔵 | **Subagents** | 大 | 専門エージェント分割（分析/最適化/チェック/提案） |

---

## 比較表

| パターン | Codex CLI | Cursor/Windsurf | Artifact 現状 | Artifact 理想 |
|---|---|---|---|---|
| Agent Loop | ✅ | ✅ | ⚠️ ToolBridge あり | 自律編集ループ |
| Apply Diff | ✅ Worktrees | ✅ Inline | ❌ | Diff プレビュー |
| Sandbox | ✅ | ❌ | ❌ | 仮想 Comp |
| Slash Cmd | ✅ | ✅ | ❌ | `/fix-flicker` 等 |
| Memories | ✅ | ⚠️ Rules | ❌ | プロジェクト学習 |
| MCP/Tools | ✅ | ✅ | ✅ 実装済み | - |
| Subagents | ✅ | ❌ | ❌ | タスク分割 |

## 参考

- `Artifact/docs/MILESTONE_AI_CLOUD_WIDGET_HARDENING_2026-04-09.md`
- `Artifact/docs/MILESTONE_AI_CLOUD_UI_2026-04-09.md`
- `docs/planned/MILESTONE_AI_MCP_TOOL_BRIDGE_2026-04-10.md`
- `docs/planned/MILESTONES_BACKLOG.md` — M-AI-1〜3
## 2026-07-25 実装監査

- Cloud／Local AI agent、context snapshot、API provider 管理、MCP bridge の既存基盤を確認でき、MCP/Tools 拡張は部分的に実装済みである。
- 一方、Codex パターンとして提案された Apply/Reject diff、sandbox preview、slash command、project memories／chronicle、subagent 委譲の専用実装は確認できない。
- AI の応答・prompt 経路は存在するが、composition 編集を観察→計画→適用→レビューする安全な agent loop も未完成である。
- よって本マイルストーンは MCP 基盤のみ部分実装、主要な Codex 応用パターンは未実装の設計・提案段階と判定する。