# MILESTONE: Data-Driven Engine (CSV / Tabular Data)

> 2026-04-21 作成

## 目的

ArtifactCore にデータ駆動機能の基盤を導入する。CSV を起点とし、将来的に JSON / TSV / Excel などの表形式データを統一的に扱えるエンジンとする。

AE における「エクスプレッションで外部データ参照」「モーショングラフ用データバインディング」「バッチ処理用パラメータシート」などを可能にする土台を作る。

---

## 設計方針

1. **データソース抽象化** — CSV / JSON / TSV を `IDataSource` インターフェースで統一
2. **型解決** — ヘッダー行からカラム型を自動推定（string / int / float / bool / date）
3. **キャッシュ** — 初回ロード後はメモリ上で高速アクセス。ファイル変更検知でリロード
4. **既存レイヤーとの接続** — `ISource` / `AssetDatabase` / `PropertyRegistry` とブリッジ可能にする
5. **Qt 非依存コア** — パーサー層は純粋 C++。Qt 統合はアダプター層で分離

---

## Phase 1: CSV Parser Core

目的: CSV の読み込み・パース・行/列アクセスを提供する。

対象:

- `ArtifactCore/include/Data/CsvParser.ixx` — CSV パーサー（RFC 4180 準拠）
- `ArtifactCore/src/Data/CsvParser.cppm` — 実装
- `ArtifactCore/include/Data/DataTable.ixx` — 表形式データモデル（スキーマ + 行データ）

完了条件:

-  quoted field（`"hoge,fuga"`）、エスケープ（`""`）、改行を含むフィールドを正しくパース
-  ヘッダー行あり/なしの両対応
-  区切り文字（`,` / `;` / `\t`）を自動検出または明示指定
-  `DataTable` が行/列・型付きアクセスを提供

---

## Phase 2: Type Inference & Typed Access

目的: カラムの型を自動推定し、型安全なアクセスを提供する。

対象:

- `ArtifactCore/include/Data/ColumnType.ixx` — 型定義（String / Int / Float / Bool / Date / Unknown）
- `ArtifactCore/include/Data/TypeInference.ixx` — 型推論エンジン
- `ArtifactCore/include/Data/TypedColumn.ixx` — 型付きカラムビュー

完了条件:

-  全行をスキャンして各カラムの最適型を推定
-  `DataTable::getInt(row, col)`, `getFloat(row, col)` などが型不一致時にフォールバック
-  型推定結果を `ColumnSchema` として公開

---

## Phase 3: Data Source Abstraction

目的: CSV 以外のフォーマットを統一的に扱えるインターフェースを提供する。

対象:

- `ArtifactCore/include/Data/IDataSource.ixx` — データソース抽象インターフェース
- `ArtifactCore/include/Data/CsvDataSource.ixx` — CSV 実装
- `ArtifactCore/include/Data/JsonDataSource.ixx` — JSON 配列オブジェクト実装（将来用スケルトン）
- `ArtifactCore/include/Data/DataSourceRegistry.ixx` — 登録・解決・ファクトリ

完了条件:

-  `IDataSource::open(path)`, `rows()`, `columns()`, `schema()` を統一 API で提供
-  拡張子または MIME type から適切な DataSource を自動解決
-  `DataSourceRegistry::registerFormat()` で拡張可能

---

## Phase 4: File Change Detection & Cache

目的: ソースファイルの変更を検知し、キャッシュを無効化する。

対象:

- `ArtifactCore/include/Data/DataCache.ixx` — キャッシュマネージャー（LRU + TTL）
- `ArtifactCore/include/Data/FileWatcher.ixx` — ファイル変更監視（`std::filesystem::last_write_time` ベース）

完了条件:

-  同一ファイルの再ロードでキャッシュヒット時は即座に返す
-  ファイルの `last_write_time` が変更されたらキャッシュを無効化
-  `DataCache::maxEntries` / `DataCache::ttl` を設定可能

---

## Phase 5: Property System Bridge

目的: データテーブルを PropertyRegistry から参照可能にする。

対象:

- `ArtifactCore/include/Data/DataPropertyBridge.ixx` — DataTable → Property 変換
- `ArtifactCore/include/Data/ExpressionDataFunctions.ixx` — エクスプレッション用関数（`dataGet()`, `dataRow()` 等）

完了条件:

-  `data://path/to/file.csv` URI で PropertyRegistry から参照可能
-  エクスプレッション内から `dataGet("file.csv", row, "column")` で値取得
-  データソース変更時に依存プロパティを dirty 通知

---

## Phase 6: Asset Integration

目的: AssetDatabase と連携し、プロジェクト内のデータファイルを管理する。

対象:

- `ArtifactCore/include/Asset/AssetType.ixx` — `DataType` 追加
- `ArtifactCore/include/Asset/DataAssetFile.ixx` — データアセット専用ファイルクラス

完了条件:

-  CSV/JSON ファイルを AssetDatabase に登録可能
-  アセットとしてメタデータ（行数、カラム数、型スキーマ）を保持
-  プロジェクト保存時にデータアセットの参照情報をシリアライズ

---

## 実装順

1. Phase 1: CSV Parser Core
2. Phase 2: Type Inference & Typed Access
3. Phase 3: Data Source Abstraction
4. Phase 4: File Change Detection & Cache
5. Phase 5: Property System Bridge
6. Phase 6: Asset Integration

---

## 変更しないこと

- 既存の `AssetImporter` / `AssetDatabase` のコアロジック
- 既存の `ISource` / `FileSource` の実装
- 既存の画像/動画/音声のメディアパイプライン
- 既存の PropertyRegistry の構造

---

## リスク

- 巨大 CSV（10万行超）のメモリ使用量 — Phase 4 のキャッシュで LRU 制御
- 文字コード（UTF-8 / Shift-JIS / EUC-JP）— Phase 1 で UTF-8 のみ対応、他は将来対応
- 型推定の誤判定 — 明示的な型指定スキーマファイル（`.schema.json`）で上書き可能にする

---

## 関連文書

- [`docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md)
- [`ArtifactCore/include/Source/ISource.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Source/ISource.ixx)
- [`ArtifactCore/include/Asset/AssetImporter.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Asset/AssetImporter.ixx)
- [`ArtifactCore/include/Property/PropertySerializationBridge.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/PropertySerializationBridge.ixx)
