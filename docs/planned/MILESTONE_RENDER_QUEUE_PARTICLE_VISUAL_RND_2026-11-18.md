# RenderQueue パーティクル描画 視覚品質 R&D 評価計画

**最終更新:** 2026-11-19
**ステータス:** 評価計画策定済み / baseline 取得前

## 目的と対象

- **目的:** `RenderQueueLayerImageIntegrationTest.Production2DAnd3DParticlesRenderThroughManagerQueue` が出力するパーティクル描画の視覚品質・挙動・性能を改善する。具体的には、(1) 煙らしい渦／ディテール、(2) 粒感・ブレンド（サイズ、加法／アルファ、色変化）を目標とする。
- **対象経路:** `ArtifactRenderQueueService` 経由の 2D Particle / 3D Particle レイヤー、CPU と GPU バックエンド。比較は同一 composition・同一フレーム・同一解像度（320x180）・30 fps・固定シード／同一エミッタ設定。
- **対象外:** 実アプリの Composition Viewer での見え方、DCC 品質への到達、動画／連番レイヤー、ReactiveEvents、ソフトレンダラーの新機能追加。兄弟キャプチャ `render_queue_smoke_*` は参考資料であり、本 R&D の正解ではない。
- **現状:** ディスク上に `render_queue_particles_*.png` は存在しない。兄弟キャプチャ `build/smoke-runtime-rnd/queue-preview-compare/captures/render_queue_smoke_gpu_0012.png` は黒背景に小さな緑の塊で、渦や粒のディテールはほぼ無い（2026-11-18 目視）。まず baseline を取得する。

## 比較対象

- **baseline:** 現行コード／現行バイナリで取得した `render_queue_particles_2d*.png`, `render_queue_particles_3d.png`, `render_queue_xpu_particles*.png`。以降の候補はすべて同一条件で取得し、baseline と比較する。
- **参照画像:** なし。目標は「煙らしい渦／ディテール」「粒感・ブレンド」という定性目標と、下記の効果固有指標で定義する。実写や他製品画像を無条件の正解とはしない。

## 評価素材

- テスト内蔵の人工素材: エミッタ 1 点、赤系単色（255,32,16）、scale 12、life 10 s、速度 0、frame 1-2、320x180。
- 失敗しやすい例として、粒子数の多い 3D ケース（rate 1000）と、Brightness エフェクト適用ケース（緑／青チャンネル変化の検証）を含む。
- 素材の出所はテストコード（`tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`）に固定され、ハッシュ管理は不要（コード管理対象）。

## 画像条件

- 入力: テスト内で構成した 2D / 3D composition。出力: PNG（8-bit）、320x180、背景は透明／黒。
- フレーム: 1, 2（必要なら range 0024 も参照）。色空間・alpha 契約は現行 RenderQueue 出力に従う（未検証項目として記録）。
- キャプチャ保存先: `ARTIFACT_UI_TEST_CAPTURE_ROOT=captures/render-queue-particles-rnd-20261118/` に固定。取得手順・環境は manifest に記録する。

## 評価と合否

| 項目 | 適用 | 方法 | 合格条件 / 重大欠陥 |
|---|---|---|---|
| 効果の正しさ | 適用 | 既存 GTest アサーション（可視画素数、色条件、背景不変）＋エミッタ設定変更時の応答 | 既存 contract が通ること。粒子が消える／背景が変化するは重大欠陥 |
| 画質と細部 | 適用 | 全体＋ROI の等倍・拡大目視。渦／粒の輪郭、ハロ、バンディング | 現在の「緑の塊」より構造が増え、粒が識別できること。過剰なノイズ・ハロは不合格 |
| 色と alpha | 適用 | 赤系パーティクルの RGB 分布、alpha 確率分布、背景合成後の縁 | 既存色条件（R>200,G<80,B<50 等）を維持。alpha が 0 の画素で RGB が漏れないこと |
| 時間安定性 | 適用 | frame 1 と 2 の差分、連続キャプチャ（range 0024）の目視 | 同一設定でフレーム間の意図しない flicker がないこと |
| 経路と解像度 | 適用 | CPU / GPU バックエンド、320x180 固定 | 両経路で粒子が可視。差は許容範囲を baseline 取得後に記録 |
| 性能 | 適用 | 既存テストのレンダ時間、粒子数、描画パケット数を baseline と比較 | 顕著な劣化がないこと。予算値は baseline 取得後に確定 |

### 効果固有の指標（案）

- 占有画素数（alpha > 閾値）、RGB チャンネル別分位点、局所コントラスト（高-pass エネルギー）、隣接フレーム差。
- 粒感: 孤立した高 alpha 画素の割合、サイズ分布の分散。
- 渦／ディテール: パーティクル雲のエッジ密度、空間周波数の高域成分（目視と併用し、指標単独で quality を断定しない）。

## 実行条件

- GPU: RTX 4070 Ti（環境既知）。バックエンド: D3D12 / Vulkan 両方を可能なら確認。描画は offscreen（`QT_QPA_PLATFORM=offscreen`）。
- ビルド・テスト実行はユーザー明示許可済み（2026-11-18）。再ビルド時も差分範囲を限定する。
- キャプチャ生成: `ArtifactRenderQueueLayerImageIntegrationTest.exe --gtest_filter=RenderQueueLayerImageIntegrationTest.Production2DAnd3DParticlesRenderThroughManagerQueue` に `ARTIFACT_UI_TEST_CAPTURE_ROOT` を設定して実行。

## 2026-11-18 baseline 取得結果

- 実行: 既存 `build/bin/Debug/ArtifactRenderQueueLayerImageIntegrationTest.exe`（2026-10-28 ビルド）を既定 Qt プラグインで実行。`QT_QPA_PLATFORM=offscreen` は `platforms/qoffscreen.dll` が未配置のため使用不可（起動時にブレークポイント／ハング）。テストは 1/1 通過（6.9 s）。
- 証跡: `captures/render-queue-particles-rnd-20261118/captures/` の 4 PNG。数値は `System.Drawing.Bitmap` で取得（8-bit sRGB 値）。
- `render_queue_particles_2d_0001.png`: 背景 (3,5,8,255) は設定値 (0.012,0.018,0.032) と一致。背景と異なる画素 434。見た目は単一の白〜橙の丸い光で、渦・粒・槍は無い。
- `render_queue_particles_2d_gpu_baseline_0001.png`: 背景 (50,36,29,255) で CPU の (3,5,8) と不一致（**D1: GPU 経路の背景色不一致**）。粒子は青系で、エミッタ設定色 (255,32,16) と不一致（**D2: GPU 粒子色不一致**）。背景差分 448 px。
- `render_queue_particles_2d_brightness_0001.png`: 背景 (50,36,29,255)、背景差分 448 px。Brightness で赤から橙へ変化することは既存アサーションが確認済み。
- `render_queue_particles_3d_0001.png`: 背景差分 **0 px**。既存アサーションは `alpha>32 && (R>40||G>40||B>40)` で、背景 (50,36,29,255) 自身が条件を満たすため、**粒子が無くても合格する空の検査**（**D3: 3D アサーションの vacuous pass**）。

### 欠陥と次の確認

- D1 / D2 は GPU 経路の色契約に関わる。製品側レンダラのバグか、テスト側の色空間・backend 設定差かを切り分ける。現時点では原因未断定、合格扱いにしない。
- D3 はテスト側の検査不備。背景色を基準にした差分検査へ置き換える。
- 視覚目標に対して、現状は「単一の丸い光」であり、煙の渦・粒感は未達。複数エミッタ、速度／方向分散、turbulence、サイズ・カラー・不透明度曲線を使った plume キャプチャを専用テストで追加し、反復評価する。

## 2026-11-18 第1イテレーション（未検証）

- `tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp` に `ParticlePlumeVisualCapture` を追加した（親リポジトリのみ）。3 エミッタ、上向き速度・方向分散、turbulence、サイズ／カラー／不透明度曲線、Normal ブレンド、8 フレーム出力、画像統計（占有画素・bbox・重心・色数・high-pass）をログし、渦・粒・上昇を検証する。既存契約テストは変更していない。
- **未検証:** 追加テストはコンパイルできていない。古い IFC（2026-10-28）に対して現行 TU を再コンパイルすると、既存コードも含め `ClonerGridPlacementRange` 型解決失敗など多数の C2039/C2440 が発生。全体ビルドは v145 の C1116（`Animation.Transform3D`）で失敗、v143 Ninja ツリーは `Core.AI.Context.ifc` 欠落で失敗。したがって新テストの実行・画像確認は未実施。
- 自分の追加コードによる実エラー（一時オブジェクトの参照渡し C2664）は 1 件検出・修正済み。

### 2026-11-18 ビルド環境修復の試行結果

- v145 / MSBuild（`build/`）: `Animation.Transform3D` の IFC を選択ビルドで再生成しても C1116（`stop_token(248,6)`）は解消しない。14.50 ツールセットへの切り替えも CL.exe 解決失敗で不可。
- v143 / 新規 CMake+Ninja ツリー（`out/build/rq-particle-v143`）: configure は成功。ただし CMake 4.2 が明示 `/reference` 先の `.ifc` を Ninja 出力として宣言せず、AI モジュール等の `.ixx.obj` を先にビルドして IFC を生成する回避策が必要。全 1,699 モジュール object の事前ビルドを試みたが、現行子リポジトリソースに既存のコンパイルエラー（`ImageSegmenter.ixx` / `DataPropertyBridge.ixx` の Qt `tst_QList`、`ShapeExtrude.cpp` の `QList` 未定義、`MeshRenderer.cpp` の C2026 文字列過大）があり完了しない。
- 結論: 現行ソースでテスト exe を再ビルドするには、子リポジトリ（`ArtifactCore`）のコンパイルエラー修正が必要。AGENTS.md により子リポジトリ編集はユーザーの明示要求がない限り禁止のため、未実施。既存 exe（10/28）は起動・既存テスト実行には成功するが、新規テストは含まれない。

## 未検証 / 制約

- 現行 exe は 2026-10-28 ビルド。ソース変更後の候補評価には、整合した IFC を持つビルドツリーの構築が必要。
- 色空間・alpha の厳密な契約、D3D12/Vulkan parity、実アプリ到達、時間安定性の知覚評価は未実施として記録する。
- 閾値は baseline 取得後に確定し、候補の最終比較より前に凍結する（ルール 5 項）。
