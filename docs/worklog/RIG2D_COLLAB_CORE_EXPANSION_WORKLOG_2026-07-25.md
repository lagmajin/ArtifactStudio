# RIG2D_COLLAB_CORE_EXPANSION_WORKLOG_2026-07-25

## 概要

コアライブラリ (ArtifactCore) の機能調査をもとに、特に薄かった Rig2D と Collaborate を拡充した。

## 調査結果

ArtifactCore (~765 .ixx + ~466 .cppm) のソースコード調査から、以下のボトルネックを特定:

- **Sound/SoundTrack, SoundType**: ほぼ空のスケルトン (SoundTrack は空 class、SoundType は空 namespace)
- **CSharpScriptEngine**: 19行、完全に空
- **Tool/ToolMode**: ただのenumのみ
- **ReactiveEvents**: 完全なデータモデル + JSONシリアライズがあるのに評価エンジンが丸々欠落
- **Rig2D Bone2D::evaluate(time)**: time を完全無視するスタブ
- **Rig2D TwoBoneIKConstraint2D poleAngle**: 宣言＋シリアライズのみで evaluate() 未使用
- **CollaborationProtocol**: 全型に export が無く、誰からも import されない孤立コード
- **CollaborationWebSocket**: セッション管理と rule sync 機能が欠落

このうち Rig2D と Collaborate を選択して実装。

## 実装内容

### Rig2D 拡充 (2ファイル)

1. `BoneTransform` に `+`, `-`, `*float` 演算子を追加 → `AnimatableValueT<BoneTransform>` が動作可能に
2. `Bone2D` にキーフレーム管理API追加: `addKeyFrame()`, `removeKeyFrameAt()`, `hasKeyFrameAt()`, `keyFrameCount()`, `clearKeyFrames()`
3. `Bone2D::evaluate(time)` 実装: キーフレームがあれば `AnimatableValueT::at()` による時間補間、なければ `localTransform_` を返す
4. JSON シリアライズ対応: `toJson()`/`fromJson()` にキーフレーム配列入出力追加
5. `TwoBoneIKConstraint2D::evaluate()` で `poleAngle_` を使用した肘の向き制御

### Collaborate 拡充 (3ファイル)

1. `CollaborationProtocol.cppm` の型を `export` 化 → `Collaborate.Protocol` module が外部から import 可能に
2. `CollaborationWebSocket` にセッションID生成 (`QUuid`) + `sessionId()` アクセサ追加
3. `CollaborationWebSocket` に `sendRuleSync()` と4種の rule sync シグナル追加
4. WebSocket メッセージハンドラに rule_added/removed/updated/executed ディスパッチ追加

## 変更ファイル一覧

| ファイル | 追加行 | 内容 |
|----------|--------|------|
| `ArtifactCore/include/Rig/Rig2D.ixx` | ~15行 | キーフレームAPI、演算子、import追加 |
| `ArtifactCore/src/Rig/Rig2D.cppm` | ~70行 | evaluate 実装、演算子実装、poleAngle、JSON |
| `ArtifactCore/src/Collaborate/CollaborationProtocol.cppm` | 2行 | export struct/class |
| `ArtifactCore/include/Network/CollaborationWebSocket.ixx` | ~12行 | sessionId、sendRuleSync、4シグナル |
| `ArtifactCore/src/Network/CollaborationWebSocket.cppm` | ~45行 | セッション管理、rule sync 送受信 |

## 成果物

- `docs/planned/MILESTONE_RIG2D_BONE_KEYFRAME_ANIMATION_2026-07-25.md`
- `docs/planned/MILESTONE_COLLAB_PROTOCOL_EXPORT_SESSION_2026-07-25.md`
