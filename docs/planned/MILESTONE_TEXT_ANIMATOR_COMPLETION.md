# MILESTONE: Text Animator Completion & Inline Editing

**日付**: 2026-08-04
**最終更新:** 2026-08-08
**現状**: データモデルとエンジンは Core 層で完成（`TextAnimatorEngine`, `RangeSelector`, `WigglySelector`, `AnimatorProperties`, `TextLayoutContract`）。GlyphAtlas + HarfBuzz + SDF の低レベルも完備。ギャップは統合・UI・ビューポート編集のみ。
**目標**: ビューポートインライン編集、Animator Engine の未接続機能の配線、AE 互換の range selector 視覚編集、`textIndex`/`textTotal` 式変数。

## 現状の10ギャップ

| # | ギャップ | 深刻度 | 工数 |
|---|---------|--------|------|
| G1 | `SelectorOrder` / `createOrderMap()` の評価・保存・Inspector 接続 | ソース実装完了（2026-08-08、runtime確認待ち） | 確認待ち |
| G2 | `AnchorPointGrouping`（character/cluster/word/line/paragraph/span/all） | ソース実装完了（2026-08-08、runtime確認待ち） | 確認待ち |
| G3 | **ビューポートインライン編集不在**（カーソル・選択・IME） | 最重要 | ~3-5d |
| G4 | Range selector の視覚ハンドル（start/end/offset ドラッグ） | 先頭Animator・Percentage単位をソース実装（2026-08-08、runtime確認待ち） | 複数Animator選択が残る |
| G5 | 式変数 `textIndex`/`textTotal` + Expression Selector 不在 | 中 | ~2-3d |
| G6 | source text キーフレーム間のグリフ識別子安定化不在 | 中 | ~2-3d |
| G7 | Timeline 上の Animator 専用表示不在 | 中 | ~2-3d |
| G8 | GPU パスのグリフ毎 blur 不在 | 低 | ~1d |
| G9 | Text tool → クリックでテキストレイヤー作成の導線未完成 | 中 | ~1d |
| G10 | HarfBuzz backend 実装が部分的 | 低 | ~2d |

---

## Phase 1: ビューポートインライン編集（G3 + G9）

### 1.1 Text Tool → テキストレイヤー作成導線

**現状**: `ToolType::Text` 選択後、`textToolCandidate_` と `textToolStartCanvas_` / `textToolCurrentCanvas_` でクリック/ドラッグを追跡しているが、テキストレイヤー作成に至っていない。

**実装** (`ArtifactCompositionRenderController.cppm`):

```cpp
// クリック（ドラッグなし）→ クリック位置にテキストレイヤー作成
// ドラッグ → 範囲指定でテキストボックス作成

void handleTextToolRelease(const QPointF& releasePos) {
    float dragDist = QLineF(textToolStartCanvas_, releasePos).length();
    
    if (dragDist < kClickThreshold) {
        // クリック: 既存テキストレイヤーを選択 or 新規作成
        auto* hitLayer = hitTestTextLayer(releasePos);
        if (hitLayer) {
            selectLayer(hitLayer);
            enterInlineEditMode(hitLayer, mapToTextPosition(releasePos));
        } else {
            createTextLayer(releasePos, "");
            enterInlineEditMode(/* new layer */, 0);
        }
    } else {
        // ドラッグ: テキストボックス作成
        QRectF box(textToolStartCanvas_, releasePos);
        createTextLayer(box.topLeft(), "", box.width(), box.height());
    }
}
```

### 1.2 インライン QTextEdit オーバーレイ

**現状**: テキスト編集はモーダル `ArtifactTextEditorDialog` のみ。ビューポート上の直接編集不在。

**実装**: `CompositionView` の上に QTextEdit をオーバーレイ。テキストレイヤーの位置・スケール・回転をカメラ投影して配置:

```cpp
class InlineTextEditor : public QTextEdit {
public:
    void beginEdit(ArtifactTextLayer* layer, const QPointF& clickPos);
    void endEdit(bool commit);
    
    // 文字スタイルをレイヤーから継承
    void syncStyleFromLayer(ArtifactTextLayer* layer);

private:
    // 編集中のレイヤー位置にオーバーレイを追従
    void updateOverlayTransform();
    
    ArtifactTextLayer* editingLayer_ = nullptr;
    QTimer* transformSyncTimer_;  // カメラ移動時にオーバーレイ追従
};
```

ビューポート座標 → オーバーレイ位置の変換:
```cpp
void InlineTextEditor::updateOverlayTransform() {
    // テキストレイヤーの 3D transform からビューポート上の矩形を計算
    QRectF viewportRect = projectCornersToViewport(
        editingLayer_->transform3D(),
        editingLayer_->textBounds()
    );
    
    setGeometry(viewportRect.toRect());
    
    // フォントサイズをビューポートスケールに合わせる
    float viewportScale = viewportRect.width() / editingLayer_->textBounds().width();
    QFont f = font();
    f.setPointSizeF(editingLayer_->fontSize() * viewportScale);
    setFont(f);
}
```

### 1.3 IME 対応

Qt の QTextEdit はデフォルトで IME 対応済み。追加で必要なもの:
- 編集中レイヤーのロック（他の操作をブロック）
- Esc キーで編集破棄
- Enter（修飾なし）= 改行、Ctrl+Enter = 編集確定
- 編集中のレイヤーのリアルタイムプレビュー更新（`textChanged` シグナル→`setSourceText`）

### 1.4 完了条件

- [ ] Text ツールでクリック→既存テキストレイヤー選択+インライン編集開始
- [ ] Text ツールでクリック→何もない場所に新規テキストレイヤー作成
- [ ] Text ツールでドラッグ→テキストボックス範囲指定
- [ ] インライン QTextEdit がテキストレイヤーの位置・サイズに一致
- [ ] 日本語 IME で直接入力可能
- [ ] Esc で編集破棄、Ctrl+Enter で確定

---

## Phase 2: SelectorOrder + AnchorPointGrouping 配線（G1 + G2）

### 2.1 SelectorOrder の配線

**状態:** 2026-08-08 ソース実装完了、runtime確認待ち。`RangeSelector::order` を正規状態として追加し、通常評価と source-aware 評価、selector preview、JSON 保存復元、Inspector の `Order` プロパティへ接続した。既存データは `Natural` を既定値として互換維持する。

`createOrderMap()` の全7モードを selector weight の順位へ変換し、通常評価と source-aware 評価の両方で利用する。

```cpp
// TextAnimatorEngine.cppm の applyAnimator()
void TextAnimatorEngine::applyAnimator(
    GlyphItem* glyphs, int count,
    const RangeSelector& selector,
    const AnimatorProperties& properties,
    float time)
{
    // ... 既存: evaluateSelector → calculateWeight ...
    
    // 新規: Order map を生成し、処理順序を制御
    auto orderMap = createOrderMap(glyphs, count, selector.order, selectorContext);
    
    for (int orderedIdx = 0; orderedIdx < count; ++orderedIdx) {
        int i = orderMap.empty() ? orderedIdx : orderMap[orderedIdx];
        auto& glyph = glyphs[i];
        
        float weight = calculateWeightForGlyph(glyph, selectorResult, selectorContext);
        
        // 既存のプロパティ適用
        if (properties.positionEnabled) {
            // ... オフセット計算 ...
        }
        // ... その他プロパティ ...
    }
}
```

### 2.2 AnchorPointGrouping の実装

**状態:** 2026-08-08 ソース実装完了、runtime確認待ち。Glyphの既存metadataとboundsから7種類のグループアンカーを構築し、scale・rotationの共通アンカー補正、JSON保存復元、Inspectorの `Anchor Grouping` へ接続した。

```cpp
// 新規: TextAnimatorEngine に追加
struct GlyphGroup {
    int startIndex;
    int endIndex;
    QPointF anchorPoint;      // グループのアンカーポイント
    QRectF boundingBox;
};

std::vector<GlyphGroup> computeAnchorGroups(
    const GlyphItem* glyphs, int count,
    AnchorPointGrouping grouping,
    const TextLayoutContract& layout)
{
    std::vector<GlyphGroup> groups;
    
    switch (grouping) {
    case AnchorPointGrouping::Character:
        for (int i = 0; i < count; ++i)
            groups.push_back({i, i+1, glyphs[i].basePosition, glyphs[i].boundingBox});
        break;
        
    case AnchorPointGrouping::Word:
        // スペース/改行で分割
        break;
        
    case AnchorPointGrouping::Line:
        // TextLineRun で分割
        for (auto& line : layout.lines) {
            groups.push_back({line.startGlyphIndex, line.startGlyphIndex + line.glyphCount,
                              line.boundingBox.center(), line.boundingBox});
        }
        break;
        
    case AnchorPointGrouping::Paragraph:
        // 改行で分割
        break;
        
    case AnchorPointGrouping::All:
        groups.push_back({0, count, layout.totalBounds.center(), layout.totalBounds});
        break;
    }
    
    return groups;
}
```

### 2.3 完了条件

- [ ] `Natural`, `Reverse`, `RandomStable`, `CenterOut` の4 order が視覚的に確認できる
- [ ] `Word`, `Line`, `All` grouping が正しいアンカーポイントでスケール/回転を適用

---

## Phase 3: 式変数 + Expression Selector（G5）

### 3.1 textIndex / textTotal 変数の追加

`ExpressionEvaluator` にコンテキスト変数を追加:

```cpp
// ExpressionEvaluator.cppm の evaluate()
// textIndex: 現在評価中のグリフのインデックス (0-based)
// textTotal: 全グリフ数

struct ExpressionContext {
    // ... 既存フィールド ...
    
    // テキストアニメーション用（TextAnimatorEngine から設定）
    std::optional<int> textIndex;
    std::optional<int> textTotal;
};
```

```cpp
// TextAnimatorEngine::applyAnimator() 内:
ExpressionContext exprCtx = createBaseContext(time, frame);
for (int i = 0; i < count; ++i) {
    exprCtx.textIndex = i;
    exprCtx.textTotal = count;
    
    // 式ベースの weight 計算
    if (selector.expressionEnabled) {
        float weight = expressionEvaluator_->evaluateFloat(
            selector.expression, exprCtx
        );
        // weight を適用
    }
}
```

### 3.2 Expression Selector UI

プロパティエディタの RangeSelector セクションに式入力欄を追加:

```cpp
// 既存の RangeSelector プロパティグループに追加
auto* exprEdit = new QLineEdit();
exprEdit->setPlaceholderText("textIndex / textTotal");  // デフォルト式例
connect(exprEdit, &QLineEdit::textChanged, [=](const QString& text) {
    selector->setExpression(text);
});
```

### 3.3 完了条件

- [ ] `textIndex / textTotal * 360` でグリフごとに回転アニメーション
- [ ] `sin(textIndex * 0.5 + time) * 50` で波状オフセット
- [ ] 式入力欄がプロパティエディタに表示され、即時反映

---

## Phase 4: ビューポート Range Selector 視覚編集（G4）

### 4.1 Range Selector ハンドル

**状態:** 2026-08-08、先頭AnimatorのPercentage Selectorについてソース実装完了、runtime確認待ち。既存のweight heatmapにStart／End／Offsetハンドルを重ね、hit testとドラッグを既存property pathへ接続した。複数Animatorの編集対象選択はTimeline専用UIと合わせて残す。

AE 互換: Text レイヤー選択時に、テキスト上に range selector の start/end/offset を表すドラッグ可能なハンドルを表示。

```cpp
// TextGizmo に追加
struct RangeSelectorHandle {
    enum Type { Start, End, Offset };
    Type type;
    int animatorIndex;
    QPointF position;     // ビューポート上の位置
    QRectF hitRect;       // ヒットテスト用矩形
};

class TextGizmo {
    // 既存: box resize handles, heatmap, cluster/line boundaries
    // 新規:
    std::vector<RangeSelectorHandle> selectorHandles_;
    
    void drawSelectorHandles(QPainter& painter, const ArtifactTextLayer* layer);
    int hitTestSelectorHandle(const QPointF& viewportPos);
    void dragSelectorHandle(int handleIdx, const QPointF& delta);
};
```

レイアウト上での位置計算:
```cpp
void TextGizmo::updateSelectorHandles(const GlyphItem* glyphs, int count,
                                       const RangeSelector& selector) {
    // start: selector.start 位置のグリフの左端
    // end: selector.end 位置のグリフの右端
    // offset: start + offset 位置
    
    float totalWidth = glyphs[count-1].basePosition.x() + glyphs[count-1].advance.x();
    
    float startX = selector.start / 100.0f * totalWidth;
    float endX = selector.end / 100.0f * totalWidth;
    float offsetX = selector.offset / 100.0f * totalWidth;
    
    selectorHandles_.push_back({RangeSelectorHandle::Start, 0, QPointF(startX, baseline)});
    selectorHandles_.push_back({RangeSelectorHandle::End, 0, QPointF(endX, baseline)});
    selectorHandles_.push_back({RangeSelectorHandle::Offset, 0, QPointF(offsetX, baseline)});
}
```

### 4.2 完了条件

- [ ] Range selector の start/end/offset がテキスト上に可視ハンドルとして表示
- [ ] ハンドルのドラッグで selector 値がリアルタイム更新
- [ ] オフセットのドラッグでアニメーションプレビューが即時反映

---

## Phase 5: Timeline 統合 + Source Text 識別子（G6 + G7）

### 5.1 Animator プロパティの Timeline 表示

各 Animator の range selector プロパティ（start/end/offset）を Timeline 上に専用トラックとして表示:

```cpp
// ArtifactTimelineTrackPainterView.cppm
void drawTextAnimatorTrack(QPainter& painter, int animatorIndex, 
                            const RangeSelector& selector) {
    // Animator の start/end/offset を timeline 上のバーとして可視化
    QRectF bar = timelineToScreen(selector.start, selector.end, trackY);
    painter.fillRect(bar, animatorColor(animatorIndex));
    
    // offset 矢印
    float offsetX = timelineToScreenX(selector.offset);
    painter.drawLine(offsetX, trackY, offsetX, trackY + trackHeight);
}
```

### 5.2 安定トークン識別子

source text がキーフレームで変化する場合のグリフ対応:

```cpp
// TextShapingBackend の shaping 時に stableTokenId を設定
void populateStableTokenIds(GlyphItem* glyphs, int oldCount, int newCount,
                             const QString& oldText, const QString& newText) {
    // レーベンシュタイン距離に基づくアラインメント
    // 変更されていない文字に同じ stableTokenId を割り当て
    // 追加された文字に新規ID、削除された文字はスキップ
}
```

### 5.3 完了条件

- [ ] Timeline 上に Animator の range selector バーが表示される
- [ ] source text を "Hello"→"Hello World" に変更しても、最初の5グリフの Animator 状態が維持される
- [ ] Timeline 上で selector バーをドラッグするとプロパティ値が更新される

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | Text tool→レイヤー作成導線 |
| P1 | 新規 `Artifact/src/Widgets/Text/InlineTextEditor.cppm` | ビューポート QTextEdit オーバーレイ |
| P1 | `Artifact/src/Widgets/Render/ArtifactCompositionView.cppm` | IME イベント処理 |
| P2 | `ArtifactCore/src/Text/TextAnimatorEngine.cppm` | `createOrderMap()` 呼出追加 |
| P2 | 新規 AnchorPointGrouping 実装 | word/line/paragraph グループ計算 |
| P3 | `ArtifactCore/src/Expression/ExpressionEvaluator.cppm` | `textIndex`/`textTotal` 変数追加 |
| P3 | `Artifact/src/Widgets/PropertyEditor/` | Expression Selector UI |
| P4 | `Artifact/src/Widgets/Render/TextGizmo.cppm` | Range selector ハンドル描画+ドラッグ |
| P5 | `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` | Animator トラック表示 |
| P5 | `ArtifactCore/src/Text/TextShapingBackend.cppm` | stableTokenId ポピュレーション |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: インライン編集 | **P0** | 大 | 最大のUXギャップ。全てのテキスト編集がこれで変わる |
| G1: SelectorOrder 配線 | **P0** | 極小 | 既存コード1行追加だけ |
| P3: 式変数 | **P1** | 中 | 表現力の飛躍的向上 |
| P4: 視覚ハンドル | **P1** | 大 | TextGizmo の主要拡張 |
| G2: AnchorPointGrouping | **P2** | 中 | word/line/paragraph 単位のアニメーション |
| P5: Timeline 統合 | **P2** | 中 | 既存 Timeline 描画の拡張 |
