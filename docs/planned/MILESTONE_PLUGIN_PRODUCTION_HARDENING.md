# MILESTONE: Plugin System Production Hardening

**日付**: 2026-08-04
**現状**: 3 つの並行プラグインシステム（ネイティブ ABI / VST+CLAP / OFX）の骨格が存在。しかし実際に動作するサードパーティプラグインはゼロ。サンプル・SDK・ドキュメント不在。
**目標**: 1 つのネイティブレイヤープラグインを完全にエンドツーエンドで動作させ、CLAP と OFX を実プラグインで検証する。

## 現行コード監査 (2026-08-15)

`PluginLoader`／`PluginLayerFactory`／`LayerPluginAdapter`、`PluginRegistry`、sandbox の native plugin 骨格は現行コードに存在する。ただし `tests/plugins/minimal_layer_plugin/MinimalLayerPlugin.cpp` の create／instance／draw 系 entry point は `nullptr` を返すスタブで、native layer の E2E 実装は未達である。

CLAP は `CLAPHost`、VST2 は `VSTHost`／`VSTEffect`、OFX は `ArtifactOfxHost`／`ArtifactOfxEffectImpl` に loader／instance／処理の主要経路がある。VST3 は loader／interfaces の骨格に留まり、実 SDK による処理経路は確認できない。実サードパーティ plugin、SDK／sample の配布契約、ABI／sandbox の安全境界、CLAP／OFX／VST の runtime 受入は未確認である。したがって native plugin E2E と各形式の実プラグイン検証は pending と判定する。

## Update 2026-08-15

現行コードを再照合した。`ArtifactPluginLoader` は API／export／plugin count を検証し、in-process DLL ロード失敗時に subprocess へフォールバックできる。`ArtifactPluginSandbox` には heartbeat、クラッシュ回数、上限到達時の失敗状態、停止処理がある。

- ただし `PluginLayerFactory::scanAndRegister()` は現在 `PluginLoadMode::DllInProcess` を明示しており、通常のネイティブレイヤー探索で sandbox を自動選択する経路は確認できない。
- サンプル native layer の create／instance／draw entry point は依然としてスタブのため、Loader → Layer → Composition View の E2E は未達。
- 署名検証、ABI compatibility の詳細な隔離、永続ブラックリスト／quarantine、権限制限、実 plugin の crash／reload 受入は未完了。CLAP／OFX の loader と処理骨格があることだけでは、本番 hardening 完了とは判定しない。

## 現状サマリ

| システム | DLL ロード | インスタンス化 | 処理 | 実プラグイン |
|---------|-----------|-------------|------|------------|
| **Native ABI (Layer)** | ✅ | ⚠️ スタブが null 返す | ⚠️ drawContent 未検証 | ❌ ゼロ |
| **Native ABI (Tool)** | ❌ consumer 不在 | ❌ | ❌ | ❌ |
| **Native ABI (Effect)** | ⚠️ 部分的 | ⚠️ | ⚠️ | ❌ |
| **VST2** | ✅ | ✅ | ✅ process() 動作 | ⚠️ 未テスト |
| **VST3** | ⚠️ プロジェクト独自スタブ | ❌ | ❌ 実SDK不在 | ❌ |
| **CLAP** | ✅ | ✅ | ✅ process() 動作 | ⚠️ 未テスト |
| **OFX** | ✅ | ✅ | ✅ render 動作 | ⚠️ 未テスト |

---

## Phase 1: ネイティブレイヤープラグイン E2E（最小実行可能パス）

### 1.1 サンプルプラグインの完成

`tests/plugins/minimal_layer_plugin/MinimalLayerPlugin.cpp` をスタブから機能実装に変更。

```cpp
#include "ArtifactPluginABI.h"
#include <cstring>
#include <cstdio>

// プラグイン状態
struct MyLayerState {
    float rotation = 0.0f;
    float scale = 1.0f;
    char name[64] = "Minimal Layer";
};

// drawContent: コンポジションビューに描画
static void my_drawContent(void* layerPtr, ArtifactDrawContext* ctx) {
    auto* state = (MyLayerState*)layerPtr;
    
    // OpenGL/Diligent がない場合はテキストのみ（検証用）
    ctx->drawText(ctx, 10, 10, state->name, 16);
    ctx->drawText(ctx, 10, 30, 
        state->rotation > 0 ? "Rotating" : "Static", 14);
}

// initialize: プラグインの初期化
static void my_initialize(void* layerPtr) {
    auto* state = (MyLayerState*)layerPtr;
    state->rotation = 0.0f;
    state->scale = 1.0f;
}

// update: 毎フレーム呼び出し
static void my_update(void* layerPtr, float deltaTime) {
    auto* state = (MyLayerState*)layerPtr;
    state->rotation += 45.0f * deltaTime;  // 45度/秒で回転
}

// serializeExtra: プロジェクト保存
static size_t my_serialize(void* layerPtr, char* buffer, size_t bufferSize) {
    auto* state = (MyLayerState*)layerPtr;
    if (buffer && bufferSize >= sizeof(MyLayerState)) {
        std::memcpy(buffer, state, sizeof(MyLayerState));
    }
    return sizeof(MyLayerState);
}

// deserializeExtra: プロジェクト読み込み
static void my_deserialize(void* layerPtr, const char* data, size_t dataSize) {
    auto* state = (MyLayerState*)layerPtr;
    if (data && dataSize >= sizeof(MyLayerState)) {
        std::memcpy(state, data, sizeof(MyLayerState));
    }
}

// プロパティグループ（Inspector に表示される）
static ArtifactPropertyGroup my_getPropertyGroups(void* layerPtr) {
    ArtifactPropertyGroup group;
    group.name = "Minimal";
    group.propertyCount = 2;
    group.properties = new ArtifactProperty[2]{
        {"Scale", ARTIFACT_PROPERTY_FLOAT, 0, 0.1f, 10.0f, 1.0f},
        {"Speed", ARTIFACT_PROPERTY_FLOAT, 1, 0.0f, 360.0f, 45.0f}
    };
    return group;
}

// プロパティ値の設定
static void my_setProperty(void* layerPtr, int propertyIndex, float value) {
    auto* state = (MyLayerState*)layerPtr;
    if (propertyIndex == 0) state->scale = value;
    // Speed は my_update の計算に使う
}

// プロパティ値の取得
static float my_getProperty(void* layerPtr, int propertyIndex) {
    auto* state = (MyLayerState*)layerPtr;
    if (propertyIndex == 0) return state->scale;
    return 45.0f;
}

// DLL エクスポート
extern "C" {

ARTIFACT_PLUGIN_EXPORT ArtifactPluginInfo ArtifactPlugin_GetInfo() {
    ArtifactPluginInfo info;
    info.apiVersion = ARTIFACT_PLUGIN_API_VERSION;
    info.pluginVersion = 1;
    info.name = "Minimal Layer Plugin";
    info.category = "Layer";
    info.author = "Artifact Team";
    info.description = "A minimal layer plugin for testing the plugin ABI.";
    return info;
}

ARTIFACT_PLUGIN_EXPORT void* ArtifactPlugin_CreateLayer(const char* pluginName) {
    if (std::strcmp(pluginName, "Minimal Layer") != 0) return nullptr;
    return new MyLayerState();
}

ARTIFACT_PLUGIN_EXPORT ArtifactLayerPluginVTable ArtifactPlugin_GetLayerVTable() {
    ArtifactLayerPluginVTable vt;
    vt.drawContent = my_drawContent;
    vt.initialize = my_initialize;
    vt.update = my_update;
    vt.serializeExtra = my_serialize;
    vt.deserializeExtra = my_deserialize;
    vt.getPropertyGroups = my_getPropertyGroups;
    vt.setProperty = my_setProperty;
    vt.getProperty = my_getProperty;
    vt.destroy = [](void* ptr) { delete (MyLayerState*)ptr; };
    return vt;
}

} // extern "C"
```

### 1.2 Consumer 配線 — Layer 作成導線

`PluginLayerFactory` がスキャンしたレイヤープラグインを、実際に Layer 作成メニューに接続する。

現状確認: `PluginLayerFactory::scanAndRegister()` は `ArtifactPluginLoader` の callback を受け取るが、その結果を Layer 作成 UI に接続するコードが不明。

**実装するコードパス:**

1. `ArtifactLayerFactory` (または `ArtifactCompositionEditor` のレイヤー作成メニュー) が `PluginLayerFactory::availablePlugins()` を呼び出す
2. 利用可能なレイヤープラグイン一覧を取得
3. `Layer → New → Plugin → [プラグイン名]` メニュー項目を動的に追加
4. 選択時に `PluginLayerFactory::createLayer(pluginId)` で ILayerPlugin インスタンスを生成
5. 生成されたレイヤーを現在のコンポジションに追加

**変更対象ファイル:**
- `Artifact/src/Widgets/Layer/ArtifactLayerMenu.cppm`（レイヤー作成メニュー）
- `Artifact/src/Plugin/PluginLayerFactory.cppm`（createLayer 確認）
- `Artifact/src/Layer/ArtifactLayerFactory.cppm`（プラグインレイヤー登録）

### 1.3 drawContent レンダリング検証

`ILayerPlugin::drawContent()` がコンポジションレンダリングパイプラインから実際に呼ばれることを確認。

1. `ArtifactCompositionEditor` がレイヤーを走査し、プラグインレイヤーを検出
2. `drawContent()` を呼び出す経路を実装（未実装の場合）
3. 最小の描画（テキスト描画）が Composition View に表示されることを目視確認

### 1.4 完了条件

- [ ] サンプルプラグイン DLL がビルドできる
- [ ] `plugins/layers/` に配置すると Artifact 起動時に自動スキャンされる
- [ ] `Layer → New → Plugin → Minimal Layer Plugin` がメニューに出現
- [ ] レイヤー作成 → Composition View に "Minimal Layer" テキストが表示
- [ ] Inspector に Scale プロパティが表示され、キーフレームアニメーション可能
- [ ] プロジェクト保存 → 再読み込みでレイヤー状態が復元される

---

## Phase 2: CLAP プラグイン実機検証

### 2.1 検証用 CLAP プラグインの準備

無料の CLAP プラグインを使用してテスト:
- **Surge XT** (オープンソースシンセ、CLAP 対応)
- **Chowdhury DSP** コレクション（無料エフェクト）

```cpp
// tests/clap_validation.cpp（新規テスト）
void test_clap_plugin_loads_and_processes() {
    CLAPHost host;
    auto* plugin = host.loadPlugin("Surge XT.clap");
    REQUIRE(plugin != nullptr);
    
    auto* instance = plugin->instantiate("Surge XT", 44100, 512);
    REQUIRE(instance != nullptr);
    
    // パラメータの取得
    REQUIRE(instance->paramCount() > 50);
    
    // オーディオ処理
    std::vector<float> input(512, 0.0f);
    std::vector<float> output(512, 0.0f);
    instance->process(&input[0], &output[0], 512);
    
    // 出力に変化があるか（少なくともゼロではない）
    bool hasOutput = false;
    for (float v : output) {
        if (std::abs(v) > 1e-10f) { hasOutput = true; break; }
    }
    REQUIRE(hasOutput);
}
```

### 2.2 Host コールバックの完成

`CLAPHost` のスタブコールバックを実装:

```cpp
// 現在スタブのコールバックを実装
void CLAPHost::hostRequestRestart() {
    // プラグインが restart を要求 → GUI 再構築
    emit pluginRequestedRestart(currentPluginId_);
}

void CLAPHost::hostRequestProcess() {
    // プラグインが即時処理要求 → オーディオスレッドにシグナル
    audioEngine_->requestProcessCycle();
}

void CLAPHost::hostRequestCallback() {
    // メインスレッドでのコールバック要求 → QTimer::singleShot で次回イベントループ
    QMetaObject::invokeMethod(this, &CLAPHost::onHostCallback, Qt::QueuedConnection);
}
```

### 2.3 完了条件

- [ ] 実 CLAP プラグイン（Surge XT 等）がロードできる
- [ ] `process()` が有効なオーディオ出力を生成する
- [ ] パラメータ列挙・読み取り・設定が動作
- [ ] プラグインが Audio Mixer にエフェクトとして追加できる

---

## Phase 3: OFX 実プラグイン検証

### 3.1 OFX プラグインテスト

無料 OFX プラグインでテスト:
- **TuttleOFX** (オープンソース OFX プラグインコレクション)
- **Natron** の内蔵 OFX プラグイン

```cpp
// tests/ofx_validation.cpp（新規テスト）
void test_ofx_blur_plugin() {
    ArtifactOfxHost host;
    auto plugins = host.scanDirectory("C:/Program Files/Common/OFX/Plugins");
    
    auto* blurPlugin = host.findPlugin("TuttleBlur");
    REQUIRE(blurPlugin != nullptr);
    
    // Describe
    host.describePlugin(blurPlugin);
    
    // Create instance
    auto* instance = host.createInstance(blurPlugin);
    REQUIRE(instance != nullptr);
    
    // テスト画像で render
    ImageF32x4 input(1920, 1080);
    ImageF32x4 output(1920, 1080);
    
    instance->setParam("size", 10.0f);
    instance->render(&input, &output);
    
    // ぼかし効果の確認（エッジが平滑化されている）
    float edgeSharpnessBefore = computeEdgeSharpness(input);
    float edgeSharpnessAfter = computeEdgeSharpness(output);
    REQUIRE(edgeSharpnessAfter < edgeSharpnessBefore);
}
```

### 3.2 GPU テクスチャ共有

現在 OFX render は CPU only。`ImageF32x4_RGBA` → Diligent テクスチャ → OFX の GPU 画像スイート の経路を追加:

```cpp
// OFX GPU path (新規)
bool ArtifactOfxEffect::applyGPU(
    Diligent::ITextureView* inputSRV,
    Diligent::ITextureView* outputRTV)
{
    if (!kOfxImageEffectSupportsTiles) {
        return false;  // プラグインが GPU 非対応
    }
    
    // OFX GPU suite 経由でテクスチャハンドルを渡す
    OfxImageEffectSuiteV1::clipGetImageGPU(...);
    OfxImageEffectSuiteV1::renderGPU(...);
    return true;
}
```

### 3.3 キーフレーム対応

`paramGetValueAtTime` / `paramSetValueAtTime` を実装し、アニメーションする OFX パラメータをサポート:

```cpp
OfxStatus ArtifactOfxHost::paramGetValueAtTime(
    OfxParamHandle param, OfxTime time, ...) 
{
    // composition の現在の frame → OfxTime にマッピング
    float value = evaluateParameterAtFrame(param, time);
    *returnValue = value;
    return kOfxStatOK;
}
```

### 3.4 完了条件

- [ ] 実 OFX プラグイン（TuttleOFX Blur 等）がロード・実行できる
- [ ] パラメータ変更が即座に render 結果に反映される
- [ ] 複数 OFX プラグインのスタックが動作する
- [ ] GPU 対応 OFX プラグインは GPU テクスチャで処理される

---

## Phase 4: プラグイン SDK と配布形式

### 4.1 SDK パッケージ

最小の SDK を構成:

```
artifact-plugin-sdk/
├── include/
│   └── ArtifactPluginABI.h        # プラグイン ABI（既存）
├── examples/
│   ├── minimal_layer/             # Phase 1 のサンプル
│   │   ├── CMakeLists.txt
│   │   └── MinimalLayerPlugin.cpp
│   └── minimal_effect/
│       ├── CMakeLists.txt
│       └── MinimalEffectPlugin.cpp
├── cmake/
│   └── ArtifactPlugin.cmake       # find_package 用
├── docs/
│   ├── GETTING_STARTED.md
│   ├── ABI_REFERENCE.md
│   └── PLUGIN_LIFECYCLE.md
└── CMakeLists.txt
```

### 4.2 プラグイン配布形式

`.artifactplugin` バンドル形式:

```
MyPlugin.artifactplugin/
├── manifest.json          # メタデータ、依存関係
├── MyPlugin.dll           # プラグイン本体
├── MyPlugin_x64.dll       # アーキテクチャ別
├── resources/
│   ├── icon.png
│   └── presets/
└── dependencies/          # バンドル依存
    └── third_party_lib.dll
```

```json
// manifest.json
{
  "name": "MyPlugin",
  "version": "1.0.0",
  "apiVersion": 1,
  "author": "Plugin Developer",
  "category": "Layer",
  "architectures": ["x86_64"],
  "entryPoint": "MyPlugin.dll",
  "dependencies": {
    "artifact-core": ">=1.0.0"
  }
}
```

### 4.3 完了条件

- [ ] SDK zip がビルドスクリプトで生成できる
- [ ] SDK のサンプルが手順通りにビルドできる
- [ ] `.artifactplugin` バンドルを `plugins/` に置くと自動ロードされる
- [ ] `GETTING_STARTED.md` が 15 分以内に最初のプラグインを作れる内容

---

## Phase 5: VST3 対応（Steinberg SDK 統合）

### 5.1 Steinberg VST3 SDK の組み込み

現在の `VST3Interfaces.ixx` はプロジェクト独自スタブ。これを本物の Steinberg SDK に置き換える。

```
# vcpkg.json に追加
"vst3sdk"  # Steinberg VST3 SDK
```

置き換え後の実装:

```cpp
// VST3Interfaces.cppm（新規実装、Steinberg SDK ラッパー）
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"

class VST3Host {
public:
    using VST3Module = VST3::Hosting::Module;
    using PluginFactory = Steinberg::Vst::IPluginFactory;
    
    std::vector<VST3PluginDescriptor> scanDirectory(const QString& path);
    bool loadPlugin(const QString& vst3Path);
    VST3PluginInstance* createInstance(const QString& pluginId);
};
```

### 5.2 完了条件

- [ ] 実 VST3 プラグインがロードできる
- [ ] パラメータ・プリセット管理が動作
- [ ] Audio Mixer に VST3 エフェクトを追加できる

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `tests/plugins/minimal_layer_plugin/MinimalLayerPlugin.cpp` | スタブ→機能実装 |
| P1 | `Artifact/src/Widgets/Layer/ArtifactLayerMenu.cppm` | プラグインレイヤーメニュー追加 |
| P1 | `Artifact/src/Layer/ArtifactLayerFactory.cppm` | プラグインレイヤー登録 |
| P1 | `Artifact/src/Plugin/PluginLayerFactory.cppm` | createLayer 実装確認・修正 |
| P2 | `ArtifactCore/src/CLAP/CLAPHost.cppm` | ホストコールバック実装 |
| P2 | 新規 `tests/clap_validation.cpp` | CLAP プラグイン検証テスト |
| P3 | `Artifact/src/Effects/Ofx/ArtifactOfxHost.cppm` | GPU texture suite、keyframe 対応 |
| P3 | 新規 `tests/ofx_validation.cpp` | OFX プラグイン検証テスト |
| P4 | 新規 `tools/artifact-plugin-sdk/` | SDK パッケージ一式 |
| P5 | `ArtifactCore/include/VST3/VST3Interfaces.ixx` | Steinberg SDK 置き換え |
| P5 | 新規 `ArtifactCore/src/VST3/VST3Host.cppm` | VST3 ホスト実装 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: ネイティブ Layer E2E | **P0** | 中 | 最もシンプルな ABI。既存インフラ最大活用。最初の成功体験 |
| P2: CLAP 検証 | **P1** | 小 | 実装ほぼ完了。テストとバグ修正のみ |
| P3: OFX 完成 | **P1** | 中 | GPU/keyframe 追加で本番品質に |
| P4: SDK | **P2** | 中 | Phase 1 成功後。ドキュメント+ビルドシステムが主 |
| P5: VST3 | **P2** | 大 | Steinberg SDK 依存。VST2 が既に動作しているので優先度低 |
