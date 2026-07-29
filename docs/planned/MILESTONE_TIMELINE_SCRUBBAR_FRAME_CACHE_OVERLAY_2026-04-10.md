# Timeline Scrub Bar Frame Cache Overlay

AE 風の RAM preview に寄せて、`ArtifactTimelineScrubBar` 上に
「どこまでフレームキャッシュ済みか」を緑色の帯で可視化するための
マイルストーン。

## Goal

- スクラブバー上で、キャッシュ済みフレーム範囲をひと目で分かるようにする
- 現在フレームの赤い進捗と、キャッシュ済み範囲の緑を同じ surface 上で共存させる
- 単一の連続帯だけでなく、分断された cache range も表現できるようにする
- `playback / scrub / seek` の状態を見ながら、再生可能範囲を直感的に把握できるようにする

## Scope

- `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`
- `Artifact/include/Widgets/Timeline/ArtifactTimelineScrubBar.ixx`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `ArtifactCore/include/Frame/FrameCache` 相当の状態共有がある場合はその接続部

## Non-Goals

- キャッシュ生成アルゴリズムそのものの再設計
- GPU texture cache の全面見直し
- disk cache の新規導入
- 再生エンジンの codec / decode policy の再構築

## Background

現状のスクラブバーは、現在フレームを示す進捗表示と、再生位置の操作面としては機能している。
ただし AE で見慣れたような「もう再生できる範囲」が見えないため、
キャッシュが効いているのか、まだ待ちなのかが UI から判断しづらい。

この milestone では、`ScrubBar` を単なるシーク UI ではなく、
`cache availability indicator` としても扱う。

## Proposed Model

- `FrameCacheRange`
  - 連続したキャッシュ済みフレーム区間
- `FrameCacheState`
  - `Empty`, `Preloading`, `Partial`, `Ready`, `Stale`
- `FrameCacheOverlayStyle`
  - `GreenFill`, `StripedFill`, `GlowEdge`, `Minimal`

## Definition Of Done

- スクラブバーに緑色の cache range が描画される
- キャッシュが分断されていても複数区間として描ける
- 現在フレームの赤い進捗表示と視認的に衝突しない
- 再生中 / 停止中 / scrub 中で、表示の意味が崩れない
- キャッシュ情報がない場合でも UI が壊れず、従来表示へ自然にフォールバックする

## Work Packages

### 1. Cache Range Contract

対象:

- `ArtifactPlaybackEngine`
- `ArtifactCompositionRenderController`
- `ArtifactTimelineWidget`

内容:

- cache 済み frame range を UI に渡す最小データ構造を決める
- 単一 range と複数 range の両方を扱えるようにする
- cache 情報が取れない backend では空状態として扱う

完了条件:

- `ScrubBar` が「どこを緑に塗るか」を受け取れる

### 2. Scrub Bar Overlay Rendering

対象:

- `ArtifactTimelineScrubBar.cppm`

内容:

- 既存の赤い current frame 表示の上または下に cache overlay を描く
- 緑の fill / 薄い stripes / 端の glow を選べるようにする
- current frame の赤が視認性を失わないようにレイヤー順を整理する

完了条件:

- cache range が視覚的に分かる
- current frame の操作感を損なわない

### 3. Playback / Cache Bridge

対象:

- `ArtifactPlaybackEngine`
- `ArtifactCompositionRenderController`

内容:

- cache warm / fill / stale の変化をスクラブバーへ通知する
- play / pause / seek / loop で cache range の見え方が変わるなら同期する
- RAM preview などの将来機能へつなげやすい形にする

完了条件:

- 再生状態に応じて cache 表示が更新される

### 4. Diagnostics / Empty State

対象:

- `ArtifactTimelineWidget`
- status / debug 表示

内容:

- cache 情報がないときに「未提供」であることが分かるようにする
- 診断用に cache hit / warm / stale の状態を確認しやすくする
- UI が詰まったり、スクラブバーの操作領域を壊したりしない

完了条件:

- cache が無くても UI が破綻しない
- 問題があるときに原因を追いやすい

## Recommended Order

1. Cache Range Contract

---

## 2026-07-25 現状確認

主要部分は実装済み。`ArtifactTimelineScrubBar` には単一の cached range と、ready / failed / on-disk の frame bitmap を受け取る API があり、分断された区間も連続 run として描画する。緑の RAM cache、青の on-disk、赤の failed 表示を current frame の進捗表示と同じ surface 上で重ね、frame count、ruler zoom、scrub／seek の visual frame と同期している。`ArtifactTimelineWidget` は playback の RAM preview cache bitmap と frame state を収集して scrubbar へ渡す経路を持つ。

未完了・未確認:

- playback の warm / stale / 再生・停止・loop 変化に対する実機回帰
- overlay style の選択や glow / striped 表現の設定化
- 大量フレーム、分断区間、長時間 RAM preview での描画負荷確認

実装済み（静的確認）:

- 表示側の `totalFrames` と供給 bitmap の長さがずれた場合も、レール外へ描画しない境界処理
- cache range / bitmap が空の場合の Accessible Description と `RAM Cache: unavailable` / `RAM Cache: errors` 表示
- マウスの interactive seeking が無効な表示専用状態でも、フォーカス移動後に Left/Right、PageUp/PageDown、Home/End でキーボードシーク
- `frameChanged` から既存の Timeline `applyTimelineSeek` 経路への同期

したがって「Cache Range Contract、Overlay Rendering、Diagnostics の静的実装は完了。実機回帰と負荷検証は未完了」と整理する。
2. Scrub Bar Overlay Rendering
3. Playback / Cache Bridge
4. Diagnostics / Empty State

## Notes

この milestone は `MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md` の
「再生のためのキャッシュ基盤」とは別に、
「見える化」を最初に仕上げるためのもの。

先に UI surface を作っておくと、
RAM preview / frame cache / decode prewarm の実装が後から来ても、
どこを緑に塗ればよいかがぶれにくい。

---

## Next Execution Slice

Phase 1 は、ScrubBar が cache range を受け取る契約を先に固める。

### Phase 1A の着手点

1. `FrameCacheRange` を単一区間と複数区間の両方に対応させる
2. `FrameCacheState` は `Empty / Preloading / Partial / Ready / Stale` に絞る
3. cache 情報が取れない backend は空状態として扱う
4. `ArtifactPlaybackEngine` から UI へ渡す最小データを決める

### Phase 1 完了条件

- `ScrubBar` が「どこを緑に塗るか」を受け取れる
- 単一 range と複数 range の両方を扱える
- cache 情報がない場合でも空状態で破綻しない

### Phase 2A の着手点

1. current frame の赤い進捗表示と cache overlay のレイヤー順を決める
2. `GreenFill / StripedFill / GlowEdge / Minimal` の使い分けを先に固定する
3. 分断された cache range が見える表示を作る
4. play / pause / scrub の状態と視認性の衝突を避ける

### Phase 2 完了条件

- cache range が視覚的に分かる
- current frame の操作感を損なわない
- 分断された cache range も読める

### Phase 3 への前提

- cache warm / fill / stale の変化は contract が固まってから bridge へ流す
- diagnostics は overlay が安定してから足す
