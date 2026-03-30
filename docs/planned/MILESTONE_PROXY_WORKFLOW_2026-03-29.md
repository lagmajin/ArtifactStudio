# Milestone: Proxy Workflow (2026-03-29)

**Status:** Not Started
**Goal:** 重いフッテージの低解像度プロキシを作成・使用して編集パフォーマンスを向上。
レンダリング時にフル解像度に自動切り替え。

---

## コンセプト

```
高解像度フッテージ (4K, 200MB)
  → プロキシ生成 (480p, 5MB)
  → 編集中はプロキシを使用（軽い）
  → レンダリング時にフル解像度に自動切り替え
```

---

## 現状

| 機能 | 状態 |
|------|------|
| フッテージの直接読み込み | ✅ 完成 |
| プロキシ生成 | ❌ 未実装 |
| プロキシ切り替え | ❌ 未実装 |
| 自動プロキシ生成 | ❌ 未実装 |

---

## Implementation

### 1. ProxyGenerator
- 入力: 高解像度ビデオ/画像
- 出力: 低解像度プロキシ (MP4, JPEG, PNG)
- プリセット: 1/4, 1/2, 1/8 解像度
- バッチ処理対応

### 2. Proxy メタデータ
```cpp
struct ProxyInfo {
    bool hasProxy = false;
    QString proxyPath;
    int proxyWidth, proxyHeight;
    float scaleFactor = 1.0f;
    bool useProxy = true;  // 編集中の使用フラグ
};
```

### 3. フッテージ切り替え
- 編集中: プロキシを使用
- レンダリング時: フル解像度に自動切り替え
- ビューポートの品質ドロップダウン: "Auto / Proxy / Full"

---

## 見積

| タスク | 見積 |
|--------|------|
| ProxyGenerator (FFmpeg ベース) | 3h |
| プロキシメタデータ管理 | 2h |
| レイヤーへのプロキシ切替ロジック | 2h |
| UI (品質ドロップダウン) | 1h |

**総見積: ~8h**
