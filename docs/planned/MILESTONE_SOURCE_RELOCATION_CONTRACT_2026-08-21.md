# マイルストーン: Source Relocation Contract（Image / Image Sequence）

**最終更新:** 2026-08-21
**ステータス:** In Progress
**優先度:** High
**識別子:** M-SRC-RELOC-1
**関連:** `docs/planned/MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`, `docs/planned/MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md`, `docs/planned/MILESTONE_ASSET_BROWSER_RELINK_WORKFLOW_2026-06-28.md`

### 進捗 (2026-08-21)

- `ArtifactProjectManager::setCurrentProjectPath()` がtrim後の入力を絶対・clean pathへ正規化し、project rootを同じ正規化値から再計算するようにした（Artifact `1a088828`、親gitlink `271f627`）。
- `ArtifactProjectExporter` がabsolute pathだけでなく、既にrelativeなsource／sequence pathもproject directory基準の `pathRelative`／`sequencePathsRelative` として保存するようにした（Artifact `c68d9e1d`、親gitlink `5f03338`）。
- 既存のproject exporter／importerにある `*Relative` と source registry復元経路は温存し、次の実装では候補順位・diagnostic reasonの共通化へ進む。

### 進捗 (2026-08-21 第2回: 候補採用とdiagnostic reasonの共通化)

- `Asset.Manager` モジュールに共通契約型を追加した（ArtifactCore）:
  - `SourceResolutionCandidateKind`（`SavedAssetId` / `ProjectRelativePath` / `RegistryRelativePath` / `AbsolutePathFallback`）
  - `SourceCandidateOutcome`（既存候補採用 / 元パス空のため未解決候補採用 / 候補欠落で元パス維持 / 候補空で元パス維持）
  - `SourceCandidateResolution`（kind、adopted、originalPath、candidatePath、resolvedPath、outcome）
- 採用判定を `resolveProjectRelativeSource()` に、保存側のrelative候補生成を `projectRelativeSourceCandidate()` に集約した。両関数は既存の採用条件（exists または元パス空で採用／連番frameはexistsのみ採用）と同一の挙動を保持する。
- Importerの5解決箇所（source registry `pathRelative`、layer `*sourcePath` + `*Relative`、layer `*sequencePaths` 要素毎、footage `filePath` + `filePathRelative`、footage `sequencePaths` 要素毎）を共通関数へ統一した。
- Importerが解決統計を収集し、候補欠落が1件以上ある場合は `[Importer][SourceResolution]` としてwarningサマリを出力する。
- Exporterの `relativePathFor` を `projectRelativeSourceCandidate()` へ委譲し、保存側・復元側で同じ正規化を使うようにした。
- `tests/ArtifactCore/SourceResolutionContractTest.cpp` を追加し、採用・維持・空元パス採用・連番slot維持・relative生成round-tripを固定した。`ArtifactCoreSourceResolutionContractTest` としてCMake登録済み。ビルド・実行はユーザー指示待ち。

## 保存キーの棚卸し（Phase 0）

| 保存キー | 対象 | 復元側の採用条件 |
|---|---|---|
| `assets.sourceRegistry.sources[].pathRelative` | source registry | 候補が存在、または元 `path` が空 |
| `<key>.sourcePath` + `<key>.sourcePathRelative` | layer単一source | 候補が存在、または元が空 |
| `<key>.sequencePaths` + `<key>.sequencePathsRelative` | layer連番 | 要素毎に候補が存在する場合のみ差し替え |
| footage `filePath` + `filePathRelative` | Project Item footage | 候補が存在、または元が空 |
| footage `sequencePaths` + `sequencePathsRelative` | Project Item footage連番 | 要素毎に候補が存在する場合のみ差し替え |

既存JSON（`*Relative` を持たない旧形式）はabsolute pathのみで復元され、挙動は変更されない。`Asset.Manager` のregistry復元（schemaVersion 2）は従来どおり厳格validationを維持する。

## 残課題

- Image／Video／Audio layerの復元順序は依然「absolute path第一候補 → Asset ID後追い」であり、契約案（Asset ID → relative → absolute）と逆。Phase 2で `loadFromPath()` 前にAsset ID／registry経路を先に試行する変更が必要。
- 解決結果のhealth reportへの反映（`AssetPathMissing` との突合）は未実装。

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

- source resolution候補、採用理由、missing理由を共通の診断表現へ整理する（完了: `SourceResolutionCandidateKind` / `SourceCandidateOutcome` / `SourceCandidateResolution`）。
- Image／Sequenceで共通の保存キーと、既存JSONの互換読込を棚卸しする（完了: 本書の保存キー棚卸し表）。
- Asset ID、relative path、absolute fallbackの優先順位を仕様化する（仕様は本書の解決優先順位のまま。実装適用はPhase 2）。

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
