# MILESTONE_PLUGIN_SYSTEM_COMPLETION_2026-07-25

**ステータス:** Partial（DLL plugin loading、callback、layer factory scan、property groups を実装済み。subprocess runner、sample plugin、hot reload、runtime 検証は未完了）
**対象:** `ArtifactCore/include/Tool/ToolMode.ixx`, `Artifact/include/Plugin/PluginLoader.ixx`, `Artifact/src/Plugin/PluginLoader.cppm`, `Artifact/include/Plugin/PluginLayerFactory.ixx`, `Artifact/src/Plugin/PluginLayerFactory.cppm`, `Artifact/src/Plugin/LayerPluginAdapter.cppm`
**位置づけ:** ArtifactCore の死にコード掃除と Plugin システムのスタブ修正
**作成日:** 2026-07-25

## 1. 目的

- 誰も使っていない `ToolMode` enum を削除する
- Plugin システムの3つのスタブを修正し、実際に動作するパイプラインを完成させる

## 2. 現状 (2026-07-25)

### ToolMode

| 要素 | 状態 | 詳細 |
|------|------|------|
| `ArtifactCore/include/Tool/ToolMode.ixx` | ❌ 死にコード | `enum class ToolMode { Move, Rotate, Scale, MaskEdit, Pen, Hand }` のみ。**ゼロコンシューマ**。誰も `import Tool.Mode` していない。CMakeLists.txt は GLOB_RECURSE 自動発見 |

### Plugin System

| 要素 | 状態 | 詳細 |
|------|------|------|
| PluginRegistry | ✅ 稼働 | シングルトン、スレッドセーフ、register/query/state 完備 |
| PluginLoader::loadDllPlugin() | ✅ 稼働 | QLibrary ロード、ABI検証、descriptor抽出、registry登録 |
| PluginLoader::loadSubprocessPlugin() | ❌ スタブ | `"Subprocess plugin loading not yet implemented"` を返すだけ |
| `onPluginLoaded` callback | ❌ 不在 | PluginLoader にカテゴリ固有フックが無い |
| PluginSandbox | ✅ 稼働 | サブプロセス管理、heartbeat 500ms、タイムアウト 1000ms、最大3回再起動 |
| LayerPluginAdapter | ✅ 基本機能稼働 | initialize/shutdown/drawContent/serialize/deserialize 完備 |
| LayerPluginAdapter::extraPropertyGroups() | ❌ スタブ | 常に空リストを返す。vtable に `getPropertyGroupCount`/`getPropertyGroupDef` があるのに未使用 |
| PluginLayerFactory::scanAndRegister() | ❌ スタブ | ループ本体が**コメントアウト**。誰も layer plugin を自動発見できない |
| ArtifactGlobalEffectManager | ✅ 稼働 | PluginLoader で effects をロード |

## 3. 実装内容 (2026-07-25)

### 3.1 ToolMode 削除

`ArtifactCore/include/Tool/ToolMode.ixx` を削除。CMakeLists.txt は `file(GLOB_RECURSE)` で自動発見するため、ファイル削除のみでビルドから外れる。

### 3.2 PluginLoader callback 追加

`PluginLoader.ixx` に `PluginLoadedCallback` 型エイリアスと `setOnPluginLoaded()` を追加。

`PluginLoader.cppm`:
- `Impl` に `std::function<PluginLoadedCallback> onPluginLoaded` メンバー追加
- `loadDllPlugin()` 内で、各プラグインの registry 登録後に callback を発火
- `Impl` に `std::vector<std::unique_ptr<QLibrary>> loadedLibs` を追加 — callback 発火後もライブラリをロードし続ける（アンロードすると解決済み関数ポインタが無効になる）
- `unloadAll()` で `loadedLibs.clear()` を追加

### 3.3 PluginLayerFactory::scanAndRegister() 実装

スタブのコメントアウトされたループボディを削除し、`PluginLoader` + callback を使った実装に置き換え。

フロー:
1. `ArtifactPluginLoader` を作成
2. `setOnPluginLoaded` callback で layer カテゴリの DLL を検出
3. callback 内で `ArtifactPlugin_CreateLayer` / `ArtifactPlugin_GetLayerVTable` を解決
4. `instance` と `vtable` を `registerFromDll()` に渡す
5. `plugins/layers/` をスキャン

### 3.4 LayerPluginAdapter::extraPropertyGroups() 実装

スタブの `return {};` を削除し、vtable の `getPropertyGroupCount` と `getPropertyGroupDef` を呼び出す実装に置き換え。

### 3.5 loadSubprocessPlugin() 改善

エラーメッセージを具体的な内容に更新:
```
"Subprocess loading requires a plugin runner executable. "
"See docs/PLUGIN_SUBPROCESS_PROTOCOL.md for the JSON IPC spec. "
"PluginSandbox is ready to manage the subprocess lifecycle."
```

## 4. 変更ファイル

| ファイル | 変更 |
|----------|------|
| `ArtifactCore/include/Tool/ToolMode.ixx` | 削除 |
| `Artifact/include/Plugin/PluginLoader.ixx` | `PluginLoadedCallback` typedef、`setOnPluginLoaded()` API追加 |
| `Artifact/src/Plugin/PluginLoader.cppm` | callback 実装、`loadedLibs`保持、loadSubprocessPlugin エラー改善 |
| `Artifact/src/Plugin/PluginLayerFactory.cppm` | `scanAndRegister()` 実装。`import Artifact.Plugin.Loader` 追加 |
| `Artifact/src/Plugin/LayerPluginAdapter.cppm` | `extraPropertyGroups()` 実装 |
| `docs/PLUGIN_ARCHITECTURE.md` | 新規作成 |

## 5. 残タスク / 将来展望

- [ ] Tool plugin consumer の実装 (`ArtifactToolPluginVTable` を使う側がまだ無い)
- [ ] Subprocess runner executable (`artifact_plugin_host.exe`)
- [ ] `docs/PLUGIN_SUBPROCESS_PROTOCOL.md` — PluginSandbox が期待する JSON IPC プロトコル仕様
- [ ] プラグインDLL のサンプルプロジェクト
- [ ] ホットリロード (DLLの変更検出 → 再ロード)

## 6. 関連

- `docs/PLUGIN_ARCHITECTURE.md` — プラグインシステム全体アーキテクチャ文書
- `ArtifactCore/include/Plugin/ArtifactPluginABI.h` — C ABI 定義
- `Artifact/src/Effects/ArtifactGlobalEffectManager.cppm` — 既存の effect plugin コンシューマ
