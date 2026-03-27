# Milestone: AE風 Text Animator システム (2026-03-25)

**Status:** Not Started
**Goal:** After Effects 風の Text Animator を段階導入。レンジセレクターで文字単位のアニメーションを実現。

---

## Architecture Overview

```
ArtifactTextLayer
  ├── TextStyle / ParagraphStyle          ← 完成
  ├── GlyphLayout::GlyphItem[]            ← M1 で実装
  ├── TextAnimatorEngine[]                ← M2 で接続
  │     ├── RangeSelector                 ← 完成（Core側）
  │     ├── WigglySelector                ← 完成（Core側）
  │     └── AnimatorProperties            ← 完成（Core側）
  └── Per-glyph rendering                 ← M3 で実装
        ├── GlyphItem.position (animated)
        ├── GlyphItem.scale (animated)
        ├── GlyphItem.rotation (animated)
        ├── GlyphItem.opacity (animated)
        └── GlyphItem.color (animated)
```

---

## Existing Infrastructure

| コンポーネント | 状態 | 場所 |
|---|---|---|
| `TextStyle` / `ParagraphStyle` | ✅ 完成 | `ArtifactCore/include/Text/TextStyle.ixx` |
| `FontManager` (QFont ラップ) | ✅ 完成 | `ArtifactCore/include/Font/FreeFont.ixx` |
| `ArtifactTextLayer` (QPainter レンダリング) | ✅ 完成 | `Artifact/src/Layer/ArtifactTextLayer.cppm` |
| `GlyphItem` 構造体 | ✅ 宣言のみ | `ArtifactCore/include/Text/GlyphLayout.ixx` |
| `TextLayoutEngine::layout()` | ⚠️ ヘッダーのみ、実装なし | `GlyphLayout.ixx` |
| `TextAnimatorEngine` (weight計算) | ✅ 完成（Core側） | `ArtifactCore/src/Text/TextAnimator.cppm` |
| `RangeSelector` (6形状) | ✅ 完成 | `ArtifactCore/include/Text/TextAnimator.ixx` |
| `WigglySelector` | ✅ 完成 | `ArtifactCore/include/Text/TextAnimator.ixx` |
| `AnimatorProperties` (pos/scale/rot/opacity) | ✅ 完成 | `ArtifactCore/include/Text/TextAnimator.ixx` |
| `TextGizmo` (レンジ編集UI) | ⚠️ スタブ（ハードコード） | `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm` |
| テキストプロパティパネル | ✅ 18項目 | `ArtifactTextLayer.cppm:173-217` |

### 未接続（Core にあるが Layer で使われていない）
- `GlyphLayout::TextLayoutEngine::layout()` — テキストを GlyphItem[] に分解する関数
- `TextAnimatorEngine::applyAnimator()` — GlyphItem[] にアニメータ適用
- `AnimatorProperties.colorEnabled` / `blur` / `skew` — 宣言のみ、適用ロジックなし

---

## Milestone 1: GlyphLayout Engine（基盤）

**目的:** テキストを文字単位の `GlyphItem[]` に分解し、各グリフの位置・メトリクスを計算する。

### 現状
- `GlyphItem` 構造体は宣言済み: `charIndex`, `position`, `advance`, `bearing`, `glyphRect`
- `TextLayoutEngine::layout()` はヘッダーのみ、**`.cppm` 実装ファイルが存在しない**

### Implementation
1. `ArtifactCore/src/Text/GlyphLayout.cppm` を新規作成
2. `TextLayoutEngine::layout()` を実装:
   - `QFontMetrics` で各グリフの幅・高さ・ベアリング取得
   - `tracking` (文字間隔) を加味してグリフ位置計算
   - `leading` (行間) で行位置計算
   - 改行で行分割
3. `GlyphItem` に追加フィールド:
   - `float localX, localY` — レイアウト後の初期位置
   - `float width, height` — グリフバウンディングボックス
   - `int lineIndex` — 行番号

### 見積
| タスク | 見積 |
|---|---|
| GlyphLayout.cppm 新規作成 | 1h |
| TextLayoutEngine::layout() 実装 | 4h |
| QFontMetrics によるグリフメトリクス取得 | 2h |
| tracking/leading/alignment 対応 | 2h |
| テスト（日本語・英語混在） | 2h |

### Acceptance Criteria
- `"Hello"` → 5個の GlyphItem が正しい位置に返される
- `tracking` を変えるとグリフ間隔が変わる
- 改行が正しく処理される
- 日本語（全角）も正しくレイアウトされる

---

## Milestone 2: TextAnimatorEngine 接続

**目的:** Core 側の `TextAnimatorEngine` を `ArtifactTextLayer` に接続し、レンジセレクターで文字単位アニメーションを有効にする。

### 現状
- `TextAnimatorEngine::applyAnimator()` は完成済み（weight 計算 → GlyphItem にオフセット適用）
- `ArtifactTextLayer` は `GlyphItem[]` を持たず、`TextAnimatorEngine` も持たない

### Implementation
1. `ArtifactTextLayer` の Impl に追加:
   - `std::vector<GlyphItem> glyphs_`
   - `std::vector<TextAnimatorEngine> animators_`
   - `bool perGlyphMode_ = false`（アニメータが1つ以上ある場合に true）
2. `updateImage()` の分岐:
   - アニメータなし → 既存のフラット QPainter レンダリング（高速パス）
   - アニメータあり → GlyphItem[] でループし個別描画
3. `GlyphItem` へのアニメータ適用フロー:
   ```
   TextLayoutEngine::layout(text, style) → glyphs
   for each animator in animators:
       animator.applyAnimator(glyphs, frame)
   renderGlyphs(glyphs) → QImage
   ```
4. AnimatorProperties の未実装項目を補完:
   - `colorEnabled` / `fillColor` → GlyphItem に color フィールド追加、適用ロジック
   - `blur` → GlyphItem に blur フィールド追加、個別ブラー
   - `skew` → transform に追加

### 見積
| タスク | 見積 |
|---|---|
| Impl に glyphs_ / animators_ 追加 | 1h |
| GlyphItem に color/blur/skew フィールド追加 | 1h |
| TextAnimatorEngine の color/blur/skew 適用実装 | 3h |
| updateImage() の分岐（フラット vs グリフ） | 3h |
| Per-glyph QPainter レンダリング | 4h |
| アニメータ追加/削除の API | 2h |
| キーフレーム連携（AnimatorProperties の時刻依存） | 3h |

### Acceptance Criteria
- テキストレイヤーにアニメータを追加すると RangeSelector が適用される
- position オフセットで文字が個別に動く
- scale/rotation/opacity がグリフ単位で変化する
- アニメータがない場合、既存のフラットレンダリングと同等のパフォーマンス

---

## Milestone 3: Per-Glyph Rendering

**目的:** 各グリフを個別の QImage としてレンダリングし、アニメータ適用後の合成位置に描画する。

### 現状
- テキストは `QTextDocument` で全体を1つの QImage に描画している
- グリフ単位の描画パスが存在しない

### Implementation
1. `renderGlyphs()` メソッドを新規追加:
   - 各 `GlyphItem` に対して `QPainterPath::addText()` で個別グリフパス生成
   - ストローク: `QPainterPath::addText()` + `QPainter::drawPath(stroke)`
   - 塗り: `QPainter::fillPath()`
   - グリフの `position` + アニメータオフセットで配置
2. 最適化:
   - 同じグリフの QImage をキャッシュ（`QHash<QChar, QImage>`）
   - 変化しないグリフはキャッシュから `drawImage`
   - position/opacity のみ変化の場合は再ラスタライズ不要
3. シャドウ: 各グリフに個別シャドウ（重い場合はまとめシャドウにフォールバック）

### 見積
| タスク | 見積 |
|---|---|
| renderGlyphs() 実装 | 4h |
| QPainterPath による個別グリフ生成 | 3h |
| グリフキャッシュ (QHash<QChar, QImage>) | 2h |
| ストローク/塗りの個別描画 | 2h |
| 個別シャドウ | 2h |
| パフォーマンス最適化 | 2h |

### Acceptance Criteria
- アニメータ適用後、各グリフが独立した位置/回転/スケールで描画される
- キャッシュにより、位置アニメーション中は再ラスタライズなし
- 50文字のテキストで Preview 60fps 維持

---

## Milestone 4: TextGizmo インタラクティブ化

**目的:** ビューポート上でレンジセレクターを直接操作する。

### 現状
- `TextGizmo` はスタブ状態（ハードコードされた 2 つのハンドル）
- レイヤーデータへのバインディングが未実装

### Implementation
1. TextGizmo を ArtifactTextLayer のグリフデータにバインド:
   - グリフのバウンディングボックスからハンドル位置を動的計算
   - RangeSelector の start/end をビューポート上の位置にマッピング
2. ハンドルドラッグ → RangeSelector の start/end/offset を更新
3. レンジの視覚表示:
   - 選択レンジ内のグリフをハイライト
   - RampUp/RampDown 形状をビューポート上に可視化
4. キャラクター選択モード:
   - グリフをクリックで RangeSelector の start/end をスナップ

### 見積
| タスク | 見積 |
|---|---|
| TextGizmo のレイヤーデータバインディング | 3h |
| グリフバウンディングボックスからのハンドル位置計算 | 2h |
| RangeSelector start/end ドラッグ処理 | 3h |
| レンジハイライト描画 | 2h |
| 形状可視化（Ramp/Triangle 等） | 2h |
| キャラクター選択スナップ | 2h |

### Acceptance Criteria
- テキストレイヤー選択時にグリフバウンディングボックスが表示される
- レンジハンドルをドラッグして start/end を変更できる
- レンジ内のグリフがハイライト表示される
- ビューポート上の操作が Inspector の値と同期する

---

## Milestone 5: Text Animator UI

**目的:** Inspector でテキストアニメータの追加・編集ができる。

### Implementation
1. Inspector に "Text Animators" セクション追加:
   - "Add Animator" ボタン
   - アニメータリスト（名前、有効/無効）
2. アニメータプロパティグループ:
   - **Range Selector:** start/end/offset/shape/smoothness
   - **Properties:** position(x,y) / scale(x,y) / rotation / opacity / fillColor / blur
   - **Advanced:** mode (add/subtract/intersect) / amount
3. Wiggly Selector 追加:
   - mode / maxAmount / minAmount / correlation / speed / frequency / complexity
4. キーフレーム編集:
   - 各プロパティのストップウォッチ
   - タイムラインにアニメータプロパティを表示

### 見積
| タスク | 見積 |
|---|---|
| Text Animators セクション UI | 3h |
| Range Selector プロパティグループ | 2h |
| Animator Properties グループ | 3h |
| Wiggly Selector グループ | 2h |
| Add/Remove Animator ロジック | 2h |
| キーフレーム連携 | 4h |
| プリセット (Fade Up Characters / Tracking / etc.) | 2h |

### Acceptance Criteria
- Inspector でアニメータを追加・削除できる
- Range Selector の start/end を Inspector で数値入力できる
- 各プロパティにキーフレームを打てる
- プリセット適用で即座にアニメーションが設定される

---

## Milestone 6: Timeline 統合

**目的:** タイムラインにアニメータプロパティを表示し、キーフレーム編集を可能にする。

### Implementation
1. タイムラインにテキストアニメータのプロパティレーン追加:
   - `layer.textAnimator[0].rangeSelector.start`
   - `layer.textAnimator[0].properties.position.x`
   - など
2. 各レーンにキーフレームマーカー表示
3. レーンの表示/非表示切替（折りたたみ）
4. グラフエディタで easing 編集

### 見積
| タスク | 見積 |
|---|---|
| アニメータプロパティレーン生成 | 4h |
| キーフレームマーカー表示 | 2h |
| レーン折りたたみ | 2h |
| グラフエディタ連携 | 4h |

### Acceptance Criteria
- タイムラインにアニメータプロパティが表示される
- キーフレームをドラッグで移動できる
- グラフエディタで easing を編集できる

---

## Milestone 7: 高度な機能

### 7a: Text on Path（テキストパス）
- マスクパスに沿ってテキストを配置
- 各グリフの回転をパスの接線に追従

### 7b: Box Text（段落テキスト）
- 固定幅テキストボックス
- 自動改行

### 7c: Per-character 3D
- Z 軸回転/位置のアニメータ対応
- カメラとのパースペクティブ

### 見積
| タスク | 見積 |
|---|---|
| Text on Path | 8h |
| Box Text | 4h |
| Per-character 3D | 6h |

---

## Deliverables

| ファイル | 内容 | マイルストーン |
|---|---|---|
| `ArtifactCore/src/Text/GlyphLayout.cppm` (新規) | グリフレイアウトエンジン | M1 |
| `Artifact/src/Layer/ArtifactTextLayer.cppm` (拡張) | GlyphItem[] / Animator 接続 | M2 |
| `GlyphLayout.ixx` (拡張) | GlyphItem に color/blur/skew 追加 | M2 |
| `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm` (改修) | インタラクティブ化 | M4 |
| `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` (拡張) | Text Animator UI | M5 |
| `Artifact/src/Widgets/ArtifactTimelineScene.cppm` (拡張) | アニメータレーン | M6 |

---

## Recommended Order

| 順序 | マイルストーン | 見積 | 理由 |
|---|---|---|---|
| 1 | **M1 GlyphLayout** | 11h | 基盤、全ての前提 |
| 2 | **M2 TextAnimatorEngine 接続** | 17h | Core の既存コードを活用、最大の価値 |
| 3 | **M3 Per-Glyph Rendering** | 15h | M2 の結果を視覚化 |
| 4 | **M4 TextGizmo** | 14h | インタラクティブ操作 |
| 5 | **M5 Text Animator UI** | 18h | Inspector 統合 |
| 6 | **M6 Timeline 統合** | 12h | キーフレーム編集 |
| 7 | **M7 高度な機能** | 18h | 拡張機能 |

**総見積: ~105h**

---

## Related Files

| ファイル | 行 | 内容 |
|---|---|---|
| `ArtifactCore/include/Text/TextStyle.ixx` | 25-56 | TextStyle / ParagraphStyle |
| `ArtifactCore/include/Text/GlyphLayout.ixx` | 17-100 | GlyphItem / TextLayoutEngine 宣言 |
| `ArtifactCore/include/Text/TextAnimator.ixx` | 14-91 | RangeSelector / WigglySelector / AnimatorProperties |
| `ArtifactCore/src/Text/TextAnimator.cppm` | 13-88 | weight 計算 / applyAnimator() |
| `Artifact/src/Layer/ArtifactTextLayer.cppm` | 74-665 | テキストレイヤー実装 |
| `Artifact/src/Layer/ArtifactTextLayer.cppm` | 514-665 | updateImage() (QTextDocument レンダリング) |
| `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm` | 23-104 | TextGizmo スタブ |
| `ArtifactCore/include/Font/FreeFont.ixx` | 17-100 | FontManager |
