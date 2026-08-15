# MILESTONE_AI_CLOUD_UI_2026-04-09.md

**最終更新:** 2026-08-15

## 概要
AI Cloud LLM統合UI。ローカルLLMに加え、OpenAI/Groq/AnthropicなどのクラウドAPIをサポート。

## 目標
- API key管理
- prompt/response UI
- ストリーミング応答
- 履歴保存
- Artifact内コンテキスト自動挿入

## スコープ
- Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm
- ArtifactCore/src/AI/CloudLLM.cppm
- AppMain統合

## Non-Goals
- フルチャットボット
- 複数モデル同時

## Phase 1: 基本UI
- API選択 (OpenAI/Groq)
- key入力/保存
- prompt送信
- response表示

## Phase 2: ストリーミング/履歴
- リアルタイム応答
- 履歴リスト

## Phase 3: Artifact統合
- 選択layer/context自動挿入
- コード生成/修正提案

## 2026-07-25 実装監査

ArtifactAICloudWidget／設定UI、API key 管理、provider／model 選択、prompt／response 表示、tool log、tool approval mode、MCP preview／transport、AIContext の自動構築、cloud session／worker protocol は実装を確認した。一方、本文の OpenAI／Groq／Anthropic 全対応、streaming／履歴の受け入れ確認、選択 layer の context 挿入からコード生成・修正提案までの一貫した運用は確認できない。したがって Phase 1〜2 は部分実装、Phase 3 は未完了・runtime未検証とする。

## 2026-08-15 現行コード監査

- `ArtifactAICloudWidget` の provider／model 選択、API key の保存・マスク、prompt／response transcript、cancel、model list、tool schema／execution log、MCP transport 操作、Main Window への dock 統合を確認した。
- transcript／streaming UI の状態管理と copy 導線は存在するが、各 provider の実ストリーミング応答、履歴の永続化、API error／rate limit の runtime 挙動は未検証。
- `AIContext` と tool bridge は context を構築できるが、選択 layer からコード生成・修正提案までを一つの受け入れ済み workflow として確認できない。
- API key／外部 tool の安全境界、approval mode、MCP の実プロセス接続も runtime 検証が必要。

判定: **基本 UI、設定、transcript、Tool／MCP 入口は実装済み。provider 実運用、履歴永続化、context から提案適用までの統合、runtime 検証は pending。**

## Update 2026-08-15

現行コードを追加確認した。`ArtifactAICloudWidget` は provider／model選択、API key保存・マスク、prompt／response transcript、cancel、model list、tool schema／execution log、MCP transport操作、Main Window dock統合を持つ。transcript／streaming UIの状態管理とcopy導線、`AIContext`／tool bridgeによるcontext構築も確認できる。

未完了・未検証なのは、各providerの実streaming、履歴永続化、API error／rate limit挙動、選択layerからコード生成・修正提案・適用までのworkflow、API key／外部tool安全境界、MCP実プロセス接続である。基本UIと入口は実装済み、provider実運用と統合受入れは pending とする。

## Update 2026-08-15

`ArtifactAICloudWidget` の provider 別 model selection を `QSettings` に保存・復元するようにした。provider を切り替えても、各 provider の最後の選択モデルを `AICloud/model/<provider>` から復元できる。既存の compact header／prompt 面と `More` 配下の advanced panel の責務分離は維持している。

API key の安全な永続化、provider migration、実 streaming／履歴受入れ、CommandSandbox／MCP の安全境界統合は引き続き pending。
