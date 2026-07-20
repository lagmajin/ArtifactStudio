# OpenTimelineIO Adapter Progress

> 2026-07-20

## 到達点

`ArtifactCore` に独立した `NLE.OTIO` module を追加し、Artifact の NLE 正規データを OTIO 1.x JSON へ変換する adapter の基礎を作成した。

- `Timeline / Stack / Track / Clip / ExternalReference`
- Track 間の `Gap`
- Sequence `Marker`
- 基本的な `Crossfade / Dissolve / Cut` transition
- Artifact 独自 ID と一部状態を `metadata` に保存
- OTIO JSON から `NLEProjectStore` への import

## 未対応・保留

- AAF / FCPXML / EDL の実ファイル adapter
- Nested Sequence
- OTIO の serializable object library との直接リンク
- Effect、Mask、Track Matte、Keyframe の完全変換
- source registry の重複排除と relink policy
- import/export の自動テスト
- UI の import/export command

## 方針

OTIO は Artifact の正規保存形式にはしない。Artifact JSON を完全形式として維持し、OTIO は編集タイムライン交換形式として扱う。

実装コミット:

- `ArtifactCore`: `2897ab1`, `02e5d07`, `1279e3f`, `91a1bf1`
- 親 gitlink: `b5a61c9`

ビルド・テストは未実行。
