# 3D 選択時ワイヤーフレームオーバーレイ（Maya/Blender ライク）

> 2026-07-06 作成
> スコープ: コンポジションエディタのビューポートで 3D レイヤー選択時にメッシュエッジを重ね描画する

> 2026-07-08 更新: Phase 1 相当の選択時ワイヤーフレーム描画を実装済み。複数選択と性能最適化は Phase 2 以降に継続。

**進捗状態:** Partial（Phase 1 source/static verified 2026-07-25）

## Goal

コンポジションエディタのビューポート上で、3D レイヤー（`Artifact3DLayer` など `is3D()` が true なレイヤー）を選択した際に、Maya や Blender のように **トライアングル・クアッドのエッジ線をメッシュの上に重ね描き** し、編集中のメッシュ構造を直感的に把握できるようにする。

## 現状とギャップ

### 既に完備している土台

| 資産 | 状態 | 部位 |
|------|------|------|
| `Artifact3DLayer`（メッシュデータ保有、Solid/Wireframe 描画） | ✅ | `Artifact/src/Layer/Artifact3DModelLayer.cppm` |
| `ArtifactIRenderer::draw3DLine()`（3D ライン描画） | ✅ | `Artifact/include/Render/ArtifactIRenderer.ixx` |
| `drawSelectionOverlay()`（選択時オーバーレイ描画） | ✅ | `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` |
| 3D カメラ行列（`set3DCameraMatrices`） | ✅ | `ArtifactIRenderer` |
| `Polygon` / `getPolygonVertices()`（メッシュポリゴン構造） | ✅ | `Mesh` モジュール |
| Cloner フレームオーバーレイ / Anchor オーバーレイ（既存オーバーレイ実装パターン） | ✅ | `ArtifactCompositionRenderOverlay.cppm` |

### ギャップ

現在 `drawSelectionOverlay()` は **2D バウンディングボックスの矩形枠線のみ** を描画しており、3D レイヤーでも同様に矩形枠だけが表示される。以下の項目が欠けている：

- 3D レイヤー選択時にメッシュの全ポリゴンエッジをオーバーレイ描画する機能
- トライアングル（3 辺）・クアッド（4 辺 + 対角線）の両方に対応したエッジ抽出
- 選択状態を視覚的に強調する色設計（Maya オレンジ / Blender ハイライト）

## Scope

- `drawSelectionOverlay()` の拡張（3D レイヤー検出とワイヤーフレーム分岐）
- 新規関数 `draw3DSelectionWireframeOverlay()` の追加（宣言 + 実装）
- メッシュポリゴンからのエッジ抽出と `draw3DLine()` による描画
- 選択色の設計（FloatColor で定義）
- `is3D()` 判定による汎用性（`Artifact3DLayer` 以外の 3D レイヤー型も将来的に対応可能）

## 実装状況

- `drawSelectionOverlay()` から 3D レイヤーの選択時ワイヤーフレーム描画へ接続済み
- 三角形ポリゴンの外周エッジと、四角形ポリゴンの対角線を含むエッジ描画を実装済み
- 暗い下地線 + オレンジ本線の 2 段描画で視認性を強化済み
- 重複エッジは `visitedEdges` で抑制済み
- 複数選択、LOD、エッジ共有の完全最適化は未着手

## Phases

### Phase 1: 選択時ワイヤーフレームオーバーレイ描画（M-3DWO-1〜2）

**目標**: 3D レイヤー選択時にメッシュエッジがビューポートに表示される

- `M-3DWO-1` **ワイヤーフレームオーバーレイ描画関数の実装**
  - `draw3DSelectionWireframeOverlay(ArtifactIRenderer*, const ArtifactAbstractLayerPtr&)` を新設
  - レイヤーの `is3D()` チェック
  - メッシュから全ポリゴンを走査し、三角形（3 辺）・四角形（4 辺 + 対角線 1 本）のエッジを抽出
  - レイヤーのグローバル変換行列を適用した頂点位置で `draw3DLine()` を呼び出し
  - 選択色: `FloatColor{1.0f, 0.55f, 0.15f, 0.92f}`（Maya ライクなオレンジ）
  - 線の太さ: 2.0f（固定値、将来的にズーム連動可）
  - `#include <Layer/Artifact3DModelLayer.ixx>` の追加

- `M-3DWO-2` **drawSelectionOverlay への統合**
  - 既存の `drawSelectionOverlay()` 末尾（2D バウンディングボックス描画後）に分岐を追加
  - `if (layer->is3D()) { draw3DSelectionWireframeOverlay(renderer, layer); }`
  - 2D レイヤー選択枠と 3D ワイヤーフレームの同時表示（共存）

### Phase 2: 複数選択・パフォーマンス最適化（M-3DWO-3〜4）

**目標**: 複数 3D レイヤー選択時も快適に動作し、ハイポリメッシュでもパフォーマンスを確保

- `M-3DWO-3` **複数選択への対応**
  - 複数 3D レイヤー選択時に各レイヤーごとにワイヤーフレームを描画
  - 選択レイヤーごとに異なる色相で差別化（任意）
  - `CompositionRenderController` 側の選択ループとの整合確認

- `M-3DWO-4` **パフォーマンス最適化**
  - ハイポリメッシュ（10 万ポリゴン超）向けの間引き描画（LOD）
  - エッジ重複除去（隣接ポリゴン間の共有エッジを 1 回だけ描画）
  - 描画バッチ最適化（`flushGizmo3D()` の活用検討）
  - 必要に応じて頂点バッファのキャッシュ（メッシュ不変時）

### Phase 3: 発展機能（M-3DWO-5〜7）

**目標**: Maya/Blender 相当の表現力と操作性の強化

- `M-3DWO-5` **非選択時のホバーハイライト**
  - マウスホバー中の 3D レイヤーに半透明ワイヤーフレームを表示
  - 色: `FloatColor{0.8f, 0.85f, 0.9f, 0.35f}`（淡いハイライト）
  - ピック判定の統合（レイキャストとの組み合わせ）
  - ホバー状態の視覚フィードバックで操作性向上

- `M-3DWO-6` **頂点/エッジハイライト**
  - サブオブジェクトモード（頂点/エッジ/面選択）導入時に拡張可能な設計
  - 選択頂点に小さな球体マーカー描画
  - 選択エッジの太線ハイライト

- `M-3DWO-7` **表示切替オプション**
  - 設定パネルでのワイヤーフレームオーバーレイ ON/OFF 切替
  - キーボードショートカット（例: `Ctrl+Shift+W` = ワイヤーフレーム表示切替）
  - 選択色のカスタマイズ

## 依存関係

```
M-3DWO-1（ワイヤーフレーム描画関数）
 └─ M-3DWO-2（drawSelectionOverlay 統合）

M-3DWO-3（複数選択対応）
 └─ M-3DWO-1, M-3DWO-2

M-3DWO-4（パフォーマンス最適化）
 └─ M-3DWO-1, M-3DWO-3

Phase 3（発展機能）
 ├─ M-3DWO-5（ホバーハイライト）
 ├─ M-3DWO-6（頂点/エッジハイライト）
 └─ M-3DWO-7（表示切替オプション）
```

## 既存マイルストーンとの関係

| 既存 | 関係 |
|------|------|
| `MILESTONE_3D_GIZMO_IMPLEMENTATION_2026-03-25.md` | 3D ギズモ描画と同じ `draw3DLine()` API を使用。本マイルストーンはギズモではなく**メッシュ構造の可視化**に焦点。 |
| `MILESTONE_COMPOSITION_EDITOR_IMPLEMENTATION_RULES_2026-04-13.md` | コンポジションエディタのオーバーレイ描画ルールに準拠。 |
| `MILESTONE_PRIMITIVE3D_RENDER_PATH_2026-03-21.md` | 3D プリミティブのレンダリングパス。ワイヤーフレームオーバーレイはこの上に重ね描きされる。 |

## 事前調査: 3Dモデルレイヤーがビューポートで描画されない原因分析

`ArtifactCompositionRenderController.cppm` の `drawLayerForCompositionView()`（5694行目）を入念に解析。
ワイヤーフレームオーバーレイの実装前に、3Dレイヤー自体が描画されない場合の原因を特定するためのチェックリスト。

### 描画パイプライン概要

```
CompositionRenderController::drawLayerForCompositionView()
  ├─ 5744行: localRect = layer->localBounds() をチェック
  │   └─ width/height <= 0 → return（3D判定にすら到達しない）
  ├─ 5734行: parent->isGroupLayer() → return
  ├─ 5770行: if (layer->is3D()) → 3D専用パス
  │   ├─ 5784行: if (cameraView && cameraProj) → set3DCameraMatrices()
  │   │   └─ cameraView/cameraProj が nullptr なら set3DCameraMatrices() 未呼出し
  │   └─ 5793行: layer->draw(renderer)
  └─ 5817行: return（3Dパス終了）
```

### 原因 1（最も可能性が高い）: カメラレイヤー不在による3D行列未設定

- **19610〜19628行**: `activeCamera` はコンポジション内の `ArtifactCameraLayer` を線形走査して発見する
- カメラレイヤーがコンポジションに1つも無い → `has3DCamera = false`
- **7908〜7910行**: `cameraViewMatrix` / `cameraProjMatrix` が `nullptr` で渡される
- **5784行**: `if (cameraView && cameraProj)` が偽 → `set3DCameraMatrices()` 未呼出し
- **結果**: `layer->draw(renderer)` は呼ばれるが、view/proj行列がデフォルト（Identity）のためメッシュが画面外に飛ぶ、または全く見えない
- **確認方法**: レイヤーパネルに Camera レイヤーが存在するか確認する

### 原因 2: `localBounds()` が無効な矩形を返す

- **5744〜5758行**: `localRect.width() <= 0.0 || localRect.height() <= 0.0` の場合、**3D判定（5770行）にすら到達せず即return**
- `Artifact3DLayer::localBounds()`（719行目）は `sourceSize()` に依存
- `updateSourceSizeFromMesh()` がメッシュ読込前に呼ばれていない場合、`sourceSize` は (0,0) の可能性
- **確認方法**: プロパティエディタで3Dレイヤーの width/height/depth が 0 でないか確認する

### 原因 3: 親レイヤーがグループレイヤー

- **5734〜5740行**: `parent->isGroupLayer()` が true の場合、即 `return`
- グループ内の3Dレイヤーはこのパスで描画されず、グループ描画に委譲されるがグループ側が3D非対応の可能性
- **確認方法**: 3Dレイヤーがグループフォルダ内に入っていないか確認する

### 原因 4: `isVisible()` が false / 不透明度が 0

- `Artifact3DLayer::draw()` の652行目で `!isVisible()` → 即リターン
- **確認方法**: レイヤーパネルで目アイコンがONかつ opacity > 0% であることを確認する

### 原因 5: RenderMode の状態

- `Artifact3DLayer::draw()` で `renderMode_` が `Solid` の場合 `drawMesh()`（Diligent GPU経由）を使用
- `Wireframe` モードの場合 `draw3DLine()` を使用
- GPUパイプライン（シェーダー/PSO）の初期化に失敗している場合、Solidモードでは何も描画されない
- Wireframeモードに切り替えることでGPUパイプラインの問題を回避できる可能性
- **確認方法**: プロパティエディタで Render Mode を Wireframe に設定し、表示されるかテストする
- **参考**: `Artifact3DModelLayer.cppm` 84行目: デフォルトは `RenderMode::Wireframe`

### 優先度別チェックリスト

| 優先度 | 確認項目 | 確認方法 |
|--------|---------|---------|
| **P0** | コンポジションに `ArtifactCameraLayer` が存在するか | レイヤーパネルで Camera レイヤーの有無を目視 |
| **P0** | 3Dレイヤーの `sourceSize` が (0,0) でないか | プロパティエディタで Width/Height/Depth の値を確認 |
| **P1** | 3Dレイヤーがグループ内に入っていないか | レイヤーツリーの階層構造を確認 |
| **P1** | 不透明度 > 0 / 可視性 ON | レイヤーパネルの目アイコンと opacity スライダー |
| **P2** | Render Mode を Wireframe にして表示されるか | プロパティ > 3D Render > Render Mode = Wireframe |
| **P2** | シェーダー/PSO の初期化ログにエラーがないか | ビルド出力 / デバッグログを確認 |

## 対象ファイル

```
Artifact/include/Widgets/Render/ArtifactCompositionRenderOverlay.ixx
  └─ draw3DSelectionWireframeOverlay() 宣言追加
  └─ 必要なインクルード追加

Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm
  └─ draw3DSelectionWireframeOverlay() 実装
  └─ drawSelectionOverlay() 内に 3D 分岐追加
  └─ #include <Layer/Artifact3DModelLayer.ixx> 追加

（将来的な拡張対象）
Artifact/include/Layer/ArtifactAbstractLayer.ixx
  └─ 3D レイヤー向け meshData() 仮想関数の追加検討
```

## 成功基準

- [x] コンポジションエディタで 3D レイヤー選択時にメッシュの全エッジがオレンジ系で重ね描画される
- [x] 三角形ポリゴンは 3 辺、四角形ポリゴンは 4 辺 + 対角線が描画される
- [x] 2D 選択枠（バウンディングボックス）と 3D ワイヤーフレームが同時に表示される
- [ ] カメラ操作（回転/ズーム/パン）に追従してワイヤーフレームも正しく投影される
- [ ] 選択解除でワイヤーフレームが消える
- [ ] 既存のオーバーレイ描画（Cloner フレーム、Anchor など）と競合しない
- [ ] ハイポリメッシュでもフレームレートが著しく低下しない（目標: 60fps 維持）
