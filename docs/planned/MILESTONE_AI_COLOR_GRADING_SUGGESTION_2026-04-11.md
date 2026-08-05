> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_AI_COLOR_GRADING_SUGGESTION_2026-04-21.md](MILESTONE_AI_COLOR_GRADING_SUGGESTION_2026-04-21.md)

# Milestone: AI Color Grading Suggestion (2026-04-11)

## Overview
AIを活用して、画像やビデオのシーンを分析し、適切なカラーグレーディングを提案する機能を追加。ユーザーの好みやシーンタイプに基づいてLUTや調整パラメータを自動生成。

## Goals
- シーン分析: 明るさ、コントラスト、色調をAIで解析
- 提案生成: シーンに適した色調整を提案
- UI統合: プロパティパネルで提案を表示し、適用可能

## Implementation Phases

### Phase 1: Core AI Analyzer
- `AIColorAnalyzer`: 画像から色統計を抽出
- `ColorGradingSuggester`: 統計から調整パラメータを生成
- API: `suggestGrading(const cv::Mat& image)`

### Phase 2: LUT and Preset Integration
- 提案をLUTとして保存
- 既存のColorGrading systemと統合
- プリセットライブラリとの連携

### Phase 3: Advanced Features
- ビデオシーケンス全体の分析
- ユーザーフィードバックによる学習
- スタイル転送 (例: 映画風、ドラマ風)

## Dependencies
- OpenCV for image analysis
- 既存のColorGrading system

## Estimation
- Phase 1: 20-25h
- Phase 2: 15-20h
- Phase 3: 25-30h

Total: 60-75h

## Success Criteria
- シーンに適した色調整が提案される
- 提案が適用可能で、結果が自然
- UIが使いやすく、提案を簡単に適用できる

## 2026-07-25 実装監査

現行コードからは、このマイルストーン固有の `AIColorAnalyzer`／`ColorGradingSuggester`、画像統計からの grading suggestion API、提案を表示・適用する専用 UI は確認できない。既存の ColorGradingEngine、Color Science Panel、AI の機能説明・自然言語導線は関連基盤として存在するが、シーン分析から提案生成・LUT／preset適用までの実装を証明するものではない。したがって Phase 1〜3 は未実装・runtime未検証とする。
