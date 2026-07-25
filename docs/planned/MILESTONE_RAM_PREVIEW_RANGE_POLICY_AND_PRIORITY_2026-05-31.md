# Milestone: RAM Preview Range Policy and Priority

> 2026-05-31

## Purpose

`RAM preview` を「たまたま当たる cache」ではなく、「いまどの frame を先に温めるべきか」が説明できる preview system に寄せる。

この milestone は cache 容量の拡大ではなく、`playhead / work area / playback direction / current composition` をもとにした range policy と priority order を固定するための整理である。

## Why This Exists

- [MILESTONE_PLAYBACK_STATE_CONTRACT_AND_TRANSPORT_COHESION_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_PLAYBACK_STATE_CONTRACT_AND_TRANSPORT_COHESION_2026-05-31.md) で state wording は揃え始めたが、`次にどの frame を作るか` の規則はまだ surface の外に散っている
- [MILESTONE_RAM_PREVIEW_CACHE_PARITY_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_PARITY_2026-05-31.md) で `requested / ready / failed` は追いやすくなったが、request order が曖昧なままだと体感品質が安定しない
- AE parity の観点でも、`playhead 近傍がすぐ出る`、`再生方向に先回りする`、`停止時は編集周辺を厚くする` という priority policy がないと、cache が存在していても制作感が弱い

## Current Findings

### 1. Request State Exists, But Request Order Is Implicit

- `ArtifactPlaybackService` は frame state を持てる
- ただし `which frame should be requested next` を 1 か所で説明できる policy がまだ薄い

### 2. Playback And Pause Want Different Warmup Shapes

- 再生中は `forward-biased` でないと気持ちよく進まない
- 停止中の編集は `playhead centered` の対称 warmup のほうが有利

### 3. Work Area And Composition Bounds Need To Matter

- `work area` 外まで均等に作ると、体感上ほしいところに cache が集まりにくい
- comp 先頭末尾や loop 区間の扱いも policy で固定する必要がある

## In Scope

- RAM preview request priority policy
- playhead 周辺の warmup range 定義
- playback / pause / scrub での priority 切り替え
- work area / loop / composition bound の扱い
- diagnostics に出す priority reason

## Out of Scope

- disk cache の全面再設計
- decoder 最適化単独
- render backend の刷新
- 新しい timeline UI の大規模追加

## Recommended Start Order

### Phase 1: Priority Vocabulary Lock

- `immediate / near / forward / backward / work-area / out-of-range` の priority vocabulary を固定する
- service 側で `why this frame now` を返せるようにする
- debugger / timeline tooltip で同じ語彙を読めるようにする

### Phase 2: Playback Direction Bias

- 再生中は `playhead -> forward band -> backward safety band` の順で priority を付ける
- 逆再生時は同じ policy を左右反転して扱う
- loop playback 時は loop end / start のまたぎを例外ではなく正式 policy にする

### Phase 3: Pause And Edit Warmup

- 停止中は `current frame` 最優先、その次に `near band` を左右対称で温める
- scrub 中は `current scrub direction` を少し優先するが、停止復帰時は centered policy に戻す

### Phase 4: Work Area And Bounds Contract

- work area がある場合は、その内側を優先し、外側は低 priority に落とす
- comp 範囲外要求は `out-of-range` として明示する
- diagnostics 上で `work-area` と `out-of-range` を分けて見せる

## Suggested Policy Shape

1. `Immediate`
   - 現在 frame
   - 表示中 surface が次に必要とする frame

2. `Near`
   - playhead 前後の狭い帯
   - 停止時の編集レスポンスを支える範囲

3. `Directional`
   - 再生方向に向いた前方帯
   - reverse playback では逆方向

4. `Safety Backfill`
   - 反対側の薄い帯
   - loop や微小 scrub の戻りを支える範囲

5. `Work Area Preferred`
   - work area 内側を優先する

6. `Out Of Range`
   - comp 範囲外、または policy 上の対象外

## Success Criteria

- `なぜこの frame がまだ無いのか` を priority reason で説明できる
- 再生中と停止中で warmup の振る舞い差が意図どおりになる
- work area を設定したとき、体感上ほしい frame に cache が寄る
- diagnostics / timeline / preview surface が同じ priority truth を読む

## Likely Touch Points

- [ArtifactPlaybackService.cppm](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [ArtifactTimelineWidget.cpp](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactTimelineWidget.cpp)
- [AppDebuggerWidget.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm)

## Related

- [MILESTONE_PLAYBACK_STATE_CONTRACT_AND_TRANSPORT_COHESION_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_PLAYBACK_STATE_CONTRACT_AND_TRANSPORT_COHESION_2026-05-31.md)
- [MILESTONE_RAM_PREVIEW_CACHE_PARITY_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_PARITY_2026-05-31.md)
- Phase 1 は本書へ統合済み
- [AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md)

## Static Audit (2026-07-25)

Phase 1〜4 の policy 部分はサービス実装に具体化されている。`ArtifactPlaybackService` は `ramPreviewRange`、radius、work-area／composition bounds、requested／ready／failed／on-disk state、generation 付き build queue、cancel／invalidate を保持する。`ramPreviewPriorityState()` は `immediate`、`near`、`forward`、`backward`、`work-area`、`out-of-range` を判定し、`ramPreviewPriorityReason()` と summary へ同じ理由を返す。View Menu は current frame の state と not-ready reason を読み、サービス API には cache bitmap、hit rate、pending、build progress、clear／build／stop 相当の導線がある。

ただし、静的確認では playback direction の反転、loop 境界での優先順位、scrub 中の policy 切替、Timeline／Debugger／Preview surface が同じ priority truth を読むことまでは証明できない。さらに前段のRAM Cache監査で確認したとおり、汎用 `FrameCache` はPlaybackServiceの主要経路へ接続されていないため、priority queue が実際の連続再生品質へ反映されることも未検証。Phase 1 は実装済み、Phase 2〜4 は基盤実装済み／実再生・surface統合確認待ちとする。

確認対象:

- `Artifact/include/Service/ArtifactPlaybackService.ixx`
- `Artifact/src/Service/ArtifactPlaybackService.cppm`
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`
