# AI Integration Milestone

日付: 2026-04-09

## Goal

ローカルAIとクラウドAIを統合し、作業规模和复杂度に応じて適切なAIを使い分けることで、効率的なワークフローを構築する。

## Architecture

```
User Request
     │
     ▼
┌─────────────────────────┐
│   Request Classifier    │
└─────────────────────────┘
     │
     ├──────────────────────────────┐
     ▼                              ▼
┌─────────────────────┐  ┌─────────────────────┐
│     Local AI        │  │     Cloud AI        │
│    (Ollama等)        │  │ (OpenRouter等)      │
│  → 軽量・即時回答     │  │  → 高精度・大規模作業 │
└─────────────────────┘  └─────────────────────┘
     │                              │
     └──────────┬───────────────────┘
                ▼
┌─────────────────────────┐
│   Result Aggregator     │
│ (並列処理・統合表示)      │
└─────────────────────────┘
```

## Milestones

### M-AI-1 Local AI Integration

軽作業（コード補完、文法修正、小さなリファクタリングなど）をローカルAIで処理する。

完了条件:

- Ollama との接続
- ローカルAIで処理可能なタスクの判定
- レスポンスの受信と表示

主な作業:

- Ollama API との接続
- タスク判別ロジック
- 結果の整形表示

### M-AI-2 Data Collection & Processing

クラウドAIに送信する前に、コンテキストデータを収集・加工する。

完了条件:

- 関連ファイルの収集
- 不要データのフィルタリング
- コンテキストサイズの最適化

主な作業:

- ファイルパスの解決
- コードの要約生成
- トークン数の估算

### M-AI-2.5 Cloud AI Provider Foundation

クラウドAI接続の共通基盤を確立する。

完了条件:

- [x] Core層に `ICloudAIAgent` 抽象クラス（`LocalAIAgent` と対になる）
- [x] OpenRouter Agent（Claude/GPT/Gemini等多种対応のプロキシパイプラ）
- [x] `APIKeyManager` - セキュア хранилище とプロキシ設定管理
- [x] `TieredAIManager` と `ICloudAIAgent` の連携

主な作業:

- [x] `Core.AI.CloudAgent` モジュール追加 (`ICloudAIAgent.ixx`, `CloudAgent.cppm`)
- [x] `ICloudAIAgent` インターフェース定義
- [x] OpenRouter Agent 実装 (`CloudAgentFactory`)
- [x] `APIKeyManager` 追加
- [ ] DirectAnthropic / DirectOpenAI Agent（直接接続用・未実装）

### M-AI-2.7 Local + Cloud Parallel Usage

ローカルとクラウドを同時に使用可能にする。

完了条件:

- リクエスト分類後の並列処理
- ローカル結果とクラウド結果を統合
- UIで両方の進捗を表示

主な作業:

- `TieredAIManager` に並列呼び出しモード追加
- 結果マージ戦略
- 進捗/完了通知

### M-AI-3 Cloud AI Integration

大規模作業（アーキテクチャ設計、大きなリファクタリング、ドキュメント生成など）をクラウドAIで処理する。

完了条件:

- クラウドAIへのリクエスト送信
- 大きなコンテキストへの対応
- 進捗表示とエラー処理

主な作業:

- API統合
- コンテキスト分割
- レート制限対応

### M-AI-4 Unified Result Display

ローカルAIとクラウドAIの結果を統一された形式で表示する。

完了条件:

- 一貫したUI表示
- エラー状态的表示
- 処理時間の表示

## Recommended Order

1. `M-AI-2 Data Collection & Processing` (共通基盤)
2. `M-AI-2.5 Cloud AI Provider Foundation` (Provider抽象化 + APIキー管理)
3. `M-AI-2.7 Local + Cloud Parallel Usage` (並列利用)
4. `M-AI-1 Local AI Integration`
5. `M-AI-3 Cloud AI Integration`
6. `M-AI-4 Unified Result Display`

## Implementation Notes

### Current Architecture

#### Core Layer (ArtifactCore)
- `TieredAIManager` - ローカル/クラウド統合管理
  - 対応: `ICloudAIAgent` によるクラウド接続（simulation → 実API）
- `LocalAIAgent` - ローカルAI抽象化（Llama / OnnxDML対応）
- `ICloudAIAgent` - クラウドAI抽象化（OpenRouter対応済み）
- `APIKeyManager` - APIキー管理

#### App Layer (Artifact)
- `AIClient` - ローカルAI管理、`setApiKey()` / `setProvider()` を持つ

### Future Core Layer Design

```
TieredAIManager
├── globalContext: AIContext
├── localAgent: LocalAIAgentPtr
├── cloudAgent: ICloudAIAgentPtr  ← NEW
└── processRequest() → 並列/単独振り分け
```

### Provider Foundation Design

```
ICloudAIAgent (interface)
├── OpenRouterAgent ← IMPLEMENTED (主要provider)
│   - OpenRouter経由でClaude/GPT/Gemini等多种に対応
│   - 单一endpointで複数のモデルにアクセス可能
├── DirectAnthropicAgent ← TODO (直接Anthropic接続)
└── DirectOpenAIAgent ← TODO (直接OpenAI接続)

APIKeyManager ← IMPLEMENTED
├── プロバイダー別のキーストア
├── 設定ファイルへのセキュア保存
└── マスク表示（***...）
```

### Provider Types

| Provider | 用途 | API方式 |
|----------|------|---------|
| OpenRouter | プロキシパイプラ（推奨） | OpenRouter API endpoint |
| DirectAnthropic | Anthropic直接接続 | Anthropic API |
| DirectOpenAI | OpenAI直接接続 | OpenAI API |

### Architecture Note

OpenRouterは「单一endpoint」で複数のAIサービスにアクセス可能。
ユーザーはOpenRouterのAPI keyを取得すれば、Claude/GPT/Gemini等多种なAIモデルを利用可能。

### Implementation Files

| ファイル | 内容 |
|----------|------|
| `ArtifactCore/include/AI/ICloudAIAgent.ixx` | `ICloudAIAgent` インターフェース |
| `ArtifactCore/include/AI/APIKeyManager.ixx` | `APIKeyManager` 宣言 |
| `ArtifactCore/src/AI/CloudAgent.cppm` | `OpenRouterAgent` 実装 + Factory |
| `ArtifactCore/src/AI/APIKeyManager.cppm` | `APIKeyManager` 実装 |
| `ArtifactCore/src/AI/TieredAIManager.cppm` | クラウドAgent統合対応 |

### App Layer Integration

```
AIClient (Artifact)
├── localAgent  ← 既存
├── cloudAgent  ← NEW: Core.AIClient→ICloudAIAgent bridge
└── sendMessage() / postMessage()
```
