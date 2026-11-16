# Shader Graph R2 — Runtime / Backend / Performance Parity

**最終更新:** 2026-11-16
**ステータス:** 契約テスト実行済み。テスト側誤り 3 件 + 実バグ 4 件を修正。Vulkan GPU capture source を追加したが未実行。残り12件は既知ギャップの expected failure

## 目的

`MILESTONE_SHADER_GRAPH_RND_2026-11-13.md` の R2（runtime/backend/性能評価）を、R0/R1 の静的監査から独立した作業単位として切り出す。R0/R1 の未検証項目を runtime 証拠に変えるのが目的。

## 未検証項目（R&D 文書より）

1. **D3D12 / Vulkan paired capture**: shader compile、Preview/final 画像、device loss/compile failure fallback、cache hit/miss。
2. **CPU compile 時間 / GPU 時間 / PSO 数・再生成 / VRAM / steady-state allocation**。
3. **render hot path に JSON parse / HLSL 再生成 / shader/PSO/resource 生成が入らないこと**の確認。
4. **compile failure 後の rollback**: `setMaterialGraphJson()` は JSON parse 失敗時に false を返し旧 graph を維持するが、`compileMaterialGraph()` が `ok=false` の場合は magenta fallback block を保存して PSO 再生成する。rollback 経路の実動作は未確認。
5. **PSO cache の実 hit/miss**: `makeMeshPipelineCacheName()` が atomic serial を含む名前を毎作成時に生成し、`hashHex` を名前へ渡していない。Diligent `IPipelineStateCache` の内部 compile key の挙動は未確認。

## 具体的なホットパス疑義（R&D 文書より）

`ArtifactIRenderer.cpp` が renderer 設定のたびに `static_cast<std::string>(material.materialGraphJson())` を渡す。`Material::materialGraphJson()` は `UniString` を値返却し、`UniString` copy constructor は `new Impl(other)` を行う。続く `UniString::operator std::string()` は `QString::toUtf8()` と `std::string` 構築を行う。同一 JSON の比較で早期 return しても、前段の値コピー・変換は残る。

**提案**: renderer へ値返却を避ける参照/view または revision 付き境界を設計し、同一 JSON の steady-state allocation をゼロ計測で確認する。graph 変更時は cold path として別計測。

### 確認済みソース証拠（2026-11-15、読解のみ）

- `Artifact/src/Render/ArtifactIRenderer.cpp` L2836: `renderer->setMaterialGraphJson(static_cast<std::string>(material.materialGraphJson()));`
- `ArtifactCore/src/Material/Material.cpp` L813-814: `UniString Material::materialGraphJson() const { return impl_->materialGraphJson_; }`（値返却）
- `ArtifactCore/src/Utils/UniString.cpp`: `UniString::UniString(const UniString& other) : impl_(new Impl(other))`（`new Impl` アロケーション）
- `ArtifactCore/src/Graphics/MeshRenderer.cpp` L6701-6703: `if (pImpl_->materialGraphJson_ == json) return ...;`（早期 return は compare 後）

つまり早期 return は `std::string` 生成後に行われるため、steady-state での allocation は残る。計測で確認するまで「疑義」扱い。

## 契約テストの状態

- `tests/ArtifactCore/CMakeLists.txt` に `ArtifactCoreShaderGraphContractTest` を登録済み（`ArtifactCoreMaterialContractTest` の隣、`if(ARTIFACT_BUILD_TESTS)` 内）。
- `tests/ArtifactCore/ShaderGraphContractTest.cpp` L353 の `toJson()` 呼び出しを宣言（`toJson(const std::vector<MaterialGraphLayout>&)`）に合わせて `toJson({})` へ修正。
- `out/build/x64-Debug-Agent`（preset `x64-Debug-Agent`、`ARTIFACT_BUILD_TESTS=ON`）で 37 case を実行。
  - 初回: **PASS 17 / FAIL 20**。
  - 失敗 20 件を分類し、テスト側の誤り 3 件を修正 → **PASS 20 / FAIL 17**。
  - その後、実際のバグ 4 件（matFloatLit / FailedLoad / SanitizedNodeIdentifiers / ColorRamp 2 件）を修正 → **PASS 25 / FAIL 12**（残り 12 件は既知ギャップの expected failure）。
- GPU capture 用の source case（`ArtifactModel3DAndLightLayerImageTest` / Vulkan RT acceptance target）は未ビルド・未実行。

### 実行時の環境要件（再現用）

- link 時に `rc.exe` が必要: `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64` を PATH へ追加しないと manifest 埋め込みで `RC Pass 1 ... failed (exit code 0)` になる。
- 実行時に vcpkg の debug DLL（`Qt6Cored.dll`, `tbb12_debug.dll`）が必要: `C:\vcpkg\installed\x64-windows\debug\bin` と `C:\vcpkg\installed\x64-windows\bin` を PATH へ追加しないと exit code `-1073741515`（DLL 未解決）で即終了する。
- `QT_QPA_PLATFORM=offscreen` を設定（GUI を開かない）。

## 契約テスト失敗の分類（初回 20 件）

分類は「テスト側の誤り（修正対象）」「実際のバグ（実装欠陥）」「現行実装の既知ギャップ（expected failure）」の 3 種。実装コードは一切変更していない。

| # | テスト | 分類 | 現行挙動 / 根拠 | 今回の扱い |
|---|---|---|---|---|
| 1 | `CanonicalSerializationIgnoresNodeAndLinkInsertionOrder` | 既知ギャップ | `toJson()` は `nodes`/`links` を格納順に出力。挿入順が違う等価 graph で JSON が一致しない | EF 登録 |
| 2 | `CompileHashTracksHelperAndBlockSource` | テスト側の誤り | `matEmitProceduralHelpers()` は graph 内容に関係なく helper 全文を常に出力するため `helperHlsl` は一致する。hash は helper+block の両方を畳み込む | 修正（下記） |
| 3 | `ColorRampInterpolationUsesExplicitDomainAndEaseMath` | **実際のバグ → 修正済み** | `matFloatLit()` を整数値に `.0` を補うよう修正。ColorNode の直接 `{}`f も matFloatLit 経由に変更 | 修正済み |
| 4 | `ColorRampFixtureClampsStopPositionsBeforeInterpolation` | **実際のバグ → 修正済み** | 同上 | 修正済み |
| 5 | `AllLegacySocketConversionsHaveExplicitExpressions` | テスト側の誤り | `matConvert()` の Vector3/Vector4→Vector2 は `v + ".xy"`（すでに float2 式）で、`float2(...)` ラップは出力しない | 修正（下記） |
| 6 | `MultipleMaterialOutputsRequireExplicitSelection` | 既知ギャップ | 複数 MaterialOutput は `ok=true` のまま storage 順先頭を採用。警告文も `Surface unconnected` 警告が先に入ると設定されない | EF 登録 |
| 7 | `FailedLoadPreservesExistingGraph` | **実際のバグ → 修正済み** | `kind` 不一致・`nodes` 欠落/非配列を拒否、skipped link 時は graph を置換しないよう fromJson を修正 | 修正済み |
| 8 | `TypeMismatchLinkDoesNotReportSuccessfulDocumentLoad` | 既知ギャップ | 不正 link を skip して `ok=true` + warning。既存 graph も置換される | EF 登録 |
| 9 | `DuplicateInputLinksAreRejectedWithoutReplacingExistingGraph` | 既知ギャップ | 同一 input への複数 link を格納し、解決時に storage 順先頭を採用 | EF 登録 |
| 10 | `FiniteJsonNumberOutsideFloatRangeIsDiagnosed` | 既知ギャップ | `1e100` を double→float 変換で inf にし、診断なし | EF 登録 |
| 11 | `OutOfRangeNodeEnumIsDiagnosed` | 既知ギャップ | `op:17` を範囲検査せず `static_cast` で正規化 | EF 登録 |
| 12 | `VoronoiFixtureConnectsCellColorAndDistance` | テスト側の誤り | `sqrt` は helper 内 `dist = sqrt(dist)` で実行され、block は `artMatGraph_voronoi(voronoi_p, voronoi_Distance, voronoi_cell)` として out 引数で渡すだけ | 修正（下記） |
| 13 | `OutOfRangeGraphBaseColorAlphaIsDiagnosed` | 既知ギャップ | RGB の alpha literal は [0,1] 検査なし（Principled Alpha のみ `saturate`） | EF 登録 |
| 14 | `EmptyColorRampStopsAreNotSilentlyIgnored` | 既知ギャップ | 空 stops を受理し、lowering で黒 fallback | EF 登録 |
| 15 | `ExcessColorRampStopsAreNotSilentlyTruncated` | 既知ギャップ | stop 数上限なし（65 stops を受理） | EF 登録 |
| 16 | `LinkRejectsPinsOwnedByAnotherGraph` | 既知ギャップ | `NodeGraph::link()` は owner graph を検査しない | EF 登録 |
| 17 | `LinkRejectsIncompatiblePinTypes` | 既知ギャップ | `link()` は PinType 整合性を検査しない（暗黙変換で吸収） | EF 登録 |
| 18 | `LinkRejectsMultipleWritersForOneInput` | 既知ギャップ | `link()` は single-writer を強制しない（2 本保持） | EF 登録 |
| 19 | `SanitizedNodeIdentifiersDoNotCollide` | **実際のバグ → 修正済み** | `sanitizeShaderId` で非識別子文字（`-` 等）を `_XX` にエスケープし、`base-color` と `base_color` が衝突しないように修正 | 修正済み |
| 20 | `GraphTraversalHandlesDeepChainsWithoutStackFailure` | 既知ギャップ | 深さ上限・診断が未定義（2048 chain をそのまま `ok=true` で compile）。`evaluation_manifest.json` の `supportedLimits` も全て `null` | EF 登録 |

集計（修正後）: テスト側の誤り 3 件（#2 / #5 / #12、修正済み）、実際のバグ 4 件（#3 / #4 / #7 / #19、全て修正済み）、既知ギャップ 12 件（EF 登録）。修正後の契約テスト結果: 37 件中 PASS 25 / FAIL 12。#3 / #4 はテストの期待（`1.0f`）が正しく、実装が不正 HLSL を出しているためテストは変更せず EF とした。

### テスト側の誤り（修正済み 3 件）

1. `CompileHashTracksHelperAndBlockSource`: `helperHlsl` が graph ごとに変わると仮定していた。helper は常に全文出力なので、`EXPECT_FALSE(helperHlsl.empty())` と `EXPECT_EQ(procedural.helperHlsl, constant.helperHlsl)` に置換し、hash 不一致の検証は維持。
2. `AllLegacySocketConversionsHaveExplicitExpressions`: Vector→Vector2 の narrowed 変換に `float2(...)` ラップを要求していた。実出力 `geometry_Position.xy` / `color_Color.xy` に合わせ、他 8 断片の検証は維持。
3. `VoronoiFixtureConnectsCellColorAndDistance`: `sqrt` が block 内に出ると仮定していた。実際は helper 内 `dist = sqrt(dist)` で、block は `artMatGraph_voronoi(voronoi_p, voronoi_Distance, voronoi_cell)` として out 引数で渡す。block の呼び出し検証 + `helperHlsl` の `dist = sqrt(dist)` 検証に置換。

## 修正済み: `matFloatLit()` が不正な integral literal を出力する（2026-11-15 修正）

`ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp` L527-533:

```cpp
std::string matFloatLit(float v) { ... return std::format("{}f", v); }
```

`std::format("{}", 1.0f)` は最短表現 `"1"` を返すため `"1f"` になる。`0.1f` のような小数は `"0.1f"` で問題ないが、整数値（`0.0f` / `1.0f`）は小数点が落ちる。

**実測（Windows SDK 10.0.26100.0）**:

```hlsl
float4 ramp_ramp = float4(0.1f,0.2f,0.3f,1f);
float t = saturate((0.5f - 0f) / 1f);
```

- `dxc.exe -T ps_6_0 -E PSMain` → `error: invalid digit 'f' in decimal constant` / `in octal constant`（exit -2147467259）
- `fxc.exe /T ps_5_0 /E PSMain` → `error X3000: syntax error: unexpected token 'f'`（exit 1）

**影響範囲**: `matFloatLit()` は Value node（`matFloatLit(n->value)`）、RGB node の `matFloat4Lit()`（alpha 既定 `1.0f`）、ColorRamp の stop 位置/色、span（`max(p1.p - p0.p, 1e-5f)`）で使われる。既定の RGB node は `a = 1.0f` なので、**RGB/Value を含む material graph の block はほぼ常に `1f`/`0f` を含む**。同様の `"{}f"` 出力が L76（legacy node の `generateHLSL()`）にもある。

**実行証拠の状態**: ソース上は `matFloatLit()` の修正がArtifactCore `main`へ入っている。上記のDXC/FXC failureは修正前の文字列に対する結果であり、現在のmaterial graphで同じ失敗が起きるとは推定しない。修正後sourceを製品PSO生成まで通したGPU/runtime証拠はまだ無い。

**修正状況（2026-11-16）**: ArtifactCore `main` の `c34524e3 fix_ArtifactCore_shader_node_emission` で、指数表記を含め小数点のない最短表現へ `.0` を加えてから `f` を付ける修正が入った。legacy `ColorNode::generateHLSL()` も同じ literal helper を使う。#3/#4 の契約テスト実行記録はこの修正後のもの。今回追加したGPU capture sourceはRampではなく UV→Multiply→Add→Roughness の経路を対象にしており、この修正のPSO適用確認にはならない。

## Vulkan capture source（2026-11-16、未実行）

親repoの `tests/ArtifactCore/RayTracingAcceptanceTest.cpp` にある `VulkanMeshRendererAcceptanceTest` へ、既存のheadless Vulkan/RGBA32F harnessを再利用した capture case を追加した。従来のRamp比較に加えて、legacy `TexCoord.UV → Math(Multiply) → Math(Add) → Principled.Roughness` fixtureを `setMaterialGraphJson()` で評価し、同じmesh UV上でR&D typed projectionのHLSL bodyを `setMaterialGraphBlock()` へ注入した候補と赤channelを比較する。五つのUV0.x sample、入力fixture/candidate hash、生RGBA32F capture hashをreportへ記録する。さらに同じcaseへBaseColor Vector4 Add / Subtract / Multiply / positive-floor Divideを追加し、legacy fixtureのMath opを切り替えたCore graphとtyped HLSL候補をRGBA32Fの全4 lane・5 sampleで比較するreport sourceを加えた。

BaseColor数値比較の絶対許容差は `1e-5` linear RGBA32F channel unitsとcapture前に固定し、manifest・test source・capture reportで同じ値を記録する。根拠は同一Vulkan draw/readback route上でのoperation-specific binary32 goldenとの数値回帰確認であり、画像品質や別backend間の許容差には流用しない。analyzerは各比較の閾値超過数を出すが合否判断はせず、C++ source assertionがそのnumeric gateを担う。global image-noise tolerance protocolとVisual/backend acceptanceは別途未実施である。

この経路は現行Core legacy loweringとR&D候補の選択値をGPU上で並べるための試験sourceであり、製品UI・ArtifactIRenderer adapter・project save/reloadを通らない。RoughnessではBaseColor AOVの赤channelへscalar値を載せるためroughness値の応答だけを検査し、BRDF見た目、roughness clampの出力、D3D12 parity、compile failure recovery、性能は評価しない。BaseColor arithmeticはcandidate goldenおよびlegacy captureとの一致をsource上で検査するが、実測値はまだない。C++ sourceは未ビルド・未実行で、capture recordsも未生成。実行には明示許可が必要。

`tools/shader_graph_rd/analyze_shader_graph_rgba32f.py --capture-report <report>` は、BaseColor capture recordsがあればrawファイルのhashを検査し、sample座標からRGBAを再読込してreport内の数値と照合したうえで、golden/legacy/projection間のlane別絶対誤差と事前固定許容差の超過数を再計算する。これはreportの内部整合性確認であり、Core採用判定はしない。新しい解析sourceも未実行。

## Legacy Math projection拡張（2026-11-16、compiler-only）

Selected BaseColor Vector4 and Roughness Float adapters now map unclamped Add/Subtract/Multiply/Divide to typed operations. Divide preserves Core's positive epsilon denominator floor. BaseColor Subtract/Multiply/Divide each compile through DXC/HLSL and glslang/GLSL for Vulkan 1.2, with exact four-lane binary32 output matches. Roughness Subtract and negative-denominator Divide match the scalar oracle at six UV samples in both frontends (24 outputs). The expanded compiler summary reports 10/10 cases passed and the manifest validator passes.

これはoffline compiler evidenceで、GPU capture、Core loader、production adapter、Diligent binding、project save/reloadは未検証。次のruntime gateは前節のVulkan capture sourceであり、Divideのフルruntime挙動を網羅するものではない。

## 非対象

- ArtifactCore / Artifact / ArtifactWidgets の実装変更（分類・記録のみ。テスト側の誤り 3 件以外は未変更）
- GPU capture（D3D12/Vulkan）の paired run
- hot-path allocation の steady-state 計測

## 次の作業

1. 追加した Vulkan capture source を許可後にビルド・実行し、Coreとtyped projectionの5 sample値およびraw capture reportを確認。
2. 実装ギャップ12件の優先順位付け（load検証 / `link()` 事前条件 / canonical serialization）。
3. production layer smokeを経て、D3D12/Vulkan paired runとsave/reload評価へ進む。
4. hot-path allocation の steady-state 計測。

## 関連

- `docs/planned/MILESTONE_SHADER_GRAPH_RND_2026-11-13.md`
- `docs/planned/MILESTONE_EFFECT_PRODUCT_CAPTURE_PARITY_HARNESS_2026-11-15.md`
- `Artifact/src/Render/ArtifactIRenderer.cpp`（`setMaterialGraphJson` 接続）
- `ArtifactCore/include/Material/Material.ixx`
- `ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx`
