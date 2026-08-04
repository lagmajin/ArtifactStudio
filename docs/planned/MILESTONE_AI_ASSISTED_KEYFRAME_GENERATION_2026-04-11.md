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

## 2026-07-25 実装監査

既存の KeyframePatternGenerator、KeyPatternDialog、Timeline の keyframe 編集・Undo 経路に加え、`KeyframePatternGenerator::generateFromTrajectory()` による有限値検証付きの 2D 軌跡再サンプリング、`RationalTime` 候補生成、`ArtifactTimelineWidget::applyTrajectoryToProperty()` による Undo 付き適用を実装した。機械学習モデル、選択レイヤーからの自動軌跡抽出、候補表示を伴う専用 UI は未接続であり、Phase 2 の自動導線・Phase 3 と runtime 検証は未完了とする。
