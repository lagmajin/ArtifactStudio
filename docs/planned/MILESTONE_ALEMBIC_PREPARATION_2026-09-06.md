# Alembic対応準備メモ

**最終更新:** 2026-09-06

**状態:** 準備・設計のみ。依存追加、実装、ビルド、テストは未実施。

## 結論

Alembicは既存の3Dモデル取り込み経路へ接続できる見込みがある。ただし、Alembicを通常の静的モデルとして扱う範囲と、時間サンプルを評価するジオメトリキャッシュとして扱う範囲を分ける必要がある。

初回の実装対象は、単一オブジェクト・単一フレームの静的ジオメトリ読み込みに限定する。キャッシュ再生は、時間サンプル、メッシュ更新、フレームキャッシュ、再生中の更新頻度を別フェーズにする。

## 調査ルート

`FileTypeDetector` → `AssetImporter` → `MeshImporter` → `Artifact3DLayer` → `Artifact3DModelViewer / Contents Viewer`

## 現状の事実

### 形式認識とAsset登録

- `ArtifactCore/src/File/FileTypeDetector.cppm` は `.abc` を `FileType::Model3D` として認識する。
- `ArtifactCore/src/Asset/AssetImporter.cppm` の対応拡張子にも `abc` が含まれる。
- したがって、ファイル選択・Asset分類の入口はすでに存在する。
- ただし、拡張子認識はimport成功を意味しない。現状は「recognized」であって「importable」ではない。

### MeshImporter

- `ArtifactCore/include/Geometry/MeshImporter.ixx` の `Backend` には Alembic がない。
- `importMeshFromFile()` は FBX、OBJ、glTF/GLB、PMD、STL、PLY、LAS、USDAなどを拡張子で分岐するが、ABC分岐はない。
- `importMeshFromFileAtTime()` は現在、FBX／glTF／GLBだけを時間指定経路へ渡し、それ以外は通常読み込みへ戻す。
- `lastBackend()` と `lastError()` があり、Alembic対応時のbackend名・失敗理由を既存の診断経路へ載せられる。

### 3Dレイヤー

- `Artifact/src/Layer/Artifact3DModelLayer.cppm` は `Mesh mesh_` と `sourcePath_` を保持する。
- 読み込み成功後、必要に応じてスキニングを適用し、メッシュをレイヤーへコピーする。
- 現在のモデルは「時刻ごとに評価するソース」ではなく、「読み込み済みMesh」を中心にした設計である。
- Alembicの時間変形をそのまま接続すると、レイヤー側に時間評価とメッシュ更新の責務が漏れる可能性がある。

### Meshの更新能力

- `ArtifactCore/include/Mesh/Mesh.ixx` はN-gonを含むポリゴン、頂点属性、face属性、face-vertex属性を保持できる。
- `Mesh::revision()`、`updateBounds()`、`generateRenderData()` があるため、静的Alembicメッシュを既存レンダー形へ変換する器は存在する。
- `Artifact3DLayer::loadFromFileAtTime()` は既に `MeshImporter::importMeshFromFileAtTime()` を呼び出し、評価結果を `mesh_` へ差し替える。
- ただし現状の時間評価は、呼び出しごとに新しい `Mesh` を受け取り、スキニング・中心化・bounds更新を行う構造である。Alembicの高頻度再生用キャッシュとしては、サンプル再利用とGPU更新の契約が未定義である。

## 対応レベルの定義

| レベル | recognized | importable | previewable | layerable | 意味 |
|---|---:|---:|---:|---:|---|
| 0 | ○ | × | × | × | 拡張子・分類のみ |
| 1 | ○ | ○ | ○ | ○ | 代表時刻の静的ジオメトリ |
| 2 | ○ | ○ | ○ | ○ | 時間サンプルによるジオメトリキャッシュ再生 |
| 3 | ○ | ○ | ○ | ○ | 複数オブジェクト、階層、マテリアル、カメラ等 |

最初の受入れ目標はレベル1。レベル2以降は別契約として扱う。

## 推奨フェーズ

### Phase A: 対応状態の明示

- Alembicを「認識済み・未import」としてUI／diagnostic上で区別する。
- `MeshImporter::Backend::Alembic` を追加するか、実装開始時に同等のbackend識別を追加する。
- 未対応時のエラーを「unsupported format」から「Alembic backend is not wired」へ具体化する。
- `.abc` のフィルタ表示と実際のimport可否が一致しない箇所を整理する。

### Phase B: 静的ジオメトリ読み込み

- 外部Alembicライブラリの採用・配布・ライセンス条件を確定する。
- 最初は指定時刻または先頭サンプルだけを読む。
- 単一ポリゴンメッシュ、頂点、法線、UV、インデックスを既存 `Mesh` へ変換する。
- 複数オブジェクト、階層、マテリアル、カメラ、パーティクルは警告付きで対象外にする。
- Contents Viewerでは、backendと「static sample」状態を表示できるようにする。

### Phase C: キャッシュ再生

- composition frameをAlembic timeへ変換する明確な契約を定義する。
- `importMeshFromFileAtTime()` をAlembicにも拡張する。
- 時間サンプル間の補間可否を明示する。初期実装はnearest sampleでもよいが、仕様として固定する。
- 毎フレーム再読込ではなく、サンプルキーとメッシュ世代を使ったキャッシュを設ける。
- 再生中の更新とseek／逆再生／停止時の再評価を分ける。
- 初期版では、既存の `loadFromFileAtTime()` を経由する最小実装と、Alembic専用reader／sample cacheを持つ実装を混同しない。性能要件を満たせない場合に後者へ移行できる境界を先に保つ。

### Phase D: 複数オブジェクト・階層

- 単一 `Mesh` ではなく、オブジェクト名・親子関係・各メッシュを保持するモデルを検討する。
- `Artifact3DLayer`へ直接階層責務を追加する前に、Core側のscene/cache modelを分離する。
- マテリアル、visibility、camera、属性の扱いは別受入れ条件にする。

## 変更候補ファイル

### 初期準備・状態表示

- `ArtifactCore/include/Geometry/MeshImporter.ixx`
- `ArtifactCore/src/Geometry/MeshImporter.cppm`
- `ArtifactCore/src/File/FileTypeDetector.cppm`
- `ArtifactCore/src/Asset/AssetImporter.cppm`
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm`

### 静的レベル1の実装候補

- `ArtifactCore/src/Geometry/MeshImporter.cppm`
- `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 必要に応じて依存登録を行うArtifactCore／Artifact側CMake

依存追加やCMake変更は、ライブラリの採用決定後に限定する。新規モジュール追加より、既存 `MeshImporter` の実装側へ閉じる方針を優先する。

## 受入れ条件

### Phase A

- `.abc` が認識済みだが未importであることを診断できる。
- 失敗時に「未接続」「対象外機能」「破損／読込失敗」を区別できる。
- 既存のOBJ／FBX／glTF経路に影響を与えない。

### Phase B

- 静的な代表サンプルを `Mesh` として読み込める。
- Contents Viewerで表示できる。
- 読み込みbackend、サンプル時刻、頂点数、面数を確認できる。
- 対象外の複数オブジェクトや属性を成功扱いにしない。

### Phase C

- composition frameとAlembic timeの変換が保存・再読込後も一致する。
- seek、逆再生、停止、再生再開でメッシュ状態が破綻しない。
- 同じサンプルを毎回ディスクから読み直さない。

## 依存・リスク

- Alembic SDKの配布形態、ABI、Windows向けビルド、ライセンス確認が必要。
- Alembicのオブジェクト階層と既存の単一 `Mesh` モデルには構造差がある。
- トポロジーが時間で変化するキャッシュでは、GPUバッファ更新とbounds更新が毎サンプル必要になる可能性がある。
- スキニング済みモデルとAlembic point cacheを同じ経路で扱えるとは限らない。
- Diligent／GPU経路を変更する前に、既存 `Mesh` の更新APIで十分か確認する。GPUレンダラーの大規模変更は初期スコープに含めない。

## 次に確認する最小項目

1. 採用候補ライブラリのWindows配布・ABI・ライセンス条件。
2. 既存 `Mesh` が法線・UV・トポロジー更新をどこまで受け入れられるか。
3. `Artifact3DLayer` のフレーム更新入口と、既存の `importMeshFromFileAtTime()` の呼び出し箇所。
4. 代表的なAlembicサンプルを、静的メッシュ／変形キャッシュ／複数オブジェクトに分類すること。
5. レベル1を先に出すか、レベル2のキャッシュ再生までを一つの製品要件にするか。

## 追加調査: 外部ライブラリと既存器

- 公式AlembicリポジトリはC++ライブラリとファイル形式を提供し、現行READMEでは必須依存としてImath 3、任意依存としてHDF5を挙げている。OgawaとHDF5の両方を扱うかは、採用時に配布サイズとサンプル互換性を含めて決める。
- 既存 `Mesh` は静的ジオメトリの変換先として十分な表現を持つ。ただし、Alembicの複数オブジェクト階層を単一 `Mesh` に潰すと、後のobject path選択や部分更新へ拡張しにくい。
- 最初の実装では `AlembicStaticSample` 相当の内部境界を想定し、readerが返すデータを既存 `Mesh` へ変換する責務と、レイヤーが時間を管理する責務を分ける。

### 根拠リンク

- [Alembic公式リポジトリ](https://github.com/alembic/alembic)
- [Alembic公式ドキュメント](https://docs.alembic.io/)

## 未検証

- Alembic実ファイルの読み込み成功・失敗。
- 採用ライブラリの実ビルド、ランタイム配布、ABI互換性。
- 既存Meshのトポロジー差し替え性能。
- D3D12／Vulkan双方でのキャッシュ再生時のGPU更新負荷。
