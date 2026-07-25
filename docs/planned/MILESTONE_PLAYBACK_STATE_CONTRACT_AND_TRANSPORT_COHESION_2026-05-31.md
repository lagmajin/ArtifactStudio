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
- [MILESTONE_RAM_PREVIEW_CACHE_PARITY_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_PARITY_2026-05-31.md)

---

## Static audit follow-up (2026-07-25)

現行の playback service と各 surface の参照語彙を確認した。ビルド・実機 UI は未確認。

| Phase | 現状 | 判定 |
|---|---|---|
| 1. State Vocabulary Lock | service 側に `requested/ready/failed`、`ready-missing-image`、`not-requested`、`requested-not-ready` と priority reason の分離がある。 | 基盤実装済み／表示確認待ち |
| 2. Transport Surface Alignment | PlaybackService、PlaybackEngine、CompositionPlaybackController、Timeline/preview の導線は存在する。全 surface が同一 wording と authority を読むことは未確認。 | 部分実装 |
| 3. Secondary Preview Explanation | fallback/status の helper と image availability 判定はある。secondary preview の not-requested/pending/missing-image/composition-mismatch の表示統一は未確認。 | 部分実装／UI確認待ち |

### 現在の判定

state vocabulary の service 基盤は進展しているが、surface cohesion と secondary preview の説明表示が残る。Phase 1 はコード上ほぼ完了、Phase 2〜3 は統合・実行確認待ちとする。
