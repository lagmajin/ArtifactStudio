# Milestone: Playback State Contract and Transport Cohesion

> 2026-05-31

## Purpose

`ArtifactPlaybackService` を state authority としたまま、`Composition Editor / Timeline / footer / render surface / debugger / secondary preview` が同じ playback truth を読むように揃える。

この milestone は、新しい playback 機能を増やすためではなく、アプリ全体で `playable / requested / ready / failed / fallback` の意味を揃えるための横断整理である。

## Why This Exists

- `docs/WIDGET_MAP.md` では `ArtifactCompositionEditor` が transport context router として定義されている
- 実装上は `ArtifactTimelineWidget`、`ArtifactCompositionViewerFooter`、`ArtifactCompositionRenderWidget`、`AppDebuggerWidget`、`ArtifactViewMenu` の secondary preview が、それぞれ playback state を別の粒度で読んでいる
- `RAM preview` は cache だけの問題ではなく、transport と diagnostics の語彙の問題でもある

## Current Findings

### 1. State Contract Is Improving, But Still Surface-Biased

- `ArtifactPlaybackService` に `requested / ready / failed` と image fetch の土台がある
- ただし surface ごとに `ready` の見せ方がズレやすく、表示と実体が完全には一致していない

### 2. Transport Is Functionally Shared, But Presentation Is Still Split

- play / stop / current composition の authority は playback service 側に寄っている
- 一方で timeline / footer / render fallback / secondary preview が、それぞれ別の wording と fallback 表現を持っている

### 3. Secondary Preview Is Under-Specified

- secondary preview は preview image が取れないと blank に落ちる
- `why blank` が surface 上で説明されないため、cache miss / not requested / ready missing image / composition mismatch の違いが読めない

## In Scope

- playback state contract wording
- transport routing wording
- footer / timeline / debugger / secondary preview の status alignment
- fallback reason の surface 表示

## Out of Scope

- playback engine の全面再設計
- render backend の刷新
- shortcut system 全体の作り直し

## Recommended Start Order

### Phase 1: State Vocabulary Lock

- `playable / requested / ready / failed / pending / ready-missing-image` を固定する
- service / timeline / footer / debugger の文言を合わせる

### Phase 2: Transport Surface Alignment

- `Composition Editor` を transport router として文書と実装の両方で揃える
- timeline / footer / editor shell の play / stop / current composition の見え方を揃える

### Phase 3: Secondary Preview Explanation

- preview image が出ない時に `blank` ではなく `reason` を返せるようにする
- at least `not requested / pending / ready missing image / composition mismatch` を区別する

## Success Criteria

- app 全体で playback wording が読み直し不要になる
- `RAM preview broken?` に対して surface ごとの差で混乱しにくくなる
- secondary preview が silent blank ではなく状態説明を持つ

## Related

- [MILESTONE_APP_SURFACE_COHESION_2026-05-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)
- [MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md)
- [MILESTONE_RAM_PREVIEW_CACHE_PARITY_EXECUTION_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_PARITY_EXECUTION_2026-05-31.md)

