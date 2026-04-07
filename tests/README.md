# Artifact Suite テストガイド

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

## 現在のテスト

- ArtifactCore
  - StringTest: Levenshtein 距離計算のテスト
