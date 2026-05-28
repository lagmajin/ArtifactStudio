# Preview / Cache Truth

`requested / ready / failed / inRam / onDisk` の意味を、再生・タイムライン・診断で
同じものとして扱えるようにするためのマイルストーン。

この milestone は、キャッシュ機構そのものを大きく作り替えるものではない。
むしろ、既に散らばっている状態語彙を整理して、
「何が再生可能で、何が単なる要求で、何が失敗なのか」を一本化することが目的。

## Goal

- `requested` と `ready` を混同しない
- `ready` は concrete な画像がある場合だけに限る
- `inRam` と `onDisk` を別状態として保持する
- timeline / footer / app debugger / render controller が同じ真実を読む
- cache が無い時も UI が壊れず、意図が見える

## Scope

- `Artifact/src/Service/ArtifactPlaybackService.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

## Non-Goals

- playback thread model の全面再設計
- full RAM preview build queue の完成
- disk cache manifest の新規設計
- renderer backend の大規模変更
- 新しい global signal/slot architecture の追加

## Background

今の codebase には、すでに preview/cache/state を読むための入口が複数ある。
ただし、それぞれが少しずつ違う言葉で同じ状態を説明しているため、
UI 上では「もう再生できるのか」「単に要求されただけか」「失敗しているのか」が
曖昧になりやすい。

AE 風の作業体験では、この曖昧さが積み重なると、
再生・scrub・diagnostics が食い違って見える。
この milestone は、その食い違いを先に潰すためのもの。

## Current Observed Boundary

重要な前提:

1. `ArtifactPlaybackEngine` は clock / audio driver として動く
2. `ArtifactPlaybackService` は preview state の owner に近い
3. `ArtifactCompositionRenderController` は final frame production 側
4. timeline / footer / debugger は state の consumer

この分離を崩さずに、`ready` の定義だけを厳密化する。

## Proposed Model

- `FramePreviewState`
  - `Empty`
  - `Requested`
  - `Ready`
  - `Failed`
  - `Stale`
- `FramePreviewStorage`
  - `None`
  - `InRam`
  - `OnDisk`
  - `InRamAndOnDisk`
- `FramePreviewReason`
  - `playback-tick`
  - `rendered-preview`
  - `disk-hydrated`
  - `request-queued`
  - `render-failed`
  - `policy-blocked`

## Work Packages

### 1. Readiness Contract

対象:

- `ArtifactPlaybackService`
- `ArtifactCompositionRenderController`
- `ArtifactTimelineWidget`

内容:

- `requested` と `ready` を明確に分ける
- 空の clock tick を ready 扱いしない
- `ramPreviewCacheBitmap()` が何を意味するかを固定する
- `tryGetRamPreviewFrameImage()` を最終的な image authority として扱う

完了条件:

- playback tick だけで cache が ready にならない
- ready と requested が UI で混ざらない

### 2. Storage Visibility

対象:

- `ArtifactPlaybackService`
- `ArtifactCompositionRenderWidget`
- `AppDebuggerWidget`

内容:

- `inRam` と `onDisk` を別表示にする
- one-shot の fallback と persistent cache を区別する
- diagnostics に「なぜ読めたのか」を残す

完了条件:

- RAM にあるのか disk から復元したのかが見える
- ready の理由が追える

### 3. Timeline / Footer Cohesion

対象:

- `ArtifactTimelineWidget`
- `ArtifactCompositionViewerFooter`

内容:

- cache bar / status label / frame summary の語彙を合わせる
- `requested`, `ready`, `failed` を同じ文法で表示する
- 空状態でも「未提供」「未準備」「失敗」の違いを出す

完了条件:

- タイムラインと footer で同じ状態を同じ言葉で説明できる

### 4. Render Policy Notes

対象:

- `ArtifactCompositionRenderController`
- `ArtifactCompositionRenderWidget`

内容:

- viewport interaction 中の fallback 方針を明文化する
- playback 中に RAM fallback を使う条件を明確にする
- policy によって read を止める場合は reason を残す

完了条件:

- 再生可否と cache 可否の差が診断で説明できる

### 5. Empty-State Guidance

対象:

- `ArtifactTimelineWidget`
- `AppDebuggerWidget`
- `FrameDebugViewWidget`

内容:

- cache が無い場合に何をすべきかの案内を短く出す
- requested-only / failed / stale を見分けやすくする
- UI が無言になりすぎないようにする

完了条件:

- 「何も起きていない」のではなく「今は何段階目か」が分かる

## Recommended Order

1. Readiness Contract
2. Storage Visibility
3. Timeline / Footer Cohesion
4. Render Policy Notes
5. Empty-State Guidance

## First Files

1. `Artifact/src/Service/ArtifactPlaybackService.cppm`
2. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
3. `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
4. `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
5. `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`
6. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

## Notes

この milestone は、`MILESTONE_PREVIEW_PLAYBACK_PERFORMANCE_LOW_LEVEL_AI_2026-05-23.md`
の low-level cleanup を土台にして、その上の意味論を揃える位置づけ。

また、`MILESTONE_TIMELINE_SCRUBBAR_FRAME_CACHE_OVERLAY_2026-04-10.md`
の「見える化」とも関係するが、こちらは cache 表示の見た目よりも、
state contract の正しさを主眼に置く。

`preview / timeline / diagnostics` がそれぞれ違う真実を読まないようにするのが、
この milestone の一番大きな価値。
