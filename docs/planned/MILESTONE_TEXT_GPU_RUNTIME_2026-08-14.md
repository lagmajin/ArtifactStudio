# Text GPU Runtime 分離マイルストーン

**最終更新:** 2026-08-14

## 目的

`TextAnimatorLab` のGPU検証を、ArtifactCore全体・Audio・Video・Particleのビルドに依存させず、製品Rendererと同じglyph atlas / shader / transform契約で実行できるようにする。

## 現状の問題

`ArtifactRenderTextSmoke` は `ArtifactRender` にリンクしている。`ArtifactRender` は `ArtifactCore`、`ArtifactCoreAudio`、`ArtifactCoreVideo`、`ArtifactCoreMedia`、`ArtifactCoreNetwork` を要求するため、テキスト1ケースのGPU検証でも全CoreのC++ modules生成が発生する。

## 分離後の責務

`ArtifactRenderTextRuntime` は次だけを所有する。

- `DiligentDeviceManager` のheadless初期化・終了
- `ShaderManager` のglyph pixel shader契約
- `PrimitiveRenderer2D` のglyph quad生成とper-glyph transform
- `DiligentImmediateSubmitter` のglyph atlas upload / draw
- `GlyphAtlas` のmonochrome coverage / color bitmap texture入力
- 明示的なGPU readback

## 禁止する依存

- `ArtifactRender` 全体へのリンク
- `ArtifactIRenderer` のcomposition / post-process / layer orchestration
- `ArtifactCoreAudio`、`ArtifactCoreVideo`、`ArtifactCoreMedia`、`ArtifactCoreNetwork`
- `QPainter`、Qt CompositionMode、GPU本流の新規QImage変換

## 合格条件

1. `Text Sample1` がGPUで非ゼロ画像を生成する。
2. CJK fixture がcoverage atlasとして描画される。
3. `🧪` とZWJ fixtureがcolor-preserved atlas入力を通る。
4. `image=幅x高さ saved=1` をログで確認する。
5. ArtifactRender/Core/Diligentの成果物が同一ビルド世代である。

## 実装順

1. glyph drawに必要なDiligent device / upload / readback境界を抽出する。
2. `ArtifactRenderTextRuntime`のmodule setとlink setを追加する。
3. standalone smokeを新Runtimeへ切り替える。
4. Latin → CJK → emoji → ZWJの順にGPU監査を実行する。
