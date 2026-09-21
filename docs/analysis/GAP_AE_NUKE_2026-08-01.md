# AE / Nuke 機能ギャップ分析

**日付**: 2026-08-01
**最終更新:** 2026-09-21
**比較対象**: Adobe After Effects 2025/2026 + Nuke 15（参考）

---

## スコア基準
- 🟢 **85-100%**: 機能的にほぼ同等
- 🟡 **50-84%**: 部分的実装、主要機能はある
- 🟠 **15-49%**: 基礎のみ、実用には不足
- 🔴 **0-14%**: ほぼ未着手

---

## 1. レイヤー種別

| レイヤー種別 | 実装率 | アーティファクト | ギャップ |
|------------|--------|-----------------|----------|
| 画像 | 🟢 95% | ArtifactImageLayer | 連番画像・RAW対応は部分的 |
| 動画 | 🟡 70% | ArtifactVideoLayer | デコード安定性・キャッシュに課題あり |
| 平面（Solid） | 🟢 95% | ArtifactSolid2DLayer | 十分 |
| テキスト | 🟡 50% | ArtifactTextLayer | データモデルはあるがTextツール未実装 |
| シェイプ | 🟡 60% | ArtifactShapeLayer | Solo ViewとメインVPの頂点/tangent/segment・角丸/星ハンドル・polygon編集・一部operator HUD/ハンドル・KF評価を実装。残りはプリセット/複数シェイプ/グループ、Merge本物化、SVG import、Taper/Wave完全化、runtime検証。詳細は下記追補2026-09-10 |
| カメラ | 🟢 80% | ArtifactCameraLayer | ステレオ・被写界深度あり |
| ライト | 🟡 60% | ArtifactLightLayer | Directional/Point/Spot/Ambient/Area+Gobo、LightLink、ShadowMap PCFあり。実機検証は残 |
| Null | 🟢 90% | ArtifactNullLayer | 十分 |
| 調整レイヤー | 🟢 85% | ArtifactAdjustableLayer | エフェクトパイプライン動作 |
| 3D モデル | 🟡 65% | Artifact3DLayer | FBX/OBJ/glTFはufbx経由で読込あり(`MeshImporter.cppm:354,2141,2149`)。USDは.usdaのみ、.usdc/usdzはスタブ(`:2188`)。Alembic/Assimpなし。ギズモ未完成 |
| パーティクル | 🟡 50% | ArtifactParticleLayer | 基本機能あり |
| フォームパーティクル | 🟡 45% | ArtifactFormParticleLayer | 基本機能あり |
| ペイント | 🟠 30% | ArtifactPaintLayer | BrushStrokeあり。ブラシUI・パネル未実装 |
| オーディオ | 🟡 50% | ArtifactAudioLayer | 波形表示・スペクトラムあり |
| プロシージャル3D | 🟡 55% | ArtifactProcedural3DLayer | Terrain/PathTubeに加えTextExtrude(押出し3Dテキスト+ベベル)実装(2026-09-21)。デフォーマ(Bend/Twist/Displace相当)なし |
| グループ | 🟢 85% | ArtifactGroupLayer | ネスト対応 |
| プリコンポ | 🟢 80% | ArtifactCompositionLayer | タイムリマップあり |
| **リグ** | 🔴 10% | **なし（新規追加済み）** | ボーン/メッシュ/IKあり。UI・ツールなし |

---

## 2. エフェクト

| カテゴリ | 実装率 | 詳細 |
|----------|--------|------|
| 色調補正（Exposure, Levels, Curves, Hue/Sat） | 🟡 60% | 基本的なものはあるが数が少ない |
| ぼかし（Gaussian, Box, Radial） | 🟡 50% | BlurShaders.hlsl に数種 |
| ノイズ（Noise, Grain） | 🟠 30% | FilmEffects に一部あるがバグあり |
| トランジション | 🟠 25% | RippleTransition のみ（バグあり） |
| 歪み（CornerPin, Bulge, Twirl） | 🟡 50% | CornerPinはCPU実装あり(`ArtifactCornerPinEffect.cppm:92`、Homography+warpPerspective)。GPUなし・特異値判定なし。Bulge/Twirlは不足 |
| キーイング（Keylight, Extract） | 🟡 50% | ChromaKey(`ChromaKey.cppm:104`)+LumaKey(`LumaKey.cppm:24`)+IBK(`Keying/IBKKeyer.cppm:53`)はCPU+GPUあり。Keylight/Primatte/Ultimatte級は未着手 |
| カラコレ（Lumetri, Colorista相当） | 🟠 20% | FloatColor + LUTのみ |
| シミュレーション（Shatter, Wave World） | 🔴 5% | SandSim2D, Fractureのみ |
| スタビライザー | 🟠 30% | `Video/Stabilizer.cppm:225`+`ArtifactStabilizer.cppm:57`はdetect/track/estimate/smoothあり。Batchは単一QImageのみで動画I/Oなし。`WarpStabilizer`名称は不在 |
| 3D チャンネルエフェクト | 🟡 60% | Depth/Normal/Velocity/ObjectId/MaterialId/Albedo/Emission/Custom+Cryptomatte+Position(XYZ)/UVあり(2026-09-21追加)。残りは native Deep multi-sample |
| **プリセットシステム** | 🟠 25% | ArtifactEffectPreset あり。ブラウザなし |

---

## 3. アニメーション

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| キーフレーム（追加/削除/編集） | 🟢 80% | AnimatableTransform3D で動作 |
| グラフエディタ | 🟢 80% | ArtifactCurveEditorWidget あり |
| 補間（Linear/Bezier/Hold） | 🟢 85% | InterpolationType あり |
| イーズ（Easy Ease 等） | 🟡 50% | プリセット補間のみ |
| **エクスプレッション** | 🟡 55% | ExpressionEvaluator/ExpressionParser あり |
| 式コパイロット | 🟢 80% | ArtifactExpressionCopilotWidget あり |
| モーションブラー | 🟡 60% | ArtifactMotionBlurPass あり |
| タイムリマップ | 🟢 80% | キーフレーム編集可能 |
| **モーションスケッチ** | 🟡 60% | ArtifactMotionSketchTool あり |
| ワイグル（Wiggle） | 🔴 0% | 未着手 |
| ループ式（loopOut 等） | 🔴 0% | 未着手 |
| **パペット** | 🟡 50% | ArtifactPuppetTool + OpenCVPuppetEngine あり |
| **リグ** | 🔴 10% | データモデルのみ |

---

## 4. 3D

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| 3D レイヤー | 🟡 60% | 描画されるがVP操作にバグ |
| 3D カメラ（1ノード・2ノード） | 🟡 50% | ArtifactCameraLayer あり |
| 3D ライト（Point/Spot/Parallel/Ambient） | 🟡 65% | Directional/Point/Spot/Ambient/Area+Gobo(`Transform/Light.ixx:14`)、ShadowMap PCF(`MeshRenderer.cppm:823`)、LightLinkあり |
| マテリアル（PBR） | 🟡 60% | ArtifactCore::Material に baseColor/metallic/roughness/normal/emission/occlusion |
| 3D 軸ギズモ | 🟢 80% | Artifact3DGizmo で動作 |
| 3D フレームギズモ | 🟠 40% | 描画はされるがドラッグ無反応（原因特定済み） |
| 深度マット | 🟡 50% | DepthMask あり |
| 環境マップ | 🟡 60% | `ArtifactEnvironmentMapLayer`+IBL畳込(`MeshRenderer.cppm:3177,3195`)あり。実機検証は残 |
| 3D レンダラー | 🟢 80% | Diligent Engine で高度な GPU レンダリング |
| **3D 地面グリッド** | 🟡 60% | `ArtifactGridSystem.ixx:273`+`drawThreeDimensionalGroundGrid`(RenderController:44372)あり。実機検証は残 |
| **ScanlineRender相当** | 🔴 5% | Nuke言及のみ。Artifact/ArtifactCoreにScanlineRender/Render3D実装なし |
| **Project3D/カメラ投影貼り込み** | 🔴 0% | `Project3D`は0件。カメラ射影→2D貼り込みブリッジなし |
| **ジオメトリ編集/UV** | 🔴 5% | EditGeo/TransformGeo/UV Unwrapなし。UV生成はTerrainのみ |
| **3D ビューポートナビゲーション** | 🟡 50% | ViewOrientationNavigator あり |

---

## 5. テキスト

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| テキストレイヤー作成 | 🟠 20% | データモデルのみ |
| VP 上テキスト編集 | 🔴 5% | 未着手 |
| Text Animator | 🔴 0% | 未着手 |
| フォント管理 | 🟡 50% | ArtifactFontPickerWidget あり |
| 段落テキスト | 🔴 0% | 未着手 |
| テキストボックス | 🟡 50% | TextGizmo でリサイズのみ可能 |
| パーフレーム文字アニメーション | 🔴 0% | 未着手 |

---

## 6. マスク・ロトスコープ

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| ベジェマスク作成 | 🟡 60% | Pen tool で動作 |
| マスク頂点編集 | 🟢 80% | ドラッグ・ハンドル編集可 |
| マスクモード（Add/Subtract/Intersect/Difference） | 🟢 85% | LayerMask + MaskMode |
| マスクフェザー | 🟡 60% | 値はあるがVP上調整不可 |
| マスク不透明度 | 🟡 50% | 値はあるがVP上調整不可 |
| マスク拡張 | 🟡 50% | Expansion プロパティあり |
| マスク反転 | 🟡 50% | Inverted プロパティあり |
| マスクアニメーション | 🟡 50% | MaskPathKeyframeSnapshot あり |
| マスク複数選択 | 🔴 0% | 未着手 |
| マスク複製 | 🔴 0% | 未着手 |
| **ロトブラシ** | 🔴 0% | 未着手 |
| **コンテンツアウェア塗りつぶし** | 🔴 0% | 未着手 |

---

## 7. トラッキング・整列

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| ポイントトラッカー | 🟡 55% | ArtifactPointTrackerTool + MotionTracker (NCC) |
| トラッカー適用（Null） | 🟢 80% | applyTrackingResult あり |
| 3D カメラトラッカー | 🟠 30% | `Tracking/CameraTracker.cppm:166`(PyrLK+Essential+recoverPose、BAなし)+`ArtifactCameraTrackerTool.cppm:32`(solve→Camera/Null生成のみ、ポーズKF書込なし)+PnP(`MotionTracker.cppm:379`) |
| **プレーナートラッカー** | 🟡 55% | `Tracking/PlanarTracker.cppm:10`(PyrLK+Homography RANSAC)+CornerPin連携(`ArtifactPointTrackerTool.cppm:293`)あり |
| **ワープスタビライザー** | 🟠 30% | 上記エフェクト節の通り部分実装。動画I/Oなし。`WarpStabilizer`名称は不在 |
| レイヤー整列 | 🟡 60% | LayerAlignment あり |
| ガイド線 | 🟡 60% | SnapLine/SnapLabel あり |

---

## 8. コンポジット

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| ブレンドモード（30種+） | 🟢 90% | BlendModes が充実 |
| トラックマット | 🟡 65% | LayerMatte あり |
| プリコンポーズ | 🟢 80% | 動作 |
| コラップストランスフォーム | 🟡 50% | 部分的 |
| エフェクトマスク | 🟡 50% | レイヤーマスクで代用可能 |
| ステンシル | 🟠 30% | ブレンドモードとしてStencilAlpha/Lumaあり(`ColorBlendMode.cppm:200`、CPU/GPU/Software)。Nuke的Stencilオペレータなし |
| 深度コンポジット | 🟡 50% | DepthMask+Deep基盤あり。Deepは`DeepImageBuffer.ixx:98`+`DeepComposite.hlsl:34`+OpenEXR deep R/W(`OpenEXR.cppm:184,303`)あり。RenderQueueはBeauty+Depthから1spp生成のみでnative multi-sample・制作UIなし |

---

## 9. レンダリング・出力

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| レンダーキュー | 🟢 80% | ArtifactRenderQueueService + Manager |
| 画像シーケンス出力 | 🟢 80% | PNG/EXR/TIFF 他 |
| 動画出力（FFmpeg） | 🟢 80% | H.264/H.265/ProRes |
| RAM プレビュー | 🟢 80% | ArtifactRamPreviewController |
| **選択的レンダーキュー** | 🟠 20% | 設計書あり、未実装 |
| マルチAOV出力 | 🟡 70% | Depth/Normal/Velocity/ObjectId/MaterialId/Albedo/Emission/Custom+Cryptomatte+Position/UVあり(2026-09-21)。native Deep multi-sampleなし |
| バッチレンダリング | 🟡 50% | ArtifactBatchRenderer あり |
| ネットワークレンダリング | 🟠 20% | farm 設定のみ |

---

## 10. カラーマネジメント

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| Float32 リニアワークフロー | 🟢 85% | ImageF32x4_RGBA が中心 |
| sRGB ↔ Linear 変換 | 🟢 85% | ColorTransferFunction |
| OCIO Config | 🟢 85% | `OCIOConfig` — フル実装。ACES/sRGB/Rec709/Rec2020プリセット + 外部OCIOファイル読み込み |
| OCIO Manager | 🟢 85% | `ArtifactOCIOManager` — プリセット切替、working space / display / view / looks 管理、ColorScienceManager 連携 |
| OCIO View Transform 適用 | 🟢 80% | `applyViewTransformToImage` / `applyInputTransformToWorkingImage` |
| ColorSciencePanel | 🟢 80% | `ArtifactColorSciencePanel` — OCIO プリセット/display/view の UI コンボボックス完備 |
| LUT（3D LUT） | 🟡 60% | LUTLoader + LUTWriter |
| カラースコープ | 🟡 50% | ColorScopes / Histogram / VectorScope / Waveform / Parade |
| カラーカーブ | 🟡 60% | ColorCurves あり |
| 画像入出力の色空間管理 | 🟡 70% | ImageExporter / FFmpegEncoder / MediaImageFrameDecoder で OCIO 連携 |
| ColorScienceManager 連携 | 🟢 80% | `syncToColorScienceManager()` で双方向同期 |
| 外部 OCIO config.ocio 読み込み | 🟢 80% | `loadFromFile(path)` 対応 |

---

## 11. パフォーマンス

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| GPU レンダリング | 🟢 85% | Diligent Engine (DX12/Vulkan) |
| マルチスレッドレンダリング | 🟡 60% | Parallel::For + ThreadPool |
| ディスクキャッシュ | 🟡 50% | ArtifactFrameCache |
| GPU テクスチャキャッシュ | 🟢 80% | GPUTextureCacheManager |
| プロキシ | 🟡 50% | プロキシ解像度あり |
| **マルチフレームレンダリング** | 🔴 5% | 未着手 |
| パフォーマンスモニター | 🟡 50% | ArtifactPerformanceMonitor |

---

## 12. UI/UX

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| ドッキングパネル | 🟢 90% | QDockWidget ベース |
| タイムライン | 🟢 80% | フル機能（トラック、キーフレーム、スクラブ） |
| カーブエディタ | 🟢 80% | ArtifactCurveEditorWidget |
| ドープシート | 🟢 75% | ArtifactDopeSheetWidget |
| ツールバー | 🟢 80% | 22 ツール |
| インスペクタ | 🟢 85% | ArtifactInspectorWidget |
| プロジェクトパネル | 🟢 80% | ArtifactProjectManagerWidget |
| エフェクトブラウザ | 🟠 20% | 検索のみ |
| ポーズライブラリ | 🔴 0% | 設計書あり、未実装 |
| **ワークスペース保存** | 🔴 0% | 未着手 |
| ショートカットエディタ | 🟡 50% | ContextShortcutProvider あり |

---

## 13. スクリプト・拡張

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| 式エンジン | 🟡 55% | ExpressionEvaluator あり |
| スクリプトVM | 🟡 50% | BuiltinScriptVM + ArtifactScript |
| Python フック | 🟡 50% | ArtifactPythonHookManagerWidget |
| **プラグインSDK** | 🟠 25% | PluginCommon あり。ドキュメントなし |
| AI ツール DSL | 🟠 20% | DSL パーサーあり。コマンド実行はスタブ |

---

## 14. オーディオ

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| オーディオレイヤー | 🟡 60% | ArtifactAudioLayer |
| 波形表示 | 🟢 80% | AudioWaveform + AudioPreviewWidget |
| スペクトラム表示 | 🟢 80% | AudioSpectrum + SpectrumAnalyzerWidget |
| オーディオエフェクト | 🟡 50% | Reverb/Compressor/Delay/Chorus/EQ 他 |
| VST3 サポート | 🟠 30% | VST3Interfaces あり |

---

## 15. Nuke 比較（簡易）

| Nuke 機能 | 実装率 | 備考 |
|-----------|--------|------|
| ノードグラフ | 🟠 25% | ArtifactCompositionGraphWidget あり。限定的 |
| ノードベースコンポジット | 🔴 5% | レイヤーベース |
| マルチチャンネル / AOV | 🟡 70% | Depth/Normal/Velocity/ID/Albedo/Emission/Custom+Cryptomatte+Position/UVあり。native Deep multi-sampleなし |
| キーヤー（Primatte/Ultimatte級） | 🟠 30% | Chroma/Luma/IBKあり。Keylight/Primatte/Ultimatte級は未着手 |
| 3D 空間コンポジット | 🟡 50% | ArtifactComposition3D+Phase1A-1K(共有depth/Light/PBR/Precomp GPU)あり。ScanlineRender/Project3Dなし |
| ノードのコピペ | 🔴 0% | レイヤー間コピーで代用 |
| Dope Sheet / Curve Editor | 🟢 80% | 共通 |

---

## 16. 総括

| カテゴリ | 総合スコア | 最重要ギャップ |
|----------|-----------|---------------|
| レイヤー種別 | 🟡 65% | リグレイヤーUI・ツール不在。3Dモデル受入はFBX/OBJ/glTF可、USD部分、ABCなし |
| エフェクト | 🟡 50% | Chroma/Luma/IBK・CornerPin CPU・LensDistortion CPU+GPUあり。Keylight級・Bulge/Twirl不足 |
| アニメーション | 🟡 60% | Wiggle/loopOut/smooth式 |
| 3D | 🟡 55% | フレームギズモバグ、ScanlineRender/Project3D・EditGeo/UVなし |
| テキスト | 🟠 20% | Text Tool・Text Animator 未着手 |
| マスク/ロト | 🟡 50% | ロトブラシ・コンテンツアウェア |
| トラッキング | 🟡 55% | プレーナー実装済み。残りはBA・ポーズKF・スタビ動画I/O |
| コンポジット | 🟡 60% | Stencilオペレータ・Deep native・制作UIなし |
| レンダリング | 🟡 65% | 選択的キュー・マルチフレーム |
| カラーマネジメント | 🟢 75% | OCIO フル実装。外部config読込・プリセット・display/view UI完備 |
| パフォーマンス | 🟡 60% | マルチフレームレンダリング |
| UI/UX | 🟡 65% | ワークスペース・ポーズパネル |
| スクリプト/拡張 | 🟠 30% | プラグインSDK未完成 |
| オーディオ | 🟡 55% | VST3 部分的 |

**全カテゴリ平均**: 🟡 ~55%

**最重要P0（クリティカルバグ）**: 成熟度分析で報告された メモリリーク、use-after-free、double-free、null dereference、スタブ多数。機能以前に安定性の課題がある。

**最重要P1（機能差）**: Keylight/Primatte級、Bulge/Twirl等の歪み、テキストツール不在、スタビ動画I/O、ScanlineRender/Project3D、Deep native・制作UI、Position/UV AOV、Alembic/フルUSD。OCIOは実装済みのため除外。

---

## 追補 2026-09-10: シェイプ再評価（20%→60%）

再棚卸し根拠: `Artifact/include/Layer/ArtifactShapeLayer.ixx`(375行)、`Artifact/src/Layer/ArtifactShapeLayer.cppm`(7572行)、`ArtifactCore/include/Shape/`(8ファイル)、Solo View分割モジュール(`LayerEditorShape*`)、メインVP(`ArtifactCompositionRenderController.cppm`/`ArtifactCompositionRenderOverlay.cppm`)、`AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md:328-329`。

### 実装済み（AE対比で加点）
- プリミティブ7種(Rect/Ellipse/Star/Polygon/Line/Triangle/Square)、fill単色+2色gradient(Linear/Radial/Conic)、stroke(幅/cap/join/align/dash/dashOffset/taper/gradient)、fillRule(Winding/EvenOdd)
- `CustomPathVertex{pos/inTangent/outTangent/smooth}`+open/closed、頂点KF(`shape.path.keyframes` JSON+`evaluatePathAt`線形補間)、ジオメトリ6種・operator群の時間評価(`resolveShapeGeomDims`/`applyAnimatedOperatorParameters`)
- operator 10種(Trim/Repeater/Merge/Offset/Pucker/Rounded/Wiggle/ZigZag/Twist/Wobble)+stack add/remove/move/clear+JSON Undo、Solo Viewで頂点/tangent/segment grammar(Shift/Ctrl)・挿入・削除(ポリゴンのみ)・角丸/星内径ハンドル・context menu/tooltip・ToolOptionsBar連動
- GPU native描画(multi-content/operator/unified/legacy)、`toCoreShapeLayer`→SVG出力(gradient defs、Inside/Outside輪郭化)、Shape↔Mask双方向変換action、`ShapePath::interpolate`・`pointAtPercent/tangent/normal/sampleEquidistant`・`MergePaths(Add/Subtract/Intersect/Difference/Merge)`

### 未導入（下記「導入すべき機能」へ）
- D-3プリセットUIとD-6のopen/closed・smooth操作、1レイヤー複数シェイプ/グループ・Contents・Group Transform、Merge native本物化、SVG import、マルチストップgradient・Taper/Wave完全化・Trim同時/個別・Repeater Composite順、Convert To Bezierのrevert・marquee・proportional・split、パスモーフィングUI・operatorの全種類編集・式/pick-whip、pixel parity・3経路一致・保存往復のruntime受入

### 導入すべき機能（概要）
1. D-1 overlay移植 2. D-2 パラメータハンドル移植 3. D-3 ShapeプリセットUI 4. D-4 選択grammar完成 5. D-5 operator HUD/ハンドル 6. D-6 open/closed・smooth・corner/bezier 7. グループ/複数シェイプ 8. Merge本物化 9. SVG import 10. gradient/pattern/noise fill拡充 11. Taper/Wave・Trim同時/個別・Repeater順 12. Convert/marquee/proportional/split 13. パスモーフィング・式連携 14. runtime受入・perf(詳細は `docs/analysis/REPORT_SHAPE_GAP_UPDATE_2026-09-10.md`)。

---

## 追補 2026-09-21: Nuke 3Dまわり再棚卸し（ソース直読）

旧記述(2026-08-01〜09-10)は実装修了分を未反映だったため、現行ソースで再判定。Grep読取のみ、ビルド・ランタイム未実行。

### 上方修正した項目
- CornerPin: `Artifact/src/Effect/ArtifactCornerPinEffect.cppm:92` でCPU実装。残りはGPU・特異値判定。
- プレーナートラッカー: `ArtifactCore/src/Tracking/PlanarTracker.cppm:10` で実装、CornerPin連携あり。
- 地面グリッド: `ArtifactCore/include/Grid/ArtifactGridSystem.ixx:273` + RenderController側描画あり。
- キーイング: Chroma/Luma/IBKはCPU+GPUあり。Keylight/Primatte/Ultimatte級のみ不在。
- Cryptomatte: `ArtifactCore/src/Image/Cryptomatte/CryptoPixel.cppm:12` + EXR manifest + VP pickあり。
- Geo受入: FBX/OBJ/glTFはufbx経由あり。USDは.usdaのみ、Alembic/Assimpなし。
- ライト/影/IBL: Area+Gobo、LightLink、ShadowMap PCF、EnvMap IBL畳込あり。
- レンズ歪み: CPU+GPUあり。
- カメラトラッカー: 単眼オドメトリ+Pnpあり。BA・ポーズKF書込なし。

### 依然不在の核
- `ScanlineRender`/`Project3D`、EditGeo/TransformGeo/UV Unwrap、Position/UV AOV、Deep native multi-sample+制作UI、スタビ動画I/O、`WarpStabilizer`名称、Stencilオペレータ、プロシージャル多様化。

### 次の順序案
1. Project3D 2. Position/UV AOV 3. Deep native+UI 4. ABC/USD完成 5. Keylight級 6. スタビ動画I/O 7. CameraTrackerのBA+KF書込

---

## 追補 2026-09-21(2): Position/UV AOV 実装

`ChannelType` (Core) と `ArtifactIRenderer::ChannelType` に PositionX/Y/Z・U/V を追加。Meshシェーダ mode 9=ワールド位置(raw)、mode 10=頂点UV(raw, マテリアルuv transformなし)で `position_` / `uv_` ターゲットへ描画。VP表示(単体+合成)・CPUフォールバック・EXR(`Position.X/Y/Z`・`UV.U/V`)・RenderQueue既定チャンネル・出力ダイアログに対応。2Dレイヤーは対象外(3D only-pass経路のため)。runtime検証は未実施。

---

## 追補 2026-09-21(3): TextExtrude 実装とデフォーマ backlog 記録

- TextExtrude: Fusion Text3D 相当の押出しテキストを `Procedural3DLayerKind::TextExtrude` として実装。
  `Procedural3DGenerators::generateTextExtrude`(QFont/QPainterPath→toSubpathPolygons→`extrudeContourMesh`→`generateRenderData`)で Mesh 化し、既存 drawMesh/material/AOV 経路へ接続。text/fontFamily/fontSize/bold/italic/depth/bevelWidth/bevelSegments をプロパティ・JSON・プリセット(`beveledText3D`)・Layerメニューへ露出。Y-up 反転・ブロック中央原点。色絵文字(bitmap glyph)は輪郭が取れない可能性あり(未検証)。runtime検証は未実施。
- デフォーマ backlog (Fusion Bend/Twist/Displace3D 相当): コア 12種実装済み(2026-09-21)。
  `Geometry.MeshDeform` (新規 .ixx 自動検出 + 非export .cppm、CMake変更なし) に
  bend/twist/taper/displace/wave/bulge/spherify/shear/pucker(AE Bloat兼用)/stretch/
  noise/smooth(ラプラシアン) を Mesh 直接変形として実装。成功時は computeVertexNormals +
  updateBounds 済み。displace/noise はゼロ平均 fBm(NoiseField)+法線/ベクトル加算、
  displace は subdivLevels 指定で createSubdivided 先行。`ArtifactProcedural3DLayer` へ
  Deform スロットとして配線済み(全kindに適用、deform.* プロパティ・JSON・`rippleText3D`
  プリセット、Wave はフレーム時刻で自動アニメ)。Model3D(読込メッシュ)への配線・
  runtime検証は未着手。ポリゴンオフセット用外部lib(Clipper等)も未導入。
