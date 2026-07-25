# マイルストーン: AI クラウドウィジェット 機能監査 (2026-07-04)

> 作成: 2026-07-04

## 監査サマリー

`ArtifactAICloudWidget.cppm`（2,725行）は Cloud AI の会話 UI。OpenAI / Grok / OpenRouter / KiloGateway / Custom プロバイダに対応し、モデル選択、APIキー管理、チャット履歴表示、Tool Bridge / MCP 連携を備える。ChatGPT / Cursor / Copilot / Claude / Windsurf 等の現代的な AI アシスタント UI と比較した不足機能を以下に収集。


---

## 🔴 P0: 会話体験の基本機能

ChatGPT / Claude / Gemini の Web UI を基準。

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Streaming 逐次表示** | 全 AI チャット | ⚠️ バックエンドは対応。UI 側の逐次トークン表示は要確認 |
| **Stop Generating ボタン** | ChatGPT/Claude | ❌ |
| **Regenerate（再生成）** | ChatGPT | ❌ 同じプロンプトで再送 |
| **Edit Previous Message** | ChatGPT/Claude | ❌ |
| **Code Block Copy ボタン** | ChatGPT/Cursor | ❌ |
| **Code Block Apply/Insert ボタン** | Cursor/Windsurf | ❌ 提案コードを現在の comp に適用 |
| **Markdown レンダリング（テーブル/リスト）** | 全 AI チャット | ⚠️ |
| **画像添付/ペースト** | ChatGPT/Claude | ❌ ビューポートのスクリーンショット→即AI送信 |
| **会話タイトル自動生成** | ChatGPT | ❌ |
| **会話履歴サイドバー** | ChatGPT/Claude | ❌ 複数会話切替 |
| **会話検索** | ChatGPT | ❌ |
| **会話エクスポート** | ChatGPT | ⚠️ copyTranscriptToClipboard あり |

---

## 🔴 P0: コスト・コンテキスト管理

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Token 使用量表示** | Cursor/Copilot | ❌ |
| **コスト概算（$）** | Cursor/OpenAI Playground | ❌ |
| **コンテキストウィンドウ使用率ゲージ** | Cursor/Windsurf | ❌ 32768/128000 のようなゲージ |
| **トークン制限警告** | ChatGPT | ❌ |

---

## 🟡 P1: プロンプト・ワークフロー

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Slash Commands（/fix /explain）** | Cursor/Windsurf | ❌ |
| **@-メンションコンテキスト（@comp @layer）** | Cursor/Copilot | ❌ |
| **Prompt Library** | Cursor/Windsurf | ❌ |
| **System Prompt エディタ UI** | OpenAI Playground | ⚠️ API はあるが UI がない |
| **Temperature/Top-P/MaxTokens 設定** | OpenAI Playground | ❌ |
| **↑↓キーでプロンプト履歴** | ChatGPT | ❌ |

---

## 🟡 P1: コード適用・Diff

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Diff プレビュー** | Cursor/Windsurf | ❌ |
| **Accept/Reject per Suggestion** | Cursor/Windsurf | ❌ |
| **Apply All ボタン** | Cursor/Windsurf | ❌ |
| **Undo AI Change** | Cursor | ❌ |
| **Tool Call 折りたたみ表示** | Claude/Cursor | ⚠️ |

---

## 🔵 P2: 高度機能

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Agent Mode トグル** | Cursor/Windsurf | ❌ |
| **Thinking/Reasoning 表示** | Claude/o1 | ❌ |
| **Web Search トグル** | ChatGPT/Copilot | ❌ |
| **Multi-modal（画像認識）** | ChatGPT/Claude | ❌ |
| **Custom Instructions** | ChatGPT | ❌ |
| **MCP Server ヘルスチェック** | Claude Desktop | ❌ |
| **Context-Aware Suggest** | Cursor | ❌ |
| **Token 数/コスト表示** | Cursor | ❌ |

---

## 📊 優先度マトリクス

| 優先 | カテゴリ | 件数 | 代表機能 |
|---|---|---|---|
| 🔴 | 会話基本機能 | 12 | Streaming/Stop/Regen/Edit/CodeCopy/Markdown/画像 |
| 🔴 | コスト管理 | 4 | Token数/コスト/コンテキスト使用率/警告 |
| 🟡 | プロンプトWF | 6 | SlashCmd/@mention/PromptLib/SystemPromptUI |
| 🟡 | コード適用 | 5 | Diff/AcceptReject/ApplyAll/UndoAI |
| 🔵 | 高度機能 | 8 | AgentMode/Thinking/WebSearch/MultiModal |

## 2026-07-25 実装監査

現在の widget には provider／model／API key、session／transcript、cancel、tool approval、tool schema／log、MCP 操作の入口があるため、表の一部は更新が必要である。一方、streaming のUI表示、regenerate／edit、Markdown／code block操作、画像添付、token／cost表示、diff／apply、MCP health、agent mode などは未確認である。また process split も未完了で、widget が通信・tool・MCP責務を保持している。したがって本書は不足機能の監査として有効だが、P0〜P2の大半は未完了・runtime未検証とする。
