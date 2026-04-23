# Group Layer Implementation Plan

## Overview
グループレイヤー（Group Layer）は複数のレイヤーをネストして一つの集合として扱える機能です。目的はレイヤーの論理的整理、まとめて変換・表示・ブレンドする能力の提供です。

## Proposed approach
1. Data model: GroupLayer クラスを追加。子レイヤーIDリスト、ローカル変換（position/scale/rotation）、collapsed/expanded フラグ、blend mode、マスク参照を持つ。
2. Serialization: プロジェクト保存/読み込み時の入出力対応（後方互換を維持）。
3. UI: レイヤーパネルで group/ungroup、expand/collapse、ドラッグによる入れ子化、グループの選択と再配置を実装。
4. Render integration: グループの子要素をオフスクリーン（accum）に描画・合成し、1つのスプライトとして親に合成。マスク/クリッピングとブレンド順序を保持し、エッジブリード対策（背景を accum に描画）をグループにも適用する。
5. Tests & QA: ネスト透明、変換、マスクなどの視覚検証ケースとユニットテスト。
6. Docs: 使い方とレンダー面の制約・注意点をドキュメント化。

## Todos (高レベル)
- group-layer-model: グループレイヤーモデル定義
- group-layer-serialization: 保存/読み込み対応
- group-layer-ui: パネル/操作UI
- group-layer-render: レンダーパイプライン統合
- group-layer-tests: 視覚テスト/ユニット
- group-layer-docs: ドキュメント追加

## Notes / Considerations
- Rendering: グループは親座標系の transform を適用してオフスクリーンに描画し、その結果を親に合成する。現在の accum ベースの対策（背景を accum に描画→子を合成→ブリット）をそのまま利用する。
- Hit-testing: ネストを考慮した深いヒットテストが必要。
- Performance: 大きなグループはオフスクリーン解像度やキャッシュ（flattened sprite）を検討。
- Backward compatibility: 既存ファイルとの互換性とフォーマットVersioningを注意。

## Milestones
1. モデル定義 + 単体検証
2. シリアライズ対応
3. UI（パネル）実装
4. レンダリング統合
5. テスト/ドキュメント

---

(このファイルは session の plan.md の内容をリポジトリの docs に保存したものです)