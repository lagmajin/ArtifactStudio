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