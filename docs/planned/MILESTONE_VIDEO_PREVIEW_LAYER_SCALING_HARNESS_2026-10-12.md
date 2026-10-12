# 動画プレビュー レイヤー数スケーリング検証ハーネス

**最終更新:** 2026-10-13
**ステータス:** In Progress — ハーネス実行済み、プレビュー性能条件は未達

## 最新進捗 2026-10-13 — 全layer matrixとCSV/manifest

- ACK修正版/Qt mode全1..8-layer各60秒・disk-readyが完了。全開始RAM90/disk90、表示/描画1800、drop/repeat0。累積lateは0/0/0/0/1/1/9/27で5..8-layer未達、正常exit1。`temp/video-qt-ack-full-matrix-60s.json`。CSV全8件のcount一致、pre/post exe SHA256一致、外部memory観測終了確認。Working Set peak 11256479744 bytes（whole process）。
- 独立Win32 wait probeを追加・実行。1ms要求各256回のp50はSleep15.4294ms／通常waitable15.5977ms／high-resolution waitable1.5371ms。timer resolution/priority設定は変更せず、自プロセスのhandleのみ使用・close。`temp/video-windows-wait-probe.json`。非GUI Pythonの結果であり、実render ticker/C++ condition-variable/previewの原因証明ではない。次に実ticker待機を確認する。

- 開始時点の途中記録: ACK修正後のQt mode全1..8-layer・各60秒・disk-ready試験を開始し、終了後に起動直前SHA256/引数/exitを記録するwrapperで実行。1/2-layerはログ上drop/late/repeat0で終了。当時は全体の完成JSON未取得（完了結果は上記）。`temp/video-qt-ack-full-matrix-60s.log`。
- exporterに`--launch-record`を追加。対象report pathと64桁SHA256を検証し、独立起動記録とfingerprintをmanifestへ添付。実paired trial1で添付一致確認、trial2へ誤添付するとexit2でCSV生成前に拒否。loaded-module検証とは区別する。

- 同binaryのpolling/qt/qt/polling順でmixed4-layer各60秒・disk-readyを比較。報告条件と起動直前SHA256は全一致。pollingは2回中1回合格（drop1/0・late1/0）、Qtは両合格（表示/描画1800、drop/late/repeat0）。`temp/video-dispatch-paired-comparison.json`。小規模逐次比較であり、因果・全matrix・安定合格は未証明。
- controllerの表示成功ACKを「statusにfailがない」から「statusがok」へ1行修正。skipped-no-swapchainや復旧状態、failを含まない例外messageを表示成功と数えない。比較終了後にobject/link成功。故障注入は未検証。新API/queue/確保なし。

- 比較用`--measurement-loop qt`を追加。従来polling既定値は維持。stack QEventLoop/QBasicTimerと既存診断callback参照で250ms sampleと測定deadlineを実行し、新signal接続・callback heap確保・製品変更なし。object/link成功。
- Qt mode・disk保存完了待ち・mixed4-layer60秒は開始時RAM90/disk90、表示/描画1800、drop/late/repeat0、実測60.000秒、正常exit0。`temp/video-qt-loop-disk-ready-4layer-60s.json`。単発合格で因果改善とは判断しない。同binary paired反復は未実施。

- disk保存完了待ちのmixed4/8-layer各60秒を実行。両開始時RAM90/disk90、描画/表示1800、drop/repeat0。4-layer late1で未達、8-layer late0で合格、正常exit1。`temp/video-disk-ready-mixed-60s.json`。4-layer first-late frame17はqueued19.74ms＋render-to-present14.50ms＝34.24ms。保存未完了だけでは失敗を説明できない。開始時cache数を新fieldで保持しCSV照合済み。待機なし版の開始時数は未取得であり、因果比較は未確定。

- disk有効/RAM併用・mixed4/8-layer各60秒は両不合格。4-layer表示1799/drop1/late6/repeat0、8-layer表示1800/drop0/late3/repeat0、正常exit1。最終RAM90/disk90は開始時の保存完了を証明しない。`temp/video-disk-cache-mixed-60s.json`。
- 同processのOS peak countersを外部1Hzで観測。Working Set最大11346812928 bytes、paged-memory counter最大11220717568 bytes、233 samples、終了確認。準備＋両case全体の値で、case別・終了直前未sample区間の保証ではない。`temp/video-disk-cache-mixed-60s-memory.json`を新`--process-memory` exporter optionでmanifestへ添付・照合済み。
- 保存完了も待つ明示`--wait-disk-cache`をbenchに追加・object/link成功。disk無効との併用はexit2で拒否。実際の持続比較は未実施、因果は未確定。製品コード変更なし。

- 同一4-layer・RAM-only・3秒窓の通常試験を120秒へ延長。表示/描画3600、drop/累積late/repeat0、正常exit0。診断sample最大3.2351ms（開始79344ms）、video部分最大3.1644ms。`temp/video-sampler-duration-4layer-120s.json`。120秒の固有映像ではなく短区間反復であり、先行失敗の原因・安定性は未確定。

- benchの定期video/renderer診断sampleの最大所要時間を固定数値で記録し、CSVにも保存。通常フレームログ・新規containerは追加していない。4-layer60秒追試は描画/表示1800、drop/late/repeat0、正常exit0。sample最大2.3833ms（開始59515ms）、video部分最大2.2934ms。`temp/video-sampler-duration-4layer-60s.json`。失敗試行との関連はまだ未検証。

- 最新の全1..8-layer・各60秒試験は全1800 presentations/drop0/repeat0、累積lateは0/0/0/3/0/0/0/0。4-layerのみ不合格、正常exit1。`temp/video-sync-gate-full-matrix-60s.json`。最初の4-layer遅延はreset後約18.541秒、frame16、queued19.96ms + render-to-present16.46ms = 36.42ms。背景decode/GPU負荷の原因断定はできない。
- benchに既存controller frame serial差分の`renderCycles`を追加し、表示通知数との比較を可能にした。physical scanoutではない。offline exporterに同指標とfirst-late phaseのCSV/manifest出力を追加。旧reportの未取得値は空欄を維持する。
- 4-layerの60秒追試はrenderCycles1800／表示通知1800、drop/late/repeat0、正常exit0（`temp/video-render-cycles-4layer-60s.json`）。この試行の余分な完成描画は確認されない。先行late3の原因解明・安定合格は未達。
- 別プロセスで4-layer各60秒をさらに2回反復。両renderCycles1800／表示1800／drop0／repeat0、late0/1で正常exit0/1。2回目のframe68はqueued18.49ms + render-to-present15.19ms = 33.68ms、decode0。余分な完成描画がない状態でも遅延が再現した。`temp/video-render-cycles-repeat-{1,2}.json`。

- 追記: first-lateの固定値記録と終了時限定の詳細snapshotを追加。描画tickがPlaybackService clockとComposition frameの不一致時に待ち、既存FrameChangedで再描画する最小修正を実施。最新mixed5/8-layer・3秒窓/各60秒・RAM-onlyは両1800 presentations/drop0/累積late0/repeat0、exit0（`temp/video-composition-sync-gate-60s.json`）。全matrixの反復合格はまだ証明していない。
- 最新native mixed2/8-layer・1秒窓/3秒の検証はfault exception0、重大D3D12/CRTエラー0、正常exit1（late1/0）。`temp/video-native-composition-sync-gate-debug.log`。詳細と旧版の比較限界はbench READMEに記録。

- 最新1..8-layer・混在素材（1-layerはH.264のみ）・論理1920×1080・RAM-only・3秒窓を各30秒ループした通常試験は全900 presentations/drop0/repeat0。lateは順に0/0/0/1/2/0/0/1で4/5/8-layerが不合格、正常terminal exit1。`temp/video-loop-window-mixed-matrix-30s.json`。先行2/8-layer単発合格を全条件の安定合格として扱わない。
- 8-layerの終了時window late0に対しcumulative late1が残り不合格となった。実際の早期遅延が窓のeviction後も保持されることを確認。
- `tools/VideoPreviewBench/export_report.py`で完成JSONからCSVとmanifestをoffline出力。全8件のCSV status/countを原JSONと照合一致。post-run CPU/GPU/driver/OS inventoryとexe fingerprintを添付し、自動launch identity／使用率traceとは区別。未取得の指標はmanifestに明記。`temp/video-loop-window-mixed-matrix-30s-export.csv`、同`.manifest.json`。
- 残る散発late、長い固有source区間、反復安定性、disk有効持続matrix、visual parity、計画内の未取得metricsは未完了。製品アプリは起動していない。

## 最新進捗 2026-10-12 — bounded loopの持続計測

- `--loop-window-seconds` とPlaybackServiceの表示成功累積数を追加。既存128-frame上限を維持し、短いcache窓を長く再生する試験を可能にした。合否は累積late/repeated/dropゼロと要求時間相当のactual presentation数で判定する。
- H.264 720p + ProRes HQ 1080pの混在、論理Composition 1920×1080、RAM-only、2/8-layer、3秒窓を30秒ループした通常試験は両900 presentations、RAM90、drop/late/repeatゼロ、terminal exit0。`temp/video-loop-window-mixed-30s.json`。長い固有映像区間、rolling cache、全layer matrix、disk有効長時間、visual parityは未達／未検証であり、全体完了ではない。
- 最新コードのnative mixed2/8-layer・1秒窓を3秒ループした検証はfault exception0、重大D3D12/CRTエラー0、正常terminal exit1（各late1の性能条件不合格）。`temp/video-native-loop-window-debug.log`。製品アプリは起動していない。

## 進捗 2026-10-12 — ベンチ実装・実行・自動デバッグ

- `tools/VideoPreviewBench` に独立 `ArtifactVideoPreviewBench` GUI target、実行手順、測定JSONを追加した。製品の `ArtifactCompositionEditor`／`ArtifactVideoLayer`／`ArtifactPlaybackService` を使い、layer countごとに動画だけのcompositionを組み立てる。別プロセスで実行し、サービス初期化前にテスト専用の一時データrootを指定する。
- 既定は現在の128 frame RAM image cache budget内に収まる3秒／30fpsのpreview範囲を全構築してから再生。合格はPlaybackServiceのdrop=0、各動画の診断windowでlate/repeated=0、timing診断を採取できたこと、要求時間相当までフレームが進むこと。cacheなし比較は `--no-ram-cache`。
- ユーザー指示に基づきCMake再生成、Debug target build、ベンチ実行と自動デバッグを行った。Debug Qt DLLとplatform/image-format pluginが実行フォルダにあり、独立GUIプロセスは正常終了した。windeployqtはvcpkg bin上のDebug DLLを検出できない警告を出したが、実行フォルダにあるDLLで起動できた。H.264 hardware decodeの初期化は失敗し、FFmpeg software decodeへfallbackした。
- 3秒・1080p30の素材を生成し、H.264約26 Mb/s、ProRes 422 HQ約231 Mb/sを測定。RAM previewなしでは1/2 layerすべてstrict条件（drop/late/repeatゼロ）を満たさなかった。H.264は1 layerで1/6/6、2 layersで0/6/4、ProResは1 layerで1/5/1、2 layersで3/3/4（順にdrop/late/repeat）。
- H.264のRAM previewありは180秒待っても0/89 frameのままreadyにならず失敗した。したがって現状の測定では、譲れない「リアルタイム寄り・コマ落ちなし」の条件は達成していない。これはこの環境での初期ベースラインで、全環境の性能を断定するものではない。
- 実行中に発見したハーネス課題（Composition Editor経路を通らないための診断欠落、動画sourceの非同期open待ち、stop後にframeを読む順序、複数caseでの早期終了）を修正し、再測定した。Artifact／ArtifactCore内でビルドを阻害した既存コードのコンパイルエラーも依頼範囲で修正した。

## 進捗 2026-10-12 — 計測に基づく修正ループ

- H.264 RGB24→float変換の全画素コピーを削減するため、CV_32FC4画像の所有権移譲APIを追加し、生成済みMatをcloneせずLayer画像へ渡すようにした。frame cacheと表示用bufferも共有デコード画像を再利用する経路を追加した。sRGB8 upload変換は行単位の並列処理を維持する。
- Debugの独立ベンチtargetを再ビルドし、H.264 1080p30・1 layer・RAM previewなしを再計測。CPU画像変換は約6.5ms、以降のraw decodeは約14ms、upload変換は約10–12ms、texture作成は約2–4ms。初回raw decodeは約155msだった。strict結果はdrop=2 / late=7 / repeat=7、3.208秒で19/89 frameまでであり不合格。
- ProRes 422 HQ 1080p30・RAM previewなしは1 layerでdrop/late/repeat=2/10/16、2 layersで1/8/7。どちらも不合格。
- H.264 720p30・RAM previewなしも1 layerで3/1/13、2 layersで1/22/4となり不合格。現環境では解像度を下げてもstrict条件を満たしていない。
- H.264 1080p30を1・2 layersでもう一度測ると1 layerは1/1/13、2 layersは0/10/5だった。drop数は変動するがlate/repeatが残り、どちらも不合格。初回profile run（1 layer: 2/7/7）との変動があるため、単発のdrop数だけで改善を断定しない。
- RAM preview有効の追試は準備中に約1.1 GiBのworking setとなり、完了レポートを得る前に停止した。先行する180秒試行も0/89 readyであり、RAM cache経路は現時点で解決になっていない。
- **結論:** コピー削減は画像変換時間を短縮したが、絶対条件の無drop/late/repeatは未達。現状の計測からはソフトウェアdecodeとフレームごとのCPU upload変換・texture作成が実測ボトルネック。GPU decodeまたは転送経路の設計を検討するには、別途Diligent/decoder統合境界の調査が必要。

## 進捗 2026-10-12 — Shared D3D12 NV12 の固定3枠非同期probe

- 独立ベンチの実験モードに、rendererと同じD3D12 device上でFFmpeg D3D12VAのNV12をcompute変換する固定3枠ringを追加した。各slotがoutput texture、descriptor heap、allocator/list、sample 0用readbackを持ち、GPU完了までAVFrame cloneを保持する。decoder fenceはdirect queue上で待ち、CPU待ちはslot再利用時と最後のdrainに限る。
- 生成済みNinjaコマンドから対象 `.cpp` のMSVCコンパイルとlinkを手動実行し、CMake再生成・Ninja buildは行っていない。独立プロセスで1080p H.264 8フレームを実行し、`decodedAndConverted=8`、`completed=8`、`maxInFlightSlots=3`、PTS単調増加、sample 0のRGBA検証成功を確認した。
- 2回目の実行ではframe 3のslot再利用待ちが1.56ms、frame 4–7は0.005–0.007ms。enqueue wallはframe 3–7で2.54–2.66ms。初回setup/sample validationは別途約94msかかる。
- これはGPU変換のbounded async submissionが可能という証拠であり、実時間再生・画面表示・複数動画Layer・drop/late/repeatゼロを証明しない。decoder fenceは各submit時に既に完了済みで、ベンチにvsync-paced drawもないため、絶対条件の達成判定は引き続き実アプリ相当の再生計測で行う。

## 進捗 2026-10-12 — Diligent immediate queue上でのGPU変換submission

- 独立probeの専用D3D12 queueを廃し、`IDeviceContext::LockCommandQueue()` から得たDiligent immediate queueへ`ICommandQueueD3D12::WaitFence`、`Submit`、`EnqueueSignal`でFFmpeg source-fence wait、compute command list、完了signalを順に投入した。queue lockはこのsubmission区間だけ保持し、RAIIでunlockする。
- 同じ1080p H.264 8-frame probeを3回実行し、毎回8/8 complete、PTS単調、`maxInFlightSlots=3`、sample pixel検証成功。全体は143–199ms。1 runでframe 3のslot reuse waitは1.36ms、frame 4–7は0.005–0.011ms、enqueue wallは2.69–3.65ms（初回setup/readbackを除く）。
- これで別queueからDiligentへ出力を引き渡す際のqueue順序問題を避けられる可能性を確認した。実 drawでのstate tracking・描画順序・cache invalidationはまだ未検証。GPU texture cache実装はVulkan imageのみを受け付け、D3D12 NV12を拒否しているため、このprobeをそのまま製品再生へ適用したとは扱わない。

## 進捗 2026-10-12 — 動画テクスチャ再利用プール

- renderer所有スレッド上に、動画Layer IDと寸法ごとのGPU texture slotを再利用する経路を追加した。slot初回作成後は `UpdateTexture` を使い、同じフレームの再描画ではconversion/uploadを省く。格納先は既存の上限50件・512 MiB texture cacheでboundedにする。
- H.264 1080p30を1/2 layerで2回追試したが全case strict不合格。drop/late/repeatは1/1/10・3/7/8、次runは1/2/13・3/6/4。個々の実行間で変動し、安定改善とはまだ断定できない。
- pool profileではslot初回texture作成は約1 ms、その後のUpdateTextureは約0–1 ms。一方、新しいフレームごとのCPU float→RGBA8変換は約11 ms残る。描画submitは約12–15 ms。GPU texture再作成の費用は抑えたが、絶対条件の無drop/late/repeatは達成していない。
- 非同期化はまだ未実装。次候補はCPU pixel conversionだけを固定数のowned upload bufferでworker化し、frame generation IDで古い結果を捨てる構成。Diligent context/resource操作はrenderer所有スレッドに残す。全RAM preview frameへのRGBA8 sidecarは1080pで約8 MiB/frame追加になるため、別budgetなしに採用しない。

## 進捗 2026-10-12 — FFmpeg GPU decode probe

- 同梱FFmpeg CLIの`-hwaccels`は`d3d11va` / `cuda` / `d3d12va`に対応。H.264 1080p30・90 framesを5回ずつ測定し、各runで90/90 decodeを確認。software median 0.18 s、D3D11VA 0.16 s、CUDA 0.16 s。GPU経路はNV12へのGPU→CPU downloadを含み、verbose outputでsoftware=`yuv420p`、D3D11VA=`d3d11`、CUDA=`cuda`を確認。CLI上のdecode throughput比較であり、Composition playback性能の証明ではない。
- ProRes 422 HQ 1080p30はsoftwareで90/90 decode、median 0.14 s。ProRes decoderはこのFFmpeg buildで利用可能なD3D11VA frameを出さなかった。
- app側は`MediaImageFrameDecoder`のdirect decodeにD3D11VA opt-inとHW frame download fallbackを加え、独立benchに`--ffmpeg-hwaccel d3d11va`を追加した。VideoLayer worker→`MediaPlaybackController::getVideoFrameAtFrameDirectRaw()`→direct decoderまでfprintf traceし、実D3D11 hardware frameを受信、CPU presentationへdownloadすることを確認した。通常video decoderやproxy controllerにはD3D11VAを渡さない。
- 同一build・H.264 1080p30 A/Bではstrict不合格。software 1/2 layerはdrop/late/repeat=4/1/13、1/6/5。D3D11VAは2/1/9、2/13/4。raw decode medianは約22.37msから10.63msへ改善した一方、CPU image conversionは6.62msと6.73msで変わらず、結果全体は混在。GPU→CPU downloadとDiligent upload conversion約11msも残る。Diligentへのdirect GPU frame presentationは未接続。
- **次の調査:** upload conversionをdecoder worker側でboundedに準備する経路と、D3D12VA frameをDiligent DX12へ渡すbridgeを個別に検討し、drop/late/repeat=0となるまでbenchmarkで評価する。現時点で無drop条件は未達。

## 進捗 2026-10-12 — 非同期変換pool並列度とD3D12VA再比較

- H.264 1080p30、同一独立bench、3秒、RAM cacheなしでsoftwareとD3D12VAを1/2 layerずつ再計測。softwareは1 layer=1/2/5、2 layers=0/4/4、D3D12VAは1 layer=2/2/4、2 layers=0/5/0（drop/late/repeat）。final frameはsoftware 15/89・8/89、D3D12VA 14/89・8/89で、全case strict gate不合格。
- 専用upload conversion poolを1から2 workerへ一時変更して比較したが、software 1/2 layer=1/2/8・0/7/2、D3D12VA=3/2/5・0/5/1。2 layer render averageもsoftware約35.9ms、D3D12VA約51.3msで改善せず、poolを1 workerへ戻した。単発run間のばらつきがあり性能順位の断定には使わない。
- D3D12VAはRTX 4070 Ti上でdevice初期化と1920×1080 `AV_PIX_FMT_D3D12` frame受信を再確認した。アプリ表示経路は引き続きCPU downloadであり、このpreview比較ではsoftwareより安定した改善を示さなかった。
- Diligentの現行texture format／video shader経路にはNV12のplane samplingがなく、外部D3D12 resource wrapperだけでは既存RGBA sprite表示へ接続できない。direct presentationにはNV12 plane SRVまたはGPU変換、FFmpeg frame lifetime、同一device、decode fence同期をまとめた設計が必要。未実装・未検証である。
- **結論:** 非同期変換とbounded buffer poolは既に試し、worker数の増加も比較したが、drop/late/repeat=0とrealtime range進行は未達。次の修正候補はpool拡張ではなく、playback frame requestからdecode完了・upload ready・render queue・presentまでをframe ID付きで揃えた時系列計測。GPU decodeは稼働確認済みだが、CPU download境界が残る。

## 進捗 2026-10-12 — D3D12VA download / CPU conversion 分離計測

- `ARTIFACT_VIDEO_PREVIEW_PROFILE_HWFRAMES=1` の時だけ、D3D12 hardware frameの`av_hwframe_transfer_data()`時間と続くCPU RGB変換時間を分離記録するbounded profile（1プロセス40 sample）を追加した。resource pointerとdecoder fence valueも出力する。診断無効時は時計取得と文字列出力を避ける。
- H.264 1080p30、1 layer、3秒、RAM cacheなしのD3D12VA runでは、warm download median=約1.41ms（初回8.76ms）、CPU変換median=約5.11ms。renderer側RGBA upload conversionはprofile sample上概ね1ms未満、texture updateは0–2msだった。
- 同runはdrop=0 / late=1 / final frame=15/89でstrict不合格。software対照も初回decode=166.75msのoutlierを含み、final frame=9/89、drop=1 / late=1。単回比較なのでbackend順位を示すものではない。
- 観測したのはGPU→CPU読み戻し時間であり、FFmpegとDiligentのGPU間転送時間ではない。この値だけでは再生停滞を説明しきれず、frame arrival / decoder queue / render readinessの時系列切り分けが必要。GPU間zero-copyの成立にはdevice identity、D3D12 resource state、FFmpeg frame lifetimeとdecode fence待ちを検証する必要があり、未実装。
- Module compileと既存Debug bench relinkは手動の既知コマンドで実施。CMake再生成なし。strict無drop条件は未達。

## 進捗 2026-10-12 — rendererと同一D3D12 deviceでのFFmpeg decode probe

- 独立ベンチの`--probe-shared-d3d12-device`で、Composition rendererのshared `ID3D12Device`をFFmpeg D3D12VA contextへ渡し、H.264をdecodeする。初期は一枚probeとして作り、その後sequential 8 frame変換とresource reuse測定へ拡張した。
- RTX 4070 Ti上で成功。1920×1080 `AV_PIX_FMT_D3D12` frameを受信し、frame textureの`GetDevice()`とrendererのnative device pointerが一致 (`sameDevice=1`)。resource formatはDXGI NV12 (103)、decode fence value=1。
- 同じデバイスの専用probe queueでFFmpeg fenceを待ち、NV12 plane SRVからcompute shaderでRGBA8へ変換できた。出力はDiligent `CreateTextureFromD3DResource`で正常にwrapされ (`diligentTexture=1`)、readbackした中心画素は`91,136,176,255`。
- sequential decodeした8 distinct framesのPTSは`0,512,...,3584`で単調増加し、各NV12 frameの同device GPU変換とDiligent wrappingに成功。FFmpegはsurfaceごとに別fenceを再利用し、fence valueは同一fence内で進むため、全フレームを跨ぐ単調値として扱えない。
- probe専用でqueue / shader / PSO / descriptor heap / output・readback resource / allocator-list / fence / Diligent wrapperを一度作成し、8 frameで再利用する形に変更。pixel validation用readbackはsample 0のみで、warm sampleはoutputを直接PSRへ戻す。
- 3 process runsで8/8 sequential framesすべて成功。warm 21 samplesのqueue submit-to-fenceはmedian 0.278 ms (0.202〜0.487 ms、compute dispatch)、decode receive (demux/decodeからframe受信まで) はmedian 0.497 ms (0.223〜1.431 ms)、conversion function wallはmedian 2.947 ms (2.654〜4.801 ms)。conversion wallはdescriptor更新・CPU command記録・blocking fence完了を含む。cold resource setupを含む初回は94.4〜96.8 ms。8 frame sequence全体は194.5〜206.2 ms。
- すべてのframeでdecoder fenceはconversion submit時点ですでにcompletedだった。計測は一slotを同期再利用するstandalone probeで、非同期overlap、複数slot、visible draw、通常再生中のdrop/late/repeatは未検証。数値は製品のsteady-state再生性能ではない。
- `ArtifactCore`の`GpuVideoFrame`へD3D12 texture/fence/value/subresource handleを追加し、FFmpeg `AVFrame` cloneをlifetime ownerとして保持する抽出処理を追加。関連するVideoFrame/decoder interfaceとdecoder実装のMSVC単独compileは成功。通常decode callerからGPU frameを返す分岐、renderer cacheへの接続はまだ行っていない。
- 現probeは一つのoutput slotを同期再利用し、各frame後にCPUがcompletionを待つ。sample 0以外はreadbackしないが、実際のdrawや通常のplayback schedulingは検証していない。製品接続にはbounded multi-slot output ring、nonblocking fence polling、conversion completionまでのAVFrame lifetime、rendererとのhandoffが必要。厳格なdrop-free preview gateは未達。
- 検証は別プロセスのベンチ専用モードで完了。Artifact appの通常VideoLayer経路には接続していない。CMake再生成なし。strict preview gateは引き続き未達。

## 目的

H.264 のような長GOP負荷と Apple ProRes のような高データレート素材を使い、動画レイヤー数を1枚から段階的に増やしたときのプレビュー再生品質・遅延・資源使用を再現可能に測る。ユーザーの「動画レイヤーだけを1、2、3…枚と増やす」テストを、素材／条件／出力レポートまで固定した独立実行プログラムとして用意する。

## 現状と根拠

- `docs/analysis/VIDEO_LAYER_PREVIEW_LATENCY_2026-08-06.md` は、cache miss時の同期 decode fallback、CPU float RGBA 変換、frame cache の枚数固定、複数動画レイヤーでのデコード負荷を潜在要因として列挙している。内容は静的調査で実測ではない。
- `docs/bugs/REPORT_VIDEO_LAYER_FLICKER_2026-06-25.md` は、再生中の decode pending による黒／古いフレーム表示を記録している。過去状態の記録であり、現在も同じかは測定が必要。
- `docs/planned/MILESTONE_PLAYBACK_DIAGNOSTIC_LOGGING_2026-08-08.md` には playback session log と preview route 診断がある。ハーネスはこれら既存ログを収集し、同じ計測を無駄に重複させない。
- `tests/fixtures/generate_test_videos.ps1` と Artifact の動画テスト群があるが、独立した実 Composition preview を複数動画レイヤーで段階負荷する専用ランナーは今回の検索では確認できなかった。
- この実行でハーネスと実測は確認できたが、計測結果のみでは製品側の遅延原因を確定できない。

## ハーネス形態

独立した **ArtifactVideoPreviewBench** 実行プログラムを作る。FFmpeg 単体のdecode benchmarkではなく、Artifact の実際の VideoLayer → Composition preview/playback → renderer 経路を通す。GUIの手動操作に依存せず、別プロセスからテストシーンを作成／実行できる形を優先する。既存の製品コードを別コピーして再実装しない。

各ケースは動画レイヤーのみを composition に作り、1枚から `N` 枚まで段階的に増やす。動画レイヤーは同一source共有ケースと独立sourceケースを区別する。音声、エフェクト、マスク、3D、画像等は初期検証では混ぜない。

## 試験軸

- **Codec / container:** H.264 (long-GOP)、ProRes 422 HQ、必要に応じ ProRes 4444。実際の codec/profile/pix_fmt は素材probeで記録する。
- **Layer count:** 1, 2, 4, 8…の設定可能な段階。失敗・閾値超過で停止する。
- **素材条件:** 解像度、FPS、bit depth、chroma、duration、GOP、平均/ピーク bitrate をmanifest化する。
- **再生操作:** warm playback、cold start、seek/scrub、速度変更、reverse（サポート範囲内）。各ケースの開始位置・測定時間を固定する。
- **経路:** interactive viewport previewを正とし、RAM preview on/off等は別ケースとして明示する。
- **環境:** CPU/GPU/driver、decoder/backend、proxy mode、app build ID、memory budgetを保存する。

## 計測結果

各ケースで playback session logs と併せて以下をJSON/CSV reportへ書く:

- requested / presented frame数、unique frame進行、dropped/repeated/stale/black frame数
- frame interval p50/p95/p99、最大遅延、開始・seek後のready時間
- decode queue depth、cache hit/miss/eviction、同期fallbackの回数と時間
- CPU/GPU使用率とmemory peak（取得可能なOS/renderer counterのみ）
- decoder/backendとcodec詳細、エラー、ケース別の合否理由

合否閾値は初回ベースライン計測後に固定する。測定環境に依存する絶対fpsを先に約束しない。初回は統計とドロップ率を採取し、閾値を提案する。

## フェーズ

1. **計測契約:** 現行 playback log のframe/decode/cache情報を監査し、欠けるcounterと安定したmachine-readable outputを確定。
2. **Single-layer baseline:** 代表H.264 / ProRes素材を1レイヤーで再生し、case manifest、環境、frame結果を保存。
3. **Layer scaling:** 1, 2, 4…と増やし、各codec・共有source/独立sourceごとの限界と失敗署名を取る。
4. **Regression run:** 再現性のある軽量セットを通常の動画変更確認へ追加。重いProRes負荷セットは手動／夜間専用に分離。

## 制約

- アプリ内部の動画デコード・Composition preview製品コードの変更は、別途修正スコープとして扱う。ハーネスだけでpreview挙動を改善したとはしない。
- FFmpeg素材生成の再配布ライセンス、ProRes decoder availability、hardware decoderの環境差を明示する。配布できない素材は手元fixtureとして登録し、生成recipe/probe JSONのみ管理する。
- ビルド、CMake configure、テスト起動はユーザーの明示指示まで行わない。
- 現在の作業ツリーには既存の多数の変更・生成物がある。ハーネス着手時はそれらを保持し、無関係なファイルを掃除・変更しない。

## 受け入れ条件

- 1レイヤーから指定上限まで自動的にcaseを生成し、失敗時にもレポートを残す。
- H.264とProResをcodec/profile実測情報付きで比較できる。
- 合成・表示されたpreview routeを計測し、FFmpeg-only decode時間と混同しない。
- 同一設定の再実行で比較可能なJSON/CSVと環境manifestを出力する。
- user data、既存プロジェクト、既存テストfixtureを書き換えない。

## 関連資料

- `docs/analysis/VIDEO_LAYER_PREVIEW_LATENCY_2026-08-06.md`
- `docs/bugs/REPORT_VIDEO_LAYER_FLICKER_2026-06-25.md`
- `docs/planned/MILESTONE_PLAYBACK_DIAGNOSTIC_LOGGING_2026-08-08.md`
- `docs/planned/MILESTONE_PREVIEW_PLAYBACK_PERFORMANCE_LOW_LEVEL_AI_2026-05-23.md`
- `docs/technical/HOT_PATH_RULES.md`

### 2026-10-13 再生中worker維持の検証

実ticker要求1msの中央値は約1.56msで、独立Python待機probeの15msを当てはめる説明は支持されなかった。GUI待ち最大32.710msを確認。idle停止診断64件中61件が再生中の連続frameSerialで、既存tickerは停止時join／再開時thread生成を行う。controller側で再生中idle時のworkerを維持する最小修正を入れ、共通tickerは未変更。既存coalesced Qt投稿の小さなイベント確保がidleにも続く代償があり、通常実行で確認する。

修正後8混合レイヤー／Qt／disk-ready／window3秒／60秒はpreparedRAM90・disk90、描画1800・提示1800、drop／late／repeat0でexit0（temp/video-playback-worker-retain-60s.json）。単一試行のため安定性・因果は未確定。native2／8レイヤー各3秒はexceptions0、重大D3D12エラー0、late1／0でexit1。Live Object警告は残る。通常1–8レイヤー各60秒の全matrixは実行中。長い固有フレームと物理scanoutの検証は未完了。

### 長い固有フレームの次段階（2026-10-13・実装未着手）

確認済みの制約: benchのRAM準備ゲートはbuildRangeReadyまたは全rangeのreadyFramesを要求する。サービスはLRU128フレームでevict時readyをfalseにするため、128超の固有区間は全RAM-readyゲートを満たせない。disk上の存在だけをRAM-readyと扱う変更は、実際に画像が使える契約を壊すので行わない。hydrateFrameFromDiskはQFileInfo／manifest検証／QImageデコード／storeFrameImageInRamを呼び出す同期処理で、seek maintenanceとprewarmから呼ばれる。upload sidecarのRGBA8変換は停止中のみであり、再生中の復元にそのまま適用すると描画側へ変換負荷を戻す。

次の実装では、全区間disk準備完了と再生開始に必要なRAM先読みを別々に観測し、bounded workerでdisk検証・デコード・RGBA8 upload準備を行う。完成結果のGUI所有cacheへの反映だけを既存経路で行い、frame hot pathで同期読込や大容量変換を起こさない。容量はframe数だけでなくCPU画像／RGBA8 sidecar／in-flight結果のbyte数を合計して固定budget内に収める。scene／namespace／generation変更とseek、reverse、loop時の古い結果を破棄し、worker停止と結果所有権を確認する。128上限を単純に拡張する方法は採らない。

検証条件: 128フレーム超の実際に異なるsource framesを再生し、短windowの反復とは別reportで記録する。H.264とProRes、1–8レイヤー、cold／warm disk、seek／reverse／loop、cache missとbackpressure、終了・取消の例外を確認する。各経路でdrop／late／repeat0、実提示数、byte上限、disk-read／conversionのworker実行、stale結果0を証拠化する。物理scanout／黒・古い画像検出も別途必要。現在の全matrix中にはrendererの再ビルドや別benchmarkを重ねない。

非同期復元の所有境界追記（2026-10-13）: 既存writerはキューmutexを保持して保存・budget整理・manifest更新を行う。既存manifest validationはGUI所有memoを更新するため、そのままworkerへ移動しない。worker専用snapshot／immutable identityと、GUIでの世代確認・結果反映を分ける。writer共用時の保存による先読み滞留とmutex競合は未計測で、warm-cache試行の原因とは断定しない。

### worker維持・全matrix終了（2026-10-13）

`temp/video-worker-retain-full-matrix-60s.json` は1–8レイヤー各60秒、Qt、disk-ready、window3秒。全case提示1800／描画1800、preparedRAM90／disk90、drop／repeat0。lateは0／0／0／0／0／0／5／1でexit1、未達。launch前後exe hash一致、observerは終了確認済み642件、OS peak working set11413118976bytes（全process／途中開始、cache単独ではない）。CSVとmanifestへlaunchとmemoryのfingerprintを添付した。

7レイヤー最初のlateはframe54・開始1806.25ms、decode0、queued6.45ms、render-to-present29.37ms、全体35.83ms。8レイヤーの最初のlateはProRes layer2・frame59・開始55969.30ms、decode0、queued1.60ms、render-to-present31.79ms、全体33.39ms。単独8レイヤーの成功で安定性は証明できず、次はrender区間全体とPresent／uploadをbounded診断する。

### 小submitのimmediate経路・検証中（2026-10-13）

再生中の2D送信は3／9packetで、旧deferred経路のExecuteCommandListsは最大11.808msだった。DiligentImmediateSubmitter.cppmの選択条件を16packet未満は既存immediate経路へ変更した。大きなbatchとcompute consumer例外、描画順序／PSO／resource transitionを維持。境界16の最適性、他描画内容の表示一致は未確認。共通submitterの変更なので動画以外の小batchにも作用し、表示確認が必要。Diligent依存や公開APIは未変更。

修正後7レイヤー診断は180非空submitすべてimmediate、record中央値0.3815ms／最大1.897ms、drop／late／repeat0。native2／8レイヤー各3秒はexceptions0、重大D3D12エラー0、late1／0で正常合否終了exit1。Live Object警告は残る。7／8レイヤー各120秒、window3秒、Qt、disk-ready、診断無効の通常試行を開始（temp/video-small-submit-immediate-7-8-120s）。長い固有source／他レイヤー表示／physical scanoutの証明ではない。

### 小submit経路・120秒試験終了（2026-10-13）

`temp/video-small-submit-immediate-7-8-120s.json` は7／8混合レイヤー各120秒、Qt、disk-ready、3秒cache window、診断無効。提示／描画は3600／3601、両caseのdrop／late／repeatはすべて0、preparedRAM／diskは90、exit0。launch前後exe SHA256一致。一回の短いcache反復であり、長い固有source、全matrix反復、他描画内容とphysical scanoutの検証は未完了。

画像取得元の調査では、rendererのactiveColorViewはswapchainのcurrent backbufferを優先する。Present後にはbackbufferが切り替わり得るため、停止後readbackを最後に提示したframeとして扱わない。保存画像はGPU targetの診断資料に限る。

### disk近傍探索の候補上限（2026-10-13・未ビルド）

`ArtifactPlaybackService.cppm::hydrateFramesFromDiskNear` の上限を成功件数から試行候補件数へ変更。範囲内の候補を選ぶたびにcountし、missing／invalid fileでも最大9候補以内で戻る。中心→前→後の順序、同期hydration自体、ready／manifest検証の契約は維持する。欠損を飛ばして遠方のcacheを探す挙動は終了する。近傍外の探索は既存build／次のfocus更新に任せる。非同期disk復元ではなく、1候補のI/O／画像変換時間を保証しない。現在の全matrix実行exeには未反映。終了後にobject compile／linkとruntime確認が必要。

### 非同期復元の所有権確認（2026-10-13）

確認済み: disk PNGとmanifestはQSaveFileでcommitする。GUI所有のmanifest memoはcurrentComposition／renderContract／stateHashを読むため、workerから既存memoを呼ばない。reader要求にはimmutableなnamespace／composition ID／contract／state hash／generationを持たせ、結果反映時に現行generationと照合する。writerは保存中にqueue／削除用mutexを保持するため、readerは同じ長時間lockへ依存させない。

確認済み: ImageF32x4RGBAWithCache::SetCpuImageはDeepCopyを行う。単にPNG読込をworkerへ移してこのAPIで毎frame反映すると、GUIでの大きなコピーと確保が残る。同型には既存swapがあるが、ImageF32x4_RGBA／wrapperはQObjectであり、worker生成objectのthread affinity／GPU resource状態を無視して移譲しない。GUIでコールド生成した固定slotのraw pixel作業域と、GUI所有cacheへの交換経路を検討する（未実装／未検証）。CPU image、RGBA8 sidecar、read中slot、完了待ちslotは合計byte budgetに含める。

### 小submit経路の全matrix終了（2026-10-13）

`temp/video-small-submit-full-matrix-60s.json` は1–8混合レイヤー各60秒、Qt、disk-ready、3秒cache window、診断無効。全8caseの提示／描画1800、drop／late／repeat0、preparedRAM／disk90、exit0。launch前後exe SHA256一致。外部memory observerはcase3途中から781 sample、終了確認、OS lifetime peak working set11296538624bytes。cache単独やcase別の値ではない。CSV／manifestにlaunchとmemory記録を添付した。長い固有source／同条件反復／physical scanoutは未確認。

探索候補上限・bounded診断・capture frame identity metadataを含むService／bench main既存objectのcompileとbench linkは成功。新exeでnative exception／capture確認を開始した。通常性能matrixはこの新exeでは未確認。

### 探索上限の実行確認（2026-10-13）

`temp/video-hydration-candidate-profile-summary.json` は32診断sample、最大候補9、成功0のsample2件でも上限以内。中央値0.407ms／最大6.955ms。2レイヤーの診断試行は提示90、drop／late／repeat0、正常exit0。これは診断有効の短試行で、通常性能の合格証明にはしない。

新exeで診断無効の1–8各60秒、Qt／disk-ready／3秒windowの通常matrixを `temp/video-hydration-bound-full-matrix-60s` で開始した。既存7／8各120秒と全matrix成功は別exeの証拠として区別する。

### 測定診断負荷の分離（2026-10-13）

hydration-bound全matrixは中断後にprocess／session消失を確認。reportなし、launch exit nullのため正常終了と扱わない。ログでは1／3／4layerのlate1／1／2、2layerはlate0。終了原因は未確認。

`--final-only-diagnostics` を追加。周期callbackは診断取得を省略し、終了時の累積late／repeat／dropと提示回数による同じgateを維持。renderer時間sampleと周期sample最大時間はこのmodeでは取得しない。reportにdiagnosticSamplingPolicyを保存し、export／比較条件にも追加。既定modeは変更しない。過去reportのpolicy欠落は不明として扱う。ビルド／実行確認中。

### ケース境界checkpoint（2026-10-13・未ビルド）

report指定時は各case終了／cache準備失敗後、`<report>.partial.json` をQSaveFileで原子的に保存する処理を追加。専用schema、complete=false、expectedCaseCountを記録し、最終reportとは区別する。measurement外でのみJSON化／I/Oし、case途中の故障は直前の完了caseまでを保持する。通常exporterはcheckpoint schemaを受け付けない。runnerは残存partialも既存出力として起動前拒否する。現在のfinal-only試行には未反映。compile／runtime確認は試行終了後。

### 終了時のみ診断の60秒試験（2026-10-13）

`temp/video-final-only-1-8-60s.json`、Qt／disk-ready／3秒window、diagnosticSamplingPolicy=final-only。1／8layer両提示／描画1800、drop／repeat0、late0／18、正常exit1、launch前後hash一致。8layer最初のlateはframe1、開始140ms付近、decode0、queued約25ms、render-to-present約38ms、全体約63ms。周期診断を省いても未達。従来成功trialとはexeと実行条件が異なるため因果比較ではない。CSV／manifestにlaunch記録添付済み。checkpoint追加のbench main compile成功、link進行中。

### checkpoint実行確認と同exe比較（2026-10-13）

`temp/video-checkpoint-runtime-1-2.json` は1／2layer各3秒、Qt、disk-ready、window1秒、final-only診断。両drop／late／repeat0、正常終了。case2準備中にpartialがcomplete=false／予定2／保存済み1で存在することを確認。終了後はpartialの2caseとfinal reportのcasesが完全一致。通常exporterのschema拒否とrunner残存partial拒否も確認済み。

同exe、8混合layer各60秒、window3秒、Qt、disk-ready、周期→final-only→final-only→周期の4trial比較を `temp/video-sampling-abba-run.py` で開始。診断無効。性能gateは同じで、diagnosticSamplingPolicyを別条件として保存する。比較中はビルド／exe更新を行わない。小さい逐次標本であり統計的因果証明にはしない。

### ABBA途中2trialの照合（2026-10-13）

`temp/video-sampling-abba-first-two-comparison.json` は入力／cache／寸法／duration／dispatch共通条件とexe前後hash一致を確認。周期late7、final-only late1、両提示／描画1800・drop／repeat0で未達。trial2最初のlateはframe62、開始50223ms、decode0、queued155.08ms、render-to-present18.31ms、全体173.39ms。trial1のrender-to-present約32–39msと異なり、今回の単発lateはGUI受け渡し区間が大きい。OS scheduling、GUI callback、cache競合などの原因は未検証。診断方式の因果効果は4trial終了まで結論を出さない。trial3進行中。

計測解釈の訂正（2026-10-13）: firstLateQueuedMsはVideoLayerのrequestedAt→markRenderQueuedの区間であり、純粋なGUI投稿→callback開始ではない。RAM経路はrequestedAtを保持し、controllerの描画前にmarkRenderQueuedする。155msをGUIキュー待ちだけと断定する説明は不正確で、フレーム要求から描画開始までの合計として扱う。GUI dispatch timestamp診断と描画前stage計測を同一frameで照合する必要がある。

ABBA trial3（2026-10-13）: final-only診断、8layer60秒、提示／描画1800、drop／late／repeat0、正常exit0。3trialの共通条件とexe前後hash一致をoffline照合済み。周期late7、final-only late1／0。trial4の周期診断が進行中で因果結論は未確定。

既存GUI dispatch診断へframeAtCallbackとframeSerialBeforeを追加（未ビルド）。既存profile有効／64sample上限の中でのみcomposition位置を読む。workerからcompositionを触らず、callback時点のframeと投稿要求frameが同じとは断定しない。ABBA終了までexeへ反映しない。

### ABBA比較終了（2026-10-13）

`temp/video-sampling-abba-comparison.json`。同exe前後hash／報告された共通条件一致、8layer各60秒／Qt／disk-ready／3秒window。周期診断late7／0、final-only late1／0。両方式2回中1回合格、全trial drop／repeat0。診断方式のみで安定性は説明できず、既定方式を切り替える根拠にはしない。フレーム対応付きdispatch／render診断のcontroller object compileを開始。比較exeの結果と次exeを区別する。

### frame付きdispatch診断実行（2026-10-13）

controller compile／bench link成功。`temp/video-dispatch-frame-identity-profile.json` は8layer、3秒測定／1秒window、Qt、disk-ready、PROFILE_SETUP有効、final-only診断。提示90、drop／late／repeat0、正常exit0。`summarize_dispatch_profile.py` により64callback中央値0.084ms／最大14.628ms、64render中央値10.7835ms／最大21.992ms。callback frameは1–6のみで上限を使い切り、renderは0–29を覆う。自動frame joinは未確認として出力。診断有効短試行なので通常性能合格には使わない。次はidle callbackでsample上限を消費しない位置へ診断を移し、frame要求→callback→pre-drawの範囲を追う。

### 長時間dispatchのbounded slow診断（2026-10-13）

最初の64実描画callbackに加えて、callback待ち20ms超を最大32件の別counterで記録する修正を追加。上限はcontroller寿命ごと、合計最大96ログ。profile有効時だけ待ち時間を検査し、frame位置の参照と整形はsample対象のみ。baseline終了後もslow上限到達まで時計読取は続くため、診断試行は通常性能合格に使わない。idle／skipのログは出さない。render duration側は従来64件のままなので、長時間slowの描画区間が常に併記されるとは限らない。compile／link確認中。

### slow dispatch60秒診断終了（2026-10-13）

`temp/video-slow-dispatch-60s-profile.json` は8layer／Qt／disk-ready／3秒window、PROFILE_SETUP有効、final-only。提示／描画1800、drop／late／repeat0、正常exit0、launch前後hash一致。64callback中央値0.598ms／最大4.814ms、64render中央値11.229ms／最大19.127ms、20ms超slow callback0件。問題未再現であり、過去の単発lateが解決した証拠ではない。

同exeの診断無効8layer300秒、同3秒windowを `temp/video-eight-layer-300s-normal` で開始。長い固有source再生とは別で、短いcache反復の長時間安定性を確認する。

### 8レイヤー5分試験終了（2026-10-13）

`temp/video-eight-layer-300s-normal.json` は診断無効、final-only、Qt／disk-ready、H.264とProRes混在8layer、3秒cache window。300秒で提示／描画9000、drop／late／repeatすべて0、正常exit0。case checkpointと最終caseは一致。CSVとlaunch記録付きmanifestを `temp/video-eight-layer-300s-normal-export` に保存。実cache画像2400×1195、90frame。短いcache反復の成功であり、長い固有source・rolling cache・物理scanoutの検証とは区別する。過去の単発lateが消滅した保証ではない。

最新controllerのネイティブ例外確認（2026-10-13）: `temp/video-native-final-controller-debug.log`／`temp/video-native-final-controller-repro.json`。2／8layer各3秒、window1秒、Qt／disk-ready、final-only、Win32 debugger下で終了までexception0。提示各90、drop／repeat0。2layer late1、8layer late0、exit1は性能判定の不合格。debugger実行の遅延は通常実行の合格根拠にしない。今回の範囲で例外は再現せず、全条件で解決済みとは断定しない。

### 固有H.264連続再生の基準（2026-10-13）

`temp/video-unique-h264-no-ram-1-10s.json` はH.264 1layer、10秒、loopなし、合成RAM cacheなし、Qt、final-only、診断無効。提示145、描画161、drop0／repeat0だがlate106で不合格（30fpsなら約300提示必要）。最初のlateはframe32、request-to-render30.35ms、render-to-present10.41ms、total40.76ms。decode時間0のsnapshotだけでデコーダ全体のコストを断定しない。shared frame cache内の先読みは継続しており、全cacheを無効にした試験ではない。

先読みcacheの値コピーを通常要求と同じputSharedへ変更し、upload workerへ渡すimmutable payloadを保持する。frame／byte上限・世代確認・upload queue経路は既存を使う。性能改善と例外安全性は再検証待ち。

### 部分準備で長い範囲を計測する設定（2026-10-13）

`--ram-warmup-frames N`（8..128、source range内、RAM cache必須）を追加。先頭N frameだけのRAM／任意disk準備を待ち、測定前にsource全範囲へRAM build rangeを戻す。既存serviceのcache／decode fallback／buildを計測し、非同期rolling実装の完成とは扱わない。`--seconds` の提示数条件、late／drop／repeatの0条件は変更しない。`--wait-disk-cache` は準備prefixだけに適用される。report／checkpointのramWarmupFramesとcachePolicyでfull準備と区別し、比較器にも条件を追加。旧reportに値がない場合は比較条件不明とする。

先読みputShared修正のobject compile／bench link成功。cacheなし同条件10秒は提示147、late108、drop／repeat0で不合格。変更前145／106から有意な改善は確認できず、1回のclone削減のみ。`temp/video-streaming-prefix-h264-1-10s` で新設定（prefix30、H.264 1layer10秒）の実行確認中。

部分cache試験終了（2026-10-13）: `temp/video-streaming-prefix-h264-1-10s.json` はprefix30のRAM／disk準備成功後、300frame範囲を10.046秒計測。提示141、late100、drop／repeat0、最終提示141、正常exit1（性能不合格）。終了時合成RAM／diskは30frameのまま。CSV／launch manifest保存済み。初lateframe30、request-to-render137.09ms、render-to-present11.38ms、total148.47ms。prefix境界からの供給待ちと再生中cache生成条件を次に調査する。実装は既存経路を測る設定であり、rolling cacheの成功を主張しない。

### 再生中のcapture対象修正（2026-10-13）

部分cache試験で合成cacheが30frameから増えない条件を調査。controllerは生成待ちの描画frameでもbuildQueueNextFrameとの一致を要求し、callbackでもキュー先頭を再検査していた。再生中に生成待ち先頭を通り過ぎると後続のexact frame captureが遮断される。再生中のrequestは描画frame自体をtargetとし、callbackの先頭一致だけを省く。composition ID／build generation／request targetとframeの一致／pending membershipを維持し、paused warmupは従来の先頭一致を維持。既存のbounded async readback経路を使用する。compile／連続再生結果は確認中。

### capture停止条件の追加確認とprefix試験修正（2026-10-13）

先頭一致を外した単独試行 `temp/video-playback-capture-prefix-h264-1-10s.json` でもRAM30のまま、提示145／late105／drop1で不合格。native `temp/video-native-prefix-capture-debug.log` はexception0、通常終了exit1。frame30／120でpendingBuild=falseを確認した。`play()` 最後のprewarmRamPreviewAroundは生成範囲をradiusへ戻すため、prefix設定がradius30まで縮めたことが、この試行で生成待ちが空になる直接原因だった。先頭一致条件の修正だけでは説明できなかった。

prefix準備数とradiusを分離し、radiusは測定source frameCountとする。先頭Nだけ準備し、開始時のprewarmで測定範囲のpendingを維持する。メモリLRU128上限は変更しない。controllerの準備有無判定2箇所も、画像全体をcloneするcachedFrameImageBufferから既存のcachedFrameImageBufferShared参照へ変更。exact sourceframeと互換性を確認する既存APIを利用。共有化が必要な古い値cache entryは既存getSharedで一度だけ小さな共有payload生成があり得るが、大容量cloneを判定のたびに作らない。exact compile／link成功、`temp/video-prefix-radius-h264-1-10s` で再計測中。

範囲修正後のprefix試行（2026-10-13）: `temp/video-prefix-radius-h264-1-10s.json` はRAM30→48／disk30→47で生成増加を確認。一方提示47、late16、drop11、repeat0で不合格。diskなしの `temp/video-prefix-ram-only-h264-1-10s.json` は提示94、late64、drop1、RAM95で不合格。disk保存を含む負荷の影響はあるが、RAM取り込みだけでも要求性能を満たさない。queue mutexがPNG encode／manifest／budget処理を通して保持される事実と、GUI側の変換／cloneを引き続き確認する。

QImage版storeRamPreviewFrameImageはRAM用とtask用に同じ画像を2回RGBA32Fへ変換していた。変換を1回にして既存CPU版overloadへ渡す変更を実施。RAM／task所有のcopy、世代判定、既存writer経路は維持。compile／link確認中、効果は未検証。

単回Qt境界変換の試行終了（2026-10-13）: `temp/video-single-convert-prefix-h264-1-10s.json` は提示48、late19、drop1、repeat0、RAM49／disk48、正常exit1。提示47から有意な速度改善は確認できない。世代・pending確認を保ったcapture生成は進むが、live生成と保存を含む経路は依然不合格。最新exeのnative prefix試行 `temp/video-native-single-convert-debug.py` を開始し、例外／GPU errorを別に確認する。

### QImageを経由しないnative readback（2026-10-13）

`ArtifactIRenderer::readbackTextureViewToBufferAsync` を追加し、Composition controllerのcache captureで使用する。既存3-slot staging／fence ringにslotごとのImageUploadBufferを保持。RGBA8 byte列への変換はworkerが直接行い、QImage pixel allocation／QImage→CPU cache変換を挟まない。現在のRGBA16／32F→sRGB8 quantizationは旧経路と同じ計算を維持する。GUI publisherは共有payloadを保持して既存CPU buffer storeへ明示変換する。

slotのbusyに加えpayload useCount>1で再利用を拒否するため、GUI待ちにもleaseが及ぶ。3 slots×各64MiB以下。buffer／staging確保は停止時のcold captureだけ許可し、再生中は容量不足／寸法変更時にfalseでdeferする。既存Qt readback APIは互換caller用に残す。ImageUploadBufferの既存byte containerを再利用し、新規std collectionは追加しない。

GPU→CPU readback自体、GUI上のRGBA32F store、disk writer内のQt PNG encodingはまだ残る。GPU resident preview cache／非Qt disk encodingの完了を主張しない。停止時の小さなSharedPtr／callback確保と既存GUI eventを使い、full-frame byte領域は3slotの冷間準備だけ。interface／implementation compile成功、controller／bench／archive／link確認中。

### native buffer経路の実行確認（2026-10-13）

interface／implementation／controller／bench compile、ArtifactRender archive、bench link成功。`temp/video-native-buffer-prefix-h264-1-10s.json` はprefix30／disk-ready、提示47、late8、drop17、repeat0、RAM48／disk47で正常exit1（不合格）。QImage境界除去だけの性能改善は確認できず。cold pool確保失敗は部分初期化bufferを公開せずfalseを返す修正もcompile／link成功。

最新版の `temp/video-native-buffer-debug.log`／`temp/video-native-buffer-repro.json` はdebugger下で正常exit1、native exception0。D3D12 ERROR／CORRUPTION、Diligent ERROR／Error、assertを検索した範囲で該当0。提示47、late18、drop1、RAM48／disk47で不合格。30frame準備と追加captureが3slotを超えて完了し、lease再利用が進むことを確認。ただし全失敗条件・色alpha parity・Vulkanは未検証。native判定と通常性能判定は混同しない。

既存disk writerはRGBA32F→RGBA8専用変換の後QImageを包んでPNG保存している。OpenCV imgcodecsは親／独立benchの既存include／link依存に含まれるため、非Qt PNG codecを使う親側代替を検討できる。GPU resident cacheの寿命／content identity／総byte budgetを先に固定し、生成queueのgenerationだけを恒久cache identityに使わない。GPU cache自体はまだ未実装。


### 2026-10-13 GPU resident replay の限定実装

- 停止時の RAM capture publication 後に固定32 ownerへGPU textureを準備する。既存50-entry / 512MiB LRU共有。再生時は upload revision / image content key 一致の resident lookup を先に使い、miss は既存 streaming path。新規イベント配線なし。
- resident lookup は miss で resource生成、pixel変換、uploadを行わない。旧4引数APIは互換維持。cold warmupに追加uploadがあり、CPU capture / disk保存を除去した実装ではない。
- interface2、renderer implementation2、controller、bench object、archive、独立bench link成功。
- 通常条件 `temp/video-resident-replay-prefix-h264-1-10s.json`: prefix30 / disk-ready / H264 1 layer、提示47、late17、drop5、repeat0、RAM48/disk47、exit1。最初のlateはframe30。長尺live生成は引き続き不合格。
- GPU resident hitの診断、native例外、ProRes、RGBA parity、Vulkanは別の確認が必要。32frame超の全面GPU cacheやGPU-only captureは未実装。


resident replay の短いloop診断: `temp/video-resident-replay-loop-h264-profile.json` はH264 1 layer / 1秒30frame loop / 10秒で提示300、late/drop/repeat 0、正常exit0。測定中の texture profile 300 samples はcreate/updateがすべて0ms。ただし整数ms診断だけで転送byte数0を証明しない。profiling有効なので通常条件の性能試行とは区別する。長尺prefix試行は不合格のまま。


最新版native prefix試行: `temp/video-resident-replay-debug.log` は正常terminal exit1 / native exception0。D3D12 ERROR/CORRUPTION、Diligent Engine ERROR、assertion failedの検索該当0。`temp/video-resident-replay-repro.json` は提示46、late18、drop1、repeat0。native debugでの例外判定と通常性能判定を区別する。native環境の長尺生成は引き続き不合格。


resident replay の通常混在試行: `temp/video-resident-replay-mixed-2-8-10s.json` は H264 / ProRes HQ を交互に配置した2 / 8 layer、1秒30frame cache loop、各10秒、diagnostic profiling無効。両case提示300、late/drop/repeat 0、正常exit0。これは長尺生成の合格・実際のscanout画像の検証を意味しない。その後、32 ownerの固定配列をimport済みCore.ArtifactArrayのArtifactStaticArrayへ変更し、controllerと独立benchの再linkを実施する。


ArtifactStaticArray版のcontroller compile / standalone link成功。最新版再試行 `temp/video-resident-static-array-mixed-2-8-3s.json` は同じ混在2 / 8 layer、1秒30frame loop、各3秒で両case提示90、late/drop/repeat 0、正常exit0。通常性能の短いloop確認。long unique source、RGBA parity、Vulkan、最新版固定配列版のnative debugger再試行は未確認。直前版native exception0の証拠とはexe fingerprintを分ける。
