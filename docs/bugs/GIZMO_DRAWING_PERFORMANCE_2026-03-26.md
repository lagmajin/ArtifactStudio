# ギズモ描画パフォーマンス問題 (2026-03-26)

## 症状

コンポジットエディタが重い。原因はレイヤー描画ではなくギズモ描画。

## ボトルネック: drawThickLineLocal のフル GPU サイクル

**場所:** `PrimitiveRenderer2D.cppm:658` (drawThickLineLocal)

各 `drawThickLineLocal` 呼び出しが毎回フル GPU パイプライン:
1. `SetRenderTargets(1, &pRTV, ...)` — RTV バインド
2. `MapBuffer` — 頂点データ書き込み
3. `MapBuffer` — 定数バッファ書き込み
4. `SetPipelineState` — パイプライン切替
5. `CommitShaderResources` — SRB コミット
6. `SetVertexBuffers` — VB バインド
7. `Draw` — 描画

→ **1本の線で 7回の GPU API 呼び出し**

## 描画呼び出し数の分析

`TransformGizmo::draw()` (line 117-180) の1フレームあたり:

| 要素 | 描画関数 | 呼び出し数 |
|------|---------|----------|
| バウンディングボックス (4辺) | `drawSolidLine` × 4 | 4 |
| 8ハンドル (塗り) | `drawSolidRect` × 8 | 8 |
| 8ハンドル (枠線) | `drawRectOutline` × 8 = 辺4×8 | 32 |
| 回転ハンドル (線) | `drawSolidLine` × 1 | 1 |
| 回転ハンドル (枠円) | `drawCircle(outline)` = bezier4×segment16 | 64 |
| 回転ハンドル (塗り円) | `drawCircle(filled)` = radius/1.0 × 64 | **~128** |
| アンカーポイント | `drawCrosshair` = thickLine4 | 4 |
| **合計** | | **~241 GPU サイクル** |

241 × 7 = **~1700 GPU API 呼び出し/フレーム**（ギズモのみ）

## 最悪の箇所: drawCircle(filled)

**場所:** `PrimitiveRenderer2D.cppm:867-892`

```cpp
void PrimitiveRenderer2D::drawCircle(..., bool fill) {
    if (fill) {
        for (float r = 0.5f; r <= radius; r += 1.0f) {
            drawCircle(x, y, r, color, 1.5f, false); // ← 円周を毎回再描画
        }
        return;
    }
    // outline: 4 bezier segments × 16 thick lines each = 64 GPU cycles
    drawBezierLocal(p0, ..., p1, thickness, color);  // 16 thick lines
    drawBezierLocal(p1, ..., p2, thickness, color);  // 16 thick lines
    drawBezierLocal(p2, ..., p3, thickness, color);  // 16 thick lines
    drawBezierLocal(p3, ..., p0, thickness, color);  // 16 thick lines
}
```

塗り円: `radius/1.0` 回 × 64 本の太い線を個別描画。
`radius = handleSize * 0.2f ≈ 1.2f` でも 2×64 = **128 GPU サイクル**。

## drawBezierLocal のサブディビジョン

**場所:** `PrimitiveRenderer2D.cppm:803-818`

```cpp
void PrimitiveRenderer2D::drawBezierLocal(float2 p0, float2 p1, float2 p2, float2 p3, float thickness, const FloatColor& color) {
    float2 lastPos = p0;
    for (int i = 1; i <= 16; ++i) {
        float t = static_cast<float>(i) / 16.0f;
        float2 currentPos = {
            (1-t)*(1-t)*(1-t)*p0.x + 3*(1-t)*(1-t)*t*p1.x + 3*(1-t)*t*t*p2.x + t*t*t*p3.x,
            ...
        };
        drawThickLineLocal(lastPos, currentPos, thickness, color); // ← 毎回フル GPU サイクル
        lastPos = currentPos;
    }
}
```

4次ベジェ1本 = 16 回の `drawThickLineLocal` = 16 × 7 = **112 GPU API 呼び出し**。

## drawCrosshair のオーバーヘッド

**場所:** `PrimitiveRenderer2D.cppm:894-905`

```cpp
void PrimitiveRenderer2D::drawCrosshair(float x, float y, float size, const FloatColor& color) {
    drawThickLineLocal(..., 3.0f, shadow);  // shadow X
    drawThickLineLocal(..., 3.0f, shadow);  // shadow Y
    drawThickLineLocal(..., 1.0f, color);   // main X
    drawThickLineLocal(..., 1.0f, color);   // main Y
}
```

4本の太い線 = 4 × 7 = **28 GPU API 呼び出し**。

## 対策案

### 対策1: drawCircle(filled) を drawSolidCircle に置き換え (最重要)

ファン型三角形で1回の描画呼び出しに:
```cpp
void PrimitiveRenderer2D::drawSolidCircle(float cx, float cy, float radius, const FloatColor& color) {
    constexpr int segments = 32;
    // Fan: center + segments triangles → 1回の Draw
    TriangleVertex vertices[segments * 3];
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * M_PI * i / segments;
        float a1 = 2.0f * M_PI * (i+1) / segments;
        vertices[i*3+0] = {cx, cy};
        vertices[i*3+1] = {cx + radius*cos(a0), cy + radius*sin(a0)};
        vertices[i*3+2] = {cx + radius*cos(a1), cy + radius*sin(a1)};
    }
    // 1回の Draw で完了
}
```
→ 128 GPU サイクル → **1 GPU サイクル**

### 対策2: drawRectOutline を1回の描画に統合

現在の `drawRectOutline` は 4辺を個別に `drawSolidLine` で描画。
1本のラインストリップとして1回の描画に:
```cpp
void PrimitiveRenderer2D::drawRectOutlineLocal(...) {
    // 4辺 = 8頂点のラインストリップ → 1回の Draw
}
```
→ 32 GPU サイクル → **1 GPU サイクル**

### 対策3: ギズモのバッチ描画

複数のプリミティブを1つの VB にまとめ、1回の `Draw` で描画。
バウンディングボックス + ハンドル枠 + 回転線 を1バッチに。

### 対策4: drawCrosshair を drawSolidRect で近似

小さな十字は四角の重ね合わせで十分:
```cpp
void drawCrosshair(...) {
    drawSolidRect(x - half, y - thickness/2, size, thickness, color);  // horizontal
    drawSolidRect(x - thickness/2, y - half, thickness, size, color);  // vertical
}
```
→ 4 GPU サイクル → **2 GPU サイクル**

---

## 推奨対応順

| 順序 | 対策 | 効果 | 見積 |
|---|---|---|---|
| 1 | drawCircle(filled) → drawSolidCircle | 最大 (128→1) | 1h |
| 2 | drawRectOutline → 1回描画 | 大 (32→1) | 30min |
| 3 | drawCrosshair → drawSolidRect 近似 | 中 (4→2) | 15min |
| 4 | ギズモバッチ描画 | 中 | 3h |

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/Render/TransformGizmo.cppm` | 117-180 | draw() — ギズモ描画呼び出し |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 658 | drawThickLineLocal — ボトルネック本体 |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 609-655 | drawLineLocal — フル GPU サイクル |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 803-818 | drawBezierLocal — 16回の drawThickLineLocal |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 867-892 | drawCircle — filled で最悪 |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 894-905 | drawCrosshair — 4回の drawThickLineLocal |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 1042 | drawRectOutlineLocal — 4辺個別描画 |
