# 独立3D／ライトテストのMCP操作

**最終更新:** 2026-10-15

`ArtifactModel3DLightCompositionIntegrationTest.exe --mcp-stdio` は、FBX Viewerと同じ `ArtifactTestMcpServer` を使う独立テストモード。引数なしのGTestモードも維持する。production `CompositionRenderController` とOBJ fixtureを使い、ユーザー設定はQt test modeへ隔離する。

## 操作

- `light.state`: light type、linear RGB、intensity percent、model position／scale、viewport／DPR、frame serial、controller failure、sticky Diligent error状態に加え、rendererのGPU validation診断文字列を返す。D3D12ではdevice removed reasonと直近のbounded InfoQueue内容を含み、Vulkanではbackendとvalidation要求状態を含む。この値は各capture metadataにも入る。

`backendRequested` は起動指定値、`backendActual` はproduction rendererのDiligent device infoから取得した実backend。runnerはこの実値が指定と一致することを検査する。
- `light.set`: `type` はpoint／spot／parallel／ambient／area、`rgb` は0〜1の3要素、`intensityPercent` は0〜1000。全入力検査後に反映する。描画完了はこの応答では保証せず、`renderPending` を返す。
- `light.controls`: booleanの `visible` と `affectedByLights` を両方検査してから、light visibilityとmodelの照明参加を反映する。描画完了はcaptureで確認する。

`light.controls` のoptional `showGizmos` は既存viewport overlay表示を切り替える。最終compositionの画素を変えないことをrunnerの2追加画像で検査する。
- `light.capture`: dirty frameを明示描画し、frame serialの進行を最大5秒待つ。completed final GPU surfaceをPNGとして返す。desktop capture fallbackは使わない。geometry bounds、edge contact、average RGBも返す。

`light.capture` のoptional boolean `includeWindow` をtrueにすると、同じ描画後のHWND画像もGPU readback前に取得する。MCPの第1画像はGPU、第2画像はwindow全体。`windowCaptureSize` と `windowCaptureError` で取得成否を明示し、window画像をGPU画像の代用にはしない。runnerの `--window-proof` 指定時だけvisibility対照の比較画像を `window_ambient_*.png` として保存する。このオプションはタイミングも変えるため通常検査では無効。

MCP hostはサーバーを開く前に、production controllerが2 frame以上を処理し、snapshotの `Layer RT` と `Accum RT` が有効で、final GPU readbackを取得できるまで最大15秒待つ。`light.state`／capture metadataにも2つの中間RTの準備状態を返す。timeout時は最後のframe snapshotをstderrへ出し、初期化・中間target・readbackのどこで止まったかを分けて調べられる。以前のframe serialだけの判定では、pipeline未準備のままMCP接続を受け付けていた。

RGBとPNGはCPU readback／encodingを伴う明示的な診断境界であり、通常frameごとの取得は行わない。Diligent error flagはprocess全体でstickyとし、後続要求で消去しない。

## 外側runner

```powershell
python tools/test_light_mcp.py --exe out/build/x64-Debug/bin/LayerTests/Debug/ArtifactModel3DLightCompositionIntegrationTest.exe --backend vulkan --output out/light-mcp/vulkan
python tools/test_light_mcp.py --exe out/build/x64-Debug/bin/LayerTests/Debug/ArtifactModel3DLightCompositionIntegrationTest.exe --backend d3d12 --output out/light-mcp/d3d12
```

runnerは5種類それぞれのred／green light画像、状態、tools inventory、stderr、process exit codeを保存する。新frame、画面端非接触、foreground存在、指定channel優位（1.2倍）、同じlight typeで色を変更した際のred／green平均値の変化（各20以上）、Diligent errorなしを確認し、不正RGBが状態を変更しないことも検査する。白いPBR反射成分を含むため、色優位だけではなく同条件の対照画像を必須とする。要求timeoutは45秒、stdin閉鎖後の終了待ちは15秒。終了しない場合はrunnerが起動した子processを停止する。

2026-10-15に両backend指定のrunnerが成功し、それぞれ10画像とreportを保存した。モデルはXYZを同一scaleとして立体形状を保つ。MCP要求の間はcontroller tickerを停止し、capture要求中だけ起動することで、light変更とMCP dispatchの描画再入を避ける。ライトガイド線はgizmo非表示設定でもfinal出力へ含まれるケースを確認した。bounds診断は実測3px幅までの線を除外する固定近傍判定で、モデルの意味的な識別ではない。元PNGにはガイド線も保存する。厚いclipped geometryは画像端でも保持し、edge contactを検出する。

MCP handlerの通常C++ exceptionと、native SEH／driver crash／process終了は別の失敗経路。後者はrunnerのexit codeとstderrで検出する。アクセス違反の最初のcontextからminidumpを保存し、アクセス種別・対象アドレス・CPUレジスタもstderrへ記録する。通常GTestはnative exceptionを捕捉せず終了し、明示的な合成SEH検出テストのみ既存のcatch契約を維持する。

`--crash-dump=<absolute-path>` でdump保存先を指定できる。省略時はWindows Tempの `ArtifactLightTest-<PID>.dmp`。外側MCP runnerは出力フォルダ内の固有ファイルを指定し、reportの `crashDump` に保存成否とbyte数を残す。保存は最初のfaultで一度だけ行い、失敗時はWin32 errorも記録する。

2026-10-15の実D3D12遮蔽クラッシュで `saved=1`、1,374,114 bytesのdumpを確認した。合成SEH検出CTestはpassし、D3D12の15 capture MCP runnerも正常終了した（`out/light-mcp/d3d12-crash-dump-enabled/report.json`）。これは診断機能の検証であり、PSO Serializeクラッシュの修正ではない。

デバッガがなくても `python tools/inspect_light_minidump.py <dump> --output <json>` でAMD64例外context、register、stack上のraw値を抽出できる。`--memory 0x...` はdumpへ含まれた指定メモリを最大160 bytes取得する。symbol解決や原因判定は行わない。実faultを解析した `out/light-mcp/occlusion-dump-context.json` ではread access violationと約2.7GBのmemcpy残量を確認した。GetDataのstackに残るsizeと出力blobのbegin/end差はともに `0x73e0e`（474,638 bytes）であり、最初から2.7GBの出力を確保した説明とは一致しない。native Serialize内部で使うコピー元・サイズの由来は未特定。

## visibility／model opt-out追加後の検証

2026-10-15にrunnerへhidden／model opt-out／restoredの3画像を追加した。ambient greenの差分が無効化時に半分未満、復帰時に元の80%以上となることを検査する。Vulkanは13画像すべて成功。D3D12は復帰画像の色差不足で失敗し、再実行では `affectedByLights=false` にもかかわらず緑優位が残って失敗した。10画像の色切替成功を、この追加契約の成功と扱わない。

`ArtifactIRenderer::Impl::readbackTextureViewToImage()`はcopy後にfence signal／flush／CPU waitを既に実行する。単純な待ち時間不足と断定できず、残存draw queue、frame間のGPU定数／resource lifetimeと更新契約の切り分けが必要。失敗画像とstateは `out/light-mcp/d3d12/` および `out/light-mcp/d3d12-repeat/` に保存した。

HWND比較追加版ではD3D12の通常runnerが `affectedByLights=false` のframe 13にgreen残留を再検出した（`out/light-mcp/d3d12-no-window-proof/`）。同じexeの `--window-proof` は13画像で成功し、windowとGPU画像はopt-out中の無彩色model、restore後のgreenを示した（`out/light-mcp/d3d12-window-proof/`）。タイミング依存を示す対照であり、renderer不具合の修正成功ではない。window proofは恒常的な待機回避策には使わない。

## readback fence修正後の確認

依存DiligentのD3D12 fence実装はmanual-reset eventを作るが `Wait()`でresetしない。再利用fenceの2回目以降、イベント通知だけでは目的の完了値を保証できない。アプリ側 `ArtifactIRenderer.cppm` のreadback slot再利用前とmap前に、Wait後も `GetCompletedValue()` が目的値へ達するまでthread yieldする共通処理を追加した。これは明示的な同期readback境界で、frameごとの追加readbackやresource再生成は行わない。依存リポジトリは変更していない。

通常D3D12 runnerは修正後に2回連続13画像成功。actual backend照合追加後もD3D12／Vulkanの13画像がそれぞれ成功した（`out/light-mcp/{d3d12,vulkan}-verified-backend/`）。二重モデル遮蔽CTestも両backendで成功し、modelはXYZ同一scaleとした。ガイド線のfinal出力への混入は別の未解決事項として残る。

## ライトガイド分離後の確認

`CompositionRenderController` のlight `draw()`をbeauty contentから既存viewport overlayへ移した。scene light収集、camera、visibility、selected-only filter、Light3D category filter、gizmo表示条件を維持する。既存layer list上のlightをoverlayで描き、そのcameraが有効なうちにgizmo batchをflushする。新規texture、画像変換、イベント接続は追加しない。overlayの追加走査は既存listへの参照のみだが、大量lightのframe costは未計測。

geometry boundsの細線除外による暫定回避を取り除いた。runnerはraw final GPU画像のboundsを使い、gizmo off／onのPNGが同一であることも検査する。両backendの15画像runnerは成功。HWND比較版でも、windowはpoint lightの線を表示し、GPU beautyは線を含まないことを目視確認した。出力は `out/light-mcp/d3d12-final-clean/` と `out/light-mcp/vulkan-final-clean-window/`。

既存CTest全18件の実行は17件成功、D3D12のtwo-model occlusionだけアクセス違反が再発した。fence修正後の単発成功を安定性の保証とはしない。テストexeのvectored observerで捕捉を妨げずfirst-chance access violationも記録するようにし、`ucrtbase.dll`の例外後、終了処理で`Qt6Cored.dll`の例外を確認した。追加repeat runも最初の実行で同じPSO serialize faultとなり、出力を `out/light-mcp/occlusion-final-3-repeat.log` に保存した。原因は未特定で、light単体15画像の成功へ含めない。

## 遮蔽例外のスタック切り分け

テストexeにDbgHelpのmodule refresh／exception contextからのstack walkと段階ログを追加した。再現時の最初の例外は `occlusion-destroy` 段階で、`ShaderManager::Impl::savePSOCache` → `PipelineStateCacheD3D12Impl::GetData` → `D3D12Core.dll` → `memcpy` の順と確認した。readback処理中の例外ではなかった。後続のQt終了例外を最初の原因と取り違えない。

PSOを生存させたままcacheを保存する破棄順変更を試したが、同じSerialize例外が再発したため変更を撤去した。保存ログは `out/light-mcp/occlusion-pso-serialize-failure.log`。D3D12 cacheのloadを切りSTOREのみで実行しても同じ例外となり、load blobは原因から外れる。テスト側でdestroy直前に全GPU処理を待つ対照も追加したが、3回実行中2回目で同じ `D3D12Core.dll` の `Serialize` 内アクセス違反が再発したため、待機コードは撤去した。ログは `out/light-mcp/occlusion-store-only-failure.log` と `out/build/x64-Debug/Testing/Temporary/LastTest.log`。一方、同じ3回中1回はpassしており、依然として間欠的である。現在のDiligent D3D12 cache constructorはnative `CreatePipelineLibrary(nullptr, 0, ...)` を使う。次に複数MeshRendererで同じpipeline名をload／storeする経路とnative cache登録の寿命を確認する。cacheを無効にして成功扱いにはしない。

`ArtifactStudio.options.ini` の `Diligent/EnableValidation=true` をテストexe隣へ一時設定した比較では、破棄前のInfoQueueに3種類のMeshRenderer PSO名でLoad descriptor mismatch／Store name already existsが出ていた。また、mesh drawではtexture descriptor format `R8G8B8A8_UNORM` を使っていたが、active RTVは `R8G8B8A8_UNORM_SRGB` で、draw時のD3D12 format mismatchも検出した。`ArtifactIRenderer.cppm` はtextureではなくbound RTV view formatをPSOへ渡すよう修正し、validation付き遮蔽runはTest Passed、該当format mismatchは消えた。一方、validation runでSerialize例外が起きない回があったため、クラッシュ修正とは判断しない。InfoQueue詳細は `out/light-mcp/occlusion-view-format-validation-detail.log`。

MeshRendererのopaque／transparent／meshlet／shadow PSOには同一の固定名を全renderer間で渡していた。共有D3D12 Pipeline Libraryでdescriptorの異なるPSOが名前を衝突させるため、ArtifactCore側で作成ごとの一意名を与える変更を加えた。回帰検査は上記validation InfoQueueのLoad mismatch／duplicate Storeが消えることと、遮蔽caseの実GPU画素条件および終了処理が成功すること。

2026-10-15にPSO名変更後のvalidation付きD3D12遮蔽caseを再実行してpassし、PSO descriptor mismatch、duplicate Store、およびbound RTV format mismatchは検出されなかった。初回Load時にはPipeline Library未登録のPSO名warningが出る。今回のvalidation logは `out/light-mcp/occlusion-core-pso-cache-validation-current.log`。D3D12のPSO Serialize終了時アクセス違反は別の間欠事象として未解決であり、これを修正済みとは扱わない。準備待ち追加後のstandalone MCP runnerはD3D12／Vulkan各15 captureを完了し、全captureで実backend一致、中間RT ready、Diligent errorなしを確認した。出力は `out/light-mcp/d3d12-readiness-guard/` と `out/light-mcp/vulkan-readiness-guard/`。

## 未確認の範囲

MCP操作はテストAPI経路を検査する。実アプリのUI編集、保存／再読込、shadow、透明境界の品質は別途検証が必要。実装直後のruntime結果はrunnerの `report.json` と画像を確認してから判断する。
