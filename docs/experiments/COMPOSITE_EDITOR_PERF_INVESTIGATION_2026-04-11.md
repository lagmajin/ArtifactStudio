# CompositeEditor パフォーマンス調査レポート — 2026-04-11

## 概要

CompositeEditor 周りの操作（マウス操作、ズーム、パン、レイヤー選択、プロパティ変更）が異常に遅い問題について、
レンダリングパイプライン全体を4カテゴリに分けて調査を行った。

**調査対象ファイル:**
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` — レンダリング制御（4000行超）
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` — エディタ UI
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` — ウィジェット入力処理
- `Artifact/src/Widgets/Render/TransformGizmo.cppm` — ギズモ操作
- `Artifact/src/Render/PrimitiveRenderer2D.cppm` — GPU 2D プリミティブ描画
- `Artifact/src/Render/ArtifactIRenderer.cppm` — レンダラーファサード
- `Artifact/src/Render/ShaderManager.cppm` — シェーダー管理
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm` — レイヤーモデル
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm` — コンポジションモデル

---

## カテゴリ 1: GPU レンダリング基盤 (`PrimitiveRenderer2D`)

### 1-1. 毎描画コールごとの Map/Unmap — 重大度: CRITICAL

**場所:** `PrimitiveRenderer2D.cppm` 全描画関数

**問題:** すべての draw 関数が個別に頂点バッファ → 定数バッファ(色) → 定数バッファ(トランスフォーム) を
Map → memcpy → Unmap する。バッチ描画ではなく1プリミティブ1回の Map/Unmap。

**影響:** 典型的なシーンで 100+ プリミティブ → 300+ Map/Unmap ペア/フレーム。
各 Map は CPU-GPU 同期を伴い、パイプラインストールの原因になる。

**推奨:** リングバッファまたは永続マッピング (`MAP_FLAG_NO_OVERWRITE`) を使用し、
フレーム単位でバッファ領域を確保して一括書き込み。

---

### 1-2. 毎描画コールごとの SetRenderTargets — 重大度: HIGH

**場所:** `PrimitiveRenderer2D.cppm` 全描画関数（drawRectLocal, drawSolidRectTransformed, drawLine, drawThickLine, drawCircle, drawSprite 等）

**問題:** 各 draw 関数の冒頭で `SetRenderTargets(1, &pRTV, ...)` を呼び出す。
同じ RTV に対して 1 フレームで 25 回以上呼ばれる。

**推奨:** フレーム開始時に 1 回 `SetRenderTargets` を呼び、描画中は省略する。
RTV 変更時のみ再設定。

---

### 1-3. RESOURCE_STATE_TRANSITION_MODE_TRANSITION の過剰使用 — 重大度: HIGH

**場所:** `PrimitiveRenderer2D.cppm` 全体（60+ 箇所）

**問題:** すべての `SetRenderTargets`, `SetVertexBuffers`, `SetIndexBuffer`,
`CommitShaderResources` が `RESOURCE_STATE_TRANSITION_MODE_TRANSITION` を使用。
これは毎回リソースステートの検証と遷移を発生させる。

**推奨:** 初回遷移後は `RESOURCE_STATE_TRANSITION_MODE_VERIFY` または `_NONE` を使用。
特にバッファは一度バインドすれば状態変更は不要。

---

### 1-4. GetVariableByName の文字列検索 — 重大度: MEDIUM

**場所:** `PrimitiveRenderer2D.cppm` 各描画関数内

**問題:** `srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")` のような名前ベースの
シェーダー変数ルックアップが毎描画コールで実行される。内部的に O(n) の文字列比較。

**推奨:** PSO/SRB 作成時に `IShaderVariable*` ポインタをキャッシュし、描画時は直接使用。

---

## カテゴリ 2: レンダリングコントローラ (`ArtifactCompositionRenderController`)

### 2-1. 毎フレームの巨大な文字列キー生成 — 重大度: CRITICAL

**場所:** `ArtifactCompositionRenderController.cppm` line 3043-3069

```cpp
const QByteArray baseRenderKey =
    QByteArray("comp=") + comp->id().toString().toUtf8() +
    "|baseSerial=" + QByteArray::number(baseInvalidationSerial_) +
    "|frame=" + QByteArray::number(framePos) +
    "|size=" + QByteArray::number(...) + "x" + QByteArray::number(...) +
    "|downsample=" + QByteArray::number(...) +
    "|zoom=" + QByteArray::number(zoom, 'f', 4) +
    "|pan=" + QByteArray::number(...) + "," + QByteArray::number(...) +
    // ... さらに 15+ の連結操作
```

**問題:** 毎フレーム 25+ の `QByteArray::number()` 変換と `operator+` 連結を実行。
各 `operator+` が新規アロケーションとコピーを発生させる。
これは変更検出（dirty check）のためだが、文字列比較は最もコストの高い方法。

**推奨:** 数値フィールドを構造体にまとめ、`memcmp` またはハッシュで比較:
```cpp
struct RenderKeyState {
    QUuid compId; uint64_t baseSerial; int frame;
    int width, height, downsample; float zoom, panX, panY;
    int bgMode; bool gpuBlend; // ...
};
bool changed = memcmp(&currentKey, &lastKey, sizeof(RenderKeyState)) != 0;
```

---

### 2-2. 毎フレームの背景キー文字列フォーマット — 重大度: HIGH

**場所:** `ArtifactCompositionRenderController.cppm` line 3023-3028

```cpp
const QString backgroundKey =
    QStringLiteral("%1,%2,%3,%4")
        .arg(comp->backgroundColor().r(), 0, 'f', 4)
        .arg(comp->backgroundColor().g(), 0, 'f', 4)
        .arg(comp->backgroundColor().b(), 0, 'f', 4)
        .arg(comp->backgroundColor().a(), 0, 'f', 4);
```

**問題:** 浮動小数点→文字列変換を 4 回、`QString::arg` 置換を 4 回、毎フレーム実行。
比較のためだけに文字列化している。

**推奨:** `FloatColor` の 4 float をビット比較、または `uint32_t` にパックして整数比較。

---

### 2-3. QElapsedTimer + markPhaseMs — 重大度: MEDIUM

**場所:** `ArtifactCompositionRenderController.cppm` line 2897-2904

```cpp
QElapsedTimer frameTimer;
frameTimer.start();
auto markPhaseMs = [&]() -> qint64 { ... };
```

**問題:** 毎フレーム `QElapsedTimer` をスタックに構築し、ラムダ内で `nsecsElapsed()` を
6+ 回呼び出す。プロファイリング用だが本番でも動作する。

**推奨:** `#ifdef ARTIFACT_PROFILING` またはカテゴリガードで囲むか、コンパイル時に除去。

---

### 2-4. 毎フレームの qInfo パフォーマンスログ — 重大度: MEDIUM

**場所:** `ArtifactCompositionRenderController.cppm` line 4016-4036

```cpp
if (frameMs >= 16) {
    qInfo() << "[CompositionView][Perf]"
            << "frameMs=" << frameMs << "pipelineEnabled=" << pipelineEnabled
            << "layersTotal=" << layers.size()
            // ... 15+ のストリーム演算
}
```

**問題:** 16ms を超えるフレーム（≈60fps 未満）のたびに `qInfo()` でログ出力。
QSize の一時オブジェクト生成を含む。遅いフレームほどログ出力でさらに遅くなる悪循環。

**推奨:** `qCDebug(compositionViewLog)` にダウングレードし、カテゴリ無効時はゼロコスト化。
または N フレームに 1 回のみ出力。

---

### 2-5. toQImage() の毎フレーム呼び出し — 重大度: HIGH

**場所:** `ArtifactCompositionRenderController.cppm` line 829, 845, 854, 866, 894

```cpp
const QImage img = solidImage->toQImage();        // line 829
const QImage img = imageLayer->toQImage();         // line 845
const QImage svgImage = svgLayer->toQImage();      // line 854
const QImage frame = videoLayer->currentFrameToQImage(); // line 866
const QImage textImage = textLayer->toQImage();    // line 894
```

**問題:** 各レイヤータイプごとに毎フレーム `toQImage()` を呼び出す。
Image/SVG/Text レイヤーは内容が変わっていなくても毎フレーム QImage を再生成している可能性がある。

**推奨:** レイヤー変更フラグ (`isDirty()`) に基づくキャッシュ。
変更がなければ前フレームの GPU テクスチャをそのまま再利用。

---

### 2-6. Solid2D のラスタライザーエフェクト時の QImage 一時生成 — 重大度: MEDIUM

**場所:** `ArtifactCompositionRenderController.cppm` line 811-816

```cpp
if (hasRasterizerEffectsOrMasks(layer)) {
    const QSize surfaceSize(...);
    QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied); // 毎フレーム
    surface.fill(toQColor(color));
    applySurfaceAndDraw(surface, localRect, false);
}
```

**問題:** エフェクト付きソリッドレイヤーは毎フレーム QImage をアロケート → fill → GPU アップロード。

**推奨:** 色が変わっていなければキャッシュ。サイズ固定のプールバッファを再利用。

---

## カテゴリ 3: UI ウィジェット・イベント処理

### 3-1. mouseMoveEvent 内の全レイヤーヒットテスト — 重大度: HIGH

**場所:** `ArtifactCompositionRenderWidget.cppm` line 273-280

```cpp
const auto layers = comp->allLayer();  // ベクタコピー
for (int i = (int)layers.size() - 1; i >= 0; --i) {
    if (!layers[i] || !layers[i]->isVisible()) continue;
    if (layers[i]->transformedBoundingBox().contains(cPos.x, cPos.y)) {
        hitLayer = layers[i];
        break;
    }
}
```

**問題:** マウス移動のたびに `allLayer()` でレイヤーリスト全体をコピーし、
逆順に `transformedBoundingBox()` を計算。`transformedBoundingBox()` は内部で
`getGlobalTransform()` を呼び、親チェーンを再帰的に辿る。

**影響:** 50 レイヤー × 60fps × 深さ 5 = 15,000 回/秒のトランスフォーム計算。

**推奨:**
1. `allLayer()` の戻り値を `const&` に変更してコピー回避
2. グローバルトランスフォームをフレーム単位でキャッシュ
3. 空間インデックス（QuadTree）の導入

---

### 3-2. TransformGizmo の getGlobalTransform 重複計算 — 重大度: HIGH

**場所:** `TransformGizmo.cppm` line 962 (hitTest), line 642 (draw), line 1140 (drag start)

**問題:** `hitTest()` は `mouseMoveEvent` から毎回呼ばれ、`getGlobalTransform()` を計算。
直後に `draw()` でも同じ計算が行われる。ドラッグ開始時にも再計算。
1 フレームで同じレイヤーに対して 3 回計算される可能性がある。

**推奨:** フレーム単位のトランスフォームキャッシュを導入。
`uint64_t frameSerial` で有効期限を管理。

---

### 3-3. layer->changed() シグナルのドラッグ中連発 — 重大度: HIGH

**場所:** `TransformGizmo.cppm` line 1306, 1332, 1360, 1386

```cpp
Q_EMIT layer_->changed();  // mouseMoveEvent 内、ドラッグ中の毎回
```

**問題:** ギズモドラッグ中、マウス移動ごとに `changed()` シグナルが発火。
これが接続先の全スロット（オーバーレイ更新、レイヤーパネル更新、タイムライン更新等）を
カスケード的にトリガーする。

**推奨:** ドラッグ中は `changed()` を抑制し、ドラッグ終了（mouseRelease）時に 1 回だけ発火。
リアルタイムプレビューが必要なら `transformPreviewChanged()` のような軽量シグナルを別途定義。

---

### 3-4. wheelEvent のタイマー競合 — 重大度: MEDIUM

**場所:** `ArtifactCompositionRenderWidget.cppm` line 480

```cpp
impl_->wheelRenderTimer_->start(16);  // 毎 wheelEvent で再起動
```

**問題:** 高速スクロール時、wheelEvent が 5-10ms 間隔で発生し、毎回 16ms タイマーを再起動。
ただし `renderOneFrame()` が直接呼ばれる場合もあり、タイマーとの二重レンダリングの可能性。

---

### 3-5. resizeEvent のデバウンス無効化 — 重大度: MEDIUM

**場所:** `ArtifactCompositionEditor.cppm` line 615-617

```cpp
resizeDebounceTimer_->start(160);  // 160ms デバウンス
controller_->renderOneFrame();      // ← しかし即座にレンダリング
```

**問題:** デバウンスタイマーを設定した直後に `renderOneFrame()` を呼んでおり、
デバウンスの目的（連続リサイズ時の負荷軽減）を無効化している。

---

## カテゴリ 4: データモデル（Layer / Composition）

### 4-1. getGlobalTransform() の再帰計算（キャッシュなし）— 重大度: CRITICAL

**場所:** `ArtifactAbstractLayer.cppm` line 500-507

```cpp
QTransform ArtifactAbstractLayer::getGlobalTransform() const {
    QTransform local = getLocalTransform();
    auto parent = parentLayer();          // → composition_->layerById() ハッシュ検索
    if (parent) {
        return combineLayerTransform2D(local, parent->getGlobalTransform()); // 再帰
    }
    return local;
}
```

**問題:** 呼び出しのたびに親チェーンを再帰的に辿り、各ステップで `layerById()` ハッシュ検索を実行。
深さ N の階層で O(N) の再帰 + O(N) のハッシュ検索。
1 フレームで複数回呼ばれるため、実質 O(N²) になる。

**推奨:** `cachedGlobalTransform_` + `globalTransformDirty_` フラグを導入。
レイヤーのローカルトランスフォーム変更時に自身と全子孫の dirty フラグを立て、
次回アクセス時のみ再計算。

---

### 4-2. LayerDirtyFlag::All の過剰使用 — 重大度: HIGH

**場所:** `ArtifactAbstractLayer.cppm`

| 行 | メソッド | 本来の dirty flag |
|----|---------|-----------------|
| 299-300 | `setGuide()` | `LayerDirtyFlag::Visibility` のみ |
| 307-308 | `setSolo()` | `LayerDirtyFlag::Visibility` のみ |
| 315-316 | `setLocked()` | UI のみ（レンダリング不要） |
| 1532 | `setOpacity()` | `LayerDirtyFlag::Opacity` のみ |

**問題:** `LayerDirtyFlag::All` はトランスフォーム、エフェクト、マスク、ソースすべてを
無効化する。ロック状態の変更でもフルレンダリングが走る。

**推奨:** 適切な粒度の dirty flag を使用。UI 専用プロパティはレンダリング無効化をスキップ。

---

### 4-3. allLayer() のベクタコピー — 重大度: MEDIUM

**場所:** `ArtifactAbstractComposition.cppm` — `allLayer()` メソッド

**問題:** `allLayer()` が `std::vector<shared_ptr>` のコピーを返す。
レンダリングループ内で毎フレーム呼ばれ、shared_ptr の参照カウント操作が N 回発生。

**推奨:** `const std::vector<...>&` を返すか、`std::span` を使用。

---

### 4-4. goToFrame() の全レイヤー反復 — 重大度: MEDIUM

**場所:** `ArtifactAbstractComposition.cppm` line 207-210

```cpp
for (auto& layer : layerMultiIndex_) {
    if (layer) layer->goToFrame(frame);
}
```

**問題:** フレーム移動時に非表示・非アクティブレイヤーも含めて全レイヤーに `goToFrame` を呼ぶ。

**推奨:** アクティブなレイヤーのみに限定 (`isActiveAt(frame)`)。

---

### 4-5. サムネイル生成のキャッシュなし — 重大度: LOW

**場所:** `ArtifactAbstractLayer.cppm` line 1442-1452

**問題:** `getThumbnail()` が毎回 `QImage` を新規アロケートして fill する。
キャッシュなし。レイヤーパネルの表示更新で頻繁に呼ばれる可能性。

---

## 優先度別サマリー

### CRITICAL（即時対処推奨）

| # | 問題 | 影響度 | 修正難易度 |
|---|------|-------|----------|
| 2-1 | renderKey の文字列連結（毎フレーム） | フレームごとに 25+ alloc | 低（構造体比較に変更） |
| 4-1 | getGlobalTransform の再帰（キャッシュなし） | O(N²)/フレーム | 中（dirty flag 導入） |
| 1-1 | Map/Unmap が毎描画コール | 300+ sync/フレーム | 高（リングバッファ化） |

### HIGH（早期対処推奨）

| # | 問題 | 影響度 | 修正難易度 |
|---|------|-------|----------|
| 3-1 | mouseMoveEvent の全レイヤーヒットテスト | O(N) × 60/sec | 中 |
| 3-3 | ドラッグ中の changed() シグナル連発 | UI カスケード | 低 |
| 2-2 | 背景キー文字列フォーマット | 4 float→string/frame | 低 |
| 2-5 | toQImage() の毎フレーム呼び出し | 大量 alloc | 中（キャッシュ） |
| 4-2 | LayerDirtyFlag::All の過剰使用 | 不要な全再描画 | 低 |
| 1-2 | SetRenderTargets の重複呼び出し | 25+/frame | 低 |
| 1-3 | RESOURCE_STATE_TRANSITION の過剰使用 | 60+/frame | 中 |
| 3-2 | getGlobalTransform の重複計算 | 3x/layer/frame | 低（キャッシュ）|

### MEDIUM（計画的対処）

| # | 問題 | 影響度 | 修正難易度 |
|---|------|-------|----------|
| 1-4 | GetVariableByName の文字列検索 | 30+/frame | 低 |
| 2-3 | QElapsedTimer の常時起動 | 軽微 | 低 |
| 2-4 | qInfo パフォーマンスログ | 遅延フレーム悪化 | 低 |
| 2-6 | Solid2D の QImage 一時生成 | alloc/frame | 中 |
| 3-4 | wheelEvent タイマー競合 | 二重レンダリング | 低 |
| 3-5 | resizeEvent デバウンス無効化 | 連続リサイズ負荷 | 低 |
| 4-3 | allLayer() ベクタコピー | N × refcount | 低 |
| 4-4 | goToFrame() 全レイヤー反復 | 不要な計算 | 低 |

---

## 推奨される修正順序

1. **renderKey を構造体比較に変更** — 最も簡単で効果大
2. **ドラッグ中の changed() シグナル抑制** — UI 応答性に直結
3. **LayerDirtyFlag の粒度改善** — 不要な再描画の大幅削減
4. **getGlobalTransform キャッシュ** — 全体的なフレームタイム改善
5. **qInfo ログのダウングレード** — 遅延フレームの悪化防止
6. **背景キーのビット比較化** — 簡単な修正
7. **toQImage キャッシュ導入** — レイヤー描画の根本改善
8. **Map/Unmap のリングバッファ化** — GPU パイプライン効率化（大規模リファクタ）

---

## 適用済み修正 (2026-04-11)

以下の修正を実施済み。ビルド未検証（pwsh 未インストール環境のため）。

### ✅ 2-1. renderKey を構造体比較に変更
**ファイル:** `ArtifactCompositionRenderController.cppm`
- `QByteArray` 連結（25+ 数値変換/フレーム）を `RenderKeyState` 構造体 + `operator==` に置換
- `lastFinalPresentKey_`（QByteArray）を `lastRenderKeyState_`（struct）に変更
- `invalidateBaseComposite()` / `invalidateOverlayComposite()` はシリアル番号のインクリメント + 構造体リセット
- 注: `CompositionID` / `LayerID` が pImpl ベースのため `memcmp` ではなく `operator==` を使用

### ✅ 2-2. 背景キーの文字列フォーマット廃止
**ファイル:** `ArtifactCompositionRenderController.cppm`
- `QString::arg` による `backgroundKey` フォーマットを `FloatColor` コンポーネント直接比較に変更
- `lastBackgroundKey_` (QString) を `lastBgColorCache_` (FloatColor) に置換

### ✅ 2-4. qInfo パフォーマンスログのダウングレード
**ファイル:** `ArtifactCompositionRenderController.cppm`
- 毎フレームの `qInfo` を `qCDebug(compositionViewLog)` に変更
- スワップチェインサイズ変更ログも `qCDebug` に変更

### ✅ 3-5. resizeEvent のデバウンス無効化を修正
**ファイル:** `ArtifactCompositionEditor.cppm`
- デバウンスタイマー起動直後の `renderOneFrame()` 呼び出しを削除
- リサイズ中の不要な再描画を防止

### ✅ 4-2. LayerDirtyFlag の粒度改善
**ファイル:** `ArtifactAbstractLayer.ixx` / `ArtifactAbstractLayer.cppm`
- `Visibility = 1 << 4`、`Property = 1 << 5` を追加
- `setGuide()` / `setSolo()` / `setLocked()` / `setShy()` → `Visibility` フラグに変更
- `setOpacity()` → `Property` フラグに変更

### 未対応（残タスク）
- 1-1. Map/Unmap のバッチ化 — 大規模リファクタリング必要
- 2-3. QElapsedTimer/markPhaseMs のガード追加
- 2-5. toQImage キャッシュ
- 3-1. mouseMoveEvent ヒットテスト最適化
- 3-2. getGlobalTransform キャッシュ
- 3-3. ドラッグ中シグナル抑制 — Qt シグナル/スロット将来廃止予定のため保留
- 4-3. allLayer() の参照返し
