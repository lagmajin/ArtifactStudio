# MILESTONE_CLAP_HOST_COMPLETION_2026-07-25

**最終更新:** 2026-10-21
**ステータス:** In Progress — loader、params、GUI、state stream API、複数バス／sidechain音声処理、thread-check host extension、要求元別host request callbackとmain-thread callback dispatch、AudioMixer結合スモークを実装。restart/process要求の実サービス処理と実プラグイン互換を含むruntime受入が残る。
**対象:** `ArtifactCore/include/CLAP/CLAPHost.ixx`, `ArtifactCore/src/CLAP/CLAPHost.cppm`
**位置づけ:** CLAP (CLever Audio Plugin) ホストのインスタンス生成を実装。
**作成日:** 2026-07-25

## 現行コード監査 (2026-08-15)

`PluginLibrary` は `clap_entry` の解決、entry の init/deinit、DLL 生存期間の保持を実装し、`Host::loadPlugin()` は descriptor 列挙・`PluginInstance` 作成・初期化・保持まで行う。`Plugin` 側には params 拡張の列挙／値取得／値設定、process 用の `AudioSegment` 変換、host callback があり、`ClapEffect` は activate／startProcessing／process／deactivate と EffectParameter の取得・設定へ接続されている。

旧文書の「params 拡張・process 変換・host callback・Effect 統合は未完了」という判定は現状と一致しない。残課題は CLAP UI／editor 導線、複数 plugin の選択・管理、実プラグインでの event／audio layout／latency 契約、scan／load／unload と render queue を含む runtime 受入である。

## Update 2026-08-15

上記の現行コード監査を、実装行まで再照合した。`clap.params` の列挙・値取得・値設定・表示文字列変換、`ClapEffect` の `AudioSegment` 接続、`Host::scanPlugins()`／`loadPlugin()`／`unloadPlugin()` はコード上で確認できるため、旧「未統合」タスク表は履歴として扱う。現在の残課題は UI 拡張、イベント／複数バス／レイアウト／レイテンシ契約、runtime 受入とする。

## Update 2026-10-20

- `clap.state` の `save`／`load` を CLAP stream ABI に沿って `Plugin`／`PluginInstance` へ公開し、短い read/write に対応する vector stream を実装した。Effect JSON には plugin path／descriptor ID／parameter 値も保存し、state extension 非対応プラグインでも再ロード時に parameter を復元する。状態保存はメインスレッド上のコールドパスで呼ぶこと。
- `Host::loadPlugin()` が1つ目の有効なdescriptorの後でループを終了し、ライブラリ内の残りのプラグインを作らない不具合を修正した。
- `Host::createEffect(nullptr)` で JSON 復元用の空の `ClapEffect` を作れるようにした。従来は `nullptr` を返し、復元テストが null オブジェクトを呼び出していた。
- `tests/Artifact/PluginHostSmokeMain.cpp` と `tests/Artifact/FakeClapPlugin.cpp` を追加し、ホスト単体の再現可能なテスト環境を用意した。fixtureで複数入力bus（stereo main + mono sidechain）、stereo output、direct process、AudioSegment変換、state round-trip、Effect JSON復元を確認する。
- CLAP audio port のbus数とbus内channel数を分離して `clap_process` に渡す。最大8 bus、各bus最大2 channelを初期化時に検査・確保し、フレーム処理では補助入力busへsidechainまたはゼロを供給する。
- `clap_output_events` の core parameter-value event を bounded lock-free queue に転送し、ホスト側から取り出せるようにした。smoke fixture が ID／値／sample time を出力し、ホストが受信内容を検証する。
- production `AudioBus` のFX rackへ実際の `ClapEffect` を挿入し、bus main inputとsidechain inputからプラグインを処理する結合スモークを追加した。
- 同一 `AudioBus` のFX rackにCLAPとVST3を連続挿入し、両プラグインがsidechainを受けながらCLAP→VST3の順で処理される結合スモーク `ArtifactAudioBusClapVst3ChainSmoke` を追加した。期待サンプル値まで確認する。
- `clap_host` をプラグインごとのcontextに分け、`request_restart`／`request_process`／`request_callback` を要求元別atomic flagsへ保存する。`Host::dispatchMainThreadCallbacks()` はHost生成スレッド上で要求元プラグインの `on_main_thread` を一度 dispatchし、`Host::takeRequests()` はrestart／process／callback要求を集約して消費する。fixtureのcallback dispatch確認を追加する。restart/process要求を実際のaudio runtimeへ反映する統合は残る。
- `ARTIFACT_USE_LLD=ON` と `ARTIFACT_LLD_THREADS=8` で smoke target をビルドし、リンク規則に `lld-link /threads:8` が指定されることを確認した。現在の Windows 環境では SDK `rc.exe` と LLVM `lld-link.exe` のディレクトリを PATH に加える必要があった。
- 同じMSVC Debugビルド、同じ既存オブジェクトで `ArtifactPluginHostSmoke.exe` のincremental relinkを各3回計測した。Ninjaコマンド全体の中央値は `link.exe` 6.043秒、`lld-link /threads:8` 4.698秒（この測定条件で約22%短縮）。Ninja／CMake linker wrapperの時間を含むため、clean linkやArtifact本体全体の代表値とはしない。
- host GUI／params/audio-port拡張、request_restart／request_processのruntime反映、異常pluginのprocess isolation、第三者プラグインとの複数bus／state互換検証は未完了。各busは現在最大2 channelに制限する。
- CLAP state契約は公式 [state extension](https://github.com/free-audio/clap/blob/main/include/clap/ext/state.h) と [stream ABI](https://github.com/free-audio/clap/blob/main/include/clap/stream.h) に合わせた。

## Update 2026-10-21

- official [CLAP thread-check extension](https://github.com/free-audio/clap/blob/main/include/clap/ext/thread-check.h) を実装した。Host作成スレッドをmain-threadとして報告し、plugin `process()` の呼び出し中だけthread-local markerで現在のOS threadをaudio-threadとして報告する。fixtureは初期化時のmain/audio判定と、worker thread上でprocessした際のmain=false／audio=trueを検証する。
- Audio Mixer の既存メーター更新タイマーから `Host::dispatchMainThreadCallbacks()` を呼び、CLAP `request_callback` をプラグインの `on_main_thread()` へ渡す実アプリ経路を接続した。`request_restart` と `request_process` はフラグ収集までで、Playback／audio runtimeへの反映は引き続き未実装。
- 標準MSVC／link.exe build treeで `AudioMixerWidget.cppm` のmodule compileが成功した。`ArtifactAppRuntime` 全体は、既存の変更中 `AudioEffectsInit.cppm` でEffect factory型のC2664により停止した。別のMSVC+LLD build treeでは `ArtifactWidgets.dll` のlink時に `RayTrace::Interval::empty`／`universe` が未解決になったが、同じresponse fileをlink.exeで診断リンクすると成功した。LLDとC++ modulesの相互作用が疑われるものの原因は未検証である。
- CLAP→VST3 fixture chainを直接AudioBus処理からproduction `AudioMixer::process()` 経由へ拡張した。main busのFX rackでCLAP→VST3、別busからpre-fader sidechain sendを実行し、そのsidechain sourceはMaster出力から切り離している。
- MSVC Debugでsmoke targetを再ビルドし、CLAP thread-check／単体処理、VST3 AudioBus、invalid bus拒否、AudioMixer経由CLAP→VST3連鎖のCTest 4件が成功した。
- clang-cl比較用の隔離configureでは、clang-cl 22.1.3にCMake 4.2.2のclang-scan-deps ruleを一時設定し、LLVM付属`libomp.lib`を指定するとconfigure／generateまで成功した。MSVCの `std.ixx` をClang driver modeでClang PCMへ変換し、隔離build dirで欠落したstandard-module artifactを補うとbuildがOpenUSDのPCHまで進んだが、OpenUSDのCMake helperがclang-clへ `/wddeprecated`／`/wddeprecated-declarations`／`/wdunused-local-typedefs` を渡して停止した。ArtifactCoreのCLAP module compileにはまだ到達せず、clang-clのコンパイル安定性・リンク速度は未評価である。

## 1. 目的

CLAP ホストの `Host::loadPlugin()` が常に `nullptr` を返すスタブ状態から脱却し、実際の CLAP プラグインDLLをロード・インスタンス化できるようにする。

## 2. 現状 (2026-07-25)

| 要素 | 状態 |
|------|------|
| データ型 (AudioBuffer, Event, Process, PluginDescriptor) | ✅ 完備 |
| Plugin 抽象クラス | ✅ 完備 (virt/init/destroy/activate/process/params) |
| PluginLibrary (DLLローダー) | ⚠️ `PluginEntryProc` の型が間違い。`clap_entry` の解決後、`entry->init()` を呼んでいない |
| Host::loadPlugin() | ❌ 常に `nullptr` を返す (`"骨格: 実際の Plugin* は CLAP SDK 統合時に"`) |
| PluginInstance 具象クラス | ❌ 不在 |
| PluginLibrary 生存管理 | ❌ unique_ptr でプラグイン生存中にDLLが解放される可能性 |

## 3. 実装内容

### GMF に CLAP C API 型追加

以下の C 互換構造体を global module fragment に定義:
- `clap_plugin_descriptor` — CLAP プラグイン記述子 (id, name, vendor, version, features等)
- `clap_plugin` — プラグインインスタンス (descriptor + vtable: init/destroy/activate/deactivate/start_processing/stop_processing/process/get_extension)
- `clap_plugin_entry` — DLLエントリポイント (clap_version, init/deinit, get_plugin_count, get_plugin_descriptor, create_plugin, destroy_plugin)
- `clap_process` — process() 呼び出し用データ (frames_count, deinterleaved audio buffers)
- `clap_host` — ホストコールバック (最小限、必要に応じて拡張)

### PluginEntryProc 削除

`using PluginEntryProc = bool (*)(const struct PluginDescriptor*)` を削除。
CLAP DLL は `const clap_plugin_entry* clap_entry` をエクスポートするため、正しい解決方法に修正。

### PluginLibrary::load() 修正

- `GetProcAddress("clap_entry")` → `*static_cast<const clap_plugin_entry**>(sym)` で解決
- `entry->init(path.c_str())` を呼び出し、成功時のみロード完了
- デストラクタで `entry->deinit()` を呼び出し
- shared_ptr で管理し、プラグイン生存中のアンロードを防止

### PluginInstance 具象クラス

`clap::Plugin` の具象サブクラス。`const clap_plugin*` をラップ:
- `init()` / `destroy()` / `activate()` / `deactivate()` — CLAP vtable に委譲
- `startProcessing()` / `stopProcessing()` / `process()` — 同上
- `process()` で `clap_plugin::process()` を呼び出し、`clap_process` 構造体に変換
- `getExtension()` — 拡張インターフェースの解決
- パラメータ系 (`paramsCount`, `paramInfo`, `paramValue` etc.) — clap_plugin-params 拡張経由（未統合、後続タスク）

### Host::loadPlugin() 修正

- PluginLibrary::load() 成功後、`entry->get_plugin_count()` でプラグイン数を取得
- 各インデックスで `entry->get_plugin_descriptor()` + `entry->create_plugin()` を呼び出し
- PluginInstance にラップして `plugins_` に追加
- 先頭プラグインを返す (複数プラグインも全量ロード)

## 4. 変更ファイル

| ファイル | 変更 |
|----------|------|
| `ArtifactCore/include/CLAP/CLAPHost.ixx` | GMF に4つのC API型追加。PluginEntryProc 削除。PluginInstance 宣言追加 (~120行) |
| `ArtifactCore/src/CLAP/CLAPHost.cppm` | PluginLibrary 全面書き換え。PluginInstance 実装。Host::loadPlugin/unloadPlugin 修正 (~280行) |

## 5. 残タスク

- [ ] clap_plugin-params 拡張の統合 (paramInfo/paramValue/paramSetValue が未実装)
- [ ] processSegment() の AudioSegment ↔ clap_process 変換完成
- [ ] clap_host コールバックの実装 (request_restart, timer 等)
- [ ] エフェクトチェインとの統合 (AbstractEffect からの CLAP プラグイン呼び出し)
- [ ] GUI 埋め込み (clap_plugin-gui 拡張)
