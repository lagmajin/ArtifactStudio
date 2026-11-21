# Artifact Suite テストガイド

**最終更新:** 2026-11-21

## テストの実行方法

```bash
# Configure the shared repository build tree with tests enabled
cmake --preset x64-Debug-Agent -DARTIFACT_BUILD_TESTS=ON

# Build only the requested test target
cmake --build --preset x64-Debug-Agent --target <TestTarget>

# Run only the requested test
ctest --test-dir out/build/x64-Debug-Agent -C Debug -R "^<TestName>$" --output-on-failure
```

ルートのテスト target はアプリ本体と同じ CMake build tree に登録し、既存の production library target をリンクします。テストごとに別の `build` directory を作ると、共通 library・module・package の生成物を再利用できません。generator、toolchain、compiler、configuration が異なる場合は互換な生成物を共有できないため、preset ごとの build tree を維持してください。

## テストの追加方法

1. 対応するサブディレクトリ（ArtifactCore, Artifact, ArtifactWidgets）にテストファイルを作成
2. そのディレクトリの CMakeLists.txt にテストを追加

新しい suite もルート CMake target として登録し、上記の共有 build tree を使います。独立 `project()` は reduced module closure などの理由がある場合だけにし、必要な場合は suite をまとめて `out/standalone-build/<preset名>/<suite名>` に構成します。

例:
```cmake
artifact_add_test(MyComponentTest
    SOURCES
        MyComponentTest.cpp
    LIBS
        MyComponentLib
)
```

## ArtifactCore standalone suites

`tests/ArtifactCore` には、フルの ArtifactCore ライブラリをリンクせず、必要な C++ module closure を各 suite が定義する standalone project があります。複数 suite をまとめて実行するときは、共通の Core / Container module artifact を同じ build tree で共有する親 project を使ってください。

```powershell
$env:VSLANG = '1033'
$repoRoot = (Get-Location).Path
cmake -S tests/ArtifactCore/StandaloneSuite -B out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite `
  -G Ninja -DCMAKE_BUILD_TYPE=Debug `
  "-DCMAKE_TOOLCHAIN_FILE=$repoRoot/cmake/toolchain-msvc-v143.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF
cmake --build out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite --config Debug --parallel 4
ctest --test-dir out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite -C Debug --output-on-failure
```

この suite はフルの `ArtifactCore` target とは異なる reduced module closure を検査するため、ルート build tree の object / IFC は共有しません。独立 configure が必要な例外として、共通 module/helper の生成物を suite 内で共有し、全152 test source を feature 単位の13 executable / CTest entry にまとめます。同じ feature 内の GoogleTest cases は同一プロセスで実行されます。各 feature の個別 configure は一時的な調査に限り、通常の継続利用では suite 全体を一つの build tree で構成してください。個別 suite では `tests/ArtifactCore/CoreValueStandalone`、`CoreUtilityStandalone`、`ColorSpaceStandalone`、`FrameRateStandaloneSuite`、`GeometryStandaloneSuite`、`RawImageStandalone`、`DataStandalone`、`AudioStandalone`、`AssetDatabaseStandalone`、`AssetMetaFileStandalone`、`AssetFingerprintStandalone`、`AssetSequenceStandalone`、`DiagnosticEngineStandalone`、`OpenExrStandalone` を対象にできます。

## Build tree sharing audit

Many standalone CMake projects remain under `tests/`. The 13 ArtifactCore feature suites share one standalone build tree. Static comparison found 20 test source files that were unique to individual standalone projects and can use production module targets; those tests now run from the root test build tree, with compatible cases added to existing feature executables. Contracts already present in either the root test graph or the combined suite remain there. The Mesh Shader LOD and Pyro Volume projects retain separate focused paths because they exercise optional GPU support and a reduced simulation closure, respectively.

The separate `tests/ImageEffects` harness duplicated Artifact production targets in another build tree. Its 52 previously unregistered test sources now use two feature executables in the root test graph; the old standalone entry point reports the root configure/build command instead of rebuilding Artifact.

The CPU Mesh Shader LOD and Animation Dynamics tests now link the production `ArtifactCore` target in the root test graph; the optional Vulkan Mesh Shader LOD acceptance test remains a separate GPU target. Pyro Volume retains a reduced simulation closure for focused investigation. Other standalone CMake projects duplicate tests already represented in the root graph or the combined suite. Keep their individual configure path for focused reduced-closure investigation only; use the root preset or the combined suite for routine work. Before consolidating more projects, compare test source files, module target names, package requirements, source-level compiler options, and runtime isolation. A raw `add_subdirectory()` sweep would recreate duplicate targets and could make one source compile with conflicting module references. Do not use per-test build trees as the routine path.

## PBR / Ray Tracing acceptance test

`tests/ArtifactCore/RayTracingAcceptanceTest.cpp` はアプリ本体を起動しない独立 executable `ArtifactCoreRayTracingAcceptanceTest` として動きます。既存の `ArtifactCore` と Diligent Vulkan backend をリンクするため、別途 Diligent や ArtifactCore の複製ビルドは不要です。GPU は Vulkan ray tracing 対応が必要です。

既存の configure 済み build tree を使い、対象だけを増分ビルドして実行します。

```powershell
# build は普段使っている configure 済み build directory
cmake --build --preset x64-Debug-Agent --target ArtifactCoreRayTracingAcceptanceTest
ctest --test-dir out/build/x64-Debug-Agent -C Debug -R "^ArtifactCoreRayTracingAcceptanceTest$" --output-on-failure
```

その build tree で初めてこの suite を有効にする場合だけ、同じ build directory を再configureし、既存の generator・toolchain・vcpkg 設定を維持したまま `-DARTIFACT_ENABLE_RAY_TRACING_TEST=ON` を追加します。新しい build tree を作ると既存ライブラリの再利用ができない場合があるため、まず `CMakeCache.txt` がある既存 tree を確認してください。configure後も上記の個別 target と CTest filter を使えば、他の suite やアプリ全体のビルド・実行を避けられます。

## 現在のテスト

- ArtifactCore
  - UtilsTest: Levenshtein 距離計算と文字列/パス変換のテスト
