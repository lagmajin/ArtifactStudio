# MILESTONE: Template Locking

日付: 2026-06-07

テンプレートの壊してはいけない部分をロックし、編集可能な項目だけを安全に差し替えられるようにする。

## Goal

レイアウト制約、Export 設定、重要なコンポ構造を保護しつつ、テキスト、画像スロット、色テーマだけを編集可能にする。

## Non-Goals

- 既存テンプレート形式を全面置換しない
- レイアウトの意味を壊すような自動修正はしない
- Diligent / D3D12 backend には触らない
- 新規の global signal-slot 経路を増やさない

## Core Concept

- `TemplateLock`
  - どの領域や設定を保護するか
- `ProtectedTemplateRegion`
  - 壊してはいけない部分
- `EditableField`
  - 差し替え可能な項目

## Locked Areas

- layout constraints
- export settings
- important composition structure

## Editable Areas

- text
- image slots
- color theme

## Why It Matters

- チーム制作でテンプレートの破壊を防げる
- 量産時に安全に差し替えられる
- 編集可能部分と保護部分が明確になる
- `Motion Tokens` や `Export Matrix` と組み合わせた運用に向く

## Phase 1: Lock Schema

目的: 何を保護するかを明示する。

- protected region の種類を定義する
- editability を項目ごとに分ける
- テンプレート保存時に保持できるようにする

確認観点:

- ロック対象が分かりやすい
- 既存テンプレートが壊れない
- 編集可能項目だけが差し替えられる

## Phase 2: Enforcement

目的: ロックを編集 UI と export 前処理で守る。

- ロックされた領域の変更を拒否する
- 編集可能な項目だけ変更を許可する
- 必要なら warning を出す

確認観点:

- 誤編集を防げる
- 自動生成や一括差し替えでも守られる
- 破壊的変更が通らない

## Phase 3: Team Workflow

目的: 共同制作や量産で使いやすくする。

- locked template の共有
- editable fields の一覧化
- テンプレート運用時の確認しやすさ

確認観点:

- 誰が見ても保護範囲が分かる
- 差し替え担当が迷わない
- 量産フローに組み込みやすい

## Implementation Notes

### 実装済み (Phase 1 + Phase 2 一部)

- `Composition.TemplateLock` — コア型
  - `LockScope` enum: Layout / ExportSettings / CompositionStructure / LayerProperties / Effects / All
  - `Editability` enum: Locked / Editable / EditableWithWarning
  - `ProtectedRegion` struct: id / displayName / scope / editability / description (JSON 対応)
  - `EditableField` struct: fieldId / displayName / slotId / allowedValueType / description (JSON 対応)
  - `TemplateLockSchema` class: templateId / templateName / protectedRegions / editableFields / isEnabled
    - JSON serialization/deserialization
    - `isFieldEditable()`, `isRegionLocked()`, `getEditableFieldsForSlot()`, `editabilityForField()`

- **TemplateSlot enhancement** (`Composition.TemplateSlot`):
  - `TemplateSlot` に `lockScope` (LockScope) と `editability` (Editability) フィールドを追加
  - JSON シリアライズ対応 (`toJson` / `fromJson`)

### ファイル一覧

| ファイル | モジュール |
|---|---|
| `ArtifactCore/include/Composition/TemplateLock.ixx` | `Composition.TemplateLock` |
| `ArtifactCore/src/Composition/TemplateLock.cppm` | (実装) |
| `ArtifactCore/include/Composition/TemplateSlot.ixx` | `Composition.TemplateSlot` (変更) |
| `ArtifactCore/src/Composition/TemplateSlot.cppm` | (変更) |

### 追加実装
- **TemplateEditGuard** (`Composition.TemplateLock`): `canEditField()`、`canModifyParameter()`、`lockReason()`、`getAccessibleFields()`
- **TemplateLockEditorWidget** (`Artifact.Widgets.TemplateLockEditor`): テンプレートロックスキーマの編集 UI
  - 保護領域 (ProtectedRegion) の追加/削除/表示
  - 編集可能フィールド (EditableField) の追加/削除/表示
  - JSON エクスポート/インポート
  - `Artifact/include/Widgets/TemplateLockEditorWidget.ixx`
  - `Artifact/src/Widgets/TemplateLockEditorWidget.cppm`
- **Export preflight チェック** (`Artifact.Render.Queue.Service`): `appendTemplateLockDiagnostics()` で protected region の警告と editable fields の情報を preflight 結果に追加

### 未着手
- Composition property editor でのフィールドレベルのロック強制 (TemplateEditGuard の呼び出し統合)

## Integration Notes

- `Smart Fallbacks` と組み合わせると missing asset 時の安全性が上がる
- `Coordinate Profiles` と組み合わせるとレイアウト制約を扱いやすい
- `Content Bounds System` と組み合わせると保護範囲の意味が明確になる
