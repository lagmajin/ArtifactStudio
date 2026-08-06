# 非同期化・GPU化 パフォーマンス調査

**最終更新:** 2026-08-06

## 実装済み（2026-08-06）

- E（RAM Preview readback）を実装。既存の `readbackTextureViewToImageAsync` / `readbackToImageAsync` 経路を、環境変数未設定時も利用するよう変更した。
- 再生中も非同期 readback の対象にし、GPU コピーと fence 待機を既存のバックグラウンド経路へ委譲する。
- B の通常のプロジェクト Open メニュー経路を `loadFromFileAsync` に切り替え、JSON 読込・再構築中の UI スレッドブロックを解消した。
- B の通常の Save / Save As メニュー経路も `saveToFileAsync` に切り替え、直列化・ファイル書込中の UI スレッドブロックを解消した。
- B の Welcome 画面と起動プロジェクト経路も `loadFromFileAsync` に切り替えた。
- K の File メニュー／Welcome 画面からの一括アセットインポートも既存の `importAssetsFromPathsAsync` に切り替え、コピー・プローブ中の UI ブロックを解消した。
- I の Project View は通常の可視内容更新でサムネイルキャッシュを全消去しないよう変更し、モデル差し替え／内容変更時だけ無効化するようにした。
- C のプロパティ式評価に既存 `ScriptContext::getOrParseAST` を接続し、同一式の毎回の tokenize／parse を避けるようにした。AST 取得に失敗した場合は従来の文字列評価へフォールバックする。
- 変更箇所: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 変更箇所: `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`

## 概要

`ArtifactStudio` は C++20 modules + Qt + DiligentEngine（DX12/Vulkan GPU レンダラ）のモーショングラフィックス/合成ソフト。本調査は「メイン(UI)スレッドで同期実行されている重い I/O/デコード/解析/合成」と「CPU(QImage/OpenCV) で行われている重い処理の GPU 化」候補を特定し、パフォーマンスとレスポンスを改善できる箇所を洗い出した。

初版は読み取り専用調査として作成した。現在は実装済み項目を冒頭に追記し、各候補は「事実（コード上の実装）」と「推測（ボトルネック/案）」を分け、未検証は `未検証` と明記する。

制約の遵守:
- 新規 signal/slot は一切提案せず、既存の非同期/スレッド経路の再利用を前提とする。
- 子リポジトリ（`ArtifactCore` 含む）の変更はユーザー依頼なしのため行わない。
- `QImage` 新規採用禁止、`QtCSS` 新規追加禁止等の規約に抵触しない方向。

## 再利用可能な既存インフラ

新規仕組みを作らず以下を使うことで安全に非同期化できる:

- **共有バックグラウンドプール**: `ArtifactCore/src/Thread/ThreadHelper.cppm` の `SharedBackgroundThreadPoolHolder`（QThreadPool、最大 4 スレッド、名前 `ArtifactBackgroundThreadPool`）。
- **非同期パターン**: `QtConcurrent::run` + `QFutureWatcher`（完了を watcher の finished 経由で処理）。
  - 実例: `ArtifactWidgets/src/Image/BasicImageViewWidget.cppm:147-161`、`WaveformScopeWidget.cppm:282-296`、`ParadeScopeWidget.cppm:464`。
- **プロジェクトの非同期メソッド（定義済み・UI 経路未使用）**: `ArtifactProjectManager.cppm:937` `loadFromFileAsync`、`:1023` `saveToFileAsync`。
- **アセット非同期モデル**: `ArtifactProjectService.cppm:2045` `importAssetsFromPathsAsync`（ワーカーでコピー/プローブ→メインで `registerImportedAssets`）。
- **レイヤー先読み**: `ArtifactImageLayer.cppm` の `startPrefetch()`、`ArtifactSvgLayer.cppm:97` `prefetchFuture_`（未完了時は空を返す＝非ブロック）。
- **動画デコード/オープンワーカー**: `ArtifactVideoLayer.cppm:1025` `decodeFuture_`、`:1028` `openFuture_`（デコード自体は別スレッド）。
- **GPU compute インフラ**: `ArtifactIRenderer::createOffscreenComputeTexture`（RGBA16F, RT|SRV|UAV）、`offscreenTextureUnorderedAccessView`、`unbindColorTargetsForCompute`、`readbackTextureViewToImageAsync` / `readbackToImageAsync`（リングバッファ非同期 readback）。
- **GPU パイプライン**: `MaskCutoutPipeline`（使用中）、`MaskPathRasterizerPipeline`（定義のみ・未使用）、`LUT3D GPUComputer`、`GaussianBlur` GPU impl。
- **エフェクト抽象**: `ArtifactAbstractEffect::ComputeMode{CPU,GPU,AUTO}` + `supportsGPU()`。
- **並列基盤**: `ArtifactCore::Parallel::For`、`QMetaObject::invokeMethod(QueuedConnection)`。
- **ジョブキュー**: `ArtifactRamPreviewController`（優先度ジョブキュー）、`ArtifactRenderScheduler`（QThreadPool）。

---

## 🔴 高優先（効果大・既存インフラで実現可能）

### A. 通常合成（Normal ブレンドのみ）が GPU ブレンドパスを使っていない

- **事実**: `ArtifactCompositionRenderController.cppm:27216-27234` `hasGpuBlendJustification` は非 `BLEND_NORMAL` レイヤーが存在する場合のみ true。`gpuBlendPathRequested`（cppm:27262-27281）も「非 Normal ブレンド / SSGI / マルチチャンネル」のいずれかを要する。全 Normal の一般的コンポジションはこのゲートから外れ、CPU 描画パス（`ArtifactCompositionViewDrawing`）が使われる。
- **推測（未検証）**: RGBA16F ブレンドパイプラインは初期化されるが Normal のみの場合は base pass すら走らず、合成の多くが CPU 側の QImage/OpenCV 処理に依存するとみられる。
- **案**: RGBA16F GPU ブレンドパイプラインを Normal ブレンドにも適用。最大の恩恵。
- **懸念**: 既存 CPU パス特有の順序（マスク→マット→ラスタライザエフェクト）との厳密な一致が必要。大幅変更。

### B. プロジェクト読込 / 保存の同期実行

- **事実**: `ArtifactProjectManager.cppm:618` `loadFromFile`（同期）、`:824` `saveToFile`（同期）。
- **呼び元**: `ArtifactFileMenu.cppm`（`handleOpenProject`→`openProjectPath`→`loadFromFile`）、`AppMain.cppm:1857` 起動リカバリ、`ArtifactMainWindow.cppm:1474` welcome の `openRecentProject`、`ArtifactFileMenu.cppm:165` `handleSaveProject`。
- **問題**: 大規模プロジェクトの JSON パース＋全オブジェクト再構築、および `toJson()` 直列化＋ファイル書込が UI スレッドでブロック。
- **案**: 既存の `loadFromFileAsync` / `saveToFileAsync` をメニュー/リカバリ経路から呼び出し、完了を `QtConcurrent + QFutureWatcher` で受ける。プールは `ThreadHelper` の共有プール。
- **効果**: 起動・オープン・保存時の数秒〜数十秒のフリーズ解消。
- **懸念**: 再構築中の進行状態 UI（プログレス）の提示要。オブジェクトグラフ構築のスレッド安全性確認要【未検証】。保存中にさらに編集された場合の整合性、atomic 書込（temp→rename）の確認要。

### C. プロパティ式の AST 再パース（純粋な無駄）

- **事実**: `ArtifactCore/src/Property/AbstractProperty.cppm` の `evaluateValue` が `ExpressionEvaluator::evaluate`（`ExpressionEvaluator.cppm:438-446`）を呼び、同メソッドは `parser_.parse` を毎回実行する。既存の `ScriptContext::getOrParseAST`（`ScriptContext.cppm:85`）キャッシュはプロパティ評価パスから呼ばれない。
- **問題**: `evaluateValue` は `getKeyFrames()` のコピー＋`loopKeyframes` 構築を毎回行い、さらに式文字列を毎回 tokenize→parse→AST 構築する。式付きプロパティ多量保持時、再生/描画で毎フレーム複数回呼ばれる可能性が高い。
- **案**: 式評価時に `ScriptContext::getOrParseAST` で AST を取得し `evaluateASTAtTime`（`ExpressionEvaluator.cppm:708`）で評価（または `ExpressionEvaluator` 側に AST キャッシュ追加）。キーフレーム配列コピーも「式モード」時のみに限定し、式が無い場合は純粋補間へ短絡。
- **効果**: 式付きプロパティ多量時の再生/描画コストが劇的に低下。改修範囲小・効果大。
- **懸念**: 式編集時のキャッシュ無効化タイミング、`ScriptContext` AST キャッシュの無制限蓄積（`clear()` は既存）。評価コンテキスト（変数セット）の共有境界確認要。
- **未検証**: `evaluateValue` の正確な呼び出し頻度（再生 1 フレームあたり何回）。

### D. 動画フレームのデコード待機（draw / サムネイル直結）

- **事実**: `ArtifactVideoLayer.cppm:1562` `getThumbnail`→`currentFrameImageBuffer().toQImage()`（`:1904` `currentFrameToQImage`、`:1943` `decodeFrameToImageBuffer`、`:2583` `cachedFrameImageBuffer`）。呼び元: `ArtifactAbstractComposition.cppm:4765` `getThumbnail`、各種 draw/サムネイル経路。
- **問題**: デコードはワーカーだが cache miss 時に呼び出し側が結果を同期待機する構造の可能性が高く、未デコードフレームで UI が止まる。
- **案**: draw 経路は「既にデコード済バッファのみ描画、未完了ならプレースホルダ」とし、完了後に再描画要求。サムネイル経路は `QFutureWatcher<QImage>` で結果を受ける（既存 `BasicImageViewWidget` パターン流用）。
- **効果**: タイムライン描画・サムネイル生成時のコマ落ち/フリーズ解消。
- **懸念**: cache miss 時の再描画トリガ設計。
- **未検証**: 実際に待機ブロックするかは実測プロファイルで確定要。

### E. RAM Preview readback の非同期化

- **事実**: `ArtifactCompositionRenderController.cppm:31067-31149` で非同期 readback（`readbackTextureViewToImageAsync` / `readbackToImageAsync`）を RAM Preview の未構築フレームに対して利用する。`storeCompositionPreviewFrameImage` の呼び出しは非同期コールバック内のみ（cppm:11379）。
- **事実**: 非同期基盤自体は堅牢 — リングバッファ + `CopyTexture` + `EnqueueSignal`/`Flush`（非ブロッキング）+ `QtConcurrent::run` バックグラウンドで fence wait とピクセル変換、コールバックで `QImage` 返却（`ArtifactIRenderer.cppm:2069-2290`）。
- **実装**: 環境変数ゲートを外してデフォルト有効化し、再生中も非同期 readback でキャプチャするよう条件を緩和した。同期 `readbackToImage` へのフォールバックはこのキャプチャ経路では使用しない。
- **効果**: 再生中の RAM プレビー構築が非ブロッキングに。
- **懸念**: 再生フレームレートとの整合、fence wait のバックグラウンド負荷。

### F. プロキシ生成 / オートセーブの UI スレッド実行

- **事実**: `ArtifactProjectManagerWidget.cppm:6557` `processNextProxyJob`（UI スレッドで逐次処理）。`ArtifactVideoLayer.cppm:2256` `ArtifactProxyManager::generateProxy`（ffmpeg 起動または `QImage` ロード＋scaled＋save）。`ArtifactRenderOutputSettingDialog.cppm:278` `ffmpegExeSupportsEncoder` が `QProcess::waitForFinished(5000)` をダイアログ表示時に同期実行。
- **事実**: `AppMain.cppm:733` `currentProjectSnapshotJson()`（`toJson(Indented)` 直列化）を `:3828` の QTimer タイムアウトで呼び、`ArtifactAutoSaveManager.ixx:164` `createRecoveryPoint`→`:181` `file.write()` で同期書込。
- **案**: `generateProxy` 実体と ffmpeg プローブ、およびオートセーブのファイル書込を共有プールのワーカーへ移し、完了を watcher/既存タスク経路で受ける。
- **効果**: プロキシ生成中・定期オートセーブ時の操作不能/フリーズ解消。
- **懸念**: ffmpeg プロセス数の同時実行上限管理、生成中ステータス表示の設計。直列化中のグラフ変更による例外（コピーオンライト的スナップショット要）【未検証】。

---

## 🟠 中優先

### G. トラックマットの CPU 処理（applyLayerMatteToSurface, QImage 版）

- **事実**: `ArtifactCompositionRenderController.cppm:6551-6741`。`convertToFormat(Format_ARGB32_Premultiplied)`、ピクセルループで luma/alpha 抽出、表面への乗算も CPU ループ。
- **事実（疑似最適化）**: 一部の `>= 256*1024` 分岐は両方とも同じ処理を呼んでおり並列化されていない（デッド/重複コードの疑い）。
- **案**: GPU ブレンドパス側は既に `matteSourceImages` を GPU アップロード→GPU track matte として実装済み（cppm:28104-28152）。CPU パスも同一 GPU マット経路へ統合、または最低限 `Parallel::For` で真の並列化。
- **懸念**: `evaluateMatteStack` 評価結果と GPU パスの厳密一致。

### H. レイヤー表面のラスタライザエフェクト / マスクの CPU バッファ構築

- **事実**: `ArtifactCompositionViewDrawing.cppm:642-`（CvMat 版）、GPU 版 (ImageF32x4_RGBA) は 826-842。表面を `qImageToCvMat`→`CV_32FC4` に変換し `LayerMask::applyToImage`（OpenCV 乗算）と `effect->applyConfigured` を適用。`applyConfigured` は CPU/GPU impl を切替（`supportsGPU()` + `ComputeMode`）。表面キャッシュは `dynamic_cast` で除外され無効化。
- **問題**: `QImage → cvMat → ImageF32x4_RGBA → QImage` の往復変換が毎フレーム発生。
- **案**: 表面バッファを GPU テクスチャとして保持し、マスク/マット/エフェクトを GPU パスで適用。
- **未検証**: `MaskPathRasterizerPipeline` が定義のみで未使用の理由。

### I. Project View サムネイルの毎回 clear ＋同期 IO

- **事実**: `ArtifactProjectManagerWidget.cppm:3615` `refreshVisibleContent` 内 `tilePreviewCache.clear()`。`:1126` `projectItemPreviewPixmap`（画像同期読込＋スケール、composition は `generateCompositionThumbnail`→`renderCompositionCanvas` 全体レンダリング）。
- **問題**: スクロール等の軽微な更新でも全サムネイルを破棄し、再描画時にファイル同期読込またはコンポジション全体レンダリングを実行。
- **案**: サムネイルをキー(アセットID/フレーム/サイズ)→Pixmap の LRU キャッシュにし、`refreshVisibleContent` では clear せず差分のみ再生成。composition サムネイルは RAM preview キャッシュや既存 base composite を流用。IO は非同期化。
- **効果**: スクロール/再構築時のカクつき解消。
- **懸念**: キャッシュ無効化（アセット更新・フレーム変更）の設計。`QImage` 新規採用禁止規約に抵触しないようバッファ表現（`ImageF32x4_RGBA`）経由を検討。

### J. タイムライン キーフレーム収集の O(n) 全マーカ走査

- **事実**: `ArtifactTimelineTrackPainterView.cppm:572` `collectKeyframeAreas`、`:3803` `collectKeyframeConnectionSegments`、`:3941` `collectKeyframeMarkers`、`:6112` `paintEvent`（ProfileTimer "TimelineTrackPaint" 付き）。各プロパティで `getKeyFrames()` コピー＋`interpolateValue`/式評価。`paintEvent` は毎フレーム全可視トラックを描画し収集自体を毎 paint で実行。
- **案**: キーフレーム収集結果を `rebuildMarkerCaches` 同様にフレーム/選択が変わらない限りキャッシュし、playhead のみオーバーレイで動かす。接続セグメント走査を可視時間範囲内サブセットへ限定。
- **未検証**: 収集が「フレーム変更ごと」ではなく「paint ごと」に走るか。

### K. アセット同期インポート ＋ ffmpeg プローブ

- **事実**: `ArtifactProjectService.cppm:1816` `importAssetsFromPaths`（同期）。内部で `copyFilesToProjectAssets`（`:1871` ファイルコピー）、連番検出、互換性チェック、`:2158` `checkImportedAssetCompatibility` が `imageSizeForPath`（画像ヘッダデコード）と `probeVideoFile`（`MediaSource.cppm:239` `avformat_open_input` 同期）を各ファイルごとに実行。
- **案**: 既存の `importAssetsFromPathsAsync`（:2045）モデルを UI 経路から使う。プローブは共有プールへ退避。
- **効果**: 大量ドロップ/一括インポート時のフリーズ解消。
- **未検証**: プローブ順序変更で互換性判定結果が変わらないか。

### L. PSD / 3D / レイヤーファクトリ / アセットブラウザの同期ロード残存

- **PSD インポート**: `ArtifactProjectService.cppm:1920` `importPsdLayersToCurrentComposition`（同期、多層展開＋レイヤービットマップ生成）。
- **レイヤーファクトリ**: `ArtifactLayerFactory.cppm:118/134/158` で `loadFromPath` を同期的に呼ぶ（画像/音声/ SVG）。
- **3D モデル**: `Artifact3DModelLayer.cppm:138` `loadFromFile`→`MeshImporter.cppm:921`（同期）。
- **アセットブラウザ**: `ArtifactAssetBrowser.cppm:1678/1688` で `watcher->waitForFinished()` を同期的に待機（サムネイル生成経路）。`:212` `loadAssetThumbnailFromDisk`（PNG 同期読込）、`:4921` `loadAudioFile`（SimpleWav 同期読込）。
- **案**: 各々を共有プールへ退避、または既存 `prefetchFuture_` パターンの拡張。「スタブ生成→バックグラウンドロード→完了通知」にする。アセットブラウザは `waitForFinished()` を削除し watcher 完了通知のみに。
- **効果**: 各操作直結のフリーズ解消。

### M. Python フックの同期実行

- **事実**: `ArtifactProjectManager.cppm:235` `runProjectHookScript`→`ArtifactPythonHookManager.cppm:141` `py.executeFile`（保存フロー内の `before_project_save` 等）。
- **案**: フック実行を共有プールのワーカーへ。
- **懸念**: フックがプロジェクト状態へ同期副作用を持つ場合の順序保証。

### N. ArtifactPropertyWidget の毎フレーム値反映

- **事実**: `ArtifactPropertyWidget.cppm:1278` および `1317/1320`（`getKeyFrames()` コピー）、`hasKeyFrameAt`（`AbstractProperty.cppm:721` O(n) 線形走査）。
- **案**: `hasKeyFrameAt` を `getKeyFrames()` のキャッシュ結果から引く。値に変化がないエディタは `setValue` をスキップ。式プロパティは候補 C の AST キャッシュで大幅軽減。
- **未検証**: 実際の呼び出し頻度。

### O. RAM Preview キャッシュ集計の毎フレーム全フレームループ

- **事実**: `ArtifactPlaybackService.cppm:254` `buildTimelineCacheVisuals`、`ArtifactTimelineWidget.cppm:4732` `updateCacheVisuals`、`ramPreviewSummary`。
- **案**: 集計を変更があったフレームのみ差分更新、または dirty フラグ付きでサマリ Pixmap を部分再描画。
- **未検証**: `updateCacheVisuals` の実際の呼び出し頻度。

---

## 🟡 低 / 局所

### P. OCIO view transform LUT の CPU 焼き込み / 入力トランスフォーム

- **事実**: `ArtifactOCIOManager.cppm:698-749` `bakeViewTransformLUT` — 3 重ループ `size^3 * 3`（size 最大 256 → 最大約 1600 万要素）。`ArtifactImageLayer.cppm:809` `applyInputTransformToWorkingImage` はソースごとの CPU per-pixel OCIO 入力トランスフォーム。
- **評価**: LUT 生成は 1 回きりなので優先度中〜低。`gpuViewTransformShader` は別経路で存在。`LUT3D GPUComputer`（`ArtifactFinalPostProcess`）の再利用で GPU 化可能か。

### Q. Viewport channel overlay の CPU compose

- **事実**: `composeViewportChannelOverlayImage`（`ArtifactCompositionRenderController.cppm:32373-`）。`readChannel` が `readbackToImage`（同期）で各チャンネル QImage を取得し `scanLine` ループで CPU 合成。
- **案**: 補助チャンネル RTV/テクスチャは既に GPU にあるため、最終表示用テクスチャを GPU で合成し readback 回数を減らす。

### R. PrimitiveRenderer2D の QImage→テクスチャ同期生成

- **事実**: `PrimitiveRenderer2D.cppm:1144-1159` `drawSpriteTransformed(QImage)`。content hash キャッシュ `m_spriteTexCache` ありだがミス時はメインスレッドで同期的に GPU アップロード。
- **案**: アップロードをバックグラウンド化、または `GPUTextureCacheManager` との統合。頻度は低い（キャッシュヒット主体）が初期/コンテンツ変化時にストール。

### S. フォント / OCIO / Localization / バイナリ ロード（起動時限定）

- **事実**: フォントコピー（`copyFontFiles` 同期）、`OCIOConfig::loadFromFile`（同期）、`LocalizationManager::loadFromFile`（同期）、`ArtifactAdhocBinaryLayer.cppm:214` バイナリロード（同期）。
- **案**: 起動シーケンス内で共有プールへ退避、または遅延ロード。
- **未検証**: 実際に顕在化するか。

---

## 優先順位サマリ

| 優先度 | 候補 | 根拠 |
|---|---|---|
| 高 | A 通常合成の GPU ブレンド適用 | 通常コンポジション全体が CPU パスに留まる最大の構造要因 |
| 高 | B プロジェクト load/save 同期 | 既存 `*Async` が未使用、フリーズ顕著、再利用容易 |
| 高 | C プロパティ式 AST 再パース | 既存 AST キャッシュが「あるのに使われていない」純粋な無駄。改修小・効果大 |
| 高 | D 動画フレーム同期デコード待機 | draw/サムネイル直結、UX 直撃 |
| 高 | E RAM Preview 非同期 readback デフォルト化 | 堅牢な非同期基盤が env ゲート＋再生除外で活かされていない |
| 高 | F プロキシ生成 / オートセーブ | UI スレッドで長時間ブロック |
| 中 | G トラックマット GPU 化 | 全ピクセル逐次 CPU ループ |
| 中 | H 表面バッファ GPU 化 | 毎フレームの QImage↔cvMat 往復 |
| 中 | I Project View サムネイル clear | スクロール等で composition 全体レンダリング発火 |
| 中 | J キーフレーム収集 O(n) | 再生時に毎フレーム全マーカ走査 |
| 中 | K アセット同期インポート＋プローブ | 既存 async モデルあり、多数ファイルで顕著 |
| 中 | L PSD/3D/ファクトリ/ブラウザ同期ロード | 操作直結 |
| 中 | M Python フック同期 | 保存フロー内ブロック |
| 中 | N プロパティ値反映 O(n) | 多項目で増大（C で大部分解消） |
| 中 | O RAM preview 集計 | 長尺で顕著 |
| 低 | P OCIO LUT / Q channel overlay / R スプライト / S 起動時ロード | 頻度または影響が局所 |

## 次に確認すべき未検証事項

1. 動画 cache-miss が実際に待機ブロックするか（実測プロファイル必要）。
2. 全 Normal 合成時の GPU ブレンド base pass が本当にスキップされるか。
3. 再生中の RAM プレビー構築がこのコントローラ以外の経路で行われているか。
4. プロパティ式評価・RAM preview 集計の実呼び出し頻度（ProfileTimer 計測）。
5. GPU 化各案における既存 CPU パス固有の合成順序・premultiplied alpha セマンティクスの厳密一致。
6. `MaskPathRasterizerPipeline` が定義のみで未使用の理由。

## 着手順の提案（費用対効果順）

1. **C プロパティ式 AST キャッシュ活用** — 改修最小・効果大。
2. **E RAM Preview 非同期 readback デフォルト化** — env ゲート解除のみ。
3. **B プロジェクト load/save の既存 `*Async` 接続** — 定義済みメソッドの活用。
4. **A 通常合成の GPU ブレンド適用** — 最大恩恵だが変更範囲大。
5. **D / F の非同期化** — UI 直結のフリーズ解消。

実装優先度は C・E・B と判断し、これらは実装済み。残りは個別にスレッド安全性・GPUパイプライン整合性・UI責務を確認してから進める。

---

# 追加調査（オーディオ/スコープ・テキスト・3D/トラッカー・エクスポート・パーティクル/流体・検索）

以下は初期調査（アセットI/O・レンダーパイプライン・CPUエフェクト・タイムラインUI）に続く追加調査。初期調査と同様に事実と推測を分離する。

## 🔴 高優先（追加分）

### T. GPUスコープ経路が実装済みなのに未接続（最重要・追加）

- **事実**: `ArtifactCore/src/Graphics/Compute/ScopeComputer.cppm`（Vectorscope/Waveform/Parade コンピュート）と `Histogram.cppm`（Luminance/RGB/Statistics）は実装済みだが `ArtifactCore/CMakeLists.txt:52-53` で**ビルド除外**。HLSL（`ScopeWaveform.ixx` 等）と `ColorScopes.ixx:278/355/396` の `render*FromBins` も**呼び出し元ゼロ**。4スコープWidgetは全てCPU実装。
- **問題**: 全スコープが「①フルフレームGPU→CPUリードバック（約8MB/フレーム）→ ②CPUピクセルループ」を実行。GPU化ならビンバッファ（数百KB）のみで済む。
- **案**: CMake除外解除＋Widget接続。`ScopeComputer::readbackResults()` は fence待ち失敗時に再試行のみ・ステージング毎回確保の問題があるため、`ArtifactIRenderer::readbackTextureViewToImageAsync`（リングバッファ+非同期）方式へ書き換え。
- **懸念**: 除外解除時にビルドが通るかは未検証。除外理由の記録なし。

### U. Waveform/Paradeスコープの非同期実装に3つの機能バグ

- **事実**: `WaveformScopeWidget.cppm:282` / `ParadeScopeWidget.cppm:450` で `connect` が非同期起動関数の**内部**にあり毎回累積。`:291`/`:459` で `watcher_->parent()` を参照するが watcher は親なし生成（`:74`/`:88`）のため `update()` が**一度も呼ばれず再描画されない**。Parade の `YCbCr` 非同期パスは `cbImg`/`crImg` が未初期化（`:553-560`）。
- **案**: connect をコンストラクタに1回だけ移動、watcher にWidget親付き生成、共有プール指定（`&sharedBackgroundThreadPool()`）、デッドコード `computeScopeImages()` 削除、係数不整合（BT.601/709）解消、worker内 `pixel()/setPixel()` を `scanLine` 化。
- **効果**: 既存非同期化の骨格は正しいが現在ほぼ壊れている。修正コスト数行。

### V. captureCurrentFrameImage の同期GPUリードバックをタイマーでUI実行

- **事実**: `ArtifactColorSciencePanel.cppm:818`（180ms）・`ArtifactCompositionEditor.cppm:10861`（150ms）から `captureCurrentFrameImage()`（`cppm:17596`）→ `readbackTextureViewToImage`（`:1571` `fence->Wait` のブロッキング）を恒常実行。Editor側には `cacheKey()` 同一フレームガードがない。
- **案**: 既存 `readbackTextureViewToImageAsync`（`ArtifactIRenderer.cppm:2069`）へ置換。Editor側に cacheKey ガード追加。非表示時はタイマー停止（AudioMixerWidget の show/hide 制御が手本）。

### W. トラッキング解析の同期実行

- **事実**: `ArtifactCore/src/Tracking/MotionTracker.cppm`（2387行）、`CameraTracker.cppm`、`Track/NccTracker.cppm`。呼び出し元 `ArtifactCompositionRenderController.cppm:24948/25159/25277`、`ArtifactLayerMenu.cppm:3744`。各フレーム NCC/Planar/Homography マッチ＋カメラsolve、前処理で `renderToQImage` をフレームごとリードバック。trackAll で数秒〜数十秒のUIフリーズ。
- **案**: `ThreadHelper` 共有プールへオフロード、進捗は既存 progress callback でポーリング。
- **懸念**: トラッカーとコンポジション状態のスレッド安全性、途中キャンセル。

### X. パーティクル goToFrame の二重呼び出し＋全期間再シミュレーション

- **事実**: `ArtifactParticleGenerator.cppm:1168` `goToFrame()` は毎回 `reset()`＋固定1/120秒刻みで targetTime までループ（30fps/フレーム300なら1200ステップ）。さらに `ArtifactCompositionViewDrawing.cppm:1820` と `ArtifactParticleLayer.cppm:319` の draw 内で**同一フレームに2回フルシミュレーション**が走る。
- **案**: `lastSimulatedFrame_` キャッシュ＋前進差分シーク（決定論は固定刻みで担保済み）。Nフレームおきスナップショットで逆方向にも対応。
- **効果**: シーク時コストが1/1200オーダーまで低下。

### Y. プロジェクト検索・フィルタのUIスレッド同期I/O/再計算

- **事実**: `ArtifactProjectManagerWidget.cppm:1972` `matchesAdvanced` 内で `QFileInfo::exists()` を全行（シーケンスは数百〜数千ファイル）同期実行。`:2001`/`:1936` で行ごとに `QRegularExpression` コンパイル。`:1828` `filterAcceptsRow` が子孫再帰で O(n·d) 重複評価。`countAcceptedRows`（`:1839`）で全ツリーをもう1周。`:6734` 検索でデバウンスなし `invalidateFilter()`×2＋`expandAll()` をキーストロークごと。
- **案**: 存在判定を `missingAssetPaths_`（既存 `unusedAssetPaths_` と同型）へ事前キャッシュ。regex/typeFilter を `parseExpression()` で1回コンパイル。メモ化テーブルで再帰除去。デバウンス（既存 `QTimer::singleShot` 踏襲）＋invalidateFilter 一本化＋expandAll 抑制。
- **効果**: `missing:true` フィルタで数秒→1ms未満、検索の体感改善最大。

### Z. 失敗フレーム検出の全フレームデコード（右クリック時）

- **事実**: `ArtifactRenderQueueManagerWidget.cppm:2247`→`ArtifactRenderQueueService.cppm:2053` で連番全フレームを右クリック時に同期フルデコード＋`pixelColor()` 16×16サンプル。
- **案**: 共有プールへ退籍、判定を「存在＋サイズ0」のみに簡略化、必要時のみ黒フレーム判定。

### AA. 動画プロキシ生成の `waitForFinished(-1)`（UIタイマー駆動）

- **事実**: `ArtifactVideoLayer.cppm:2271` `process.waitForFinished(-1)` を `ArtifactProjectManagerWidget.cppm:6576` のUI `QTimer`（5ms）内で実行＝タイムアウト無しでUI完全停止。
- **案**: `QProcess` を非ブロッキング化、または共有プール＋`QtConcurrent::run`（`runExternalRendererJob` の100msポーリング＋キャンセル方式を流用）。ffmpeg を `h264_nvenc`+`scale_cuda` へ。

### AB. 出力設定ダイアログの `ffmpeg -encoders` ×4 同期実行

- **事実**: `ArtifactRenderOutputSettingDialog.cppm:223-226` で同一コマンドを4回起動、`waitForFinished(5000)`（最悪20秒フリーズ）。`ffmpegExeSupportsEncoder` は `RenderQueueService.cppm:197` に重複実装。
- **案**: 結果を static キャッシュ1回取得＋文字列検索。初回取得のみ共有プールへ。

### AC. MFR（マルチフレームレンダー）の巨大ミューテックスによる実質直列化

- **事実**: `ArtifactRenderQueueService.cppm:5800` `compositionFrameStateMutex_` が `renderSingleFrame` 全体（GPU描画・readback・crop・CPU合成）を覆う。numWorkers=4 起動でも描画はシリアル。
- **案**: `cloneCompositionSnapshot`（`:3754`）をワーカー数ぶん用意し per-worker 化でロック撤廃。
- **懸念**: クローンは JSON 経由で重い。隠れた共有状態の競合要確認。

### AD. AOV（マルチチャンネル）出力の8回同期リードバック

- **事実**: `ArtifactIRenderer.cppm:2946-2964` で beauty/emission/objectId/materialId/albedo/normal/velocity/depth を個別に同期リードバック（フレームあたり最大8回のGPU同期＋1GB級転送）。
- **案**: 1コマンドで全AOVを複数ステージングへCopy→fence1回→まとめてMap。compute shaderで必要チャンネルのみRGBA32Fへパッキングし転送量削減。

## 🟠 中優先（追加分）

- **AE VectorScopeWidget**: `VectorScopeWidget.cppm:42-98` でフルフレーム `convertToFormat`＋25万回の `pixel()/setPixel()` をUIスレッド毎回実行、非同期化ゼロ。GPU化（T）または `scanLine` 化＋`QFutureWatcher` 移植。
- **AF HistogramWidget**: `HistgramWidget.cppm:81-185` で50万回ループ＋最大8パス構築。GPU集計＋パス1回化。
- **AG ArtifactContentsViewer の `pixelColor()` ループ**: `ArtifactContentsViewer.cppm:164` で10万回の最遅アクセサ。既存 `WaveformScopeWidget` へ差し替え。
- **AH AudioMixerWidget**: `AudioMixerWidget.cppm:634` 33msタイマーでバス数ぶん FFT をUI実行、毎回ヒープ確保＋1024点で513ビンの94%破棄。`AudioAnalyzer` に作業バッファを持たせ、`ArtifactCore::AudioAnalyzer`（radix-2）の FFTサイズ256へ、共有プールへ非同期化。
- **AI Timeline 波形生成**: `ArtifactTimelineWidget.cppm:486-534` `buildAudioWaveformForLayer` で全PCM走査＋映像レイヤーは全長音声同期デコード。`:7892` `waveformPreviewSummary()` が2回目のフルスキャン。非同期化（`QFutureWatcher<CachedAudioWaveform>`）＋キャッシュ設計（トリムで再計算しないようフル長ピークを1度計算）。
- **AJ テキスト再評価/再ラスタ**: `ArtifactTextLayer.cppm:2261` `toQImage()`→`updateImage()`→`updateGlyphEvaluation` をUI同期実行。アニメータ有効時は CacheKey 早戻り条件を満たさず毎フレーム全文再シェイプ＋再ラスタ。静的グリフ配置をキャッシュ。GPU経路でも `DiligentImmediateSubmitter.cppm:160` `shapeGlyphsForRender`＋`makeFont` を draw 毎に呼ぶ（メモ化要）。
- **AK テキスト getThumbnailAtFrame 毎フレーム再ラスタ**: `ArtifactAbstractComposition.cppm:4805` はクロスフレームキャッシュを意図的に bypass。
- **AL パペット MLS 変形**: `OpenCVPuppetEngine.cppm` `calculateMLSDeformation` をドラッグ中にUI実行。zDepth と同様に `Parallel::For` 化＋共有プールオフロード＋デバウンス。
- **AM RenderScheduler の未使用**: `ArtifactRenderScheduler.cppm` は優先度・キャンセル・進捗・重複排除・スレッド数適応を実装済みだがレンダーキューから未使用。ワーカースレッド群をこれに置換するだけで既存機能が手に入る。
- **AN 連番書き出しの直列I/O**: `ArtifactRenderQueueService.cppm:6246-6431` で毎フレーム `scaled(SmoothTransformation)`＋同期OIIO書き込みを1スレッド直列。`AsyncImageWriterManager` が実装済みだが未使用。
- **AO GPUパスの同期リードバック**: `:5875` が非同期API（`readbackToImageAsync`）を使わず同期版。パイプライン化でGPUアイドル削減。
- **AP NVENC でも CPU `sws_scale`**: `FFmpegEncoder.cppm:298` でRGBA→YUVをCPU変換（PCIe2往復）。CUDA/D3D11 interop または compute でNV12化し転送量62%削減。
- **AQ GPU描画後のCPU(OpenCV)最終エフェクト**: `ArtifactCompositionViewDrawing.cppm:1092-1144` で QImage→cvMat→float32→…→QImage をフレームごと4回以上変換。`ArtifactFinalPostProcess` のGPU経路へ。
- **AR ROI/解像度プリセットのCPUコスト＋crop二重適用疑い**: `:5876-5881` で crop を再 `copy`、かつGPUレンダラにROI原点が渡っていない疑い（要検証）。ROIをビューポート/シザーに渡して必要領域のみ描画。
- **AS LayerPanel フィルタ判定の getLayerPropertyGroups 呼び出し**: `ArtifactLayerPanelWidget.cppm:2493` でレイヤーごと218プロパティ再構築。検索用名前文字列をキャッシュし呼び出し除去（構造キャッシュの無効化リスクを避ける安全案）。

## 🟡 低 / 局所（追加分）

- **AT FluidSolver2D 並列パス未配線**: `FluidSolver2D.cppm` `useParallelPath()` は定義のみ・呼び出しゼロ。行分割ラムダ（`advectRows`）まで用意済み。`advect`/`project` は即 `Parallel::For` 可。`linSolve` は red-black Gauss-Seidel 化が必要。
- **AU FlockingEffector O(n²)**: `ArtifactParticleGenerator.cppm:171` 全ペア総当り。同ファイルの空間グリッド（`:887`）を共有。
- **AV applySelfCollisionBroadPhase の std::map**: `:865` 毎ステップ再構築。平坦グリッド＋13近傍へ（決定論的順序維持必須）。
- **AW 粒子ローカル積分の Parallel::For 化**: `:996-1000`（エフェクタは粒子ローカルで競合なし）。
- **AX ソフトレンダの QPainter ループ内構築 / QImage::pixel**: `:1475`、`:1443`。
- **AY CloneGenerator 棄却サンプリング**: `:492` 密度高で O(n²·64)。signature キャッシュ有効。
- **AZ AbstractGeneratorEffector の signature キャッシュ欠如**: `:99` パラメータ不変でも毎回再生成。Inspector の `lastComponentPropertyStateSignature_` パターンを流用。
- **BA CommandPalette の toLower 毎回確保 / MRU 線形探索**: `ArtifactCommandPaletteWidget.cppm:207-208,312-314,539`。小文字化済みキャッシュ＋`QHash` 化。
- **BB AudioAnalyzer O(N²) DFT / AudioSyncTools O(N²) 相互相関 / AudioSpectrum 誤実装**: 現在未使用だが呼ばれた瞬間に致命的。`ArtifactCore::AudioAnalyzer`（radix-2）へ委譲。
- **BC メッシュ import / テキストアレイアウト / Rig2D / CPUフラスタムカリング不在**: 優先度低（個別の重さは中〜軽、または現状ボトルネック外）。
- **BD renderSingleFrameGPU のデッドコード/未定義参照**: `ArtifactRenderQueueService.cppm:3902` で別スコープの `output` を参照。要整理・要ビルド確認。

## 既存の未使用GPU/非同期資産（要接続）

追加調査で判明した「実装済みだが未配線」の資産（接続するだけで恩恵）:

| 資産 | 場所 | 状態 |
|---|---|---|
| ScopeComputer（Vectorscope/Waveform/Parade コンピュート） | `ArtifactCore/src/Graphics/Compute/ScopeComputer.cppm` | CMakeビルド除外 |
| HistogramComputer（Luminance/RGB/Statistics） | `ArtifactCore/src/Graphics/Compute/Histogram.cppm` | CMakeビルド除外 |
| ParticleCompute（Boids/パーティクル GPU シミュレーション） | `ArtifactCore/src/Graphics/ParticleCompute.cppm` | `Artifact/` から参照ゼロ |
| BoidsCompute | `ArtifactCore/src/Graphics/BoidsCompute.cppm` | 参照ゼロ |
| AsyncImageWriterManager（連番非同期書き出し） | `ArtifactCore/src/IO/Image/AsyncImageWriterManager.cppm` | 呼び出し元ゼロ |
| ArtifactRenderScheduler（優先度ジョブキュー） | `Artifact/src/Render/ArtifactRenderScheduler.cppm` | レンダーキューから未使用 |
| RendererQueueManager / MFRDispatcher | `ArtifactCore/src/Render/RendererQueueManager.cppm` | 片方は空回り |

また `ArtifactCore::Parallel::For` は200以上のファイルで採用済みだが、**パーティクル・クローン・流体の3モジュールだけが未採用**という不均一が確認された。

## 全体を通じた最重要発見

1. **GPU実装が複数「完成済みのまま死んでいる」**（スコープ、パーティクル、Boids）。接続するだけで大きな恩恵。
2. **非同期基盤は既に整っているのに使われていない**（RenderScheduler、AsyncImageWriterManager、非同期readback、共有プール）。
3. **既存の良い最適化パターン（signatureメモ化、キャッシュ、QTimer::singleShot、QtConcurrent+QFutureWatcher）が一部の箇所で使われ、別の同型箇所で使われていない**——横展開だけで多くの改善が可能。
4. **決定論性（パーティクル・流体・空間分割）の維持**がGPU化/並列化の最大の制約。

## 全体優先順位（初期＋追加を統合）

### 最優先（接続/修正だけ・既存資産活用）
1. T GPUスコープ経路の接続（CMake除外解除＋Widget）
2. U スコープ非同期バグ3件修正
3. X パーティクル goToFrame 二重呼び出し除去＋キャッシュ
4. Y 検索・フィルタの同期I/O/再計算排除（デバウンス含む）
5. C プロパティ式 AST 再パース（初期）
6. E RAM Preview 非同期 readback デフォルト化（初期）
7. B プロジェクト load/save の既存 `*Async` 接続（初期）

### 高（非同期化でフリーズ解消）
8. W トラッキング解析
9. AA 動画プロキシ `waitForFinished(-1)`
10. Z 失敗フレーム検出
11. I Project View サムネイル clear（初期）
12. AI Timeline 波形生成
13. AL パペット MLS
14. D 動画デコード待機（初期）
15. F プロキシ/オートセーブ（初期）

### 高（構造的GPU化・並列化）
16. A 通常合成の GPU ブレンド適用（初期）
17. AC MFR 直列化解除
18. AD AOV 8回リードバック
19. AJ/AK テキスト再ラスタ・キャッシュ
20. G トラックマット GPU（初期）

### 中〜低
- 各種 `Parallel::For` 化（流体・Flocking・self-collision・粒子積分）、AO/AP/AQ 出力パイプライン、AE〜AS その他、低優先項目（AT〜BD）。
