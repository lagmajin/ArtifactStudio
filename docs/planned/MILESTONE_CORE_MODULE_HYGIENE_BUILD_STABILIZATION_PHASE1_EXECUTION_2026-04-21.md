# Phase 1 実行メモ: Module Boundary Cleanup - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE1_2026-04-21.md` の内容を、実装にそのまま切れるファイル別チケットへ落とす。

この Phase は build stability を優先し、機能追加はしない。

## チケット 1: `SessionLedger.ixx`

対象:
- [`ArtifactCore/include/Diagnostics/SessionLedger.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Diagnostics/SessionLedger.ixx)

やること:
- `QUuid` 依存を module 境界で壊れにくい生成方式へ寄せる
- `sessionId_` の生成を read-only helper に切る
- `QDateTime` / `QRandomGenerator` の include を global fragment に置く

完了条件:
- `SessionLedger` が `QUuid` を直接要求しない
- セッション ID が生成できる

## チケット 2: `LayerMatte.ixx`

対象:
- [`ArtifactCore/include/Layer/LayerMatte.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Layer/LayerMatte.ixx)

やること:
- `QJsonObject` / `QJsonArray` の読み書きを `insert()` ベースへ寄せる
- `matteStackModeFromString()` の宣言順を安定化する
- `toJson()` / `fromJson()` を module-safe な形に整える

完了条件:
- `QJsonArray` / `QJsonObject` の未定義エラーが減る
- `matteStackModeFromString()` の参照が解決する

## チケット 3: `Property.ixx`

対象:
- [`ArtifactCore/include/Property/Property.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/Property.ixx)

やること:
- 存在しない `AbstractProperty::getTypeName()` 参照を消す
- `PropertyType` から string へ変換する helper を使う
- read-only snapshot の propertyType 表示を安定化する

完了条件:
- `Property` の compile error が消える
- read-only snapshot が type name を表示できる

## チケット 4: `Acoustic` 群

対象:
- [`ArtifactCore/src/Acoustic/Acoustic.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/src/Acoustic/Acoustic.ixx)
- [`ArtifactCore/src/Acoustic/AcousticSystem.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/src/Acoustic/AcousticSystem.ixx)
- [`ArtifactCore/src/Acoustic/ModalResonator.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Acoustic/ModalResonator.cppm)
- [`ArtifactCore/src/Acoustic/SpatialSystem.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Acoustic/SpatialSystem.cppm)
- [`ArtifactCore/src/Acoustic/FrictionModel.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Acoustic/FrictionModel.cppm)
- [`ArtifactCore/src/Acoustic/WindModel.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Acoustic/WindModel.cppm)
- [`ArtifactCore/src/Acoustic/RainModel.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Acoustic/RainModel.cppm)

やること:
- `module;` と `#include` の位置を揃える
- `import <...>` を減らす
- `AudioTask` 初期化の引数並びを揃える
- `std::uint32_t` / `std::uint64_t` を明示する

完了条件:
- `C5050` / `C7612` / `C2397` 系が減る
- `Acoustic` 配下の module ルールが揃う

## 実装順

1. `SessionLedger.ixx`
2. `LayerMatte.ixx`
3. `Property.ixx`
4. `Acoustic` 群

