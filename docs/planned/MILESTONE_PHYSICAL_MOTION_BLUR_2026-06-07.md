# Physical Motion Blur Milestone

**最終更新:** 2026-11-06

## 現行状態の再判定 (2026-11-05)

- GPU `MotionBlurPass` は velocity/depth 参照、シャッター位相、rectangular / triangle / trapezoid の重み付けを行う。Composition preview と GPU Render Queue beauty の両方から呼び出される。
- shader は各出力 pixel の中心 velocity に沿って現在フレームの color/depth を再サンプルする。これは velocity-guided spatial reconstruction であり、シーンを shutter 内の複数時刻で再描画する temporal accumulation ではない。オクルージョン・alpha 境界や速度勾配の品質は画像受入が必要。
- `tests/Artifact/PostProcessPassContractTest.cpp` の小画像visual fixtureは、移動色pixelだけでなく隣接pixelにも速度を手動設定して軌跡を模擬する。これは実scene velocity passの背景（neutral velocity）を再現しないため、実物体coverage外のtrail生成やalpha境界拡張を証明する受入結果として扱わない。
- 実物体coverage外trail用に、8x8 tileあたり速度方向quadrant別のsource-pixel indexをseedし、tile-grid jump-flood candidate propagationと背景合成branchを実装した。通常gatherのdepth edge判定は維持する。tile内で同quadrantの複数trajectoryが競合する場合やcandidate pathの穴、遮蔽、画像品質、dispatchコストはruntime未確認で、受入済みとは扱わない。
- 通常のpixel gatherの深度判定と既存depth-edge contractは維持する。coverage外trailは、coverage・source pixel・signed velocity・source depthが揃った候補に限定した別の前景寄与として背景へ重ねる必要がある。全tapのweightで正規化し、背景側の露光時間を保持しないと、少数の前景sampleが不透明な帯になる。近い遮蔽物は候補を拒否する。
- custom shutter curve、velocity/coverage に基づく adaptive sample count、preview/final 固有の品質ポリシーは未実装。
- Render Queue の physical velocity pass は 3D mesh に限定される。2D image/text/shape の layer-local 2.5D copy blur は独立機能で、Velocity AOV と physical post blur の入力にはならない。
- camera blur と timeline blur は別設定だが、同時有効時は同一 velocity/depth で順に二回積分する。意図した合成露光か重複かは仕様・視覚比較待ち。
- GPU shader compile、preview / Render Queue parity、連続・選択範囲・MFR・stereo・encoded video、性能計測は未検証。リポジトリ指示により、この作業ではビルド・テストを実行していない。
- Motion blur用candidate ping/pong scratchは`AuxiliaryTargetRequest`経由でpreview/Render Queueに要求され、RenderPipeline initialize/resizeで確保・destroyで解放される。Raw Velocity AOVだけの要求では確保しない。形式はtileあたり速度quadrant別4候補を持つ`RGBA32_UINT`、寸法はceil(width/8)×ceil(height/8)。seed・jump-flood・coverage外resolveへの接続は実装済みだが、shader compile、画像品質、GPU時間、MFR時の複製メモリは未検証。

## 2026-11-05 実装追記

2.5D レイヤーモーションブラーの前フレーム差分評価を composition の絶対フレーム基準に統一した。
Render Queue の `goToFrame()` はレイヤー相対フレームを保持するため、相対値を
`getGlobalTransformAt()` に渡すと in-point / start-time があるレイヤーで誤った速度になる。
現在の composition frame を取得し、前フレームと現フレームの global transform を比較する。
コード静的確認のみで、オフライン出力・MFR・プレビューの実描画は未検証。

Render Queue の Timeline Motion Blur 設定（有効、angle、phase、sample count、shape、
trapezoid plateau）は job 開始時に `FrameRenderSnapshot` へ値コピーし、全フレームと
MFR worker が同じ設定を使うよう固定した。これはフレーム途中の設定変更で一つの出力動画に
異なるシャッター設定が混ざることを防ぐ。静的な配線確認のみで、連続/MFR出力は未検証。

Mesh velocity shader は previous clip W が epsilon 以下、または投影後velocityが非有限なら、
coverage alphaを1に保ったまま中立速度を出すようにした。前フレーム位置がカメラ背面／投影面上に
あるとき、正のepsilonで割って巨大速度を作る挙動を避ける。camera-plane crossingの画像結果と
通常移動の回帰は未検証。

`Artifact3DLayer` の複数material-slot instanceが前フレームtransformに現在値を複製していた
ため、アニメーションしている multi-mesh/USD layer の velocity がゼロになるケースを修正した。
各instanceの previous transform に、layer の previous transform と同じ mesh-local transform
を合成して渡す。速度AOV・motion blur出力のruntime確認は未実施。

MotionBlur shader の `sampleCount == 1` を無条件 pass-through にしていた分岐を修正した。
phase が非ゼロなら、1サンプルは露光位置 `t = phase` のサンプルを意味するため、velocity に沿った
単一時刻サンプルを行う。phase=0 の1サンプルと angle=0/phase=0 は exact copy として早期終了する。
GPU契約テストの実行は未実施。

深度ウェイトの対称差分判定をD3D depth (near=0, far=1) に沿う片方向判定へ変更した。
中心pixelより遠いサンプルは拒否し、同深度または前景サンプルだけ許可することで、背景色が
前景輪郭へ逆流するのを避ける。coverage外trailはvelocity dilation未接続のため未解決。
shader/runtimeでの遮蔽テストは未実施。

> 2026-06-07 作成

## 目的

モーションブラーを「雰囲気のためのぼかし」ではなく、
**サブフレーム運動を積分して得られる物理ベースの時間的露光表現**として扱う。

このマイルストーンでは、以下を同時に満たすことを狙う。

- GPU ベースで高速に評価できること
- サブフレームの動きベクトルを正確に積分すること
- プレビューでは簡易版を軽く表示し、本レンダリングでは高精度版を使えること
- シャッター角度だけでなく、開口形状も調整できること

## 背景

現状のモーションブラーは、機能の入口は見えているが、制作現場で期待される
「カメラの露光として自然な残像」までは届いていない。

特に不足しているのは次の 4 点。

- レンダリングが重く、ON にすると使いにくい
- サンプル分布が単純で、残像の質が人工的
- シャッター角度を変えたときの表情が荒い
- プレビューと最終レンダリングの整合が弱い

## Goal

- GPU 上で motion vector を積分する物理モーションブラーを実装する
- シャッター開口を `angle` だけでなく `triangle / trapezoid / custom` 形状で扱えるようにする
- `preview` 用の簡易ブラーと `final render` 用の高精度ブラーを分離する
- 速度に応じた可変サンプルや重要度サンプリングを使い、重さを抑える

## Scope

- `Artifact/src/Effect`
- `Artifact/src/Composition`
- `Artifact/src/Widgets/Render`
- `Artifact/src/Widgets/Timeline`
- `ArtifactCore/include/Graphics`
- `ArtifactCore/src/Graphics`
- `Artifact/shaders`
- `docs/planned/MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md`
- `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`

## Non-Goals

- CPU での全フレーム総当たり積分
- 単純な Gaussian / directional blur だけでモーションブラーを名乗ること
- 既存の再生・キャッシュ・輸出経路を壊すこと
- モーションブラー以外の時間系エフェクトを一度に全部作り直すこと

## Design Principles

- 「見た目の近似」より「露光の意味」を優先する
- preview は軽さ、final は正確さを優先する
- motion blur は layer state ではなく、motion sample と shutter profile の組み合わせで考える
- シャッター形状は角度の派生量ではなく、時間重みの分布として扱う
- 高速化は省略ではなく、サンプル計画と GPU 実装で達成する

## Functional Requirements

### 1. GPU Physical Integration

- サブフレームの motion vector を使って、露光区間を積分する
- 静止画に対する後段 blur ではなく、時間サンプルの加算で作る
- velocity / depth / occlusion を使って、破綻したにじみを抑える

### 2. Shutter Profile Control

- `shutterAngle`
- `shutterPhase`
- `sampleCount`
- `shape`

  を持つ

- `shape` は少なくとも次をサポートする
  - `rectangular`
  - `triangle`
  - `trapezoid`
  - `custom`
- `custom` は時間重みカーブとして定義できるようにする

### 3. Preview And Final Split

- timeline / composition viewer では簡易版 preview を表示する
- render/export では高精度版を使う
- preview は低サンプル数でも動きの方向と重さが分かることを重視する
- final は残像の形と密度を優先する

### 4. Quality And Performance Policy

- sample count を固定値だけでなく、速度や画面占有率で調整できるようにする
- 遅い対象は LOD / partial update / cache を使って負荷を下げる
- 露光が短い場合はサンプルを減らしても自然に見えるようにする

## Phases

### Phase 1: Motion Sample Model

- 目的:
  - motion blur の入力を明確にする

- 作業項目:
  - motion vector / depth / occlusion の入力契約を整理する
  - shutter profile の表現を定義する
  - preview と final の品質レベルを分ける

- 完了条件:
  - 何を積分するかをコード上で説明できる
  - 既存の layer / composition state と競合しない

### Phase 2: GPU Accumulation Path

- 目的:
  - 物理積分を GPU で行う

- 作業項目:
  - サブフレームサンプリングの描画/蓄積パスを作る
  - velocity に応じた sample distribution を実装する
  - オクルージョンや深度破綻の抑制を入れる

- 完了条件:
  - 高速移動の残像が極端に汚くならない
  - sample count の増加が結果に反映される

### Phase 3: Preview Blur Path

- 目的:
  - 編集時に軽い見た目を返す

- 作業項目:
  - 低サンプル・低負荷の簡易ブラーを用意する
  - UI 操作中は preview 設定へ切り替える
  - 本レンダリングと見た目が大きく乖離しないようにする

- 完了条件:
  - preview で motion blur の有無と方向が把握できる
  - 編集操作のレスポンスを阻害しない

### Phase 4: Shutter Shape Editing

- 目的:
  - シャッターの開閉形状を表現できるようにする

- 作業項目:
  - triangle / trapezoid / custom curve を実装する
  - UI から shape を選べるようにする
  - 角度・位相・形状の関係を整理する

- 完了条件:
  - 角度だけではない残像の質を調整できる
  - camera-like な表情差を作れる

### Phase 5: Performance Tuning And Caching

- 目的:
  - 実運用で使える速度に寄せる

- 作業項目:
  - adaptive sampling
  - early-out
  - cache reuse
  - ROI / partial update の見直し

- 完了条件:
  - ON にしても作業継続可能な応答性を維持する
  - heavy shot でも破綻しにくい

## Related Milestones

- `docs/planned/MILESTONE_MOTION_BLUR_2026-03-29.md`
- `docs/planned/MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md`
- `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`
- `docs/planned/MILESTONE_MOTION_NEXT_2026-06-02.md`
- `docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`
- `docs/planned/MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md`

## Acceptance Checklist

- GPU ベースの時間積分として説明できる
- preview と final の経路が分かれている
- shutter angle 以外の形状制御がある
- 高速移動時の見た目が単純 blur より自然である
- 編集中の重さが作業を邪魔しない

## 2026-07-28 実装着手

判定: Phase 1 完了、Phase 2 実装中。モーションブラー固有の責務を `RenderPipeline` へ密結合させず、独立した `MotionBlurPass` として接続した。

入力契約:

- `color`: 現在の合成色バッファ
- `velocity`: 画面空間速度バッファ
- `depth`: 深度バッファ（オクルージョン抑制用）
- `output`: 作業用の別バッファ
- `MotionBlurSettings`: enabled / shutter angle / phase / sample count / quality

責務分離:

- `RenderPipeline`: 色・速度・深度・作業用テクスチャの提供
- `MotionBlurPass`: compute PSO、リソースバインド、ディスパッチ、サンプル積分
- `ArtifactCompositionRenderController`: パスの実行順と preview/final の設定選択

現段階では `motionblurCS.hlsl` の既存実装を直接 `RenderPipeline` に埋め込まず、独立パスから再利用できる形へ整理する。Phase 2 の完了条件は、速度・深度を入力に取り、作業用バッファへ書き出して合成バッファへ戻せることとする。

実装:

- `Artifact/include/Render/ArtifactMotionBlurPass.ixx`
- `Artifact/src/Render/ArtifactMotionBlurPass.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`MotionBlurPass` は色・速度・深度・出力を入力契約とし、速度に沿ったサブサンプルを深度差で重み付けして GPU compute で積分する。成功時だけ `RenderPipeline::swapAccumAndTemp()` を行うため、パス失敗時は従来の resolve にフォールバックする。

この時点で未完了: shutter shape（triangle / trapezoid / custom）、adaptive sampling、preview/final の品質差、実機での shader compile / runtime 検証。

ビルド・実行確認はリポジトリ方針により未実施。

## 現行コード監査 (2026-08-15)

`ArtifactMotionBlurPass` が現行ツリーに存在し、color／velocity／depth／output の入力契約、velocity に沿うサブサンプル、深度差による重み付け、GPU compute 実行、失敗時の従来 resolve fallback を実装している。したがって、前回監査の「専用 physical accumulation pass は未接続」という判定は更新が必要で、Phase 1〜2 の GPU pass 基盤は静的に存在する。

一方、shutter shape の triangle／trapezoid／custom、adaptive sampling、preview／final の品質ポリシー、実際の全 composition path への安定接続、shader compile／実機性能・画質検証は未完了である。既存の `ArtifactMotionBlur` や `TimeRemap` の簡易 blur と、physical pass の機能パリティも未確認とする。

判定: **GPU physical motion blur pass と velocity/depth 入力基盤は部分実装済み。shutter profile、品質制御、全経路統合、runtime 検証は pending。**

## Next Step

真のshutter-time accumulationへ進む前提として、時間評価の入口を設計する。`FramePosition`とComposition / Layer `goToFrame()`は整数フレームだが、`getGlobalTransform4x4At(RationalTime)`には限定的な明示時刻経路がある。ただし3D transform向けで、2D branchは整数frameへ変換し、シーン全体・component/physics stateを評価するものではない。`Artifact3DLayer::draw()`もinteger `currentFrame()`でtransformを構築し、skin animationは共有meshを`setAnimationTime()`で更新しているため、各サンプルで同じlayerを再評価する方式は順序依存・共有状態汚染の恐れがある。Composition seekはphysics / component simulationにも触れる。PreviewのN-1 pre-rollは履歴効果用の整数フレーム描画で、サブフレームsample蓄積ではない。現行フレーム描画を丸めた時刻で複数回呼ぶだけでは要件を満たさない。property/keyframe evaluator、layer-local time conversion、skin pose出力、simulation snapshot/restore、rendererの中間出力保持を調査し、副作用を隔離したfractional-time sample contextをpreviewとRender Queueで共有できる境界を決める。

Render Queue の 3D velocity target は clear alpha 0、通過したmesh fragment alpha 1で書く。Opaque / alpha-masked range はblend無効・depth write有効で、mask holeはpixel shaderでclipされる。Blended rangeはblend有効・depth write無効、velocity shaderの出力alphaは1固定なので、このalphaを透明度や厳密な被覆率には使えない。`MotionBlurPass` はvelocityをfloat4で読み、coverage alphaをcandidate seed時に利用する。

legacy tile-max shadersはsigned `float2`の最大／最小速度のみを集約し、現行AOVの`[0,1]` encoding、coverage alpha、発生元depthと契約が異なるため、そのまま再利用できない。Timelineのshutter angleは0–720度、phaseは±360度で、passの時刻式では最大2 frame離れる。投影速度に明示上限はないが、出力先は画面内に限られるため候補source探索の正しさ上限は画面対角、単純な全画素探索は実用的でない。

candidate seedは各8x8 tile内のcoverage付きvelocityを走査し、方向quadrant別に最大移動量のsourceを一つ保持する。ceil(width/8)×ceil(height/8) gridのjump-floodは各tileで9近傍×4方向候補を検査する。同quadrantに複数物体が入る場合は候補を落とし得る。次の受入では実mesh coverage、moving foreground over background、近い遮蔽物、alpha mask、tile境界をまたぐ交差軌跡を含む短い動画を出力し、候補の穴・誤伝播を確認する。エンコード経路の差とblur差を分けるため、同じ範囲を連番画像でも出力して比較する。続けて先頭・中間・末尾、連続出力と選択範囲、serialとMFR、previewとRender Queueを比較し、フレーム欠落・重複、ちらつき、輪郭漏れ、フレーム境界の速度誤りがないことを確認する。RGBA32_UINT scratchのGPU時間・VRAM量を1080p/4KとMFR worker数別に計測する。Blended materialは別途仕様化する。

## 2026-07-25 実装監査

判定: Phase 1〜5 は未着手。既存の velocity / motion blur 基盤はあるが、物理的な時間積分までは接続されていない。

- `ArtifactMotionBlur` の既存処理は Directional / Radial / Zoom / Velocity / Camera / Transform 系の汎用エフェクトであり、物理的なサブフレーム蓄積そのものではない。
- Render 側には velocity texture / SRV / resolve と 3D mesh の velocity-only pass があるが、専用の physical motion blur accumulation pass、深度・オクルージョンを考慮した temporal resolve は確認できない。
- shutter angle / phase / sample count と、rectangle / triangle / trapezoid / custom の shutter profile を扱う実装は確認できない。
- preview / final の経路分離、adaptive sampling、early-out、cache reuse、ROI 更新も未接続。
- したがって、現状は後続実装の前提となる velocity 基盤と簡易 blur 基盤まで。次の実装単位は、まず motion sample のデータ契約と shutter profile の定義。

ビルド・実行確認はリポジトリ方針により未実施。

## 2026-11-05 Velocity / shutter phase contract correction

静的なコード照合で、Composition の Velocity AOV は画面 UV/frame 差分を `[0, 1]` に符号化して保存していることを確認した。`ArtifactMotionBlurPass` はこれを符号付き UV/frame 差分へ復号し、dispatch の幅・高さで pixel/frame に変換してサンプル位置を求める。静止速度の AOV 中立値は `(0.5, 0.5)`。

Preview 設定の shutter phase は度数で保持されるため、Composition renderer から pass へ渡す際に `degrees / 360` へ正規化するよう接続した。GPU pass 契約 fixture も production AOV と同じ速度符号化を使い、静止・水平・垂直・回転ベクトルを検査するよう拡張した。

さらに pass の公開設定値に NaN / Inf が渡された場合、GPU 定数へ伝播させず shutter angle は 180 度、phase は 0、velocity scale は 1 へ戻す。GPU 契約テストにこの有限値フォールバックの比較ケースを追加した（実行は未確認）。

矩形を既定値として維持したまま、三角形と台形の shutter profile を pass に追加した。三角形は露光中央を最大、両端をゼロとし、台形は `trapezoidPlateau` で中央の一定重み区間を指定する。GPU 契約テストには矩形より三角形の中央寄与が強く、台形がその中間となる比較を追加した（実行は未確認）。custom curve と adaptive sampling は未実装。

Preview Settings に shape 選択と trapezoid plateau を追加し、`ArtifactAppSettings` へ保存する。Composition controller は同設定を Timeline Blur と Camera Blur の pass に渡す。UI / 保存 / GPU 実行は未検証。

Render Queue 統合監査では、GPU の通常 beauty path に Timeline / active Camera の `MotionBlurPass` を接続した。velocity target は既存 `RenderPipeline` から確保し、beauty 描画後に 3D mesh の velocity-only pass を実行する。MFR worker は pass を個別所有する。AOV-only 要求では blur を有効化せず、multi-channel の beauty だけを blur し、velocity AOV は raw 値を維持する。deep export は blur をスキップする。

Image / Text / Shape の GPU draw は既存 `twoPointFiveDRenderPasses()` を使い、レイヤー設定に応じた前フレーム変換 delta の複製 blur を行う。これは物理 velocity AOV による post blur とは別の近似であり、Render Queue の `VelocityX/Y` には 2D / 2.5D motion vector を含まない。両方の blur を使う出力は別ケースとして視覚比較が必要。

Render Queue は current / previous active-camera view・projection matrix を GPU frame path の beauty 描画と velocity-only redraw の両方へ渡す。shake 時刻は Composition View と同じフレーム基準で評価し、stereo は左 eye view を使う。shader compile / GPU 実行および選択フレーム、MFR、stereo の結果は未確認。

### Render Queue / video acceptance matrix (pending runtime verification)

| Case | Setup | Expected result |
| --- | --- | --- |
| Static hold | Static 3D and 2D layers; blur enabled | No visible trail or frame-to-frame flicker. |
| Object motion | Translate and rotate a 3D mesh at constant speed | Trail follows the measured screen-space velocity; length scales with shutter angle. |
| Camera motion | Move, rotate, and shake the active camera while a 3D mesh is static | Beauty framing and velocity direction follow the same camera matrices; no first-frame streak. |
| Occlusion / alpha | Overlapping meshes, masked material, transparent edges | Foreground depth rejects background color; alpha boundaries do not leave solid rectangular trails. |
| 2.5D layer blur | Enable the layer-local 2.5D blur, then compare Timeline blur on/off | Existing transform-copy blur remains distinct; Timeline post blur must not be mistaken for it. |
| Frame dispatch | Render sequential frames, a selected range starting mid-shot, and MFR | Same requested frame is stable across dispatch modes; no worker-start history artifact or cross-frame contamination. |
| Multi-channel EXR | Request Beauty plus VelocityX/Y and Depth | Beauty is blurred; Velocity and Depth retain raw channel semantics and match the unblurred render. |
| Deep EXR | Enable deep output with blur settings active | Deep samples and beauty stay mutually consistent; current policy skips physical blur. |
| Encoded video | Compare an image-sequence render with extracted frames from the encoded video | Frame count, ordering, duration, and motion-blur timing agree; account for codec loss separately from render differences. |
| Stereo | Render a moving camera in stereo mode | Left-eye path is stable and matches the configured output contract; verify whether right-eye export is required. |

Capture the same frame numbers from the unblurred reference, GPU Render Queue, and encoded video. Record backend, adapter, resolution, frame rate, shutter angle/phase/shape, sample count, frame-range mode, and MFR worker count with each result. Run every output case in both `ja` and `en` only when the tested artifact includes UI capture; image/video render comparisons themselves are locale independent.
