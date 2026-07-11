# M-ASSET-1 Asset Instance Sharing Milestone

作成日: 2026-06-16
ステータス: In Progress
対象: `ArtifactCore/include/Asset/AssetImporter.ixx`,
      `ArtifactCore/include/Asset/AssetDatabase.ixx`,
      `ArtifactCore/src/Asset/AssetManager.cppm`,
      `ArtifactCore/src/Asset/AssetImporter.cppm`,
      `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Layer/ArtifactVideoLayer.cppm`,
      `Artifact/src/Layer/ArtifactImageLayer.cppm`,
      `Artifact/src/Service/ArtifactAssetService.cppm`,
      `Artifact/src/Render/GPUTextureCacheManager.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `Artifact/src/Undo/*`
位置づけ: 同じ source を参照する複数 layer が **1 個の decoded payload を共有** する foundation を、`ArtifactCore` 側の `AssetInstance` 抽象と `Artifact/` 側の UI 導線で導入する。
参照:
- `docs/analysis/MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md` (🟠)
- `docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md` (🟠)
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §1
- `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`
- `docs/planned/MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md`
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md` (linear premultiplied canonical)
- `docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md`

---

## 2026-07-12 Foundation progress

- `AssetDatabase::registerAsset()` と `findAssetByPath()` を同一のsource identity正規化へ統一した。
- 既存ファイルはcanonical path、未作成・missing pathはabsolute clean pathをidentityに使う。
- Windowsでは大小文字差をcase-foldし、区切り・相対/絶対・case差による重複UUIDを防ぐ。
- database読込時も同じ正規化を通し、空ID、空path、同一identityの重複entryを除外する。
- これはdecoded payload registry導入前の不変条件であり、既存APIは変更していない。
- 既存 `AssetManager` をsource registryの正規入口にし、`acquireSource / releaseSource / sourceId / useCount` を追加した。
- source UUID単位の単調増加versionを持ち、`invalidateSource()` でdecode/GPU cacheが共有できる世代境界を追加した。
- registry stateはmutexで保護し、release underflowとnull UUIDを拒否する。
- payload型はまだ保持せず、Core registryへQt画像/GPU backend型を流入させない境界を維持した。
- `ArtifactImageLayer` / `ArtifactVideoLayer` / `ArtifactAudioLayer` のload、reload、clear、destructionをsource leaseへ接続した。
- Image/Audioはロード成功時だけleaseを切り替え、失敗時は既存source leaseを維持する。
- Videoは既存の非同期open契約に合わせ、open世代開始時にleaseを切り替える。
- 同一canonical sourceを参照するlayer数を `AssetManager::useCount()` から取得できる状態になった。
- `AssetManager` に `asset UUID + source version + representation` keyedのgeneric decoded payload cacheを追加した。
- cacheはweak ownershipとし、Layerがpayload寿命を所有するためregistry起因の常駐を避けた。
- `invalidateSource()` はversion更新と同時に旧version payload keyを除去する。
- `ArtifactImageLayer` はrender本流の `ImageF32x4_RGBA` を共有cacheから取得し、decode完了時にversion一致を確認して公開する。
- Image Layerはsource UUID/versionと未加工条件を公開し、GPU upload側が共有可否を判定できるようにした。
- Composition previewとcomposition viewのGPU cache owner/keyを、未加工Imageに限って `asset UUID + version` へ切り替えた。
- effect / mask / matte / source cropがあるsurfaceは従来のlayer-local GPU keyを維持する。
- source version更新後は新keyでuploadされるため、旧GPU entryを再利用しない。
- source registryにJSON snapshot/restoreを追加し、stable asset UUID、canonical path、type、versionを永続化する。
- use countとdecoded/GPU payloadは保存せず、Layer復元時にleaseから再構築する。
- Project JSONの `assets.sourceRegistry` に保存し、Composition/Layer生成前に復元する。
- 旧Projectの `assets.sourceRegistry` 欠落は正常として扱い、従来どおりsource pathから再構築する。
- source health snapshotにpath、type、version、use count、live payload countを公開した。
- ProjectHealthCheckerがLayer sourceのmissing path、orphan source、invalid versionを検出する。
- Problem View変換でmissingをFile、orphanをPerformance、versionをConfigurationへ分類し、修復ヒントを追加した。
- 既存 `replaceLayerSourceInCurrentComposition()` をImage/Video/Audio/SVG共通のRelink正規入口として維持した。
- Relinkを `ReplaceLayerSourceCommand` 経由へ移し、source leaseとcache更新を含むUndo/Redoに対応した。
- 新sourceは実在するlocal fileに限定し、空path、missing path、同一path、非media layerを拒否する。
- Core registryに同一pathのまま独立UUIDを持つlocalized source identityを追加した。
- Localize時はuse countをshared identityからlocalized identityへ移し、既存decoded payloadを初期共有する。
- localized identityは独立version/keyを持つため、以後のinvalidateとGPU cacheをshared sourceから分離できる。
- Image LayerのLocalize / Relink SharedをService + Undo commandへ接続した。
- Image JSONにsource UUID/localized状態を保存し、Project registry復元後に同じidentityを再取得する。
- Image固有 `fromJsonProperties()` を追加し、source path、fit、source cropの復元漏れも修正した。
- Video / AudioにもLocalize / Relink Shared、source UUID/localized JSON復元を展開した。
- ProjectServiceのLocalize入口はImage/Video/Audio共通となり、同じUndo commandを再利用する。
- Video LayerFactoryは専用 `ArtifactVideoLayer::fromJson()` を正規復元経路にし、playback/proxy/audio/crop設定の復元漏れを防いだ。
- Image/Video/AudioのProperty Editorに `Localized Source` と `Source Uses` を追加した。
- Localized toggleはpreview mutationを行わず、commit時だけProjectServiceのUndo対応Localize/Relink Sharedへ流す。
- use countはregistry由来の診断表示値として公開し、Layer側での直接mutationは拒否する。
- Project ViewのFootage行メタデータと選択詳細に、`AssetManager::useCount()` 由来の `Source Uses` を追加した。表示値は保存せず、source registryから再計算する。
- Asset Browserのファイル行表示、ツールチップ、選択詳細にも `Source Uses` を追加し、filesystem探索とproject source leaseの状態を同じ行から確認できるようにした。
- `ArtifactAudioLayer` のdecoded PCMを `asset UUID + source version + audio.pcm.f32` の共有payloadへ移し、resample結果と時間窓cacheはlayer-localのまま維持した。
- `ArtifactVideoLayer` の初期フレームと通常の非同期F32フレームを `asset UUID + source version + video.f32.frame` の共有payloadへ接続し、layer-localのLRUはフォールバック／時間窓として維持した。
- Composition ViewのVideo GPU fallback cache keyも `asset UUID + source version + frame` に揃え、同じsource/frameのGPU entryをlayer間で共有するようにした。
- Source/diff checked only. Build / runtime verification is intentionally deferred.

## 1. 目的

`MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md` の 🟠:

> 同じアセットを5回タイムラインに置くと、5回分全部メモリにロードされる
> 1回だけ置いて何度も複製する。編集すると全部に反映されてしまうので最後に一個ずつ切り離す

これは **AE 風の "share source" がない**ことが根本原因。

現状の `ArtifactAbstractLayer` は `sourcePath_` を持つが、各 layer が **独自に** `AssetImporter` を呼んで **独自に** decode / upload する。同じ source を 5 個の layer が参照しても 5 個の decoded payload がメモリに乗り、5 個の GPU texture upload が発生する。

AE の挙動は:
- 同じ source への参照は **1 個の decoded payload を共有**
- source を編集すると **全 layer に伝播**（=「インスタンス」）
- 切り離し（`Unlink` / `Convert to Local`）で **独立した decoded payload** を持つ
- 同じ source を参照するかどうかは `ProjectItem.useCount` 相当で見える化

この milestone は **AE 風の instance 共有** の foundation を 1 つの表にまとめる。`QImage` の hot path 流入は禁止し、`ImageF32x4RGBAWithCache` 等の既存 typed image に寄せる。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactCore/include/Asset/AssetImporter.ixx`
  - `static QUuid importFile(const QString& filePath)` — ファイルを import して **asset UUID を返す** 単発 API
  - `static bool isSupported(const QString& extension)`
  - `static AssetType detectType(const QString& filePath)`
- `ArtifactCore/include/Asset/AssetDatabase.ixx`
  - `AssetInfo { id (QUuid), name, absolutePath, type, metadata }`
  - `registerAsset(path, type) -> id`
  - `findAssetByPath(path) -> id` — **同じ path に対して同じ id を返す**（=ここで「source 共有」の下地はある）
- `ArtifactCore/src/Asset/AssetManager.cppm` (24 行) — ほぼ空の PImpl
- `Artifact/src/Layer/ArtifactVideoLayer.cppm` — `sourcePath_` を持つ。**独自に decode** する
- `Artifact/src/Layer/ArtifactImageLayer.cppm` — 同様
- `Artifact/src/Render/GPUTextureCacheManager.cppm` — GPU texture cache

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Instance データモデル | なし。`AssetInfo` の UUID は project 単位。layer 単位の instance 概念がない | 共有の単位が曖昧 |
| Reference count | なし。layer が 5 個あっても独立 | 5 個分のメモリ |
| Decoded payload cache | 各 layer が独自に `QImage` / `ImageF32x4_RGBA` を保持 | 5 回 decode + 5 回 upload |
| Unlink / Relink | なし | 共有から独立にできない |
| useCount 表示 | なし | 「このアセット何個で使われてる」不明 |
| GPU cache key | `cacheKey` が layer 単位。instance 単位でない | 5 layer → 5 GPU entry |
| Project 保存 | `sourcePath_` のみ。instance 概念は保存されない | 復元時に共有状態が再現不能 |
| Diagnostics | `useCount=0` の孤立 asset / `sourcePath` missing の警告が薄い | 壊れた link を見過ごしやすい |

### 2.3 既存 milestone との関係

- `MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md` — 既に layer-local mask / matte / effect result の cache あり。本 milestone は **decoded payload** の cache を instance 単位で共有する **上位**
- `MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md` — GPU effect。本 milestone は **source decode / upload** 側で effect には触れない
- `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` — diagnostics 文法。本 milestone Phase 5 が contribution
- `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` — `ImmediateContext` を widget から触らない方針。Instance 共有は texture cache 側で完結

---

## 3. 設計の柱

### 3.1 AssetInstance データモデル

`ArtifactCore/include/Asset/AssetInstance.ixx` を新規追加:

```cpp
namespace ArtifactCore {

// 共有の最小単位
struct AssetInstanceKey {
    QUuid assetId;        // AssetDatabase の id
    int   decodedVersion; // decode 結果の version。source 更新で increment
    QSize pixelSize;      // decode 後のサイズ
    AssetType type;
};

class AssetInstance {
public:
    AssetInstance(AssetInstanceKey key);

    // payload
    const std::shared_ptr<ImageF32x4RGBAWithCache>& decodedF32() const;
    void setDecodedF32(std::shared_ptr<ImageF32x4RGBAWithCache> v);

    // 参照カウンタ
    int  refCount() const;
    void ref();
    bool unref();   // true when refCount reaches 0

    // GPU side
    const AssetInstanceKey& key() const;

private:
    AssetInstanceKey key_;
    std::shared_ptr<ImageF32x4RGBAWithCache> decodedF32_;
    std::atomic<int> refCount_{0};
};

} // namespace ArtifactCore
```

- **`decodedF32_` は `std::shared_ptr`** で保持
- payload 型は `ImageF32x4RGBAWithCache` を使う。**`QImage` を内部に持たない**（`RENDER_FORMAT_CONTRACT_2026-05-16.md` の canonical: linear premultiplied float）
- audio / video は別の instance 派生（Phase 2 で追加）
- `decodedVersion` は source 更新検出用。`AssetDatabase::findAssetByPath` が version を bump する

### 3.2 AssetInstanceRegistry

`ArtifactCore/include/Asset/AssetInstanceRegistry.ixx` を新規追加:

```cpp
class AssetInstanceRegistry {
public:
    static AssetInstanceRegistry& instance();

    // 取得
    std::shared_ptr<AssetInstance> acquire(QUuid assetId, QSize size, AssetType type);
    std::shared_ptr<AssetInstance> acquireByPath(const QString& absPath, QSize size, AssetType type);

    // 解放
    bool release(QUuid assetId, QSize size, AssetType type);

    // 参照
    std::shared_ptr<AssetInstance> find(QUuid assetId, QSize size, AssetType type) const;

    // diagnostics
    int  useCount(QUuid assetId) const;             // 何個の layer が参照しているか
    QList<AssetInstanceKey> orphaned() const;       // useCount=0 で残っている instance

    // project save / restore
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& obj);

    // source 更新通知
    void onAssetChanged(QUuid assetId);             // ref++ した instance を invalidate
};
```

- `acquire` は `find` ヒットで ref++、ミスで新規作成
- `release` は ref--。0 なら `orphaned()` に積む
- **layer 側が直接 `shared_ptr` を保持** し、`unload` トリガは `release` の副作用
- project 復元時は `fromJson` で instance を再生

### 3.3 ArtifactAbstractLayer への instance 導入

- `ArtifactAbstractLayer` に **`sourceInstanceId_` (QUuid)** を追加
- 既存 `sourcePath_` は **温存**。`sourceInstanceId_` は `sourcePath_` から派生
- `setSourcePath(path)` で:
  1. `AssetDatabase::findAssetByPath(path)` で `assetId` を取得
  2. `AssetInstanceRegistry::acquireByPath(path, size, type)` で `shared_ptr<AssetInstance>` を取得
  3. `sourceInstanceId_` に key を保存
- `releaseInstance()` で既存 instance を release
- **`ArtifactVideoLayer` / `ArtifactImageLayer` / `ArtifactAudioLayer`** はこの基底 API を使う

### 3.4 GPU 経路

- `GPUTextureCacheManager::cacheKey` を **`{instanceId, decodedVersion, size}` ベース** に変更
- 同じ instance を参照する 5 個の layer は **同じ GPU texture エントリ** を共有
- 既存の `cacheKey` 構築は layer ID を含む。**instance 単位に切り替える**
- layer 側の `invalidateLayerSurfaceCache()` は **instance の shared_ptr を保持** していれば不要

### 3.5 Unlink / Relink

- `LayerService::unlinkAsset(layerId, newPath)` 追加
  - 既存 `sourceInstanceId_` を release
  - `newPath` から新規 instance を acquire
  - `QUndoCommand` 派生 `UnlinkAssetCommand` / `RelinkAssetCommand` を `Artifact/Undo/` に追加
- Inspector の source path 横に **`Unlink` / `Relink`** ボタンを追加

### 3.6 useCount 表示

- Project View / Asset Browser の asset row に **useCount バッジ** を追加
- `useCount == 0` の asset は `orphaned` 表示 + `Delete` / `Relink` ボタン
- Tooltip に「used by 5 layers」を出す

### 3.7 Project 永続化

- `ArtifactProjectManager` の project JSON に `assets.instances[]` セクション追加
- 各 instance:
  - `instanceId`
  - `assetId`
  - `pixelSize`
  - `decodedVersion` (最後に復元した version)
  - `placeholder: bool` (payload 復元前なら true)

```json
{
  "assets": {
    "instances": [
      {
        "id": "inst_001",
        "assetId": "asset_001",
        "size": "1920x1080",
        "decodedVersion": 1,
        "placeholder": true
      }
    ]
  }
}
```

- 復元時は `placeholder=true` で `AssetInstanceRegistry` に空 entry を作り、layer 側で **lazy decode** する
- `fromJson` は `decodedVersion` 不一致で invalidate

### 3.8 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `asset.orphan` (severity=info, `useCount==0` で残った instance)
- `asset.path-missing` (severity=error, `sourcePath_` が disk 上に存在しない)
- `asset.version-mismatch` (severity=warning, `decodedVersion` が `AssetDatabase` と不一致)
- `asset.duplicate-decode` (severity=info, 同一 path に対し instance 1 個であるべきところ複数)
- `asset.gpu-leak` (severity=warning, instance release 後に GPU texture だけ残存)

### 3.9 不変条件 (Guardrails)

- `QImage` を **新規 hot path に入れない**。`ImageF32x4RGBAWithCache` に寄せる
- 既存 `AssetImporter` / `AssetDatabase` の API は **温存**
- 共有された instance の **mutation は全 layer に伝播**（AE と同じ挙動）
- 切り離し（`Unlink`）は明示操作のみ。暗黙の unlink 禁止
- 1 instance あたりの layer 参照上限は 1024 推奨
- GPU texture 単位でも instance 共有が成立すること
- 共有 instance の `setDecodedF32` は **version bump とペア**
- 新規 signal-slot 接続の追加は **設計レビュー必須**。`AssetInstance` 自体は `W_OBJECT` 派生ではなく、POD 寄りの plain class にする
- 既存 `setStyleSheet` / 新規 `QImage` 流入禁止

---

## 4. フェーズ計画

### Phase 1: Core data model + registry (P0, 1〜2 セッション)

- `ArtifactCore/include/Asset/AssetInstance.ixx` 新規
- `ArtifactCore/include/Asset/AssetInstanceRegistry.ixx` 新規
- `ArtifactCore/src/Asset/AssetInstance.cppm` 実装
- `ArtifactCore/src/Asset/AssetInstanceRegistry.cppm` 実装
- `CMakeLists.txt` に新規ファイルを登録（既存 target を壊さない）

**Done criteria:**
- `acquire / release / find` が unit test で動作
- `refCount` が atomic に増減
- `fromJson / toJson` が round-trip
- `useCount / orphaned` が diagnostics 用に expose

### Phase 2: Layer 側 instance 移行 (P0, 2〜3 セッション)

- `ArtifactAbstractLayer::sourceInstanceId_` 追加
- `setSourcePath` で `AssetInstanceRegistry` 経由
- `ArtifactVideoLayer` / `ArtifactImageLayer` / `ArtifactAudioLayer` を移行
- 既存 `sourcePath_` ベースの動作を保ちつつ **内部で instance 共有** が成立

**Done criteria:**
- 同じ source を持つ 2 個の layer を timeline に置くと、`AssetInstanceRegistry::find` で同じ `shared_ptr` を返す
- `useCount == 2` が Diagnostics に反映
- 既存プロジェクトの読み込みで instance が `placeholder=true` として復元

### Phase 3: GPU cache 共有 (P0, 1〜2 セッション)

- `GPUTextureCacheManager::cacheKey` を instance ベースに変更
- 同じ instance を参照する 5 layer が同じ GPU entry を共有
- 既存 `invalidateLayerSurfaceCache` の挙動を維持しつつ、instance 単位で invalidate

**Done criteria:**
- 5 個の同じ source layer で GPU texture 数が 1 個になる
- `asset.gpu-leak` の warning が出ない
- source を更新すると 5 layer 全てが新しい texture を見る

### Phase 4: Unlink / Relink + UI (P0, 1〜2 セッション)

- `LayerService::unlinkAsset` / `relinkAsset` 追加
- `UnlinkAssetCommand` / `RelinkAssetCommand` 追加
- Inspector の source path 横に `Unlink` / `Relink` ボタン
- Project View / Asset Browser の asset row に useCount バッジ

**Done criteria:**
- `Unlink` 後に 2 layer の片方を編集しても他方に反映されない
- `Relink` 後に再び共有される
- `useCount` バッジが Asset Browser に表示される

### Phase 5: Project 保存 + Diagnostics 統合 (P0, 1 セッション)

- `ArtifactProjectManager` の project JSON に `assets.instances[]` 追加
- 復元時の lazy decode
- Problem View への `asset.*` 健全性 contribution
- `FrameDebugSnapshot.resources` に `label=Asset Instance Health` を追加

**Done criteria:**
- project 保存 → 再読込で instance が完全復元
- 旧プロジェクトは `assets.instances` 欠落を許容
- `asset.orphan` / `asset.path-missing` が Problem View に出る

### Phase 6: Audio / video / image 派生 (P1, 1 セッション)

- `AssetAudioInstance` / `AssetVideoInstance` 派生を追加
- 既存の `ArtifactAudioLayer` / `ArtifactVideoLayer` を専用派生に切替
- GPU upload 経路は video 側で `ImageYUV420` 等

**Done criteria:**
- audio も同じ instance 共有が成立
- video frame の decode は instance 単位で cache
- 派生型の mix が可能

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md` | 下位 cache。本 milestone は source 側の cache 共有で補完。 |
| `MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md` | GPU effect。本 milestone は effect 入力 (source decode) 側。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | texture cache は layer / widget から触らない方針。instance 共有は registry 側で完結。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |
| `MILESTONE_MARKER_FOUNDATION_2026-06-16.md` | 別 topic。 |
| `MILESTONE_LUT_BROWSER_2026-06-16.md` | 別 topic。 |
| `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` | 別 topic。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **shared_ptr の寿命**。layer 側が `shared_ptr<AssetInstance>` を保持し、`AssetInstanceRegistry` も保持する。cycle を避けるため registry 側は `weak_ptr` で参照する。`acquire / release` 時に `weak_ptr.lock()` で昇格する設計
2. **Version bump**。`AssetDatabase` の `version` を誰が管理するか。`onAssetChanged` トリガを `AssetImporter` の `importFile` 完了時に発火する
3. **GPU 共有と `ImageF32x4` の canonical**。`RENDER_FORMAT_CONTRACT_2026-05-16.md` の linear premultiplied と整合させる。video の場合は YUV → float linear への明示的 path
4. **Lazy decode**。project 復元時に `placeholder=true` だと **最初のアクセスで decode**。これが UI 応答を悪化させないよう、`QFuture` ベースの非同期 decode を検討
5. **Unlink 後の source**。`newPath` を指定せず Unlink すると source が空になる。`sourcePath` を維持しつつ reference を切る `Localize` モードを別途用意するか、Phase 4 で決定

### 6.2 契約上の未解決

- **同期 vs 非同期 decode**。動画など重い source は decode が遅い。`shared_ptr` への書き込みを誰が担当するか。Phase 6 で決定
- **Audio instance**。audio は decoded payload が sample 配列。`AssetAudioInstance` 派生で PCM buffer を `std::shared_ptr<AudioSegment>` で保持
- **Source 取り違え**。`sourcePath_` が **同じ path だが** 内容が更新された場合、instance は新 version で再 decode。`AssetDatabase` 側で hash 比較するかどうか
- **`AssetType` 拡張**。PSD / EXR / Lottie などが今後増えたとき、`AssetInstanceRegistry` 側の type dispatch をどうスケールさせるか
- **メモリ上限**。全 instance の合計 decoded payload サイズが閾値超えで自動 unload する policy。Phase 5 で `FastSettingsStore` のキー化

### 6.3 サブモジュール境界

- `ArtifactCore/src/Asset/AssetManager.cppm` (24 行) は PImpl の素体。本 milestone で `AssetInstanceRegistry` とは別ファイルで共存させる
- `ArtifactCore/CMakeLists.txt` に新規ファイルを登録。既存 target を壊さない
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- 同じ source の 5 個の layer が `AssetInstanceRegistry::find` で同じ `shared_ptr` を返す
- 5 layer の `useCount` が Diagnostics に反映
- 5 layer の GPU texture 数が 1 個
- `Unlink` 後に 1 layer を編集しても他 layer に反映されない
- `Relink` 後に再び共有される
- project 保存 → 再読込で instance が完全復元
- 旧プロジェクトは `assets.instances` 欠落を許容
- Problem View に `asset.orphan / asset.path-missing / asset.version-mismatch / asset.duplicate-decode / asset.gpu-leak` が出る
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- `ArtifactWidgets` を触っていない
- `ArtifactCore` への bump 手順が `.github/GIT_WORKFLOW_PARENT_CHILD.md` に整合

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §1 を正式 milestone に起こした。
