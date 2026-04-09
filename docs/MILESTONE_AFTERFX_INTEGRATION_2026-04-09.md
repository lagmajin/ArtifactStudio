# AfterEffects Integration Milestone

日付: 2026-04-09

## Goal

After Effects の主要な機能を統合し、ユーザーが After Effects で慣れ親しんだワークフローやエフェクトを Artifact で再現可能にすることで、学習コストを削減し、シームレスな移行を実現する。

## Architecture

```
Artifact Core
├── Effect System (After Effects エフェクトの実装)
├── Timeline System (After Effects ライクな操作性)
├── Layer System (After Effects レイヤー管理)
└── Import/Export (After Effects プロジェクトファイルの読み書き)
```

## Milestones

### M-AF-1 After Effects エフェクトの実装

After Effects で使用される主要なエフェクトを実装する。

完了条件:
- [ ] 少なくとも 10 種類の主要な After Effects エフェクトを実装
- [ ] エフェクトのパラメータを After Effects と同様に編集可能にする
- [ ] エフェクトのプレビューをリアルタイムで表示可能にする

主な作業:
- [ ] エフェクトのシェーダー/アルゴリズム実装
- [ ] UI によるパラメータ編集インターフェース
- [ ] エフェクトの適用・解除機能
- [ ] エフェクトのプリセット保存・読み込み

主なエフェクト候補:
- CC Sphere
- Fractal Noise
- CC Particle World
- Curves
- Levels
- Hue/Saturation
- Time Remap
- Motion Blur
- Depth of Field
- 3D Camera

### M-AF-2 After Effects ワークフローのエミュレーション

After Effects のタイムライン操作やレイヤー管理を再現する。

完了条件:
- [ ] タイムラインの操作性が After Effects に近い
- [ ] レイヤーの親子構造が正しく機能する
- [ ] ショートカットキーが After Effects と一致する
- [ ] ワークエリアのループ再生が可能

主な作業:
- [ ] タイムラインのナビゲーション改善
- [ ] レイヤーパネルの機能強化
- [ ] ショートカットキーのマッピング
- [ ] フレームステップ機能
- [ ] コンテキストメニューの充実

### M-AF-3 After Effects プロジェクトのインポート

After Effects プロジェクトファイルを読み込んでコンポジションを作成する。

完了条件:
- [ ] .aep/.aepx ファイルの読み込み
- [ ] コンポジション、レイヤー、エフェクトの変換
- [ ] タイムライン情報のインポート
- [ ] メタデータの保持

主な作業:
- [ ] XML パーサーの実装
- [ ] After Effects プロジェクトファイルの仕様調査
- [ ] データ変換ロジック
- [ ] エラーハンドリングと検証

## Recommended Order

1. **M-AF-1 After Effects エフェクトの実装** (コア機能)
2. **M-AF-2 After Effects ワークフローのエミュレーション** (操作性向上)
3. **M-AF-3 After Effects プロジェクトのインポート** (データ互換性)

## Implementation Notes

### エフェクト実装のポイント

- After Effects エフェクトは主に GLSL/HLSL シェーダーで実装可能
- パラメータは `Property` システムと連携
- リアルタイムプレビューには `SoftwareRenderer` を使用

### ワークフロー改善のポイント

- タイムライン操作は `ArtifactTimelineWidget` の拡張で対応
- ショートカットは `InputOperatorManager` から設定
- レイヤー親子構造は `LayerSystem` で実装

### プロジェクトインポートのポイント

- After Effects プロジェクトファイルは XML ベース
- 仕様は Adobe の公式ドキュメントを参照
- 変換ロジックは Artifact のデータ構造に合わせて設計

## Related Files

| ファイル | 内容 |
|----------|------|
| `ArtifactCore/include/Effect/AfterEffectsEffects.ixx` | After Effects エフェクト実装 |
| `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp` | タイムライン操作の改善 |
| `Artifact/src/Import/AfterEffectsImporter.cppm` | After Effects プロジェクトインポーター |
| `Artifact/include/Asset/AfterEffectsProject.ixx` | After Effects プロジェクトデータ構造 |