# AE 不満点 → ArtifactStudio 改善マップ

**最終更新:** 2026-08-20

After Effects 利用者の不満点を、ArtifactStudio のソースコード現状（2026-08-13 調査時点）に照らして、改善・差別化できる点を優先度順に整理したもの。

## 調査方針

- 一次情報はソースコード（`.ixx` / `.cppm` / `.cpp` / `.hpp`）。
- `docs/` の監査系文書は日付ごとに結論が異なるため参考程度とし、コードを優先。
- 「実装済み」「部分実装」「未実装」を明確に区別する。
- 未検証の断定は「未検証」「要確認」と明記する。

## 調査対象（2 回に分けて実施）

1. レイヤー/コンポジション、タイムライン/キーフレーム、プロパティ/エフェクト、レンダリング/プレビュー
2. テキスト、アセット/メディア I/O、3D/カメラ/ライト、カラーマネジメント、ワークスペース/ドッキング UI

---

## レイヤー/コンポジション モデル（現状）

- アプリレイヤー（`ArtifactAbstractLayer`）と Core 合成プリミティブ（`Layer2D` / `LayerMatte` / `LayerStrip`）の 2 系統が並存。
- レイヤー種別は 27 種以上。AE 標準（footage/solid/text/shape/camera/light/null/adjustment/precomp）に加え、SDF / SandSim2D / FormParticle / Procedural3D / MaterialContainer / Switch / Clone / ParametricComposition / EnvironmentMap など。
- Transform（position/rotation/scale/anchor/opacity）、親子関係、Null、Adjustment、Precomp + Master Properties は実装済み。

## 実装済み・強み（全体）

| 領域 | 機能 | 状態 | 主要パス |
|---|---|---|---|
| レイヤー | プリコンポーズ / ネスト / 時間変換 | 実装済み | `Composition/PreCompose.ixx` |
| レイヤー | Master Properties（exposed properties） | 実装済み | `ArtifactCompositionLayer.cppm` |
| マスク | ベジェ + ラスタライズ | 実装済み | `MaskPath.ixx` / `LayerMask.ixx` |
| マット | 任意レイヤー参照 + 複数合成 | 実装済み（精度確認待ち） | `ArtifactLayerMatte.ixx` / `LayerMatte.ixx` |
| 式 | エクスプレッションエンジン | 約 95% 完成 | `Script/Expression/*` |
| アニメ | キーフレーム補間（40 種近く） | 実装済み | `Geometry/Interpolate.ixx` |
| アニメ | 時間リマップ / フレームブレンド / モーションブラー | 実装済み | `Time/TimeRemap.ixx` |
| 合成 | ブレンドモード（35 種 GPU + CPU） | 実装済み | `LayerBlend.ixx` / `LayerBlendComputeShader.ixx` |
| 物理 | ソフトボディ / フラクチャ / 流体 / 群衆 / パーティクル / クローン | 実装済み（AE には無い拡張） | `PhysicsSystem` / `FluidSolver2D` / `BoidsSwarmSystem` / `ParticleSystem` |
| キャッシュ | RAM/ディスクキャッシュ + 優先度ビルド | 実装済み（二重管理） | `ArtifactPlaybackService.cppm` / `ArtifactRamPreviewController.cppm` |
| テキスト | テキストアニメーター（range/wiggly/order/anchorGrouping） | 実装済み | `TextAnimator.ixx` / `TextAnimator.cppm` |
| テキスト | GPU グリフ atlas + per-glyph 色/blur 描画 | 実装済み | `GlyphAtlas.ixx` / `PrimitiveRenderer2D.ixx` |
| アセット | 連番検出 / キャッシュ / 欠落検出 | 実装済み | `AssetSequence.ixx` / `ImageSequenceSource.cppm` |
| アセット | 素材ブラウザ + リリンク + Undo | 実装済み | `ArtifactAssetBrowser.cppm` / `ArtifactProjectService.cppm` |
| アセット | プロジェクト保存 + 健康チェック + 自動保存 | 実装済み | `ArtifactProjectHealthChecker` / `ArtifactAutoSaveManager` |
| 3D | モデル読込（FBX/glTF/OBJ/PLY/STL/USDA）+ PBR | 実装済み | `MeshImporter.cppm` / `Material.ixx` |
| 3D | ライト 5 種 + GOBO + ライトリンク | 実装済み | `ArtifactLightLayer.cppm` / `Light.ixx` |
| カラー | 伝達関数 16 種 / 色域 11 種 / Bradford | 実装済み | `ColorTransferFunction.ixx` / `ColorGamutConversion.ixx` |
| カラー | 3D LUT（.cube/.csp/.3dl/HaldCLUT）+ GPU 適用 | 実装済み | `ColorLUT.ixx` / `LUT3DComputer.cppm` |
| カラー | **実 OpenColorIO v2 統合済み** | 実装済み | `ArtifactOCIOManager.cppm` / `OCIOConfig.cppm` |
| UI | QADS ドッキング + フォーカスモード + 没入モード + レイアウト Undo | 実装済み | `ArtifactMainWindow.cppm` / `ArtifactDockManager.ixx` |
| UI | QPalette 主導テーマ（QtCSS 不使用）+ AE プリセット | 実装済み | `WindowStyleCSS.ixx` / `CommonStyle.cppm` |

---

## 改善マップ（優先度順）

### 1. レンダリング待ちの排除（最優先）

**AE の不満**: 緑バー / RAM プレビュー待ち、プレビュー中の編集待ち。

**現状**: `ArtifactPlaybackService` 側にはRAMキャッシュ、先読み、世代番号によるキャンセル、進捗／ヒット率表示、再生開始をキャッシュ準備でブロックしない経路が存在する。一方、別の `ArtifactRamPreviewController` も残っており、本線利用箇所は確認できず、内部の `startBuild()` は同期ループである。したがって二重管理と品質ポリシー分散は残るが、「全体が未接続」とは言えない。

**改善**: 制御を一本化し、常時バックグラウンド先回りレンダー + 編集即反映 + フレーム単位キャンセル + 適応品質にする。部品は揃っており、統合層が未実装。

### 2. シミュレーション統合（決定的差別化）

**AE の不満**: Newton / Particular / Trapcode など有償プラグイン必須。

**現状**: soft body / fracture / fluid / crowd / particle / cloner / SDF / sand sim がレイヤーとして非破壊で載る。エンジンは実装済み。

**改善**: 「モーショングラフィックス + シミュレーション統合」を前面に。AE と土俵が違う。

### 3. トラックマットの位置依存からの脱却

**AE の不満**: 上に重ねる位置ベースで直感的でない。

**現状**: `LayerMatteReference.sourceLayerId` による任意レイヤー参照 + 複数マットの Add/Subtract/Intersect/Difference（Nuke 寄り）。実装済み。

**改善**: UX で前面に出すだけ。

### 4. ピックウィップと式の型安全リンク

**AE の不満**: 式が壊れやすい、ピックウィップが煩雑。

**現状**: プロパティ間ピックウィップ未実装。ただし式エンジン 95% 完成 + `globalPropertyRegistry` あり。`index` 変数と `marker` API は未実装。テキスト側の Expression Selector（`textIndex`/`textTotal`）も未実装。

**改善**: 型安全な視覚リンク + `thisComp.layer("X").position` の自動補完 + ライブエラー + 式の一時無効化。

### 5. プリコンのコンテキスト喪失

**AE の不満**: プリコンに飛んで戻る往復が辛い。

**現状**: Precomp + nesting + Master Properties は実装済み。

**改善**: 親コンポを背景に透かして子を編集する「ピーク編集」的な UX。

### 6. グラフエディタの速度グラフ

**AE の不満**: 速度グラフが初心者殺し。

**現状**: 値グラフは編集可、速度グラフは読み取り専用。EasingLab / EasingCurveUtil は既存。

**改善**: 加減速を数値 + プレビューで直感的に調整する代替 UX。

### 7. エフェクトコントロールの散らかり

**AE の不満**: パネルが散らかる。

**現状**: Inspector はスタック管理とプロパティ詳細が別タブ。エフェクトカタログは `availableEffects()` と `buildEffectCatalogEntries()` で二重管理。

**改善**: AE 風「1 パネル縦並び折りたたみ」+ ID 一元管理。

### 8. 素材再リンクの一括解決（アセット）

**AE の不満**: 素材移動後に大量の「missing footage」を 1 個ずつ直す苦痛。

**現状**: 単一パス導線に加え、Asset Browserで候補探索・複数選択・候補確認・複合Undoを実装済み。連番フレームからFootageItemを解決し、候補適用途中のロールバックにも対応。全コンポジションの image/video/audio/svg `sourcePath` も同一Undo単位で更新し、各レイヤーのAssetManager共有ソースは再取得・旧参照解放される。AssetDatabaseにも旧パス→新パス移行APIを追加した（実行検証待ち）。

**改善**: 絶対パス → 相対パス → basename → ハッシュ/サイズ/mtime の多段解決で一括リリンク候補を提示し、全参照へ一括伝播。

### 9. プロジェクト肥大の抑制（アセット）

**AE の不満**: プロジェクトが重い・遅い・破損しやすい。

**現状**: 連番は `sequencePaths[]` を全フレーム列挙し、全コンポ/レイヤを単一 JSON にインライン。サイズ上限と件数上限はあるが構造的肥大は残る。

**改善**: 連番は `SequenceGroup`（prefix/suffix/padding/first/last）のみ保存し読込時に再構築。分割保存と UUID 参照化で文字列重複を削減。

### 10. プロキシの自動化（アセット）

**AE の不満**: プロキシ生成が遅く、切替が手動。

**現状**: 映像プロキシは外部 ffmpeg を同期実行（UI ブロック）。静止画は OIIO。生成・切替はパネル導線のみで、**再生パイプラインが実際に proxyPath を自動で使う経路は未確認**。

**改善**: 非同期/バックグラウンド生成 + プレビュー品質プリセット（Draft/Preview/Final）と連動したプロキシ自動切替。連番・静止画も `ArtifactProxyManager` に統一。

### 11. テキストのビューポートインライン編集（テキスト）

**AE の不満**: テキスト編集がモーダルで、プレビューと往復する。

**現状**: ビューポートインライン編集（カーソル・選択・IME）は未実装。modal 編集のみ。

**改善**: ビューポート上で直接編集 + リアルタイムプレビュー。docs の P0 に一致。

### 12. テキストの文字範囲スタイル編集（テキスト）

**AE の不満**: 文字パネルで個別スタイルを当てる操作が煩雑。

**現状**: フォントウェイト/スタイルが 2 値のみ、baseline shift / kerning なし、グリフ単位・範囲単位のスタイル変更なし、複数 fill/stroke / グラデーション fill なし。

**改善**: AE の「文字パネル」相当の per-character / per-range スタイル編集。フォントウェイト数値化・可変フォント軸。

### 13. テキストアニメーターの拡充（テキスト）

**AE の不満**: 複雑な文字アニメに専用の selector 表現が必要。

**現状**: range/wiggly/ease は実装済みだが、Expression Selector（`textIndex`/`textTotal`）未実装、1 アニメーター複数 selector と合成モードなし、color 系（Fill RGB / Hue / Sat / Brightness）未実装、ease が pow 近似。

**改善**: Expression Selector + 複数 selector 合成 + color 系の細分化。

### 14. 3D レイヤーの 3 軸回転（3D・最優先ギャップ）

**AE の不満**: 3D 配置が面倒、Cinema 4D 連携が重い。

**現状**: `AnimatableTransform3D` は既存単一rotationをZ互換として維持しつつ、X/Y/Zの値・キーフレーム・スナップショット・保存／再読込に対応。3Dモデル、Procedural3D、共通行列、ギズモ、Undoも3軸化済み。プロパティ専用UIの軸別編集契約は未整理。

**改善**: フル 3 軸回転（Euler/Quaternion）と `transform3D()` の拡張。AE 比較で最も根本的な未実装。

### 15. 環境マップ / IBL の実配線（3D）

**AE の不満**: 環境レイヤー / 反射が扱いにくい。

**現状**: `MeshRenderer::setEnvironmentMap()` が HDRI/LDR 環境画像を読み込み、cubemap、cosine-convolved irradiance cube、GGX prefiltered mip chain、BRDF LUT を生成して PBR shader へバインドする。環境単独の IBL、環境回転、Clearcoat／Transmission／AO も shader 経路に接続済み。`ArtifactIRenderer`／`ShaderManager` には環境キューブの skybox 背景表示経路も存在し、同一 renderer/device 内の環境リソース共有も実装済み。`ArtifactEnvironmentMapLayer` の専用レイヤーとしての表示・編集責務、プロセス全体／LRU キャッシュ、runtime/backend差分は未検証。

**改善**: 専用環境レイヤーの編集・保存契約、skybox 背景、環境キューブの共有キャッシュ、D3D12/Vulkan の実機 parity を整備する。

### 16. シャドウの強化（3D）

**AE の不満**: ソフトシャドウ / 多点光源が制限される。

**現状**: Directional/Spot の単一ハードシャドウのみ。`shadowRadius`（ソフトネス）未使用。Point/Area はキャスター対象外、複数キャスター非対応。

**改善**: ソフトシャドウ、Point（cube shadow）、Area 近似、複数キャスター、CSM。

### 17. 色の自己記述化（カラー・最優先ギャップ）

**AE の不満**: 色管理が混乱し、作業空間が曖昧で結果が環境依存になる。

**現状**: `FloatColor` は素の float4 で**色空間メタデータなし**。`SurfaceColorDescriptor`（ピクセル格納/プライマリ/伝達関数/シーン or 表示参照）は正しい設計だが、ピッカー・エフェクト・コンポジットまで一貫して通っていない。

**改善**: `FloatColor` に色空間情報を持たせる（または `SurfaceColorDescriptor` を全面採用）。型レベルで「今この値は何色空間か」を保証し、AE の曖昧さを構造的に排除。

### 18. OCIO 単一ソース + GPU 直結（カラー）

**AE の不満**: 色モード（8/16/32bpc）とプロファイル変換の三つ巴が混乱を生む。

**現状**: `ArtifactOCIOManager` が画像入力変換、ビューポート表示変換、プロジェクト保存/読込の現行経路として使われている。`ArtifactColorScienceManager` はパネルと旧LUT管理を保持し、`ColorManager` は公開モジュール契約と実装が残るが `ColorManager::instance()` の実呼び出しはソース検索上確認できない。OCIO の GPU シェーダ（`gpuViewTransformShader`）は生成APIまであり、ビューポートの実GPU直結は未確認。

**改善**: 入力解釈 → シーンリニア作業空間 → OCIO display/view の一本化。OCIO の GPUProcessor を直接配線し、LUT ベイクはプレビュー近似に限定。ACES RRT は簡易近似（`applySimpleRRT`）を OCIO 非依存フォールバックと明示。

### 19. HDR / ビット深度の徹底（カラー）

**AE の不満**: 32bpc でもピッカーが表示参照で混乱、HDR 値が潰れる。

**現状**: カラーピッカーは [0,1] 固定・sRGB 固定・HSB/RGB/HSL/HEX のみ。HDR 値（>1.0）の編集不可。`ColorLUT::applyToImage` は 8bit で劣化。これはUI側の確定的な制約で、`FloatColor` の型変更だけでは解決しない。

**改善**: シーン参照/HDR 値（>1.0）の編集、作業空間表示、OCIO view プレビュー。HDR 値を 8bit に潰さない経路を保証。

### 20. ワークスペースプリセットへの配置保存（UI・最重要ギャップ）

**AE の不満**: ワークスペース保存が分かりづらく、カスタム配置が壊れやすい。

**現状**: `savePreset()` はウィンドウ geometry、workspaceMode、QADSのdockStateを保存し、復元時にモード適用後のdockState復元を行う。表示メニューから保存・削除・セッション復元・デフォルト復元・プリセット適用も実行できる。無効なworkspaceModeとdock backend不在も防御する。ユーザー向けのモード別カスタム配置UIは別途未整理。

**評価**: 基本的な配置保存・復元は実装済み。残る改善は、モード切替時にユーザー配置を自動適用するモード別マッピングと、その編集導線。

### 21. ワークスペースモードのユーザーカスタム配置（UI）

**AE の不満**: ワークスペース切替が固定で、カスタマイズが面倒。

**現状**: 10 モードの表示/非表示パネルが `workspaceVisibilityRuleFor()` のハードコード。汎用プリセットの永続化はあるが、ユーザー配置をモードごとに自動適用する紐付けが無い。

**改善**: 「現在の配置をこのモードに保存」操作を追加し、モード → dockState マッピングを永続化。`ArtifactWorkspaceManager` + `DockLayoutDocument` で実現可能。

---

## 未実装・要確認項目（領域横断）

### レイヤー / アニメ / 合成
- プロパティ間ピックウィップ（未実装）
- エクスプレッション `thisComp.layer("X").position.x` の transform サブオブジェクト解決（未完成）
- エクスプレッション `index` 変数 / `marker` API（未実装）
- マスクのパスアニメーション（土台あり、UI/評価経路は要確認）
- トラックマットの合成順・premultiplied alpha 一貫性（要確認）
- Adjustment Layer の描画統合（要追加監査）
- 速度グラフの編集（読み取り専用）
- RAM プレビュー制御の一本化（二重管理）
- MFR（farm distribution）は AGENTS.md 上「実装済み」だが `ArtifactRenderer` 本体は骨組み段階に見え、整合は要確認

### テキスト
- ビューポートインライン編集（未実装、docs の P0）
- Expression Selector / `textIndex` / `textTotal`（未実装）
- 1 アニメーター複数 selector と合成モード（未実装）
- 文字範囲単位のスタイル編集（未実装）
- アニメーションプリセット（`.ffx` 相当）のファイル保存/読込/カタログ（未実装）
- フォントウェイト/スタイルの数値化・可変フォント軸（未実装）
- kerning / baseline shift（未実装）
- HarfBuzz backend は stub（`QtShapingBackend` へフォールバック）
- カラー/ZWJ 絵文字の GPU 描画は source 実装済みだが統合ビルド検証待ち

### アセット / メディア
- バッチ再リンク（候補確認・複合Undo・レイヤーsourcePath伝播・AssetDatabase移行APIまで実装、実行検証は未実施）
- リリンクの `AssetDatabase` への直接伝播（API実装済み・実行検証待ち、レイヤーsourcePath経由のAssetManager更新は確認済み）
- プロキシ自動切替経路（未確認）
- 連番のパターン保存（prefix/suffix/padding）は未活用
- 連番プロキシ生成（明示実装なし）
- 動画対応は AGENTS.md 方針で後回し（デコード基盤は実装済み）

### 3D
- 3D レイヤーの 3 軸回転（主要モデル・行列・ギズモ・保存経路は実装、プロパティUIは要整理）
- カメラの POI / 2 ノード相当（トランスフォームベースのみ）
- 被写界深度 / カメラモーションブラーの実レンダリング（パラメータのみ、未配線）
- モデルアニメーション / skinning / Alembic（未実装）
- デフォーマ / モディファイアスタック（未実装）
- 頂点カラーの描画反映（読込は一部あるが描画未使用）
- SSGI / DDGI はコンポジション側の GI パイプラインとして実装済み。`MeshRenderer` の per-material PBR へ GI 結果を直接注入する契約は未定義で、現状は別段階の合成経路として扱う。

### カラー
- `FloatColor` の色空間メタデータ（なし、最大の弱点）
- 真の ACES RRT（CTL 移植）未実装（簡易近似のみ）
- OCIO GPUProcessor の直接適用（LUT ベイク経由のみ）
- カラーピッカーの HDR/OCIO/作業空間対応（なし）
- `Grading/LUTLoader.ixx` と `ColorLUT` の重複
- 旧 `ColorManager` / `ArtifactColorScienceManager` の二重管理（ただし `ColorManager` はCoreの公開契約として残るため、削除ではなくOCIO委譲または互換層化が必要）

### UI / ワークスペース
- ワークスペースプリセットへの dockState 保存（実装済み、モード別ユーザー配置UIは未整理）
- ワークスペースモードのユーザーカスタム配置（ハードコード）
- `NativeDockSurface` は未使用のプロトタイプ
- `buildDCCStyleSheet()`（QSS）が未使用のまま残る（誤って復活させない注意）
- ポータブルレイアウト（`DockLayoutDocument`）と QADS blob の二重管理（移行途中）

---

## 結論

「AE の不満を潰す」より「AE にできないことをやる」方向が強い。優先度 1（待ちの排除）と 2（シミュレーション統合）は、いずれも新規大実装より既存部品の統合・露出が中心で、差別化効果が高い。

領域横断で見ると、各領域に共通するパターンが 2 つある:

1. **部品は実装済みだが統合・配線が未完成**（RAM プレビュー、OCIO、環境マップ、プロキシ、ワークスペース配置）。これは AE の「後付けで絡まったレガシー」を追体験する前に、今のうちに一本化するのが勝ち筋。
2. **根本的な型/モデルの欠落**（3D の 3 軸回転、`FloatColor` の色空間情報、ピックウィップ）。これは表現力・堅牢性の土台に関わるので、早めに手を打つべき。

最優先 3 点を挙げるなら:
- **レンダリング待ちの排除**（部品統合）
- **シミュレーション統合の前面化**（差別化）
- **色の自己記述化 + OCIO 一本化**（AE の混乱を構造的に回避）

次点で **3D の 3 軸回転**（根本欠落）と **ワークスペース配置の保存**（UI 完成度）。
