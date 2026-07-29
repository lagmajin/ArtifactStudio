# MILESTONE_CLAP_HOST_COMPLETION_2026-07-25

**ステータス:** Partial（CLAP DLL 読み込み・PluginInstance 基盤を実装済み。params 拡張、process 変換、host callback、Effect 統合、GUI、runtime 検証は未完了）
**対象:** `ArtifactCore/include/CLAP/CLAPHost.ixx`, `ArtifactCore/src/CLAP/CLAPHost.cppm`
**位置づけ:** CLAP (CLever Audio Plugin) ホストのインスタンス生成を実装。
**作成日:** 2026-07-25

## 1. 目的

CLAP ホストの `Host::loadPlugin()` が常に `nullptr` を返すスタブ状態から脱却し、実際の CLAP プラグインDLLをロード・インスタンス化できるようにする。

## 2. 現状 (2026-07-25)

| 要素 | 状態 |
|------|------|
| データ型 (AudioBuffer, Event, Process, PluginDescriptor) | ✅ 完備 |
| Plugin 抽象クラス | ✅ 完備 (virt/init/destroy/activate/process/params) |
| PluginLibrary (DLLローダー) | ⚠️ `PluginEntryProc` の型が間違い。`clap_entry` の解決後、`entry->init()` を呼んでいない |
| Host::loadPlugin() | ❌ 常に `nullptr` を返す (`"骨格: 実際の Plugin* は CLAP SDK 統合時に"`) |
| PluginInstance 具象クラス | ❌ 不在 |
| PluginLibrary 生存管理 | ❌ unique_ptr でプラグイン生存中にDLLが解放される可能性 |

## 3. 実装内容

### GMF に CLAP C API 型追加

以下の C 互換構造体を global module fragment に定義:
- `clap_plugin_descriptor` — CLAP プラグイン記述子 (id, name, vendor, version, features等)
- `clap_plugin` — プラグインインスタンス (descriptor + vtable: init/destroy/activate/deactivate/start_processing/stop_processing/process/get_extension)
- `clap_plugin_entry` — DLLエントリポイント (clap_version, init/deinit, get_plugin_count, get_plugin_descriptor, create_plugin, destroy_plugin)
- `clap_process` — process() 呼び出し用データ (frames_count, deinterleaved audio buffers)
- `clap_host` — ホストコールバック (最小限、必要に応じて拡張)

### PluginEntryProc 削除

`using PluginEntryProc = bool (*)(const struct PluginDescriptor*)` を削除。
CLAP DLL は `const clap_plugin_entry* clap_entry` をエクスポートするため、正しい解決方法に修正。

### PluginLibrary::load() 修正

- `GetProcAddress("clap_entry")` → `*static_cast<const clap_plugin_entry**>(sym)` で解決
- `entry->init(path.c_str())` を呼び出し、成功時のみロード完了
- デストラクタで `entry->deinit()` を呼び出し
- shared_ptr で管理し、プラグイン生存中のアンロードを防止

### PluginInstance 具象クラス

`clap::Plugin` の具象サブクラス。`const clap_plugin*` をラップ:
- `init()` / `destroy()` / `activate()` / `deactivate()` — CLAP vtable に委譲
- `startProcessing()` / `stopProcessing()` / `process()` — 同上
- `process()` で `clap_plugin::process()` を呼び出し、`clap_process` 構造体に変換
- `getExtension()` — 拡張インターフェースの解決
- パラメータ系 (`paramsCount`, `paramInfo`, `paramValue` etc.) — clap_plugin-params 拡張経由（未統合、後続タスク）

### Host::loadPlugin() 修正

- PluginLibrary::load() 成功後、`entry->get_plugin_count()` でプラグイン数を取得
- 各インデックスで `entry->get_plugin_descriptor()` + `entry->create_plugin()` を呼び出し
- PluginInstance にラップして `plugins_` に追加
- 先頭プラグインを返す (複数プラグインも全量ロード)

## 4. 変更ファイル

| ファイル | 変更 |
|----------|------|
| `ArtifactCore/include/CLAP/CLAPHost.ixx` | GMF に4つのC API型追加。PluginEntryProc 削除。PluginInstance 宣言追加 (~120行) |
| `ArtifactCore/src/CLAP/CLAPHost.cppm` | PluginLibrary 全面書き換え。PluginInstance 実装。Host::loadPlugin/unloadPlugin 修正 (~280行) |

## 5. 残タスク

- [ ] clap_plugin-params 拡張の統合 (paramInfo/paramValue/paramSetValue が未実装)
- [ ] processSegment() の AudioSegment ↔ clap_process 変換完成
- [ ] clap_host コールバックの実装 (request_restart, timer 等)
- [ ] エフェクトチェインとの統合 (AbstractEffect からの CLAP プラグイン呼び出し)
- [ ] GUI 埋め込み (clap_plugin-gui 拡張)
