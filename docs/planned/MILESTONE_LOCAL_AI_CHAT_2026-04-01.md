# Milestone: Local AI Chat Integration (2026-04-01)

**Status:** Phase 0+2 Complete (遅延初期化 + 設定UI実装済み)
**Goal:** ローカルLLM (llama.cpp) を接続し、アプリ内チャットで会話できるようにする

---

## 現状

| 機能 | 状態 |
|------|------|
| `AIChatWidget` (チャットUI) | ✅ 実装済み |
| `AIClient` (送受信) | ✅ 実装済み |
| `AIPromptGenerator` | ✅ ヘッダー |
| `AIContext` | ✅ ヘッダー |
| 遅延初期化 (`initialize()`/`shutdown()`) | ✅ 実装済み |
| `LlamaLocalAgent` | ❌ ヘッダーのみ、実装ファイルなし |
| `llama.cpp バインディング` | ❌ 未実装 |
| モデルファイル | ❌ 未配置 |

---

## Phase 1: LlamaLocalAgent 実装

### 実装内容
- `LlamaLocalAgent.cppm` の実装
- llama.cpp の初期化・推論パイプライン
- モデルロード、プロンプト処理、ストリーミング出力

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `ArtifactCore/src/AI/LlamaLocalAgent.cppm` | 実装 |

### 外部依存
- llama.cpp (vcpkg または submodule)
- モデルファイル (llama-3.2-1b-instruct.q4_k_m.gguf 等)

### 見積: 6h

---

## Phase 2: AI Chat Widget 完成

### 実装内容
- チャットUIの改善（Markdown表示、コードブロック、スクロール）
- 設定パネル（モデルパス、プロバイダ切替）
- 会話履歴の保存/読み込み

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `Artifact/src/Widgets/AIChatWidget.cppm` | 改善 |
| `ArtifactWidgets/src/AI/AIChatSettingsDialog.cppm` | 設定ダイアログ |

### 見積: 4h

---

## Phase 3: アプリケーション統合

### 実装内容
- メニューからのチャット起動
- ドッキングパネルとしての配置
- 選択中のレイヤー/コンポジション情報をコンテキストに自動追加

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `Artifact/src/Widgets/Menu/ArtifactHelpMenu.cppm` | チャット起動項目追加 |
| `Artifact/src/Widgets/MainWindow.cppm` | ドッキング統合 |

### 見積: 3h

---

## Recommended Order

| 順序 | フェーズ | 見積 |
|---|---|---|
| 1 | **Phase 1: LlamaLocalAgent 実装** | 6h |
| 2 | **Phase 2: AI Chat Widget 完成** | 4h |
| 3 | **Phase 3: アプリケーション統合** | 3h |

**総見積: ~13h**

---

## 技術的注意点

1. **llama.cpp のビルド**
   - vcpkg で `llama` パッケージをインストール可能
   - または submodule として追加
   - CUDA/Vulkan バックエンドのオプション検討

2. **モデルファイル**
   - HuggingFace からダウンロード (例: `llama-3.2-1b-instruct.q4_k_m.gguf`)
   - 初回起動時にダウンロード or 手動配置
   - パスは設定から変更可能に

3. **メモリ使用量**
   - 1B モデル: ~1GB RAM
   - 3B モデル: ~2GB RAM
   - 7B モデル: ~4GB RAM
   - デフォルトは 1B を推奨

4. **ストリーミング対応**
   - `AIClient::postMessage()` は既にストリーミング対応済み
   - llama.cpp の `llama_decode` をチャンク単位で呼び出す
