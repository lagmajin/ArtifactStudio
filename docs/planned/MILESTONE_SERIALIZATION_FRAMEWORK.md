# MILESTONE: Unified Serialization Framework

**日付**: 2026-08-04
**最終更新:** 2026-08-05
**今回の移行:** 既存の Frame / NLE / Coordinate / Template / Parametric Composition / Export Matrix 系は JSON adapter と Registry 経由の typed envelope を利用する状態を維持。`UiLayoutState` と `ArtifactEffectPreset` を `ISerializable` / Registry へ移行し、それぞれ migration 登録を追加。新規対象を推測で追加せず、復元 API が存在する型から段階移行する方針を明記。
**調査メモ (2026-08-05):** Phase 5 のP0候補として記載されている `LayerMatteReference` / `PropertyKeyframeSet` は現ツリーの型名として確認できなかった。キーフレーム関連は `QJsonArray` や用途別構造体に分散しているため、対象型を特定せずに移行を進めない。
**実装状況:** Phase 1 の共通契約（`ISerializable`）、Phase 2 のマイグレーション登録基盤、Phase 3 の型レジストリと共通エンベロープ、Phase 4 のJSON/CBOR文書I/O・分割ドキュメントストア・`ProjectSerializer` Facadeを追加。`ProjectSerializer::saveSerializable/loadSerializable` による typed envelope API、スレッドセーフな型・migration registry も実装済み。`ArtifactProjectExporter` / `ArtifactProjectImporter` / `ArtifactProjectPackager` の単一文書入出力をFacadeへ接続し、Packager/Importer は `manifest.json` + root document + composition documents の分割形式にも対応した。直接登録11件（Project/Layer/Composition/FX defaults/UI/Session）、JSON adapter51件（AI/Color/Frame/NLE/Coordinate/Template/Parametric Composition/Export Matrix/FrameDebug/Composition Variant/Composition Layout/Audio Reactive/Matte/Shape/Rig/SurfaceFX系、配列 payload adapter を含む）、v0→v1 migration62件を監査で確認済み。文書サイズ上限、manifest/document名検証、複数段階マイグレーション経路探索も実装済み。レイヤー・アセット等の更なる分割と既存型の全面移行は未完了。
**移行メモ:** `ArtifactProject` は現状 `toJson()` のみでインスタンス復元APIがないため、統一契約への移行対象としては保留。復元責務を先に定義してから移行する。既存メンバー名が契約メソッドと衝突する型（`ColorPalette`）はJSONアダプター経由で登録する。
**現状**: 既存型の多くは独自の `toJson()`/`fromJson()` を使用している。共通契約・マイグレーション・型レジストリの基盤は追加済みで、Exporter/Importer/Packager の単一文書経路と、Packager 設定時の composition 分割経路を接続済み。既存型の全面移行、レイヤー/アセット分割、typed envelope のプロジェクト実利用は未完了。既定のプロジェクトファイルは単一 JSON。
**目標**: `ISerializable` 統一インターフェース、スキーマバージョン管理、前方互換マイグレーション、任意型のシリアライズレジストリ。

## 現状の問題

| 問題 | 影響 |
|------|------|
| 統一インターフェース不在 | 全クラスが独自実装。書式・エラー処理がバラバラ |
| バージョン移行不在 | 新バージョンで旧ファイルが読めない（受入/拒否のみ） |
| 型安全でない | `QVariant` / `QJsonObject` の動的キャストだらけ |
| 全プロジェクトが単一 JSON | 大規模プロジェクトで数100MBの JSON。分割不可 |
| バイナリフォーマット不在 | CBOR は設定のみ。プロジェクトデータに高速読込不可 |

## 設計方針

- Qt 依存を最小限に（Core 層で完結、QJson は Artifact 層のみで使用）
- バイナリフォーマットは CBOR（`QCborValue`）を標準とする。JSON はテキスト可読用
- マイグレーションは宣言的：各バージョン間の変換を個別関数として登録
- 型登録レジストリで多相デシリアライズを実現

---

## Phase 1: ISerializable 統一インターフェース

### 1.1 基底インターフェース

```cpp
// ArtifactCore/include/Serialization/ISerializable.ixx
namespace ArtifactCore::Serialization {

// シリアライズ可能な全型の基底
class ISerializable {
public:
    virtual ~ISerializable() = default;
    
    // シリアライズ — 型固有のデータを出力
    virtual void serialize(WriteContext& ctx) const = 0;
    // デシリアライズ — 型固有のデータを読み取り
    virtual void deserialize(ReadContext& ctx) = 0;
    
    // 型識別子（レジストリ用）
    virtual std::string_view typeName() const = 0;
    // 現在のスキーマバージョン
    virtual int schemaVersion() const = 0;
};

// 値型（コピーでシリアライズ可能）用
template<typename T>
concept SerializableValue = std::copyable<T> && requires(T v, WriteContext& w, ReadContext& r) {
    { v.serialize(w) } -> std::same_as<void>;
    { T::deserialize(r) } -> std::same_as<T>;
};

} // namespace ArtifactCore::Serialization
```

### 1.2 ReadContext / WriteContext

```cpp
// ArtifactCore/include/Serialization/SerializationContext.ixx

// バイナリ書出（CBOR デフォルト）
class WriteContext {
public:
    explicit WriteContext(Format format = Format::Cbor);
    
    // プリミティブ
    void writeInt(int64_t value);
    void writeUInt(uint64_t value);
    void writeFloat(double value);
    void writeBool(bool value);
    void writeString(std::string_view value);
    void writeBytes(const uint8_t* data, size_t size);
    
    // 構造化
    void beginObject();
    void endObject();
    void beginArray();
    void endArray();
    void writeKey(std::string_view key);
    
    // 型情報
    void writeTypeName(std::string_view typeName);
    void writeSchemaVersion(int version);
    
    // 生データアクセス
    std::vector<uint8_t> toBuffer() const;
    QJsonObject toJson() const;  // JSON 可読出力（デバッグ用）
    
    // ファイル出力
    bool writeToFile(const QString& path);
    
private:
    QCborStreamWriter writer_;
    Format format_;
};

class ReadContext {
public:
    explicit ReadContext(const std::vector<uint8_t>& buffer, Format format = Format::Cbor);
    explicit ReadContext(const QJsonObject& json);
    
    // プリミティブ
    int64_t readInt();
    uint64_t readUInt();
    double readFloat();
    bool readBool();
    std::string readString();
    std::vector<uint8_t> readBytes();
    
    // 構造化
    bool beginObject();
    void endObject();
    bool beginArray();
    void endArray();
    bool hasKey(std::string_view key);
    
    // 型情報
    std::string readTypeName();
    int readSchemaVersion();
    
    // エラー
    bool hasError() const;
    std::string errorMessage() const;
    
    static ReadContext fromFile(const QString& path);
    
private:
    QCborValue root_;
    // イテレータ状態...
};
```

### 1.3 具象型への適用例

```cpp
// 既存の toJson/fromJson パターンを置き換え
class LayerMatteReference : public ISerializable {
public:
    // ... 既存のゲッター/セッター ...
    
    void serialize(WriteContext& ctx) const override {
        ctx.writeString("layerId", layerId_.toString());
        ctx.writeString("channel", channelToString(channel_));
        ctx.writeFloat("opacity", opacity_);
        ctx.writeBool("inverted", inverted_);
    }
    
    void deserialize(ReadContext& ctx) override {
        layerId_ = QUuid::fromString(ctx.readString("layerId"));
        channel_ = channelFromString(ctx.readString("channel"));
        opacity_ = ctx.readFloat("opacity");
        inverted_ = ctx.readBool("inverted");
    }
    
    std::string_view typeName() const override { return "LayerMatteReference"; }
    int schemaVersion() const override { return 1; }
};
```

### 1.4 完了条件

- [x] `ISerializable` インターフェースが `ArtifactCore/include/Serialization/` に定義される
- [x] JSON/CBORの共通文書I/Oが動作する
- [x] オブジェクトの読み書きと形式自動検出を実装する
- [x] 分割ドキュメントの保存・読込を実装する
- [x] エラー時は `false` / `nullptr` で明示的に失敗する

---

## Phase 2: スキーマバージョン管理とマイグレーション

### 2.1 SchemaVersion と MigrationRegistry

```cpp
// ArtifactCore/include/Serialization/SchemaMigration.ixx
class SchemaMigrationRunner {
public:
    // バージョン間変換関数を登録
    // fromVersion=1, toVersion=2 の変換: QJsonObject v1 → QJsonObject v2
    using MigrationFn = std::function<QJsonObject(const QJsonObject&)>;
    
    void registerMigration(
        std::string_view typeName,
        int fromVersion,
        int toVersion,
        MigrationFn transform
    );
    
    // 登録された変換を適用して現在のバージョンに引き上げる
    // typeName=LayerMatteReference, dataVersion=1, currentVersion=3
    // → v1→v2 変換 → v2→v3 変換 を順次適用
    QJsonObject migrateToCurrent(
        std::string_view typeName,
        int dataVersion,
        const QJsonObject& data
    ) const;
    
    // マイグレーションパスが存在するか
    bool hasMigrationPath(std::string_view typeName, int fromVersion, int toVersion) const;
    
    // 利用可能な全バージョンを列挙
    std::vector<int> availableVersions(std::string_view typeName) const;

private:
    // typeName → {fromVersion → {toVersion → transform}}
    std::map<std::string, std::map<int, std::map<int, MigrationFn>>> migrations_;
};
```

### 2.2 マイグレーション登録例

```cpp
// プロジェクト起動時に登録
auto& runner = SchemaMigrationRunner::instance();

// LayerMatteReference v1 → v2（channel を文字列からintに変更）
runner.registerMigration("LayerMatteReference", 1, 2,
    [](const QJsonObject& v1) -> QJsonObject {
        QJsonObject v2 = v1;
        // "channel": "red" → "channel": 0
        static const std::map<QString, int> channelMap = {
            {"red", 0}, {"green", 1}, {"blue", 2}, {"alpha", 3}
        };
        v2["channel"] = channelMap.at(v1["channel"].toString());
        return v2;
    }
);

// LayerMatteReference v2 → v3（opacity の型を double→float に正規化）
runner.registerMigration("LayerMatteReference", 2, 3,
    [](const QJsonObject& v2) -> QJsonObject {
        QJsonObject v3 = v2;
        // 特に変換不要（JSON 上は同じ）だが、バージョンスタンプを更新
        v3["_schemaVersion"] = 3;
        return v3;
    }
);
```

### 2.3 プロジェクトロード時の自動マイグレーション

```cpp
// ArtifactProjectImporter のロード処理に統合
QJsonObject ArtifactProjectImporter::loadAndMigrate(const QString& path) {
    QJsonObject raw = loadJsonFile(path);
    
    int fileVersion = raw["version"].toString().toInt();
    int currentVersion = ArtifactProject::currentSchemaVersion();
    
    if (fileVersion > currentVersion) {
        throw ProjectVersionError("File was created with a newer version");
    }
    
    // プロジェクト全体のバージョン変換
    raw = SchemaMigrationRunner::instance()
        .migrateToCurrent("ArtifactProject", fileVersion, raw);
    
    // 各 composition 内の layer を再帰的にマイグレーション
    for (auto& comp : raw["compositions"].toArray()) {
        for (auto& layer : comp["layers"].toArray()) {
            QString typeName = layer["_type"].toString();
            int layerVersion = layer["_schemaVersion"].toInt(1);
            layer = SchemaMigrationRunner::instance()
                .migrateToCurrent(typeName, layerVersion, layer);
        }
    }
    
    return raw;
}
```

### 2.4 完了条件

- [x] マイグレーション登録とチェーン実行の基盤を実装する
- [x] マイグレーション不在時は明示的に失敗する
- [x] バージョン情報がエンベロープに付与される
- [x] 既存型3つでv0→v1移行を登録する
- [x] 型ごとの利用可能スキーマバージョンを列挙できる

---

## Phase 3: 型レジストリと多相デシリアライズ

### 3.1 SerializationRegistry

```cpp
// ArtifactCore/include/Serialization/SerializationRegistry.ixx
class SerializationRegistry {
public:
    static SerializationRegistry& instance();
    
    // 型登録
    template<typename T> requires std::derived_from<T, ISerializable>
    void registerType() {
        auto factory = +[](ReadContext& ctx) -> std::unique_ptr<ISerializable> {
            auto obj = std::make_unique<T>();
            obj->deserialize(ctx);
            return obj;
        };
        registerTypeInternal(T::staticTypeName(), factory);
    }
    
    // 多相デシリアライズ
    std::unique_ptr<ISerializable> deserialize(ReadContext& ctx) const;
    
    // 特定の型としてデシリアライズ
    template<typename T> requires std::derived_from<T, ISerializable>
    std::unique_ptr<T> deserializeAs(ReadContext& ctx) const {
        auto obj = deserialize(ctx);
        return std::unique_ptr<T>(dynamic_cast<T*>(obj.release()));
    }
    
    // 型情報の検証
    bool isRegistered(std::string_view typeName) const;
    std::vector<std::string> registeredTypes() const;

private:
    using FactoryFn = std::function<std::unique_ptr<ISerializable>(ReadContext&)>;
    std::map<std::string, FactoryFn, std::less<>> factories_;
};
```

### 3.2 多相コレクションのシリアライズ

```cpp
// 異種オブジェクトの配列をシリアライズ
template<typename Base>
void serializePolymorphicArray(
    WriteContext& ctx,
    std::string_view key,
    const std::vector<std::unique_ptr<Base>>& items)
{
    ctx.writeKey(key);
    ctx.beginArray();
    for (const auto& item : items) {
        ctx.beginObject();
        ctx.writeTypeName(item->typeName());
        ctx.writeSchemaVersion(item->schemaVersion());
        item->serialize(ctx);
        ctx.endObject();
    }
    ctx.endArray();
}

// 読み取り
template<typename Base>
std::vector<std::unique_ptr<Base>> deserializePolymorphicArray(ReadContext& ctx) {
    std::vector<std::unique_ptr<Base>> result;
    ctx.beginArray();
    while (ctx.hasNextElement()) {
        ctx.beginObject();
        auto obj = SerializationRegistry::instance().deserialize(ctx);
        result.push_back(std::unique_ptr<Base>(dynamic_cast<Base*>(obj.release())));
        ctx.endObject();
    }
    ctx.endArray();
    return result;
}
```

### 3.3 使用例 — コンポジションセーブ

```cpp
void Composition::serialize(WriteContext& ctx) const {
    ctx.writeString("id", id_.toString());
    ctx.writeString("name", name_);
    ctx.writeInt("width", width_);
    ctx.writeInt("height", height_);
    ctx.writeFloat("frameRate", frameRate_);
    
    // 異種レイヤーの多相配列
    serializePolymorphicArray<AbstractLayer>(ctx, "layers", layers_);
}

std::unique_ptr<Composition> Composition::deserialize(ReadContext& ctx) {
    auto comp = std::make_unique<Composition>();
    comp->id_ = QUuid::fromString(ctx.readString("id"));
    comp->name_ = ctx.readString("name");
    comp->width_ = ctx.readInt("width");
    comp->height_ = ctx.readInt("height");
    comp->frameRate_ = ctx.readFloat("frameRate");
    comp->layers_ = deserializePolymorphicArray<AbstractLayer>(ctx);
    return comp;
}
```

### 3.4 完了条件

- [ ] `SerializationRegistry` が全 ISerializable サブクラスを型名で検索できる
- [ ] 多相コレクションの save/load がラウンドトリップ成功
- [ ] 未知の型名を含むデータを読んだ場合のエラーハンドリングが明確
- [ ] 登録されていない型のシリアライズ試行がコンパイル時に検出される（concept 制約）

---

## Phase 4: バイナリフォーマット + プロジェクト分割保存

### 4.1 CBOR バイナリの標準化

プロジェクトファイルの拡張子を `.artifact`（CBOR バイナリ）とする。JSON はエクスポート/デバッグ用。

```cpp
// プロジェクト全体の保存形式
class ProjectSerializer {
public:
    // CBOR バイナリ保存（本番用）
    bool saveAsBinary(const QString& path, const ArtifactProject& project);
    
    // JSON 保存（可読・デバッグ用）
    bool saveAsJson(const QString& path, const ArtifactProject& project);
    
    // ロード（フォーマット自動検出）
    std::unique_ptr<ArtifactProject> load(const QString& path);
    
    // フォーマット検出
    static Format detectFormat(const QString& path);

private:
    void saveHeader(WriteContext& ctx, const ArtifactProject& project);
    void saveCompositions(WriteContext& ctx, const std::vector<CompositionPtr>& compositions);
    void saveAssets(WriteContext& ctx, const AssetDatabase& assets);
    void saveExtensionData(WriteContext& ctx, const QJsonObject& extensionData);
};
```

### 4.2 大規模プロジェクトの分割保存

```cpp
// プロジェクト構造
// MyProject.artifact/          ← ディレクトリ
//   project.json               ← プロジェクトメタデータ
//   compositions/              ← コンポジションごとに分割
//     00000000-0000-0000.cbor  ← composition UUID 単位
//     00000000-0001-0000.cbor
//   assets.json                ← アセットレジストリ
```

```cpp
class SplitProjectSerializer {
public:
    bool saveSplit(const QString& projectDir, const ArtifactProject& project);
    std::unique_ptr<ArtifactProject> loadSplit(const QString& projectDir);
    
    // 単一コンポジションの独立保存/読込
    bool saveComposition(const QString& path, const Composition& comp);
    std::unique_ptr<Composition> loadComposition(const QString& path);
    
    // 差分保存（変更されたコンポジションのみ）
    bool saveDelta(const QString& projectDir,
                   const std::vector<QUuid>& changedCompositionIds);
};
```

### 4.3 フォーマット自動検出

```cpp
ProjectSerializer::Format ProjectSerializer::detectFormat(const QString& path) {
    QFileInfo info(path);
    
    if (info.isDir()) {
        // .artifact/ ディレクトリ = 分割形式
        if (QFile::exists(path + "/project.json")) return Format::Split;
        return Format::Unknown;
    }
    
    // 単一ファイル = 先頭バイトで判定
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    QByteArray header = file.read(128);
    
    // CBOR の最初のバイトはメジャータイプで識別可能
    if (header.size() >= 1) {
        uint8_t firstByte = static_cast<uint8_t>(header[0]);
        uint8_t majorType = firstByte >> 5;
        if (majorType == 5) return Format::Cbor;  // CBOR map
    }
    
    // JSON は { で始まる
    if (header.trimmed().startsWith('{')) return Format::Json;
    
    return Format::Unknown;
}
```

### 4.4 完了条件

- [x] 共通文書 I/O が JSON / CBOR の保存・読込と形式自動検出を提供する
- [x] 分割保存された root document と composition documents を再構築する
- [x] 単一 JSON / 単一 CBOR / manifest 付き分割形式を自動検出する
- [x] `.artifact` 用の CBOR 保存・読込 Facade API（`saveArtifact/loadArtifact`）を提供する
- [x] `ArtifactProjectExporter` が `.artifact` 拡張子を CBOR 出力として扱う
- [ ] JSON から `.artifact` への専用移行コマンドを提供する

---

## Phase 5: 既存コードの段階的移行

### 5.1 移行戦略

全型を一度に置き換えるのではなく、以下の3段階で移行:

1. **新規型は ISerializable 必須**: 新しく作成するシリアライズ可能な型は必ず ISerializable を実装
2. **既存の toJson/fromJson をラップ**: ISerializable 実装が内部で既存の toJson/fromJson を呼び出すアダプター期間
3. **段階的にネイティブ移行**: アダプターを外し、直接 ReadContext/WriteContext を使用

```cpp
// アダプター例: 既存の toJson/fromJson を ISerializable に橋渡し
template<typename T>
class JsonSerializableAdapter : public ISerializable {
public:
    explicit JsonSerializableAdapter(T& obj) : obj_(obj) {}
    
    void serialize(WriteContext& ctx) const override {
        QJsonObject json = obj_.toJson();
        jsonToContext(ctx, json);  // QJsonObject → WriteContext 変換
    }
    
    void deserialize(ReadContext& ctx) override {
        QJsonObject json = contextToJson(ctx);  // ReadContext → QJsonObject 変換
        obj_.fromJson(json);
    }
    
    std::string_view typeName() const override { return T::staticTypeName(); }
    int schemaVersion() const override { return T::staticSchemaVersion(); }

private:
    T& obj_;
};
```

### 5.2 移行優先順位

| 優先度 | 型 | 理由 |
|--------|-----|------|
| P0 | `LayerMatteReference` | シンプル、他からの参照が多い |
| P0 | `Keyframe` / `PropertyKeyframeSet` | タイムラインデータの核心 |
| P1 | `Composition` / `AbstractLayer` | プロジェクトデータのルート |
| P1 | `RenderJob` / `RenderQueueEntry` | レンダーキューの永続化 |
| P2 | その他の Effect / Layer サブクラス | 末端の型 |

### 5.3 完了条件

- [ ] P0 の型3つ以上が ISerializable を直接実装
- [ ] プロジェクトの save/load がバイナリ形式で動作
- [ ] 旧 toJson/fromJson との相互運用が保証される（アダプター経由）
- [ ] 全テストが新旧両方のシリアライズ経路で通過

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `ArtifactCore/include/Serialization/ISerializable.ixx` | 新規: 基底インターフェース |
| P1 | `ArtifactCore/include/Serialization/SerializationDocument.ixx` | JSON/CBOR文書I/O |
| P2 | `ArtifactCore/include/Serialization/SchemaMigration.ixx` | 新規: マイグレーションランナー |
| P3 | `ArtifactCore/include/Serialization/SerializationRegistry.ixx` | 新規: 型レジストリ |
| P3 | `ArtifactCore/include/Serialization/SerializationEnvelope.ixx` | 型名・スキーマ付き共通エンベロープ |
| P3 | `ArtifactCore/include/Serialization/JsonSerializableAdapter.ixx` | 既存JSON型向けアダプター |
| P4 | `ArtifactCore/include/Serialization/SplitDocumentStore.ixx` | 分割ドキュメント保存 |
| P4 | `ArtifactCore/include/Serialization/ProjectSerializer.ixx` | プロジェクトシリアライザFacade |
| P5 | 既存の `toJson`/`fromJson` 各所 | アダプター追加・段階的ネイティブ移行 |
| P5 | `tools/serialization/audit_serialization.py` | ビルド不要の登録型監査とCI接続 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: インターフェース | **P0** | 中 | 全後続の基盤 |
| P2: マイグレーション | **P0** | 小 | Phase 1 の上に薄く追加 |
| P3: 型レジストリ | **P1** | 小 | レジストリパターンのみ |
| P4: バイナリ+分割 | **P1** | 中 | CBOR は既存、分割はファイルシステム操作が主 |
| P5: 既存移行 | **P2** | 大 | 段階的、他 Phase と並行可能 |
