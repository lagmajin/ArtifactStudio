# C# Script Engine (hostfxr + CSX) 実装マイルストーン

**日付**: 2026-08-04
**ステータス**: Phase 0〜4 実装完了（hostfxr/CSX API、Roslynブートストラップ、Script Menu、テスト基盤を追加。ビルド・実行確認待ち）
**ベース**: hostfxr (.NET Hosting API) + Roslyn Scripting API
**現状**: `CSharpScriptEngine.ixx` (66行) + `CSharpScriptEngine.cppm` (408行) で Singleton+Pimpl 実装済み。実装は `ARTIFACT_HAS_DOTNET` の有無で分岐する。`Script.ixx` 未登録、CMake 未配線。`.bak` に Qt 依存の旧実装あり（hostfxr.h / nethost.h / coreclr_delegates.h 使用）。
**狙い**: .NET アセンブリ (.dll) の hostfxr ロード実行を産線にし、さらに CSX (.csx) スクリプトの Roslyn 経由実行まで通す。

## 類似ツール参考

| ツール | 実装方式 |
|--------|----------|
| **Unity** | Mono / CoreCLR hostfxr + internal `MonoBehaviour` binding |
| **Nuke** | Python scripting (`nuke` module) + C++ bindings |
| **Houdini** | Python + VEX + HScript, C++ HDK |
| **Maya** | Python (`maya.cmds`) + C++ MPx API |
| **Unreal Engine** | C++ reflection + Blueprint, no hosted CLR |

ArtifactCore の既存スクリプトエンジンはいずれも Singleton+Pimpl パターンで、CMake の optional `find_package` → `target_compile_definitions(..._HAS_...)` → `Script.ixx` export の流れで統合されている。

---

## 現状詳細 (2026-08-04 コード確認)

### CSharpScriptEngine.ixx (`ArtifactCore/include/Script/CSharpScriptEngine.ixx`)
```
実際の行数: 66 行
内容: module Script.CSharp.Engine;
      class CSharpScriptEngine (Singleton+Pimpl, LIBRARY_DLL_API)
      initialize(dotnetRoot), finalize(), isInitialized()
      execute(assemblyPath), loadAssembly(assemblyPath)
      evaluate(typeName, methodName, argument)
      setOutputCallback, getLastError, hasError, clearError
```

### CSharpScriptEngine.cppm (`ArtifactCore/src/Script/CSharpScriptEngine.cppm`)
```
実際の行数: 408 行
内容:
  - #ifdef ARTIFACT_HAS_DOTNET 〜 #endif でコード全体をガード
  - hostfxr 関数ポインタ型を自己定義（hostfxr.h 非依存）
    hostfxr_initialize_for_runtime_config_fn
    hostfxr_get_runtime_delegate_fn
    hostfxr_close_fn
  - coreclr_delegates.h 相当の型と enum を自己定義
    load_assembly_fn, get_function_pointer_fn
    hdt_load_assembly=1, hdt_get_function_pointer=3
    UNMANAGEDCALLERSONLY_METHOD=0
  - DotnetRuntimeHost クラス（Impl 内部）
    initialize(): host/fxr/ ディレクトリ走査、最新バージョン選択
      Windows: LoadLibraryW("hostfxr.dll") + GetProcAddress
      Linux/macOS: dlopen("libhostfxr.so") + dlsym
    loadAssembly(): runtimeconfig.json 推測 → ランタイム初期化 → delegate 取得 → アセンブリロード
    getFunctionPointer(): get_function_pointer_fn で型名・メソッド名から関数ポインタ取得
    shutdown(): closeFn → FreeLibrary/dlclose
  - evaluate(): component_entry_point_fn シグネチャで呼び出し
    int(__cdecl*)(wchar_t*, wchar_t*, wchar_t*, wchar_t*, void*, int)
  - #else: 全メソッドがエラーメッセージ付き stub
```

### hostfxr.ixx.bak / hostfxr.cppm.bak（旧実装、ビルド除外）
```
hostfxr.ixx.bak (58行): Qt依存
  #include <QString>, <QLibrary>
  #include <hostfxr.h>, <nethost.h>, <coreclr_delegates.h>
  DotnetRuntimeHost クラス（QLibrary 使用）

hostfxr.cppm.bak (188行): Qt依存
  #include <QString>, <QDebug>, <QDir>, <QFile>, <QVersionNumber>
  #pragma comment(lib,"libnethost.lib")
  QLibrary で hostfxr.dll を動的ロード
  QVersionNumber で最新バージョン選択
  component_entry_point_fn 型定義
```

### CMake 既存パターン（参照: AngelScript / Python）

```cmake
# AngelScript パターン (ArtifactCore/CMakeLists.txt L639-646)
find_package(Angelscript CONFIG QUIET)
if(Angelscript_FOUND)
    target_link_libraries(ArtifactCore PUBLIC Angelscript::angelscript)
    target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_ANGELSCRIPT=1)
    message(STATUS "AngelScript scripting enabled: Angelscript ${Angelscript_VERSION}")
else()
    message(STATUS "AngelScript not found (AngelScriptEngine will be a stub)")
endif()

# Python パターン (ArtifactCore/CMakeLists.txt L627-634)
find_package(Python3 QUIET COMPONENTS Interpreter Development)
if(Python3_FOUND)
    find_package(pybind11 QUIET)
    if(pybind11_FOUND)
        target_link_libraries(ArtifactCore PUBLIC pybind11::headers Python3::Python)
        target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_PYTHON=1)
    endif()
endif()
```

### Script.ixx 現状 (`ArtifactCore/include/Script/Script.ixx`)
```
module;
#include <utility>

export module Script;

export import Script.Runtime;
export import Script.AngelScript.Engine;
export import Script.AngelScript.Behaviour;
export import Script.ArtifactScript;
export import Script.CSharp.Engine;
// ❌ Script.Python.Engine 未登録
```

### CMake 除外フィルター問題 (`ArtifactCore/CMakeLists.txt` L35-56)
```cmake
function(_artifact_exclude_check file_path result_var)
    if("${file_path}" MATCHES "hostfxr" OR   # ← これが CSharpScriptEngine に影響するか？
       ...)
```
`CSharpScriptEngine.ixx` と `CSharpScriptEngine.cppm` のファイル名には `hostfxr` が含まれていないため、このフィルターには引っかからない。ビルド自体は試行されるが、`ARTIFACT_HAS_DOTNET` 未定義のため stub としてコンパイルされる。

### vcpkg 現状 (`vcpkg.json`)
`angelscript` は登録済み。.NET / hostfxr 関連パッケージは未登録（hostfxr は vcpkg 非経由 — .NET SDK の一部）。

---

## Phase 0 — 既存コード検証・修正（現在地）

### Step 0.1 — CSharpScriptEngine.cppm の self-import 除去
`CSharpScriptEngine.cppm` L63:
```cpp
module Script.CSharp.Engine;
import Script.CSharp.Engine;  // ← 自己 import → Ninja dyndep 破損リスク
```
**修正**: `import Script.CSharp.Engine;` 行を削除する。このモジュールが自身を import する必要はなく、これは C++20 modules の循環参照ルール（AGENTS.md）に違反する。

### Step 0.2 — CSharpScriptEngine.ixx の include 位置確認
L1-8 の GMF:
```cpp
module;
#include "../Define/DllExportMacro.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
```
`#include` はすべて GMF (`module;` と `export module Script.CSharp.Engine;` の間) にある。問題なし。

### Step 0.3 — CSharpScriptEngine.cppm の include 位置確認
L1-11:
```cpp
module;
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <filesystem>
```
すべて GMF 内。問題なし。

### Step 0.4 — モジュール名の整合性確認
- `.ixx`: `export module Script.CSharp.Engine;` ✅
- `.cppm`: `module Script.CSharp.Engine;` ✅
- `.cppm` が `.ixx` と同じモジュール名を実装ユニットとして名乗っている → 正しい

---

## Phase 1 — CMake 配線（hostfxr 検出 + ARTIFACT_HAS_DOTNET）

.NET SDK のインストール検出と hostfxr へのコンパイル・リンク経路を整備する。
hostfxr は vcpkg 経由で取得できない（.NET SDK に同梱されるコンポーネント）。
AngelScript/Python のパターンに倣い、optional 検出とする。

### Step 1.1 — hostfxr のパス解決ロジック

```cmake
# .NET SDK / hostfxr 検出
# Windows: dotnet --list-runtimes からパスを取得
# Linux/macOS: dotnet --list-runtimes からパスを取得

find_program(DOTNET_EXECUTABLE dotnet)
if(DOTNET_EXECUTABLE)
    # dotnet --list-runtimes の出力から Microsoft.NETCore.App の最新バージョンパスを取得
    execute_process(
        COMMAND ${DOTNET_EXECUTABLE} --list-runtimes
        OUTPUT_VARIABLE DOTNET_RUNTIMES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # Microsoft.NETCore.App X.Y.Z [C:\Program Files\dotnet\shared\Microsoft.NETCore.App]
    # → host/fxr/ ディレクトリを特定
    ...
endif()
```

**実際の検出方針**:
- `dotnet --list-runtimes` はランタイムパスを表示するが、hostfxr はランタイムではなく SDK/host ディレクトリにある
- `dotnet --info` の `Base Path` から `host/fxr/` を辿るのが確実
- Windows デフォルト: `C:\Program Files\dotnet\host\fxr\`
- Linux デフォルト: `/usr/share/dotnet/host/fxr/` または `/usr/lib/dotnet/host/fxr/`
- macOS デフォルト: `/usr/local/share/dotnet/host/fxr/`

実装方針:
1. `find_program(DOTNET_EXECUTABLE dotnet)` で dotnet CLI を検出
2. dotnet の実体パスから `../host/fxr/` を解決
3. `host/fxr/` が存在すれば `ARTIFACT_HAS_DOTNET=1` を定義
4. `nethost.lib` / `libnethost.so` をリンク（hostfxr のロードに必要な薄いスタティックライブラリ）

**注意**: `nethost` は vcpkg の `dotnet-runtime` ポートには含まれず、.NET SDK の `packs/Microsoft.NETCore.App.Host.*/` に含まれるネイティブライブラリ。代替として CSharpScriptEngine.cppm は既に `LoadLibraryW`/`dlopen` で hostfxr.dll を動的ロードする実装になっているため、**リンク時依存はゼロ**で完結できる。

### Step 1.2 — ヘッダ探索（`<nethost.h>`, `<hostfxr.h>`, `<coreclr_delegates.h>`）

現在の CSharpScriptEngine.cppm は関数ポインタ型を**自己定義している**ため、これらの SDK ヘッダは不要。この設計は意図的（milestone 原文: "hostfxr.h/coreclr_delegates.h 不要（自己定義型で代替）"）。

ただし、将来の保守性のために SDK ヘッダの所在を確認するオプションは残す。

### Step 1.3 — 最小限の CMake 実装（推奨）

```cmake
# C# scripting support (hostfxr — .NET Hosting API).
# When the .NET SDK is available, ARTIFACT_HAS_DOTNET enables the real
# hostfxr-based implementation; otherwise CSharpScriptEngine compiles as
# a no-op stub (AngelScript と同じパターン).
find_program(DOTNET_EXECUTABLE dotnet)
if(DOTNET_EXECUTABLE)
    # host/fxr/ ディレクトリの存在確認
    get_filename_component(_dotnet_base_dir "${DOTNET_EXECUTABLE}" DIRECTORY)
    if(EXISTS "${_dotnet_base_dir}/host/fxr")
        target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_DOTNET=1)
        message(STATUS "C# scripting enabled: .NET SDK found, hostfxr available")
    else()
        message(STATUS "C# scripting disabled: .NET SDK found but host/fxr missing")
    endif()
else()
    message(STATUS "C# scripting disabled: .NET SDK not found (CSharpScriptEngine will be a stub)")
endif()
```

**リンク不要理由**: `CSharpScriptEngine.cppm` は `LoadLibraryW("hostfxr.dll")` / `dlopen("libhostfxr.so")` で動的ロードするため、リンク時依存ゼロ。`nethost.lib` も不要。

### Step 1.4 — Script.ixx への export import 追加

`ArtifactCore/include/Script/Script.ixx` に以下を追加:
```cpp
export import Script.CSharp.Engine;
```

### Step 1.5 — 除外フィルターの調整

`_artifact_exclude_check` の `"hostfxr"` マッチは `.bak` ファイルだけを狙っている。`CSharpScriptEngine.*` に影響しないため変更不要。ただし将来の混乱防止のためコメントを追加してもよい。

---

## Phase 2 — アセンブリ実行の完成

この Phase の完了条件: `CSharpScriptEngine::execute("path/to/MyPlugin.dll")` が実際に .NET アセンブリをロードし、`evaluate("MyClass", "MyMethod")` で関数を呼び出せること。

### Step 2.1 — runtimeconfig.json の自動生成

`DotnetRuntimeHost::loadAssembly()` はアセンブリパスから runtimeconfig.json を自動推測するが、任意の .dll に runtimeconfig.json が常に存在するとは限らない。以下のフォールバックを追加:

```cpp
// runtimeconfig.json がない場合、最小限の設定を自動生成
if (!fs::exists(configPath)) {
    // アプリケーション自身の .runtimeconfig.json を流用 or 一時生成
    // または、デフォルトの TFM (net8.0/net9.0) でフォールバック
}
```

### Step 2.2 — C# からの Artifact コールバック登録

`evaluate()` の `component_entry_point_fn` シグネチャに加え、C# 側から C++ の関数を呼べるコールバック登録を追加:
- `registerCallback(name, callback)` — C# 側が `Artifact.Call("name", args)` で呼び出せるようにする
- 出力キャプチャ（stdout/stderr → C++ 側の OutputCallback）

### Step 2.3 — エラーハンドリング強化

- hostfxr の HRESULT エラーコードの文字列化
- アセンブリロード失敗時の詳細例外メッセージ取得（C# 側 try-catch の伝播）
- ランタイムバージョン不一致の検出とエラーメッセージ

### Step 2.4 — NuGet アセンブリ解決

AssemblyLoadContext 経由で依存アセンブリを解決する仕組み。C# 側に小さなブートストラップアセンブリを用意:
- `Artifact.Runtime.dll` — AssemblyLoadContext.Resolving で .deps.json または NuGet キャッシュから依存解決
- これがないと `System.Text.Json` 等を使うアセンブリがロード失敗する

### Step 2.5 — Script Menu 統合

`Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm` に C# アセンブリのロード・実行メニュー項目を追加。既存の Python / AngelScript 導線に倣う。

---

## Phase 3 — CSX (.csx) スクリプト実行（Roslyn Scripting API）

hostfxr だけでは .csx の JIT コンパイル・実行はできない。CSX は Roslyn の `Microsoft.CodeAnalysis.CSharp.Scripting` パッケージが必要。

### 方式 A: ブートストラップ .dll（推奨）

1. `Artifact.Scripting.dll` を作成（.NET 8.0/9.0）
   - `Microsoft.CodeAnalysis.CSharp.Scripting` NuGet パッケージに依存
   - 公開 API:
     ```csharp
     public static class ArtifactScriptHost {
         public static string Evaluate(string code);
         public static string ExecuteFile(string path);
         public static void SetOutputCallback(CallbackDelegate cb);
     }
     ```
2. C++ 側:
   ```cpp
   cs.initialize();                          // hostfxr 準備
   cs.execute("Artifact.Scripting.dll");     // ブートストラップ .dll ロード
   cs.evaluate("ArtifactScriptHost", "Evaluate", ".csx コード文字列");
   ```
3. この方式の利点:
   - hostfxr 経路をそのまま使える
   - NuGet パッケージの依存解決をブートストラップ .dll 側で完結できる
   - C# 側のデバッグが独立して可能

### 方式 B: Microsoft.CodeAnalysis.CSharp.Scripting を直接ホスト

1. `Microsoft.CodeAnalysis.CSharp.Scripting` の NuGet パッケージを展開
2. 展開先を hostfxr の追加 probing path として登録
3. C++ から直接スクリプト評価用のアセンブリをロード
4. 欠点: NuGet 依存解決が複雑、デバッグ困難

### 方式 A の実装手順

#### Step 3.1 — Artifact.Scripting.dll プロジェクト作成

```
Artifact/scripts/dotnet/Artifact.Scripting/
├── Artifact.Scripting.csproj
├── ArtifactScriptHost.cs
└── NativeCallbacks.cs
```

`Artifact.Scripting.csproj`:
```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="Microsoft.CodeAnalysis.CSharp.Scripting" Version="4.8.0" />
  </ItemGroup>
</Project>
```

#### Step 3.2 — ArtifactScriptHost 実装

```csharp
using Microsoft.CodeAnalysis.CSharp.Scripting;
using Microsoft.CodeAnalysis.Scripting;

public static class ArtifactScriptHost
{
    private static Action<string, bool>? _outputCallback;

    // C++ から呼ばれる
    public static int Evaluate(
        [Runtime.InteropServices.UnmanagedCallersOnly] ...);
    
    // CSX ファイル実行
    public static string ExecuteFile(string path) { ... }
    
    // CSX コード文字列実行
    public static string EvaluateCode(string code) { ... }
    
    // NuGet パッケージ参照付き実行
    public static string EvaluateWithImports(string code, string[] imports) { ... }
}
```

#### Step 3.3 — C++ 側の CSX API 追加

```cpp
// CSharpScriptEngine に以下を追加
bool executeScript(const std::string& code);       // .csx 文字列実行
bool executeScriptFile(const std::string& path);   // .csx ファイル実行
bool executeScriptWithImports(const std::string& code,
                              const std::vector<std::string>& imports);
```

#### Step 3.4 — CSX の検出と自動選択

`.csx` ファイルが指定された場合、自動的に CSX 実行パス（Phase 3）を使用。`.dll` の場合は Phase 2 のアセンブリロードパスを使用。

---

## Phase 4 — テストと検証

### Step 4.1 — 単体テスト

`tests/ArtifactCore/CSharpScriptEngineTest.cpp`:
- stub モードのテスト（`ARTIFACT_HAS_DOTNET` 未定義時のエラーメッセージ検証）
- hostfxr が利用可能な環境での initialize / finalize / re-initialize
- 存在しないアセンブリのロード → エラー
- runtimeconfig.json 不在時のフォールバック
- 有効なテストアセンブリのロード・評価

### Step 4.2 — CSX テスト

- 簡単な式評価: `"2 + 3"` → `"5"`
- 変数あり: `"var x = 10; x * 2"` → `"20"`
- ファイル実行
- コンパイルエラー → エラーメッセージ
- Artifact API 呼び出し（`Artifact.Log("hello")` 等）

### Step 4.3 — 結合テスト

- Artifact Menu からの CSX スクリプト実行
- OutputCallback の stdout/stderr キャプチャ
- スクリプトのタイムアウト処理

---

## 変更ファイル一覧

| ファイル | 変更種別 | 内容 |
|----------|----------|------|
| `ArtifactCore/include/Script/CSharpScriptEngine.ixx` | **既存修正** | CSX API 追加: `executeScript()` / `executeScriptFile()` / `executeScriptWithImports()` |
| `ArtifactCore/src/Script/CSharpScriptEngine.cppm` | **既存修正** | self-import 除去、CSX 実装追加、runtimeconfig.json フォールバック、エラーハンドリング強化 |
| `ArtifactCore/include/Script/Script.ixx` | **既存修正** | `export import Script.CSharp.Engine;` 追加 |
| `ArtifactCore/CMakeLists.txt` | **既存修正** | hostfxr 検出 + `ARTIFACT_HAS_DOTNET` define |
| `Artifact/scripts/dotnet/Artifact.Scripting/Artifact.Scripting.csproj` | **新規** | CSX ブートストラッププロジェクト |
| `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs` | **新規** | Roslyn Scripting ホスト実装 |
| `Artifact/scripts/dotnet/Artifact.Scripting/NativeCallbacks.cs` | **新規** | C++ コールバック用 UnmanagedCallersOnly API |
| `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm` | **既存修正** | C# スクリプトメニュー項目追加 |
| `tests/ArtifactCore/CSharpScriptEngineTest.cpp` | **新規** | GTest 単体テスト |

---

## 依存関係とリスク

| リスク | 内容 | 対策 |
|--------|------|------|
| .NET SDK バージョン不一致 | ユーザーの .NET バージョンがブートストラップ .dll の TFM と合わない | `rollForward: latestMajor` 設定 |
| NuGet パッケージ復元 | 初回 dotnet build で NuGet パッケージ復元が必要 | CMake のビルド前ステップで `dotnet restore` 実行 |
| Roslyn Scripting の起動遅延 | 初回の Roslyn コンパイルは1〜2秒かかる | 初回初期化時にプリウォーム |
| CSX のスレッド安全性 | Roslyn Scripting はスレッドセーフだが、状態共有に注意 | 現在の Singleton パターンで mutex 保護 |
| ARM64 Windows | hostfxr.dll のパスが異なる可能性 | `%DOTNET_ROOT%` 環境変数を優先参照 |

---

## 優先順位とマイルストーン分割

- **M-CS-1** (Phase 0-1): self-import 除去 + CMake 配線 → CSharpScriptEngine が実ビルドで有効化
- **M-CS-2** (Phase 2): アセンブリ実行の完成 + runtimeconfig.json フォールバック
- **M-CS-3** (Phase 3): CSX ブートストラップ .dll + C++ API 追加
- **M-CS-4** (Phase 4): テスト + メニュー統合

各 Phase は独立して完了可能（M-CS-1 だけでも .dll ロードは試せる）。
