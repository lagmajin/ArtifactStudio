# Composition Viewport Channel Test

**最終更新:** 2026-10-18
**ステータス:** ✅ Complete

## 目的

FBX Animation Viewer と同じく、製品アプリ本体から独立して起動する GUI で Composition Viewport の表示チャンネルを確認する。実際の `CompositionRenderController` を専用の viewport host 上で使い、UI 選択または MCP stdio から表示モードを切り替える。

## 実装

- `ArtifactCompositionViewportChannelTest` を opt-in の独立 executable として追加する。CMake option は `ARTIFACT_BUILD_COMPOSITION_VIEWPORT_CHANNEL_TEST`（既定 OFF）。
- テスト用 `ArtifactComposition3D` は R/G/B/補色/白/グレーのカラーバー、連続アルファランプ、透明・半透明ブロックを持つ RGBA 画像と、画面中央に配置した `tests/models/light_composition_icosphere.obj` で構成する。AOV モードではカラーチャートを隠し、OBJ 単体で Depth／Albedo／Normal／Position／UV などを確認する。Velocity は再生中に確認する。
- 表示モードを選ぶたびに `CompositionRenderController::renderOneFrame()` で更新後の frame を出し、切替直後の古い frame を capture しない。
- RGB / Alpha / RGBA / R / G / B と、Depth、Emission、Object ID、Material ID、Albedo、Normal、Velocity、Position、UV、Unpremultiplied、Luminance、Matte を同じ VP へ設定できる。
- `--mcp-stdio` では `viewport.state`、`viewport.setChannel`、`viewport.capture` を公開する。画像キャプチャは Windows Graphics Capture を使う。

## 起動

既存のビルドディレクトリを使う場合は、CMake configure 時に option を ON にしてから target を build する。既定の出力先は `out/build/<構成>/bin/<構成>/ArtifactCompositionViewportChannelTest.exe`。

```powershell
cmake -S . -B out/build/x64-Debug -DARTIFACT_BUILD_COMPOSITION_VIEWPORT_CHANNEL_TEST=ON
cmake --build out/build/x64-Debug --target ArtifactCompositionViewportChannelTest --config Debug
```

`tools/test_mcp_client.py` から `viewport.setChannel` を呼び出し、`viewport.capture` で各表示を記録できる。2026-10-18 に MSVC Debug で target をビルドし、MCP runtime を確認した。31 モードすべての `setChannel` が応答し、RGB / R / G / B / Alpha / Depth / Normal / UV の capture に成功した。最終 capture は `out/build/x64-Debug/channel-viewport-fresh-frame-captures/` に保存される。

## チャンネル確認結果

| モード | 結果 |
|---|---|
| RGB / R / G / B | 切替と色バーを目視確認 |
| Alpha | アルファランプがあるが VP 全体が白い |
| Depth | OBJ 単体でも一様なマゼンタ |
| Normal | OBJ 単体でも一様な淡色 |
| UV | OBJ 単体でも黒 |
| その他 24 モード | MCP の切替と同期 frame 描画は成功。個別の目視確認は未実施 |

このテストプログラムと MCP 経由の切替・capture 経路は完成した。Alpha と auxiliary AOV の表示結果は別の製品 renderer 調査で追う。Velocity の再生中表示は今回未確認。
