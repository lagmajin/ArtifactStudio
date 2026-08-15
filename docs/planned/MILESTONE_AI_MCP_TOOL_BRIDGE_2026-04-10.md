# MILESTONE: AI MCP Tool Bridge

> 2026-04-10 作成
> 2026-08-15 現行コード監査

## 目的

ArtifactStudio の AI 基盤を、外部 MCP 互換ツール群や将来の function calling へ接続しやすい形へ整える。

現状は `DescriptionRegistry` / `AIToolExecutor` / `AIContext` が内部用の tool / context 土台として存在するが、
MCP の transport や session 管理、外部 tool server との双方向接続までは未実装である。

この milestone では、まず内部 tool schema を安定化し、次に MCP 風の接続境界を切る。

---

## 背景

既存の AI 基盤には次の要素がある。

- `ArtifactCore::DescriptionRegistry`
- `ArtifactCore::IDescribable`
- `ArtifactCore::AIToolExecutor`
- `ArtifactCore::AIContext`
- `ArtifactCore::AIPromptGenerator::generateToolSchemaJson()`

これらは「AI に見せる説明」や「JSON tool call を実行する」ための基盤としては使えるが、
MCP のような

- tool discovery
- schema negotiation
- request / response session
- external server bridge

はまだ揃っていない。

---

## 方針

1. まず内部 tool schema を registry 由来で一元化する
2. 次に tool call 形式を `class / method / arguments` から schema-aware へ寄せる
3. その後で外部 MCP server / client bridge を追加できる境界を切る
4. cloud AI と local AI のどちらからも共通の tool/context を参照できるようにする

---

## 既存の土台

- `ArtifactCore/include/AI/IDescribable.ixx`
  - class / property / method の AI 向け説明
- `ArtifactCore/include/AI/AIToolExecutor.ixx`
  - JSON tool call の実行
- `ArtifactCore/include/AI/AIContext.ixx`
  - project / composition / selection の snapshot
- `ArtifactCore/include/AI/AIPromptGenerator.ixx`
  - registry 由来の tool schema 出力

---

## 実装フェーズ

### Phase 1: Schema Stabilization

- registry を元に tool schema を生成する
- tool / component / method の名前空間を揃える
- context snapshot を tool schema と一緒に配布できるようにする

### Phase 2: Internal Tool Bridge

- cloud AI / local AI から同一の schema を参照する
- JSON tool call のフォーマットを固定する
- tool result を AI conversation に再注入する

### Phase 3: MCP Boundary

- external MCP server との bridge を定義する
- transport 層と internal executor 層を分離する
- MCP tool 名と Artifact internal tool 名の mapping を作る

### Phase 4: Tool UX

- tool 実行ログ
- dry-run
- confirmation
- denied / error / not-found の可視化

---

## 非目標

- 完全な MCP サーバー実装
- 全 tool の外部公開
- すべての AI 機能の一括置換

---

## 進捗

- `ArtifactCore::AIPromptGenerator::generateToolSchemaJson()` を registry 由来の出力に変更済み
- `ArtifactCore::ToolBridge` を追加し、tool schema / tool call parse / tool trace を共通化済み
- `ArtifactCore::McpBridge` を追加し、stdio JSON-RPC 風の initialize / tools/list / tools/call frame を生成・処理できるようにした
- `ArtifactAICloudWidget` から、tool call JSON を検出して `AIToolExecutor` で実行する loop を追加済み
- `ArtifactAICloudWidget` に tool schema preview / tool log / MCP preview を追加済み
- `ArtifactAICloudWidget` の transport panel から `initialize` / `tools/list` / `ping` を直接試せるようにした
- `ArtifactAICloudWidget` の transport panel から `tools/call` を直接試せるようにした
- `ArtifactAICloudWidget` に tool selector と arguments template を追加し、`tools/list` の候補から呼び出しやすくした
- `Artifact.exe --mcp-server` の自己ホスト transport mode を追加し、同一実行体で MCP framed JSON-RPC を試せるようにした
- `AIClient` でも同じ tool loop を使うようにした
- cloud prompt に live `AIContext` snapshot を含めるようにした

### 2026-08-15 実装確認

- `AIPromptGenerator`／`ToolBridge` は registry 由来 schema、tool call の unwrap／validation、arguments 検査、tool result／trace を共通化している。
- `McpBridge`／`McpTransport` は initialize、tools/list、tools/call、ping、stdio framed JSON-RPC の encode／decode と外部 process 入口を持つ。ArtifactAICloudWidget には preview、tool selector、arguments template、approval mode、transport log がある。
- `Artifact --mcp-server` の自己ホスト経路と AIClient／cloud widget の tool loop を確認したため、旧来の「transport 未実装」は現状に適用しない。
- 全 provider／local agent の同一 runtime 契約、外部 MCP process の安全な allowlist 統合、tool result の会話履歴への完全な再注入、実サーバー互換性は未検証。

判定: **Phase 1〜4 の schema／internal bridge／MCP boundary／Tool UX は主要経路実装済み。外部互換性、安全境界統合、provider 横断 runtime 検証は pending。**

## Update 2026-08-15

現行コードを追加確認した。`AIPromptGenerator`／`ToolBridge` は registry由来schema、tool call unwrap／validation、arguments検査、tool result／traceを共通化し、`McpBridge`／`McpTransport` は initialize、tools/list、tools/call、ping、stdio framed JSON-RPC、外部process入口を持つ。AI Cloud UIにはpreview、tool selector、arguments template、approval mode、transport logがあり、`--mcp-server` 自己host経路とAIClientのtool loopも存在する。

未完了・未検証なのは、全provider／local agentの同一runtime契約、外部MCP processのallowlist統合、tool resultの会話履歴への完全再注入、実MCPサーバー互換性とruntime受入れである。主要bridgeとTool UXは実装済み、安全境界・外部互換性は pending とする。

## 期待する完了条件

- registry 由来の tool schema が常に生成できる
- AI が tool call を schema-aware に解釈できる
- internal / external の橋渡しを後から追加しやすい
