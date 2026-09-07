> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_SHAPE_PATH_CORE_IMPLEMENTATION_2026-04-16.md](MILESTONE_SHAPE_PATH_CORE_IMPLEMENTATION_2026-04-16.md)

**最終更新:** 2026-09-05

# ShapePath 自作ジオメトリ／描画経路移行マイルストーン

**ステータス:** In Progress（native geometry の通常描画移行済み、gradient／stroke alignment等は明示fallback、runtime検証待ち）

**ステータス:** Partial implementation / Phase 0 contract documented / runtime verification pending
**作成日:** 2026-07-27
**対象:** 静止画・連番・シェイプ・画像処理・3Dレイヤーの基盤強化

## 目的

シェイプ描画の主経路を、Qt の `QPainterPath`／`QPainter` に依存するハイブリッド実装から、`ArtifactCore::ShapePath` を中心としたバックエンド非依存のジオメトリ経路へ段階的に移行する。描画品質、変換、マスク、合成、保存／再読込、プレビュー安定性を維持しながら、将来の GPU 描画と専用 CPU フォールバックの共通基盤を作る。

## 現状ベースライン

- `ArtifactCore::ShapePath` は Move／Line／Quadratic／Cubic／Close のコマンド列と JSON 化を持つ。
- `ShapePath` の bounds、contains、長さ、arc、simplify、addPath などは現在も `QPainterPath` 変換を利用する箇所がある。
- `ArtifactShapeLayer`、`ShapeLayer`、ShapeGroup 周辺には `QPainterPath`、`QPainter`、`QImage` キャッシュを使う既存経路が残っている。
- OpenCV は主に画像変換・画像処理境界で使われており、パスジオメトリの本体ではない。
- したがって、現時点は「自作クラスへ置換済み」ではなく、「自作コマンドモデルを導入したが、Qt 変換・描画経路が残る状態」である。

## 到達目標

1. `ShapePath` のコマンド列を、描画バックエンドが直接利用できる安定したジオメトリ契約にする。
2. Bézier の評価、解析的 bounds、平坦化／テセレーション、サブパス、fill rule を Qt 型なしで扱えるようにする。
3. fill／stroke の形状、join、cap、dash、transform、opacity を renderer 向けの明示的な geometry packet として渡せるようにする。
4. プリミティブとカスタム Bézier の描画を新経路へ移し、ShapeGroup／マスク／キャッシュを順次追従させる。
5. `QPainterPath`／`QPainter` は import／export、互換境界、診断用フォールバックに限定し、通常のシェイプ描画と Qt 合成を主経路にしない。

## フェーズ

### Phase 0 — 責務と互換性の固定

- `ShapePath` の command、subpath、閉じ状態、fill rule、座標系、epsilon の契約を文書化する。
- 現行の Qt 経路を呼び出し箇所単位で棚卸しし、許容する境界と撤去対象を分類する。
- 既存 JSON、undo／redo、変換、マスク、レイヤーキャッシュの互換条件を固定する。

### Phase 1 — Core ジオメトリ基盤

- Qt の `QPainterPath` に依存しない bounds、point／tangent、length、subpath 処理を整備する。
- tolerance と transform を入力に取る flatten／tessellation API を設計する。
- 退化した曲線、空パス、極端なスケール、自己交差、閉じていないサブパスを定義済み動作にする。

### Phase 2 — Renderer 接続

- fill／stroke の geometry packet と renderer 境界を追加する。
- まず矩形・楕円・多角形・Cubic の代表ケースを新経路へ接続する。
- 旧経路との比較用に、描画結果、bounds、頂点数、フォールバック理由を診断できるようにする。

### Phase 3 — レイヤー／演算子移行

- `ArtifactShapeLayer` と `ShapeLayer` の通常描画を新経路へ移行する。
- ShapeGroup、複数サブパス、fill／stroke、マスク、変換、キャッシュ無効化を順に追従させる。
- 画像レイヤー、連番レイヤー、3Dレイヤーとの合成境界は既存の画像／renderer 契約を維持する。

### Phase 4 — Qt 経路の縮小と品質確認

- 通常描画から不要になった `QPainterPath`／`QPainter`／`QImage` キャッシュを撤去または明示的フォールバック化する。
- 既存プロジェクトの保存／再読込、表示、変換、マスク、合成、プレビュー安定性を確認する。
- 旧経路との差分を確認し、品質・性能・メモリの基準を満たした対象から段階的に旧経路を無効化する。

## 非対象

- 動画読み込み、動画デコード、動画専用の大規模設計変更。
- DiligentEngine／DX12 backend の広範な変更。
- `ReactiveEvents` の変更。
- SVG 等の入出力互換を理由としない Qt API の一括撤去。

## 実装制約

- 新規の `QPainter` 合成、`QImage` ホットパス、QtCSS、公開シグナル／スロットは追加しない。
- C++20 modules の module purview、自己 import、循環依存、直接 include の規約を守る。
- renderer API は既存の `ArtifactIRenderer` 責務と重複させず、必要な境界を先に確認する。
- 既存の Qt 経路は、移行完了まで比較可能なフォールバックとして保持する。

## 完了条件

- ShapePath の主要ジオメトリ処理が Qt 変換なしで実行できる。
- 主要なシェイプ種別とカスタム Bézier が新 renderer 経路で同一の保存データから再現できる。
- fill／stroke、変換、マスク、合成、キャッシュ無効化、空／不正入力の扱いが定義されている。
- 旧 Qt 経路へのフォールバックが明示的な条件と診断情報を持つ。
- 既存の静止画・連番・画像・3Dレイヤーの作業を阻害せず、動画対応を新たな優先作業に戻さない。

## 次の実装単位

Phase 0 として、`ShapePath` のコマンド／サブパス／fill rule／flatten tolerance の契約を短い技術メモに固定し、その後に Qt 非依存 bounds と flatten の最小実装を追加する。実装開始時は `.ixx` の依存を増やさず、既存の実装モジュールで代替できるかを先に確認する。

## 実装進捗

- `ShapePath` の bounds、length、位置サンプリング、接線／法線、contains を自作 geometry 経路へ移行した。
- `PathFillRule`（Winding／EvenOdd）を `ShapePath` と JSON／Qt 境界へ追加した。
- bounds は二次／三次 Bézier の解析的 extrema を使い、制御点包絡矩形による過大評価を削減した。
- Bézier flatten API と `Close` の終端セグメントを追加し、stroke／fill の geometry 生成で再利用できるようにした。
- arc、角丸矩形、楕円、矩形、多角形、星形のプリミティブを Qt パス変換なしで構築する経路を整備した。
- `ShapePath::addPath()`、`ShapePath::reverse()`、`ShapeGroup::processedPaths()` の不要な Qt 往復を削減した。
- ArtifactShapeLayer では、単純なカスタム Bézier（単色 fill、標準 stroke、演算子なし）を `ShapePath::flatten()` から renderer へ渡す経路を追加した。特殊 fill／stroke と ShapeOperator は互換キャッシュ経路に残している。
- `ShapePath::triangulate()` を追加し、fill rule（Winding／EvenOdd）と穴を考慮した多輪郭 triangulation を Core 側で実装した。穴はゼロ幅ブリッジで外輪郭へ統合し、既存の ear-clipping で三角形列にする。分類は輪郭内外の filled 判定で行い、冗長輪郭は除外する。
- ArtifactShapeLayer の native カスタム Bézier fill を `triangulate()` ベースへ切り替えた（ローカル座標で分割し、変換後に `drawSolidTriangleLocal` へ渡す）。stroke は全サブパスを描画する。triangulation 失敗時は単一輪郭のみ従来の polygon fallback を使い、多輪郭は穴を失う polygon 描画をせず fill をスキップする。

残作業は、stroke join／cap と標準シェイプを含む dash の全面的な geometry 化、ShapeOperator の geometry packet 化、ShapeGroup の多輪郭 fill の新経路接続、そして QPainter／QImage キャッシュの段階的縮小である。カスタム Bézier の dash は native stroke 経路へ接続済み。

## 現在の Qt 境界

- `PathShape::toPainterPath()` は既存 Qt API／互換描画の境界として残す。
- ArtifactShapeLayer の特殊 stroke、グラデーション、ShapeOperator は現時点では QImage キャッシュを使う。
- 単純なカスタム Bézier は `ShapePath::flatten()` と renderer の polygon／line API を使い、上記キャッシュ境界から分離した。
- 次の移行では、特殊 stroke（join／cap／dash）の geometry 化を設計する。多輪郭 fill の packet 境界は `ShapePath::triangulate()` として Core 側に確立済み。
- winding／even-odd と穴の所属は `triangulate()` 内で解決し、renderer へは三角形列のみを渡す。複数輪郭を個別 polygon として描く実装は行わない方針を維持し、triangulation 失敗時の多輪郭 fill はスキップする（単一輪郭のみ polygon fallback）。
- Core 側には `flattenSubpaths()`／`triangulate()` を追加済み。既存の `drawSolidTriangleLocal` の内部バッチ経路を利用し、新しい低レベル描画 API は追加していない。
- native custom Bézier fill は単一輪郭・多輪郭とも `triangulate()` を使う。退化・失敗時は単一輪郭のみ既存 polygon API へ戻る。

## 2026-08-08 implementation update

- 標準プリミティブ、custom polygon、custom Bézier を同じ `ShapePath` → `triangulate()`／`flattenSubpaths()` 経路へ統合した。
- 標準シェイプの cap／join／dash は `ArtifactIRenderer::drawStyledPolyline()` を使う native stroke へ移行した。
- ShapeOperator 入力生成から `QPainterPath` → `ShapePath::fromPainterPath()` の不要な往復を撤去した。
- operator結果は多輪郭を含めて `ShapePath` geometry cache から三角形／subpath列としてrendererへ渡す。
- Qt/QImage互換キャッシュは、現rendererに同等契約がない gradient fill、inside/outside stroke、taper／gradient stroke、およびnative operator処理が結果を返さない場合に限定した。
- fallback理由は `gradient-fill`、`stroke-alignment`、`custom-stroke-effect`、`shape-operator` として明示的に診断ログへ記録する。
- source変更時は native geometry cache と互換image cacheを同時にinvalid化する。
- `MaskPath::fromShapePath()`／`toShapePath()` をApp境界に追加し、CoreからMaskへの逆依存を避けながらCubic tangentを保持する双方向変換を実装した。
- `ArtifactShapeLayer` に順序付き `ShapeStackNode`（Path／Fill／Stroke／Operator）を追加した。空の stack は旧プロジェクトの content + layer-global operator 評価を維持し、stack を持つレイヤーだけが GPU 描画時に順序どおり評価される。stack は JSON 保存／復元、nativeShapePaths、bounds 計算、および content/operator の構造変更時の参照更新を行う。
- この段階では stack を操作する専用リスト UI と、SVG／CPU fallback／bounds の同一評価器への統合は未完了である。
- `WigglePaths` は旧 `amount`／`frequency` を保持したまま、Temporal Phase、Detail、Correlation、Smooth/Corner を JSON・clone・Property 経路へ追加した。ArtifactShapeLayer の GPU 評価では、複製した operator に composition time を加算して自動変化させる。
- Layer Menuに独立した「マスクとシェイプ」面を追加し、「シェイプをマスクに変換」「マスクをシェイプに変換」をUndo対応で接続した。従来Proxy submenu内に混在していたmask preset／text mask導線も同面へ移した。
- シェイプの互換キャッシュは一辺 16,384 px・合計 64 Mi px に正規化し、巨大な `QImage` 確保を防止する。縦横比は維持する。
- 星形／多角形、custom polygon／Bézier、dash pattern の入力・復元上限を固定し、非有限値と座標絶対値 1,000,000 超を除外する。
- stroke width と corner radius はキャッシュ寸法を上限として正規化する。

残る完了ゲートはビルド、代表shapeの描画比較、保存／再読込、mask／composition、preview安定性のruntime検証である。

## Static audit follow-up (2026-07-29)

`ArtifactCore::ShapePath` と `ArtifactShapeLayer` の実装を静的に確認した。ビルド・描画比較は未実施。

| 項目 | 現状 | 判定 |
|---|---|---|
| Core geometry | bounds、flatten、subpath、fill rule、triangulate、主要 primitive は自作経路に存在する。 | 実装済み／描画確認待ち |
| Native renderer path | 単純カスタム Bézier の fill/stroke は renderer 経路へ接続済み。 | 部分実装 |
| Advanced stroke/operator | join/cap、標準シェイプを含む dash の全面移行、ShapeOperator の geometry packet 化は未完了。カスタム Bézier の dash は native 経路に接続済み。 | 部分実装 |
| Qt boundary reduction | 特殊 stroke、gradient、operator、互換キャッシュに QPainter/QImage 経路が残る。 | 未完了 |

### 判定

`Not Started` ではなく、Core geometry と単純 Bézier の native path は実装済み。ただしシェイプ全体の主経路移行、特殊 stroke/operator、Qt キャッシュ縮小、runtime parity 検証が残るため、マイルストーン全体は完了扱いにしない。

## 関連文書

- `Artifact/docs/MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md`
- `Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`
- `docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md`
- `docs/planned/CLASS_DICTIONARY_DEPENDENCY_RESPONSIBILITY_2026-04-17.md`
- `docs/DOC_LIFECYCLE.md`
