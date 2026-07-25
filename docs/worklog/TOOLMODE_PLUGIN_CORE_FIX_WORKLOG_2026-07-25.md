# TOOLMODE_PLUGIN_CORE_FIX_WORKLOG_2026-07-25

## 概要

ToolMode 削除と Plugin システムのスタブ修正。

## 実装内容

### ToolMode 削除

`ArtifactCore/include/Tool/ToolMode.ixx` — 誰も使っていない `enum class ToolMode` を削除。ゼロコンシューマ、CMakeLists.txt は GLOB_RECURSE 自動発見のためファイル削除のみで完了。

### Plugin スタブ修正 (3ヶ所)

1. **PluginLoader callback 機構追加** — `setOnPluginLoaded(callback)` 追加。`loadDllPlugin()` で各プラグインの registry 登録後に callback を発火。ライブラリハンドルを `loadedLibs` に保持してアンロード防止。

2. **PluginLayerFactory::scanAndRegister() 実装** — コメントアウトされたスタブループを削除。PluginLoader + callback を使い、layer カテゴリの DLL を自動発見し `ArtifactPlugin_CreateLayer`/`ArtifactPlugin_GetLayerVTable` を解決して adapter に登録。

3. **LayerPluginAdapter::extraPropertyGroups() 実装** — 空リストを返すスタブから、vtable の `getPropertyGroupCount`/`getPropertyGroupDef` を呼び出す実装に。

4. **loadSubprocessPlugin() 改善** — エラーメッセージを具体的に。

5. **ドキュメント** — `docs/PLUGIN_ARCHITECTURE.md` 作成 + `docs/planned/MILESTONE_PLUGIN_SYSTEM_COMPLETION_2026-07-25.md` 作成。

## 変更ファイル一覧

| ファイル | 追加行 | 内容 |
|----------|--------|------|
| `ArtifactCore/include/Tool/ToolMode.ixx` | 削除 | 死にコード削除 |
| `Artifact/include/Plugin/PluginLoader.ixx` | ~5行 | callback typedef + API追加 |
| `Artifact/src/Plugin/PluginLoader.cppm` | ~25行 | callback実装、loadedLibs保持、error改善 |
| `Artifact/src/Plugin/PluginLayerFactory.cppm` | ~40行 | scanAndRegister 実装、import追加 |
| `Artifact/src/Plugin/LayerPluginAdapter.cppm` | ~20行 | extraPropertyGroups 実装 |
| `docs/PLUGIN_ARCHITECTURE.md` | 新規 | プラグインアーキテクチャ文書 |
| `docs/planned/MILESTONE_PLUGIN_SYSTEM_COMPLETION_2026-07-25.md` | 新規 | マイルストーン文書 |
