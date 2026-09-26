**最終更新:** 2026-09-26

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

## 2026-09-20 — Detached Task 実装中に判明した既存構造の前提違い

- **関連:** `Artifact/src/AI/AIClient.cppm`（`tryHandleToolCallResponse` `:241-262`、`runCloudChatWithToolLoop` `:354-420`）、`Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`（承認ダイアログ）、`Artifact/src/Undo/UndoManager.cppm`（`push` `:4799-4810`、`beginActionRecording` `:4822`、`endActionRecording` `:4831`、`cancelActionRecording` `:4845`）、`Artifact/include/AI/WorkspaceAutomation.ixx:3660-3668`、`Artifact/include/AI/AgentApprovalPolicy.ixx`（本作業で新規）、`docs/planned/MILESTONE_DETACHED_TASK_2026-09-20.md`
- **確認できた事実（静的読み取り）:**
  - `AIClient` のツールループは承認を一切通さず `ToolBridge::executeToolCall` を実行する。承認判定を持つのは `ArtifactAICloudWidget` の経路だけ。したがって「承認を尊重する非モーダルな AI ツールループ」は現存しない。
  - `beginActionRecording` / `endActionRecording` は名前に反して Undo をまとめない。記録中も 1 コマンド = 1 履歴で push され、`endActionRecording` は 5〜10 件のシリアライズ済みコマンドを JSON で返すだけ（リプレイ用の観測機構）。5 件未満・11 件以上・非シリアライズ可では空を返す。`cancelActionRecording` は適用済み変更を戻さない。複数コマンドを 1 Undo ステップへまとめる「push 保留」機構は存在しない。
  - `delete_layer` は `WorkspaceAutomation::removeLayerFromCurrentComposition`（`:3660-3668`）を通り、確認 gate を通らない。`SafeWriteRemovalGate` が入っているのは `*Confirmed` 系 6 メソッドのみ。
  - コマンド実行（`WorkspaceAutomation::executeCommand`）は Qt ウィジェット・Undo・各サービスを触るため UI スレッド必須。`Core.Thread.BackgroundTaskWorkerPool` の worker thread からは実行できない。
  - `ToolApprovalMode` と `isReadOnlyToolCall` は `ArtifactAICloudWidget.cppm` の匿名 namespace に閉じており他から再利用できなかった。read-only 判定も二重で、`isReadOnlyToolCall` はツールメソッド名の接頭辞、`CommandIR::isReadOnlyType` は語彙の完全一致。
- **価値または懸念:** 承認が UI ウィジェット側にしかなく、`AIClient` 経由のツール実行は無承認で通る。自動化経路を増やすたびに承認が漏れる構造なので、承認判定は共通モジュールへ寄せ、呼び出し側ではなく実行経路で強制するのが望ましい。`delete_layer` が確認 gate の外にあることも、安全契約の適用が `*Confirmed` 系に偏っていることを示す。
- **次に確認:** `AIClient` 経由のツール実行が実際に無承認で書込みを行うこと（実機で承認設定を `AskEveryTime` にし、`AIChatWidget` から書込み系ツールを呼ぶ）。既存コードに `BackgroundTaskWorkerPool` の worker thread から `WorkspaceAutomation` を呼ぶ箇所が無いこと。

## 2026-09-19 — VP操作時にキー時刻スケールが実ストレージとずれるとキー付きフレームへ書けない

- **関連:** `Artifact/src/Widgets/Render/TransformGizmo.cppm`（`captureTransformSnapshot`、`transformKeyframeTimeAtFrame`、`beginHandleDrag`、`handleMouseRelease`）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`gizmoTransformTime`）、`Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm`（`transformTime`）、`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`（`setRotation` は `offset_=initialRotation_` を加算）、`Artifact/src/Layer/ArtifactAbstractLayer.cppm:2046`（`setComposition` が `setKeyframeTimeScale`）。
- **確認できた事実（静的読み取り）:** キーは `AnimatableTransform3D::timeScale_`（層が composition へ参加する時点の `FrameRate::storageScaleForFps(fps,24)`）へ保存される。一方 VP 側のドラッグ・Undo・キャンセルはコンポジション fps から `storageScaleForFps` を再計算していた。両者が一致しない場合（例: 29.97fps で旧タイムラインが作った 29 scale のキー、fps 変更前に作られたキー、保存データの移行前キー）、`captureGizmoPropertyKeys`/`has*KeyFrameAt` の厳密比較が既存キーを見つけられず、`setInitialPosition` 系の分岐へ落ちて「そのフレームのキーが更新されない」状態になる。`setKeyframeTimeScale` は spatial tangent は再スケールするが既存キー時刻は書き換えないため、fps 変更後のキーは自動移行されない。
- **対応:** レイヤー自身がキー時刻ドメインの唯一の定義を持つようにした（`ArtifactAbstractLayer::keyframeTimeScale()` / `keyframeTimeAtFrame(frame)` / `currentKeyframeTime()`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx` に宣言）。VP 側の時刻生成を、コンポジション fps からの再計算ではなくこの API へ統一した: `TransformGizmo`（`transformKeyframeTimeAtFrame`、`multiDragState_->timeScale`、`captureTransformSnapshot`、未使用の `effectiveTransformKeyframeRate` と `import Frame.Rate` を削除）、`ArtifactCompositionRenderController`（`gizmoTransformTime`）、`ArtifactCompositionGizmoUndoCommands` / `ArtifactCompositionEditUndoCommands` / `ArtifactCompositionLayerUndoCommands`（各 `transformTime`。Edit/Layer の2件は double fps をそのまま `RationalTime` へ渡す**切り捨て**バグも同時に解消）。`TransformSnapshot` も同ドメインの frame/timeScale を保持するため Undo・Redo・キャンセルが同じ時刻を指す。加えて `TransformGizmo::handleMouseRelease` で最終状態の `changed()` と `LayerChangedEvent` を1回発行する（ドラッグ中の通知は 33ms スロットルのため、最後の書き込みがタイムライン／Inspector に届かない経路があった）。回帰テスト `Artifact/src/Test/ArtifactTestPropertyKeyframe.cppm` に 29.97fps のキー付きフレーム更新、レイヤー API 一致、`setFrameRate` によるドメイン再ピン留め（キー時刻は移動しない）のケースを追加。`ArtifactTextGizmo` の `textAuthoringTimeScale` はテキストプロパティ（Transform3D チャンネル外）の時刻でありコンポ fps が正のため現状維持。
- **未検証:** ビルド・実機確認は未実施（ユーザー指示待ち）。ユーザー報告の再現条件（コンポ fps、fps 変更の有無、Auto-Key 状態、キーの作成元が左ペイン／タイムライン／過去の VP ドラッグのどれか）は未確認。2026-09-17 の項目で挙げた「シーク未反映時に編集が直前フレームへ書く」可能性（`gizmoUndoFrame_` は `comp->framePosition()` を読む）は今回の変更では扱っていない。
- **次に確認:** 30fps と 29.97fps の両方で「既存キーありフレームでの VP ドラッグ」→ キー値更新、キー数不変、Undo/Redo 一致を比較する。再現が続く場合は `gizmoUndoFrame_` と UI プレイヘッドの一致（seek 完了契約）を次の候補として調べる。残る重複: タイムライン左ペイン（`ArtifactLayerPanelWidget`）と `ArtifactTimelineTrackPainterView` は Transform3D チャンネル以外（opacity 等）の時刻も同じ算出で作っており、これらは Transform3D ドメインとは無関係なため今回の対象外。

## 2026-09-17 — Core キーフレーム監査: 保存時刻と評価時刻の契約を分離しない

- **関連:** `J:/dev/ArtifactStudio/ArtifactCore/src/Property/AbstractProperty.cppm:650-699,736-777,827-932`、`J:/dev/ArtifactStudio/ArtifactCore/include/Geometry/Interpolate.ixx:670-734`、`J:/dev/ArtifactStudio/ArtifactCore/include/Animation/AnimatableValue.ixx:127-154,198-235`、`J:/dev/ArtifactStudio/ArtifactCore/include/Property/PropertySerializationBridge.ixx:218-260`。
- **確認できた事実（静的監査）:** Property の値のみ addKeyFrame は既存キーの補間・ハンドル・roving を既定値へ戻す一方、AnimatableValueT は保持する。Property の Constant は中央キー時刻でも直前キー値を返すが、テンプレート側の Constant は alpha=1 で次キー値を返す。retime は Absolute にも newInPoint.scale() への丸めを行い、衝突を sort/unique で削除する。数値評価は毎回全キーを補間器へコピーして各挿入で再ソートし、evaluateValue も式なしで全キーをコピーする。既存 Property へ deserializeProperty すると、空の expression は適用されず旧式が残る。
- **懸念／設計仮説（未検証）:** RationalTime の比較・保存が厳密でも、評価用 double 時刻への変換で近接キーが同一時刻へ潰れる。時刻の一意性をストレージだけで保証しても不十分。評価区間の探索は RationalTime のまま行い、選んだ区間内の差だけを数値化する設計を検討する。time.scale() を式の FPS として使う現在の経路も、同一時刻を別 scale で表した際の評価一致と衝突するため、FPS を時刻分母から分離する契約確認が必要。
- **価値:** 「保存往復に成功」「個別 getter にロックあり」だけで production-ready と判断せず、キー時刻での値一致、編集メタデータ保持、非破壊 retime、評価スナップショットの一貫性を受入条件にできる。
- **2026-09-17 修正:** 値のみ更新は既存メタデータを保持し、Property と共通補間器は正確なキー時刻でそのキー値を返すよう変更。Absolute はリタイム対象から除外。リタイムは編集時だけ一時コピー上で処理し、衝突・範囲外は Property 全体の変更を拒否して lastError に記録する。既存の標準 vector ストレージをコピーするため追加の標準コンテナへの置換はない。フレーム評価への追加確保はない。既存 PropertyKeyframe テストへ値更新・全型 Hold 境界・Absolute・衝突・アンカーの回帰を追加。ビルド・テストは未実行。
- **2026-09-17 検証（実行済み）:** `out/build/x64-Debug`（Ninja/MSVC 14.51）で `ArtifactCore` を差分ビルド → 成功（`Interpolate.ixx` / `AbstractProperty.ixx|.cppm` 再コンパイル、警告は既存 C5202 のみ、エラーなし、`ArtifactCore.lib` 再リンク）。変更後の ifc/ライブラリを実際に import/link する検証 exe（`temp/verify_keyframe_behavior.cpp`）をビルドし実行 → 14/14 PASS（値更新のメタデータ保持、NaN 拒否、Hold の 9/10/11/20/25 フレーム境界、Color/Point2D、Absolute の 1/48・1/24・巨大整数保持、衝突時の全体拒否、既存の置換・削除・整列・線形補間）。リポジトリ追加分の回帰テストは実ビルドの単一モジュールターゲットでコンパイル確認済み（`ArtifactTestPropertyKeyframe.cppm.obj` 06:03 更新、エラーなし）。検証用スクリプトは `temp/run_verify.bat`、`temp/run_verify.ps1`、`temp/build_test_module.bat`。
- **未検証:** `Artifact.TestRunner` の全テスト実行（アプリ exe のビルドが必要なため未実施）。修正前コードでの失敗再現（意図的なロールバックビルド）は未実施で、旧挙動は差分からの読み取りに基づく。空式の復元、並行編集・評価、アロケーション計測は未着手。
- **残る境界（未検証）:** レイヤーの setInPoint/setOutPoint はリタイム前に尺を確定し、Property の lastError を確認しない。今回の拒否は Property 単位であり、レイヤー全体の尺変更はロールバックしない。複数 Property と尺変更を一括で確定／拒否する設計は別途判断が必要。
- **次に確認:** 追加した回帰テストの実行、空式の既存オブジェクト復元、並行編集・評価とアロケーションの実測。既存テストの呼び出しは Test.cppm の runAllTests 経由で、PropertyKeyframe 専用の実行フィルタは確認できていない。


## 2026-09-17 — キー編集時刻統一後に残る独立した確認事項

- **関連:** `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`（`cancelInteraction` / `pushTransformUndoIfNeeded`）、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`（`applyTimelineSeek`）、`Artifact/src/Service/ArtifactPlaybackService.cppm`（`publishFrame`）。
- **確認できた事実:** Text の anchor 操作は静的チャンネルにもキーを作るが、キャンセル／Undo 分岐は操作開始時の `before.animated` を使用する。タイムラインの表示フレーム更新は停止時の queued な composition 同期に先行する。主コントローラーの単発リセット等には今回のドラッグ修正とは別に `layer->currentFrame()` を使用する箇所が残る。
- **懸念（未検証）:** 静的 anchor のキャンセルでキーが残る可能性と、シーク未反映時の diamond toggle が直前フレームのキーを削除する可能性がある。今回報告されたアニメーション不成立との一致は未確認。既存の誤った時刻のキーは自動移行していない。
- **次に確認:** 別途、静的 anchor の Undo／キャンセル契約、編集直前の seek 完了契約、単発リセットの時刻を個別に検証する。今回のドラッグ時刻・FPS 統一とは分離し、追加配線やシーク方式変更は行わない。

## 2026-09-17 — A6 の Text offline raster cache は既存 TextLayer が所有済み

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:2268`、`Artifact/src/Layer/ArtifactTextLayer.cppm:2725`。
- **確認できた事実:** offline/Render Queue 側は `textLayer->toQImage()` を呼ぶが、`ArtifactTextLayer` は `isDirty_` または未生成の `renderedImage_` のときだけ `updateImage()` を実行し、それ以外では保持済み `renderedImage_` を返す。通常の非編集フレームで Text raster を再実行する経路ではない。
- **価値または懸念:** A6 を別の Composition View キャッシュとして重ねると、既存の TextLayer dirty invalidation と二重化し、古い画像を返すリスクがある。最適化対象は `toQImage()` 自体ではなく、Text の dirty 化頻度または後段の surface/effect 処理を計測してから決めるべきである。
- **次に確認:** 実機プロファイルで `ArtifactTextLayer::updateImage()` が非編集フレームに現れる場合だけ、dirty 要因とアニメータ時刻評価を分けて調査する。

## 2026-09-17 — A3 と A10 の一部は既に実装済み、残る有効化は runtime parity が条件

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:37546`（Composition-space GPU cache）、`:38486`（timeline transition 評価）、`:42920`（選択レイヤー overlay）。
- **確認できた事実:** composition-space GPU cache は `Render/Experimental/CompositionSpaceGpuCache` の opt-in で、キーに pan/zoom を含めず、hit 時には保持済み composition texture を現在の表示変換で描画する。A3 の camera-only reuse 自体はこの限定構成（2D solid/still、Normal、非3D、mask/effectなし）で成立している。timeline transition progress は層ループ外で一度だけ評価されているため、A10 の同呼び出しを層数比例で削減する余地は現状ない。
- **価値または懸念:** A3 の既定有効化は Diligent backend ごとの offscreen presentation / color-transform parity を runtime で確認してから行う必要がある。選択 overlay では `selectedLayersInOrder()` が `QVector` の値コピーを返すが、reference API へ変えると選択変更時の寿命・スレッド境界契約を再検討する必要がある。
- **次に確認:** D3D12 と Vulkan で pan/zoom 中および停止後の full-resolution presentation を比較する。overlay copy は selection manager の immutable snapshot 契約を明文化できる場合のみ reference/view API を検討する。

## 2026-09-17 — A4/A7/A10 残部は正本契約の整理が先行する

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:2410`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:36813`、`:40106`。
- **確認できた事実:** controller の調整レイヤー主経路には、pointwise GPU effect と interaction/draft 時の CPU readback 短絡が実装済みである。一方 exported drawing fallback の `readbackToImage()` は CPU rasterizer effect 正本へ渡す唯一の完全品質入口だった。カメラの前フレーム行列は、親子 transform と shake を含めるため composition 全体を N-1/N と往復評価している。mask overlay の選択頂点は多くの入力 controller が共有する可変 vector であり、描画時だけの hash 化は毎フレーム確保を生む。
- **価値または懸念:** 既存の GPU spatial/raster effect 完全互換パス、時刻指定の親子カメラ評価 API、または selection の immutable revisioned snapshot なしに置換すると、settled 出力、motion vector、選択操作のいずれかを壊す可能性がある。
- **次に確認:** 各 CPU rasterizer effect の GPU 対応表と adjustment mask の semantics、parent transform を含む camera-at-time API、selection snapshot の所有／寿命を先に設計レビューする。runtime parity なしにこの3項目を有効化しない。

## 2026-09-17 — A5 の GPU matte output は frame 内共有済みで、跨 frame cache には time identity が不足する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:38232`、`:10097`。
- **確認できた事実:** `matteGpuOutputs_` は matte source ID ごとの offscreen texture を保持し、同一 frame・同一 size・同一 `surfaceGeneration` の source を複数の target layer が参照する場合に再レンダリングを避ける。CPU fallback は GPU intermediate を使えない matte に限定される。
- **価値または懸念:** `surfaceGeneration` は layer mutation を追跡するが、keyframe transform、親 transform、時刻依存 source/effect の各評価を単独では表さない。この条件から `frame == frame` を外すと、静的に見える matte が別フレームで古い位置・内容になるおそれがある。
- **次に確認:** source layer の全時間依存性（transform / parent / effect / mask / source mapping）を表す revisioned render identity を整備し、その identity が不変な matte だけ frame をまたいで再利用する。

## 2026-09-17 — 3D AOV は target reset で queue submit 済みのため per-AOV Flush を不要化できる

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:11568` 以降、`Artifact/src/Render/ArtifactIRenderer.cppm:2865`（render target override）、`:3125`（`flush()`）。
- **確認できた事実:** `setOverrideRTV` / `setOverrideDSV` は override 値を変更する前に `submitQueuedDraws()` を呼ぶ。3D mesh の AOV mode / ID 値は `ArtifactIRenderer::Impl::drawMesh()` の呼び出し時に primitive renderer へ反映される。したがって AOV の `layer->draw()` 後に target reset する順序は維持したまま、各 AOV の `IDeviceContext::Flush()` を省ける。
- **価値または懸念:** emission / normal / velocity / object ID / material ID / albedo の per-layer 強制 submission がなくなる。beauty path の MSAA resolve 前 flush は queued draw の提出順序を担うため残す。backend 共有経路なので D3D12/Vulkan の multi-channel output を runtime で確認する必要がある。
- **次に確認:** 3D layer で beauty + 全 AOV を出力し、各 channel の内容と `flushCount` を修正前後で比較する。

## 2026-09-16 — VP描画は2実装が同名で併存し、scene light lift は offline 側にしかない

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:8306`（モジュール内ローカル `drawLayerForCompositionView`、21引数）、
  `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:1460`（export 版、11引数）、
  `Artifact/src/Render/ArtifactRenderQueueService.cppm:3882/6019`（export 版の呼び元）。
- **確認できた事実:** 対話VPの `renderOneFrameImpl`（同 cppm:39599 直接パス / 11484 GPUパス）は引数個数からモジュール内ローカル実装を呼び、
  Render Queue / offline は `import Artifact.Render.CompositionViewDrawing` の export 版を呼ぶ。両者は同名・別実装で、キャッシュ・マット・マスク処理を二重に持つ。
  非3Dレイヤーへの決定的な明度リフト（`min(0.18, 0.03 * lightCount)`）は export 版の `applySurfaceAndDraw` 内にのみ存在し、
  ローカル実装は 3D（`setSceneLights`）以外にライトを適用しない。したがって lit な 2D レイヤーは VP と レンダー出力で見え方が異なる。
- **価値または懸念:** 「VPの scene light CPU ループ」という指摘は実は offline（Render Queue）側の話であり、VP のフレーム時間には効かない。
  逆に 2D ライトのパリティ差は未整理の仕様差であり、どちらが正なのかは未確定。実装を片方だけ直すと差が広がる。
- **次に確認:** VP と offline の 2D ライト適用を統一するか、VP を正として offline のリフトを撤去するかをユーザー判断で決める。
  統合する場合はローカル実装の削除（マスク・マット・GPUラスタ効果の順序parity確認が前提）が本筋の整理。

## 2026-09-16 — Normal-only コンポジションの GPU ブレンド経路（帯域見積りと opt-in 化）

- **関連:** `ArtifactCompositionRenderController.cppm`（`hasGpuBlendJustification`、`gpuBlendPathRequested`、新規 `gpuBlendForNormalCompositionEnabled()`）。
- **確認できた事実:** 既定の直接パスは、レイヤー内容がキャッシュに乗っていれば sprite draw のみ。GPUパスはレイヤー毎に
  全画面 `convertLayerToFloat` + 全画面 `blendLayers` + ping-pong を追加する。1080p・10層の概算で、GPUパスは
  約 20 回の全画面 32F read/write（≒0.6GB/frame、RTX4070Ti で約2ms超）に対し、直接パスは 10 枚の quad（≒0.17GB）で、単純な Normal 構成では逆効果になり得る。
- **仮説（未検証）:** 効果（特に spatial）・マット・マスクを持つ層、または毎フレーム内容が変わる層（アニメーション効果、video、particle）だけを
  GPUパスへ寄せれば bandwidth は正当化できる。逆に静的効果層は CPU 側キャッシュ（signature 一致）で既にスキップされているため対象外が妥当。
- **対応:** 既定挙動を変えず、`ARTIFACT_COMPOSITION_GPU_BLEND_NORMAL=1` で Normal-only も GPU パスへ入る opt-in を追加（計測用）。
- **次に確認:** 同条件（1/10/30層、Normal と Mixed、効果あり/なし）で `[CompositionView][Perf]` の frameMs/layerPassMs を A/B し、
  既定を反転してよい構成条件（層種別・効果種別）を確定する。反転する場合は 8bit sRGB 合成→float linear 合成の画質差も同時に確認する。

## 2026-09-16 — 確定できていないVPホットパス候補（未着手・要計測）

- **関連:** `ArtifactCompositionRenderController.cppm:38688/38830/38865`（層ごとの `FunctionalRenderPass` 3個）、
  `:10563`（`RenderGraph::execute` が層ごとに `std::function` を構築）、`ArtifactCore/include/Graphics/RenderGraph.ixx:108`、
  `ArtifactCompositionRenderController.cppm:36065`（30フレームごとの診断）。
- **仮説（未検証）:** 層×フレームで `std::function`（キャプチャ4参照＝SSO超過）と RenderGraph 実行記録が確保される可能性がある。
  また `captureRenderDiagnostics` が30フレーム毎に RenderCostCaptureGuard / TraceGuard / frame RenderGraph 構築+compile を走らせるため、
  周期的な stutter になり得る。`flushMs` は2026-08-15のInsight以降、累積差分として算出されるようになっており意味が変わっている。
- **価値または懸念:** どちらも描画結果を変えずに検証・修正できる低リスク候補。ただし確保量は未計測で、推測で消すと
  RenderGraph の検証経路を失う可能性がある。
- **次に確認:** allocation トレースで層数比例の確保を確認してから、`runAllWithRenderGraph` の素通し化（`run` 直呼び）と
  診断の延実行を検討する。
## 2026-09-16 — i18n P0-1：監査の実効化とtimeline tooltip 137キーの翻訳リンク

- **関連:** `tools/i18n/audit_translations.py`、`.github/workflows/i18n-check.yml`、
  `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、
  `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm`、`Artifact/translations/{en,ja}.json`。
- **確認できた事実:** 監査の `KEY_PATTERNS` が `tt()` を抽出できず `Keys used: 3` だった。
  CIの99.6%は虚偽で、実際には `timeline.*` 133キーがJSON未登録（JAでも英語表示）だった。
  文面そのままキー2件は `QObject::tr`（Qt側、JSON系と無関係）で常時英語だった。
  同一キーに複数フォールバック（メニュー`...`付き vs ダイアログタイトル素形）が7件あった。
- **対応:** 監査に `tt(` 抽出＋命名規約lint（`^[a-z][a-z0-9_]*(\.[a-z0-9_]+)+$`、`--max-invalid-keys` 既定0）＋
  意図的同一値の `--allow-same` を追加。文キーは正式キーへ移行（`script.*` 2件、
  `layer_panel.keyframe_value_hint`／`matte_list_hint`）。衝突5キーはメニュー形を基本キー、
  ダイアログ形を `*_title` へ分離（`rename` のみ既存 `layer_panel.rename_layer_title` 再利用）。
  ENはコードのフォールバックから採取、`timeline.*` 137件にJA訳を付けて両JSONへ追加。
- **価値または懸念:** 監査は `Keys used: 410`・coverage 100%・untranslated 4（既存技術表記のみ）で真緑になった。
  JAロケールでタイムライン tooltip が日本語表示になる。`linear`／`linear_word` のように
  大文字小文字だけが違う文脈依存キーが今後も増える余地あり（命名規約では検出不可）。
- **次に確認:** ビルド許可後に tooltip の日英表示、`--lang en/ja` 両方での監査通過。
  **注意：本ツリーには別作業の未コミット変更が混在**（`ArtifactCompositionViewDrawing.cppm`、
  `ArtifactCompositionRenderController.cppm`）。コミット時は本件分離のこと。

## 2026-09-16 — タイムライン左メニューの翻訳リンクと潜在バグ3件

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`（レイヤーパネル右クリックメニュー）、`Artifact/src/Widgets/CommonStyle.cppm`（`sizeFromContents`／`drawControl` の CT_MenuItem／CE_MenuItem）、`Artifact/translations/{en,ja}.json`（`layer_panel`）。
- **確認できた事実:** ① メニュートップが英語の `Frequent`／`All` の開発者用語2階層で、最大4階層ネスト＋「整理」の同名重複があった。② `CommonStyle` の CT_MenuItem 幅計算（68/58px予約）が CE_MenuItem 実描画（84/72px使用）よりサブメニュー項目で約16px・通常項目で約14px不足し、長い項目混在で文字が切れていた。③ `kColNames` が5要素なのに `kLayerPropertyColumnCount`（6）でループし `kColNames[5]` が範囲外参照だった（6列目は Pick Whip／親リンク列）。④ 同ブロックとラベル色ブロックが `QString::fromLatin1` で日本語を渡しており非Latin文字化けの状態だった。
- **対応:** Frequent／All を廃止して全項目をトップレベルへ＋区切り線3本＋重複整理の改名。CT_MenuItem の左右予約幅を描画と一致させた。`tt("layer_panel.*", "English")` へ約160箇所を変換し en／ja に計153＋14＋6キーを追加（既存12キーは再利用）。6列目 `Parent Link`／`親リンク` を追加し tt 化で fromLatin1 と範囲外参照を同時解消。日本語表示は従来文言と同一。
- **価値または懸念:** ロケール切替で英日メニューが切り替わる（起動時 `loadFromDirectory`＋CMake が translations を出力へ複写）。QInputDialog の種別名（Crossfade 等）は保存値と往復するため翻訳対象外に据え置き。Undo ラベルも英語のまま。
- **次に確認:** ビルド許可後に右クリックメニューの表示・幅・英日切替、`missingKeys()` が空であること。`Matte`／`<missing>` の他箇所（2010行付近・7874行付近）およびダイアログ文言は未リンクの残件。

## 2026-09-16 — Animation Layer Inspector評価とMSVC Modules ICE

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`（`getLayerPropertyGroups()`）、`ArtifactCore/include/Animation/AnimatableValue.ixx`、`ArtifactCore/include/Geometry/Interpolate.ixx`。
- **確認できた事実:** Property Group生成中の`AnimationLayerStackT<float>`値表示が`AnimatableValueT<float>::at()`を通じて汎用`interpolate<float>()`を実体化し、MSVC 14.51のIFC import環境でC1001を起こした。通常のInspector表示は評価値ではなく永続値で十分に編集可能である。
- **対応:** InspectorのAnimation Layer値を`current()`による永続値表示へ切り替え、補間評価はレンダー／専用アニメーション経路に限定した。
- **価値または懸念:** 巨大な`ArtifactAbstractLayer`を即時にABI分割せずに、該当テンプレート実体化をコンパイル単位から除去できる。フレーム評価値をProperty Inspectorへ再導入する場合は、専用状態モジュールまたは非テンプレート評価APIが必要。
- **確認:** `cmake --build out/build/x64-Debug --target Artifact --config Debug --parallel 4` が成功した。`Impl`内のAnimation Layer状態ラッパー化、およびUtilitiesモジュールへ新しい公開Animation Layer型を出す二案は、MSVC 14.51のIFC import環境でC1001を再発させたため採用していない。
- **次に確認:** 実機でAnimation Layerの値編集・保存・復元・レンダー評価が維持されること。物理分割を再開する場合は、`Impl`の共有内部表現を先に設計し、公開テンプレート型を新規IFC境界へ出さないこと。

## 2026-09-16 — Diligent版タイムライン上部バー操作負荷の最適化

- **関連:** ArtifactTimelineWidget.cppm（syncPlayheadOverlay、syncGpuTimelineSnapshot、uildGpuTimelineSnapshot、TimelineScrubFinishedEvent）。
- **確認できた事実:** ① syncPlayheadOverlay() において gpuTimelinePreviewEnabled_ を判定していなかったため、GPU描画モード中にもかかわらずシーク毎に Qt 側の TimelinePlayheadOverlayWidget が再有効化・二重描画されていた。② スクラブ・ナビゲータードラッグ操作による連続マウス移動ごとに syncGpuTimelineSnapshot() がキュー投入され、GUIスレッドでスナップショット生成が過剰実行されていた。③ uildGpuTimelineSnapshot() 内の staticHit 判定で !view->isInteracting() を要求していたため、上部バーのシーク・スクラブ操作時にも全トラック行・全グリッド・全クリップ・UniString 含む静的ジオメトリが全件再生成されていた。
- **対応:** ① syncPlayheadOverlay() で gpuTimelinePreviewEnabled_ を加味し、GPUモード時は Qt 側オーバーレイを無効化。② syncGpuTimelineSnapshot() に最小16ms（約60FPS相当）のタイマースロットリングを導入し、TimelineScrubFinishedEvent で最終フレームを確実に同期。③ uildGpuTimelineSnapshot() の staticHit 条件から !view->isInteracting() を外して静的キャッシュのヒット率を高め、プレイヘッド移動のみの再構築負荷を最小化。
- **価値または懸念:** スクラブ・シーク中の不要なオーバーレイ描画と大量のスナップショット再構築が消え、上部バー操作の追従性・フレームレートが大幅に改善される。
- **次に確認:** ビルド許可後、Diligentプレビュー時のスクラブバー・ナビゲーター操作時の軽快感、シーク終了時のプレイヘッド正確性、通常QWidgetモードへの切り替え時の動作整合性を確認すること。

## 2026-09-16 — スクラブ中VP draft＋timeline Present(0)

- **関連:** `ArtifactCompositionRenderController.cppm`（`TimelineSeekRequestedEvent` 購読→`notifyViewportInteractionActivity`）、`ArtifactDiligentTimelineRenderWindow.cppm`（`Present(0)`）。
- **確認できた事実:** VP 本流は controller 経路で、既存の interacting/draft 機構（effect 解像度0.25・CPU readback 回避・LOD）が viewport 操作時だけ有効だった。timeline スクラブは seek を publish するが interacting に入らないためフル描画だった。また Diligent の `Present()` 既定は vsync 有効で、timeline の Present が GUI スレッドを止めていた。VP worker 経路（`CompositionRenderWidget`）は `AppMain` で生成されておらず、スクラブ時の描画本流は controller 側。
- **対応:** 全スクラブ身振り（scrub bar／overlay／track view）が publish する seek を interacting へ流し、120ms 無 seek で自動復帰（既存 tick 処理を再利用、bracket 不要）。timeline 面は `Present(0)` で GUI ブロックを除去（tearing 許容、VP は vsync 維持）。
- **価値または懸念:** スクラブ中の VP 1描画あたりが軽くなり、timeline 操作の応答が上がる。tearing が目立つ場合は要調整。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** スクラブ中の描画負荷・tearing の見え方、120ms 復帰の体感、release 直後のフル画質復帰を確認すること。

## 2026-09-16 — Timeline GPU面のPresent間引き（30Hz pacer）

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`（`Impl::render`、`kMinPresentInterval`）。
- **確認できた事実:** 再生・スクラブ中は snapshot 毎に submit＋Present が走り最大60Hz。静動分離で CPU 再構築は軽くなったが GPU 投入は毎 tick 残っていた。
- **対応:** `render()` 入口で前回 Present から33ms未満なら描画を捨て、残り時間後に1回だけ wakeup して最新 snapshot を描く。snapshot 差し替え自体は継続されるため中間フレームは自然に間引かれる。タイマーは window をコンテキストにし、破棄時は自動キャンセル。resize/expose 直後も最大33ms遅延する。
- **価値または懸念:** 独立 device 上の submit 量が半減し、VP との GPU 奪い合いが減る。単発シークの表示遅延は最大33ms。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** 再生・スクラブ中の滑らかさと遅延感、30Hz で不足なら間隔調整、Qt fallback 面には影響なし（同クラスだが pacer は GPU render 経路のみ）。

## 2026-09-16 — GPU timeline snapshot の静動分離（static cache＋dynamic tail）

- **関連:** `ArtifactTimelineTrackPainterView`（`timelineVisualRevision`／`isInteracting`／`touchTimelineVisuals`）、`ArtifactTimelineWidget::buildGpuTimelineSnapshot`（`gpuTimelineStatic_`）。
- **確認できた事実:** 従来は再生 tick 毎（16ms）に snapshot 全体（行・グリッド・全クリップ・UniString タイトル・波形64本・マーカー）を作り直していた。text クリップのタイトル等は tick 毎に変わらない（`refreshTracks` 時のみ更新）ため static 化しても描画は一致する。
- **対応:** 行・グリッド・クリップ・comp/marker を `(ppf, h/vOff, viewport, revision)` キーでキャッシュし、再生 tick は frame 依存のマーカー強調＋playhead のみ再構築（QVector 暗黙共有で static 部は無コピー）。revision は View 内の全 visual 変更点＋`mouseReleaseEvent` で bump、操作中（drag/marquee/scrub/pan）は `isInteracting()` で毎回再構築。`audioMuted` のみ snapshot に効くためそこだけ個別 bump（rate/pan/gain/reverse は snapshot 非消費）。
- **価値または懸念:** tick 毎の O(n) 再構築と UniString 生成が消える。GPU 全描画＋Present 自体は残る（次の Present 間引き対象）。atCurrentFrame 強調は dynamic へ移動し画素等価を維持。
- **次に確認:** ビルド・`check_module_hygiene`・実機で再生中の表示一致と負荷減を確認すること。**注意：本ツリーでは別作業の未コミット変更が同一関数に重なっている**（色・グリッド密度等）。コミット時は分離し、本キャッシュのキー／bump 点を壊さないこと。

## 2026-09-16 — Timeline GPU面を独立D3D12 deviceへ分離（shared immediate競合の解消）

- **関連:** `Artifact/src/Render/DiligentDeviceManager.cppm`（`createIndependentRenderDevice`、`createSwapChainForIndependentDevice`）、`Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`（`Impl::initialize`）。
- **確認できた事実:** shared device は device＋immediate各1個のプロセス共用で、submit 時の同期が acquire/release の mutex だけだった。VP worker と timeline GUI が同一 immediate へ並行 submit していた（viewport/RTV ステート踏みの可能性あり）。
- **対応:** DeviceManager に独立払い出し（共有なし・失敗時フォールバックなし・契約コメント付き）を追加し、timeline 初期化を独立 device へ切替。失敗時は従来どおり Qt painter へ戻る（`setGpuTimelinePreviewEnabled` の既存処理）。curve 面は同クラスなので自動で独立する（＝timeline＋curve＋VP で最大3 device）。
- **価値または懸念:** context 競合のクラスごと消える。代償に VRAM・PSO・シェーダコンパイルが device 数分、D3D12 trim 通知は shared のみ（独立分は未登録・未検証）、起動時間増の可能性。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** ビルド・`check_module_hygiene`、timeline/curve GPU 初期化・表示、VP と同時操作時の停滞解消、複数 device 時の VRAM・起動時間、trim 登録の要否。

## 2026-09-13 — 任意Mesh Shader PSO失敗を3Dレイヤー生成から隔離する

- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cppm`（`createPSO`、Meshlet LOD Mesh Shader PSO）。
- **確認できた事実:** D3D12では通常のindexed mesh PSOが成功した後、任意のmeshlet PSO生成だけが失敗していた。従来はデバイスがMesh Shaderを広告すれば未検証の任意PSOを常に作り、失敗後にも汎用の「PSO created successfully」を出すため、3Dレイヤーの必須経路と誤認しやすかった。
- **対応:** Mesh Shader経路を`ARTIFACT_ENABLE_EXPERIMENTAL_MESH_SHADERS=1`の明示opt-inへ戻し、PSO例外時は参照を破棄して通常のindexed GPU描画へfail-softする。成功ログもindexed PSOを明示する。可変light loop内のGoboサンプルはimplicit gradientを使わない`SampleLevel(..., 0)`へ変更した。
- **価値または懸念:** 3Dレイヤー作成は実績のあるDiligent indexed GPU経路を使い続け、任意高速化のPSO拒否で落ちない。報告されたheap破損の検出地点はQt repaintであり、同じ操作で再現しないことを実機確認するまで破損元を断定しない。
- **次に確認すること:** 通常環境で3Dレイヤー作成・描画・終了時解放を再確認し、mesh shaderはD3D12／Vulkan個別のPSO validationを通した後にopt-inで再検証する。

## 2026-09-13 — Composition Viewerの3Dグリッド面はXYへ合わせる

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`drawThreeDimensionalGroundGrid`）、`ArtifactCore/include/Grid/ArtifactGridSystem.ixx`（`computeGroundGridLines`）。
- **確認できた事実:** 共通`GridSystem`のground gridは3Dシーン用のXZ面を生成する。Composition Viewerの平面レイヤー／コンポジションはXY面にあるため、FrontではXZグリッドがedge-onになり、赤いX軸だけがコンポジションを横切る線として見えていた。
- **対応:** Coreの`GroundGridSettings`へ既定XZを維持する`GridPlane3D`（XY／XZ／YZ）を追加し、共通生成器が指定平面の座標を直接返すようにした。Composition ViewerはXYを指定し、提出側の座標読み替えを撤去した。軸もX/Yの2本へ揃えた。
- **価値または懸念:** Frontではコンポジションと平行な編集グリッドになり、斜視ではコンポジション面と同じ傾きを持つ。グリッドは既存どおりコンポジション背景より先に描くため、面の上へ重ならない。
- **次に確認すること:** ビルド許可後、Front／Top／Right／Perspectiveで面の向き、コンポジション外の表示、D3D12／Vulkanの線描画を確認する。

## 2026-09-13 — Unity Orientation overlayとTimeline GPU面の視覚責務

- **関連:** `Artifact/src/Widgets/Render/ArtifactViewOrientationWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelinePresentation.cppm`。
- **確認できた事実:** Unity公式のOrientation overlayは中央キューブ、円錐状のXYZ軸アーム、キューブ下のPersp/Iso表示を基本形とする。既存Simple表示はpresentation切替バッジと面内`Persp`が支配的で、回転後も軸クリック判定だけ固定位置だった。Timelineは環境変数未設定時にDiligent面が既定だが、GPU面のclearがcomposition canvasにも使う汎用背景色だった。
- **対応:** Simple表示を円錐軸、面ラベルと投影ラベルの分離、低コントラストのstyle切替へ寄せ、クリック判定も表示中のorientationから投影する。Timeline GPU面はsecondary background由来のcharcoalへ変更し、採用モックどおりcache/work areaを上、navigatorを下へ置いた。
- **価値または懸念:** ViewCubeの向き表示とクリック対象が一致し、Timelineの空状態でも明るいcanvasに見えない。投影切替そのものは既存のorientation snap契約を維持しており、Unity完全互換のcenter-click projection toggleは未実装。
- **次に確認すること:** 実機でSimpleの各軸click／drag／Front表示、timeline GPU初期化成功時の暗色面・下部navigator、Qt fallbackとの操作同等性を確認する。

## 2026-09-13 — VP Cryptomatte表示は生のfloat payloadを保持する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`composeViewportChannelOverlayImage`、Object ID / Material ID AOV）、`Artifact/src/Render/ArtifactIRenderer.cppm`（通常画像readback）。
- **確認できた事実:** Object ID / Material ID は `uint32_t` を float bit pattern として格納するCryptomatte形式だが、従来のVP表示は通常の`readbackChannelToImage()`経由で0〜1へclampして8-bit化していた。このためIDが失われ、擬似色がほぼ単色になった。加えてGPU ID passは現時点で`layer->is3D()`だけを描画対象にしている。
- **対応:** VP表示だけは`readbackToMultiChannelImage()`から生float payloadを取得し、`floatToId()`後のhashで安定した識別色を作るようにした。クリック選択も、host全域ではなく表示画像のaspect-fit矩形からAOV座標へ変換する。export・クリック選択の生AOV値は変更しない。
- **価値または懸念:** 同一IDは常に同色、背景ID 0は黒で表示される。これは表示の復旧であり、2D/Textのalpha形状へIDを書く機能はまだ未実装のため、2Dレイヤーだけのシーンで正しいマットになる保証はない。
- **次に確認:** ビルド許可後、複数3Dレイヤーで色が分かれること、Object ID上のクリック選択、2Dテキストへalpha付きID passを追加するための既存2D draw/mask経路を確認すること。

## 2026-09-13 — RGB成分表示はfinalize後のdisplay surfaceを保持する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`finalizeGpuRenderToViewport`、AOV source公開、`drawViewportChannelOverlayImage`）。
- **確認できた事実:** Red/Green/Blue/Alphaは`finalizeGpuRenderToViewport()`でcompute変換済みの`viewportChannelDisplaySRV_`を作るが、同フレーム後半のAOV source公開処理が無条件にnullへ戻していた。結果としてGreenなどは表示用surfaceでなく、途中までpresentされたBeautyのreadbackへフォールバックし、VP内に再帰的な見た目が出た。
- **対応:** 表示surfaceはフレーム開始時にのみclearし、finalizeが作ったsurfaceがある場合は後段で保持する。Emission等の専用AOVはsurface未設定時だけ既存のsource公開を行う。
- **次に確認:** ビルド許可後、RGB／Alphaの単成分表示がグレースケールで安定し、pan／zoom／Perspectiveの表示状態に依存してBeautyやグリッドが混入しないことを確認する。

## 2026-09-12 — テキストレイヤーの移動・リサイズ不能（2D TextGizmo の未バインド）

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`drawViewportOverlayPass` のギズモ選択、`handleMousePress`）、`Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`。
- **確認できた事実:** `viewportOrientationActive_` は宣言時 `true` で、以後どこでも `false` に代入されない（grep で全書き込みが `= true`）。そのため `use2DTransformGizmo = !viewportOrientationActive_ && layerUsesTextGizmo(...)` は常に false になり、テキストレイヤーの 2D TextGizmo は描画・バインドされず `sync2DGizmosForLayer(nullptr)` で解除される。一方で 3D ギズモは `showProjected3DGizmo = true` で全レイヤーに描画されるが、そのヒットテストは `!layerUsesTextGizmo` でテキストを除外しているため、テキストは「フレームは見えるが移動・リサイズ不能」になっていた。
- **対応:** ① `use2DTransformGizmo = layerUsesTextGizmo(selectedLayer)` に変更（テキストは常に 2D TextGizmo を使う）。② `showProjected3DGizmo = !layerUsesTextGizmo(selectedLayer)` に変更し、テキストへの 3D ギズモ二重描画を回避。③ テキストギズモの `handleMousePress` 前に `setLayer(gizmoLayer)` を追加（contentGizmo と同様の防御）。
- **価値または懸念:** `viewportOrientationActive_` が常に true のため、コード内の `!viewportOrientationActive_` ガード（`layerUsesProjectedFrameGizmo && !viewportOrientationActive_` 等）は複数箇所で実質デッドコード化しており、「3D 統一フレームギズモ」への移行が未完の状態。また `use2DTransformGizmo` ブロック内の contentGizmo（シェイプ等のコンテンツ編集）も同様に遮断されたままで、これは別の潜在バグ。
- **次に確認:** ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。テキストレイヤー選択時の移動（Offset）とボックス角・辺ハンドルのリサイズ、回転・アンカーのドラッグを確認すること。シェイプのコンテンツ編集モードの動作も確認（本修正では対象外）。

## 2026-09-12 — グリッド描画のビューポート全域化とcanvasSize欠落の修正

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`(`drawViewportCanvasOverlay`)、`Artifact/src/Render/PrimitiveRenderer2D.cppm`(`drawGrid`)、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`(`submitGrid`)、`Artifact/include/Render/ViewerHelperShaders.ixx`(`g_gridPS`)。
- **確認できた事実:** 2D矩形グリッドは `drawGrid(0,0,cw,ch,…)` とコンポ寸法でquadを切っており、ズームアウト時にコンポ塗りつぶし領域にしか出なかった。加えて `submitGrid` が `helper._pad`(=シェーダーの `canvasSize`) を `{0,0}` で上書きしていたため、グリッドは線でなく塗りつぶしで描画されていた。また `ArtifactCore` の `Artifact::Grid::GridSystem`(`computeVisibleLines`) は `visibleCanvasRect` 基準でビューポート全域グリッドを正しく計算できるが、コントローラは独自のインライン描画(`gridPolarMode_`/`gridIsometricMode_`/autoStep)で `GridSettings` を別経路に再実装しており重複している。
- **対応:** `drawGrid` に grid quad のcanvas起点を `color2` で渡し、シェーダーは `gridOrigin.xy + uv*canvasSize` でコンポ原点基準に再構築、負座標対応の対称距離判定へ変更。`submitGrid` は `canvasSize`/`gridOrigin` を正しく転送。`drawViewportCanvasOverlay` は pan/zoom から可視ビューポート矩形を計算して矩形グリッド・原点軸・数値ラベルをビューポート全域に拡張、太さは `thickness/zoom`(canvas単位)へ正規化。
- **価値または懸念:** `GridSettings`(`Artifact::Grid::GridSettings`) を唯一の設定源として維持し、新規設定や並行機構は追加していない。長期的にはコントローラのインライン実装を `GridSystem`/`GridLayer` へ寄せる余地がある(重複解消)。polar/isometric は依然コンポ中心基準でビューポート全域化は未実施。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。ズームアウトでビューポート全体に線が出ること、パンで線がコンポ原点に固定されること、グリッドが線として描画されることを確認すること。

## 2026-09-12 — バインドなし固定キーの残存棚卸しと最小4点の局所化

- **関連:** `ArtifactCore/.../ShortcutBindings`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`。
- **確認できた事実:** `matches()` は `ArtifactPr` のみ使用で `Artifact` 本流の `keyPressEvent` 約365件・`new QShortcut` 約54件は固定のまま。今回分は既存ID流用とコンテキスト縮小のみで新規ID・新規接続なし。`immersiveExit Esc` に `WidgetWithChildrenShortcut` 付与、`clearSearch Esc` を `WidgetShortcut` 化、`複製` を `LayerDuplicate(Ctrl+D)`、`Edit Text` を `LayerRename(F2)` へ統一(既定値同一)。第2弾で `Timeline` 検索2件に `WidgetWithChildrenShortcut`、ツール `V/H/Z/R/S` を `Timeline{Selection,Hand,Zoom,Rotate,Slide}Tool` へ統一、`WorkCursorPlace/Center/Clear`+`ViewUndo/Redo` 5件に `WidgetWithChildrenShortcut` 付与。
- **第3弾(①〜④):** 新ID `ProjectClearSearch`/`TimelineFocusSearch`/`TimelineClearSearch`/`CompositionImmersiveExit`(既定Esc/Esc/Find/Esc、表示名・永続化キー・設定画面コンテキスト付き)を追加。変更通知はQtシグナルではなく `addChangeListener`/`removeChangeListener`+`revision()` の軽量オブザーバで実装(AGENTS.mdの新規シグナルスロット禁止に配慮、単一GUIスレッド前提)。4ウィジェット(Timeline/Project/CompositionEditor/EditMenu)に `updateShortcuts()` を設け、作成時のコピーを通知駆動で再適用。`EditMenu::rebuildMenu()` から `setShortcut` を除去し、有効/文言のみに分離。
- **価値または懸念:** Esc/Find横取りと `Ctrl+D`/`F2` 二重定義の競合表示を縮小。設定変更が再起動なしで4面へ反映。残存は `CompositionRenderWidget` モーダルEsc群、`LayerPanelWidget`、`ContentsViewer J/K/L`、`File/Animation/EffectMenu` 固定群と比較系QShortcut群(新規IDが必要)。`loadFromJson` はID毎に通知が飛ぶため一括適用時は複数回更新が走る(安価だが将来集約可)。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。設定変更→4面即時反映、空バインド無効化、同一コンテキスト競合検出を確認すること。

## 2026-09-11 — Clone生成数の上限(巨大グリッドのハング防止)

- **関連:** `Artifact/src/Layer/ArtifactCloneLayer.cppm`(`generateCloneData`)。
- **確認できた事実:** clone数・grid各次元・radial数は下限1のみで上限なし。Gridは`cols*rows*depth`がint64オーバーフローし得て、毎drawの生成でハングする。描画側は4096でclamp済みだが生成側が無制限だった。
- **対応:** `kMaxGeneratedClones=4096`を全モードへ適用。Gridはint64で積算しreserveはcap、3重loopはprefix順でbreak。既定小規模の見た目は不変。
- **価値または懸念:** 上限超過は黙って切捨て(描画clampと同一方針)。UIへの上限表示は未追加。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。大grid指定時の打切りを確認すること。

## 2026-09-11 — カメラのクリックフォーカス(Alt+ダブルクリック)

- **関連:** `ArtifactCompositionRenderController.ixx/.cppm`(`focusActiveCameraAtViewportPos`)、`ArtifactCompositionEditor.cppm`(`mouseDoubleClickEvent`+操作ヒント)。
- **確認できた事実:** 3Dレイ・三角ヒット距離(`intersectModelLayerPickingRay`)とactive camera解決が既存。フォーカス面ギズモ(`ArtifactCameraLayer.cppm:179-182`)・DOF/MB実配線も済みで、欠落はヒット点→フォーカス距離の導線だけだった。
- **対応:** ヒット点をactive camera前方軸へ投影した真のフォーカス面距離を near/far でclampし、既存property path(`Camera Options/Focus Distance`)経由で設定。Alt+単クリック=オービット・素ダブルクリック=既存動作は不変。新規signalなし。
- **価値または懸念:** depth readback不要でview非依存。2D層・空振り・非正ヒットは無操作。skinning変形後の頂点には未対応(静止mesh基準)。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。Alt+ダブルクリックでのフォーカス変化とHUD表示を確認すること。

## 2026-09-11 — 3Dマテリアル環境強度(IBL間接光の個別スケール)

- **関連:** `ArtifactCore/.../Material`、`MeshRenderer`(`MaterialConstants` 32→36 floats、`EnvFactors`、PS乗算)、`ArtifactIRenderer::drawMesh`、`Artifact3DLayer`(JSON/property/signature)。
- **確認できた事実:** 環境強度はグローバル(`EnvironmentSettings.y`)のみで、マテリアル別の反射調整(E3Dの常用操作)がなかった。
- **対応:** `environmentIntensity_` (0〜4、既定1)を追加し、間接diffuse/specular/transmission合算へ乗算。ベースambient・直接光は不変。既定値1で既存見た目不変。
- **価値または懸念:** Look-devの基本操作を低コストで補完。環境なし時は効果なし。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。0/1/2での間接光変化と保存再読込を確認すること。

## 2026-09-11 — 3Dアニメ再生モード/速度(Loop固定の拡張)

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` (Impl/draw/toJson/fromJson/property)。
- **確認できた事実:** draw時のclip評価は`fmod`固定Loopで、Holdや速度調整がなかった。毎フレーム`loadFromFileAtTime`再importは既存仕様のため対象外。
- **対応:** `animationPlaybackMode_` (0 Loop/1 Hold/2 PingPongの無状態三角波)+`animationSpeed_` (0〜8、0は静止)を追加し、評価・JSON・property・setterへ接続。変更時は`lastSkinAnimationFrame_`を無効化して同フレームでも再評価。
- **価値または懸念:** E3D式の再生制御の最小形。PingPong・逆再生・Bakeは対象外。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。Loop/Hold/速度0・2倍と保存再読込を確認すること。

## 2026-09-11 — Cloner 3D最小接続(Phase2→描画の接続)

- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`、`Artifact/src/Render/ArtifactIRenderer.cppm`(`drawMesh`/`drawMeshInstanced`)、`ArtifactCore/.../MeshRenderer`(`maxInstances`)、`Artifact/include/Layer/Artifact3DModelLayer.ixx`(`material()`追加)、`Artifact/src/Layer/ArtifactCloneLayer.cppm`(`drawInstancedSource`)。
- **確認できた事実:** `getInstanceData()`は呼出し元ゼロ、`CloneLayer::draw()`は2D矩形のみでソース内容を見ない。`MeshRenderer::draw(ctx,count)`はN instance対応済みだが`initialize(1,...)`でinstance bufferが1固定だった。
- **対応:** `Impl::drawMesh`へinstance配列引数を追加し、容量不足時は`initialize(wanted,...)`+再upload、ray登録・mesh-shader LODは単一時のみ、ID pass時のみ複写。Clone側は`sourceLayerId→layerById→Artifact3DLayer`解決(`Procedural3D`と同型)、clone行列×globalのworld化+層opacity bake、`clone3d|src|rev`キーで`drawMeshInstanced`。Phase2変換器は転置なし複写のためGPU提出に再利用せず新helperで置換(旧関数は残す)。
- **価値または懸念:** Grid/Random/Linear等の既存配置・effectorが3Dメッシュにそのまま適用、頂点色/UV変換/影は同一経路で効く。4096上限、source可視時の二重描画は仕様未定、debug shading mode・rayは単一のみ。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。3Dソース指定時の散布・影・保存再読込を確認すること。

## 2026-09-11 — 3D読込時PBR係数(metallic/roughness factor)の適用

- **関連:** `ArtifactCore/include/Geometry/MeshImporter.ixx`、`ArtifactCore/src/Geometry/MeshImporter.cppm`(`detectTexturesFromUfbx`)、`Artifact/src/Layer/Artifact3DModelLayer.cppm`(`loadFromFile`)。
- **確認できた事実:** 読込はテクスチャパスのみ採取し、glTF/FBXのmetallic/roughnessスカラー係数を捨てていた。`ufbx_material_map`は`has_value`/`value_real`を持つため未指定と既定値1.0を区別できる。base-color係数はsRGB/linear曖昧のため対象外。
- **対応:** 先勝ちで係数採取(`hasLastMetallicFactor`/`lastMetallicFactor`等、ufbxパスのみ有効)。層側はufbx系backendかつマテリアルが既定値(metallic 0.0/roughness 0.5)の場合のみ適用し、ユーザー編集の上書きとtimed再評価経路への波及なし。
- **価値または懸念:** 金属質glTFが誘電体表示になる誤りを解消。色係数は色空間メタ欠落(`FloatColor`素float4)のため別途要設計。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。metallic glTFでの質感と既存JSON再読込を確認すること。

## 2026-09-11 — 3D層Transformの3軸露出(Core済み・UI未整理の接続)

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`(`getLayerPropertyGroups`、`transformChannelProperty`)。Core `AnimatableTransform3D`はX/Y/Z・保存・行列・ギズモ済み。
- **確認できた事実:** 共有Transformグループは2D subset(posX/Y・scaleX/Y・単一rotation・anchorX/Y)のみで、`transform.rotation.x/y`等のchannel pathは解決できるのにUIに露出していなかった。`transform.rotation.z`のpath自体が未定義だった(Rotation=Z互換の別名なし)。
- **対応:** `transform.rotation.z`→Rotation channelの別名を追加。`is3D()`時のみ同TransformグループへPosition Z・Rotation X・Rotation Y・Rotation Z(alias)・Scale Z・Anchor Zを追加し、既存Rotation表示を「Rotation Z」へ(3Dのみ)。新規グループ・signalなし。
- **価値または懸念:** 3軸編集・キーフレーム・式解決が既存Property経路で通る。2D層の表示は不変。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。3D層での表示・編集・保存再読込を確認すること。

## 2026-09-11 — 3Dソフトシャドウ仕上げ(UI飽和の解消)

- **関連:** `Artifact/src/Layer/ArtifactLightLayer.cppm`。受渡しは`ArtifactCompositionRenderController.cppm:4945-4946`(radius/10→softness)、`ArtifactIRenderer.cppm:1098-1106`、`MeshRenderer.cppm:892-903,3143-3160`で接続済み。
- **確認できた事実:** UI hard 0〜500・soft 0〜200に対し、MeshRendererはsoftness 0〜2へclampするため、radius 20超は描画不変だった。`setShadowRadius()`にfinite/clampがなく、fromJsonも無制限だった。
- **対応:** 格納を0〜20へclamp(既定10)、UI hard 0〜20・soft 0〜10へ整合、tooltip/whatsthisを「20 = softest」へ修正。fromJsonとproperty setterは同setter経由で自動整合。
- **価値または懸念:** 単一caster・Directional/Spotのみ・CSM/Point cubeなしの範囲は不変(コントローラ側に明記)。見た目の既定値は不変。
- **次に確認:** ビルド・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。radius 0/10/20の影 edge を確認すること。

## 2026-09-11 — 3Dテクスチャトランスフォーム共有UV(offset/scale/rotation)

- **関連:** `ArtifactCore/include/Material/Material.ixx`、`ArtifactCore/src/Material/Material.cppm`、`ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- **確認できた事実:** テクスチャトランスフォームは存在せず、glTF `KHR_texture_transform`相当も未対応。全テクスチャが`In.UV`直サンプリングだった。
- **対応:** Materialへ共有UV `offsetU/V(-10〜10)`・`scaleU/V(0.01〜10)`・`rotation度(-360〜360)`を追加。`MaterialConstants`へ`uvTransformA/B` 8 floats追加(24→32 floats、static_assert更新)。PSは`transformMeshUv()`でscale→UV中心回転→offsetを適用し、全6テクスチャと法線マップ接線フレームへ反映。`ArtifactIRenderer::drawMesh()`で受渡し、3D層のJSON/property/signatureへ接続。既定値(0,0,1,1,0)は恒等で既存見た目不変。
- **価値または懸念:** E3D/他DCCの質感調整の基本操作を低コストで補完。per-texture個別変換・glTF import時自動反映は対象外。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。回転中心・法線接線・旧JSON既定値を確認すること。

## 2026-09-11 — 3D頂点カラー描画反映(MeshImporter保持→Mesh描画断絶の接続)

- **関連:** `ArtifactCore/include/Mesh/Mesh.ixx`、`ArtifactCore/src/Mesh/Mesh.cppm`、`ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`。メモ:`docs/analysis/THREED_AE_E3D_C4D_GAP_IMPROVEMENT_2026-09-11.md`。
- **確認できた事実:** `MeshImporter.cppm:880-884`はufbx頂点カラーを`color`属性へ保持するが、`Mesh::generateRenderData()`はcolorsを持たず、`MeshRenderer::updateMeshGeometry()`も位置/法線/UVのみで頂点カラーバッファ・シェーダ入力がなかった。Wicked系`surfaceHF/objectHF`の頂点色対応とは別系統のMesh PBR経路が対象。
- **対応:** `RenderData`へ`colors`追加、生成時に`color`属性から展開(欠落時白)、meshlet remap用`PackedVertex`へcolorを含めて異色頂点の統合を防止。`MeshRenderer`へ`pColorBuffer_`追加、VS `ATTRIB3`/PS `TEXCOORD7`で受渡し、baseColorへ`vertexColor(rgb linear, a)`乗算。欠落時は白で既存見た目不変。`ArtifactIRenderer::drawMesh()`でcolors構築・hash・upload。
- **価値または懸念:** glTF/PLY等の頂点色付き資産がそのままPBR表示される。確保は形状キャッシュ更新時のコールドパスのみ。ホットパスはバインド済みバッファ参照のみ。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施(AGENTS.md制約でユーザー許可待ち)。頂点色付きglTF/PLYでの色反映、白資産の不変、D3D12/VulkanのATTRIB3レイアウトを確認すること。

## 2026-09-10 — シェイプレイヤーの画像エフェクト適用時のサーフェスキャッシュ統合と最適化

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`ArtifactShapeLayer.cppm`。
- **確認できた事実:** シェイプレイヤー（`ArtifactShapeLayer`）に画像エフェクト（`EffectPipelineStage::Rasterizer`）やマスクが適用された際、`drawLayerForCompositionView` 内に ShapeLayer の専用分岐がなくフォールバック描画（`layer->draw(renderer)`）に落ちていた。また、エフェクト適用時のラスタライズ結果をキャッシュするキー（`buildLayerSurfaceCacheKey`）にシェイプの形状・アニメーション判定が含まれておらず、毎フレームCPUでのフルラスタライズおよびGPU再アップロードが走って激重となっていた。
- **対応:** `buildLayerSurfaceCacheKey` に ShapeLayer の幅・高さ・タイプおよびアニメーションプロパティ（パスキーフレーム等）判定を追加し、静止時はキャッシュヒットするように改善。さらに `drawLayerForCompositionView`（コントローラ側およびビューポート描画側）に `dynamic_cast<ArtifactShapeLayer*>` 分岐を新設し、エフェクトまたはマスクが存在する場合にのみ `toQImage()` / `downsampleForLOD` を経由して `applySurfaceAndDraw`（サーフェスキャッシュと低解像度LODスケール）を通すように接続した。エフェクトなし時は従来の高速GPUベクターダイレクト描画を維持。
- **価値または懸念:** プレビュー時の解像度スケーリング（ドラフト時1/4など）や静止フレームでのサーフェス再利用が効くようになり、大幅なプレビュー軽量化を実現。将来的な完全オフスクリーンFBOレンダリング（GPU上でのラスタライズ＆コンピュートエフェクト結合）へのステップとなる。
- **次に確認:** ビルド不可環境ルールに基づきコンパイル・手動確認の要否をユーザーと連携。エフェクト適用時のプレビュー滑らかさおよびアニメーション更新時のキャッシュ破棄を確認すること。
- **2026-09-11 追記・確認事実:** 対応済みのRasterizer effect列については、Shape分岐が `shapeLayer->draw(renderer)` を再利用可能な layer RTV へ直接出力し、F32 GPU texture上で処理できる。既存の個別GPU effectは入力upload／staging readback／`WaitForIdle()` を含むものがあり、GPU実装であってもGPU常駐とは限らない。
- **対応:** `ArtifactAbstractEffect` に具体型非依存の `GpuRasterEffectDomain`／固定容量 `GpuSpatialEffectNode` を設け、pointwiseとspatialを順序どおり実行するGPU planへ接続した。Gaussian Blur、Sharpen、Vignette、Chromatic Aberration、Stripes、Hex Grid をDiligent共通のSRV/UAV compute passへ移し、対応ShapeではCPU画像境界を通さない。
- **懸念・次に確認:** LayerMask は `LayerMask::applyToImage()` のOpenCV実装だけで、Bezier path、feather、invert、各modeのGPU契約は未確立。マスクありを無理に部分GPU化せず、mask raster／alpha-composite passを仕様化してからGPU化する。対応エフェクトのCPU/GPU pixel parity、アニメーション時のframe time、D3D12/Vulkan両backendでのshader compilationをビルド後に確認する。

## 2026-09-10 — Shape F6 open/closed・smooth/corner-bezier メインVP移植

- **関連:** `Artifact/src/Widgets/LayerEditorGeometry.cppm`（新`togglePathVertexSmooth`）・`LayerEditorContextMenu.cppm`（Solo TogglePathSmooth）・`ArtifactCompositionRenderController.ixx/.cppm`（hovered頂点5メソッド）・`ArtifactCompositionEditor.cppm`（右クリックShapeメニュー）。Artifactリポ内のみ、Core不変・新規ファイルなし・新規シグナルなし。
- **確認できた事実:** Solo側はOpen/Close・Smooth切替＋Undoが既存だがsmooth反転はフラグのみでtangent初期化なしだった（handle非表示のまま）。メインVP右クリックはmask分岐のみでshape分岐なし。`evaluatePathAt`はtangentを直接cubic評価するため、tangent初期化が描画・補間に直結する。`contextMenuEvent`は先頭で`handleMouseMove`済みのためhoverは新鮮。
- **対応:** Geometry共有ヘルパー追加（smooth化は隣接弦方向へ±ハンドル初期化・長さは隣接距離25%を4〜64pxにクランプ・既存非ゼロハンドルは保持、corner化は両ハンドル破棄、開パス端点は単一隣接方向）。Solo切替をヘルパーへ寄せ。メインVPは`hasHoveredShapePathVertex`/`hoveredShapePathVertexSmooth`/`isSelectedShapePathClosed`/`toggleHoveredShapePathClosed`/`toggleHoveredShapePathSmooth`を追加し、既存`ShapePathVertexEditCommand`＋delete系と同一ガード（pending作成中は無効・lock・drag中・3頂点未満のclose抑止）・同一Undo末尾処理で接続。右クリックはmask分岐踏襲のQMenu（Make Smooth/Corner＋Open/Close Path）で確定。
- **価値または懸念:** marquee・multi-move・proportional・handle-only選択はF12残件として対象外。smooth化のハンドル長は固定ヒューリスティック（ズーム非依存・local px）。
- **次に確認:** ビルド・`check_module_hygiene`・実機runtimeは未実施（AGENTS.md制約でユーザー許可待ち）。特に`.cppm`追加import（Geometry）のdyndep、右クリック時のhover更新、`contextMenuEvent`のShape/menuフォールバック順、旧JSON再読込（ix/iy/ox/oy/smoothキーは既存のため互換のはず）を確認すること。

## 2026-09-10 — Shape Core縦断（WavePaths新設・Repeater複合順・SVG多段化）

- **関連:** `ArtifactCore/.../ShapeOperator.ixx`、`AeOperators.ixx`、`Repeater.ixx`、`ShapeTypes.ixx`、`ShapeGroup.cppm`、`ShapeLayer.cppm`、`ArtifactShapeLayer.cppm`、`LayerEditorContextMenu.*`、`ArtifactCompositionRenderOverlay.cppm`、`ArtifactCompositionRenderController.cppm`。子リポ編集はユーザー許可済み。
- **確認できた事実:** Wave operatorはCoreに不存在、Repeater複合順フィールドも不存在、SVG出力は2-stop固定だった。Trim同時/個別・Repeater本体は実装済み。親子ともoperator type分岐はdefault付きで新enum値に安全だった。
- **対応:** Coreに`WavePaths`（振幅・周波数・位相、弧長一様サイン変位、clone/JSON/process、`ShapeGroup` factory含む）、Repeater `compositeBelow`（clone/JSON/process末尾反転）、`FillSettings::gradientStops`+SVG `<stop>`列出力（空=従来2-stop、キャッシュキーにstops混入）を追加。親側はcreate/name/value読取/property群/setter/正規化/KF検出（`phase`/`composite`含む）/時刻評価/Solo追加メニュー/HUD表示・詳細/VPダイヤ量編集/`toCoreShapeLayer` stops受渡を接続。
- **価値または懸念:** 新規`.ixx`なし・CMake変更なし・旧ファイルは未知type/欠落キーを無視して読める。Wave processは~4px細分（上限128/seg）で滑らかさを確保。
- **次に確認:** ビルド不可PCのため未検証。特にCore側`W_OBJECT_IMPL(WavePaths)`、Q_PROPERTY NOTIFY配線、SVGの`<stop>`列とキャッシュキー、旧版での新type=int 11読飛ばしを確認すること。

## 2026-09-10 — Audio Mini をモーショングラフィックス用時間ナビゲーターとして分離

- **関連:** `ArtifactAudioWaveform`、`AudioSyncTools::detectBeats()`、`ArtifactTimelineWidget`、`ArtifactTimelineTrackPainterView`、composition marker / keyframe snapshot Undo。
- **確認できた事実:** 音声レイヤーの peak/RMS 波形キャッシュとタイムライン描画は既存の正規経路である。`AudioSyncTools` はサンプル位置の beat 検出と tempo 推定を持ち、Keyframe Pattern には手入力 BPM の Beat Sync がある。一方、検出 beat / transient / section を composition 時刻・marker・snap target・選択キー操作へ結ぶ編集契約と常設の全体波形 UI は未実装。
- **対応（2026-09-10）:** `ArtifactAudioMiniWidget` を通常 Timeline から独立した dock として追加した。最初に見つかる loaded audio layer の全体 waveform と、peak envelope のローカル最大値から得る transient cue を composition frame へ正規化して表示する。クリックで seek、`B` / `Shift+B` で次／前 cue へ移動できる。`ArtifactAnimationTimelineWidget` も別 dock とし、選択レイヤーの keyframe 時間域を `ENTER` / `ANIMATE` / `EXIT` の読み取り用 semantic span として表示・クリック seek できる。
- **価値または懸念:** main timeline に巨大な audio lane を常設せず、motion の時間合わせを速くできる。複数 audio layer 時の guide source 選択、tempo half/double の信頼度、section 推定の誤認、trim / slip / time-remap 後の sample→composition frame 対応、解析のバックグラウンド実行とキャッシュ無効化は先に固定が必要。
- **次に確認:** Waveform cache が保持する source offset と layer timing の対応を確認し、beat marker と keyframe snap の Undo 境界を定義する。transient / section 推定と自動 marker 大量生成は信頼度表示・preview / undo policy を決めてから追加する。

## 2026-09-10 — Shape F11/F13 表現系（stroke波・テーパーイーズ・Trim同時/個別・モーフ再サンプル）

- **関連:** `ArtifactShapeLayer.ixx/.cppm`、`ArtifactCompositionRenderOverlay.cppm`、`LayerEditorContextMenu.ixx/.cppm`。子リポ（ArtifactCore）のoperatorクラスはAGENTS.md制約で不変とした。
- **確認できた事実:** Trim同時/個別とRepeater本体はCore側に完成済みで親側の露出だけが不足、Wave operatorはCoreに存在せず、テーパーは線形rampのみ、頂点数不一致のパスKFはsnapだった。GPU/ソフトのstroke描画は`drawTaperedPolylineGPU`/`drawStrokePath`の2経路に集約され、legacy GPUは`GpuPaintItem.stroke`（=ShapeContentStroke）経由だった。
- **対応:** strokeに`waveEnabled/Amount/Frequency/Phase`+`taperEase`を追加し、新旧両描画経路・新旧JSON・property（wave系とeaseはキーフレーム可、`hasAnimatedShapeGeometry`の検出にも追加）へ接続。wave無効/量ゼロ時は従来経路と同一結果になる早期設計。HUDのTrim行へM:Sim/Ind表示、Solo ViewのManage Operatorsへ同時/個別トグル（JSONスナップショットUndo再利用）。頂点数不一致KFは`ShapePath::interpolate`+等間隔再サンプルでモーフ補間し、失敗時のみsnapへ縮退。
- **価値または懸念:** Repeater Composite順・SVG多段出力・contents stroke KF・式/pick-whip配線は親だけでは完結しないため対象外（Core改修または別器が必要）。モーフ再サンプルは不一致KF区間のみ毎フレーム64点評価する。
- **次に確認:** ビルド不可PCのため未検証（ユーザー申告）。特に描画シグネチャ変更の呼び出し3件、property order id -191〜-187、旧JSON再読込、wave+Dash併用時のDash優先を確認すること。

## 2026-09-10 — Shape F1/F2/F4/F5/F9/F10 メインVP移植とfill拡充

- **関連:** `ArtifactCompositionRenderController.cppm`、`ArtifactCompositionRenderOverlay.cppm`(+`.ixx`)、`ArtifactShapeLayer.ixx/.cppm`、`ArtifactCompositionEditor.cppm`、`ArtifactLayerMenu.cppm`、`ArtifactCompositionLayerUndoCommands.cppm`、`ArtifactPropertyWidgetShared.cppm`。
- **確認できた事実:** Solo View側のShape編集資産（頂点/tangent/segment grammar、角丸/星ハンドル、operator stack、Pen生成、頂点KF）は実装済みだったが、メインVP側は頂点ドラッグの断片と点描画のみで、ホバー・選択保持・パラメータハンドル・operator HUD・SVG入力導線・多段グラデーションが未接続だった。SVGパース（`parseShapeContentsFromSvg`）と`ShapeContent`モデル自体は存在し、UI呼び出しだけがなかった。
- **対応:** F1 overlay強調（頂点/tangent/選択/挿入マーカー/開閉、DTO渡し）+ホバー更新、F2 角丸/星ドラッグ（既存`ShapeCornerRadiusUndoCommand`+新`ShapeStarInnerRadiusUndoCommand`）とポリゴン頂点ドラッグ/Shift挿入（新`ShapePolygonPointsUndoCommand`）、F4 選択文法（Shift toggle/Ctrl add/置換、ボディクリック解除、Delete/Backspace削除、Ctrl+A、Escape解除）、F5 operator HUD（常時パネル）+Trim三角/ダイヤハンドル（新`ShapeOperatorValueUndoCommand`、`shapeOperatorValue`読取API）、F9 レイヤーメニュー2導線（SVG取込は新`ShapeSvgImportUndoCommand`、ベクターから作成は`AddLayerCommand`取引）、F10 マルチストップ（`ShapeGradientStop`+`gradientStops`、CPU/QGradient/JSON/property露出、空=従来2色に縮退）。
- **価値または懸念:** いずれも既存関数の拡張と既存パターンの再利用に留め、新規モジュール・CMake変更・シグナル追加なし。`WigglePaths`無条件キャッシュ回避、SVGグラデーションのCore単色縮退、stroke多段・Noise/pattern fill、Repeater対話編集は対象外として残る。
- **次に確認:** ビルド（`check_module_hygiene`含む）と実機runtime検証は未実施（AGENTS.md制約でユーザー許可待ち）。特にF5のTrim百分率クランプ、F2 Starクランプ（Solo View踏襲0.05〜0.99）、F4 Deleteのmask優先順位、F10旧JSON再読込互換を確認すること。

## 2026-09-10 — Composition VP の直接編集導線（V1〜V6）

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`ArtifactCompositionRenderController.cppm`、`TransformGizmo.cppm`。
- **確認できた事実:** 画像クロップ、平面サイズ／グラデーション、Fit／Align／Distribute、画像ソース差し替え、アンカー編集のコア処理は途中実装として既存ギズモ／数値プロパティと接続済みだったが、差し替え時の配置選択、9点プリセット、Comp／他レイヤーガイドへのアンカースナップ、Fit後の結果枠表示、VP下部の整列導線が不足していた。
- **対応（2026-09-10）:** 無修飾ドロップは変形を維持し、Shiftドロップは新素材を原寸・Comp中央へ置くUndo付き差し替えへ拡張。アンカー9点を既存Pivotメニューから選択できるようにし、CtrlスナップはComp端／中心と他レイヤーの可視境界へ拡張。Fit／Fill／Stretch後はComp枠と結果枠をVPへ重ね、下部Arrange HUDから同操作へ到達できるようにした。
- **懸念／未検証:** Shiftの配置リセットは現在フレームのTransform3Dへ書き込み、既存アニメーションの他時刻は変更しない。Qt／C++20 moduleのビルド、D&Dの実機修飾キー取得、回転・親子変形下のアンカーガイド位置、Fit結果枠の複数選択表示は未検証（ビルド・実行はユーザー許可後）。

## 2026-09-09 — ポイントトラッカーのコンポジション切替境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `setComposition()` / `trackerDelete()`。
- **確認できた事実:** トラッカーは `CompositionRenderController` が `TrackerManager` から一時生成し、現在の選択レイヤーをオフスクリーン取得して解析するコントローラローカル状態である。
- **対応:** 同一コンポジションの再バインド時は解析状態を維持し、別コンポジション（同一 ID の置換インスタンスを含む）へ切り替える場合だけトラッカーを停止・破棄する。
- **価値／懸念:** 前コンポジションの軌跡や ROI が新しいコンポジションに残る誤表示と、`TrackerManager` に残る一時トラッカーを防ぐ。永続保存が必要な場合は、コントローラ一時状態とは別のプロジェクト保存契約を設計する必要がある。
- **次に確認:** コンポジション切替を含む UI 操作でトラッカーパネルの表示・非表示とギズモ再接続が期待通りになるかを手動確認する（ビルド／実行はユーザー許可後）。

## 2026-09-09 — 単一フレームのポイントトラッキング

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm` の `trackRange()` / `trackBackwardRange()`。
- **確認できた事実:** 既存の成功条件は「シード後に少なくとも 1 ステップの信頼できる測定があること」だったため、フレームが 1 枚だけの静止画では有効なシード結果まで `false` になっていた。
- **対応:** サンプル数が 1 の場合はシードフレームを有効結果として扱い、2 フレーム以上では従来どおり測定ステップを要求する。
- **価値／懸念:** 単一フレームでも位置／アンカー適用を使える。移動量や品質を推定したわけではないため、長い範囲の品質判定は緩めていない。
- **次に確認:** 単一フレームの UI 操作で適用ボタンがシード位置を使うことを手動確認する（ビルド／実行はユーザー許可後）。

## 2026-09-09 — コントローラ破棄時の一時トラッカー解放

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `destroy()`。
- **確認できた事実:** 解析ジョブの停止・待機は行われていたが、`TrackerManager` に登録したコントローラ専用トラッカーを破棄する経路がコンポジション切替以外にはなかった。
- **対応:** 破棄時も `trackerDelete()` を通し、ジョブ待機後に manager から除去し、ギズモ参照を切る。
- **価値／懸念:** エディタ再生成や renderer 再初期化を繰り返しても、古いポイント軌跡と manager 所有オブジェクトが残らない。破棄中は再描画を要求するだけで、GPU リソース解放順序は従来のまま。
- **次に確認:** エディタタブの閉じる／再オープンを繰り返したときの tracker 数とパネル状態を手動確認する（ビルド／実行はユーザー許可後）。

# Insight Register

## 2026-09-09 — Planar Tracker の連番・非同期実行境界

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm`、`ArtifactCore/src/Tracking/PlanarTracker.cppm`、`Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Tool/ArtifactPointTrackerTool.cppm`。
- **確認事実:** `MotionTracker` のPlanarモードは4点＋ROI、Shi-Tomasi/PyrLK、RANSAC、ECC fallback、homography/confidence/JSONを持つ。Composition VPにはPlanar切替、4点投影overlay、Forward/Backward/All実行、Corner Pin keyframeへのUndo付き適用が接続済みだった。一方、旧Forward/Backwardは範囲内の連番ではなく始点・終点の1ペアだけを解いており、Backwardの画像入力順も現在点から過去点への写像と逆だった。
- **対応（2026-09-09）:** 最初の受入対象を選択中の静止画／画像連番レイヤーに限定した。GPU offscreen取得はUIスレッド上で1フレームずつイベントループへ返し、OpenCV solveは共有background poolへ分離した。Forward/Allは隣接フレーム順、Backwardは新設した`trackBackwardRange()`で逆順の隣接フレームを追い、VP toolbarにBackward／Stop／Forward／Allと進捗HUDを追加した。Cancel、tracker削除、controller破棄ではjob寿命を収束させる。
- **懸念:** `setFrame()`は互換境界の`QImage`から内部`cv::Mat`へ正規化して全対象フレームを保持するため、UIの連続停止は避けられても長尺・高解像度ではCPUメモリ量が大きい。GPU readback自体は安全のためUIスレッドに残しており、1フレームのreadback時間は隠蔽しない。動画素材、部分結果の採用、problem frame reviewは今回の対象外。
- **価値／次に確認:** 短い静止画連番で4点ROI→Forward/Backward/All→途中Stop→overlay→Corner Pin Bakeを実機確認する。次段ではリングバッファによる逐次solve、native frame snapshot、失敗フレームのreview UIを検討する。

## 2026-09-09 — Point Tracker の現状とPlanar共存

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm`、`Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認事実:** Point modeはPyrLKベースの単一点追跡、feature/search boxのサイズ調整、motion path表示、path pointの手動補正、Position／Anchor／Nullへの適用を既に持つ。Tracking実行はPlanar専用に固定されておらず、Tracker typeをPointに戻せば同じ非同期範囲jobを利用できる。
- **対応（2026-09-09）:** Planar job開始時の強制的なtype変更を外し、Point／PlanarのTracker typeを保持するようにした。VPにPoint／Planarのモードボタンとコンテキストメニュー導線を追加し、Point modeではROI外のクリックで追跡点を直接配置できるようにした。Gizmoの現在位置表示もフレーム番号の丸めではなく、結果フレームのtimeに最も近いpath pointを選ぶよう修正した。Point modeではFeature枠からLK windowを設定し、Search枠をPyrLK/NCC候補の境界として解析へ渡すようにした。完了HUDにはproblem frame数を表示し、Reviewボタン／メニューから次のproblem frameへジャンプできるようにした。
- **追加対応（2026-09-09）:** Forward／Backward範囲解析は開始フレームだけで結果を有効化せず、少なくとも1つの隣接フレームが信頼度閾値を通過した場合だけ成功扱いにした。全失敗トラックが誤ってBake可能になる経路を閉じた。
- **追加対応（2026-09-09）:** Point modeでROI外へ再配置した場合は旧トラック結果をクリアし、再配置点と過去のmotion pathが混在しないようにした。
- **追加対応（2026-09-09）:** Point modeのFeature枠はNCC template sizeにも反映し、Search枠全体を候補範囲として探索できるようにした。大きなSearch枠では探索コストが増えるため、実機で上限とPreview品質のバランスを確認する。
- **追加対応（2026-09-09）:** Point modeのpath point hit-testをROI内部より先に評価し、追跡後もFeature枠内の点を直接ドラッグ補正できるようにした。
- **懸念／次に確認:** Point modeには検索品質のproblem-frame一覧、テンプレート／補正履歴のreview UI、multi-pointの個別品質表示がまだない。まず単一点の短い連番で初期点→Forward→path correction→Bakeを確認し、Planarとの差をUI上のTrack Type選択へ整理する。

## 2026-09-09 — Composition VP と Layer Solo View のマスク編集能力差

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm`、`Artifact/src/Widgets/LayerEditorMaskOverlay.cppm`、`Artifact/src/Widgets/LayerEditorMaskDragController.cppm`。
- **確認事実:** Composition VP は複数頂点選択、ラバーバンド選択、辺上頂点挿入、cubic Bezier の表示・hit-test、in/out tangent と feather handle、linked/broken 表示、Alt 分離、Ctrl reset、Undo snapshot を持つ。一方 Layer Solo View は既存 anchor／in-out handle の hit-test・単点 drag・Delete・close・Undo・proportional edit は持つが、overlay のセグメント描画は anchor 間の直線で、複数頂点選択、辺上挿入、feather handle、linked/broken tangent 操作文法、マスク新規作成の経路は確認できなかった。
- **対応（2026-09-09）:** `MaskVertexAddress` を共通の選択アドレスとして `MaskPath` module に置き、Composition VP と Layer Solo View の選択集合を同じ型にした。Solo View は18分割Cubic Bezierの描画・segment hit-test、単一／Shift追加／矩形選択、選択集合の一括移動・削除へ対応した。overlay には選択集合をポインタで渡し、フレームごとのコピー確保を避けた。
- **価値／懸念:** Solo View でも曲線上の選択と複数頂点操作がComposition VPに近づいた。Bezierサンプラー実装自体は両面に重複しているため、完全な数値共有は今後の検討対象。プロパティ／モード導線と実機受入れも別途確認が必要。
- **次に確認:** 実機で曲線表示、セグメント選択、Shift追加、矩形選択、一括移動／削除、Undoを確認する。続いてAlt/Ctrl tangent操作とfeather handleのSolo View parityを判断する。

## 2026-09-09 — Render Queue のフレーム単位ログ flush

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` の `processFramesForJob()`。
- **確認事実:** フレームの render begin／render end／encode begin／encode end 周辺で `Logger::flushFile()` がフレームごとに複数回呼ばれている。GPU readback、画像変換、プレビュー生成と同じ逐次処理経路にあり、ストレージ待ちをフレーム処理へ直接持ち込む構造になっている。
- **未検証:** 実ジョブでの flush 所要時間、OS キャッシュやログ出力先による差、障害復旧時に必要な永続化粒度。
- **対応（2026-09-09）:** 詳細なframe begin/end・encode begin/endを既定無効の`artifact.render.queue.frames` categoryへ移し、通常時のストリーム整形を遅延評価した。同期flushはフレーム失敗、encoder拒否、ジョブ終了の境界へ集約した。
- **価値／懸念:** 通常レンダリングからフレーム単位のログ整形・mutex・ファイルflushを除外した。categoryを明示的に有効化すれば従来相当の詳細イベントは取得できるが、正常フレームごとの即時永続化は行わない。
- **次に確認:** 代表的なRender Queueジョブで通常時とcategory有効時のログ内容、失敗時ログ、終了時flushを確認する。

## 2026-09-08 — ArtifactAbstractLayer の C++20 module 実装を内部パーティションへ分割する

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/cmake/ArtifactSources.cmake`。
- **確認事実:** `ArtifactAbstractLayer.cppm` は約13,700行で、約1,000行の `ArtifactAbstractLayer::Impl` と、物理、コンポーネント、エフェクト、プロパティ、マスク、JSON保存を一つの実装モジュール単位に集約している。MSVC 14.51 は13,683行、14.52 Previewは5,026行および診断用の単純化後7,591行で IFC import を伴う C1001 を起こした。
- **対応・確認:** `Artifact.Layer.Abstract.Utilities` へ有限値clamp処理を切り出し、`QDebug` / `qWarning` のストリーム演算子を可変長書式のログ呼び出しへ置換した。MSVC 14.52 Preview と、障害を再現していた MSVC 14.51.36231 の双方で `ArtifactAbstractLayer.cppm` のオブジェクト生成が成功し、14.51では `Artifact` ターゲット全体のリンクと `bin/Debug/Artifact.exe` 再生成まで成功した。
- **価値／懸念:** 公開 `.ixx`、状態遷移、ログ内容を維持したまま、IFC境界でのストリーム型展開を除去できた。巨大な実装単位自体は残るため、将来は `Impl` 状態パーティションを導入し、物理／シリアライズ／プロパティ／マスクの順に責務別分割を進める。

## 2026-09-08 — Audio Mixer のパン編集トランザクション

- **関連:** `Artifact/src/Widgets/ArtifactCompositionAudioMixerPresentation.cppm` の `setPanChangedCallback` と `recordMixerLayerPropertyChange`。
- **確認事実:** 既存のパン編集は値変更ごとにUndoコマンドを記録する。音量フェーダーにはドラッグ開始／終了単位の記録経路があるが、パンには同じ境界がない。
- **未検証:** 長いパン操作で履歴が細分化する程度、およびLayerChangedによる行再構築がドラッグ継続へ与える影響。
- **価値／次に確認:** 実機でパンの連続ドラッグとUndo回数を確認し、必要なら既存のUndo統合仕様を調査して1操作へまとめる。今回は採用デザインの反映と操作部品の整備に留め、履歴システムの構造は変更していない。

## 2026-09-08 — プレビュー重さの主犯はテキスト毎フレーム shaping と平面グラデ再生成(対応済み3点)

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm:3057-3078` (plain stroke)、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm:643-670` (gradient分岐)、`Artifact/src/Render/PrimitiveRenderer2D.cppm:970-1024,1035-1090` (qCDebug/ sprite cache)、`Artifact/src/Render/DiligentImmediateSubmitter.cppm:1536-1780` (shape+atlas)。
- **事実:** plain text の stroke がレイヤー側で8方向 `drawTextTransformed` を発行し、submitter 側でパケット毎に `shapeGlyphsForRender + FontManager::makeFont + atlas.acquire` が再実行されていた。SolidImage の gradient 分岐が `currentFillImage()` のキャッシュを使わず `makeSolidGradientImage()` (QImage+QPainter 全画面生成) をクローン毎・毎フレーム実行していた。`drawSpriteTransformed / drawMaskedTextureLocal` が毎スプライト `qCDebug` と先頭4KB hash+IMMUTABLE 再生成を行っていた。
- **対応:** stroke 8連打を `outlineColor/outlineThickness` 付き単発 `drawTextTransformed` に集約 (submitter の8方向 outline に委譲)。gradient 分岐を `currentFillImage()` キャッシュ参照+opacity パラメータ渡しに変更 (QImage 新規生成なし)。ホットパスの `qCDebug` を撤去 (挙動不変)。
- **価値／懸念:** GPU 経路優先・QImage/QPainter 新規なし・signal 追加なし。stroke 見た目はシェーダ側 outline (対角 0.707 補正) に寄るため厳密には非同一 (未検証)。rich text (`QTextDocument` 毎フレーム再構築+run毎 shaping) は未着手で残る。
- **次に確認:** ユーザー環境でテキスト (plain/rich/stroke/shadow)・平面 (Solid/SolidImage gradient) の体感比較、rich 側の CacheKey 付き runs キャッシュ、plain 側 QFont キャッシュの要否。ビルド・実機計測は未実施 (ユーザー指示待ち)。

## 2026-09-07 — モーションパスUndoに残る24fps固定時刻

- **対応追記（2026-09-07）:** ユーザー依頼により対象コマンドをRationalTime保持（複数キーは時刻スケール保持）へ変更し、関連ドラッグ・確定処理もコンポfpsに統一。以下は修正前の調査記録。静的確認済み、実操作検証待ち。

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionMotionPathCommands.cppm` の位置・接線・複数キーUndo、`ArtifactCompositionRenderController.cppm` の過去Planeリリース処理。
- **事実:** これらには `RationalTime(frame, 24)` が残る。一方、通常の編集開始は `gizmoTransformTime` でコンポfpsを使う。
- **懸念（未検証）:** 非24fpsでUndo対象時刻がずれる可能性がある。今回追加した過去枠Scaleは開始時のRationalTimeをコマンドへ保持する。
- **次に確認:** 30/60fpsで位置・接線編集とUndoの対象キーを比較し、既存コマンドの時刻受け渡しを別途そろえる。

## 2026-09-06 — AnimatableTransform3Dの24fps固定量子化(未検証・要修正)

- **関連:** `ArtifactCore/src/Animation/AnimatableTransform3D.cppm` (`setPosition:355`、`positionXAt:498`ほか全域で`toFrameCount(24)`/`rescaledTo(24)`)、`ArtifactCore/include/Animation/AnimatableValue.ixx` (`addKeyFrame:225`は同フレーム上書き)。
- **事実:** Transform3Dのキー格納・評価がコンポfps無関係に24で量子化される。30fpsでは評価バケットが5フレームに1回重複([3,8,13,18,23,28]が前フレームと同値)、60fpsでは半数以上が重複。重複書込みは上書きでキーを潰す。コンポが24fpsの場合は無害。
- **価値／懸念:** 非24fpsコンポで平面等の移動が周期的に止まって跳ぶ「がたつき」の最有力原因(未検証)。修正はfpsの配管が必要で`AnimatableTransform3D`単体では完結しない。
- **次に確認:** ユーザーのコンポfpsと平面がアニメーション有りかを確認し、再現すればfpsパラメータ化を実施する。

## 2026-09-06 — カーブ/Gizmoの時刻スケールと二重書きの乖離

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` (`applyCurveEditorMove`、`writeBackCurveEditorStructureDiffs`、削除ハンドラ)、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`gizmoTransformTime`、`applyLiveGizmoTransform`)。
- **事実:** Gizmoは`RationalTime(frame, doubleのfps)`を暗黙のint64変換で作り(29.97→29)、カーブは`llround`(29.97→30)で作っていた。`RationalTime::operator==`は既約分数の厳密比較のため別時刻となり、カーブ移動の旧キー照合が失敗して無音破棄された。GizmoのPropertyミラー条件(`!empty || autoKey`)とTransform3D条件(`hasKey || animated || autoKey`)も不一致で片方だけ更新された。`addKeyFrame`は同時刻上書きのため移動先衝突で隣キーが消えた。
- **対応:** fpsは`llround`+下限1に統一、キー照合は`rescaledTo(fpsInt)`のフレーム番号比較に変更、移動先衝突は拒否、ミラー条件はTransform3D側に合わせた。未検証: ビルド・実機確認は未実施(ユーザー指示待ち)。
- **次に確認:** カーブ→Transform3D方向の逆同期(現状はPropertyのみ書き戻し)、`clear+再add`の一括置換のトランザクション化、既存の混合スケールキーの救済が必要か。
- **2026-09-09追加調査:** 単一フレームギズモの拡縮をアンカー固定に変更し、開始snapshotにPropertyの時間評価値を反映。通常レイヤーのtoJsonはTransform3DのpositionKeyframes/scaleKeyframes等を書き出す一方、PropertySerializationBridgeは同関数ではeffect用に使用されている。二重保存の解消は、ライブ編集のミラー削除だけでは保存データを失う恐れがある。共有チャンネルへの集約と、既存Transform3Dの初期値＋位置オフセット、補間、空間タンジェント、旧JSON、Undo snapshotの移行を一体で設計する必要がある。2026-09-09に共有Propertyチャンネルへ移行。Transform3Dは同じPropertyへの互換APIとし、JSONはchannelsへ一本化、旧配列は読込のみ。初期値読込はキーを生成しない。Undoはキーと基底値を同じPropertyへ復元する。ビルド・実機未検証。
- **保存互換性:** 新形式のTransformキーは`channelSchema: 1` / `channels`のみを正本にするため、旧アプリへの保存互換性はない。旧ファイルにそもそも保存されなかったProperty専用キーや初期オフセットを、移行処理で復元することはできない。旧配列が存在する範囲で読込を維持する。
- **2026-09-09追記（静的確認）:** 左ペインのキー切替はPropertyを直接変更し、UndoとsetDirtyを迂回していたため共通TimelineKeyframeModelへ統合。LayerChangedで最終プレビューとoverlayキャッシュの更新漏れも修正。共有化後はmotionPathPositionKeyTimesのnativeフォールバックも同じキー集合を読む。空間タンジェントは対応する位置キーが残る場合だけ有効にした。次に、位置X単独削除・Y保持・空間タンジェント・Undo/Redo・保存再読込を同じキー正本で成立させる境界を確認する（実機未検証）。

## 2026-09-06 — Render Queue全消去を永続化する

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **事実:** 個別削除は `handleJobRemoved()` 経由で `render-queue.json` を更新していたが、`removeAllRenderQueues()` は全消去後に `persistQueueState()` を呼んでいなかった。そのため再起動時の `loadPersistentQueue()` で過去ジョブが復元され得た。
- **対応:** 全消去後に `impl_->persistQueueState()` を追加した。完了履歴の削除責務は既存の `clearCompletedJobHistory()` に残した。
- **価値／懸念:** 「全削除した過去キューが再起動後に戻る」経路を塞げる。ビルド・実機確認は未実施。

## 2026-09-06 — D3D12アダプタ列挙に有効なFeature Levelを渡す

- **関連:** `Artifact/src/Render/DiligentDeviceManager.cppm`、Diligent `EngineFactoryD3DBase`。
- **事実:** D3D12の `selectGpuAdapter()` が `EnumerateAdapters(Version{})` を呼び、Diligentの `GetD3DFeatureLevel()` にMajor/Minorが0の無効なVersionを渡してDebug assertionを発生させていた。
- **対応:** D3D12最小Feature Levelである `Version{11, 0}` を2回の列挙呼び出しへ指定した。
- **価値／懸念:** DiligentのD3D12アダプタ列挙契約に一致する。Vulkan経路やDiligentEngine本体は変更していない。ビルド・実機確認は未実施。

## 2026-09-06 — TimelineのRAM previewイベントからUIスレッドへ復帰する

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Service/ArtifactPlaybackService.cppm`。
- **事実:** Render Queue由来の `PlaybackRamPreviewStatsChangedEvent` が発行元スレッドでTimeline購読コールバックを実行し、`updateCacheVisuals()` 内の `QWidget::setToolTip()` が所有スレッド外から呼ばれてQt assertで停止していた。Stateイベントも同じ経路を持ち得る。
- **対応:** 両イベントの購読コールバックから `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` でTimeline所有スレッドへ処理を転送した。
- **価値／懸念:** Render Queue実行中のUI操作をQtのスレッド規約に揃えられる。ビルド・実機再生による確認は未実施。

## 2026-09-06 — MpmSnapshotのmap値をSharedPtr化してMSVCのvector ICEを回避する

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`。
- **事実:** MSVC 14.51が `std::map<LayerID, std::map<int64_t, MpmSnapshot2D>>` の値型デストラクタ展開中に、`MpmSnapshot2D` 内の `std::vector` でC1001／Access Violationを起こしていた。
- **対応:** `materialSnapshots_` の値を既存の `SharedPtr<MpmSnapshot2D>` に変更し、保存・検証・復元箇所で明示的に生成／デリファレンスした。
- **価値／懸念:** スナップショット内容とキャッシュ制御は維持しつつ、IFC経由のvectorデストラクタ実体化をmap値型から外せる。ビルドは未実施。

## 2026-09-06 — ProjectManagerWidgetはGenerationPreset型を直接importする

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/include/Layer/ArtifactGenerationPreset.ixx`、`Artifact/include/Layer/ArtifactGenerationPresetLibrary.ixx`。
- **事実:** Widgetは `ArtifactGenerationPreset` を直接使っていたが、`GenerationPresetLibrary` は型定義モジュールを再エクスポートしていないため、利用側で型が未定義になっていた。
- **対応:** `import Artifact.Layer.GenerationPreset;` をライブラリimportの前に追加した。
- **価値／懸念:** C2065および後続のconst int誤推論を、再エクスポート拡張なしで解消できる。ビルドは未実施。

## 2026-09-06 — SolidLayerテスト実装をArtifactのモジュールmanifestへ登録する

- **関連:** `Artifact/cmake/ArtifactSources.cmake`、`Artifact/src/Test/ArtifactTestSolidLayer.cppm`、`Artifact/src/Test.cppm`。
- **事実:** `ArtifactTestSolidLayer.cppm` は `Artifact.Test.SolidLayer` をexportし、`Test.cppm`も同モジュールをimportしていたが、Artifactの明示的なソースmanifestに実装ファイルが未登録だった。
- **対応:** `ARTIFACT_APP_IMPL_SOURCES` 相当のテスト実装一覧へ `ArtifactTestSolidLayer.cppm` を追加した。
- **価値／懸念:** C2230と連鎖する `runSolidLayerTests` 未定義を、モジュール依存追加ではなく正しいソース登録で解消できる。CMake再生成・ビルドは未実施。

## 2026-09-06 — GenerationPresetのネストラムダ捕捉型を明示する

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **事実:** プリセット追加用のネストラムダで `preset` の型解決がMSVCの診断上 `const int` として扱われ、`validateGenerationPreset` に渡せなかった。また `FrameRange` 初期化は関数宣言と解釈される形だった。型付きcapture initializerはこのMSVC環境で構文エラーになった。
- **対応:** 外側で `ArtifactGenerationPreset presetValue` をコピーして通常の値捕捉へ分離し、内側の参照を `presetValue` に統一した。さらに外側のcallbackを `std::function<void(const ArtifactGenerationPreset&)>` として明示した。`FrameRange` はブレース初期化へ変更した。
- **価値／懸念:** C2664とC4930を対象箇所だけで解消できる。ビルドによる確認は未実施。

## 2026-09-06 — AbstractLayerのMSVC内部エラーをローカルラムダ依存から分離する

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** MSVC 14.51 が巨大な `setLayerPropertyValue` 内で、ローカル `finiteClampedValue` を別のローカルラムダからcaptureする構造を処理中にC1001／アクセス違反で終了した。
- **対応:** clamp処理をArtifact名前空間内の無名名前空間関数へ移し、`setJointFloat` のcapture依存を除去した。
- **価値／懸念:** 挙動を変えずにMSVCのラムダcapture解析経路を単純化できる。再ビルドによる確認は未実施。

## 2026-09-06 — ShapePathテストのShapeOperator import名を実モジュール名に合わせる

- **関連:** `Artifact/src/Test/ArtifactTestShapePath.cppm`、`ArtifactCore/include/Shape/ShapeOperator.ixx`。
- **事実:** テストは `Shape.ShapeOperator` をimportしていたが、Coreの公開モジュール名は `Shape.Operator` だった。
- **対応:** importを `Shape.Operator` に修正した。
- **価値／懸念:** C2230を依存追加なしで解消できる。ビルドによる確認は未実施。

## 2026-09-06 — AbstractLayerの補助ラムダはローカルclamp関数を明示captureする

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** `setJointFloat` ラムダが、同じ関数スコープの `finiteClampedValue` を既定キャプチャなしで参照していた。
- **対応:** `finiteClampedValue` を参照captureに明示追加した。
- **価値／懸念:** C3493/C2326を最小修正で解消できる。ビルドによる確認は未実施。

## 2026-09-06 — AbstractLayerのRigidBody2D参照はPhysics2Dを直接importする

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/include/Physics/2D/Physics2D.ixx`。
- **事実:** `RigidBody2D` は `Physics2D` モジュールの `ArtifactCore` 型だが、AbstractLayerは `Physics.System` のimportだけで直接参照していた。`Physics.System` は再エクスポートではないため型が可視にならない。
- **対応:** 実装ファイル側に `import Physics2D;` を追加した。インターフェース側や広域依存は変更していない。
- **価値／懸念:** C2039/C2065以下の連鎖エラーを最小依存で解消できる。ビルドによる確認は未実施。

## 2026-09-06 — SpatialAudioのBooleanプロパティ名を既存enumに合わせる

- **関連:** `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`。
- **事実:** `ArtifactCore::PropertyType` には `Bool` ではなく `Boolean` が定義されており、SpatialAudioの `muted` / `enabled` プロパティだけが存在しない列挙値を参照していた。
- **対応:** 2箇所を `PropertyType::Boolean` に修正した。
- **価値／懸念:** C2838/C2065の直接原因を依存追加なしで解消できる。ビルドによる確認は未実施。

## 2026-09-06 — SpatialAudioLayerの3D判定は基底のvirtual契約に揃える

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/include/Layer/ArtifactSpatialAudioLayer.ixx`、`Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`。
- **事実:** `ArtifactAbstractLayer::is3D()` は実装を持つ非virtual関数だったが、SpatialAudioLayerが `override` として宣言していた。
- **対応:** 基底の `is3D()` をvirtualへ変更し、SpatialAudioLayerの既存overrideを有効化した。SpatialAudioLayerは生成時に既存の `setIs3D(true)` も実行している。
- **価値／懸念:** レイヤー種別ごとの3D判定を多態的な契約で扱える。既存呼び出し側の挙動差はビルド後に確認する。

## 2026-09-06 — NoiseLayerはNoiseSourceの状態へImplアクセサ経由でアクセスする

- **関連:** `Artifact/include/Source/ArtifactNoiseSource.ixx`、`Artifact/src/Layer/ArtifactNoiseLayer.cppm`。
- **事実:** `ArtifactNoiseLayer::Impl` は `ArtifactNoiseSource` を継承しているが、`ArtifactNoiseLayer` の外側のメンバー関数から基底のprotected状態へ直接アクセスしていた。
- **対応:** 設定、カラーマッピング、色、CPUバッファ、キャッシュに対する `Impl` の公開アクセサを追加し、外側の実装をアクセサ経由へ変更した。継承関係とキャッシュ所有権は維持した。
- **価値／懸念:** protected境界を破らずLayer側の評価・保存処理を継続できる。アクセサの公開範囲が広がったため、将来はSource専用の評価サービスへ分離できるか確認する。

## 2026-09-06 — WigglePathsの拡張プロパティはCore APIを先に揃える

- **関連:** `ArtifactCore/include/Shape/AeOperators.ixx`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- **事実:** ShapeLayer側は `temporalPhase`、`detail`、`correlation`、`smooth` を編集・正規化するコードを持っていたが、Coreの `WigglePaths` は `amount` と `frequency` だけを公開していた。
- **対応:** Coreへ4値の最小アクセサ、clone、JSON保存／復元を追加し、`temporalPhase` を既存の揺らぎ位相へ反映した。新規signal／slotは追加していない。
- **懸念:** `detail`、`correlation`、`smooth` は現段階では値の保持と編集基盤までで、形状評価への詳細な意味付けは未検証。

## 2026-09-06 — GPUComputeContext実装のモジュール依存はGPUCapabilities IFCを明示する

- **関連:** `ArtifactCore/include/Graphics/GPUComputeContext.ixx`、`ArtifactCore/src/Graphics/GPUComputeContext.cppm`、`ArtifactCore/CMakeLists.txt`。
- **事実:** `GPUComputeContext.ixx` は `Graphics.GPUCapabilities` をimportしているが、実装 `.cppm` 用のMSVC `/reference` 一覧には `Graphics.GPU.Info` と自己モジュールしか登録されていなかった。そのため実装コンパイル時にGPUCapabilitiesのIFCを解決できなかった。
- **対応:** `GPUComputeContext.cppm` のモジュール依存へ `Graphics.GPUCapabilities.ifc` の明示参照を追加した。
- **追補:** `Compute.cppm` も `GPUComputeContext` 経由で同じ interface import を解決する実装単位のため、同じ `GPUCapabilities.ifc` 参照を追加した。
- **確認:** 2026-09-06の実コンパイルコマンドには追加後の `GPUCapabilities` `/reference` と `MpmCompute` の `OBJECT_DEPENDS` が反映されておらず、生成済みCMakeビルドが古いことを確認した。
- **追補:** 個別のCMake分岐だけではBoids／Compute系の漏れが再発するため、`CORE_IMPL` の実装ソースを `import Graphics.GPUcomputeContext;` で検出し、GPUCapabilitiesのIFC参照とGPU関連interfaceの順序依存を後段で共通追加する。
- **再追補:** 実装 `.cppm` は同名の `.ixx` のimportを暗黙に引き継ぐため、実装本文だけのスキャンでは `BoidsCompute` や `LayerBlendPipeline` を検出できない。対応する `src/...cppm`→`include/...ixx` も走査対象にする。
- **価値／懸念:** C++20 moduleの実装単位でもinterface側importの依存を解決できる。CMake再生成後の実ビルド確認は未実行。

## 2026-09-06 — シェイプ形状からマスクを生成する責務はShapeLayerへ集約する

- **関連:** `Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`。
- **事実:** シェイプの評価済みジオメトリを `MaskPath::fromShapePath()` へ渡す一回限りの変換処理がメニュー側に存在していた。`nativeShapePaths()` は複数コンテンツ、シェイプ演算子、現在フレームのパス評価後の形状を返す。
- **対応:** `ArtifactShapeLayer::createMaskFromShape()` を追加し、変換と空結果の無効化をShapeLayer側へ集約した。メニューはUndo付きの既存導線を維持したまま新APIを利用する。
- **価値／懸念:** 将来のライブ形状マット、自動化、別UIから同じ変換契約を再利用できる。現時点ではスナップショット変換であり、形状変更への自動追従や専用の保存形式は未実装。
- **次に確認:** ライブ追従を導入する場合の所有関係、フレーム評価時の再生成コスト、マスクとシェイプの座標空間・反転／穴あきパスの受入れを定義する。ビルド・テスト・実機確認は未実行。

## 2026-09-06 — Solver横断Physics Snapshotはruntime handleではなくauthoring topologyを識別子にする

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`、`ArtifactCore/include/Physics/FluidSolver2D.ixx`、`ArtifactCore/src/Physics/PhysicsSystem.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **事実:** Soft Body／MPMには既存Snapshotがあったが、FluidとBox2D worldはSolver横断の復元契約を持たなかった。Box2Dのbody IDはworld再構築で変わるruntime handleである。
- **対応:** Rigid Bodyはbody index、LayerID、cloneIndex、transform、速度、typeをSnapshot化し、topology一致を検証してから復元する。Fluidもgrid dimensionsと作業バッファを含むSnapshotを追加し、PhysicsSystemに登録されたRigid／Soft／Fluid／MPMを同一frame keyでcapture／restoreする入口を追加した。既存のSoft/MPM専用復元APIは後方互換のため残した。
- **価値／懸念:** スクラブ／ループの共通基盤を作れる。現在のRigid Snapshotはshape／joint topologyそのものを再構築するものではないため、body追加・削除・collider変更時はcacheを無効化し、将来はauthoring revisionをcache keyへ追加する必要がある。
- **次に確認:** 現在Layer内で直接更新されるFluidSolver2Dを、入力注入とSolver更新の二重実行なしにPhysicsSystemへ移す。続いてPhysicsSystemの共通fixed-stepからframe indexを管理し、nearest snapshotからの前方向replay、loop range／cache offset、Fluidを含む実ランタイムのseek復元を接続する。ビルド・テスト・実機確認は未実行。

## 2026-09-06 — 環境変数をスクリプトへ公開 (getEnv/setEnv/hasEnv)・unsetVariable整備

- **関連:** `ArtifactCore/include+src/EnvironmentVariable/EnvironmentVariable.{ixx,cppm}`、`ArtifactCore/include/Script/Expression/ExpressionEvaluator.ixx`、`ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm`、`ArtifactCore/CMakeLists.txt`、`tests/ArtifactCore/EnvScriptTest.cpp`。
- **事実:** `EnvironmentVariableManager` に単体削除がなく `clear()` 全消去のみだった。スクリプト (ExpressionEvaluator) から環境変数を読む手段がなく、OS直読み (`qEnvironmentVariable`) が各所に散在していた。ArtifactCore→ArtifactCoreEnvironment の参照は静的ライブラリのため終端リンクで解決し、CMakeのターゲット循環にはならない。モジュール参照は既存の `/reference` + `OBJECT_DEPENDS` パターンで配線できた。
- **対応:** マネージャに `unsetVariable` (revision bump付き) を追加。式ビルトイン `getEnv(name[, default])` / `setEnv(name, value)` / `hasEnv(name)` を `registerStandardFunctions` に登録。setEnvはマネージャのオーバーレイのみに書き、OSプロセス環境は変更しない。新規テスト6件を追加。
- **価値／懸念:** TokenExpansion と同じマネージャを参照するため `$VAR` 展開とスクリプトが一貫する。一方、ビルドツリーには無関係の作業中変更 (DebugIdentity/ArtifactRegex等) による既存コンパイルエラーがあり、検証時は一時退避→復元した。
- **次に確認:** 実機での式エディタ経由の利用、OS環境への書戻しが必要かの判断、他スクリプト種別 (Python/C#/AngelScript) への公開要否。テスト実行は実績あり (6/6 passed)。

## 2026-09-06 — Alembicは既存MeshImporterの静的経路とキャッシュ経路を分けるべき

- **関連:** `ArtifactCore/include/Geometry/MeshImporter.ixx`、`ArtifactCore/src/Geometry/MeshImporter.cppm`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`、`ArtifactCore/src/File/FileTypeDetector.cppm`。
- **事実:** `.abc` はFileTypeDetectorとAssetImporterで認識されるが、MeshImporterのBackendと拡張子分岐にはAlembicがない。MeshImporterには既に`importMeshFromFileAtTime()`がある一方、Artifact3DLayerは読み込み済み単一`Mesh`を保持する。
- **判断:** Alembicの対応準備では、代表時刻の静的ジオメトリ読み込みと、時間サンプルを再評価するキャッシュ再生を別フェーズにする必要がある。前者は既存MeshImporterへ閉じ込めやすいが、後者はframe/time変換、サンプルキャッシュ、メッシュ更新世代の契約が必要になる。
- **価値／懸念:** 既存のOBJ／FBX／glTF経路を広げずにレベル1の受入れを作れる。一方、複数オブジェクトや階層を単一Meshへ早期に押し込むと、後のキャッシュ／シーン対応で再設計になる可能性がある。
- **次に確認:** 採用ライブラリの配布条件、代表Alembicサンプルの分類、既存Meshのトポロジー更新API、GPUバッファ更新の必要範囲。今回、依存追加・ビルド・テストは未実施。

## 2026-09-06 — 既存MeshとloadFromFileAtTimeはAlembicの初期接続点になる

- **関連:** `ArtifactCore/include/Mesh/Mesh.ixx`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- **事実:** `Mesh`はN-gon、頂点／face／face-vertex属性、revision、bounds更新、GPU向け三角形化を持つ。`Artifact3DLayer::loadFromFileAtTime()`は時間指定import結果をレイヤーのMeshへ差し替える既存入口である。
- **判断:** Alembicの静的サンプルは既存Meshへ変換できる可能性が高い。時間キャッシュは既存入口を使って最小実装を試せるが、再生性能が必要になった時点でreaderのサンプルキャッシュとGPU更新境界を分離するべきである。
- **価値／懸念:** 新規レンダラー経路を作らずに初期対応できる。一方、毎サンプルのMesh丸ごと差し替えを製品版の再生経路とみなすと、大きなキャッシュでCPUコピー・bounds計算・GPU再アップロードがボトルネックになる可能性がある（未検証）。
- **次に確認:** Alembicサンプルのトポロジー固定／可変、既存GPU uploadがMesh revisionをどう扱うか、代表キャッシュの1秒再生時の更新量。公式依存情報はAlembicリポジトリと公式ドキュメントを参照した。

## 2026-09-06 — 4分割VPは単一Swapchainのpresentation段で扱う

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`。
- **事実:** 実際のComposition Editorは`CompositionRenderController`が単一の`ArtifactIRenderer`と物理pixelサイズのswapchainを所有する。`ArtifactCompositionRenderWidget`は同名目的の軽量surfaceだが、現行Editorから生成・参照されていない。Diligentはbackend-neutralな`SetViewports`/`SetScissorRects`をD3D12とVulkanの双方で実装している。
- **対応:** rendererにoffset付きviewport/scissor APIを追加し、軽量surfaceには同一RT上で各paneをflushするQuad layoutの基礎を追加した。main controllerではGPU resolve済みのpresentation textureだけを4回drawし、重いcomposition-space cache／layer再合成は共有する。既存command infrastructureへ`View: Toggle Quad Presentation`を追加した。追加のswapchain、QSplitter、QImage合成は作らない。
- **価値／懸念:** QuadはDiligentの単一swapchain上で動作し、GPU合成を4回実行しない。一方、現在は同一の最終表示を4ペインに表示するpresentation sliceであり、gizmo／hit test／独立camera stateはまだpane routingされない。
- **次に確認:** D3D12/Vulkan双方でscissor復元、resize、overlay、GPU frame timeを実機確認する。次段階でpane固有cameraとinput routingを、controllerの既存camera stateを複製して接続する。ビルド・テストは未実行。

## 2026-09-05 — Box2D接触イベントはstep内で正規化する

- **関連:** `ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** Box2D 3.1.1のbegin/end/hit配列はworld step後の一時データで、shape削除後のend eventには無効shapeが含まれ得る。接触／hit eventはshapeごとに既定OFFである。
- **対応:** レイヤーbodyとfloor shapeでcontact/hitを有効化し、step直後に`PhysicsContactEvent`へコピーする。compositionが対象レイヤーへ配布し、レイヤーはstep単位のbegin/end/hit数、継続接触数、最大接近速度、直近hit情報を保持する。保存・Undo・新規signalは追加しない。
- **価値／懸念:** 将来の衝突particle／sound／break判定は同一の正規化済みデータを利用できる。shapeが削除されたend eventは安全のため解決不能なら破棄するので、その稀な経路ではactive数はworld resetまで残り得る。
- **次に確認:** 床・Dynamic/Static/Kinematicの組合せ、複数同時接触、body削除直後、hit speed閾値、将来の視覚／音反応のrate limit。ビルド・実機確認は未実行。

## 2026-09-05 — 再生中の物理ドラッグは保存済みJointと分離する

- **関連:** `ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **事実:** Box2D 3.1.1のMouse Jointは静的bodyとDynamic bodyの間で、world targetを追従させるランタイム拘束である。既存の`layerJoints`はComponentsから復元される保存済みconstraintの寿命を管理する。
- **対応:** `mouseJoints`を別のowner管理にし、再生中のSelectionツールで選択済みDynamic bodyだけに作成する。停止・release・body破棄で除去され、authoring transform、Components JSON、Undo履歴を変更しない。
- **価値／懸念:** 通常のVP Transformと競合せず、Spring/Rope/Sliderを維持したまま直接演技を調整できる。一方、現在は選択済みレイヤー全体を掴むため、collision shapeの厳密なポインタhit testやドラッグ強度のUI調整は未実装。
- **次に確認:** 再生中のbody質量ごとの追従感、ウィンドウ外release、再生停止・layer削除中のjoint除去、既存joint併用時の安定性。ビルド・実機確認は未実行。

## 2026-09-05 — 次の物理機能は「固定／操作可能なbody」と「スライド拘束」が最短

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** 2D剛体はcomposition共有world、owner別jointの破棄、fixed-step更新まで接続済み。現行ラッパーが露出するjointはDistance/Revoluteのみだが、導入済みBox2D 3.1.1にはPrismatic、Mouse、Motor、Weld等のAPIがある。bodyにはsleep、CCD、damping、gravity scaleも既にある。LiquidSolver2Dはcontainer、opening、spill、checkpoint、collision layerへの衝突、surface snapshotまで存在し、SoftBodyはsnapshot、wind、collider、tear基盤を持つ。
- **判断:** 次の小さく実用的な追加は、Static/Kinematic/Dynamic body modeとViewportのMouse Joint操作、その次にPrismatic（軸・移動範囲・motor）である。新エンジンを導入せず、既存Box2DのCore ownershipとComponents面を維持できる。
- **価値／懸念:** kinematic targetは動く親・衝突壁・接続先の明示に使え、mouse jointは再生中の物理演出を直接調整できる。joint追加はanchor座標・Undo・seek再生成・joint別寿命管理を共有する必要がある。MPM、Fluid、SoftBody、PyroのGPU化はDiligent/CPU parityとsnapshot検証を伴うため別規模。
- **次に確認:** body typeのJSON／Components UI、編集時にkinematicへ安全にtransform同期する経路、mouse dragと既存VP transform toolの入力競合、Prismatic jointのlocal axis／limit／motorの最小契約。実機・ビルド未実行。

## 2026-09-05 — 2D body種別／Slider／破断を既存Box2Dへ追加

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **対応:** CollisionにBody Type（Dynamic/Static/Kinematic）、JointにSlider（Prismatic: axis/limit/motor）とBreak Forceを追加。Kinematic/Staticは各fixed step前に編集Transformから位置／角度を同期する。Box2Dのconstraint forceがBreak Forceに達するとownerのjointだけを破棄し、runtime broken stateで再生成を抑止する。
- **寿命:** 閾値・body/joint設定はComponents JSONへ保存する。broken stateは保存せず、seek/reset／joint設定編集でfalseへ戻す。新規signalは追加せず、既存のLayerDirty/Components descriptor更新を使う。
- **次に確認:** Sliderのローカル軸・limitの視覚的な向き、kinematic bodyのアニメーション速度、破断境界値、Dynamic/Static/Kinematicの複数body衝突、Undo/Redo後の再接続。ビルド・実機未実行。

## 2026-09-05 — レイヤー間Spring／Ropeと接続点

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactCore/src/Physics/Physics2D.cppm`。
- **事実:** 既存Springはhertz/dampingだけを指定し、Box2DのenableSpringを設定していなかった。joint専用layer worldは接続先をstatic proxyで追従し、Composition共有worldとは分離される。
- **対応:** Spring=3を有効化、Rope=4はenableSpring=true/hertz=0/enableLimit=trueで最大長のみ拘束する。owner body中心／target layer中心からのlocal X/Y offsetをComponents・JSON・descriptor・joint生成に接続。名前は一意な場合だけIDへ解決して保存する。床proxy(-3)をprimary bodyに選ばないよう修正。
- **根拠:** 既存依存Box2D 3.1.1、公式 `src/distance_joint.c`（MIT、 https://github.com/erincatto/box2d/blob/v3.1.1/src/distance_joint.c ）とインストール済み `types.h`。既存API利用であり外部コードの複製なし。CPU物理のみ、Diligent/D3D12/Vulkan資源・同期に変更なし。
- **双方向対応:** Collision / Joint有効の2D layerをcomposition共有worldへ統合し実body同士を接続。CoreのNamedVectorでowner別jointを所有し、body破棄時のBox2Dによるjoint破棄に登録情報を追従させる。固定targetはowner別proxyを維持する。
- **確認した問題:** bodyの位置をレイヤーpositionへ直接戻していたほか、Box2Dのradianを表示のdegreeへ直接代入していた。初期transform＋body差分で合成し、初期値を含むsnapshotを使用。PlaybackServiceの通常再生はgoToFrameを使うため、setFramePositionだけを直してもfixed-stepに到達しない。両入口のclockを統合した。
- **制限／次の確認:** 逆行／大きなseekは移動先編集値から再生成し、rigid snapshot完全復元は未実装。非一様scale親によるshearとcolliderの一致、layout/modifierとの併用、ネストcompositionの時間サンプリングは未検証。必要なら将来rigid snapshotとauthoring revisionによる無効化を分離する。今回ビルド・テスト・実機操作は未実行。双方への反作用、三者連鎖、片側無効化・削除、ばね振動、ロープ最大長、開始フレーム復帰、Undo・保存復元を次に確認する。

## 2026-09-05 — Construction Layerの描画と吸着を接続

- **関連:** `ArtifactConstructionLayer.cppm`、`ArtifactCompositionRenderController.cppm`、`ArtifactSmartGuidesManager.cppm`。
- **事実:** itemsは保存だけでdrawが参照していなかった。Smart GuidesはGuideSetの座標を親子transformなしで利用し、VPのprojected-frame候補はconstructionの内容を使っていなかった。最終出力包含ONでもguide除外条件と競合し得た。
- **対応:** Line/Circle/Annotationの描画・Inspector値編集と共通のlocal snap pointsを追加。VPとSmart Guides双方でtransformを適用する。包含ON時のguideフラグを整合させた。
- **価値／懸念:** スナップと表示が同じconstruction設定を使える。Inspector項目の追加Undoは非表示状態と値を復元し、配列内の無効項目は残す。既存APIからitemsを並べ替えるとordinalプロパティの対応が変わるため、将来の並べ替えUIにはIDベースのUndoが必要（未実装）。
- **次の確認:** ビルド・実機操作は未実行。保存復元、親子変換、編集Undo、出力ON/OFFを確認する。追加専用UI・寸法線・VP個別制御点編集は別作業。
- **VP編集追補:** 個別制御点ドラッグを実装。表示・pickは同じlocal handle座標を使い、press時のカメラと逆world transformでlocal z=0に交差させる。端点／平行移動／半径は開始時スナップショットから計算し、releaseで単一Undo、Esc／右クリックで復元する。Undo時に既存LayerChangedEventを再利用し、内容更新にはSource dirtyを使う。新規作図・文字入力はInspectorに残す。ビルド・実機操作は未実行。

## 2026-09-05 — VPフレーム吸着の残課題

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `snapProjectedFramePointer` とmouseMoveの呼出し。
- **事実:** コンポジションと他レイヤーの端・中央を候補化する。呼出しはFrontに限定され、ローカル軸移動はaxisMoveSnap対象外。候補は各pointer更新で全レイヤーのboundsを投影して再生成し、各軸の最短距離で選ぶ。
- **未検証の仮説:** 多数レイヤーでは候補再生成が入力負荷に寄与し、近接ガイド間では吸着先が切り替わりやすい可能性がある。平面1枚時の引っかかりの原因とは断定しない。
- **価値／次の確認:** ドラッグ中の候補計算時間を確認し、必要なら候補キャッシュと吸着の保持・解除閾値を導入する。今回は調査のみ。
- **実装追補:** ユーザー指定の1〜3に対応。吸着10 logical px／解除16 logical pxの保持帯を導入。候補配列を既存Core.Arrayへ移し、カメラ・viewport・選択・composition変更時と250ms間隔で再生成、ソートして二分探索する。press/modal開始/終了でキャッシュを無効化し、Alt/OFFで保持を解除する。Front限定を外し、ドラッグrayと同じカメラ行列で画面上のboundsへ吸着する。World/Local/ViewのX/Y/Z移動はギズモ内部のdragAxisDirectionを参照し、一つの画面ガイドへ軸方向の補正を行う。GPU資源・Diligent backend・同期経路は変更しない。
- **未検証／制限:** ビルド・テスト・実機操作は未実行。透視投影下の吸着は画面上の整列であり、3D面・頂点への吸着ではない。候補の外部変更は最大250msの更新遅延がある。透視ビュー・親付きローカル軸・Shift精密操作・Ctrl量子化の組合せで、ガイドと確定結果の一致を次に確認する。負荷軽減量は未計測。

## 2026-09-05 — Native Dock のQADS相当操作

- **関連:** `Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`。
- **事実:** Native DockはQADSを実行時に生成せず、Qtの `QTabWidget` / `QSplitter` と所有する `QDialog` によるbackend-neutral surfaceである。既存実装は他タブ面へのdropは持つが、タブ順の永続化、タブを外へドラッグして分離、フローティング状態からの再ドックを一貫して扱っていなかった。
- **対応:** タブ面の表示順をportable layoutへ保存・復元し、同一面のdropを順序変更として扱う。受け取り先のないtab dragは所有panelを保持したままフローティング化し、浮動ウィンドウのDock backボタンで元のareaへ戻す。復元時にもembedded/floating間を変換する。closeは非破壊の非表示を維持する。ドラッグ中は、候補tabまたはdock areaに半透明のアクセント色プレビューをowner-drawで表示する。tab面とtab barの双方でdock MIMEを受け、既存tab上はtab化、空白部はその領域へ追加する。tabのダブルクリックも同じ安全なフローティング経路へ接続した。
- **未検証:** 実機でのtab drag、外部dropのcancelと分離の境界、Dock back、ドロッププレビュー、再起動後の順序／geometry、各既存panel（viewportを含む）のfloating再親子化とサイズ更新。Build/testは未実行。

## 2026-09-05 — App Debuggerの自動更新とVP入力停止の候補

- **関連:** `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm` のtimerEvent/refresh、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のframeDebugSnapshot。
- **事実:** App Debuggerは250msのtimerでrefreshし、controllerのsnapshotを取得する。snapshotにはGPU画像のreadback、最終effect処理、preview差分画像作成が含まれる。refresh入口は再入ガードのみで非表示チェックがない。VPの定期描画はrenderOneFrameを通らずrenderOneFrameImplを直接呼ぶため、追加したイベント6/8はその経路を計測していない。
- **未検証の仮説:** 診断UIの周期的な更新がUIスレッドを占有し、パン入力の約397msの空白や描画開始間隔の約570ms〜1.09秒の空白へ寄与している可能性。実際に当該widgetが生成済みか、更新の実測時間は未確認。
- **価値／次の確認:** 診断機能自身の観測負荷を分離する。App Debuggerを生成しない起動で比較し、必要ならrefresh前後とtickキュー投入/受信、renderOneFrameImpl全体を計測する。修正は今回行っていない。
- **追加確認・対応:** Native DockのaddLazyDockedWidgetFloatingはfactoryを即時呼び出すため、App Debuggerは未表示でも生成されていた。ユーザー依頼によりconstructorでのtimer開始を削除、非表示refreshを抑止、show/hideでtimerを開始/停止。初期登録と保存レイアウト復元後は非表示へ固定した。Native Dockのタブに標準closeアイコンを持つボタンを追加し、既存eventFilterからsetDockVisibleへ渡す（新しいsignal/slot接続なし）。タブの非表示・再表示はsetTabVisibleで保持し、内容widgetは破棄しない。ビルド・非表示時の負荷・全タブを閉じた後のメニュー再表示・キーボードSpace操作は実機未検証。

## 2026-09-04 — 物体検出バックエンドの共通契約

- **関連:** `ArtifactCore/include/AI/ObjectDetector.ixx`。
- **確認できた事実:** 既存の物体検出は具体クラスしかなく、ONNX等の実検出器を同じ呼び出し側へ接続する抽象契約がなかった。`IObjectDetector` に ready、detect、error 状態を定義し、既存検出器を適合させた。
- **価値／懸念:** 将来の実モデルを App API の変更なしに差し替えられる。現行の輝度ベース検出はフォールバックであり、物体認識の品質を保証しない。
- **次に確認すべきこと:** 実ONNX検出モデルのラベル・矩形・NMS出力をこの契約へ正規化する。

## 2026-09-04 — 連番マスクの変化診断

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 連番のマスクを正規化座標で比較し、平均差分、最大差分、大きく変化した面積率を返す診断APIを追加した。
- **価値／懸念:** App側は急なマット変化を検出して再推論や手動確認を促せる。これは動き補償を行わないため、被写体が移動する連番ではRoto Brush伝播後の比較を前提とする。
- **次に確認すべきこと:** 実連番で警告閾値と、再推論・安定化のUI方針を決める。

## 2026-09-04 — セグメンテーションマスクの非破壊プレビュー

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** `DepthMap` は正規化された単一チャンネル値を保持する。これを直接 `ImageF32x4_RGBA` のグレースケール画像として生成するプレビュー API を追加した。
- **価値／懸念:** App側は元画像やalphaを変更せず、推論・Roto Brush・手動補正のマスクを共通表示できる。GPUプレビューとの最終的な見え方の一致は実機確認が必要。
- **次に確認すべきこと:** App の既存マスク表示導線へ接続し、比較表示と反転表示を確認する。

## 2026-09-04 — ONNX セグメンテーション設定契約のテンプレート化

- **関連:** `ArtifactCore/docs/ONNX_IMAGE_SEGMENTATION_CONFIG.md`。
- **確認できた事実:** モデル固有の入力サイズ・正規化・色順・出力選択は `loadOptionsFromJson()` で外部化されている。設定ファイルの最小テンプレートと許可値を文書化した。
- **価値／懸念:** モデル導入時にコード変更ではなくモデル配布物だけで契約を更新できる。テンプレート値は特定モデルの推奨値ではないため、実モデル仕様との照合が必須。
- **次に確認すべきこと:** 最初の採用モデルについて、モデル／設定／ライセンス情報を同じ配布単位にまとめる。

## 2026-09-04 — 一括セグメンテーション後処理へのクリーンアップ統合

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** batch API は `SegmentationMaskRefinementOptions` を通じて後処理を実行する。穴埋めと小領域除去も同設定に統合し、モデル推論後の各フレームへ一貫して適用できるようにした。
- **価値／懸念:** 単一画像と連番バッチでマット整形の条件がずれない。既定では両方無効であり、形状を変える処理は明示設定時だけ適用される。
- **次に確認すべきこと:** App側で素材カテゴリに応じたプリセットを設けるか検討する。

## 2026-09-04 — セグメンテーションマスクの小領域除去

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 推論マスクには孤立した小さな前景島が現れる。閾値以上の連結成分を走査し、指定面積以下だけを透明化する処理を追加した。
- **価値／懸念:** 背景上の小さな誤検出を、モデル変更なしにプレビュー段階で抑えられる。小物や細部まで消す可能性があるため、既定値は無効で明示的な面積指定を必要とする。
- **次に確認すべきこと:** 人物の髪・アクセサリー、製品写真で妥当な面積範囲を確認する。

## 2026-09-04 — セグメンテーションマスクの穴埋め

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** AIマットには前景内の小さな背景穴が発生する。外周へ到達しない背景連結成分だけを検出し、面積上限を指定可能な穴埋め処理を追加した。
- **価値／懸念:** 人物・製品の内側に残る小穴をモデル非依存で整えられる。細いリング状オブジェクトの内側を消してしまうため、面積上限とプレビューでの確認が必要。
- **次に確認すべきこと:** 文字、メガネ、穴のある製品素材で既定の面積上限を決める。

## 2026-09-04 — ONNX セグメンテーションの入力色順設定

- **関連:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。
- **確認できた事実:** ONNX の画像モデルには RGB だけでなく BGR の入力テンソルを前提とするものがある。`inputColorOrder` を JSON 設定および型付き option に追加し、色順に対応する mean/stddev も正しい色成分へ適用する。
- **価値／懸念:** モデル固有の色順のためだけに変換ノードを増やさず、U²-Net系などの導入候補を設定で試せる。実モデルでの色順・正規化仕様の確認は未実施。
- **次に確認すべきこと:** 導入する実モデルの preprocessing 定義を JSON と照合する。

## 2026-09-04 — 物体検出から共通マットへの接続

- **関連:** `ArtifactCore/include/AI/ObjectDetector.ixx`、`ArtifactCore/src/AI/ObjectDetector.cppm`。
- **確認できた事実:** 検出結果はラベル、信頼度、矩形を持つが、既存コードにはマスク処理へ渡す経路がなかった。検出矩形をsoft edge対応の`DepthMap`へ rasterize するAPIを追加した。
- **価値／懸念:** 将来のYOLO等の実検出器でも、矩形を初期選択・保護領域・Roto Brushの開始マットとして共通利用できる。矩形は物体輪郭ではないため、最終切り抜きにはセグメンテーションとの合成が必要。
- **次に確認すべきこと:** 実検出モデルを導入後、複数検出のラベル選択とセグメンテーション初期化のUI導線を設計する。

## 2026-09-04 — AI マスクの切り抜き・背景置換を共通 CPU 経路に集約

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** セグメンテーション結果は `DepthMap` の正規化マスクとして扱える。切り抜き、自動前景クロップ、単色背景、背景画像の合成を `ImageF32x4_RGBA` と `FloatRGBA` の直接操作で追加し、Qt 合成・`QImage` 変換を経由しない。非AIの輝度フォールバックも既存の透明領域を前景として復活させないよう、入力alphaを既定で尊重する。
- **価値／懸念:** ONNX、Roto Brush、将来のモデルが同じ出力処理を共有できる。背景画像はバイリニアでサンプルし、異解像度の置換で段差を作らない。将来は GPU 経路で同じ straight-alpha 契約を維持する必要がある（未検証）。
- **次に確認すべきこと:** GPU 合成経路へ接続する際、straight-alpha の契約を保持してプレビューと書き出しの結果を一致させる。

## 2026-09-04 — セグメンテーション境界の色かぶり補正

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 前景マスクの半透明境界では、グリーン／ブルースクリーンの色が残る。mask coverage が中間値の画素だけに green/blue の過剰成分を抑える処理を追加した。
- **価値／懸念:** 切り抜きの縁をモデル非依存で改善できる。人物固有の緑／青を過度に変えないよう、完全不透明領域には適用しない。強さと境界幅は実素材で調整が必要。
- **次に確認すべきこと:** 緑髪・青い衣装を含む素材で、補正量とエッジ幅の既定値を決める。

## 2026-09-04 — 高解像度向けマスク後処理の分離パス化

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** foreground の expand／contract は矩形要素の max/min 演算、feather は矩形 box blur として実装されている。どちらも水平・垂直の二段に分離しても edge-clamp を含む結果は同じになる。
- **価値／懸念:** 半径に対する計算量を二次から線形へ下げ、4K素材のマスク調整を実用的にする。CPU処理のままなので、GPU経路が整った段階で置き換え候補として確認する。
- **次に確認すべきこと:** 実素材で既存 GPU 経路とのエッジ見え方とCPU処理時間を確認する。

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

## 現在の優先検証

### 静止画・連番画像 — GPU cache と実素材の再生／出力確認

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`、`Artifact/src/Render/GPUTextureCacheManager.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** 4K連番、欠番、Time Remap、再リンク、Preview／Render Queueでフレーム・GPUメモリ・出力が一致すること。

### Shape — Path keyframe／Merge Paths／SVG出力の実機確認

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **状態:** GPU／互換フォールバック／boundsの同期は実装済み。SVGのグラデーション、stroke taper／alignは未対応。
- **確認すること:** 複数subpathと各Merge Paths mode、Path keyframe、GPU／出力SVGの一致。

### Shape — 複数コンテンツ／GPUベクター描画の実機確認（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`ShapeContent`、`paintGpuPaintItems`、`resolveContentVisPaths`、`renderContentsToImage`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証（ビルドは指示待ちのため未実行）。
- **内容:** 1レイヤー複数パス（形状＋塗り＋線＋表示＋結合モード、空＝従来動作）。グラデーションは三角形重心サンプリング、Inside／Outsideはハーフオフセット＋中央線、テーパー／勾配線はセグメント分割でGPU描画し、viewportのQImageスプライト分岐を撤去。結合はCPU側QPainterPath真偽値演算で解決し、スタイルは保持。物理グリッド・3Dカード高速パス・オペレータキー評価は従来のまま。
- **確認すること:** 既存単一シェイプの見た目不変（solid高速パス・operator分岐は温存）、グラデーション／align／taperのGPU描画、Subtract／Intersect／Differenceの穴・境界線、連番・サムネイル・SVG出力、Repeater大量複製時の負荷。
- **既知の近似（未検証の仮説ではない仕様）:** テーパー線の結合部は Butt 重ね、Roundキャップは矩形延長近似、dash＋taper併用はdash優先、コンテンツのパス頂点アニメは未対応（静的）。

### Shape — 沿路グラデーション線／ダッシュオフセット（2026-09-03）

- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`（`PolylineStyle`）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`drawStyledPolyline`）、`Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `PolylineStyle`に`gradientEnabled/gradientStart/gradientEnd/dashOffset`を追加。`drawStyledPolyline`は累積長パラメータでセグメント・ダッシュ・結合・キャップを沿路補間色で描画し、dash位相は`dashOffset`の剰余で解決。レイヤー側は`shape.dashOffset`（アニメ可）＋コンテンツ別`dashOffset`、勾配のみの線は taper 分割器ではなく`drawStyledPolyline`経由に変更（結合・キャップ・dashと合成可）。QImage互換・SVG出力（`dashOffset`のみ）・保存も配線。
- **確認すること:** 既存実線の見た目不変（新フィールド既定で旧経路と同一）、勾配＋dash＋round結合の合成、負offset・巨大offset、マーチングアンツのキーフレーム補間。
- **既知の近似:** taper＋dash併用はdash優先でtaper無効、勾配サンプリングは線形補間。

### Shape — SVG相互運用（取込・書出）（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`SvgImport`、`shapeContentsToSvg`、`parseShapeContentsFromSvg`、`addShapeContentsFromSvg`、`importSvgFileContents`）。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** 書出は結合解決済みパス＋塗り／線／dash／fill-ruleを`<path>`＋`linear/radialGradient` defsで出力（taper線→通常線、conical→単色、勾配線→中間色に縮退）。取込は`path(d全命令・Aはベジェ化)`・rect（角丸可）・circle・ellipse・polygon・polyline・line＋線形／円形グラデーション（前方参照可）＋transform bake＋継承スタイルを編集可能コンテンツ化（座標はbounds正規化、256件cap、64MB cap）。`ClipboardManager`は未変更で、受渡し自体は素のSVGテキストを呼出側に委譲。
- **確認すること:** Illustrator／Figma出力SVGの往復、userSpace勾配・奇数dash・相対命令・指数表記の数値、空・不正SVG（0件／-1）、既存JSON互換。
- **既知の近似:** 複数subpathは1要素に統合（線描画で連結線が出る）、3 stops以上は両端のみ、gradientTransform・非等方scale下の線幅・group fill-opacity継承は近似、strokeのurl()は勾配線として解決（fillのみ前方参照対応だった点をstore側で統一）。

### Shape — コンテンツ編集サポート（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`activeContentIndex_`、`ShapeContentProxy`、`duplicateShapeContent`、`moveShapeContent`、`insertShapeContent`、`swapShapeContents`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `activeContentIndex_`（-1 = レガシーモード）と`ShapeContentProxy`（`ArtifactShapeLayer*` + index）を導入。Proxyは`name`/`visible`/`opacity`/`merge`/`fill`/`stroke`/`geometry`/`duplicate`を`setShapeContentAt`経由で直接編集し、PropertyEditorは`shape.activeContentIndex`で操作対象を切り替える。複製（挿入位置にコピー）、挿入、`move`、`swap` APIを追加。`shape.content.<i>.type/width/height/cornerRadius/starPoints/starInnerRadius/polygonSides/fillRule` を `setLayerPropertyValue` で直接編集可能に拡張。JSONシリアライズに`activeContentIndex`を含む。
- **確認すること:** Proxyのスワイプ（他のインデックス参照）、move/swap後のbounds・visPaths再構築、JSON往復、PropertyEditorでのアクティブコンテンツ切り替え時の描画反映、contentジオメトリ編集時の再構築・保存。

### 2.5D — 局所DOF／motion blurの品質と負荷

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Layer/Artifact{Image,Shape,Text}Layer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** Image／Shape／Textで深度・focus・shutterを変え、alpha順、極端なblur、再生時のGPU負荷、書き出し結果を確認する。

### Gobo runtime texture — 将来接続の安定ID境界

- **関連:** `ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`。
- **状態:** ファイルGoboを置換できるruntime SRV入力は実装済み。Image Layerとの接続・UI・保存は未実装。
- **判断待ち:** 接続を始める時点で、packed scene-light slotではなく安定したLight IDとresource revisionを対応付ける。

## 現在の設計判断

### Semantic Debugger — 外部 `ArtifactDebugger.exe` を正規UI境界とする

- **関連:** `docs/planned/MILESTONE_EXTERNAL_SEMANTIC_DEBUGGER_2026-09-02.md`、既存のMCP／Trace／Shared Memory IPC診断基盤。
- **状態:** 未着手。設計判断をマイルストーン化。
- **判断:** ArtifactStudio本体には低コストの `ArtifactDebugRuntime`（semantic identity、mutation provenance、frame snapshot、safe-point制御）だけを置き、意味表示・原因解析・timeline・semantic breakpointのUIは外部プロセスへ分離する。既存MCPはAI専用に作り直さず、Debuggerとheadless harnessが共有するread／control protocolの基盤として再利用する。
- **価値／懸念:** 本体のQt／レンダリング状態をデバッガUIから隔離し、VS native debuggerとの併用、実行中Attach、Debugger単独更新を可能にする。一方、protocol version、履歴欠落の「未観測」表示、frame boundaryでのpause、snapshot復元とdeterministic replayの境界が必要。
- **次に確認すること:** Phase 0で既存MCP TCP／QLocalSocket／Named Pipeの接続候補、共有されるsemantic schema、diagnostic buildと通常buildのruntime有効化方針を確定する。

### Layer modulation は opacity以外へ拡張しない

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/include/Audio/Modulation/Router.ixx`。
- **状態:** opacityの評価、保存、Undo基盤は実装済み。Inspector導線とruntime確認は未完。
- **判断:** Transformはvariant／physics／layoutとの評価順を定義するまで追加しない。

### 物理 — rigid joint／polygon colliderの動作確認を先行する

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactCore/src/Physics/Physics2D.cppm`。
- **状態:** jointとpolygon colliderの導線は実装済み、runtime未検証。
- **確認すること:** 重力の画面座標符号、scrub復元の制約、凹形状の凸近似、SoftBody／MPMとの接触。

### 3D rendering — AOVと実GPU契約は別スライスに保つ

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`。
- **状態:** World Position AOVのtarget基盤はあるが、書き込みPSO／runtime検証は未実装。
- **判断:** 2D／2.5Dの完成度を優先し、AOV、GPU skinning、ray tracingは個別の受入条件を定義してから進める。

### プロキシ生成 — Out-of-Process 専用ワーカー (ArtifactProxyWorker) 方式の採用

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`、`Artifact/include/Proxy/ProxyService.ixx`。
- **状態:** `ArtifactProxyWorker.exe` の native / Media Foundation / ffmpeg 経路と JSON Lines 通知を実装。native は実エンコーダー名と検出候補も返す。host は jobId/outputPath/outputBytes を照合し、Project View から queue キャンセルも可能。Eighth、音声再エンコード、hardware encoder、auto fallback、staged package の worker 実機確認済み。Media Foundation は H.264 入力で成功するが、ProRes 入力は非互換で、極小出力は 64px/axis にクランプする。成功・失敗・キャンセル時の partial cleanup と、検証ツールの worker timeout 回収も確認済み。host UI 統合の実機確認は未完。
- **判断:** 本体プロセスの安定性（クラッシュ・OOM 巻き添え防止）と FFmpeg C API 直接利用（進捗通知・GPU HW エンコード）を両立するため、専用の子プロセスワーカー（`ArtifactProxyWorker`）を設けて非同期 IPC で連携する構成を正規方針とする。

### Proxy worker — 成果物の原子性と timeout 回収を同じ受入条件にする

- **関連:** `Artifact/src/Worker/ArtifactProxyWorker.cpp`、`tools/proxy_worker_smoke_test.py`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **状態:** 実装済み、host UI実機確認待ち。
- **判断:** worker の exit code だけでは成功とみなさず、completed message、final output、partial cleanup、timeout／強制終了後の状態を一組で検証する。これにより、ハングや中断を有効な proxy と誤認する経路を受入段階で検出できる。

### 2026-09-06: 個別 proxy playback toggle の未接続を解消

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` の Project View footage context menu、`ProxyMeta::enabled`、`syncProxyPathToProject()`。
- **事実:** `ProxyMeta::enabled` と global proxy toggle は存在していたが、footage 単位で enabled を変更する操作がなく、生成完了時は常に `enabled=true` として同期していた。
- **対応:** footage context menu に `Enable/Disable Proxy Playback` を追加し、個別設定を `syncProxyPathToProject()` へ接続。worker 成功時も既存の個別 enabled 設定を尊重し、生成時の source timestamp を成功メタデータへ確定するよう修正した。
- **未検証:** runtime の context menu 操作、global toggle との組み合わせ、保存／再読込後の個別設定保持は未確認。ビルド・テストはユーザー指示待ち。

## 保留中の設計判断

### シェイプレイヤー VP 操作の増強範囲（2026-09-02 ユーザー質問）

- **質問:** 「シェイプレイヤーのVP操作機能増強いけそうか」
- **解釈:** `VP = メインコンポジション Viewport`（`taste.md` の communication-integrity 規範に従い grep で `Viewport`/`TextViewport` へ展開、コード上に独立した `VisualProgramming` 系は無いため）。
- **現状（コード読みで確認済み、未検証含む）:**
  - `Artifact/src/Tool/ArtifactToolManager.cppm:44-46` に `ToolType::Shape / Rectangle / Ellipse` が定義され、`ToolType::Shape` は `ArtifactToolService` で `Shape modeling` 入口と紐付け済み（`docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:258`）。
  - `Artifact/src/Layer/ArtifactShapeLayer.cppm` は `ShapeType`（Rect/Square/Ellipse/Star/Polygon/Triangle/Line）+ `customPolygonPoints_` + `CustomPathVertex { pos, inTangent, outTangent, smooth }` + `customPathClosed_` を保持し、`evaluatePathAt(frame)` でパス頂点キーフレーム評価を実装。
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のシェイプ系 VP ハンドラ:
    - `mousePress` L22853-22923 で Rectangle/Ellipse/Shape ツールのドラッグ → 矩形/楕円/スター/ポリゴン/三角のシェイプレイヤー新規作成（選択レイヤー有りは mask 経路、無しなら `RectangleToolMode::Shape/EllipseShape/StarShape/PolygonShape/TriangleShape` で `ArtifactShapeLayer` を生成、L27803-27873）。
    - `mousePress` L22940-22991 で Pen ツールが Shape レイヤー選択時のみマスクではなくカスタムパスを `pendingShapePathVertices_` へ追加（開始点クリック or Enter で確定、Backspace で取消、Escape 取消）。
    - `mousePress` L23527 `beginShapePathVertexDrag`、`updateShapePathVertexDrag` L25095、`endShapePathVertexDrag` L27345 経由でカスタムパスの頂点/タンジェントドラッグ編集。`ShapePathVertexEditCommand` で Undo。
    - `mousePress` L23506-23527 で Line シェイプの端点ドラッグ (`isDraggingLineEndpoint_` / `draggingLineLayer_`) を実装。
    - `isDraggingShapePathVertex_` / `shapePathEditPending_` / `shapePathEditDirty_` / `hoveredShapePathVertex_` / `hoveredShapePathTangent_` / `draggingShapePathTangent_` (0=vertex / 1=in / 2=out) を state に保持 (L12488-12505)。
  - `ArtifactCompositionRenderOverlay.cppm` のシェイプレイヤー描画: L1028-1105 でカスタム Polygon の頂点ストローク描画、Line の 2 端点描画、customPathVertices のベジェ描画を実装。ただし **頂点ハンドル/タンジェントハンドル/選択ハイライト/セグメント挿入マーカーのオーバーレイ描画パスは未確認**（grep 上このファイルには vertex overlay / hit-area / ハンドルサイズ定数が Shape 用に出てこない）。
  - `ArtifactRenderLayerWidgetv2`（LayerEditorPanel 内、`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:255-279`）に vertex / segment / tangent のコンテキストメニュー・Ctrl-click 選択追加・Shift toggle・numbering・hover/select 表示・path vertex duplication・polygon vertex duplication・segment insert 経路がある。`MILESTONE_LAYER_EDIT_2026-04-25.md:209-219` で `customPolygonPoints` を `CustomPathVertex` へ拡張済み。
- **増強候補（メイン VP 視点で未着手／不足）:**
  - (1) シェイプ専用 vertex/tangent/segment overlay 描画の RenderOverlay 統合（`LayerEditorPanel` の機能をメイン VP へ移植した残骸: `INSIGHT_ARCHIVE_2026-09-01.md:4549-4551` の懸念「ビルド未実施。タンジェント smooth 反射の長さ保存比、パスキーフレームの UI は次段階」）。
  - (2) Rect の `cornerRadius` ハンドル（`hitTestCornerRadiusHandle()` は `ArtifactRenderLayerWidgetv2` にあり、メイン VP 側は `setSize()` を width/height ドラッグで更新する経路のみ、`MILESTONE_LAYER_EDIT_2026-04-25.md:60` に「ShapeEditCommand 同様」とあるが RenderController 側 grep で未確認）。
  - (3) Star の `starInnerRadius_` ハンドル、Polygon の頂点ドラッグ挿入。
  - (4) `ToolType::Shape` のプリセット図形選択 UI（Rect/Ellipse/Star/Polygon/Line/Triangle のアクティブ切替。現状シェイプ作成は `Shape` 単独か `Rectangle/Ellipse` ツールの `rectangleToolMode_` 切替のみで、Panel 上にプリセット導線なし）。
  - (5) シェイプ operator stack（TrimPaths/Merge Paths/Offset/Pucker/Rounded/Wiggle/ZigZag/Twist/HandDrawnWobble/Stroke taper、9種実装済）のシェイプ VP 上インスペクタ／数値ハンドルドラッグ編集。
  - (6) パスの open/closed トグル、smooth toggle、corner ↔ bezier 切替を VP ハンドルで（`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:243-245` の selection grammar 整備と並ぶ）。
  - (7) シェイプレイヤー選択時の頂点/セグメント/タンジェントの選択 grammar をメイン VP 上で完成させ、`MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md:11-12,29` で計画中の `Artifact.Widgets.LayerEditor.ShapeOverlay` / `ShapeEditSession` / `ShapeHoverController` へ接続する。
- **価値／懸念:** AE 互換のシェイプ編集（特に頂点ドラッグ・tangent smooth・polygon segment insert・operator ハンドル）は既存の描画・データ層を破壊せずに機能を乗せられる層が既に厚く、メイン VP 側の実装ギャップはおおむね UI と routing 追加で済む。一方で (1) RenderOverlay への新規シェイプ専用 HUD 描画と `(7) ShapeEditSession` 抽出は `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` と相互作用し、`ArtifactCompositionRenderController.cppm` 28214 行・`ArtifactShapeLayer.cppm` 3380 行・`ArtifactCompositionRenderOverlay.cppm` 1827 行という巨大ファイル状態では変更影響範囲の見積もりが難しい。`MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md` の "incremental / stable" 方針に従い、まず最小スライスで 1 機能ずつ上げるのが安全。
- **次に必要なユーザー判断:**
  1. スコープ: 既存の `MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md` Phase D（キャンバス頂点編集）と Phase E（複数シェイプ）をそれぞれ独立に進めるか、または一括で (1)〜(7) をフェーズ計画に起こすか。
  2. 編集ホスト: メイン VP で直接編集（既存 `ArtifactCompositionRenderController` を拡張）か、`ArtifactRenderLayerWidgetv2`（LayerEditorPanel）側に集約して「ソロビュー」相当の編集ペインにするか。
  3. データモデル: 現状の単一 primitive を維持して `ShapeType` をツールプリセットにマップするか、コア `ShapeGroup` ベース（Phase E）へ移行してから VP 操作を実装するか。

## 検証運用

- **2026-09-04 — AI セグメンテーションの Core 契約:** `ArtifactCore/include/AI/ImageSegmenter.ixx` と `ArtifactCore/include/Image/DepthMap.ixx`。**事実:** 既存の `applySegmentationMask()` は空実装で、推論結果を書き込む `DepthMap` API がなかった。**対応:** 推論を `IImageSegmenter`（正規化された1ch前景マスク出力）へ限定し、Core 側で bilinear resample、閾値・softness・反転、alpha乗算／置換を適用する共有契約を実装した。`refineSegmentationMask()`で閾値／softness、foreground expand／contract、featherの共通後処理を追加し、`segmentBatch()`で複数の静止画／フレームから非破壊マスクを一括生成できるようにした。`analyzeSegmentationMask()`は foreground coverage／平均信頼度／bounds を返し、空マスクや過大マスクを App 側で警告できる。連番では `stabilizeSegmentationMask()` が前フレームマスクを控えめに混ぜ、推論のちらつきを抑える（動き追従は行わない）。モデル未配置時は、非AI・低品質であることを明示した `LuminanceImageSegmenter` を高コントラスト素材用のフォールバックとして追加した。**価値／懸念:** ONNX／DirectML、CPU fallback、将来のGPU推論はいずれも同一結果型に接続できるが、実モデル・モデル資産契約・GPU経路／実機品質は未検証。**次に確認:** 人物セグメンテーションモデルを1つ選定し、静止画の alpha 結果と既存 GPU mask 合成の preview／export parity を確認する。

- **2026-09-04 — ONNX/DirectML セグメンテーションアダプタ:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**事実:** 既存のONNX DirectML実装はテキスト生成専用で、画像モデルの入力・出力を `IImageSegmenter` に正規化する実装がなかった。またONNX compile definition/link は `ArtifactCoreAI` ではなく親 target にのみ付与されていた。**対応:** NCHW float 入力、最終2次元をマスクとするfloat出力のONNXモデルを、DirectML優先で読み込み、`DepthMap`へ戻すアダプタを追加。入力RGBのscale／mean／stddevと出力のNone／Sigmoid／Softmax、複数出力モデルの `outputIndex` を設定可能にし、出力マスクは bilinear で元解像度へ戻してsoft matteの連続値を保つ。AI target 自身へONNX link/defineを付与した。**価値／懸念:** 背景除去モデルをCoreだけで動かせるが、複数入力・動的shapeなどは設定契約を拡張してから対応する。**次に確認:** 実モデル（例: U²-Net系）を配置し、人物／髪のマット品質、DirectML利用、失敗時メッセージを実機確認する。

- **2026-09-04 — ONNX image module の明示BMI参照:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`。**事実:** `ArtifactCore` は実装 `.cppm` の primary interface attachment を自動dependency scanへ任せず、同ファイルで明示的なBMI参照を管理する。**対応:** `OnnxImageSegmenter.cppm` に primary interface と `Core.AI.ImageSegmenter` の参照を追加した。**価値／懸念:** Ninja/MSVCのdyndep不安定化を避けられるが、今後の新規 `import` 追加時にも同ファイルを同期する必要がある。**次に確認:** ユーザー許可後のCMake生成／ビルドで、OnnxImageSegmenterのIFC参照とONNXヘッダ解決を確認する。

- **2026-09-04 — ONNX image model diagnostics:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `modelInfo()` に ready、DirectML有効状態、入力サイズ・チャンネル、入力／出力テンソル名をまとめた read-only snapshot を追加。**価値:** App/UIを変更せずに、モデル契約と実行バックエンドの診断を接続できる。**次に確認:** 実モデル読み込み時にsnapshotとONNX Runtimeのsession情報が一致すること。

- **2026-09-04 — ONNX segmentation JSON configuration:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `loadOptionsFromJson()` を追加し、入力サイズ、前処理、letterbox、出力選択／activation、DirectML優先度を外部JSONから読み込む。既存sessionは設定変更時にresetする。**価値:** モデル資産を後で導入する際、コード変更なしにモデル固有契約を再現できる。**次に確認:** 実モデルの配布設定JSONを1つ作成し、モデル入力仕様と照合する。

- **2026-09-04 — ONNX segmentation letterbox pre-process:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `preserveAspectRatio` と padding value を追加し、固定サイズモデルへletterboxで渡し、出力マスクを同じ座標変換で元解像度へ戻す処理を追加。既定はstretchで後方互換を維持。**価値:** 縦長素材や正方形モデルで人物形状を歪めずに推論できる。**次に確認:** 16:9／9:16／1:1の実モデル結果でpadding境界とmask座標を確認する。

- **2026-09-04 — セグメンテーション失敗診断の統一:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `IImageSegmenter::lastError()` を共通契約へ加え、`segmentBatch()` が未ready・各item失敗・不正itemの最後の理由を返すようにした。**価値:** App側の一括処理UIが推論失敗を空マスクと誤認せず、ユーザーへ具体的に表示できる。**次に確認:** 実ONNXモデル不在・不正モデル・正常モデルでエラーが期待どおり更新されること。

- **2026-09-04 — 複数セグメンテーションマスクのCore合成:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `combineSegmentationMasks()` に Replace／Union／Intersect／Subtract を追加。入力解像度が異なっても `DepthMap` のbilinear samplingで target座標へ合わせる。**価値:** 人物＋髪、AIマスク＋手動補正、複数推論モデルの結果をQImage経由なしに共通マットへ統合できる。**次に確認:** 異解像度マスクでの境界品質、連続アルファのSubtract意味論、GPU cutoutとのpixel parity。

- **2026-09-04 — セグメンテーション自動適用の受入れガード:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `acceptsSegmentationMask()` と coverage／平均信頼度のしきい値設定を追加。**価値:** 空、または誤って画面全体を前景と判定したマスクを、Appが非破壊プレビューのまま停止・確認できる。**次に確認:** 実モデル別に人物／物体／背景なし素材の適正な閾値を決める。

- **2026-09-04 — OpenCV RotoBrush の IImageSegmenter adapter:** `ArtifactCore/include/AI/RotoBrushImageSegmenter.ixx`、`ArtifactCore/src/AI/RotoBrushImageSegmenter.cppm`。**事実:** 既存 `OpenCVRotoBrushEngine` はGrabCut初期マスク、前景／背景ストローク、Optical Flow伝播を実装済みだが、画像AIの共通契約へ未接続だった。**対応:** canonical BGRA float bufferを既存engineへ明示的に渡し、出力 `CV_8UC1` を `DepthMap` へ変換するadapterを追加。`propagateToNextFrame()` で、初期マスクを作成後の既存 Farneback flow 伝播をCore APIとして公開した。**価値:** モデルが無い環境でも、手動補正付きマットをONNX経路と同じbatch／refine／apply経路に渡せ、連番ではRotoBrushの追従を利用できる。**次に確認:** 現行engineのストローク座標・GrabCutマット・OpenCV例外時・大きなオクルージョンでの実機結果を確認する。

- **2026-09-04 — 軽量タスク facade の配置:** `ArtifactCore/include/Thread/LightweightTask.ixx` に、共有 `QThreadPool` を使う `executeLightweightTask` / `dispatchLightweightTasks` と、完了・キャンセル・失敗状態だけを持つ `LightweightTaskContext` を追加した。**事実:** 既存の `ThreadPool`、`Parallel`、`BackgroundTaskWorkerPool` は粒度や責務が異なる。**価値／懸念:** 短い非同期処理の入口を統一できる一方、context はタスク完了前に破棄できず、タスク内から `wait()` するとデッドロックする。**次に確認:** 実利用箇所を1つ選び、キャンセル・例外・pool飽和時の挙動をビルド／runtimeで検証する。

- **2026-09-04 — 2D Transform Gizmo の視覚ノイズ削減（Scale 中央 Y+ 軸線・Rotate 楕円重ね・軸 sweep 縮小）:** `Artifact/src/Widgets/Render/TransformGizmo.cppm` の `drawScaleCenterHandle` から Y+ 軸線 + tip ハンドルを撤去し、Rotate 描画ブロックから `drawEllipse` 2 本（X 軸赤 / Y 軸緑）と 68° sweep の X/Y 色分け弧 4 本のうち範囲を 36° に縮小。**事実:** 旧 `GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md` の「Scale の中心→四隅 X 線」記述は既に解消済みで、現コードの X 線正体は中央ハンドルの Y+ 軸線だった。Aspect Lock は `isCornerScaleHandle()` 側にあり、Center ハンドルの Y 軸線とは無関係。Rotate リングは `hitThickness = ringThickness * rotateRingHitBoost` で既に hit area と visual thickness が分離済み。**価値／懸念:** X 線ノイズ・4 軸 rainbow 効果・楕円重ねがそれぞれ薄れ、平面/画像レイヤーの Scale と Rotate 操作の視認性が上がるはず。`drawEllipse` ローカル関数（816 行）は未使用になるが残置、hit test・Undo・ショートカットには触れていない。**次に確認:** ユーザー許可後に `Artifact` のモジュールビルドを実行し、`ArtifactTransformGizmo` の IFC が正常に再生成され、Scale 4 隅ハンドル・Center ハンドル・Rotate リング・Leader・Drag arc の描画が既存と一致することを確認。

- **2026-09-04 — M-VP-9 Navigation Contract 現状マップの固定:** `docs/technical/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_STATUS_2026-09-04.md` を新規作成し、既存実装の静的マップ（Alt+LMB orbit / MMB pan / Wheel zoom / `PreviewOrbitSnapshot` による orientation・pan・zoom 保存復元 / Frame Selected・All・View Undo・Redo の QAction + QShortcut 経路 / `activeViewport()` 系）と未着手項目（navigation cross 表示 / active viewport 細い枠 / preview-only と camera layer の厳密分離 / pivot・orbit source selector / surface snap）を表形式で明文化した。**事実:** `ArtifactCompositionEditor.cppm:9220-9272` の `setPreviewOrbitMode` は camera state のみを snapshot 化し、navigation session フラグ（`isAltOrbiting_` / `isPanning_` / `isAltZooming_`）は含まれない。`maskNavigationLocked` 経路は ON 時の抑制のみ。Work Cursor は配置・中央化・消去・overlay 表示まで既存、Pivot source 切替と surface snap は未着手。**価値／懸念:** AGENTS.md の「RenderScheduler / DX12 パスはシビア扱い」「既存挙動を不用意に変えない」「新規 signal/slot 接続禁止」「QPainter / QImage / QtCSS 禁止」に従うと、navigation cross 追加は Editor → Overlay への状態渡し経路が必要で pane manager (M-VP-2) 移行と密結合のため、Phase 3 では**コード改変ではなく状態マップの固定**で止めた。**次に確認:** ユーザー許可後に (1) preview-orbit snapshot に `isAltOrbiting_` / `isPanning_` / `isAltZooming_` フラグを含めた場合の復元整合、(2) navigation cross を `previewOrbitMode_` ON 時のみ theme token のみで描画する場合の最小実装可否、(3) active viewport 細い枠を pane manager 移行なしで 1 段重ね描画できるかどうか、を順に判断する。

- **2026-09-04 — FFmpeg C API のモジュール境界:** `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm` では、vcpkg の FFmpeg ヘッダが C リンケージを自動付与しない構成だったため、`extern "C"` でグローバルモジュールフラグメント内のヘッダ群を包む必要がある。**事実:** 未解決シンボルが `?av...` と C++ 修飾されていたが、修正後は通常リンクまで進み、`/WHOLEARCHIVE` は複数定義を起こした。**価値／懸念:** C++20 module の import／リンク問題に見えても、まず ABI のリンケージ名を確認する。**次に確認:** FFmpeg を参照する他の module 実装でも同じヘッダ配置を維持する。

- **2026-09-02 — C++ module split target の IFC 参照は分岐順に注意:** `ArtifactCore/CMakeLists.txt` では `src/Mask/` の包括分岐が個別の `RotoMask.cppm` 分岐より先に評価されるため、後置した個別参照設定だけでは実際のコンパイルコマンドに反映されない。**関連:** `ArtifactCore/CMakeLists.txt`, `RotoMask.cppm`, `ConfigSchema.cppm`, `Artifact/CMakeLists.txt`。**価値／懸念:** split target 化では「設定が存在する」だけでなく、最終的な source property の適用順と生成コマンドへの反映を確認する必要がある。**次に確認:** ユーザー許可後の再生成・ビルドで、対象コマンドに `/reference` が現れ、C1199 が解消することを確認する。

- ビルド・テスト・CMakeはユーザーの明示許可後に実行する。
- runtime検証済みになった項目は、このファイルからアーカイブへ移す。
- 実装済みの細かな履歴や重複した検証候補は、新規Insightとして追加せずアーカイブを更新する。
## 2026-09-04 — Spatial Audio Object契約を既存3D Audio Layerへ接続

- **関連:** `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`、`ArtifactCore/include/Audio/Spatial/SpatialParams.ixx`、`ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 既存のSpatialRendererは距離減衰・azimuth panningを実装済みだったが、Audio Objectのstable ID、spread、gain、mute、enabledの保存契約が不足していた。
- **対応:** stable UUIDとAudio Object状態を追加し、JSON保存/復元、Property経路、最小ステレオspreadを既存レンダラーへ接続した。
- **未検証:** ビルド・実機再生・旧プロジェクトfixtureによる復元は未実行（ユーザー許可待ち）。
- **次の確認:** M-AU-9.2としてcallback境界でのallocation/lock不在、mono/stereo入力、seek/restart時の状態リセットを確認する。

## 2026-09-04 — mono空間音源のstereo preview拡張

- **関連:** `ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 入力がmonoの場合、従来の出力チャンネル数判定が1chを維持し、左右のazimuth gainを利用できなかった。
- **対応:** Phase 1のpreview契約として、出力バッファが1ch以下でも最低2chを確保し、stereo layoutを設定するよう修正した。
- **未検証:** 実機再生とサンプルレート別の音量・位相確認は未実行。

## 2026-09-04 — 7.1.4レイアウト契約の共通化

- **関連:** `ArtifactCore/include/Audio/AudioSegment.ixx`、`ArtifactCore/src/Audio/AudioBus.cppm`、`ArtifactCore/src/Audio/AudioDownMixer.cppm`、`ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm`
- **事実:** 既存レイアウト列挙には 7.1.4 がなく、12ch入力が汎用Stereoへフォールバックしていた。
- **対応:** 12chを `Surround714` として識別し、バス確保、downmixerの恒等マッピング、リングバッファ、LipSync、FFmpeg decoder、LFE判定へ接続した。
- **価値／懸念:** 7.1.4素材のチャンネル数とレイアウトを失わず保持できる。現時点では各スピーカーへの object 配分係数、UI／Render Queue選択、7.1.4からの明示的downmix係数は未実装・未検証。
- **次に確認:** 7.1.4 bedの標準チャンネル順を固定し、Render QueueとPreviewの出力選択へ接続する。

## 2026-09-05 — Particle 3D のレイヤー変換境界

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`ArtifactCore/include/Graphics/ParticleData.ixx`
- **事実:** `ArtifactParticle3DLayer` は `is3D=true` で camera view/projection 経路へ入り、3D分岐では2D `QTransform` を避けている。`ArtifactFormParticleLayer` も Grid3D 時に同じ経路へ入るが、レイヤーのモデル行列は未設定だった。
- **対応:** `ParticleRenderData`、Core constant buffer、GPU cull shader、vertex shader、Diligent submitterを通るmodel matrix経路を追加し、Form Particle のGrid3Dにも接続した。2D経路はidentity model matrixを維持する。
- **価値／懸念:** camera orbit とlayer transformの責務を分離できる。constant-buffer layoutを変更したため、D3D12/Vulkan双方でshader/PSO再生成を伴う。
- **次に確認:** runtimeで Particle 3D の位置・回転・scale を個別に変更し、camera orbitとは独立して反映されるか、GPU cullと2D Particleに回帰がないか確認する。

## 2026-09-05 — PhysicsSystem のモジュール内コンテナ破棄

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`、`ArtifactCore/include/Physics/MpmSolver2D.ixx`、`ArtifactCore/src/Memory/SharedPtr.cppm`
- **事実:** MSVC 19.51 は、exported `PhysicsSystem` の `std::map<LayerID, SharedPtr<MpmSolver2D>>` を破棄するテンプレート展開中に C1001 を発生させた。`MpmSolver2D` は import 済みの完全型であり、所有契約の不備は確認されていない。
- **対応:** `LayerID` には `std::hash` がないため `IdMap` は採用せず、該当ストアを既存の `NamedVector` による小さなキー付きエントリへ移した。デストラクタもクラス外 default 定義へ置き、キー別の登録・取得・削除・列挙の契約を保持しつつ、MSVCが落ちる `std::_Tree` と `std::_Hash` の実体化を排除した。
- **未検証:** 影響する最小経路は Core の起動・PhysicsSystem singleton の終了時破棄。ユーザーのビルドで C1001 が解消すること、および Physics/Material solver の生成・破棄を確認する。

## 2026-09-05 — Native Dock の右端編集面を限定

- **関連:** `Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **事実:** Native Dock Surface では Inspector 系に加え、Layer View、Contents Viewer、Audio Mixer も右端へ登録されていた。AI Cloud は既に起動時非表示だった。
- **対応:** Layer View を Composition Viewer の中央タブ、Contents Viewer を Project の左タブ、Audio Mixer を Timeline の下部タブへ移した。右端は Inspector / Properties / Components / Effects を優先し、AI Cloud は非表示のままにした。
- **未検証:** 初期レイアウト、既存保存レイアウトからの復元、Timeline 未生成時の Audio Mixer の下部配置は未実行。

## 2026-09-05 — Default ワークスペースの初期可視性を軽量化

- **関連:** `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **事実:** Default ワークスペースは Composition Viewer / Project / Asset Browser / Inspector / Effects / Properties と Timeline を同時に可視化していた。
- **対応:** Default は Composition Viewer / Project / Inspector のみを表示し、Asset Browser、Effects、Properties と Timeline を初期非表示にした。Animation ワークスペースは Timeline の自動表示を維持する。
- **未検証:** 保存済みレイアウト復元後の可視状態、および明示的に開いた Asset Browser / Timeline の操作性は未確認。

## 2026-09-05 — ギズモ入力座標と即時描画の境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactCompositionRenderOverlay.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`
- **事実:** press/move の入口で物理ピクセルへ変換済みだが、projected frame press と3D dragで DPR を再乗算していた。フレームの8ハンドルより3D軸判定が優先されていた。描画は即時ではなく頂点を蓄積し、flush 時のカメラ行列を使う。即時描画と判断して末尾 flush を削除したのは誤りだった。
- **対応:** 二重変換を削除し、近いフレームハンドルを優先。フレーム描画前に flush し、選択イベントでも base composite を無効化。標準モードは移動軸とフレームに整理した。
- **未検証の別件:** 過去フレームの平面操作にも同様の DPR 再乗算が見える。今回の現行フレーム修正範囲から分離し、motion frame操作を次に確認する。共通矢印プリミティブの形状変更はライト等の矢印にも反映されるため外観確認が必要。
- **確認待ち:** 平面追加直後、100/150/200%表示倍率での8方向ドラッグ、回転モード、Undo/キャンセル。ビルド・実機操作は未実行。
- **水色全面表示への修正:** フレーム末尾の `flushGizmo3D()` を復元し、カメラ行列のリセット前にハンドル頂点を送信する。水色全面表示の解消は実機未確認。
- **初期フィット・リサイズ追補:** 通常2D平面のフレームに scene camera が使われる経路を canvas pan/zoom に統一。外側余白を描画・pick・snap・固定点の全箇所から除去した。ドラッグ中の単一レイヤー再同期を止め、固定点補正は2Dのvisual/local scale比と3D回転を考慮する。単一レイヤーのdrag通知は既存の間引き処理を使用し、releaseで最終値を強制通知する。負荷改善と初期フィット、回転・親付きレイヤーの対辺固定は実機未検証。
- **位置飛びの再調査:** `positionXAt/YAt` は初期位置を含まないoffsetで、`snapshotAt` が初期位置を加算することをCore実装で確認。ギズモのpress/modal/group/release保存を後者へ変更した。`UndoManager::push` は即redoするため、releaseでoffsetを絶対位置として再適用すると位置が飛ぶ。通常ビュー行列は実際の `canvasToViewport` から構成し、ギズモ描画による2D行列上書きを撤去、drag rayは開始カメラを保持する。直交ギズモのサイズは描画viewの倍率から計算する。ユーザー報告の3症状について実機での改善確認は未完了。
- **正面直交ビューへの統一:** ユーザー指定により起動時・2D復帰をFrontへ統一。正面の投影は直交、その他の方向は既存透視投影を使用する。直交カメラはprojection側にzoomを保持するため、ギズモのサイズ補償もviewport zoomへ合わせた。コンポジション境界の水色重ね描きを中立色へ変更。panにも既存のinteraction通知を追加し、操作中のLODとreadback抑制を有効にする。パン・ズームが重い主因と改善量は未測定で、実機確認が必要。
- **スナップ設定:** 既存ViewメニューのsnapGuidesチェックと永続化経路を再利用し「コンポジション／ガイドにスナップ」としてprojected frameの移動・リサイズへ接続。Frontのみ、Altで一時解除、10 logical pxの閾値。コンポジション境界はフレームと同じ投影を使用する。OFF時は吸着ガイドを消去。定規は未設定時に非表示、保存済み設定は維持。実機でON/OFF・再起動・Alt・ズーム倍率別の吸着・Undoは未検証。
- **正面消失・スナップ追補:** Qt直交行列の近側は負のNDC深度になるが、PrimitiveRenderer3Dのshaderは投影結果をそのままSV_Positionへ送っていた。Frontの直交投影をD3D12/Vulkanの0..1深度へ補正した。移動軸のpress経路はprojectedFrameMoveを立てないため吸着対象外だったので、位置適用方式は変えずScreen移動とWorld/ViewのX/Y移動を吸着対象へ追加した。軸拘束と直交するガイドは表示しない。ローカル軸の吸着、Shift精密操作・Ctrl量子化との併用は未検証。正面での再表示、通常移動・8方向リサイズの吸着、OFF/Alt、ドラッグ確定・Undoを実機で確認する必要がある。GPUリソース・同期・キャッシュの寿命は変更していない。

## 2026-09-05 — 頂点のみPLY点群の最小受入

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、Artifact/src/Layer/Artifact3DModelLayer.cppm (draw)、3D系ファイルフィルタ3箇所
- **事実:** loadPly は aceCount<=0 を拒否していたため頂点のみPLYが読めず、FileTypeDetector は既に ply を Model3D 扱いなのに開く側のフィルタ3箇所に *.ply が無かった。Mesh::isValid() は polygon>0 必須だが Artifact3DLayer::loadFromFile は ertexCount>0 判定なので importer 側の修正だけで meshLoaded_ まで届く。generateRenderData() は polygon 無しで空を返すため drawMesh 経路では何も出ない。
- **対応:** loadPly で face無しを受入れ、頂点色 (red/green/blue 系・uchar/float両対応) を color アトリビュートへ格納、頂点上限200万で拒否。Artifact3DLayer::draw の Solid/Wireframe 両経路に polygon==0 時の点描画フォールバック (3軸クロス、32768点cap・stride間引き、頂点色・opacity反映、trace points-submitted) を追加。フィルタ3箇所へ *.ply 追加。.ixx 変更なし。
- **未検証:** ビルド・実機表示 (頂点のみPLY、色付きPLY、既存ポリゴンPLYの回帰)、200万超・バイナリPLYのエラー表示、大規模点群の描画負荷。バイナリPLY/LAS/LAZ/E57 は対象外のまま。

## 2026-09-05 — 点群フォールバックの継続 (選択表示・テスト)

- **関連:** Artifact/src/Layer/Artifact3DModelLayer.cppm (drawSelectionOutline)、	ests/ArtifactCore/MeshImporterPlyTest.cpp、	ests/ArtifactCore/CMakeLists.txt
- **事実:** polygon==0 の点群は選択アウトラインの polygon ループが空振りで何も出なかった。MeshImporter への gtest は存在せず、	ests/models/test.obj を参照するテストも無かった。drawFractureOverlay は mesh 非依存のため点群でも安全。
- **対応:** 選択時は world-space の bounds box (12辺) を outline 色で描画。PLY は一時ファイル自己完結の gtest 6件 (face無し/face0/uchar色/ポリゴン回帰/欠損/バイナリ拒否) を追加し、ArtifactCoreMeshImporterPlyTest として登録。
- **未検証:** ビルド・テスト実行はユーザー指示で保留 (ArtifactCore の増分ビルドは MeshImporter.cppm のコンパイル・リンクまで成功済み、Artifact 側は中断)。

## 2026-09-05 — バイナリPLYと点サイズ調整

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、Artifact/src/Layer/Artifact3DModelLayer.cppm、	ests/ArtifactCore/MeshImporterPlyTest.cpp
- **事実:** バイナリPLYは finiteness・色・face を含め未対応だった。点の見た目は bounds 由来の自動サイズのみで調整手段が無かった。
- **対応:** loadPly をヘッダ構造体解析へ組替え、binary_little/big_endian に対応 (型: char〜double・list face・uchar/float等の色正規化、200万点上限・truncated 検出は ASCII と共通)。Artifact3DLayer に 
ender.pointSize (0.25〜8.0、既定1.0、JSON保存・Inspector・set 反映、crossHalf に乗算) を追加。.ixx 変更なし (normalLength と同方式)。テストはバイナリ実ペイロード (LE/BE/truncated) へ置換え。
- **検証:** ArtifactCore 増分ビルド成功、Artifact3DModelLayer.cppm.obj 単体コンパイル成功。Python ミラーで LE/BE 値と byteswap 経路を確認。テスト実行・実機表示は未実施。LAS/LAZ/E57 は対象外のまま。

## 2026-09-05 — 非圧縮LASの最小受入とE57見送り判断

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadLas)、ArtifactCore/include/Geometry/MeshImporter.ixx (Backend::Las)、ArtifactCore/src/File/FileTypeDetector.cppm、3D系フィルタ3箇所、	ests/ArtifactCore/MeshImporterLasTest.cpp
- **事実:** LAS 1.0-1.4 の非圧縮 point format 0-8 はヘッダ固定オフセット (scale/offset/count) と record 先頭の XYZ・format別RGB位置で読める。LAZ は同一シグネチャのまま圧縮本体のため別 decoder が要る。E57 (ASTM E2807) は XML＋packet/codec バイナリ構成で、Qt の XML だけでは binary section の実装が数百行規模になる。
- **対応:** loadLas を追加 (format 0-8、XYZ+RGB/intensity→gray、上限200万点、waveform/未知format・不正scale・truncated は明示エラー、LAZ は非対応メッセージ)。dispatch・Backend::Las (末尾追加)・FileType・フィルタ3箇所へ las 配線。gtest 4件 (format2 RGB / format0 intensity / waveform拒否 / 非LAS拒否) を登録。E57 は外部libなしの自前実装を見送り。
- **検証:** ArtifactCore・ArtifactCoreFile 増分ビルド成功。Python ミラーで header/record オフセットと期待値を照合。テスト実行・実機表示は未実施。

## 2026-09-05 — 空間音声の時刻評価と広域出力の境界

- **関連:** `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 音声要求は start frame を持つが、`getGlobalTransform4x4()` は現在タイムライン時刻を参照する。SpatialRenderer の係数配列は 8 要素で、出力 scratch のチャンネル数を元にループする。
- **今回の対応:** SpatialAudio の出力 scratch を空から開始し、mono/stereo 素材の stereo preview に限定した。
- **懸念（未検証）:** 先読み／export 時の位置評価が要求音声時刻からずれる可能性がある。汎用レンダラーへ 12ch scratch を直接渡す将来経路では固定長配列の範囲外アクセスに注意が必要。
- **価値／次の確認:** 任意時刻の親子 transform 評価を共通 API で提供できるか確認する。7.1.4 接続前に出力 layout と係数容量の契約を確定する。


## 2026-09-10 — プリコンポーズ改善 B/F 採用と A/C/D/E 見送り

- **関連:** `docs/planned/MILESTONE_PRECOMPOSE_BREADCRUMB_DUPLICATE_2026-09-10.md`、`docs/planned/COMPOSITION_PRECOMPOSE_ANALYSIS_2026-04-17.md`、`docs/planned/GROUP_CONTAINER_MIGRATION_PLAN_2026-08-27.md`、`docs/analysis/AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`。
- **確認できた事実:** ユーザー指示は B (Breadcrumb + In-place) と F (Duplicate Deep / Instance / Un-precompose) を採用、残りは検討。`PreComposeManager::precompose()` は未実装部あり、`GroupContainer` 移行は Phase 0/1 済み・実体化未着手。Viewport モックは周辺 UI のみ採用でキャンバス内変更不可。
- **対応:** B/F の契約定義 milestone を `docs/planned/` に新規作成。親ゴースト表示は既定範囲外、A/C/D/E は着手条件付きの検討事項として記録。子 repo 変更・ビルド実行なし。

## 2026-09-10 — AE メニューバー不満の付録化

- **関連:** `docs/planned/MILESTONE_PRECOMPOSE_BREADCRUMB_DUPLICATE_2026-09-10.md` 付録。
- **確認できた事実:** ユーザー承認で前回回答のメニュー別不満・改善を同 milestone へ付録追記した。B/F 本体との対応表付き。コード変更なし。
- **価値／懸念:** Navigate と複製命名を B/F と同一に縛り、メニュー肥大・用語分裂を防ぐ。コマンドパレット・配置保存は範囲外として分離。

## 2026-09-10 — AE タイムライン不満の付録化

- **関連:** `docs/planned/MILESTONE_PRECOMPOSE_BREADCRUMB_DUPLICATE_2026-09-10.md` 付録。
- **確認できた事実:** ユーザー承認でタイムライン不満・改善を同 milestone へ付録追記した。左ペイン痩身・Baseline 維持・B/F 連動・ホットパス制約を含む。コード変更なし。
- **価値／懸念:** 親子切替・複製命名を B/F と同一に縛り、Timeline 肥大・用語分裂を防ぐ。時間集約・検索は定義のみで独立スライス候補。
- **次に確認:** 付録が肥大化したら独立 milestone へ分割する。

- **次に確認:** 付録が肥大化したら独立 milestone へ分割する。

- **価値／懸念:** 往復コストと複製事故の解消に絞り、GroupContainer 移行との依存を契約先行で吸収する。一方 F-2 実装は PreCompose 未実装部と Container 実体化に依存するため、定義だけでは動作確認できない。
- **次に確認:** B-2 共有状態の保存先、F-1 既定選択、Tracker 状態除外の維持をレビューする。

## 2026-09-05 — 点群のvoxel間引き (LOD最小スライス)

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (decimatePointCloud)、	ests/ArtifactCore/MeshImporterPlyTest.cpp
- **事実:** 取込上限200万点でも描画は32768クロスへstride間引きするだけで、メモリ (2M点で約100MB超) と形状代表性に課題があった。octree/out-of-core は別規模の設計になる。
- **対応:** 頂点のみ点群に voxel 間引きを追加 (budget 262144、初回cellは体積/上限の立方根、不足時は最大8回半分化、セル先着・入力順決定性・色連動、NaN/極端座標ガード)。PLY/LAS 両経路で適用し、間引き時は qInfo で before→after を出す。gtest に30万点バイナリの間引き・決定性テストを追加。
- **検証:** ArtifactCore・ArtifactCoreFile 増分ビルド成功 (Property.ixx の C5202 は既存)。Python ミラーで cell・kept数・先頭点保持を確認。テスト実行・実機表示は未実施。

## 2026-09-05 — 点群テスト13件が実実行で成功・QDataStreamの罠

- **関連:** 	ests/ArtifactCore/MeshImporterPlyTest.cpp (9件)、	ests/ArtifactCore/MeshImporterLasTest.cpp (4件)
- **事実:** ARTIFACT_BUILD_TESTS=OFF のため再configure (-DARTIFACT_BUILD_TESTS=ON) して実行。初回は binary 4件が失敗したが、原因はローダーではなく QDataStream の既定 DoublePrecision (float が8バイトで書かれる) だった。デバッグテストで bodyHex を確認し、setFloatingPointPrecision(SinglePrecision) で解決。LAS側は整数/double明示書きのため当初から成功。
- **対応:** binary fixture 全件に SinglePrecision を指定、デバッグテストは削除。PLY 9件・LAS 4件の全成功を確認。
- **教訓:** バイナリ fixture を QDataStream で書く場合は SinglePrecision を必ず指定すること。
- **未検証:** 実機での点群表示・選択・Inspector (アプリ全体ビルドは未実施)。

## 2026-09-05 — 3D Stroke (PathTube trim) と Lux (Light glow) の最小実装

- **関連:** ArtifactCore/.../Procedural3DGenerators.{ixx,cppm}、Artifact/src/Layer/ArtifactProcedural3DLayer.cppm、Artifact/src/Layer/ArtifactLightLayer.cppm、Artifact/include/Layer/ArtifactLightLayer.ixx、	ests/ArtifactCore/PathTubeTrimTest.cpp
- **事実:** PathTube に trim 概念が無く、Light層に可視 glow が無かった。ArtifactLightLayer::shouldIncludeInFinalRender() は false のため Light の draw 内容はビューポート限り。3D層描画時は Scoped3DLayerCamera により particle 3D カメラが有効 (ArtifactCompositionRenderController.cppm:7924) のため、Light の draw 内 drawParticles は world 座標で解釈される。
- **対応:** PathTube に 	rimStart/trimEnd (既定0/1で旧挙動一致、taper/twist/UV は trim 範囲追従、反転は空メッシュ) を追加し、層の JSON・Inspector・setter を配線。Light層に Light/Glow・Glow Size・Glow Intensity (既定ON/1/1、Point/Spot/Area のみ、range/cone連動サイズ・additive billboard 1 sprite) を追加し、JSON・Inspector・setter を配線。最終レンダーへの glow は render-queue 側の別件として残す。
- **検証:** ArtifactCore ビルド、ArtifactProcedural3DLayer・ArtifactLightLayer 単体TUコンパイル、trim gtest 4件全成功。実機表示は未実施。

## 2026-09-05 — Mesh法線生成とbounds sphere

- **関連:** ArtifactCore/include/Mesh/Mesh.ixx、ArtifactCore/src/Mesh/Mesh.cppm、ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、	ests/ArtifactCore/MeshGeometryTest.cpp
- **事実:** PLYポリゴンは法線ゼロで書き出され、ライティングが壊れていた (STLはfacet法線を自前計算、ufbxはソース保持)。Mesh にトポロジからの法線生成が無く、bounds も AABB のみだった。
- **対応:** Mesh::computeVertexNormals() (面積重みスムーズ法線、縮退面スキップ、未使用頂点は(0,0,1)、revision bump) と oundingSphereCenter/Radius() (AABB版、updateBoundsで同時更新) を追加。loadPly は面あり時のみ法線生成。gtest 5件 (quad/縮退/点群no-op/sphere/PLY経由) を追加。
- **検証:** MeshGeometryTest 4件・MeshImporterPlyTest 10件の全成功を確認。

## 2026-09-05 — 真の弧長サンプリングと解析的接線

- **関連:** ArtifactCore/.../BezierCalculator.{ixx,cppm}、ArtifactCore/.../BezierPathSampler.{ixx,cppm}、	ests/ArtifactCore/BezierArcLengthTest.cpp
- **事実:** evaluatePath はセグメント毎t均等であり、sampleEquidistant/sampleByCount/sampleWithTangents の「等間隔」は不正確だった (不等長セグメントで偏る)。接線はeps差分近似だった。外部呼出しはサンプラ内部のみで、変更の波及は閉じている。
- **対応:** 弧長テーブル (32分割/segment・二分探索・t補間) を追加し、sampleEquidistant/sampleByCount/sampleWithTangents を弧長経路へ切替 (シグネチャ不変)。pointAt はt均等のまま互換維持し、pointAtArcLength/	angentAtArcLength/sampleArcLength を新設。evaluateTangent (解析的導関数・縮退時(1,0)) を追加し、	angentAt を含め差分近似から置換え。縮退パスは有限値を返す。
- **検証:** gtest 6件 (不等長均等・端点・直線接線・縮退・閉ループ・解析接線) の全成功を確認。

## 2026-09-05 — CatmullRom/Hermite実装とbezierEvaluateの式バグ修正

- **関連:** ArtifactCore/include/Geometry/Interpolate.ixx、	ests/ArtifactCore/KeyframeSplineTest.cpp
- **事実:** InterpolationType::CatmullRom/Hermite は宣言のみで dispatch は Linear 落ちだった。また ezierEvaluate の Newton ソルバとy評価式に余分な mt3 項があり (P0=(0,0) なのに +mt3)、全Bezierイージングがずれていた。テストが easy-ease 中点 5.0 に対し 6.25→39.32 を返したことで発覚。Pythonミラーで正値 (0.1292/0.5/0.8708) を確認。
- **対応:** hermiteInterpolate/catmullRomInterpolate を追加し、KeyframeInterpolator::evaluate で隣接キー参照の CR (均一) と有限差分接線 Hermite (非均一対応) を実装。2点版 interpolate() は Linear 維持。bezierEvaluate は mt3 を除去し正規形へ。gtest 8件を追加。
- **検証:** 8件全成功。既存 Linear/Bezier 挙動の回帰テストを含む。Bezier全般の値が変わるため、既存プロジェクトの見た目差分は実機で要確認。

## 2026-09-05 — CatmullRom/Hermiteのメニュー露出

- **関連:** Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm、Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm
- **事実:** pplyInterpolationToSelectedKeyframesImpl は type を汎用設定するため、メニュー追加だけで CR/Hermite が適用可能だった。キーフレーム色・ラベル・形状は型別 switch で、新規型は default 落ちだった。
- **対応:** Animationメニュー (キーフレーム補間) と Timeline右クリック (Interpolation) に Catmull-Rom/Hermite を追加。ショートカットは追加しない。キーフレーム色 (紫系2色)・ラベル・形状 (六角/五角) を追加。EasingLab は単区間previewのため対象外。
- **検証:** 両TUの単体コンパイル成功。実機のメニュー表示・適用・保存復元は未実施。

## 2026-09-05 — 不足easingの一括実装

- **関連:** ArtifactCore/include/Geometry/Interpolate.ixx、	ests/ArtifactCore/EasingFunctionsTest.cpp
- **事実:** enumにありながら dispatch が Linear 落ちだった型が多数 (Smooth/EaseOutIn/Quadratic/Cubic/Quartic/Quintic/Exponential/Logarithmic/Sine/Circular/Cosine)。2点版 interpolate() の Bezier は 
eturn start のままだった。
- **対応:** 純alpha系19種を追加 (CubicIn/InOut、Quartic/Quintic各3種、SineIn/InOut、Circular各3種、Exponential各3種、Logarithmic、Cosine、EaseOutIn、Smooth; Quadratic→EaseIn、Cosine/Smoothは同一曲線)。新規は alpha clamp 付き。Bezierスタブは Linear フォールバックへ (呼び出し側はbezierInterpolateへ迂回済み)。gtest 5件 (端点・既知中点・dispatch・Keyframe経由・範囲外有限) を追加。
- **検証:** 5件全成功。Spring/SmoothDamp (状態持ち)、CustomCurve/Polynomial (係数要)、色文脈系、2点Hermite/CR は対象外のまま。

## 2026-09-05 — EasingLab候補の拡張とBezierプレビュー修正

- **関連:** ArtifactCore/include/Animation/EasingCurveUtil.ixx、Artifact/src/Widgets/Timeline/EasingLabWidget.cppm、	ests/ArtifactCore/EasingLabCurveTest.cpp
- **事実:** EasingLab候補は16種で、新規 easing (Quartic/Quintic/Sine/Circular 等) が未露出だった。同ファイルの Bezier プレビューにも ezierEvaluate と同じ余分な mt3 項があった。
- **対応:** EasingType に EaseOutIn/Smooth/Quartic/Quintic/Sine/Circular を追加し、評価・名称・Interpolation対応・候補一覧を配線。Bezier の mt3 を除去。EasingLab の対応ラベルを追加。gtest 4件を追加。
- **検証:** 4件全成功、EasingLabWidget 単体TUコンパイル成功。実機のダイアログ表示は未実施。

## 2026-09-05 — マット検証 (順序・premult・GPU上限・輝度)

- **関連:** ArtifactCore/.../LayerMatte.{ixx,cppm}、Artifact/src/Render/ArtifactCompositionViewDrawing.cppm、Artifact/src/Render/ArtifactRenderQueueService.cppm、	ests/ArtifactCore/MatteStackTest.cpp
- **事実:** stack順序 (参照順・初回引継) と premult 一貫性 (RGBA一律乗算) は CPU/GPU/Core で一致。相違点: (1) GPU は3マット超で stack 全体を無適用化 (CPU は全数適用)、(2) GPU は Stretch 以外・ネスト/3D/adjustment ソースで stack 全体を無適用化、(3) Core evaluateMatteStack は alpha のみで luma 未対応、(4) 実働経路 (preview CPU・GPU queue) は BT.601 で一致する一方 Core 既定は 709、(5) Core MatteStackMode に Difference が無かった。evaluateMatteStack/MatteEvaluator に実働呼出しは無い。
- **対応:** MatteStackMode::Difference を Core + view の switch に追加 (末尾追加・既定値不変)。>3マット時に preflight Warning 診断を追加。gtest 11件 (sample/combine/apply/順序/skip/反転/空passthrough/roundtrip) を追加。
- **検証:** 11件全成功。TU単体コンパイル成功。実コンポでの受入れ・GPU実機は未実施。

## 2026-09-05 — マスク編集ハンドルは画面座標で判定する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、VP マスク編集
- **事実:** レイヤーローカル座標でハンドルのヒット距離を測ると、レイヤーの非一様スケールやズームにより見た目のクリック領域と判定がずれる。フェザー値ゼロでは実ハンドルが頂点と重なり、直接ドラッグを開始できない。
- **対応:** ハンドル判定を viewport のピクセル距離に統一し、競合時は最も近い候補を選ぶ。ゼロ・フェザーには画面上だけの最小距離ハンドルを表示し、ドラッグ開始点からの法線方向差分でフェザーを立ち上げる。
- **価値／次の確認:** マスク頂点とセグメントのヒット判定も同じ画面距離モデルへ段階的に揃えると、極端なレイヤー変形時の操作一貫性をさらに高められる。実機で非一様スケールしたレイヤーの操作感を確認する。

## 2026-09-05 — 生成プリセットの既存基盤と拡張境界

- **関連:** `Artifact/include/Project/ArtifactPresetManager.ixx`、`Artifact/include/Layer/ArtifactGenerationPreset.ixx`
- **事実:** 既存の `ArtifactPresetManager` は平面・画像・Shape・Text と基本マスクの作成定義を JSON 化できるが、レイヤーエフェクトと Text Animator を同一トランザクションに含める表現を持たない。
- **対応:** レイヤー／マスク／エフェクト／Animator を同じ JSON レシピで表す `ArtifactGenerationPreset` を追加し、`New > Presets` 実行時は既存 `AddLayerCommand`・`MaskEditCommand`・`AddLayerEffectCommand` を `MacroUndoCommand` に集約した。
- **価値／次の確認:** 基本作成プリセットと生成プリセットの JSON 統合は将来候補。現時点では両者の機能範囲が異なるため、既存スキーマを変更せず併存させている。実機で redo/undo 後の選択状態とユーザー JSON 再読込を確認する。

## 2026-09-06 — マルチスレッド基盤の段階移行 1+2: ThreadPoolのTBB shim化とTaskflow TaskSystemの併存

- **関連:** ArtifactCore/include/Common/ThreadPool.ixx、ArtifactCore/include/Common/TaskSystem.ixx、ArtifactCore/CMakeLists.txt、ArtifactCore/cmake/ArtifactCoreSources.cmake、cpkg.json。
- **事実:** ThreadPool は集中キュー+mutexでfine-grainedに弱く、Core.Parallel は既に tbb::parallel_for で TBB 依存だった。vcpkg には tbb のみで taskflow は未導入。ArtifactCore→ArtifactCoreEnvironment は静的リンクで参照循環しないが、モジュールは /reference + OBJECT_DEPENDS の既存パターンを踏襲する必要があった。
- **対応:** 1) ThreadPool を TBB task_arena/task_group 背景の shim に置換。API(enqueue/enqueueTask/waitAll/globalInstance)は完全維持し、内部のみ work-stealing化。concurrency は hardware_concurrency()で統一。globalInstance は deprecated 付与し DAG は TaskSystem 推奨へ誘導。2) Taskflow (header-only) を vcpkg.json に追加し、Core.TaskSystem (tf::Executor ラッパ、async/silent_async/run/corun/wait_for_all、for_each_index ヘルパ)を新設。3) CMake は Taskflow::Taskflow を ArtifactCore にリンクし、TaskSystem.ixx を CORE_MODULES へ追加。configure/build の疎通を確認。無関係な作業中変更(ArtifactRegex等)によるビルド破綻は一時退避で分離し、検証後は復元せず除外。
- **価値／懸念:** 既存呼び出しは再コンパイル不要で即座に work-stealing の恩恵。Taskflow は DAG/協調実行(corun)でデッドロック回避が可能。3プール(QThreadPool/TBB/Taskflow)が一時併存するためスレッド過剰生成に注意が必要だが concurrency 統一で緩和。P2300 全面採用は見送り、データ並列は TBB、DAG は Taskflow の分担で最新知見を段階導入。
- **次に確認:** 実機での ThreadPool 呼び出しの順序依存有無、TaskSystem を用いた matte/DAG の PoC、QThreadPool との最終一本化、P2300 のコンパイラ対応推移。

## 2026-09-06 — TaskSystem DAGのマットPoC

- **関連:** ArtifactCore/include/Common/TaskSystem.ixx、	ests/ArtifactCore/TaskSystemMattePoCTest.cpp。
- **事実:** ThreadPool はデータ並列向け、TaskSystem は DAG/協調実行向けに分担したが、マット処理は依然逐次の evaluateMatteStack のみだった。Taskflow の for_each_index はモジュール境界で ODR/link 問題を起こしやすい(今回も LNK2019)。
- **対応:** PoC として TaskSystem を用いたマット並列評価を gtest 4件で実証。DAG依存(A→B,C→D)、corun デッドロック回避、Taskflow DAGで3ソースのマスク生成を並列化して逐次結果と一致(64x64 Add, 期待1.0)、並列 for 相当の動作。task_for ヘルパはモジュール内テンプレートのリンク問題で一旦除去し、直接 emplace で代替。テストは <taskflow/taskflow.hpp> を直接 include してモジュール透過問題を回避。
- **検証:** 4/4 passed。MatteStack の実運用への組み込みは未着手だが、PoCで並列化の等価性と協調実行の有効性を確認。

## 2026-09-06 — Phase2: 専用プールのTBB一本化 (AsyncAssetRead / RenderScheduler)

- **関連:** Artifact/src/IO/AsyncAssetReadScheduler.cppm、Artifact/src/Render/ArtifactRenderScheduler.cppm、ArtifactCore/include/Common/TaskSystem.ixx。
- **事実:** 専用プール2つが QThreadPool に依存していた。AsyncAssetRead は I/O バーストで priority 付き start、RenderScheduler は QThreadPool::start で並列実行し invokeMethod でメインへ帰着。どちらも QThread のイベントループには依存せず、ScopedThreadName/Trace は TBB スレッドでも有効。
- **対応:** AsyncAssetRead: QThreadPool → TaskSystem (1-8 arena), setMaxThreadCount/expiryTimeout を除去、waitForDone→wait_for_all、priority は一旦無視 (別arenaで優先度エミュレート可能だが初期は均一)。RenderScheduler: unique_ptr<QThreadPool> → TaskSystem, ensureTaskSystem()で遅延生成+concurrence変更時は再生成、maxThreadCount 参照を concurrency() に、start→silent_async に。Thread.Helper の ScopedThreadName は維持。
- **検証:** cmake configure 成功 (33s)。ビルドはユーザ指示で中断、オブジェクト個別の dyndep 生成は確認。QtConcurrent 13箇所・QThreadPool globalInstance は Phase3 で残置。

## 2026-09-06 — Phase3ヘルパ: QFutureWatcher代替の asyncPostToObject

- **関連:** ArtifactCore/include/Common/TaskSystem.ixx。
- **事実:** 残る13箇所の QtConcurrent::run(&sharedBackgroundThreadPool()) は QFutureWatcher::finished でメインへ帰着していた。TaskSystem::async は std::future を返すため QFutureWatcher と非互換。
- **対応:** TaskSystem に asyncPostToObject<T>(QObject* context, work, onFinished) を追加。TaskSystem::silent_async で実行し、結果を QPointer ガード付きで QMetaObject::invokeMethod(QueuedConnection) で context スレッドへ配送。QThreadPool 依存の prewarm/専用プールは Phase1/2 で除去済みのため、残りはこのヘルパで1行置換可能。ArtifactCore ビルド確認済み。
- **次に確認:** VideoLayer/ImageLayer/AssetBrowser 等の各サイトを同ヘルパで順次置換し、QThreadPool globalInstance への依存を完全に除去するか検証。
## 2026-09-06 — GPUジョブプール基盤の初期境界

- **関連:** `ArtifactCore/include/Graphics/GPUThreadPool.ixx`、`ArtifactCore/src/Graphics/GPUThreadPool.cppm`
- **事実:** GPU側には個別のCompute dispatch経路はあるが、共通のジョブ投入・容量制限・診断契約は無かった。
- **対応:** Diligentを公開APIに露出させないホスト側キューを追加。`enqueue`、`drain(executor)`、キャンセル、ジョブハンドル、キュー統計を提供し、既存のレンダリング経路には接続していない。
- **価値／懸念:** 将来のDiligent Compute executorを差し込めるが、現時点の完了状態はGPU fence完了ではなくexecutor受理結果である。GPU非同期完了を扱う段階でfence世代を追加する必要がある。
- **次に確認:** 実際のDiligent command recording／submission境界と、D3D12・Vulkan共通のfence再利用契約を確定する。

## 2026-09-06 — View メニューの情報設計整理

- **関連:** `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`、`Artifact/src/Widgets/ArtifactMenuBar.cppm`。
- **事実:** View の直下にはズーム、viewport 保存、比較、preview 品質、表示オーバーレイ、Rig 操作、workspace、個別パネル起動、パネル一覧、追加アセットブラウザ、secondary preview が同居する。さらに `Color Science` action は同一の QAction が直下へ二度追加されている。パネル追加／再表示は専用の `Window Panels` submenu とメニューバー右上 `+` にも既に存在する。
- **対応:** 直下を Navigation / Preview / Overlays / Rig / Workspace / Window Panels に限定し、Grid & Snap と Rig 操作を各 submenu に入れた。個別パネル起動と新規 Asset Browser は Window Panels 内の Utility Panels に統合し、重複していた Color Science entry は一つにした。既存 QAction とショートカット、Dock registry の再表示経路は維持した。
- **価値／次の確認:** 直下項目の走査負荷と重複を減らした。ビルド・runtime 確認は未実施のため、メニュー階層、アクセラレータ、パネル作成・再表示、狭幅表示を実機で確認する。


## 2026-09-06 — エフェクト詳細の共有所有と複数展開

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/ArtifactInspectorInteraction.cppm`。
- **事実:** Effects面は1つのPropertyWidgetとSurfaceFX専用編集部を共有している。今回のインライン化でも1件だけを展開し、リスト項目の削除に編集部の寿命を連動させていない。
- **仮説・未検証:** 将来複数エフェクトを同時展開する場合、単純な編集部の複製はfocusedEffectIdや専用編集操作の対象を混線させる可能性がある。
- **価値／次に確認:** 同時展開を追加する前に、編集対象・コールバック・所有権を各エフェクト単位に分離できるか調べる。


## 2026-09-06 — 左ペインのキー操作とUndo経路

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の `togglePropertyKeyframeAtCurrentTime` と今回追加した値編集。
- **事実:** 既存の菱形クリックはプロパティのキーを直接追加・削除する。今回の値編集は既存のUndoコマンド／KeyframeModelを利用するため、同じペイン内でも操作経路が異なる。
- **懸念・未検証:** 菱形クリックのUndo体験が値編集と一致しない可能性がある。今回はリデザイン対象の既存操作を保ち、この経路は変更していない。
- **次に確認:** 菱形のキー追加・削除のUndo/Redoを確認し、別途KeyframeModelの共通経路へ統一する範囲を判断する。

## 2026-09-07 — ツールバーの表示モード操作の実行先

- **関連:** `Artifact/src/Widgets/ArtifactToolBar.cppm` の Normal / Grid / Detail actions。
- **事実:** これらは既存のQActionGroupで選択状態を保持するが、同ファイル内に `viewModeChanged` の発火や表示サービスへの委譲が見当たらない。今回の配置変更では既存QActionをメニューに再利用した。
- **懸念・未検証:** 表示切替が実際のビューに反映されない可能性がある。今回の外観変更とは分けて確認する必要がある。
- **価値／次に確認:** 実アプリで3種の表示操作を確認し、必要なら既存の表示コマンドとの対応を調査する。

### 2026-09-07 — カラーピッカーの色空間契約
- 関連: Artifact/src/Widgets/Dialog/FloatColorPickerHooks.cppm、ArtifactCore/src/Color/LabColor.cppm、XYZColor.cppm。
- 確認事実: Lab/XYZの既存変換はsRGB符号値・D65を前提とし、戻りRGBを0–1にクリップする。ピッカーのFloatColor引数には色空間タグがない。
- 未検証: 全呼び出し元が同じ符号値契約であるかは未確認。
- 懸念・次の確認: 将来のHDRや作業色空間対応時は、呼び出し元の色空間を明示してから変換へ渡す必要がある。今回の追加UIにはsRGB/D65基準を明記した。

### 2026-09-07 — 3D回転の操作数学と表示の区別
- 関連: Artifact/src/Widgets/Render/Artifact3DGizmo.cppm の updateDrag。
- 確認事実: 現行回転は開始角との差をEuler成分へ加算し、スナップは各Euler成分へ適用する。atan2境界の差の連続化はこの経路にはない。
- 未検証: ±180度をまたぐドラッグ、傾いたView回転、非ゼロ開始角でのスナップの操作整合性。
- 懸念・次の確認: 今回は外観変更のため数学を変更していない。上記操作を再現してから、必要なら回転更新とピボット更新の一致を別途修正する。
### 2026-09-07 — MSVC IFC C1001 と initializer-list append
- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `cloth3DDeformationMesh()`。
- **確認事実:** C1001 の報告位置は namespace 終端直後の空行だが、直前の追加処理には import された ClothSolver3D の値を `std::vector::insert(..., { ... })` で追加する箇所があった。
- **仮説・未検証:** 大規模な module implementation unit での initializer-list overload 解決が MSVC の IFC 処理を誘発している可能性がある。`push_back` の明示列へ分解して回避した。
- **価値／次に確認:** 再ビルドで C1001 が消えるか確認し、再発時は Cloth3D 実装を別の既存 `.cppm` 境界へ移す切り分けを行う。
- **2026-09-08 追記・確認事実:** 同じ C1001 が継続し、当該実装unitには未使用の `Physics2D`、`Artifact.Composition.Nodes`、`Artifact.Effect.Generator.Cloner` import が残っていた。利用箇所がないことを静的確認して除去した。
- **次に確認:** この依存グラフ縮小後も再現する場合は、次の候補を当てずっぽうに変えず、物理・component runtimeの大きな実装ブロックを既存moduleの実装unitへ分離する。

### 2026-09-08 — setComposition overload の再入リスク

- **関連:** `Artifact/src/Layer/ArtifactAdjustableLayer.cppm`、`Artifact/src/Layer/ArtifactPaintLayer.cppm`、`Artifact/src/Layer/ArtifactSwitchLayer.cppm`。
- **確認事実:** `QObject*` overload が `void*` overload を呼び、その `void*` overload が `ArtifactAbstractLayer::setComposition(void*)` を呼ぶと、base 実装内の virtual `QObject*` dispatch により派生 overload へ戻る。Adjustment Layer の実行スタックでこの循環を確認した。
- **対応:** Adjustment Layer は base の `QObject*` 実装を明示呼出しするよう修正した。
- **懸念・次に確認:** Paint Layer と Switch Layer に同じ実装パターンが残る。今回の依頼範囲外のため未変更であり、各レイヤー追加・composition attach の実機確認後に同じ修正を適用するか判断する。

### 2026-09-08 — エフェクトのGPU常駐チェーン契約

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Effects/ArtifactAbstractEffect.cppm`、`ArtifactCore::LayerBlendPipeline`。
- **確認事実:** Composition View の通常レイヤー用 raster surface builder は CPU の `ImageF32x4_RGBA` を入出力とする。一方、AUTO/GPU の各エフェクト実装は入力を個別アップロードし、dispatch後に staging texture、`WaitForIdle()`、CPU readbackを行うため、複数エフェクトでは同期往復が段数分発生する。調整レイヤーの対応済みpointwise処理だけは `LayerBlendPipeline` 内でGPU常駐する。
- **対応:** CPU所有のsurface builderではCPU実装を明示利用し、GPU専用エフェクトだけ従来経路へフォールバックすることで同期往復を除去した。さらに通常レイヤーでも、完全に表現できる Exposure / Hue・Saturation / Levels / Brightness / White Balance(tintのみ) / Invert / Grayscale を既存 `LayerBlendPipeline` のF32 SRV/UAV pointwise passへ接続した。
- **価値／次に確認:** 通常レイヤーの対応カラー処理は `layerFloat → pointwise → matte → blend` でGPU常駐する。region、effect mask、mix、未対応パラメータ、CPU明示、GPU専用エフェクトは互換性優先で既存経路を使う。残る根本拡張は、任意のエフェクトAPIへSRV/UAVまたはrender-graph resourceを渡すGPU常駐チェーン契約である。D3D12/Vulkan共通のDiligent境界、ping-pong texture寿命、mask/region/mixの適用順を先に確定する。
## 2026-09-08 — Composition controller の旧画像境界と色順

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `buildRasterizedSurfaceBuffer`、`ArtifactCore/src/Image/ImageF32x4_RGBA.cppm` の `setFromCVMat`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx`。
- **確認事実:** controllerのARGB32画像はCV_32FC4へ数値変換した後、descriptorなしのsetFromCVMatへ渡る。一方、同関数はCV_32FC4をRGBAとして記録する。controllerのコメントはupload側でBGRA変換すると説明しており、現行のdescriptor依存変換との不整合がある。
- **未検証:** 実機で赤青が反転する条件と、もう一つのCompositionViewDrawing経路との差。新しい単色GPU経路では従来controllerの格納順・transfer境界を維持し、この調査を色補正変更に広げていない。
- **価値／次に確認:** 赤・青・半透明の固定入力で両描画経路を比較し、色descriptor修正を別途扱う。GPU常駐化の性能比較と色仕様修正を混ぜない。

## 2026-09-09 — Light Layer 作成時の初期設定境界

- **関連:** `Artifact/src/Widgets/Dialog/CreateLightLayerDialog.cppm`、`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/include/Layer/ArtifactLightLayer.ixx`。
- **確認できた事実:** Light Layer は Point / Spot / Parallel / Ambient / Area、色、強度、距離、Spot cone、Area shape/size、影を既存APIとして持つ一方、2つの作成導線はいずれも `Light 1` の既定値を即時作成していた。
- **対応:** 作成ダイアログで初期値だけを選べるようにし、GOBO / Glow / Light Linking は既存Inspector責務として残した。要約表示はダイアログ入力値から導出するだけで、GPU preview / readback / texture確保を増やさない。
- **価値／懸念:** 作成時に重要な種別差を明示できるが、作成直後の設定適用は既存Camera作成と同じ選択レイヤー取得経路に依存する。
- **次に確認:** 実機でLayerメニュー／Composition Editor双方から各5種を作成し、選択同期、保存・再読込、Spot/Areaの描画、影の有効状態を確認する。

## 2026-09-09 — Quick Layer 作成ダイアログの再配置境界

- **関連:** `Artifact/src/Widgets/Dialog/QuickLayerCreationDialog.cppm`、`docs/design/quick-layer-creation-dialog/README.md`。
- **確認できた事実:** 既存ダイアログは Source、Size、Mask、Envelope、Placement を縦一列に表示していたが、作成オプションと `QSettings` 保存は UI の並び順に依存しない。
- **対応:** Source / Size と Mask / Placement を二列化し、Entry / Exit Envelope を下段に残した。入力値、既存の接続、設定キー、作成オプション、画像選択経路は変更していない。
- **価値／懸念:** 視線移動を減らせる一方、狭い画面では最小幅が従来より広くなる。
- **次に確認:** 実機でPlane/Image切替時の画像入力有効化、各Placement、Mask、Entry/Exitの保存復元と作成結果を確認する。

## 2026-09-09 — ダイアログモック反映時の選択モデル維持

- **関連:** `ArtifactResolutionRemapDialog`、`ArtifactImportAssetsDialog`、`ArtifactObjectPickerDialog`。
- **確認できた事実:** 3ダイアログとも表示構造と選択結果の取得が分離されており、追加イベント配線なしでレイアウトと選択面を整理できる。Resolution Remap の policy は index 順が `RemapPolicy` の列挙値と一致する。
- **対応:** Remap policy を同じ順序の単一選択リストへ置換し、Import と Object Picker は既存モデル／接続を保ったまま視覚階層のみ変更した。
- **価値／懸念:** モックに近い一覧性を得つつ処理境界は維持できる。Remap policy の列挙順変更時はリスト構築と結果変換を同時に確認する必要がある。
- **次に確認:** 実機でキーボード選択、ダブルクリック、Cancel／Skip、各policyのApply結果、狭い画面での最小サイズを確認する。

## 2026-09-09 — Point / Planar Tracker のモード永続化

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm` の `setTrackerType`、`setSettings`、`fromJson`。
- **確認できた事実:** トラッカー種別はトップレベルの `trackerType` と設定内の `settings.type` の二箇所へ保存される。切替 setter が片方だけを更新すると、保存・再読込時に Point / Planar が食い違う余地があった。
- **対応:** setter 同士で両フィールドを同期し、旧形式で `settings.type` が欠落した JSON はトップレベル種別を既定値として復元するようにした。
- **価値／懸念:** UI のモード切替とプロジェクト再読込の整合性を保てる。既存ファイルの不正な種別値は従来どおり setter の範囲 clamp に委ねる。
- **次に確認:** 実機で Point / Planar 切替後に保存・再読込し、ツールバー選択状態、検索領域、結果フレームが同じモードで復元されるかを確認する。

## 2026-09-09 — Tracker キャプチャ失敗の受け入れ境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `trackerCaptureNextFrame`。
- **確認できた事実:** オフスクリーンレンダラーが `QImage` を返せない場合でも、従来はフレーム数だけを進めて不完全な画像列を解く可能性があった。
- **対応:** null 画像を検出した時点でキャプチャとジョブを停止し、ユーザーへ失敗フレームを表示する。部分的なトラッキング結果を完了扱いにしない。
- **懸念／次に確認:** 実機でGPU初期化失敗・対象レイヤー範囲外・画像シーケンス欠落の各ケースを確認し、必要なら再試行導線を追加する。

## 2026-09-09 — Tracker point ID の固定値依存

- **関連:** `MotionTracker::addTrackPoint`、`ArtifactPointTrackerGizmo`、`ArtifactPointTrackerTool`。
- **確認できた事実:** `MotionTracker` の点ID採番は1始まりだが、軌跡表示・補正・単一点書き出しがID `0` を固定参照していたため、最初の点が表示／適用されない経路があった。
- **対応:** Coreに最初の登録点の実ID取得APIを追加し、GizmoとApply経路がそのIDを使うようにした。Toolの既定値も「最初の登録点を解決」に変更した。
- **価値／次に確認:** 既存の1始まりIDとJSON復元を壊さず、Point Trackerの軌跡・補正・Bakeが同じ点を参照できる。複数点の明示選択UIは別途検討する。
- **追記:** JSON復元時に次の点／領域IDも最大既存IDの後ろへ再同期し、追加登録時のID衝突を避けるようにした。

## 2026-09-09 — Motion path と結果フレームの対応

- **関連:** `ArtifactPointTrackerGizmo` の軌跡描画・PathPoint補正。
- **確認できた事実:** `motionPath(pointId)` は点が存在するフレームだけを返すため、path index と `result.frames` index は常に一致するとは限らない。
- **対応:** 点IDの存在を基準に結果フレームを解決してから信頼度表示・現在点表示・補正時刻を決めるようにした。
- **価値／次に確認:** 欠落点を含む結果でも別フレームへ補正を書き込まない。欠落フレームの補間表示は既存 `TrackResult::interpolateAt` の責務として残す。

## 2026-09-09 — Tracker入力のレイヤー境界

- **関連:** `ArtifactCompositionRenderController::trackerCaptureNextFrame`、`OffscreenCompositionRenderer`。
- **確認できた事実:** 追跡キャプチャがコンポジション全体を入力にしていたため、選択画像レイヤー以外の模様が特徴点候補へ混ざる可能性があった。
- **対応:** 指定レイヤーだけを透明背景へ描画して読み戻す `renderLayerToQImage` を追加し、Point／Planar Trackerのキャプチャを選択画像レイヤーに限定した。
- **懸念／次に確認:** レイヤー変換・親子階層・マスクを含む画像レイヤーで、VP座標と読み戻し画像の座標が一致するか実機で確認する。
- **追記:** レイヤー専用読み戻し後は元の `currentFrame()` へ戻し、キャプチャだけで編集対象レイヤーの表示時刻を変更しないようにした。

## 2026-09-09 — TrackPoint のVP操作面

- **関連:** `ArtifactCompositionEditor` の上部ツールバーと `compositionTrackerPanel`。
- **確認できた事実:** 追跡コマンド自体はツールバー／コンテキストメニューに接続済みだったが、モックアップの右側Tracker操作面は未実装だった。
- **対応:** VP内にTrackerパネルを追加し、Point設定、Planar切替、前後／全範囲解析、停止、問題フレーム確認、Position／Anchor／全ポイント／Corner Pin適用を既存controllerへ接続した。信頼度・問題数・結果フレーム数はcontrollerの読み取りAPIで表示する。
- **懸念／次に確認:** パネルはVP上に重ねる方式のため、Four-Up時の占有範囲と狭い画面での折り返しを実機確認する。QtCSSや新規signal/slotは追加していない。
- **追記:** ネイティブswap-chain面による子Widgetの遮蔽を避けるため、パネルをviewportHostの子から外側の横レイアウトへ移し、表示時はVP幅を確保して並べる構造に変更した。

## 2026-09-10 — History Timeline の部分復元には command payload 契約が必要

- **関連:** `ArtifactHistoryTimelineWidget` / `UndoManager` / Source Patch History
- **確認できた事実:** 現在の `UndoManager` は履歴ラベル、Undo/Redo、シリアライズ可能なコマンドを扱えるが、任意の履歴点から「Blur 設定だけ」のようなプロパティ単位payloadを共通形式で列挙する公開 API はない。
- **気づき:** History Timeline の安全な部分復元は、UI側でコマンド型を推測するのではなく、コマンドが復元可能payloadの種類・対象ID・preview値を明示する契約を持つと Project History と Source Patch History の双方で再利用できる。
- **価値／懸念:** 共通契約があれば部分復元ボタンを実データにのみ有効化できる。契約なしで実装すると、型別分岐がUIへ漏れ、誤った対象への適用や復元不能状態を招く。
- **次に確認すること:** `UndoCommand` の serialization schema と AI patch metadata を横断し、read-only の `restorablePayloads()` 相当を追加できるか設計レビューする。現段階では未対応コマンドに対する部分復元を無効表示に留める。

## 2026-09-11 — Glyph atlas の差分アップロード境界

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`ArtifactCore/include/Text/GlyphAtlas.ixx`。
- **確認できた事実:** `GlyphAtlas` は固定 2048×2048 の CPU atlas と dirty bool を持つ。従来の GPU 側は新規 glyph の追加ごとに immutable texture を破棄・再作成していた。
- **対応:** Artifact 側の command-buffer と immediate-submitter の両経路を updateable texture の再利用へ移し、既存 texture へ upload するようにした。glyph 提出用の一時配列も renderer lifetime の scratch buffer として初期化時に確保し、通常のテキスト編集では再確保しない。
- **対応（追記）:** `GlyphAtlasDirtyRegion` を公開し、追加 glyph の矩形を union、clear と初回を全量更新として表す契約を追加した。
- **対応（追記）:** `PrimitiveRenderer2D` と `DiligentImmediateSubmitter` は、Diligent の `UpdateTexture` に image stride を保った矩形 source pointer と destination box を渡す。新規 GPU texture は dirty state にかかわらず全量初期化する。
- **価値／懸念:** texture object の再生成を避けたまま、通常の新規 glyph 追加では更新領域だけを転送できる。全量／矩形の実転送量と backend parity は未検証である。
- **次に確認すること:** atlas reset 時は全量、それ以外は矩形 upload になること、CJK／emoji glyph と複数 glyph の同フレーム追加で union 範囲が正しいことを実機プロファイルで測る。

## 2026-09-11 — 基本テキストの shaping 再利用境界

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- **確認できた事実:** fill-only の通常テキストは layout cache に `GlyphItem` を持つ一方、従来は `drawTextTransformed()` が immediate submit 時に同じ文字列を再 shaping していた。
- **対応:** plain text の fill / stroke / shadow を cached glyph direct-draw へ切り替えた。stroke の 8 方向 outline と shadow offset は既存 immediate path と同じ値を glyph quad へ渡す。underline / strikethrough は既存 immediate glyph submit でも独立線として描かれていないため、今回の経路切替で表示を増減させない。
- **価値／懸念:** 静的な通常テキストでは layout 成果を再利用できる。実機で alignment・CJK fallback・stroke / shadow・cloner transform・長文の frame cost を比較するまで parity / 性能は未検証。

## 2026-09-11 — glyph 単位 font fallback の再利用

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **確認できた事実:** cached glyph direct-draw は `GlyphAtlas` が hit しても、各 glyph ごとに `FontManager::makeFont()` を呼び、fallback family を解決していた。
- **対応:** `TextStyle` が同一の間は code point ごとの解決済み `QFont` を renderer lifetime cache に保持する。キャッシュは最大 2,048 glyph で clear し、font style 変更時にも clear する。
- **価値／懸念:** CJK fallback の glyph 単位意味論を維持したまま、静的テキストの font database 問い合わせを避ける。font install/uninstall 中の動的更新は未検証。

## 2026-09-11 — transformed glyph key の再利用

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **確認できた事実:** transformed glyph の提出は、font fallback cache が hit しても glyph ごとに `GlyphKey` と `fontFamily` の UTF-8 文字列を再構築していた。
- **対応:** style と code point ごとの fallback cache に `GlyphKey` も保持し、atlas acquire にその値を渡すようにした。
- **価値／懸念:** static text の CPU submit で繰り返される文字列確保を避ける。atlas clear 時の glyph rect は保持せず、従来どおり atlas から都度取得する。
- **次に確認すること:** CJK fallback、emoji、style切替と atlas clear 後に正しい key で再取得されることを実機で確認し、長文の CPU submission cost を測定する。

## 2026-09-11 — animator glyph 提出の一時表撤去

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` の `drawGlyphs()`。
- **確認できた事実:** animator などが使う pre-laid-out glyph 経路は、呼び出しごとに unique glyph table を確保し、同じ fallback font と atlas key を作っていた。
- **対応:** renderer lifetime の style / code point cache を使い、atlas acquire の二段階処理は維持したまま局所 vector を撤去した。
- **価値／懸念:** 動的 text でも glyph 提出に伴う局所ヒープ確保を避ける。cache は2,048 entryで clear するため、それを超える多言語文書の warm-up は未検証。
- **次に確認すること:** animator 有効なCJK・emoji長文で、glyph color override、opacity、atlas reset 後の表示と frame cost を確認する。

## 2026-09-11 — 未接続の ArtifactTextGlyphSubmitter

- **関連:** `Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm`。
- **確認できた事実:** この submitter は atlas を `clear()` して immutable texture、vertex buffer、constant buffer を submit ごとに作る。一方、現行の `ArtifactTextLayer` GPU 経路は `PrimitiveRenderer2D` と `DiligentImmediateSubmitter` を使い、検索上この submitter の呼び出し元は確認できなかった。
- **判断:** 稼働中の GPU text 経路を重複実装へ切り替えず、部分 upload を既存二経路へ適用した。
- **価値／懸念:** 未接続コードを性能根拠にして現行経路を誤って置換しない。将来この contract を有効化する場合は、resource reuse と呼び出し ownership を先に設計する必要がある。
- **次に確認すること:** module registration と将来の consumer を監査し、不要なら削除、必要なら既存 atlas uploader へ統合する判断を別作業として行う。

## 2026-09-11 — rich GPU run の callback copy

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`。
- **確認できた事実:** rich text の GPU run は `drawWithClonerEffect` へ値 capture され、同期 callback を受ける `std::function` の構築時に glyph 配列全体をコピーしていた。
- **対応:** run を参照 capture に変更した。clone helper は callback をその呼び出し内で直ちに実行し、保持しない。
- **価値／懸念:** rich text の clone pass ごとに発生していた run copy を除去する。QTextDocument の再構築・run 分解は静的 rich GPU run cache の対象として別途整理した。

## 2026-09-11 — 静的 rich GPU run の再利用境界

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- **確認できた事実:** rich text の GPU 経路は、内容・書式が変わらないフレームでも `QTextDocument` の構築、block / fragment / line の分解、各 run の shaping を実行していた。
- **対応:** animator 未使用時だけ、既存のテキスト cache key と同じ入力で GPU run を保持して再利用する。画像 object を含む rich text と animator 有効時は cache を使わず、従来どおり毎フレーム構築する。
- **価値／懸念:** 静的な rich text の CPU 側レイアウト作業を避けられる。一方、run の実フレーム時間と画像 object を含む文書の fallback parity は未検証。
- **次に確認すること:** 実機でHTML書式、複数行・box alignment、CJK fallback、underline / strikethrough、animator 有効／無効切替を確認し、長文の frame cost を計測する。

## 2026-09-11 — Shape F12 主ビューポートの頂点マーキー選択

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** Shape のカスタムパス／ポリゴンにはクリック選択と単一頂点ドラッグがある一方、主ビューポートで頂点を矩形選択し、複数頂点を同じデルタで移動する導線がなかった。
- **対応:** 空キャンバスからの通常ドラッグ、または選択Shape上でのShift/Ctrlドラッグを頂点マーキーとして追加し、Replace/Add/Toggleを既存の選択文法へ接続した。カスタムパスとポリゴンの両方で選択でき、選択済み複数頂点の移動は既存の単一Undoトランザクション内で相対移動する。Clonerと重複するRepeater操作や新規ショートカットは追加していない。
- **価値／懸念:** レイヤー内部の通常クリック移動、Altオービット、既存ギズモ／Pen操作を優先順位ごと維持したまま、Shape編集の基本選択文法を補完した。ハンドルのみ、比例編集、分割、ミラーなどF12残項目は未実装。
- **次に確認すること:** ビルド・`check_module_hygiene`・実機runtimeは未実施（AGENTS.md制約でユーザー許可待ち）。変形済みShape、Replace/Add/Toggle、空キャンバス上の既存レイヤーマーキー、Undo／再読込時の選択状態を確認する。

## 2026-09-11 — 911開発ブランチ統合後の3D開発順序

- **関連:** `Artifact/docs/MILESTONE_3D_GIZMO_IMPLEMENTATION_2026-03-25.md`、`Artifact/include/Layer/Artifact3DModelLayer.ixx`、`ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/include/Geometry/MeshImporter.ixx`。
- **確認できた事実:** `origin/codex/2026-09-11-dev` は3Dレイヤー、クローン、カメラ、メッシュ描画・マテリアルの基盤を親子リポジトリへ追加した。一方、3Dギズモ文書は実機操作確認を保留し、メッシュ／レンダー側には読み込み・GPU資源・環境／材質の統合入口が増えた段階である。
- **気づき:** 次の高価値な実装単位は、3D機能をさらに広げる前に「モデル読み込み→シーン配置→カメラ操作→保存／再読込→GPU描画」の最小縦切りを受入れ可能にすること。これにより未検証の3D基盤を制作フローへ接続できる。
- **価値／懸念:** 911の変更効果を実際のユーザー操作で確認でき、後続のライト、マテリアル、クローン拡張の回帰範囲を小さくできる。ビルド／実機検証は未実施であり、低レベルDiligent変更を広げる前に既存経路の責務確認が必要。
- **次に確認すること:** 3Dレイヤー生成ダイアログ、プロパティ編集面、プロジェクト保存形式、`ArtifactCompositionRenderController` の3D入力・描画呼び出しをつなぎ、最小のruntime受入れ項目を定義する。

## 2026-09-11 — サイドカーUUIDを再リンク候補の一次キーにする

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`。
- **確認できた事実:** ファイル名・拡張子・サイズだけでは、移動や改名後の3Dモデルを確実に特定できない。Project Item、Asset Database、旧パス／候補パスのサイドカーから論理UUIDを取得できる。
- **対応:** 一致する論理UUIDを再リンク候補の最上位スコアにし、複数選択時のID一覧コピーも追加した。
- **価値／懸念:** 命名規則に依存せずアセットを復旧できる。一方、サイドカーが欠落・複製された場合は従来のファイル特徴量へフォールバックするため、同一UUIDの重複検出は別途必要。
- **次に確認すること:** 実ファイルを移動・改名し、サイドカーを保持した状態で候補順位と再リンク後のID維持をruntimeで確認する。

## 2026-09-11 — Nuke型ワークフローのグラフ正本境界

- **関連:** `Artifact/src/Engine/DAG/CompositionGraphBuilder.cppm`、`Artifact/include/Engine/DAG/CompositionGraph.ixx`、`Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`、`Artifact/include/Color/ArtifactColorNodeGraph.ixx`。
- **確認できた事実:** 現行のComposition graph builderはレイヤー順・親子・各レイヤーのeffect列から評価用DAGを都度生成する。Composition Graph WidgetはComposition／Layer／Effectの可視化を目的とし、ColorNodeGraphは独立した色補正DAGである。
- **気づき（未検証）:** Nuke型の任意接続を追加する際、表示用DAGを直接編集可能にすると、タイムラインのレイヤー順・effect stack・保存形式との正本が二重化する。まずは「レイヤー合成の正本を維持した read-only graph + viewer／channel／AOV検査」を制作導線にし、任意ノード接続は明示的なNode Comp資産として別の正本・入出力契約・Undo／保存を設計してから導入するのが安全である。
- **価値／懸念:** 既存のレイヤー制作フローを壊さず、Nukeの強みである中間結果の観察とAOV利用を早く提供できる。グラフ編集を先行すると、レンダー順・キャッシュ無効化・親子関係・project round-tripの不整合が起きやすい。
- **次に確認すること:** `CompositionGraphBuilder`のノードID安定性、render controllerの中間surface／AOV寿命、project JSONにNode Compを独立assetとして保存できるかを、実装前の設計レビューで確認する。

## 2026-09-11 — Cryptomatte draft の識別子正規化

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/include/Image/Cryptomatte/CryptoPixel.ixx`。
- **確認できた事実:** AOV passはlayer IDとmaterial keyから24bit値を描画する一方、Render Queueのmanifestは別実装のhashを保持していた。3D layerでは描画時に`materialSignature()`を使うが、manifestはlayer名由来だった。
- **対応:** 描画とmanifestの両方を`CryptoSample::nameToId()`へ統一し、3D material entryは描画時と同じ`materialSignature()`を使うようにした。
- **価値／懸念:** draft EXRのID値とname manifestが一致する。これはrank付きcoverage、Cryptomatte標準のfloat payload変換、pick-to-matte UIを実装する前提であり、それらの未実装を完了扱いにはしない。
- **次に確認すること:** 3D／2D混在sceneをEXR出力して外部Cryptomatte consumerでmanifestとIDが一致すること、半透明edgeの複数coverageを保持するGPU passとpick UIの契約を検証する。

## 2026-09-12 — Flat AOVからDeepへの明示変換境界

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/include/Image/DeepImageBuffer.ixx`。
- **確認できた事実:** rendererはBeauty RGBAとDepthを`MultiChannelImage`へ一度readbackでき、CoreはRGBA+depthから`DeepImageBuffer`を構築する明示APIを持つ。
- **対応:** Render Queueでreadback済みのRGBAとDepthから一サンプル／pixelのDeep bufferを構築し、同じAOVの二重GPU readbackを避けた。
- **価値／懸念:** Deep merge、holdout、Deep EXRの既存基盤へ通常render結果を接続できる。これは複数visibility sampleを持つnative Deep rendererではなく、書き出し時の冷経路で一時バッファを確保する。
- **次に確認すること:** flat-to-deepを示すmetadata、Depthの空間・単位契約、native Deep sample passとflat bridgeの選択基準を設計レビューする。

## 2026-09-12 — Chroma Key GPUのフレーム資源再利用

- **関連:** `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`。
- **確認できた事実:** GPU Chroma Keyはパイプライン、定数バッファ、入力／出力／readback staging textureを再利用する。入力は同一サイズなら`UpdateTexture`で更新し、device/context変更時は全GPU資源を破棄して再構築する。
- **価値／懸念:** 解像度が安定した連続プレビューではtexture生成を避けられる。一方、GPU readbackは同期`WaitForIdle()`を維持しており、effect-chain全体のCPU待機は残る。
- **次に確認すること:** effect frame samplerと共有resourceの寿命・resize契約を調査し、D3D12/Vulkan双方でallocationとGPU待機を計測してから、非同期readbackまたはchain内GPU保持を設計する。

## 2026-09-12 — Project Viewタイルの混合グリッド境界

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`docs/design/project_view_redesign_2026-09-08/project-view-tile-workbench.png`。
- **確認できた事実:** 現行Tile表示は全visible rowを同じ`tileRectForRow()`格子へ配置し、描画、ヒットテスト、スクロール範囲がその単一寸法を共有する。09-08タイルモックはフォルダーを低い上段カード、アセットをサムネイル付き下段カードとして表現している。
- **気づき:** モックへ正しく寄せるには、描画だけフォルダーを小さくせず、項目種別を考慮した共通layout結果を描画・選択・ドラッグ・スクロールで再利用する必要がある。
- **価値／懸念:** 構造を上段で読み、素材を下段で見渡せる一方、paintだけの局所変更ではクリック対象と表示位置がずれる。
- **次に確認すること:** filtered visible rowsからboundedな配置情報を更新時に構築し、同じ配置をpaint、hit test、scroll extent、drop indicatorへ渡せるかを確認する。

## 2026-09-12 — Voronoi/Bricks GPU Spatial化（HexGrid/Stripes型）

- **関連:** Artifact/include/Effects/ArtifactAbstractEffect.ixx（Kind追加）、Artifact/include/Effects/Rasterizer/VoronoiEffect.ixx、BricksEffect.ixx、Artifact/src/Render/ArtifactRenderLayerPipeline.cppm（kVoronoiShader/kBricksShader＋executor分岐）。
- **確認できた事実:** Spatial GPUパスは全effectがGPU対応・mix=1・mask/regionなしのときだけ選択され（uildGpuRasterEffectPlan）、失敗時はレイヤー拒否でfail-closed（effect落としなし）。CPU実装は参照用に残る。GpuSpatialEffectKind末尾追加なので既存値の採番は不変。
- **価値／懸念:** Voronoi HLSLはCPUのhash11/hash12・3x3探索を鏡写し、Bricksは行オフセット＋fmod判定を鏡写し。CPU/GPU画素一致は未検証。Kaleidoscope/Halftone（入力サンプリング型）とGlow（2パス）はKindのみ先行追加し実装は残課題。
- **次に確認すること:** ビルド許可後にVoronoi/BricksのCPU/GPU画素差分を計測し、toleranceを決めてからKaleidoscope→Halftone→Glowの順で spatial 化する。

## 2026-09-12 — 正規KaleidoscopeのGPU常駐化（Kind配線）

- **関連:** Artifact/include/Effects/Kaleidoscope/KaleidoscopeEffect.ixx、同 src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm:197 kKaleidoscopeHlsl、Artifact/src/Render/ArtifactRenderLayerPipeline.cppm。
- **確認できた事実:** 表のkaleidoscope（Service登録あり）は旧式単体GPU済みだが常駐未対応だった。常駐HLSLは旧式 kKaleidoscopeHlsl をbilinearヘルパごと流用しentryのみ KaleidoscopeCS 化。7パラメータは parameters[8] に収まりpixel単位なしのためmask=0。旧式applyGPUと常駐は共存する。RasterizerKaleidoscopeEffect はimportゼロ・Service登録なしのデッドコードのため対象外。
- **価値／懸念:** 常駐化で他GPU effectとの連鎖時にper-effectのupload/readback/WaitForIdleが消える。一方atan2/cos/sinのCPU/GPU精度差がセグメント境界で1px級差分を出す可能性あり。
- **次に確認すること:** ビルド許可後にCPU/旧GPU/常駐GPUの3者画素差分を計測し、toleranceを決めてからHalftone→Glowへ進む。

## 2026-09-12 — 汎用GPU常駐の設計メモ＋Halftoneパイロット

- **関連:** docs/analysis/GENERIC_RESIDENT_GPU_DESIGN_2026-09-12.md、Artifact/include/Effects/ArtifactAbstractEffect.ixx（Kind::Generic・共有レジストリ）、Artifact/src/Render/ArtifactRenderLayerPipeline.cppm（汎用PSOキャッシュ＋分岐）、ArtifactHalftoneEffect。
- **確認できた事実:** 汎用GPU実行基盤は既存（
unCreativeCompute＋labelキーキャッシュ、ArtifactCreativeEffects.cppm:3725）だが常駐でない・パラメータなし。新設でなく拡張する方針にした。登録済みHalftone/Glitch/OldTVは既にcreative GPU済み。Rasterizer::HalftoneEffect はimportゼロ・Service登録なしのデッドコード。旧式単体GPU・creative・常駐の3経路が共存する。
- **価値／懸念:** 新規effectはRenderPipeline無改修で常駐化可能（HLSL＋登録のみ）。g_Time/g_Frame は0埋めのTODO。旧式・creative経路の一本化はPhase 2に先送り。
- **次に確認すること:** ビルド許可後にCPU/creative/常駐の3者画素差分とフォールバック動作を確認し、受入基準（設計メモ§6）を満たしたらGlitch/OldTVへ横展開する。

## 2026-09-12 — GlowのGeneric常駐化（params受け渡し実証）

- **関連:** Artifact/include/Effects/Glow/GlowEffect.ixx、src/Effects/Glow/GlowEffect.cppm（kGlowResidentHlsl）。
- **確認できた事実:** 登録済み glow は旧式GPU済みの多層ブルーム。常駐bodyは kGlowHlsl のcbuffer除去＋ g_P0..g_P6 置換で再現し、旧b0とのregister衝突を回避。baseSigmaのみpixel単位のためmaskはbit2だけ。旧b0付きHLSLをそのまま登録するとpreludeと衝突する制約を設計メモへ反映要。
- **次に確認すること:** ビルド許可後にCPU/旧GPU/常駐の3者画素差分を計測する。

## 2026-09-12 — LiquidGlowの新規HLSL常駐化（radiusゲート先例）

- **関連:** Artifact/include/Effects/Glow/LiquidGlowEffect.ixx、src/Effects/Glow/LiquidGlowEffect.cppm（kLiquidGlowResidentHlsl）。
- **確認できた事実:** 旧GPUなしの初GPU化。CPUのthreshold→separable Gaussian→flow remap→加算を単一ノードで再現。フル分離形のbilinearは計算量が爆発するため、まだらサンプルではなく水平ブラー＋垂直ガウス＋x方向bilinearの厳密分離形にした。luma重み（R*0.114の変則）もCPU鏡写し。
- **価値／懸念:** コスト超過パラメータは ppendGpuSpatialNodes で alse を返してCPUに残す先例を作った（radius>8）。見た目を変えずに適用範囲だけ絞るfail-closedの応用。remap境界はclamp/reflect101差あり。
- **次に確認すること:** ビルド許可後にtolerance計測。ResidualGlow等も同型で続ける。

## 2026-09-12 — 用途別ファイルピッカーの共通ブラウザ基盤

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`Artifact/src/Widgets/Dialog/ArtifactImportAssetsDialog.cppm`、`Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **確認できた事実:** Asset Browser はファイル探索、サムネイル、Favorites、Recent を持つ一方、Project Open、Import、Relink、LUT／OCIO、export destination などは複数箇所から `QFileDialog` を直接呼ぶ。`ArtifactImportAssetsDialog` は既に選ばれたパスを確認する段階を担当し、選択前のブラウザではない。
- **気づき:** OSファイルダイアログを全面置換する単一巨大Widgetではなく、Asset Browserの探索／履歴／サムネイル基盤を再利用し、Open Project、Import Media、Relink、Reference/LUT、Export Destinationを用途別モードとして構成する方が責務を保ちやすい。ネットワーク共有やOS固有場所のため、native pickerへの退避導線は残す。
- **価値／懸念:** DCC固有のsequence検出、color space、proxy、missing media候補、output token／上書き衝突を選択前に示せる。一方、import／relink／saveを一つのモデルに押し込むと状態と検証規則が肥大化する。
- **次に確認すること:** Asset Browserのdirectory model、thumbnail cache、recent/favorite保存APIを非Dockのdialog shellから安全に再利用できるかを確認し、まずImport Media Pickerを最小縦切り候補にする。
- **対応:** `ArtifactMediaImportPickerDialog` を既存Import dialog module内に追加し、FileメニューとImport requestの選択前段を統一した。選択・検索・種別filter・連番展開のみを行い、Project mutationとlogical Asset ID登録は既存の確認／非同期Import経路へ残した。新規signal/slot、QImage decode、thumbnail再生成は追加していない。

## 2026-09-12 — LUTライブラリ選択をColor Science Managerの前段へ分離

- **関連:** `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`、`ArtifactColorScienceManager`、`docs/design/lut-color-reference-picker/`。
- **確認できた事実:** Color Science Managerはbuilt-in／外部LUTの列挙とload、現在LUTのformat／size／errorを持つ一方、preview-only routing、domain metadata、favorite／recent保存APIを公開していない。
- **対応:** `ArtifactLutColorReferencePickerDialog` は一覧・検索・種別別閲覧・互換性確認だけを受け持ち、accept後に既存managerの`loadBuiltinLUT`／`loadLUT`へ委譲するようにした。存在しないstateをUI上で操作可能に見せず、working-preview checkboxはdisabledとした。新規signal/slot、QImage、QPainter描画、Core変更なし。
- **価値／懸念:** 既存Color Science Panelの単一list／file dialogをDCC型library chooserへ置き換えつつ、LUT適用の責務を複製しない。同一reference thumbnailとbefore/after splitはGPU preview境界を設計してから追加する必要がある。
- **次に確認すること:** ビルド許可後、built-in／外部／破損LUTの選択、検索とcategory切替、accept後のmanager更新、panel preview更新を実機確認する。

## 2026-09-12 — PhysicalHalation coreの2層拡散が互いを消す（要判断）

- **関連:** ArtifactCore/include/ImageProcessing/Halation.ixx:51-54、Artifact/src/Effects/Glow/PhysicalHalationEffect.cppm。
- **確認できた事実:** process() は同一バッファに diffuse(..., spread*redDiffusion, 1,0,0) → diffuse(..., spread, 0,0.1,0.05) を順に掛ける。1回目でG/Bが0になり、2回目でRが0になるため、最終highlightsは既定値で (0,0,0) に潰れる。composite加算も0。意図（層別バッファ）と実装（同一バッファ破壊）が矛盾。softness設定も process() 未参照。
- **価値／懸念:** 鏡写し移植は無意味（no-opの再現になる）。core修正はCPU挙動の可視変化を伴うため独断不可。
- **次に確認すること:** ユーザー判断（core修正してからGPU化／現状維持）。常駐化は見送り。

## 2026-09-12 — Open Project の事前表示は未検証状態を明示する

- **関連:** `Artifact/include/Widgets/ArtifactImportAssetsDialog.ixx`、`Artifact/src/Widgets/Dialog/ArtifactImportAssetsDialog.cppm`、`Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`。
- **確認できた事実:** 既存のOpen Project導線はfile pathを得て既存の非同期loadへ渡すだけであり、migration、project health、missing sourceの事前検証結果を提供していない。
- **対応:** `ArtifactProjectOpenPickerDialog` はrecent projectの探索・選択とSystem Picker fallbackだけを担当し、詳細欄では状態を`Not checked until opening`と表示する。load／migration／検証／recent更新の責務は既存Project Serviceに残した。
- **価値／懸念:** mockのhealth表現を根拠のない正常表示にせず、open前のUIで保証できる情報だけを示せる。tile preview、favorite、実データに基づくhealthは将来のProjectメタデータ境界が必要。
- **次に確認すること:** ビルド許可後、recentなし／recentあり／missing recent／System Picker／load失敗時の既存エラー経路を実機確認する。

## 2026-09-12 — 出力先の衝突回避は選択時に確定pathへ反映する

- **関連:** `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm`、`docs/design/export-destination-picker/`。
- **確認できた事実:** Render Output Settingsはformat／codec／frame rangeを保持し、BrowseはOSのsave dialogだけを開いていた。出力衝突を避けるversion policyのUIはなかった。
- **対応:** Browseをdestination pickerに変更し、folder、base name、version、frame token、extension、最終pathを確認できるようにした。候補が既存なら、返却する最終path自体を次の空きversionへ移すため、表示だけが安全で実際は上書きする不整合を防ぐ。
- **価値／懸念:** Render／queue mutationやformat決定を複製せず、出力命名だけに責務を限定する。複数ジョブのtoken規則、容量見積り、空き容量表示はRender Queueの実データ境界を定義してから追加する。
- **次に確認すること:** ビルド許可後、既存versionの連番、image-sequence token、format切替後のextension更新、System Picker fallbackを実機確認する。

## 2026-09-12 — Relink候補は理由を読んでから採用する

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`、`docs/design/asset-relink-picker/`。
- **確認できた事実:** 既存の候補検索はlogical Asset ID、filename、extension、sequence pattern、asset typeなどをscoreとreasonに保持しているが、Asset Browserは`QInputDialog`の文字列選択で表示していた。
- **対応:** Candidate Pickerにscore、full path、Match Reasons、sequence frame一致数を表示し、採用後は既存の`relinkFootageByPath`とUndo commandへ渡す。filename単独で「Best match」と表示する処理は追加していない。
- **価値／懸念:** 採用判断を観測可能にしつつ、logical Asset IDとcache／Undoの既存責務を変えない。Project Viewのbulk relinkも同じ候補UIへ統一するには、対象セットとmappingを渡す別のbatch contractが必要。
- **次に確認すること:** ビルド許可後、同名別content、logical Asset ID一致、連番の部分一致／完全一致、Undo失敗時のrollbackを実機確認する。

## 2026-09-13 — GPU常駐Blurの実surface契約が設計記述とずれている

- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx:89-104`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:181-220`、`Artifact/include/Effects/GauusianBlur.ixx`。
- **確認できた事実:** `RenderConfig::PipelineColor` は canonical linear-premultiplied と宣言する一方、`layerToFloatShaderText` は入力を unpremultiply して `float4(straight, alpha)` を `layerFloat` へ書き込む。既存の常駐Gaussian shader はその実体に合わせて RGB×alpha の平均後に unpremultiply しており、CPU Gaussian fallback も同じ前提である。
- **価値／懸念:** 通常 `BlurEffect` を常駐Gaussianへ安易に接続すると、sRGB/premultiplied を明示変換する旧GPU実装との出力差が透明境界で発生し得る。descriptorだけを根拠に常駐shaderを premultiplied平均との差し替えると、現行 `layerToFloat` の実体と逆行する。
- **対応:** ユーザーの明示許可を得て、`layerToFloat` の unpremultiply を撤去し、Core blend shader と resident Gaussian を premultiplied 契約へ切り替えた。通常 `BlurEffect` は premultiplied・mix 100%・CPU half-resolution未使用（sigma < 3）に限り、Gaussian反復は最大8 pass、Edge Preservingは二段Gaussianとして最大4反復まで常駐化した。
- **次に確認すること:** ビルド許可後に `GaussianBlur`／通常Blur／blend/matte のCPU・旧GPU・常駐GPU fixtureを比較し、encoded-sRGB ingressのlinearizationが一度だけ行われることも確認する。

## 2026-09-13 — Lens Distortionを常駐Spatialへ移す際のstage境界

- **関連:** `Artifact/include/Effects/LensDistortion/LensDistortionEffect.ixx`、`Artifact/src/Effects/LensDistortion/LensDistortionEffect.cppm`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`。
- **確認できた事実:** Lens Distortionの旧GPU実装は単一フレームの画像warpだが、毎回のtexture生成→staging copy→`Flush`／`WaitForIdle`→CPU readbackを含む。CPU式とGPU式は同じ radial/tangential/zoom/invert/edge-fill と bilinear sampling を持つ。
- **対応:** 専用 `GpuSpatialEffectKind::LensDistortion` と16-slot固定ノードを追加し、常駐CSでlinear-premultiplied RGBAを直接bilinear処理する。画像を変形するラスター処理として `EffectPipelineStage::Rasterizer` に変更し、composition GPU planから呼べるようにした。
- **価値または懸念:** effect chain内のCPU readbackとGPU waitを除去できる。一方、stage変更はGeometryTransformとして保存された既存stackの順序意味に影響し得るため、runtimeでLensとTwist/Bendの混在順序を確認する必要がある。旧GPU経路は互換fallbackとして残置。
- **次に確認すること:** D3D12/VulkanのPSOコンパイル、CPU/旧GPU/常駐GPUの画素差分、透明端とcenter/zoom/invert境界、stage順序、sRGB ingressのlinearize回数をfixtureで確認する。

## 2026-09-13 — Chroma Keyのstraight/premultiplied境界をCoreで統一

- **関連:** `ArtifactCore/src/ImageProcessing/ChromaKey.cppm`、`Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`。
- **確認できた事実:** 旧Chroma KeyはRGBをそのままYCbCrへ送り、matte finishing後もRGBをstraightのまま残していたため、canonical `layerFloat`（linear-premultiplied）と契約がずれていた。旧GPU経路は同じ式をstaging readback付きで実行していた。
- **対応:** Core／旧GPU／常駐CSともに、入力をalphaで明示unpremultiply→key/despill→最終matteでpremultiplyする順序へ統一した。常駐はYCbCr、clip、despill、3種のdiagnostic viewを12パラメータで実行し、choke/matte blurは追加alpha面を表現できるまで旧経路へfail-closedする。
- **価値または懸念:** 半透明のgreen-screen境界で色漏れを抑え、通常keyer連鎖のCPU実行・readback・GPU waitを除去できる。直接Core APIへstraight RGBを渡していた外部呼出しは意図的にcanonicalへ移行したため、互換性の実測が必要。
- **次に確認すること:** D3D12/Vulkan PSO、CPU/旧GPU/常駐GPU parity、view mode、clip／despill、半透明入力、choke／blur fallbackをfixtureで検証する。

## 2026-09-13 — ArtifactAbstractLayerのMSVC C1001はEOFではなくIFC依存境界を疑う

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm:12481`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`、直近の`TimeRemap`／`PosterizeTime`追加。
- **確認できた事実:** 報告された12481行は名前空間の閉じ括弧直後で、構文エラーになる実体がない。MSVC 14.51の診断にも`IFC インポートが検出`と出ている。`QVector`は公開宣言と実装で使用していたが、当該ファイル自身のglobal module fragmentから直接includeしていなかった。
- **対応:** `ArtifactAbstractLayer.ixx` と `.cppm` に`<QVector>`を直接includeし、Qt型の可視性をtransitive include／IFCの副作用に依存しないようにした。CMake再生成やビルドは未実行（AGENTSの明示許可待ち）。
- **価値または懸念:** 巨大moduleのIFC importerが暗黙依存を展開する経路を一つ減らせる。再発時は`PosterizeTime` importとTimeRemap JSON処理を別module／file-local helperへ分離するのが次の最小切り分け候補であり、ユーザーの既存dirty変更を巻き戻さない。
- **次に確認すること:** 許可後に`ArtifactAbstractLayer.cppm`だけを再ビルドし、同じC1001が残るかを確認する。残る場合はimportを一時的に外す診断ビルドで原因moduleを二分する。

## 2026-09-13 — Projected frameのハンドルとUnity Simple ViewCubeは別の固定座標系が必要

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`、`Artifact/src/Widgets/Render/ArtifactViewOrientationWidget.cppm`。
- **確認できた事実:** projected frameの枠線とscale/rotateハンドルは同じ`world.map(localPoint)`経路で描かれていたため、レイヤーの回転・非一様scaleでハンドルの四辺まで傾く。Unity Simple navigatorは軸deltaを固定値で描き、`orientation_`を更新しても中央面と三軸の見た目が静止していた。
- **対応:** 枠線は従来どおりレイヤー変換へ追従させ、scale/rotateハンドルだけをview逆行列から得たcamera-facing billboardへ分離した。ハンドル中心はレイヤー上の実点を維持し、サイズはlocal-to-world平均scaleで補正する。Unity Simpleは同じorientation quaternionからX/Y/Zの投影deltaと中央面polygonを再計算するようにした。
- **価値または懸念:** DCCの「操作点は対象上、操作面は画面に安定」という読みやすさを守り、ViewCubeのviewport orbitとの視覚的な不一致を解消できる。billboardの遠近差や極端な非一様scaleは実画面での確認が必要で、入力hit-testは既存の投影中心判定を維持している。
- **次に確認すること:** ビルド許可後、同一の3D planeでrotate／non-uniform scale／oblique viewを比較し、scale handleが正方形を保つこと、Unity SimpleのドラッグおよびAlt-orbitで軸と中央面が同じ向きへ追従することを確認する。

## 2026-09-13 — 3Dグリッドをcomposition平面からworld groundへ戻し、Dockタブ右端にPinを追加

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`。
- **確認できた事実:** 3Dグリッドは`(0..cw, 0..ch)`のcomposition矩形へ描画する経路に変わっており、斜めViewでは白いcomposition平面の外側へ到達しない。CoreにはX-Z平面の`computeGroundGridLines`があり、Native Dock側には`pinned_`とlayout JSONの`pinned`保存、ViewメニューのPin操作が既に存在したが、タブ上の直接操作はなかった。
- **対応:** 3D表示ではcomposition矩形ではなく、既存のworld-space X-Z ground gridをアクティブカメラへ描画する経路へ戻した。spacing・extent・fadeはviewport scale／カメラ距離から決め、軸線もground plane上へ出す。Dockタブの右側ボタンをPin＋Closeの複合ボタンへ変更し、既存の`setDockPinned`／保存状態と同期するイベントフィルタを追加した。新規signal/slot、CSS、GPU resource生成は追加していない。
- **価値または懸念:** グリッドがcomposition平面へ貼り付かず、3Dシーンの基準面としてviewportの外側まで連続する。ピン操作はDock責務内でViewメニューと同じ状態へ収束し、QADS側と同じくピン中はCloseを無効化する。ground-gridの見え方はカメラ極角とカメラ距離、絵文字Pin glyphのfont依存は実画面で確認が必要。
- **次に確認すること:** ビルド許可後、oblique／perspective／top-down viewでcomposition外にも線が出ること、Pinのクリック・キーボード・Viewメニュー・layout再読込の相互同期、D3D12/Vulkan共通経路を確認する。

## 2026-09-13 — 初期ワークスペースと空状態の意味をスクリーンショット基準で整理

- **関連:** `Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/Widgets/ArtifactProjectItemPresentation.cppm`、`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/CommonStyle.cppm`。
- **確認できた事実:** 起動時は保存済みDockグラフをworkspace mode適用後に復元していたため、Default設定でも過去セッションのTimeline／Curve Editorが再表示され得た。Project Viewの空状態イラストは外部ファイル探索だけで、パッケージ済みリソースへのfallbackがなく、プロジェクト未接続でも「Asset Browser linked」と表示していた。右側のProject情報面も未接続時に一般的な「Project／PREVIEW」を表示していた。Effectsの空状態アイコンは「No effects yet」以外で意図的に隠れていた。Composition breadcrumbは解決不能な親compositionのUUIDを表示し得た。選択中ツールバーのunderlineはラベル直下の2px帯だった。
- **対応:** Dock復元後にも選択workspaceを再適用し、DefaultのTimeline再開を抑止した。Project Viewは`composition_empty_composition.svg`を空状態イラストのfallbackにし、未接続時の一覧・右側情報面・下部同期表示を「No project」系へ揃えた。Effectsは未接続／未選択状態でも意味のあるアイコン、短い見出し、次の操作説明を表示するようにした。breadcrumbの未解決IDは表示対象から除外し、選択underlineは最下端へ1pxで移動して文字との間隔を確保した。
- **価値または懸念:** 起動時の「何を編集すべきか」がDefault workspaceと一致し、空状態でも次の操作が読み取れる。UUIDを隠すのは表示層のみで内部identity・診断ログは変更しない。fallback iconとowner-draw変更の実画面密度、既存ユーザーが明示的にAnimation workspaceを保存した場合の可視性はruntime確認が必要。
- **次に確認すること:** ビルド許可後、保存済みAnimation／Defaultの再起動、Project Viewの無プロジェクト／検索0件、Effectsの各未選択段階、nested composition breadcrumb、Motion Path選択時のunderline間隔を確認する。

## 2026-09-13 — Timeline Diligent snapshotの同一イベント内再構築を抑制

- **関連:** `Artifact/include/Widgets/ArtifactTimelineWidget.ixx`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** Diligent timeline previewは既に共有Diligent device／D3D12・Vulkan swap chain／`PrimitiveRenderer2D`で表示できるが、`refreshTracks()`中にplayhead・vertical offset・selection同期が連続して`syncGpuTimelineSnapshot()`を呼び、同じUIイベント内でもsnapshotのQVector構築とGPU render event投稿を複数回行っていた。
- **対応:** snapshot要求をUI event queueへcoalesceし、同一イベント内の連続要求は1回の`buildGpuTimelineSnapshot()`へまとめた。Diligent windowの初期化・submit・present、既存QWidget/QPainterの編集正規経路は変更していない。新規Qt signal/slot、CPU readback、GPU waitは追加していない。
- **価値または懸念:** GPU preview opt-in時の不要なCPU snapshot確保と重複submit要求を減らせる。snapshotは1つのqueued turn分だけ遅延するため、再生ヘッドやscrollの可視応答はruntimeで確認が必要。
- **次に確認すること:** ビルド許可後、`ARTIFACT_GPU_TIMELINE_PREVIEW=1`でscroll／zoom／selection／playhead更新を連続操作し、snapshot generationの増分がイベント単位でまとまり、D3D12／Vulkan表示が最新状態へ追従することを確認する。

## 2026-09-13 — Timeline Diligent snapshotをムーブ引き渡しにして一時QVectorコピーを削減

- **関連:** `Artifact/include/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.ixx`、`Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** `buildGpuTimelineSnapshot()`はそのイベント内でrect／line／triangle配列を構築した後、const参照版`setSnapshot()`が`make_shared`時に配列をもう一度深いコピーしていた。
- **対応:** `DiligentTimelineVisualSnapshot&&` overloadを追加し、GPU windowの共有snapshotへムーブして所有権を移す経路を追加した。UI側は完成したlocal snapshotを`std::move`で渡し、既存のconst参照APIとgenerationのlatest-wins判定は維持した。
- **価値または懸念:** 同一snapshotのQVector配列コピーを1回減らし、GPU submit／presentや既存の非待機設計は変更しない。共有snapshot自体の1回の確保と、描画中の読み取りは残るため、ムーブだけで全フレームallocationが解消するわけではない。
- **次に確認すること:** ビルド許可後、D3D12／VulkanのGPU previewでsnapshot generation、表示の最新性、scroll／zoom連続操作時のCPU allocationとフレーム時間を計測し、const／rvalue overloadのABIとmodule再スキャン影響を確認する。

## 2026-09-13 — 通常タイムラインの採用モックを先に固定し、Graph Editorを分離

- **関連:** `docs/design/timeline/README.md`、`docs/design/timeline/approved-normal-timeline-2026-09-13.png`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** 統合案では通常タイムラインとGraph Editorの操作が同じ画面へ重なり、レイヤー行・時間バー・カーブ操作の優先順位が読み取りにくかった。既存資料には左ペイン採用案、右ペイン比較案、カーブ参考案が別々に存在する。
- **対応:** ユーザーが承認した案2の改訂版を通常レイヤーバーモードの正本モックとして保存した。採用範囲を左ペイン、右時間領域、ルーラー、キャッシュ、ワークエリア、下部ナビゲーターに限定し、Graph Editor／Dope Sheetは専用面の切替責務として残した。
- **価値または懸念:** Diligent表示面はまず通常タイムラインのrow／bar／key整列へ集中できる。モックに描かれたキー操作位置は視覚案であり、Parent列の責務や新規ショートカットを暗黙に変更しないよう実装時に再配置が必要。
- **次に確認すること:** ビルド許可後、通常Timelineの左列とDiligent右面で同じ行高・時刻変換・選択状態が一致すること、キャッシュ状態の実データ表示が正しいことをruntimeで確認する。

## 2026-09-13 — 採用タイムラインの確認対象をDiligent表示面へ切り替え

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`docs/design/timeline/README.md`。
- **確認できた事実:** Diligent timeline pageは環境変数指定時だけ表示され、採用モックとの目視比較には毎回の明示切替が必要だった。GPU初期化失敗時のpainter復帰は既存の`setGpuTimelinePreviewEnabled(true)`に実装済み。
- **対応:** 環境変数が未設定ならDiligent pageを既定表示にし、`ARTIFACT_GPU_TIMELINE_PREVIEW=0`で従来面を強制できるようにした。初期化失敗時の既存fallback、編集・入力のQWidget正本、D3D12／Vulkan共通のDiligent経路は維持する。
- **価値または懸念:** 採用モックに対するGPU面の行／バー／キー調整を起動直後から確認できる。GPU初期化が重い環境ではTimeline生成時のcold pathが増える可能性があるため、初回表示の遅延とfallback理由はruntime確認が必要。
- **次に確認すること:** ビルド許可後、環境変数未設定／`=0`／GPU初期化失敗の3条件で表示切替、編集入力の正本維持、D3D12／Vulkanの表示を確認する。

## 2026-09-13 — Diligent snapshot用にTimeline visual配列の参照取得口を追加

- **関連:** `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** Diligent snapshot生成は`clips()`と`keyframeMarkers()`の値返しを使っており、GPU表示を有効にした各更新で可視判定前にQVector全体をコピーしていた。
- **対応:** 既存の値返しAPIを互換維持したまま、UIスレッドのDiligent生成専用に`clipsView()`／`keyframeMarkersView()`のconst参照APIを追加した。GPU snapshot builderだけが参照口を使い、編集・選択・既存呼出しの所有権契約は変更していない。
- **価値または懸念:** snapshot生成前の一時QVectorコピーを削減し、GPU面のCPU負荷を下げられる。参照はUIスレッド内の同期済みviewに限定しており、他スレッドからのview mutationやGPU resource lifetimeを拡張していない。
- **次に確認すること:** ビルド許可後、Timelineのselection／scroll／refresh中に参照の寿命と行同期を確認し、値返し経路との表示 parity、D3D12／VulkanのGPU描画を実機で比較する。

## 2026-09-13 — Diligent Timelineの選択表現を採用モックへ寄せる

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` の `buildGpuTimelineSnapshot()`。
- **対応:** GPU面のclipに左右のin/out端線を常時描き、selected clipだけ上下線もアクセント色で描く。selected keyframeには細い明色diamond outlineを重ね、通常keyとの差をQWidget版と同じ読み順にした。
- **価値または懸念:** 期間バーの境界と選択キーが暗い背景上で読みやすくなる。テキストラベルや新規GPUリソースは追加せず、既存のDiligent primitive batchへ線分を加えただけである。
- **次に確認すること:** ビルド許可後、D3D12／Vulkanでclip端が1px相当で過度に明るくならないこと、selected outlineがplayhead／隣接keyと干渉しないことを確認する。

## 2026-09-13 — 3D ground gridを後段overlayからworld backgroundへ移動

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** world ground gridは`drawViewportCanvasOverlay()`からレイヤー合成後に`draw3DLine()`／`flushGizmo3D()`されていたため、コンポジション面とレイヤーを深度・合成順に関係なく横切った。
- **対応:** grid生成を専用関数へ分離し、GPU resolve、RAM preview、direct fallbackの各経路でviewport背景の直後、コンポジション背景・レイヤー合成の直前に実行するようにした。
- **価値または懸念:** gridはコンポジション外のworld背景として残り、コンポジション／レイヤーが前面になる。既存の3D gizmo線は従来どおり後段overlayであり、挙動を変えない。未検証: Quad presentationで各pane固有のgrid passが必要かはruntimeで確認する。
- **次に確認すること:** ビルド許可後、D3D12／Vulkan、GPU resolve／RAM preview／direct fallbackで、perspectiveのコンポジション面・3D layerがgridを隠し、外側だけにgridが見えることを確認する。

## 2026-09-13 — Project Viewの未選択状態と操作surfaceを分離

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/Widgets/ArtifactProjectItemPresentation.cppm`、`docs/design/project_view_redesign_2026-09-08/project-view-focused-workbench.png`。
- **確認できた事実:** 右detail railは未選択時にもpreview説明、selection説明、無効なitem／proxy操作を同時表示し、中央empty stateと意味が重複していた。また作成toolboxがstatus barの直前に独立していたため、状態表示より強い別帯に見えていた。
- **対応:** projectなし／未選択時は右railを単一empty stateへ切り替え、選択時だけ詳細と操作を表示する。作成toolboxはbrowse context行へ移し、status barをproject health／Asset Browser同期の表示専用に戻した。選択詳細はtitle／metadata／previewの縦構成へ変更した。
- **価値または懸念:** Project Viewの「構造と状態を読む」責務が明瞭になり、Asset Browserとの責務境界を増やさず採用モックへ近づく。未検証: 狭幅時のsearch／filter row、選択したcompositionでinline editorを同時表示した場合の右rail高さはruntime確認が必要。
- **次に確認すること:** ビルド許可後、projectなし／projectあり未選択／composition／footage／複数選択、Tree／Tile、狭幅dockでレイアウトとselection同期を確認する。

## 2026-09-16 — Timelineトランジション範囲編集だけUndo経路が分離している

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`TimelineClipMoveRequestedEvent`、`TimelineClipResizeRequestedEvent`。
- **確認できた事実:** 通常レイヤーのクリップ移動／トリムは既存の`MoveLayerToFrameCommand`／`TrimLayerToFrameCommand`へ接続できる一方、トランジションは同じイベント購読内で`setTimelineTransitionRange()`を直接呼び、Undo snapshotを作成していない。
- **価値または懸念:** Diligent面とQPainter面は同じ入力経路を共有するため、トランジションだけUndo不能という差は両表示面に現れる。今回の通常クリップ編集対応へ混在させるとcomposition-owned transition契約まで範囲が広がる。
- **次に確認すること:** transition範囲・関連レイヤー・重なり制約を復元できる既存command／snapshot所有者を調べ、単一ドラッグを1 Undoへまとめる。未検証のため今回の実装対象外。

## 2026-09-16 — Diligent TimelineのPhase 3は既存GPUテキスト経路を再利用する

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`docs/planned/MILESTONE_TIMELINE_DILIGENT_GPU_SURFACE_2026-08-29.md`。
- **確認できた事実:** Timeline snapshotの`texts`は既に`PrimitiveRenderer2D::drawGlyphText()`へ渡されており、Diligent側にラベル描画の入口が存在する。新しいQt描画経路や個別GPU実装は不要。
- **価値または懸念:** Phase 3のglyph atlasは既存の共通glyph／shader管理を拡張する形で進めるべきで、Timeline専用のテキスト資源を増やすとD3D12／Vulkan parityとキャッシュ寿命が二重化する。
- **次に確認すること:** `PrimitiveRenderer2D`のglyph atlasキャッシュ、atlas更新タイミング、device loss後の再生成契約を確認してからTimelineラベルの実機受入条件を定義する。未検証のため実装は次段階とする。

## 2026-09-16 — Timeline primitiveの色変換はrender-local cacheで共有できる

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm` のDiligent draw loop。
- **確認できた事実:** snapshotは`QColor`を保持し、描画時にlinear `FloatColor`へ変換していた。同一フレーム内ではrow／clip／markerの色が繰り返し現れる。
- **対応:** 32スロットの固定長cacheをrender-localに置き、`QColor::rgba()`が一致するprimitiveは変換結果を再利用する。cacheはヒープを使わず、snapshotの所有権やD3D12／Vulkan resource lifetimeを変更しない。
- **価値または懸念:** 色変換の`pow`回数を減らせる一方、色数が32を超える場合は循環置換される。未検証: 実機のprimitive分布とGPU submit時間への寄与はプロファイルで確認する。
- **次に確認すること:** static／dynamic両laneを同一renderで記録する場合のcache hit率と、D3D12／Vulkan別のCPU submit時間を計測する。低hit率ならスロット数を増やす前に、色tokenの共有化を検討する。

## 2026-09-16 — Timeline glyph描画のフォント解決をrenderer寿命へ寄せる

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` の `drawGlyphText()`、`Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`。
- **確認できた事実:** Diligent Timelineのstatic snapshotはprimitive配列を再利用できても、各presentでラベルをglyphへ展開する際にコードポイントごとの一時vector、`QFont`解決、`GlyphKey`構築を行っていた。既存のrenderer寿命フォントキャッシュは実装済みだが、この入口では使われていなかった。
- **対応:** 既存のフォントキャッシュを利用し、コードポイントの重複解決用scratch容量をrenderer寿命で再利用するようにした。glyph atlas、command buffer、D3D12／Vulkanのresource lifetimeは変更していない。
- **価値または懸念:** staticラベルを含むTimeline再描画で一時確保とフォントフォールバック問い合わせを減らせる。`UniString::toStdU32String()`とglyph packet appendは現状維持で、完全なゼロアロケーションを意味しない。
- **次に確認すること:** ビルド許可後、長いクリップ名・CJK・emojiを含むTimelineでatlas更新、ラベル表示、D3D12／VulkanのCPU submit時間を確認する。未検証のため、キャッシュ変更だけで表示 parityを断定しない。

## 2026-09-16 — Timeline waveform fallbackは表示幅で線分数を上限化できる

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm` のwaveform fallback描画。
- **確認できた事実:** immutableなピークpayloadは最大64本へ縮約していたが、表示幅が狭いclipでも同じ本数をsubmitしていた。細いバーは隣接線と同じpixelへ重なり、視認情報を増やさない。
- **対応:** fallback線分数を`min(64, peakCount, clipWidthInPixels)`でbounded化し、波形色のlinear変換結果も波形単位で共有する。texture payload、Diligentのresource所有、Qt fallbackは変更しない。
- **価値または懸念:** 小さいaudio clipやズームアウト時のcommand buffer append量を減らせる。1px未満の幅は1本に丸めるため、極端に狭いclipの表現はruntimeで確認が必要。
- **次に確認すること:** ビルド許可後、ズーム・スクロール中のaudio clipで波形の連続性、選択色、D3D12／Vulkan submit時間を比較する。

## 2026-09-16 — TimelineラベルのTextStyle一時値をsnapshot描画内で再利用する

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm` の`drawSnapshot()`。
- **確認できた事実:** static／dynamic laneのラベル描画で、同じ`TextStyle`の値をclip／markerごとに構築していた。glyph cacheはstyle一致を前提にしているため、描画側での一時値生成は不要だった。
- **対応:** snapshot単位で`ArtifactCore::TextStyle`を1つ再利用し、ラベルのpixel sizeだけ更新して既存`drawGlyphText()`へ渡すようにした。
- **価値または懸念:** ラベル数に比例する小さな一時オブジェクト生成を抑え、既存glyph atlasのキャッシュ契約を維持する。フォント選択やラベル内容の意味は変更しない。
- **次に確認すること:** ビルド許可後、clip／markerの異なるpixel sizeが混在するケースで表示とglyph cache更新が正しいことを確認する。

## 2026-09-19 — i18n hardening 進捗: 翻訳キー移行と共通ラッパーの二重化

- **関連:** `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`（完了）、
  `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`・`ArtifactRenderMenu.cppm`（残6＋12を `TranslationManager::instance().tr(key, fallback)` 化）、
  `ArtifactCore/src/Localization/Localization.cppm`（連鎖フォールバック + 複数形 helper）、
  `tools/i18n/migrate_remaining_menus.py`（自動移行スクリプト）。
- **確認できた事実:** 4メニューファイルのうち `ArtifactFileMenu.cppm`・`ArtifactViewMenu.cppm` は翻訳可能なハードコード日本語0行。`ArtifactLayerMenu.cppm` 残4行は開発者コメント（翻訳対象外）、`ArtifactRenderMenu.cppm` 残2行もコメント。監査は `Keys used 1121 / Expected 1344, Coverage 100%` を維持。`--min-coverage 95` は `.github/workflows/i18n-check.yml` で既に設定済み。
- **懸念／仮説（未検証）:** 各メニューファイルで `static QString tt()`/`static QString menuText()` が独自に再定義されているが、名前は重複している。`tt()` は `TranslationManager::tr(key, fallback)` の薄いラッパー（フォールバックを必須にするだけ）で、`menuText()` は `tt()` に同じ。これらを `Core.Localization` の `inline` ヘルパーへ集約すれば、ハードコード移行の指示ミス（ラッパー定義忘れ）をコンパイルエラーで検知できる。ただし集約は呼び出しを大幅に書き換えるため、ビルド検証後に別フェーズで実施。
- **次に確認:** ビルド許可後、`tr(key, fallback)` の fallback を含む新規キー18が en/ja JSON に正しく載り、`added_to_queue` の `%1 ... .arg(added)` チェーンが維持されていることをコンパイル＋監査で確みめる。
## 2026-09-19 — 2Dフレームギズモの拡縮ゴースト基盤

- **関連:** `Artifact/src/Widgets/Render/TransformGizmo.cppm`、Composition Viewport の2D拡縮オーバーレイ。
- **確認済み事実:** 2D TransformGizmo はドラッグ開始時の global transform／local bounds／canvas bounding box を既に保持し、拡縮中の破線ゴースト、サイズバッジ、スマートガイド、Undo境界まで同じ所有者で扱っていた。新しいオーバーレイサービスやイベント配線は不要だった。
- **対応:** 元枠ゴーストを固定低透明度へ変更し、バッジを操作ハンドル外側へ配置。現在サイズ、X/Y倍率、幅・高さ差分、中央基準時の Anchor 表示を既存の `ArtifactIRenderer` 描画内へ統合した。
- **価値／懸念:** GPU資源・同期・レンダリング本流を変えずに操作フィードバックを強化できる。一方、表示値は既存 `targetBox` のcanvas-space寸法であり、回転レイヤーや特殊なsource-size編集でユーザーが期待する「素材ピクセル寸法」と一致するかは実機確認が必要。ドラッグ中の `QString` 更新は既存方式を踏襲しており、アロケーション実測は未実施。
- **次に確認:** 回転済み平面、辺／角／中央ハンドル、複数選択、Shiftによる初期サイズ編集、ズーム端、画面端でのHUDクランプを実機で確認する。

## 2026-09-19 — i18n: 言語切替のオンデマンド再翻訳はメニューの aboutToShow で既に成立していた

- **関連:** `ArtifactCore/src/Localization/Localization.cppm`（`setLanguage` → `LocaleChangedEvent` 発火、`fallbackChainFor`、`pluralCategoryFor`）、`ArtifactCore/include/Utils/Localization.ixx`（`LocaleChangedEvent` / `TranslationsReloadedEvent`）、`Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`（`GeneralSettingPage::saveSettings`）、`Artifact/src/Widgets/Menu/*.cppm`（全メニューが `QMenu::aboutToShow` で `rebuildMenu()`）。
- **確認できた事実:** `ArtifactFileMenu` を除く全メニュー（View/Edit/Layer/Render/Time/Option/Script/Composition 等）が `aboutToShow` で `rebuildMenu()` を呼ぶ。したがって「設定 OK でアクティブ言語を切り替える」だけで、次にメニューを開いた時点で新言語のラベルになる。購読者を各メニューへ新規配線する必要はなく、`LocaleChangedEvent` は将来の即時再翻訳（常時表示UI・ステータスバー等）用の通知として用意した。
- **対応:** `ArtifactAppSettings` に `General/LanguageCode` を追加し、環境設定の Language セレクタで保存。`saveSettings` で保存＋`LocalizationManager::setLanguageCode()` 即時適用。起動時は `--lang` > 保存設定 > システムロケール > `en` の順で確定し、`[AppMain] Language decided: <code> by <reason>` を1行出力。
- **価値／懸念:** 言語切替の即時反映を新しいシグナル／スロット配線なしで実現できる（既存の `aboutToShow` 再構築に乗る）。一方、メニューバーのトップレベルやステータスバーなど「常時表示で再構築されない」UIは次回起動まで旧言語が残り得る。また `LocalizationManager` に `QReadWriteLock` と `Event.Bus` 依存が入ったため、**コンパイル未検証**（AGENTS.md のビルド禁止による）。ロックは `translate()` の読取、`loadFromDirectory`/`reload`/`addTranslation`/`clearTranslations` の書込に限定し、`loadFromFile` は呼出側がロック済み前提（再入デッドロック回避）。
- **次に確認:** ビルド許可後、`QReadWriteLock` の再入（`translatePlural` → `translate` / `pluralCategory` の読取ロック重複）が Qt の仕様どおり安全か、`reload()` 中の `availableLocales()` 呼出がデッドロックしないかを確認する。常時表示UIの再翻訳は `retranslateUi` 相当の所有者責務を決めてから着手する。

## 2026-09-19 — i18n: 同一性比較に使う表示文字列は「同じキー」で両側を翻訳しないと壊れる

- **関連:** `Artifact/src/Widgets/Dialog/ArtifactImportAssetsDialog.cppm:72`（`group.title == QStringLiteral("連番")` の比較）と `:689`（`ImportGroup sequences{QStringLiteral("連番")}` の構築）。
- **確認できた事実:** `ImportGroup.title` は UI 表示ラベルでありながら、ツールチップ判定で `== "連番"` の同一性比較にも使われていた。片側だけを翻訳するとロケールによって一致しなくなる。両側を同じキー `import.group.sequence` で `tr()` 化したため、どのロケールでも比較が成立する。
- **価値または懸念:** i18n 移行時の典型的な罠。表示文字列を識別子として流用している箇所（グループ名・種別名の `==` 比較、`switch` の対象、保存値との照合）は、単に `tr()` で包むと壊れる。移行前に「この文字列は表示専用か、識別子も兼ねているか」を必ず確認し、識別子兼用なら同じキーで両側を揃えるか、enum/ID へ置き換えるべき。
- **次に確認（実施済み 2026-09-19）:** 横断検索の結果、`==` による日本語識別子比較は次の3ファイルに限られる。
  - `Artifact/src/Widgets/ArtifactMainWindow.cppm:1562-1696`（`toolName == "ブラシ" / "消しゴム" / "コピースタンプ" / "モーションスケッチ" / "テキスト"`）
  - `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm:1136-1507`（`toolName == "選択" / "移動" / "回転" / "スケール" / "アンカー" / "ペン" / "シェイプ" / "楕円" / "テキスト" / "モーションスケッチ" / "ブラシ" / "コピースタンプ" / "消しゴム"`、`primaryLabel == "点数" / "辺数"`）
  - `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cppm:895`（`activeLibrary_ == "お気に入り"`）
  `startsWith` / `contains` / `endsWith` / `compare` に日本語リテラルを渡す箇所は0件。
  **結論:** これら3ファイルはツール名／ライブラリ名を識別子として流用しているため、P0-2 の単純な `tr()` 置換の対象にしてはならない。移行するなら「表示名は翻訳しつつ、識別子は enum/ID へ分離する」設計変更が必要。当面は除外リストとして扱う。
  残りの P0-2 対象（`ArtifactAnimationMenu` / `ArtifactCompositionMenu` / `CreatePlaneLayerDialog` / `CreateCameraLayerDialog` / `PrecomposeDialog` / `ColorSwatchDialog` / `QuickLayerCreationDialog`）にはこの比較が無いため、通常の移行で問題ない。

## 2026-09-20 — `<stop_token>` C1116はAPI置換だけでは除去できない

- **関連:** `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`、`ArtifactCore/include/Thread/BackgroundTaskRuntime.ixx`、`ArtifactCore/include/Thread/LightweightTask.ixx`、MSVC 14.51 C++ Modules。
- **確認できた事実:** 製品コード内に `std::stop_token`、`std::stop_source`、`std::stop_callback`、`std::jthread` の直接利用は見つからない。一方、独自の `CancelToken` と `LightweightTaskContext::requestCancel()` が既にあり、キャンセル契約は `std::atomic<bool>` ベースで実装されている。今回のC1116は `Animation.Transform3D` のIFC import中にMSVC標準ライブラリ内部の `<stop_token>` 特殊化で発生しており、`<thread>`／`<future>`／`<memory>` 等から間接的に入る経路である。
- **仮説（未検証）:** キャンセルAPIを独自型へ統一すること自体は可能だが、それだけではMSVC標準ヘッダーの間接依存を消せず、今回のIFCエラーは解消しない。解消には、IFCへ取り込まれる標準ヘッダー面の縮小、header-based STLとnamed std BMIの混在防止、または該当implementation unitの非module化／分離が必要になる可能性が高い。
- **価値または懸念:** 独自キャンセル契約の統一は設計上有益だが、C1116回避と混同すると広範な置換を行ってもビルド障害が残る。標準ライブラリ完全置換はthread、future、condition_variable、memoryまで波及し、費用対効果が悪い。
- **次に確認すること:** 許可を得て該当IFCのみ再生成し、再現する場合は `/showIncludes` とproducer／consumerのcompile optionsを比較する。その後、`Artifact.Layer.Abstract` implementation群のGMF標準ヘッダーを1つずつ最小化し、C1116を起こす具体的なinclude境界を特定する。

## 2026-09-21 — ArtifactHashMap iteratorは衝突チェーンの巡回確認が必要

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`Physics.System` の標準連想コンテナ移行。
- **確認できた事実:** `ArtifactHashMap::iterator::operator++()` は現在のnodeの `next` を確認せず、直ちに次bucketへ進む。bucket内に複数nodeがある場合、range-forとiterator走査から2件目以降が見えない可能性がある。
- **対応:** 今回のphysics registryには採用せず、キー順を維持する `NamedVector` 基盤の内部registryを使用した。
- **価値または懸念:** `find()` / `operator[]` はbucket chainを走査するため、個別参照と全件走査で見える要素数が異なる恐れがある。未検証のため、既存利用箇所を一括変更しない。
- **次に確認すること:** collisionを意図的に発生させる小さなcontainer testを用意し、iterator、rehash、erase後の走査を確認してから共通mapとして採用する。

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

## 2026-09-23 — CLI の property.set と Command IR の二重編集経路

- **関連:** `Artifact/src/Application/ArtifactInteractiveShell.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`、`docs/planned/MILESTONE_CLI_PYTHON_AUTOMATION_2026-09-23.md`。
- **確認できた事実（静的読み取り）:** CLI `property.set` は限られたプロパティを JSON ファイルへ直接書き換え、CLI 内部だけの project snapshot undo/redo を使う。一方、WorkspaceAutomation の `set_property` は現在ロード中のレイヤーへサービス経由で適用し、Command IR の結果型を返す。Python bridge はアプリ API の戻り値を JSON から dict/list/scalar へ復元し、引数側も JSON 値で型を保つ。`artifact.core.automation` から command vocabulary、validate、execute を呼べる。さらにCLI `command-ir` は catalog / validate / execute requestを受け、executeの `saveProject:true` で既存Project Exporterを呼ぶ実装を追加した。`command-ir -` はJSON Linesを同一プロジェクトsessionで処理する（いずれも実行未検証）。
- **追加修正:** CLI `property.set` で文字列プロパティ `layerNote` に `true` / `false` / 数値文字列を渡すと、共通の値判定が JSON bool / number に変換していた。プロパティ種別ごとに代入を分け、文字列は常に文字列、bool は bool、数値は有限値のみ保存するよう修正した（ビルド・実行未確認）。
- **価値または懸念:** CLIシェルとアプリ自動化 API で同じ編集でも Undo・dirty state・型検証・戻り値の意味が異なる。AI が一方から他方へスクリプトを移すと、動作差を誤認する可能性がある。
- **次に確認すること:** 実行許可後、headless service 初期化、project load、active composition fallback、Command IR execute、`saveProject:true` の保存結果をCLI runtimeで確認し、コマンドシェル／JSONL request 経路での統合も検討する。既存の `property.set` は互換挙動を確認するまで拙速に置換しない。

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
