**最終更新:** 2026-10-09

# Insight Register — 2026-W39

期間: 2026-09-21 – 2026-09-27

## 2026-09-27 — 2D 流体のコアは既に実装済みだった。pyro は CMake マクロ未定義でコンパイルされず、smoke の発生源はプロシージャル固定だった

- **関連:** `ArtifactCore/include/Physics/FluidSolver2D.ixx`、`ArtifactCore/src/Physics/FluidSolver2D.cppm`、`ArtifactCore/include/Simulation/PyroSimulation.ixx`、`ArtifactCore/src/Simulation/PyroSimulation.cppm`、`ArtifactCore/src/Physics/PhysicsSystem.cppm`、`Artifact/src/Layer/ArtifactLayerFluidRuntimeState.cppm`、`Artifact/src/Layer/ArtifactAbstractLayerFractureRuntime.cppm`、`docs/planned/MILESTONE_PYRO_DOMAIN_LAYER_INTEGRATION_2026-09-27.md`、`docs/planned/MILESTONE_SMOKE_EMITTER_AND_VIEWPORT_INTERACTION_2026-09-27.md`。
- **確認できた事実（実コード照合）:** 「2D 流体シミュのコア」を `rg FluidSolver2D` で引くと `Physics.Fluid` モジュール（`FluidSolver2D` = Stable Fluids グリッド、`LiquidSolver2D` = 粒子液体、同一 `.ixx`/`.cppm`）が既に CMake 登録・`PhysicsDeterminismTest` でテスト済みだった。**pyro 側は逆で**、`Artifact/` に `pyro|Pyro|PYRO` の出現がゼロで、`PhysicsSystem.cppm` の pyro ブリッジ 5 箇所（`:19, 501, 1044, 1065, 1177`）は全て `#ifdef ARTIFACT_ENABLE_PYRO` 囲み。**そのマクロを定義する `.txt`/`.cmake` がリポジトリに存在しない**（全構成ファイルに grep してヒットなし）ため、`createPyroSimulation` / 毎フレーム `step()` は現状コンパイルされない。`PyroBackendKind::GPUCompute` は `toString` の case ラ벨（`PyroSimulation.cppm:889`）だけが唯一の参照。`PyroFieldChannel::Color` も `PyroFieldSet` に storage が無い宣言のみ。pyro のテストもゼロ件。
- **訂正したドキュメント:** `docs/planned/MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md:9` の「GPU backend は enum／契約上の存在に留まり」は 2026-09-27 時点で**そのまま正しい**（過大評価ではなく）。一方同 doc の Phase ログ（`:145, 257, 264, 279, 287, 294, 315, 322`）が「**Property** へ接続した」と書く `component.fluid.*` 設定 26 個は、`getComponentPropertyGroups` に登録されているのが `component.fluid.enabled` **1 個だけ**（`ArtifactAbstractLayerPropertyGroups.cppm:1098`）だった。setter（`ArtifactAbstractLayerPhysicsRouting.cppm:514-762`）・descriptor settings・JSON 保存は揃っているが **UI から到達できない**。Inspector の `componentInspectorFilter("Fluid")` はこの1件にのみマッチする。
- **smoke 経路の実態:** `renderLayerSmokeRuntime`（`ArtifactLayerFluidRuntimeState.cppm:43-151`）は注入を 1 点（`centerX = gridWidth/2`, `centerY = gridHeight-3`）に固定し、`sin(frame*0.07f)` のプロシージャル渦を `addDensity`/`addVelocity` している。`emitterCount`/`emitterSpeed` は fluid 設定ではなく **Particle Emitter component から供給**されている（`ArtifactAbstractLayerFractureRuntime.cppm:737-738` が `impl_->particleEmitterCount_`/`particleEmitterSpeed_` を aggregate init している）。描画色は `r=0.42, g=0.72, b=1.0` の固定青、28x28=784 粒子を Additive+VelocityAligned で描く。**`FluidVisualizer`（`ImageProcessing.FluidVisualizer`）は完全に孤立**——`import` する箇所も `render()` を呼ぶ箇所も grep ゼロで、fire-gradient 分岐も到達不能。`ToolType` 30 値に流体関連は無く、マウス/ギズモからの流体注入経路は**存在しない**（非テストの `addDensity` 呼び出しはプロシージャル emitter と `ParticleSystem.ixx:422-429` の audio reactivity のみ）。
- **対応:** コード変更はせず、`docs/planned/` に 2 マイルストーンを新設した——`MILESTONE_PYRO_DOMAIN_LAYER_INTEGRATION_2026-09-27.md`（Phase 0 で `ARTIFACT_ENABLE_PYRO` の有効化可否を切り分け、Phase 1-4 で component/Property/emitter/可視化/テスト。GPU 実装は非対象）と `MILESTONE_SMOKE_EMITTER_AND_VIEWPORT_INTERACTION_2026-09-27.md`（Phase 0 で入力経路の評価、Phase 1-2 で emitter と色の Property 化、Phase 3 でビューポートかき混ぜ、Phase 4 で parity）。**Phase 1 で既存 fluid と同じ「setter はあるが Property 未登録」の欠陥を再現しないことを受入条件に明記した。**
- **価値または懸念（未検証）:** 2 つのマイルストーンいずれも**ビルド未実施**。Phase 0 の `ARTIFACT_ENABLE_PYRO` 有効化は `PhysicsSystem.cppm` の 5 ブロックを新たにコンパイル対象にし、**モジュール境界や型不一致によるビルド破壊の可能性がある**ため工程の最初に置いた。また Phase 3 のビューポート注入は毎フレーム drag 状態を読み取る形でないと、solver の毎フレーム再計算（`renderLayerSmokeRuntime.cppm:70-94` で 10 フレーム以上 ギャップで reset）に食われて消えるという設計上の制約を明記した。
- **次に確認すべきこと:** ① `ARTIFACT_ENABLE_PYRO` を有効化したとき `PhysicsSystem.cppm` がコンパイルできるか（Phase 0 の実測）、② pyro の `PyroFieldChannel::Color` を宣言のまま残すか実装するか、③ smoke の fluid 側 Property 26 個が Inspector から本当に到達せずグループ表示されないのか（実 UI 確認）、④ `FluidVisualizer` の孤立コードを削除するか、別用途（CPU 屈折の受け口）へ移すか。



## 2026-09-27 — pyro / smoke 両マイルストーンの Phase 0 調査完了。pyro は宏1行、smoke は新規 ToolType 不要と確定

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`、`ArtifactCore/include/Simulation/PyroSimulation.ixx`、`ArtifactCore/CMakeLists.txt`、`Artifact/CMakeLists.txt`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Tool/ArtifactBrushTool.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **pyro Phase 0 の結論（実コード照合）:** **`ARTIFACT_ENABLE_PYRO` の `target_compile_definitions` 1 行追加でよい。** CMake の接続は既に完了していた。`ArtifactCoreSimulation` STATIC は定義済み（`ArtifactCore/CMakeLists.txt:6202-6214`）、`include/Simulation/` は依存ターゲットが `ArtifactCoreSimulation.dir` に解決される（`:2577-2578`）、`PhysicsSystem.cppm` は既に `Core.Simulation.Pyro.ifc` の `/reference`（`:1227`）と `OBJECT_DEPENDS`（`:1234`）を持ち、`Artifact` の `target_link_libraries` に `ArtifactCoreSimulation` は列挙済み（`Artifact/CMakeLists.txt:2727`）。`PyroSimulation` は `PyroSimulation() = default` を持つ（`PyroSimulation.ixx:260`）ため `makeShared<PyroSimulation>()` と整合する。**ファイル単位やリンク順の問題は存在しなかった。**
- **pyro の留意点:** `PyroSimulation` に `LIBRARY_DLL_API` が無い（`PyroSimulation.ixx:258`）が、`PhysicsSystem` 自体も無い（`PhysicsSystem.cppm:161`）ため全 static 構成では問題にならない。`checkpointCache_` は `std::unordered_map<uint64_t, PyroFieldSnapshot>` で、レイヤー側 checkpoint と二重管理にならないよう注意が必要。
- **smoke Phase 0 の結論（実コード照合）:** **新しい `ToolType` は不要。既存 `Brush` 系に mode flag を足す。** press/move の入力は既に 2 箇所に集約されている——press は `ArtifactCompositionRenderController.cppm:26452-26509`（`viewportToCanvas` は `:26490-26492`、`mousePressEvent` は `:26502-26503`）、move は `:29255-29274`（`viewportToCanvas` は `:29261-29263`、`mouseMoveEvent` は `:29269`）。両方とも既に `markRenderDirty()` を呼ぶ。`ArtifactBrushTool` には既に `setRotoInputMode(bool)` / `setEraserMode(bool)` があり（`ArtifactCompositionRenderController.cppm:26500-26501` で使用）、fluid も対称に `setFluidInputMode(bool)` を追加できる。**Undo は行わない**方針を確定（solver バッファは毎フレーム再計算されるため意味が無い）。
- **smoke の重大な発見:** **`ArtifactAbstractLayer` に fluid コンポーネントの有効状態を取得する公開 API が無い。** `fluidComponentEnabled_` は `ArtifactAbstractLayer::Impl` の private メンバで、`boolFromHost`（`ArtifactAbstractLayer.cppm:420-430`）が内部で同期するだけで、layer 外から読み取る接口が存在しない。`ArtifactAbstractLayer.ixx` に `findByType` / `hasComponent` 系の宣言は 0 件。**Phase 3 には読み取り専用 getter の追加が必須**になる。これは新規 signal ではなく既存方針と矛盾しない。
- **対応:** コード変更はせず、2 つのマイルストーンの Phase 0 節を実コード照合結果で書き換えた（pyro は「1 行追加でよい」と明記、smoke は「新規 ToolType 不要 / Undo なし / getter 追加が必須」を明記）。smoke の Phase 3 節も新知見に合わせて更新。
- **価値または懸念（未確認）:** Phase 0 は調査のみ。**ビルドは未実施**のため、pyro の宏1行追加が実際に通るかは未確認。smoke の getter 追加は `ArtifactAbstractLayer.ixx` のモジュール変更になるため、モジュール再スキャンの影響を持ちうる。
- **次に確認すべきこと:** ① `target_compile_definitions(ArtifactCore PUBLIC ARTIFACT_ENABLE_PYRO)` を追加して `PhysicsSystem.cppm` が通るか（実装＋ビルド）、② `ArtifactAbstractLayer` に fluid 有効 getter を追加することが妥当か、または既存の `getComponentPropertyGroups` 経由で判定する方が既存方針に沿うか。



## 2026-09-27 — SolidRect バッチは clear-only 回帰のため保留、診断ゲートは既存 opt-in に追従させた

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm`。
- **確認できた事実（実コード照合）:** `kSolidRectBatchValidated = false`（`DiligentImmediateSubmitter.cppm:925`）は、batchReady が常に false になるため `DrawIndexedIndirect` 経路（`:944`）が到達不能。`m_batch_solid_rect_cpu_` の頂点計算（`:1153-1172`）は `row0.x*u + row0.y*vv + row0.w` を `row3` の w で除算し、非Xform パス（`:1128-1139`）とは別経路。**clear-only frame の原因はこの2経路の clip 空間変換のいずれかに起因する可能性があるが、静的レビューでは特定できない**（描画されたフレームが必要）。
- **対応:** D（バッチ有効化）は保留し、コメントに「1行で有効化できる価値」と「静的レビューでは確定できない」を明記した。採用した3点はいずれも正しさがコードで明白なもの：A（FrameDebug レコード生成を `ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS` に追従させてゲート）、B（`present()` の 9連続 `.arg()` を `SubmitDiagnosticKey` タプル比較の `if` 内へ移動）、C（`ArtifactTextGlyphSubmitter` の atlas texture / VB / CB をキャッシュ再利用、`DRAW_FLAG_VERIFY_ALL` を `DRAW_FLAG_NONE` に）。
- **価値または懸念（未検証）:** A のゲートは FrameDebug 画面から submitter 由来のパス行が消える。`ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS=1` で従来と同一内容に戻る。C の atlas キャッシュは `GlyphAtlas::isDirty()`（`GlyphAtlas.ixx:119`）をキーにし、upload 後に `clearDirty()` する。`QImage` のアドレスや `sizeInBytes()` をキャッシュキーにすると in-place な画素更新を検出できず危険。**A/B/C の効果量・ビルド・実機はすべて未確認。** また作業ツリーには本件以外の未コミット変更（HarfBuzz shaping 化、emoji glyph キャッシュ拡張）があり、diff 混在状態。
- **次に確認すべきこと:** ビルド許可を得たうえで ① FrameDebug 画面が診断 on/off で期待どおり変化するか、② `ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS=1` で従来内容と一致するか、③ glyph テキスト描画がキャッシュ導入後も正しく atlas 差分更新されるか（特に atlas 満杯時の `clear()` 後）、④ SolidRect バッチの clear-only 回帰を実フレームで再現し、clip 空間変換の2経路の差分を特定する。




## 2026-09-27 — SolidRect/Checkerboard/Grid/GradientRect の頂点バッファは「色込み」で毎 draw 上書きされていたが、シェーダーは位置しか読んでいなかった

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`Artifact/include/Render/DiligentImmediateSubmitter.ixx`、`Artifact/src/Render/ShaderManager.cppm`、`ArtifactCore/src/Graphics/Shader/BasicVertexShader.cppm`。
- **確認できた事実（実コード照合）:** solid 系 PSO の頂点レイアウトは `LayoutElement{0, 0, 2, VT_FLOAT32, false, 0, 6*sizeof(float)}`（`ShaderManager.cppm:744-746`）で **ATTRIB0 = float2（位置のみ）**。シェーダー側も `drawSolidRectVSSource`（`BasicVertexShader.cppm:114-146`）と `drawSolidRectTransformVSSource`（`:170-198`）はいずれも `VSInput{ float2 pos : ATTRIB0 }` のみで、`PSInput` は `pos` と `uv` を持つだけ。**頂点色はどの solid 系シェーダーからも読まれていない**。にもかかわらず `submitSolidRect` / `submitSolidRectXform` / `submitGradientRect` / `submitCheckerboard` / `submitGrid` は 4頂点（position + color, 24 byte/頂点）を**毎 draw .MapBuffer→memcpy→Unmap** していた。Checkerboard/Grid は元から色 `{1,1,1,1}` 固定で完全に冗長、SolidRect 系は packet の色が焼かれてTakashi Shigeruも読まれていない。
- **対応:** `USAGE_IMMUTABLE` の unit quad VB `m_draw_solid_rect_unit_quad_vb_`（`{{0,0},{1,0},{0,1},{1,1}}`, 色白）を `createBuffers` に追加し、上記5 submitter の頂点アップロードと `SetVertexBuffers` バッファ指定を差し替え。`submitLine`（`p.p1/p.p2` が可変）と `submitRectOutline`（`p.xform.scale` から幅高算出）は可変座標のため共有動的バッファ `m_draw_solid_rect_vertex_buffer` のまま据え置き。
- **価値または懸念（未検証）:** SolidRect 密集フレーム（2D 合成・画面|Chrome）に 5 draw あたり 1回の MapBuffer 削減。**ただし `mapWriteDiscard` は `MAP_FLAG_DISCARD` で GPU 待ちと直列化しないため、削減量は未計測。** 別途 GradientRect は元実装で `vertexColor = {1,1,1,p.opacity}` を焼いていたが、gradient PS は `GradientCB.startColor/endColor.w` に opacity を適用済みで（`DiligentImmediateSubmitter.cppm:1434-1436`）頂点色は未使用だったことを確認済み。**ビルド・実機は未確認。**
- **次に確認すべきこと:** ビルド後、solid 矩形・グラデーション・チェック柄・グリッドの表示が従来と同一か（特に Checkerboard/Grid は元から色白固定で、頂点を白へ固定した影響がゼロであることの目視確認）。`ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS=1` で solid 系の debug パスが不変か。`bufferUpdates` カウンタ（`RenderCostStats::bufferUpdates`）が減っているかの確認。




## 2026-09-27 — ParticleRenderer の debugState_ を QString から enum へ（ホットパスの文字列生成をゼロに）

- **関連:** `ArtifactCore/include/Graphics/ParticleRenderer.ixx`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`docs/technical/HOT_PATH_RULES.md`。
- **確認できた事実（実コード照合）:** `debugState_` は `QString` メンバで、24 箇所の関数から `QStringLiteral(...).arg(...)` で**毎フレーム再構築**されていた。ホットパス上の実行は `setProjectionMatrix` / `setViewMatrix` / `setModelMatrix` / `updateBuffer` / `prepare` / `draw` の 6 関数（`submitParticles` がParticlePkt ごとに全部呼ぶ）。 PARTICLE が無いフレームでも `setXxxMatrix` は `submitParticles` からのみ呼ばれるため影響は粒子使用時のみ。**より深刻なのは `DiligentImmediateSubmitter.cppm:1353-1354` の `const QString preparedState = debugState(); if (!preparedState.startsWith("state=prepared"))` で、制御フロー判定に文字列を使っていた**。
- **対応:** `DebugState` enum（`std::uint8_t`、24 値）を追加し、状態は `debugState_`（値）+ 数値/フラグメンバ（`debugCount_` / `debugUploaded_` / `debugMax_` / `debugA_` / `debugB_` / `debugFlagA_`〜`debugFlagC_`）で保持。文字列生成は `debugStateText()` の switch に集約し、**実際に報告する時だけ**呼ばれる。公開 API は `debugState()` が enum を返す（inline、アロケーションゼロ）、`isPrepared()` を追加、`debugStateText()` を追加。`submitParticles` の判定は `debugState().startsWith()` ではなく `isPrepared()` を使う。`ArtifactIRenderer::particleDebugState()`（`frameDebugSnapshot` 経由、UI ポーリング時のみ）だけが `debugStateText()` を呼ぶ。
- **価値または懸念（未検証）:** 粒子使用フレームで QString 生成がゼロになる。`HOT_PATH_RULES.md:100`「診断が無効なときは文字列を構築しない」に合致。**ただし旧 `debugState_` は「最後に何か起きた状態」を保持していたのに対し、新実装も同じ（enum 上書き）**ため、`debugStateText()` の出力内容は状態ごとに同じフィールド 조합を再現しない场合がある（特に `PsoReady` と `Drawn` は別 enum 値なので互いに上書きしない）。`setModelMatrix` は旧実装で成功時に状態更新していなかったが、新実装では `MatrixUpdatedModel` を設定するため `debugStateText()` の出力が `state=matrix-updated model=1` に変わる。**ビルド・実機は未確認。**
- **次に確認すべきこと:** ビルド後、粒子レイヤーの描画が従来と同一か。`ArtifactIRenderer::particleDebugState()`（Frame Debug 画面）で `state=` 文字列が従来とacceptableか、特に `state=matrix-updated` の変化。`qWarning` の警告メッセージは文字列を直接 `<<` している箇所は変更していないため、警告出力のログは同一であるべき。




## 2026-09-27 — Qt 6 はフォントファイルパスと sfnt bytes 全体を公開していない

- **関連:** `ArtifactCore/include/Font/FreeFont.ixx`（`FontManager::fontFileBytes`）、`ArtifactCore/src/Text/TextShapingBackend.cppm`（`acquireHarfBuzzFace`）、`build/vcpkg_installed/x64-windows/include/Qt6/QtGui/qfontdatabase.h`。
- **確認できた事実:** Qt 6.10.2 の `QFontDatabase` に `findFontFile()` は存在せず、`QRawFont::fontTable(const char*)` / `fontTable(QFont::Tag)` は単一 OpenType テーブルしか返さない（`qrawfont.h:107-108`）。したがって公開 API だけで `QFont` → ファイル bytes の変換は不可能で、harfbuzz の `hb_face_t` を得るには自前の解決が必要。既存コードには OS フォントの解決済み経路として `GlyphAtlas.cppm:49-69` の DirectWrite（Windows 専用）がある。
- **対応:** `FontManager::fontFileBytes(family, style)` を追加し、`addApplicationFont()` で登録した family→path の対応表と、OS フォントディレクトリ走査（Windows は `%WINDIR%\Fonts`、その他は `/usr/share/fonts` 等）の2段で解決する。解決できないフォントは Qt fallback のまま残る。
- **価値または懸念（未検証）:** 家族名→実ファイル名の照合は Windows のフォント登録規則に依存する approximated match（`contains()` フォールバック含む）であり、誤マッチの可能性が残る。実測・実行検証は行っていない。
- **次に確認すべきこと:** 主要フォント（Yu Gothic UI / Segoe UI / Arial / Noto CJK）で `fontFileBytes()` が正しいファイルを返すか、family 名に空白やスタイル名を含む場合の照合精度、陈腐化したキャッシュ（OS フォント追加／削除後の再解決）を確認する。




## 2026-09-27 — HarfBuzz は行分割を行わないため座標系が Qt 経路と一致しない

- **関連:** `ArtifactCore/src/Text/TextShapingBackend.cppm`（`shapeWithHarfBuzz`、`layoutWithQtTextLayout`）、`ArtifactCore/include/Text/TextLayoutContract.ixx`（`GlyphItem::basePosition` / `bounds`）。
- **確認できた事実:** `GlyphItem::basePosition` は Qt 経路では絶対 pen 位置（`xOffset + localX`, `y + line.ascent`）、`bounds` は矩形（`TextShapingBackend.cppm:980,987`）で、行分割・折り返し・整列（Center/Right/Justify）を Qt の `QTextLayout` が担当している。HarfBuzz は run の shaping のみで ink.box と仮 advance を返すため、x_advance をそのまま `basePosition` に書くと contract の座標契約に反する。`hb_glyph_position_t` の `y_advance` は通常の水平書記では常に 0。
- **対応:** pen を明示的に積み上げて絶対 `basePosition` と `bounds` を生成し、26.6 fixed point は `1/64` で float 変換。行分割が未実装の段階では、`boxWidth > 0`（折り返し指定）または改行文字を含むテキストは Qt へ委譲するガードを入れ、単一行の unwrapped テキストのみ HarfBuzz 経路で処理する。`info.cluster` は UTF-32 インデックスなので既存の `buildContract()` / `makeIdentityResult()`（codepoint 基準）とそのまま整合する。
- **価値または懸念（未検証）:** 単一行テキストに対する HarfBuzz の glyph 並びは正しく並ぶはずだが、実機での位置合わせ・ベースライン・Platform 差（DirectWrite と FreeType の hinting）は未検証。`logicalToVisual` / `visualToLogical` は `makeIdentityResult()` の恒等写像のままで、RTL・bidi は visual reverse されない。`FontManager::fontFileBytes()` の解決失敗時も Qt へ落ちるため、family 名の合致 oversight は「HarfBuzz 経路が静かに使われない」形で現れる可能性がある。
- **次に確認すべきこと:** Latin / CJK / アラビア語 / 絵文字 ZWJ（`👍🏽`, `🏳️‍🌈`）で Qt 経路と並走し、glyph 数・cluster・ベースライン位置を pixel 比較する。複数行テキストと `boxWidth` 指定が確実に Qt へ落ちること、RTL テキストの visual 順序が壊れていないことを確認する。



## 2026-09-27 — ArtifactPr の合成結果は descriptor が Unknown のため、CPU/GPU で同じ色になるとは限らない

- **関連:** `ArtifactCore/include/Image/SurfacePixelConversion.ixx`（`convertSurfacePixels`、`decodeToLinear`、`decodeLegacySrgbBoundary`）、`ArtifactCore/include/Graphics/SurfaceColorContract.ixx`（`unknownRgba32Float`）、`ArtifactPr/src/SequenceCompositor.cppm`、`ArtifactPr/src/GpuProgramMonitor.cppm`。
- **確認できた事実:** `ArtifactPr` の合成結果は `ImageF32x4_RGBA::resize` / `fill` で作られ、その descriptor は `SurfaceColorDescriptor::unknownRgba32Float()`（`transferKnown == false`, `primaries == Unknown`, `alphaMode == Unknown`）のまま。`toQImage()` は `convertSurfacePixels(..., Rgba8SrgbStraight)` を通り、`decodeToLinear` は `!transferKnown` のため `decodeLegacySrgbBoundary`（範囲内なら `srgbToLinear`）を選び、続けて `linearToSRGB` で再エンコードする。つまり CPU 経路は「sRGB として_decode→linear→sRGB 往復」しており、identity ではない。GPU シェーダーもこれと同じ往復を実装して初めて CPU とピクセル値が比較できる。
- **対応:** `kProgramMonitorPS` に `srgbToLinear` / `linearToSRGB` / `decodeLegacySrgbBoundary` を同じ定数（0.04045 / 12.92 / 0.0031308 / 1.055 / 2.4 / 0.055）で入れ、`toQImage` と同じ順序・同じ alpha クランプ・`alpha <= 1e-6` のゼロ化も揃えた。uv の orientation は `Artifact/shaders/globals.hlsli:1074-1080` の `vertexID_create_fullscreen_triangle` と同じ式にした。
- **価値または懸念（未検証）:** descriptor が Unknown なので「本来は linear 前提」の画像（OpenCV で読んだ素材など）は CPU 側で意図せず 2 度高彩度になる可能性がある。今回 GPU にしたことで surface が揃いやすくなったので、この contract を `SequenceCompositor` 側で正規 descriptor（`canonicalLinearPremultiplied` など）へ明示するかどうかは別途検討が必要。PSO の RTV format は swap chain の実 format に合わせているが、HDR 有効時の `RGBA16_FLOAT` で表示が変わらないかは未確認。viewport は Diligent 側が `devicePixelRatio` を適用した値を使っている。
- **次に確認すべきこと:** 同一 `ImageF32x4_RGBA` を CPU `toQImage` と GPU present で side-by-side 撮影し、pixel diff を取る（平坦なグレーと彩度の高い赤で差が出るはず）。`setGpuPresentEnabled(false)` から CPU 復帰時に `WA_PaintOnScreen` が完全に解除されて再描画されるか、device loss 時に `reset()` 後の子 HWND `DestroyWindow` が成功するかを確認する。



## 2026-09-27 — ArtifactPr の QtAdvancedDocking 依存を撤去し、DockSurface を自前実装した

- **関連:** `ArtifactPr/include/DockSurface.ixx`、`ArtifactPr/src/DockSurface.cppm`、`ArtifactPr/src/ArtifactPrMainWindow.cppm`、`ArtifactPr/CMakeLists.txt`。
- **背景:** ArtifactPr は QADS (`qtadvanceddocking-qt6`) を REQUIRED で link していたが、接触は `ArtifactPrMainWindow` の 1 ブロック（10 個の `ads::CDockWidget` 生成と `addDockWidget` / `addDockWidgetTabToArea`）だけだった。レイアウト永続化は QADS 既定の `saveState()` を呼ぶだけで、QSettings への書き出しは未実装だった。
- **実装:** `DockSurface` を PImpl で新設。5 エリア (Left/Center/Right/Top/Bottom) を `QSplitter` + `QTabWidget` で構成し、`floatDockWidget` / `floatDockTabGroup` は所有するトップレベル `QDialog` として表現。タブのドラッグは `QDrag` で `application/x-artifactpr-dock-id` MIME を運び、drop 先が `QTabWidget` ならタブ挿入、area なら `moveDockWidget`。`saveLayoutState()` / `restoreLayoutState()` は JSON（version 1、entries + areaTabPositions）で往復し、`ArtifactPrMainWindow::closeEvent` が QSettings (ArtifactStudio/ArtifactPr) へ保存、コンストラクタ終端で復元する。
- **価値（確度高い）:** vcpkg の `qt-advanced-docking-system` 依存が、Artifact 側の opt-in `ARTIFACT_QADS_COMPAT_*`（既定 OFF）だけを残して消えた。QADS の floating resize や palette propagation の既知バグ（`docs/bugs/BUG_QADS_FLOATING_*.md` 群）からも解放される。
- **懸念（未検証）:** 実ビルド・実機確認は未実施。QADS と異なる点として、(1) アクティブタブのフォーカスアクセント（violet contour + underline）は未実装で、標準 `QTabWidget` のスタイルに委ねている。(2) タブを誤って閉じないための pinned 機能は未実装で、Artifact 側の `NativeDockSurface` が持つ `setDockPinned` 相当がない。(3) ドラッグの `areaForPosition` は QADS の dock guide のようなガイド表示を持たず、位置からの推定のみ。(4) `dropEvent` の `event->target()` 経路がタブ挿入優先の判定を簡易化しており、実 drag で意図したタブへ入らない可能性がある。
- **次に確認すべきこと:** `cmake -DARTIFACT_BUILD_PR=ON` で configure して `ArtifactPr` ターゲットが通るか。通ったら ① タブ drag の順序変更 ② float → 再 dock ③ 終了→再起動でレイアウト復元 ④ close タブ → tab list から restore、を順に実機で確認する。特に (4) の drop 判定は最初に見るべき。




## 2026-09-27 — ICU は qtbase の推移依存で既にディスク上にあったが、名前が無かった

- **関連:** `vcpkg.json`、`ArtifactCore/CMakeLists.txt`（`find_package(ICU REQUIRED COMPONENTS uc)`）、`build/vcpkg_installed/x64-windows/include/unicode/`、`build/vcpkg_installed/x64-windows/share/harfbuzz/harfbuzzConfig.cmake`。
- **確認できた事実:** ICU 78.2 のヘッダ（`uscript.h` / `ubidi.h` / `uchar.h`、205ファイル）は既に `vcpkg_installed` にあり、qtbase の feature として導入されていた。`icuuc.lib` も存在した。一方 `harfbuzzConfig.cmake:39` の `HARFBUZZ_FEATURES` は `core;freetype` のみで、同ファイルの `icu` ブロック（`:71-74`）は dead コードだった。**つまり HarfBuzz の unicode 関数は ICU ではなく内蔵実装のままで、ICU をリンクしても HarfBuzz の unicode 関数が ICU になるわけではない。**
- **対応:** `vcpkg.json` に `icu` を名前として追加し、`ArtifactCore` から `ICU::uc` をリンク。script property と UAX #9 は ArtifactCore 側が ICU を**直接**使う。
- **価値または懸念（未検証）:** 名前が無い依存は「使えるのに使っていない」状態で、発見にはディスク上の実体確認が必要になる。icuuc / icuin / icudt / icutu / icuio のうち `uc` のみで十分なのは、script と bidi が `uc` にあるため。リンク時間への影響は未計測。
- **次に確認すべきこと:** `find_package(ICU REQUIRED COMPONENTS uc)` が `icuuc.lib` のみに解決されることを確認する。`icu` feature を HarfBuzz に有効化する必要があるか（unicode 関数を ICU 実装に切り替えたい場合）を判断する。




## 2026-09-27 — 「未検出」を既定値にするとセレクタが嘘を伝播する

- **関連:** `ArtifactCore/src/Text/TextShapingBackend.cppm`（`scriptTagForCodepoint`、`isComplexScriptTag`）、`ArtifactCore/include/Font/FreeFont.ixx`（`resolvedFamilyForText`）。
- **確認できた事実:** 旧 `scriptTagForCodepoint()` は未検出の文字に `Latn` を返していた。これは単なる「未対応」ではなく、`contract.scriptRuns` 経由で Text Animator のセレクタとインスペクタに誤った値を渡すことになる。**同じ「既定値が嘘になる」形が `FreeFont.ixx:227` にもあり、`needsFallback` が `containsCjkCharacters() || containsEmojiCharacters()` で AND されているため、Devanagari / Thai / Arabic / Hebrew の glyph 欠落を検出しても fallback が発火しない。** 検出できた場合は正しいが、検出できなかった場合の既定値が「無害な fallback」ではなく「存在しない字体」になっている。
- **対応:** script は `Zyyy`(Common) / `Zinh`(Inherited) をそのまま返すようにした。「スクリプトを持たない文字」は「スクリプトを持つ文字」ではない、という区別を contract に残す。`isComplexScript` の判定も「`Latn` 以外」ではなく複雑 script の明示リストにした（`Zyyy` / `Zinh` は複雑ではない）。
- **価値または懸念（未検証）:** **フォント fallback の gate は未修正**。script 判定が正しくなっても、Preferred 字体が Devanagari を持たない場合 `needsFallback` は false のままで、豆腐のまま描画される。script を正しくするだけではフォールバックは直らない。
- **次に確認すべきこと:** `FreeFont.ixx:227` の gate を撤廃して「Preferred 字体有这个 codepoint の glyph があるか」だけを条件にする。script キーの fallback テーブル（Devanagari → Noto Sans Devanagari、Thai → Noto Sans Thai、Arabic → Noto Naskh Arabic など）を追加し、対応する字体を同梱するかどうかを決める。




## 2026-09-27 — shaping 成果が「正しいのに描かれない」原因はキャッシュのキーが狭すぎた

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`（`ResolvedGlyphFont`、`resolvedGlyphFont`）、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`（同名のキャッシュ）、`ArtifactCore/src/Text/GlyphAtlas.cppm`（`acquire`）、`ArtifactCore/include/Text/GlyphAtlas.ixx`（`GlyphKey`）。
- **確認できた事実:** `GlyphKey` は `shapedGlyphIndex` / `shapedGlyphIndices` / `sequenceUtf8` を持っており、`GlyphAtlas::acquire` も `shapedGlyphIndex != 0` ならそちらを優先する（`GlyphAtlas.cppm:355`）。**しかし production の2つのレンダラは `charCode` だけで GlyphKey を作り、これらの欄を空にしていた。** atlas は `glyphIndexesForString(QString::fromUcs4(&key.codePoint, 1)).first()` で1 codepoint の glyph を引くので、U+0301 は glyph index 0 になり `valid=false`（`:360-365`）で描かれない。`shapedGlyphIndex` を実際に埋めていたのは `ArtifactTextGlyphSubmitter.cppm` だけで、Insight.md に「未接続」と記録された smoke 用 Piece だった。emoji ZWJ も同じ理由で最初の codepoint だけになり、ZWJ（index 0）が飛ばされた。
- **対応:** キャッシュの同一性判定に `shapedGlyphIndex` と `clusterText` を含め、`resolvedGlyphFont` を `GlyphItem` を受ける形にした。ハッシュには `shapedGlyphIndex` と `clusterText.size()` を使い、全文比較は衝突時のみ行う。`renderModeForCodePoint` の再推測もやめ、contract の `glyph.renderMode` を使う。
- **価値または懸念（未検証）:** **「データ構造は準備済みなのに production パスへ接続していなかった」典型例。** 契約（`GlyphKey::shapedGlyphIndex`）と実装（`acquire` の優先分岐）は 2026-08-14 の時点で既に正しく、production 2箇所の接続だけが欠けていた。shaping を正しくしても、キャッシュのキーが狭ければ情報は黙って捨てられる。Font キャッシュを `codePoint` だけで持つ設計は「1 codepoint → 1 glyph」が暗黙の前提になっている場合にだけ安全で、shaping を入れると破綻する。
- **次に確認すべきこと:** `ResolvedGlyphFont` の 2048枠が `clusterText` の QString コピー込みで足りるかを測る。script run ごとに `shapedGlyphIndex` が変わるため、cache hit 率が下がっていないかプロファイルする。`ArtifactTextGlyphSubmitter` を production に繋ぐか smoke 専用として残すかを決める（`Insight.md` の「未接続」記録を解消する）。




## 2026-09-27 — QLocale::name() は POSIX 形式、HarfBuzz は BCP-47 を期待する

- **関連:** `ArtifactCore/src/Text/TextShapingBackend.cppm`（`toBcp47LanguageTag`、`shapeWithHarfBuzz`）、`ArtifactCore/src/Text/GlyphLayout.cppm`（`TextLayoutEngine::layout`）、`Artifact/src/Layer/ArtifactTextLayer.cppm` / `Artifact/src/Render/DiligentImmediateSubmitter.cppm`（`request.locale` の設定）。
- **確認できた事実:** production の3経路は `request.locale = QLocale::system().name()` を設定していたが、Windows 上の値は `"tr_TR"` のようなアンダースコア形式。HarfBuzz の `hb_language_from_string` は BCP-47（`"tr-TR"`）を期待し、アンダースコア形式は `HB_LANGUAGE_INVALID` を返す。つまり **トルコ語の点なし i が反映されない状態だった。** さらに `TextLayoutEngine::layout` は空文字を渡し、HarfBuzz 経路に language が渡らないケースがあった。
- **対応:** `toBcp47LanguageTag()` でアンダースコアをハイフンに変換してから `hb_buffer_set_language` に渡す。language だけでなく script も明示的に渡す（`locl` の選択には script が必要）。`TextLayoutEngine::layout` も `QLocale::system().name()` に変更。
- **価値または懸念（未検証）:** locale は**テキストの言語ではなく宿主の言語**。`QLocale::system().name()` は OS の UI 言語なので、ファイルやペーストされたテキストの言語とは一致しない。OS が `tr_TR` なら無関係なテキストに点なし i が適用される可能性もある。テキストごとの locale 指定は `TextShapingRequest.locale` で既に可能だが、それを設定する UI が存在するかは**未確認**。
- **次に確認すべきこと:** `TextShapingRequest.locale` を UI から設定できる経路があるか確認する。テキストの言語を自動判定するなら、script 検出（ICU 済）と Unicode の language tag（script からの推定）から求める手もある。`locl` の実効を `i` / `İ` / `ı` の3形態で shaped glyph index を比較して検証する。



## 2026-09-27 — ArtifactPr の dock は一度もビルドされていないため、PImpl 型ミスマッチが潜伏していた

- **関連:** `ArtifactPr/include/DockSurface.ixx`（削除済み）、`ArtifactPr/src/DockSurface.cppm`（削除済み）、`ArtifactPr/src/ArtifactPrMainWindow.cppm`（`setCentralWidget`、10 パネルの `addDockWidget`）、`ArtifactPr/include/ArtifactPrMainWindow.ixx`、`ArtifactPr/CMakeLists.txt`、`Artifact/include/Widgets/Dock/NativeDockSurface.ixx`、`Artifact/include/Widgets/Dock/DockManager.ixx`、`Artifact/CMakeLists.txt`（`ArtifactDockFoundation`）。
- **確認できた事実（静的読み取り、ビルド未実行）:** `ArtifactPr` には `Artifact.NativeDockSurface` の独立 fork である `ArtifactPr.DockSurface` が存在した。データモデル（`QHash<QString,QWidget*>` の dock 登録、固定 5 個の `QTabWidget` メンバ、`QHash<QString,QDialog*>` の浮動管理）、JSON キー構造、`tabGroup` の `tabs:` / `floating-tabs:` 規則、`area` の文字列表現は `Artifact` 側と**一致**していた。差分は `Area` enum の**数値**（Artifact は `Right=1, Center=4`、ArtifactPr は `Center=1, Right=2`）、`pinned`、`DockTabBar` / `DockSurfaceStyle` の owner-draw、`DockDropPreview`、`DockSplitter`、`qApp->installEventFilter` の欠落だった。**旧 `DockSurface` は PImpl のみで `QWidget` を継承しておらず**、`ArtifactPrMainWindow.cppm` の `setCentralWidget(dockSurface)` に暗黙変換が成立しない。`ARTIFACT_BUILD_PR` は既定 OFF（全 preset で OFF）で `out/build/x64-Debug/CMakeCache.txt` でも OFF であり、**一度もコンパイルされたことが潜在阻害として潜伏していた**。
- **訂正した誤情報:** 調査過程で「ArtifactPr の dock レイアウトは永続化されていない」という記述を一旦得たが、これは誤り。`saveDockLayout()` / `restoreDockLayout()`（`ArtifactPrMainWindow.cppm:3795-3832`）が `closeEvent` とコンストラクタから呼ばれ、`QSettings` INI に `dock/layout` と `dock/geometry` を保存していた。同様に「`ArtifactPr/CMakeLists.txt:145` に QADS リンクが残存」も誤りで、同ファイルは 153 行で `:145` は `target_compile_definitions` であり、QADS リンクは既に削除済み（ソース内 `ads::` 参照 0 件）。`grep` の行番号を鵜呑みにした推測だった。
- **対応:** 共有 leaf pack `ArtifactDockFoundation`（STATIC）を `Artifact` サブツリーに新設し、`Artifact.DockManager` と `Artifact.NativeDockSurface` を移設。`Artifact` と `ArtifactPr` / `ArtifactPrCLI` の双方がリンクする形にした。`ArtifactPr.DockSurface` は削除。`setAreaTabPosition` / `areaTabPositionAtBottom` は `NativeDockSurface` の private を public 契約へ昇格した。無効だった `QMainWindow::saveState` / `restoreState` の永続化も削除した。
- **価値または懸念（未検証）:** **「一度もビルドされていないターゲットには、コンパイル阻害が静かに積み上がる」典型例。** `ARTIFACT_BUILD_PR=OFF` のため、型ミスマッチも enum 数値の不一致も検出されずに残っていた。さらに `Artifact` 側は `QMainWindow::saveState()` を意図的に不使用（`ArtifactWorkspaceManager.cppm:124-125`）のに、ArtifactPr 側はこれを永続化していた。中身は `QDockWidget` を登録していないので空だった。**統合は機能追加ではなく、この潜在阻害を解消する作業だった。** 残る懸念は、`NativeDockSurface` が `import Widgets.Utils.CSS;` しており pack 側が `ArtifactCore` 経由でそのモジュールを提供している構成になっている点（静的確認のみ、実ビルド未検証）。
- **次に確認すべきこと:** `ARTIFACT_BUILD_PR=ON` で configure し、`ArtifactPr` が首次にコンパイルされることを確認する。その後 10 パネル（project / media / sourceMonitor / programMonitor / timeline / audioMeters / transitions / effects / proxy / clipProperties）の 5 エリア配置、QSettings 経由の保存・復元、`Artifact` 側の既存 dock 操作（タブ drag / float / dock back / pinned / close）の無回帰を確認する。実ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-27 — ArtifactViewMenu の補助パレットだけ生 QDockWidget のまま

- **関連:** `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`（`addFloatingDock`、`setFloating`）、`Artifact/src/Widgets/Menu/ArtifactTimeMenu.cppm`、`Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm`、`Artifact/include/Widgets/Dock/NativeDockSurface.ixx`。
- **確認できた事実（静的読み取り）:** `addFloatingDock()` が生の `QDockWidget` を `setFloating(true)` + `setGeometry()` で生成している。Color Palette / Effect Palette / Color Science などの補助パレットが対象で，`NativeDockSurface` とは別系統のまま混入している。`QMainWindow::saveState()` を使わないため、これらの floating は `main_window_layout.cbor` に永続化されない見込み。`setAllowedAreas` は `Artifact` 配下に grep 0 件。
- **価値または懸念（未検証）:** 同一画面内に 2 系統の docking が混在している状態。用户在補助パレットを float しても、workspace 保存・復元では戻らない可能性が高い。ただしこれは既存の挙動であり、本統合（`ArtifactDockFoundation`）の範囲外として意図的に残した。
- **次に確認すべきこと:** 補助パレットを `NativeDockSurface` の floating へ統一するか、生 `QDockWidget` のまま受け入れるかを決める。統一する場合は `ArtifactMainWindow.cppm` の dock 登録側が変わるので、workspace モードの可視性ルール（`workspaceVisibilityRuleFor`）との整合も確認が必要。



## 2026-09-27 — テキストレイヤー Gizmo 整合性監査と 4 件の修正

- **関連:** `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`（`draw` / `hitTest` / `handleMousePress` / `handleMouseMove`）、`Artifact/include/Widgets/Render/ArtifactTextGizmo.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（Puppet press、`isTransformGizmoHovered`）、`Artifact/src/Widgets/Render/ArtifactCompositionTextPuppetUndoCommands.cppm`（`Deformation2DStateUndoCommand::apply`）、`Artifact/src/Render/PrimitiveRenderer2D.cppm`（`drawSolidRectTransformed`）。
- **確認できた事実（静的読み取り、ビルド・実機未確認）:** (1) Box ハンドルは `transformedBoundingBox()`（scale/rotation 適用済みのワールド AABB）を直接リサイズし、その結果をローカルの `text.maxWidth` / `text.boxHeight` へ書き戻していたため、scale>1 で 1 ドラッグごとに寸法が倍増し、回転中は方向依存の歪みになっていた。(2) 範囲セレクタの hit 判定は `RangeOffset` を固定優先で返すため、重なった場合は常に offset が掴まれた。(3) 3 箇所にあった `QRectF(0,0,400,100)` フォールバックは到達不能だった。`ArtifactTextLayer` は `localBounds()` を override せず基底実装（`sourceSize()` 由来）に依存するが、`setSourceSize` が常に `max(1, ...)` を保証するため `transformedBoundingBox()` は空にならない。(4) `isTransformGizmoHovered` が `TextGizmo` を参照せず、テキスト選択時の hover が通知されなかった。`CharacterSelect` は enum 宣言のみで実装が 0 件だった。
- **訂正:** 調査エージェントが「ベースライン描画 (`:535-540`) の座標系が `drawSolidLine` 側 (`:543-552`) と不整合」と報告したが誤り。`PrimitiveRenderer2D::drawSolidRectTransformed` は `a*x+c*y+tx` / `b*x+d*y+ty` で正しく線形写像しており、両者は同じ `overlayTransform` に渡している。バグではない。
- **実装:** Box リサイズをローカル矩形ベースへ変更（押下時に `maxWidth()` / `boxHeight()` と global transform の inverse を捕捉し、canvas delta を inverse でローカル delta へ変換）。範囲セレクタの hit を近接順に変更。400x100 フォールバックを 3 箇所で廃止。`CharacterSelect` を削除。`isTransformGizmoHovered` に TextGizmo を追加。Puppet のピン追加をテキストで fail-closed 化。
- **価値または懸念:** Puppet の fail-closed は `Deformation2DStateUndoCommand` を使う全 8 経路に適用した（`handleMousePress` の追加ピン `:26653` と既存ピン掴み `:26614`、`deleteSelectedPuppetPin` `:36004`、`setSelectedPuppetPinType` `:36047`、`resetSelectedPuppetPinRotation` の Starch `:36208` と Bend `:36227` と Overlap `:36252`、`adjustSelectedPuppetPinWeightAt` `:36285`、`adjustSelectedPuppetPinDepthAt` `:36321`）。`handleMouseMove` の drag 経路（`:31497` / `:31521`）は press 時に `puppetPinDragging_` が立たないため到達不能。**ただし `Deformation2DStateUndoCommand::apply` 自体は依然として `ArtifactImageLayer` cast のままであり、テキスト以外の新規レイヤー型を追加したfuture に同じ穴が再発する。command 側の型ガードは未実装。**
- **次に確認すること:** scale/回転状態で Box ドラッグが意図量だけ寸法を変えるか、縦書きの Range セレクタが近接側で掴めるか、Puppet の警告表示が適切に出るか、テキスト選択時にピンが一切つかめないことを実機で確認する。



## 2026-09-27 — 画像/平面レイヤーの移動ギズモ: 描画されない2Dギズモが不可視ハンドルで押下を奪っていた

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`sync2DGizmosForLayer`、`layerUsesProjectedFrameGizmo`、`handleMousePress` の projected-frame / 3D / 2D の3ブロック、overlay draw）、`Artifact/src/Widgets/Render/TransformGizmo.cppm`（`allowsHandle`、`hitTest`、`handleMousePress`、`computeRotateRingGeometry`）、`Artifact/include/Widgets/Render/TransformGizmo.ixx`。
- **確認できた事実（静的読み取り、ビルド・実機未確認）:** 画像/平面/ソリッドレイヤーの選択時，2D `TransformGizmo` は `sync2DGizmosForLayer` で `setLayer(layer)` されるが，overlay draw は `use2DTransformGizmo = layerUsesTextGizmo(selectedLayer)` の分岐に閉じており（`ArtifactCompositionRenderController.cppm:48579`），これらレイヤー型では `gizmo_->draw()` が一切呼ばれない。通常の press パス（`:28361` → `handleMousePress` → `hitTest`）は生存しており，`hitTest` は Mode::All で（画像選択時に `:18574-18577` で Mode::All へ強制）レイヤー矩形内部の `HandleType::Move` を返し，さらに `computeRotateRingGeometry` の外周リング（`baseRadius + max(18/zoom, 14)`）で `Rotate` を返す。投影フレームの8隅ヒット（`:6338-6349`，±max(18, 32*DPR)/2 px）は小さく，2D 側のリング/ハンドルはこの外側まで及ぶため，**不可視ギズモが押下を奪い，`AnimatableTransform3D` 直書き + `MultiTransformUndoCommand` という，描画している 3D 経路（property path + key，`GizmoTransformUndoCommand`）とは異なる transform/undo 経路で確定する**。外周リングでの回転と，隅の外向き `offsetPointAwayFromCenter(..., scaleOutset)`（`TransformGizmo.cppm:2495-2503`）バンドでのリサイズが該当。2D 経路は Alt でスナップ解除のみ，Shift 軸拘束なし，とモディファイ挙動も 3D 経路と異なる。
- **訂正:**  調査サブエージェント（2つ目）が提示した「Move バグ候補10件」は**旧 2D `TransformGizmo` の分析であり、本件（画像/平面）の実際の移動経路ではない**ため採用しなかった。誤った前提の上に 10 件の候補が積み上がっていた。また、「`gizmoBasisFor` が `GizmoSpace::World` で空基底を返すので軸矢印が死んでいる」という初期の未確認記載も**訂正**する。`space_` の既定は確かに `GizmoSpace::World`（`Artifact3DGizmo.ixx:118`）で通常マウスドラッグ中に `setSpace(Local)` は一度も呼ばれない（`:25345` は modal 変形時、`:28186` は投影フレーム resize 時のみの一時設定で、いずれも drag 終了時に復元）。**しかし空基底でも `axisHandleEndFor` が `center + dir*length` で `end == center` に縮退するため、描画と hit が同時に縮退し、視覚的な破綻（見えているのに掴めない）は表面化しない。** Screen 内側ドラッグ（`projectedFrameMove_`）は `axisDir = viewDir` で基底不要（`Artifact3DGizmo.cppm:1049-1051`）、投影フレーム resize は Local 強制のため、いずれも影響を受けない。軸矢印が実質縮退している自体は事実だが、ユーザー可視の「デッドハンドル」として表面化しないため、**本件とは別の優先度低い観察**とする。
- **実装（2段階）:** ① `TransformGizmo` に `setInvisibleHandlesSuppressed(bool)` を追加し，`allowsHandle` の先頭で `Rotate` / `Scale_*` を無効化（`Move` と `Anchor` は残す）。`sync2DGizmosForLayer` で `!useTextGizmo && layerUsesProjectedFrameGizmo(layer)` のとき有効化。`allowsHandle` は `hitTest` の単一 choke point なので，`cursorShapeForViewportPos` / `handleAtViewportPos` も同時に直る。② **本命の press パス側ガードを追加:** anonymous namespace に `legacy2DGizmoShouldOwnPress(controller, layer)`（`:21054`）を定義し，2D press ブロックの条件を `impl_->gizmo_ && legacy2DGizmoShouldOwnPress(this, gizmoLayer)` にした（`:28380-28381`）。投影フレーム対象レイヤーでは Design ワークスペースでない限り false を返す。**このガードが成立する根拠は，Design 並べ替えの `isDesignWorkspace(this) && activeHandle()==Move` ガード（`:28383-28388`）が既に press 内側にあり，Design 外では 2D ギズモに legit な押下理由がゼロだったこと。** これにより press パスから 2D ギズモの呼び出し自体が消え，所有権の二重化を根本から解消した。`handleMouseMove` / `handleMouseRelease` / `cancelInteraction` はいずれも `isDragging()` ガードで無害（`TransformGizmo.cppm:2924` で確認）。①と②は二層防御として両方残す。**`setLayer(nullptr)` による全解除を避けた理由:** Design 並べ替えが `Move` に依存しており，全解除するとその機能が壊れる。`Mode::Full` で 2D ギズモに `Mode::None` が渡されるケース（`:18004`）は `allowsHandle(Mode::None)` が既に Move のみを許可しており（`TransformGizmo.cppm:2908-2909`），追加抑制の影響を受けない。
- **価値または懸念（未検証）:** 「同じ選択に2つのギズモが束縛され、片方が非可視のまま hit を持つ」型の所有権不整合。描画パス（テキストのみ）と press パス（全レイヤー型）の**分岐基準がずれている**ことが根本原因で，`setInvisibleHandlesSuppressed` はその症状への対処である。②の press ガードも追加したため，press パスから 2D ギズモの呼び出し自体が残るという懸念は解消した。現状のフラグ方式は Design 機能を犠牲にせず所有権の二重化を解く最小変更だった。②のガードがあるためフレーム外の点でも 2D ギズモには到達しない。**`hitTestProjectedFrameInterior` は projected.z() を 0..1 で clip する（`:6409-6411`）ため，稀に内側ドラッグが 2D ギズモの Move に落ちる。ただし発生条件は狭い: 2D 合成ビューのフォールバックは ortho 投影で near=0 / far=±1000（`:48548-48550`）かつ 2D レイヤーの z≈0 なので clip しない。`viewportOrientationMatricesValid_` は実 3D カメラがある時のみ true（`:39075-39078`）で、その場合だけ perspective 投影が near/far を突き抜ける可能性がある。つまり **カメラシーンで選択レイヤーが near/far 外にある場合のみ** の残存であり、デフォルト运用では起きない。
- **次に確認すべきこと:** (1) 画像レイヤー選択時，フレーム外周（リング相当の位置）で左クリック→ドラッグしても**回転/リサイズが起きない**こと。代わりに通常の操作（3D 軸 / 2D 移動）に流れること。(2) 内部ドラッグ（Screen）と 3D 軸ドラッグ，X/Y 軸矢印ドラッグに回帰がないこと。(3) Design ワークスペースでの兄弟並べ替えが引き続き機能すること。(4) DPR=2 で実際に不可視バンドが消えるか（修正前の斜め約 10px バンドは DPR=1 の理論値）。(5) カメラを極端な斜角／背面に向けたとき，フレーム内ドラッグが 2D ギズモの Move に落ちないか。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-26 — Text Animator 整合性監査：3つの追加関数がプロパティキャッシュを更新せず、Undo でキーフレームが消える

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`（`addAnimator` / `addAnimatorProperty` / `addAnimatorPreset` / `removeAnimator` / `restoreTextAnimatorStack`）、`Artifact/src/Layer/ArtifactAbstractLayerPropertyRouting.cppm`（`getProperty` / `persistentLayerProperty`）、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`（`applyTextAnimatorStackMutationWithUndo`）、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`（`applyTextAnimatorMutationWithUndo`）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`（`addDefaultTextAnimatorWithUndo`）。
- **確認できた事実（実コード照合）:** `getProperty` は `propertyCache_` を引くだけで、**キャッシュミスなら nullptr を返す**（`ArtifactAbstractLayerPropertyRouting.cppm:187`）。プロパティの実体は `persistentLayerProperty` が `getLayerPropertyGroups()` 内でしか作られない。`serializedAnimatorProperties` は `layer->getProperty(prefix + suffix)` を使い、null なら `continue` する（`ArtifactTextLayer.cppm:1535-1536`）。ここで `removeAnimator:2384` と `restoreTextAnimatorStack:2440` はどちらも `(void)getLayerPropertyGroups();` を明示的に呼んでいるが、**`addAnimator` / `addAnimatorProperty` / `addAnimatorPreset` の3関数だけが呼んでいなかった**。Undo ヘルパーは変更直後に after スナップショットを取る（`ArtifactLayerPanelWidget.cppm:354`、`ArtifactPropertyWidgetShared.cppm:1143`、`ArtifactCompositionRenderWidget.cppm:98`）ため、新 animator の `animatedProperties` が空になり、**keyframe / expression / envelope が snapshot 欠落 → Undo で消える**。
- **対応:** 3関数に `(void)getLayerPropertyGroups();` を追加。BUG-1 と同型の 3箇所すべてが対象で、1箇所だけ直すと他2経路に同じ欠陥が残っていた。
- **価値または懸念（未検証）:** 同じクラスで**dirty flag が経路ごとに違う**（`ArtifactPropertyWidgetShared.cppm:1008` は `LayerDirtyFlag::Effect`、他2経路は `LayerDirtyFlag::Property`）。また `ArtifactCompositionRenderWidget.cppm:97` の `addDefaultTextAnimatorWithUndo` は `addAnimator()` しか呼ばず、`addAnimatorProperty` / `addAnimatorPreset` に到達できないため、Viewport 右クリックだけは個別プロパティ追加ができない。`ArtifactTextGizmo.cppm` 全体に `areLayerMutationsAllowed` が 0 件で、他3経路と異なる collaboration guard の抜け穴になっている。`applyColorToSelectorRange` は Undo コマンドもガードも無い。ビルド・実機は未確認。
- **次に確認すること:** BUG-3 の collaboration guard を 3 経路に揃える。dirty flag を `Property` に統一する。Viewport 右クリックを個別プロパティ追加まで届くようにする。`addAnimatorProperty` / `shouldHideTimelinePropertyGroup` / `text.animators` を含むテストが 0 件なので、キャッシュ登録の回帰テストを追加する。



## 2026-09-26 — 式エディタを ReSharper 化。署名基盤・AST 位置情報・診断・Pick Whip・フォーマッタを実装

- **関連:** `ArtifactCore/include/Script/Expression/ExpressionEvaluator.ixx`、`ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm`、`ArtifactCore/include/Script/Expression/ExpressionParser.ixx`、`ArtifactCore/src/Script/Expression/ExpressionParser.cppm`、`Artifact/src/Widgets/ArtifactExpressionCopilotWidget.cppm`、`ArtifactCore/include/UI/ShortcutBindings.ixx`、`Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`。
- **確認できた事実（実コード照合）:** ① 構文エラーの波線は既に動作（`getErrorPosition/getErrorLength` → `applyErrorSelection`）。② `ExprNode::Impl` は位置情報ゼロ（メンバ 5 個のみ）。③ 一方 `Token` は既に start offset を保持し、ノード生成箇所では `tokens_[currentToken_-1]` が取れた。④ `ExpressionEvaluator::Impl::error_` は `ZeroString` のみで評価エラーに位置が無く、public に `getErrorPosition()` も無かった。⑤ **補完リストが実エンジンと乖離**：`registerStandardFunctions()` は 47 関数だが UI 側 `rootSuggestions()` は 16 個のみで **31 関数が補完に現れなかった**。⑥ `registerFunction()` は map 代入のみでシグネチャ情報の記録先がどこにも無かった。⑦ `ArtifactProblemViewWidget` はコンパイルされるが `import`/`new` の実使用箇所ゼロ（UI 未マウント）。⑧ **レイヤーの名前取得は `name()` ではなく `layerName()`**（`ArtifactAbstractLayer.ixx:393`）。⑨ `ArtifactProblemViewWidget` が使う `ProjectDiagnostic` には line/column フィールドが無い。
- **対応:** `ExpressionFunctionInfo` / `ExpressionParamInfo` を evaluator に追加し、`registerStandardFunctionInfos()` で **47 関数すべてに実挙動に基づくシグネチャ**を張った（登録 47 / 署名 47 の完全一致を powershell で diff 検証済み）。UI のハードコード 16 個は削除し、署名から候補を生成する形に変更。`ExprNode` に source range、`Token` に end offset、`lastConsumedTokenEnd()` を追加（全 `makeShared<ExprNode>` 箇所で `setSourceRange`、quoted string の start が開き引用符の後を指す不整合も修正）。評価エラーは `setErrorAt()` で失敗ノードの範囲に紐付け。`getErrorPosition/getErrorLength` を public 化。UI 側は行番号＋現在行ハイライト、署名ホバー、Problems ストリップ、`thisComp.layer("...")` の Ctrl+クリック Pick Whip、ロールバック付きフォーマッタを追加。ショートカットは AGENTS.md に従い `ShortcutBindings` のローカルバインド 11 件として登録。
- **価値または懸念（未検証）:** 評価器は `evaluateNode` が**最初のエラーで打ち切る**ため、Problems は現状 1 件表示に確定した。複数件を本当に成立させるには評価器の error recovery が必要だが、挙動リスクが高い。署名の `ExprValueType` は保守的に決め、判断できない引数は `Null`(=any) のままにして型を隠していない。`error_` をメモ化ハッシュが読んでいる（`ExpressionEvaluator.cppm:203`）ため、位置は別フィールドに分離してある。**署名は評価結果を左右しない純粋な記述データ**なので、誤った記述があっても実行結果には影響しないが、补完表示の誤りは残る。フォーマットは字句処理のみで識別子名を変えず、整形後に再パースして失敗時は元に戻す。ビルド・実機は未確認。
- **次に確認すること:** ビルド許可後に (a) 補完に `sqrt` `loopIn` `valueAtTime` `posterizeTime` 等 47 関数が出ること、(b) `wiggl(3,50)` で波線＋Problems ストリップが**評価エラー位置**（構文でなく）に出ること、(c) `thisComp.layer("Text1")` の Ctrl+クリックでタイムライン選択が変わること、(d) 意図的に構文を壊した式に Format を当て元に戻ること、(e) Ctrl+= / Ctrl+- / Ctrl+0 が Timeline 側と衝突せず式エディタ内だけで効くこと、(f) Ctrl+F / Ctrl+H / F3 / Shift+F3 が式エディタ内でだけ効き、Find バーが表示・非表示になること。
- **補足（自己レビューで判明した不整合）:** ショートカット 11 件のうち `ExpressionFind` / `ExpressionReplace` / `ExpressionFindNext` / `ExpressionFindPrevious` は**初期登録だけしてイベント側を配線していなかった**ため、設定画面には「Ctrl+F 検索」と表示されながら実際には何も起きない状態だった。发现自己で Find/Replace バー（検索入力、件数表示、Next/Previous、Replace、Replace All、Close）を追加し、11 件すべてを配線済み。`Replace All` はカーソル走査ではなく `QString::replace` で実装しており、置換文字列自身に検索語が含まれる場合の無限ループを構造的に排除している。



## 2026-09-26 — `ArtifactWidgets::CodeEditor` は死にコードで、記述した 2 文書が実態と乖離

- **関連:** `ArtifactWidgets/include/Code/CodeEditor.ixx`、`ArtifactWidgets/src/Code/CodeEditor.cppm`、`ArtifactWidgets/src/Code/SyntaxHighlighter.cppm`、`docs/FEATURE_DICTIONARY_2026-04-17.md:131`、`docs/CHILD_MODULE_IMPLEMENTATION_MAP_2026-07-02.md:104`。
- **確認できた事実（実コード照合）:** `ArtifactWidgets::CodeEditor` は行番号ガターが実装済み（`CodeEditor.cppm:61-100`）だが、**ハイライタの `highlightBlock` が空関数**（`SyntaxHighlighter.cppm:99-102`）で、number/string/comment/builtin の regex は宣言のみ（`:59-62`）。**参照元はゼロ** — `import CodeEditor` / `import Code.` の grep 結果は自身のファイル内のみ。`ArtifactWidgets/CMakeLists.txt` の GLOB でコンパイルされるが誰も使わない。
- **価値または懸念:** 上記 2 文書はこれを「実装済み コードエディタ」として記載しているが実態と一致しない。再実装を選ぶ場合はまずこの 2 文書の記述を訂正し、新規実装（死にコードの蘇生ではなく）とするのが妥当。**今回は Expression エディタのみを対象にしたため未着手。**
- **次に確認すること:** 2 文書の記述訂正をユーザー判断で行うか。「内蔵コードエディタ」という名称を今後は実動中の `ArtifactExpressionCopilotWidget` を指す運用に統一するか。



## 2026-09-26 — Project Open ダイアログの採用モックと実装の乖離を画像モックで可視化

- **関連:** `docs/design/project-open-picker/README.md`、`generate_project_open_v2_mockups.py`、`Artifact/src/Widgets/Dialog/ArtifactImportAssetsDialog.cppm:263-371`、`ArtifactProjectOpenPickerDialog`。
- **確認できた事実（実コード照合）:** 2026-09-12 に採用された DCC モック（3 ペイン、project tile、preview、health、composition/asset 数、外部 source 警告、Favorites、grid/list 切替）が存在するが、実装の `ArtifactProjectOpenPickerDialog` は約 110 行で、その意図をまだ満たしていない。Places は `QListWidget` の 2 項目（Recent / System Files）のみ、tile は `QIcon::fromTheme("document-open")` の同一アイコンと basename + lastModified テキストのみ、inspector は `No project selected` と固定文言のラベルだけ。health / composition 数 / asset 数 / 外部 source 警告 / preview 画像 / Favorites / grid-list 切替はいずれも未実装。`QFrame::StyledPanel` と `QListWidget::IconMode` の素の Qt 既定スタイルをそのまま使っているため、密度も色も採用モックと離れている。
- **対応:** 採用モックの 3 ペイン骨格を維持したまま情報密度を埋める Ver2 候補と、現行との before/after 対比を PIL で生成した（既存の `generate_project_view_v2_mockups.py` と同じ流儀）。採用済み画像は読み取り専用として一切変更していない。README に現状と Ver2 の差分表を追記した。
- **価値または懸念（未検証）:** プレビュー画像の実データ取得経路は未設計。project の最終 composition を 1 枚レンダリングしてキャッシュする想定だが、`ArtifactProjectService` 側に project 単位の thumbnail 所有者が存在するかは未確認。health も「未計算なら Healthy と推測せず Unknown / Not checked」とする責務境界があるため、値を推測で埋める実装を 1 枚に決めてはならない。D3D12/Vulkan での preview 取得は未確認。コード実装・ビルドはいずれも未実施。
- **次に確認すること:** project thumbnail の既存所有者があるか（`ArtifactProjectService` / recent project キャッシュ）、health 計算の既存トリガーは何か。preview と health のデータ経路を確定してから実装に入る。ユーザーの承認が得られるまでモックは未採用のまま。



## 2026-09-26 — ビューポート露出コントロール (P1-5) は表示専用 compute 段として新規実装

- **関連:** `Artifact/include/Render/ViewerHelperShaders.ixx`（`g_viewportExposureCS`）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`applyViewportExposure`）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`finalizeGpuRenderToViewport`）、`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`（exposure submenu）。
- **確認できた事実（静的読み取り）:** `finalizeGpuRenderToViewport` の戻り値 `finalPresentSRV` は表示専用ではない。`:12778` で `lastPresentedReadbackSRV_` に代入され、color sampler（`updateColorSamplerOverlay`）、Color Science / Scopes パネル（`ArtifactColorSciencePanel.cppm:1257`）、虫眼鏡オーバーレイ、RAM preview readback がこれを参照する。また `Color` モードはチャンネル表示 compute を通らない（`channelComponentSource` が null のままで `presentationSRV = finalPresentSRV`）ため、Beauty 経路には表示専用 compute 段が従来 1 つも存在しなかった。
- **対応:** 露出結果を `renderPipeline.tempUAV()` に書き、`presentationSRV` の選択時にだけ差し込む設計にした。`finalPresentSRV` と `lastPresentedReadbackSRV_` は無変更のまま維持するため、サンプリング値・出力・Render Queue には露出が一切掛からない。`Color` モード限定なので AOV 表示との衝突は構造上起きない。compute は `ArtifactIRenderer::Impl` の自己完結 executor とし、`ArtifactCore::LayerBlendPipeline`（子リポジトリ）には依存していない。既存 `BlendParams` の `static_assert(sizeof == 48)` も無変更。
- **identity 保証:** 既定値（gain 0 stop / gamma 1 / saturation 1）は shader 側で厳密な恒等変換になる（`exp2(0)=1`、`pow(c,1)=c`、`lerp(luma,c,1)=c`）。alpha は無変更で通し、color sampler が alpha を参照するため表示専用でも不変が安全。`saturate` は表示専用パスにしか掛からない。
- **価値または懸念（未検証）:** Render Queue は `CompositionRenderController` を参照しないため影響を受けない。一方 `tempUAV` を露出とチャンネル表示の双方が使うため、両者が同一フレームで走ることはないものの、将来的に `Color` 以外のモードへ露出を拡張する場合はこの共有頂点に注意が必要。hot path の毎フレーム確保はゼロ（executor と 16 byte の cbuffer は初回のみ確保、以降は map/memcpy のみ）。PSO 構築失敗時は fail-soft で無露出表示へ素通しする。ビルド・実機・D3D12/Vulkan の parity は未確認。
- **次に確認すること:** ビルド許可後に `check_module_hygiene`、Gain をプラス・マイナスに動かして HDR 明暗が変化し、**color sampler と Color Science の値が変わらない**こと、Render Queue で同じ project をレンダーして出力に露出がかからないこと、既定値では表示が完全に同一であること。P1-10 Clipping 警告と P1-12 カラーサンプルバーも同じ表示専用ポストプロセス段の派生として接続できる。



## 2026-09-26 — P1-12 カラーサンプルバーは既に実装済みで、マイルストーンだけが「未着手」だった

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`updateColorSamplerOverlay:46315`、`drawColorSamplerOverlay:46405`、`captureCurrentFrameImage:22477`）、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`（トグルと状態復元）、`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`（P1-12）、`docs/analysis/HIEROPLAYER_GAP_ANALYSIS_2026-09-22.md`（#4）。
- **確認できた事実（実コード照合）:** M-VP-DCC-1 の P1-12「カラーサンプルバー（ソース RGBA 生値）」は `Not Started` と書かれていたが、実装は既に存在した。`updateColorSamplerOverlay` が `captureCurrentFrameImage()` から 1px を読み、RGB / HSL / hex / Layer ID / canvas XY / image pixel を保持し、`drawColorSamplerOverlay` がそれを HUD パネルへ描画する。表示トグルは `setShowColorSamplerOverlay`、UI と状態復元は `ArtifactCompositionEditor` に既にある。一方 `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md` は「Color Sampler = 実装済み」と正しく記録しており、**マイルストーンだけがコードと乖離していた**。
- **対応:** 3 文書（マイルストーン、HieroPlayer 分析、DCC パリティ分析）の P1-12 / #4 / 優先順位 #9 相当を「実装済み・再実装不要」へ更新した。HieroPlayer 分析の #1 露出調整も今回実装済みなので「未実装」から更新し、未実装は #2 Clipping / #3 スコープ / #5 OCIO / #6 アスペクトマスクの 4 件のみとした。
- **価値または懸念（未検証）:** 今回の P1-5 露出は `lastPresentedReadbackSRV_` を無変更で保つため、Color Sampler の読み取り値は露出の影響を受けない（要件どおり）。ただし両者が同じ readback 面を共有しているので、将来 Color Sampler を「表示後（露出適用後）」の値に変更する場合は P1-5 と明示的に切断する必要がある。P1-12 の残る差は HieroPlayer の複数点・常時バー形式への拡張のみで、これは本マイルストーンのスコープ外とした。実機表示は未確認。
- **次に確認すること:** ビルド許可後に Color Sampler の表示が従来どおりであること、露出を動かすと Sampler の数値が**変わらない**ことを確認する。



## 2026-09-26 — Hiero 残り 4 件（P1-10/11/13/14）を実コード照合。2 件が「未着手」表記と乖離

- **関連:** `docs/planned/HIEROPLAYER_VIEWER_INSPECTION_PRESTUDY_2026-09-26.md`（新設）、`Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`、`ArtifactCore/cmake/ArtifactCoreSources.cmake:279-281`、`Artifact/src/Widgets/Render/ViewportColorPipeline.cppm:63-69`、`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm:1541`。
- **確認できた事実（実コード照合）:** P1-12 に続いて、残り 4 件も 2 件がマイルストーンの「Not Started」と実態が乖離していた。**P1-11** は 4 スコープ（Histogram / Waveform / Vectorscope / Parade）が既に実装済みで、`ArtifactColorSciencePanel` の 2×2 ダッシュボードと `ArtifactCompositionEditor` の dialog 両方から開ける。GPU 版の `ScopeComputer` / `Histogram` はソースが完全なまま `.cppm` だけビルド除外されている（`.ixx` のみ `ArtifactCoreSources.cmake:279,281` に登録）。**P1-13** は OCIO が実依存（`find_package(OpenColorIO CONFIG REQUIRED)`）で統合済み、`bakeViewTransformLUT` による 33³ LUT も存在するが、適用は `applyDisplayColorTransform` 経由で **composition-space cache 経路の 2 か所だけ**（`:42141` / `:42155`）で、メインの `finalizeGpuRenderToViewport` には入っていない。View menu の `useDisplayColorManagementAction` は connect が無く dead。
- **対応:** 検討文書を作成し、4 件の判定を「P1-10 / P1-14 = 未着手（導入可）」「P1-11 / P1-13 = 一部実装済（接続と範囲の設計が先行）」に分けた。コード変更はゼロ。
- **価値または懸念（未検証）:** P1-11 の GPU 化と既存スコープ widget の改修は ArtifactCore / ArtifactWidgets（いずれも submodule）の変更を要し、AGENTS.md により親だけでは完結しない。fork／パッチ運用の判断か、CPU 側で完結する実装（既存 widget へ ROI 矩形だけ渡す）の選択が必要。`docs/memo/OCIO_MISSING_FEATURES_2026-08-01.md` の「実 OCIO 未統合」「TransferFunction は 4 種のみ」はいずれも古い（実際は 17 種）。ビルド・実機は未実施。
- **次に確認すること:** マイルストーンの P1-11 / P1-13 行を本検討の判定へ書き換える。P1-11 の ROI 集計を CPU 側で完結させるか GPU 化するか、P1-13 の per-pane 状態の期待動作をどうするかをユーザー判断で確認する。



## 2026-09-26 — Timeline glyph submission allocated a UTF-32 string every draw

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`drawGlyphText`), `Artifact/include/Render/PrimitiveRenderer2D.ixx`, `ArtifactCore/include/Utils/UniString.ixx`。
- **確認できた事実:** TimelineのDiligent rendererはlabelsを各presentで再送する。`drawGlyphText()`は既存のglyph scratch vectorを再利用していたが、`UniString::toStdU32String()`の戻り値として毎call `std::u32string`を作っていた。ArtifactCoreのpublic `UniString`にはallocation-free codepoint view/iteratorがない。
- **対応:** `UniString::toQString()`のimplicitly shared copyを読み取り、UTF-16 surrogate pairを直接decodeして、既存のrenderer-owned `glyphCodePointScratch_`を2回走査するよう変更。UTF-32のtemporary stringをなくし、QtのUCS-4 conversionと同じく不正surrogateをU+FFFDとして扱う。constructorで1024個分reserve済みのscratchは`clear()`後もcapacityを保つため、入力UTF-16長に基づく過大reserveも外した。
- **価値または懸念（未検証）:** static timeline labelsからper-present UTF-32 heap buffer生成がなくなる。QString shared copyはQt documented O(1) copy-on-write。scratch vectorは実際のunique codepoint数がcapacityを越すと拡張する。新規glyph font/atlas cache miss、実際のallocation削減量とCPU時間は未計測。
- **次に確認すべきこと:** allocation counterでlabel長別のsteady-presentを測り、scratch growth後にUTF-32/string allocationが残らないこと、BMP・supplementary plane・孤立surrogateが従来のUCS-4変換と一致することを確認する。
- **資料:** [Qt QString implicit sharing and UTF-16/UCS-4 conversion](https://doc.qt.io/qt-6/qstring.html), [Qt implicit sharing](https://doc.qt.io/qt-6/implicit-sharing.html)。



## 2026-09-26 — Sprite texture cache collision and QImage identity

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`computeImageContentKey`, `m_spriteTexCache`, `m_maskTexCache`).
- **確認できた事実:** The ImageF32 fallback cache key uses a bounded pixel sample and cannot provide exact identity; Qt documents `QImage::cacheKey()` as identifying image contents, changing when the image is altered, and remaining shared for implicitly-shared copies.
- **対応:** Replaced the QImage sampled fingerprint with `QImage::cacheKey()`. Added a stable ImageF32 texture-key overload for image-layer, composition draw, SVG, Text raster fallback, and Puppet paths, keyed by source UUID/version, optional sequence-frame or timeline-frame key, dimensions, and color descriptor. Temporary source overrides keep using the transient sampled fingerprint. Added a UV-aware texture-view sprite draw overload so cropped paths preserve UV mapping while reusing the keyed texture; the existing no-UV overload retains its prior packet path.
- **video経路の境界:** Stable frame identity is not added to Video. `cachedFrameImageBuffer()` and `isFrameCached()` query the decoded frame cache without first calling `refreshSourceVersionIfNeeded()`, so a stale decoded frame can briefly be paired with the current Asset version. A source/frame cache key alone could then retain old pixels under the new version. Decoder and last-good repeat behavior remain unchanged.
- **確認追記:** Text fallback uses `contentRevision()` plus the layer's `currentFrame()`. `ArtifactAbstractLayer::setCurrentFrame()` maps each composition frame to `globalFrame - inPoint + startTime`, so the frame portion remains distinct while text keyframes/animators are evaluated at the composition timeline frame.
- **価値または懸念（未検証）:** Prevents QImage cache hits across distinct contents while preserving hits for implicitly-shared copies. Stable ImageF32 callers avoid pixel sampling; transient fallback reads at most 4096 evenly spaced bytes and mixes them into a 64-bit fingerprint instead of hashing only the leading 4 KiB into 32 bits. This reduces alias risk for changes elsewhere in the image, but remains a lossy key. Separately materialized but identical QImages no longer deduplicate. Build and runtime behavior remain unverified.
- **追記 (2026-09-26):** Composition draw call sites include transient overlay images, generated ghost/frame images, and rasterized layer surfaces; some composition surfaces already have owner/version handles in `GPUTextureCacheManager`, while the low-level sprite map receives only pixels. Masked texture draws also use the sampled key. This confirms that one universal source ID is unavailable at the primitive API boundary.
- **次に確認すべきこと:** Continue classifying transient ImageF32 call sites by mutation lifetime; prefer owner/version keys where available, and measure the bounded sampler cost and collision behavior on representative generated surfaces.



## 2026-09-26 — Viewport culling repeats effect-expanded bounds work

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`effectExpandedLayerBounds`, `recordLayerDamage`, partial-recompose eligibility, composition draw loops).
- **確認できた事実:** TGFX 2.1.1 release notes call out culling layers outside the visible area. Artifact already skips layers whose effect-expanded bounds miss the ROI, but the bounds helper calls `enabledRasterizerOverscanPixels()`, which walks the layer effect list. The render controller calls this helper from damage recording, partial-recompose eligibility, and drawing loops; eligible partial frames can evaluate the same layer bounds in the eligibility pass and again in the draw pass.
- **確認できた事実:** `effectRevision()` advances through `setDirty(LayerDirtyFlag::Effect)`. `hasAnimatedEffectProperties()` recognizes keyframes, expressions, envelopes, and modulation and memoizes that classification by the same revision. Effect-service property edits mark the layer's Effect dirty.
- **対応:** Added a per-layer rasterizer-overscan cache keyed by `effectRevision()` for effects classified as static. Animated effects keep the direct evaluation path, preserving frame-varying `roiHint()` behavior.
- **価値または懸念（未検証）:** Repeated bounds expansion no longer walks static effect lists after the first evaluation at a revision. The cache uses a short mutex for concurrent access; contention and the net CPU effect are unmeasured. Correct invalidation depends on effect mutations continuing to advance Effect revision.
- **次に確認すべきこと:** Trace enabled-state and undo/redo edits through `setDirty(Effect)`; compare overscan before/after property edits and across animated frames; measure effect-list traversals in representative compositions.



## 2026-09-26 — Composition View matte self-reference filter allocation

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`docs/technical/HOT_PATH_RULES.md`。
- **確認できた事実:** Composition frame rendering copied each layer's matte references into `effectiveMatteReferences` with `reserve`/`push_back` solely to exclude a self-reference before applying mattes. The matte application helper now accepts an excluded source layer ID and skips it during its existing loops, removing that extra per-frame vector allocation. `ArtifactAbstractLayer::matteReferences()` still returns a vector by value; changing that API's ownership/thread-safety contract is a separate, unverified design question.
- **価値または懸念（未検証）:** This reduces allocator work in matte-enabled composition rendering without changing the matte stack order or the incomplete-source behavior. Runtime allocation and image parity have not been measured.
- **次に確認すること:** After build/runtime authorization, compare self-reference, disabled reference, multiple matte blend modes, and missing-source diagnostics against the prior behavior; profile allocation counts. Consider a read-only matte-reference view only after checking mutation synchronization and lifetime guarantees.



## 2026-09-26 — LOD source conversion precedes surface-cache lookup

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`buildLayerSurfaceCacheKey()`、`drawLayerForCompositionView()`。
- **確認できた事実:** The image branch with rasterizer effects or masks calls `toQImage()` and `downsampleForLOD()` before `applySurfaceAndDraw()` checks the surface cache. The cache key uses source version, crop signature, sequence frame/content key, LOD surface dimensions, and frame only for animated inputs. A cache hit can therefore still pay for source conversion and downsampling first. `ArtifactImageLayer::toQImage()` caches its F32-to-QImage conversion and crop only on the main thread; the background path converts the buffer on each call. `ArtifactRenderQueueService` calls the same drawing helper for each rendered frame with a persistent surface cache, so unchanged static image inputs may repeatedly cross that boundary before hitting the effect-surface cache.
- **対応:** `buildLayerSurfaceCacheKey()`を`QSize`ベースにし、静止画は現行F32 buffer、連番画像はsequence更新後のsource寸法とresolved frame identity、SVGはloaded sourceのversionとsource寸法、legacy shapeはparametric width/heightから既存経路のsurface寸法を求め、crop状態更新後に既存のstatic/surface cacheを先行照合する。全レイヤーのkeyに`maskRevision()`と`effectRevision()`を追加する。Effect revisionはLayerDirtyFlag::Effectを含むsetDirty、effect stack操作、Effect Serviceのproperty setter、enable操作、UndoManagerのproperty／modulation／effect-mask変更通知、layer effect JSON restoreから進める。animated effect propertyの判定結果をrevision-keyed atomic stateに保持し、steady frameでの`getEffects()`／`editableProperties()` snapshotとkeyframe走査を避ける。`AbstractProperty::hasKeyFrames()`を追加し、cache key内のgradient/crop/shape animation判定とlayer opacity評価ではkeyframe vectorをコピーせずshared lock下で有無だけを見る。さらに`keyFrameCount()`を追加し、変換チャンネル、Undo検証、テキストキー復元、アニメーションコマンドの件数比較では配列をコピーせず件数だけ取得する。純粋な有無判定の残存箇所にも同APIを適用する。静止画／連番画像／legacy shapeのsurface寸法は既存どおりLOD縮小後、SVGは既存どおりsource寸法とする。連番のresolved indexが無効なら従来経路へ戻す。shape contents stackは`localBounds()`と既存の64Mpx上限から寸法を求める。Shapeのcache keyには新設した`contentRevision()`を含め、shape `markDirty()`とpath-keyframe編集で進める。静的Textはanimator、source-text keyframe、scene lightがない場合にF32処理結果をsurface cacheへ保存し、以降のcache hitでは`toQImage()`とeffect処理を避ける。enabled external matte、cache miss、source buffer不在、無効なGPU handleとCPU content不在でも従来のQImage経路へ戻す。ヒット時は既存の処理済みbuffer/surfaceまたは有効textureを直接描画し、ミス時には同じmatte reference snapshotを既存処理へ渡す。
- **価値または懸念（未検証）:** 背景Render Queueでの反復F32→QImage変換、LOD縮小、およびrasterizer処理をcache hit時に避けられる見込み。cache identity構築とopacity判定でkeyframe vectorの一時コピーも避ける。連番についてはフレーム更新処理自体のdecode/refreshは残る。静的Textでは最初のcache miss時だけ処理済みF32 bufferを共有cacheへ移し、draw時に同じbufferを参照する。通常経路と色・alphaが一致すること、surface generationやlayer mutation後にstale entryを拾わないこと、GPU再uploadが正しいkeyで行われることは静的変更のみでは証明できない。ArtifactPropertyWidgetのeffect edit通知は`setDirty(Effect)`を行う一方、`ArtifactEffectService::setEffectProperty()`は成功後にLayerChangedEventを発行するだけだったため、Effect Service setterにもeffect dirty revision更新を加えた。UndoManagerのproperty／modulation／effect-mask通知も所有layerを走査してrevisionを更新する。layer effect JSON restoreも既存effectのenabled／stage／property値を適用した後にrevisionを進める。Shapeのrevisionを通らない直接mutation経路も呼出し元レビューとruntimeで確認する。effect parameter identityは次の調査対象とする。
- **次に確認すること:** ビルド・実行許可後、静止画像のeffect/mask、crop、LOD切替、scene light、matte source有無、cache eviction/device resetをfull pathと比較する。toQImage/downsample/effect実行数とGPU時間をRender Queueで測定し、差がない場合やkeyのずれがあれば早期経路を修正する。




## 2026-09-26 — Static layer cache trim runs only on insertion

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`trimStaticLayerGpuCache()`、`applySurfaceAndDraw()`、`tryDrawCachedRasterizedSurface()`。
- **確認できた事実:** Before this change, cache trim traversed the entire static layer cache before and after surface draws and before every cached rasterized-surface lookup. The cache is capped at 128 entries and 512 MiB; entries are inserted in the surface draw path and the static text path.
- **対応:** Removed trim calls from lookup/hit paths and run maintenance only after a static cache insertion or replacement. The existing entry and byte limits and LRU-by-frame eviction remain unchanged. Cache byte total is now maintained by subtracting the replaced/evicted entry and adding the inserted entry; trim no longer sums every entry on each insertion. Diagnostics read the maintained total.
- **価値または懸念（未検証）:** A cache hit no longer scans all static entries, and a normal in-budget insert performs no full-cache byte scan. Over-budget eviction still searches the bounded 128-entry cache and may evict the just-inserted entry if its size exceeds the cache budget; the active draw retains its local shared buffer. Runtime cache pressure and frame-time effect have not been measured.
- **次に確認すること:** Build/runtime authorization後、steady hitsでtrimが起動しないこと、128件／512MiB超のinsertで上限維持とcurrent draw成功を確認し、cache hit pathのCPU時間を計測する。




## 2026-09-26 — Surface cache keys must preserve authored numeric precision

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`。
- **確認できた事実:** Solid surface identity serialized RGBA values and gradient parameters with fixed precision of four decimals (and bounds with two); crop signature used 12 significant digits; source-time identity used three fractional digits. A distinct input could therefore serialize to the same key and reuse a stale processed surface.
- **対応:** Float key components now use 9 significant digits and double components use 17, including Solid colors/gradient values, Solid bounds, crop state, and source-time mapping.
- **価値または懸念（未検証）:** This removes decimal-rounding aliases for finite float/double values at the cache-key boundary. Longer keys may cost more to format and compare; rendered output and timing have not been measured.
- **次に確認すること:** Compare full-frame and cache-hit output for parameter edits smaller than 1e-4, crop changes below 1e-12, and nearby source-time values; profile key construction cost.




## 2026-09-26 — Move processed F32 surfaces into the cache

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`ArtifactCore/include/Image/ImageF32x4_RGBA.ixx`。
- **確認できた事実:** Three cache-miss paths wrapped a local processed `ImageF32x4_RGBA` in `SharedPtr` by lvalue, invoking its deep-copy constructor even though the local value was not used afterward. The type already provides a `noexcept` move constructor that transfers its backing `cv::Mat`.
- **対応:** Those paths now move the processed image into the shared cache entry.
- **価値または懸念（未検証）:** Avoids one full processed-image copy and its temporary peak memory on each of those cache-miss paths. Cache-hit behavior is unchanged; frame-time savings have not been measured.
- **次に確認すること:** Profile cache-miss allocation and copy time at representative surface sizes; verify later cache hits and fallback draws preserve the same pixels.




## 2026-09-26 — First-applied effect presets must invalidate layer surfaces

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`EffectPresetSnapshotCommand`、layer surface cache revision key。
- **確認できた事実:** Effect preset callers mutate the effect before pushing `EffectPresetSnapshotCommand`. Its first `redo()` intentionally skips the already-applied snapshot, but also skipped `notifyPropertyChanged()`. Later undo/redo notified owners, so the initial preset application alone could leave the layer's cached effect revision unchanged.
- **対応:** The first redo now emits the same effect-owner property notification as later redo/undo, advancing each owning layer's effect revision.
- **価値または懸念（未検証）:** This prevents the surface cache from accepting the pre-preset entry under an unchanged revision after preset application. Runtime cache hit behavior is unverified.
- **次に確認すること:** Apply an effect preset to a cached layer, compare the next render to a full rebuild, then verify undo/redo and preset rejection compensation.




## 2026-09-26 — Layer effect envelopes participate in surface invalidation

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** `LayerEffectEnvelope` drives per-frame effect strength in the rasterizer effect context. Its setter marked only the generic `Property` flag, leaving the effect revision unchanged, and both surface cache keys omitted the frame unless an effect property itself was animated. Thus an enabled envelope with effects could reuse a processed surface across frames or after envelope edits.
- **対応:** Envelope mutation now marks both Property and Effect and records both reasons. Both surface key paths include the requested frame, layer-relative frame, and active composition frame when the envelope is enabled and the layer has effects (or effect properties are animated). The Render Controller also uses the cached `hasAnimatedEffectProperties()` query instead of taking and scanning effect/property snapshots on every key build.
- **価値または懸念（未検証）:** Avoids stale surface reuse when effect context frames differ and removes recurring snapshot/scan work. Using effect count is conservative: disabled or non-rasterizer effects can still cause per-frame cache entries when the envelope is enabled. Runtime correctness and cache pressure are unverified.
- **次に確認すべきこと:** With build/runtime authorization, compare envelope scrubbing and edit/undo in Composition View and Render Controller against forced surface rebuilds; inspect opacity/effect dirty consumers and measure cache churn.




## 2026-09-26 — Render Controller surface identity omitted effect revision

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`buildLayerSurfaceCacheKey()`。
- **確認できた事実:** The Render Controller key contained surface generation and mask revision but no effect revision, unlike Composition View. A static effect value mutation could therefore keep the same key when no animated-frame field was present.
- **対応:** Added the layer effect revision to the controller key.
- **価値または懸念（未検証）:** Static effect changes now invalidate that surface identity consistently with Composition View. Actual cache eviction/replacement and output parity need runtime verification.
- **次に確認すべきこと:** Edit a cached static effect parameter and enabled state, then check cache miss/replacement and undo/redo against a full surface rebuild.




## 2026-09-26 — Controller cache key retained rounded numeric aliases

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`buildLayerSurfaceCacheKey()`。
- **確認できた事実:** The Render Controller owns a separate key builder from Composition View. It formatted remapped source time/blend/rate at three decimals, Solid/SolidImage colors at four fixed decimals, and bounds at two decimals. Different authored inputs could produce the same key.
- **対応:** Raised float identity fields to 9 significant digits and double identity fields to 17 significant digits in this key builder.
- **価値または懸念（未検証）:** Avoids stale surface reuse caused by those rounding aliases; larger serialized keys may have a small formatting and comparison cost.
- **次に確認すべきこと:** Exercise small parameter edits through the controller cache against full rebuild output and measure key generation cost.




## 2026-09-26 — Render Controller key omitted Shape and SVG content revisions

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`buildLayerSurfaceCacheKey()`。
- **確認できた事実:** Composition View keys track Shape `contentRevision()` and SVG `sourceVersion()`, but the controller did not. A Shape fill/path edit with stable dimensions/type or same-path SVG reload could collide with the old controller surface identity.
- **対応:** Added those two revision fields to the Render Controller key.
- **価値または懸念（未検証）:** Prevents stale cache identity for these content mutations. Cache replacement behavior and pixel parity need runtime verification.
- **次に確認すべきこと:** Exercise Shape path/fill edits and SVG reload with unchanged bounds/path through the controller cache, compare against full surface rebuilds.




## 2026-09-26 — Render Controller image key omitted sequence and animated crop state

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、image `buildLayerSurfaceCacheKey()` branch and image rasterizer path.
- **確認できた事実:** Composition View tracks sequence index/content and frame-scopes animated crop properties. The controller did neither, and its effect/mask path converted to QImage without refreshing animated crop first.
- **対応:** Added sequence frame identity, animated crop frame scope, and crop refresh before the controller's rasterizer/mask image conversion.
- **価値または懸念（未検証）:** Prevents stale sequence/crop surfaces and ensures effect processing sees the evaluated crop. Runtime frame/parity behavior remains unverified.
- **次に確認すべきこと:** Test sequence advance plus animated source crop with effects/masks through both cache paths against forced surface rebuilds.




## 2026-09-26 — Animated cache identity should include evaluated clocks

- **関連:** Both `buildLayerSurfaceCacheKey()` implementations in Composition View and Render Controller.
- **確認できた事実:** Effect keys carry requested, layer-relative, and active composition frames. Animated crop, Shape, Video, and Text branches had only the requested frame even though render inputs may use `layer->currentFrame()` or active composition state.
- **対応:** Added the three-clock identity to these animated/time-dependent branches in both builders.
- **価値または懸念（未検証）:** Prevents cache hits across differing evaluation clocks; it may conservatively split entries where the resulting pixels are equivalent.
- **次に確認すべきこと:** Exercise explicit-frame/offline rendering with caller, layer, and composition frames intentionally desynchronized, then compare each animated input against uncached output.




## 2026-09-26 — Controller solid keys omitted gradient inputs

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、Solid2D/SolidImage branches in `buildLayerSurfaceCacheKey()`.
- **確認できた事実:** Controller identities included only base color and bounds, while the rasterized source also depends on fill type and gradient endpoints/angle/reverse/center/scale/offset. Composition View tracked those inputs and gradient keyframes.
- **対応:** Added those parameters and animated-gradient frame identity to both controller branches.
- **価値または懸念（未検証）:** Prevents same-key reuse after gradient edits; key formatting and property checks may add a small CPU cost. Runtime cache behavior is unverified.
- **次に確認すべきこと:** Validate gradient edits and animation scrubbing with effect/mask surfaces against forced rebuild output.




## 2026-09-26 — Unsupported controller surface types must not inherit generic identities

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`buildLayerSurfaceCacheKey()` and `applySurfaceAndDraw()`.
- **確認できた事実:** Unknown surface types received only common layer/effect fields. Precomp pixels also depend on referenced composition revision, child frame, and per-instance overrides. The caller appended option suffixes even when the base key was empty, defeating an unsupported-key opt-out.
- **対応:** The key builder now returns empty for unsupported layer types, and cache-affecting suffixes are applied only to nonempty source identities. Unsupported types render through the existing uncached processing path.
- **価値または懸念（未検証）:** Avoids stale surface reuse for precomp and future unkeyed sources. Effect/mask precomp rendering may cost more until a complete instance-aware key is implemented.
- **次に確認すべきこと:** Verify child edits, child-frame changes, and instance overrides against uncached output; profile the recomputation cost before designing a complete precomp key.




## 2026-09-26 — Solid gradient frame keys needed the same evaluated clocks

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、Solid2D/SolidImage branches of `buildLayerSurfaceCacheKey()`.
- **確認できた事実:** Gradient animation entries in Composition View keyed only the requested frame, while Render Controller used requested, layer-relative, and active composition frames.
- **対応:** Aligned both Composition View gradient branches to the same three-clock identity.
- **価値または懸念（未検証）:** Removes cache aliases when clocks diverge without affecting static gradients. Runtime parity remains unverified.
- **次に確認すべきこと:** Compare gradient animation through both cache paths with caller and active clocks out of sync.




## 2026-09-26 — Static solid pointwise GPU hits can bypass stack reconstruction

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`SolidPointwisePreviewCache::resolve()`.
- **確認できた事実:** The bounded cache checked its signature only after copying effect vectors and reconstructing the pointwise stack. Static supported inputs are identified by the layer effect revision and quantized opaque source color; animated effect properties and layer envelopes need dynamic evaluation.
- **対応:** Added a pre-stack hit for static inputs after device/context validation. Envelope cases bypass it and include current sampled strength; exact-signature hits refresh the cached revision.
- **価値または懸念（未検証）:** Avoids repeated vector allocation and stack work on steady static hits. GPU command ordering, animated behavior, and hit performance need runtime verification.
- **次に確認すべきこと:** Measure static-hit CPU cost and compare output across static edits, animated Exposure, envelope scrubbing, and device/context reset.




## 2026-09-26 — Effect modulation was absent from animated cache classification

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`hasAnimatedEffectProperties()`；`Artifact/src/Service/ArtifactEffectService.cppm`、`setEffectModulationSnapshot()`.
- **確認できた事実:** Effects evaluate modulation assignments in `setContext()` each composition frame. The layer animation detector ignored router targets, and the direct no-Undo service path did not advance effect revision after restoring a changed modulation snapshot.
- **対応:** Include active modulation targets in animated-effect detection and mark layer effects dirty after successful direct modulation changes. Undo paths already notify owners.
- **価値または懸念（未検証）:** Prevents modulated values from taking the static GPU shortcut and invalidates the detector's revision cache on direct edits. Runtime modulation behavior needs verification.
- **次に確認すべきこと:** Scrub LFO/Macro-driven Exposure and edit/undo router assignments while comparing cached output to uncached evaluation.




## 2026-09-26 — Text cache identity omitted animated style properties

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm` (`draw()` animated property paths)、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (Text surface keys).
- **確認できた事実:** Text drawing evaluates keyframes for 33 font, layout, color, stroke, and shadow properties. The surface key included frame clocks only for source-text keyframes or animator stacks, allowing stale reuse when only a style property was animated.
- **対応:** Keep one static list for the 33 Text draw-evaluation paths. Cache identity and Composition View's separate static raster cache use `hasAnimatedTextProperties()` to scan registered `text.*` entries with one property-cache lock rather than 33 independent path lookups.
- **価値または懸念（未検証）:** Prevents stale Text surfaces for text-property animation while leaving static Text reusable and reduces lock acquisition count in both cache paths. Runtime parity and relative cost of map scan versus path lookups remain unmeasured.
- **次に確認すべきこと:** Compare font-size/color/shadow animation against forced rebuilds and confirm static cache hits after build/runtime authorization.




## 2026-09-26 — Shape cache key used display sorting for an animation predicate

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (Shape cache keys)、`ArtifactCore/src/Property/PropertyGroup.cppm` (`allProperties()` / `sortedProperties()`).
- **確認できた事実:** Both Shape cache key paths only test for any keyed property but called `sortedProperties()`. That method copies the property list and invokes `std::stable_sort`; `allProperties()` returns the same insertion-ordered property snapshot without display sorting. The returned collections and Shape layer property-group construction remain allocations.
- **対応:** Switched both cache-key scans to `allProperties()` so they do not sort properties for a boolean keyframe check.
- **価値または懸念（未検証）:** Removes unneeded ordering work and sorting workspace in a render hot path. It does not remove the property group/vector allocations, so runtime effect may be small.
- **次に確認すべきこと:** Profile Shape cache-key cost and allocation count; investigate a non-copying Shape-owned animation summary only if measurements justify it and it can include dynamic content/operator properties.




## 2026-09-26 — Shape surface keys can inspect registered properties without rebuilding groups

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/src/Layer/ArtifactAbstractLayerPropertyRouting.cppm`、Shape surface keys in `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` and `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** `persistentLayerProperty()` stores property objects in the layer's `propertyCache_`; Shape's `getLayerPropertyGroups()` repeatedly constructs vectors and dynamic property groups from those objects. Keyframe mutation operates on the cached property object, so the keyframe state can be queried by scanning registered `shape.*` cache entries.
- **対応:** Added a prefix-based cached-property query that holds one cache lock, visits registered property handles without copying the map or building groups, and checks keyframe state. Both surface keys use it while retaining the explicit path-keyframe query.
- **価値または懸念（未検証）:** Removes group/vector/sort construction from Shape surface key generation and still covers dynamic content/operator paths that have been registered. Map traversal and nested property shared locks remain; actual CPU/allocation impact needs profiling. Keyframe state created before the property is registered is not discoverable by this API, so load/edit ordering must be verified.
- **次に確認すべきこと:** Verify fresh-project load and timeline keyframe creation for legacy and dynamic Shape paths, then compare the query's cache contents and resulting rendered surfaces against forced rebuilds.




## 2026-09-26 — Source Crop cache identity can use a mutation revision

- **関連:** `Artifact/include/Layer/ArtifactSourceCrop.ixx`、`Artifact/src/Layer/ArtifactSourceCrop.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`、Composition View and Render Controller image surface keys.
- **確認できた事実:** Both image surface key builders called `sourceCropSignature()`, which formatted 12 numeric/boolean fields, and the cropped-QImage cache built the same string for equality on each access. Crop state mutations use `SourceCrop` setter methods, `fromJson()`, or `clampToSource()`.
- **対応:** Added a monotonic revision in `SourceCrop`, exposed it through `ArtifactImageLayer`, and replaced cache-key and cropped-QImage comparisons with that revision. Both key builders use a `sourceCrop.*` property-cache scan to detect animated fields; animated crop still includes its three frame clocks.
- **価値または懸念（未検証）:** Removes repeated formatted-string work and repeated per-field property lookups from crop cache checks. The revision advances only when normalized state changes, so repeated evaluation of a held keyframe can hit the same entry; static crop reuse and animated correctness remain runtime-unverified.
- **次に確認すべきこと:** Verify every crop field through edit/undo, animated scrubbing, source dimension changes, and relink; compare both cache paths to forced rebuild output.




## 2026-09-26 — Video surface keys omitted the asset source version

- **関連:** `Artifact/include/Layer/ArtifactVideoLayer.ixx`、`Artifact/src/Layer/ArtifactVideoLayer.cppm`、Video branches of `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` and `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** Video cache identity used the source path, frame clocks, proxy quality, and dimensions but not `AssetManager::sourceVersion()`. Asset content can be revised while retaining the same asset identity/path.
- **対応:** Exposed the current AssetManager source version from `ArtifactVideoLayer` and added it to both surface keys.
- **価値または懸念（未検証）:** Prevents a cache hit from selecting an older frame surface after source revision changes. Version propagation through decode queues and both render paths still needs runtime verification.
- **次に確認すべきこと:** Replace/relink a Video asset at the same identity, then compare Composition View and Render Controller output with cache disabled.




## 2026-09-26 — Solid gradient cache keys repeated property lookups

- **関連:** Solid2D/SolidImage branches in `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` and `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** Each branch tested eight gradient paths separately to determine whether frame clocks belong in the key. Each `getProperty()` locks the layer property cache; static and animated keys both ran that path list.
- **対応:** Replaced the loops with one cached-property prefix scan for `solid.gradient*` in both branches. The exact gradient values and animation clocks in the keys are unchanged.
- **価値または懸念（未検証）:** Reduces repeated mutex acquisition for the predicate. Whether scanning the full cached-property map beats eight path lookups depends on cache size and still needs profiling.
- **次に確認すべきこと:** Validate static gradient cache hits and gradient-only animation against forced rebuilds, then measure key-build time and allocations.




## 2026-09-26 — Matte eligibility copied its reference vector

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (`hasEnabledMatteReferences()`)、`Artifact/include/Layer/ArtifactAbstractLayer.ixx` and `Artifact/src/Layer/ArtifactAbstractLayer.cppm`.
- **確認できた事実:** The cache eligibility predicate needed a boolean for enabled external matte presence, but `matteReferences()` returned a copied `std::vector` before the scan. The layer already owns these entries in `NamedVector`.
- **対応:** Added a layer query that iterates the owned `NamedVector` directly, replaced copied-vector scans in cache eligibility and cached-surface lookup, and deferred `applySurfaceAndDraw`'s reference copy until a matte will actually be applied. Image, Shape, SVG, and static Text cache hits no longer snapshot matte references.
- **価値または懸念（未検証）:** Removes vector copies on no-matte draws, cache hits, and draws without matte source images while retaining enabled/source/self-reference conditions. Runtime allocation impact and matte behavior remain unverified.
- **次に確認すべきこと:** Compare the new predicate to previous semantics across enabled, disabled, unresolved, and self-referencing mattes, then measure allocation count.




## 2026-09-26 — Partial recompose skipped missing layer resources

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (opt-in `TGFXPartialRecompose` layer pass).
- **確認できた事実:** Three layer-stage resource guards (intermediate draw, float conversion, and blend) used `continue` regardless of whether partial recompose was active. A skipped intersecting layer could therefore leave the partial region incomplete while the frame pass continued toward damage consumption.
- **対応:** Each guard now aborts the active partial pass and sets its failure state; the existing failure path prevents presentation and requests a full redraw. Non-partial full rendering retains its prior continue behavior.
- **価値または懸念（未検証）:** Keeps the retained composition and damage tracker consistent when a required GPU resource is missing. Recovery behavior and backend parity have not been exercised.
- **次に確認すべきこと:** Exercise each resource failure point and verify no partial region is presented or consumed, followed by a complete D3D12 and Vulkan redraw.




## 2026-09-26 — Render reuse eligibility copied effect lists

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`, `Artifact/src/Layer/ArtifactImageLayer.cppm`, and `Artifact/src/Render/ArtifactRenderQueueService.cppm`.
- **確認できた事実:** Cache eligibility/frame-sync, image texture-sharing, and Render Queue format predicates used `getEffects()` snapshots for count/emptiness; rasterizer-work helpers copied the list to inspect enabled pipeline stages, and the surface-cache probe repeated the caller's effect check. `getEffects()` materializes a `std::vector` copy from the layer-owned `NamedVector`.
- **対応:** Replaced count/emptiness snapshots with `effectCount()`, added `hasEnabledRasterizerEffect()` to scan owned entries directly, and removed the redundant cache-probe effect check after verifying all callers are gated.
- **価値または懸念（未検証）:** Removes temporary vector construction and duplicate effect scans from render reuse, frame-sync, image texture-sharing, Render Queue format selection, and rasterized-surface probe paths while preserving effect-presence/enabled-stage semantics. Allocation and frame-time impact are unmeasured.
- **次に確認すべきこと:** Confirm predicate equivalence and measure allocations when the opt-in reuse settings are enabled.




## 2026-09-26 — Render Controller matte predicates copied references

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** Render Controller's matte presence and count helpers called the vector-returning `matteReferences()` API. Its pre-render matte-source pass also copied each active layer's references before checking whether any existed. Most call sites needed only a boolean or count, and the layer state stores references in `NamedVector`.
- **対応:** Added an external matte count query over the owned entries, routed the presence query through it, replaced helper snapshots, and gated the pre-render source enumeration on enabled external-reference presence.
- **価値または懸念（未検証）:** Avoids temporary reference-vector construction in render eligibility, damage dependency checks, matte diagnostics, and matte-free prepass scans, preserving the current enabled/non-nil/non-self predicate. Thread-safety remains governed by the existing layer mutation/rendering contract; runtime behavior is unverified.
- **次に確認すべきこと:** Verify semantic equivalence for disabled, nil, self, and multiple valid references; measure allocations and render-side lock/thread assumptions.




## 2026-09-26 — Rasterized surfaces had cache-dependent LOD dimensions

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (`applySurfaceAndDraw`, Image/Shape/Particle branches).
- **確認できた事実:** Image, Shape, and Particle branches downsampled their QImage before calling the helper, then could downsample again when matte processing bypassed the surface cache. SVG cache lookup intentionally used source dimensions, but its cache-free fallback downsampled.
- **対応:** The helper now accepts whether LOD scaling should be skipped; pre-downsampled Image/Shape/Particle inputs avoid a second resize, while SVG keeps source resolution in cache-free fallback.
- **価値または懸念（未検証）:** Aligns cache and matte fallback dimensions with each source type's cache identity and avoids duplicate CPU scaling. Pixel parity and runtime cost are unmeasured.
- **次に確認すべきこと:** Compare matte-enabled cache-bypass output with cache-enabled output at Low/Medium/High LOD; compare SVG cache/no-cache output and measure resize counts.




## 2026-09-26 — Export snapshot predicates copied effect and matte lists

- **関連:** `Artifact/src/Export/ArtifactExportSession.cppm` (export layer snapshot construction).
- **確認できた事実:** Export snapshot construction copied `matteReferences()` to compute a single active-external-matte boolean and called `getEffects()` twice only to test whether effects existed. It also called `layerHasCpuRasterizerWork()` once while deciding pre-render eligibility and again while building the reason label.
- **対応:** Reused `hasEnabledExternalMatteReference()` and `effectCount()` for those predicates, and cached the rasterizer-work predicate once per layer snapshot; references/effects are still enumerated by actual export work where needed.
- **価値または懸念（未検証）:** Avoids unnecessary vector snapshots and duplicate enabled-stage scans during export preflight while preserving matte/effect selection. Export output is unaffected by these predicate-only substitutions in the inspected path.
- **次に確認すべきこと:** Compare pre-render selection and reason labels for layers with disabled, self, and valid matte refs, and zero/nonzero effect stacks.




## 2026-09-26 — Inspector matte presence copied references

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, and `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` (`setMatteContext()`).
- **確認できた事実:** Inspector interaction state only tested whether the matte reference list was empty, which copied the entire `std::vector`. Its existing semantics count every stored reference, including disabled and self references.
- **対応:** Added `matteReferenceCount()` over the layer-owned `NamedVector` and used it for the presence check, preserving the all-reference semantics.
- **価値または懸念（未検証）:** Avoids allocating a temporary vector while updating Inspector cursor affordance without changing which stored matte references enable it.
- **次に確認すべきこと:** Verify Inspector affordance for empty, disabled-only, self-only, and valid external-reference lists.




## 2026-09-26 — Matte application copied references before discovering no work

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (CPU and GPU matte application helpers).
- **確認できた事実:** Two CPU matte application helpers and the GPU layer-matte preparation path copied the layer's full reference vector before filtering enabled, non-nil, non-self entries. For lists with no active external matte, the vector was used only to return without processing.
- **対応:** Added an owned-entry predicate guard before each snapshot; the QImage helper now also handles a null layer before dereferencing it.
- **価値または懸念（未検証）:** Removes avoidable temporary vector allocations on matte-free paths without changing active-reference iteration. Runtime behavior and allocation impact are unverified.
- **次に確認すべきこと:** Verify CPU/GPU results for empty, disabled-only, self-only, missing-source, and valid references, then profile matte-free rendering.




## 2026-09-26 — Matte summary predicates copied reference lists

- **関連:** `Artifact/src/Widgets/LayerEditorSurfaceInfo.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`.
- **確認できた事実:** Layer Editor's surface summary counted enabled external matte references by iterating the vector-returning accessor. Timeline's layer-tone summary copied the same list only to determine whether any active external matte existed.
- **対応:** The count now uses `enabledExternalMatteReferenceCount()` and the Timeline badge predicate uses `hasEnabledExternalMatteReference()`.
- **価値または懸念（未検証）:** Avoids vector snapshots while preserving the same enabled, non-nil, non-self filters; runtime UI behavior and allocation impact are unmeasured.
- **次に確認すべきこと:** Compare summary counts and Timeline tones for empty, disabled-only, self-only, and valid matte references.




## 2026-09-26 — Rasterizer surface builders copied effects for eligibility

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** Both surface builders copied and traversed the effect list to detect enabled rasterizer work. The snapshot was consumed only in the actual rasterizer application branch, so mask-only/matte-only work still paid for that copy. Render Controller also performed the check when CPU rasterizer effects were deferred to GPU.
- **対応:** Both paths now use `hasEnabledRasterizerEffect()` and retrieve effect snapshots only when applying CPU rasterizer effects; the controller skips the query when those effects are deferred.
- **価値または懸念（未検証）:** Reduces temporary effect-vector construction and duplicate scans for surfaces without CPU rasterizer work. Concurrent effect mutation consistency and runtime output remain unverified under the existing render mutation contract.
- **次に確認すべきこと:** Compare effect-free, mask-only, matte-only, CPU rasterizer, and GPU-deferred outputs; profile effect-free surface processing.




## 2026-09-26 — Composition final effects copied an empty stack

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (`applyCompositionFinalEffectsToBuffer`).
- **確認できた事実:** The final-effect helper copied the composition-owned effect list before checking whether any entries existed. Composition View may call this helper after producing its surface even when the stack is empty.
- **対応:** Added an `effectCount()` guard before the list snapshot.
- **価値または懸念（未検証）:** Removes a temporary empty vector on effect-free compositions; output is unchanged by inspection because the previous path returned false without modifying the buffer when no rasterizer effect was enabled.
- **次に確認すべきこと:** Confirm empty-stack helper behavior and profile compositions without final effects.




## 2026-09-26 — Overscan calculation copied inactive effect stacks

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`layerOverscanPixels`).
- **確認できた事実:** Damage-expanded bounds calculation retrieved the effect vector for each non-adjustment layer, even when no enabled rasterizer effect could contribute expansion.
- **対応:** Added a `hasEnabledRasterizerEffect()` early return before retrieving the list.
- **価値または懸念（未検証）:** Avoids vector construction for empty, disabled-only, and non-rasterizer-only stacks; those cases previously produced zero expansion after traversal.
- **次に確認すべきこと:** Compare bounds for empty, disabled-only, non-rasterizer, rasterizer-without-overscan, and overscan stacks.




## 2026-09-26 — Composition final-effect eligibility copied disabled stacks

- **関連:** `Artifact/include/Composition/ArtifactAbstractComposition.ixx`, `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`.
- **確認できた事実:** Composition View final-effect processing copied all composition effects to determine whether any enabled rasterizer effect existed, then reused the copy for ordered application. Composition owned the effect entries but exposed only count and vector snapshot queries.
- **対応:** Added an owned-entry `hasEnabledRasterizerEffect()` query to Composition and use it to return before taking the effect snapshot when no rasterizer stage is active.
- **価値または懸念（未検証）:** Removes vector creation and a duplicate scan for disabled-only/non-rasterizer stacks. The new public module API and runtime behavior are not build-verified.
- **次に確認すべきこと:** Compare the query against the prior predicate for null/disabled/non-rasterizer/rasterizer entries and verify composition effect rendering after build authorization.




## 2026-09-26 — Final rasterizer sorting copied already ordered stacks

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (layer and composition final rasterizer paths).
- **確認できた事実:** Each path retrieved an effect-list snapshot and then unconditionally called `sortedByStage()`, which copied it again. `ArtifactAbstractEffect::isStageOrderValid()` checks enabled effects in their existing order.
- **対応:** Reuse the retrieved vector when enabled effects are already stage-ordered and invoke stable sorting only for out-of-order stacks.
- **価値または懸念（未検証）:** Saves a second vector copy on canonical stacks. Null and disabled effects are ignored during both order validation and rendering, so their position does not change the executed stage sequence; visual behavior remains unverified.
- **次に確認すべきこと:** Verify ordered/out-of-order, disabled, and null entries against the old sort behavior, then profile allocations and output.




## 2026-09-26 — GPU raster plan copied stacks with no active rasterizer work

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`buildGpuRasterEffectPlan`).
- **確認できた事実:** The GPU raster plan builder retrieved the full effect vector before determining that no enabled rasterizer effect could produce a plan. The builder runs during GPU layer preparation.
- **対応:** Added a direct enabled-rasterizer predicate to its initial eligibility checks.
- **価値または懸念（未検証）:** Avoids the effect-list snapshot and plan setup for empty, disabled-only, or non-rasterizer-only stacks, returning the same ineligible result by inspection.
- **次に確認すべきこと:** Compare plan eligibility for those cases and for supported/unsupported active rasterizer stacks; profile layer preparation.




## 2026-09-26 — Damage invalidation copied effects for a full-frame predicate

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`, `Artifact/src/Layer/ArtifactAbstractLayerImpl.cppm`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** Damage invalidation copied the layer effect vector solely to test whether an enabled effect's `roiHint()` required a full redraw.
- **対応:** Added `hasEnabledFullFrameEffect()` over the owned effect list and routed invalidation through it.
- **価値または懸念（未検証）:** Avoids a temporary vector on damage recording and preserves the exact enabled/full-frame condition by inspection. Render-thread mutation assumptions remain unchanged.
- **次に確認すべきこと:** Compare against the old loop for empty, disabled, full-frame, and bounded-ROI effects; verify property damage is promoted to full redraw only for full-frame hints.




## 2026-09-26 — Overscan sum required only a scalar query

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`, `Artifact/src/Layer/ArtifactAbstractLayerImpl.cppm`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** `layerOverscanPixels()` copied the layer effect vector only to sum `max(0, roiHint.expansionPixels)` for enabled, overscan-enabled rasterizer effects whose hints were not full-frame.
- **対応:** Added `enabledRasterizerOverscanPixels()` on the layer and moved this exact filter/sum over owned entries; the controller now asks for the scalar directly.
- **価値または懸念（未検証）:** Avoids per-call vector construction and shared-pointer copies in render bounds expansion. Numeric equivalence is based on matching the old predicates; runtime impact is unmeasured.
- **次に確認すべきこと:** Compare scalar results for all effect eligibility combinations and verify damage bounds before measuring allocations.




## 2026-09-26 — Mask rasterization copies paths during frame work

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Layer/ArtifactLayerMaskPropertySupport.cppm`, `Artifact/src/Layer/ArtifactLayerMaskMatteState.cppm`.
- **確認できた事実:** Composition View and Render Controller mask application loops call `layer->mask(index)` per render. That API copies the stored `LayerMask`, then applies animated property values to the copy. `applyMaskPropertyState()` also copies each `MaskPath` before applying property overrides. The base mask collection is layer-owned `NamedVector` storage.
- **仮説（未検証）:** Mask-heavy previews may spend notable CPU time and allocate while copying path collections and reconstructing property-path strings on each evaluated frame. A resolved-mask cache keyed by mask revision plus relevant property/frame revision, or a lazy override view over immutable base paths, could remove repeated work while preserving animation. Cache invalidation and concurrent editing make this more delicate than the effect/matte boolean queries.
- **価値または懸念:** Avoiding these copies could reduce frame preparation cost for masked layers, but a stale cache would render incorrect masks. No implementation or performance claim is made yet.
- **次に確認すべきこと:** Trace mask property revision and frame identity ownership; determine whether an existing per-layer cache can own resolved mask data; profile path-copy and property-lookup counts on static and animated masks before designing the API.




## 2026-09-26 — Animated mask properties were missing from surface keys

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayerPropertyRouting.cppm`, `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Layer/ArtifactLayerMaskPropertySupport.cppm`, `Artifact/src/Layer/ArtifactLayerTimelineSupport.cppm`.
- **確認できた事実:** Mask property overrides are evaluated at `currentTimelineTime()`, which reads the active composition frame. Both layer-surface key builders included `maskRevision` but omitted frame identity for animated `mask.*` properties. The existing `hasCachedAnimatedPropertiesWithPrefix()` predicate recognized keyframes but ignored expressions, although mask properties use `evaluateValue(time)`.
- **対応:** Broadened the predicate to include expressions and added requested/layer/composition frame identity to both surface keys when `mask.*` properties are time-varying.
- **価値または懸念（未検証）:** Prevents stale mask surfaces across animation frames in both paths. Expression-driven properties that are effectively static may lose cache hits conservatively; pixel parity and invalidation behavior are runtime-unverified.
- **次に確認すべきこと:** Compare keyframed and expression-driven mask output across frames to forced rebuild, and confirm static mask cache reuse remains intact.




## 2026-09-26 — Static masks resolved unused property paths every draw

- **関連:** `Artifact/src/Layer/ArtifactLayerMaskPropertySupport.cppm`, `Artifact/src/Layer/ArtifactAbstractLayerPropertyRouting.cppm`, `Artifact/src/Layer/ArtifactLayerMaskMatteState.cppm`.
- **確認できた事実:** `applyMaskPropertyState()` constructed per-mask/per-path property names, looked up all supported fields, and copied each `MaskPath` even when no relevant property had keyframes or an expression. Static mask property writes route through `setLayerPropertyValue()`, update the base mask, and advance `maskRevision`.
- **対応:** Added an exact per-mask dynamic-property prefix check before time lookup and resolution work. The prefix includes a trailing dot to avoid index 1 matching index 10.
- **価値または懸念（未検証）:** Avoids repeated path copies, property lookups, and path string construction for static masks; dynamic overrides keep the existing resolver. Rendering parity remains unverified.
- **次に確認すべきこと:** Compare static edits and animated overrides, test adjacent mask indices, and profile static/animated path resolution.




## 2026-09-26 — Static mask rendering can borrow immutable base paths

- **関連:** `Artifact/include/Layer/ArtifactLayerMaskMatteState.ixx`, `Artifact/src/Layer/ArtifactLayerMaskMatteState.cppm`, `Artifact/include/Layer/ArtifactAbstractLayer.ixx`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** The two main CPU mask rasterization loops copied `LayerMask` values from the layer-owned `NamedVector`. Static masks need no property resolution; any dynamic override is evaluated into a copy by `mask(index)`.
- **対応:** Added `maskView()` as an immediate-read borrowed pointer, returning null for invalid indices. Surface rasterization borrows the stored value for static masks and uses the resolved copy only when the matching `mask.N.` property prefix contains keyframes or expressions.
- **価値または懸念（未検証）:** Removes deep copies of mask path collections from static surface processing. The borrowed pointer is invalidated by mask mutation, so callers must not retain it or race with edits; current render-thread assumptions must be verified.
- **次に確認すべきこと:** Verify static/animated output parity, null handling, and mask mutation lifetime; profile allocations and CPU time on path-heavy layers.\n- **追記 (2026-09-26):** `resolvedMaskView()` still calls the general animated-property prefix query per mask. The query now parses each cached path directly, avoiding prefix-string construction, and requires a dot after the numeric index so mask 1 cannot match mask 10. It still locks and scans entries; per-frame rasterization trades some path copies for repeated lookup/lock work. Do not extend this borrowed-view path into more render or hit-test loops until profiling shows a net win; consider revisioned precomputed metadata if cache scans are material.




## 2026-09-26 — Effect-free composition images were converted before eligibility

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (`applyCompositionFinalEffectsToImage`).
- **確認できた事実:** The image helper performed LOD downsampling, QImage normalization, OpenCV conversion, and F32 buffer construction before calling a helper that returned false when no enabled composition rasterizer effect existed.
- **対応:** Added the direct composition effect predicate before those conversions.
- **価値または懸念（未検証）:** Avoids heavyweight temporary image work for empty/inactive final-effect stacks while preserving the helper's prior false result and leaving the input image untouched.
- **次に確認すべきこと:** Verify false/no-mutation behavior for empty, disabled-only, and non-rasterizer-only stacks; measure effect-free finalization cost.




## 2026-09-26 — Composition image finalization repeated effect lookup

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (`applyCompositionFinalEffectsToImage` and buffer helper).
- **確認できた事実:** The image finalizer's new eligibility query was followed by a call to the public buffer helper, which repeated that query and copied the effect list separately after QImage/OpenCV conversion.
- **対応:** Added a private buffer helper that consumes the already retrieved effect list. The image finalizer now shares one snapshot with application; standalone buffer callers keep their own validation and snapshot.
- **価値または懸念（未検証）:** Removes redundant stage detection and effect-vector allocation from the image finalizer while preserving the public function contract and stage ordering.
- **次に確認すべきこと:** Compare image/buffer output for ordered and out-of-order stacks and measure scans/allocations.



## 2026-09-26 — 3D card texture cache needed a hard entry bound

- **関連:** `Artifact/src/Render/PrimitiveRenderer3D.cppm` (`textureFromImage`, `textureCache_`).
- **追記して確認できた事実:** The old `frameCount_` advanced inside each billboard draw call and triggered an O(cache size) prune scan every 60 calls; it was not synchronized to presented frames. `textureFromImage()` updates usage on lookup, so the entry limit and access-order eviction already provide bounded retention without age scanning.
- **対応:** Kept least-recently-accessed eviction and estimated RGBA8 byte accounting. Cache misses evict until both the 50-entry ceiling and 512 MiB byte budget hold; a single larger image is returned for drawing without being retained. Removed the draw-call-based age sweep and its misleading frame counter.
- **価値または懸念（未検証）:** Bounds retained entries and estimated pixel storage while removing periodic full-cache sweeps and false frame-based expiration. The byte estimate is width × height × 4 and excludes backend allocation overhead; evictions can cause re-uploads for larger working sets. GPU residency and pixel behavior remain unverified.
- **次に確認すること:** Exercise >50 images and mixed-size images around 512 MiB on a GPU device; inspect cache residency and confirm oversized one-off images render while leaving the cache unchanged.




## 2026-09-26 — Deferred sprite packets require pinned texture lifetime

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`m_spriteTexCache`, `m_maskTexCache`), `Artifact/include/Render/RenderCommandBuffer.ixx` (`SpritePkt`, `MaskedSpritePkt`), `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`.
- **確認できた事実:** PrimitiveRenderer2D appends sprite packets containing raw `ITextureView*` values to `RenderCommandBuffer`; those packets are submitted later. `ArtifactCompositionRenderController` explicitly submits queued sprites before replacing/evicting a texture they reference. Therefore cache-entry eviction during packet collection can invalidate a queued packet even if the texture was valid when the packet was appended.
- **追記:** PrimitiveRenderer2D now keeps sprite and mask caches to 50 entries each and a shared 512 MiB estimated RGBA8 budget, evicting the globally least-recently-used entry when over budget. `createBuffers()` reserves 50 buckets for each cache. If one requested texture exceeds the byte budget, other cache entries are evicted and that single texture is retained so existing raw-view callers still receive a cache-owned resource.
- **追記:** `RenderCommandBuffer::append()` now forwards packet values directly into its vector's `DrawPacket` variant and pins views in place, removing the intermediate by-value `DrawPacket` move for common typed packet callers. This is a bounded copy/move reduction, not a measured frame-time claim.
- **価値または懸念（未検証）:** Cache eviction no longer invalidates views in queued packets. The per-textured-packet strong reference adds reference-count traffic; GPU output, backend resource retention through deferred execution, and performance impact are unverified. The 512 MiB estimate covers cache-owned entries only: views pinned by the pending packet list can keep evicted textures alive until submit/reset, and the packet count currently has no explicit cap. Byte estimates omit backend allocation overhead; one oversized texture may exceed the cache budget by itself.
- **追加確認できた事実:** Every `ITextureView*` stored directly in a `DrawPacket` is covered by `pinTextureViews()`, including both views in `MaskedSpritePkt` and `BillboardPkt`. `ParticleRenderData` contains CPU particle values and transforms, no GPU view/resource pointer, so `ParticlePkt` needs no additional pin field.
- **次に確認すべきこと:** Verify every packet texture reference survives through D3D12/Vulkan submit and deferred execution, then check sprite ordering across target switches and inspect cache sizes with more than 50 distinct images. Measure packet-size, allocation, and AddRef overhead; inspect cache accounting with mixed-size images and an oversized single texture. If peak in-flight memory proves material, add a bounded submit/backpressure policy instead of assuming cache eviction releases those resources.




## 2026-09-26 — Glyph atlas dirty upload already uses a bounded rectangle

- **関連:** `ArtifactCore/include/Text/GlyphAtlas.ixx`, `ArtifactCore/src/Text/GlyphAtlas.cppm`, `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`uploadGlyphAtlasIfNeeded`).
- **確認できた事実:** A newly packed glyph marks its atlas rectangle dirty, and the Artifact uploader sends only that box with `UpdateTexture()`. Atlas initialization/reset marks a full upload. Multiple glyph writes before upload are merged into one union rectangle.
- **価値または懸念（未検証）:** The main partial-upload optimization already exists; duplicating it in Artifact would be redundant. If many distant glyphs are created before upload, the union may include substantial untouched pixels, but changing that requires a bounded multi-rectangle contract at the ArtifactCore ownership boundary.
- **次に確認すべきこと:** Instrument dirty-region area versus glyph bytes on font-cache cold starts. Only consider a bounded multi-region API if profiling shows meaningful wasted transfer, and make that a separately scoped ArtifactCore request/change.




## 2026-09-26 — Resolved glyph font lookup was linear in the full cache

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`resolvedGlyphFont`, `glyphFontCache_`).
- **確認できた事実:** The renderer caches up to 2048 resolved fonts per TextStyle, but each codepoint lookup scanned the full vector. `drawGlyphText()` invokes that lookup during preload and packet generation; repeated timeline labels can therefore repeat the scan each presentation.
- **対応:** Added a fixed 4096-slot open-address index of 16-bit vector indices. The existing vector remains the owner and 2048-entry reset policy is unchanged; style changes and capacity resets clear the table without allocating.
- **価値または懸念（未検証）:** Reduces lookup probes from up to 2048 comparisons to a bounded hash probe sequence with at most 50% table occupancy, while adding a fixed 8 KiB per-renderer index. CPU impact and hash clustering on real multilingual projects are unmeasured.
- **次に確認すべきこと:** Exercise repeated Latin, CJK, combining-mark, and supplementary-plane text with style changes and the 2048-entry rollover; compare glyph output with the previous resolver and profile lookup probes/frame time.




## 2026-09-26 — Per-string glyph deduplication was quadratic

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`drawGlyphText`, `glyphCodePointScratch_`).
- **確認できた事実:** `drawGlyphText()` retained unique code points in first-appearance order but linearly scanned the accumulated scratch vector for every decoded code point. A string with many distinct characters therefore required quadratic duplicate-check comparisons before atlas upload.
- **対応:** Added a renderer-owned fixed 4096-slot open-address index and a fixed list of occupied slots. The first call initializes empty-slot sentinels; later calls clear only previously occupied slots, preserving first-appearance order and avoiding a full-table clear each label. Once all 4096 slots are occupied, the existing linear scan is used for additional characters so arbitrary input length remains supported.
- **価値または懸念（未検証）:** Up to 4096 unique code points, duplicate checks use bounded probing instead of rescanning all prior unique values. Adds 24 KiB fixed scratch metadata per renderer. Hash distribution, table-saturation behavior, output parity, and actual frame cost are unmeasured.
- **次に確認すべきこと:** Test empty/repeated/mixed-script/4096-plus-unique input; verify slot reset between calls and preserve atlas acquisition order; profile repeated long timeline labels and measure scratch capacity growth.




## 2026-09-26 — Render pass diagnostics are collected on every submit

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm` (`recordDebugPass`, `submitGlyphTextTransformed`) and `Artifact/include/Render/DiligentImmediateSubmitter.ixx`.
- **確認できた事実:** `recordDebugPass()` unconditionally copies each `FrameDebugPassRecord` into `m_currentFrameDebugPasses_`; `endFrameDebugCapture()` copies that vector into the last-frame vector every frame. The transformed text submit path also constructs formatted QString debug bindings before recording. App Debugger's one-second visible timer refreshes summary counters only; detailed snapshots, which query `frameDebugPasses()`, are captured only through the detailed refresh path. A request/configuration gate was not found in submitter collection.
- **価値または懸念（未検証）:** This may create per-frame vector growth/copies and QString formatting work even when the debugger is closed. Simple show/hide gating would lose the latest render-pass details when the user manually requests a snapshot, so demand-driven collection needs an explicit freshness/request contract. Aggregate cost per frame remains unmeasured.
- **次に確認すべきこと:** Trace all detailed-refresh triggers and renderer ownership to design a request that reaches the render submission lane before the desired capture frame without adding cross-thread races or stale snapshots; then measure allocations and formatting cost before changing the capture contract.




## 2026-09-26 — Diligent glyph submission repeated font fallback resolution

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm` (`submitGlyphText`, `submitGlyphTextTransformed`) and `Artifact/include/Render/DiligentImmediateSubmitter.ixx`.
- **確認できた事実:** Both submit paths shape glyphs, then for every shaped glyph create a one-codepoint `QString`, call `FontManager::makeFont()`, and convert the resolved family to UTF-8 while constructing `GlyphKey`. The submitter owns one atlas and is reused across submits.
- **対応:** Added a submitter-owned 2048-entry resolved-font/key cache shared by both paths, indexed by input `QFont`, code point, and glyph render mode. Multiple QFont settings coexist in the cache; a 4096-slot index hashes common QFont properties and verifies full QFont equality on hits. The font fingerprint is computed once per submitted text packet rather than once per glyph. Empty-slot sentinels initialize on first use; capacity rollover clears the table, and maximum entry capacity is reserved during buffer setup. The entry collection uses the existing `ArtifactArray` container rather than introducing a new `std::vector` member.
- **価値または懸念（未検証）:** Repeated text can reuse fallback resolution and family conversion across frames, including when several font settings alternate. Cache memory is entry-bounded but variable because each entry stores source/resolved QFonts and a GlyphKey family string. `FontManager::loadFontFromFile()` can mutate Qt's application font database, although no in-repository caller was found; if runtime font registration becomes active, cached fallback results may need a font-database generation in their identity. Hash clustering, QFont equality behavior, device/runtime behavior, output parity, and frame-time effect are not yet verified.
- **次に確認すべきこと:** Compare cache hits and misses for Latin, CJK fallback, color emoji, mixed render modes, QFont changes, font registration, and the 2048-entry rollover; verify fallback family and GlyphKey equality against the previous per-glyph path; measure CPU time and allocation counts on both text submit paths.




## 2026-09-26 — Diligent glyph scratch copied unused cluster payloads

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm` (`submitGlyphText`, `submitGlyphTextTransformed`) and `Artifact/include/Render/DiligentImmediateSubmitter.ixx` (`GlyphSubmission`).
- **確認できた事実:** The shaped `GlyphItem` contains several QString fields and a vector of shaped glyph indices. After atlas acquisition, the Diligent outline/fill loops read only base position, offset position, offset rotation, offset scale, offset opacity, and `GlyphRect` from the scratch entries.
- **対応:** Reduced `GlyphSubmission` to those two positions, three scalar offsets, and `GlyphRect`; both submit paths copy only these fields instead of copying the full `GlyphItem` into the reusable scratch vector. Increased its cold-path reserved capacity from 1024 to 2048 entries, matching the adjacent resolved-glyph working-set bound.
- **価値または懸念（未検証）:** Removes per-glyph copies of unused cluster metadata and any shaped-index vector storage from the submit scratch path while preserving values consumed by fill and outline passes. The doubled up-front scratch reservation is about a small fixed CPU memory cost; runs longer than 2048 glyphs may still grow during submission. Pixel parity and copy/allocation impact remain unverified.
- **次に確認すべきこと:** Compare transformed and untransformed fill/outline output for ligatures, emoji clusters, offsets, rotations, scale and opacity overrides; inspect scratch capacity behavior with long strings and profile memory/copy cost.




## 2026-09-26 — Glyph atlas rects cannot be reused across the prewarm pass

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`drawGlyphText`) and `ArtifactCore/src/Text/GlyphAtlas.cppm` (`acquire`, `clear`, `packGlyph`).
- **確認できた事実:** `drawGlyphText()` prewarms unique code points, uploads the atlas, then calls `GlyphAtlas::acquire()` again while emitting per-character packets. `GlyphAtlas::acquire()` clears the full atlas and its key map when shelf packing fails, so a rect returned during prewarm can become stale if a later unique glyph triggers a reset.
- **価値または懸念（未検証）:** Reusing prewarm rects could eliminate repeated hash lookups for common repeated characters, but without an atlas generation/epoch exposed to Artifact, it can point at overwritten pixels after a large run resets the atlas. The second acquire is a correctness guard, not a redundant operation that can simply be removed.
- **次に確認すべきこと:** If profiling shows these repeated lookups matter, add a read-only atlas generation counter at its owner boundary, then reuse per-call rects only when the generation stayed stable; any such ArtifactCore API change requires a separate explicit scope.




## 2026-09-26 — Frame debug pass publication deep-copied and returned discarded data

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm` (`beginFrameDebugCapture`, `endFrameDebugCapture`) and `Artifact/src/Render/ArtifactIRenderer.cppm` (`endFrameCostCapture`).
- **確認できた事実:** Every render frame called `endFrameDebugCapture()` and ignored its returned `std::vector`. The method deep-copied `m_currentFrameDebugPasses_` into `m_lastFrameDebugPasses_`, then returned another by-value copy; the public consumer reads the stored last-frame vector separately through `frameDebugPasses()`.
- **対応:** Changed `endFrameDebugCapture()` to return void and swap current/last vectors. `beginFrameDebugCapture()` clears the reused current vector next frame, preserving the prior publication behavior while removing both deep copies at the end boundary.
- **価値または懸念（未検証）:** Avoids per-frame copies of all diagnostic records, QString bindings, and nested arrays. The vectors alternate their retained capacities; data freshness and debugger snapshots remain source-equivalent but runtime behavior is unverified.
- **次に確認すべきこと:** Compare `frameDebugPasses()` before and after each begin/end cycle, including empty frames and frames with many text bindings; measure copy/allocation counts during rendering and ensure all call sites use the new void signature.




## 2026-09-26 — Completed debug pass records were copied into the frame list

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm` (`recordDebugPass` call sites and implementation).
- **確認できた事実:** Five submit paths construct a local `FrameDebugPassRecord`, finish filling its strings/bindings, and never use it after `recordDebugPass()`. The method accepted a const reference and copied the record into the current frame vector.
- **対応:** Changed the private recorder to take an rvalue reference, and moved each completed local record into the vector.
- **価値または懸念（未検証）:** Avoids copying implicitly shared QString/QVector members and their reference-count traffic for each recorded pass. Vector growth behavior and frame-time effect remain unmeasured.
- **次に確認すべきこと:** Confirm every recorder call moves a one-use local; compare recorded pass fields and binding contents before/after; count copies/allocations during representative draw workloads.




## 2026-09-26 — Particle rendering formatted unconditional success logs per frame

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm` (`drawParticles`) and `Artifact/src/Render/DiligentImmediateSubmitter.cppm` (`submitParticles`).
- **確認できた事実:** The particle path emitted qDebug/qInfo success records on empty submission, every active draw, every frame's view/projection matrices, and successful GPU submission. The submitter success log also requested a formatted `debugState()` string. Warnings are separately used for invalid resources and failed preparation.
- **対応:** Added the `artifact.render.particles` logging category and moved recurring informational/success records to `qCDebug`; renderer initialization uses `qCInfo`. Failure `qWarning()` paths remain unconditional.
- **価値または懸念（未検証）:** Disabled particle debug logging can skip per-frame stream formatting and the submitter's `debugState()` construction while retaining explicit opt-in diagnostics and failure warnings. Logging-category configuration and frame-time impact are unverified.
- **次に確認すべきこと:** Run with the category disabled and enabled; verify no success records are formatted/emitted when disabled, matrix and particle counts remain available in explicit diagnostics, and warning paths still report missing resources; profile long particle playback.




## 2026-09-26 — Particle queue state now stands apart from diagnostic text

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm` (`drawParticles`, `particleDebugState`) and `Artifact/src/Layer/ArtifactParticleLayer.cppm`.
- **対応:** Added `particleDrawQueued()` as an explicit ArtifactIRenderer result and changed ArtifactParticleLayer to use it. Queued draw metadata is stored as scalar fields; the existing diagnostic string is assembled only when `particleDebugState()` is requested for a snapshot. Both a new particle draw and `beginFrameCostCapture()` clear the queued flag, while device/viewport/RTV failures continue to publish their prior diagnostic strings.
- **価値または懸念（未検証）:** The render decision no longer creates, returns, or searches a QString. Successful diagnostic formatting moves from every queued draw to explicit snapshot reads. `particleDebugState()` content parity, interface/module integration, and runtime fallback behavior remain unverified because build and runtime checks are not authorized.
- **次に確認すべきこと:** Compare queued debug strings byte-for-byte for 2D/3D camera modes and ensure empty, invalid-viewport, no-RTV, device-null, and next-frame-without-draw paths all clear the public queued result; profile queued rendering with snapshots enabled and disabled.




## 2026-09-26 — RenderCommandBuffer retains packet-vector high-water capacity

- **関連:** `Artifact/include/Render/RenderCommandBuffer.ixx` (`RenderCommandBuffer::reset`, `append`, `packets_`).
- **確認できた事実:** `reset()` calls `packets_.clear()`, which destroys packet objects and their `RefCntAutoPtr` texture pins but retains the `std::vector` capacity. `DiligentImmediateSubmitter::submit2D()` consumes the packet array and calls `buf.reset()` after finishing the deferred command list and executing it on the immediate context. Ordinary frames therefore reuse prior peak packet storage, while a one-off packet-heavy frame can keep that allocation until renderer destruction.
- **価値または懸念（未検証）:** This is useful for steady-frame reuse, but retained high-water size is not currently observable or bounded. Shrinking on every reset would reintroduce recurring allocations; any trim policy belongs after packet consumption and needs frame-level allocation measurements.
- **次に確認すべきこと:** Record packet count and capacity at submit/reset across representative static, text-heavy, and particle-heavy scenes. If rare spikes materially retain memory, evaluate a hysteretic trim at the post-submit reset boundary and measure the next-frame allocation tradeoff.




## 2026-09-26 — GPU texture cache hits can use the existing composite index

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm` (`tryAcquireExistingLocked`).
- **確認できた事実:** The cache maintains `ownerCacheKeyToIds_` with format on each entry, but every `tryAcquireExistingLocked()` call previously concatenated owner, cache key, and format into a temporary QString before consulting `keyToId_`, including ordinary texture hits.
- **対応:** Added an owner/cache-key index lookup that checks the existing candidate entries for the requested format and returns on a valid hit before constructing the composite key. Misses retain the original composite-key path for pending-upload and stale-entry bookkeeping.
- **価値または懸念（未検証）:** Removes composite key allocation/formatting from the common hit path while keeping the authoritative entry map, full key, and existing invalidation bookkeeping. Variant count, cache-hit behavior, and lock-time improvement are statically reasoned but runtime-unverified.
- **次に確認すべきこと:** Compare cache hit/miss counters and handle identity for same source with multiple formats; profile QString allocations and mutex hold time for image, F32, and Vulkan frame hits.




## 2026-09-26 — F32 color-aware cache keys avoid chained QString formatting

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm` (`colorAwareImageCacheKey`).
- **確認できた事実:** F32 `acquireOrCreate()` and `findExisting()` construct a color-aware key on each call. The helper chained seven `QString::arg()` calls for descriptor fields, creating intermediate formatted strings before cache lookup.
- **対応:** Replaced the chain with one reserved QString and stack-buffer signed-decimal appends. The append path preserves negative underlying `TransferFunction` values too, including the `int` minimum. The serialized suffix remains `|color:<storage>,<order>,<primaries>,<transfer>,<alpha>,<range>,<known>`.
- **価値または懸念（未検証）:** Removes intermediate QString formatting from repeated F32 lookup and allows the suffix to append into pre-reserved capacity. Exact key parity, allocation count, and lookup time remain unmeasured.
- **次に確認すべきこと:** Compare generated keys against the prior `QString::arg()` form for every SurfaceColorDescriptor enumerator combination, invalid/negative transfer values, and descriptors differing in exactly one field; then verify cache hit/miss behavior.




## 2026-09-26 — Texture-cache expiration scan is bounded by the configured entry cap

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm` (`pruneExpiredLocked`) and `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (cache setup).
- **確認できた事実:** The app configures `setMaxEntries(256)`. Expiration performs one pass over `entries_` and keeps at most eight oldest expired candidates in fixed arrays before deleting them. The scan is therefore bounded to 256 entries in this app path; the public setter permits larger bounds for other callers.
- **価値または懸念（未検証）:** No extra heap-backed age queue or secondary ordering structure is needed for the configured cap. A caller that raises the entry limit also raises worst-case per-frame expiration scan work.
- **次に確認すべきこと:** Measure expiration scan duration at 256 entries before considering an auxiliary index; if external consumers need a strict ceiling, review a setter cap separately against their memory-budget use cases.




## 2026-09-26 — Cache-miss upload payload copy holds the cache mutex

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm` (`acquireOrCreateFromRgbaBytes`) and `Artifact/include/Render/DiligentUploadCoordinator.hpp`.
- **確認できた事実:** On a miss, the manager holds `mutex_` while constructing `QByteArray(bytes, memoryBytes)`, which copies the image payload, and while calling `uploadCoordinator_->enqueue()`. The viewport render tick is marshalled to the controller QObject thread; Render Queue uses separate cache managers per GPU worker. `EventBus::publishRaw()` invokes subscribers synchronously on the publisher thread, and the controller's `LayerChangedEvent` callback calls `invalidateLayerSurfaceCache()` → `GPUTextureCacheManager::invalidateOwner()`.
- **価値または懸念（未検証）:** The manager lock serializes publication against owner invalidation as well as protecting its maps. Moving payload preparation outside the lock could allow an invalidation between cache check and upload enqueue, republishing stale content; concurrent same-key misses could also duplicate full payload copies. A safe change needs per-owner invalidation generations or a bounded reservation protocol, plus proof of producer-thread ownership.
- **次に確認すべきこと:** Trace all manager creation/callers and characterize payload sizes and lock wait/hold time. If contention is material, design an invalidation-safe bounded reservation that prevents duplicate copies and rejects a payload whose owner changed during preparation; compare the same upload workload before and after.




## 2026-09-26 — Bounded partial recompose must schedule its deferred tiles

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`renderOneFrameImpl`, `damageTileCursor`, `markRenderDirty`).
- **確認できた事実:** The TGFX-inspired partial recompose schedules at most eight tiles per frame, consumes only the presented region, and retains the cursor while slot damage remains. The render tick clears `renderDirty_` before calling `renderOneFrameImpl()` and stops on a later clean tick. In addition, `renderOneFrameImpl()` returns early when its render key is unchanged, before the partial damage planner is reached.
- **対応:** After a successful frame, the controller schedules another tick only if any preview slot's remaining damage produces a non-empty plan inside the current visible ROI, regardless of whether that frame used a partial pass or a full redraw for its selected slot. A dedicated atomic continuation flag bypasses the render-key early return for that requested follow-up; it is consumed after slot acquisition so the draw-plan check is not repeated before and after every tick. Offscreen-only damage does not keep the ticker running; failed partials retain the existing full-redraw recovery path.
- **価値または懸念（未検証）:** Visible-area damage larger than one batch can continue across the independently retained preview slots, including when a selected slot first needs a full redraw, while offscreen damage stays pending until the viewport changes. The static control flow now allows continuation frames to reach slot acquisition and recompose planning with one bounded damage-map scan per successful frame; runtime convergence and tick pacing still require viewport verification, and build/runtime checks remain unauthorized.
- **次に確認すべきこと:** With more than eight visible dirty tiles and no interaction, verify repeated partial commits advance the cursor until the visible plan is empty, then confirm the ticker stops while offscreen damage remains pending. Pan to the deferred region and verify it is then rendered; also test ROI movement and a failure during a later batch.




## 2026-09-26 — Failed render recovery now schedules one bounded retry

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`renderOneFrameImpl`, `markRenderDirty`, `scheduleFailedFrameRetry`).
- **確認できた事実:** A failed frame invalidates retained preview slots, marks every slot for full redraw, sets `fullRedrawPending_`, and clears the render key. The render tick consumes `renderDirty_` before calling the frame routine, so these state changes alone do not schedule another attempt when the viewport is idle.
- **対応:** Failure now requests one retry by setting the dirty flag and starting the existing ticker if needed. An atomic guard prevents an ongoing device/present failure from causing an unbounded retry loop. A successful frame or a new external `markRenderDirty()` request resets the guard.
- **価値または懸念（未検証）:** A transient pass/present failure can recover without waiting for another user action, while persistent failures stop after one automatic retry and retain full-redraw damage for later activity. Retry timing and device-loss behavior remain unverified; build/runtime checks are not authorized.
- **次に確認すべきこと:** Inject one transient pass failure and confirm the following full redraw succeeds and clears damage; inject repeated failures and confirm exactly one automatic retry, then trigger a new dirty event and confirm the retry allowance resets.




## 2026-09-26 — RR4 must not retain the current DrawPacket variant directly

- **関連:** `Artifact/include/Render/RenderCommandBuffer.ixx` (`DrawPacket`, `RenderCommandBuffer::reset/append`) and `Artifact/src/Render/DiligentImmediateSubmitter.cppm`.
- **確認できた事実:** `DrawPacket` stores already-transformed matrices and a mix of borrowed `ITextureView*` pointers plus `RefCntAutoPtr` pins, QString/QFont text state, QImage billboard payloads, and particle render data. It carries no layer ID, content revision, cache-handle generation, or device generation. `reset()` clears all packets and releases pins after submission. `GPUTextureCacheHandle` does carry ID/generation, but `textureView()` returns a raw pointer after releasing the cache mutex. The composition loop has per-layer ROI/opacity checks around `drawLayerForCompositionView()`, whose implementation may emit multiple primitive packets.
- **価値または懸念（未検証）:** Retaining these variants as-is would freeze frame-specific transforms and extend resource pins beyond the established submit/reset lifetime; cache eviction or device reset would have no packet-level stale check. Re-resolving a raw texture pointer and pinning it later also leaves a lifetime race with concurrent cache invalidation. RR4 should first establish RR0 packet-build measurements and a layer-scoped immutable-content/frame-state boundary, then test a narrow static Image/Simple Shape candidate with atomic generation validation and pin acquisition. Text, Particle, Billboard, and temporary/masked sources need separate lifetime contracts.
- **次に確認すべきこと:** Measure static-scene packet reconstruction cost; trace all Composition View layer draw call sites and ownership of their emitted packet ranges; design a generation-checked pinned texture acquisition API that does not expose Diligent backend types through the public module.




## 2026-09-26 — 対抗案 UI モックはコード描画（Pillow）にすると既存画像を壊さずに回帰できる

- **関連:** `docs/design/timeline/generate_counter_timeline_mockups.py`、`docs/design/aidaw-widgets/generate_mockups.py`、`docs/design/timeline/README.md`。
- **確認できた事実:** 既存の UI モックは 1832×858 等の固定サイズで、承認済み PNG は約 1.1MB のフルカラー画像である一方、本スクリプトの出力は 50〜65KB。`docs/design/composition-viewport/` には既に「対抗案」同士のペア（radial / quadrant）があり、比較用モックを別ファイルで持つ運用が定着している。生成スクリプトはネットワークや外部 API を使わず、Windows のローカルフォント（segoeui / seguisb）に依存する。
- **対応:** 共通関数（chrome / ruler / cache bar / work area / playhead / transport / clip bar）を共有し、3案でペイン構成パラメータだけを差し切った。同じサイズ・同じ配色・同じレイヤー定義にそろえ、差分がペイン構成だけになるようにした。
- **価値または懸念（未検証）:** 文言・色・行高をスクリプト側で diff できるため、承認済み画像を一切触らずに反復改善できる。懸念はフォント環境依存（Windows の Segoe UI 前提）と、`docs/design` 配下に Python スクリプトが常駐することのノイズ。実機 UI との差分確認・DPI・ビルド検証は一切未実施。
- **次に確認すべきこと:** 対抗案のいずれかを採用する場合、先に「生成スクリプトを正とする」か「採用画像を正とする」かを決める。画像を正とするなら生成画像は履歴扱いに降格させる。




## 2026-09-26 — Viewer clipping warnings can share the exposure display pass

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, and `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`.
- **確認できた事実:** The viewer exposure pass writes to a scratch surface before presentation, while `lastPresentedReadbackSRV_` continues to reference the unmodified composite. The pass already owns a persistent compute executor and fixed parameter buffer.
- **対応:** Added optional under/over false-color checks to the same pass, preserving the source/readback surface and avoiding a second GPU resource/pipeline. The warnings can run with exposure disabled. Since the incoming accumulator is premultiplied, the pass now unpremultiplies nonzero-alpha RGB for linear transforms and threshold tests, then premultiplies the result by the original alpha; fully transparent pixels remain zero and warning-free. Review found the first-use path created the PSO and parameter buffer during a frame; creation now happens once in `ArtifactIRenderer::initialize()`. The controller skips the full-surface dispatch when exposure remains at identity and warnings are off; otherwise the presentation path maps the fixed 32-byte parameter block and dispatches.
- **価値または懸念（未検証）:** A single display transform keeps warning colors out of Render Queue and pixel sampling while avoiding per-frame resource creation and the default identity dispatch. Threshold interpretation is linear luminance for under and per-channel linear RGB for over. The alpha-edge behavior is code-reviewed but still unverified in runtime; visual usability, initialization cost, and D3D12/Vulkan behavior also remain unverified.
- **次に確認すべきこと:** After an authorized runtime check on the existing executable or a later build, inspect toggle/defaults and thresholds in the View menu; verify gain/exposure off plus warnings on, fully transparent pixels, semi-transparent edges, below/above thresholds, unchanged sampler/readback/output, renderer reinitialization, and both Diligent backends.




## 2026-09-26 — Scope signal excursion and delivery-gamut checks need separate contracts

- **関連:** `Artifact/include/Render/ArtifactHDRMonitor.ixx`、`Artifact/src/Render/ArtifactHDRMonitor.cppm`、将来の CIE chromaticity scope。
- **確認できた事実:** `ScopeAnalysisDescriptor` は入力 `primaries` と納品先 `targetGamut` を別々に保持し、解析時に scene-linear RGB を既存の XYZ／Bradford 経路で target RGB へ変換してから 0..1 包含判定と legal-range 判定を行うようになった。これにより Rec.2020 素材を Rec.709 納品域に照合できる。単なる入力信号の 0..1 逸脱は clipping 集計として別に残る。
- **価値または懸念（未検証）:** delivery-gamut 判定と signal clipping の意味が分離され、UIでも Gamut と Low／High を別々に表示できる。一方、これは target RGB cube の包含判定であり、CIE xy 図そのものや perceptual gamut mapping の結果ではない。
- **次に確認すべきこと:** CIE scope実装時に target gamut三角形、輝度0近傍の色度不定、境界epsilon、実際の出力変換／gamut mapper後の判定を追加し、RGB cube判定との数値整合を比較する。




## 2026-09-26 — GPU scope buffers now share a clear/barrier pattern

- **関連:** `ArtifactCore/src/Graphics/Compute/ScopeComputer.cppm`、`ArtifactCore/src/Graphics/Compute/Histogram.cppm`。
- **確認できた事実:** Scope と Histogram はどちらも `RWStructuredBuffer<uint>` をフレームごとにゼロ初期化し、同じ UAV へ集計dispatchを続ける。両実装に固定CB、256-thread clear shader、UAV barrierという同じ処理が必要になった。
- **対応:** 現段階では各computer内に閉じたclear pipelineを持たせ、公開APIやモジュール依存を広げずに正しい同期を優先した。
- **価値または懸念（未検証）:** 重複は小さいが、今後GPU解析器が増えるとclear shaderとバッファ検証が分散する。早期に共通化すると逆に低レベル依存を広げるため、3個目の利用箇所と実測されたPSO初期化コストが揃うまでは保留が妥当。
- **次に確認すべきこと:** 新しいGPU解析器を追加する時点で、内部限定のbounded UAV-clear utilityへ切り出すか、バックエンド標準clear APIの利用可否をD3D12/Vulkan両方で確認する。




## 2026-09-25 — Solid2D multi-stop fill needs a bounded GPU packet contract

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`, `Artifact/src/Render/PrimitiveRenderer2D.cppm`, `Artifact/src/Render/DiligentImmediateSubmitter.cppm`, `Artifact/include/Render/ArtifactIRenderer.ixx`。
- **確認できた事実:** Solid2DのGPU経路は `drawGradientRectTransformed` に開始色・終了色・形状パラメーターのみを渡し、PrimitiveRenderer2DからGradientRect packetへ送る。Shapeのmulti-stop modelをSolid2Dへそのまま公開しても、このrenderer経路では描画されない。SolidImageの共有gradient utilityはQImage生成を使うため、GPU描画の共通実装にはならない。
- **仮説（未検証）:** Solid2D／Shape共通Fillを完成するには、描画呼出しごとにvectorを確保せず、stop上限32の固定容量データか事前確保されたGPU bufferをGradientRect packetへ渡し、Diligent shaderでstop rampを評価する契約が必要。固定配列の定数buffer負荷とper-draw upload量は測定前で、現在のTriangle paint pathとの品質・速度比較も未確認。
- **価値または懸念:** UI／serializationだけを共通化して描画が2色のまま残る見かけの対応を防げる。Diligentとrenderer packetの変更は低レベル影響が大きいため、GPU shader・resource lifetime・hot-path allocationを調査してから、最小限のGPU実装とする。
- **次に確認すること:** `GradientRectPkt`／parameter bufferの容量・更新頻度・renderer queue ownershipを追跡し、固定32-stopのupload案とstop texture案を比較。ユーザーの許可がない間はビルド・GPU runtime計測をしない。



## 2026-09-24 — Collaboration review operations need ordered acknowledgement

- **関連:** `ArtifactCore/src/Collaborate/CollaborationSessionAdapter.cppm`, `CollaborationSession.cppm`, `CollabOperations.cppm`, `CollabReview.cppm`。
- **確認できた事実:** ローカル operation は送信時点で session log に version `-1` の pending として記録され、サーバー echo 時に同じ dedupe key へ version が付与される。レビューコメントを送信前に UI モデルへ適用すると、他クライアントの operation より先にローカル状態だけが進む可能性がある。
- **対応:** レビュー操作は echo で session が確定した後に同じ apply 経路で反映し、operation adapter の適用 callback をリモート operation とローカル echo の両方で呼ぶようにした。編集・削除は author identity を schema と現在の review model の双方で検証し、削除は返信表示と operation replay のため tombstone として保持する。
- **価値または懸念:** サーバー version 順でレビュー状態を反映できる。operation は JSONL に保存され再参加・再起動後に replay される。ファイルは暗号化されず保持期限はない。コメントと返信本文を含むため、保存先へのアクセス制御と容量管理が必要。
- **追記 (2026-09-24):** WebSocket client の reconnect queue は上限時に先頭を捨てていたため、bounded rejection に変更。durable operation は非接続時に queue へ積まず、送信拒否時は session の pending operation log と dedupe key を戻す。lock request/release の UI pending 状態も拒否時に戻し、presence payload は bounded exponential backoff で再送する。
- **追記 (2026-09-24):** サーバーは空 room を破棄するため memory-only history は最後の離脱で消失することを確認。project ID を SHA-256 でファイル名化した JSONL append log、起動時の履歴 replay、壊れた末尾行の回復を追加した。`clientId + opSeq` を冪等キーにし、同一内容の再送は元の version で応答、異なる内容の再利用は拒否する。さらに WebSocket control ping/pong で半開き接続を検出し、切断 cleanup による lock/presence の解放へ繋げた。既存クライアントの application ping も server が pong 応答し、通常通信として処理する。
- **追記 (2026-09-24):** 長期運用で JSONL と参加時の全件 replay が無制限に拡大する問題に対し、プロジェクト単位で既定 16 MiB／100,000 operation の上限を設けた。設定値は環境変数で変更でき、上限到達時は古い履歴を自動削除せず新規 operation を拒否する。snapshot／圧縮がない状態で履歴を切り詰めると replay 結果が変わるため。既存の上限超過ファイルは読み込めるが追記は拒否する。
- **追記 (2026-09-24):** operation の JSONL 追記後に `fsync` を行い、成功前に ACK／broadcast しないようにした。部分書込みは元のファイルサイズへ truncate して再同期する。rollback 自体に失敗した場合は room を fail-closed にし、再起動・履歴調査まで追記を拒否する。これは OS の file sync 境界までであり、directory entry やストレージの電源断耐性を保証しない。
- **追記 (2026-09-24):** WebSocket の受信 frame 2 MiB、operation 1 MiB、presence 64 KiB と join identity／layer ID の byte 長を制限し、入力経路でメモリ・永続化を無制限に消費しにくくした。これは DoS 耐性の一部にすぎず、接続数制限、認証、権限、rate limit は依然ない。
- **追記 (2026-09-24):** 起動時の JSONL 回復が不正行の位置を問わず、その後ろのレコードを黙って削除し得ることを確認。最終レコードのみ回復し、途中破損は元ファイルを保持して join を拒否するようにした。これで履歴の自動回復可能性とデータ保持を区別できる。
- **追記 (2026-09-24):** project ID を自由に作れる prototype server では room 数・同時接続によるメモリ圧迫もあるため、全体 256／room 64 の上限を join 時に履歴読込前に検査する。client identity は認証済みではないため、これは資源上限であって権限境界ではない。
- **追記 (2026-09-24):** manual layer reservation を編集拒否へ接続するには初期 lock roster 完了境界が必要。`room_ready` を初期 history／participants／locks の後に送り、UndoManager へ layer target を公開した command は push／undo／redo の前に guard する。対象 ID が未実装の command は guard を通らないため、すべての mutation 経路をカバーしたと扱わない。
- **追記 (2026-09-24):** 他者の lock がある場合だけ拒否すると、未ロック layer を複数人が同時に編集でき、予約が排他制御にならない。追跡対象 Undo command は collaboration session 中に自分の lock 保持も要求し、server の既知 property／transform/remove operation も送信者が lock owner の場合だけ受理する。未追跡 command と operation type は依然 enforcement 外。
- **追記 (2026-09-24):** layer ID を持つ Undo command をさらに棚卸しし、mask/matte、text/source、components、modulation、automation、variants、layer effects 等を lock guard 対象へ追加した。multi-layer alignment と composition resolution remap も全対象 ID を列挙して検査する。Add/Remove layer は参照が変わる既存 layer を含め、Add の新規 ID は生成前の push では lock を要求せず、後の undo では既存 ID として検査する。
- **追記 (2026-09-24):** effect Undo command は effect ID だけを持ち owner layer を直接保持しない。project item tree 上の全 Composition の layer effect stack から pointer identity で owner を解決し、複数 owner がある場合は全 layer lock を要求する。未所属／Composition 直結など owner を解決できない場合は mutation scope unknown として collaboration guard が拒否する。Composition-wide effect lock UI は未実装。
- **追記 (2026-09-24):** 通常の `property.set`／`layer.transform`／`layer.add`／`layer.remove` は Session の dedupe/version history と server broadcast に届くが、MainWindow の operation-applied callback は review operation にだけ接続している。送信元の編集 UI と remote project mutation の経路は未接続。次段階では「編集 command → 安定 layer/property identity を持つ semantic operation → server echo/version → remote apply」と undo の補償操作・重複 echo の扱いを同じ仕様にしてから接続する。変換値を直接 setter に流すだけではローカル undo 履歴と競合順序がずれるため、適用モデルの具体案は未検証。
- **追記 (2026-09-24):** review operation の編集は operation log に旧本文が残るが、review model は最新本文だけを表示していた。各クライアントが同じ version 順で replay して旧本文・editor ID・時刻を revision として保持し、後日再参加しても表示名が残るよう編集者名だけを operation payload に追加した。実装では deletion tombstone の履歴本文を UI に露出させない。
- **追記 (2026-09-24):** revision をコメントごとに全件保持すると `CollabComment` の値コピー（一覧表示・lookup）でも履歴配列を深く複製するため、review model は直近 50 版に制限してコピーコストを bounded にした。元 operation は JSONL に残るため、model の保持数と永続履歴 retention は別責務である。
- **追記 (2026-09-24):** 静的な `setLayerPropertyValue()` 使用箇所の調査で、Inspector、Property Widget、Text Editor、Content/Text Gizmo などに UI 直接 mutation が複数残っていることを確認した。検索結果には command の undo/rollback と初期化経路も混ざるため、直接編集経路の総数や共同編集中の到達可能性は未検証。UndoManager の command preflight だけでは、これらの直接 setter 経路へ lock と operation sync を強制できない。次はユーザー操作の入口ごとに対象 layer、編集開始／commit／cancel の境界、undo 所有者を分類し、専用 command 化か共通 mutation gateway 化の順序を決める。
- **次に確認すること:** 実サーバー再起動後の履歴 replay、同時編集競合時の利用者向け状態、運用者がバックアップ・整理する保持手順、snapshot 導入後の履歴圧縮を確認する。ビルド・実機動作は未確認。



## 2026-09-24 — EffectContract の C++ module 依存を所有元へ限定

- **関連:** `Artifact/CMakeLists.txt` の `ArtifactEffectContract`、`ArtifactRender`、`ArtifactRenderSupport`、`ArtifactRenderSupportContracts` のリンク設定。
- **確認できた事実:** EffectContract の `.ddi` が import するモジュールは ArtifactCore、ArtifactCoreAudio、RenderSupportContracts の所有範囲に収まる。`ArtifactRender` と `ArtifactRenderSupport` を同時にリンクしていた状態では、同一 ArtifactCore モジュールの BMI が複数の `@synth_*` パスとして解決され、CMake 4.4.3 が dyndep 生成時に location disagreement を報告した。両ターゲットを外し、直接 import している所有ターゲットに絞った後は、同じ dyndep が成功し、BMI が各所有ターゲットの `.ifc` に一意に解決された。
- **仮説:** CMake の synthetic partition を介して広い公開リンク閉包を重ねると、モジュール所有ターゲットの BMI 解決が曖昧になる可能性がある。今回の修正では再現エラーが消えたが、他ターゲットでも同じ制約になるかは未検証。
- **価値または懸念:** モジュール利用側は、実際に import するモジュールの所有ターゲットをリンクすることで、不要な依存伝播と BMI 配置の競合を抑えられる。通常の静的リンクシンボルが Render 実装ライブラリに依存するかは別途確認が必要。
- **次に確認すること:** `ArtifactEffectContract` の通常コンパイルとリンクを進め、Render / RenderSupport 実装シンボルの未解決がないことを確認する。



## 2026-09-24 — MSVC C3474 の単一 IFC 再試行

- **関連:** `ArtifactCore/include/Script/Expression/ExpressionEvaluator.ixx` と `out/build/x64-Debug-cmake443` の Ninja/MSVC 出力。
- **確認できた事実:** C3474 で `Script.Expression.Evaluator.ifc` を開けなかった後、IFC と `.obj` は前回時刻のファイルとして残っていた。排他的な読み書きテストではロックや書き込み拒否がなく、`CXX.dd` 上の IFC provider も1件だった。対象オブジェクトを `ninja -j 1` で再実行すると、依存7件の更新後にコンパイルが成功した。
- **未検証の仮説:** 失敗時だけ発生した一時的なファイルシステム／コンパイラ出力障害の可能性があるが、元の失敗時点のハンドル状態は取得できず、原因は確定できない。
- **価値または懸念:** C3474 が単発で、同じ出力を持つ provider が1件なら、クリーンビルドより先に対象を直列で再試行することで回復し、原因の切り分けにもなる。再発時は失敗時のプロセスと出力ファイル状態を採取する必要がある。
- **次に確認すること:** 同種の C3474 が再発した場合、失敗時点の Ninja/CL プロセス、provider 数、出力ファイルのアクセス状態を比較する。



## 2026-09-24 — Plugin editor window ownership and thread boundary

- **関連:** `ArtifactCore/include/CLAP/CLAPHost.ixx`, `ArtifactCore/src/CLAP/CLAPHost.cppm`, `ArtifactCore/include/VST3/VST3Interfaces.ixx`, `Artifact/src/VST/VSTHost.cppm`。
- **確認できた事実:** VST2 の editor path は VST3 を除外し、`effEditOpen` / `effEditClose` opcode 値も VST2 ABI と不一致だった。Audio Mixer は `openEditor(nullptr)` を渡す一方、VSTHost は有効な native handle を要求する。Mixer の挿入メニューに CLAP はなかった。VST3 の `IEditController::createView()` 宣言はあるが、`IPlugView` / `IPlugFrame` の契約は未実装だった。加えて現 CLAP 手書き ABI は official `clap_host` の version/name/vendor/url 領域、descriptor/process layouts と一致せず、`clap_plugin.reset` slot も欠落している。さらに公式 entry は plugin factory を返す `get_factory` 構成だが、現宣言は entry 自体に plugin count/create 関数を置いている。CLAP GUI の lifecycle は main-thread 操作を要求し、host callback は thread-safe な非同期通知を含む。ビルドでは CLAP host と VST3 loader はコンパイルしたが、Audio Mixer/VST 実装は別の既存 dirty collaboration module のコンパイルエラーで未到達。
- **対応:** VST3 の既存 `VST3EffectHost` を `VSTEffect` から使い、UI thread 上で native child window に editor view を attach/detach する実装を追加。CLAP entry/factory、host、descriptor、plugin/process、audio buffer、parameter event の ABI 宣言を公式仕様に合わせ、bounded parameter queue / audio scratch と mono/stereo 単一 bus 制約を追加。各 format の mono/stereo 変換は preallocated scratch 上で行い、host segment のチャンネル数を保つ。Audio Mixer から CLAP を挿入し、CLAP GUI を浮動 host window に attach する経路を追加。
- **価値または懸念:** plugin editor は audio effect 本体と異なる thread / native-window lifetime を持つ。window close で effect を unload せず、plugin/library の破棄前に view を閉じる所有権が必要。現 CLAP host GUI callback は plugin からの resize/show/hide 要求を拒否し、multi-bus / sidechain / floating-only GUI は未対応。実プラグインでの ABI と native handle は未検証。
- **次に確認すること:** CLAP descriptor selection、非同期 GUI request のbounded UI-thread dispatch、VST/CLAP の unload 順と repeated open/close のソース監査を続ける。VSTHost / VSTEffect / AudioMixerWidget は個別コンパイルでき、serialized Ninjaもこれらを通過した。フルアプリビルドは未変更の `ArtifactShapeLayer.cppm:4713` にある重複 `this` capture の C3483 で停止した。これは本作業外の既存ソースなので触らず、リンク/runtime の確認はこのエラー解消後に続ける。並列 Ninja では別途 assertion が出たため、直列を維持する。



## 2026-09-24 — Collaboration operation schema の二重定義

- **関連:** `ArtifactCore/src/Collaborate/CollabSchema.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`、`ArtifactCore/src/Collaborate/CollaborationSessionAdapter.cppm`、`Artifact/src/AppMain.cppm`。
- **確認できた事実:** `CollabSchema.cppm` は `Collaborate.Schema` として transform.move / transform.rotate / transform.scale などを定義するが、現在の ArtifactCore / Artifact から import されていない。実際に adapter と MainWindow が import する `Collaborate.Operations` は property.set / layer.transform など別名の operation 群とその validator を持つ。MainWindow の operation-applied callback は review operation のみを project state に適用する。
- **価値または懸念（未検証）:** 未使用 schema を整理せずに mutation sync を足すと、旧 schema と現行 wire schema のどちらを producer / consumer が使うか不明確になり、移行時に互換性のない operation が混在する可能性がある。
- **次に確認すること:** `Collaborate.Schema` の外部参照と履歴データ利用を再確認し、現行 `Collaborate.Operations` を正規 wire contract とするか決めたうえで、Undo 履歴との競合方針を含めた property / transform remote-apply 経路を設計する。



## 2026-09-24 — Live operation sync の前提になる project snapshot

- **関連:** `ArtifactCore/src/Collaborate/CollaborationSessionAdapter.cppm`、`Artifact/src/AppMain.cppm`、`Artifact/src/Project/ArtifactProjectManager.cppm`、`tools/collaboration-server/server.js`。
- **確認できた事実:** server は project ID ごとに operation JSONL を replay するが、join 時に client の project state を交換・照合する protocol はない。ArtifactProjectManager には local project の保存 API がある一方、CollaborationSessionAdapter は review operation のみを project state に適用する。したがって operation history の順序だけでは、late joiner の baseline state が同一であることを証明できない。
- **価値または懸念:** Undo 方針だけを選んで `property.set` / `layer.transform` を自動適用すると、異なる project copy や既に反映済みの snapshot に対して operation を適用する可能性がある。静的調査で見つかった範囲では、これは競合処理より前に解くべき document-sync の欠落。
- **次に確認すること:** read-only project snapshot の形式、サイズ／chunking、server の authoritative baseline と snapshot version、asset path の扱い、late join/reconnect の replay 開始位置を定義する。基準が固まるまでは remote project mutation を自動適用しない。


## 2026-09-24 — Property Widget preview は Undo command 前に直接 mutation する

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`, `Artifact/src/Undo/UndoManager.cppm`。
- **確認できた事実:** 共通 Property Widget 行の preview／commit は `AbstractProperty::setValue()` を呼び、後から commit callback へ渡す。UndoManager の command guard だけでは preview 中の直接 mutation を止められない。
- **対応:** UndoManager の layer guard を直接照会する API を追加し、共通 Property Widget 行の preview と commit で値を変える前に適用した。
- **価値または懸念:** 共同編集の layer reservation が値のプレビュー段階から効き、全選択 layer を一括して確認する。guard が preview 中に拒否へ変わった場合と cancel の双方で全対象の編集開始 value/keyframe/animatable 状態を戻す。Inspector、Timeline、専用 editor、ツールなど他の直接 mutation 経路には未適用で、対象範囲を全 UI に一般化したとは言えない。ロック lease 失効を含む実機動作は未検証。
- **次に確認:** `setLayerPropertyValue()` と keyframe/property API を呼ぶ UI 入口を、ユーザー操作・プレビュー・初期化／rollback に分類し、guard 適用と取消時の復元を検討する。


## 2026-09-24 — Undo command の未指定 layer scope は collaboration guard を通過する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`, `Artifact/src/Widgets/Render/ArtifactCompositionTextPuppetUndoCommands.cppm`。
- **確認できた事実:** UndoCommand の既定 `collaborationTargetScopeResolved()` は true で、UndoManager は layer ID が空かつ scope resolved の場合に layer guard を呼ばず許可する。Text/Puppet custom command は owner layer identity を持っていたが guard API に公開していなかった。
- **対応:** TextContent、PuppetPin、Deformation2D state、Deformer keyframe command が layer ID を公開し、identity 解決不能時は scope unresolved を返す。
- **価値または懸念:** これらの未対応 operation は session 中に lock gate と dispatch fail-closed を通り、owner lock のない編集として漏れない。remote 同期は未実装のため command は push preflight で拒否される。
- **次に確認:** Undo command 全種について mutation scope の ID が guard に届くか棚卸しし、直接 setter は mutation 前 guard と rollback を分けて追加する。


## 2026-09-24 — Composition-wide Undo command は scope 未解決で閉じる

- **関連:** `Artifact/src/Widgets/ArtifactCompositionAudioMixerPresentation.cppm`, `UndoManager` collaboration target contract。
- **確認できた事実:** `AudioMixerSnapshotUndoCommand` は Composition の AudioMixer 全体を serialize/deserialize する一方、layer ID を保持せず、UndoCommand の既定 collaboration scope は resolved 扱いだった。
- **対応:** snapshot command の collaboration scope を unresolved として明示し、共同 session の mutation guard で fail-closed にした。呼び出し元 helper には push 拒否時に変更前 snapshot を再適用する rollback がある。
- **価値または懸念:** Composition-wide mixer mutation が layer lock のない状態で共同編集 guard をすり抜けない。mixer state の remote operation と composition-level lock model は未実装。
- **次に確認:** 他の project/composition-wide Undo command を同じ scope contract で棚卸しし、layer lock で表せない操作の lock model を検討する。


## 2026-09-24 — Inspector stack helper は command push 前に model を変える

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` の component descriptor、clone effector stack、cloner transform stack helper。
- **確認できた事実:** これらは snapshot を取得した後に setter／stack mutation を実行し、その後 Undo command を push する。UndoManager guard だけでは lock 未取得時の直接 mutation を先に防げない。
- **対応:** 各 helper の mutation 前に UndoManager layer guard を呼び、lock 未取得なら早期 return するようにした。
- **価値または懸念:** 対象 Inspector edit は lock がない状態で model を一時変更しない。共同 operation dispatcher はこれらの command type に未対応であり、lock があっても preflight 拒否後の既存 rollback が必要。
- **次に確認:** Inspector の他の専用 action、Timeline、各 tool について同じ「setter が command push より前か」を追跡し、mutation 前 guard と rollback を個別に接続する。



## 2026-09-24 — Legacy layer.transform wire shape is not a safe transform-sync boundary

- **関連:** `ArtifactCore/src/Collaborate/CollabOperations.cppm`, `tools/collaboration-server/server.js`, `Artifact/src/AppMain.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm`, `Artifact/src/Widgets/Render/TransformGizmo.cppm`.
- **確認できた事実:** `layer.transform` remains a server-known and lock-protected operation with five finite fields (position X/Y, rotation degrees, scale X/Y). Search found no producer call to `makeLayerTransformOperation`; the MainWindow receive callback reports `layer.transform` as unhandled. Transform gizmo Undo commands retain richer frame-aware snapshots, including keyed channel state, anchor values, and (for text) box dimensions.
- **価値または懸念:** Reusing this legacy payload for current transform commands would omit animated/keyed and 3D state and has no expected-value snapshot for conflict detection. Applying it directly risks overwriting divergent collaborator state.
- **次に確認すること:** Before enabling transform synchronization, define a versioned frame-aware snapshot/CAS contract and handle single- and multi-layer transforms atomically, including local Undo compensation and text box state; then retire or migrate the unused five-field operation.



## 2026-09-24 — 構造編集同期は layer.add の単体化から始める

- **関連:** `Artifact/src/Undo/UndoManager.cppm`（`AddLayerCommand` / `RemoveLayerCommand`）、`Artifact/src/AppMain.cppm`（remote operation routing / `ArtifactLayerFactory::createFromJson`）、`Artifact/src/Layer/ArtifactLayerFactory.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`tools/collaboration-server/server.js`。
- **確認できた事実（静的読み取り）:** サーバーは `layer.add` の `layerType` と `layerJson` を検証するが、クライアントの remote handler は `layer.add` / `layer.remove` / legacy `layer.transform` を「適用できない」と扱う。`AddLayerCommand` は layer本体の JSON を durable undo serialization に含まず、親参照・matte参照の影響を受ける dependent layer を検出している。remote reconstruction factory と composition insertion API は既存で利用可能。
- **実装した設計（runtime 未検証）:** `AddLayerCommand` の bounded snapshot operation を追加し、composition ID／index／左右 anchor／layer JSON を送る。既存 composition の追加には anchor layer reservation を要求し、空 composition で同時追加が発生した場合は layer ID 順で整列する。remove は path-free layer snapshot を比較し、dependent parent／matte reference があれば fail-closed とした。Core と server は AddLayer snapshot に加え、source path 系 property、batch、component／stack snapshot の path field も履歴へ保存しない。
- **価値または懸念:** layer構造同期を安全に広げるには、composition identity、payload上限、挿入順序、参照依存、lock対象の一貫した契約が必要。見かけだけの layer.add/remove broadcast は別クライアントで異なる構造を作る恐れがある。
- **次に確認すること:** 複数 client で同時追加／undo／redo の最終順序を実機確認する。asset identity／blob portability と path string 以外の private metadata を監査し、remote operation を shared Undo history に統合する契約を設計する。



## 2026-09-24 — Layer reorder collaboration uses index CAS

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/AppMain.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`、`tools/collaboration-server/server.js`。
- **確認できた事実:** `MoveLayerIndexCommand` は通常の layer index 移動を undo/redo できるが、collaboration encoder と remote handler がなく、同期中は未対応 command として fail-closed だった。
- **対応:** `layer.reorder` を追加し、composition ID、expected index、target index を送る。server は対象 layer reservation を要求し、remote apply は expected index を照合してから Composition の既存 move API を呼ぶ。
- **価値または懸念:** per-layer reservation で操作対象を限定し、index CAS で古い reorder の適用を拒否できる。一方、別 layer の追加・削除が index をずらす場合や同時 reorder の解決方針は未検証。
- **次に確認:** 複数 client で reorder と add/remove が交錯する場合の convergence、layer parent／matte／clone 参照や描画順への影響を実機確認する。



## 2026-09-24 — Opacity undo now compares shared state

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/AppMain.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`、`tools/collaboration-server/server.js`。
- **確認できた事実:** `ChangeLayerOpacityCommand` は layer ID と before／after float を保持し、共同編集 encoder はまだなかった。Abstract layer opacity setter は値を 0〜1 に clamp する。
- **対応:** `layer.opacity` expected/value operation、server lock/schema 検証、UndoManager remote CAS apply を追加した。local Undo／Redo も現在値が command の期待値と異なる場合は拒否する。
- **価値または懸念:** opacity を同期でき、古い undo が後から届いた変更を黙って上書きしない。共通 Property Widget row の preview／commit は UndoManager に全対象 layer ID を照会するため、opacity preview も同じ guard を通る。調査時点で preview 値の開始 opacity は単一値しか保持せず、multi-selection の remote expected value が誤る可能性を確認した。layer ごとの開始値 map を追加し、cancel／lock 失効時も個別値を戻す。
- **次に確認:** opacity preview／cancel と lock 失効の実機挙動、multi-client の競合を確認する。



## 2026-09-24 — Parent changes can reuse the layer setter as the hierarchy validator

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、collaboration operation adapter/server。
- **確認できた事実:** `ChangeLayerParentCommand` は child layer の parent ID を変更する。ProjectService は parent が同一 composition 内にあり cycle がないことを確認し、`ArtifactAbstractLayer::setParentById` も self-parent、存在しない parent、循環を拒否する。
- **対応:** `layer.parent` CAS と remote apply を追加し、同じ setter の拒否結果を照合する。Undo／Redo も expected parent ID が一致するときだけ適用する。
- **価値または懸念:** parent ID を共有しながら既存の階層制約を維持できる。child と旧／新 parent IDs を command lock scope に公開し、server も同じ parent IDs の予約を要求する。これにより parent remove と parent change の交錯を reservation で直列化するが、runtime での確認は未実施。
- **次に確認:** multi-client の親変更／削除交錯、階層描画と transform propagation を実機で確認する。



## 2026-09-24 — Text animator stack fits the bounded layer stack protocol

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`、`tools/collaboration-server/server.js`。
- **確認できた事実:** `SetTextAnimatorStackCommand` stores before/after JSON snapshots, and `ArtifactTextLayer` exposes snapshot restore and readback APIs. The collaboration stack protocol already bounds each snapshot and applies expected-value checks.
- **対応:** Added the `textAnimators` `layer.stack` kind, command encoder, remote apply, and local undo/redo precondition. Failed restore returns to the compensation snapshot.
- **2026-09-26 追記:** AE風の個別プロパティ追加は Animator 数だけでなく、名前、Range Selector、対象プロパティ値、override flagを同時に増やす構造変更になる。Inspector／Timelineの新しい `Animate` 導線に加え、InspectorのDefault／Preset追加・末尾削除・既存Preset列のstack置換／Clear、TimelineのPreset置換／Clear、Composition Viewport右クリックのDefault追加も、`text.animatorCount` や `text.animatorPreset` の一時値または直接変更ではなく、変更前後の完全snapshotを `SetTextAnimatorStackCommand` へ渡す形に統一した。これにより削除・置換のUndoもAnimator内容とキーフレームを復元し、全追加導線がcollaboration mutation guardを共有する。Inspectorの複数対象は1 Macro Undoで扱う。さらに `SetTextAnimatorStackCommand` の Undo/Redo が `text.animators` の既存プロパティ変更通知を発行するようにし、snapshot復元後のlayer dirty・再描画・Timeline更新もコマンド責務として閉じた。
- **価値または懸念:** Text animator stack structure can travel without introducing a new operation family. Expression strings remain part of animator snapshot data and are bounded by the same JSON cap. 単一レイヤーの `SetTextAnimatorStackCommand` は `layer.stack` として同期できるが、複数選択時の `MacroUndoCommand` は `AppMain.cppm` の共同編集batchが property value / keyframe / expression 子だけを許可しており、stack子コマンドはpreflightで拒否される。ローカル編集とロック安全性は維持されるものの、複数レイヤー同時Animator追加の共同編集同期には、複数CASを原子的に扱う専用batch設計が別途必要。
- **次に確認:** Runtime creation/reorder/removal, playback evaluation, and undo/redo parity across two clients.



## 2026-09-24 — Animation layer stack command exposes a complete collaboration boundary

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`、`tools/collaboration-server/server.js`。
- **確認できた事実:** `AnimationLayerStackSnapshotCommand` already exposes layer identity and before/after object snapshots; its UI helper applies the snapshot before pushing the Undo command. Its collaboration target method was declared without a definition.
- **対応:** Implemented target IDs, bounded `layer.animationStack` expected/value schema, encoder, remote apply, and local undo/redo CAS. Initial redo accepts an already-applied after snapshot to preserve the caller's existing sequence.
- **価値または懸念:** Animation layer structure can sync while enforcing the same layer lock and snapshot size limits. Other directly edited animation fields may still use different command paths.
- **次に確認:** Animation layer create/reorder/delete, undo compensation, and playback evaluation parity across clients.



## 2026-09-24 — Audio de-click ranges use exact integer collaboration payloads

- **関連:** Artifact/src/Undo/UndoManager.cppm、ArtifactCore/src/Collaborate/CollabOperations.cppm、	ools/collaboration-server/server.js。
- **確認できた事実:** de-click range endpoints are qint64 sample indices and the audio layer normalizes range ordering/overlap. JSON numbers cannot represent all qint64 values exactly.
- **対応:** Added bounded expected/value synchronization using canonical decimal strings, validation for normalized non-touching ranges, layer lock enforcement, and CAS for local undo/redo and remote apply.
- **価値または懸念:** Sample indices retain exact values through server persistence and replay. Runtime parity is unverified.
- **次に確認:** Exercise range add/clear, undo/redo, and conflicting edits across two clients.



## 2026-09-24 — Deformation 2D snapshots require PuppetTool cache restoration

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionTextPuppetUndoCommands.cppm`、`Artifact/src/Tool/ArtifactPuppetTool.cppm`、`Artifact/src/AppMain.cppm`。
- **確認できた事実:** Deformation 2D edit paths mutate layer JSON state before pushing `Deformation2DStateUndoCommand`. `ArtifactPuppetTool::restoreLayerData` resets/rebinds cached pin state after restoring the layer snapshot.
- **対応:** Added bounded `layer.deformation2D` expected/value snapshots. Initial redo accepts the caller pre-applied after state; later local undo/redo compare the expected snapshot. Remote apply uses the PuppetTool restore API and compensates to the expected state if verification fails.
- **価値または懸念:** Sync transfers only semantic layer deformation data, not renderer/GPU state. Runtime determinism across clients remains unverified.
- **次に確認:** Compare pin add/delete/type changes, undo/redo, and rendered deformation on two clients.

- **追記 2026-09-24:** Puppet pin keyframe drag では、`persistLayerData` 後の deformation JSON が keyframe data を含み、`restoreLayerData` が persistent property を再構築する。before/after snapshot があるケースは専用 sub-property command より `Deformation2DStateUndoCommand` に集約して送る経路へ接続した。snapshot 欠落 fallback と複数 client の評価結果は未検証。



## 2026-09-24 — Solid Gradient drag maps to the existing property batch protocol

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm`、`Artifact/src/Widgets/Render/ArtifactContentGizmo.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`。
- **確認できた事実:** Viewport commits three `solid.gradient*` property values through `SolidGradientUndoCommand`; the existing `property.batch` protocol already validates layer-scoped value CAS and atomically compensates failed remote batches.
- **対応:** Added layer lock targets and a three-entry `property.batch` encoder. Local undo/redo preflight all three expected values; first push accepts only the complete after snapshot already written by the viewport.
- **価値または懸念:** One drag stays one semantic operation and reuses existing server lock and remote conflict handling. The grouped setter rollback and two-client visual parity are runtime unverified.
- **次に確認:** Validate solid gradient drag, undo/redo, server rejection rollback, and competing edits on two clients.



## 2026-09-24 — Source Crop CAS needs an explicit empty-rectangle restore path

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** Source Crop property setters materialize a full-source rectangle when the current rectangle is invalid; width/height setters clamp to at least 1. An initial disabled crop can therefore have an empty rectangle that cannot be reconstructed by replaying the five current property setters.
- **対応:** Added `ArtifactImageLayer::restoreSourceCropSnapshot`, which round-trips the canonical SourceCrop JSON without clamping empty rectangles and refreshes cached non-keyframed property values.
- **価値または懸念:** Sending the current five values as a batch could make undo or rejection rollback leave a one-pixel crop, so a property-only collaboration patch would be unsafe.
- **次に確認:** Define whether empty crop means reset/default/full-source, then add an exact restore API before synchronizing SourceCrop commands.



## 2026-09-24 — Solid size is a bounded layer operation

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm`、`Artifact/src/AppMain.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`、`tools/collaboration-server/server.js`。
- **確認できた事実:** Solid2D and SolidImage source dimensions use integer setters clamped to 1..16384; SolidImage size changes also update rigid/soft body collider state.
- **対応:** Added a layer-scoped `layer.solidSize` expected/value operation. Local undo/redo and remote apply compare dimensions before setting and verify readback; failed readback restores the prior dimensions.
- **価値または懸念:** A viewport resize is synchronized as one semantic operation while retaining existing size setter side effects. Runtime parity, especially physics collider updates, is unverified.
- **次に確認:** Compare Solid2D/SolidImage resize, undo/redo, and collider behavior across two clients.

- **追記 2026-09-24:** Static review found that accepting `current == next` for every Undo/Redo could let stale local history report success. Solid size and gradient now permit this idempotence only for the first redo after the viewport pre-applies the edit; later Undo/Redo require the expected state.

- **追記 2026-09-24:** Added `layer.sourceCrop` as a bounded full-snapshot operation with server lock/schema validation, local Undo/Redo CAS, and remote snapshot CAS. Empty crop geometry survives restore; cross-client rendering with animated crop properties and differing source dimensions remains unverified.



## 2026-09-24 — Shape parameter drags need the same property CAS as inspector edits

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionLayerUndoCommands.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** Viewport corner-radius and star-inner-radius edits already create dedicated Undo commands, while the layer exposes the matching `shape.cornerRadius` and `shape.starInnerRadius` property paths.
- **対応:** Connected both commands to the existing `property.set` expected-value operation and exposed their layer lock scope. Their first redo permits the already-applied viewport value; later undo/redo requires expected-state equality.
- **価値または懸念:** Viewport shape edits now use the same collaboration property CAS lane as inspector edits without a new wire schema. Cross-client rendering and stale-history behavior still need runtime verification.
- **次に確認:** Compare both parameter edits, undo/redo, and concurrent conflict rejection across two clients.



## 2026-09-24 — Editable polygon points require whole-state collaboration snapshots

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionLayerUndoCommands.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`、`tools/collaboration-server/server.js`。
- **確認できた事実:** Polygon vertex drags update a vector of points before recording a dedicated undo command; the setter filters unsupported coordinates, so a partial decode could silently produce a different shape.
- **対応:** Added a bounded expected/value geometry snapshot for the complete Polygon/Bézier override state. Core/server validate all coordinates and sizes; restore verifies exact readback and repairs prior state if a setter drops any point.
- **価値または懸念:** A vertex edit is atomic at the collaboration layer and preserves the full polygon state through undo/redo. Concurrent drags and rendering parity remain unverified.
- **次に確認:** Exercise vertex drag, insertion, undo/redo, stale conflict rejection, and cross-client shape output.



## 2026-09-24 — Bézier path edits share one vertex-state command

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditUndoCommands.cppm`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/AppMain.cppm`。
- **確認できた事実:** Path vertex drags, deletion, smooth/closed toggles, and pending path creation all record `ShapePathVertexEditCommand`; `CustomPathVertex` carries position, relative tangents, and a smooth flag.
- **対応:** Connected this command to bounded `layer.shapePath` snapshots and exact restore/readback. Core/server validate each coordinate and require the layer lock; first redo alone accepts caller pre-application.
- **価値または懸念:** A follow-up found that path creation can start while a custom polygon override exists and `setCustomPathVertices()` clears it. `ShapePathVertexEditCommand` and `layer.shapePath` now capture the complete mutually exclusive polygon/path geometry so Undo can restore that override. Cross-client rendering remains unverified.
- **次に確認:** Verify path creation over a polygon override, vertex/tangent editing, toggle/delete, and undo/redo across two clients.

- **追記 2026-09-24:** Static review found that the polygon command's previous CAS failure branch could apply `expected` after discovering the current state differed, overwriting a newer edit. It now returns immediately on mismatch and only attempts compensation after a matched expected state entered restore.



## 2026-09-24 — Shape operator viewport drags need direct value CAS

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionLayerUndoCommands.cppm`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/AppMain.cppm`。
- **確認できた事実:** Shape operator viewport drags use `ShapeOperatorValueUndoCommand` and update through `shapeOperatorValue`/`setLayerPropertyValue`. Generic property CAS reads the layer's cached `AbstractProperty`, which may lag direct operator mutation.
- **対応:** Added `layer.shapeOperator` with bounded operator index/field and finite expected/value numbers. Local Undo/Redo and remote apply compare the operator's direct getter; remote restore uses its existing setter and verifies readback.
- **価値または懸念:** This avoids using potentially stale property-cache values for a direct viewport mutation. Supported fields still rely on the shape operator setter to reject fields invalid for the current operator type.
- **次に確認:** Verify trim, merge, repeater, and other supported operator fields with drag, Undo/Redo, and conflicting remote edits.



## 2026-09-24 — SVG import synchronization must preserve stack order

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionLayerUndoCommands.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`。
- **確認できた事実:** Shape rendering stores contents and ordered stack nodes separately; adding the first content to a legacy shape materializes that legacy shape at content index 0. A content-only snapshot would lose evaluation order and could change the rendered result.
- **対応:** Added a single bounded snapshot containing contents, stack nodes, and active content index, then connected SVG import undo/redo and remote apply through expected-value CAS.
- **価値または懸念:** Import and Undo now move the rendering-order state together, and restore validates all nodes before replacing live arrays. Cross-client render parity remains unverified.
- **次に確認:** Compare SVG import, Undo/Redo, mixed path/operator ordering, and legacy-shape restoration across two clients.




## 2026-09-24 — Puppet pin fallback coordinates are not portable

- **関連:** `Artifact/src/Tool/ArtifactPuppetTool.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionTextPuppetUndoCommands.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** `pinPosition()` returns a display-space canvas position when the owner layer is selected; `movePin()` converts display to authored space only through the current selected layer. The fallback Undo command is chosen when the current composition cannot provide a layer state snapshot.
- **価値または懸念:** Sending the fallback's position and rotation alone could apply in a different coordinate space or against a different selected layer. Its unresolved collaboration operation should remain fail-closed until a layer-independent authored-state API exists.
- **次に確認:** Review remaining collaboration gaps for operations with stable, project-addressable state; do not promote this fallback to a coordinate-only wire operation.




## 2026-09-24 — Layer move undo needs a frame-scoped CAS

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/AppMain.cppm`、`ArtifactCore/src/Collaborate/CollabOperations.cppm`。
- **確認できた事実:** `MoveLayerCommand` applies position deltas at a frame using `AnimatableTransform3D::setPosition`; remote `layer.transform` had no apply handler and its payload did not identify a frame.
- **対応:** Added `layer.moveAtFrame` with expected/value position X/Y, frame, and time scale. Local undo/redo and remote apply compare evaluated values at that exact time before calling the same transform setter.
- **価値または懸念:** Frame-keyed layer move now has a replayable semantic operation without collapsing it into an unframed transform. The separate Transform Gizmo snapshot command and generic `layer.transform` remain unsupported.
- **次に確認:** Validate keyed/unkeyed moves, conflicts, and undo/redo against a second client; then address gizmo snapshots with full keyframe semantics.



## 2026-09-24 — Gizmo collaboration must distinguish key existence from animatable capability

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm`、`property.batch` collaboration path。
- **確認できた事実:** Gizmo snapshot の `animated` は keyframe 配列が空でないことを表す一方、`AbstractProperty::isAnimatable()` は property capability flag を表す。両者は独立しており、同じ wire flag にすると remote apply が capability を誤変更する。
- **対応:** Snapshot に capability を別保持し、keyframe serialization の `expectedAnimatable`／`animatable` には capability を用いる。key existence は keyframe list のみから復元する。
- **価値または懸念:** Keyed transform の複数 frame を維持しつつ property capability のずれを防ぐ。複数 client runtime と変換対象すべてでの実機 parity は未検証。
- **次に確認:** Check single/group gizmo edits on keyed and unkeyed frames, capability preservation, stale CAS rejection, and Undo/Redo on two clients.



## 2026-09-23 — Procedural3DGenerators の ShapeExtrude IFC順序

- **関連:** `ArtifactCore/CMakeLists.txt` の `ARTIFACTCORE_IMPLEMENTATION_MODULE_REFERENCES` 最終化処理と `src/Geometry/Procedural3DGenerators.cppm`。
- **確認できた事実:** 前段の個別設定は `ShapeExtrude.ixx.obj` を `OBJECT_DEPENDS` に追加するが、後段の一般処理が同プロパティを主インターフェースだけで上書きしていた。実装は module scanner 無効で `/reference` を手動指定するため、ShapeExtrude IFC producer の順序が失われると `Geometry.ShapeExtrude` が見つからず、後続の `ShapeExtrudeParams` も未定義になる。
- **対応:** 最終化処理の上書き後に `ShapeExtrude.ixx.obj` を再追加した。CMake 再生成・ビルドは未実施。
- **次に確認すること:** ユーザーのビルド環境で CMake を再生成し、ArtifactCore を並列ビルドして IFC順序と Procedural3DGenerators のコンパイルを確認する。



## 2026-09-23 — パーティクル完全非表示（症状1）の簡単修正

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm`（`draw`）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`drawParticles` の `ParticlePkt` 構築）、`Artifact/include/Generator/ArtifactParticleGenerator.ixx`（`ParticleRenderSettings.depthTest`）。
- **実装（静的コード変更のみ、未ビルド・未実機）:**
  1. GPU 経路で `particleDebugState` が `state=queued` でない場合（no-rtv / invalid-viewport / device-null 等）はソフトフォールバックへフォールスルーし、GPU 失敗時にレイヤーが真っ白にならないようにした。
  2. `submitParticles()` は color RTV のみバインドするため、キュー投入時の `pkt.data.options.depthTest/depthWrite` を強制 `false`。`ParticleRenderSettings.depthTest` の既定も `true`→`false` に変更（DSV なしの DepthEnable は破棄要因になり得る）。
  3. `draw()` 内の完全再シム直後に `impl_->lastTime` を同期し、ソフトフォールバックの二重 `update` を抑制。ソフト側のフレーム時刻も `max(1, frameNumber)` で揃えた。
  4. JSON の `emitters` が空／無効のみで復元0件の場合、`changed()` を発火しない直接 create でデフォルトエミッターを確保。
- **価値または懸念:** FormParticle の一部 preset（`starfield` 等）は `depthTest=true` のまま。FormParticle が同じ `drawParticles` キュー経由なら強制 false の影響を受ける。既存 JSON の `depthTest:true` はキュー側で無効化される。
- **次に確認すること:** ビルド許可後に新規パーティクルレイヤー表示、GPU 初期化失敗時のフォールバック、空 emitters 保存後の再読込を確認する。



## 2026-09-23 — パーティクル描画経路のシミュレーション不整合（調査）

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm`（`draw` 503、`goToFrame` 1966、`renderToImage` 2019）、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`（particle 分岐 2318）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`layerNeedsFrameSyncForCompositionView` 4706、`drawLayerForCompositionView` 9921）、`Artifact/src/Generator/ArtifactParticleGenerator.cppm`（`ParticleSystem::goToFrame` 1518）、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`（VS `halfWidth` 187）。
- **確認できた事実（静的コード読取のみ、未ビルド・未実機）:** GPU 経路の `ArtifactParticleLayer::draw()` は毎回 `particleSystem->goToFrame`（reset + 固定ステップ完全再シム）で決定論を取るが、`lastTime` を更新しない。`renderToImage` は `playing`（既定 true）かつ `time > lastTime` のとき `particleSystem::update(deltaTime)` をさらに走らせるため、`draw()` のソフトフォールバック（`!rendererReady`）では完全再シム直後に二重シミュレーションになり得る。一方 `CompositionViewDrawing` のラスター／非初期化経路は `layer->goToFrame` → `renderFrame` で、`playing` 時は layer 側 `lastTime` を先に同期するため同フレームでの二重 `update` は起きにくいが、シムは增量（`lastTime` 差分）であり GPU の「フレーム 0 から完全再シム」と同一状態にならない。`playing == false` の `layer->goToFrame` は `particleSystem` を進めずキャッシュ無効化のみで、ソフト面経路はシム未実行のまま描く。サイズはソフト `drawEllipse` 半径 `scale*10`（直径 20×scale、`ArtifactParticleGenerator.cppm:2053,2089`）、GPU VS は `c_Offsets=±0.5` と `localOffset = c_Offsets*(halfWidth*2)` により quad 全幅 `2*halfWidth = 5*size`（`halfWidth=max(0.375,size*2.5)`、`ParticleRenderer.cppm:145-147,187-189`）で、`captureRenderData` の `v.size=p.scale` 時にソフト直径が GPU の約 4 倍。`PARTICLE_LAYER_STATUS_AND_ISSUES_2026-03-27.md` の「halfWidth=size*2.5 で直径一致」は現コードと不整合（直径一致なら halfWidth≈size*10 相当が必要、未検証の数式推論）。シェーダコメント「halfWidth = size」も実装と不一致。`transformParticleRenderData` は GPU 2D で `v.size = size * layerScale` を掛ける。
- **価値または懸念:** 症状が「ソフト面と GPU で見た目が違う」「pause/シークで粒子が進まない」「GPU 粒子だけ小さい」「フォールバック時だけ壊れる」のどれかで優先修正が変わる。実機ログ（`[ParticleRenderer] state=` / `submitParticles drawn` / `layer->debugState()`）無しに単一原因を断定できない。
- **次に確認すること:** 症状の特定（非表示／サイズ／フレーム同期／2D・3D の別）、`playing` false 時のシーク、ラスター effect 付き particle のソフト面と GPU 直描画の parity、ビルド・実機はユーザー指示待ち。



## 2026-09-23 — 2D Deformer連番のメッシュトポロジー

- **関連:** `Artifact/src/Tool/ArtifactPuppetTool.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`、`ArtifactCore/src/ImageProcessing/OpenCV/OpenCVPuppetEngine.cppm`。
- **確認できた事実（コード読取のみ、未ビルド・未実機）:** `ArtifactImageLayer` は解決済みSequenceフレームを `ImageF32x4_RGBA` として保持し、同寸法でないフレームは拒否する。GPU texture cacheはresolved frame indexとcontent keyを区別する。静止画Deformerは初回ソースのalpha輪郭から三角meshを構築し、Sequence Deformerは初回bind時に矩形トポロジーを構築する。いずれもUVは実際の現在フレームテクスチャを参照する。
- **実装:** Deformer描画をSequenceにも呼び出し、ImageF32→OpenCV surface viewを使う。初回または寸法変更時だけSequence用の矩形トポロジーを作り、解決フレームが変わっても同寸法なら再利用する。矩形meshはアルファ255でトポロジー化し、実際の透明度は各フレームのGPU textureが保持する。Sequenceを拒否していたGrid切替、位置キー確定、キー復元の条件を外し、Composition-frameで同じアニメーション経路を使う。輪郭生成／トポロジー確保はcold bind時のみ。
- **価値または懸念:** 初回シルエットの外側へ被写体が移動しても矩形meshが全画面を覆える。一方、alpha-aware silhouetteより頂点／三角形数が増え、MLS評価時間と画質が変わる可能性がある。8-bit矩形トポロジー生成はcold pathだが大解像度時の一時バッファと初回コストは未計測。
- **次に確認すること:** 同解像度でシルエットが異なる複数フレーム、細部の透過境界、大解像度SequenceでGPU結果・画質・bind時間を確認する。ビルド・実機確認待ち。



## 2026-09-23 — 2D Deformer描画経路の統合境界

- **関連:** `Artifact/src/Tool/ArtifactPuppetTool.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`。
- **確認できた事実（静的読み取りのみ、実機未確認）:** Puppet GPU描画フックは CompositionRenderController の画像描画ブランチ内にあり、current frame bufferあり・ラスターeffect／maskなしの場合に呼ばれる。ImageLayerのsource crop layoutからcrop pixel rect、表示rect、回転を受け取り、メッシュUVは元フレームbufferを参照する。通常のeffect/maskは `ArtifactCompositionViewDrawing` の別surface path、ShapeLayerは専用draw pathを通る。
- **実装更新 (2026-09-23):** ImageLayer自身の通常描画も `sourceCropDrawLayout()` を使うよう統一し、source crop矩形、preserve-aspect letterbox、回転transformの算出元をメッシュ描画と共有した。静的読み取りのみで直接描画との実機parityは未確認。
- **価値または懸念:** source cropを現状のPuppet APIへ単純追加すると、source UVと表示キャンバスの対応、mask/effect適用順がずれる。フレームごとのQImage変換やCPU画像warpはホットパス規則に反する。GPUメッシュ処理とsurface合成の共有境界を明確にする価値がある。
- **実装更新 (2026-09-23):** 通常GPUベクター描画ではShapeの三角形／ストローク点にローカル点写像を接続した。PinsはMLS、Gridは双線形評価を使い、描画公開APIはモジュール依存を増やさない関数ポインタ契約にした。GPU effect planが成立してレイヤーマスクが無い場合は後段のGPU effect/matteも通る。レイヤーマスクやGPU plan非対応effectはQImage surfaceへ落ちてDeformer未適用。ShapeのMLSは頂点ごとの評価なので多数制御点／多数パス頂点での時間は未検証。
- **次に確認すること:** ビルド許可後にShapeの両方式、crop回転・aspect保持、アニメーション、Undo/Redo、通常変換との組み合わせを確認し、マスク／effect surfaceへGPU経路で変形を統合する。



## 2026-09-23 — 2D Deformerの有効状態とSequence編集導線

- **関連:** `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`、`Artifact/src/Tool/ArtifactPuppetTool.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionTextPuppetUndoCommands.cppm`。
- **確認できた事実:** Tool Optionsには2D DeformerのPins/Gridと格子密度があったが、描画をバイパスする永続有効状態はなかった。またMainWindowはSequence選択時にPuppet Optionsを無効化し、Grid方式を選べなかった。
- **実装:** Tool Optionsに有効checkboxを追加し、`deformation2D.enabled`をJSONへ保存、既存のDeformation state Undo commandで切替を戻せるようにした。古いJSONでenabledが欠落する場合はtrue。SequenceをOptionsから除外する条件を削除し、既存のImageLayer frame-key経路へ渡す。
- **価値または懸念:** Deformerを破壊せず一時バイパスでき、制御点とキーを保持したまま比較できる。保存再読込・Undo/Redo・Sequenceの実操作確認は未実施。
- **次に確認すること:** checkbox切替→Undo/Redo→保存／再読込、disabled時は原画像表示かつoverlay編集可能、SequenceでPins/Grid・frame-keyがCompositionフレームと一致することをruntimeで確認する。



## 2026-09-23 — XPU P0/P3 追補（S15 atomic・S9 容量メモ・clone check・preset override）

- **関連:** `Artifact/src/Layer/ArtifactSolidImageLayer.cppm:653`（`drawLogSamples` atomic 化）、`Artifact/src/IO/AsyncAssetReadScheduler.cppm:101`（容量コメント）、`Artifact/src/Render/ArtifactRenderQueueService.cppm`（`xpuCloneCheckEnabled`/`verifyCloneParityForJob`）、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`（`xpuPresetOverride`）。
- **事実:** `drawLogSamples` は `static std::atomic<int>` に置換し UB を解消。`AsyncAssetReadScheduler` は worker 3・maxQueuedJobs 256・512 MiB を維持し、MFR concurrency（4–8）との合算でも飽和しないことをコメントで固定。clone parity は `ARTIFACT_XPU_CLONE_CHECK=on` で job 先頭フレームの software path のみを hash 比較し、simulation 使用時は skip。preset は `ARTIFACT_XPU_PRESET` / `preset=` で opt-in 上書き可（未設定は slow/p4 維持、出力が変わるため既定では使わない）。ビルド・実機未確認。
- **次に確認すべきこと:** 実機で `clonecheck=on` の ok/reason ログ、preset override 時の出力差と bench、S9 の実キュー飽和を 4K 連番で確認。



## 2026-09-23 — XPU P4 部分（iGPU assist 予約＋preview 縮小の async 雛形＋throughput 計測）

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`（`xpuIgpuRole`/`buildXpuPlan`/`shouldIncludeIntegratedGpuWorkers`、preview 縮小の `assistPreview` 分岐、`xpuFrameTimer`/`xpuCpuMs`/`xpuGpuMs`）、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`（`ARTIFACT_XPU_MAX_HW_ENCODERS`）。
- **事実:** `igpu=assist` は plan 上で `GpuAssist` として列挙されるのみで、full worker にはならない。preview 縮小だけを assist 時に `std::async` で実行する雛形を入れ、full/off では同期のまま。throughput は frame 毎の `QElapsedTimer` で `xpuCpuMs`/`xpuGpuMs` を蓄積し、job summary に `gpuAvgMs`/`cpuAvgMs`/`weightCpu(=gpuAvg/cpuAvg)` を出すが dispatch への反映はまだしない。NVENC/QSV 上限は env でログのみで実 backoff は未接続。既定動作は従来と同一。
- **次に確認すべきこと:** 実機で `igpu=assist`/`full`/`off` での plan 列挙と preview/出力の差分、weight の妥当性、VRAM 競合時の fallback を確認。



## 2026-09-23 — XPU P3 3-stage pipeline 最小（pipeline=on で convert parallel→encode serial）

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`（`xpuPipelineEnabled`、`isVideo` 分岐の `std::async` convert→`addFrame` serial）。
- **事実:** pipeline は `ARTIFACT_XPU_PIPELINE=on` / `ARTIFACT_XPU=pipeline=on` でのみ有効、既定 off（従来の `addFrame(qimg)` 直呼び）。parallel 段は RGBA8888 への detach のみを `std::async` で実行し、encode は `serial_in_order` を維持して順序・出力不変。preview 縮小は既に時間間引き済みで pipeline とは独立。
- **次に確認すべきこと:** 実機で pipeline on/off での出力同一と wallMs 内訳、例外時の failureReason 伝播を確認。



## 2026-09-23 — XPU P5 部分（job summary＋parity hash＋bench JSON opt-in）



## 2026-09-23 — XPU P3 部分（encode threads・preview間引き・maxInFlight可変化・bounded async sequence）

- **関連:** `ArtifactCore/include/Video/FFMpegEncoder.ixx`（`threadCount`）、`ArtifactCore/src/Image/FFmpegEncoder.cppm`（`codecCtx_->thread_count`）、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`（`xpuEncoderThreadCount`）、`Artifact/src/Render/ArtifactRenderQueueService.cppm`（`resolveMaxInFlightFrames`/`xpuPreviewMinIntervalMs`/`xpuAsyncSequenceEnabled`、consumer preview 間引き＋bounded async）。
- **事実:** `threadCount<=0` は encoder default（現行動作そのまま）、preset/crf/gop は不変。preview 間引きは最終フレーム以外のみ 250ms 既定で publish を間引き、`lastPreviewPublishTime_` は consumer スレッドのみが触る。`maxInFlightFrames_` は env で 1–64 にクランプ、未設定 4。連番 async は単チャンネル image sequence のみ、`std::async` で `2*maxInFlight` を上限に並列書込し、失敗は ledger/failureReason へ反映して consumer を止める。multi-channel/deep/video/HTML/SVG は同期のまま。ビルド・実機未確認。
- **閃き・仮説（未検証）:** 4K での consumer 律速は sws/YUV 変換と preview scaled の直列が主因のはず。P3 の残り（本格的な 3段 pipeline＋sws 並列＋`AsyncImageWriterManager` 置換）が無い限り encode thread＋async sequence だけでは律速が render→consumer 間で移動するだけで wall-time 改善は限定的。
- **次に確認すべきこと:** 実機で 4K 連番の出力同一（hash）＋ async on/off での wall-time と I/O エラー伝播＋ preview が間引かれても最終フレームが必ず publish されることを確認。`ARTIFACT_XPU_ENCODER_THREADS` と `_PREVIEW_MIN_INTERVAL_MS` と `_ASYNC_SEQUENCE` の env が既定時に挙動を変えないことも確認。



## 2026-09-23 — XPU P2 最小作動（mixed=on の CPU+GPU 混在、既定不変）

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`（`xpuMixedRequested`、`xpuMixedCpuCompositions`、`renderOneFrame(forceCpuPath)`、worker 割当）、`docs/planned/MILESTONE_HETERO_COMPUTE_FOUNDATION_2026-09-14.md`（P2）。
- **事実:** CPU worker は MFR と同一契約（isolated snapshot＋software path）で mutex を取らず並列する。合計 thread は `maxInFlightFrames_` 以内（GPU 優先）。順序復元は既存 outputBuffer、失敗は既存 ledger/consumer 経路。Tiled・HTML・multi-channel/deep・simulation は混在対象外にゲート。spec 未設定時の動作は従来と同一。
- **閃き・仮説（未検証）:** 混在の parity リスクは CPU-vs-GPU の画素差（M-IR 未達分）に集約される。P2 DoD の「逐次一致」は実機 hash 比較（P5）でしか証明できない。`mixed=on` を付けたジョブの `xpu-cpu` frame 比率と wall-time 内訳が最初の観測点。
- **次に確認すべきこと:** 実機で opt-in 混在の全 frame 成功＋順序＋逐次一致＋cancel を確認。ビルド・ベンチはユーザー指示待ち。コミット時は子→親順（現 main）。



## 2026-09-23 — XPU 正式命名＋P1残分（XpuNodeDesc 統一 plan・起動ログ）

- **関連:** `docs/planned/MILESTONE_HETERO_COMPUTE_FOUNDATION_2026-09-14.md`、`Artifact/src/Render/ArtifactRenderQueueService.cppm:2699-2812`（XpuNodeDesc・buildXpuPlan）、`:3402-3410` 付近（shouldIncludeIntegratedGpuWorkers）、`:6627-6639`（XPU plan ログ）。
- **事実:** `buildXpuPlan()` はジョブ setup のコールドパスでのみ動き、既存 `GpuFinalWorker` ディスパッチは変えていない。新規 include/import・signal/slot・QImage・QtCSS なし。`ARTIFACT_XPU` 正規、`ARTIFACT_HETERO`／`ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED` は後方互換別名。
- **閃き・仮説（未検証）:** 計画書 P1 の「iGPU=assist 既定」と実装済み挙動（iGPU を独立 full worker 化）が食い違うため、既定 `full`＋`igpu=assist` opt-in 予約に倒した。assist を既定にすると現行の multi-GPU 挙動が変わる。P4 で consumer offload を接続する時に既定値の再レビューが要る。
- **価値または懸念:** plan 列挙ログは multi active 時と single fallback 時の両方に出るが、`mainRendererUsesD3D12 && totalFrames > 1` の外（CPU backend・単フレーム）では出ない。P5 診断で全 backend カバーが必要。
- **次に確認すべきこと:** 実機で `xpuPlan` ログの列挙正しさ（単GPU／複数dGPU／iGPU混在）を確認。ビルド・ベンチはユーザー指示待ち。コミット時は子→親順（Artifact→親 gitlink更新→親push、現 main）。



## 2026-09-23 — CLI の property.set と Command IR の二重編集経路

- **関連:** `Artifact/src/Application/ArtifactInteractiveShell.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`、`docs/planned/MILESTONE_CLI_PYTHON_AUTOMATION_2026-09-23.md`。
- **確認できた事実（静的読み取り）:** CLI `property.set` は限られたプロパティを JSON ファイルへ直接書き換え、CLI 内部だけの project snapshot undo/redo を使う。一方、WorkspaceAutomation の `set_property` は現在ロード中のレイヤーへサービス経由で適用し、Command IR の結果型を返す。Python bridge はアプリ API の戻り値を JSON から dict/list/scalar へ復元し、引数側も JSON 値で型を保つ。`artifact.core.automation` から command vocabulary、validate、execute を呼べる。さらにCLI `command-ir` は catalog / validate / execute requestを受け、executeの `saveProject:true` で既存Project Exporterを呼ぶ実装を追加した。`command-ir -` はJSON Linesを同一プロジェクトsessionで処理する（いずれも実行未検証）。
- **追加修正:** CLI `property.set` で文字列プロパティ `layerNote` に `true` / `false` / 数値文字列を渡すと、共通の値判定が JSON bool / number に変換していた。プロパティ種別ごとに代入を分け、文字列は常に文字列、bool は bool、数値は有限値のみ保存するよう修正した（ビルド・実行未確認）。
- **価値または懸念:** CLIシェルとアプリ自動化 API で同じ編集でも Undo・dirty state・型検証・戻り値の意味が異なる。AI が一方から他方へスクリプトを移すと、動作差を誤認する可能性がある。
- **次に確認すること:** 実行許可後、headless service 初期化、project load、active composition fallback、Command IR execute、`saveProject:true` の保存結果をCLI runtimeで確認し、コマンドシェル／JSONL request 経路での統合も検討する。既存の `property.set` は互換挙動を確認するまで拙速に置換しない。



## 2026-09-22 — HieroPlayer ギャップ分析：レビュー系機能は既存計画と大きく重なる

- **関連:** `docs/analysis/HIEROPLAYER_GAP_ANALYSIS_2026-09-22.md`（新規）、`docs/planned/MILESTONE_REVIEW_WORKSPACE_2026-04-03.md`、`docs/planned/MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md`。
- **事実（Web 公式ドキュメント + 静的コード検索）:** HieroPlayer の中核は「A/B バッファ比較・バージョンスキャン・アノテーション・スコープ・表示専用調整（gain/gamma/clipping 警告）」。ArtifactStudio にはこれらの計画が M-FE-7 系と Compare/Annotation マイルストーンとして既に存在し、Phase 1（compare 基盤）のみ部分実装。
- **閃き・仮説:** 新規マイルストーンを立てるより、既存の未完成 Phase を HieroPlayer 仕様で具体化する方が重複を避けられる。スコープ（histogram 等）と表示調整・clipping 警告は M-VP-DCC-1 の Viewer exposure controls と同一課題であり統合候補。
- **価値／懸念:** 動画依存の機能（Skip Frames 再生、Broadcast Monitor、Sync Review）は開発優先方針（2026-07-27、動画後回し）と衝突するため P2 に分離した。
- **次に確認すべきこと:** ~~ユーザー判断 — P0「Viewer Inspection Controls」を M-VP-DCC-1 に統合するか~~ → **2026-09-22 解決**: ユーザー判断で M-VP-DCC-1 へ統合。P1-10〜P1-14 として `MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md` に追加済み。P1/P2 の比較・アノテーション系は M-FE-7 系マイルストーン側に残す。



## 2026-09-22 — VP DCC パリティ P0-3a（Interactive Render Region 矩形のみ）着手

- **関連:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactCore/include/UI/ShortcutBindings.ixx`、`ArtifactCore/src/UI/ShortcutBindings.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`docs/planned/P0_DESIGN_NOTES_2026-09-22.md`、`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) `RenderContext::roi` は CompositionRenderController から現在呼ばれていない（`EffectContext ctx` のみ使用、damage tracker の `RenderROI` 値は tile plan 計算専用）。よって「`RenderContext::roi` に IRR 矩形を代入」という設計メモ §3 の計画はそのまま実行できない。**P0-3a は矩形を controller が保持し、overlay + HUD に露出する最小実装**に切り替え、RenderContext 経路の統合は P0-3b（または別マイルストーン）に分離。(b) `setInfoOverlayText(QString title, QString detail)` は既存の軽量 HUD。`setInteractiveRenderRegion / clearInteractiveRenderRegion` 内で更新する。(c) IRR ハンドルは 9 個（move + 8 隅/辺）。ヒットテストは viewport → canvas 変換の逆（pan/zoom で canvas→viewport）に統一。ドラッグ中の modifier を 0（none）/ 1（move）/ 2-9（NW/N/NE/E/SE/S/SW/W）で識別し、`updateInteractiveRenderRegionDrag` で 1 つの switch 文に集約。(d) `isBoxZooming_` と同じ modality の流儀で `interactiveRenderRegionHandleDrag_ != 0` を `interactionBusy()` に追加し Detached Task 実行を正しくブロック。(e) `ShortcutBindings::matches` 経由で Ctrl+Shift+R を `ViewInteractiveRenderRegion` として登録、`Count = 156`。
- **価値または懸念:** (a) IRR 矩形は RenderContext 統合前なので**表示のみで再レンダーは発生しない**。マイルストーン記述の「P0-3 で ProgressiveRenderer を再利用する」も未接続で残っており、別マイルストーンで RenderContext 経路を設計する必要がある。(b) BoxZoom / Tumble pivot と modality が重なる瞬間があるため、`handleMousePress` の判定順序（BoxZoom 修飾 → IRR ハンドル → 既存 press）に注意。Esc 経路も 3 件並列で追加し、優先順位は active 状態で決定。(c) ShortcutId が 155→156 まで増えたことで `std::array<..., Count>` のサイズは依然自動追従だが、JSON 永続化キー（既存ユーザー設定）に後方互換性が必要かは再確認したい。
- **次に確認すること:** (a) RenderContext を CompositionRenderController の render path から使えるようにするための最小インターフェース設計（`getRenderContext() / setRenderContext()` のような setter が必要か）、(b) 矩形外側をクリックした場合の挙動（現状は IRR が外側をクリックしても既存 press が走る。これは IRR が「背景マスク」として機能したい場合に再設計が必要）、(c) Quad レイアウト時の 4 ペイン同期は P0-3b の PaneState 拡張と同時に着手。



## 2026-09-22 — VP DCC パリティ P0-1 / P0-2 着手

- **関連:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactCore/include/UI/ShortcutBindings.ixx`、`ArtifactCore/src/UI/ShortcutBindings.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`docs/planned/P0_DESIGN_NOTES_2026-09-22.md`、`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) `viewportOrientationViewMatrix(orientation, QPointF target, float distance)` は `target` を `(x, y, 0)` の world 座標として lookAt 計算する。既存呼び出しの `orientationTarget` は zoom/pan 逆算の canvas pixel（中心点）で、Z=0 面上に置かれる前提。つまり tumble pivot は **canvas pixel のまま渡すのが既存と完全一致**（前回の world 変換案は誤り）。(b) 既存 `beginBoxZoomInteraction` 系の modality ガード（rubber-band, lasso, shape marquee, modal gizmo, text edit, interactionBusy）を流用できる。`isBoxZooming_` を `interactionBusy()` に加え Detached Task を正しくブロック。(c) BoxZoom の確定は `fitToRect` が renderer に存在しないため、`viewportW/canvasW` と `viewportH/canvasH` の小さい方から倍率を計算し `zoomAtFactor(rectCenter, factor)` で smooth-zoom を再利用。(d) Tumble pivot marker は既存十字描画パターン（pivotColor 半透明黄、`pivotRadius = max(5.0f, 7.0f * invZoom)`）を inline 複製 10 行で実装。`LineDebugKind` enum を増やさない。(e) `usesSpatialViewportOrientation()` が false（Front ortho）のときは発動も描画も完全抑止。(f) Esc で BoxZoom キャンセル / Tumble pivot クリアの経路を追加、Space+Z で Tumble pivot 設定、Ctrl+Alt+B で BoxZoom 開始（既定キーは ShortcutBindings に登録、UI 側は修飾一致を `ShortcutBindings::matches` で判定し固定キーは使わない）。(g) logical → physical 変換は begin/update/setTumblePivot の入口で `* devicePixelRatio_`、既存 `handleMousePress / handleMouseMove` と同じ規約。
- **価値または懸念:** (a) BoxZoom は logical で受け取って内部で physical に統一、UI 側で 2 度掛けしない。CompositionEditor の mousePressEvent で logical の `event->position()` をそのまま `beginBoxZoomInteraction` に渡す。(b) ShortcutId を 153→155 に増加。`Count = 155` を含む 4 箇所（ixx, shortcutIdKey, shortcutDisplayName, defaults_）を更新済み。`std::array<..., Count>` のサイズは自動追従。(c) `resetView()` で tumble pivot override もクリア（saved camera target と reset 後の整合性）。
- **次に確認すること:** (a) ビルド許可後に D3D12 / Vulkan で `viewportOrientationViewMatrix` の差し替えと十字描画が描画結果を変えずに表示されること、(b) BoxZoom 終端の zoom factor 計算で `std::min(fitX, fitY)` が矩形のアスペクトを歪めないこと、(c) Quad レイアウト時 4 ペインすべてで tumble pivot が同期するか（現状は単一 controller 状態のみなので、PaneState 拡張は P1-3 で行う）、(d) Space+Z が PlaybackToggle（Space 単独）と衝突しないこと（修飾 Z で分離済み、ユーザーが上書き可能）。



## 2026-09-22 — VP ナビゲーション cross は既存実装済み

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`docs/technical/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_STATUS_2026-09-04.md`。
- **確認できた事実（静的読み取りのみ）:** `drawNavigationCrossOverlay()` は既に Diligent overlay API の `drawSolidLine` で中央 cross を描画し、`CompositionRenderController` の描画経路から `previewOrbitActive_` 時に呼び出されている。したがって、ナビゲーション cross を新規実装する必要はない。
- **価値または懸念:** ナビゲーション契約の状態文書には未着手と残っているため、同じ機能を重複実装すると二重描画や既存ギズモ仕様との競合を招く。今回の VP ギャップ調査では、文書上の未実装表示と現行コードの差分を先に再照合する必要がある。
- **次に確認すること:** DCC パリティ計画の実未実装候補から、Qt/Win32 の二重入力経路を伴う Box zoom/crop または既存 ROI を UI 化する IRR を選び、現在の並行変更と衝突しない範囲を確認する。



## 2026-09-22 — Bitwig着想モーション変調 Phase 2: AutomationClip（所有者決定・評価・保存・Undo・最小UI）

- **関連:** `ArtifactCore/include/Animation/AnimatableValue.ixx`（Clip型・評価・JSON変換追記）、`Artifact/include/Composition/ArtifactAbstractComposition.ixx`＋`Artifact/src/Composition/ArtifactAbstractComposition.cppm`（共有パターンライブラリ・Clip化API・JSON）、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`＋`ArtifactAbstractLayer.cppm`（インスタンス保持・評価配線・JSON）、`Artifact/include/Undo/UndoManager.ixx`＋`UndoManager.cppm`（InstancesCommand・encode/decode修正）、`ArtifactTimelineWidget.cppm`（Clip化）、`ArtifactPropertyEditor.cppm`＋`ArtifactPropertyWidgetShared.cppm`（割当メニュー）、`tests/ArtifactCore/AutomationClipTest.cpp`。
- **確認できた事実（静的読み取りのみ、ビルド・テスト未実行）:** (a) Phase 1 の Undo encode/decode が新 source フィールド未対応だったため今回修正（decode の型上限も Macro→Steps）。(b) 新規 `.ixx` を作らず既存 `Animation.Value` に追記したため CMake の module 登録変更は不要。新規テストファイルのみ `tests/ArtifactCore/CMakeLists.txt` に登録。(c) Clip 評価は keyframe→modulation→clips の順で dynamics/envelope の前に適用。存在しないパターン参照・無効値は base へフォールバックし再生を壊さない。(d) UI は Timeline 変換と Property 割当/再利用/全解除の最小導線のみ。Unique 化・Curve Browser は Phase 3。(e) `curvature` は保存のみで評価未適用、Color remap は先送り。
- **価値または懸念:** 並行セッションの Layer 分割と本 Phase の Layer/Composition/Undo/Widget 編集が重なる。分割完了後の突合せが必要。`getLocalTransform`/`opacity` のホットパスは instances 空時に無負荷（早期 return＋`empty()` ガード）。
- **次に確認すること:** (a) ビルド許可後に `AutomationClipTest`＋既存変調テスト実行、(b) Clip の保存/再読込・別レイヤー適用・loop/stretch/free-time の Preview/RenderQueue 一致、(c) Phase 3（Alias/Unique・プリセット）着手可否。



## 2026-09-22 — C4D/Houdini/Maya ビューポート比較と、DCC パリティ導入の着手前状態

- **関連:** `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`、`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`、`docs/planned/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_TODO_2026-09-04.md`、`ArtifactCompositionRenderController.ixx`、`ArtifactCompositionEditor.cppm`、`ArtifactFrameCache.cppm`。
- **確認できた事実（静的読み取り＋公式ドキュメント）:**
  - Houdini の Display Options は Markers/Guides/Visualize/Geometry/Scene/Camera/Lights/Material/Fog/Grid/Background/Texture/Optimize のタブ構成。ナビゲーションは Box zoom/crop、Screen pan、Space+Z の tumble pivot、Home all/selected/c-plane、tear-off viewport copy、Ghosted objects を持つ。
  - Maya の Shading メニューは Wireframe on Shaded / X-Ray / X-Ray Joints / Backface Culling / Smooth Wireframe / Bounding Box / Cycle rig display など。Viewport 2.0 Options は Transparency Algorithm（Simple/Object Sorting/Weighted Average/Depth Peeling）、GPU Instancing、Light Limit、Hardware Fog、Object Type Filter を持つ。
  - C4D は IRR（領域レンダー＋解像度スライダ＋Alpha Mode/Lock to View/Gadget Overlay）、Viewport Solo、Filter タブ、HUD、Workplane/Snapping/Quantize、Camera Navigation プリセットを持つ。
  - 一方 ArtifactStudio は `ViewportChannelDisplayMode` で Depth/Emission/ObjectId/MaterialId/Albedo/Normal/Velocity/Position/UV まで分離表示できており、バッファ可視化は Nuke/Maya 相当に届いている。逆に未導入は、種類別ビューポートフィルタ（`CompositionLayerRenderFilter` は All/SelectedOnly の2値）、IRR 相当（`ArtifactRenderROI` と `ProgressiveRenderer` は既存だが VP の矩形 UI が無い）、Box zoom/crop、tumble pivot のカーソル下設定、ghosted context、isolate の状態復元、tear-off viewport。
  - `CompositionViewport::NavigationSessionState` が既に存在し、`PreviewOrbitSnapshot` に接続済み。ナビゲーション契約 TODO の T2 は実装済みだった（TODO 文書を最新化）。
  - Maxon Autograph の Viewer も比較に追加した。接続スロット＋Lock/Freeze、2要素比較（ブレンド込み）、Channel Selector（premult / Straight / Luminance / Matte）、Gain/Gamma/Saturation の露出コントロール、ビューポート単位のフォーマット上書き（Responsive Design 相当）、パス overlay の4モード可視性（Always/Never/Hovered or selected/Selected Layers）を持つ。ArtifactStudio はズーム/フィット/100%、回転スナップ、`setCompareMode` による A/B 比較は既にあるが、露出コントロール・チャンネルバリアント・フォーマット上書き・パス overlay の可視性モードが未導入（計画の P1-5〜P1-7、P2-6 に追加）。
  - Blender も追加（Viewport Shading / Overlays / Sidebar）。新規ギャップは per-viewport Local Camera / Focal / Clip、Local View・Local Collections の分離と復元、View Regions、Cavity / Studio Shadow、Fly/Walk。既存監査（2026-08-15）の 3ds Max / Unreal / Nuke 系（Show Flags、Sample Points、SteeringWheels、Dope Sheet in Viewer）も分析へ統合した（計画の P1-8〜P1-9、P2-7〜P2-8）。
  - Editor のナビゲーション入力は Qt イベント経路と Win32 ネイティブ経路（WM_LBUTTONDOWN 等）の二重構造。Box zoom を追加する場合は両経路に同じ状態遷移を実装する必要がある。
- **価値または懸念:** 既存監査（2026-07-04）で「未実装」とされた項目の一部は既に実装済みで、ドキュメントが実装に追いついていない。逆に IRR やタイプ別フィルタのように、既存インフラ（ROI/Progressive/filter）が揃っていて UI だけ未着手の項目は低コストで導入できる。P0 の実装は `ArtifactCompositionRenderController` / `ArtifactCompositionEditor` 触りとなるが、両ファイルは並行セッションが同日中に更新しており、着手タイミングの調整が必要。
- **対応:** 分析と導入計画を新設し、監査文書・ナビゲーション契約 TODO・バックログを最新化した。実装は未着手。
- **次に確認すること:** 並行セッションの完了後に P0-1（Box zoom/crop）と P0-2（tumble pivot）を実装する。いずれも `ShortcutBindings` の Viewport ローカルコンテキスト登録と、Qt/Win32 の二重入力経路の両方への反映が前提。IRR（P0-3）は `ArtifactRenderROI` と `ProgressiveRenderer` の契約確認から始める。



## 2026-09-22 — Dithering の Bayer 8×8／16×16 CPU 参照には行列境界の不整合がある

- **関連:** `Artifact/src/Effects/Dithering/DitheringEffect.cppm`（`DitheringEffectCPUImpl::applyCPU`、Bayer 分岐）。
- **確認できた事実（静的読み取り）:** 2×2 と 4×4 の行列だけが定義されている一方、Bayer 8×8／16×16 選択時には `bayerN` をそれぞれ 8／16 に変更して、4×4行列ポインタのまま `bi % (bayerN * bayerN)` を参照する。参照インデックスが16以上になり得るため、CPU参照の境界外読み取りになる。
- **対応:** CPU Bayer 経路を行列配列参照から、座標ビットごとに順位を計算する `bayerRank` へ変更。2×2／4×4 は既存行列と同じ順位になり、8×8／16×16 も範囲内の順位を生成する。固定容量の整数演算のみで追加確保はない（ビルド・画像 parity 未確認）。
- **価値または懸念:** 8×8／16×16をGPU常駐化する場合も、CPU側の正規順位との画素一致を先に確認する必要がある。今回のGPU常駐化はBayer 2×2／4×4だけに限定されている。
- **次に確認すること:** 許可後に2×2／4×4既存出力との一致、8×8／16×16の色数・pattern scale別CPU基準画像、GPU経路との差を確認する。



## 2026-09-22 — Bitwig着想モーション変調 Phase 0/1: Router拡張＋Transform接続（ビルド未実施）

- **関連:** `ArtifactCore/include/Audio/Modulation/Modulator.ixx`（Constant/Noise/Steps追加）、`Router.ixx`（型4-6・Binding・empty）、`Artifact/src/Layer/ArtifactAbstractLayer.cppm:1401`（getLocalTransform変調適用）、`ArtifactAbstractLayerModulation.cppm`、`ArtifactAbstractComposition.cppm:809`、`UndoManager.cppm:1401`、`tests/ArtifactCore/AudioModulationRouterTest.cpp`、`docs/analysis/MOTION_MODULATION_PHASE0_CONTRACT_2026-09-22.md`。
- **確認できた事実（静的読み取りのみ、ビルド・テスト未実行）:** 2026-08-29実装のRouter（LFO/ADSR/Random/Macro・processAtFrame冪等・snapshot往復）を正規基盤として再利用し、置換していない。Transform適用はkeyframe評価→変調→dynamicsの順でopacity()と同順序。割当なし時は`empty()`ガードで無負荷。
- **価値または懸念:** (a) 並行セッションがArtifactAbstractLayerを*Support.cppmへ分割中のため（本ファイル冒頭の2026-09-22記録）、getLocalTransform周辺の移動と衝突する可能性がある。分割完了後に同関数の所在確認が必要。(b) Transform適用時のQString構築は割当存在時のみだがbounded確保の例外として記録済み。(c) Phase 2のAutomationClip所有者（レイヤー所有か共有アセットか）は未解決事項のまま。
- **次に確認すること:** (a) ビルド許可後に`AudioModulationRouterTest`実行、(b) layer JSON round-tripとpreview/export一致、(c) 並行分割との突合せ後にPhase 2（AutomationClip）着手。



## 2026-09-22 — 並行セッションによる ArtifactAbstractLayer 分割中にプロパティグループ述語の定義が宙吊り

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`（2026-09-22 01:22 に別プロセスが更新）、`Artifact/include/Layer/ArtifactAbstractLayer.ixx:340`、`Artifact/src/Layer/ArtifactAbstractLayerUtilities.cppm:99`、今夜新設の `ArtifactAbstractLayer*Support.cppm` 群（23:35–01:37 に連続更新）。
- **確認できた事実:** `isTimelineHiddenLayerPropertyGroup` ほか isTimeline*/isInspector* 系述語の定義が `.cppm` から削除され、どのファイルにも再配置されていない（宣言と呼出しは残存）。別プロセスがレイヤーモジュールを `*Support.cppm` 群へ分割中。私が P2 作業で追加した `isTimelineTextAnimatorLayerPropertyGroup` の宣言は `.ixx:344` に残っているが、実装・呼出しは未配置のためリンク影響はない。
- **価値または懸念:** 並行セッションと同じモジュールを同時に編集すると変更が衝突する。分割が完了するまで同モジュールの編集は控えるべき。
- **次に確認すること:** 並行作業の完了後に述語の新しい定義場所を確認し、P2（Timeline 左ペインへの Text Animator 露出）の実装（述語の実装＋8箇所の呼出し例外）を再開する。手順は `docs/planned/MILESTONE_TEXT_ANIMATOR_ADD_WORKFLOW_2026-09-21.md` の P2 に記録した。
- **更新 (2026-09-26):** 上記の宙吊りは解消済み。分割は完了し、述語は `Artifact/src/Layer/ArtifactAbstractLayerPropertyGroups.cppm` に定義が戻っている（`isTimelineHiddenLayerPropertyGroup:108`、`isTimelineTextAnimatorLayerPropertyGroup:149`）。`ArtifactAbstractLayerPropertyGroups.cppm` はモジュール partition（`:Impl`）なので `Artifact/cmake/ArtifactSources.cmake:926` への明示登録も存在する。`isTimelineTextAnimatorLayerPropertyGroup` は宣言・実装・登録の三方が揃った**未接続コード**（呼出し 0 件）として残っていた。
- **対応 (2026-09-26):** P2 の配線を完了した。既存述語 `isTimelineHiddenLayerPropertyGroup` はグループ名文字列のみで判定するため、Text Animator グループ（AGENTS.md により表示名ではなくプロパティパスで識別해야する）を判定できない。そこで `ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup` に `PropertyGroup` 版オーバーロードを追加し、`isTimelineTextAnimatorLayerPropertyGroup(group)` が true なら非表示にせず、false のときだけ既存の名引判定へ委譲する。呼出し側は 9 箇所すべてが `group.name()` だけを渡していたため、`group` 全体を渡す形へ置換した（`ArtifactTimelineKeyframeModel.cppm:366,758`、`ArtifactTimelineTrackPainterView.cppm:1028,1248,4558,5391`、`ArtifactLayerPanelWidget.cppm:1218,1250,2970`）。名引版オーバーロードは削除せず温存している。
- **懸念 (2026-09-26):** `ArtifactLayerPanelWidget.cppm:1250` は判定後に `result.push_back(group)` があり、`PropertyGroup` のコピコンは `Impl` を新規確保して `properties_` をベクタコピーする（`ArtifactCore/src/Property/PropertyGroup.cppm:56-59`）。この経路で `allProperties()` を呼ぶと確保が増えるが、左ペイン再構築のみでフレーム毎ではないため追加の最適化はしていない。`AGENTS.md` の Transform 限定条項には、本件を例外とする追記を同日行った。
- **次に確認すること:** ビルド許可後に `check_module_hygiene`、テキストレイヤーで Animator 追加 → 左ペイン露出 → キー追加 → 再生 → 他レイヤー（Image / Shape）で非 Transform が出ないこと、Undo/Redo を確認する。



## 2026-09-22 — P1-6 着手: チャンネル表示バリアント（Straight / Luminance / Matte）

- **関連:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) `ViewportChannelDisplayMode` は ixx に 24 値定義済み。`Color / Alpha / ColorAlpha / Red / Green / Blue` は補助チャンネル不要、`Depth / Emission / ObjectId / MaterialId / Albedo 系 / Normal 系 / Velocity 系 / Position 系 / UV 系` は補助チャンネル必要。`syncViewportChannelReadbackConfiguration` は `needsAuxChannel` 判定で `setMultiChannelEnabled` と 16 個の `setChannelEnabled` を呼ぶ。(b) 既存 switch は **全網羅** が C++ で要求されるため、新 3 値を no-op case として `Color / Alpha / Red / Green / Blue` と同じ break 群に追加する必要あり。(c) `Unpremultiplied / Luminance / Matte` のいずれもが **RGB + Alpha SRV だけ**で描画可能（Depth / Emission / ObjectId / Albedo / Normal / Velocity / Position / UV は不要）。よって `needsAuxChannel` 判定に 3 値も追加して `false` を返すべき。既存の判定式（`!= Color && != Alpha && != ColorAlpha && != Red && != Green && != Blue`）に 3 値を追加。(d) `viewportChannelDisplayLabel()` は HUD 風の表示ラベルで、24 ケースを switch で `"RGB" / "Alpha" / ...` に変換している。新 3 値は `"Straight" / "Luminance" / "Matte"` のラベルで表示。(e) `setViewportChannelDisplayMode` 本体は単純（保存 + `syncViewportChannelReadbackConfiguration` + dirty）なので switch 拡張不要。`syncViewportChannelReadbackConfiguration` 内の switch だけ拡張。
- **価値または懸念:** (a) **最小限の実装で済み、ビルド・実機確認なしでもリスクが低い**: enum 追加 3 値 + 既存 2 switch の no-op case 追加 + label 3 ケース + command palette 3 エントリ。(b) `Unpremultiplied` の正確な計算（`RGB / α`）と `Luminance` の Rec 709 係数、`Matte` の α を赤に加算する合成は、**post-process shader / readback overlay pass** に委譲する想定で、本マイルストーンでは `ViewportChannelDisplayMode` の値だけ追加。**実描画反映は別マイルストーン**で readback overlay に追加（decision doc §3 P1-6 の「チャンネル表示バリアント」の実体部分）。(c) `ArtifactViewMenu` にはチャンネルメニューが **存在しない**（grep 確認済み）。`CompositionEditor` の command palette のみが拡張経路。channel button (13079 行付近) のチェック状態はそのまま維持され、新 3 値に切り替えたとき "Color / Alpha / Red / Green / Blue のいずれも checked にならない" 状態になる。channel button 側の switch 拡張は本マイルストーン外。
- **次に確認すること:** (a) `ViewportChannelDisplayMode::Unpremultiplied` を選んだとき、`readbackOverlay` 経路で RGB を unpremultiplied で再描画するポストプロセス shader が D3D12 / Vulkan 両方で同一結果を返すか、(b) HUD の `viewportChannelDisplayLabel()` が `"Straight" / "Luminance" / "Matte"` を正しく表示するか、(c) `syncViewportChannelReadbackConfiguration` の新 3 値 no-op case が C++ の switch 網羅要件を満たすか（gcc / clang / MSVC の warning `-Wswitch` で警告が出ないこと）。



## 2026-09-22 — P0-4 着手: ビューポート タイプ別フィルタ

- **関連:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/.../ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`、`ArtifactCore/include/UI/ShortcutBindings.ixx`、`ArtifactCore/src/UI/ShortcutBindings.cppm`。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) `CompositionLayerRenderFilter` を拡張すると Render Queue に波及するため、**新規 `CompositionViewportLayerCategoryMask`** を別系統として追加。bit flag (uint32_t) で `Solid2D / Text / Image / Shape / Adjustment / Null / Mask / Audio / Particle / Clone / Light3D / Camera3D / Model3D` の 13 カテゴリ + `All = 0xFFFFFFFF`。(b) `ArtifactAbstractLayer` には `isLightLayer / isCameraLayer` 仮想関数が無い。`className()` で文字列比較する経路が現実的。`dynamic_pointer_cast` を毎フレーム呼ぶのはホットパスを太らせるため、`QHash<LayerID, CompositionViewportLayerCategory> layerCategoryCache_` を Impl に追加し、`setComposition` で `clear()` して再分類する設計。(c) 既存 `passesLayerRenderFilter` の隣に `passesLayerCategoryMask(mask, layer, category)` を free function として追加し、layer 描画ループの最初にチェックを挟む。`skipCategoryCount` を追加して既存 `skipRoiCount / skipLodCount` と並列で診断ログを揃える。(d) 公開 API は `setViewportLayerCategoryMask / viewportLayerCategoryMask() / toggleViewportLayerCategory / isViewportLayerCategoryVisible` の 4 つ。`toggle*` は mask の対応 bit を XOR 反転する軽量実装。(e) `ShortcutId::ViewToggleLayerTypeFilter` を追加（`Count = 157`）。既定キーは **空** にし、View メニューの Show サブメニューを主導線とする方針。ユーザーは設定画面で任意キーへ bind 可能。(f) `ArtifactViewMenu` に `表示(&W) > レイヤー種別(&L)` サブメニューを追加し、3D Lights / 3D Cameras / Audio / Particle の 4 トグル + 「すべて表示(&S)」リセットを配置。`ArtifactCompositionEditor` の command palette にも同等の 5 アクションを追加し、View menu 経由と palette 経由の両方で操作可能に。
- **価値または懸念:** (a) カテゴリ判定を `className()` ベースにしたため、新しいレイヤーサブクラスを追加するときに `classifyLayerCategory` を更新する必要がある。これを忘れると未知クラスは `is3D()` を見て `Model3D` か `Image` にフォールバックされるが、設計としては「新クラス追加時に P0-4 拡張」を明示すべき。(b) `setComposition` 入口で `layerCategoryCache_` を `clear()` する経路は composition pointer が同じでも安全に動く（QHash の `clear()` は O(n) だが composition 切替時のみ発火）。(c) `currentEditor()` が `ArtifactViewMenu.cppm` で **未定義のまま呼ばれていた**既存問題（1780, 1804 行）。私の追加も同じパターンを使ったため既存問題が温存される。ビルド時に `currentEditor` の宣言がエラーになるため、ユーザーは別途 `activeCompositionEditor` などに置換する必要がある。これは本 P0-4 のスコープ外として記録し、別セッションで修正する。(d) `toggleViewportLayerCategory` で bit を反転する際、`Light3D = 1 << 10` のように high bit のカテゴリも問題無く反転できるが、`All`（0xFFFFFFFF）との XOR は全 bit が反転するため注意。コードでは個別 bit 単位の反転ロジック（`(mask & bit) != 0 ? (mask & ~bit) : (mask | bit)`）を使っており、`All` の特殊ケースは起きない。
- **次に確認すること:** (a) ビルド許可後に `CompositionLayerRenderFilter::SelectedOnly` と `CompositionViewportLayerCategoryMask` を同時に使ったとき、フィルタ → カテゴリマスクの順で正しくレイヤーがスキップされるか、(b) `is3D()` フォールバックが意図せず `Model3D` に分類される未知 3D レイヤーが表示されてしまう問題、(c) `currentEditor()` の置換が必要な件（既存 + 新規 3 箇所）。



## 2026-09-22 — P0-3d 着手: RenderPartialRegion 関数と D4 強制ダウングレード

- **関連:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/.../ArtifactCompositionRenderController.cppm`、`Artifact/include/Render/ArtifactFrameCache.ixx`（`RenderQuality`）、`Artifact/include/Service/ArtifactProjectService.ixx`（`PreviewQualityPreset`）。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) `ProgressiveRenderer::setRenderCallback(RenderCallback)` の signature は `std::function<bool(RenderQuality)>` で 1 引数固定。decision doc D3 の「`PartialRenderCallback = std::function<bool(RenderQuality, const RenderROI&)>` を `CompositionRenderController::Impl` 内にローカル typedef として置く」とは互換せず、**ProgressiveRenderer は所有せず callback 経由の委譲も見送り**、CompositionRenderController 内に `renderPartialRegion(RenderQuality, RenderROI)` を private 関数として実装する方針に変更。(b) `setPreviewQualityPreset` は `factor` (int) を `previewDownsample_` に保存するが、`PreviewQualityPreset` enum を保持しないため、IR 解除時に preset を復元できない。**`previewQualityPreset_` メンバを `Impl` に追加**し、`setPreviewQualityPreset` 内で保存する経路を追加。(c) `RenderQuality` は `Artifact.Render.FrameCache` モジュールから提供され、`{Draft, Preview, Final, Custom}` の 4 値。`ArtifactCompositionRenderController.ixx` に `import Artifact.Render.FrameCache;` を追加することで `RenderQuality` が `namespace Artifact` 内で解決可能。(d) `interactiveRenderRegionResolutionScale_`（既存 P0-3a）は `[0.25, 1.0]` の float。これを RenderQuality に変換する閾値（`≤0.34 → Draft`, `≤0.67 → Preview`, それ以外 → Final`）を `renderOneFrameImpl` 内のフックで適用。
- **実装:** (1) ixx に `import Artifact.Render.FrameCache;` を追加（`RenderQuality` 解決用）。(2) Impl に `previewQualityPreset_` / `irrForcedPreview_` / `lastPartialRenderQuality_` / `partialRenderCount_` を追加。(3) `setPreviewQualityPreset` の入口で `previewQualityPreset_ = preset` を保存。(4) `setInteractiveRenderRegion` 入口で `irrForcedPreview_ = true` + `setPreviewQualityPreset(Preview)` を実行し、HUD に `(quality forced to Preview)` を追記。(5) `clearInteractiveRenderRegion` で `irrForcedPreview_` を確認し、preset を復元。(6) `renderOneFrameImpl` の sync ブロック内で `renderContext_.setMode(interactiveRenderRegionActive_ ? RenderMode::Preview : RenderMode::Editor)` を呼ぶ（decision doc D4: IR 中は RenderMode を Preview 強制）。(7) `damageTracker_.clearAll()` 直後に `interactiveRenderRegionActive_` 時に `renderPartialRegion(owner, quality, rect)` を呼び、解像度スケール → quality マッピングを実行。(8) `Impl::renderPartialRegion` 本体は品質を `lastPartialRenderQuality_` に記録し、`damageTracker_.markFullRedraw(QString())` + `invalidateBaseComposite()` + `markRenderDirty()` でフレーム再描画を要求。**実描画は既存の `renderOneFrameImpl` 経路に委譲**し、IRR scissor がすでに active なため矩形内だけ再レンダーされる。
- **価値または懸念:** (a) `setPreviewQualityPreset` の保存ロジック追加は API 互換性に影響なし（内部メンバ追加のみ）。(b) `setInteractiveRenderRegion` で Final → Preview 強制後、`clearInteractiveRenderRegion` で元に戻すパスが**再帰呼び出し**になる（`setPreviewQualityPreset` → `invalidateBaseComposite` → `markRenderDirty` → 再描画トリガ）。再描画は `markRenderDirty()` で dirty フラグを立てるだけなので、Immediate な再帰は起きないが、コメントで明示すべき。(c) `renderPartialRegion` の本体は現状 **品質記録 + 再描画要求**のみで、実描画の再絞り込み（downsampling factor を pipeline に渡す）は未実装。decision doc §3 P0-3d.1 で `ProgressiveRenderer` 統合または別経路で実装する想定。(d) `RenderQuality` の閾値マッピング（0.34, 0.67）は暫定値。`interactiveRenderRegionResolutionScale_` の UX（0.25〜1.0）と `RenderMode` のセマンティクス（Draft = useScissorTest/useROICache/skipEmptyROI が true）を擦り合わせる必要があり、ビルド・実機確認後に調整する。
- **次に確認すること:** (a) ビルド許可後に `PreviewQualityPreset::Final` で開始した Composition で IR 矩形を有効化したとき HUD に「(quality forced to Preview)」が表示され、`clearInteractiveRenderRegion` で元に戻ること、(b) `RenderMode::Editor` のとき `getModeSettings(RenderMode::Editor)` は `useScissorTest=true` を返すため、scissor が動作する前提。IR active 中は Preview 強制なので `useScissorTest=true` が維持される、(c) `renderPartialRegion` を毎フレーム呼ぶと再描画要求が連続するが、現状の `markRenderDirty()` はアトミックなフラグ更新なので問題なし。次フレームで `damageTracker_.markFullRedraw` の効果が出る。



## 2026-09-22 — P0-3b.2 着手: scissorROI を ArtifactIRenderer に適用

- **関連:** `Artifact/include/Render/ArtifactRenderLayerPipeline.ixx`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) `RenderPipeline::renderComposition` は実装が薄いスタブ（コメントで「The controller currently owns the actual layer draw/blend loop」と明記）。`SetRenderTargets + ClearRenderTarget` だけが本体で、実描画は CompositionRenderController 側にある。(b) `RenderPipeline::renderComposition` の **呼び出し箇所はコードベース内に他に存在しない**（grep 確認済み、`ArtifactCompositionEditor` 等から直接呼ばれていない）。よってシグネチャ拡張はリスクが極めて低い。(c) `ArtifactIRenderer::setViewportRect(x, y, w, h, renderTargetW, renderTargetH)` の 5 引数版が既に **`SetViewports + SetScissorRects` を一括で呼ぶ**既存経路。`RenderContext::scissorROI` は画面座標（左上原点、Y ダウン）なのでそのまま渡せる。(d) `ArtifactIRenderer` に新 API を追加せず、CompositionRenderController から直接 `setViewportRect(5引数版)` を呼ぶ構成にした。`ArtifactIRenderer` の `m_viewportWidth/Height` は `setViewportSize` 経由で更新されるが、5 引数版は `setViewportSize(w, h)` を内部で呼ぶので結果として `hostWidth_/hostHeight_` と整合する。
- **実装:** (1) `ArtifactRenderLayerPipeline.ixx` に `Artifact.Render.ROI` を import、`renderComposition` のシグネチャに `const RenderROI& renderROI = RenderROI()` を追加（デフォルト引数で既存スタブの挙動を維持）。(2) cppm 実装で `RenderROI` が空でないとき `ctx->SetScissorRects` を呼び、`impl_->width_/height_` を render target size として渡す。(3) CompositionRenderController の `renderOneFrameImpl` 内で、`comp` あり経路の入口（early return 直後）に `irrScissorApplied` フラグ + `setViewportRect(5引数版)` で scissor を適用。(4) 同じ `renderOneFrameImpl` の `present()` 後の終端で `setViewportRect(hostWidth_, hostHeight_)` を呼んで full-frame に restore。
- **価値または懸念:** (a) スタブへの引数追加は ABI/呼び出し側への波及なし。コンパイル時の整合性のみが問題で、ビルド時にすぐ判明する。(b) restore を `present()` 直後に置くことで、**次のフレーム冒頭で sync ブロックが scissorROI を再計算する前に full-frame に戻っている**ため、IRR 解除直後の最初のフレームで意図せず scissor が残らない。ただし、present() 自体が swapchain をフラッシュする API なので、restore が次フレームの冒頭までに間に合うかは backend（D3D12 / Vulkan）の queue 同期次第。安全策として `setViewportRect` 自体を毎フレーム冒頭で呼ぶ構成も検討したが、既存の `setViewportSize` 経路が毎フレーム呼ばれているはずなので、`setViewportRect(hostWidth_, hostHeight_)` の restore で十分。(c) `interactiveRenderRegionActive_` が inactive になると `renderContext_.scissorROI` が空 `RenderROI()` になり、`if (!renderContext_.scissorROI.isEmpty())` でガードされるため、`setViewportRect` は呼ばれず full-frame が維持される（restore 不要）。
- **次に確認すること:** (a) ビルド許可後に D3D12 / Vulkan 両方で `SetScissorRects` が同一結果になるか（Diligent Engine の抽象化で同等のはずだが、scissor origin が左上原点か左下原点かが API ごとに違う可能性あり）、(b) `present()` 直後の `setViewportRect(hostWidth_, hostHeight_)` が実際に次フレームに適用されるタイミングの検証、(c) Quad レイアウト（4 ペイン）時に `hostWidth_/hostHeight_` が pane 全体で 1 つの値になるが、scissor は pane 単位ではなく画面単位なので、4 ペインすべてに同じ scissor が適用される（decision doc D2 の「P0-3b は全ペイン同期」と整合）。



## 2026-09-22 — P0-3b.1 着手: render path への RenderContext 同期

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` `renderOneFrameImpl`、`Artifact/include/Render/ArtifactRenderContext.ixx`、`docs/planned/DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) `RenderContext::setROI(roi)` は内部で `updateViewportROI() / updateScissorROI()` を連動発火するが、`setPan / setZoom / setViewportSize` はそれぞれ単独で `update*ROI` を呼ばない（変更前と ROI が同期しないように設計）。順序を **setViewportSize → setZoom → setPan → setROI** に統一すれば、`setROI` の 1 回の呼び出しで両 `update*ROI` が完了し、二重計算を避けられる。(b) `RenderContext::canvasSize` は setter が無い公開フィールドなので直接代入。`composition->effectiveCompositionSize()` から取得し、composition がない場合は 1920x1080 にフォールバック。(c) `composition->size()` は存在しない（`ArtifactAbstractComposition` に size() メソッドは無い）。`effectiveCompositionSize()` が正解。(d) `composition->framePosition()` は存在するが `RenderContext::setCurrentFrame(int64_t)` に渡す形が他箇所で見当たらないため省略（default の 0 で害なし、必要になったら P0-3d 段階で追加）。(e) `interactiveRenderRegionActive_` フラグで分岐し、active 時は IRR 矩形を `RenderROI` に変換して渡し、inactive 時は空 `RenderROI()` を渡して full-frame にフォールバック。decision doc D6 の「P0-3b はキャッシュ保持」を満たすため、空 ROI = full-frame として扱い、矩形外は前回フレーム結果を保持するパイプライン挙動を壊さない。(f) `resolutionScale` は `interactiveRenderRegionResolutionScale_`（既存 P0-3a 実装）をそのまま反映し、inactive 時は 1.0f。
- **価値または懸念:** (a) sync ブロックは `renderOneFrameImpl` の冒頭 1 箇所に集約し、毎フレーム renderer の `getZoom / getPan` を 1 回ずつ呼ぶだけ。`composition->effectiveCompositionSize()` も毎フレーム呼ぶが、これは値型の const メソッドなので allocation は無い。ホットパスへの負荷は微小。(b) sync ブロックの呼び出し順を誤ると（例: `setROI` の前に `setPan` が漏れる）scissorROI が古い pan を反映してしまう。コメントで順序の根拠を明記した。(c) RenderContext は `Artifact` namespace 内で定義された値型なので `Artifact::` プレフィックス無しでアクセスできる（cppm は `namespace Artifact { ... }` 内）。(d) P0-3b.1 の段階では矩形はまだレンダラに届かない（RenderPipeline::renderComposition に `RenderROI` 引数が無いため）。`RenderContext` の `viewportROI / scissorROI` が更新されるだけで、それを消費する経路は P0-3b.2 で着手する。
- **次に確認すること:** (a) ビルド許可後に `getter` 経由で `viewportROI / scissorROI` が正しい composition → viewport → scissor の 3 段階で更新されているかログ確認、(b) `setPan` 後に `setROI` を呼ばずに次のフレームに進んだ場合、scissorROI が古いままになるリスクの検証（毎フレーム必ず `setROI` を呼ぶので実際には起きないが、明示的にコメント）、(c) P0-3b.2 で `RenderPipeline::renderComposition` に `RenderROI` 引数を追加した際、`renderContext_.scissorROI` を渡して `IDeviceContext::SetScissor` に転写する経路の検証（D3D12 / Vulkan 共通）。



## 2026-09-22 — P0-3b.0 着手: RenderContext getter 追加のみ

- **関連:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Render/ArtifactRenderContext.ixx`、`docs/planned/DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) decision doc §3 の P0-3b.0 は「`RenderContext` getter を CompositionRenderController に追加、ROI 矩形はまだ流さない」段階。(b) `ArtifactRenderContext.ixx` の `struct RenderContext` は `Artifact` namespace 内にあり、`setMode(RenderMode) / setROI(const RenderROI&) / reset() / updateViewportROI() / updateScissorROI()` を公開する値型。`ArtifactCompositionRenderController.ixx` は既に `import Artifact.Render.IRenderer` を持つので `import Artifact.Render.Context` を 1 行追加するだけで取り込める。(c) `CompositionRenderController::Impl` は `namespace Artifact { ... }` 内にいるため、`Artifact::` プレフィックスを書かずに `RenderContext` / `RenderMode` を直接参照できる。(d) `RenderContext::reset()` は `RenderROI()` に置き換える他、`mode / viewportSize / canvasSize / pan / zoom` も初期化する。(e) decision doc D4 で `setInteractiveRenderRegion` 入口で Preview にダウングレードする計画を立てているが、P0-3b.0 ではまだ矩形を pipeline に流さないので、`RenderMode::Editor` をデフォルトに維持（Editor は Preview に比べて低解像度・高速、リアルタイム編集用）。
- **価値または懸念:** (a) `RenderContext` が値型なので `Impl` 内にデフォルト構築で存在し、`initialize` で `setMode(Editor)` を呼ぶだけで ownership の round-trip が完成する。動的メモリ確保は増えず、ホットパスにも乗らない。(b) `destroy()` で `renderContext_.reset()` を呼ぶのは Composition が再読込されたときに ROI 状態（将来 P0-3b.1 で代入される）が残らないための予防。`reset()` が呼ばれないと `RenderROI()` が空のまま残り、視覚的に「最後に設定した矩形」が幽霊表示になる懸念があったが、今回は矩形を流していないので影響なし。(c) `Artifact::RenderContext` は `namespace Artifact { ... }` 内にいるが、ixx 側で `export namespace Artifact { using namespace ArtifactCore; ... }` の構造なので、ixx 内で `Artifact::RenderContext` と書いても cppm 内で `RenderContext` と書いても同じ意味。cppm 側は前者を `namespace Artifact { ... }` 内なので省略形を採用。
- **次に確認すること:** (a) ビルド許可後に `RenderContext` の `mode / useROICache / sampleCount` が Editor で期待値を取るか（getter 経由で読み出してログ）、(b) `setInteractiveRenderRegion` が呼ばれた後に `renderContext_.setROI(irrROI)` を render path に組み込む段（P0-3b.1）で、和集合ロジック（decision doc D5）が `damageTracker_.combinedDirtyROI()` と conflict しないか、(c) `RenderPipeline::renderComposition` に `RenderROI` 引数を追加する場合の API 互換性（既存呼び出し箇所はすべて更新必要）。



## 2026-09-22 — 次回セッション候補: ROI 設計統合 decision doc

- **関連:** `docs/planned/DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`、`docs/planned/P0_DESIGN_NOTES_2026-09-22.md` §3、`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`、`Artifact/include/Render/ArtifactRenderContext.ixx`、`Artifact/include/Render/ArtifactRenderROI.ixx`、`Artifact/src/Render/ArtifactFrameCache.cppm`（`ProgressiveRenderer`）。
- **着手する内容:** decision doc は作成済み（`docs/planned/DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`）。P0-3b / P0-3c は **ビルド・実機確認の許可後** に decision doc §3 の着手順序に従って進む。
- **decision doc の 8 結論（要約）:**
  - **D1**: 矩形状態の一次所有は `CompositionRenderController::Impl`。PaneState は mirror しない
  - **D2**: P0-3b は全ペイン同期、P1-3 でペイン単位対応
  - **D3**: ProgressiveRenderer callback は引数追加せず、`CompositionRenderController::Impl` 内の `PartialRenderCallback` を新設
  - **D4**: `setInteractiveRenderRegion` 入口で `PreviewQualityPreset` にダウングレード。HUD に通知
  - **D5**: damage ROI と IRR 矩形の **和集合** を effective ROI として tile plan に渡す
  - **D6**: P0-3b は矩形外キャッシュ保持、P0-3c で完全スキップ
  - **D7**: 解像度スライダは composition pixel 基準（既存 P0-3a 実装維持）
  - **D8**: `getRenderContext() const` の getter のみ追加、setter は作らない
- **decision doc 着手前の重要な発見:**
  - `ArtifactRenderContext.ixx` は **孤児モジュール**（どこからも include されていない）
  - `ArtifactIRenderer` には `roi` / `partialRender` API が一切無い
  - `RenderPipeline::renderComposition(ctx, layers, currentFrame, outputRTV)` も `RenderROI` を受け取らない
  - `ArtifactCompositionEditor.cppm` の `PaneState` は 4 ペイン分あるが `renderController_` は単一
- **理由:** decision doc を書いたことで、P0-3b.0（getter 追加のみ）→ P0-3b.1（render path への ROI 注入）→ P0-3b.2（RenderPipeline::renderComposition への RenderROI 引数追加）→ P0-3c（矩形外スキップ）→ P0-3d（ProgressiveRenderer 所有）の段階着手が明確になり、各段でビルド・実機確認が挟まる。
- **価値または懸念:** decision doc が既存コードに整合しすぎて「RenderContext の setter なし」に留めた点は将来 ProgressiveRenderer 統合時に再検討の余地あり。PaneState 拡張（P1-3）との整合は P0-3b では全ペイン同期に留めたため、P1-3 着手時に controller 4 つ化か PaneState 拡張かの選択が必要。
- **次に確認すること:** (a) P0-1 / P0-2 / P0-3a のビルド・実機確認結果、(b) `RenderPipeline::renderComposition` の内部で `IDeviceContext::SetScissor` を使った場合の既存パス（SSAO / Bloom 等）への影響、(c) `RenderModeSettings::useROICache` のデフォルト値と IRR active 時の挙動。



## 2026-09-22 — Dock タブバーの上下配置（下端バリアント）設計メモ

- **関連:** `docs/planned/MILESTONE_NATIVE_DOCK_TAB_ENHANCEMENT_2026-09-22.md`（M5 / M6）、`Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`、`Artifact/include/Widgets/ArtifactDockManager.ixx`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`、`Artifact/src/AppMain.cppm`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- **確認できた事実（静的読み取りのみ、ビルド・実機未確認）:** (a) タブバー配置は全領域で Qt 既定（上端）のまま。`Artifact` 配下に `setTabPosition` は存在しない（grep 0 件）。(b) owner-draw chrome は上端前提で、反転対象は `DockTabBar::paintEvent` の選択タブ contour（`rect.bottom()` を開けて描画、86 / 93 行付近）、`DockTabSurface::paintEvent` の `contentTop`（`tabBar()->geometry().bottom() + 1` 基準、191-193 行付近）、タブ一覧ボタンの `Qt::TopRightCorner`（`createTabSurface` 1776 行付近と corner 参照 2 箇所）。(c) ドックレイアウト保存は `DockLayoutEntry`（dockId / area / tabGroup / geometry / visible / active / pinned / floating）にタブ領域そのものを表す項目が無く、`kDockLayoutDocumentVersion = 1` を `DockLayoutDocument::fromJson` と `restoreLayoutState` が厳密一致で検査し、不一致時は entries を破棄する。(d) Visual Studio 公式ドキュメントの `Set tab layout` は Top / Left / Right のみで、下端配置は提供されていない。(e) タイムライン系ドックはコンポジション単位で生成され（`timeline::<compId>` / `dopesheet::<compId>` / `animation-timeline::<compId>` / `audio-mini::<compId>`、`AppMain.cppm` 4107-4130）、タブ名は解決時点のコンポジション名（同 4088-4106、4166）。(f) 未保存はプロジェクト全体のみで、`UndoManager::hasUnsavedChanges()` は単一の `version_` / `savedVersion_` 比較（`UndoManager.cppm` 714-715、5481）。`UndoCommand` の基底（`UndoManager.ixx` 72-93）にコンポジション scope のアクセサは無く、`compositionId_` は各サブクラスの private メンバに散在する。(g) `NativeDockSurface` に登録済みドックのタイトルを後から更新する公開 API は無く、`titles_` はタブ文字列・浮動ウィンドウタイトル・タブ一覧・保存／復元から参照される。
- **価値または懸念:** (a) 下端配置を `Bottom` ドック領域で使うと、ウィンドウ下端の領域タブとステータス行が近接して混同しやすい。特別扱い（下端配置を禁止する、余白や区切りを足す、のいずれか）を決める必要がある（ユーザー判断待ち）。(b) 保存フィールドの追加は version 据え置きの任意フィールド追加が安全だが、`restoreLayoutState` の配列形式フォールバックと旧ビルドとの相互運用も含めて方針を決める必要がある（ユーザー判断待ち）。(c) 浮動タブグループは安定したグループ ID を持たないため、浮動グループ単位の配置保存は M3 の ID 整備が前提。(d) タイムラインのコンポジションタブへの未保存表示（M6）は、`AGENTS.md` / `docs/design/composition-viewport/README.md` / `MILESTONE_DOCK_ENHANCEMENT_PACK_2026-09-13.md` の「未保存マークを Dock タブに出さない」規則に触れる。例外を明示するか、当該タブを編集コンテキストタブへ再分類するかの決定が必要（ユーザー判断待ち）。(e) 同表示の粒度は、既存のプロジェクト全体 dirty を使う暫定案と、コンポジション単位 dirty を新設する本来案がある。暫定案は未編集のコンポジションのタブにも印が出るため、意味を誤解させない文言が要る。本来案は Undo コアの基底インターフェース追加を伴う（ユーザー判断待ち）。
- **次に確認すること:** (a) 実機で `QTabWidget::South` のタブ形状と owner-draw contour／外枠が一致するか、(b) 下端配置時の D&D 挿入予告と確定位置が一致するか、(c) 複数行タブの方式（自前レイアウトへ置き換えるか、行数分の `QTabBar` を並べるか）、(d) `titles_` を更新する setter を入れたとき、タブ文字列・浮動ウィンドウタイトル・タブ一覧・保存／復元の全経路で名前が一致するか、(e) コンポジション改名時にタイムラインタブの名前と未保存印が両方追随するか。



## 2026-09-21 — Text Animator の追加機構は実装済み。残るのは Timeline 左ペインのポリシー例外と個別追加導線

- **関連:** `ArtifactCore/include/Text/TextAnimator.ixx`、`Artifact/src/Layer/ArtifactTextLayer.cppm`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`（4974 行付近の Text Animator サブメニュー）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`（1148 行付近）、`Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorTextAnimatorColor.cppm`（Animator count エディタ）、`Artifact/src/Layer/ArtifactAbstractLayerUtilities.cppm`（`computeTimelineHiddenLayerPropertyGroup`）、`docs/done/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`。
- **確認できた事実（静的読み取り）:**
  - `TextAnimatorEngine`（Range/Wiggly/Expression セレクター、`AnimatorSelectorSet` のスタック適用）は Core に実装済み。`ArtifactTextLayer` は `addAnimator()` / `setAnimatorCount()`（最大16）/ プリセット7種 / Undo スナップショットを持ち、`perGlyphMode_` で `resolvedTextAnimatorStackAtTime()` → `applyAnimatorSets()` をタイムライン時刻で評価する。
  - 追加導線は3箇所: Inspector の `text.animatorCount` エディタ（Add ボタン＋プリセットメニュー）、Timeline 左ペイン右クリックの `Text Animator` サブメニュー（プリセット7種＋Clear）、VP 右クリックの `Add Text Animator`。`text.animators.N.*` は `getLayerPropertyGroups()` 経由で Inspector とキーフレームモデルに接続済み。
  - 一方で `computeTimelineHiddenLayerPropertyGroup` は Transform 以外をすべて非表示にするため、Timeline 左ペインでは Animator グループが見えない。2026-08 の分析が「timeline 未配線」としたのはこの表示ポリシーに起因する。`collectAnimatablePropertyRefs()` 自体は収集するが、左ペインの表示で遮られる。
  - `docs/spec/SPEC_TEXT_TOOL_REQUIREMENTS_2026-07-31.md` 5.2 は Text Animator を「未実装」のままだった（今回、実コード照合で更新）。
- **価値または懸念:** 「追加できない」ではなく「追加後に Timeline でキーフレームが見えない」「個別プロパティ追加がない」が実質のギャップ。Timeline 左ペインのポリシー変更は AGENTS.md の例外手続き（明示要求または設計レビュー）が必要で、今回の依頼が例外承認に相当するかはユーザー確認が要る。
- **対応:** 現状照合と残作業を `docs/planned/MILESTONE_TEXT_ANIMATOR_ADD_WORKFLOW_2026-09-21.md` に計画として記録した。
- **次に確認すること:** (a) Timeline 露出の例外承認をユーザーから得るか、(b) P0 の runtime 受入（ビルド許可後）、(c) リッチテキスト境界（`perGlyphMode_ = !isRichText`）の実機挙動。



## 2026-09-21 — Projected frame の SPEC は実装より古く、未完項目はガイド線だけではなかった

- **関連:** `docs/spec/SPEC_3D_FRAME_GIZMO_REQUIREMENTS_2026-07-31.md`（7章）、`docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`（Phase 1）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- **確認できた事実（静的読み取り）:**
  - SPEC 7.2 の「未実装」リストは大半が実装済みだった。コーナー／エッジのヒットテスト（`hitTestProjectedFrameCorner`、`Scale_T/B/L/R`）、フレーム内移動（`hitTestProjectedFrameInterior`）、リサイズバッジ（`projectedFrameWidthBadgeRect_` / `projectedFrameHeightBadgeRect_`）、ドラッグHUD（W/H・X/Y/Z・dX/dY/dZ・S・倍率・RZ/dR・操作種別）、スナップ（`snapProjectedFramePointer` + `ProjectedFrameSnapCache`）、Undo（`beginGizmoUndoSnapshot` → `GizmoTransformUndoCommand`）、ダブルクリックリセット（`resetProjectedFrameHandleAt`）、最小サイズクランプ、near/far クリップと部分可視の減衰（`projectedLayerFrameCorners`）、数値入力（`beginFrameSizeBadgeInput` + editor の `modalTransformNumericInput_` で `w`/`h` 入力→Enter確定）、3D軸ギズモとの優先順位、モード別フィルタ（`projectedFrameHandleEnabled`）、複数選択の包絡フレーム＋包絡内の各レイヤー細枠（`projectedSelectionFrameBounds`）はすべて現行コードに存在する。
  - 「回転ハンドル（Z軸回転）」だけは設計変更で置き換えられている。`showProjectedRotationHandle = !projectedFrame` として projected frame に回転ハンドルを描かず、`hitTestProjectedFrameCorner` もコーナー／エッジしか返さない。Z回転は3D回転リングと既存HUDが担う。
  - 単一レイヤーのリサイズ固定点は SPEC の「対角コーナー固定」ではなく `projectedFrameCorrectedLocalPosition_` によるアンカー固定（AE準拠）。対角／対辺固定は複数選択の `projectedFrameScaleFixedPoint` 側の挙動。
  - Move モードでは `showProjectedScaleHandles` によりフレームのスケールハンドルを出さない（SPEC 12.4 の「Move=コーナーのみ」とは差がある）。
  - `Viewport/ProjectedFrame/ShowDiagonals` は `ArtifactCompositionRenderOverlay.cppm` の関数内 `static const bool` として `ArtifactCore::LayeredConfigStore` から1回だけ読まれ、既定は無効で UI からは切り替えられない。コードコメントには「枠内の X がドラッグ可能なハンドルと誤認される」ため無効のままにする旨が書かれている。SPEC 13.1 の対角線はこの常設表示であり、今回追加したドラッグ中だけのガイドとは別物。
- **対応（今回の実装）:** リサイズ中のガイドを追加した。`projectedFrameGuidePoints`（コーナーは対角、辺は対辺中点を固定点として返す）を追加し、ドラッグ開始時に `projectedFrameScaleStartHandlePoint_` へハンドル投影位置を記録、`drawViewportInteractionOverlay` で固定点と駆動点を結ぶ線・固定点マーク・開始位置マークをビューポート画素空間に描く。包絡フレーム（複数選択）は対象外。新しい signal/slot、QtCSS、QImage、外部行列の変更は追加していない。
- **価値または懸念:** SPEC を「実装予定リスト」として読むと、既に終わっている項目を再実装する危険がある。今回は差分を SPEC 7章と MILESTONE Phase 1 へ反映したので、次に着手すべきは `ArtifactProjectedFrameGizmo` の分離と runtime 受入（ビルド・実機）になる。`ShowDiagonals` は起動時に固定されるため、UI から切り替えるには `static` を外す必要がある（未検証・未着手）。
- **次に確認すること:** ビルド許可後に (a) 単一レイヤーのコーナー／エッジ／Shift・Ctrl 併用で、ガイド線・固定点マーク・開始位置マークが投影フレームへ追従すること、(b) Move モードと包絡フレームではガイドが出ないこと、(c) 数値入力（バッジクリック→`w`/`h`→Enter→Undo）の往復、(d) D3D12/Vulkan 双方での描画。実機操作が必要。今回の変更は静的確認のみでビルド未検証。段取りと残項目は `docs/planned/MILESTONE_PROJECTED_FRAME_GIZMO_2026-09-21.md` に予定として記録した。



## 2026-09-21 — Position/UV AOV 追加と既存 Velocity CPU フォールバックの疑義

- **関連:** `ArtifactCore/include/Channel/Channel.ixx`(PositionX/Y/Z・U/V 追加)、`ArtifactCore/src/Graphics/MeshRenderer.cppm`(PS mode 9/10)、`Artifact/src/Render/ArtifactIRenderer.cppm`(only-pass・readback)、`Artifact/include/Render/ArtifactRenderLayerPipeline.ixx`+`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`(position_/uv_ ターゲット)、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`(要求・描画・表示・CPUフォールバック)、`Artifact/src/Render/ArtifactRenderQueueService.cppm`(キー・既定)、`Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm`(チェック)、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`(表示メニュー)、`docs/analysis/GAP_AE_NUKE_2026-08-01.md`(追補)
- **確認できた事実（静的読み取り）:** Position=ワールド位置raw・UV=頂点UV raw(マテリアルuv transformなし)を RGBA16F/32F ターゲットへ描画し、readback は無変換で書込む(Normal/Velocity の 2.0/-1.0 デコードなし)。VP合成表示は displayComposite mode 1(raw RGB)、単体表示は displayComponent。2Dレイヤーは only-pass が `is3D()` で弾くため対象外。`readbackChannelToImage` のグレー抽出に Position/UV を追加したため CPU フォールバックの単体表示も成立する。
- **懸念（未検証）:** 既存 `composeVelocity` は `readChannel(VelocityX/Y)` の先頭バイト(R)を両方に使うが、VelocityX/Y はグレー抽出対象外のため両画像とも velocity ターゲットの RGBA 全体で、先頭バイトはどちらも X 成分のはず。CPU フォールバックの Velocity 合成表示は G に X が入っている可能性がある。今回 U/V・Position はグレー抽出へ入れたため同問題なし。実機の CPU フォールバック表示での確認が必要。
- **次に確認:** ビルド・実機 (3Dシーンで Position/UV の VP 表示・EXR 出力・RenderQueue 既定キー)。`check_module_hygiene` ターゲット。Velocity フォールバック表示の実機確認。



## 2026-09-21 — ArtifactHashMap iteratorは衝突チェーンの巡回確認が必要

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`Physics.System` の標準連想コンテナ移行。
- **確認できた事実:** `ArtifactHashMap::iterator::operator++()` は現在のnodeの `next` を確認せず、直ちに次bucketへ進む。bucket内に複数nodeがある場合、range-forとiterator走査から2件目以降が見えない可能性がある。
- **対応:** 今回のphysics registryには採用せず、キー順を維持する `NamedVector` 基盤の内部registryを使用した。
- **価値または懸念:** `find()` / `operator[]` はbucket chainを走査するため、個別参照と全件走査で見える要素数が異なる恐れがある。未検証のため、既存利用箇所を一括変更しない。
- **次に確認すること:** collisionを意図的に発生させる小さなcontainer testを用意し、iterator、rehash、erase後の走査を確認してから共通mapとして採用する。
