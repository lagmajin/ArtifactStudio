# マイルストーン: Module 実装単位の `.cppm` → `.cpp` 段階移行と Clang / sccache 対応

**最終更新:** 2026-10-22
**ステータス:** In Progress

## 目的

C++20 module の境界と公開 API（主に `.ixx`）を維持しながら、module implementation unit の拡張子を `.cppm` から `.cpp` へ少しずつ移す。同時に MSVC、clang-cl、本家 clang (`clang++`) のビルド経路を整え、キャッシュ可能な構成では sccache を活用する。

一括リネームは行わない。拡張子による暗黙の分類を減らし、ソースの module 宣言と明示 manifest を基準にする。

## スコープ

- `module X;` で始まる実装単位を対象にし、まず依存の少ないファイルを1つずつ `.cpp` に移す。
- `export module X;` を持つ primary interface / partition は、分類と依存スキャンが安定するまで `.ixx` / `.cppm` に残す。
- CMake の module source manifest、依存スキャン、MSVC の `CompileAsCpp` 回避策を compiler-neutral な分類へ段階的に整理する。
- clang-cl と本家 clang のコンパイル、リンク、module dependency scan を別の検証レーンとして扱う。
- sccache の利用可否と実際の cache hit を compiler・translation unit 種別ごとに記録する。非対応時は通常コンパイルへフォールバックし、正しさをキャッシュに依存させない。

## 現状監査（2026-10-22）

### ソース量の目安

Tracked source file を宣言行で分類した数。`.cpp` は拡張子で数え、`.cppm` は宣言行で分類している。ビルド対象数そのものではない。

| リポジトリ | `.cpp` | `export module` のある `.cppm` | 実装形式の `.cppm` | module 宣言のない `.cppm` | `.ixx` |
|---|---:|---:|---:|---:|---:|
| 親 ArtifactStudio | 328 | 0 | 27 | 0 | 29 |
| Artifact | 11 | 55 | 625 | 6 | 700 |
| ArtifactCore | 126 | 45 | 348 | 22 | 851 |
| ArtifactWidgets | 47 | 0 | 0 | 4 | 63 |

### CMake の現行責務

- ルート `CMakeLists.txt` はローカル module target の `CMAKE_CXX_SCAN_FOR_MODULES` を有効にし、clang-cl 用に `clang-scan-deps` を探す。ただし探索先には Windows / Visual Studio 固有パスが含まれる。
- ルートでは `ARTIFACT_USE_SCCACHE` が既定 ON。本家 clang++ GNU frontend は launcher を使い、clang-cl と MSVC-style frontend は module/PDB 対応が不足するため launcher を無効化する。非対応 module command は sccache が bypass する。
- `ArtifactCore/cmake/ArtifactCoreSources.cmake` は source manifest を明示管理し、`.cppm` を interface sources と implementation sources に分けている。新しい実装 `.cpp` を登録する規則は別途必要。
- `ArtifactWidgets/CMakeLists.txt` は `.cpp` / `.cppm` / `.ixx` を glob し、`export module` の有無で module / implementation に分類する。MSVC では実装 `.cppm` に `CompileAsCpp` を付けている。
- `Artifact/CMakeLists.txt` も manifest に加え、`.cppm` 実装の明示登録と MSVC 固有の実装コンパイル設定を持つ。

### 外部仕様から分かること

- CMake 3.30 は CXX module source を内容スキャンし、CXX として認識される拡張子を対象にする。拡張子そのものは module 種別を決めない。ただし compiler と generator の対応が前提で、Ninja / Ninja Multi-Config / Visual Studio generator の制約を確認する必要がある。
- [CMake 4.4 release notes](https://cmake.org/cmake/help/latest/release/4.4.html) と [C++ modules manual](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html) では、clang-cl 19.1+ の named modules 対応は 4.4 で追加され、Visual Studio generator は対象外。プロジェクト最小の CMake 3.30 は clang-cl module scanning の基準にできない。
- [sccache README](https://github.com/mozilla/sccache/blob/main/README.md#c20-modules) は Clang named modules を部分対応とし、Clang の `-fmodule-file` / `-fmodule-output` / `--precompile` 等を cache 対象候補に挙げる。MSVC `/interface` / `/ifcOutput` 等の IFC と GCC modules は未対応。CMake の `@*.modmap` response-file を渡す Clang module command は [sccache issue #2650](https://github.com/mozilla/sccache/issues/2650) で非 cacheable と報告されており、現行版での解消状態を cache stats と `--show-stats` で確認する。
- Clang の module dependency discovery は `clang-scan-deps` による P1689 scan を前提とする。

## 検証マトリクス

| レーン | Host / compiler | CMake / generator | Module scan | sccache の期待値 | 状態 |
|---|---|---|---|---|---|
| 基準 | Windows / MSVC `cl.exe` | CMake 4.4.4 / Ninja | IFC 経路 | plain `.cpp` は単独 command で miss→hit、module interface / `/reference` 実装は bypass | ContentViewer の隔離 DLL、ArtifactCoreEnvironment archive、launcher 無効構成での ArtifactWidgets.dll link が成功。sccache opt-in の並列 OpenUSD `tf.pdb` compile は C1041/C1090 で失敗 |
| Windows LLVM | Windows / clang-cl | CMake 4.4.4 / Ninja | `clang-scan-deps` と P1689 を確認 | ArtifactCoreEnvironment implementation は1 miss→1 hit。interface は `multiple input files` で bypass | Pane / DialogPlacement / ToggleGlyphButton の隔離 DLL と ArtifactCoreEnvironment archive が成功。root 全体は未完了 |
| **先行対象** | Windows / upstream `clang++`、MSVC ABI (`--target=x86_64-pc-windows-msvc`) | CMake 4.4.4 / Ninja | 実 target の P1689 scan と `.cpp` implementation を確認 | Core target 3 miss→3 hit。ToggleGlyphButton は2 miss→2 hit。過去 pilot の warm-cache hit は別記 | Environment target archive と ClickableLabel / DialogPlacement / ToggleGlyphButton の隔離 DLL 成功。ArtifactCore target は OpenUSD `arch` の `demangle.cpp` / `errno.cpp` で停止 |
| LLVM frontend 比較 | Windows / `clang-cl` | CMake 4.4.4 / Ninja | `clang-scan-deps` と P1689 を確認 | `.cpp` hit、module interface bypass を実測 | sccache smoke 成功、root full target 未完了 |
| 後続候補 | Linux / `clang++` GNU frontend | 3.30 以上 / Ninja | P1689 scan、`.cpp` implementation unit を確認 | Clang named-module 対応範囲で段階検証 | 未実行 |

### `.cpp` implementation-unit pilot（ArtifactWidgets / MSVC）

- `ArtifactWidgets/src/Dock/Pane.cppm` の内容を保ったまま `Pane.cpp` へ移し、`ArtifactWidgets/CMakeLists.txt` の明示 source 登録と `.cpp eol=lf` を追加した。
- 既存 `out/build/x64-Debug-Agent` を再構成した MSVC + Ninja で `Pane.ixx` の依存スキャン後に `src/Dock/Pane.cpp.obj` が正常にコンパイルされた。よって `.cpp` が implementation source として扱われ、module interface の依存も解決することを確認した。
- `ArtifactWidgets.dll` の最終リンクは CMake の `vs_link_dll` 内で manifest resource compiler が `no such file or directory` と報告して停止した。同じ `rc.exe` と `manifest.rc` を手動実行すると成功するため、最終成果物のリンク成功は未確認であり、MSVC lane 全体の受け入れとはしない。
- ビルドは既存 MSVC 構成の依存全体を再構築し 2,111 action に達した。既存コード由来の警告が複数出たが、`Pane.cpp` のコンパイル失敗はなかった。
- 追加 pilot として `ArtifactWidgets/src/Common/AlignmentComboBox.cppm` を `AlignmentComboBox.cpp` へ移し、CMake の implementation source list に登録した。CMake 4.4.4 / clang-cl / Qt6 の隔離 target で実ソースを compile/link した。Widgets 全体の再リンクは未確認。
- `ArtifactWidgets/src/Common/ClickableLabel.cppm` を `ClickableLabel.cpp` へ移し、同じ明示 implementation source list に登録。CMake 4.4.4 / Ninja / clang-cl 22.1.3 + Qt6 の隔離 DLL を compile/link。続けて upstream clang++ GNU frontend + Windows MSVC ABI の Qt6 隔離 DLL も compile/link 成功。Verdigris `W_OBJECT_IMPL` を含む実装で両 frontend の module scan / object compile / link を確認した。
- upstream clang++ ClickableLabel pilot を sccache launcher 下で clean/build 2回実行し、interface `.ixx` と implementation `.cpp` の4 compile request がすべて hit。直前の同一 command の compile cache が既に存在していたため、同 pilot での fresh miss は測っていない。
- `ArtifactWidgets/src/Common/DialogPlacement.cppm` を `.cpp` へ移行し、明示 source list に登録。event filter と dialog centering を含む source を clang-cl と upstream clang++ の両方で scan / compile / Qt6 DLL link。upstream clang++ の sccache 下で clean/build 2回、interface / implementation 合計4 compile hit。cache entry はその前の初回 pilot build で作成済み。
- `ArtifactWidgets/src/Common/ToggleGlyphButton.cppm` を `.cpp` へ移行。CMake の明示 source list に追加し、clang-cl と upstream clang++ + MSVC ABI の両方で P1689 scan / interface・implementation compile / Qt6 DLL link 成功。upstream clang++ + sccache 0.18 で clean build 2回、初回2 miss・2回目2 hit、bypass なし。
- `ArtifactWidgets/src/Viewer/ContentViewer.cppm` を `.cpp` へ移行。明示 source list に追加し、CMake 4.4.4 / Ninja / Qt6 で両 Clang frontend の P1689 scan、interface / implementation compile、DLL link が成功。同じ隔離 target は MSVC 19.51 でも IFC scan / compile / DLL link 成功。sccache を通した2回の MSVC build では `-interface` 1件と `/reference` 1件が各回 bypass。
- `ArtifactWidgets/src/Time/TimeCodeLabel.cppm` を `.cpp` へ移行し、明示 implementation source list に追加した。公開 `Widgets.TimeCodeLabel` interface はそのまま保持し、実装本文は拡張子移行と LF 改行だけを変更した。
- `ArtifactWidgets/src/Color/ColorViewLabel.cppm` を `.cpp` へ移行し、明示 implementation list に追加した。公開 interface、`W_OBJECT_IMPL`、Impl 所有、color accessors、paintEvent の実装を維持し、本文を変えず LF 改行へ正規化した。
- `ArtifactWidgets/src/Common/CollapsibleSection.cppm` を `.cpp` へ移行し、明示 implementation list に追加した。公開 `CollapsibleSection` interface の全 method definition、`W_OBJECT_IMPL`、Impl 所有を保ち、本文を変えず LF 改行へ正規化した。
- 追加で clang-cl の実ソース経路を root `ArtifactCoreEnvironment` target で検証。CMake 4.4.4 / Ninja が `EnvironmentVariable.cpp` の P1689 scan と dyndep を生成し、clang-cl compile と static library link に成功。clang-cl では移行済み `.cpp` のみを scan 対象にし、GNU frontend の `-x c++` は付けない。

### ArtifactCore EnvironmentVariable / upstream clang++

- `ArtifactCore/src/EnvironmentVariable/EnvironmentVariable.cppm` を `.cpp` へ移し、source manifest、module reference map、split target の source path を更新した。ArtifactCore の `.cpp` に LF 指定を追加。
- Upstream Clang 22.1.3 GNU frontend + `x86_64-pc-windows-msvc` ABI、CMake 4.4.4 / Ninja で root configure と `ArtifactCoreEnvironment` target build に成功。vcpkg `FindStb.cmake` の directory を `CMAKE_MODULE_PATH` に足す必要があった。P1689 scan、`.cpp` compile、static library archive まで確認。
- `.cppm` implementation が clang++ で interface 扱いされないよう `-x c++` を付ける。Clang GNU lane では MSVC 用 `/reference` と source `OBJECT_DEPENDS` の `.ifc/.obj/.dd` 手動依存を除外し、CMake の module map / dyndep に任せる。これにより `clang-scan-deps` の正しい module attachment requirement と既存 MSVC object ordering が競合していた cycle を解消。
- `ArtifactCoreEnvironment` に限った検証であり、他 split targets、Qt / Diligent 統合、アプリ全体 link は未確認。

- 追加確認として `ArtifactCoreColorCollection` target を起動した。`arch` target の `stackTrace.cpp` は親 CMake から `NOMINMAX` を与えると compile できた。一方 OpenUSD `arch` は `demangle.cpp` の `cxxabi.h` / Itanium ABI 分岐、`errno.cpp` の `strerror_r`、`attributes.cpp` の compiler/platform 分岐で停止した。OpenUSD submodule は変更していない。試した `-U__clang__` target flag は MSVC STL の Clang builtin detection を壊すため撤回。ColorCollection の source compile には到達せず、別 split target の module 設定は未確認。

### sccache を使った実 target 再ビルド（2026-10-22）

- root configure で `ARTIFACT_USE_SCCACHE=ON` と sccache 0.18.0 の実行ファイルを明示し、GNU frontend launcher として認識された。OpenUSD の `arch` target には Windows SDK `min/max` macro を抑止する `NOMINMAX` を親側から追加し、`stackTrace.cpp` object の compile 成功を確認。
- `ArtifactCoreEnvironment` を sccache 下で2回 clean/build。1回目は3 compile miss、2回目は `.ixx` 2件と implementation `.cpp` 1件の3 hit。累計 6 compile requests / 3 hits / 3 misses / 0 non-cacheable calls。両方 static archive まで成功。
- clang-cl lane も opt-in (`ARTIFACT_USE_SCCACHE_WITH_CLANG_CL=ON`) で `ArtifactCoreEnvironment` を2回 clean/build。今回の2回分は合計6 compile requests、実装 `.cpp` が1 miss・1 hit、module interface が `multiple input files` により各回2件 bypass。両回で P1689 scan と static archive 生成に成功。
- cache hit 時も `ArtifactCoreEnvironment.lib` の生成と Ninja dyndep scan を確認。異なる build の成果物との bytewise 比較、広い target の cache behavior は未確認。
- MSVC 19.51 の ContentViewer 隔離 target を sccache 0.18 経由で2回 build。各回の module interface (`-interface`) と module implementation (`/reference`) は bypass (合計4 calls)。通常 `plain_implementation.cpp` は単独 compile で1 miss→1 hit。MSVC root configure で launcher を opt-in にし `ArtifactCoreEnvironment` archive は成功したが、`ArtifactWidgets` build は OpenUSD `tf` の同一 PDB へ複数 compile が並列書き込みする時に `C1041/C1090` で停止した。同 compile は sccache 経由の単独実行と直接 cl.exe では成功し、parallel compile の launcher 相互作用が疑われる。MSVC 用 launcher opt-in は root CMake から撤去し既定無効を維持した。launcher 無効構成では dedicated build tree で `ArtifactWidgets` target が 576 action を完了し、`bin/Debug/ArtifactWidgets.dll` の link に成功した。

### clang-cl: CMake 4.4 + actual Pane source

- CMake 4.2.2 は clang-cl の module scan/generate rule を提供せず、CMake 4.4.4 では同じ lane の rule が利用できた。clang-cl を使うルート CMake 構成は 4.4 以上と Ninja に限定する gate を加えた。ArtifactCore の移行済み `.cpp` で実 target scan / compile / archive も確認した。
- CMake 4.4.4 / clang-cl 22.1.3 / Ninja で、実際の `ArtifactWidgets/src/Dock/Pane.cpp` と `include/Dock/Pane.ixx` をコンパイルし、Qt6 Core / Widgets をリンクした `PaneClangClSmoke.dll` が生成された。
- Root configure は OpenMP の flags、`libomp` 名、VS toolset の `libomp.lib` パスを build-dir cache で明示すると configure/generate まで通った。既定の `find_package(OpenMP)` compiler probe 自動検出は引き続き別課題。別の root configure 再試行では vcpkg manifest install が長時間走ったため中断した。
- Root の実 target build は OpenUSD static build 中に clang-cl が `/wddeprecated`, `/wdunused-local-typedefs` など無効な warning id を拒否して停止した。これは Pane の compile 失敗ではなく、third-party compiler options を clang-cl 向けに切り分ける必要がある。
- したがって clang-cl の実ファイル compile/link feasibility と `ArtifactCoreEnvironment` は確認済み。root 全体 build と OpenMP 自動検出は未成立。

### sccache v0.18 実測

- Windows sccache v0.18.0 で upstream clang++ の通常 `.cpp` forced rebuild は 1 hit / 0 miss。module smoke の `.ixx` interface、importer `.cpp`、implementation `.cpp` は各 1 hit となり、build/run exit 0。
- clang-cl の module smoke では `.ixx` interface が `multiple input files` で non-cacheable、importer `.cpp` と implementation `.cpp` は 2 hits / 0 misses。build/run exit 0。
- 初回 compile はそれぞれ cache miss で完了し、非対応 command の bypass を確認した。従って clang-cl は sccache opt-in の現行方針を保ち、cache hit を前提条件にしない。
- P1689 の `requires: Smoke.Greeting` は primary module attachment の正規情報だった。最小 CMake/Ninja smoke では処理でき、root split target cycle は ArtifactCore に残っていた MSVC `OBJECT_DEPENDS` / `/reference` 手動依存との重複に起因した。
- GNU frontend lane で implementation scan を有効に戻し、MSVC 固有 module reference と file-level object edges を除外した後、`ArtifactCoreEnvironment` の実 target で `.cppm` と `.cpp` の両方の scan/build が成功。scan を無効化する回避策は採用しない。
- **次の作業:** OpenUSD `CXXDefaults.cmake` に GNU-like Clang + MSVC ABI を区別する公開 option は見つからず、compiler ID `Clang` は `clangdefaults.cmake` へ分岐する。現状は split target を止めるため、module map dependency ordering は未確認。clang-cl root build の third-party warning flags、OpenMP 自動検出、全体 link も残る。
- CMake 4.2.2 の `Compiler/Clang-CXX.cmake` は `CMAKE_CXX_SCANDEP_SOURCE` と Clang module map / BMI flags を GNU frontend の場合だけ設定する。MSVC frontend へ同じ GNU-style `-x c++`, `-MD`, `-MF` を流用すると clang-cl が未対応引数として警告する。MSVC frontend 用の P1689 command translation / module map flags、または CMake の対応版更新が必要。
- これは pilot の `.cpp` compile エラーではない。clang-cl lane は未成立。

ユーザー決定により **Windows upstream `clang++` + MSVC ABI を先行対象**とする。Windows clang++ は **MSVC ABI と Windows SDK / vcpkg triplet (`x64-windows`) を共有し、GNU driver frontend として動かすレーン**を先に成立させる。`clang-cl` と同一視せず、CMake の `MSVC` 判定、MSVC 専用 compiler options / `/reference` / `std.ifc`、ATL・SDK の探索、Qt / Diligent / vcpkg の ABI・toolchain 整合を個別に監査する。先行受入は Core / Widgets の小さな module subset で、GUI 全体は別の段階とする。

### Windows clang++ (MSVC ABI) feasibility gate

- この環境には Visual Studio LLVM 配下の `clang++.exe` と `clang-scan-deps.exe` がある。ただし導入済み実体が upstream LLVM 配布版と同一のオプション・module 対応とは限らないため、配布元と版を確認してから preset に固定する。
- CMake は GNU frontend と認識するため、既存 `ARTIFACT_FRONTEND_MSVC` 経路には自動で入らない。一方、プロジェクトには `MSVC` 判定に結びついた Windows SDK / ATL セットアップ、MSVC `/reference`・`.ifc` の供給、MSVC module flag が多数残る。GNU frontend で必要な `--target`、MSVC STL include / library paths、COFF linker / resource compiler の選択、および各依存ライブラリの ABI を設計する必要がある。
- vcpkg の `x64-windows` ABI は方向性として合うが、現在の `cmake/toolchain-msvc-v143.cmake` は `cl.exe` と v143 の検出を行う。clang++ 用にこの toolchain を流用せず、compiler/toolchain の初期化を分ける。
- Windows clang++ を `--driver-mode=cl` で MSVC frontend として扱う案は upstream `clang++` の GNU driver という要件から逸れるため、先行対象には採らない。
- 手元の CMake 4.2.2 `Compiler/Clang-CXX.cmake` は GNU frontend の Clang 16+ に P1689 scan rule を用意する。`Windows-Clang.cmake` は Windows + GNU frontend を MinGW/GNU toolchain lane として選び、`-fuse-ld=lld-link` と GNU-style link rules を構成するため、GNU frontend のままで MSVC ABI を使うには linker/runtime/library rules を toolchain で明示上書きする必要がある。これはプロジェクト全体に preset を追加する前に、隔離 smoke project で検証する。
- Windows clang++ の driver probe (`--target=x86_64-pc-windows-msvc` なし) は MSVC 互換 target と Windows SDK header paths を選び、MSVC 14.51 と ATL include は VS Insiders 側を指した。smoke toolchain もこの toolset に合わせた。vcpkg manifest は `x64-windows` のまま使える見込みだが、CMake ABI tag と compiler hash が一致するか未確認。
- GNU-style `-###` link command は MSVC CRT defaultlib と SDK / toolset library paths を示した。CMake 4.2.2 の既定 Windows GNU-frontend rule にある `-nostartfiles` / `-nostdlib` が COFF link と衝突したため、`CMAKE_USER_MAKE_RULES_OVERRIDE_CXX` で GCC/MinGW の標準 library 初期値と Clang の MSVC ABI linker defaults を設定し、Clang driver の `lld-link` 経路を維持する smoke 用 rules override を作成した。
- ルート + `ArtifactCore` / `ArtifactWidgets` / `Artifact` / `DockFoundation` の CMake 監査では、explicit `/reference;Module=...ifc`、MSVC std.ifc producer、MSVC `/std` `/experimental:module`、`VS_SETTINGS CompileAsCpp` に加え、MSVC linker 専用 link option と library/layout 前提が見つかった。MSVC frontend を真似て compiler variant を上書きするのではなく、Clang GNU frontend 用の標準 module map と compile rule を保ちつつ、Windows MSVC ABI の COFF link/runtime/search path を toolchain 側で設定する必要がある。上記 module-reference source overrides は Clang lane では除外・置換する設計が必要で、repo-wide GNU frontend 対応は小さな subset の成功後に扱う。
- 実行環境には CMake 4.2.2、clang++ / clang-cl / clang-scan-deps 22.1.3、Ninja、MSVC 14.51、Windows SDK 10.0.26100、`C:/vcpkg/installed/x64-windows` が存在する。sccache v0.18.0 は isolated cache smoke で upstream clang++ の `.cpp` hit と module interface / importer / implementation hit を記録した。
- **合格条件:** CMake が `Clang` + GNU frontend と検出し、Windows MSVC ABI の最小 smoke module provider / importer が Ninja dyndep で順序付けられ、`.cpp` implementation unit をコンパイル・リンクできる。続いて Qt / vcpkg / Diligent の最小依存を一つずつ追加する。
- **結果 (2026-10-21):** `tools/clang-msvc-abi-smoke` と `cmake/toolchain-clangxx-msvc-abi.cmake` で CMake 4.2.2 + Ninja 1.12.1 + clang++ 22.1.3 の configure/build に成功した。GNU frontend / `x86_64-pc-windows-msvc` を確認し、通常 importer と、別 primary module `Smoke.Greeting` に属する `module Smoke.Implementation;` 実装単位の両方から `Smoke.Greeting` を import する構成で compile/link と Windows 実行 exit 0 を確認した。実装ユニット自身が primary `Smoke.Greeting` に attachment する場合は clang-scan-deps 22.1.3 が自己 `requires` を出力し、CMake 4.4.4 / Ninja で cycle を作ることも確認した。
- **制約:** isolated smoke は独立 primary module の実装を検証したが、ArtifactCore の多くを占める同名 primary interface attachment は scanner cycle により未成立。ArtifactCore / Widgets 全体の `/reference`・`std.ifc`・compiler option / link 統合、vcpkg / Qt / Diligent の本家 clang++ 対応は未確認。

## フェーズと合格条件

### Phase 0: 分類と基準を固定

- `.ixx` interface、`.cppm` interface / partition、`.cppm` implementation、通常 `.cpp` を manifest 上で区別する。
- compiler ID、frontend variant、CMake version、generator、`clang-scan-deps` の実体を configure summary で確認できるようにする。
- 現行 MSVC 構成の module scan / link baseline を採取する。
- **合格:** 1ファイルの移行後に、提供 module と import dependency の登録先が一意に追跡できる。

### Phase 1: 小さな implementation unit を `.cpp` に移す

- 依存が少なく、対応する public interface が明確な implementation unit を選ぶ。
- `.cppm` から `.cpp` への rename と、そのファイルを列挙する manifest 変更だけに差分を限定する。
- `.cpp` でも module scan が有効であり、provider / importer の dyndep 順序が維持されることを確認する。
- **合格:** MSVC と clang-cl、本家 clang の対象 subset でコンパイル・リンクが成立し、同名 module / unresolved symbol / stale BMI がない。

#### Pilot 1: `ArtifactWidgets/src/Dock/Pane.cppm` → `.cpp`（2026-10-21）

- `module DockWidget;` の implementation unit で、interface は `include/Dock/Pane.ixx`。
- `W_OBJECT_IMPL(Pane)`、constructor、destructor が実装ファイルに各1箇所あることを静的確認した。移動ファイルには元ファイルと同じ内容・BOMを維持し、改行のみ LF へ正規化した。Git working tree が CRLF だった旧 `.cppm` と新 `.cpp` の blob hash は異なるため、内容・BOM と LF-only を別々に確認した。
- `ArtifactWidgets/CMakeLists.txt` の `src/*.cpp` glob と、pilot `.cpp` を `WIDGETS_IMPL` に明示追加する manifest を確認した。明示登録で CMakeLists の変更を発生させ、次回 configure で source list が更新される。MSVC の `CompileAsCpp` 指定は `.cppm` のみを対象とする。
- `ArtifactWidgets/.gitattributes` に `*.cpp text eol=lf` を追加し、このサブモジュールで `.cpp` の LF を固定した。
- **状態:** MSVC + Ninja で `.cpp` の module dependency scan / object compile 済み。最終 DLL link は manifest resource wrapper エラーで未完了、runtime も未確認。

### `.cpp` implementation-unit pilot（ArtifactWidgets / MSVC）

- `ArtifactWidgets/src/Dock/Pane.cppm` の内容を保ったまま `Pane.cpp` へ移し、`ArtifactWidgets/CMakeLists.txt` の明示 source 登録と `.cpp eol=lf` を追加した。
- 既存 `out/build/x64-Debug-Agent` を再構成した MSVC + Ninja で `Pane.ixx` の依存スキャン後に `src/Dock/Pane.cpp.obj` が正常にコンパイルされた。よって `.cpp` が implementation source として扱われ、module interface の依存も解決することを確認した。
- `ArtifactWidgets.dll` の最終リンクは CMake の `vs_link_dll` 内で manifest resource compiler が `no such file or directory` と報告して停止した。同じ `rc.exe` と `manifest.rc` を手動実行すると成功するため、最終成果物のリンク成功は未確認であり、MSVC lane 全体の受け入れとはしない。
- ビルドは既存 MSVC 構成の依存全体を再構築し 2,111 action に達した。既存コード由来の警告が複数出たが、`Pane.cpp` のコンパイル失敗はなかった。

### clang-cl root configure blocker

- clang-cl の root configure は既存 scan override の下で ArtifactWidgets の `find_package(OpenMP)` 内部 `try_compile` に到達するが、CMake `FindOpenMP` の試験コンパイルが `clang-scan-deps` へ clang-cl 固有の引数を渡せず `.ddi` 生成に失敗した。
- これは pilot の `.cpp` compile エラーではない。`OpenMP` 検出時の scanner / try-compile 境界を別途解決する必要があり、clang-cl lane は未成立。- OpenMP の候補 flags / libs / spec date (`-Xclang -fopenmp`, VS `libomp.lib`, `202011`) と Qt の `HAVE_STDATOMIC=ON` を build-dir cache だけに指定すると依存 configure は通過したが、generate 時に CMake 4.2.2 が module target の clang-cl scan rule を提供せず停止した。
- CMake 4.2.2 の `Compiler/Clang-CXX.cmake` は `CMAKE_CXX_SCANDEP_SOURCE` と Clang module map / BMI flags を GNU frontend の場合だけ設定する。MSVC frontend へ同じ GNU-style `-x c++`, `-MD`, `-MF` を流用すると clang-cl が未対応引数として警告する。MSVC frontend 用の P1689 command translation / module map flags、または CMake の対応版更新が必要。
- これは pilot の `.cpp` compile エラーではない。clang-cl lane は未成立。
### Phase 2: CMake の拡張子依存を縮小

- `.cppm` 固定の implementation list、`CompileAsCpp` 指定、force list の根拠を整理する。
- public interface の登録は `FILE_SET CXX_MODULES` に保ち、実装単位は通常 source として登録する。
- interface と implementation の判定を、可能な範囲で明示 manifest 化する。内容スキャンの重複・曖昧判定を増やさない。
- **合格:** 移行対象の拡張子を戻さなくても MSVC と Clang の依存グラフが一致する。

### Phase 3: 移行バッチを広げる

- 末端・低 fan-in の implementation unit から開始し、service、layer、composition の順に進める。
- 1バッチごとに renamed source、manifest、module hygiene、link/runtime の確認をまとめる。
- Artifact / ArtifactCore / ArtifactWidgets は各 child repository で順に更新し、親の gitlink は child の整合後に反映する。
- **合格:** `.cppm` は真の module interface / partition に限定され、残存 implementation の例外と理由が記録されている。

### Phase 4: sccache を実測で有効化

- まず本家 clang の通常 `.cpp`、次に Clang module interface / importer、最後に clang-cl の対応 flag 構成を別々に試す。
- 既定無効・明示 opt-in から始め、sccache statistics の compile / hit / miss / unsupported を記録する。
- MSVC IFC は非対応のまま cache bypass を許容する。clang-cl で launcher を使う場合も module scanner と compiler invocation の双方を測る。
- **合格:** cache hit が再現し、clean / incremental build の出力が uncached build と一致し、非対応 compiler では通常起動へ戻る。

#### 初期 opt-in

ルート `CMakeLists.txt` に `ARTIFACT_USE_SCCACHE_WITH_CLANG_CL`（既定 OFF）を追加した。MSVC-style frontend は module/PDB の並列 compile が未解決のため launcher を無効に保つ。sccache v0.18.0 の隔離 smoke では clang-cl の通常 `.cpp` module units が cache hit となり、module interface command は bypass した。

## 実施制約と未確認事項

- CMake 3.30 の clang-cl module support、root full build の clang-cl/OpenMP の自動検出、GUI 全体の upstream clang++ 対応は未検証。Upstream Clang root target build は OpenUSD `arch` で停止する。Windows MSVC ABI 向けの isolated Qt / module pilots は成功しているが、OpenUSD は `__clang__` を優先して Itanium/GNU ABI 分岐を選び、Windows SDK 上で `cxxabi.h` / `strerror_r` / constructor-entry declarations に失敗する。コンパイラ識別マクロの上書きでは `defines.h` が再定義するため解消できず、portable な親側 workaround は確認できていない。OpenUSD submodule patch はユーザー指示により適用しない。
- sccache v0.18.0 で `ArtifactCoreEnvironment` の実 target hit と隔離 smoke の hit/bypass を確認した。ArtifactStudio full build の cache hit rate と uncached build との成果物比較は未検証。
- 本家 clang を Windows GUI / Diligent の正式対応に含めるか、まず portable subset とするかは feasibility 結果で判断する。

## 参考資料

- [CMake 3.30 C++ modules manual](https://cmake.org/cmake/help/v3.30/manual/cmake-cxxmodules.7.html)
- [Latest CMake C++ modules manual](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html)
- [sccache README: C++20 Modules](https://github.com/mozilla/sccache/blob/main/README.md#c20-modules)
- [Clang Standard C++ Modules](https://clang.llvm.org/docs/StandardCPlusPlusModules.html)
- Existing migration milestone: `MILESTONE_CPP_HPP_ELIMINATION_2026-06-12.md`

### sccache を使った Clang module 実測（2026-10-21）

- root configure で `ARTIFACT_USE_SCCACHE=ON` を指定すると upstream clang++ 用 launcher として認識された。ただし再 configure は `ArtifactCore` の `find_package(Stb REQUIRED)` で停止し、新 build graph は生成されなかった。
- 既存 upstream clang++ / MSVC ABI module smoke target を sccache launcher で2回 clean/build した。両回とも interface `.ixx`、importer `.cpp`、implementation `.cpp` の3 compile request がすべて cache hit。累計 6 requests / 6 hits / 0 misses / 0 non-cacheable calls。
- この測定は isolated module smoke に限る。root `ArtifactCoreEnvironment` の sccache compile hit は未確認。

### 次の migration pilot（2026-10-22）

- MSVC 19.51 / CMake 4.4.4 / Ninja の dedicated `x64-Debug-MSVC-SccachePilot` build tree で `ArtifactWidgets` target を launcher 無効構成で再実行し、Ninja 576 action 完了、`ArtifactWidgets.dll` link 成功を確認した。sccache opt-in で起きた OpenUSD PDB エラーは再現しなかった。
- 次の小規模 pilot として `ArtifactWidgets/src/Time/TimeCodeLabel.cppm` を `TimeCodeLabel.cpp` に移し、CMake の明示 implementation list に加えた。公開 interface とソース本文を保持し、改行だけ LF に正規化した。MSVC 19.51 の本体 `ArtifactWidgets` target は依存 CMake 再生成後に576 actionを完了し、`ArtifactWidgets.dll` のリンクに成功。upstream clang++ MSVC ABI と clang-cl の独立 pilot でも P1689 scan / compile / Qt DLL link に成功した。独立 pilot は `TimeCodeEditor.ixx` が import するが宣言に使わない `Frame.Position` を最小スタブで補っているため、実 Core module との統合確認は MSVC 本体 target の証拠を使う。両 Clang lane の sccache compile request ごとの hit/miss 数はこの pilot では採取していない。



### Upstream clang++ Artifact target 再開（2026-10-22）

- 先行対象の Windows upstream clang++ / MSVC ABI build tree で `Artifact` target を再実行した。ArtifactCore と ArtifactWidgets の関連 target は再構築後に成功しているが、Artifact 全体は未完了。
- `ArtifactPreviewCompositionPipeline.cppm` が Clang 22.1.3 の `malformed or corrupted precompiled file: ran out of source locations` で停止。最新再実行の診断は約2.135 GB（99%）で、`Artifact.Layer.Text` import の位置で失敗した。別試行でも約2.14–2.15 GBに達し、隣接する `ArtifactProject.cppm` と `ArtifactLayerFactory.cppm` にも同種の上限超過が出た。
- 失敗時 modmap は154個の module BMI を列挙し、主要 BMI は約68–85 MiB。直接 import を確認すると、`Artifact.Layer.Abstract` は約30 module、`Artifact.Composition.Abstract` は約24 moduleを実装ファイル全体の推移依存として取り込んでいる。Qt `qcompilerdetection.h` が同 translation unit に約1,451回、約80 MiBのsource location entryとして現れる診断も得た。
- Preview 実装の未使用標準／Qt／OpenCV header を整理し、実使用する型の直接 import は維持した。再ビルドでも2.13 GB・99%で失敗し、実装ファイルの include 整理だけでは解消しないと確認した。この整理は clang++ full target を通す修正としては未成立。`Artifact.Layer.Abstract` / `Artifact.Composition.Abstract` の公開依存を安全に分割するか、Clang の module AST source-location limit を回避できるかが次の調査対象。clang++ full target、clang-cl full target、Artifact 全体での sccache 統計は引き続き未確認。
### ColorViewLabel migration / compiler cache pilot（2026-10-22）

- MSVC 19.51 / CMake 4.4.4 / Ninja の dedicated `x64-Debug-MSVC-SccachePilot` で `ArtifactWidgets` target を再生成後に build。436 action を実行し、`ColorViewLabel.cpp` の module scan、object compile、`ArtifactWidgets.dll` link が成功した（sccache launcher 無効）。
- 独立 Qt6 pilot は実 `ColorViewLabel.ixx` / `.cpp` を使い、`Color.Float` のみテスト用BMI stubで補完。CMake 4.4.4 / Ninja の upstream clang++ + Windows MSVC ABI と clang-cl で P1689 scan、interface/implementation compile、DLL link が成功した。Clang pilot は実 ArtifactCore dependency graph の代替証拠ではない。
- sccache 0.18.0 + upstream clang++ で cache buster を分離し、clean/build を2回実施。compile requests 6、初回3 misses、2回目3 hits、non-cacheable calls 0、失敗0。
- 同じ測定を clang-cl で実施。compile requests 6、cacheable compile 2件は1 miss→1 hit、`multiple input files` reason の bypass 4件、失敗0。いずれの2回目も DLL link 成功。


### CollapsibleSection migration / compiler cache pilot（2026-10-22）

- MSVC 19.51 / CMake 4.4.4 / Ninja の dedicated `x64-Debug-MSVC-SccachePilot` で `ArtifactWidgets` target を再生成後に build。436 action を実行し、`CollapsibleSection.cpp` の module scan、object compile、`ArtifactWidgets.dll` link が成功した（sccache launcher 無効）。
- Qt6 pilot は実 `CollapsibleSection.ixx` / `.cpp` と Verdigris include を使用し、CMake 4.4.4 / Ninja の upstream clang++ + Windows MSVC ABI と clang-cl で P1689 scan、interface / implementation compile、DLL link が成功した。独自 module import のstubは使っていない。
- sccache 0.18.0 + upstream clang++ で cache buster を分離し、clean/buildを2回実施。compile requests 4、初回2 misses、2回目2 hits、non-cacheable calls 0、失敗0。
- 同じ測定を clang-cl で実施。compile requests 4、cacheable implementation compile は1 miss→1 hit、module interface command は `multiple input files` により2回 bypass、失敗0。両回 DLL link 成功。

### Upstream Clang root feasibility check（2026-10-22）

- root `arch` target を実行し、Windows clang++ MSVC ABI の OpenUSD 依存 blocker を再現した。`demangle.cpp` は `cxxabi.h` 不在、`errno.cpp` は `strerror_r` 不在、`attributes.cpp` は Windows constructor-entry types 不在で失敗する。
- 親 CMake から Clang builtin macro を先読み後に解除する preinclude shim、OpenUSD compiler macros の configure-scope / target-scope 上書きを一時検証した。いずれも source/directory scope と OpenUSD `defines.h` の再定義により root-level の安全な workaround にはならず、試験変更は残していない。OpenUSD submodule は変更していない。
- isolated Qt pilots での upstream `clang++ --target=x86_64-pc-windows-msvc` scan / compile / link 成功は維持される。これを OpenUSD を含む root build の対応証拠と混同しない。
- Pixar の OpenUSD `dev` branch の現行 `defines.h` / `attributes.h` / `demangle.cpp` / `errno.cpp` も確認した。`__clang__` 優先の compiler 判定と、Windows MSVC ABI では使えない `cxxabi.h` / `strerror_r` 分岐は残っている。参照: [defines.h](https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/dev/pxr/base/arch/defines.h)、[attributes.h](https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/dev/pxr/base/arch/attributes.h)、[demangle.cpp](https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/dev/pxr/base/arch/demangle.cpp)、[errno.cpp](https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/dev/pxr/base/arch/errno.cpp)。
- 現行 upstream にも修正がないため、root full Clang build は blocker 付きで記録する。OpenUSD submodule の修正・gitlink 更新は行わず、親側の安全な workaround またはユーザーの明示依頼を待つ。

### OpticalFlowResult migration / three-compiler pilot（2026-10-22）

- `ArtifactCore/src/Analyze/OpenCV/OptifalFlowResult.cppm` は `module Analyze.OpticalFlow;` と空の実装コメントだけを持ち、型と関数は public interface `.ixx` 内で inline 定義されている。source body は変更せず `.cpp` へ移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の専用 Analyze target source registration を更新した。
- MSVC 19.51 の既存 `ArtifactCoreAnalyze` compile command を `.cpp` source で再現し、OpenCV headers / BMI reference を含む implementation compile に成功した。続いて再生成後の `ArtifactCoreAnalyze` target が完了し、新しい `.cpp.obj` が生成された。
- OpenCV 4.12.0 を使う isolated CMake 4.4.4 / Ninja pilot では、upstream clang++ (`--target=x86_64-pc-windows-msvc`) と clang-cl の両方が `.cpp` implementation / `.ixx` interface の P1689 scan、compile、`OpticalFlowPilot.dll` link に成功した。clang-cl は CMake の `/TP` に未使用警告を出したが完了した。
- OpenUSD を含む full Clang root target は別記 blocker のまま。isolated pilot は root dependency graph の証拠ではなく、この migration の sccache hit/miss は未計測。

### Utils.Tag migration / three-compiler pilot（2026-10-22）

- `ArtifactCore/src/Utils/Tag.cppm` を本文保持で `.cpp` へ移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の MSVC reference-map 分岐を更新した。implementation が直接 import する module は `Utils.String.UniString` 1件。public `Tag.ixx` は `UniString` と `StringLike` を import する。
- MSVC 19.51 の再生成後 `ArtifactCore` target は完了し、`ArtifactCore.dir/src/Utils/Tag.cpp.obj` と `ArtifactCore.lib` が生成された。
- CMake 4.4.4 / Ninja の isolated upstream clang++ MSVC ABI と clang-cl pilots で、実 `Tag.cpp` / `Tag.ixx` の P1689 scan・compile・`TagPilot.dll` link が成功した。`UniString` と `StringLike` は stub BMI / 最小 implementation で補った。clang-cl は既知の `/TP` unused warning を表示したが build は成功。
- この pilot の sccache hit/miss は未計測。OpenUSD root blocker は独立して継続する。

### Utils.NameGenerator migration / compiler and cache pilot（2026-10-22）

- `ArtifactCore/src/Utils/NameGenerator.cppm` を本文保持で `.cpp` へ移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の MSVC reference-map 分岐を更新した。implementation は `Core.ArtifactString` と `Container.NameMap` を import し、MSVC reference map は `Container.Debug` / `Container.NamedVector` も含む。`Container.NameMap.ifc` の既存 `OBJECT_DEPENDS` を維持した。
- MSVC 19.51 / CMake 4.4.4 / Ninja で CMake を再生成した。`ArtifactCore` の広域 target build は依存再構築 5,054 action に拡大したため中断。対象 object の直接指定でも CMake の order-only target dependency により外部依存 build が必要になったが、Windows SDK `fxc.exe` を一時 PATH に加えた実行で `NameGenerator.ixx`、`Container.NameMap`、`Core.ArtifactString` と `NameGenerator.cpp.obj` の compile を確認した。`ArtifactCore.lib` / 全 target link は今回未確認。
- CMake 4.4.4 / Ninja の isolated pilot では実 `NameGenerator.cpp` / `NameGenerator.ixx` を使用し、依存 `Core.ArtifactString` と `Container.NameMap` のみ pilot stub で補った。upstream clang++ (`--target=x86_64-pc-windows-msvc`) と clang-cl native scan rule の両方で P1689 scan、interface / implementation compile、`NameGeneratorPilot.dll` link が成功した。clang-cl は既知の `/TP` unused warning を出したが build は成功。
- sccache 0.18.0 の clean build 2回では、upstream clang++ が累計 8 requests / 4 misses / 4 hits / 0 bypass / 0 failure。clang-cl は累計 8 requests のうち `.cpp` compile が 1 miss → 1 hit、module compile 6件は `multiple input files` で non-cacheable、0 failure。全 build で DLL link 成功。
- ソース bytes は改行を LF に正規化した比較で旧 `.cppm` と一致し、実 `.cpp` は LF-only。OpenUSD root blocker は別途継続する。

### Utils.Path migration / real Qt module dependency and cache pilot（2026-10-22）

- `ArtifactCore/src/Utils/Path.cppm` を本文保持で `.cpp` へ移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の MSVC reference-map 分岐を更新した。実装の PImpl constructor / destructor を維持し、public `Path.ixx` とその `Utils.String.Like` import は変更していない。
- MSVC 19.51 / CMake 4.4.4 / Ninja で root build tree を再生成し、実 `Path.ixx` / `StringLike.ixx` interface object と `Path.cpp.obj` の scan / compile を確認した。全 `ArtifactCore.lib` link は未確認。
- CMake 4.4.4 / Ninja の upstream clang++ + Windows MSVC ABI と clang-cl では、実 `Path.cpp`、実 `Path.ixx`、実 `StringLike.ixx`、Qt6 Core を組み合わせた P1689 scan・compile・`PathPilot.dll` link が成功した。両 lane で Qt の `QStringLiteral` が `qMakeStringPrivate` の TU-local entity exposure warning を出したが、build は成功。
- sccache 0.18.0 の2 clean build は upstream clang++ が6 requests / 3 misses / 3 hits / 0 bypass / 0 failure。clang-cl は6 requests中 `.cpp` が1 miss→1 hit、interface compile 4件が `multiple input files` で non-cacheable、0 failure。全 build で DLL link 成功。
- 実 `.cpp` は LF-only。ソース module declaration と公開 interface は維持し、OpenUSD full-root blocker は独立して継続する。

### AnimationDynamics migration / real Core dependency and cache pilot（2026-10-22）

- `ArtifactCore/src/Animation/AnimationDynamics.cppm` を本文保持で `.cpp` に移し、source manifest、module reference map、MSVC 用 source reference-map 分岐を更新した。実装の直接 import は `Core.ArtifactString` 1件。pilot は実 `Core.ArtifactString` → `Core.ArtifactArray` → `Core.ArtifactOptional` module graph を含め、依存 stub を使っていない。
- Clang pilot が `AnimationDynamics.ixx` 内の `export namespace ArtifactCore { ... }` にある内側の `export` 2箇所を拒否したため、namespace export block の内側にある関数宣言から冗長な `export` を削除した。関数は引き続き export namespace 内にある。修正後、upstream clang++ + MSVC ABI と clang-cl native scan rule の双方で実 module dependency の P1689 scan、compile、`AnimationDynamicsPilot.dll` link が成功した。
- MSVC 19.51 / CMake 4.4.4 / Ninja で root CMake を再生成し、`AnimationDynamics.ixx` scan / compile と `AnimationDynamics.cpp.obj` compile が実 Core graph で成功した。`ArtifactCore.lib` 全体の再リンクは未確認。
- sccache 0.18.0 の2 clean build は upstream clang++ が10 requests / 5 misses / 5 hits / 0 bypass / 0 failure。clang-cl は10 requests中 implementation `.cpp` が1 miss→1 hit、8 module compile calls は `multiple input files` で non-cacheable、0 failure。すべての Clang build で DLL link 成功。

### Clang migration exception: FileInfo module partition

- `src/File/FileInfo.cppm` は `module File:Info;` で partition implementation を宣言し、`include/File/FileInfo.ixx` は `export module File:Info;` を宣言する。現在の root CMake はこのソースを Clang scan から除外しており、MSVC 用の専用 `ArtifactCoreFile` reference と object ordering を持つ。
- CMake 4.4.4 / upstream clang++ isolated pilot で `.cpp` partition implementation を P1689 scan に含めると、`cmake_ninja_dyndep` は implementation object が `File:Info` を提供するが `CXX_MODULES` file set に見つからないと停止した。scan を無効にし、interface BMI を明示参照しても `FileInfo` 宣言が implementation unit から見えず compile できなかった。
- **扱い:** 今回は `FileInfo.cppm` を変更しない。partition interface / implementation の Clang attachment と CMake provider model を整理するまで、`.cppm` に残す。`Image/OpenEXR.cppm` も同じ root scan 除外規則に含まれるため、別途同型確認が必要。

### Platform.Power migration / Windows API and sccache pilot（2026-10-22）

- `ArtifactCore/src/Platform/Power.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の専用 Core source registration を更新した。module declaration `Platform.Power`、`Power.ixx`、Windows API calls、`winmm` link requirement は維持した。
- MSVC 19.51 / Visual Studio 18 generator で実 interface と `.cpp` を module scan・compileし、`PlatformPowerPilot.dll` を link した。
- CMake 4.4.4 / Ninja の upstream clang++ 22.1.3 + Windows MSVC ABI と clang-cl 22.1.3 でも、実 interface / implementation の P1689 scan、compile、`winmm` を含む DLL link が成功した。clang-cl は CMake の `/TP` unused warning を出したが完了した。
- sccache 0.18.0 の clean build 2回では、upstream clang++ は4 requests / 2 misses / 2 hits / 0 bypass / 0 failure。clang-cl は4 requests中 `.cpp` が1 miss→1 hit、module interface compile 2件は `multiple input files` で non-cacheable、0 failure。すべてのbuildで DLL link 成功。
- この移行では `.cpp` に `-x c++` を付けずとも CMake / clang++ が implementation として扱うことを確認した。ArtifactCore 全体の archive link と runtime 呼び出しは未確認。

### Platform.ShellUtils migration / Qt Core+Gui dependency pilot（2026-10-22）

- `ArtifactCore/src/Platform/ShellUtils.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の Platform source registration を更新した。public `ShellUtils.ixx` と `Platform.ShellUtils` module declaration は維持した。
- MSVC 19.51 / Visual Studio 18 generator で `Platform.Power` と `Platform.ShellUtils` の実 interfaces / implementations をscan・compileし、Qt6 Core/Gui、`winmm`、`shell32` を含む `PlatformUtilitiesPilot.dll` を link した。
- CMake 4.4.4 / Ninja の upstream clang++ 22.1.3 + Windows MSVC ABI と clang-cl 22.1.3 でも、同じ実 source pair の P1689 scan、compile、DLL link が成功した。clang-cl は既知の `/TP` unused warning を表示したが完了した。
- sccache 0.18.0 の2 clean buildでは upstream clang++ が8 requests / 4 misses / 4 hits / 0 bypass / 0 failure。clang-cl は8 requests中、2 implementation `.cpp` が各1 miss→1 hit、4 module interface calls は `multiple input files` によりnon-cacheable、0 failure。両方とも2回目にDLL link成功。
- 既存関数をそのまま保持した拡張子移行であり、ArtifactCore全体のPlatform archive linkと実行時のshell actionは未確認。

### FloatRGBA conversion implementation migration / real Color module graph（2026-10-22）

- `ArtifactCore/src/Color/FloatRGBA.cppm` の `operator FloatColor()` 実装を `.cpp` に移し、source manifest、module reference map（`FloatRGBA` provider と `Color.Float` dependency）、MSVC source reference branch を更新した。`FloatRGBA.ixx` とその `Color.Float` import は維持した。
- 実 dependency graphを保つ Clang pilot は、実 `FloatRGBA.cpp` / `FloatRGBA.ixx` / `FloatColor.ixx` / `ColorTransferFunction.ixx` と、`FloatColor.cppm` の本文をbyte-equivalentで `.cpp` に置いた検証用 implementation copy を使った。CMake 4.4.4 が pilot 内 `.cppm` をmodule interfaceとしてコンパイルし、`module Color.Float;` を拒否したためであり、repository側の `FloatColor.cppm` は変更していない。3 module interface、2 implementationsのP1689 scan / compile、DLL linkが両Clang frontendで成功した。
- `ColorTransferFunction.ixx` の `export namespace ArtifactCore` 内にあった冗長な `export` を1箇所削除した。namespace内宣言のexport状態は維持した。修正後は実interfacesを含む Clang scan / compile / link が成功した。
- MSVC 19.51 / Visual Studio 18 generatorでは、CMake 4.4.4 pilot source 内に実 `FloatColor.cppm` の byte-equivalent `.cpp` copy を置き、実 FloatRGBA implementationと Color module graphのscan / compile / `FloatRGBAPilot.dll` linkが成功した。
- sccache 0.18.0 clean build 2回で upstream clang++ は10 requests / 5 misses / 5 hits / 0 bypass / 0 failure。clang-cl は10 requests中、`.cpp` 2件が各1 miss→1 hit、interface compile 6件は `multiple input files` でnon-cacheable、0 failure。各2回目もDLL link成功。
- 2026-10-22 に root Clang側の実 `.cpp` を使う object-only sccache pilotを再実行。4 cacheable compile requestsが初回4 misses、clean後4 hitsとなり、追加miss 0・cache error 0。両build成功。
- ArtifactCore全体のColor target archive linkとruntime conversion呼び出しは未確認。

### ColorLuminance implementation migration / three-compiler cache pilot（2026-10-22）

- `ArtifactCore/src/Color/ColorLuminance.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` のsource / MSVC reference registrationを更新した。`Color.Luminance` interfaceと関数本体は変更していない。
- 実 `ColorLuminance.ixx` / `.cpp` の依存scan、compile、DLL linkがMSVC 19.51、upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3で成功した。
- sccache 0.18.0の2 clean buildでは、upstream clang++が4 requests / 2 misses / 2 hits / 0 bypass / 0 failure。clang-clは4 requests中 `.cpp` が1 miss→1 hit、module interface compile 2件が `multiple input files` でnon-cacheable、0 failure。両laneとも各2回目のDLL linkが成功した。
- ArtifactCore全体のarchive linkと実consumer runtime呼び出しは未確認。

### Graphics.GPUCapabilities migration / implementation-only API（2026-10-22）

- `ArtifactCore/src/Graphics/GPUCapabilities.cppm` を本文保持で `.cpp` へ移行した。`normalize()`、`nextTimelineToken()`、`indirectCount()`、`hasUsableMemoryBudget()` は計算処理のみで、GPU APIやQtへの直接依存を持たない。公開 `GPUCapabilities.ixx` と `Graphics.GPUCapabilities` module name は変更していない。
- `ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の登録、および `ArtifactCore/CMakeLists.txt` のMSVC GPU implementation IFC指定2か所を `.cppm` / `.cpp` 両対応にした。ArtifactCore の `.gitattributes` で `CMakeLists.txt` / `*.cmake` のLFを固定し、移行元CRLFを新 `.cpp` とともにLFへ揃えた。正規化後の実装内容は旧 `.cppm` とbyte一致した。
- CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + `x86_64-pc-windows-msvc` で実ファイルのP1689 scan、interface / implementation compile、DLL linkに成功。さらにroot ArtifactCore graphでも844件のmodule scan完了後、`GPUCapabilities.ixx` と `GPUCapabilities.cpp` の両objectが生成され、再実行は `no work to do` となった。
- sccache 0.18.0 を使った独立pilotのclean build 2回で、対象compileは2 missesの後に2 hitsとなり、両回DLL link成功。統計差分で計測し、既存cache全体の統計はリセットしていない。
- MSVC 19.51 / Visual Studio 18 generatorでも実ファイルのinterface / implementation compile、IFC参照、DLL linkが成功した。clang-cl はこの環境のCMake 4.3.1 scanner ruleが `-MF` をclang-clへ誤渡しするため、この移行でのscanner / compile / linkは未確認。既存のCMake 4.4.x clang-cl pilot結果と混同しない。
- root Clang scan前に、vcpkg `Taskflow::Taskflow` が `/wd4324` を無条件伝播していたことを確認した。`ArtifactCore/CMakeLists.txt` でupstream Clang GNU frontendに限ってこのMSVC専用optionを除外し、root scanを通した。ArtifactCore全体のarchive linkとruntime呼び出しは未確認。
### Frame.SkipTracker migration / Qt Core implementation（2026-10-22）

- `ArtifactCore/src/Frame/FrameSkipTracker.cppm` を本文保持で `.cpp` へ移行し、source manifest、module reference map、MSVC primary IFC reference 分岐を更新した。旧実装と新 `.cpp` はLF正規化後にbyte一致した。
- clang++ が `FrameSkipTracker.ixx` の `export namespace` 内にある3つの冗長な `export` を拒否したため、型・enumの公開範囲を保って内側の指定だけを削除した。root CMake 4.3.1 / Ninja / clang++ 22.1.3 + MSVC ABIで、interfaceと新implementationのP1689 scan・compileが成功した。
- Qt6 Coreを含む隔離DLLがupstream clang++とMSVC 19.51の双方でリンク成功。sccache 0.18.0はunique probe keyで2 clean buildを実施し、初回2 misses、2回目2 hits、両方link成功。失敗カウンタはこの2回で増えていない。
- clang-clのこのsource pairは未検証。現行CMake 4.3.1ではclang-cl P1689 scanner ruleが `/MF` を正しく扱わず、CMake compiler sanity testで停止する。ArtifactCore全体のarchive link/runtime呼び出しは未確認。
### Frame.Time migration / RationalTime module dependency（2026-10-22）

- `ArtifactCore/src/Frame/FrameTime.cppm` を本文保持で `.cpp` に移し、source manifest、module reference map（`Frame.Time` provider と `Time.Rational` dependency）、MSVC source reference-map 分岐を更新した。LF 正規化後の本文は旧 `.cppm` と一致し、`FrameTime.ixx` と module declaration は維持した。
- root CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABI で実際の `FrameTime.cpp` P1689 scan と compile が成功し、依存 `RationalTime.ixx` と `FrameTime.ixx` も生成・compileされた。再度の対象 build は no-op。
- Qt6 Core と実 `Frame.Time` / `Time.Rational` interfaces を使う object-only isolated pilot は成功。sccache 0.18.0 clean build 2回で対象3 compile unitsは初回3 misses、2回目3 hits、追加miss 0、各build成功。
- MSVC 19.51 の isolated DLL pilot では `FrameTime.cpp` 自体と module interface compile は成功したが、既存 `RationalTime.cppm` を standalone target へ追加しても interface attachment を再現できず依存実装 compile が失敗した。したがって、このpilotでのDLL linkは未確認であり、MSVC root target の完了証拠とは扱わない。clang-cl もこのファイルでは未確認（ローカルCMake 4.3.1のscanner引数制約）。ArtifactCore全体のarchive link/runtime呼び出しは未確認。
### Utils.HashValue migration / Qt Core + MSVC ABI compile and cache pilot（2026-10-22）

- `ArtifactCore/src/Utils/HashValue.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の実装登録を更新した。実装は module import を持たず、Qt Core APIを利用する。public `HashValue.ixx` と `Utils.HashValue` 名は変更していない。
- CMake 4.4.4 / Ninja の upstream clang++ 22.1.3 (`x86_64-pc-windows-msvc`) pilotで、実 interface と `.cpp` の P1689 scan、compile、Qt6 Coreを含む DLL link が成功した。sccache 0.18.0 clean build 2回では2 compile requestsが初回2 misses、2回目2 hits、cache error 0。
- MSVC 19.51 / Visual Studio 18 / CMake 4.4.4 でも、同じ実 interface / `.cpp` / Qt6 Core構成の scan、compile、DLL link が成功した。
- 旧 `.cppm` と新 `.cpp` は改行をLFに正規化した比較で本文一致し、新 `.cpp` はLF-only。root ArtifactCore全体のarchive link/runtime呼び出し、clang-cl laneは未確認。

### Utils.Id migration / Qt + Boost UUID compiler pilot（2026-10-22）

- `ArtifactCore/src/Utils/Id.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` のprovider登録を更新した。`Utils.Id` は interface-only import を持ち、public `Id.ixx` / module名は維持。MSVC per-source IFC overrideは既存CMakeに存在しないことを確認した。
- CMake 4.4.4 / Ninja の upstream clang++ 22.1.3 + Windows MSVC ABI pilotで、実 `Id.ixx` / `Id.cpp` のP1689 scan、compile、Qt6 Core + Boost.UUIDを含むDLL linkが成功した。wobjectdefs.h include pathはArtifactWidgets既存配置を参照した。sccache 0.18.0 clean build 2回でimplementation compileは1 miss→1 hit、cache errors 0。
- MSVC 19.51 / Visual Studio 18 / CMake 4.4.4 でも、同じ実 source pairとQt/Boost headersでDLL link成功。
- 旧 `.cppm` と `.cpp` はLF正規化後に本文一致し、新 `.cpp` をLF-onlyへ統一した。root ArtifactCore全体のarchive link/runtime UUID操作、clang-cl laneは未確認。


### Utils.MultipleTag migration / root Clang + dual compiler pilot（2026-10-22）

- ArtifactCore/src/Utils/MulitpleTag.cppm を本文保持で .cpp に移し、ArtifactCoreSources.cmake と ArtifactCoreModuleReferences.cmake のprovider登録を更新した。直接のmodule importはなく、Qt CoreをGMFで利用する。Utils.MultipleTag と MultipleTag.ixx は維持した。
- root CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABI で、実 .cpp と MultipleTag.ixx のscan・compileが成功した。root dyndep generation時に同時更新済みの HashValue.cpp / Id.cpp もscanを通過した。
- CMake 4.4.4 isolated pilotsでは、実 source pairを使ったupstream clang++ + Qt6 Core と MSVC 19.51 のDLL linkが成功。sccache 0.18.0 clean build 2回で2 compile requestsが初回2 misses、2回目2 hits、cache errors 0。
- 旧 .cppm と新 .cpp はLF正規化後に本文一致し、.cpp はLF-only。ArtifactCore全体のarchive link/runtimeタグ操作、clang-cl laneは未確認。


### Utils.Fingerprint migration / Qt Core and root Clang validation（2026-10-22）

- ArtifactCore/src/Utils/AssetFingerprint.cppm を本文保持で .cpp に移し、ArtifactCoreSources.cmake と ArtifactCoreModuleReferences.cmake のprovider登録を更新した。直接のmodule importはなく、Qt Coreの QFile / QCryptographicHash APIを使う。公開 Utils.Fingerprint interfaceとmodule名は維持。
- root CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABI で実 .cpp / interfaceのP1689 scan・compileが成功し、対象 implementation objectが生成された。
- CMake 4.4.4 isolated pilotではupstream clang++とMSVC 19.51の双方でQt6 Coreを含むDLL linkに成功。sccache 0.18.0 clean build 2回で2 compile requestsが初回2 misses、2回目2 hits、cache errors 0。
- 旧 .cppm と新 .cpp はLF正規化後に本文一致し、新 .cpp をLF-onlyに統一した。ArtifactCore全体archive link、実ファイルhash runtime、clang-cl laneは未確認。


### Utils.ExplorerUtils migration / imported module and three compiler lanes（2026-10-22）

- ArtifactCore/src/Utils/ExplorerUtils.cppm を本文保持で .cpp に移し、ArtifactCoreSources.cmake、ArtifactCoreModuleReferences.cmake のprovider/imported-reference、ArtifactCore/CMakeLists.txt のMSVC per-source reference mappingと imported-module reference map の Utils.String.UniString dependencyを更新した。依存 Utils.String.UniString と公開API/module名は維持した。ExplorerUtils.ixx の export namespace 内冗長な export 指定を削除し、clang++ が拒否する既存構文を公開性を変えず修正した。
- CMake 4.4.4 / Ninja の upstream clang++ + Windows MSVC ABI、clang-cl、MSVC 19.51 の各pilotで実 ExplorerUtils.cpp / public interfacesをscan・compileし、Qt Coreと UniString の実 interface dependencyを使ったDLL linkが成功した。UniString implementationはpilot内に実 cppm の本文コピーを .cpp として配置し、既存 repository sourceは変更していない。MSVCでは既存 StringConvertor.ixx 由来 warning C5202 が1件。
- root CMake 4.3.1 / Ninja / upstream clang++ では ExplorerUtils.cpp / .ixx scanとcompileが成功した。
- upstream clang++ / sccache clean build 5 compile unitsは5 miss→5 hit。clang-cl clean buildでは各回3 non-cacheable calls、計2 cache hits、cache errors 0。upstream clang++ clean buildは5 misses→5 hits、cache errors 0。両laneで2回DLL link成功。
- CMake 4.3.1のclang-cl scanner limitationは継続する一方、CMake 4.4.4 native clang-cl scan ruleでは本source pairのP1689 scan/compile/linkが通った。ArtifactCore全体のarchive link/runtime Explorer起動は未確認。


### Color.XYZ migration / two imported modules and three compiler lanes（2026-10-22）

- ArtifactCore/src/Color/XYZColor.cppm を本文保持で .cpp に移し、ArtifactCoreSources.cmake、ArtifactCoreModuleReferences.cmake のprovider / Color.Float / Utils.String.UniString imported registrations、および ArtifactCore/CMakeLists.txt のMSVC reference branchを更新した。public XYZColor.ixx と Color.XYZ APIは維持。
- root CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABIで実 XYZColor.cpp / interfaceのP1689 scanとcompileが成功。
- CMake 4.4.4 isolated pilotは実 XYZColor implementationと Color.Float / UniString interfacesを用い、依存implementation本文のcopyを検証用 .cpp に配置。upstream clang++、clang-cl、MSVC 19.51の各laneでDLL link成功。MSVC側で既存 StringConvertor.ixx由来warningが1件、エラー0。clang-clは既知の /TP unused warningを表示。
- upstream clang++ sccache clean buildsでは8 compile unitsが8 misses→8 hits、cache error 0。旧 .cppm と新 .cpp はLF正規化後に本文一致し、.cppはLF-only。ArtifactCore全体archive link / runtime conversionは未確認。

### Math.Rotation implementation extension migration（2026-10-22）

- `ArtifactCore/src/Math/Rotation.cppm` は `module Math.Rotation;` で始まる実装単位で、公開宣言は `include/Math/Rotation.ixx` にある。`export module` を持たず、`.cpp` 移行の対象条件に合う。
- 実装内容・module declaration・公開 interface は変えず `Rotation.cpp` へ移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の source path を更新した。汎用 CMake source-property mapping が `CORE_IMPL` を元に適用されるため、source-specific な `CMakeLists.txt` 登録は追加していない。
- CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 (`x86_64-pc-windows-msvc`) で再生成後、`Rotation.ixx` の interface object / `Math.Rotation.pcm` と `Rotation.cpp.obj` を生成した。dyndep に implementation object → `Math.Rotation.pcm` の依存が記録された。
- 対象を clean して2回ビルドし、sccache 統計の差分は4 compile requests / 4 hits / 0 misses。既存 cache error は 1 のまま増えず、non-cacheable calls も増えなかった。
- `ArtifactCore` 集約ターゲットは OpenUSD `arch` の再ビルド中に停止した。`demangle.cpp` で Windows環境に `cxxabi.h` がなく、`attributes.cpp` でも `Arch_ConstructorEntry` 等が未定義となった。変更した Rotation source の失敗ではない。ArtifactCore archive link は未確認で、OpenUSD submodule は変更していない。
- clang-cl 22.1.3 / CMake 4.4.4 の root configure は完了したが、当初は OpenUSD `arch` の PCH が `/wddeprecated` 等の clang-cl 非対応 warning option で失敗した。後述の root compatibility helper を追加し、Rotation の isolated pilot で clang-cl compile を確認した。OpenUSD submodule は変更していない。
- このファイルでの MSVC lane とリンクも未確認。旧 `.cppm` と新 `.cpp` はLF正規化後に内容一致し、新 `.cpp` はLF-only。

### OpenUSD clang-cl warning option compatibility（2026-10-22）

- OpenUSD の `CXXHelpers.cmake` は compiler ID が Clang でも `MSVC=TRUE` を見て `/wddeprecated` などを生成する。clang-cl は warning 番号形式の `/wdNNNN` と異なり、これらの名前指定を受理せず、`arch` の PCH compile 前に停止していた。OpenUSD source/submodule は変更していない。
- ルート CMake に OpenUSD の configure 中だけ使う `cmake/openusd-clangcl-compat/CXXHelpers.cmake` を追加し、clang-cl frontend に限って `_disable_warning()` が `-Wno-*` を生成するようにした。その他のコンパイラの warning flag 選択と OpenUSD の MSVC ABI / PCH / link 分岐は維持する。
- 最初の再検証時は build directory 名が `ClangCL-CMake444` でも、実際に使える PATH の CMake が 4.2.2、Visual Studio 同梱版が 4.3.1 で、4.4.4 executable がなかった。4.2.2 は compiler feature configure 時に clang-cl module dependency scanner 未対応で停止し、OpenUSD helper を読み込む段階へ到達しなかった。その後、公式 CMake 4.4.4 ZIP を検証して再試行した。
- CMake 4.4.4 公式 Windows ZIP を一時ディレクトリへ展開し、公式 SHA-256 と一致することを確認して再構成した。OpenUSD `arch` の生成 flags が3つの `/wd<名前>` から `-Wno-deprecated` 等へ変わり、`pch.cpp.obj` は clang-cl でコンパイル成功した。
- Clang frontend の OpenUSD defaults が Windows SDK の `min` / `max` macroを抑止しないため、root 側の `arch` target に `NOMINMAX` を設定した。これにより `systemInfo.cpp` / `timing.cpp` / `stackTrace.cpp` の min/max macro衝突は解消した。
- 実 `Rotation.ixx` を `CXX_MODULES` file set、実 `Rotation.cpp` を実装 source として参照する isolated Ninja project を `out/pilots/clangcl-rotation` に構成した。CMake 4.4.4 / clang-cl 22.1.3 で両ファイルの P1689 scan、interface object / module IFC 生成、実装 object compile、static archive 作成が成功した。`-TP` unused warning のみ。
- root `arch` target 全体は引き続き失敗する。残件は OpenUSD の `demangle.cpp` が MSVC ABI環境にない `cxxabi.h` を要求すること、`attributes.cpp` が Windows 向け型を Clang compiler macro分岐で定義しないこと、`errno.cpp` が ClangをMSVC ABIとして扱わず `strerror_r` を要求すること。これらは OpenUSD source 内の compiler/OS 判定であり、同 submodule を変更せず root 側で回避できる範囲を確認中。

### Math.Interpolate implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Math/Interpolate.cppm` は import のない primary-interface attachment (`module Math.Interpolate;`) で、本文は空 namespace のみ。`.cpp` へ移し、source manifest と module provider reference を更新した。LF 正規化後、旧実装 blob と新 `.cpp` は byte 一致し、末尾 newline を含む LF-only を確認した。
- 実 interface `Geometry/Interpolate.ixx` の `export namespace ArtifactCore` 内に、さらに9個の redundant `export` 指定があり Clang が拒否していた。内側の指定を除去した。外側の export namespace により公開範囲は維持される。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 (`x86_64-pc-windows-msvc`) で root configure と実 `Interpolate.cpp` object target が成功した。1,722 actions の module graph を通過し、`CXX.dd` に implementation object → `Math.Interpolate.pcm` の依存が生成された。
- 同じ実 `.ixx` / `.cpp` の isolated CMake 4.4.4 Ninja static archive が、clang-cl 22.1.3、upstream clang++ 22.1.3 + MSVC ABI、MSVC 19.51 の全 lane で成功した。MSVC lane は SDK `INCLUDE` / `LIB` を明示した。
- LF 版の upstream clang++ pilot を clean 再ビルドした際、sccache 統計は2 compile requests / 2 cache hits、追加 miss 0、cache errors 0 だった。これは同一 pilot の反復ビルドで module interface と implementation の両方が cache hit した結果。

### Math.ArtifactMinMax implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Math/ArtifactMinMax.cppm` は標準ヘッダーのみを global module fragment で include し、primary-interface attachment `module Math.ArtifactMinMax;` を持つ実装単位だった。実装本文を `.cpp` に移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の source/provider 登録を更新した。公開 interface と module name は維持した。
- 実 `ArtifactMinMax.ixx` / `.cpp` を使う isolated CMake 4.4.4 + Ninja pilot が upstream clang++ 22.1.3 + `x86_64-pc-windows-msvc`、clang-cl 22.1.3、MSVC 19.51 の各 lane で P1689 scan、compile、static archive 作成まで成功した。clang-cl では `-TP` unused warning が出たが処理は成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 でも configure と `ArtifactMinMax.cpp` object target が成功し、`CXX.dd` は implementation object → `Math.ArtifactMinMax.pcm` の依存を記録した。
- root clang++ build の対象を clean して再ビルドした sccache 統計差分は2 requests / 2 hits / 0 misses / 0 errors。統計は共有 cache 全体の累積値から対象 clean build 前後を比較した。
- ArtifactCore 全体 archive link と runtime は未確認。clang-cl の OpenUSD `arch` は既知の upstream `cxxabi.h` / compiler-macro / MSVC ABI 障害で未解決のまま。

### Text.Style implementation migration / upstream clang++（2026-10-22）

- `ArtifactCore/src/Text/TextStyle.cppm` は `<utility>` のみを global module fragment で include し、`module Text.Style;` を宣言する実装単位だった。本文を `.cpp` に移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の明示登録を更新した。module name と public interface は維持した。
- upstream clang++ 22.1.3 / CMake 4.4.4 / Ninja の実 `ArtifactCoreText` split target で interface の P1689 scan・IFC/object compile と、実装 object compile が成功した。`CXX.dd` は TextStyle implementation object が `Text.Style.pcm` を使い、interface が `Utils.String.UniString`、`Utils.Optional`、`FloatRGBA` を使う依存を記録した。対象 pair の clean 後に root build で再ビルドし、sccache 統計差分は11 requests / 11 hits / 0 misses / 0 errors（module graph に伴う他の compile requests を含む）。
- 同じ interface を standalone project で試すと、その project に Qt include path と imported module がないため scan が `QString` 不在で停止した。これは独立プロジェクトの依存不足。root clang-cl target は指定 object に到達する前に OpenUSD `arch` の `errno.cpp` (`strerror_r` / MSVC ABI)、`demangle.cpp` (`cxxabi.h`)、`attributes.cpp` (Windows constructor attribute) で停止したため、この移行の clang-cl レーンは未検証。MSVC lane も未検証。
- Clang が拒否した既存 `TextStyle.ixx` の `export namespace ArtifactCore` 内にある7個の重複 `export` 指定を削除した。外側の export namespace は維持し、module の公開範囲を保った。`.cpp` はLFで末尾改行あり。

### Media.TimeStamp implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Video/MediaTimeStamp.cppm` は import のない `module Media.TimeStamp;` 実装単位だった。`.cpp` に移し、source manifest、provider mapping、`ArtifactCore/CMakeLists.txt` の Media split target 登録、および MSVC reference IFC 出力先を選ぶ拡張子条件を更新した。
- 実 `MediaTimeStamp.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja pilot は upstream clang++ 22.1.3 (`x86_64-pc-windows-msvc`)、clang-cl 22.1.3、MSVC 19.51 の全 lane で P1689 scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 `ArtifactCoreMedia` split target で `MediaTimeStamp.cpp` object が成功した。関連 CMake 条件も新 `.cpp` パスで評価されることを確認した。
- `.cpp` の既存本文を保持し、LF で末尾改行を付けた。ArtifactCore 全体 archive link と runtime は未確認。

### MonochromeCS implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/ImageProcessing/DirectCompute/MonochromeCS.cppm` は標準ヘッダーのみで `module MonochromeCS;` を実装する単位だった。`.cpp` に移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の登録を更新した。追加の ArtifactCore/CMakeLists.txt 例外登録はなかった。
- 実 `MonochromeCS.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の各 lane で scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の ArtifactCore object target でも `MonochromeCS.cpp` を compile できた。
- 旧実装本文を保持し、新 `.cpp` は LF・末尾改行に統一した。ArtifactCore 全体 archive link と runtime は未確認。

### AddBlockNoise implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/ImageProcessing/OpenCV/AddBlockNoise.cppm` は依存 import のない小さな `AddBlockNoise` 実装だった。`.cpp` に移し、source manifest と module provider mapping を更新した。
- 実 `AddBlockNoise.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 でも実 ArtifactCore object target の `AddBlockNoise.cpp` を compile できた。
- 旧実装本文を保持し、新 `.cpp` は LF・末尾改行に統一した。ArtifactCore 全体 archive link と runtime は未確認。

### Platform.Windows.Process implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Platform/ProcessHelperWin.cppm` は import のない `module Platform.Windows.Process;` 実装単位だった。`.cpp` に移し、source manifest、module provider mapping、Platform split target source list を更新した。
- 実 `ProcessHelperWin.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の各 lane で scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 `ArtifactCorePlatform` object target で `.cpp` implementation object が成功した。
- module 名と公開宣言を保ち、新 `.cpp` は LF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Codec.AbstractEncoder implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Video/AbstractEncoder.cppm` を `.cpp` に移し、source manifest、module provider mapping、`ArtifactCore/CMakeLists.txt` の Video split target source list と IFC `/reference` の source path を更新した。
- 実 interface / implementation の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で compile / static archive 作成に成功した。MSVC pilot は Qt の要求に合わせ `/Zc:__cplusplus` と VS/Windows SDK の `INCLUDE` / `LIB` / `PATH` を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の `ArtifactCoreVideo` target で新 `AbstractEncoder.cpp` object が成功し、Video split IFC 参照が維持された。
- module 名と public API は変更せず、新 `.cpp` を LF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### ImageProcessing.AutoGamma.CV implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/ImageProcessing/OpenCV/AutoGammaCV.cppm` は import なしで実装 module に attachment し、実装本体は空 namespace だった。`.cpp` に移し、source manifest と module provider mapping を更新した。public interface は OpenCV header のみを直接 include し、外部 module import はない。
- 実 `AutoGammaCV.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 で scan・compile・static archive 作成に成功した。OpenCV include directory を明示した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target でも `AutoGammaCV.cpp` を compile できた。
- 同じ候補探索で `FileInfo` partition (`File:Info`) の移行を試したが、CMake dyndep が `.cpp` source を `CXX_MODULES` file set 外の module provider として拒否したため、移行・登録変更を取り消して元の `.cppm` に戻した。partition implementation の scanner / provider 分類を整理できるまで対象外とする。
- `.cpp` はLF・末尾改行。ArtifactCore 全体 archive link と runtime は未確認。

### Script.Intrinsics implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Script/Engine/Func/ExprIntrinsics.cppm` は import のない実装単位だった。`.cpp` に移し、source manifest と module provider mapping を更新した。CMake の個別 source 条件や additional reference mapping はない。
- 実 `ExprIntrinsics.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target で `.cpp` が成功し、root compiler command は既定の sccache launcher を通った。
- module name / public declarations を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Render.Worker implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Render/RenderWorker.cppm` は `QRunnable` 実装を含み、外部 module import のない実装単位だった。`.cpp` に移し、source manifest と module provider mapping を更新した。
- 実 `RenderWorker.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core include path と `QT_CORE_LIB` を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の ArtifactCore object target で `.cpp` compile に成功し、コンパイル command は sccache launcher 経由だった。対象 object を削除して再ビルドした統計差分は1 request / 1 hit / 0 misses / 0 cache errors。
- public module name と API を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Graphics.ImageCache implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Graphics/GPUTextureCacheManager.cppm` は Qt Core (`QList` / `QUuid`) を使う実装単位だった。`.cpp` に移し、source manifest、provider mapping、`ArtifactCore/CMakeLists.txt` の module IFC reference condition を更新した。
- 実 `GPUTextureCacheManager.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core headers / `QT_CORE_LIB` を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の ArtifactCore object target で `.cpp` compile に成功した。対象 object を削除して再ビルドした sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- module name / public API は維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Render.Statics implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Render/RenderStatics.cppm` は Qt `QElapsedTimer` include を持つ `module Render.Statics;` 実装だった。`.cpp` に移し、source manifest、provider mapping、`ArtifactCore/CMakeLists.txt` の `Render.Statics` / `std` IFC reference 条件を新 path に更新した。
- 実 interface / implementation の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core include / define を指定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 でも ArtifactCore object target が成功した。対象 object の再ビルドで sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- public module name と declarations を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Blend2D_CS implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/ImageProcessing/DirectCompute/Blend2D_CS.cppm` は OpenCV `cv::Mat` を使う実装で、追加の root CMake source exception はなかった。`.cpp` に移し、source manifest と module provider mapping を更新した。
- 実 `Blend2D_CS.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。OpenCV include directory を明示した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target でも compile 成功。対象 object を削除して再ビルドした sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- module name と public declarations を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### CompressQuality implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Image/CompressQuality.cppm` は Qt `QString` を使う実装 attachment だった。`.cpp` に移し、source manifest、provider mapping、`ArtifactCore/CMakeLists.txt` の `CompressQuality` IFC 参照条件を更新した。
- 実 `CompressQuality.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core include / define と interface の相対 DllExport header include root を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の ArtifactCore object target で `.cpp` compile に成功した。対象 object の再ビルドで sccache 差分は1 request / 1 hit / 0 misses / 0 errors。
- module name / public API は維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Image.CvMatPaintEngine implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Image/CvMatPaintEngine.cppm` は Qt Gui paint API を使う実装 attachment だった。`.cpp` に移し、source manifest と module provider mapping を更新した。root CMake に追加の per-source exception はない。
- 実 `CvMatPaintEngine.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core/Gui include paths と defines を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target でも compile 成功。対象 object の再ビルドで sccache 差分は1 request / 1 hit / 0 misses / 0 errors。
- module name / public API は維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Graphics.GPU.Info implementation migration（2026-10-22）

- `ArtifactCore/src/GPU/GPUInfo.cppm` は実質空の module implementation attachment で、旧 `vendor()` / `renderer()` 実装はコメントアウトされていた。`.cpp` に移し、source manifest、module provider mapping、`ArtifactCore/CMakeLists.txt` の自己 IFC reference 条件を更新した。
- CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABI の実 ArtifactCore object target で P1689 scan と `.cpp` compile が成功し、`Graphics.GPU.Info` IFC 依存も `CXX.dd` に記録された。
- 対象 object のみ削除して root target を再ビルドした sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- clang-cl / MSVC の isolated lane、ArtifactCore 全体 archive link、runtime は未確認。新 `.cpp` はLF・末尾改行にした。

### Math.RotationTurns implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Math/RotationTurns.cppm` は module import のない実装単位だった。`.cpp` に移し、source manifest と module provider mapping を更新した。追加の `ArtifactCore/CMakeLists.txt` source exception はなかった。
- 実 `RotationTurns.ixx` / `.cpp` の isolated CMake 4.4.4 project は upstream clang++ 22.1.3 + Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51 / Visual Studio generator の全 lane で P1689 scan または MSVC module scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target でも compile 成功。対象 object の再ビルドで sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- public module name / API を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Windows Clang / OpenUSD `arch` compatibility follow-up（2026-10-22）

- `ArtifactCore` target を upstream clang++ MSVC ABI で起動したところ、ArtifactCore のコンパイルへ進む前に OpenUSD `arch` の `attributes.cpp` が Windows 用 `Arch_ConstructorEntry` / `Arch_ConstructorInit` 宣言を認識せず停止した。
- 親 `CMakeLists.txt` は OpenUSD ソースを編集せず、ビルドディレクトリに `attributes.h` の互換コピーを生成する。Windows Clang では compiler attributes の Clang 分岐を保ちつつ、Windows constructor 定義が Windows 分岐へ到達するよう constructor 条件だけを狭める。
- `arch` を再ビルドし、`attributes.cpp` と `stackTrace.cpp` の compile が進むことを確認した。現在の次の blocker は `demangle.cpp` の MSVC ABI 環境にない `cxxabi.h` と、`errno.cpp` が Windows Clang を MSVC strerror path と認識しない `strerror_r` 呼び出し。OpenUSD submodule は clean で未変更。全 ArtifactCore build / link は未完了。

### ArtifactCore upstream clang++ full-target investigation（2026-10-22）

- 上記 OpenUSD `arch` の互換対応を通過し、upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja で `ArtifactCore` の P1689 scan と多数の module compile に進んだ。sccache 経由で実行されている。
- 5回の段階的な build pass で、`PyroSimulation.cppm`、`Diagnostics/Trace.ixx`、stb 実装、複数の GPU/image/physics/script implementation を含む多くの object compile が成功した。ArtifactCore の compile 全体、archive link、runtime は未完了。
- Clang が顕在化させた直接 include / import 不足を局所修正した。例: Diligent `PipelineState.h`、`Frame.Debug`、`Container.Debug`、`Math.Interpolate`、`Render.ImageBuffer`、`Image.ExportOptions`、`Image.ImageF32x4RGBAWithCache`、Qt `QString`、標準 `<span>` / `<vector>` / `<map>`。
- 継続 blocker は、`Utils.String.Like` / `Utils.String.View` / `Utils.Path` の重複 `toQString` 宣言、`ScriptContext` の同一型前方宣言競合、`LayerMatte` / `FallbackPolicy` の二重 export、Mesh の nested default initializer、`NetworkRPCServer` module provider 不在、`Audio.Modulation.Router` / `Event.Bus` の provider 順序または CMake target dependency、及び `ContainerName` を使う複数 module の明示 import 漏れである。これらは `.cppm`→`.cpp` 対象単体の失敗ではなく、ArtifactCore全体を upstream clang++ で通すための module 境界・登録課題として分離する。
- sccache の再ヒット統計は今回の全体 target pass では集計していない。既に記録済みの単体 object hit 実績と、全体 target の成功は区別する。次は module provider の未登録・順序を確認し、API重複や unrelated interface 修正を広げずに blocker を小さく分割する。

### ArtifactCore upstream clang++ target build pass（2026-10-22）

- CMake 4.4.4 / Ninja / upstream clang++ 22.1.3（Windows MSVC ABI）/ sccache の構成で `ArtifactCore` target の全 object compile と `ArtifactCore.lib` archive link が完了した。
- 依存循環を避けるため、ArtifactCore が直接使う `Event.Bus`、`Audio.Modulation.Modulator` / `Router`、`NetworkRPCServer` の provider を ArtifactCore target に統合し、分割 target 側から外した。
- `Math.Random` を `float2` / `float3` の唯一の型所有 module とし、`Particle` は再 export する形に整理した。`ParticleSystem.cppm` に `QVector3D` の直接 include を加えた。
- これで前項の blocker として記録した module 重複、provider順序、`ContainerName`、Router の名前衝突を越えて ArtifactCore target が完了した。アプリ全体の link、runtime、clang-cl / MSVC の root project ビルドは未確認。
- `sccache` は build tree 内の絶対パスで CMake launcher に設定されている。現在の PowerShell PATH からは `sccache` コマンド名を解決できず、今回の全体ビルドで cache hit 数は未集計。以前の個別 object hit 実績と区別する。
### LLVM 23.1.4 Windows upstream clang++ / Artifact target follow-up（2026-10-22）

- LLVM 23.1.4 公式 Windows x64 配布の SHA-256 を release metadata と照合し、独立 Ninja build tree を作成した。最初は J: の空き容量不足で PCM 出力に失敗したため、build tree を空き容量のある V: に移して CMake を再構成した。
- Artifact target の最初の pass は `Artifact/src/Widgets/ArtifactColorPaletteWidget.cppm` の `#ifndef Q_MOC_RUN` が閉じられず、`module Artifact.Widgets.ColorPaletteWidget;` が条件コンパイル下に入る Clang scanner error で停止した。標準ヘッダーの skip guard を module declaration 前で閉じる最小修正を加え、Artifact と親の `main` に push 済み。
- `ArtifactColorPaletteWidget.cppm` の guard 修正後、LLVM 23.1.4 で Artifact の依存 scan と多数の implementation compile が進んだ。`ArtifactCompositionPlaybackController` / `ArtifactAudioScrubController` / `ArtifactPlaybackEngine` / `ArtifactPlaybackService` / `ArtifactProjectManager` の Verdigris 型登録重複、`UndoManager.ixx` の `SharedPtr` import 漏れ、`VSTEffect.ixx` の `Core.ArtifactString` import 漏れ、`GauusianBlur.cppm` の Qt 型 include 漏れを修正した。
- Artifact full target は未完了。Clang 23.1.4 が `ArtifactPreviewCompositionPipeline.cppm` で 1,260 files / 約964.86 MB、`ArtifactLayerFactory.cppm` で1,160 files / 約976.44 MB、`ArtifactProject.cppm` で1,490 files / 約966.30 MBを読み込んだ後、`ran out of source locations` で停止した。Preview の video frame access を独立 interface に分離する試行でも、次の `Artifact.Layer.Solid2D` import で同じ上限に達したため、その試行コードは残していない。Artifact 全体 link と runtime は未確認。
- full target の compile command は WinGet 配布の `sccache` launcher 経由だった。Artifact 全体の cache 統計は未集計。別途、isolated smoke の cache hit 統計を以下に記録する。
### LLVM 23.1.4 isolated module / sccache smoke（2026-10-22）

- `tools/clang-msvc-abi-smoke` を CMake 4.2.2 / Ninja / upstream clang++ 23.1.4 / `x86_64-pc-windows-msvc` で configure し、P1689 dependency scan、`.ixx` module interface、module importer/implementation、通常 `.cpp` translation units の compile と両 executable の link に成功した。
- sccache 0.18.0 は WinGet install path から指定した。clean build 2回後の統計は15 compile requests、5 cache hits、10 misses、0 errors。通常 TU と module-related commandsを合わせた部分 hit が確認できた。これは smoke project の結果であり、Artifact 全体 build の cache rate ではない。
- Artifact full target は Clang 23.1.4 の source-location 上限で停止したため、Artifact 全体 link / runtime と全体 sccache 統計は未確認。

### Artifact implementation `.cpp` attachment pilot（2026-10-22）

- `Artifact/src/Composition/ArtifactCompositionSetting.cppm` は `module Artifact.Composition.Setting;` のみを持つ import-free implementation unit だったため、同じ module name の `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の `ARTIFACT_APP_IMPL_SOURCES` 登録を更新した。Artifact 側 `.gitattributes` に `.cpp` / `.cmake` の `eol=lf` を明示追加した。公開 `ArtifactCompositionSetting.ixx` と module API は維持した。`.cpp` と変更した manifest は LF。
- upstream clang++ 23.1.4 の CMake 4.4.4 / Ninja 1.13.2 target で実ソースを P1689 scan し、`Artifact.Composition.Setting` provider attachment として認識することを確認。Artifact の既存 build tree にある同 module の実 BMI を指定した CMake 生成 compile command で object を生成した。sccache 0.18.0 で同じ command を2回実行し、2 compile requests / 2 cache hits / 0 errors。
- clang-cl 22.1.3 と upstream clang++ 23.1.4 の両方で、CMake 4.4.4 の isolated executable に実 `.cpp` を登録し、同名 module の最小 stub interface と importer を用いて P1689 scan、compile、link、実行に成功した。これは拡張子・module attachment・scanner の pilot であり、Artifact の実 `ArtifactCompositionSetting.ixx` 依存を使う clang-cl 統合確認ではない。
- 各 isolated CMake pilot は clean build を2回実行した。clang-cl は 6 compile requests のうち4 cacheable executions がすべて hit、0 miss / 0 cache errors（2 requests は multiple inputs で non-cacheable）。clang++ は6 requests、3 misses / 3 hits、0 cache errors。Artifact 全体の build / link / runtime は引き続き未確認。

### Artifact.Gui.Manager implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/GUI/ArtifactGuiManager.cppm` は import-free implementation unit で、constructor / destructor の defaulted definitions のみを持つ。module name と公開 QObject interface を変えず `.cpp` へ移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation manifest を更新した。
- 実 `ArtifactGuiManager.ixx` / `.cpp` を使う CMake 4.4.4 / Ninja smoke で、upstream clang++ 23.1.4（Windows MSVC ABI）、clang-cl 22.1.3、MSVC 19.51 の各 lane が P1689 または MSVC module scan、interface / implementation / importer compile、static library と executable link に成功し、3 executable の実行も成功した。Qt Core をリンクした。
- upstream clang++ の clean build と clean rebuild は sccache 0.18.0 下で合計6 compile requests、3 misses / 3 hits、0 errors。build tree は隔離 smoke 用で、Artifact 全体の sccache 統計ではない。
- 実 Artifact target の object-only Ninja build は source manifest の再構成後に6,622 actionsへ依存展開したため約180 actionsで中断した。したがって root target への統合 object build / Artifact 全体 link は今回未確認。`.cpp` と manifest は LF、`git diff --check` は成功。

### Core.SystemStats implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/Core/SystemStats.cppm` は Windows / POSIX の system metrics 実装を含む `module Core.SystemStats;` implementation unit で、外部 module import はない。`.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation manifest を更新した。公開 interface と実装本文は維持し、移行後 source は LF。
- 実 `SystemStats.ixx` / `.cpp` と importer を使う CMake 4.4.4 / Ninja smoke で、upstream clang++ 23.1.4（Windows MSVC ABI）、clang-cl 22.1.3、MSVC 19.51 の各 lane が P1689 または MSVC module scan、interface / implementation / importer compile、static library と executable link、executable runtime に成功した。
- upstream clang++ の clean build と clean rebuild は sccache 0.18.0 下で合計6 compile requests、3 misses / 3 hits、0 errors。Artifact 全体の cache 統計ではない。
- `git check-attr eol` は `.cppm` に `eol=lf` を示したが、移行前 worktree は CRLF だった。`.cpp` 化の際に LF へ揃え、Git の正規化後は元実装から100% renameとして判定された。Artifact 全体 target / link は引き続き未確認。

### Artifact.Widgets.LayerLockIndicator implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/Widgets/Timeline/LayerLockIndicator.cppm` は Qt Widgets / Verdigris を使う implementation unit で、module import はない。`W_OBJECT_IMPL`、widget の状態操作と既存 paint implementation を含む本文をそのまま `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation manifest を更新した。公開 `.ixx` は変更していない。
- 実 `LayerLockIndicator.ixx` / `.cpp` と importer の CMake 4.4.4 / Ninja smoke で、upstream clang++ 23.1.4（Windows MSVC ABI）、clang-cl 22.1.3、MSVC 19.51 の全 lane が dependency scan、interface / implementation / importer compile、static library と executable link に成功した。clang-cl は CMake の `-TP` に unused argument 警告を出したが build は成功。
- upstream clang++ + sccache 0.18.0 の初回 build は3 misses、clean rebuild は3 hits、cache errors 0。後続の同 smoke clean build では既存 cache から6 requestsすべて hitした。
- `QApplication` を使う executable runtime は Qt platform initialization 中に停止し、widget 生成前に到達しなかったため未確認。移行した `.cpp` と manifest は LF、rename diff は100%。Artifact 全体 target / link は未確認。

### Artifact.Generator.Abstract implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/Generator/AbstractGenerator.cppm` は module import のない implementation unit で、raw `Impl*` の生成と破棄だけを定義する。本文と `Artifact.Generator.Abstract` module name を維持して `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の explicit implementation manifest を更新した。
- 実 `AbstractGenerator.ixx` / `.cpp` と importer を使う CMake 4.4.4 / Ninja smoke で、upstream clang++ 23.1.4（Windows MSVC ABI）、clang-cl 22.1.3、MSVC 19.51 の各 lane が module dependency scan、interface / implementation / importer compile、static library / executable link、executable runtime に成功した。clang-cl は unused `-TP` warning を出したが build と run は成功。
- upstream clang++ + sccache 0.18.0 の clean build / clean rebuild は合計6 requests、3 misses / 3 hits、cache errors 0。`.cpp` と manifest は LF、rename は100%。Artifact 全体 target / link は未確認。

### ArtifactWidgets.AbstractDialog implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/AbstractDialog.cppm` は `Widgets.Dialog.Abstract` の import-free implementation unit。公開 `AbstractDialog.ixx` と実装の挙動を維持して `.cpp` に移し、Clang のコンパイル境界を明確にするため直接使う Qt 型のヘッダーを global module fragment に追加し、`ArtifactWidgets/CMakeLists.txt` の明示 implementation list を更新した。
- `.cpp` は LF、module declaration 前に必要な Qt ヘッダーを配置し、`git diff --check` は成功。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.GlowFrame implementation migration（2026-10-22）

- `ArtifactWidgets/src/Effect/WidgetGlowFrame.cppm` は `Widgets.Effects.GlowFrame` の import-free implementation unit で、公開 interface を import する利用箇所は2つ。`.cpp` に移し、`ArtifactWidgets/CMakeLists.txt` の明示 implementation list を更新した。描画・focus 表示の実装は変更していない。
- `.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.TimeCodeEditor implementation migration（2026-10-22）

- `ArtifactWidgets/src/Time/TimeCodeEditor.cppm` は `Widgets.TimeCodeEditor` の空の import-free implementation unit だったため、module 宣言を維持して `.cpp` に移し、明示 implementation list を更新した。公開 `TimeCodeEditor.ixx` は変更していない。
- `.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.AppProgress Windows implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/AppProgress_Win.cppm` は `AppProgress` module の import-free Windows implementation unit で、`RenderQueueManagerWidget` から実際に利用されている。module 名と taskbar progress 処理を維持して `.cpp` に移し、明示 implementation list を更新した。
- `.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.DragSpinBox implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/DragSpinBox.cppm` は `DragSpinBox` module の実装で、`Core.ArtifactMath` を import する。Artifact の composition / plane creation dialogs が `DragSpinBox` / `DoubleDragSpinBox` を利用している。module import と操作実装を維持して `.cpp` へ移し、CMake の明示 implementation list を更新した。
- 直接使う Qt の spin box、event、point、widget ヘッダーを global module fragment に置き、`.cpp` は LF に揃えた。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.InterpretFootageDialog implementation migration（2026-10-22）

- `ArtifactWidgets/src/Dialog/InterpretFootageDialog.cppm` は `Widgets.Dialog.InterpretFootage` implementation unit で、`Media.SourceInterpret` を import する。Artifact の Asset Browser がこの dialog を利用している。module import と既存 UI 実装を維持して `.cpp` へ移し、明示 implementation list を更新した。
- 直接使う Qt dialog / widget / string headers と `<memory>` を global module fragment に置き、`.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.EditableLabel implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/EditableLabel.cppm` は `Widgets.EditableLabel` implementation unit で、`Common.ClikableLabel` を import する。Artifact の composition / plane dialogs と render queue panel から利用されている。既存の editing / animation implementation と module import を維持して `.cpp` へ移し、明示 implementation list を更新した。
- 直接使う animation、layout、event、widget、string の Qt headers を global module fragment に追加し、`.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.EnhancedSlider implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/EnhancedSlider.cppm` は `EnhancedSlider` module implementation で、Float Color Picker が horizontal / vertical slider として利用する。`Core.ArtifactMath` import と custom painting / pointer interaction implementation を保ったまま `.cpp` へ移し、CMake の explicit implementation list を更新した。
- 直接使う slider、widget、geometry、color、palette、pen、object の Qt headers を global module fragment に置いた。`.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.ColorWheel implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/ColorWheel.cppm` は `Widgets.ColorWheel` implementation unit で、`Core.ArtifactMath` と `Color.Float` を import する。Float Color Picker の色選択 UI から利用される。import、QImage の既存 wheel cache、色選択・paint implementation を保ったまま `.cpp` へ移し、CMake explicit implementation list を更新した。
- 直接使う color、paint event、pen、point、rect、size の Qt headers を global module fragment に置き、既存 UTF-8 BOM を保持して `.cpp` を LF に揃えた。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.KeyboardOverlayDialog implementation migration（2026-10-22）

- `ArtifactWidgets/src/Dialog/KeyboardOverlayDialog.cppm` は `Widgets.KeyboardOverlayDialog` implementation unit で、`Input.Operator` を import する。Artifact Help menu から起動され、key binding を検索・表示する。既存 UI / filtering / shortcut lookup と import を維持して `.cpp` に移し、CMake explicit implementation list を更新した。
- 直接使う abstract item view、GUI application、QObject、QString、QWidget のヘッダーを global module fragment に追加した。source は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.VectorScopeWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/VectorScopeWidget.cppm` は `VectorScopeWidget` implementation で、`Core.ArtifactMath` と `AbstractWidget` を import する。Artifact の Color Science panel と Composition Editor から利用される。module imports と既存 vectorscope 実装を維持して `.cpp` に移し、CMake の通常 implementation list に追加した。legacy scope list からは旧 `.cppm` entry を外し、同じ実装の重複登録を避けた。
- 直接使う paint/resize event、pen、point、QString、QRgb と math headers を global module fragment に置いた。source は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.WaveformScopeWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/WaveformScopeWidget.cppm` は `WaveformScopeWidget` implementation unit で、`Core.ArtifactMath` を import する。Artifact の Color Science panel と Composition Editor から利用される。既存 QtConcurrent / QFutureWatcher の実装と module import を維持して `.cpp` へ移し、CMake の通常 implementation list に追加、専用 legacy list から旧 path を除いた。
- 直接使う image pixel、color、font、paint/resize event、pen、rect、QString、QObject の headers を global module fragment に追加し、`.cpp` を LF に揃えた。async completion の動作は変更していない。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.HistgramWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/HistgramWidget.cppm` は `HistgramWidget` module implementation unit で、Artifact の Color Science panel と Composition Editor から HistogramWidget として利用される。module 名・公開 interface・histogram / paint implementation を維持して `.cpp` に移し、CMake の通常 implementation list に追加した。これで専用 legacy scope implementation list は不要になったため削除した。
- 直接使う QRgb、widget、color、font、pen、rect、QString headers を global module fragment に置き、`.cpp` を LF に揃えた。描画・集計動作は変更していない。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.ParadeScopeWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/ParadeScopeWidget.cppm` は `ParadeScopeWidget` implementation unit で、Color Science panel、Composition Editor、Contents Viewer から利用される。module 名・公開 interface・非同期 scope 集計・描画処理を保って `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う Qt の QObject / QRect / QString / Qt、color / font / paint event / pen / resize event / QRgb headers を global module fragment に追加し、LF 改行に揃えた。動作経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.FloatColorPicker implementation migration（2026-10-22）

- `ArtifactWidgets/src/Dialog/FloatColorPicker.cppm` は `FloatColorPickerDialog` module implementation unit で、Color Palette、Property Editor、Composition Editor、複数の作成／設定 dialog から import される。module 名・公開 interface・color selection UI と既存処理を維持し、implementation source を `.cpp` に移して CMake の明示リストに登録した。
- 直接使う QChar / QString / QOverload 定義 / QFont / QPalette / QAbstractSpinBox / QWidget headers を global module fragment に追加し、LF 改行へ揃えた。既存 UI 接続と signal 経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.AbstractWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/AbstractWidget.cppm` は `AbstractWidget` module implementation unit で、scope widgets、video preview、image viewer の interface から参照される。`Widgets.Effects.GlowFrame` import と implementation body を保って `.cpp` へ移し、CMake の明示 implementation list に登録した。
- `QFocusEvent` / `QResizeEvent` / `QWidget` の直接 headers を global module fragment に加え、LF 改行へ揃えた。公開 interface・挙動は変更せず、ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.RenderQueueManagerWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Render/RenderQueueManagerWidget.cppm` は `Widgets.Render.Queue` implementation unit で、Artifact `AppMain` が public module を import している。4 つの `W_OBJECT_IMPL`、既存 module imports と implementation body を維持して `.cpp` へ移し、CMake の明示 implementation list に登録した。
- source が直接使う Qt model / view / image / object / string / geometry / style-option headers を global module fragment に追加し、LF 改行に揃えた。UI 接続や描画・queue 処理は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.SyntaxHighlighter implementation migration（2026-10-22）

- `ArtifactWidgets/src/Code/SyntaxHighlighter.cppm` は `Code.SyntaxHighlighter` implementation unit で、`CodeEditor.cppm` が module を import する。module name・public interface・既存 highlighter 実装を維持して `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う QColor / QFont / QObject / QString headers を global module fragment に追加し、LF 改行へ揃えた。動作は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.CodeEditor implementation migration（2026-10-22）

- `ArtifactWidgets/src/Code/CodeEditor.cppm` は `CodeEditor` implementation unit で、`Code.SyntaxHighlighter` を import する。module import、`W_OBJECT_IMPL`、既存 editor / line-number rendering implementation を維持して `.cpp` へ移し、CMake の明示 implementation list に登録した。
- 直接使う QChar / QColor / QFont / QFrame / QPaintEvent / QPalette / QRect / QSize / QString / Qt headers を global module fragment に追加し、LF 改行に揃えた。既存 signal connections を含め動作は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.FloatColorPickerButton implementation migration（2026-10-22）

- `ArtifactWidgets/src/Button/FloatColorPickerButton.cppm` は `FloatColorPickerButton` implementation unit で、`Color.Float` と `FloatColorPickerDialog` を import する。module imports、`W_OBJECT_IMPL`、picker 起動と色の反映処理を維持して `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う QColor / QDialog / QPalette / QWidget headers を global module fragment に追加し、LF 改行に揃えた。既存 click connection と signal 経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.AudioBusWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Audio/AudioBusWidget.cppm` は `AudioBusWidget` implementation unit で、`Audio.Mixer` / `Audio.Bus` / `Core.ArtifactString` を import する。これらの import、`W_OBJECT_IMPL`、既存 mixer / bus UI 処理を維持して `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う QPaintEvent / QString / Qt / QWidget / `std::shared_ptr` の headers を global module fragment に加え、LF 改行に揃えた。既存 UI connections と signal 経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.BasicImageViewWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Image/BasicImageViewWidget.cppm` は `BasicImageViewWidget` implementation unit で、`Widgets.Effects.GlowFrame` を import する。module import、`W_OBJECT_IMPL`、既存 image view / file-drop loading implementation を維持して `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う QDebug / QFocusEvent / QGraphicsPixmapItem / QImage / QObject / QPainter / QPixmap / QPoint / QRectF / QSize / QString / QUrl / QWheelEvent / QWidget / Qt headers を global module fragment に加え、LF 改行へ揃えた。既存 image loading connection と描画動作は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.WinJumpList implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/WinJumpList.cppm` は Windows Shell COM を使う import-free `WinJumpList` implementation unit で、対応する public `WinJumpList.ixx` と module 名が一致する。実装と公開 API を維持して `.cpp` に移し、明示 implementation source list に登録した。
- 既存 Windows 専用 include と Jump List 処理は変更していない。`.cpp` と `.cppm` の LF 属性を `ArtifactWidgets/.gitattributes` に明記した。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.WinTaskbarProgress implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/WinTaskbarProgress.cppm` は Windows Shell Taskbar COM の import-free implementation unit で、対応する public `WinTaskbarProgress.ixx` と module 名が一致する。既存の progress state 実装を保って `.cpp` へ移し、明示 implementation source list に登録した。
- 現行 Artifact / ArtifactWidgets source には呼び出し元が見つからなかった。public interface と実装は削除せず、直接使う Windows / QWidget headers を維持する。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.CommandAction implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/CommandAction.cppm` は `CommandAction` の import-free implementation unit で、`include/Common/CommandAction.ixx` が同名 module interface を宣言する。`W_OBJECT_IMPL`、PImpl ownership、既存 QAction connection、method bodies を変更せず `.cpp` に移し、明示 implementation source list に登録した。
- implementation は既存どおり interface module から型宣言を得る。`.cpp` は LF。ビルド／依存スキャン／リンクは実行していない。別途確認候補の既存 `hasCommand()` 実装上の懸念は `Insight.md` に未検証として記録した。

### ArtifactWidgets.ConsoleWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Console/ConsoleWidget.cppm` は `ConsoleWidget` module の implementation unit で、`include/Console/ConsoleWidget.ixx` が対応する public interface を宣言する。既存の `W_OBJECT_IMPL`、Impl ownership、console append / clear 処理を保って `.cpp` に移し、明示 implementation source list に登録した。
- Qt headers は既存どおり global module fragment にあり、既存 context-menu / action connections を含め動作変更はない。source は LF。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.KnobSlider implementation migration（2026-10-22）

- `ArtifactWidgets/src/Knob/KnobSlider.cppm` は `Widget.Knob.Slider` module implementation unit で、`include/Knob/KnobSlider.ixx` が同 module の public interface を宣言する。`W_OBJECT_IMPL` と既存 event / paint method bodies を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- interface include への依存を避けるため、source が署名に使う `QPaintEvent` / `QPoint` / `QWheelEvent` headers を global module fragment に追加した。deprecated API と実装動作は変更していない。`impl_` 初期化に関する別件の未検証懸念は `Insight.md` に記録した。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.RotaryKnob implementation migration（2026-10-22）

- `ArtifactWidgets/src/Knob/KnobEditorWidget/RotaryKnob.cppm` は `Widgets.Knob.Rotary` module implementation unit で、`include/Knob/RotaryKnob.ixx` が対応 interface を宣言する。`W_OBJECT_IMPL` と Impl の constructor / destructor、widget constructor / destructor を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- deprecated API と公開 interface は変更していない。source は LF。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.AbstractKnobEditor implementation migration（2026-10-22）

- `ArtifactWidgets/src/Knob/AbstractKnobEditor.cppm` は `Widgets.Knob` implementation unit で、`include/Knob/AbstractKnobEditor.ixx` が同 module の public interface を宣言する。`W_OBJECT_IMPL`、Impl ownership、drag/drop acceptance と event methods を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- source が直接使う `QWidget` を global module fragment に追加した。deprecated API と処理は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.VolumeSlider implementation migration（2026-10-22）

- `ArtifactWidgets/src/Knob/VolumeSlider.cppm` は import-free `VolumeSlider` implementation unit で、`include/Audio/VolumeSlider.ixx` が対応する public interface を宣言する。既存 `W_OBJECT_IMPL` と class / method definitions を維持して `.cpp` に移し、明示 implementation source list に登録した。
- source が直接使う `QWidget` header を global module fragment に追加した。repository source 内での利用元は見つからなかった。既存の空 method bodies や public API は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.HeadPanel implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/HeadPanel.cppm` は `HeadPanel` module implementation unit で、`include/Dock/HeadPanel.ixx` が `HeaderPanel` public interface を宣言する。`W_OBJECT_IMPL`、layout / child widget construction、既存 context-menu style collapse connection を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- implementation に `Widgets.Utils.CSS` の symbol use がないことを確認し、不要な module import と `using namespace ArtifactCore` を除いて Clang module dependency を減らした。直接使う `QString` header を global module fragment に加えた。UI behavior と public interface は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.RayTracerWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Render/RayTracerWidget.cppm` は `ArtifactWidgets.Render.RayTracerWidget` implementation unit で、`include/Render/RayTracerWidget.ixx` が対応 interface を宣言する。`Render` module import、既存 ray tracing / image / paint / key handling implementation を保ったまま `.cpp` へ移し、明示 implementation source list に登録した。
- source が直接使う `QString` / `QColor` / `QRect` headers を global module fragment に明示した。描画・計算経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.PreviewControlWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Preview/PreviewControlWidget.cppm` は `Preview.ControlWidget` implementation unit で、`include/Preview/PreviewControlWidget.ixx` が対応する public interface を宣言する。`W_OBJECT_IMPL`、Impl ownership、既存 method definitions を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- source の Qt dependencies は既に global module fragment に直接 include されている。現行 repository source 内に利用元は見つからなかった。空の既存 method bodies と public interface は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.ArtifactBasicVideoPreviewWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Video/ArtifactBasicVideoPreviewWidget.cppm` は `Widgets.VideoPreview` implementation unit で、`include/Video/ArtifactBasicVideoPreviewWidget.ixx` が同名 public interface を宣言する。FFmpeg decoder / thumbnail / video-frame module imports と既存 implementation を保って `.cpp` へ移し、明示 implementation source list に登録した。
- source が直接使う `QAudioFormat` / `QByteArray` / `QDragEnterEvent` / `QObject` / `QPixmap` / `QSize` / `QString` headers を global module fragment に追加した。video decode / preview behavior は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.ModelViewer implementation migration（2026-10-22）

- `ArtifactWidgets/src/Viewer/ModelViewer.cppm` は `Widgets.ModelViewer` implementation unit で、`include/3D/ModelViewer.ixx` が同名 public interface を宣言する。既存 `ModelViewer::Impl` definition と constructor / destructor definitions を保持して `.cpp` に移し、明示 implementation source list に登録した。
- source 内で使用されていなかった `QList` include を取り除き、不要な Qt header dependency を減らした。stub の挙動や public interface は拡張していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.GraphicsFloatImageItem implementation migration（2026-10-22）

- `ArtifactWidgets/src/Graphics/GraphicsItemFloatImageItem.cppm` は `GraphicsFloatImageItem` module implementation unit として分類されているため `.cpp` に移し、明示 implementation source list に登録した。使用されていなかった `QList` include を削除した。
- 既存 source は public `GraphicsFloatImageItem` method definitions を含まず、private class stub だけを持つ。この API / implementation mismatch は変更・解決していないため `Insight.md` に記録した。ビルド／依存スキャン／リンクは実行していない。

### ArtifactCore.Rotate implementation migration（2026-10-22）

- `ArtifactCore/src/Transform/Rotate.cppm` は `module Rotate;` implementation unit で、`include/Transform/Rotate.ixx` の非 inline method definitions を持つ。本文を `.cpp` に移し、明示 manifest `cmake/ArtifactCoreSources.cmake` の source path を更新した。
- source は追加 module import / source-specific CMake reference / scan-disable rule を持たないため、既存 scanner classification を維持する。public interface と method behavior は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactCore.AssetConverter implementation migration（2026-10-22）

- `ArtifactCore/src/Asset/AssetConverter.cppm` は `AssetConverter` module の image conversion implementation unit で、`include/Asset/AssetConverter.ixx` の宣言を定義する。本文を `.cpp` に移し、`ArtifactCoreSources.cmake` / `ArtifactCoreModuleReferences.cmake` / Asset domain source list の自己 path を更新した。`AssetImporter` consumer mapping は public interface path を参照するため変更していない。
- source が直接使う `<memory>` / `<utility>` / `QByteArray` / `QSize` / `QString` headers を global module fragment に明示した。変換処理と public API は変更していない。ビルド／依存スキャン／リンクは実行していない。

- `ArtifactCore/src/Asset/AssetMetaFile.cppm` は `Asset` module の実装ユニットで、公開 `AssetMetaFile.ixx` の定義を含む。module 名、定義本文と API を保って `.cpp` に移し、source manifest、module reference map、Asset domain source list の自己 path を更新した。`AssetType` 依存と `AssetImporter` consumer は変更していない。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Asset/AssetDatabase.cppm` は `Asset.Database` module の実装ユニットで、path normalization、asset identity 操作、JSON import/export の定義を持つ。module 名・`AssetType` import・実装本文を維持して `.cpp` に移し、明示 source manifest、module reference map、Asset domain source list の自己 path を更新した。MSVC の Asset domain reference 設定と importer consumer は変更していない。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Asset/ImageAsset.cppm` は `ImageAsset` module の短い実装ユニットで、`ImageAsset.ixx` の定義を持つ。module 名、`IO.ImageImporter` / `Image.Raw` / `Utils.String.UniString` imports と本文を保って `.cpp` に移し、明示 source manifest、module reference map、Asset domain source list の自己 path を更新した。Tracked source の宣言行を再集計し、表の Core / Widgets 件数を現在値へ合わせた。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Time/RealTime.cppm` は `Time.Real` module の PImpl 実装ユニットで、公開 interface の全 method 定義を持つ。module 名、状態更新ロジックと PImpl ownership を保って `.cpp` に移し、明示 source manifest と module reference map の自己 path を更新した。外部 module import はない。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Time/TimeCode.cppm` を `Time.Code` module implementation unit のまま `.cpp` に移した。module 名、`Time.Rational` import と既存実装を維持し、Core の明示 source manifest、module reference map、MSVC 用 source-specific `/reference` 条件の path を更新した。実装が使う `std::max` / `int64_t` に `<algorithm>` / `<cstdint>` を直接追加した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Time/TimeCodeRange.cppm` を `TimeCodeRange` implementation unit のまま `.cpp` に移した。module 名と `Time.Rational` import、既存実装を維持し、明示 source manifest、module reference map、MSVC 用 source-specific `/reference` 条件の path を更新した。`int64_t` 用の `<cstdint>` を global module fragment に追加した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Frame/FrameOffset.cppm` を `Frame.Offset` implementation unit のまま `.cpp` に移した。module 宣言、既存 imports と arithmetic / time conversion 実装を保ち、Core source manifest、module reference map の provider・全 imported reference rows、および MSVC source-specific `/reference` 条件を更新した。既存の直接標準 header include は維持。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Media/MediaFrame.cppm` を `Media.MediaFrame` implementation unit のまま `.cpp` に移した。module 名、既存 PImpl と constructor / destructor definitions を保ち、明示 source manifest、module reference map、Media source list の path を更新した。`std::make_unique` の直接依存として `<memory>` を追加した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Graphics/Effect/CreativeEffect.cppm` を `Graphics.Effect.Creative` implementation unit のまま `.cpp` に移した。実装本文と標準 header 群を維持し、明示 source manifest と module reference map の provider path を更新した。CMake は Graphics Effect 群の共通 reference 分岐を使い、個別 source path 条件はないことを確認した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Image/PNGImage.cppm` を `Image.Png` implementation unit のまま `.cpp` に移した。module 宣言、PNG header parsing の本文と直接 header 群を保ち、source manifest、module reference map、MSVC 用 source-specific `Image.Png` reference 条件の path を更新した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Mask/DepthMask.cppm` を `Core.Mask.DepthMask` implementation unit のまま `.cpp` に移した。alpha 計算の本文を維持し、manifest・module reference map・Core source list の path を更新した。実装で使う `Q_UNUSED` / `QVector4D` に対応し `QtGlobal` / `QVector4D` を直接 include した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Image/ImageF32x4.cppm` を `ImageF32x4` implementation unit のまま `.cpp` に移した。OpenCV `cv::Mat` PImpl、constructor / destructor、dimension accessor を維持し、明示 source manifest・module reference map・MSVC 用 `/reference` と既存 `OBJECT_DEPENDS` 条件の path を更新した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Transform/StaticTransform3D.cppm` を `Transform._3D` implementation unit のまま `.cpp` に移した。PImpl・copy/move ownership と method definitions を維持し、明示 source manifest と module reference map の provider path を更新した。source-specific CMake 設定・外部 module import はない。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Common/Point2D.cppm` を `Core.Point2D` implementation unit のまま `.cpp` に移した。Point2DF / Point2DI の PImpl 定義と accessors を維持し、`int32_t` 用 `<cstdint>` を追加。source manifest、module reference map と MSVC primary IFC reference 条件を新 path に更新した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Channel/Channel.cppm` を `Channel` implementation unit のまま `.cpp` に移した。VideoChannel / VideoFrame の実装と既存 module dependencies を維持し、直接 `size_t` を使っていた箇所を `std::size_t` と `<cstddef>` に明示した。明示 source manifest、module reference map の provider / imported rows、CMake の generated-reference source 条件を更新した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/ImageProcessing/AbstractImageEffect.cppm` を `ImageProcessing` implementation unit のまま `.cpp` に移した。public virtual method 群、partition imports の CMake `/reference` list と implementation を維持し、明示 source manifest・module reference map・個別 source 条件の path を更新した。`size_t` を `std::size_t` にし、`<cstddef>` を直接追加。ビルド／依存スキャン／リンクは未実行。

### Geometry.LayerBounds implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/LayerBoundsCalculator.cppm` は `Geometry.LayerBounds` implementation unit で、対応する `LayerBoundsCalculator.ixx` に `calculate` / `calculateAABB` / `contains` の宣言が揃っている。module 宣言と実装本文を維持して `.cpp` に移した。
- Core source manifest、module reference map、MSVC generated-reference source 条件、および LayerBounds / LayerAlignment standalone target の source path を更新した。既存 include 群と geometry behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Graphics.GPUAlgorithms implementation migration（2026-10-22）

- `ArtifactCore/src/Graphics/GPUAlgorithms.cppm` は独立した `Graphics.GPUAlgorithms` implementation unit で、`GPUAlgorithms.ixx` の `prefixSum` / `compaction` / `radixSort` と実装が対応する。module 宣言、関数本体、algorithm / limits / DLL export headers を維持して `.cpp` に移した。
- Core source manifest と module reference map の provider path を更新した。個別 `/reference` 条件や standalone CMake source 参照は存在しないことを確認した。挙動変更はない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Color.Harmonizer implementation migration（2026-10-22）

- `ArtifactCore/src/Color/ColorHarmonizer.cppm` は `Color.Harmonizer` implementation unit で、公開 interface の6つの調和色 API に対応する定義を持つ。module 宣言、Color.Float / Color.Conversion imports、QList / cmath headers と既存の色計算を維持して `.cpp` に移した。
- Core source manifest、provider / imported module reference rows、MSVC source-specific IFC reference 条件、ColorSpaceStandalone の source path、および Qt header audit script の対象 path を更新した。色計算の挙動変更はない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Composition.Registry implementation migration（2026-10-22）

- `ArtifactCore/src/Composition/CompositionRegistry.cppm` は `Composition.Registry` implementation unit で、公開 interface の global / register / unregister / lookup / names / clear API を実装する。module 宣言、NameMap / Container.Debug imports、Qt headers と既存 registry behavior を保って `.cpp` に移した。
- 専用 composition target の source path、Core source manifest と依存スキャンに関する CMake コメントを更新した。実装本文は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Math.Bezier implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/BezierCalculator.cppm` は `Math.Bezier` implementation unit で、evaluateQuadratic / evaluateCubic / evaluatePath / getTangentAngle / evaluateTangent の定義が interface と一致する。module 宣言、Qt headers、標準 headers、実装本文を維持して `.cpp` に移した。
- Core source manifest、provider module reference、MSVC source-specific IFC 条件、BezierPathSamplerStandalone の source path を更新した。幾何計算の挙動変更はない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Color.Swatch implementation migration（2026-10-22）

- `ArtifactCore/src/Color/ColorSwatch.cppm` は `Color.Swatch` implementation unit で、公開 interface の palette access / copy-move / GPL import-export API を実装する。module 宣言、Color.Float / ArtifactString / TrackedPtr imports、既存 implementation と file I/O を保って `.cpp` に移した。
- Core source manifest、provider / imported module reference rows、2つの MSVC source-specific IFC / OBJECT_DEPENDS 条件、ColorSwatchStandalone の source path を更新した。palette behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Frame.Rate implementation migration（2026-10-22）

- `ArtifactCore/src/Frame/FrameRate.cppm` は `Frame.Rate` implementation unit で、FrameRate の construction / copy-move / rational FPS / display / JSON serialization / operators を公開宣言に対応して実装する。module 宣言、Serialization / UniString imports、直接 Qt / standard headers と本文を維持して `.cpp` に移した。
- Core source manifest、provider / imported module reference rows、2つの MSVC IFC / OBJECT_DEPENDS 条件、FrameRateStandalone の source path を更新した。FPS・serialization behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Graphics.Effect.Creative.Manager implementation migration（2026-10-22）

- `ArtifactCore/src/Graphics/Effect/CreativeEffectManager.cppm` は `Graphics.Effect.Creative.Manager` implementation unit で、constructor / destructor、add / remove、effect stack の適用定義を持つ。module 宣言、既存 imports / headers / implementation を維持して `.cpp` に移した。
- Core source manifest と provider module reference の path を更新した。個別 IFC 条件・standalone source path は見つからなかった。render/effect behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Property.LinkManager implementation migration（2026-10-22）

- `ArtifactCore/src/Property/PropertyLinkManager.cppm` は `Property.LinkManager` implementation unit で、instance / addLink / updateTargets / links / clear の公開 API 定義が揃っている。module 宣言、既存実装を維持して `.cpp` に移した。
- Core source manifest、provider map、MSVC provider IFC 条件を更新した。実装が直接使う `QMetaType` を global module fragment に明示 include した。プロパティ更新動作は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Script.AngelScript.Behaviour implementation migration（2026-10-22）

- `ArtifactCore/src/Script/AngelScript/AngelScriptBehaviour.cppm` は `Script.AngelScript.Behaviour` implementation unit で、public interface の load / start / update / destroy API の定義が揃っている。module 宣言、直接 `<string>` include、AngelScript lifecycle 実装を保って `.cpp` に移した。
- Core source manifest、provider module reference と MSVC IFC 条件の source path を更新した。script lifecycle behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Configuration.ConfigSchema implementation migration（2026-10-22）

- `ArtifactCore/src/Configuration/ConfigSchema.cppm` は `Configuration.ConfigSchema` implementation unit で、公開 API の schema registration / validation / lookup / defaults 定義が揃っている。module 宣言、既存 imports / headers、mutex と validation 実装を保って `.cpp` に移した。
- Core source manifest、configuration split target source list、Container debug IFC options、provider / imported module reference rows を更新した。設定検証動作は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Configuration.LayeredConfigStore implementation migration（2026-10-22）

- `ArtifactCore/src/Configuration/LayeredConfigStore.cppm` は `Configuration.LayeredConfigStore` implementation unit で、層の load / save / resolve / import-export / reload API を実装する。module 宣言、既存 imports / Qt headers、FastSettingsStore integration と本文を維持して `.cpp` に移した。
- Core source manifest、configuration split target の implementation list、provider / imported module reference rows を更新した。起動設定仕様と configuration milestone の実装パスも現行名へ直し、両文書の最終更新日を記録した。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Script.AngelScript.Engine implementation migration（2026-10-22）

- `ArtifactCore/src/Script/AngelScript/AngelScriptEngine.cppm` は `Script.AngelScript.Engine` implementation unit で、singleton lifecycle / SDK initialization / module compile / function execution API を実装する。module 宣言、条件付き AngelScript SDK includes と実装本文を保って `.cpp` に移した。
- Core source manifest と provider module reference を更新した。SDK headers は既存どおり global module fragment 内にあり、macro / script behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Core.FastSettingsStore implementation migration（2026-10-22）

- `ArtifactCore/src/Core/FastSettingsStore.cppm` は `Core.FastSettingsStore` implementation unit で、CBOR file load / atomic save / cache / batch API を実装する。module 宣言、Qt / mutex headers と実装本文を維持して `.cpp` に移した。
- Core source manifest、MSVC provider / std IFC source condition、module reference、FastSettingsStoreStandalone の source path を更新した。CBOR persistence behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### UI.Layout.State implementation migration（2026-10-22）

- `ArtifactCore/src/UI/LayoutState.cppm` は `UI.Layout.State` implementation unit で、JSON / QSettings / FastSettingsStore serialization API と schema registration の定義が公開 interface に対応している。module 宣言、imports、headers と実装本文を維持し `.cpp` に移した。
- ArtifactCore source manifest、UI split target source path、MSVC IFC / OBJECT_DEPENDS 条件、provider と Serialization import の module reference rows を更新した。Clang frontend の最終設定では `.cpp` implementation scan と module map に移行する。保存・migration behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### File.TypeDetector implementation migration（2026-10-22）

- `ArtifactCore/src/File/FileTypeDetector.cppm` は `File.TypeDetector` implementation unit で、公開 interface の constructor / destructor / detect API と拡張子・magic 判定の定義が揃っている。直接使う Qt Core headers、module 宣言と実装本文を保って `.cpp` に移した。
- ArtifactCore の File split target source list、明示 source manifest、provider module reference を更新した。Clang lane は `.cpp` を P1689 scan / module map に載せ、MSVC は既存の明示 IFC reference と interface object ordering を使う。file type 判定動作は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Asset.File implementation migration（2026-10-22）

- `ArtifactCore/src/Asset/AbstractAssetFile.cppm` は `Asset.File` implementation unit で、公開 interface の PImpl lifecycle、asset identity / metadata / dependency / load-state API を実装する。`W_OBJECT_IMPL`、既存 module imports、Qt / Verdigris headers と実装本文を維持して `.cpp` に移した。
- Asset split target source list、明示 source manifest、MSVC の NamedVector / Container.Debug IFC options、provider / imported module reference rows を新 path へ更新した。Clang frontend は `.cpp` を module map scan に載せ、既存の MSVC 専用参照／object ordering を除去する。asset behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Coordinate.Profile implementation migration（2026-10-22）

- `ArtifactCore/src/Coordinate/CoordinateProfile.cppm` は `Coordinate.Profile` implementation unit で、value / space / profile / resolver の公開 API と JSON 登録・schema migration を実装する。module 宣言、Qt / standard headers、Serialization imports と実装本文を保って `.cpp` に移した。
- Coordinate split target source list、明示 source manifest、Serialization IFC options、provider / imported module reference rows を新 path に更新した。Clang frontend は `.cpp` を P1689 scan / module map に載せる。座標変換・serialization behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Color.LUTWriter implementation migration（2026-10-22）

- `ArtifactCore/src/Color/LUTWriter.cppm` は `Color.LUTWriter` implementation unit で、Cube / 3dl memory export と file write API の定義が公開 interface に揃っている。`Color.LUT` import、Qt Core headers と出力実装を保って `.cpp` に移した。
- Color source manifest、既存の MSVC module reference source condition、provider / `Color.LUT` imported reference rows を新 path に更新した。Clang frontend は `.cpp` を P1689 scan / module map に載せる。LUT のフォーマット・出力 behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### ArtifactCore stale `.cppm` source conditions repair（2026-10-22）

- CMake source-specific IFC / compile-option branches still compared `_artifact_impl_relative` against `.cppm` for 14 files that are already registered as `.cpp`: AnimationDynamics, ColorLuminance, FloatRGBA, XYZColor, FrameSkipTracker, GPUInfo, GPUTextureCacheManager, CompressQuality, RenderStatics, ExplorerUtils, NameGenerator, Path, Tag, and MediaTimeStamp.
- Each corresponding `.cpp` exists in the source manifest and has a provider row in the module reference map. Updated only the stale CMake conditions so the intended MSVC IFC / ordering settings apply to the actual source path. Clang continues to use its P1689 module map lane.
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.AspectRatio implementation migration（2026-10-22）

- `ArtifactCore/src/Core/AspectRatio.cppm` は `Core.AspectRatio` implementation unit で、公開 interface の constructor / copy-move / ratio / string parse API を実装する。module 宣言、標準 / Qt headers、UniString import と実装本文を保って `.cpp` に移した。
- Core source manifest、既存の MSVC module reference source condition、provider / UniString imported reference rows を新 path に更新した。decimal parsing のロジックと PImpl ownership は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Command.LayerLockManager implementation migration（2026-10-22）

- `ArtifactCore/src/Command/LayerLockManager.cppm` は `Command.LayerLockManager` implementation unit で、constructor / destructor、lock acquire / release、remote state、expiry purge API の定義が揃っている。`Event.Bus` import、`W_OBJECT_IMPL`、既存実装を保って `.cpp` に移した。
- source manifest、Command split target source list、Event.Bus / Memory.SharedPtr の MSVC IFC options、provider module reference を新 path に更新した。実装で直接使う Qt Core の `QList` / `QObject` / `QString` を global module fragment に明示 include した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Command.Lambda implementation migration（2026-10-22）

- `ArtifactCore/src/Command/LambdaCommand.cppm` は `Command.Lambda` implementation unit で、公開 interface の constructor / destructor / undo / redo 定義が揃っている。module 宣言と command behavior を維持して `.cpp` に移した。
- Command split target source list、明示 source manifest、provider module reference を新 path に更新した。未使用の `<QUndoStack>` を実際に使う `<QUndoCommand>` へ置き換え、`std::move` 用の `<utility>` を直接 include した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Asset.Importer implementation migration（2026-10-22）

- `ArtifactCore/src/Asset/AssetImporter.cppm` は `Asset.Importer` implementation unit で、公開 interface の import / supported-extension / type-detection API と asset metadata 登録処理を実装する。module 宣言、imports、Qt headers と importer behavior を維持して `.cpp` に移した。
- Core source manifest、明示 source registration、provider / imported module reference rows を新 path に更新した。Clang lane は `.cpp` implementation scan / P1689 module map を使い、MSVC は既存の explicit IFC references を継続する。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Layer.LayerStrip implementation migration（2026-10-22）

- `ArtifactCore/src/Layer/LayerStrip.cppm` は `Layer.LayerStrip` implementation unit で、public interface の PImpl lifecycle と frame range API の定義が揃っている。module 宣言、`<cstdint>` / `<utility>` headers、実装を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。専用 split target / source-specific CMake 条件に旧 path はなく、追加変更は不要だった。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Script.Builtin.Manager implementation migration（2026-10-22）

- `ArtifactCore/src/Script/Engine/Func/BuiltinManager.cppm` は `Script.Builtin.Manager` implementation unit で、公開 interface の register / remove / call / list API を実装する。module 宣言、既存 headers と関数呼び出し・lookup behavior を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。専用 split target / source-specific CMake 条件に旧 path はなく、Clang の `.cpp` implementation scan / P1689 module map に載る。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Script.Enviroment implementation migration（2026-10-22）

- `ArtifactCore/src/Script/Engine/Enviroment/EnvironmentManager.cppm` は `Script.Enviroment` implementation unit で、公開 interface の constructor / destructor 定義を実装する。module 宣言、`Script.Engine.Value` import と既存 implementation を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference、CMake の MSVC IFC reference / interface object dependency の両 source-specific 条件を新 path に更新した。Clang lane は `.cpp` implementation scan / P1689 module map を使う。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Diagnostics.FallbackPolicy implementation migration（2026-10-22）

- `ArtifactCore/src/Diagnostics/FallbackPolicy.cppm` は `Core.Diagnostics.FallbackPolicy` implementation unit で、fallback tracker の記録・検索・policy API を実装する。module 宣言、`Container.NamedVector` import、既存実装を保って `.cpp` に移した。
- ArtifactCore source manifest と provider / imported module reference、CMake の MSVC IFC option と container registry object dependency の source-specific 条件を新 path に更新した。fallback event behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Diagnostics.DiagnosticEngine implementation migration（2026-10-22）

- `ArtifactCore/src/Diagnostics/DiagnosticEngine.cppm` は `Core.Diagnostics.DiagnosticEngine` implementation unit で、rule registry と validate-all / delta / cache API を実装する。module 宣言と validation behavior を保って `.cpp` に移し、`std::ptrdiff_t` / `QStringList` の直接利用に `<cstddef>` / `<QStringList>` を追加した。
- ArtifactCore source manifest、provider / `Core.Diagnostics.ProjectDiagnostic` reference、MSVC IFC source condition と `DiagnosticEngineStandalone` の source path を更新した。診断キャッシュ・検証動作は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Light.IESProfile implementation migration（2026-10-22）

- `ArtifactCore/src/Light/IESProfile.cppm` は `Light.IESProfile` implementation unit で、IES parse / file load、candela sampling、LUT 構築 API を実装する。module 宣言、imports、既存実装を保って `.cpp` に移した。
- source manifest、provider module reference、`ArtifactCoreLight` split source と MSVC IFC / object dependency の各 path を更新した。parse / sample / LUT behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Preview.Settings implementation migration（2026-10-22）

- `ArtifactCore/src/Preview/PreviewSettings.cppm` は `Preview.Settings` implementation unit で、preview settings の PImpl lifecycle、getter / setter、reset API を定義する。module 宣言と設定値の挙動を保って `.cpp` に移した。
- ArtifactCore source manifest、provider module reference、Preview split target source list の path を更新した。preview 設定 API は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Database implementation migration（2026-10-22）

- `ArtifactCore/src/Database/Database.cppm` は `Core.Database` implementation unit で、Qt SQL connection / prepared query、SQLite transaction、migration runner の API を実装する。module 宣言と database behavior を保って `.cpp` に移し、直接利用する Qt / standard headers を明示した。
- ArtifactCore source manifest、provider / `Core.Database.Types` references、`ArtifactCoreDatabase` target source と MSVC IFC / object dependency 条件を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### ArtifactCore.Plugin.Registry implementation migration（2026-10-22）

- `ArtifactCore/src/Plugin/PluginRegistry.cppm` は `ArtifactCore.Plugin.Registry` implementation unit で、plugin register / activate / lookup / enumeration API を実装する。module 宣言、imports と registry behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、Plugin split target、initial scan-off source list、MSVC の IFC / interface-object dependencies と NamedVector option list の path をすべて更新した。Clang lane は implementation source を再度 scan 対象にし、P1689 依存を生成する。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.AI.TieredAIManager implementation migration（2026-10-22）

- `ArtifactCore/src/AI/TieredAIManager.cppm` は `Core.AI.TieredAIManager` implementation unit で、agent configuration、request routing、cloud escalation、background analysis API を実装する。module 宣言、imports、既存 PImpl と request behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、imported module references、AI split target、initial scan-off / MSVC IFC / interface-object / NamedVector source conditions と AI feature map path を更新した。Clang lane は migrated `.cpp` を implementation source collection から P1689 scan に載せる。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.AI.APIKeyManager implementation migration（2026-10-22）

- `ArtifactCore/src/AI/APIKeyManager.cppm` は `Core.AI.APIKeyManager` implementation unit で、provider key / proxy の設定 load / save / lookup API を実装する。module 宣言、imports と persistence behavior を保って `.cpp` に移した。
- ArtifactCore source manifest と provider / imported module references、AI split target、MSVC explicit-reference exception と configuration interface object dependency の source path を更新した。Clang lane では implementation source collection を通じて P1689 scan が有効になる。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Property.Group implementation migration（2026-10-22）

- `ArtifactCore/src/Property/PropertyGroup.cppm` は `Property.Group` implementation unit で、public interface の PImpl lifecycle、property add / remove / lookup / sort API を実装する。module 宣言と実装を保って `.cpp` に移した。
- ArtifactCore source manifest、provider module reference と MSVC の source-specific IFC reference path を更新した。PImpl ownership、container と PropertyGroup behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Diagnostics.ProjectDiagnostic implementation migration（2026-10-22）

- `ArtifactCore/src/Diagnostics/ProjectDiagnostic.cppm` は `Core.Diagnostics.ProjectDiagnostic` implementation unit で、diagnostic factory と result の集計・検索・merge API を実装する。診断内容・カテゴリ・severity の挙動を保って `.cpp` に移した。
- ArtifactCore source manifest、provider / `Container.Debug.Registry` module references、MSVC の IFC reference / NamedVector interface object dependency と `DiagnosticEngineStandalone` source path を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Time.Rational implementation migration（2026-10-22）

- `ArtifactCore/src/Time/RationalTime.cppm` は `Time.Rational` implementation unit で、有理時間値の算術・比較・scale 変換 API を実装する。既存の overflow fallback と丸め挙動を含め、ソース内容を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Transform._2D implementation migration（2026-10-22）

- `ArtifactCore/src/Transform/StaticTransform2D.cppm` は `Transform._2D` implementation unit で、PImpl の lifecycle、copy / move、transform 値の getter / setter を実装する。公開 interface と所有権挙動を保ったまま `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### asio_async_file_writer implementation migration（2026-10-22）

- `ArtifactCore/src/IO/asio_async_file_writer.cppm` は `asio_async_file_writer` implementation unit で、Boost.Asio thread pool 経由の非同期ファイル書き込み API を実装する。既存の worker / callback / atomic shutdown 挙動を含め、ソース内容を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Position2D implementation migration（2026-10-22）

- `ArtifactCore/src/Core/Position2D.cppm` は `Core.Position2D` implementation unit で、`Position2D` / `Position2DF` の PImpl lifecycle、copy / move と座標 getter / setter を実装する。公開 API と実装内容を保って `.cpp` に移した。
- ArtifactCore source manifest、MSVC の source-specific IFC reference、provider module reference と `Position2DStandalone` source path を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Media.Info implementation migration（2026-10-22）

- `ArtifactCore/src/Media/MediaInfo.cppm` は `Media.Info` implementation unit で、MediaInfo / MediaInfoBuilder の PImpl と metadata API 定義を実装する。公開 API と既存実装をそのまま `.cpp` に移した。
- ArtifactCore source manifest、Media split target source list と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.UniformScale implementation migration（2026-10-22）

- `ArtifactCore/src/Core/UniformScale.cppm` は `Core.UniformScale` implementation unit で、scale 値の PImpl lifecycle と getter を実装する。公開 API と実装内容を保って `.cpp` に移した。
- source manifest、provider module reference、MSVC の source-specific IFC reference と `UniformScaleStandalone` source path を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Diagnostics.CrashHandler implementation migration（2026-10-22）

- `ArtifactCore/src/Diagnostics/CrashHandler.cppm` は `Diagnostics.CrashHandler` implementation unit で、Windows unhandled exception filter、report path / content 生成と pending report ingestion を実装する。Windows API、report format、handler behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、provider module reference と MSVC の source-specific IFC reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Localization implementation migration（2026-10-22）

- `ArtifactCore/src/Localization/Localization.cppm` は `Core.Localization` implementation unit で、locale selection、translation load / fallback、plural formatting と reload API を実装する。既存の言語決定・翻訳・Event.Bus 通知動作を保って `.cpp` に移した。
- ArtifactCore source manifest、Localization split target source list / source property と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### NLE.OTIO implementation migration（2026-10-22）

- `ArtifactCore/src/NLE/OTIO.cppm` は `NLE.OTIO` implementation unit で、OpenTimelineIO JSON の track / clip / transition / marker 読み書きを実装する。schema mapping と既存 import / export behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、NLE split target、MSVC IFC options / interface object dependencies と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### VST3.Interfaces implementation migration（2026-10-22）

- `ArtifactCore/src/VST3/VST3Loader.cppm` は `VST3.Interfaces` implementation unit で、VST3 TUID conversion、dynamic library entry loading と host / editor ABI adapter を実装する。既存 ABI / loader behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、VST3 source list、source compile property、provider map と CMake の実装説明コメントを新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Simulation.OpenVDBVolumeReference implementation migration（2026-10-22）

- `ArtifactCore/src/Simulation/OpenVDBVolumeReference.cppm` は volume metadata inspect、density grid snapshot load と Pyro field copy API を実装する。既存処理を `.cpp` に移し、`std::size_t` 使用に必要な `<cstddef>` を直接 include した。
- ArtifactCore source manifest、split target source list、MSVC の source-specific IFC references と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Thread.Helper implementation migration（2026-10-22）

- `ArtifactCore/src/Thread/ThreadHelper.cppm` は `Thread.Helper` implementation unit で、shared background QThreadPool、thread snapshot / diagnostics と scoped thread naming API を実装する。pool 設定、同期、diagnostic behavior を変更せず `.cpp` に移した。
- ArtifactCore source manifest、Thread split target、MSVC IFC reference / interface object dependency と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### UI.ShortcutBindings implementation migration（2026-10-22）

- `ArtifactCore/src/UI/ShortcutBindings.cppm` は `UI.ShortcutBindings` implementation unit で、shortcut ID serialization、key-event conversion と binding override API を実装する。既存の key mapping / override behavior を保って `.cpp` に移した。
- 実装で直接使う `std::array` / `std::size_t` に `<array>` / `<cstddef>` を追加した。CMake source path、MSVC IFC reference / interface object dependency、manifest と provider map を更新し、この source の `CXX_SCAN_FOR_MODULES OFF` を解除して Clang implementation dependency scan を有効にした。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Asset.Manager implementation migration（2026-10-22）

- `ArtifactCore/src/Asset/AssetManager.cppm` は `Asset.Manager` implementation unit で、source reference count、decoded payload cache、version invalidation と registry snapshot restore を実装する。既存の asset lifecycle behavior を保って `.cpp` に移した。
- Clang の直接 include を明確にするため `<QString>`、`<QUuid>`、`<cstdint>` を追加し、未使用の `QFileSystemWatcher` include を除去した。ArtifactCore source manifest、Asset split target、provider / imported module references と追跡済み Visual Studio backup project の source paths を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Source.ISource implementation migration（2026-10-22）

- `ArtifactCore/src/Source/ISource.cppm` は file / generated source の metadata、read / reload / relink と source capability API を実装する。既存 source cache behavior を保って `.cpp` に移した。
- 直接利用する Qt 型（`QByteArray` / `QDateTime` / `QSize` / `QString` / `qint64`）の include を明示し、ArtifactCore source manifest、provider / `Memory.SharedPtr` / `Utils.Id` references、MSVC IFC references を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Frame.Position implementation migration（2026-10-22）

- `ArtifactCore/src/Frame/FramePosition.cppm` は frame index と rational time 間の値変換を実装する。公開 `Frame.Position` interface と演算を保って `.cpp` に移した。
- source manifest、provider mapping、MSVC source-specific IFC reference 条件、および追跡済み Visual Studio backup project の source paths を更新した。実装本文は改行を LF に揃えた以外は同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Geometry.ResolutionRemap implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/ResolutionRemap.cppm` は解像度変更時の位置・bounds・scale・transform remap と影響情報を計算する。既存挙動を保ち `.cpp` に移した。
- source manifest、provider mapping と MSVC source-specific IFC reference 条件を更新した。Qt / 標準ヘッダーの直接 include は既に揃っており追加変更は不要。実装本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 適用対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### GenerateTestImage implementation migration（2026-10-22）

- `ArtifactCore/src/Generate/GenerateTestImage.cppm` はカラーバー、グラデーション、チェッカーボード等を呼び出し時に RGBA float buffer へ生成する utility である。描画 frame loop からの参照は見つからず、既存挙動を保って `.cpp` に移した。
- ArtifactCore の core / Generate source lists、provider mapping と `Core.Parallel` IFC reference の source path を更新した。実装本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 適用対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Geometry.LayerAlignment implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/LayerAlignment.cppm` はユーザー操作による layer align / distribute と layout collision resolution を実装する。frame loop からの参照は見つからず、既存挙動を保って `.cpp` に移した。
- source manifest、provider mapping と MSVC source-specific IFC reference 条件を新 path に更新した。直接利用する Qt / 標準ヘッダーは既に global module fragment に含まれており追加変更は不要。本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 適用対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Color.OCIOConfig implementation migration（2026-10-22）

- `ArtifactCore/src/Color/OCIOConfig.cppm` は OpenColorIO 設定の file / JSON load、組み込み preset と color space / display metadata を実装する。color conversion の per-pixel 経路ではなく、既存処理を保って `.cpp` に移した。
- source manifest、provider mapping と MSVC source-specific IFC reference 条件を新 path に更新した。Qt / OpenColorIO の直接 include は既に global module fragment に揃っている。本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 適用対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### StarfieldGenerator implementation migration（2026-10-22）

- `ArtifactCore/src/Generate/StarfieldGenerator.cppm` は恒星・星雲・惑星データを RGBA float buffer に生成する公開 utility である。現行 repo 内での呼び出し元は見つからず、既存実装を保って `.cpp` に移した。
- core / Generate source lists、provider mapping と `Core.Parallel` / `Math.Random` の MSVC IFC reference source path を更新した。実装本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 対象になる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Analyze.Histgram implementation migration（2026-10-22）

- `ArtifactCore/src/Analyze/Histgram.cppm` は OpenCV image から channel histogram を計算する utility である。現行 app/widget に呼び出し元は見つからず、描画 frame path 外の一回計算として既存実装を `.cpp` に移した。
- Analyze target / source manifest、provider mapping、Container IFC reference 条件、および追跡済み Visual Studio backup project の paths を更新した。本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Animation.KeyframeEditingTools implementation migration（2026-10-22）

- `ArtifactCore/src/Animation/KeyframeEditingTools.cppm` はキー編集操作（thin / easing / scale / smooth / quantize / mirror）と keyframe 変換 utility を実装する。現行 repo 内の呼び出し元は見つからず、既存挙動を保って `.cpp` に移した。
- Audio split target の source list / source 条件 / object-order 条件、source manifest、provider と imported-module mappings を新 path に更新した。実装本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 対象になる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Export.Lottie.Exporter implementation migration（2026-10-22）

- `ArtifactCore/src/Export/Lottie/LottieExporter.cppm` は Lottie JSON validation / serialization、keyframe compression と file output を実装するユーザー実行の export path である。既存処理を保って `.cpp` に移した。
- Export target source list、scanner exception list、provider map、専用 IFC / object dependency paths を更新した。実装本文は LF 化以外同一。Clang final configuration は migrated `.cpp` の P1689 scan を有効化し、旧 MSVC-only module edges を除去する。
- ビルド、依存スキャン、テスト、CMake は実行していない。
