# Artifact Suite テストガイド

**最終更新:** 2026-11-10

## テストの実行方法

```bash
# Configure
cmake -B build -S .

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## テストの追加方法

1. 対応するサブディレクトリ（ArtifactCore, Artifact, ArtifactWidgets）にテストファイルを作成
2. そのディレクトリの CMakeLists.txt にテストを追加

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
cmake -S tests/ArtifactCore/StandaloneSuite -B tests/ArtifactCore/StandaloneSuite/build `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF
cmake --build tests/ArtifactCore/StandaloneSuite/build --config Debug --parallel 4
ctest --test-dir tests/ArtifactCore/StandaloneSuite/build -C Debug --output-on-failure
```

この親projectはCore value 5 suite、Core utility 54 suite（AnimationDynamics・SimulationSystemを含む）、Color 16 suite、FrameRate 3 suite、Geometry 10 suite、RawImage 1 suite、Data 7 suite、Audio 50 suite、AssetDatabase 1 suite、AssetMetaFile 1 suite、AssetFingerprint 1 suite、DiagnosticEngine 1 suite、OpenEXR 2 suiteの計152テストソースを、feature単位の13 executable / CTest entryにまとめます。同じfeature内のGoogleTestケースは同一プロセスで実行されます。個別 suite の開発では `tests/ArtifactCore/CoreValueStandalone`、`CoreUtilityStandalone`、`ColorSpaceStandalone`、`FrameRateStandaloneSuite`、`GeometryStandaloneSuite`、`RawImageStandalone`、`DataStandalone`、`AudioStandalone`、`AssetDatabaseStandalone`、`AssetMetaFileStandalone`、`AssetFingerprintStandalone`、`AssetSequenceStandalone`、`DiagnosticEngineStandalone`、`OpenExrStandalone` を独立configureできます。共有生成物を使う一括構成には `StandaloneSuite` を使います。

## PBR / Ray Tracing acceptance test

`tests/ArtifactCore/RayTracingAcceptanceTest.cpp` はアプリ本体を起動しない独立 executable `ArtifactCoreRayTracingAcceptanceTest` として動きます。既存の `ArtifactCore` と Diligent Vulkan backend をリンクするため、別途 Diligent や ArtifactCore の複製ビルドは不要です。GPU は Vulkan ray tracing 対応が必要です。

既存の configure 済み build tree を使い、対象だけを増分ビルドして実行します。

```powershell
# build は普段使っている configure 済み build directory
cmake --build build --config Debug --target ArtifactCoreRayTracingAcceptanceTest
ctest --test-dir build -C Debug -R "^ArtifactCoreRayTracingAcceptanceTest$" --output-on-failure
```

その build tree で初めてこの suite を有効にする場合だけ、同じ build directory を再configureし、既存の generator・toolchain・vcpkg 設定を維持したまま `-DARTIFACT_ENABLE_RAY_TRACING_TEST=ON` を追加します。新しい build tree を作ると既存ライブラリの再利用ができない場合があるため、まず `CMakeCache.txt` がある既存 tree を確認してください。configure後も上記の個別 target と CTest filter を使えば、他の suite やアプリ全体のビルド・実行を避けられます。

## 現在のテスト

- ArtifactCore
  - UtilsTest: Levenshtein 距離計算と文字列/パス変換のテスト
