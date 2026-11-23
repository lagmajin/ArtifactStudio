# DCC ツールとして実装が薄い分野 継続サーベイ

**最終更新:** 2026-11-19
**目的:** 既存の DCC ギャップ分析がカバー済みの分野を避け、現行ソースから「実装が薄い／存在しない」分野を継続的に見つけて記録する。1 回の調査で完了させず、発見次第追記する。
**方法:** `docs/analysis/` の既存文書でカバー済み分野を整理し、その上で `Artifact/` / `ArtifactCore/` のヘッダ・ソースを grep / read して実-code で裏取りする。ビルド・実機検証は行わない。

## 0. 既存文書でカバー済み（重複を避けるための整理）

| 分野 | 主な既存文書 | 状態 |
|---|---|---|
| 3D: IBL / 3 軸回転 / デフォーマ / スキニング / 影 / リフレクションプローブ | `REPORT_DCC_GAP_3D_TEXT_2026-08-18.md`、`THREED_*` | 調査済み |
| ビューポート: 表示 / オーバーレイ / ナビゲーション / スナップ | `VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md` | 調査済み |
| カラー: OCIO / ACES / LUT / チャンネル順 | `COLOR_PIPELINE_AUDIT_2026-08-02.md`、`COLOR_CHANNEL_ORDER_CONSISTENCY_AUDIT_2026-10-16.md` | 調査済み |
| キーイング / トラッキング / シミュレーション / エフェクト数 | `GAP_AE_NUKE_2026-08-01.md`、`MISSING_MODULES_2026-08-01.md` | 調査済み |
| ワークフロー: アセット共有 / キーフレームコピー / オーディオスクラブ / Auto-Orient | `WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` | 調査済み |
| 公開プロパティ / ライブ入力 / State Variant / Field | `REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md` | 調査済み |
| コラボレーション | `COLLABORATION_IMPLEMENTATION_AUDIT_2026-08-22.md` | 調査済み（ReactiveEvents は触らない） |

## 1. アセットインスタンス共有（既存指摘が現行コードでも有効）

- **根拠:** `ArtifactCore/include/**/*.ixx` を grep し、`AssetInstance` / `instanceId` / `refCount` / `referenceCount` は **0 件**（2026-11-18 実行）。
- 既存の `AssetManager` / `AssetDatabase` / `AssetImporter` は source 取得・localize・version/health・relink 等を持つが、複数レイヤーが同じ source を共有する概念はない。
- `WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §1 の提案は 2026-09-07 時点で「instance 概念なし」としていたが、現行コードでも解消していない。
- **判定:** 重複ではあるが「未解消」を再確認。新規候補としては数えない。

## 2. マルチシーン／シーンブレンド（新規候補）

- **確認方法:** `Artifact/include/**/*.ixx` を grep し、`SceneBlend` / `multiScene` / `MultiScene` / `SceneManager` / `activeScene` / `setActiveScene` は **0 件**。
- 存在するのは `ArtifactCompositionManager`、`ArtifactCore/include/Composition/CompositionRegistry.ixx`、`Artifact/include/AI/WorkspaceAutomation.ixx` の `changeCurrentComposition` 等、**1 つの composition を active に切り替える** 経路のみ。
- DCC 側:
  - Maya: scene blend / reference / namespace による複数シーン共存
  - Nuke: read ノードで複数シーンを重ねるのが前提
  - UE: level blending / sublevel
  - Blender: linked scene / scene strip
- Artifact は `ArtifactCompositionLayer`（precomp）で入れ子は作れるが、**複数 composition を同時に保持して重み付きでブレンドするモデルは持たない**。
- **判定:** 既存の「コンポジション切替」と「precomp 入れ子」の中間にあたる空白。DCC ツールとしての実装が薄い候補。
- **未検証:** runtime / UI 受入れ、既存 `CompositionRegistry` のスレッド契約との整合、Undo 境界。

## 3. シーン依存の逆引き可視化（新規候補・2026-11-18 修正）

- **確認方法:** `Artifact/include/**/*.ixx` を grep し、`DependencyGraph` / `ReferenceGraph` / `whatReferences` / `referencedBy` / `collectDependencies` は **0 件**。ただし `Artifact/src/Diagnostics/AppValidationRules.cpp` には **マット参照に限定した循環依存検出**（`createCircularDependency`）が存在する。当初の「依存逆引きは完全に 0 件」という記述は不正確だったため修正する。
- 存在するのは以下のみ:
  - `Artifact/src/Diagnostics/AppValidationRules.cpp`: `ArtifactMatteReferenceRule` が matte 参照の missing / self-reference / hidden source / **cycle 検出**（DFS + `reportedCycleKeys` で重複抑制）を実行
  - `ArtifactCore/include/Diagnostics/ValidationRules.ixx` の `MissingFileValidationRule`
  - `ArtifactCore/include/Diagnostics/ProjectDiagnostic.ixx` の `createMissingFile` / `createMissingMatte`
  - `ArtifactCore/include/Asset/AssetDatabase.ixx` の `relinkAssetPath`（id↔path のマップのみ、参照元は保持しない）
  - `ArtifactCore/include/NLE/Core.ixx` の `relinkSource`
- DCC 側: Nuke の Dependencies ビュー、Maya の Reference Editor、Houdini の dependency view は「どのノードがどのアセットを参照し、どこに影響するか」を逆引きで見せる。AE の「Missing Footage」も 1 件ずつではなくまとめて表示・置換する。
- **正確な空白（2026-11-18 再訂正）:** マット参照は cycle 検出、precomp 参照は Flowchart 可視化と BFS 循環検出が実装済み（§5A 参照）。残るのは **アセット（footage）と effect の参照元逆引き** で、`AssetDatabase` は `assets_` (id→info) と `pathToId_` (path→id) のみで「どのレイヤーが参照しているか」を持たないため、missing を 1 件ずつ直す運用から抜け出せていない。
- **判定:** 既存の asset project 監査が挙げる「missing footage を 1 個ずつ直す苦痛」は未解消。DCC ツールとして明確に薄い。
- **未検証:** `AssetDatabase` / `relinkAssetPath` の既存契約、Undo 境界、UI の Problem View との責務分離。

## 4. カバー済みと判定した分野（新規候補に含めない）

| 分野 | 確認結果 |
|---|---|
| ユーザー定義ツール／shelf 永続化 | `MacroUndoCommand` は Undo 内部。`ArtifactScript` / Command IR / Python は既にあり、重複リスクが高い |
| ワークスペースレイアウト保存 | `NativeDockSurface::saveLayoutState()` / `DockLayoutRegistry` が存在 |
| バージョン自動付与 | `ExportMatrix::namingRule` / `TemplateSlot::namingRule` / `PreCompose::autoNaming` / `checkNamingIssues` が存在。production naming の深掘りは responsive output / Ad Production 計画と重複するため後回し |
| ローカルレンダーファーム | `RenderFarmMaster` / `RenderFarmWorker` / `RenderFarmSharedBuffer` / `submitJob` が存在。外部 farm 連携はこの上に乗るため単独候補にしない |

## 5. マルチシーン候補の接続点（2026-11-18 追記）

- `Artifact/include/Composition/ArtifactCompositionManager.ixx` は `compositionCount()` / `compositionAt()` / `allCompositions()` / `compositionById()` / `search()` / `createNewComposition()` / `removeAllCompositions()` を持つ。**複数 composition の保持と列挙は既に存在**。
- `ArtifactCore/include/Composition/CompositionRegistry.ixx` は「script host 向けの name → opaque pointer 辞書」。コメントで「誰かが所有し、composition が死ぬ際に owner が除去する」と明記されており、**寿命・ブレンド状態は持たない**。
- したがってマルチシーン実装の空白は「composition を増やすこと」ではなく、**複数 composition を同時に active にし、重み・順序・依存・Undo 単位を定義するモデル**にある。
- `WorkspaceAutomation.ixx` の `changeCurrentComposition` は単一 active の切替であり、複数同時保持の API ではない。
- 既存の `ArtifactCompositionLayer`（precomp）は入れ子の参照であり、並列ブレンドではない。
- **次に確認すべきこと:** `RemoveAllCompositionResult` / `CreateCompositionResult` の契約、Undo コマンドが composition コレクションをどう扱うか、Render Controller が複数 composition を同時に描画できるか。

## 5A. 参照横断調査の追加確認（2026-11-18）

- `ArtifactCore/include/**/*.ixx` を grep し、`effectReferences` / `referencedEffects` / `dependsOn` / `usedBy` / `referenceCount` は **0 件**（`DiskSize::usedBytes` は無関係な一致）。effect 参照を逆引きする API は存在しない。
- precomp は **逆引きも含めて一部実装済み**だった。当初「逆向き API は 0 件」と書いたのは不正確だったため訂正する。
  - `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cpp:722-760`: composition 内の `ArtifactCompositionLayer` を走査し、`sourceCompositionId()` を辿って Project Flowchart に依存エッジを描画（solid dependency edge、ツールチップ付き）
  - `Artifact/src/Service/ArtifactProjectService.cpp:3119-3168`: `addCompositionLayerToCurrentComposition()` が BFS で直接・推移的自己参照を検出し循環を拒否
  - `ArtifactCore::PreComposeManager::registerPrecompLayer(targetId, layerId, sourceId)` で参照を登録
- **正確な空白:** 参照グラフの可視化と循環検出は **precomp と matte に限定**されている。`AssetDatabase` は `assets_` (id→info) と `pathToId_` (path→id) のみで「どのレイヤーが footage を参照しているか」を持たず、effect 参照の逆引き API もない。したがって **footage / effect の参照元逆引き** が残る。
- **`ObjectReference` の実態（2026-11-18 追加裏取り）:** `ArtifactCore/include/Property/AbstractProperty.ixx:60` に `ObjectReference, // ID 参照` の enum があり、`Property.ixx:235-236` が表示名を返す。しかし `ArtifactCore/src/Property/AbstractProperty.cpp:93-95` では `String` と同じく `value.canConvert<QString>()` として扱われ、**参照 ID の検証・解決ロジックは Core に存在しない**。UI の `ObjectReferencePropertyEditor` はピッカーを提供するが、共通の参照契約（参照先 type / id / 解決規則）は無い。effect の asset 参照を共通化する土台としては不十分。
- **effect 参照の保存形式（2026-11-18 追加裏取り）:** `Artifact/src/Composition/ArtifactAbstractComposition.cpp:2022-2102` の `serializeEffect()` は id / displayName / enabled / mix / allowOverscan / pipelineStage / effectRegion / properties / modulation を保存するが、**参照先 asset を表す共通フィールドは持たない**。個別効果がバラバラに文字列 property として保持する:
  - `LUTEffect.cpp:176-189`: `LUT File` は `PropertyType::String` の property、`setPropertyValue()` から直接 `setFilePath()` を呼ぶ（Undo / dirty / asset identity との接続はこの断片からは確認できない）
  - `SurfaceFXEffect.cpp:100-108`: `texturePath` + `assetId` を要素ごとに保持し、解決時に `AssetDatabase::getAssetInfo(assetId).absolutePath` を優先。asset が missing の場合の diagnostic や参照元記録はこの経路には無い
  - したがって「effect がどの asset を参照しているか」を横断的に得るには、各効果の property を知る必要があり、一般の参照レコードとして扱えない。
- **共通化の土台評価:** `PropertyType::ObjectReference` は Core で文字列として扱われるだけ（前述）で検証・解決ロジックが無い。`SurfaceFXEffect` の `assetId` + `AssetDatabase::getAssetInfo()` が最も近い既存パターンだが、effect ごとに手実装されており共通契約になっていない。effect 参照を共通化するなら、この `assetId` パターンを property contract へ昇格させるのが最小変更経路と考えられる（未検証・未実装）。
- **判定:** 「シーン依存の逆引き可視化」は完全な空白ではなく、precomp/matte は実装済み。新規候補として扱うなら footage/effect の参照元逆引きと一括 relink の判断材料に限定すべき。effect 側は共通の参照契約が無いことが根本要因。

## 6. マルチシーン候補の Undo 境界（2026-11-18 追加調査）

- `Artifact/include/Undo/UndoManager.ixx` の composition 系コマンドは `SetCompositionNoteCommand` / `RenameCompositionCommand` / `SetCompositionWorkAreaCommand` / `SetCompositionSettingsCommand` / `SetCompositionResponsiveLayoutCommand` / `ChangeCompositionResolutionCommand` の **6 件** のみ。
- `CreateCompositionCommand` / `RemoveCompositionCommand` / `AddCompositionCommand` / `DeleteCompositionCommand` は `Artifact/` 全体で **0 件**。
- `Artifact/src/Service/ArtifactProjectService.cpp:5973` の `createComposition()` は `manager.createComposition(params)` を直接呼び、`UndoCommand` を push していない（同関数の `changeCurrentComposition()` も直接呼び）。
- `removeComposition()` も `projectShared->removeCompositionById(id)` を直接呼ぶ。
- したがって **composition 自体の生成・削除は Undo 履歴に載らない** 可能性が高い。precomp / unprecomp の Undo は実装済み（`MILESTONES_BACKLOG.md` 2026-09-19 確認）だが、それは既存 composition 内の入れ子操作であり、composition コレクション寿命の Undo ではない。
- **判定:** マルチシーンを導入する場合、複数 composition の同時保持だけでなく、**生成・削除の Undo 単位を新たに定義する必要がある**。既存の `CompositionChangeDetector` / `CreateCompositionResult` / `RemoveAllCompositionResult` は結果型であり Undo コマンドではない。
- **未検証:** UI 側で別の Undo ラッパーを使っている可能性、`ArtifactProjectService` を通らない生成経路、runtime の Undo 履歴。

## 7. カバー済みと判定した追加分野（新規候補に含めない）

| 分野 | 確認結果 |
|---|---|
| タイムコード同期 / Genlock | `TimeCode` / `TimeCodeRange` / `PlaybackClock::timecode()` / render tmcd / drop-frame が存在。Genlock はハード寄りで対象外 |
| バッチ renamed / 一括プロパティ | `WorkspaceAutomation` の `batchRenameProjectItems` と batch property 経路が存在（UI 側の深掘りは別課題） |
| TimeRemap / 複数レイヤー同期 | `TimeRemapProcessor` / `TimeRemapEffect` / `AudioTimeStretchProcessor` が Core に存在 |
| 外部 render farm | ローカル `RenderFarmMaster` / `RenderFarmWorker` が存在。外部 adapter はこの上に乗る |

## 8. 次に探す分野（未調査）

- シーン依存グラフを `AssetDatabase` / Problem View とどう接続できるか（`assets_` は id→path、`pathToId_` は path→id のみで参照元を持たない）
- カスタムツールの「実行履歴」ではなく「再利用可能なパラメータ付き操作」の保存先（effect preset とは別）
- `ArtifactProjectService::createComposition` 以外の composition 生成経路を追い、Undo 履歴の実態を確定する
