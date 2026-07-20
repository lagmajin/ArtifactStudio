# Blender / Shotcut ソース監査メモ (2026-07-20)

## 参照用チェックアウト

- Blender: `J:\dev\_reference-audit-20260720\blender` (`96294be`)
- Shotcut: `J:\dev\_reference-audit-20260720\shotcut` (`81af75c`)

全履歴・全ビルドは取得せず、必要なソースパスだけを shallow / sparse checkout した。ArtifactStudioへソースはコピーしていない。

## Blender から参考にする範囲

### Asset Browser / Asset Catalog

主な参照箇所:

- `source/blender/asset_system/`
- `source/blender/editors/space_file/asset_catalog_tree_view.cc`
- `source/blender/editors/space_file/file_context.cc`

有効な設計要素は、アセット本体と Catalog の分類を分離し、UUIDでCatalogを安定参照すること、Catalog TreeをUIのフィルタとして扱うこと、Read-only Libraryでは変更操作を明確に拒否することである。ArtifactStudioでは Asset Browser の検索・コレクション・タグ面に応用できる。

### Library Override

主な参照箇所:

- `source/blender/blenkernel/BKE_lib_override.hh`
- `source/blender/blenkernel/intern/lib_override.cc`

参考にするのは、リンク元とローカル差分を別管理する考え方、差分の適用順序、再同期時の競合扱いである。Blenderのデータブロック実装を移植せず、ArtifactStudioでは「共有Asset / ProjectローカルOverride / 再同期結果」の3層モデルとして再設計する。

### Cycles の Render Pass / AOV / Denoise

参照対象:

- RenderPassの名前付きパス管理
- AOVを名前で登録し、レンダー結果のパスとして保持する構造
- Denoise用の補助パス（通常のカラーだけでなく、Albedo / Normal 等）
- OpenImageDenoiseをCompositor / Render Resultに接続する境界

ArtifactStudioでは、まず `RenderPassDescriptor` / `AOVDescriptor` / `RenderResultChannel` のようなデータ契約を設けるのが適切であり、Cyclesコードの移植はしない。

### GPU Compositor

主な参照箇所:

- `source/blender/compositor/`
- `source/blender/compositor/algorithms/compute_preview.cc`
- `source/blender/compositor/shaders/`
- `gpu_shader_compositor_ocio_processor.glsl`

参考になるのは、ノード単位でGPU実装を持ち、共通のGPU shader library、preview用の計算経路、OCIO processorのGPU評価を分離している点である。ArtifactStudioでは、既存のCompute Shader資産を単発で増やすより、RenderPassの入力・出力契約とノード評価境界を先に揃えるべきである。

## Shotcut / MLT から参考にする範囲

Shotcut本体はGPLv3のアプリケーションであるため、ソースコードの移植対象にはしない。参考対象はUIの挙動、プロジェクト表現、MLTとの接続パターンに限定する。

特に有効だった箇所:

- `src/mltcontroller.cpp`: MLT runtimeとの境界
- `src/docks/timelinedock.cpp`: Timeline UIとモデルの分離
- `src/proxymanager.cpp`: proxyの識別、元メディアへの復帰、ハッシュによる生成管理
- `src/commands/timelinecommands.cpp`: XMLスナップショットを使ったUndo/Redo
- `src/models/attachedfiltersmodel.cpp`: Producerに紐づくFilter編集

ArtifactStudioへの安全な応用は、Shotcutコードのコピーではなく、次の抽象化である。

- `Producer / Filter / Transition / Consumer` を既存のLayer / Effect / RenderQueueへ対応付ける
- Preview proxyとoriginal sourceをstable IDで分離する
- Undo用に構造化スナップショットを保持する
- Timeline上の編集状態とMedia/Render runtimeを分離する

## ライセンス判定

- Blender本体: GPLv3。設計・挙動・公開仕様・論文を参考にし、コードを流用しない。
- Shotcut本体: GPLv3。ArtifactStudioへリンク・コピー・派生実装として取り込まない。
- MLT framework: LGPLv2.1部分とGPLアプリケーション部分が分かれる。MLTを導入する場合は、framework、`mlt++`、個別サービス、GPLプラグインを分けて依存監査する。
- FFmpeg / Frei0r等のShotcut依存も、リンク形態とビルド構成ごとに別途確認する。

## 結論

最も安全で効果が高い順は以下。

1. BlenderのAsset Catalog / Overrideの考え方をArtifactStudioのAssetモデルへ反映
2. BlenderのRenderPass / AOV / Denoise契約をArtifactStudioのRenderResultへ設計反映
3. Blender GPU Compositorのノード境界とOCIO GPU処理を設計参考にする
4. ShotcutはUI/UXとproxy/Undoの挙動だけ参照
5. MLTはGPL部分を除外できるかを確認してから、必要なら別途依存評価
