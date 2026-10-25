# レイヤーマスク独立テスト

**最終更新:** 2026-10-25
**ステータス:** In Progress

## 進捗

- 既存の `ArtifactMaskVisualTest` に、空mask／disabled maskの全不透明契約、open pathの無視、copy-edit isolation、pathの追加／置換／削除／clearと範囲外indexのno-op、`applyToImage()` のRGBA各チャンネル適用とdisabled時no-opを追加した。既存の合成演算ケースは重複追加していない。
- `ARTIFACT_ENABLE_LAYER_MASK_TEST=ON` で `ARTIFACT_BUILD_TESTS` とGoogleTestを有効にせずCPUテストを選択登録できるようにした。CTestは独立processとして起動し、並列実行を避ける。
- このCPU executableは既存 `ArtifactAppRuntime` とOpenCVへリンクするため、アプリ本体processは起動しないが、Artifact runtimeのビルド依存は残る。
- `ArtifactProjectRoundTripTest.LayerMaskStateAndPathRoundTrip` に、実Project save/loadを通してmask有効・ロック・色、path geometry／全属性、アニメーションkeyframeの復元を検査するケースを追加した。既存のProject RoundTrip executable内であり、専用mask targetではない。
- `ARTIFACT_ENABLE_COMPOSITION_LAYER_MASK_GPU_TEST=ON` で選択可能な `ArtifactCompositionLayerMaskGpuIntegrationTest` を専用CTestとして登録した。実CompositionRenderControllerの提示frameでSolid layerのmask有効／無効／反転を描画し、path内外の画素を検査する。3状態のPNGも既存capture出力先へ保存する。GPU依存で、未ビルド・未実行。
- `ArtifactCompositionLayerMaskTest` を `ARTIFACT_BUILD_COMPOSITION_LAYER_MASK_TEST=ON` で選択できる独立GUI/MCP executableとして追加した。MCPの `mask.state`／`mask.setMode`／`mask.verify`／`mask.capture` で状態取得、mask切替、提示画素検査、非アクティブ窓captureとPNG・診断JSON保存を行う。ビルド・起動・MCP runtimeは未確認。
- ソース変更のみ。ビルド・実行は未実施のため、追加assertの実行結果は未確認。
- GPU統合ケースは既存のComposition統合test executableを使い、独立GUI/MCP executableは別途選択できる。

## 目的

FBXアニメーション独立テストと同じく、製品アプリ本体とは別に選択・起動できるテストを用意し、レイヤーマスクの演算、レイヤー状態への適用、保存／復元、Composition描画までを切り分けて確認する。GUI確認が必要なケースは独立テスト窓とMCP操作・非アクティブ窓captureを使い、再現手順・診断JSON・画像をテスト出力へ残す。

## 既存資産と責務

- `tests/mask_visual/MaskVisualTest.cpp` は `LayerMask::compositeAlphaMask()` のCPUアルファ合成契約をPNG出力付きで確認する既存テスト。引き続き低レベルの演算回帰に使い、重複した演算ケースをGUIテストへ移さない。
- `tests/ArtifactTestMcpServer.h` と `tests/ArtifactTestWindowCapture.h` はFBX Viewer等の独立GUIテストで共有されている。新規テストもこの共通helperを使う。
- `docs/planned/MILESTONE_COMPOSITION_LAYER_SURFACE_CACHE_TEST_2026-10-16.md` はレイヤーsurface cacheの検査が対象であり、マスク機能全体の独立テストを代替しない。共有可能なfixture手法のみ参照する。
- 実装前に `LayerMask`、`MaskPath`、`ArtifactAbstractLayer` のmask API、layer serialization、既存Composition描画でのmask適用・fallback経路を読み、fixtureが実際に通る経路を特定する。

## 独立GUI harnessの選択

- CMake opt-in: `ARTIFACT_BUILD_COMPOSITION_LAYER_MASK_TEST=ON`（Windows GPU Composition viewportが必要）。
- MCP mode: `ArtifactCompositionLayerMaskTest.exe --mcp-stdio`。
- MCP tools: `mask.state`, `mask.setMode`（`enabled` / `disabled` / `inverted`）, `mask.verify`, `mask.capture`。
- `mask.verify` は提示GPU frameのpath内外画素と同条件の連続2 frame一致を返す。`mask.capture` はアクティブ化せずwindow captureを行い、PNGと診断JSONを `ARTIFACT_COMPOSITION_LAYER_MASK_CAPTURE_DIR` またはOS temp配下へ保存する。
- 追加したが、configure/build、MCP呼び出し、window captureのruntime受入は未確認。

## 対象範囲

1. **演算契約（CPU）**: 現行 `MaskMode` のAdd/Subtract/Intersect/Difference、opacity、inversion、feather、expansion、enabled状態、空maskと複数pathの境界値を決定的な数値assertと画像出力で確認する。既存 `ArtifactMaskVisualTest` のケースを棚卸しし、不足分だけを足す。
2. **レイヤー状態・永続化**: CPU suiteでmaskの追加／更新／削除、index境界、copy/edit isolationを検査する。既存Project RoundTrip suiteでは実save/load経路でpath属性・頂点・keyframe・mask状態を検査する。後者のruntime結果は未確認。
3. **実描画（Composition GPU）**: Solid layer fixtureにmaskを適用し、mask有効／無効／反転の提示frameを同一backend・同一条件で画素比較する（test追加済み、未実行）。
4. **独立操作**: CPU契約suiteは `ARTIFACT_ENABLE_LAYER_MASK_TEST` と単独executable、GUIは `ARTIFACT_BUILD_COMPOSITION_LAYER_MASK_TEST` と独立executableで選択できる。GUI executableはMCP server／window capture helperを使い、fixture準備、case実行、state・診断取得、PNG captureを行う（実行未確認）。

## 受け入れシナリオ

- 初期maskの内側・外側・境界付近の期待alphaと、画像の寸法／形式を確認する。
- 合成モードを複数pathで適用し、重なり領域と非重なり領域の期待値を検査する。
- enabled切替／inversion／opacity／feather／expansionの変更が結果へ反映され、無効化やpath削除後に古い結果が残らない。
- レイヤーへ設定したmaskを保存して再読込し、path geometry・keyframeとmask設定を比較する（テスト追加済み、未実行）。
- Composition GPU fixtureでmask有効／無効／反転の内外画素が期待どおりになり、capture画像を保存する（test追加済み、実行未確認）。
- 独立GUI fixtureでMCPからmask状態を変更し、同条件で提示画素を再検査して診断JSONを保存する（executable追加済み、runtime未確認）。
- 失敗時にcase名、期待値、観測値、layer識別情報、render path、capture pathをJSONとログに残す。
- それぞれのCPU suiteとGUI executableを個別に選択でき、通常アプリ起動・データを変更しない。

## 制約・確認方法

- 変更は親リポジトリの `tests/`、root CMake設定、`docs/` に限定する。`Artifact` 等の子リポジトリは編集しない。
- fixtureと診断はテスト専用とし、新しい製品UI、公開signal/slot、mask編集仕様、render fallbackは導入しない。
- 描画確認は実際の製品Composition経路を使う。CPU合成結果だけでGPU／Composition表示まで合格とみなさない。
- GUIが見られない環境ではCPU契約・永続化suiteの結果とGUI受け入れ未確認を分けて報告する。
- ビルド、CMake configure、テスト起動はユーザーが明示するまで実行しない。

## 既知の未決事項

- 現行 `MaskMode` enumにReplaceはなく、Add/Subtract/Intersect/Differenceを定義している。Replaceを追加要件とするかは、このテスト作業では仕様変更せず未決事項として残す。
- 既存 `ArtifactMaskVisualTest` の不足演算境界と精度許容値を追加で棚卸しする。
- GUI fixtureのmask path内外の座標・GPU readback閾値をruntimeで確認する。
- 独立GUIのMCP captureが他windowに隠れた状態でも成立することをruntimeで確認する。
