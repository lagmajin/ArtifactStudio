# Contents Viewer 独立表示テスト

**最終更新:** 2026-10-16
**ステータス:** In Progress（PNG/SVG表示・resize assertion修正、WAV再生位置、MP4再生とキーボードシークを確認。マウスドラッグシークと長時間resize stressは未確認）

## 目的

`ArtifactContentsViewer` をアプリの通常レイアウトから切り離して起動し、画像・SVG・動画・音声・3D モデルそれぞれの表示経路を確認する。これはウィジェット単独の画面スモークテストであり、内部ロジックを検査する自動 unit test とは区別する。

## 起動方法

既存の `ArtifactUiTest.exe` を `tools/capture_ui_test.py` から起動する。`--contents-viewer` に1素材を渡すと、その素材だけを Contents Viewer で開き、他のドックを隠したスクリーンショットを保存する。

```powershell
python tools/capture_ui_test.py --exe <path-to-ArtifactUiTest.exe> --output <output.png> --language en --contents-viewer tests/models/checker.png
```

同じ手順を `ja` / `en` / `zh` / `zh-TW` で実行し、媒体ごとに別の出力先を指定する。リポジトリ内の標準素材は次のとおり。

| 媒体 | 素材 |
| --- | --- |
| ラスター画像 | `tests/models/checker.png` |
| ベクター画像 | `tests/fixtures/contents-viewer-vector.svg` |
| 動画 | `tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4` |
| 音声 | `tests/fixtures/contents-viewer-audio.wav` |
| 3D モデル | `tests/models/test.obj` |

## 確認項目

- 起動ログに `[ContentsViewerUiFixture] showing` が出て、指定ファイルがヘッダーに表示される。
- ラスター画像と SVG が表示され、空白や読み込み失敗表示にならない。
- 動画がフレームを表示し、再生・停止・シーク操作で UI が固まらない。
- 音声で波形／メタデータが表示され、再生位置が更新される。
- OBJ でモデルまたは明示的な読み込みエラー状態が表示され、表示モードや Reset 3D が利用できる。
- 各言語で文字の欠け、クリッピング、RTL の崩れがない。

## 制約

- この入口は通常アプリのプロジェクトや既存ドックに依存しないが、起動バイナリと Qt / GPU / media backend は共有する。
- PNG キャプチャは表示結果を記録するもので、レンダリング成功をピクセル判定する自動 assertion ではない。
- 実行環境にない backend / codec による失敗は、Contents Viewer の routing failure と分けて記録する。


## 2026-10-15 初回実行結果

- `ArtifactUiTest` のビルドは完了。CMake の依存再スキャン後、Windows SDK の `rc.exe` が PATH にないため一度リンク停止した。SDK `x64` bin を PATH に加えて再実行し、リンク／DLL 配置まで成功した。POST_BUILD の windeployqt は翻訳カタログ警告を出したが、fixture 起動は可能だった。
- PNG / SVG / MP4 / WAV / OBJ を `ja` / `en` / `zh` / `zh-TW` で計20回起動し、全キャプチャが生成された。PNG `en` の単体 widget capture は中身が黒く取得されたため、実ウィンドウ capture へ切り替えた。ドックは Contents Viewer Fixture のみ表示される。
- **Routing / metadata は各素材で認識されるが、表示は全形式で受入れ不可。** PNG と SVG は Preview 領域が空、MP4 は白画面、WAV は `No audio data` と波形 idle、OBJ は vertex/polygon metadata が出る一方で viewport が白画面だった。言語による差ではなく、全ロケールで同じ状態。
- Contents Viewer の chrome は4ロケールを通して英語のままで、表示崩れは見られなかった。翻訳が未接続か意図的な英語固定かは未確認。
- したがって、この実行は独立 fixture の起動・形式別経路の再現性を確認したが、ビューワー機能の合格を示すものではない。次の作業は画像 GPU surface、video output、audio decode/waveform、3D viewport を各々切り分けること。
- キャプチャ保存先: `temp/contents-viewer/`。`png-en-window.png`、`svg-en.png`、`video-en.png`、`audio-en.png`、`model-en.png` が形式ごとの代表画像。

## 2026-10-15 原因切り分け

- **OS画面での再確認:** ImageGrab.grab(all_screens=True) でアプリ実画面を撮影し、SVGの表示領域が空、WAVの波形が No audio data、MP4の表示が黒であることを確認した。QtのQScreen::grabWindow()がDiligentのネイティブ子ウィンドウだけを撮り損ねた結果ではない。OBJは試行時にアプリが前面に出なかったため、OS画面での再判定は未完了。
- **画像経路:** activateImage() は QPixmap のデコード後、GpuImagePreviewWidget のみを表示面に使い、隠した QLabel へCPU画像を用意するだけでfallback表示には使わない。GpuImagePreviewWidget::ensureRenderer() は ArtifactIRenderer::initialize() 後の isInitialized() を確認せず、失敗時にも再試行／代替表示をしない。
- **音声経路:** `play()` は非同期readerの開始直後に `pumpAudioPlayback()` を同期実行する。reader queueが空なら `getNextAudioFrame()` は空を返し、viewerはそれを終端としてstop／sink suspendするため、開始時raceの可能性はあるがruntime未計測。標準WAV fixtureはモノラル・24kHzだが、`MediaAudioDecoder::initialize()` は既定resamplerを初期化時に構築し、出力PCMはステレオ・44.1kHz・S16となる。sink／waveformの形式とは一致するため、以前の形式不一致という説明は誤り。PCM bytes／sink write／QAudio errorを得るruntime assertionはまだない。
- **動画経路:** `activateVideo()` はsourceとposition 0を設定するがplayは呼ばない。さらに通常captureは `ArtifactUiTest.exe` をビルド出力から起動し、そこにはQt MultimediaのFFmpeg pluginがない。同じfixtureをUI AutomationでPlayしたA/Bでは、Qt Debug plugin pathなしは黒画面のまま、`C:/vcpkg/installed/x64-windows/debug/Qt6/plugins` を `QT_PLUGIN_PATH` に指定した実行ではH.264映像フレームが実画面に表示された。pluginありのstderrには対象MP4をFFmpegがdemuxしたstream metadataも出た。従って独立fixtureでの動画黒画面はQt multimedia backend pluginのruntime配置差で再現できる。通常起動時に初期フレームを要求しない点も別に残る。
- **3D経路（追加確認で訂正）:** 前面化したArtifactUiTestのOS desktop capture `temp/contents-viewer/model-os-active.png` でOBJ立方体がネイティブviewport上に表示されている。metadataは24 vertices / 6 polygons、backend `ufbx`。白く見えたQt widget captureはnative surfaceを取得できなかったためで、3D表示失敗の証拠ではなかった。GPU draw/presentの内部カウンターは未取得だが、実画面の表示は確認済み。
- **追加実行ログ:** PNG/SVGの起動ではDiligentがNVIDIA GeForce RTX 4070 Tiを選び、Vulkan swap chainのsurface transformとpresent modeを初期化した。プロセス標準出力にDiligent ERRORは無かった。これはfixtureプロセス内でVulkan device／swap chain系の初期化が行われた証拠だが、ログはwidget識別子を含まず、画像widget固有の初期化を証明しない。present成功やsprite submit完了も示さない。WAVとMP4の標準出力は空で、OBJもQtログを取得できなかった。ログは各captureの .console.log / .diagnostics.txt に保存するようcapture scriptを拡張した。\n- **追加ビルド確認:** 追加診断hookを一時的に試したが、CMake再生成後のArtifactUiTest buildは変更済みの NativeDockSurface.ixx にある未宣言メンバーで停止した。診断hookのC++変更は取り除いた。先に成功したArtifactUiTestバイナリで capture を継続した。\n- **追加3D確認（訂正）:** `temp/contents-viewer/model-os-active.png` にArtifactUiTest前面化後のOS desktop captureを保存。test.objの立方体がnative viewportに表示されている。以前のwidget captureだけで3Dを表示失敗とした判断を訂正する。
- **更新結論:** SVGはimageformats plugin未配置でdecode自体に失敗し、PNGとplugin指定後SVGはrenderer presentまで進んだ。画像のQt captureにはnative GPU surfaceが欠ける場合がある。OBJは前面化したOS captureで表示を確認し、3D表示失敗ではない。動画はfixtureにQt multimedia pluginがなく、plugin path指定時だけPlay後のH.264 frame表示を確認した。音声はdefault resamplerによりviewerのsink／waveform形式と合う。queue race仮説のruntime実測が残る。製品コードは変更していない。

- **2026-10-16 画像経路の実行時切り分け:** 一時的な環境変数gate付きログを用いて、SVGを通常のfixture環境で開くと `QPixmap` が `0x0` となり、画像読み込みの早い段階で停止することを確認した。出力先には `Qt6Svgd.dll` と SVG icon engine はあるが、Qt の `imageformats/qsvgd.dll` はなく、`Artifact.exe` と `ArtifactUiTest.exe` の通常実行フォルダーにも同プラグインは配置されていない。vcpkg側のimageformats pluginを `QT_PLUGIN_PATH` で指定して再実行すると、SVGは `640x360` として decode され、GPU image load成功、widget renderer初期化済み、swapchainあり、present成功 2/2・失敗0を記録した。PNGはプラグイン指定なしで `128x128` にdecodeされ、present成功を記録した。よってこの環境のSVG空表示は描画以前のQt image-format plugin未配置で説明でき、PNG/SVGのデコード後GPU経路もpresentまで進む。一方、キャプチャ画像はネイティブGPU子surfaceを含まないため、実デスクトップ上の最終ピクセル表示は未確認。
- **診断コードとビルド状態:** 計測用C++変更は一時的なもの。Ninjaの標準ビルドは、既存の未コミット NativeDockSurface.ixx にある未宣言メンバーで停止したため、対象モジュールだけを既存module mapで個別コンパイルし、fixture exeを直接linkして診断を実施した。ソースから一時ログを除去し、通常ソースへ戻す。
- **2026-10-16 動画／音声配布経路の追加確認:** 通常fixture exeのruntime directoryには `Qt6Multimediad.dll` とFFmpeg codec DLLがある一方、Qt `plugins/multimedia/ffmpegmediaplugind.dll` は存在しない。vcpkg package側には存在し、Artifact配布フォルダーには通常版 `plugins/multimedia/ffmpegmediaplugin.dll` がwindeployqtで配置されていた。`ArtifactUiTest` targetは `Artifact`へ依存しruntime DLLをコピーするが、`windeployqt`実行とQt plugin copyのPOST_BUILD commandはArtifact targetだけに付いている。従って独立テストfixtureはQt Multimedia backendの有無が製品Artifactの配布と異なる環境で起動している。A/B実行 `video-no-plugin-after-play.png` は黒画面、`video-uia-play.png` はH.264映像表示となり、Qt plugin配置がfixture動画表示を左右した。
- **2026-10-16 音声format確認（訂正）:** fixture WAVは1ch/24kHz/16bit・2秒。Contents Viewerのsinkとwaveform処理は2ch/44.1kHz/Int16を前提にする。`MediaAudioDecoder::initialize()` は codec初期化時に `setupResampler()` を呼び、明示resampling設定が無効でも既定出力を2ch/44.1kHz/S16に設定する。このためmono fixtureとのformat不一致という前回の見立ては誤り。初回pump時にreader queueが空なら直ちに停止するraceはコード上残り、既存のja/en/zh/zh-TW各fixture captureでNo audio dataが4/4観測されたため有力候補。ただしqueue状態、decoded byte数、device write結果は未採取で、直接因果は未確定。
- **2026-10-16 音声Play後の追加実行:** 既存 `ArtifactUiTest.exe` を起動し、UI AutomationでPlayを押して2秒後にOS画面をcaptureした。`audio-after-play-os.png` でも波形は `No audio data`、解析表示は `Audio analysis: idle`、時刻は `00:00.000` のまま。コード上、`activateAudio()` 自身もopen直後に `owner_->play()` を呼ぶ。`MediaPlaybackController::play()` はreader workerを非同期startし、viewerはその直後の同一UI呼び出し内で `pumpAudioPlayback()` を実行する。`MediaReader::getNextPacket()` はqueue空時に即nullptrを返し、`getNextAudioFrame()` は空QByteArrayを返す。viewerは一時的な空queueとEOFを区別せず、空なら `stop()`・sink suspendを実行してreturnする。従って初回packet到着前のpollが音声再生を打ち切るという因果経路があり、実行結果もそれと一致する。ただしqueue長／PCM bytes／sink writeの実値は未記録なので、直接原因確定にはこの計測が残る。
- **追加の入力・出力環境確認:** 同じWAVは付属ffmpegで `volumedetect` を通り、mean `-11.9 dB` / peak `-8.7 dB`、decode exit 0。fixture自体に有効な非ゼロ音声が含まれる。Qt Multimedia logging ruleを指定した既存アプリ実行ではstdout/stderrに情報なし。WindowsのAudioSrv / AudioEndpointBuilderはいずれもRunningで、USB / NVIDIA / Realtek / SteelSeriesの音声デバイスがOS上Status OKだった。これはfixture破損やサービス停止を否定するが、Qtが開いたdefault endpoint、`QAudioSink::error()`、実write byte数を示さず、sink側の失敗は未除外。
- **Profilerによる境界切り分け:** 再生後画面内のAudio Playback Profilerは `Scopes 3`。Profilerはprefix一致した各scope名の最新サンプルを集計する実装で、この処理経路にあるAudio scopeは `pumpAudioPlayback`、`fillPendingBuffer`、`getNextAudioFrame`、`audioSinkWrite`、`waveformAppend`。この実行では最初の3つが記録され、`audioSinkWrite`／`waveformAppend`は未記録だった。よってreader/controllerのpollまでは進んだが、PCMバッファ空判定でreturnし、sinkへのwriteを試みず、波形生成にも到達していない。sinkがSuspended/Stoppedとしてpump先頭でreturnした場合はfill/getNextAudioFrameのscopeが出ないため、その経路は今回の表示値と合わない。Profilerだけではqueue emptyとdecoder no-outputの区別はつかないが、fixtureは遅延のないPCM S16でFFmpeg decodeも成功しているため、初回packet未到着による空queueが最有力。queue長とdecoder出力bytes自体は未計測。
- **追加の音声表示不整合:** `appendAudioWaveformSamples()` と `updateAudioAnalysis()` は出力PCMを44.1kHz stereoとして処理する一方、表示用sample rateはmetadataの元音声24kHzを使用している。これは空波形の原因ではないが、PCMが届いた後の波形時間軸とFFT周波数表示を誤らせる可能性がある（未検証）。また波形はファイルopen時にdecodeせず、再生PCMをsinkへ書けた後だけ生成されるため、停止中の初期表示は空の設計となっている。
- **2026-10-16 音声修正:** `MediaReader::isStreamFinished()` と `MediaPlaybackController::isAudioStreamFinished()` を追加し、空poll時はreaderがまだ実行中なら再生を維持して次の16ms timer tickで再試行する。reader終了かつaudio queue空の時だけ再生をstopし、timerも止める。WAVのresample出力を44.1kHz stereo/Int16へ明示設定し、waveformとFFTもその出力sample rateを使う。
- **2026-10-16 独立fixture runtime修正:** Windowsの`ArtifactUiTest` POST_BUILDにも`windeployqt`、vcpkg runtime DLL、Qt platform pluginの配置を追加し、SVG imageformatsとFFmpeg multimedia pluginが通常Artifactと同じ実行フォルダーに入るようにする。
- **2026-10-16 音声runtime確認:** WAV fixtureを`ArtifactUiTest`から`ja` / `en` / `zh` / `zh-TW`で起動し、4キャプチャすべてで波形と解析値（RMS 18%、Peak 26%）を確認。ProfilerはScopes 5となり、`audioSinkWrite`と`waveformAppend`まで進んだ。従来の`No audio data`表示は解消した。短いfixtureのためキャプチャ時点では再生終了後に位置が先頭へ戻っており、進行中の時刻表示は別途操作試験が必要。
- **2026-10-16 Qt runtime配置:** `ArtifactUiTest`のCMake POST_BUILDにwindeployqt、vcpkg runtime DLL、platform、imageformats、multimedia pluginの配置を追加。windeployqtは既存環境の翻訳カタログとDebug Qt DLL探索警告を出すが、補助コピーで`qsvgd.dll`と`ffmpegmediaplugind.dll`の配置を確認。対象ビルドは終了コード0。
- **残る静止画／動画確認:** 配置済みpluginで再撮影したPNG/SVGはPreviewが空のまま、MP4は初期表示が白だった。MP4 fixtureは4秒のH.264としてFFmpegに認識されたが、このcapture入口は再生操作を行わないため、動画のPlay後表示は未判定。後続のネイティブ属性／renderer初期化診断を試したところ画像fixture起動が`0x80000003`で終了したため、その診断コードは撤回した。現ソースにはこの実験的な属性変更・診断は残っていない。画像GPU surfaceの故障箇所はまだ特定できていない。
- **3D確認:** OBJは以前の前面OS captureでnative viewportへの表示を確認済み。今回3D描画経路のコード変更はない。
- **ビルド上の注意:** SDK `rc.exe`をPATHに加えると対象リンクは進む。CMakeの初回再構成で別の同時ビルドとNinja compactionが競合し、一度再生成エラーとなったが、重複ビルド停止後の単一ビルドは終了コード0で完了した。POST_BUILDのwindeployqt警告は残る。


## 2026-10-16 PNG preview resize assertionの切り分け

- `ArtifactUiTest` のPNG fixtureでは画像decode、renderer初期化、最初のGPU描画・presentまで進む一方、直後のpreview widget resizeでMSVC Debug Runtimeの `vector subscript out of range` assertionが発生した。phase traceは `recreateSwapChain` 呼び出し直前まで記録し、呼び出しから戻らなかった。
- renderer初期化を遅延し、resizeを100ms debounceしても、swapchain physical sizeが160×160から163×163へ変わるケースで再現した。したがって遅延・debounceは修正にならず、原因は未特定。診断用コードとこの試行変更は取り除いた。
- PNG/SVGの可視表示は未合格。この時点では原因未特定だった。その後の確認と修正は、下記「PNG/SVG resize assertion修正と再確認」を参照。

## 2026-10-16 PNG/SVG resize assertion修正と再確認

- **再現原因:** `ArtifactIRenderer::present()` 後、command bufferは現フレームのbackbuffer RTVを次フレーム用に保持する。swapchain resize前に即時context側のrender target bindingとcommand bufferの保持参照を解放していなかった。
- **修正:** `recreateSwapChain()` でcontextのrender targetをunbindし、`cmdBuf_.setTargetRTV(nullptr)` を呼んでからswapchainをresizeする。画像preview側はresizeを100ms debounceし、physical sizeが変わった時だけ再生成し、resize中の再入paintを抑制する。
- **確認:** `ArtifactUiTest`を再ビルドし、独立fixtureでPNGを起動してOS desktop capture上のchecker表示を確認。45秒間の観測では複数回のresize後もrecreate/presentが完了し、assertionは再発しなかった。独立デバイス化の試作を外した共有renderer経路でもPNGとSVGの実画面表示を確認し、各fixtureは撮影までassertionなしで完了した。
- **ロケールfixture:** 共有renderer経路のPNG/SVGを`ja` / `en` / `zh` / `zh-TW`で各起動し、8件すべて撮影まで完了。widget captureはGPU画像面を含まないため、実表示の可視確認はOS desktop captureで代表ケースを確認した。
- **制約:** fixtureのQt widget captureはネイティブGPU surfaceを含まないため、PNG/SVGの画面表示判定にはOS desktop captureを使用。共有renderer経路で連続・異サイズの手動resizeを長時間行う試験、およびMP4のマウスドラッグシークは未完了。
- **次に確認:** Contents Viewer fixture上でマウスドラッグシークと複数サイズの連続resize stressを確認する。WAVの再生中positionとMP4の再生・キーボードシークは確認済み。

## 2026-10-16 Transport表示更新の切り分け

- **確認できた事実:** MP4をPlayすると映像フレームが変化し、WAVは波形・解析値とPlayback Profilerのscope 5まで進む。一方、両方のfixtureでtransport表示時刻は初期値の`00:00.000 / 00:00.000`のまま。
- **原因:** `updatePlaybackState()`先頭の`if (!stateLabel) return;`に対し、`stateLabel`には代入箇所がない。任意の状態ラベルがnullのため、生成済みの時刻・フレーム・範囲・比較ラベルと再生ボタン有効状態まで更新されず、すべて初期値のままになる。
- **修正:** nullならreturnする処理を削除し、個々のoptional labelは既存のnull guardで更新しつつ、transportとaction stateの更新を続行する。
- **修正と再確認:** `ArtifactContentsViewer::Impl::updatePlaybackState()` の先頭にあった `stateLabel == nullptr` による早期returnを削除した。`stateLabel` は初期値nullのまま代入されず、生成済みのtransport時刻・フレーム表示更新も同時に止めていた。ArtifactUiTestを再ビルド後、20秒WAVで `00:05 / 00:20` から `00:11 / 00:20` への進行を確認。4秒H.264 MP4は `00:00 / 00:04`、`Frame 0 / 120` を表示し、Play後は映像フレームとframe番号が進んだ。
- **追加修正と確認:** `QSlider::sliderMoved` のみを監視していたシーク入力を `valueChanged` に変更。独立MP4 fixtureでposition 0からスライダーへフォーカスしRightを押すと、slider value `1000`、transport `00:01 / 00:04`、frame `30 / 120` となり、再生位置が1秒へ移動した。コード側のposition同期は既存の `QSignalBlocker` で再帰seekを抑止している。
- **シーク確認:** MP4のviewer面のRightによる1フレーム送りと、seek sliderのRightによる1秒移動を確認。マウスドラッグの操作結果は未確認。
- **検証範囲:** PNG/SVG `ja`/`en`/`zh`/`zh-TW` の表示、MP4/WAV再生位置、MP4の1フレーム送りとキーボードシークを確認。長時間の連続異サイズresize stressと3Dモデルの独立操作は未完了。3D表示については先のOS desktop captureでOBJ立方体を確認済み。
