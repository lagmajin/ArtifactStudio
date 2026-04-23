# Milestone: AI Basic Assistant (2026-04-11)

## Overview
基本的なAIアシスタント機能を追加し、ユーザーの質問に応答し、プロジェクト情報を提供する。

## Goals
- 質問応答: プロジェクトに関する質問に答える
- 情報提供: ドキュメント, コードの検索と提供
- 統合: MCP経由で外部AIと連携

## Implementation Phases

### Phase 1: Core Assistant Engine
- `AIBasicAssistant`: 質問を受け取り、応答を生成
- シンプルなルールベース + MCP連携
- API: `std::string respond(const std::string& query)`

### Phase 2: Integration
- UI統合: チャットウィンドウ
- プロジェクトデータベースのアクセス

## Estimation
- Phase 1: 20-30h
- Phase 2: 15-20h

Total: 35-50h

## Success Criteria
- 基本的な質問に正しく応答
- プロジェクト情報を提供可能