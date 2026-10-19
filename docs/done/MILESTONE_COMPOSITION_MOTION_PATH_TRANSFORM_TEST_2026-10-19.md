**最終更新:** 2026-10-19
**ステータス:** ✅ Complete

# Composition Motion Path / Transform 独立統合テスト

## 目的

FBX Animation Viewer と同じく製品アプリから独立して起動するGUIテストで、Composition ViewportのMotion Path描画、Transform Position評価、Controllerのキー操作を固定fixtureで確認する。外部FBXや配布素材には依存しない。

## 実装

- `ARTIFACT_BUILD_COMPOSITION_MOTION_PATH_TEST=ON` で `ArtifactCompositionMotionPathTest` を構成する。`--mcp-stdio` を渡すとMCPからfixture状態、frame seek、回帰、captureを操作できる。
- 640×360、30 fps、90 frameのCompositionに静止画レイヤーを作り、Position keysをframe 0 `(120,180)`、30 `(320,100)`、60 `(520,180)` に配置する。Bezier補間とframe 30のspatial tangentで曲線を作る。
- 回帰ではkey／non-key／奇数frameのTransform値、Bezierが直線補間と異なること、Controllerがframe 15の評価済Positionをキー化すること、Undo/Redoを確認する。
- `motionpath.captureOverlayArtifacts` はMotion Path OFF/ONの同一Viewportを個別に撮り、二値差分mask、変更pixel数、外接矩形を返す。差分maskはピクセル寄与の画像で、構造化されたキー座標ではない。
- `tests/ArtifactTestImageArtifacts.h` に状態差分のtest-only helperを追加した。`ArtifactTestWindowCapture.h` は既存呼び出しとの互換性を保ち、任意captureでカーソルを除外できる引数を追加した。
- 過去のMotion Path key/sampleを選択・ドラッグしてもPlayback Serviceのcurrent frameを変更しない。独立テストはframe 45を維持したままframe 0のキーをドラッグし、frame 45のTransform評価が不変であること、Undoでframe 0のキーが戻ることを確認する。

## 検証結果

- Visual Studio 2026（Visual Studio 18）/ MSVC 19.51.36246、Ninjaで独立targetを `-j 2` でビルドできた。
- `motionpath.state`、`motionpath.runRegression`、`motionpath.captureOverlayArtifacts` の全ステップが成功した。Transform samplesはframe 0/15/30/45/59/60で有限値、端点とframe snapshotが一致し、Bezier中間値も直線補間からずれる。historical dragはframe 45と評価値 `(425,100)` を維持し、frame 0のPosition Xを120から約130.65へ変更、Undoで復元した。
- 取得した3画像を目視し、OFF画像には曲線がなく、ON画像にはBezier軌跡と接線があり、差分maskは曲線・接線領域を抽出することを確認した。最終結果は `out/build/motionpath-artifacts/result.json`、画像3枚は同ディレクトリの `motion-path-overlay*.png` にある（build成果物でありgit管理対象外）。
- MCP実行は画面を前面化せず、Windows Graphics Captureで独立テストウィンドウを撮り、CompositionViewport領域をcropする。cursor captureも無効にしている。最初に試した `captureCurrentFrameImage()` はデスクトップ重なりの影響を受け、viewport子HWND単体captureは空画像だったため、この経路を採用した。

## VS2026 / Ninjaの再現メモ

- opt-in build treeは `out/build/x64-Debug-Agent`。CMake cacheで `ARTIFACT_BUILD_TESTS=OFF` を確認し、NinjaをVS Developer Command Prompt（x64）から起動する。`VSLANG=1033` をconfigure/buildで設定し、`/showIncludes`の英語prefixとNinjaの依存スキャンを合わせる。
- 現行root `CMakeLists.txt` には、この独立targetのための親側ArtifactCore IFC `/reference` 補完がある。C++ module dyndepを広く再走査した後、GPU helperやEnvironmentVariable等でIFC名／出力pathが一致しないエラーが出た。子リポジトリは変更せず、該当module参照を親target設定で補って通した。CMake/Ninja生成方式が変わった場合は補完がまだ必要か再確認する。
- MSVC 19.51で `ArtifactCoreAudio.lib(AudioBassTreble.cppm.obj)` から `_Fill_vbool` のLNK2019が出た。テスト翻訳単位に限った互換shimと `/alternatename` が独立exeのlinkを通す。これはArtifactCoreの修正ではなく、MSVC 19.51固有のテストlink workaroundである。compiler更新時に不要か確認する。
- MCP GUIの起動にはVS Debug CRT、vcpkg debug/binとbin、Windows SDK x64 binをPATHに含め、Qt plugin pathを指定する。実行例は `tools/test_mcp_client.py --server <exe> --server-arg=--mcp-stdio --steps <json> --output-dir <dir>`。`--server-arg=--mcp-stdio` を忘れると通常GUIが立ち上がるだけでMCP応答がなく、timeoutする。
- fixtureはアプリTimelineの選択ブリッジを持たない。GUIイベント処理をpumpした後にテストレイヤーをControllerで再選択し、fixture操作を対象へ向ける。ProjectServiceの選択通知をWindow準備中に発行するとMCP初期化前に応答しなくなるため、この独立fixtureからは呼ばない。

## 既知事項・未検証

- 過去キーhit時の再生位置変更は修正し、テストで選択・ドラッグの経路を確認した。キーの選択・ドラッグはsampleのframeを編集対象として使い、Playback Serviceをseekしない。
- 自動合否はTransform数値、Controllerキー操作、画面captureと状態差分を検証するが、画面上の各pixel座標とTransform座標の対応そのものは機械判定しない。差分maskの画像分類安定性は今回の一つのVS2026/Windows実行で目視した範囲である。
- 共通paired-state helperを他のGUIテストへ適用した場合の再現性は未検証。
