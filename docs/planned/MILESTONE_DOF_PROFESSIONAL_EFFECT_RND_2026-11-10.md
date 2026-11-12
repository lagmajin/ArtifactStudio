# Professional Depth of Field Effect R&D

**最終更新:** 2026-11-10
**ステータス:** In Progress

## 目的

被写界深度（Depth of Field, DoF）を、単にピント外をぼかす機能から、制作で信頼して使えるレンズ系エフェクトへ育てるための R&D プロジェクト。画質、深度の扱い、時間安定性、操作の予測しやすさ、GPU 負荷を、再現できるシーンと測定可能な合否条件で評価する。

この文書は研究・受入プロジェクトの起点であり、現行レンダラーの置換や新しい UI／機能の即時実装を承認するものではない。調査結果から実装候補を絞り、変更は後続の明示された実装段階で行う。

## 現行実装の確認済み範囲

- `Artifact.Render.DepthOfFieldPass` は GPU compute pass として存在する。非線形 window depth を view depth に戻し、線形 ramp または thin-lens 式から CoC 半径を求め、Golden-angle spiral の固定サンプルでカラーを集約する。foreground bleed を抑える深度重み、sample count／CoC の clamp、in-focus pixel の保持を実装している。
- `ArtifactCompositionRenderController` は active camera の DOF 要求、camera post-depth SRV、scene color／output を使う 3D composition 経路に pass を接続している。
- camera controller は full-resolution preview で 16 sample、downsample preview で 8 sample を指定し、最大 CoC 半径は 16 output pixels に固定している。これらは設定値ではなく現行実装の固定値である。
- `ArtifactCameraLayer::depthOfFieldParameters()` は `aperture` 値を `apertureSize` として返す一方、controller は同じ値を `DepthOfFieldSettings::fStop` に渡す。shader の thin-lens 式は `fStop` を分子に掛ける「aperture scale」として扱い、大きい値ほど CoC が増える。UI の `FStop` 単位名と光学的な f-number の一般的な意味（数値が大きいほど開口が小さい）との整合は未確定で、R&D の最優先仕様課題とする。既存テストも fStop 増加で blur が増える応答を契約化しているため、変更前に互換性と意図を確認する。
- 2.5D layer 側には `buildTwoPointFiveDRenderPasses()` の別方式がある。現在は focus depth からの距離で blur offset を決め、最大5回の draw offset を重ねる近似である。カメラ DoF pass と同じアルゴリズム・画質契約ではない。
- `DeepImageBuffer` には深度依存 CPU DoF があり、これは上記 GPU camera pass や 2.5D layer pass と別の経路である。
- `PostProcessPassContractTest` は off-focus blur、thin-lens aperture 応答、clamp、不正値の正規化、in-focus 保持などの GPU 契約を持つ。`DepthBokehEffectContractTest` は depth input／luma fallback／寸法不一致を扱い、Deep image にもカーネルと focus-depth の契約テストがある。
- これらのテストは個別の数値応答や契約を検査するもので、実写・複雑な遮蔽での画質、ボケ形状、色にじみ、連続フレームのちらつき、D3D12／Vulkan の性能・視覚 parity、制作 UI から最終出力までの受入を証明しない。

## R&D の範囲

### DoF ファミリーの境界

「DoF」を一つの処理としてすべての素材へ適用せず、用途が異なる効果群として整理する。R&D の段階では、各モードが入力深度、ピント指定、出力品質、適用レンダー経路を別々に持つことを明示し、同じ画質契約を満たすと仮定しない。

- **Camera DoF:** 3D camera の view depth と focal settings を使う光学系の主要モード。今回の独立 GPU fixture はまずこの `DepthOfFieldPass` の focus-plane 応答を対象にする。
- **Depth Blur / 2.5D DoF:** 2D／2.5D 素材と depth map を対象にする制作向けの深度ぼかし。既存の 2.5D layer 近似や Depth Bokeh と同一実装として扱わず、深度入力・前景境界・出力の基準を別に定める。
- **Bokeh / Lens Blur:** ハイライトの形状・エネルギー・絞り表現を評価する関連モード。単なる radius 違いの別名にせず、Camera DoF／Depth Blur との関係と重複処理の回避を設計判断にする。
- **Deep image DoF:** 独立したユーザー向けモードと即断せず、multi-sample depth を必要とする場合の入力・レンダー統合経路として評価する。

UI が suite を提供する場合も、共通化するのは一貫した用語・プリセット導線・品質表示などの外枠に留める。focus distance、f-stop と blur amount の意味、depth source、alpha/HDR の扱いはモードごとに明示し、効果の重ね掛け・優先順位を検証してから共通コントロールを決める。

### 1. 参照シーンと再現性

固定カメラ・固定解像度・固定出力色変換で、次を分離して検証できる小さな scene pack を定める。

- ピント面の鋭さと前後の焦点遷移を測る slanted edge／解像チャート
- 小さな点光源と高輝度領域による円形・多角形・アナモルフィック bokeh の評価
- 高コントラスト輪郭、細い枝／髪、透明物、重なる前景・背景による halo／bleed／occlusion 評価
- HDR highlight、premultiplied alpha、透明背景での色と alpha の保持
- camera／subject motion と focus change を含む短い frame sequence による flicker／temporal stability 評価
- 3D mesh、image／shape／text 2.5D、Deep sample の経路を区別する scene と expected limitation

素材はライセンスと出所を記録し、可能なら同一シーンの合成 ground truth と depth ground truth を用意する。実写参照を使う場合は「物理的正解」と断言せず、撮影条件・レンズ・センサー・画像処理の差を記録する。

### 2. 画質評価軸

- **Focus fidelity:** focus plane の解像度保持、前後の CoC の連続性、focus distance／f-stop／focal length に対する予測可能な応答
- **Occlusion:** foreground object の背景へのにじみ、silhouette halo、depth discontinuity の破綻、透明 surface の扱い
- **Bokeh:** disk の輪郭、sample noise／banding、aperture shape、回転、highlight の energy と HDR clipping
- **Image integrity:** premultiplied alpha、straight-alpha 境界、透明画素の RGB、HDR 値、色空間変換後の一貫性
- **Temporal quality:** カメラ移動、物体移動、focus pull での shimmer、sample pattern の揺れ、history の ghosting
- **Resolution robustness:** preview／final 解像度、縮小率、非正方 pixel／aspect ratio、各 render backend での見え方

各指標は pixel metrics だけで合否にせず、拡大 crop と全体像の blind visual review を併用する。SSIM／PSNR 等は補助データとして記録し、正解画像や露出・色変換条件が揃わない比較の品質証明には使わない。

### 3. 技術比較

現行 Golden-angle gather pass を baseline とし、参照シェーダー／論文を調査した上で、候補を同じ scene pack で比較する。

- near／far CoC の分離と depth-aware compositing
- tile / neighborhood classification、prefilter、gather／scatter の品質・負荷差
- aperture-shaped sampling、低分解能処理と edge-aware upsample
- temporal jitter／history を導入する場合の ghosting、camera cut、seek／frame random access の扱い
- 複数深度層や alpha/deep input を消費する案の価値と統合コスト

候補は「専門品質に必要な視覚改善」「GPU cost」「メモリ／resource lifetime」「D3D12・Vulkan portability」「既存 color/depth contract との整合」「プレビューと最終レンダーの一致」で比較し、結果を記録してから採否を決める。

### 4. 制作上の受入

- camera の focus distance、f-stop／aperture、focal length、blur limit の意味と単位を実際の画像応答で確かめる。
- Inspector／既存 camera 操作で設定を変更し、preview・連続再生・frame seek・Render Queue の結果と設定が一致することを確認する。
- DoF 無効時は既存画像に差を出さず、深度がない／不正な場合は予測可能に失敗または bypass する。
- 2.5D layer DoF と 3D camera DoF は UI／ドキュメント上で適用対象と品質差が明確になるまで、一つの同等機能として扱わない。

## 進行段階と成果物

### R0 — Baseline と仕様台帳

- 現行 pass、camera parameter mapping、2.5D／Deep の別経路をコードとテストで追跡する。
- scene pack の仕様、撮影・色管理条件、測定方法、GPU／backend 記録フォーマットを確定する。
- 各テストを「数値契約」「画質」「統合」「性能」のどれを証明するか分類する。

**Exit:** baseline の生成手順と既知の limitation を別の作業者が再現できる。

### R1 — 画質ベンチマーク

- 現行 pass を変更せず、scene pack の still／sequence capture と reference comparison を行う。
- halo、foreground/background bleed、bokeh shape、alpha/HDR、focus transition、temporal stability の結果を記録する。
- D3D12 と Vulkan が使える環境では同じ条件で取得し、違いを backend ごとに記録する。

**Exit:** 主要な画質欠陥に優先度・再現条件・視覚 evidence が付き、合否基準が定義される。

### R2 — アルゴリズム候補の比較

- R1 で重要と判定された欠陥にだけ候補を作る。候補は production path と分離できる小さな実験として扱う。
- 品質、frame time、GPU memory、resolution scaling、backend portability を比較する。
- 外部コードや shader を参照・採用する場合はライセンス、由来、変更範囲を記録する。

**Exit:** 採用・不採用の理由、残る制限、実装コストを含む設計判断がレビュー可能。

### R3 — 実装計画と受入契約

- 採用候補を既存 Diligent pass、camera settings、preview/final render の責務に割り当てる。
- visual regression fixture、GPU contract test、runtime acceptance、性能予算を定める。
- 実装を別の小さな milestone として切り、明示された範囲だけに着手する。

**Exit:** 視覚・機能・性能の各 gate と未対応機能が承認可能な形で示される。

## 初期受入の測定方針

R0 で機材・GPU 構成を記録した上で、最低限、同じ scene／camera／resolution において以下を比較する。

| 領域 | 記録するもの | 初期判定 |
|---|---|---|
| Focus | focus plane の chart sharpness、f-stop と focus distance sweep | ピント面の鋭さが保たれ、変更に対して単調・連続に応答する |
| Edge | silhouette の前後、細線、高コントラスト境界 crop | 目視できる halo／背景色漏れ／前景の不正な消失がない |
| Bokeh | point light と HDR highlight の形・面積・最大輝度 | sampling noise が安定し、alpha／HDR が規定以上に失われない |
| Temporal | 固定 frame sequence の隣接 frame 差と crop | 意図した focus/motion 以外のちらつき・ghosting がない |
| Performance | GPU timestamp、resolution、設定、device/backend | 同じ画質設定で preview/final の cost を測定し、R0 後に予算を決定 |
| Parity | 同一入力の D3D12／Vulkan／preview／final capture | 許容差を scene pack で校正し、runtime capture で評価 |

数値の閾値は機材・色管理・scene pack を確定する前に推測で固定しない。R0 で計測方法を決め、R1 で基準値と許容差を提案する。

## R0 静的調査結果（2026-11-10）

- Camera DoF pass は `ArtifactCompositionRenderController` の 3D camera post-process block に接続され、current camera の focus distance、near/far clip、focal length、aperture、blur amount を渡す。pass 適用後は accumulation／temporary target を swap する。
- controller 内の呼び出しでは `DepthOfFieldPass::apply()` を Composition Render Controller から確認した。現時点で `Artifact/src`／`Artifact/include` の全文検索では Render Queue／offline renderer から同 pass を呼ぶ経路は見つからない。これは「最終レンダーに必ず不適用」と断定する根拠ではなく、適用範囲を引き続き追う。
- source code 上、camera aperture の `FStop` 値は shader の `apertureScale` として式の分子に入り、大きい値ほど blur が増える。既存 GPU contract test も 0.1 より 2.0 の方が blur が広がると期待する。これはコードとテストから確認した挙動であり、実機画像・UI ラベルの意味・撮影者の意図は未検証。
- `CameraDOFParameters::maxCoc` は `aperture × blurScale` から導出されるが、controller は `DepthOfFieldSettings::maxCocRadius` に固定 16 px を渡す。最終半径はこの上限と物理式／CoC scale に clamp される。property の `maxCoc` が描画上どう使われるかは追加追跡が必要。
- R0 は source trace のみ。build、test、shader compile、runtime capture、GPU timing は未実施。

**R0 未完了項目:** camera post-depth SRV の texture format／sample count／resolution lifecycle、Render Queue／offline render／software fallback 適用範囲、alpha/HDR format contract、backend 差、Property の aperture 表示単位と初期値を現行ソースで追跡する。

## R1 初回 scene capture（2026-11-10）

- `experiments/DepthOfFieldLab/capture_depth_sweep.py` で、既存の写真プレートと Depth Anything V2 の relative-depth map から source／far focus／subject focus／near focus の比較画像を生成した。
- 出力: `experiments/DepthOfFieldLab/captures/depth-sweep-rd1/dof-focus-sweep.png`、各 full-resolution variant、入力 SHA-256 と条件を記録した `capture-metadata.json`。
- この画像はオフライン image-space Gaussian pyramid の参照 capture で、ArtifactStudio の GPU DoF pass の結果ではない。metric depth、aperture bokeh、layer compositing、実時間安定性は表現していない。現行 production renderer の画質・性能証拠としては扱わない。
- capture を見て、subject focus の正確な plane を指定するには face／coat／car が分離する depth region を測る必要があると分かった。既存の relative depth は推定値なので、次段で synthetic 3D chart／point-light scene と metric depth を用意する。
- Artifact アプリは未起動だったため runtime capture は取れていない。shader test、build、CMake は実行していない。

**R1 判定:** 再現可能な scene reference と focus sweep の初回画像は取得済み。Production renderer の capture／backend parity／GPU timing は未完了。

## R1 独立 GPU pass capture（2026-11-10）

- `tests/Artifact/PostProcessPassContractTest.cpp` に 160×96 の合成 depth-chart fixture を追加し、`DepthOfFieldPass` を headless Diligent Vulkan で実行して input/output を PPM readback する。near／focus／far の平面距離は 4／10／30、focus distance は 10、CoC 上限は 12 px、sample count は 64。
- `tests/depth_of_field_visual/check_depth_of_field_image.py` が GPU test を filter 実行し、画像を読み戻して focus-plane chart contrast retention と両 defocus plane を比較する。PNG contact sheet と JSON metrics を保存する。
- Capture: `experiments/DepthOfFieldLab/captures/gpu-dof-r1/dof-gpu-comparison.png`（左 input、右 GPU pass output）、`dof-gpu-metrics.json`。
- NVIDIA GeForce RTX 4070 Ti / Vulkan で GPU test 1/1 passed、画像検査 3/3 passed。相対 chart contrast retention は focus 1.000、near 0.119、far 0.022。これは既知の合成 scene で focus response と blur を検証した結果であり、production quality のスコアではない。
- 併せて既存 `DepthOfField*` GPU contract tests 6/6 passed。Capture は UI に依存せず、production camera scene integration、scene silhouette halo、aperture-shaped bokeh、透明/HDR、temporal stability、backend parity、GPU timing は未検証のまま。

**R1 判定:** GPU pass を通した独立画像と再現可能な focus-plane 比較を取得した。Camera DoF の scene-level 画質受入には、3D mesh／point-light を含む production integration capture が別途必要。

## 既存確認項目との対応

- GPU pass 数値契約: `tests/Artifact/PostProcessPassContractTest.cpp`、CMake test `ArtifactDepthOfFieldPassContractTest`
- DepthBokeh effect input contract: `tests/Artifact/DepthBokehEffectContractTest.cpp`
- Deep CPU DoF: `tests/ArtifactCore/DeepImageBufferContractTest.cpp`
- camera pass dispatch／input contract: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp`
- camera physical parameter mapping: `Artifact/src/Layer/ArtifactCameraLayer.cpp`、`Artifact/include/Render/ArtifactDepthOfFieldPass.ixx`
- 2.5D layer approximation: `Artifact/src/Layer/ArtifactLayerTransformSupport.cpp`
- historical design references: `docs/IMPL_DOF.md`、`docs/planned/MILESTONE_CAMERA_ENHANCEMENT_DOF_2026-07-09.md`、`docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`

## 未確認・リスク

- この turn ではソースとテストを読んだだけであり、テスト・shader compile・runtime capture は行っていない。
- 既存 DoF test は contract test であり、画質ベンチマークとしての期待画像や現行 baseline は確認できていない。
- 物理式の unit／sensor model の意味、透過物・multi-layer depth の規約、preview と offline render の最終適用条件は、R0 で現行経路をさらに追跡する。
- 強い bokeh、深度 discontinuity の改善は gather 半径とサンプル数を増やすだけでは不十分な場合があり、性能・halo・occlusion を一緒に評価する。
- temporal accumulation は still render／seek の再現性と衝突し得るため、R2 で明確な reset と deterministic output 方針なしに導入しない。

## 次の作業

1. R0: `DepthOfFieldPass` の入力／出力 format、camera depth SRV の生成、preview と Render Queue の適用先を現行コードで追跡する。
2. scene pack の合法な素材／ground truth を選定し、静止画と sequence の baseline 条件を記録する。
3. 現行 D3D12／Vulkan capture と GPU timestamp を取得できる環境で R1 を実施する。ビルド・テスト・capture 実行はユーザーの明示指示後に行う。
4. R1 の結果を受けて R2 の候補と数値 gate を確定する。
