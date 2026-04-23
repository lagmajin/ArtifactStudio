# Milestone: AI Assisted Keyframe Generation (2026-04-11)

## Overview
AIを活用して、オブジェクトの動きを分析し、自動的にキーフレームを生成する機能を追加。ユーザーの手動入力からパターンを学習し、スムーズなアニメーションを提案。

## Goals
- 軌跡解析: 既存のキーフレームから動きのパターンを学習
- 自動生成: 新しいキーフレームをAIが提案
- UI統合: タイムラインで提案を表示し、適用可能

## Implementation Phases

### Phase 1: Core AI Engine
- `AIKeyframeGenerator`: 軌跡データを入力としてキーフレームを生成
- シンプルな機械学習モデル (線形回帰やRNN)
- API: `generateKeyframes(const std::vector<Point>& trajectory, int numFrames)`

### Phase 2: Integration with Timeline
- ArtifactTimelineWidget と統合
- 選択されたレイヤーの軌跡を抽出
- 生成されたキーフレームを提案として表示

### Phase 3: Advanced Features
- 複数オブジェクトの相互作用考慮
- 物理ベースの補間 (ばね、摩擦など)
- ユーザーフィードバックによる学習改善

## Dependencies
- 既存のKeyframe system
- AI/MLライブラリ (TensorFlow Lite や ONNX)

## Estimation
- Phase 1: 15-20h
- Phase 2: 10-15h
- Phase 3: 20-25h

Total: 45-60h

## Success Criteria
- 手動キーフレームから自動生成が可能
- 生成されたアニメーションが自然に見える
- UIが直感的で適用しやすい