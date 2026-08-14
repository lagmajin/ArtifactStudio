# MILESTONE: Text Animator Completion & Inline Editing

**日付**: 2026-08-04
**最終更新:** 2026-08-14
**現状**: Coreの設計シミュレーション122件、ArtifactCoreの実テキストスモーク、DX12 GPUの通常文字・日本語描画を確認済み。絵文字grapheme metadataはshapingからglyphへ伝播済みで、DirectWriteのカラーrun取得・alpha texture化・RGBA合成まで診断スモークで確認済み。カラー／ZWJ絵文字のGPU atlas描画は未完了。統合Artifactビルドには既存の壊れたIFC参照が残る。
**目標**: ビューポートインライン編集、Animator Engine の未接続機能の配線、AE 互換の range selector 視覚編集、`textIndex`/`textTotal` 式変数。

## 実験検証スナップショット（2026-08-14）

| 検証対象 | 結果 | 根拠 |
|---|---|---|
| `Text Sample1` のlayout・selector・回転 | ✅ | `ArtifactCoreTextSmoke`、設計テスト |
| 日本語フォントfallback + DX12描画 | ✅ | `gpu_text_japanese.png` |
| emoji modifier / ZWJのcluster grouping | ✅ | 設計テスト122件、`clusterIndex`出力 |
| QPA自動起動 | ✅ | `run_gpu_smoke.ps1` |
| BMP記号 `★` のGPU描画 | ⚠️ | 通常フォント分類へ修正済み、GPU統合再ビルド待ち |
| カラー絵文字のCore atlas生成 | ✅ | DirectWriteカラーrunからRGBA atlasを生成、`colorPreservedGlyphCount=1` |
| カラー／ZWJ絵文字のGPU描画 | ⚠️ | Core atlasとGPU分岐は実装済み。GPUスモークは実行ファイル鮮度ゲートで `stale-binary`、最新ArtifactRender統合ビルド待ち |
| GPU監査の実行ファイル鮮度 | ✅ | `audit_gpu_matrix.ps1` が契約ソースより古い実行ファイルを合格扱いしない |
| DirectWriteカラーrun取得 | ✅ | `directwrite_color_glyph_smoke.exe`、`🧪`で5run／alpha texture確認 |
| DirectWrite runColor + alphaのRGBA合成 | ✅ 診断スモーク | `directwrite_color_glyph.ppm`、93x92合成 |
| GPU glyphのrotation／scale反映 | ✅ source patch | `DiligentImmediateSubmitter` のquad／matrix生成へ `offsetRotation` と `offsetScale` を接続。統合GPU再ビルド待ち |

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

---

## モーションデザイナーペルソナ検証 Todo（2026-08-13追加）

121件の設計テスト、smoke / contract / stress 監査では、基礎Animatorの有限性・選択単位・順序・文字変形・合成・絵文字/合字/改行の静的整合性を確認済み。以下は、ペルソナの可能性マップに対して未接続または未検証の残タスクである。

### 現時点で検証済みの範囲

- [x] `Text1` / `Text Sample1` / 日本語 / 絵文字 / 合字 / 改行 / 長文の静的監査
- [x] 文字・書記素・単語・行・段落・タグ・正規表現の選択契約
- [x] Natural / Reverse / CenterOut / EdgeIn / RandomStable の順序契約
- [x] Position / Scale / Rotation / Opacity / Skew / Tracking / Z / Blur / Stroke / Color の値契約
- [x] Wiggly、seed、Animator stack、有限値、範囲、未知フィールドの監査
- [x] `preview` の選択結果・演算子・タイミング・診断の機械可読出力

ただし、これは設計モデルの証拠であり、実ランタイムでCore設定へ適用された証拠ではない。次の実装段階では、同じ入力をCoreへ渡した結果とこのモデルの差分を比較する。

### P0: Intentから実動作までの検証可能な経路

- [ ] `selection/order/unit/property/from/to/timing/easing/anchor/space/blend/seed` を実際のCore Animator設定へ変換する
- [ ] `Intent → preview → diff → apply → verify` の一連の結果を機械可読で返す
- [ ] `Text1`、`Text Sample1`、日本語、絵文字、合字、改行で同一Intentの再現性を確認する
- [ ] UI、プリセット、AI/APIの3経路で同じ結果になることを比較する
- [ ] 実ランタイムのプレビュー結果とPython設計モデル/Core snapshotの差分監査を行う

### P1: モーション表現の拡張

- [ ] パス、円、螺旋、グリッド、自由曲線に沿った位置/回転/整列を設計する
- [ ] 3D回転、カメラ相対、ビルボード、文字ごとの深度と遮蔽を設計する
- [ ] 物理風の落下、衝突、跳ね、ばらし、再集合を非破壊Animatorとして設計する
- [ ] 音声、音量、ビート、マーカー、外部データを時間入力へ接続する
- [ ] グリフ輪郭・塗りのワイプ、マスク、クリップ、グリフ置換を検討する
- [ ] ばね、慣性、オーバーシュート、減衰、ステップ、量子化を共通時間プリミティブとして整理する

### P1: 文字構造と編集の堅牢性

- [ ] フォント変更、可変フォント軸、サイズ変更、桁増減後の選択/時間/レイアウト再計算を監査する
- [ ] RTL、縦書き、CJK、フォールバックフォント、欠落グリフを検証する
- [ ] 文字列の差し替えで stable token とAnimator状態が意図どおり維持されることを確認する
- [ ] 選択順・時間順・描画順・読み順をUI/APIで別々に表示する
- [ ] Animator合成の加算/置換/乗算/最大/最小/ブレンドと変形順序を差分表示する

### P2: 時短・品質・AI親和性

- [ ] 代表シナリオ（タイトル登場、単語強調、タイプオン、文章入替、ロゴ、音楽同期、物理分解、データ表示、多言語）を固定fixture化する
- [ ] どのパラメータが結果に影響したかを説明する診断情報を提供する
- [ ] 文字列変更時に壊れる範囲、再計算内容、修正候補を提示する
- [ ] プリセットをIntentへ展開し、部分上書きして再保存できるようにする
- [ ] 細かいキーフレーム探索を不要にするコンポーネント編集導線を評価する

### 完了条件

- [ ] P0の5項目を実ランタイムで確認する
- [ ] 代表シナリオfixtureに対して、静的監査と実動作監査の結果が一致する
- [ ] 少なくとも1つのパス系、1つの3D系、1つの物理系表現をIntentから再現する
- [ ] 未対応の表現は、制限・代替手段・将来計画をAPI診断に明示する

### Core実動作検証の環境ブロッカー（2026-08-13）

- [ ] 現行ソース専用のCoreビルドを成立させる
- [ ] `build` / `cmake-build-debug` は `X:\Dev\ArtifactStudio` の旧キャッシュを持つため検証対象外とする
- [ ] `build_j_vs` / `build_j_vs18` は `J:\dev\ArtifactStudio` を参照するが、CMake再構成時にprotobuf/OpenCVとDiligent側Abseilのターゲットが衝突する
- [ ] 隔離構成 `build_text_runtime_vs` でも `absl_strings links to itself` / `absl::abseil_dll not found` が発生し、TextAnimatorのコンパイル・実行へ到達していない
- [ ] 上記の依存構成を整理した後、古い `.obj` / `.lib` を証拠として再利用せず、TextAnimatorを再ビルドする

現在のCore実動作判定は未確認。Python設計モデルの成功をCore成功として扱わない。
