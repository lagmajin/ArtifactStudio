# Phase 1 実行メモ: Module Boundary Cleanup

> 2026-04-21 作成

## 目的

`Qt` / `STL` / custom module type の見え方を整えて、`C2027` / `C7612` / `C2039` 系のビルド崩れを減らす。

この Phase は「新機能」ではなく「今あるコードを module-safe にする」ことだけをやる。

## 重点対象

- `ArtifactCore/include/Diagnostics/SessionLedger.ixx`
- `ArtifactCore/include/Layer/LayerMatte.ixx`
- `ArtifactCore/include/Property/Property.ixx`
- `ArtifactCore/include/Acoustic/*`

## 方針

1. `module;` の global fragment に必要な include を寄せる
2. `import <...>` の header unit 依存を減らす
3. `QUuid` / `QJsonArray` / `QHash` / `QString` の見え方を安定化する
4. 変換しづらい箇所は、公開 API を壊さない範囲で read-only helper に寄せる

## 実装タスク

### 1. `SessionLedger` のセッション ID 生成を安定化

- `QUuid::createUuid()` のような module 境界で壊れやすい依存を減らす
- `QDateTime` / `QRandomGenerator` ベースの軽い ID 生成にする

### 2. `LayerMatte` の JSON 読み書きを安定化

- `QJsonObject::insert()` を優先する
- `QJsonArray` を module 内で明示的に扱う
- helper の宣言順と定義順を揃える

### 3. `Property` の type-name 参照を実体 API に合わせる

- `AbstractProperty::getTypeName()` のような存在しない参照を消す
- `PropertyType` から文字列化する read-only helper を足す

## 完了条件

- 該当モジュールのビルドエラーが減る
- API の意味が変わらない
- 互換 wrapper なしで参照していた場所は、必要なら local helper へ移す

## File Tickets

- [`docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE1_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE1_EXECUTION_2026-04-21.md)
- `SessionLedger.ixx`
- `LayerMatte.ixx`
- `Property.ixx`
- `Acoustic` 群

## 触らないこと

- property UI の見た目
- layer / matte の挙動
- render path の処理順
