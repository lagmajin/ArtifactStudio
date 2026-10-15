# プリコンポーズ独立GUI・MCPテスト

**最終更新:** 2026-10-15
**ステータス:** 実装済み、ビルド・実行確認中。

## 対象

FBX独立Viewerのように製品本体と起動を分け、既存のProject ServiceとUndoManagerを直接検証する。製品・子リポジトリのコードは変更しない。GUIは実際のレイヤー構造と属性をJSON表示する検査画面であり、Composition Viewportの描画テストではない。

`ArtifactPrecomposeViewer` は A、B、Untouched のNullレイヤーを作成する。Aにはposition `(12,-8,3)`、opacity `0.42` を設定し、3レイヤーには異なる In/Out と start frame を設定する。GUIのキーボード操作とMCPは同じ処理を呼ぶ。Pは選択相当のA+Bを移動、Aは全属性モード、K/Dは子を保持/削除して解除、Z/YはUndo/Redo、Rは新しいfixture、Tは既存の `runPreComposeTests()` を実行する。fixtureの再作成は同じ一時プロジェクトに新しいcompositionを追加するため、長時間の連続負荷試験には使わない。

## ビルド・実行

Windows専用。既存のArtifactAppRuntimeにリンクする独立したconsole executableで、追加の製品モジュール登録は不要。

```powershell
$env:VSLANG = '1033'
cmake --preset x64-Debug-Agent -DARTIFACT_BUILD_PRECOMPOSE_VIEWER=ON
cmake --build out/build/x64-Debug-Agent --target ArtifactPrecomposeViewer -j 4
out/build/x64-Debug-Agent/bin/Debug/ArtifactPrecomposeViewer.exe
```

`--check` は既存プリコンポーズ回帰テストだけを実行し、失敗があれば終了コード1を返す。`--mcp-stdio` は改行区切りJSON-RPCの旧MCP `2024-11-05` initialize、tools/list、tools/call、pingに対応する。stdin EOFで終了する。製品ログはstderrへ分離する。新MCP server/discover形式は未対応。GUIを前面化せず、左端ディスプレイへ配置する。

ツールは引数なしの `test.state`、`test.reset`、`test.cycleRejected`、`test.precompose`、`test.precomposeAll`、`test.unpackKeep`、`test.unpackDelete`、`test.undo`、`test.redo`、`test.runRegression`、`test.capture`。`test.cycleRejected`は親を参照するComposition Layerを作り、選択したプリコンポーズが循環を拒否し、階層とUndo履歴を変えないことを検査する。captureはQtのwidget grabからPNGを返す。これはCPU描画の検査画面専用のQt API境界であり、GPU viewport captureの代用ではない。新規signal/slot接続や製品Transport経路は追加しない。

```powershell
python tools/test_precompose_mcp.py `
  --server out/build/x64-Debug-Agent/bin/Debug/ArtifactPrecomposeViewer.exe `
  --language ja `
  --output-dir "$env:LOCALAPPDATA/Temp/ArtifactStudio/precompose-test/ja"
```

同じコマンドをja/en/zh/zh-TWで実行する。`--lang`でQLocaleを指定できるが、テスト専用ラベル・キー名は全ロケールで英語の診断表記とする。製品の翻訳済みプリコンポーズダイアログの翻訳・レイアウト検査は対象外。

検査はレイヤー順序、ID・transform・opacity保持、非選択レイヤー不変、選択/全移動、子保持/削除、Undo/Redo時の子ID・時間範囲・レイヤー復元、および既存の回帰テストを対象とする。選択移動では最小Inを0へ正規化し、親側nest layerは元の時間範囲とoffsetを保つこと、解除では元のIn/Outへ戻ることを照合する。全移動では子レイヤーの時間が維持され、nest layerはframe 0から始まることを照合する。結果JSON、stderr、検査画面PNGは指定した出力先へ保存する。出力先はrepo外を推奨する。GPU描画、保存・再読込、実アプリの選択UI操作はこのテストの合格範囲に含めない。
