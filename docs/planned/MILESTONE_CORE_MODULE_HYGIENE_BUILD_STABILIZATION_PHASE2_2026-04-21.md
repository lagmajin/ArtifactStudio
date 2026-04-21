# Phase 2 実行メモ: Numeric Helper Consolidation

> 2026-04-21 作成

## 目的

`std::min` / `std::max` の型不一致を共通 helper で減らす。

`qreal` / `float` / `int` / `size_t` が混ざる箇所は多く、個別に直しても再発しやすいので、ここで共通化する。

## 追加するもの

- `ArtifactCore/include/Utils/Numeric.ixx`
- `Utils.Numeric` の export
- `min_same()`
- `max_same()`

## 使い方の想定

- `QRectF` / `qreal` / `float` の混在
- `size_t` / `int` の比較
- `std::min` / `std::max` の毎回の明示 template 引数指定の削減

## 実装タスク

### 1. 共通 helper を置く

- `std::common_type_t` を使って型を寄せる
- `ArtifactCore` namespace で使えるようにする

### 2. まずは geometry 系に当てる

- `Artifact/include/Render/ArtifactRenderROI.ixx`
- `ArtifactCore/src/Geometry/*`
- `ArtifactCore/src/Text/*`

### 3. その後、頻出箇所に広げる

- `MediaPlaybackController`
- `PerformanceProfiler`
- `AI` / `Codec` 系の数値比較

## 完了条件

- `std::min` の型崩れが減る
- `qreal` / `float` の明示変換が局所化される
- helper が増えすぎない

## 注意

- なんでも helper に逃がしすぎない
- 単位が違う値まで雑に比較しない

## File Tickets

- [`docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE2_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE2_EXECUTION_2026-04-21.md)
- `Utils.Numeric`
- `ArtifactRenderROI.ixx`
- numeric comparison sites
