# マイルストーン: Automated Testing Foundation

**最終更新:** 2026-08-21
**ステータス:** Not Started
**優先度:** High
**関連:** `docs/planned/MILESTONE_AUDIO_ROUTING_PRODUCTION_READINESS_2026-08-21.md`(既存テスト運用の実例), `docs/MILESTONES_BACKLOG.md`, `tests/README.md`

## 目的

全モジュール横断の成熟度監査(2026-08-21)で最大の弱点と判定された「自動テスト基盤の欠如」を解消する。ゼロから作るのではなく、既に存在する骨格(`ARTIFACT_BUILD_TESTS` オプション、`tests/` + gtest、`artifact_add_test()` ヘルパー、vcpkg gtest 依存)を完成させ、回帰を機械的に検出できる状態にする。

## 現状(2026-08-21 時点の実測)

- ルート `CMakeLists.txt:747-756` に `ARTIFACT_BUILD_TESTS`(デフォルト OFF)+ `enable_testing()` + `add_subdirectory(tests)` が存在。
- `vcpkg.json` に `gtest` 依存を追加済み。
- `tests/CMakeLists.txt` に `artifact_add_test()` ヘルパー、`tests/ArtifactCore/` に gtest テスト6ターゲット登録済み。`UtilsTest.cpp` は C++20 modules を `import` して動く実例。
- `x64-Debug-ClangCL-LLD` ビルド配下に `GTestConfig.cmake` が存在(発見実績あり)。`x64-Debug` 用の `out/vcpkg_installed` には未確認。
- 全モジュールで CTest/GTest 以外の自動検証なし。既存のセルフテスト(`CoreDiagnostic.Test.cppm` 等)は手動実行。
- CI(GitHub Actions 等)は存在しない。

## 完了条件

1. `ARTIFACT_BUILD_TESTS=ON` で configure → build → `ctest` が通る通し手順が確立し、手順書として文書化されている。
2. 既存6テスト(ArtifactCore)が CTest で緑になる。
3. ArtifactCore の数値系(Color変換、アニメーション補間、ジオメトリ)に gtest を追加し、既知の境界ケース(NaN、ゼロ除算、範囲端)を固定している。
4. `ArtifactRenderer` CLI(`--validate-only` / `--dump-summary`)の CTest 登録とアサーションを追加している。
5. `CoreDiagnostic.Test.cppm` 等の手動セルフテストのうち、少なくとも Diagnostics 契約分を gtest へ移殖している。
6. テスト実行を自動化する CI ワークフローが存在し、PR/push で実行される。

## 実施フェーズ

### Phase 1 — 既存骨格の通し検証

- `cmake --preset x64-Debug -DARTIFACT_BUILD_TESTS=ON` で configure し、vcpkg が gtest を解決するか確認する。
- テストターゲットをビルドし、`ctest --test-dir out/build/x64-Debug --output-on-failure` で既存6テストを実行する。
- 失敗する場合、GTest 発見問題(`test_gtest_cmake.cmake` / `CMakeLists_test.txt` の過去のデバッグ残骸が示す失敗パターン)を切り分ける。
- 通し手順を `tests/README.md` に反映する。

### Phase 2 — 数値系コアのテスト固定

- 対象: `ArtifactCore` の Color 変換(`Color/ColorConversion.cppm` 等)、アニメーション補間、ジオメトリ。
- 既知の緩い箇所を固定する: `ColorConversion` は入力 NaN 検証なし(監査で確認済み)、境界値の挙動を先に観察してから期待値を決める。
- ドメイン別ターゲット(`ArtifactCoreAudio` 等の分割 STATIC ライブラリ)単位でリンク範囲を絞り、ビルドコストを抑える。

### Phase 3 — ArtifactRenderer CLI テスト

- `--validate-only` / `--dump-summary` は Qt6 Core/Gui のみで完結し、GPU 不要。最初のアプリレベル回帰ゲートに適する。
- 有効/無効/境界のジョブJSONフィクスチャを `tests/fixtures/` に置き、CTest で実行する。

### Phase 4 — 手動セルフテストの移殖

- `src/Diagnostics/CoreDiagnostic.Test.cppm`(約15ケース)を gtest へ移殖する。
- `FFmpegEncoder.Test.cppm`、`SoftwareRayTracer.Test.cppm` はスモーク性质が強いため、環境依存を分離した上で後続とする。

### Phase 5 — CI 化

- GitHub Actions 等で `ARTIFACT_BUILD_TESTS=ON` の configure + build + ctest を自動化する。
- モジュール全体ビルドが重いため、CI ではテストに必要なターゲットに絞ってビルドする(`cmake --build ... --target <test targets>` 依存解決に任せる)。
- vcpkg キャッシュを設定し、依存解決を高速化する。

## 対象外

- UI ウィジェットのスナップショットテスト・UI自動操作テスト
- GPU/Diligent 経路の自動テスト(既存の `ArtifactRenderTextSmoke` の運用は現状維持)
- カバレッジ計測基盤の導入
- `/W4 /WX` への警告レベル変更(別途検討)

## リスクと確認方法

- **GTest 発見失敗の再燃**: 過去に vcpkg + gtest の発見問題でつまずいた痕跡がある。Phase 1 で最初につぶす。確認は configure ログと `GTestConfig.cmake` の存在。
- **C++20 modules + gtest の組み合わせ**: テスト側は非モジュール `.cpp` から `import` する構成が既存実績(`UtilsTest.cpp`)。BMI 依存解決がテストターゲットで壊れた場合は、`artifact_add_test()` の CMake 設定を疑う。
- **ビルド時間の増大**: テストターゲットは依存 STATIC ライブラリを引きずる。ドメイン別ターゲット単位の絞り込みで緩和する。
- **AGENTS.md 制約**: ビルド・テスト実行はユーザー明示指示が必要。Phase 1 の実行前にユーザー確認を取る。

## 監査での根拠(参考)

- Artifact 本体: テスト基盤なし、`src/Test/` はアプリ内メニューからの手動実行。
- ArtifactCore: tests/ なし、`CoreDiagnostic.Test.cppm` 等の自己検証3ファイルのみ、PS1 契約検査は正規表現ベース。
- ArtifactWidgets / ArtifactPr: テスト皆無。
- ArtifactRenderer: テストなし(Qt6 Core/Gui のみの小さなCLIで、テスト容易性は最高)。
