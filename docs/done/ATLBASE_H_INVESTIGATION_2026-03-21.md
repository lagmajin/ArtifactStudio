# atlbase.h 調査レポート

> 2026-03-21 18:11 調査

## 結論: ファイルは存在するが include パスに `atlmfc` が含まれていない

## ファイル場所

`atlbase.h` は以下の場所に存在する:

```
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\atlmfc\include\atlbase.h
```

MSVC バージョン一覧 (全バージョンで ATL がインストール済み):

| MSVC バージョン | パス |
|---|---|
| 14.40.33807 | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\atlmfc\include\atlbase.h` |
| 14.42.34433 | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\atlmfc\include\atlbase.h` |
| 14.44.35207 | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\atlmfc\include\atlbase.h` |

## 問題の原因

ビルドログのインクルードパス (`-I` フラグ) を確認すると:

```
-IX:\Dev\ArtifactStudio\ArtifactCore\include
-IC:\vcpkg\installed\x64-windows\include
-external:IX:\Dev\ArtifactStudio\out\build\x64-Debug\vcpkg_installed\x64-windows\include
```

**`atlmfc\include` ディレクトリが `-I` パスに含まれていない。**

通常、Visual Studio の C++ プロジェクトでは MSVC ツールチェーンが自動的に `atlmfc\include` を追加するが、CMake プロジェクトでは手動で追加する必要がある。

## 現在のビルド環境

- **Visual Studio**: 2022 Community (18)
- **MSVC**: 14.44.35207 (ビルドログ: `Hostx64\x64\cl.exe` + `MSVC\1444~1.352`)
- **ビルドシステム**: CMake + Ninja
- **コンパイラ**: `cl.exe` (MSVC)

## 修正方法

### 方法1: CMakeLists.txt に ATL include パスを追加

```cmake
# VS Install Dir を取得
set(VS_INSTALL_DIR "$ENV{VSINSTALLDIR}")
if(NOT VS_INSTALL_DIR)
    # MSVC ツールチェーンから推測
    get_filename_component(MSVC_TOOLCHAIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(MSVC_TOOLCHAIN_DIR "${MSVC_TOOLCHAIN_DIR}" DIRECTORY)
    get_filename_component(MSVC_TOOLCHAIN_DIR "${MSVC_TOOLCHAIN_DIR}" DIRECTORY)
    set(VS_INSTALL_DIR "${MSVC_TOOLCHAIN_DIR}")
endif()

# ATL include パスを追加
set(ATL_INCLUDE_DIR "${VS_INSTALL_DIR}/atlmfc/include")
if(EXISTS "${ATL_INCLUDE_DIR}")
    include_directories("${ATL_INCLUDE_DIR}")
endif()
```

### 方法2: VS Installer で ATL コンポーネントを確認

VS Installer → Visual Studio 2022 → 変更 → C++ 用の ATL (最新バージョン) がチェックされているか確認。

### 方法3: 不要な依存関係を削除

`atlbase.h` を直接 include しているソースファイルを特定し、ATL に依存しない代替手段に置き換える。

## 関連ファイル

`atlbase.h` を間接的に参照している可能性のあるファイル:
- Diligent Engine の D3D12 バックエンド (`EngineFactoryD3D12.h`)
- Windows プラットフォーム関連コード
