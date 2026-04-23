# Phase 3 実行メモ: API Compatibility Pass

> 2026-04-21 作成

## 目的

既存の呼び出し側を壊さずに、read-only API / getter / import の齟齬を潰す。

この Phase では新しい抽象を増やすより、今ある API の接続ミスを最小差分で直す。

## 対象例

- `ArtifactCore/include/Property/Property.ixx`
- `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- `Artifact/include/Render/ArtifactRenderROI.ixx`
- `ArtifactCore/include/Frame/FrameDebug.ixx`

## 実装タスク

### 1. 参照名の齟齬を埋める

- `getTypeName()` のような存在しないメンバー参照を整理する
- 必要なら `PropertyType` -> string の変換 helper を使う

### 2. import の漏れを埋める

- `ArtifactRenderQueueService` が `SessionLedger` を読めるようにする
- 依存 module を宣言側に明示する

### 3. read-only API を維持する

- `frameDebugSnapshot()` や `sessionLedger()` のような read-only 参照を壊さない
- mutating API はここで増やさない

## 完了条件

- `C2039` / `LNK2019` の再発を抑えられる
- 呼び出し側の修正が局所的に収まる
- build log が「型エラー」よりも、本来の未実装箇所へ寄る

## 次の Phase への橋渡し

Phase 3 が終わると、残りはモジュール境界の局所修正と、必要な箇所だけの互換 wrapper になる。

## File Tickets

- [`docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE3_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE3_EXECUTION_2026-04-21.md)
- `Property.ixx`
- `ArtifactRenderQueueService.ixx`
- `ArtifactRenderROI.ixx`
- `FrameDebug.ixx`
