# マイルストーン: Source Relocation Contract（Image / Image Sequence）

**最終更新:** 2026-08-21
**ステータス:** Not Started
**優先度:** High
**識別子:** M-SRC-RELOC-1
**関連:** `docs/planned/MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`, `docs/planned/MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md`, `docs/planned/MILESTONE_ASSET_BROWSER_RELINK_WORKFLOW_2026-06-28.md`

## 背景

静止画と連番画像の保存・再読込経路には、project root、相対パス、Asset ID、絶対パス fallback、relink の責務が複数箇所に分散している。現行コードには `*Relative` 保存項目や relocation 復旧の実装がある一方、Image Layer、Image Sequence、AssetManager、Project importer の間で「どの候補をどの順序で採用するか」を一つの契約として固定した文書と受入ケースが不足している。

このマイルストーンは新しい画像デコード機能を追加せず、既存の source identity と保存経路を、project移動・source移動・relink後も同じ結果へ到達できる境界契約へ整理する。

## 目的

projectを別ディレクトリへ移動した場合でも、静止画・連番画像の source を安全に再解決し、missing／relink／localized の状態を失わずに編集・保存・再読込できるようにする。

## 解決優先順位（提案契約）

1. 保存済み Asset ID と project内 registry の一致
2. project root 基準の保存済み relative path
3. source registry の relative path
4. 保存済み absolute path（互換 fallback）
5. ユーザー指定の relink 候補
6. いずれも解決できない場合は missing state として source identity／metadata／相対候補を保持

同一候補が複数経路から得られた場合は、正規化した絶対 path と filesystem identity を比較し、重複登録を作らない。解決に成功しても、ユーザー操作なしに source path を別候補へ書き換えて dirty 化しない。

## 対象範囲

- `ArtifactImageLayer` の単一 source
- `ImageSequenceSource` の sequence paths、frame range、欠番候補
- `ArtifactProjectManager` / project exporter・importer の root／relative path 境界
- `AssetManager` の registry、Asset ID、missing／relink状態
- Asset Browser の relink 結果と layer／sequence 参照の同期
- 保存／再読込後の source metadata、color interpretation、source version の保持

## 対象外

- 動画デコード・動画再生の新規対応
- 新しい画像フォーマットやOIIO decode機能
- Asset Browser UIの大規模再設計
- `ReactiveEvents` の変更
- GPU／Diligent backendの構造変更

## 実装フェーズ

### Phase 0 — 契約と状態モデル

- source resolution候補、採用理由、missing理由を共通の診断表現へ整理する。
- Image／Sequenceで共通の保存キーと、既存JSONの互換読込を棚卸しする。
- Asset ID、relative path、absolute fallbackの優先順位を仕様化する。

### Phase 1 — Project境界の統一

- project root設定時の正規化、相対化、root変更時の再解決責務を一箇所へ寄せる。
- exporter／importer、composition layer、source registryで同じ relative path policyを使う。
- project移動後に absolute pathだけへ戻る経路を検出する。

### Phase 2 — Image／Sequence適用

- 単一画像と連番画像で同じ resolution result を layerへ適用する。
- 欠番、missing、relink成功、source更新時の generation／cache invalidation を保持する。
- 同一sourceの複数参照で Asset ID と decoded cache を誤って複製しない。

### Phase 3 — 受入マトリクス

- project移動、source移動、相対候補のみ、absolute fallbackのみ、Asset ID欠落、relink、missing復帰を固定ケース化する。
- 保存前後で layer path、sequence paths、metadata、color interpretation、source versionを比較する。
- 実素材確認は既存の静止画受入マトリクスへ接続し、Preview／Render Queueの受入と重複させない。

## 完了条件

- [ ] Image／Sequenceのsource解決順位が文書・実装・診断で一致する。
- [ ] project移動後、relative pathまたはAsset IDから自動復旧できる。
- [ ] absolute path fallbackは互換用途として残るが、正常時の第一候補にならない。
- [ ] missing状態でもsource identity、metadata、relative候補が保持される。
- [ ] relink成功後に参照layer／sequence、Asset registry、cache generationが一貫して更新される。
- [ ] 保存／再読込でdirty副作用、sourceの意図しない書換え、重複Asset登録が発生しない。
- [ ] 単一画像と連番画像の代表ケースが受入表へ記録される。

## 主な実装候補

- `Artifact/src/Service/ArtifactProjectService.cppm`
- `Artifact/src/Layer/ArtifactImageLayer.cppm`
- `ArtifactCore/src/Asset/AssetManager.cppm`
- project exporter／importer と source registry の保存境界
- `docs/planned/MILESTONES_BACKLOG.md` の `WALK-IMG-1`

## リスク

既存projectの absolute path fallbackを壊すと過去データを開けなくなるため、旧JSONは必ず読込可能なままにする。resolution resultを保存値へ自動反映すると不要なdirtyやUndo履歴を生むため、解決状態とユーザー編集値を分離する。動画対応は対象外として、共通化が動画実装を誘発しない粒度に留める。
