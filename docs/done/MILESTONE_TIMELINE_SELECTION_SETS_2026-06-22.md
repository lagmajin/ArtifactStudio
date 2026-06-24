# MILESTONE: Timeline Selection Sets

> 2026-06-22 作成 / 2026-06-24 完了（実装済み確認）

## Goal

選択中のレイヤー群やタイムライン上の位置を保存・復帰できるようにする。— ✅ 完了

## 確認

- `SelectionSetEntry` / `SelectionSetStore` (CBOR永続化) — `ArtifactViewMenu.cppm` に実装済み
- View > Selection セット メニュー → 保存・名前付き呼び出し・削除
- Composition 単位で分離（composition ID がキー）
- 選択保存時には frame position も保存
