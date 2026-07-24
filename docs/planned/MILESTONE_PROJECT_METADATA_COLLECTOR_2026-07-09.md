# MILESTONE: 汎用メタデータ収集基盤（Project Visitor / Collector）

**ステータス:** Phase 1A Completed (static verified 2026-07-22; runtime/build verification pending)

Phase 2 (Statistics Collector migration) and the Phase 3 asset/file integration slice are completed at source level.

> 2026-07-09 作成

## 目的

プロジェクトを走査して「何かを集める」ニーズ（統計 / 使用アセット / 使用フォント /
欠損参照 / 未使用アセット / エフェクト使用状況など）に対し、共通の走査 + 収集基盤
（`ProjectVisitor` + `Collector`）を導入する。既存の目的別収集コードをこの基盤へ
載せ替え、走査ロジックの重複と収集単位の不一致を解消する。

## 背景

現状、プロジェクトから情報を集める機能が独立して複数存在し、それぞれが独自に
走査している:

| システム | 場所 | 集めるもの | 走査方法 |
|---|---|---|---|
| `ArtifactProjectStatistics::collect` | `Project/ArtifactProjectStatistics` | comp/layer/effect 数・使用回数 | comp/layer/effect 走査 |
| `ArtifactProjectHealthChecker::check` | `Project/ArtifactProjectHealthChecker` | 循環参照・重複ID・欠損アセット等 | 独自走査 + HealthIssue |
| `ArtifactProjectPackager::getAllExternalFiles` | `Project/ArtifactProjectPackager` | 使用中の全外部ファイル | ProjectItem ツリー traverse |
| `ArtifactAbstractComposition::usedAssets()` | `Composition/ArtifactAbstractComposition` | 使用 AssetID | layer JSON の sourcePath/filePath 再帰 |
| `AssetReferenceTracker::getUnusedAssets` | Asset Browser | 未使用アセット | 参照追跡 |
| `ArtifactProjectCleanupTool` | `Project/ArtifactProjectCleanupTool` | 未使用アセット除去用 collectUsed | ProjectItem 再帰 |

問題:

1. **走査ロジックが重複** — Packager / CleanupTool / Statistics / Composition が
   それぞれ別々に comp→layer やツリーを走査している。共通の走査抽象がない。
2. **収集単位がバラバラ** — ProjectItem ツリーを歩くもの、layer JSON の
   sourcePath/filePath 文字列を拾うものが混在し一貫性がない。
3. **新規ニーズが毎回穴を踏む** — フォント使用レポート（別マイルストーン
   `MILESTONE_FONT_USAGE_REPORT_2026-07-09`）も結局この走査基盤の不在に当たり、
   また独自走査を書くことになる。

## ターゲット像

- プロジェクト構造（project → composition → layer → effect → property）を
  一貫した順序で走査する単一の `ProjectVisitor`。
- 走査中に情報を集める `Collector` インターフェース（1 走査で複数 Collector を同時実行可能）。
- 既存の Statistics / Packager / usedAssets / Font Report を Collector として再実装。
- 収集結果を共通の `MetadataReport` 形へ集約し、JSON / CSV 出力の再利用を可能にする。

## 非ゴール（このマイルストーンの範囲外）

- 既存機能の挙動変更・機能追加（あくまで基盤化とリファクタ）
- Health Checker の修復ロジック（auto-repair）の再設計
- 収集結果の永続キャッシュ / インクリメンタル走査（後段で検討）
- UI の大規模刷新（既存導線はそのまま保つ）

## 現状とギャップ

| 項目 | 現状 | ギャップ |
|---|---|---|
| 走査抽象 | 各機能が独自実装 | 共通 `ProjectVisitor` がない |
| 収集抽象 | 戻り値の型がバラバラ | 共通 `Collector` 契約がない |
| 収集単位 | ProjectItem / layer JSON 混在 | 統一されたノードモデルがない |
| 出力形式 | 機能ごとに独自 | 共通 `MetadataReport` がない |
| 再利用 | コピペ気味の走査 | 1 走査 N 収集ができない |

## 設計原則

1. **既存機能の挙動を変えない**。まず基盤を作り、既存を段階的に載せ替える。
2. 走査は「構造の順序」を保証し、Collector は「何を拾うか」だけに集中させる。
3. 1 回の走査で複数 Collector を同時実行できる（走査コストの共有）。
4. layer JSON 依存の収集（sourcePath/filePath）と型付き API 収集の橋渡しを 1 箇所に寄せる。
5. `ArtifactCore` 側に基盤を置き、`Artifact` 側の各機能から利用する。

## Scope（想定する変更ファイル）

- `ArtifactCore/include/Project/ProjectVisitor.ixx`（新規）
- `ArtifactCore/include/Project/MetadataCollector.ixx`（新規: Collector 契約 + MetadataReport）
- `Artifact/include/Project/ArtifactProjectStatistics.ixx`（Collector 化）
- `Artifact/include/Project/ArtifactProjectPackager.ixx`（getAllExternalFiles を Collector 化）
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`（usedAssets を基盤経由へ）
- `Artifact/include/Project/ArtifactProjectCleanupTool.ixx`（collectUsed を基盤経由へ）
- 連携: `MILESTONE_FONT_USAGE_REPORT_2026-07-09`（Font Collector として実装）

## Phases

### Phase 1: Visitor / Collector 契約

走査と収集の抽象を定義する。

- `ProjectVisitor`（project → comp → layer → effect → property の走査順）を定義
- `Collector` インターフェース（`onComposition` / `onLayer` / `onEffect` / `onProperty` フック）
- `MetadataReport`（キー付き収集結果 + JSON/CSV シリアライズ）を定義
- 1 走査で複数 Collector を実行できる driver を用意

**Done when:**

- 空 Collector で全ノードを一度ずつ訪問できる
- 収集結果を共通 report に集約できる

### Phase 2: Statistics を Collector 化

既存 `ArtifactProjectStatistics` を基盤に載せ替える。

- comp/layer/effect カウント・effect 使用マップを `StatsCollector` として実装
- 既存 `collect()` API は互換維持（内部で新基盤を呼ぶ）
- 出力が従来と一致することを確認

**Done when:**

- `ProjectStats` の結果が従来と一致
- 走査ロジックの重複が 1 つ減る

**実装確認:**

- [x] `ProjectStatsCollector` が `MetadataCollector` として comp / layer / effect を集計
- [x] 既存 Statistics の収集経路が `ProjectVisitor` / `MetadataCollectorDriver` を使用
- [x] 既存の font / value collector と同一走査で併行実行
- [ ] runtime / build による出力一致確認

### Phase 3 実装確認

- [x] `ArtifactProjectCleanupTool` が共通 metadata traversal を利用
- [x] serialized `sourcePath` / `filePath` の収集経路を共通走査へ寄せている
- [x] Packager / usedAssets の統合対象となる共通ノード契約を確認
- [ ] runtime / build による既存出力一致確認

### Phase 3: Asset / File 収集を Collector 化

Packager / usedAssets / CleanupTool の収集を統合する。

- `ExternalFileCollector`（Packager の getAllExternalFiles 相当）
- `UsedAssetCollector`（composition usedAssets 相当）
- layer JSON の sourcePath/filePath 抽出を基盤の 1 箇所へ集約
- 既存 API を互換維持しつつ内部を差し替え

**Done when:**

- Packager / usedAssets / CleanupTool が同じ走査基盤を使う
- 収集結果が従来と一致

### Phase 4: Font Collector 統合

フォント使用レポートを本基盤の Collector として実装する。

- `FontUsageCollector`（テキスト/シェイプの fontFamily 収集）を実装
- `MILESTONE_FONT_USAGE_REPORT` の Phase 1 をこの基盤で置換
- 1 走査で Stats + Font + Asset を同時収集できることを確認

**Done when:**

- フォント収集が独自走査でなく基盤経由になる
- 複数 Collector 同時実行が実証される

### Phase 5: 出力・検証

共通レポート出力と回帰確認。

- `MetadataReport` の JSON / CSV 出力を共通化
- 既存機能（Statistics / Packager / Cleanup）の回帰確認
- 走査コスト（1 走査 N 収集）の効果を確認

**Done when:**

- 既存機能が挙動を変えずに基盤上で動く
- 新規収集ニーズが Collector 追加だけで済む

## Recommended Order

1. Phase 1 (Visitor / Collector 契約)
2. Phase 2 (Statistics 載せ替え)
3. Phase 3 (Asset / File 統合)
4. Phase 4 (Font 統合)
5. Phase 5 (出力・検証)

### Why This Order

- Phase 1 で契約が固まらないと以降の載せ替えがブレる。
- Statistics は依存が少なく、基盤の妥当性を最初に検証しやすい。
- Asset/File 系は依存が多く重複も大きいので契約が固まってから。
- Font は新規基盤の「1 走査 N 収集」を実証する良いショーケース。
- 出力・検証は最後にまとめて回帰確認する。

## 連携先

- `Artifact/include/Project/ArtifactProjectStatistics.ixx`
- `Artifact/include/Project/ArtifactProjectPackager.ixx`
- `Artifact/include/Project/ArtifactProjectCleanupTool.ixx`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`（usedAssets）
- `Artifact/include/Project/ArtifactProjectHealthChecker.ixx`（将来的な載せ替え候補）
- `ArtifactCore/include/Project/ProjectVisitor.ixx`（新規）
- `ArtifactCore/include/Project/MetadataCollector.ixx`（新規）
- 関連: `docs/planned/MILESTONE_FONT_USAGE_REPORT_2026-07-09.md`

## Validation Checklist

- 空 Collector で全ノードを一度ずつ訪問できる
- Statistics の結果が従来と一致する
- Packager / usedAssets / Cleanup が同じ基盤を使い結果一致
- フォント収集が基盤経由で動く
- 1 走査で複数 Collector を同時実行できる
- 既存機能の挙動が変わらない

## Notes

このマイルストーンは「機能追加」ではなく「共通基盤の抽出」。
フォント使用レポート・アセット収集・統計・欠損チェックなど、繰り返し出てくる
「プロジェクトから何かを集める」ニーズを 1 つの走査基盤に集約するのが狙い。
新規の収集ニーズが出るたびに独自走査を書く現状を止める。

---

## Phase 1A Execution Result

Phase 1A の走査順・Collector フック・複数 Collector driver は実装済み。

### Phase 1A の実装点

1. project → composition → layer → effect → property の走査順を確定する
2. `Collector` のフック（onComposition / onLayer / onEffect / onProperty）を定義する
3. layer の型付き API と JSON（sourcePath/filePath）の両アクセスをどう抽象するか決める
4. 1 走査で複数 Collector を回す driver の最小形を作る

### Phase 1A 実装確認

- [x] `ProjectVisitor` に project / composition / layer / effect / property の走査ノード契約を追加済み
- [x] `MetadataCollector` の型別 hook と reset / report 契約を追加済み
- [x] 複数 Collector を同一走査で実行する `MetadataCollectorDriver` を追加済み
- [x] `ArtifactProjectStatistics` の既存統計・値・font collector を新しい走査経路へ接続済み
- [ ] runtime / build による最終確認

### Phase 1 完了条件

- 空 Collector で全ノードを一度ずつ訪問できる
- 収集結果を共通 report に集約できる
- 既存機能を壊さない（この時点では並存）

### Phase 2 の前提

- 走査順が既存 Statistics の集計順と矛盾しない
- Collector 契約が Stats / Asset / Font の 3 用途を無理なく表現できる
- 既存 API は互換維持し、内部だけ差し替える方針を守る
