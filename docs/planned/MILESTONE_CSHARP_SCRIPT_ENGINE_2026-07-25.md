# MILESTONE_CSHARP_SCRIPT_ENGINE_2026-07-25

**ステータス:** Partial（C# engine と hostfxr 経路を実装済み。CMake 定義、複数プラットフォーム対応、Script export、実環境検証は未完了）
**最終更新:** 2026-08-15
**対象:** `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
**位置づけ:** PythonEngine / AngelScriptEngine と同じ Singleton+Pimpl パターンで C# スクリプトエンジンを実装。
**作成日:** 2026-07-25

## 2026-08-15 現行コード監査

`CSharpScriptEngine.ixx`／`.cppm` と hostfxr の Windows 動的ロード、runtimeconfig 解決、assembly load／evaluate、error state の経路は現行ソースに存在する。Inspector／layer の Script component・binding 表示も別のアプリ導線として確認できる。

一方、`ARTIFACT_HAS_DOTNET` の CMake 定義、`Script.ixx` の composite export、Linux／macOS の hostfxr 経路、C# script の安全な sandbox／permission 契約、UI からの execute／export、実 .NET runtime での end-to-end 検証は未確認。ビルドを行っていないため、モジュール登録とリンク成立は静的確認を超えて断定しない。

判定: **C# engine のコード基盤は部分実装、build integration／cross-platform／安全境界／runtime 検証は pending。**

## 1. 目的

空ファイルだった CSharpScriptEngine を PythonEngine と同等の API を持つスクリプトエンジンとして実装。.bak にあった DotnetRuntimeHost を復活させ、hostfxr 経由で .NET アセンブリをロード・実行できるようにする。

## 2. 現状 (2026-07-25)

| 要素 | 状態 |
|------|------|
| CSharpScriptEngine.ixx | ❌ 空ファイル (19行、module名+namespaceのみ) |
| CSharpScriptEngine.cppm | ❌ 不在 |
| .bak hostfxr | ⚠️ 存在するがビルドから除外、Qt依存 (QString/QLibrary) |
| PythonEngine | ✅ 完備 (Singleton+Pimpl, initialize/finalize/execute/executeFile/evaluate) |
| AngelScriptEngine | ✅ 完備 (同パターン) |

## 3. 実装内容

### CSharpScriptEngine.ixx を PythonEngine パターンで実装

Singleton (private ctor/dtor, delete copy, static instance()) + Pimpl (`class Impl`):
- `initialize(dotnetRoot)` — hostfxr.dll の探索と読み込み
- `finalize()` — .NET ランタイムのシャットダウン
- `execute(assemblyPath)` / `loadAssembly(assemblyPath)` — アセンブリのロード
- `evaluate(typeName, methodName, argument)` — 関数の呼び出しと結果取得
- `setOutputCallback(callback)` / `getLastError()` / `hasError()` / `clearError()`

### CSharpScriptEngine.cppm

DotnetRuntimeHost を Impl として内蔵:
- `.bak` のコードを Qt非依存に移植 (`QString`→`std::wstring`, `QLibrary`→`LoadLibraryW`/`GetProcAddress`)
- `#ifdef ARTIFACT_HAS_DOTNET` 条件付きコンパイル
- コンパイル時は `.bak` と異なり hostfxr.h/coreclr_delegates.h 不要（自己定義型で代替）
- hostfxr.dll のバージョン検出 (最新版を自動選択)
- runtimeconfig.json の自動解決 (アセンブリパスから推測)
- `component_entry_point_fn` シグネチャによる関数呼び出し

## 4. 変更ファイル

| ファイル | 変更 |
|----------|------|
| `ArtifactCore/include/Script/CSharpScriptEngine.ixx` | 空→CSharpScriptEngine クラス定義 (~60行) |
| `ArtifactCore/src/Script/CSharpScriptEngine.cppm` | 新規 (~250行) |

## 5. 残タスク

- [ ] `ARTIFACT_HAS_DOTNET` の CMake 定義追加
- [ ] Linux/macOS での libhostfxr.so パス確認
- [ ] Script.ixx (composite module) への export import 追加
- [ ] エラーハンドリングの実環境テスト
