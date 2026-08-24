# MILESTONE: シェイプ機能拡充（SVG品質・ブール演算・複数シェイプ・性能・頂点編集）

**最終更新:** 2026-08-23

ユーザー承認済みの拡充案 6 項目。段階的に実装する。

| Phase | 項目 | 状態 |
|---|---|---|
| A | 1. SVG グラデーション出力 / 2. SVG ストローク属性 | **実装済み (2026-08-23)、ビルド検証待ち** |
| B | 6. キャッシュのフレームキー化（アニメ時の差分再構築） | 未着手 |
| C | 3. Merge Paths（ブール演算オペレータ） | 未着手（設計メモあり） |
| D | 7. キャンバス頂点編集 | 未着手（大型・UI設計要） |
| E | 4. 1レイヤー複数シェイプ（グループコンテンツ） | 未着手（最大・設計レビュー推奨） |

## Phase A — 実装内容（2026-08-23）

### コア（`ArtifactCore`）

- `ShapeTypes.ixx`: `StrokePlacement` enum 追加。`StrokeSettings` に placement / taperStartScale / taperEndScale / isTapered() 追加。`FillSettings` に FillType::Repeating/Mirrored 追加＋gradientStart/End/Angle/Center/Radius フィールド
- `ShapeLayer.cppm`: SVG 出力を `<defs>` ベースのグラデーション対応に拡張
  - `SvgExportContext`（defs 収集 + gradient id キャッシュ）、`gradientFillReference()`（linear/radial を実出力、conic は線形近似、repeating/mirrored は spreadMethod）
  - `elementToSvg()`: gradient fill は `url(#gradN)`、Inside/Outside 配置とテーパー付きストロークは `QPainterPathStroker` による輪郭塗りパスとして出力（Inside=intersected / Outside=subtracted）

### アプリ（`Artifact`）

- `ArtifactShapeLayer::toCoreShapeLayer()`: fill グラデーション全パラメータと stroke align/taper をコア設定へマッピング（中間色縮退を廃止）

### 既知の制限

- テーパーは輪郭化時に均一幅（平均スケールではなく start 幅の輪郭）での近似。可変幅パス生成は将来課題
- Conic グラデーションは SVG 規格外のため線形近似

## Phase C 設計メモ（Merge Paths）

- `ArtifactCore::ShapeOperatorType::Merge` 新設、`ShapeOperator` 派生で mode(Add/Subtract/Intersect/Exclude) 保持
- パスブール演算はコア新規実装が必要（候補: Clipper2 相当の even-odd boolean、または triangulate 前提の領域演算）。`ShapePath` 単位で apply
- アプリ側は既存オペレータ UI 枠組み（createShapeOperator / getLayerPropertyGroups の operator group）に追加するのみ

## Phase D/E メモ

- D（頂点編集）: データモデルは `CustomPathVertex` 済み。CompositionEditor の gizmo/hit-test に vertex handle 層を追加する UI 実装が本体
- E（複数シェイプ）: アプリ単一プリミティブ→コア `ShapeGroup` モデル移行を伴うため設計レビューを推奨。`toCoreShapeLayer()` が変換層の雛形

## 未検証（ビルド検証待ち）

1. Core 変更（ShapeTypes/ShapeLayer.cppm）による ArtifactCore 再ビルド範囲
2. グラデーション fill の SVG 出力をブラウザ/Illustrator で表示確認
3. Inside/Outside stroke の輪郭出力見栄（自己交差パスでの破綻確認）

2026-08-22 のシェイプ領域コードベースウォークで判明した、後日修正予定項目。**P1 は案Aで実装済み（2026-08-23）、P2 も実装済み。ビルド・ランタイム検証は未実施。**

## P1. ベクター（SVG）出力の接続 — 実装済み（案A、2026-08-23）

ユーザー判断により案A（出力をやる）で実装。

- `ArtifactShapeLayer::toCoreShapeLayer()` を新設（`ArtifactShapeLayer.ixx` / `.cppm`）。`nativeShapePaths()` の演算子処理済みパス + fill/stroke 設定（色・幅・cap/join・dash）を コア `ShapeLayer` / `PathShape` に変換する
- `ArtifactRenderQueueService.cppm` の SVG 出力分岐の黒矩形スタブを置き換え。可視かつアクティブなシェイプレイヤーを `allLayerRef()` から収集し、`getGlobalTransformAt(f)` を外部 transform として `SvgFrameExporter::exportLayerToSvg` に渡してフレーム SVG を書き出す
- **既知の制限:** コア `svgStyleString()` は単色のみ対応のため、グラデーション fill/stroke は開始色と終了色の中間色に縮退される。stroke taper / stroke align (Inside/Outside) は未反映。将来拡張は Insight.md 参照

### 未検証（ビルド検証待ち）

1. SVG 出力ジョブが実パス幾何を出すこと（黒矩形が出ないこと）
2. グラデーション fill の中間色縮退の見栄差
3. `import Shape.Layer` 追加によるモジュール依存の再スキャン

## P2. シェイプパラメータのキーフレーム再生経路 — 欠落を確認、実装済み（2026-08-23）

検証の結果、`shape.width` 等の animatable プロパティは**再生・レンダリング経路で評価されていなかった**（transform.* / layer.opacity はアクセサ内で lazy 評価されるが、shape.* はメンバ値を直接返すのみ）。

対応として `ArtifactSolidImageLayer::color()` と同じ lazy 評価パターンを適用:

- `effectiveShapeTimelineTime()` / `animatedShapeNumber()` ヘルパーを新設（composition framePosition + fps から RationalTime を生成）
- **`ShapeGeomDims` 一元化**: `resolveShapeGeomDims()` を唯一のジオメトリ解決入口とし、GPU draw（native operator / soft-body / compatibility cache の全分岐）、ソフト描画 `toQImage()`、`localBounds()`、D3D card points、SVG 出力（`toCoreShapeLayer` → `nativeShapePaths`）のすべてが同一の評価値を使う
- キーフレーム存在時（`hasAnimatedShapeGeometry()`）は該当キャッシュをフレームごとに再構築（nativeGeometry は `cacheable=false`、rebuildCache/bounds/card points は再計算）。キーフレーム無しの場合は従来のキャッシュ経路・メンバ値フォールバックでホットパス影響ゼロ
- 新規 import（`.cppm` 実装側のみ）: `Property.Abstract` / `Artifact.Composition.Abstract` / `Time.Rational`

### 未検証（ビルド検証待ち）

- width キーフレームを打って再生追従を実機確認（GPU path / 互換キャッシュ path の両方）
- 式プロパティ（wiggle 等）との併用時の非影響
- キーフレーム有り時のキャッシュ再構築コスト（大サイズシェイプでのフレームレート影響）

## 関連メモ

- GPU path の非ソリッドフィル（グラデーション等）は QImage 互換キャッシュ経由のフォールバック（設計通り、fallback ログ付き）。P1 を進める際は SVG 側のグラデーション属性マッピングと合わせて扱う。
- `ShapePath::triangulate`（fill rule 対応・キャッシュ付き）とオペレータ5種のソフト/GPU 接続は良好、本計画では変更しない。
