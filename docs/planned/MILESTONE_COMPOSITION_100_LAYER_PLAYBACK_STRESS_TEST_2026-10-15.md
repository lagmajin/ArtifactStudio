# Composition 100-Layer Playback Stress Test

**最終更新:** 2026-10-15
**ステータス:** In Progress（実装済み。複雑なTransform負荷を追加し、ビルド・実行確認中）

## 目的

製品アプリ本体から独立して起動するテスト用GUIで、合成レイヤーを100枚持つ単一Compositionの連続再生を行い、停止・フレーム進行・表示・診断を再現可能な形で記録する。FBX Animation Viewerの独立実行ファイル、MCP stdio操作、非アクティブwindow capture、`tools/test_mcp_client.py`によるstep実行を参考にする。

## 初期スコープ

- Composition数と各Compositionのレイヤー数を独立した負荷軸として指定できるようにする。既定は1 Composition／100レイヤー。
- まずは1 Compositionの100／200レイヤーを標準プリセットにし、複数Composition条件はComposition切替を含むシナリオとして追加する。組合せの全掛けはせず、選んだ条件だけを生成する。
- テスト用の合成画像レイヤーを使い、外部fixtureやユーザー設定に依存しない。
- 生成データ、再生、capture、診断はテスト専用プロセス内に閉じる。製品アプリの設定・既存プロジェクトを変更しない。
- Play / Pause / Stop は `ArtifactPlaybackService` 経由に統一し、CompositionやPlayback EngineへUIから直接Transport操作を送らない。
- GUI起動、Composition生成、再生開始、一定時間再生、状態取得、任意capture、終了をMCP toolと共通CLI clientから操作可能にする。
- テスト成果物（JSON結果、stderr診断、任意のPNG）は、FBXテストと同じく明示した出力ディレクトリに保存する。

## 再生・負荷条件

- 既定1 Composition／100レイヤー、Transform profile `animated`、30 fps、1920×1080、10分連続再生。`--transform-profile static|animated` で単純な静的配置と複雑なアニメーションTransformを比較できる。
- `animated` は各レイヤーの位置XY/Z、回転、X/Y傾き、XYZスケール、アンカーに5つのキーフレームを設定する。値と位相はレイヤー番号・Composition番号から決定し、全レイヤーの動きが揃わない構成にする。
- 200レイヤーおよび複数Compositionも選べるよう、テスト起動時にComposition数・レイヤー数・Transform profile・解像度・fps・再生時間・capture間隔を指定可能にする。
- 初期の負荷比較では、Composition数を1に固定して100対200レイヤーを比較する。次にレイヤー数を固定してComposition数を増やし、Composition切替を伴う再生を別シナリオとして測る。結果JSONには両方の数を必ず記録する。
- レイヤーは単色／簡単な生成パターンとし、可視・非表示、opacity、Transformなどのケースを決定的に作る。初期段階では動画デコード、外部画像I/O、物理simulationを含めない。
- 再生中は一定間隔でframe、PlaybackState、経過時間を記録し、captureは要求時に取得する。最後にframe進行と再生継続時間を結果へ記録する。
- CPU/RAMの継続計測、GPUメモリの定量計測、性能合格閾値は初期段階では必須にせず、取得できる診断値と未取得項目をmanifestへ明記する。

## FBXシステムから再利用するもの

- CMake optionでopt-inする独立実行ファイル（`ARTIFACT_BUILD_COMPOSITION_PLAYBACK_STRESS_TEST`）。
- `ArtifactTestMcpServer.h`によるstdio MCP transportとテスト固有tool登録。
- `ArtifactTestWindowCapture.h`による対象HWND capture。
- `tools/test_mcp_client.py`のstep JSON、timeout、stderr log-pattern失敗判定。
- 実行結果JSONに設定・各step所要時間・再生区間・frame観測・capture path・失敗理由を残す。

## 実装段階

1. Standalone GUIを作り、単一Compositionへ決定的な100レイヤーを生成して初期表示する。
2. `ArtifactPlaybackService`に接続し、MCPからplay/pause/stop/stateを操作する。
3. duration指定の連続再生と周期的state採取、要求時capture、異常終了条件、result manifestを追加する。
4. D3D12／Vulkanで実行し、対象windowのcapture、frame進行、長時間運転の結果を別々に記録する。

## 実装記録（2026-10-15）

- `ARTIFACT_BUILD_COMPOSITION_PLAYBACK_STRESS_TEST` と独立target `ArtifactCompositionPlaybackStressTest` を追加した。
- `CompositionPlaybackStressMain.cpp` はテストプロセス内で指定数のCompositionと32×32 Solidレイヤーを生成し、Composition Viewへ表示する。レイヤーはキャンバスのアスペクト比に合わせた格子で全域へ配置する。Composition数とレイヤー数を別々に指定し、生成総数は5,000レイヤーまでに制限する。
- MCP toolは `stress.state` / `stress.start` / `stress.wait` / `stress.pause` / `stress.stop` / `stress.switchComposition` / `stress.capture`。PlaybackのTransport操作は `ArtifactPlaybackService` を呼ぶ。
- `--composition-count` / `--layer-count` / `--transform-profile static|animated` / `--width` / `--height` / `--fps` / `--duration-seconds` / `--sample-interval-ms` / `--output-dir` を実装。結果は `playback-run.json`、キャプチャは `capture-####.png`。sample recordは最大3,600件を保持し、超過時はtruncate状態を記録する。
- `animated` profileでは各レイヤーに5時点の位置、Z位置、回転、X/Y回転、XYZスケール、アンカーを設定する。Transform評価の負荷は再生中に発生し、キーフレーム作成はComposition構築時に限る。profile名をMCP stateと再生結果JSONへ記録する。
- `ArtifactTestMcpServer::dispatchRequest` の固定120秒上限が長時間の `stress.wait` を切断する不具合を修正し、サーバー上限をテストAPIの最大24時間に合わせた。
- MCP clientはGUIプロセスが終了猶予内に終了せず強制終了された場合、成功扱いにせず失敗を記録する。
- configureと `ArtifactCompositionPlaybackStressTest` のビルドに成功。MSVC警告 `ArtifactMainWindow::closeDock` のC4715は既存のまま。
- 多数レイヤーの複雑なTransform比較向けに `--transform-profile static|animated` を追加し、既定を `animated` にした。`animated` は各レイヤーに5時点の位置XY/Z・回転・X/Y傾き・XYZスケール・アンカーを設定する。レイヤー配置もキャンバス全体を覆うアスペクト比対応格子に変更した。
- `ArtifactCompositionPlaybackStressTest` を再ビルドし、1 Composition／100レイヤー／animated／Vulkan／3秒を実行。MCP client成功、`playback-run.json` の `passed=true`、実時間6,012 ms、フレーム変化3、stall 0、stdio終了正常、capture取得を確認。
- ビルドを止めた `NativeDockSurface.ixx` のC2352は、staticな `createTabSurface` 内からインスタンス関数 `updateAutoHideButtonPresentation` を呼んでいたため発生していた。インスタンス設定に依存しない既定表示は生成時に設定済みなので、その誤った呼出しのみ削除した。子リポジトリの他の既存変更は維持した。
- 1 Composition／100レイヤー／1920×1080／30 fps／10分／D3D12: 再生マニフェストは `passed=true`、実時間603,008 ms、600サンプル、600フレーム変化、stall 0、capture取得。以前はMCPの120秒上限で切断されたが、上限修正後はwait・capture・shutdown tool応答まで成功した。ただしD3D12プロセスは30秒の終了猶予内に終了せずclientが強制終了しており、全体の実行結果は正常終了合格ではない。
- 1 Composition／200レイヤー／3秒／Vulkanは短時間の再生とcaptureに成功。2 Composition×100レイヤー／3秒／D3D12ではComposition切替が動作。Vulkanは再生中captureも成功。
- D3D12では再生中captureのあとに `stress.wait` を呼ぶと `resource deadlock would occur` が返るケースを確認した。D3D12のcaptureは再生完了後に実行する。
- ユーザー提供のダイアログ画像で、MSVC `optional` の `operator*() called on empty optional`（ヘッダー230行）を確認した。呼び出し元のスタックは未取得。Visual Studioへアタッチした部分実行では再現せず、呼び出し元特定は保留中。以前の `0x80000003` / vector範囲アサートという説明は、この画像の例外とは一致しない。
- 結果には実presentation数の計測値がなく、`observedPresentedFrames` は0。frame変化サンプルはTransportの進行を示すもので、実際にGPUへpresentされたフレーム数やFPS保証の根拠ではない。

起動例（ビルド後の実行ファイルを使う。configure/buildは別途明示指示があるまで実行しない）：

```powershell
ArtifactCompositionPlaybackStressTest.exe --mcp-stdio --composition-count 1 --layer-count 200 --transform-profile animated --fps 30 --duration-seconds 600 --output-dir C:/temp/artifact-playback-200
```

既存の静的Transform条件と比較する場合は `--transform-profile static` を指定する。

MCP clientのstep JSONは `stress.start` → `stress.wait`（`timeoutMs`を指定再生時間より長くする）→ `stress.capture` とする。`tools/test_mcp_client.py --timeout` も同じ待機時間を越える値にする。

```json
[
  {"tool":"stress.start"},
  {"tool":"stress.wait","arguments":{"timeoutMs":660000}},
  {"tool":"stress.capture","arguments":{"timeoutMs":10000},"image":"final.png"}
]
```

`stress.wait` を含む長時間実行では、CLI clientの `--timeout` も指定時間に合わせて延長する。例の600秒なら少なくとも660秒程度を指定する。

## 受け入れ条件

- 製品本体を起動せず独立targetを実行できる。
- 指定したComposition数・各レイヤー数・再生範囲・fpsがMCP stateとresult JSONで確認できる。100／200レイヤー条件を個別に実行できる。
- Transport操作は `ArtifactPlaybackService` を通り、指定時間後に停止して結果を確定する。
- 再生中にframe進行が観測され、tool failure、timeout、診断log patternを失敗として報告する。
- captureがテストwindowを前面化せずに取得でき、環境制約で取得できない場合は未確認として記録する。
- D3D12／Vulkanごとにbackend、実行条件、ログ、captureを保存し、片方の未実行を合格と数えない。

## 制約・未確認事項

- D3D12テストプロセスは再生・captureの後、shutdown requestを応答しても30秒以内に終了しない。GPU teardownの停止箇所は未特定であり、正常終了の検証は未完了。
- 100レイヤーの再生成功は性能保証ではない。目標FPS、許容drop率、長期メモリ増加の閾値は実測後に定める。
- 性能閾値、GPU present数、GPUメモリの定量計測は未設定・未取得。
