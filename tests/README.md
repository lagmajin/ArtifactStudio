# Artifact Suite テストガイド

**最終更新:** 2026-10-31

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

## 現在のテスト

- ArtifactCore
  - UtilsTest: Levenshtein 距離計算と文字列/パス変換のテスト
