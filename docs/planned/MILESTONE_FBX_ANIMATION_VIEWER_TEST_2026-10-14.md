# FBX アニメーション独立ビューアテスト

**最終更新:** 2026-10-14
**ステータス:** In Progress

## 目的

製品アプリ本体から起動を分けた小さなGUI実行ファイルで、FBXの読み込み、アニメーションクリップ検出、スキニング結果の表示を確認する。

## 現状

- `Artifact3DModelViewer` と `MeshImporter` の既存経路を使う独立ターゲット `ArtifactFbxAnimationViewer` を追加した。
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
