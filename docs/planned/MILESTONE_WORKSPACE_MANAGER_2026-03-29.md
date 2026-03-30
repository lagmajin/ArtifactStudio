# Milestone: Workspace Manager (2026-03-29)

**Status:** Not Started
**Goal:** カスタムワークスペースレイアウトの保存/読込。
用途別にパネル配置を切り替えられるようにする。

---

## 現状

| 機能 | 状態 |
|------|------|
| ドッキングパネル (Qt ADS) | ✅ 完成 |
| レイアウトリセット | ⚠️ 未接続 |
| ワークスペース保存/読込 | ❌ 未実装 |
| デフォルトワークスペース | ❌ 未実装 |

---

## 実装

### 1. WorkspaceManager
- Qt ADS の `CDockManager::saveState()` / `restoreState()` を使用
- JSON 形式でレイアウトを保存
- プリセット: Animation / Color / Edit / Compositing / Effects

### 2. プリセットワークスペース

| 名前 | パネル配置 |
|------|----------|
| **Animation** | タイムライン最大化、カーブエディタ表示 |
| **Color** | スコープ最大化、カラーホイール表示 |
| **Compositing** | コンポジションビューポート最大化、レイヤーパネル |
| **Effects** | インスペクタ最大化、エフェクトリスト |

### 3. UI
- メニュー: 「表示 > ワークスペース > Animation/Color/Edit/...」
- ショートカット: F5-F8 で切り替え
- ワークスペース保存ダイアログ

---

## 見積

| タスク | 見積 |
|--------|------|
| WorkspaceManager (save/load) | 3h |
| プリセット定義 | 1h |
| メニュー/ショートカット接続 | 1h |
| レイアウト自動保存 | 1h |

**総見積: ~6h**
