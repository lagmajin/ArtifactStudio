# Text GPU Runtime 分離マイルストーン

**最終更新:** 2026-10-02

**ステータス:** Existing glyph GPU path / standalone runtime split pending

## 現行コード監査 (2026-08-15)

製品 Renderer 側には `GlyphAtlas`、atlas texture upload、glyph quad／transformed glyph submit、glyph PSO、coverage／color bitmap の入力経路が存在する。2026-10-02 の再監査では `ArtifactTextGlyphSubmitterRuntime`／`ArtifactTextGlyphSmoke` もコード上確認した。ビルド・GPU実行はこの監査では行っていないため、実行受入は未完とする。

## 現行コード再監査 (2026-10-02)

- `Artifact/CMakeLists.txt` に `ArtifactTextGlyphSubmitterRuntime` と `ArtifactTextGlyphSmoke` のターゲット定義があり、Glyph submitter の契約／実装 module を含む独立候補経路が存在する。2026-08-15 の「分離 module／link target を確認できない」という記録は古いため訂正する。
- `ArtifactTextGlyphSubmitter::submit()` は glyph atlas upload、通常／変形 vertex 組み立て、GPU submit を所有し、device／pipeline provider を外部注入する。atlas dirty 判定と vertex／constant buffer payload cache もある。
- ただし smoke target の実ビルド／実行、製品 `DiligentImmediateSubmitter` からの atlas・resource・submit ownership 移行、複数 script／emoji fixture の現行再受入は今回実行していない。独立ターゲット定義の存在を runtime 成功の証拠にはしない。

判定: **独立 submitter と smoke target はコード上実装済み。製品 renderer 移行と runtime 受入は pending。**

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
