# バグ修正レポート: コンポジション描画レイヤー非表示・外側グレー問題

**調査・修正日**: 2026-04-09  
**対象ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`  
**修正担当**: Copilot

---

## 症状

1. **コンポジション領域が塗り潰されない** — レイヤーが描画されても常にフラットなグレーにしか見えない
2. **コンポジット領域の外側が薄いグレーのまま** — コンポジション外の背景色が意図した暗い色に見えない

---

## 根本原因調査

### 仮説 A: `drawCompositionRegionOverlay` がオーバーレイフェーズで bgColor を上書きしている（★主犯）

#### 証拠

`drawCompositionRegionOverlay()` 関数（旧実装, line ~863-864）に以下の行が存在していた:

```cpp
// ✅ コンポジション背景色を最初に不透明塗りつぶし
renderer->drawSolidRect(0.0f, 0.0f, cw, ch, comp->backgroundColor(), 1.0f);
```

この関数は **オーバーレイフェーズ（line ~3685）** に呼び出されている。
描画順序は以下の通り:

```
1. renderer_->clear()         ... main FB を clearColor_ で初期化
2. [GPU] accum に bgColor 描画
3. [GPU] レイヤーを accum にブレンド
4. [GPU] accum を SRC_ALPHA で main FB にブリット → レイヤー込みで表示
5. ...safe margins, gizmo, guides...
6. drawCompositionRegionOverlay()  ← ★ ここで bgColor を不透明で上書き
```

ステップ 6 でコンポジション矩形全体に `bgColor` を不透明（opacity=1.0）で塗りつぶすため、
**ステップ 4 でブリットされたレイヤーコンテンツが完全に隠れる**。

#### 結果
ユーザーに見える画面は「フラットな bgColor（中程度グレー）の矩形」のみとなり、レイヤーが存在しても常に不可視状態になる。

---

### 仮説 B: GPU accum パスがボーダーアウトラインを accum RT に描画している（副次バグ）

#### 証拠

GPU パス開始時（旧 line ~3028）:

```cpp
renderer_->setOverrideRTV(accumRTV);
renderer_->setClearColor({0,0,0,0});
renderer_->clear();
drawCompositionRegionOverlay(renderer_.get(), comp);  // ← accum に描画中
```

`drawCompositionRegionOverlay` は:
1. `drawSolidRect(bgColor)` — accum に背景色（正しい）
2. `drawRectOutline(...)` × 2 — accum に**ボーダーラインも描画**（誤り）

ボーダーラインは「コンポジションの枠線」として最終的な画面に表示するための UI 要素であり、
accum（レイヤーブレンド結果）に含めるべきではない。
accum に含まれると、ボーダーの色がレイヤーコンテンツと SRC_ALPHA ブレンドされてしまう。

---

### 仮説 C: main FB への二重 bgColor 描画（冗長）

GPU パス main FB 描画時に:
1. `drawCompositionCheckerboard()` (main FB)
2. `drawSolidRect(bgColor, 1.0f)` (main FB) ← 不要
3. accum blit (SRC_ALPHA) — accum にはすでに bgColor が含まれる

accum の SRC_ALPHA blit が完全に main FB の bgColor を上書きするため、ステップ 2 は冗長。
また、bgColor が半透明（`alpha < 1.0`）の場合、main FB と accum の両方から bgColor が適用され、
半透明コンポジションの見え方が想定より不透明になるという副作用もある。

---

### 仮説 D: 外側グレーについての考察

`clearColor_` はコンストラクタ内で `currentDCCTheme().backgroundColor` から初期化される。
StudioStyle（デフォルト）では `#24272D`（RGB 0.141, 0.153, 0.176）。

外側が「薄いグレー」に見える原因として以下を検討:
- **A: bgColor ≈ clearColor_** だった旧状態（bgColor = 0.10, clearColor_ = 0.12）→ 既に bgColor = 0.47 に修正済み
- **B: テーマが DefaultQt** の場合 `backgroundColor = #F0F0F0`（薄いグレー）→ ユーザー設定依存
- **C: 仮説 A の修正後** に bgColor（0.47 = 中程度グレー）と clearColor_（0.14 = 暗い）が視覚的に区別できるようになることで、相対的に外側が「濃い」と認識できるようになる見込み

---

## 修正内容

### Fix 1 — `drawCompositionRegionOverlay` から `drawSolidRect(bgColor)` を削除 ★主要修正

```cpp
// Before:
void drawCompositionRegionOverlay(ArtifactIRenderer *renderer, ...) {
  // ...
  renderer->drawSolidRect(0.0f, 0.0f, cw, ch, comp->backgroundColor(), 1.0f); // ← 削除
  renderer->drawRectOutline(...);
  renderer->drawRectOutline(...);
}

// After: ボーダーアウトラインのみ描画。背景塗りつぶしはバックグラウンドフェーズで実施。
void drawCompositionRegionOverlay(ArtifactIRenderer *renderer, ...) {
  // ...
  renderer->drawRectOutline(...);
  renderer->drawRectOutline(...);
}
```

**期待効果**: オーバーレイフェーズでレイヤーコンテンツが上書きされなくなる。

---

### Fix 2 — GPU accum パスをボーダーなしに変更

```cpp
// Before:
drawCompositionRegionOverlay(renderer_.get(), comp);  // bgColor + borders が accum に入る

// After: bgColor のみ accum に描画。ボーダーは overlay フェーズで main FB に描画。
renderer_->drawSolidRect(0.0f, 0.0f, cw, ch, bgColor, 1.0f);
```

**期待効果**: ボーダーアウトラインが accum RT に混入しなくなる。

---

### Fix 3 — GPU main FB の冗長な bgColor 描画を削除

```cpp
// Before:
if (showCheckerboard_) { drawCompositionCheckerboard(renderer_.get(), comp); }
renderer_->drawSolidRect(0.0f, 0.0f, cw, ch, bgColor, 1.0f); // ← 削除（accum blit で上書きされるため冗長）

// After:
if (showCheckerboard_) { drawCompositionCheckerboard(renderer_.get(), comp); }
// bgColor は accum 内で描画済み。accum の SRC_ALPHA blit で適切に合成される。
```

**期待効果**: 半透明 bgColor のコンポジションで二重適用が起きなくなる。

---

## 修正後の描画順序

### GPU パス

```
1. renderer_->clear(clearColor_)           main FB 全体を暗い背景色でクリア
2. setOverrideRTV(accum); clear({0,0,0,0}) accum を透明にクリア
3. drawSolidRect(bgColor) → accum         コンポジション矩形に背景色
4. 各レイヤー → layerRTV → blend → accum  レイヤーを accum にブレンド
5. [main FB] drawCheckerboard             チェッカーボード（透過時に透けて見える）
6. drawSpriteTransformed(accum) → main FB SRC_ALPHA blit → bgColor + レイヤーが main FB に出る
7. [overlay] drawCompositionRegionOverlay ボーダーアウトラインのみ（★ 塗りつぶしなし）
8. drawViewportGhostOverlay               ドラッグ/スケール時のみ
```

### Fallback パス

```
1. renderer_->clear(clearColor_)
2. drawCheckerboard → main FB
3. drawSolidRect(bgColor) → main FB
4. 各レイヤー直接描画 → main FB
5. [overlay] drawCompositionRegionOverlay ボーダーのみ
```

---

## 影響範囲

| ファイル | 変更箇所 | 影響 |
|---------|---------|------|
| `ArtifactCompositionRenderController.cppm` | `drawCompositionRegionOverlay` ヘルパー | bgColor 塗りつぶし削除 |
| `ArtifactCompositionRenderController.cppm` | GPU パス accum セットアップ (~L3029) | `drawCompositionRegionOverlay` → `drawSolidRect(bgColor)` |
| `ArtifactCompositionRenderController.cppm` | GPU パス main FB 背景フェーズ (~L3118) | 冗長な `drawSolidRect(bgColor)` 削除 |

---

## 未解決事項

- DefaultQt テーマ利用時は `clearColor_` = `#F0F0F0`（薄いグレー）となり、コンポジション外背景も薄いグレーになる。これはテーマ設定依存の仕様であり、別途ダークテーマをデフォルトとする対処が必要な場合はユーザー設定で解決すること。
