# MILESTONE: Element 3D 相当機能 — Shape/Text 押し出しジオメトリと World Position AOV

**最終更新:** 2026-11-21

**ステータス:** In Progress（押し出しコア・TextExtrude レイヤー配線・Position/UV AOV 実装済み。Shape 3Dモード・runtime検証残）

## 目的

Element 3D 相当の機能のうち、(1) Shape/Text 輪郭からの Extrude + Bevel 3D ジオメトリ生成、
(2) world position / depth のコンポジット連携出力(AOV)を実現する。

## 実装済み（2026-08-25）

1. `Geometry.ShapeExtrude` モジュール（ArtifactCore）
   - `ArtifactCore/include/Geometry/ShapeExtrude.ixx`
   - `ArtifactCore/src/Geometry/ShapeExtrude.cppm`（非 export 実装ユニット、force list 不要）
   - `extrudeContourMesh(contours, params, outMesh)`: 閉輪郭群（even-odd で穴分類）から
     `ArtifactCore::Mesh` を生成。外周=CCW / 穴=CW に回転方向を正規化し、
     ear clipping キャップ（穴内部三角形は重心判定で除去）+ 四分円ベベルリング +
     側壁クワッドで構成。position / normal / uv 属性付きで
     Model3D の `generateRenderData()` 経由 GPU 経路にそのまま乗る形式。
   - ベベル無効（bevelWidth <= 0）時は直角押し出しにフォールバック。
2. `ArtifactCoreShapeExtrudeTest`（2026-08-30、ビルド未実行）
   - 閉じた矩形の position / normal / uv 属性、depth 範囲、render data 生成を静的な
     回帰対象へ追加。
   - bevel による幾何増加と、不正 depth 入力時に既存 `Mesh` を置換しない契約を追加。

## 未着手

- **レイヤー配線 (2026-09-21 TextExtrude まで実装)**: `ArtifactProcedural3DLayer` に
  `Procedural3DLayerKind::TextExtrude` を追加。`Procedural3DGenerators::generateTextExtrude`
  (QFont/QPainterPath → toSubpathPolygons → `extrudeContourMesh` → `generateRenderData`)
  で Mesh 化し、既存 drawMesh/material/AOV 経路へ接続。text/fontFamily/fontSize/bold/italic/
  depth/bevelWidth/bevelSegments をプロパティ・JSON・プリセット(`beveledText3D`)・
  Layerメニュー(`Text 3D (Extrude)`)へ露出。残りは (a) Shape レイヤーの 3D モード切替。
- **UI**: extrude depth / bevel width / bevel segments のプロパティ露出（Inspector）。
  ※TextExtrude 分は実装済み。Shape 3Dモード分が残。
- **World Position / Depth AOV**: 2026-09-21 に Position(XYZ)/UV チャンネルとして実装済み
  (`ChannelType` 拡張 + mesh only-pass mode 9/10 + pipeline target + EXR/Queue/VP 配線)。
  残りは DOF / fog / 2D エフェクトへの depth 参照で Phase 3 の depth/DOF 連携計画と合流。

## 未検証事項

- `ArtifactCoreShapeExtrudeTest` を含む ArtifactCore build/test は、リポジトリ方針により未実行。
- TextExtrude の制作受入（フォント欠落診断、形状品質、再現性、視覚基準）は別途完了条件とする。`experiments/Procedural3DTextExtrude/ACCEPTANCE_PLAN.md` に複雑スクリプト・フォント来歴・ジオメトリ捕捉の受け入れ手順を記録し、未実行ゲートを残す。
- Shape Layer の 3D 表示は現時点では Composition Render Controller の
  `DirectShape3DCard` 経路であり、`Mesh` ownership・再生成・material 契約を持たない。
  押し出しを接続する際はこのカード経路を置換せず、明示的な 3D mesh slice として導入する。

## 対象ファイル

- `ArtifactCore/include/Geometry/ShapeExtrude.ixx` / `ArtifactCore/src/Geometry/ShapeExtrude.cppm`

## 制作受入の追加契約（2026-11-21）

- TextExtrude の `Procedural3DResult` は、成功／空入力／不正設定／欠落グリフ／輪郭なし／不正メッシュ／リソース上限を区別し、解決フォント名、SHA-256、欠落グリフ数を返す。レイヤー JSON に解決フォントと識別スナップショットを保持し、再読込後の差異・未検証状態を Inspector の font row help と debug state に示す。
- 生成時に depth・font size の入力、glyph 数の上限、render mesh の属性整合性、有限値、index 範囲、退化三角形を検証する。
- glyph 上限は `TextExtrudeSettings.maxGlyphCount`（既定 4,096、ハード上限 16,384）として保存し、Shape Geometry プロパティから変更できる。shape 前の UTF-16 長とshape後の glyph 数の両方を制限し、レイヤー入力では上限超過を黙って切り詰めず診断可能な状態で保持する。改行のみの入力に対しても行数を最大4,096に制限する。古い JSON は既定値で復元する。
- glyph 数だけでは制限できないフォント輪郭の計算量に対して、`QPainterPath` 要素 1,000,000、平坦化輪郭 4,096、輪郭点合計 250,000 のハード上限を設けた。上限超過は `ResourceLimitExceeded` と診断する。
- `ShapeExtrude` 実行前に、輪郭点合計と実効ベベル分割数からソース頂点／面数と `Mesh::generateRenderData()` の三角形コーナー展開数を保守的に見積もり、各 1,000,000 を超える入力を `ResourceLimitExceeded` で拒否する。
- 段落内の左／中央／右揃え、tracking、行送り倍率を TextExtrude 設定・レイヤー JSON・既存 Geometry プロパティへ追加した。`QGlyphRun::stringIndexes()` と source string から欠落 glyph の UTF-16 offset と Unicode code point を特定し、font row help に先頭8件を表示する。
- 自動契約には ASCII 正常系、空入力、不正 depth、glyph 上限超過、カウンター字形 `O`・`B`・`8`・`&` の閉じた edge incidence とベベル付き `O` を加えた。TextExtrude の LOD は bevel 分割数へ反映し、テキストメッシュは同じ revision／品質のフレーム間で再生成しない。UI／GPU のビルド・テストをまだ実行していないため、この追記は実装済みの宣言ではなく受入対象の記録である。
- 複雑スクリプトの形状テストは、要求した family が実際の shaped run に使われたことと missing-glyph がないことを確認する。ホスト側のフォント差し替え・未対応は部分メッシュ成功として扱わず、要求／解決 family を示して個別 skip する。
- 対応フォントで生成できた複雑スクリプトの三角形についても、有限UV・単位法線・index 範囲・退化面なしを検査する。
- 新しいレイアウト設定・診断キーを除去した旧 TextExtrude JSON の読み戻し契約を追加し、tracking／leading／alignment／glyph budget の既定値とテキスト値を確認する。
- 診断の生成・外部JSON読み込みの両方で、family/fingerprint各64件、family 256文字・fingerprint 512文字、欠落位置128件・各128文字、欠落glyph数16,384を上限とする。切り詰め時はfont identityまたは位置情報の完全性を未検証へ落とす。
- headless OBJ capture hook は隣接 JSON sidecar に入力・設定・shaped run family・Qt/OS・UTC時刻・artifact SHA-256 と、指定されたフォントファイル／ライセンス／source commit の出所を記録する。未指定 provenance は `unrecordedFields` に明示し、画像基準とは区別する。
- 残る完了条件: 多重輪郭・self-intersection・RTL / complex-script 字形の閉じ具合と法線を検証する。ライセンス確認済みフォントを使う正面／斜視の固定画像と manifest（font hash、入力、設定、Qt／OS、取得日、出所）を登録し、Windows と別対応環境の出力を比較する。font row help の実表示、再読込後の fingerprint mismatch と missing glyph 表示を実行確認する。
