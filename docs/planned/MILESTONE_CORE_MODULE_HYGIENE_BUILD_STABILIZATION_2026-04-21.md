# マイルストーン: Core Module Hygiene / Build Stabilization

> 2026-04-21 作成

## 目的

ArtifactCore と Artifact の module 境界を安定化し、`std` / Qt / module import の噛み合わせで繰り返し出るビルド崩れを減らす。

このマイルストーンは新機能追加ではなく、既存機能の土台を固めるためのもの。
今まさに出ている `SessionLedger` / `Property` / `LayerMatte` / `ArtifactRenderROI` / `Acoustic` の系統を代表例として扱う。

## 背景

現在のビルド崩れは、機能不足よりも次の問題に集中している。

- `QUuid` / `QJsonArray` / `QHash` など Qt 型の module 境界不整合
- `std::min` / `std::max` / `qreal` / `float` / `int` の型不一致
- `export module` / `module;` / `import <...>` の配置揺れ
- 既存 API の削除ではなく、型名変更や getter 名変更によるリンク不整合

これらは個別修正でも直せるが、同系統が再発しやすいため、共通の衛生ルールと helper を先に整える。

## Goal

- `ArtifactCore` の module で `Qt` / `STL` / custom type の依存形を揃える
- `std::min` / `std::max` の型崩れを減らす共通 helper を用意する
- 文字列化や JSON 化のような read-only surface を、各モジュール内で一貫した書き方に寄せる
- 既存挙動を変えずに build failure を減らす

## Non-Goals

- 新しい UI の追加
- render algorithm の変更
- signal/slot の追加
- 既存 class の責務変更
- 大きなリネームや API の全面置換

## Design Principles

- Build-first
  - まず通る形を作る
- Localized fix
  - 触る範囲を小さく保つ
- Type-stable
  - `float` / `qreal` / `int` の混在を減らす
- Module-safe
  - `module;` / `export module` / `import` / `#include` の境界を明確にする
- Helper over repetition
  - 同じ型吸収を毎回書かず共通化する

## Current Hotspots

- `ArtifactCore/include/Diagnostics/SessionLedger.ixx`
- `ArtifactCore/include/Property/Property.ixx`
- `ArtifactCore/include/Layer/LayerMatte.ixx`
- `Artifact/include/Render/ArtifactRenderROI.ixx`
- `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- `ArtifactCore/include/Utils/Numeric.ixx`
- `ArtifactCore/include/Utils/Utils.ixx`
- `ArtifactCore/include/Utils/NumericLike.ixx`
- `ArtifactCore/include/Frame/FrameDebug.ixx`
- `ArtifactCore/src/Acoustic/*`

## Phase 1: Module Boundary Cleanup

### 目的

Qt 型や STL 型が module 境界で見えなくなる問題を減らす。

### やること

- `module;` の global fragment に必要な include を寄せる
- `import <...>` で不安定な header unit 依存を減らす
- `export module` の前後で役割を揃える
- `SessionLedger` / `LayerMatte` / `Acoustic` のような頻出モジュールを優先する

### 完了条件

- 同系統の `C2027` / `C7612` / `C2039` が減る
- `QUuid` / `QJsonArray` / `QHash` の見え方が安定する

## Phase 2: Numeric Helper Consolidation

### 目的

`std::min` / `std::max` の型不一致を共通 helper で吸収する。

### やること

- `Utils.Numeric` を追加する
- `min_same()` / `max_same()` を導入する
- `qreal` / `float` / `int` / `size_t` 混在箇所を徐々に置換する

### 完了条件

- 代表的な `std::min` エラーが減る
- `ArtifactRenderROI` のような geometry 系で型指定の重複が減る

## Phase 3: API Compatibility Pass

### 目的

既存クラスの read-only API を壊さずに、リンク不整合を潰す。

### やること

- getter 名の変更や追加があれば互換 wrapper を残す
- `Property` のように、実体クラスにないメンバー参照を変換する
- `ArtifactRenderQueueService` の session ledger 参照など、import 漏れを埋める

### 完了条件

- `C2039` / `LNK2019` の再発が抑えられる
- 既存呼び出し側の修正が最小限で済む

## Suggested Execution Order

1. `ArtifactCore/include/Diagnostics/SessionLedger.ixx`
2. `ArtifactCore/include/Layer/LayerMatte.ixx`
3. `ArtifactCore/include/Property/Property.ixx`
4. `Artifact/include/Render/ArtifactRenderROI.ixx`
5. `Artifact/include/Render/ArtifactRenderQueueService.ixx`
6. `ArtifactCore/include/Utils/Numeric.ixx`

## Related Docs

- [`docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md`](X:/Dev/ArtifactStudio/docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md)
- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
- [`docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE1_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE1_EXECUTION_2026-04-21.md)
