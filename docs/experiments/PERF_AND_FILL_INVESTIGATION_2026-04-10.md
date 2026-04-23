# 性能調査・コンポジット塗りつぶし手法変更レポート

**調査日**: 2026-04-10  
**対象**: コンポジションエディタ性能 / ソロビュー性能 / コンポジット領域塗りつぶし / レンダーキューマネージャー性能  
**担当**: Copilot (静的解析)

---

## 1. コンポジションエディタのパフォーマンス

### 1-1. `renderOneFrame()` 呼び出し箇所数（38 箇所）

`ArtifactCompositionRenderController.cppm` 内で `renderOneFrame()` が呼ばれている主な箇所：

| 行（概算） | 呼び出し元 | 理由 |
|-----------|-----------|------|
| ~1322 | compositionChanged handler | コンポジション変更 |
| ~1409 | bindCompositionChanged lambda | サーフェスキャッシュクリア後 |
| ~1518, 1535 | setComposition | コンポジション設定/解除直後 |
| ~1604 | setViewportSize | ビューポートサイズ変更 |
| ~1684 | resize debounce timer | ウィジェットリサイズ後 |
| ~1697, 1720 | setZoom / setPan | ズーム・パン変更 |
| ~1786 | layer->changed 接続 | 各レイヤーの変更 |
| ~1816, 1828 | setSelectedLayerId / setClearColor | レイヤー選択・クリアカラー変更 |
| ~1837–2061 | 各種 set* / handle* | 計 ~20 箇所 |
| ~2475 | handleMouseMove (rubber band) | ラバーバンド選択中の**毎 mouseMoveEvent** ← 問題 |
| ~2610, 2668, 2681 | handleMouseMove / Release | ギズモ hover・リリース |
| ~2776 | reschedule (再帰) | 前フレームが描画中だった場合の再スケジュール |

**coalescing の仕組み（L2751–2778）:**

```cpp
if (impl_->renderScheduled_) {
    return;  // 重複呼び出しをスキップ
}
impl_->renderScheduled_ = true;
QTimer::singleShot(scheduleDelayMs, this, [this]() { ... });
```

インタラクション中は `scheduleDelayMs = 16ms` のデバウンスが入り、非インタラクション時は `0ms` で即時実行。38 箇所の呼び出しはほぼ coalescing で 1 フレーム 1 回に収まる。

**問題点:**

- **rubber band 選択中（~L2473–2476）:** `if (impl_->isRubberBandSelecting_)` ブロックで即 `renderOneFrame()` して `return` するため、coalescing をバイパスしている。マウス移動の頻度でそのまま毎イベント再描画が発生する可能性がある。

---

### 1-2. `makeMayaGradientSprite()` が毎フレーム QImage を生成

**ファイル:** `ArtifactCompositionRenderController.cppm` ~L968

背景モードが `MayaGradient` の場合、`drawCompositionBackgroundViewportSpace()` が呼ばれるたびに `makeMayaGradientSprite(bgColor)` で `2×256` の `QImage` をヒープ確保し、GPU テクスチャを毎フレーム生成する。

```cpp
// 毎フレーム実行
const QImage sprite = makeMayaGradientSprite(bgColor);
renderer->drawSpriteTransformed(..., sprite, 1.0f);
```

`drawSpriteTransformed` は `image.cacheKey()` で GPU テクスチャキャッシュを引くが、**毎フレーム新しい `QImage` インスタンス**が渡されるため `cacheKey` が毎回変わり、テクスチャを毎フレーム再作成・再アップロードする。

**修正方向:** `bgColor` が変化しない限りスプライトをメンバ変数にキャッシュする。

```cpp
// Impl メンバに追加
QImage cachedMayaGradientSprite_;
FloatColor cachedMayaGradientBgColor_ = {-1.f, -1.f, -1.f, -1.f};

// drawCompositionBackgroundViewportSpace の MayaGradient 分岐
if (toQColor(bgColor) != toQColor(cachedMayaGradientBgColor_)
    || cachedMayaGradientSprite_.isNull()) {
    cachedMayaGradientBgColor_ = bgColor;
    cachedMayaGradientSprite_ = makeMayaGradientSprite(bgColor);
}
renderer->drawSpriteTransformed(..., cachedMayaGradientSprite_, 1.f);
```

---

### 1-3. デバッグログが毎 60 フレームに無条件出力

**ファイル:** `ArtifactCompositionRenderController.cppm` ~L2794–2796

```cpp
static int renderCount = 0;
if (renderCount++ % 60 == 0) {
    qDebug() << "[CompositionChangeDetector]" << changeDetector_.debugInfo();
}
```

`qDebug()` は Qt のメッセージハンドラを経由するため、Debug ビルドでは毎 60 フレームにログ I/O が発生する。`qCDebug(compositionViewLog)` でカテゴリ制御に移行するか、定常稼働時は削除を推奨。

---

### 1-4. 各レイヤー変更で 4 処理が連鎖

**ファイル:** `ArtifactCompositionRenderController.cppm` ~L1779–1787

```cpp
connect(layer.get(), &ArtifactAbstractLayer::changed, this,
    [this, layer]() {
        impl_->invalidateLayerSurfaceCache(layer);   // 1
        impl_->invalidateBaseComposite();             // 2
        impl_->syncSelectedLayerOverlayState(...);   // 3
        renderOneFrame();                             // 4
    });
```

1 レイヤーの `changed` シグナルが 4 処理を起動する。`syncSelectedLayerOverlayState` は変更がそのレイヤーと無関係でも毎回走る。

**修正方向:** 変更されたレイヤーが選択レイヤーでない場合は `syncSelectedLayerOverlayState` をスキップする条件分岐を追加。

---

## 2. レイヤーソロビューのパフォーマンス

### 2-1. `hasSoloLayer` の計算が毎フレーム O(N)

**ファイル:** `ArtifactCompositionRenderController.cppm` ~L3086–3090

```cpp
// renderOneFrameImpl の先頭（毎フレーム実行）
const bool hasSoloLayer = std::any_of(
    layers.begin(), layers.end(), [](const ArtifactAbstractLayerPtr &l) {
        return l && l->isVisible() && l->isSolo();
    });
```

`layers` は `comp->allLayer()` で取得した全レイヤーリスト（N 個）。GPU パス (~L3170) と Fallback パス (~L3364) の両方で参照される。レイヤー数が多い場合（50 層以上）、毎フレームの線形スキャンが積み重なる。

**修正方向:** `hasSoloLayer_` をメンバにキャッシュし、ソロ状態・可視状態が変化したときのみ再計算する（dirty フラグ方式）。

```cpp
// Impl メンバ
bool hasSoloLayerCache_ = false;
bool soloLayerCacheDirty_ = true;

// layer->changed 接続内（ソロ/可視変更時）
soloLayerCacheDirty_ = true;

// renderOneFrameImpl 先頭
if (soloLayerCacheDirty_) {
    hasSoloLayerCache_ = std::any_of(...);
    soloLayerCacheDirty_ = false;
}
const bool hasSoloLayer = hasSoloLayerCache_;
```

---

### 2-2. ソロ状態切り替え時のシグナル経路の確認が必要

`isSolo()` の変化は `ArtifactLayerSetting::setSolo()` → `layer->changed()` → `renderOneFrame()` 経路で正しく再描画される。  
しかし `setSolo()` から `changed()` が確実に emit されているか、あるいは UI パネルが直接 `project->projectChanged()` を呼ぶケースがないか確認が必要。  
`EVENTBUS_CASCADE_REDRAW_PERF_2026-04-05.md` で記録されたブレンドモード変更時の `ProjectChangedEvent` 全体 broadcast 問題と同様のパターンが、ソロ切り替えでも発生していないか要確認。

---

### 2-3. ソロビュー + 非ソロレイヤーのループスキップは正しく実装済み

GPU パス (~L3174–3177)・Fallback パス (~L3370–3374) 共に、ソロが有効な場合は非ソロレイヤーの描画開始前に `continue` しており、GPU コマンドの発行を抑制できている。既存の実装は正しい。

---

## 3. コンポジット領域の塗りつぶし手法変更

### 3-1. 現在の状態（2026-04-10 時点）

前回調査 `COMPOSITION_FILL_NOT_VISIBLE_2026-04-05.md` の推奨対処はほぼ適用済み：

| 推奨対処 | 状態 |
|---------|------|
| チェッカーボード alpha 修正 (0.05→1.0) | ✅ 適用済み (~L922–924) |
| `drawCompositionRegionOverlay` をアウトラインのみに変更 | ✅ 適用済み (~L883–902) |
| チェッカーボードをレイヤー前に描画 | ✅ GPU: ~L3247–3248 / Fallback: ~L3314–3316 |
| 背景色を viewport-space fill で描画 | ✅ `drawCompositionBackgroundViewportSpace()` (~L944–985) |

### 3-2. 残存する問題：viewport-space 手動変換の不安定性

**ファイル:** `ArtifactCompositionRenderController.cppm` ~L944–985

```cpp
void drawCompositionBackgroundViewportSpace(...) {
    const QRectF rect = compositionViewportRect(renderer, cw, ch);
    // compositionViewportRect は canvasToViewport() で Composition 座標を
    // Viewport 座標に変換する。zoom/pan の状態に依存する。
    renderer->drawRectLocal(rect.left(), rect.top(),
                            rect.width(), rect.height(), bgColor, 1.0f);
}
```

`canvasToViewport()` の変換チェーン（zoom × pan × canvasSize の三重状態管理）に実装ミスがあると、塗りつぶし矩形がずれる・サイズが合わない問題が再現しやすい。特に zoom が変化したフレームや、pan が大きい場合に誤差が出やすい。

### 3-3. 推奨手法変更：Composition Space 直接指定（シェーダー委譲）

**座標ルール根拠（`docs/done/COORDINATE_SYSTEMS.md`）:**

```
// 描画順序
1. clear (clearColor_)
2. チェッカーボード描画 → Composition Space (0, 0, compW, compH)
3. 背景塗りつぶし       → Composition Space (0, 0, compW, compH)
4. レイヤー描画         → Local → Composition → View → NDC（シェーダーが処理）
5. オーバーレイ/ギズモ
```

仕様書の原則通り、**背景塗りつぶしも Composition Space `(0, 0, cw, ch)` で直接指定し、viewport 変換をシェーダーに任せる**実装が正しい。現在の `canvasToViewport()` 経由の手動変換はこの原則に反しており、変換の二重適用リスクがある。

**提案実装（`drawCompositionBackgroundViewportSpace` の差し替え）:**

```cpp
// Composition Space での直接描画
// renderer は既に setCanvasSize(cw, ch) + zoom/pan 設定済みのため、
// Composition Space で drawRectLocal(0, 0, cw, ch) を呼ぶだけでよい。
void drawCompositionBackgroundDirect(ArtifactIRenderer* renderer,
                                     float cw, float ch,
                                     const FloatColor& bgColor,
                                     CompositionBackgroundMode mode,
                                     const QImage& cachedGradientSprite) {
    if (mode == CompositionBackgroundMode::MayaGradient) {
        renderer->drawSpriteTransformed(0.f, 0.f, cw, ch,
                                        QMatrix4x4{}, cachedGradientSprite, 1.f);
    } else {
        renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.f);
    }
}
```

**GPU パスでの呼び出し変更（~L3250–3252）:**

```cpp
// Before:
drawCompositionBackgroundViewportSpace(renderer_.get(), origViewW, origViewH,
                                       cw, ch, bgColor, backgroundMode);
// After:
drawCompositionBackgroundDirect(renderer_.get(), cw, ch,
                                bgColor, backgroundMode,
                                impl_->cachedMayaGradientSprite_);
```

**Fallback パスでの呼び出し変更（~L3317–3320）:**

```cpp
// Before:
drawCompositionBackgroundViewportSpace(renderer_.get(), viewportW, viewportH,
                                       cw, ch, bgColor, backgroundMode);
// After:
drawCompositionBackgroundDirect(renderer_.get(), cw, ch,
                                bgColor, backgroundMode,
                                impl_->cachedMayaGradientSprite_);
```

この変更により `canvasToViewport()` の変換チェーンへの依存がなくなり、zoom/pan 変化時の矩形ズレが解消される。

---

## 4. レンダーキューマネージャーのパフォーマンス

### 4-1. 進捗更新のたびに全リストを再構築（最重要）

**ファイル:** `ArtifactRenderQueueManagerWidget.cpp` ~L615–620, L248–292

```cpp
// jobProgressChanged が発火するたびに呼ばれる
connect(impl_->service, &ArtifactRenderQueueService::jobProgressChanged,
        this, [this](int index, int progress) {
    Q_UNUSED(progress);
    impl_->postQueueChanged(QStringLiteral("Job progress updated")); // ← 毎フレーム
});

void updateJobList() {
    jobListWidget->clear();   // 全アイテム削除
    visibleToSource.clear();
    for (int i = 0; i < jobs.size(); ++i) {
        auto* item = new QListWidgetItem(line); // 毎回アロケーション
        item->setFont(QFont("Consolas", 10));   // 毎回 QFont 生成
        jobListWidget->addItem(item);
    }
    updateSummary();  // progressBar setValue も毎回
}
```

レンダリング中、`jobProgressChanged` はフレームごとに発火する。その都度 `updateJobList()` がリスト全体を `clear()` して再構築する。ジョブが 10 本あれば 10 個の `QListWidgetItem` を毎フレーム削除・生成する。

**修正方向A（インデックス指定で差分更新）:**

```cpp
// jobProgressChanged ハンドラを変更
connect(impl_->service, &ArtifactRenderQueueService::jobProgressChanged,
        this, [this](int index, int progress) {
    impl_->updateJobItemAtIndex(index);  // 対象アイテムのみ更新
    impl_->updateSummary();              // サマリーバー更新
});

void updateJobItemAtIndex(int index) {
    if (index < 0 || index >= jobListWidget->count()) {
        updateJobList();  // 追加・削除時のみフル再構築
        return;
    }
    auto* item = jobListWidget->item(index);
    if (item) {
        item->setText(buildJobLine(index));
        item->setForeground(jobTextColor(index));
    }
}
```

**修正方向B（タイマーによる throttle）:**

```cpp
// 30ms デバウンスタイマーで最大 ~33fps に制限
if (!updateTimer_->isActive())
    updateTimer_->start(30);
```

---

### 4-2. GPU フレームごとに `qInfo()` + `pixelColor()` がデバッグログを出力

**ファイル:** `ArtifactRenderQueueService.cppm` ~L2940–2954

```cpp
impl_->gpuRenderer_->flush();
qimg = impl_->gpuRenderer_->readbackToImage();
if (!qimg.isNull()) {
    const QColor topLeft = qimg.pixelColor(0, 0);
    const QColor center  = qimg.pixelColor(w/2, h/2);
    qInfo().nospace() << "[RenderQueue][GPU] frame=" << f
                      << " topLeft=(...) center=(...)";  // 毎フレーム文字列フォーマット + I/O
}
```

`qInfo()` は Qt のメッセージハンドラを経由するため、毎フレーム文字列フォーマット + I/O が発生する。30〜60 fps で数十 ms/秒の純粋なオーバーヘッドになる。

**修正:** このブロックを削除するか、`qCDebug(renderQueueLog)` でカテゴリ制御に移行する。

---

### 4-3. ヘッドレス GPU レンダラーが毎フレーム同期 flush + readback

**ファイル:** `ArtifactRenderQueueService.cppm` ~L2939–2940

```cpp
impl_->gpuRenderer_->flush();           // GPU コマンド完了まで CPU がブロック
qimg = impl_->gpuRenderer_->readbackToImage();  // staging 転送 + CPU マップ
```

`flush()` → `readbackToImage()` は同期 GPU→CPU 転送であり、1 フレームごとに GPU パイプラインを完全に止めて CPU がコピーを受け取る（`RENDER_QUEUE_PERF_BOTTLENECK_HYPOTHESES_2026-04-02.md:H1` で既出）。

**修正方向（中長期）:**
- GPU サブミットを N フレーム先行させ、まとめて readback する（ダブルバッファリング）
- または readback を別スレッドで実行し、GPU が描画継続できるようにする

---

### 4-4. `QFont` の毎呼び出し生成

**ファイル:** `ArtifactRenderQueueManagerWidget.cpp` ~L254

```cpp
void updateJobList() {
    QFont fixedFont("Consolas", 10);  // updateJobList() 呼び出し毎に生成
    for (...) {
        item->setFont(fixedFont);
    }
}
```

`Impl` のメンバとして一度だけ生成すること（コンストラクタ内）。

---

## 5. 優先度別まとめ

| 優先度 | 問題 | ファイル | 行（概算） | 期待効果 |
|--------|------|---------|-----------|---------|
| 🔴 CRITICAL | jobProgressChanged で全リスト再構築 | RenderQueueManagerWidget.cpp | ~L615, L248–292 | 大：UI フリーズ解消 |
| 🔴 CRITICAL | GPU ループ内で qInfo() が毎フレーム出力 | ArtifactRenderQueueService.cppm | ~L2946–2954 | 大：I/O 除去 |
| 🔴 HIGH | 塗りつぶしを Composition Space で直接描画（canvasToViewport 除去） | ArtifactCompositionRenderController.cppm | ~L944–985, ~L3250, ~L3317 | 大：fill ズレ完全解消 |
| 🔴 HIGH | makeMayaGradientSprite() 毎フレーム QImage 生成 | ArtifactCompositionRenderController.cppm | ~L968 | 中：テクスチャ再アップロード除去 |
| 🟡 MEDIUM | hasSoloLayer を毎フレーム O(N) 計算 | ArtifactCompositionRenderController.cppm | ~L3086–3090 | 中（レイヤー数次第）|
| 🟡 MEDIUM | GPU readback が毎フレーム同期ストール | ArtifactRenderQueueService.cppm | ~L2939–2940 | 大（変更コスト高）|
| 🟡 MEDIUM | rubber band 選択中 renderOneFrame が coalescing をバイパス | ArtifactCompositionRenderController.cppm | ~L2473–2476 | 中 |
| 🟢 LOW | layer->changed で syncSelectedLayerOverlayState が常に実行 | ArtifactCompositionRenderController.cppm | ~L1784 | 小 |
| 🟢 LOW | QFont を毎 updateJobList() 生成 | RenderQueueManagerWidget.cpp | ~L254 | 小 |
| 🟢 LOW | デバッグログが 60 フレーム毎に出力（Debug ビルド）| ArtifactCompositionRenderController.cppm | ~L2794–2796 | 小 |

---

## 付記: 既に修正済みの既知問題

以下は過去の調査・修正で解決済みのため本レポートには含めない：

- `if (true) { qInfo() }` デバッグブロック（COMPOSITION_FILL_NOT_VISIBLE: 根本原因 A）
- `flushAndWait()` の毎フレーム同期（コメントアウト済み）
- チェッカーボード alpha 値 (0.05 → 1.0 修正済み)
- ブレンドモード変更の `project->projectChanged()` 伝播（EVENTBUS: 修正 1 適用済み）
- `addLayer` の `notifyProjectMutation` 削除（EVENTBUS: 修正 2 適用済み）
