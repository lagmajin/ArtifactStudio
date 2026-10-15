# プリコンポーズ独立GUI・MCPテスト

**最終更新:** 2026-10-15
**ステータス:** 実装済み、ビルド・実行確認中。

## 対象

FBX独立Viewerのように製品本体と起動を分け、既存のProject ServiceとUndoManagerを直接検証する。製品・子リポジトリのコードは変更しない。GUIは実際のレイヤー構造と属性をJSON表示する検査画面であり、Composition Viewportの描画テストではない。

`ArtifactPrecomposeViewer` は解像度1280×720、24fps、背景色 `(0.125,0.25,0.5,1)` のcompositionに A、B、Untouched のNullレイヤーを作成する。Aにはposition `(12,-8,3)`、opacity `0.42` を設定し、3レイヤーには異なる In/Out と start frame を設定する。選択移動・全移動後に子Compositionの解像度、fps、背景RGBAが親設定と一致することも確認する。GUIのキーボード操作とMCPは同じ処理を呼ぶ。Pは選択相当のA+Bを移動、Aは全属性モード、K/Dは子を保持/削除して解除、Z/YはUndo/Redo、Rは新しいfixture、Tは既存の `runPreComposeTests()` を実行する。fixtureの再作成は同じ一時プロジェクトに新しいcompositionを追加するため、長時間の連続負荷試験には使わない。

## ビルド・実行

Windows専用。既存のArtifactAppRuntimeにリンクする独立したconsole executableで、追加の製品モジュール登録は不要。

```powershell
$env:VSLANG = '1033'
cmake --preset x64-Debug-Agent -DARTIFACT_BUILD_PRECOMPOSE_VIEWER=ON
cmake --build out/build/x64-Debug-Agent --target ArtifactPrecomposeViewer -j 4
out/build/x64-Debug-Agent/bin/Debug/ArtifactPrecomposeViewer.exe
```

`--check` は既存プリコンポーズ回帰テストだけを実行し、失敗があれば終了コード1を返す。`--mcp-stdio` は改行区切りJSON-RPCの旧MCP `2024-11-05` initialize、tools/list、tools/call、pingに対応する。stdin EOFで終了する。製品ログはstderrへ分離する。新MCP server/discover形式は未対応。GUIを前面化せず、左端ディスプレイへ配置する。

ツールは引数なしの `test.state`、`test.reset`、`test.cycleRejected`、`test.invalidRequestsAtomic`、`test.openChildComposition`、`test.matchWorkspaceDuration`、`test.childDurationPropagates`、`test.internalParentRestored`、`test.transitiveCycleRejected`、`test.externalParentRestored`、`test.animatedTiming`、`test.precompose`（逆順・重複selection IDでもソース順を維持）、`test.precomposeAll`（空のlayerIdsで全移動を要求）、`test.saveReload`、`test.nestedHierarchy`、`test.reparentExistingPrecompose`、`test.unprecomposeNested`、`test.nonContiguousSelectionOrder`、`test.unpackKeep`、`test.unpackDelete`、`test.undo`、`test.redo`、`test.runRegression`、`test.capture`。`test.cycleRejected`は自身を参照するComposition Layerを作り、選択したプリコンポーズが直接循環を拒否し、階層とUndo履歴を変えないことを検査する。`test.invalidRequestsAtomic`は空白のcomposition名とnil LayerIDを選択した要求が拒否され、レイヤー／composition数、状態、Undo履歴、結果IDが変わらないことを検査する。`test.openChildComposition`は初回プリコンポーズで子Compositionへ移動し、Undoで親へ戻り、Redoでは親フォーカスを維持したまま子とManager登録を復元することを検査する。`test.matchWorkspaceDuration`は開始frameが0でないWorkspace範囲を使い、子CompositionのdurationとWork Areaがそのdurationへ正規化される一方、親側レイヤーのIn/Out/Startが元の選択範囲を保つことを検査する。`test.childDurationPropagates`は子Compositionのdurationを短縮・延長し、親側In/Start固定でOutが新durationへ追従することを検査する。`test.transitiveCycleRejected`は親→子の既存階層で子から親を参照する候補をプリコンポーズし、間接循環を拒否してComposition数・レイヤー数・Undo履歴を変えないことを検査する。`test.externalParentRestored`は選択外レイヤーを親に持つレイヤーをプリコンポーズし、子内で親が外れ、解除とUndo/Redoで親リンクと復元メタデータが戻ることを検査する。`test.internalParentRestored`は選択したA/B間の親子リンクを、子Composition内、解除、Undo、Redoの各段階で検査する。`test.animatedTiming`は異なるIn点を持つ2レイヤーにキーを設定し、選択移動では正規化後のキー時刻と値・補間・アンカー・制御点を、全移動ではキー時刻が変わらないことを照合し、解除・Undo/Redoで両方の元データが戻ることを検査する。`test.saveReload`は一時 `.artifact` ファイルへ保存後、PreComposeManager登録を明示的に解除してから再読込する。親子レイヤーデータに加え、実行時のネスト登録、親逆引き、階層が読み戻しで復元されることを確認する。`test.nestedHierarchy`は2段のプリコンポーズを作り、ルート→子→孫の階層IDと、ルート／孫間の時間変換往復を検査する。`test.reparentExistingPrecompose`は既存プリコンポーズレイヤーをさらに別のプリコンポーズへ移し、子に対するManager親参照とルート→ラッパー→既存子の階層が更新されること、Undoで元親へ戻ること、Redoでラッパー親へ再登録されることを検査する。`test.unprecomposeNested`はネストした子レイヤーを解除し、子の時間範囲とManager登録解除、Undoでの階層復元、Redoでの登録解除を検査する。`test.nonContiguousSelectionOrder`はAとUntouchedを非連続選択して元の子順を保持し、解除、Undo、Redoの各時点で元レイヤー順へ戻ることを検査する。captureはQtのwidget grabからPNGを返す。これはCPU描画の検査画面専用のQt API境界であり、GPU viewport captureの代用ではない。新規signal/slot接続や製品Transport経路は追加しない。

```powershell
python tools/test_precompose_mcp.py `
  --server out/build/x64-Debug-Agent/bin/Debug/ArtifactPrecomposeViewer.exe `
  --language ja `
  --output-dir "$env:LOCALAPPDATA/Temp/ArtifactStudio/precompose-test/ja"
```

同じコマンドをja/en/zh/zh-TWで実行する。`--lang`でQLocaleを指定できるが、テスト専用ラベル・キー名は全ロケールで英語の診断表記とする。製品の翻訳済みプリコンポーズダイアログの翻訳・レイアウト検査は対象外。

検査は直接・間接循環と不正要求の副作用なし拒否、子Compositionを開く遷移とUndo/Redo時のフォーカス、Workspace durationへの一致と子duration変更の親Out伝播、内部／外部親リンクとアニメーションキーの保持、逆順・重複selectionを含むレイヤー順序、ID・transform・opacityおよび親Composition設定の保持、非選択レイヤー不変、選択/全移動（全移動は空選択引数も含む）、2段のネスト階層と時間変換、既存プリコンポーズの再親子付け、ネスト解除時の時間と登録、非連続選択解除の順序復元、子保持/削除、Undo/Redo時の子ID・時間範囲・レイヤー復元、保存・再読込、および既存の回帰テストを対象とする。選択移動では最小Inを0へ正規化し、親側nest layerは元の時間範囲とoffsetを保つこと、解除では元のIn/Outへ戻ることを照合する。全移動では子レイヤーの時間とアニメーションキーが維持され、nest layerはframe 0から始まることを照合する。結果JSON、stderr、検査画面PNGは指定した出力先へ保存する。出力先はrepo外を推奨する。GPU描画、実アプリの選択UI操作はこのテストの合格範囲に含めない。
