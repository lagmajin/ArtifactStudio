# Phase 2 実行メモ: Numeric Helper Consolidation - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE2_2026-04-21.md` を、実装にそのまま切れるファイル別チケットへ落とす。

## チケット 1: `Utils.Numeric`

対象:
- [`ArtifactCore/include/Utils/Numeric.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Utils/Numeric.ixx)
- [`ArtifactCore/include/Utils/Utils.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Utils/Utils.ixx)

やること:
- `min_same()` / `max_same()` を提供する
- `ArtifactCore` namespace から使えるように export する

完了条件:
- 共通 helper が 1 箇所に定義される

## チケット 2: `ArtifactRenderROI.ixx`

対象:
- [`Artifact/include/Render/ArtifactRenderROI.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderROI.ixx)

やること:
- `QRectF` / `qreal` / `float` 混在箇所を helper に寄せる
- `std::min` / `std::max` の型崩れを抑える
- `TileGrid` / `DirtyRegion` の numeric 比較を整理する

完了条件:
- `std::min` の曖昧さが減る
- geometry 系の意図が読みやすくなる

## チケット 3: 頻出 numeric comparison sites

対象候補:
- [`ArtifactCore/src/Media/MediaPlaybackController.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Media/MediaPlaybackController.cppm)
- [`ArtifactCore/src/Geometry/Fracture.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Geometry/Fracture.cppm)
- [`ArtifactCore/include/Utils/PerformanceProfiler.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Utils/PerformanceProfiler.ixx)
- [`ArtifactCore/src/Text/GlyphLayout.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Text/GlyphLayout.cppm)

やること:
- `std::min` / `std::max` が頻出する箇所だけ helper へ寄せる
- helper の乱用は避ける

完了条件:
- 同系統の compile error が減る
- numeric helper が広がりすぎない

## 実装順

1. `Utils.Numeric`
2. `ArtifactRenderROI.ixx`
3. 頻出 numeric comparison sites

