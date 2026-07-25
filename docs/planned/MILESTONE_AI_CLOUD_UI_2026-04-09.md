# MILESTONE_AI_CLOUD_UI_2026-04-09.md

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
