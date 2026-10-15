# FBX アニメーション独立ビューアテスト

**最終更新:** 2026-10-15
**ステータス:** D3D12／VulkanでFBX表示・MCP・独立metallic／roughness画像を確認済み。Core importer 6ケースpass。

## 目的

製品アプリ本体から起動を分けた小さなGUI実行ファイルで、FBXの読み込み、アニメーションクリップ検出、スキニング結果の表示を確認する。

## 現状

- **2026-10-15 材質マップ追加検証:** 自作のPhong／3ds Max PBR Metal/Rough cubeを`tests/fixtures/fbx-materials/`へ追加。DiffuseFactor画像がpacked metallic-roughnessへ誤分類される問題と、独立metallic／roughness画像がImporter結果に保持されない問題を、修正前に2ケースともfailとして再現した。Coreは`base_factor.texture`をMRへ割り当てず、明示PBR材質のmetalness／roughness画像とUV情報を独立slotへ保持する。Viewerにも独立slotを配線し、linear UNORM画像のRを読む。packed B/Gとは区別し、独立画像がある場合はCore rendererと同様にそちらを優先する。
- **受入結果:** Core importer全6ケース／CTestがpass。自作cubeのmetallic画像はR=51、roughness画像はR=204で、他のRGB成分は意図的に異なる。画像slot版と定数metallic=0.2／roughness=0.8版を同じcameraで描画すると、D3D12／Vulkanともviewport RGB MAE=0.0。途中で別材質へ切り替えて戻した場合も0.0。これは同じデータを再比較した検査ではなく、画像チャンネル指定と定数指定の異なる入力経路を比較している。
- **統合回帰:** 材質17操作と人体Viewerの既存15操作を同一processで続け、両backendとも32操作／11 PNGを完走。Diligent Error/Fatalは0件、Nathanの正面画像は前回基準とviewport画素一致。leftmost display／作業領域内とforeground HWND不変を確認。今回はcursor座標が変化していたため、cursor前後一致の検証には使わない。出力は `%LOCALAPPDATA%/Temp/ArtifactStudio/fbx-test/channel-regression-d3d12/`、`channel-regression-vulkan/`、比較値は`channel-regression-comparison.json`。新規のrepo内fixtureは自作cubeと1px PNGだけであり、配布不可人体モデルは追加していない。
- **2026-10-15 Quaternius Animated Human再確認:** 以前unsupported formatと記録した `quaternius-animated-man-pack/FBX/Animated Human.fbx` を現行Viewerで読み直したところ、ufbxでロード成功。1,578 polygons／4,734 vertices／41 bones／9 clipsを報告し、Walk (#7)の0.35秒と0.80秒をHWND直接capture。姿勢とboundsが変わり、D3D12 Error/Fatalは0件。captureは平坦なグレー素体で、このFBXファイル自体にtexture filename／画像参照は見つからず、テクスチャ確認用ではない。素材パック公式ページはAnimated／Textured、FBX、CC0を掲示するが、配布内容のFBXとBlend/Texturesの対応関係は別途整理が必要。出力は `%LOCALAPPDATA%/Temp/ArtifactStudio/fbx-test/quaternius-human-walk/`。背景窓capture中foreground HWNDは維持、cursor前後一致は未確認。
- **実装境界:** 独立slotのtexture生成は読込／path変更時だけ行い、frameごとは既存SRBへのbindとuniform flag参照だけ。descriptor用の作業領域は32要素のstack配列で、既存の最大mip数以内に収める。モデル差し替えとclearで独立mapをリセットする。現在のViewerの独立mapはUV0を使い、map固有のUV transform／UV1はCore材質データとして保持されるがViewerでは未対応。DiffuseFactor画像そのものをbase-color weightとして適用する機能も今回追加していない。

- **2026-10-15 検証結果:** `ArtifactCoreMeshImporterFbxTest`の4ケースがpass。FBX UV原点変換（source ufbx UV0／UV1との照合とtimed import）、skin pose変化、Nathanのembedded JPEG完全decode／UV、Standard_Walkのbase-color／normal texture・UV・material-slot assignmentを検査した。CTest登録経由もpass。Rangerに加え、Nathanのskin poseテストも直接実行してpass。
- **2026-10-15 表示修正:** FBX UVをdecoded image rowと同じtop-left原点へCore importerで正規化し、texture transformも同じ座標へ変換した（glTFのUVは変更しない）。ViewerではsRGB RTVへの二重gamma encodeを除き、clear colorをlinearにして渡す。Core meshのCCW頂点順に対してViewerがCW面を表としていた点を直し、頭部の皮膚が消えて眼球／歯が前後から見える表示を解消。同梱`rp_nathan_animated_003_walking_A.jpg`を目視基準として、衣服・肌・顔・靴のtexture配置と外面を確認した。照明・pose・cameraの異なる見本との厳密pixel parityではない。
- **2026-10-15 Diligent修正:** PSO static bindingに加え、モデル差し替え時のVB／IB作成を空のreplacement pointerで受けてから置き換える。初期fallback cubeから実モデルへの変更で出ていた`Overwriting reference to existing object may cause memory leaks`を解消。swapchainは実windowのphysical pixel寸法へresizeし、読込後のlayout変更による黒帯を防ぐ。Vulkan shared deviceには既存wireframe PSOが要求する`WireframeFill`をOPTIONALで要求する。Diligent forkは変更しない。
- **2026-10-15 Runtime証拠:** NVIDIA RTX 4070 Ti上でD3D12／Vulkanを別process起動し、それぞれ15 MCP操作（Nathan読込、0.8／1.5秒seek、正面／背面camera、Solid／Wireframe／Solid+Wire、Standard_Walkへ差し替え、5 PNG capture）を完走。両方のstderrはDiligent Error/Fatalが0件。最終同時刻の正面Solidのviewport RGB MAEは0.0/255、背面／別モデルSolidは0.000002/255未満、Wireframeは0.10016/255、Solid+Wireは0.00888/255。終了時のflush／release診断も含むログを保存した。
- **2026-10-15 非干渉確認:** captureは背面窓のHWND指定で取得し、両backendともforeground HWND／cursor座標が前後で一致。`window.onLeftmostDisplay`と`window.withinScreenWorkArea`が全stepでtrue、24E1N5500Bの作業領域内（frame x=-1440, y=144, width=1152, height=830）で実行した。長いstatus pathがテスト窓の最小幅を押し広げないよう、テスト側のstatus labelだけhorizontal size policyをIgnoredにした。全診断文字列はMCP stateに残る。
- **2026-10-15 初回描画:** 同一8K Nathan atlasを使うDebug Viewerのstartup読込で、GUI要求が初回描画を待つ時間を41.43秒から9.62秒へ短縮した（各1回の比較、steady-state FPSではない）。型名比較を画素loop外へ移し、sRGB byte decodeを256要素のstack lookupへ置換し、QVectorの書込pointerを事前に取得する。Core MeshRendererにも同じ型判定／byte decodeの改善を反映したが、Core製品renderの速度は未計測。texture／mipの解像度と色計算の方式は維持した。
- **保存先と範囲:** runtimeの`result.json`、`server-stderr.log`、PNGは `%LOCALAPPDATA%/Temp/ArtifactStudio/fbx-test/final-d3d12/` と `final-vulkan/` に保存。これらのPNGは配布不可モデルを含むため、モデル・textures・capturesをリポジトリへ追加しない。parent／Artifact／ArtifactCoreのtracked filesにNathan assetがないことも確認。今回の画面確認はstandalone Viewerであり、製品Composition／RTの最終画像、HDR表示、未試験の全FBXを保証しない。

- `Artifact3DModelViewer` と `MeshImporter` の既存経路を使う独立ターゲット `ArtifactFbxAnimationViewer` を追加した。
- **MCP・非アクティブ窓キャプチャ（実装済み・2026-10-15実機確認）:** テスト共通の `tests/ArtifactTestMcpServer.h` を追加し、改行区切りJSON-RPC stdio transportから各テストアプリのツールを登録できるようにした。`tests/Artifact/` と `tests/ArtifactCore/` など複数のテスト群から相対includeで再利用できる。tool名は空名・128文字超・ASCII英数字と`_`／`-`／`.`以外の文字を拒否し、handlerなし・重複名・起動後登録・object以外のinput schemaも拒否する。`tools/call` はnameとargumentsの型を検証し、空でないhandlerの`error` markerをMCPの`isError`へ反映する（Viewer状態の空`error`文字列は成功扱い）。MCP stdout切替に失敗した場合やサーバーを破棄する場合はdescriptorとstdoutを復元する。現行MCP `2026-07-28` の `server/discover` とper-request metadata（必須protocol version・client capabilities検証、応答server info metadata）、旧 `initialize` handshakeの両方に対応する。旧protocolの`ping`には応答し、modern protocolでは削除済みmethodとして扱う。FBX Viewerは `--mcp-stdio` で状態取得（既存診断ラベルのclip時刻を含む）、FBX読込、再生切替、clip選択、絶対時刻へのseek（seek後はpause）、shading切替、camera reset、画像captureを公開する。要求した絶対秒は状態・capture応答にも `requestedSampleTimeSeconds` として残す（clip範囲外ではViewer側でclampされる）。captureの待ち時間は`timeoutMs`（1〜60000、既定3000）で指定できる。handler例外はMCPツールエラーへ変換する。`tests/ArtifactTestWindowCapture.h` はWindows Graphics Captureの`CreateForWindow(HWND)`で指定対象を取得し、capture開始時にforeground化やカーソル移動をしない。`--capture-frames` も同方式を使う。window閉鎖時はcapture待機を解除し、capture中のresizeを検出した場合は1回再取得する。MCP captureは50ms後に別workerで取得を開始し、待機中は`QCoreApplication::processEvents()`でユーザー入力以外を処理してGUI render tickerを進める。新規signal/slot接続は使わない。worker開始失敗・capture失敗はツールエラーになる。Windows Graphics Captureは最小化・保護コンテンツ等のOS制約を受ける。Microsoft API資料はHWND単位captureを示すが、非アクティブ／最小化状態での画像内容を保証せず、Windows SDKのC++/WinRT header/APIとD3D12／Vulkan Viewerで、非最小化の背面窓からPNGを取得できた。最小化・保護コンテンツは未試験。各MCPクライアントのserver設定には、ビルド後の実行ファイルと `--mcp-stdio` を登録する必要がある。
- Viewer起動位置は左端ディスプレイの作業領域左上に置き、画面サイズに合わせてウィンドウ寸法を制限する。MCP/capture起動時は `WA_ShowWithoutActivating` を使い、前面化を避ける。
- **配布不可Nathanモデル:** DownloadsのZIPを `%LOCALAPPDATA%/Temp/ArtifactStudio/fbx-test/nathan-private/` へ展開し、プロジェクトにはコピーしていない。標準FBXのDiffuse embedded JPEGは8192×8192で、存在しない配布元source pathに代わりTemp cacheへ抽出・全画素decodeできた。ZIPにはnormal画像もあるが、標準FBXからnormal map接続は検出されなかった。normal-map試験には別fixtureのStandard_Walkを使う。

### MCPからの起動と操作

MCPクライアントのstdio server設定に、ビルドした実行ファイルと `--mcp-stdio` を登録する。一般的な `mcpServers` 形式の例:

```json
{
  "mcpServers": {
    "artifact-fbx-viewer": {
      "command": "J:/dev/ArtifactStudio/out/build/x64-Debug/bin/Debug/ArtifactFbxAnimationViewer.exe",
      "args": ["--mcp-stdio"]
    }
  }
}
```

起動後は `viewer.loadModel` にローカルFBX pathを渡し、`viewer.state` の `clips` からindexを選んで `viewer.selectClip` を呼ぶ。`viewer.seekTime` はclipの絶対秒を評価して停止し、続けて `viewer.capture` を呼ぶと非アクティブでもウィンドウ画像をMCP image contentとして返す。必要なら `timeoutMs` を指定する。操作中にポインターやアクティブウィンドウを動かす必要はない。

他のGUIテストでは `tests/ArtifactTestMcpServer.h` をincludeし、アプリ固有handlerを `registerTool` で登録する。画面画像は `tests/ArtifactTestWindowCapture.h` の `artifactCaptureWindow(HWND, ...)` を使う。これらはテスト側の共通helperで、製品アプリの制御経路には入らない。

共通CLI client `tools/test_mcp_client.py` でも再現できる。外部のsteps JSONには次のような配列を保存する（pathを手元のfixtureへ置き換える）。他のtest serverではtool名とargumentsを差し替える。

```json
[
  {"tool":"viewer.loadModel","arguments":{"path":"C:/path/to/model.fbx"}},
  {"tool":"viewer.seekTime","arguments":{"timeSeconds":0.8}},
  {"tool":"viewer.setCamera","arguments":{"yaw":28,"pitch":10,"zoom":1.6}},
  {"tool":"viewer.capture","arguments":{"timeoutMs":10000},"image":"pose.png"}
]
```

```powershell
python tools/test_mcp_client.py `
  --server J:/dev/ArtifactStudio/out/build/x64-Debug/bin/Debug/ArtifactFbxAnimationViewer.exe `
  --server-arg=--mcp-stdio `
  --steps "$env:LOCALAPPDATA/Temp/ArtifactStudio/fbx-test/steps.json" `
  --output-dir "$env:LOCALAPPDATA/Temp/ArtifactStudio/fbx-test/check" `
  --env ARTIFACT_FBX_VIEWER_DIAGNOSTICS=1 `
  --fail-log-pattern '\[DiligentViewport\]\[(error|fatal)\]'
```

`viewer.setCamera`はdegree単位のyaw `[-360,360]`、pitch `[-89,89]`、zoom `[0.05,100]`を受け、非finite値／型違い／範囲外を拒否する。stateにはcamera値、import backend、requested sample time、window位置と左端display／作業領域内の判定が残る。`backend`はImporter backend（ufbx等）であり、Diligent backendはstderrの`shared device acquired type=`で確認する。

独立material画像の回帰は、`create_material_fixtures.py`で自作データを再生成し、`separate-pbr.fbx`と`scalar-reference.fbx`をそれぞれyaw=28／pitch=10／zoom=1でcaptureする。初期zoomのcubeは下端に接触したため、全体が収まるzoomを明示した。保存した`maps.png`を次の既存画像比較ツールで判定できる（現在の1443×1039 capture寸法用）。D3D12／Vulkanともedge contact 0、detected modelあり、frames_failed 0だった。

```powershell
$materialCaptureDir = "$env:LOCALAPPDATA/Temp/ArtifactStudio/fbx-test/channel-regression-d3d12"
python tools/fbx/analyze_viewer_frames.py "$materialCaptureDir/maps.png" `
  --reference-image "$materialCaptureDir/reference.png" --viewport 12,118,1419,857 `
  --max-reference-color-mae 0.001 --max-reference-local-color-mae 0.001 `
  --output "$materialCaptureDir/analysis"
```

CLIは`result.json`へ各stepの結果・所要時間・画像path・desktop前後状態、`server-stderr.log`へ診断、PNGへ画像contentを保存する。tool error／timeout／指定log-pattern検出は非zero終了になる。`--timeout`は1 requestの待ち秒（既定90）。終了時はstdin EOFでserverを閉じ、5秒以内に終了しない場合だけ自分で起動したprocessをterminateする。PowerShellのProcessStartInfo／ReadLineAsyncを個別に組み立てる必要はない。private fixtureを使う場合はsteps／出力先もTemp等のrepo外に置く。

### 過去の作業記録

- Knightは既存ビルド済みViewerで全身が描画され、FFmpeg `gdigrab`で取得した約0.7秒間隔の画面では脚・足の画素形状とアニメーション時刻が変化した。詳細は後段の視覚検証記録を参照。
- KnightはQuaternius製の人型条件に合う一方、足先付近に装備が重なり、低ポリ・単色表示であるため、足向き確認用fixtureとしては読みやすさが不足する。
- 手元のQuaternius `Animated Human.fbx`（2,399,964 bytes、FBX 7400）を同じ既存Viewerへ読み込むと`unsupported format`で失敗した。以前の記録にあるViewer読込成功（4,734 vertices、41 bones、9 clips、Walk #7）をこのpathから再現できていない。別候補のUltimate Platformer `Character_HumanoidRig.fbx`もViewerはunsupported format、`Character.fbx`は人ではなくウサギと判明した。これらを足向き検証結果として扱わない。
- **現時点の選択:** Quaternius `LowPoly Animated Knight`を実画面animation確認素材として使い、テクスチャ確認は既存`Standard_Walk.fbx`を継続する。今回のKnight画面から足向きの正誤を判定する証拠は得られず、同一の読みやすいCC0人体FBXでanimationとtextureを一括確認する目的は未達。
- FBXはドラッグ＆ドロップ、起動引数、または `O` キーで開く。`Space` で再生／停止、`C` でクリップ切り替え、`R` でビューをリセットする。
- CMake option `ARTIFACT_BUILD_FBX_ANIMATION_VIEWER` は既定OFF。ONにするとテスト用GUI実行ファイルを構成する。既定のビルド先は `out/build/x64-Debug/bin/Debug/ArtifactFbxAnimationViewer.exe`。
- 2026-10-14に明示依頼を受けて構成・ビルドし、`animation_with_skeleton.fbx` を起動引数から読み込んだ。ログは `Model loaded successfully. Vertices: 4224`、viewport状態は1 clip (`ArmatureAction`)、再生中、14 bonesを報告した。
- ビルド時にFBX test utilityの未完了箇所、D3D12 HLSLの未対応`inverse()`、静的shader resource結合順、frame-scoped dynamic buffer更新を修正した。runtime後半のログには、shader compile・resource binding・dynamic allocationエラーが残らなかった。
- Qt `QScreen::grabWindow()`画像はD3D12 swapchain内容を正しく含まないため、viewport確認にはComputer Useの実画面キャプチャを使った。モデル表示と歩行中の姿勢差を目視確認した。
- Blenderリポジトリ内のFBX fixtureはGit LFS参照だけで、対象blobもサーバーから404だったため、Assimp `animation_with_skeleton.fbx` の作業用コピーを `out/build/x64-Debug/fbx-animation-test/` に置いた。レポジトリへ追加していない。
- `--capture-frames <directory>` で6枚のviewport画像を保存する診断オプションを追加した。D3D12 surfaceを正しく捕捉できないため、別のswapchain対応キャプチャ方式を選ぶ必要がある。
- 画面取得ツールで実画面を確認し、`animation_with_skeleton.fbx` は横長の細いメッシュ、GitHubのFBX viewerサンプル由来のY Botは人型メッシュとして描画されることを確認した。Y Botは112,296 vertices、104 bones、GPU LBS、2 clipsを報告する。
- Y Botのclip #1 (`mixamo.com`) は長さが0.03秒で、再生検証用として不適切だった。歩行アニメーションを含む`Standard_Walk.fbx`へ切り替え、clip #1 (`mixamo.com`, 1.63秒) で時刻表示と歩行姿勢の変化を確認した。保存フレームは `out/build/x64-Debug/fbx-animation-test/frames_validation/walk_final_*.jpg`。
- `--clip-index <index>` を追加し、複数clipを起動時に選択して調査できるようにした。
- 再生ごとにFBX全体を再importし、UI queueにtimer callbackも蓄積して応答なしになる問題を修正した。Viewerが`MeshImporter`を保持し、通常の骨アニメーションでは`updateSkinPose()`で既存meshのposeを更新する。姿勢配列を事前確保・再利用し、renderer側でも再利用バッファへコピーする。CPU fallbackは同じ変形を2回実行せず、timer callbackをcoalesceする。時刻／clip時間をステータスに表示し、30秒連続表示中の応答と時刻進行を確認した。
- FBXの先頭PBR材質のベースカラー、metallic／roughness等の値と、base color／metallic-roughness／normal texture pathをViewerへ渡すよう接続した。Mixamoの`Standard_Walk.fbx`は有効な画像を持つ一方、記録済みpathは配布元の一時Linuxパスだった。ImporterでFBX内のembedded image bytesを一時ディレクトリへ展開し、既存の画像読込経路から使えるようにした。実行ログに`Guard_02__diffuse.png`相当のbase colorとnormal textureの一時cache pathが出ること、および画面上で材質が描画されることを確認した。現在のテストViewerは複数material slotごとの描き分けには未対応で、先頭材質のみを全meshへ適用する。
- Reset Viewの初期カメラをyaw 28°／pitch 10°、距離4.0の3/4視点に変更した。再ビルド後、両足が画面端で切れないことを人型サンプルの複数walk cycle時刻で確認する。
- 既存候補では`Y_Bot.fbx` clip #0が歩行ポーズでなく、`Standard_Walk.fbx`は片足の装備差で判断しにくかった。代わりにOpenGameArtでCC0表記のあるQuaternius `Animated Human`を作業用ビルド出力へ取得し、FBXと同梱`License.txt`を確認した。4,734 vertices、41 bones、9 clipsで読み込めた。clip #7 `Human Armature|Walk`（1.03秒）を選び、D3D12画面の複数時刻で左右脚の歩行姿勢が変わることを確認した。clip #6 `Run`（0.63秒）、#2 `Death`（3.53秒）、#3 `Idle`（10.00秒）、#4 `HeroJump`（1.00秒）、#5 `Punch`（1.03秒）、#8 `Working`（6.43秒）も状態表示で識別した。アセットZIPと展開先は`out/build/x64-Debug/fbx-animation-test/quaternius_cc0/`以下で、リポジトリへは追加していない。
- 2026-10-14の再開時、既存 `out/build/x64-Debug-Agent` を使って `ARTIFACT_BUILD_FBX_ANIMATION_VIEWER=ON` で構成した。CMakeは`tests/Artifact/ArtifactFractureLayerRenderTest.cpp`欠落で失敗したため、ビューア作業では無関係な`ARTIFACT_ENABLE_OFFLINE_RENDER_TEST`だけOFFにして再構成した。MSVC/Ninjaでは`Shape.Types`と`Memory.SharedPtr`のIFC生成前に利用側が走る順序問題が再現し、`ArtifactCoreShape`と`ArtifactCore`を先行ビルドしてから専用ターゲットを再開した。最終的に `ArtifactFbxAnimationViewer.exe` のビルドは成功した。
- 同梱`License.txt`がCC0 1.0であるQuaternius `Animated Human`を`out/build/x64-Debug-Agent/fbx-test-assets/quaternius-animated-human/`へ取得した。ビューアの通常起動は実行時DLL不足（終了コード`-1073741515`）になったが、vcpkgの通常／debug `bin`を`PATH`に加えると20秒間プロセスが稼働した。`--clip-index 7`付きで起動したものの、起動ステータスを読める画面操作APIが利用できず、FBX読込・Walk再生状態は今回の再ビルドでは再確認できていない。
- 現在のcomputer-use端末では`sky` trusted RPCが未設定で、Windowsウィンドウを操作・撮影できなかった。組み込み`--capture-frames`も終了コード3で画像を出さず、既知のD3D12 swapchain capture制限もあるため、今回のビルドから視覚証拠は得られていない。以前の画面確認記録は履歴として残すが、今回の再ビルド後の受け入れ確認とは分ける。
- 2026-10-14、既存Viewerを`Standard_Walk.fbx`のclip #1で起動し、Windows.Graphics.Capture経由のライブ画面を複数時刻で画像確認した。時刻0.23／0.75／1.47秒相当で歩行姿勢が変化し、拡大表示では色付き衣服・防具のベースカラーが見えた。モデル外形の欠落、四肢の分断、明白なskin pose崩れは確認できなかった。Importerの一時cacheには1024x1024のPNGが2枚あり、1枚は色付きdiffuse atlas、もう1枚は青紫のnormal mapとして画像認識できた。D3D12画面の小さい標準ズームでは細部のテクスチャ異常を判定しにくいため、今後の手動QAでは拡大フレームを含める。これはStandard_Walkの該当clipの確認であり、全FBX・全clipの視覚保証ではない。
- `tests/ArtifactCore/MeshImporterFbxTest.cpp` にImporter契約テストを追加した。アニメーションテストは指定FBX内の各clipを開始前・開始・中間・終了・終了後の時刻で評価し、範囲外時刻が境界へclampされ、bone pose matrixと頂点positionが有限値のまま変化することを確認する。独立base-colorテストは埋め込み画像のTemp cache展開、PNG/JPEG signature、Renderer `ImageImporter`での全画素decode、finite UV／transform、material slot参照を確認する。完全textureテストはbase colorとnormalの両方について同じ契約を確認する。明示したfixture pathが不在ならGTest assertionが失敗する。
- Renderer `ImageImporter`による全画素デコードで、画像寸法・channel数・pixel buffer長の整合も確認する。PNG/JPEG headerだけ読める不完全データを通さない。
- 足向き確認用にQuaternius RPG Character Packの`Ranger.fbx`を`out/build/x64-Debug/fbx-animation-test/quaternius-rpg-character-pack/`へ取得した。配布元`License.txt`はCC0 1.0。FBX binary内に`Foot.L`／`Foot.R`と`CharacterArmature|Walk`／`CharacterArmature|Run`があり、足を含む人体アニメーションfixtureとして使える。SHA-256は`CECC642A3CEDCD319FE0CB07F71985E07C788FE6759EF1CDA2240A52EC1D14A9`。FBXからPNG/JPEG signatureと画像拡張子参照は見つからず、embedded texture fixtureとしては使わない。素材は`out/`配下でgit管理外。
- 公式Quaternius `Animated Man Pack`はCC0・FBX・animated・texturedとして公開され、公式説明はatlas textured maleとmany animationsを明記する。現行素材から`Animated Human.fbx`の読込・Walk再生は確認済みだが、このFBXは見た目がグレー素体で画像参照を含まない。パック内の`Blend/Textures`にある画像はClothedMan/Blendとの対応が示唆されるが、FBXへの割当は未確認。テクスチャ付き人体FBXとしての合格には至っていない。
- Quaternius公式配布の`LowPoly Animated Knight`を無料取得。`FBX/KnightCharacter.fbx`は2,092,236 bytes、FBX 7400、SHA-256 `41f2616fb4a42f9d35e242bbed1450ededc7537def4bf38cbb11b4e54ff2bf21`。公式説明はCC0・Walk／Run付き。ZIPとFBXにPNG/JPEG等のテクスチャ画像参照はなく、同梱`Preview.gif`（1000x1000、109 frames）は色付き騎士の歩行表示。
- 既存Viewerで2,058 vertices、31 bones、12 clips、ufbxを確認。`HumanArmature|Idle` #1を再生すると全身が描画され、約0.7秒差のviewport cropで両脚の画素形状とstatus時刻が変化した。単色表示では足先が小さく装備も重なるため、足の向き・接地姿勢の正しさは判定していない。
- 2026-10-14に既存Ranger ViewerをWindowsデスクトップキャプチャ（FFmpeg `gdigrab`）で撮影した。単一時刻で両脚が開いた歩行姿勢を含め3時刻の描画が確認でき、下腿と靴状の足形状は単色表示でも見える。一方、画面はCodexウィンドウに隠れていたためフレーム全体の連続比較ではなく、切出し位置も一定でない。Rangerにテクスチャがあること、足の解剖学的／接地姿勢が正しいこと、bottom-20%定量ROIの実モデル評価は確認していない。
- 既存ビルド済み`ArtifactFbxAnimationViewer.exe`でRangerを起動し、14 clips、#13 `CharacterArmature|Walk`、1.25秒、7,704 vertices、32 bones、ufbx backendとBoundsをstatus/logで確認した。6 frame capture間でstatus時刻は0.26秒から0.86秒へ進んだ。既存exeでのimport/playback確認であり、再ビルドやImporter GTest実行ではない。`QScreen::grabWindow()`の画像領域は白く、D3D12モデル描画を取得できなかったため、Rangerの見た目・足向きは未判定。
- 既存のQuaternius素体は外形確認に向くが色テクスチャを持たず、Standard_Walkはテクスチャがある一方で防具・片足装備が輪郭判定を難しくする。見た目が明瞭なモデル候補を探したところ、OpenGameArtの`Basic char textured animated`はCC0・低ポリ・diffuse/normal map・Run/Idle等を含むが、配布ZIP内は`.blend`とPNGだけでFBXは含まれない。ローカルにBlender実行ファイルもなく、今回のQAモデルには採用していない。FBXの形状/skin QAとテクスチャQAを同一モデルで行うには、FBX形式の明瞭なCC0素材を選ぶ必要がある。
- `tools/fbx/analyze_viewer_frames.py` を追加した（Python OpenCV / NumPyが必要）。固定 viewport crop の複数キャプチャから背景色を推定して前景を分離し、モデルbbox／画面端接触、前景占有率、彩度ピクセル率、隣接フレームのsilhouette IoUと前景面積比、遠隔foreground component面積をJSONへ出し、bboxとmask輪郭を重ねた一覧画像を保存する。テクスチャが期待されるfixtureでは`--min-saturated-fraction`、アニメーションfixtureでは`--expect-motion`を指定する。`--reference-image`は同じpose/timeの単一基準、`--reference-dir`は入力と同じファイル名を持つ時刻別基準画像群を使う。比較ではsilhouette IoU、前景BGR MAE、HSV histogram similarity、foreground bboxを16列x32行に分けた最大局所silhouette mismatch／BGR MAEを出す。各`--min-reference-*`／`--max-reference-*`引数で閾値を設定できる。異なるpose/timeやcameraを比較すると誤検出する。色histogramはUV配置全体を判定せず、局所色差と輪郭差は正常なpose差にも反応し得るため、matched frameでの回帰検査と目視triageに使う。
- 同ツールに、立位人型の画面で下端20%を足先付近として別集計する`reference_bottom_20pct_*`指標と、annotated preview上の位置線を追加した。これは方向の意味判定ではなく、基準画像と比べて下端領域の輪郭・色が変わった位置をtriageするもの。`--min-reference-bottom-silhouette-iou 0.80`を指定したときだけ足先付近の輪郭差を失敗扱いにする。
- この解析を保存済みのQuaternius `Human Armature|Walk` 3フレームに適用した結果、3/3フレームで下端接触を検出し、silhouette IoU変化は2区間で観測した。モデルの足が画面下端で切れ、これまでの「足が切れていない」という仮判断を訂正する。色付きtextureを期待する0.05基準では彩度ピクセル率0.0で不合格となり、素体FBXはテクスチャQA用fixtureに向かないことも確認した。旧スクリーンショットの観察は再ビルドを検証していなかったが、2026-10-15に現行Viewerから別キャプチャを取得し、Animated Human.fbx自体が無textureであることとWalk clip再生を確認した。
- 別の保存済みStandard_Walk 3フレーム（各status timeは約0.46／1.10／1.50秒、同一Solid viewport capture）では、画面端接触なし・輪郭変化2区間を検出し、annotated previewの目視でも人型の外形と四肢接続は保たれている。Solid表示のためtexture QAの証拠には使わず、clip内の外形変化を見る補助フレームに限定する。これは保存済み画像のみの確認で、Importer poseや全clipの保証ではない。
- 同じ解析で保存済み `Standard_Walk` の拡大viewport 1フレームは、bbox `[796,93,1047,769]`、画面端接触なし、彩度ピクセル率0.21505で0.05基準を通過した。viewport背景色を除いた前景は3つの離れたcomponent（合計面積率0.03434）も持つため、この値は診断に残すが、現段階ではそれだけで破損とは判定しない。これは色付きtextureが画面に見える証拠であり、UV mappingの正しさや複数時刻のskin品質までは判定しない。
- 画像指標の簡易校正として、Standard_Walkのviewport中のモデルbboxをグレースケール化した合成異常画像を作成し、0.05彩度基準が不合格（色変化フレーム0）になることを確認した。この意図的な単一変異に対する検出確認であり、一般的なUV歪みや局所texture欠落の検出率を示すものではない。
- Reference比較の校正ではStandard_Walkの同一画像を自身と比較して合格、同じviewport画像のモデル領域左半分を背景色で消した合成mesh欠損をsilhouette IoU 0.60095で不合格、モデル領域をグレースケール化した合成texture喪失をHSV histogram similarity 0.09797で不合格とした。大きい可視欠損／全面的色喪失の検出確認であり、微細なUV distortion・局所texture defectや本物のskin deformationを網羅しない。
- 16x32局所比較の校正では、モデル胴体に8／12／16／24／32／40／60 pxの単色patchを合成した。8 pxは最大局所BGR MAE 23.58で通過し、12 px以上は最大局所BGR MAE 52.73〜121.05で不合格となった。60 px時の全体BGR MAEは3.89、HSV similarityは0.86で、全体統計では目立たない局所色異常を検出できた。小さい細部には見逃し限界が残る。
- 足先ROIの基準確認では、同時刻のStandard_Walk matched frame 3枚がbottom-20% silhouette IoU 1.0で3/3合格した。JPEG quality 92へ再圧縮した同一フレームもIoU 0.99714／差分13pxで`0.80`閾値を通過した。足先帯を左右反転した合成異常はROI IoU 0.36048／差分4,272px、局所color MAE 33.625で不合格になった。これは表示領域指標の校正であり、Rangerの実FBX姿勢を画像確認した結果ではない。
- 単一referenceをStandard_Walkの別時刻へ使うと、正常なwalk pose差でも1/3枚がsilhouette IoU 0.77913で不合格になった。入力と同名の3枚のmatched referenceを`--reference-dir`で使った場合は同じ3枚が3/3合格した。時間の異なるposeを照合せず、capture timestamp／clip index等を含めた安定したファイル名で対応付ける。
- annotated outputs: `out/build/x64-Debug/fbx-animation-test/vision-analysis/fbx_frame_analysis.png` と `out/build/x64-Debug/fbx-animation-test/vision-textured-analysis/fbx_frame_analysis.png`。同一解像度のviewport-only framesを使う。全画面captureを使う場合は`--viewport X,Y,W,H`でviewer viewportを切り出す。現在の保存済みフレーム解析はCV指標と画像認識による目視の併用であり、自動semantic model-break detectorではない。
- Additional animation preview: `out/build/x64-Debug/fbx-animation-test/vision-walk-analysis/fbx_frame_analysis.png`。
- WindowsではテストターゲットのPOST_BUILDでruntime DLLを実行ファイルと同じディレクトリへコピーする。
- このテストは `ARTIFACT_ENABLE_FBX_IMPORT_TEST=ON` で登録する。fixtureはCMake cacheの `ARTIFACT_FBX_ANIMATION_TEST_FILE`、`ARTIFACT_FBX_BASE_COLOR_TEST_FILE`、`ARTIFACT_FBX_TEXTURE_TEST_FILE` で個別指定でき、同じFBXを使う場合は `ARTIFACT_FBX_TEST_FILE` を使う。base-color fixtureの既定値はtexture fixture。配布可能な既定fixtureはアニメーションに`out/build/x64-Debug/fbx-animation-test/quaternius-rpg-character-pack/Ranger.fbx`、embedded base color／normal PNGに`out/build/x64-Debug/fbx-animation-test/Standard_Walk.fbx`。手元の配布不可モデルを一時的なanimation/base-color fixtureにする場合は`$env:LOCALAPPDATA/Temp/ArtifactStudio/fbx-test/nathan-private/rp_nathan_animated_003_walking.fbx`を両方に指定し、texture fixtureには引き続き`Standard_Walk.fbx`を指定する。Nathan ZIPはプロジェクトへ置かない。
- 追加したテストは今回ビルド・実行していない。fixtureを指定した実行確認は未完了。

## テストループ確認マトリクス

| 分野 | 現在の経路／確認観点 | 状態 |
|---|---|---|
| アニメーション／足向き | clip一覧・指定clip・時刻進行・ループ端・Space停止／再開・Cでのclip切替。`Ranger.fbx`の`Foot.L`／`Foot.R`とWalkを重点確認 | 既存exeで#13 Walkの読込・再生時刻変化を確認。gdigrab画面に歩行姿勢3時刻が写り、少なくとも下腿と両足の形状を目視可能。ただし遮蔽された不均一な画面切出しで、同時刻reference比較・定量ROI・足先方向／接地の受入確認は未完了 |
| シェーディング | FBX先頭PBR材質のbase color、metallic、roughness等をviewerへ渡し、同じモデル／ライト条件で反射・陰影を確認 | 経路のコード接続あり。今回の見た目・scalar応答は未確認 |
| テクスチャ | base color、metallic-roughness、normal map pathとembedded image解決を確認。UV表示、欠落path、embedded画像のケースを分ける | `Standard_Walk.fbx`をembedded base color／normal PNG用fixtureとする。cache展開・PNG signature・Renderer `ImageImporter`全画素decode・finite UV／transform・material slot参照を検査するGTestは未実行。Rangerはテクスチャなし。CC0テクスチャ付きアニメーションFBXとして公式Quaternius Animated Manを特定したが、Google Drive HTTP 500で取得できずViewer受入未実施 |
| 複数材質 | material slotごとに異なる色／textureを使うFBXでpart別表示を確認 | 現在のviewerは先頭材質を全meshへ適用するため非対応。専用テストviewerの制限 |
| 入力・異常系 | 起動引数／Oダイアログ／drop、読み込み失敗、clipなし、Space／C／R | 起動引数の処理は実装済み。手動入力・異常系表示は未確認 |
| 安定性 | Walkを30秒以上再生し、UI応答、時刻進行、CPU LBS fallbackの負荷を確認 | 以前30秒連続表示の応答を確認。再ビルド後はプロセス稼働のみ20秒確認、負荷計測なし |

## 受け入れ確認

1. 独立実行ファイルをビルドし、FBXを起動引数・ファイル選択・ドラッグ＆ドロップの各方法で読み込める。
2. 読み込み失敗とアニメーションクリップがない状態を表示できる。
3. 起動時のclip指定、スキニングされたモデルの描画、時間経過による姿勢変化をswapchainを含むキャプチャで確認した。Space / C の手動操作は未確認。
4. 製品アプリのプロジェクトや設定ファイルを要求せず起動できる。

## 制限と次の確認

- テストは既存の `Artifact3DModelViewer` と製品描画ランタイムを利用するため、Importer／renderer自体を最小構成で分離するものではない。
- ufbxのscene評価を含む再生時フレーム時間とメモリ／CPU負荷は未計測。
- Space / C の手動入力、クリップ間の差、CPU負荷・フレーム時間は未確認。
- CPU LBS (8+ influences) fallbackのフレーム時間・負荷、blend shapeを含むFBXのtimed evaluationは未確認。
- **テスト実行方法:** `ARTIFACT_ENABLE_FBX_IMPORT_TEST=ON` と `ARTIFACT_FBX_TEST_FILE=<FBX path>`（またはanimation／base-color／full-texture個別path）をCMakeへ指定して構成し、`ArtifactCoreMeshImporterFbxTest` を実行する。GTestがない場合、fixtureが未指定・不在の場合はconfigureを失敗させる。CTestがfixture pathを各ケースの環境変数へ渡す。テストexeを直接起動する場合も環境変数を必須とし、未指定ならテストを失敗させる。
- **足向き視覚ループ:** 次はQuaternius Knight Walkを同一時刻・カメラ・viewportの複数フレームで撮影し、下端20%のROIとシルエットを比較する。今回のviewer screenshotでは全身が描画されて時間経過で脚が動くが、解剖学的な足向きの正誤は判定していない。Qt `QScreen::grabWindow()`はD3D12画面部分が欠けるため、今回のWindows画面captureはFFmpeg `gdigrab`を使用した。
- **追加確認（2026-10-14）:** ユーザーが `C:\Users\lagma\Downloads\Modular Character Outfits - Fantasy[Standard].zip` の取得成功を報告。Quaternius Standard版をignoredな`out/build/x64-Debug/fbx-animation-test/modular-character-outfits-fantasy/`へ展開。`License_Standard.txt`はCC0 1.0。`Male_Ranger.fbx`（1,648,444 bytes）は既存Viewerに読み込めるが`0 clip(s) | no animation clips found`で、衣装FBXのためwalk animation確認用にはならない。FBXに`T_Ranger_BaseColor.png`／`T_Ranger_Normal.png`の参照文字列があるものの、参照先は配布元作業環境の絶対path `C:/Dropbox/Work/Blender/...`。Viewerはbase color画像を解決できず、水色の代替表示となった。
- **左側サブディスプレイ視覚確認（2026-10-14）:** 既存Viewerを左側ディスプレイへ移し、Male_RangerのSolid／Wireframeを表示。静止A-poseでブーツのwireframe輪郭は連続し、目視できる明白な欠損・分離はない。ただしanimation poseではなく、足先形状とlightingの見え方を同一poseで切り分ける材料にはならない。足先の違和感はシェーディングだけとは断定できず、モデルの低ポリ形状とKnightの歩行姿勢・脚装備も候補に残る。
- **次に必要:** 既存Importerの6 fixture testとD3D12／Vulkan material regressionsは確認済み。残りはAnimated Man Pack内のテクスチャ割当があるFBX variantを特定し、同じ読みやすい人体でアニメーション／テクスチャ／足先を一緒に目視できるか確認する。Viewerの独立mapはUV0固定で、map固有UV transform／UV1は未対応。Space／C／RとOpen/drop、読み込み失敗／clipなし、CPU負荷・フレーム時間も引き続き未確認。

### MCPテストアプリへの接続

`tests/ArtifactTestMcpServer.h` はGUIテストアプリ側でツールを登録し、stdio transportを起動する小さな共通ホストとして使う。各アプリは自分の状態や操作に合わせたhandlerを用意する。handlerはGUIスレッドで呼ばれるため、Qt widgetやrenderer状態へのアクセスをそのまま記述できる。

```cpp
#include "../ArtifactTestMcpServer.h"

ArtifactTestMcpServer mcpServer(QStringLiteral("artifact-my-test"),
                                QStringLiteral("1.0.0"));
const bool toolRegistered = mcpServer.registerTool(
    QStringLiteral("test.state"), QStringLiteral("Read test state"),
    QJsonObject{{QStringLiteral("type"), QStringLiteral("object")} },
    [&window](const QJsonObject&) { return window.stateAsJson(); });
if (!toolRegistered || !mcpServer.start()) return 2;
```

ビルド済みのFBX ViewerをCodexのMCP設定へ登録する例（実行ファイルの場所はビルド構成に合わせる）：

```toml
[mcp_servers.artifact-fbx-viewer]
command = 'J:\dev\ArtifactStudio\out\build\x64-Debug\bin\Debug\ArtifactFbxAnimationViewer.exe'
args = ["--mcp-stdio"]
```

この設定はプロセス起動時にGUIを開き、MCPのstdioを制御路として使う。再現可能な静止フレームは `viewer.selectClip` → `viewer.seekTime`（絶対秒、評価後pause）→ `viewer.capture` の順で取得できる。`viewer.capture` はHWNDを直接captureして画像を返すため、アプリをforegroundへ移さずに状態取得・再生操作・キャプチャを行える。ユーザー設定へ登録する場合は、MCP対応ソースを含むexeをビルドしてから登録すること。既存のexeは今回のソースより古く、対応状況を確認できていない。
