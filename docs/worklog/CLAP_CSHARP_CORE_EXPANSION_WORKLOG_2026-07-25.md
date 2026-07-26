# CLAP_CSHARP_CORE_EXPANSION_WORKLOG_2026-07-25

## 概要

CLAP ホストのインスタンス生成バグ修正 + CSharpScriptEngine の実装。

## CLAP

**問題点:**
- `PluginEntryProc` の型が間違っていた (`bool (*)(const PluginDescriptor*)` — 実際は `const clap_plugin_entry*`)
- `Host::loadPlugin()` が常に `nullptr` を返していた

**修正:**
1. GMF に CLAP C API 型 (`clap_plugin_descriptor`, `clap_plugin`, `clap_plugin_entry`, `clap_process`, `clap_host`) を追加
2. `PluginEntryProc` 削除、正しいエントリポイント解決に修正
3. `PluginLibrary::load()` 修正 — `*static_cast<const clap_plugin_entry**>(sym)` + `entry->init()` 呼び出し
4. `PluginInstance` 具象クラス追加 — `const clap_plugin*` をラップ
5. `Host::loadPlugin()` 修正 — `get_plugin_count()` + `create_plugin()` でインスタンス生成
6. PluginLibrary を `shared_ptr` で保持 (プラグイン生存中のDLL解放防止)

## CSharpScriptEngine

**問題点:** 19行の空ファイル。`.bak` に hostfxr コードあり (ビルド除外)。

**修正:**
1. `CSharpScriptEngine.ixx` を PythonEngine/AngelScriptEngine と同じ Singleton+Pimpl パターンで実装
2. `CSharpScriptEngine.cppm` 新規作成
3. `.bak` の DotnetRuntimeHost を Impl として復活、Qt依存排除
4. `#ifdef ARTIFACT_HAS_DOTNET` 条件付きコンパイル
5. hostfxr.h/coreclr_delegates.h 非依存（自己定義型で代替）

## 変更ファイル

| ファイル | 追加行 | 内容 |
|----------|--------|------|
| `ArtifactCore/include/CLAP/CLAPHost.ixx` | ~120行 | C API型、PluginInstance宣言 |
| `ArtifactCore/src/CLAP/CLAPHost.cppm` | ~280行 | PluginLibrary修正、PluginInstance実装 |
| `ArtifactCore/include/Script/CSharpScriptEngine.ixx` | ~60行 | 空→実装 |
| `ArtifactCore/src/Script/CSharpScriptEngine.cppm` | 新規 ~250行 | DotnetRuntimeHost + CSharpScriptEngine |
