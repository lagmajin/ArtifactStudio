# AE / Nuke 機能ギャップ分析

**日付**: 2026-08-01
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
| シェイプ | 🟠 20% | ArtifactShapeLayer | 作成・編集ツール未実装 |
| カメラ | 🟢 80% | ArtifactCameraLayer | ステレオ・被写界深度あり |
| ライト | 🟡 60% | ArtifactLightLayer | 基本のみ |
| Null | 🟢 90% | ArtifactNullLayer | 十分 |
| 調整レイヤー | 🟢 85% | ArtifactAdjustableLayer | エフェクトパイプライン動作 |
| 3D モデル | 🟡 60% | Artifact3DLayer | メッシュ読み込み・描画あり。ギズモ未完成 |
| パーティクル | 🟡 50% | ArtifactParticleLayer | 基本機能あり |
| フォームパーティクル | 🟡 45% | ArtifactFormParticleLayer | 基本機能あり |
| ペイント | 🟠 30% | ArtifactPaintLayer | BrushStrokeあり。ブラシUI・パネル未実装 |
| オーディオ | 🟡 50% | ArtifactAudioLayer | 波形表示・スペクトラムあり |
| プロシージャル3D | 🟠 25% | ArtifactProcedural3DLayer | Terrain/PathTubeのみ |
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
| 歪み（CornerPin, Bulge, Twirl） | 🔴 5% | CornerPin がスタブ |
| キーイング（Keylight, Extract） | 🔴 0% | 未着手 |
| カラコレ（Lumetri, Colorista相当） | 🟠 20% | FloatColor + LUTのみ |
| シミュレーション（Shatter, Wave World） | 🔴 5% | SandSim2D, Fractureのみ |
| スタビライザー | 🔴 5% | ArtifactStabilizer がアルゴリズムバグで機能せず |
| 3D チャンネルエフェクト | 🔴 10% | Depth/Position/Normals AOV のみ |
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
| 3D ライト（Point/Spot/Parallel/Ambient） | 🟡 50% | Light 型あり |
| マテリアル（PBR） | 🟡 60% | ArtifactCore::Material に baseColor/metallic/roughness/normal/emission/occlusion |
| 3D 軸ギズモ | 🟢 80% | Artifact3DGizmo で動作 |
| 3D フレームギズモ | 🟠 40% | 描画はされるがドラッグ無反応（原因特定済み） |
| 深度マット | 🟡 50% | DepthMask あり |
| 環境マップ | 🟠 20% | ArtifactEnvironmentMapLayer あり |
| 3D レンダラー | 🟢 80% | Diligent Engine で高度な GPU レンダリング |
| **3D 地面グリッド** | 🔴 0% | 未着手（設計書あり） |
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
| 3D カメラトラッカー | 🟠 20% | ArtifactCameraTrackerTool あり |
| **プレーナートラッカー** | 🔴 0% | 未着手 |
| **ワープスタビライザー** | 🔴 5% | アルゴリズムバグで機能せず |
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
| ステンシル | 🔴 0% | 未着手 |
| 深度コンポジット | 🟠 30% | DepthMask のみ |

---

## 9. レンダリング・出力

| 機能 | 実装率 | 詳細 |
|------|--------|------|
| レンダーキュー | 🟢 80% | ArtifactRenderQueueService + Manager |
| 画像シーケンス出力 | 🟢 80% | PNG/EXR/TIFF 他 |
| 動画出力（FFmpeg） | 🟢 80% | H.264/H.265/ProRes |
| RAM プレビュー | 🟢 80% | ArtifactRamPreviewController |
| **選択的レンダーキュー** | 🟠 20% | 設計書あり、未実装 |
| マルチAOV出力 | 🟡 60% | MultiChannelImage あり |
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
| マルチチャンネル / AOV | 🟡 50% | MultiChannelImage あり |
| キーヤー（Primatte/Ultimatte級） | 🔴 0% | 未着手 |
| 3D 空間コンポジット | 🟠 30% | ArtifactComposition3D あり |
| ノードのコピペ | 🔴 0% | レイヤー間コピーで代用 |
| Dope Sheet / Curve Editor | 🟢 80% | 共通 |

---

## 16. 総括

| カテゴリ | 総合スコア | 最重要ギャップ |
|----------|-----------|---------------|
| レイヤー種別 | 🟡 65% | リグレイヤーUI・ツール不在 |
| エフェクト | 🟠 25% | 数が絶対的に不足、スタブ/バグ多数 |
| アニメーション | 🟡 60% | Wiggle/loopOut/smooth式 |
| 3D | 🟡 50% | フレームギズモバグ、地面グリッド |
| テキスト | 🟠 20% | Text Tool・Text Animator 未着手 |
| マスク/ロト | 🟡 50% | ロトブラシ・コンテンツアウェア |
| トラッキング | 🟠 30% | プレーナー・ワープスタビライザー |
| コンポジット | 🟡 60% | ステンシル不在 |
| レンダリング | 🟡 65% | 選択的キュー・マルチフレーム |
| カラーマネジメント | 🟢 75% | OCIO フル実装。外部config読込・プリセット・display/view UI完備 |
| パフォーマンス | 🟡 60% | マルチフレームレンダリング |
| UI/UX | 🟡 65% | ワークスペース・ポーズパネル |
| スクリプト/拡張 | 🟠 30% | プラグインSDK未完成 |
| オーディオ | 🟡 55% | VST3 部分的 |

**全カテゴリ平均**: 🟡 ~55%

**最重要P0（クリティカルバグ）**: 成熟度分析で報告された メモリリーク、use-after-free、double-free、null dereference、スタブ多数。機能以前に安定性の課題がある。

**最重要P1（機能差）**: エフェクト不足（キーイング・歪みゼロ）、テキストツール不在、ワープスタビライザーがスタブ、OCIO不在。
