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

---

## Static audit follow-up (2026-07-25)

旧来の現状表を現行ソースと照合した。`LlamaLocalAgent.cppm` は既に存在し、llama.cpp の GGUF load／context／sampler／生成・streaming 経路を実装しているため、冒頭の「ヘッダーのみ」は更新が必要である。ビルド・実モデル推論は未実施。

| 項目 | 現状 | 判定 |
|---|---|---|
| AIChatWidget / AIClient / prompt / context | Chat UI、provider／model path、遅延 initialize／shutdown、context生成、streaming応答が存在する。 | 実装済み／実行確認待ち |
| LlamaLocalAgent / llama.cpp | `LlamaLocalAgent.cppm` に GGUF load、architecture check、sampler、同期・streaming生成、error state がある。 | 実装済み／実モデル確認待ち |
| 会話履歴 | `AIChatWidget` が複数 session と JSON 設定保存／復元を持つ。 | 実装済み |
| アプリ統合 | Main Window の layout preset に AI Chat が含まれ、AI client の context／tool 経路も存在する。 | 部分実装／導線確認待ち |
| モデル配置・配布 | モデル path は設定可能だが、既定 GGUF の同梱／download／配布契約は確認できない。 | 未完了 |

### 現在の判定

Local AI Chat の主要な UI／client／llama.cpp backend は実装済み。実モデルでの推論、配布モデル、起動導線の runtime 確認が残るため、全体は「実装済み／実行・配布確認待ち」とする。
