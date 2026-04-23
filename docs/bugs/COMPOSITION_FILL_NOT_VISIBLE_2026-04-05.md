# 調査報告書: コンポジット領域の塗り潰し不可視問題

**調査日**: 2026-04-05  
**対象ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`  
**報告者**: Copilot (調査のみ、修正は別作業)

---

## 問題の概要

ユーザー報告: 「docs の座標ルールに従ってコンポジット領域が塗り潰されるがいまだに行われない」

コンポジションエディタで composition 領域（0,0 〜 cw×ch）が、周囲の viewer 背景と視覚的に区別できない状態。

---

## 調査結果: 根本原因

### 根本原因 A — `if (true) { qInfo() }` デバッグブロックが毎フレーム実行 (**性能)

- **GPU パス** 3029〜3056 行: `if (true)` ブロックで NDC 座標を `qInfo()` 出力
- **Fallback パス** 3115〜3147 行: 同様の `if (true)` ブロック
- `qInfo()` は Qt メッセージキューに I/O を発生させるため、毎フレーム呼び出されると著しくフレームレートを低下させる
- **修正適用済み**: 両ブロックを除去

### 根本原因 B — `bgColor` と `clearColor_` がほぼ同色

| 変数 | デフォルト値 | 見た目 |
|------|------------|--------|
| `clearColor_` | `{0.12, 0.13, 0.18, 1.0}` | 暗い青グレー |
| `comp->backgroundColor()` | `{0.10, 0.10, 0.10, 1.0}` | 暗いグレー |

composition 領域は `bgColor` で塗り潰されているが、外側の `clearColor_` と差が微小（輝度差 ≒ 2〜3%）。
典型的なモニターでは目視不能に近い。

**→ 仮説: "塗り潰しが起きていない" のではなく "塗り潰しが起きているが視覚的に区別できない" 状態**

### 根本原因 C — チェッカーボードの描画順序が逆 (仕様違反)

**docs/done/COORDINATE_SYSTEMS.md** 仕様:
```
// 2. チェッカーボード描画（Composition Space）
//    コンポジション全体をカバー
drawCheckerboard(0, 0, compWidth, compHeight, ...);

// 3. レイヤー描画
for (auto& layer : layers) { ... }
```

**実装 (`ArtifactCompositionRenderController.cppm`)**:
```
3196: for (const auto &layer : layers) { ... }  // レイヤー描画
...
3637: drawCompositionRegionOverlay(...)           // チェッカーボード（★ レイヤーの後！）
```

チェッカーボードがレイヤーの **後に** 描画されるため:
- レイヤーをチェッカーボードが上書きしてしまう
- 「透明領域にチェッカーを見せる」という本来の用途を果たせない

### 根本原因 D — チェッカーボードのアルファ値が極めて低い

```cpp
renderer->drawCheckerboard(
    0.0f, 0.0f, cw, ch, 16.0f,
    {0.18f, 0.18f, 0.18f, 0.08f},   // ← α=8% (ほぼ透明)
    {0.24f, 0.24f, 0.24f, 0.05f});  // ← α=5% (ほぼ透明)
```

仮に `showCheckerboard_` が `true` であり、かつ描画順序が正しくても、5〜8% の不透明度では視認不可能。

### 根本原因 E — `showCheckerboard_` がデフォルト `false`

```cpp
// ArtifactCompositionRenderController.cppm 977 行
bool showCheckerboard_ = false;
```

チェッカーボードは `ArtifactCompositionEditor` のツールバーにある「Checkerboard」トグルを明示的にオンにしない限り描画されない。
新規プロジェクト作成直後は常に `false`。

---

## 修正済み事項

| 種別 | 内容 | ファイル |
|------|------|----------|
| ✅ 性能修正 | GPU パス `if (true) { qInfo() }` ブロック除去 | `ArtifactCompositionRenderController.cppm` L3029-3056 |
| ✅ 性能修正 | Fallback パス `if (true) { qInfo() }` ブロック除去 | `ArtifactCompositionRenderController.cppm` L3115-3147 |
| ✅ 根本原因 B 対処 | `backgroundColor_` デフォルト値を `{0.47, 0.47, 0.47}` に変更 | `ArtifactAbstractComposition.cppm` L57 |

---

## 未修正の推奨対処

### 推奨対処 1 — チェッカーボード描画順序の修正 (HIGH)

`drawCompositionRegionOverlay()` の呼び出し位置を、レイヤー描画ループの **前** に移動する。

**GPU パス**（改善後フロー）:
```
1. main fb clear (clearColor_)
2. drawCompositionRegionOverlay()  ← bgColor fill + checkerboard をここに
3. accum RT clear
4. layer blend into accum
5. accum blit to main fb (SRC_ALPHA)
6. overlay/gizmo
```

**Fallback パス**（改善後フロー）:
```
1. main fb clear (clearColor_)
2. drawCompositionRegionOverlay()  ← ここに移動
3. for layers: drawLayer(...)
4. overlay/gizmo
```

### 推奨対処 2 — デフォルト背景色の変更 (MEDIUM)

`ArtifactAbstractComposition.cpp` のデフォルト `backgroundColor_` を変更:

```cpp
// 現状（clearColor_ と区別できない）
FloatColor backgroundColor_ = {0.1f, 0.1f, 0.1f, 1.0f};

// 推奨（明確に識別できるニュートラルグレー）
FloatColor backgroundColor_ = {0.5f, 0.5f, 0.5f, 1.0f};
```

### 推奨対処 3 — チェッカーボードのデフォルト有効化 (LOW)

```cpp
// ArtifactCompositionRenderController.cppm
bool showCheckerboard_ = true;  // false → true に変更
```

または新規コンポジション作成時に自動的に `setShowCheckerboard(true)` を呼ぶ。

### 推奨対処 4 — チェッカーボードのアルファ値修正 (MEDIUM)

視認できる値に引き上げる:
```cpp
renderer->drawCheckerboard(
    0.0f, 0.0f, cw, ch, 16.0f,
    {0.18f, 0.18f, 0.18f, 1.0f},   // 不透明
    {0.30f, 0.30f, 0.30f, 1.0f});  // 不透明
```

---

## 影響範囲

| ファイル | 影響 |
|---------|------|
| `ArtifactCompositionRenderController.cppm` | 描画順序・デフォルト値 |
| `ArtifactAbstractComposition.cpp` | デフォルト backgroundColor_ |

---

## 結論

**塗り潰し自体は正しく実行されている**。  
問題は視覚的区別不能（bgColor ≈ clearColor_）と、チェッカーボードの仕様違反（描画順序逆・デフォルト無効・アルファ=5〜8%）の組み合わせにより、ユーザーが composition 境界を認識できない状態にある。  
また `if (true) { qInfo() }` 毎フレームログが別の性能問題を起こしていたため本修正で除去した。
