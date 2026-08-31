# Text Glyph Submitter 分離マイルストーン

**最終更新:** 2026-08-30

**ステータス:** 独立 Submitter 実装・GPU smoke path proven / 製品 renderer 移行 pending

## 現行コード監査 (2026-08-15)

既存コードと直近の GPU 監査記録から、製品 Renderer 内の glyph atlas upload、通常／変形 glyph submit、color emoji、ZWJ cluster 処理、readback は実証済みの範囲がある。G3 の focused runtime／smoke も記録上は `Text Sample1`、CJK、emoji、変形を通過している。現ワークツリーには独立 `ArtifactTextGlyphSubmitter` 本体、shader source、pipeline provider contract/adapter が存在する。未完なのは製品 `DiligentImmediateSubmitter` から atlas/resource/submit の所有を移し、同一出力由来の runtime 再現性を確認することであり、マイルストーン全体は未完とする。

## 目的

`ArtifactIRenderer` と `DiligentImmediateSubmitter` の全描画依存を経由せず、
Glyph atlas の GPU 描画だけを実行できる検証経路を作る。

## 2026-08-14時点の監査結果

- QPAは `qwindowsd.dll` / `qoffscreend.dll` を同一Debug出力から解決でき、Core text smokeで起動確認済み。
- `ArtifactCoreTextRuntime` は `Text Sample1 🧪` で14 glyph、カラーglyph 1件、カラー保持1件を実行確認済み。
- `ArtifactTextGlyphSubmitterRuntime` はShader source / PSO生成コードを含めてDebugビルド済み。
- 契約moduleと実装moduleを分離し、`ArtifactTextGlyphSubmitter.cppm` の実描画実装（Core GlyphAtlas upload、複数quad、カラーalpha分岐）をDebugコンパイルした。Smokeを同一module所有ターゲットへ構成し、正式Submitter API経由で `Text Sample1 🧪` の12 glyph、カラーglyph 1件、1000x180 readbackを成功させた。出力は `artifact_text_submitter_api.png`。
- `ArtifactTextGlyphSmoke` はRTX 4070 Ti / D3D12 / QPA環境で実行し、Core GlyphAtlasから生成した `Text Sample1 🧪` の12 glyph（カラーglyph 1件）をGPU描画した。1000x180 readback画像の保存、GPU alpha `min=0 max=255`、カラーemojiのRGB保持を画像で確認した。出力は `artifact_text_sample1_gpu2.png`。
- 実行スクリプトにテキスト引数と出力引数を追加し、同じQPA/D3D12経路で `Text1`、`文字サンプル`、`👩‍💻` を個別にreadbackした。Latin/CJKは期待通り描画でき、ZWJは現行のQt glyph列経路では3 glyph（カラー2件）に分解されるため、合字を単一描画単位にする作業が残る。これはQPA起動失敗ではなく、Unicode sequence shaping / color glyph rasterization境界の未完了点である。
- Qt `QGlyphRun` のstring indexとshaped glyph indexをCore `GlyphItem`へ保持し、DirectWriteのcolor glyph rasterizerへ渡す経路を追加した。`👩‍💻` はQt側で1 glyph（index 1623）として取得され、GPU readbackで合成済みの女性＋ノートPC画像を1描画単位として確認できた。`Text1`、CJK、`Text Sample1 🧪`も同一経路で退行なく描画できた。
- 追加のGPUケースでは `👍🏽` と `🏳️‍🌈` は期待通り描画できた。一方、Regional Indicatorの国旗 `🇯🇵` はQtのglyph indexとUTF-16位置の対応が不足して空描画となり、複数ZWJの家族絵文字も部分表示に留まる。これらは未完了ケースとして完了条件へ残す。
- clusterのshaped glyph配列を`GlyphKey`へ接続し、家族絵文字の4 glyphをDirectWriteへ1 runとして渡す経路を実GPUで確認した。Atlas登録は1 cluster単位になり、scalar glyphの重複描画は抑止できた。ただしDirectWrite run boundsの左bearing／color layer境界がまだ完全一致せず、家族画像は部分clipが残るため製品完了とは扱わない。
- 検証用にclusterを画面端から離して再実行した結果、家族絵文字の合成run自体は欠けずに表示できた。以前の左端clipはAtlas画像ではなく、Smokeがx=0付近で回転変形した画面端clipだった。変形なし／余白ありの確認画像は `gpu_family_margin.png`。
- 複合ケースで通常文字が欠落した退行は、Alpha8 coverageをGrayscale8へ変換していたためだった。Alpha8の直接読み取りへ戻し、無変形の `A B` と `A 👨‍👩‍👧‍👦 B` を実GPUで復旧確認した。変形比較用に `ARTIFACT_TEXT_SMOKE_NO_TRANSFORM` をSmokeへ追加した。
- 前後Latin文脈で家族emojiが部分化する問題は、重複したQt `stringIndexes`に対するfallback開始位置の誤りだった。runのcluster先頭位置からglyph配列を割り当てる修正後、変形ありの `A 👨‍👩‍👧‍👦 B` をA・家族・B全て表示した。出力は `gpu_family_context_mappingfix.png`。
- `ArtifactRenderTextRuntime` は既存の実装moduleを focused targetへ再利用する段階で、`DiligentDeviceManager` / `LODManager` のIFC関連付けエラーが発生する。これはGPU描画失敗ではなく、独立ターゲットの境界不成立を示す。
- 実GPUの最小経路はG3のデバイス・PSO・atlas upload・alpha形状・複数glyph・カラーemoji・readbackを正式 `ArtifactTextGlyphSubmitter` API経由で完了した。さらに `offsetRotation`、`offsetScale`、`offsetOpacity` を反映した変形画像 `artifact_text_submitter_transform.png` を生成確認した。

## 現状の事実

- `PrimitiveRenderer2D::drawGlyphs()` は `AtlasSpritePkt` を
  `RenderCommandBuffer` に追加する。
- 実際の描画は `DiligentImmediateSubmitter::submitAtlasSprite()` と
  `submitAtlasSpriteXform()` が担当する。
- 現行 Submitter は `PrimitiveRenderer3D`、Particle、通常Sprite/Rect/Line、
  FrameDebug などを同じモジュールに抱えている。
- `ArtifactTextRenderTargetRuntime` は単独ビルドできる。

## 新しい境界

`ArtifactTextGlyphSubmitter` は次だけを所有する。

1. Glyph atlas texture / SRV / sampler
2. Glyph用 vertex buffer と transform constant buffer
3. 通常Glyph quad / transformed Glyph quad のPSO・SRB
4. `AtlasSpritePkt` 相当のGlyph draw packet処理
5. `IDeviceContext` と `ITextureView` に対する submit

次は所有しない。

- swap chain
- layer / composition
- PrimitiveRenderer3D
- ParticleRenderer
- LOD / ray tracing
- 通常のSprite、Rect、Line、post process

## 実装段階

### G1: 契約固定

- **完了（契約）**: `ArtifactTextGlyphSubmitter.ixx` を追加し、入力を
  `std::span<const GlyphItem>`、`TextStyle`、色、opacity として固定した。
- **完了（ビルド境界）**: `ArtifactTextGlyphSubmitterRuntime` のDebugビルドに成功した。
- **完了（契約）**: 色絵文字は atlas の RGBAを保持し、通常Glyphはcoverage tintを使う方針を固定した。
- **完了（契約）**: `offsetRotation`、`offsetScale`、`offsetOpacity` をGPU packetへ反映するAPI境界を固定した。
- **完了（focused 実装）**: `ArtifactTextGlyphSubmitter.cppm` は atlas upload、通常／変形 glyph submit、target への描画を所有する。製品 renderer への移行は未完。

### G2: GPU資源抽出

- **進行中**: 既存 `DiligentImmediateSubmitter` から、Glyph atlas、Glyph PSO、
  vertex/index/transform buffer、`submitAtlasSprite*`相当の処理を分離対象として確定した。
- **完了（provider契約）**: `ArtifactTextGlyphPipelineProvider` を追加し、
  Glyph PSO/SRBとatlas samplerだけをSubmitterへ渡す境界を固定した。
- **完了（検証用GPU実装）**: `ArtifactTextGlyphSubmitter` 経由のatlas upload、quad submit、変形submitを実GPUで確認済み。製品rendererへの全面移行と専用providerへの完全分離は未完了。
- **進行中（2026-08-24）**: 製品 `DiligentImmediateSubmitter::setPSOs()` の glyph PSO/SRB/sampler 取得を
  `makeArtifactTextGlyphPipelineProvider(ShaderManager&)` 経由に切り替えた（同一PSOオブジェクトで描画挙動は不変）。
  provider契約module（Contract / Adapter）を製品ビルドの module グラフへ登録済み。
  ShaderManager 内の Glyph PSO 生成本体の専用providerへの移動は残課題。
- **検証結果**: ShaderManagerを公開IFCごと単独ターゲットへ取り込むと、
  `Graphics.Shader.Set` / `Graphics` などのtransitive module依存が発生する。
  そのため移行アダプタはTextRuntimeの正規moduleグラフ内に置き、真の独立化は
  ShaderManager内部のGlyph PSO生成を専用providerへ移した後に行う。
- Glyph PSO生成に必要なShaderManager APIを専用providerへ移す（製品renderer分離の残課題）。
- Sprite系PSOやParticle系importを専用モジュールから排除する（製品renderer分離の残課題）。

### G3: 実行ターゲット

- `ArtifactTextRenderTargetRuntime`
- `ArtifactTextGlyphSubmitterRuntime`
- `ArtifactTextGlyphSmoke`

の3ターゲットを構成し、`Text Sample1 🧪`、CJK、ZWJ emojiをGPU readbackする。

### G4: 静的監査と実行監査

- 既存125件の設計シミュレーションを維持する。
- readback画像の非透明ピクセル、色絵文字のRGB保持、回転Glyphの境界を検証する。
- 古い `ArtifactRender.lib` や standalone binaryを検証に使用しない。

## 完了条件

`ArtifactTextGlyphSmoke` がQPA設定下で実行され、次を同時に出力すること。

- `glyphs > 0`
- `image.width > 0 && image.height > 0`
- `saved = 1`
- color emojiの色保持確認
- 実行バイナリと依存ライブラリが同一ビルド出力由来
