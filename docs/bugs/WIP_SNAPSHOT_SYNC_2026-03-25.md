# WIP Snapshot Sync Memo (2026-03-25)

2026-03-25 時点の作業中変更は、`master` へ直接入れず、以下の snapshot branch へ退避済み。

## Snapshot Branches

- `ArtifactStudio`: `wip/ai-sync-20260325`
- `Artifact`: `wip/ai-sync-20260325`
- `ArtifactCore`: `wip/ai-sync-20260325`
- `ArtifactWidgets`: `wip/ai-sync-20260325`

## Current Status

この snapshot には:

- 今回の Composition View / FFmpeg bridge / Playback / milestone docs 変更
- 他の AI による未整理 WIP

が同時に入っている。

そのため、`master` へは snapshot をそのまま merge せず、内容を分離して扱う。

## Safer Integration Order

### 1. Docs Only

先に寄せやすいもの:

- root `docs/bugs/*`
- root `docs/planned/*`
- `Artifact/docs/MILESTONE_*`

理由:

- 実コードと依存が薄い
- 他の変更と競合しにくい

### 2. Small Fixes

次に寄せやすいもの:

- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`

理由:

- 影響範囲が比較的小さい
- 症状と修正内容が明確

注意:

- 同じファイルに他変更が混ざっているため、commit 単位ではなく hunk 単位で抽出すること

### 3. Composition View Perf Work

後で分離して扱うもの:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`

理由:

- 同ファイルに別件の大きい変更が混在
- そのまま切ると他の AI の WIP を巻き込みやすい

### 4. Large Feature Bundles

最後に個別検討するもの:

- Particle 3D 関連
- PrimitiveRenderer3D
- SVG layer
- Audio / WASAPI / fracture / source abstraction など大きな追加物

理由:

- スコープが広い
- repo またぎの整合確認が必要

## Excluded From Snapshot Push

今回 intentionally 除外したもの:

- `third_party/Qt-Advanced-Docking-System`
- `nul`

## Recommended Next Step

`master` へ寄せる作業は次の順が安全。

1. docs-only branch を切って整理
2. small fixes branch を切って `PlaybackEngine` と `FFmpeg bridge` を抽出
3. composition perf branch を別に切ってレビュー前提で扱う
