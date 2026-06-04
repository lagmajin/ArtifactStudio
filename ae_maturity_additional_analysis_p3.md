# AE-Like 成熟度追加分析 — パート3（パート1/2 + 既存1-31 以外の問題）

**調査対象**: `Artifact/src/Widgets/Timeline/`, `Artifact/src/Render/*.cppm`, shader/pipeline/resource 周辺, CMake/build  
**制約**: ソースコードのみ。`docs/` は未読。

---

## P0 — 即時修正必須

### 1. OffscreenRenderer2D が null RTV で SetRenderTargets を呼ぶ
- **状態**: 修正済み
- **内容**: `layerRenderTarget_` の RTV を渡すように変更し、null の場合は早期 return するようにした。
- **影響**: Diligent/D3D12 の未定義動作を回避。

### 2. gizmo3DVS の `SourceLength` が未設定
- **状態**: 修正済み
- **内容**: `gizmo3DVsInfo.SourceLength` を `strlen` で設定するように変更。
- **影響**: 3Dギズモ用 VS が Diligent に正しく渡るようになった。

### 3. ShaderManager の CreateShader 戻り値が未確認
- **状態**: 修正済み
- **内容**: `CreateShader` 後に各 shader ポインタを確認し、失敗時はログを出すように変更。
- **影響**: シェーダー失敗時の silent failure を減らした。

### 4. Keyframe 移動でフレーム範囲チェックが未実施
- **状態**: 修正済み
- **内容**: `moveTimelineKeyframe()` の入口で composition の frame range に clamp するように変更。
- **影響**: 範囲外のキーフレーム移動がモデルへ流れなくなった。

### 5. Drag/drop ハンドラが raw `this` をキャプチャ
- **発生箇所**: `Artifact/src/Widgets/ArtifactTimelineWidget.cpp:3901, 3906`
- **内容**: `clipMoved` / `clipResized` が `QObject::connect` の結果を queued lambda で受け取るが `QPointer` ガードなし。widget 部分的破棄後に発火で UAF。
- **影響**: クラッシュ。

### 6. LayerPanel widget の trackIndex ガード不足
- **状態**: 修正済み
- **内容**: nil ガードの後で `trackIndex` を確定するように順序を変更。
- **影響**: タイムライン painting のずれ/クラッシュ要因を解消。

---

## P1 — 高重大度

### 7. FilmEffect のマスター強度が累積適用
- **発生箇所**: `Artifact/src/Effect/ArtifactFilmEffects.cppm:338, 655`
- **内容**: `applyMasterIntensity()` が毎フレーム呼ばれるたびに `grain_.intensity *= masterIntensity_`。n 回適用で `intensity^n`。
- **影響**: 時間経過でエフェクト強度が暴走。

### 8. ArtifactTransition が全トランジションで alpha を破棄
- **発生箇所**: `Artifact/src/Effect/ArtifactTransition.cppm:116/219/264/357`
- **内容**: 全 `Format_RGB32` 固定。アルファチャンネルが保持されない。
- **影響**: フェード/ワイプの縁が黒つぶれ。

### 9. AudioMixer バスサイクルがサイレントドロップ
- **発生箇所**: `ArtifactCore/src/Audio/AudioMixer.cppm:33-63`
- **内容**: サイクル検出で return するが visited に追加しない。次回訪問で再度 drop。
- **影響**: 循環バスに接続された音声が消える。

### 10. AudioRenderer::levelCallback が data race
- **発生箇所**: `ArtifactCore/src/Audio/AudioRenderer.cppm:177-187, 467-474`
- **内容**: `std::function` の非アトミック読み書きを UI スレッドと audio スレッドが競合。
- **影響**: torn std::function によるクラッシュ。
- **状態**: 修正済み

### 11. PSO 生成後 SRB 生成失敗が未処理
- **発生箇所**: `Artifact/src/Render/ShaderManager.cppm:539-542`
- **内容**: PSO 生成成功 → SRB 生成失敗時に SRB が null のまま。描画時に `CommitShaderResources` が no-op/silent。
- **影響**: 任意のパイプラインで描画欠落。

### 12. OffscreenRenderer2D のブレンドPSO生成が全部コメントアウト
- **発生箇所**: `Artifact/src/Render/ArtifactOffscreenRenderer2D.cppm:220-226`
- **内容**: `createLayerBlendPSO()` の `CreateComputePipelineState` が全部コメントアウト。PSO map は事前埋めだが全要素 null。
- **影響**: 2D レイヤーブレンド描画が破損。

### 13. PrimitiveRenderer3D の Billboard PSO 失敗がログなし
- **発生箇所**: `Artifact/src/Render/PrimitiveRenderer3D.cppm:383-386`
- **内容**: `CreateGraphicsPipelineState` 失敗で silent return。呼び出し側が `!pso` で先に return するが診断なし。
- **影響**: パーティクル/ビルボードが描画されない原因が不明。

### 14. RenderLayerPipeline::renderComposition がスタブで常にクリア
- **発生箇所**: `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:212-214`
- **内容**: `layers` / `currentFrame` を無視し `{0,0,0,0}` でクリア。`ready()` 内で呼ばれるが実質 no-op。
- **影響**: レイヤー合成パイプラインが機能しない。

### 15. swapAccumAndTemp がメモリバリアなしでテクスチャ入れ替え
- **発生箇所**: `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:232-235`
- **内容**: `TextureBundle` (SRV/UAV/RTV) の swap のみ。D3D12/Vulkan の resource-barrier 必要なしに UAV→SRV 遷移が発生。
- **影響**: GPU バリデーションエラー/描画破損。

### 16. ArtifactRenderQueueService の GPU テクスチャ参照カウントが不明
- **発生箇所**: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- **内容**: `GPUTextureManager::createTexture()` の戻り値の生ポインタを map に保存。`RefCntAutoPtr` で包まないため参照カウントが不明。map erase で Release() されない。
- **影響**: テクスチャリーク、または二重解放。

### 17. PlaybackEngine のフレーム更新で空 QImage emit
- **発生箇所**: `Artifact/src/Playback/ArtifactPlaybackEngine.cppm:314`
- **内容**: `Q_EMIT owner_->frameChanged(pos, QImage())` — 空の QImage を送信。実描画パスが未接続。
- **影響**: 再生プレビューが黒画面。

### 18. TextGizmo がどのコントローラからも import されていない
- **発生箇所**: `Artifact/src/Tool/ArtifactTextGizmo.cppm`
- **内容**: 完全な実装があるが CompositionEditor / RenderController のいずれからもインポートされていない。
- **影響**: テキストGizmo の range selector ハンドルが動作しない。

---

## P2 — 中程度/保守負債

### 19. タイムラインに 30fps 固定フォールバックが3箇所
- **状態**: 修正済み
- **内容**: 30fps 直書きのフォールバックをやめ、composition fps / playback fps から解決するヘルパーに集約。
- **影響**: 24/60fps 環境でもタイムライン表示がコンポジション設定に追従しやすくなった。

### 20. Shader ホットリロード機構が皆無
- **発生箇所**: `Artifact/src/Render/ShaderManager.cppm:148-405`
- **内容**: 全 HLSL がインライン文字列。ファイルタイムスタンプのキャッシュも `QFileSystemWatcher` もない。
- **影響**: シェーダー修正ごとにバイナリ再起動が必要。

### 21. PSO ディスクキャッシュがソース変更で無効化されない
- **発生箇所**: `Artifact/src/Render/ShaderManager.cppm:407-506`
- **内容**: キャッシュキーは GPU アダプタ ID のみ。HLSL 変更時も同一キーで古いバイナリが再利用される。
- **影響**: シェーダー修正が反映されない異常が起きやすい。

### 22. タイムラインのキーフレームヒット半径が DPI 非対応
- **発生箇所**: `Artifact/src/Widgets/ArtifactTimelineWidget.cpp:2151, 2170`
- **内容**: `handleHalfW = 6`, `kReservedClickDragThresholdPx = 4` が物理ピクセル固定。
- **影響**: HiDPI 環境で選択がカスる/反り」。

### 23. ArtifactCompositionViewDrawing の ProjectedCorners 凸検証なし
- **発生箇所**: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- **内容**: `std::array<Point2D,4>` が包丁凸四辺形不変を持たない。
- **影響**: 退縮カメラ/極端なパースでラスタライズが変形。

### 24. PrimitiveRenderer3D のテクスチャキャッシュキーが QImage 内部キー
- **発生箇所**: `Artifact/src/Render/PrimitiveRenderer3D.cppm:438`
- **内容**: `textureCache_[cacheKey]` の `cacheKey` が `QImage` の内部ハッシュ。コピー/変更後に不一致。
- **影響**: キャッシュミス、または誤ったテクスチャ返却。

### 25. AudioRenderer の memset サイズ計算が int オーバーフロー
- **発生箇所**: `ArtifactCore/src/Audio/AudioRenderer.cppm:120`
- **内容**: `frames * channelsRequested * sizeof(float)` が signed int。 large buffer → wrap → 未初期化領域の memset。
- **影響**: 音声ノイズ/クラッシュ。

### 26. FFmpegVideoDecoder stride 計算の int オーバーフロー
- **発生箇所**: `ArtifactCore/src/Codec/FFMpegVideoDecoder.cppm:82-83`
- **内容**: `width * 3` が `int` 演算。4K+ でラップ → 極小バッファ確保 → memcpy overrun。
- **影響**: メモリ破壊。

### 27. MediaPlaybackController の UI スレッド sleep
- **発生箇所**: `ArtifactCore/src/Media/MediaPlaybackController.cppm:838-846`
- **内容**: 動画パケット待ちで `std::this_thread::sleep_for(1ms)` / `yield()` を UI スレッド上で実行。
- **影響**: 最大 125ms のフリーズ。

### 28. ArtifactProjectPackager が部分コピー失敗時に後続を継続しない
- **発生箇所**: `Artifact/src/Project/ArtifactProjectPackager.cppm:136-139`
- **内容**: 1ファイルでも `QFile::copy` 失敗で `return false`。すでにコピー済みファイルをクリーンアップしない。
- **影響**: 中途半端なパッケージが残る。

### 29. ArtifactProjectItems FolderItem の子 alloca 所有権が未規定
- **発生箇所**: `Artifact/src/Project/ArtifactProjectItems.cppm:49`
- **内容**: `new FolderItem()` で割り当て、親の破棄が保証されない場合リーク。
- **影響**: 大規模プロジェクトツリーで蓄積的リーク。

### 30. 複数の build スクリプトがユーザー固有絶対パスで他環境で破綻
- **発生箇所**: `build_artifact.bat:2`, `build_artifact_core.bat:3/6`, `build_artifact_final.py:11`, `build_artifact_target.py:6`, `build_artifactcore.py:11/41`, `build_and_report.py:11`
- **内容**: 全体的に `X:\dev\artifactstudio` (小文字), `C:\Users\lagma\...`, `C:\Program Files\CMake\...` が直書き。
- **影響**: 他の開発者/CI でビルド不能。

---

## テクスチャ/GPU パス固有の追加 (前回のG-1〜G-8に追加)

| # | カテゴリ | 発生箇所 | 内容 |
|---|----------|----------|------|
| T-1 | リソースリーク | `Artifact/src/Render/ArtifactOffscreenRenderer.cppm:92` | CreateTexture 戻り値未確認で pRenderTarget_ が null のまま放置 |
| T-2 | GPU リソースリーク | `Artifact/src/Render/ArtifactOffscreenRenderer.cppm:520-526` | CreateRayTracingPipelineState 失敗で rayTracingPSO_ が null のまま DispatchRays を呼ぶ |
| T-3 | スレッド競合 | `Artifact/src/Render/GPUTextureCacheManager.cppm:355` | textureCache_.reserve() が mutex 外 → 並列 upload 中に rehash → UB |
| T-4 | 例外安全 | `PrimitiveRenderer2D.cppm:809` | ctor 内 build() 例外で GPU buffer shared_ptr リーク |
| T-5 | 例外安全 | `ArtifactCore/src/Graphics/GPUTexture.cppm:91` | デストラクタが例外送出 → std::terminate |
| T-6 | 例外安全 | `Artifact/src/Render/Software/ArtifactSoftwareImageCompositor.cppm:708` | noexcept 関数内で ImageF32x4_RGBA 確保失敗 → std::terminate |
| T-7 | 所有権曖昧 | `RenderCommandBuffer.cppm` | ITextureView* 生ポインタ、寿命要件文書化なし |
| T-8 | ポインタ寿命 | `Artifact/src/Render/CompositionRenderer.cppm:8` | renderer_ が raw ポインタ、ArtifactIRenderer 破棄後にダングリング |
| T-9 | タイミングスタール | `Artifact/src/Render/ArtifactIRenderer.cppm:1409` | GetData(True) で毎フレーム CPU スタール。1-2ms/frame のコスト |
| T-10 | VSync 設定不可 | `Artifact/src/Render/ArtifactIRenderer.cppm:1551` | Present() が引数なし。VSync off/Mailbox などが常にデフォルト |
| T-11 | UAV バリア欠落 | `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:232-235` | swapAccumAndTemp() で texture bundle 入れ替えのみ、subresource transition なし |
| T-12 | QImage キャッシュキー不安定 | `PrimitiveRenderer3D.cppm:438` | QImage 内部実装依存キーで miss/wrong-texture |

---

## マーカー・ツール未接続の再確認

| # | 内容 | 発生箇所 | 影響 |
|---|------|----------|------|
| M-1 | TextGizmo 未接続 (210行実装が無視) | `Artifact/src/Tool/ArtifactTextGizmo.cppm` | テキスト range selector が動作しない |
| M-2 | SpeedGraph sample 未実装 | `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm:815` | Alt+G でデータなし |
| M-3 | Text Animator timeline 未配線 | `Artifact/src/Layer/ArtifactTextLayer.cppm` | キーフレームトラック露出なし |
| M-4 | Group Layer mask 未接続 | drawMaskedTextureLocal 未呼び出し | グループマスク効果なし |
| M-5 | ShapeOperator 6種 未実装 | `ArtifactCore/src/Shape/ShapeGroup.cppm:294+` | Trim/Repeater/Merge/Offset/Pucker/Twist なし |

---

## 新規合計: パート3 単体カウント

| カテゴリ | P0 | P1 | P2 |
|----------|----|----|----|
| メモリ破壊/UB | 4 | 2 | 1 |
| GPU リソース/PSO | 2 | 5 | 2 |
| データ race | 1 | 2 | 1 |
| スタブ/論理破綻 | 4 | 1 | 0 |
| タイムライン UI | 1 | 0 | 2 |
| Build/環境 | 0 | 0 | 1 |
| レンダリング正否 | 0 | 3 | 4 |
| 合計 | 12 | 13 | 11 |

---

*分析日: 2026-06-03*  
*派生元: ae_maturity_additional_analysis.md, ae_maturity_additional_analysis_p2.md の重複を除外*
