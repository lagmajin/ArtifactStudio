# ShapePath 自作ジオメトリ／描画経路移行マイルストーン

**ステータス:** Not Started
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
- bounds は二次／三次 Bézier の解析的 extrema を使い、制御点包絡矩形による過大評価を削減した。
- Bézier flatten API と `Close` の終端セグメントを追加し、stroke／fill の geometry 生成で再利用できるようにした。
- arc、角丸矩形、楕円、矩形、多角形、星形のプリミティブを Qt パス変換なしで構築する経路を整備した。
- `ShapePath::addPath()`、`ShapePath::reverse()`、`ShapeGroup::processedPaths()` の不要な Qt 往復を削減した。
- ArtifactShapeLayer では、単純なカスタム Bézier（単色 fill、標準 stroke、演算子なし）を `ShapePath::flatten()` から renderer へ渡す経路を追加した。特殊 fill／stroke と ShapeOperator は互換キャッシュ経路に残している。

残作業は、fill の複数サブパス／穴、stroke join／cap／dash、ShapeOperator の geometry packet 化、renderer の専用 fill tessellation、そして QPainter／QImage キャッシュの段階的縮小である。

## 現在の Qt 境界

- `PathShape::toPainterPath()` は既存 Qt API／互換描画の境界として残す。
- ArtifactShapeLayer の特殊 stroke、グラデーション、ShapeOperator は現時点では QImage キャッシュを使う。
- 単純なカスタム Bézier は `ShapePath::flatten()` と renderer の polygon／line API を使い、上記キャッシュ境界から分離した。
- 次の移行では、複数サブパスと穴を扱える fill tessellation を先に設計し、特殊 stroke の置換はその後に行う。
- 現行 renderer の polygon／line API だけでは穴を正しく表現できないため、複数輪郭を単純に個別 polygon として描く実装は行わない。winding／even-odd を保持した triangulated fill packet の境界を先に追加する。

## 関連文書

- `Artifact/docs/MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md`
- `Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`
- `docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md`
- `docs/planned/CLASS_DICTIONARY_DEPENDENCY_RESPONSIBILITY_2026-04-17.md`
- `docs/DOC_LIFECYCLE.md`
