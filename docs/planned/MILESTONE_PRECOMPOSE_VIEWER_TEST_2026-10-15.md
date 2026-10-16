# プリコンポーズ独立GUI・MCPテスト

**最終更新:** 2026-10-16
**ステータス:** 実装済み、ビルド・実行未確認（依頼により省略）。

`test.unpackLayerIdCollisionAtomic`は解除対象の子レイヤーIDを親に残るレイヤーIDと衝突させ、解除が拒否され、親・子Composition、wrapper、Undo/Redo履歴が変更されないことを確認する。

`test.unpackDuplicateChildLayerIdsAtomic`は子Composition内の2レイヤーを同じIDにし、解除が拒否され、重複した子レイヤーと親Composition、wrapper、Undo/Redo履歴が保持されることを確認する。

`test.unpackNilChildLayerIdAtomic`は子レイヤーのIDをnilにして解除を試し、拒否後も親・子Composition、wrapper、Undo/Redo履歴が保たれることを確認する。

`test.openChildComposition`は子Compositionにフォーカスしたまま親wrapper IDで解除を要求し、拒否後もactive composition、親wrapper、子レイヤー、Undo/Redo履歴が変化しないことを確認する。

`test.saveReload`は保存後に再読込した子レイヤーをそのまま解除し、エフェクト・アニメーション付きマスク・マット参照・コンポーネントdescriptorが親へ移ること、親Compositionのエフェクトが子レイヤーへ混入せず親に残ること、Undoで子とwrapperへ戻り、Redoで同じ状態を再適用することも検査する。

`test.rationalFrameRatePreserved`は30000/1001 fps（transform keyframe scale 30）のpositionカーブを選択移動・保存再読込・解除とMoveAllAttributes・Undo/Redoへ通し、キー配列と中間フレームの評価値を照合する。

`test.negativeTimingRoundTrip`は負のIn点と負時刻キーを含むpositionカーブを−24 frame正規化し、負／正キー間の中間評価値が親時刻と子時刻で一致して、解除・Undo/Redoでも保たれることを確認する。

`test.nonContiguousSelectionOrder`はAとUntouchedを選ぶ非連続移動で、異なるフレームにあるposition／opacityキーを選択範囲の最小In点に合わせて補正し、子側の中間評価値と解除・Undo/Redo後のキー／評価値が一致することも確認する。

`test.animatedTiming`は30000/1001 fpsのCompositionで30 ticks/secのキー時刻を使い、選択移動・全移動それぞれで子Compositionの正確なfpsとキーフレームtime scaleが保たれることも確認する。選択移動では40フレームのローカル時刻補正後、Bezier／Smoothを含む位置・回転・scale・anchor・opacityのキーと、opacityを除くTransform3D snapshotが元Compositionの対応時刻と一致することを、解除・Undo/Redoまで確認する。全移動は時刻を変えず、解除・Undo/Redo後のキーと評価値も照合する。

同テストのアニメーション保存・再読込検査はposition／rotationの各軸、scale X/Y、anchorの各軸、opacityのキーフレーム配列に加え、Bezier／ConstantとSmooth／Linearの混在カーブを中間フレームで評価した値も比較する。

opacity override解除時には、既存の有効範囲内opacityカーブを残したまま現在フレームへoverride値がキーフレームとしてmaterializeされ、Undo/Redo後もキー配列と評価値が維持されることを確認する。

`test.childCompositionEffectStaysWithChild`は子Compositionへ設定したBrightness effectがプロジェクト再読込後も子に保持され、子レイヤー解除時に親へ移らず、Undo/Redoでも子Compositionの属性として残ることを検査する。

`test.sharedChildReferences`は同一親Compositionの2つのwrapperが共有するchildを保存・再読込し、runtime mappingを復元した後に片方だけ解除しても他方のwrapperとchildが残ること、Undo/Redoで2参照と1参照へ正しく往復することを検査する。


`test.sharedChildDurationPropagates`は別々の親Compositionから共有するchildを再読込した後、一方の親だけでwrapperを削除してももう一方のruntime mappingとchild内容が残ること、Undo/Redoで参照を復元すること、さらに子を保持した解除でも片親共有を維持することを検査する。

`test.nestedSaveReload`は再読込後に最深階層のComposition Layerを解除し、最深階層のdurationが親からの短縮・延長に追従し、両段wrapperのin/start固定とout追従、解除後のレイヤー順・タイミング、3階層のruntime mappingがUndo/Redoで復元されることも検査する。

`test.reparentExistingPrecompose`は既存wrapperを別階層へ移した3階層状態を保存・再読込し、親子mappingとwrapperの3D transform・animation keyを確認した後、解除・Undo/Redoまで継続する。

`test.missingExternalParentUnwrapAtomic`は外部親レイヤー削除後に保存・再読込し、残存wrapperの親metadataを保ちつつ、解除時に存在しない親を復元しないことをUndo/Redoまで検査する。

## 対象

FBX独立Viewerのように製品本体と起動を分け、既存のProject ServiceとUndoManagerを直接検証する。製品・子リポジトリのコードは変更しない。GUIは実際のレイヤー構造と属性をJSON表示する検査画面であり、Composition Viewportの描画テストではない。

`ArtifactPrecomposeViewer` は解像度1280×720、24fps、背景色 `(0.125,0.25,0.5,1)`、duration 300 frameのcompositionに A、B、Untouched のNullレイヤーを作成する。Aにはposition `(12,-8,3)`、opacity `0.42` を設定し、3レイヤーには異なる In/Out と start frame を設定する。選択移動・全移動後に子Compositionの解像度、fps、背景RGBA、Workspace一致を指定しない場合のframe range durationが親と一致することも確認する。GUIのキーボード操作とMCPは同じ処理を呼ぶ。Pは選択相当のA+Bを移動、Aは全属性モード、K/Dは子を保持/削除して解除、Z/YはUndo/Redo、Rは新しいfixture、Tは既存の `runPreComposeTests()` を実行する。fixtureの再作成は同じ一時プロジェクトに新しいcompositionを追加するため、長時間の連続負荷試験には使わない。

## ビルド・実行

Windows専用。既存のArtifactAppRuntimeにリンクする独立したconsole executableで、追加の製品モジュール登録は不要。

```powershell
$env:VSLANG = '1033'
cmake --preset x64-Debug-Agent -DARTIFACT_BUILD_PRECOMPOSE_VIEWER=ON
cmake --build out/build/x64-Debug-Agent --target ArtifactPrecomposeViewer -j 4
out/build/x64-Debug-Agent/bin/Debug/ArtifactPrecomposeViewer.exe
```

`--check` は既存プリコンポーズ回帰テストだけを実行し、失敗があれば終了コード1を返す。`--mcp-stdio` は改行区切りJSON-RPCの旧MCP `2024-11-05` initialize、tools/list、tools/call、pingに対応する。MCPハーネスはtools/listの重複・形式と、呼び出す全ツール名が公開されていることを検査する。stdin EOFで終了する。製品ログはstderrへ分離する。新MCP server/discover形式は未対応。GUIを前面化せず、左端ディスプレイへ配置する。

ツールは引数なしの `test.state`、`test.reset`、`test.guiKeyboardRoundTrip`、`test.cycleRejected`、`test.undoBudgetRejectsStructureChanges`、`test.invalidRequestsAtomic`、`test.redoBranchDiscarded`、`test.keepParentFocused`、`test.componentDescriptorsSurviveUnprecompose`、`test.matteReferencesSurviveUnprecompose`、`test.maskSurvivesUnprecompose`、`test.effectSurvivesUnprecompose`、`test.compositionEffectStaysOnParent`、`test.childCompositionEffectStaysWithChild`、`test.openChildComposition`、`test.negativeTimingRoundTrip`、`test.rationalFrameRatePreserved`、`test.matchWorkspaceDuration`、`test.childDurationPropagates`、`test.parentDurationPropagatesToChild`、`test.sharedChildDurationPropagates`、`test.internalParentRestored`、`test.transitiveCycleRejected`、`test.exposedOverrideMaterialized`、`test.nestedExposedOverrideMaterialized`、`test.missingChildReferenceAtomic`、`test.emptyChildUnwrapRejected`、`test.emptyMoveAllRejectedAtomic`、`test.unpackDeleteRedo`、`test.layerNotesRoundTrip`、`test.layerFlagsAndBlendRoundTrip`、`test.singleLayerRoundTrip`、`test.sharedChildReferences`、`test.sharedInstanceOverridesRoundTrip`、`test.externalParentRestored`、`test.missingExternalParentUnwrapAtomic`、`test.childEditsSurviveUnwrap`、`test.selectionTransitions`、`test.externalParentSaveReload`、`test.animatedTiming`、`test.precompose`（逆順・重複selection IDでもソース順を維持）、`test.precomposeAll`（空のlayerIdsで全移動を要求）、`test.saveReload`、`test.nestedHierarchy`、`test.nestedSaveReload`、`test.nestedExternalParentRestored`、`test.reparentExistingPrecompose`、`test.unprecomposeNested`、`test.nonContiguousSelectionOrder`、`test.unpackKeep`、`test.unpackDelete`、`test.undo`、`test.redo`、`test.runRegression`、`test.capture`。`test.guiKeyboardRoundTrip`はGUIへQt KeyPressを送り、Pで選択移動、Aで全移動、Kで子を保持して解除、Dで子を削除して解除し、Z/Yで各操作をUndo/Redo、Rで履歴を消去してfixtureを再作成し、選択状態も初期化する。各段階のcomposition state、active compositionと選択/current layer、子の存在、Undo/Redo可否、親子の解像度・fps・背景・duration、全レイヤー移動後のA/B/Untouched順と各In/Out/Startを照合する。`test.cycleRejected`は自身を参照するComposition Layerを作り、選択したプリコンポーズが直接循環を拒否し、階層、Undo/Redo履歴、選択レイヤー、active compositionを変えず、拒否後にfixtureだけを除去できることを検査する。`test.undoBudgetRejectsStructureChanges`はUndoManagerのmaxSingleEntryBytesを一時的に0へ設定し、precomposeとunprecomposeが履歴へ保持できない場合に拒否され、レイヤー列・Composition数・Manager登録を変えないことを検査する。`test.invalidRequestsAtomic`は空白名、nil LayerID、空の選択vectorをMoveSelectedへ渡した要求が拒否され、レイヤー／composition数、状態、Undo/Redo履歴、結果IDに加えて選択レイヤーとactive compositionも変わらないことを検査する。有効なプリコンポーズをUndoしてRedo stackを用意し、不正要求後に履歴件数とRedo実行可能性を確認する。`test.redoBranchDiscarded`は有効なプリコンポーズをUndoしてRedo分岐を作り、別のプリコンポーズを実行すると古い分岐が破棄され、新しい分岐だけがUndo/Redo可能なことを検査する。MoveAllAttributesで空vectorを受理するケースと対にする。また、解除へnil LayerIDまたは通常のNullレイヤーIDを渡した場合も拒否され、レイヤー、Composition数、Undo履歴、解除結果ID、選択状態が変わらないことを検査する。`test.keepParentFocused`はopenNewComposition=falseでプリコンポーズしても親Compositionとwrapper選択が維持され、Undo/Redo後も親フォーカスが保たれる一方、子CompositionとManager登録が正しく作成・復元されることを検査する。`test.componentDescriptorsSurviveUnprecompose`は選択レイヤーのcomponent descriptor graph・generator・field・clone modifier・cloner transformにversion・enabled・phase・scope・order・依存関係・nested settings・各transform値を設定し、プリコンポーズ、解除、Undo/Redo後もsnapshot全体が一致することを検査する。`test.matteReferencesSurviveUnprecompose`はAから選択レイヤーBへの2つのmatte referenceについてID・順序・source ID・互換asset path・enabled/type/blend/fit/opacity/invertを子移動、解除、Undo/Redoで照合する。`test.maskSurvivesUnprecompose`は選択レイヤーのmask enabled/locked/color、path名・閉鎖・opacity・inversion・modeとvertex位置・tangent geometryと複数時刻のanimation key（頂点・opacity・feather・falloff・mode）が子移動、子を残す解除、Undo/Redoで保持されることを検査する。`test.effectSurvivesUnprecompose`は選択レイヤーにBrightness effectを設定し、同一effect objectとbrightness値がchild移動、子を保持した解除、Undo/Redoで保たれることを検査する。`test.compositionEffectStaysOnParent`は親CompositionのBrightness effect objectと値が選択レイヤーのプリコンポーズ後も親に残り、子Compositionには複製されないこと、および解除・Undo/Redoでもこのscopeが保たれることを検査する。`test.openChildComposition`は初回プリコンポーズで子Compositionへ移動し、Undoで親へ戻り、Redoでは親フォーカスを維持したまま子とManager登録を復元することを検査する。`test.matchWorkspaceDuration`は開始frameが0でないWorkspace範囲を使い、子CompositionのdurationとWork Areaがそのdurationへ正規化される一方、親側レイヤーのIn/Out/Startが元の選択範囲を保つことを検査する。同時に子Compositionを開く設定も有効にし、duration一致とフォーカス遷移の併用を確認する。`test.negativeTimingRoundTrip`は負のIn/Startと負時刻キーを正規化して子へ移し、wrapperの負時間範囲、解除、Undo/Redoで元の範囲とキーを復元することを検査する。`test.rationalFrameRatePreserved`は30000/1001のexact rational frame rateを選択移動・全移動の子Compositionへ引き継ぎ、保存・再読込時に比率とManager登録が復元され、解除とUndo/Redoでも親側の正確な比率を維持することを検査する。`test.childDurationPropagates`は子Compositionをactiveにした状態でdurationを短縮・延長し、親側In/Start固定でOutが新durationへ追従し、child focusが維持されることを検査する。 `test.parentDurationPropagatesToChild`は親durationの段階的な短縮・延長が子durationへ反映され、wrapperのIn/Start固定とOut追従が維持されることを検査する。`test.sharedChildDurationPropagates`は1つの子Compositionを異なる2つの親から参照し、durationの短縮・延長が両wrapperのIn/Startを保ったままOutへ反映されること、および2つのManager登録とwrapperの時間範囲が保存・再読込で復元されることを検査する。 `test.sharedParentDurationPropagates`は1つの子Compositionを同じ親の2つのwrapperから参照し、durationの短縮・延長時に両wrapperのIn/Startを維持してOutを更新することを検査する。`test.transitiveCycleRejected`はルート→子→孫の3階層を作り、孫からルートを参照する候補のプリコンポーズを拒否してComposition数・レイヤー数・Undo履歴を変えないことを検査する。`test.exposedOverrideMaterialized`はComposition Layerに公開したopacity overrideが解除時にsource layerへ反映され、Undoでchild内へ戻った後もRedoまで値が保持されることを検査する。`test.nestedExposedOverrideMaterialized`は内側source opacity overrideと外側nested wrapper opacity overrideを別々に設定し、両方が解除前は遅延されること、ネスト状態の保存・再読込後も各階層のManager登録と値が復元すること、外側解除で内側wrapperだけへ外側値がmaterializeされ、内側解除ではsourceへ内側値が反映されてUndo/Redoでも保たれることを検査する。`test.missingChildReferenceAtomic`はwrapperのsource IDがnilの壊れた参照状態で解除を試み、親state、子Composition、wrapper、Undo履歴を保持してからfixtureの参照を戻す。`test.emptyChildUnwrapRejected`は子レイヤーを空にした状態で解除を試み、親wrapper、空child、Composition数、Undo履歴、解除結果記録が変わらないことを検査する。 `test.emptyMoveAllRejectedAtomic`は全レイヤー移動モードへ空のlayerIdsを指定した要求自体は許容される一方、Compositionが空の場合は拒否され、Composition数とUndo/Redo履歴を変えず、fixtureのレイヤー順も復元することを検査する。`test.unpackDeleteRedo`は子Composition削除を伴う解除を行い、Undoで同じ子IDとManager登録を復元し、Redoで子を再削除して元レイヤー列を復元することを検査する。`test.singleLayerRoundTrip`はAだけを選択移動し、source layer object・ID、wrapper位置、Manager登録がUndo/Redoで往復することを検査する。`test.layerNotesRoundTrip`はA/BのUnicode・複数行ノートと選択外レイヤーのノートがプリコンポーズ、解除、Undo/Redo後も保持されることを検査する。`test.layerFlagsAndBlendRoundTrip`はA/Bのvisibility・lock・solo・blend modeと選択外レイヤーの各値がプリコンポーズ、解除、Undo/Redo後も保持され、MCP状態JSONから参照できることを検査する。`test.sharedInstanceOverridesRoundTrip`は同じ子Compositionを参照する2つのwrapperへ異なるopacity overrideを設定し、子のbase値を変えずに両インスタンスの値が独立すること、保存・再読込後も2つのManager登録と各override値が保持されることを検査する。`test.sharedChildReferences`は同じ子Compositionを参照する2つのwrapperを作り、片方の解除とRedo後ももう片方の参照・Manager登録と子のA/Bレイヤーデータを保持することを検査する。残った最後のwrapperを子削除付きで解除し、Undoで同一child ID・レイヤーデータ・Manager登録を戻し、Redoで再削除することも確認する。`test.externalParentRestored`は同じ選択外レイヤーを親とするA/Bをプリコンポーズし、子内で両リンクが外れ、解除とUndo/Redoで各レイヤーの復元metadataと親リンクが戻ることを検査する。 `test.missingExternalParentUnwrapAtomic`は外部親レイヤーが子プリコンポーズ中に削除された状態で解除し、sourceのdangling parent IDを残さず、Undoでwrapper metadata、Redoでparent linkなしのsourceを復元することを検査する。`test.nestedExternalParentRestored`は異なる外部親を持つA/Bの復元情報を持つ既存プリコンポーズをさらにネストし、外側解除後のManager親参照と両メタデータ、内側解除後に各リンクが別々の親へ戻ること、および内側解除のUndo/Redoを検査する。`test.childEditsSurviveUnwrap`は子Composition内でA/Bのposition・rotation・scale・anchor・opacityを変更し、解除後とUndo/Redo後も同じlayer objectに編集値が残ることを検査する。MCP状態JSONもactive composition・選択/current layerとcompositionの解像度・fps・背景・durationとlayer note、visibility、lock、solo、blend mode、3D rotation／scale／anchorを公開する。`test.selectionTransitions`はプリコンポーズ後のwrapper選択、解除後の先頭source選択、Undo/Redo時の選択対象とactive compositionを照合する。`test.externalParentSaveReload`はA/Bが異なる選択外レイヤーを親に持つ複数の復元メタデータと実行時mappingを保存・再読込で復元し、その後の解除で両方の外部親リンクが戻ることを確認する。`test.internalParentRestored`は選択したA/B間の親子リンクを、子Composition内、解除、Undo、Redoの各段階で検査する。`test.animatedTiming`は異なるIn点を持つ2レイヤーにposition Xとrotation X／scale X／anchor Xのキーを設定し、選択移動では正規化後のキー時刻と値・補間・アンカー・制御点を、全移動ではキー時刻と3レイヤーのIn/Out/Startが変わらないことを照合し、解除・Undo/Redoでキーと時間範囲の元データが戻ることを検査する。`test.saveReload`はlayer note・visibility・lock・solo・blend mode・3D rotation／scale／anchorとrotation／scale／anchorのキー、Brightness effect、静的・アニメーションmask、matte reference、component descriptor snapshot、および公開opacity property定義・instance overrideを設定して一時 `.artifact` ファイルへ保存後、PreComposeManager登録を明示的に解除してから再読込する。親子レイヤーデータ、全属性とtransform keyframe、effect値、mask属性とキー、matte ID／source／設定、component descriptor、property定義・override値、overrideが子レイヤーへまだ適用されていないことに加え、実行時のネスト登録、親逆引き、階層が復元されることを確認する。`test.nestedSaveReload`はルート→子→孫を保存し、両段のManager登録解除後に再読込する。二つの親逆引き、三段階層、compositionデータとルート／孫時間変換の復元を検査する。`test.nestedHierarchy`は2段のプリコンポーズを作り、ルート→子→孫の階層IDと、ルート／孫間の時間変換往復を検査する。`test.reparentExistingPrecompose`はtransform channel keyを含む位置・回転・scale・anchor・opacity・時間範囲を設定した既存プリコンポーズwrapperをさらにネストし、wrapper objectと属性を保持したままManager親参照と階層が更新されること、Undo/Redoと外側解除のUndo/Redoでも同一wrapperと属性を復元することを検査する。`test.unprecomposeNested`はネストした子レイヤーを解除し、子の時間範囲とManager登録解除、Undoでの階層復元、Redoでの登録解除を検査する。`test.nonContiguousSelectionOrder`はAとUntouchedを非連続選択して元の子順を保持し、解除、Undo、Redoの各時点で元レイヤー順へ戻ることを検査する。captureはQtのwidget grabからPNGを返す。これはCPU描画の検査画面専用のQt API境界であり、GPU viewport captureの代用ではない。新規signal/slot接続や製品Transport経路は追加しない。

```powershell
python tools/test_precompose_mcp.py `
  --server out/build/x64-Debug-Agent/bin/Debug/ArtifactPrecomposeViewer.exe `
  --language ja `
  --output-dir "$env:LOCALAPPDATA/Temp/ArtifactStudio/precompose-test/ja"
```

同じコマンドをja/en/zh/zh-TWで実行する。`--lang`でQLocaleを指定できるが、テスト専用ラベル・キー名は全ロケールで英語の診断表記とする。製品の翻訳済みプリコンポーズダイアログの翻訳・レイアウト検査は対象外。

検査は直接循環と3階層の間接循環、プリコンポーズ／解除の不正要求の副作用なし拒否、子Compositionを開く遷移とUndo/Redo時のフォーカス、Workspace durationへの一致と同時に子を開く遷移、プリコンポーズ／解除後の選択とactive composition、子duration変更の単一／複数親Out伝播、内部／外部親リンク、子内編集後のposition／rotation／scale／anchor／opacityと各transform channelのアニメーションキー保持、外部親メタデータの保存・再読込、逆順・重複selectionを含むレイヤー順序、ID・transform・opacityおよび親Composition設定の保持、非選択レイヤー不変、選択/全移動（全移動は空選択引数も含み、全レイヤーの時間範囲を解除・Undo/Redoまで検証）、2段のネスト階層と時間変換・保存再読込、既存プリコンポーズの再親子付け、ネスト解除時の時間と登録、非連続選択解除の順序復元、Master Property overrideの解除時反映と壊れたchild参照・空child解除の副作用なし拒否、GUIキーボードA/P/K/D/R/Z/Y経路、子保持/削除、削除解除のUndo/Redoでの子Composition ID・Manager登録・レイヤー列の往復、レイヤーvisibility／lock／solo／blend modeの往復、Undo/Redo時の子ID・時間範囲・レイヤー復元、保存・再読込時のlayer metadataおよびMaster Property定義とoverride値の保持、および既存の回帰テストを対象とする。選択移動では最小Inを0へ正規化し、親側nest layerは元の時間範囲とoffsetを保つこと、解除では元のIn/Outへ戻ることを照合する。全移動では子レイヤーの時間とアニメーションキーが維持され、nest layerはframe 0から始まることを照合する。結果JSON、stderr、検査画面PNGは指定した出力先へ保存する。出力先はrepo外を推奨する。GPU描画、実アプリの選択UI操作はこのテストの合格範囲に含めない。
