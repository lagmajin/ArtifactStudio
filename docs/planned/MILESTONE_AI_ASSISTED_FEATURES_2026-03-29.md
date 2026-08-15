# Milestone: AI-Assisted Features (2026-03-29)

**Status:** 部分実装（AI 基盤・Expression Copilot・Tool Bridge は実装済み、制作補助機能は未完了）
**最終更新:** 2026-08-15
**Goal:** AI を活用したアニメーション制作支援。
手動で難しい作業を自動化して生産性を向上。

---

## 現状

| 機能 | 状態 |
|------|------|
| AI Expression Copilot | ✅ 完成 |
| AI Image Generation | 🟡 基盤／入口あり、実サービス応答・成果物適用は未検証 |
| AI Prompt Engineering | ⚠️ 基本のみ |
| Auto-Reframe | ❌ 未実装 |
| Background Removal | ❌ 未実装 |
| Style Transfer | ❌ 未実装 |
| Auto-Rigging | ❌ 未実装 |

## 2026-07-25 実装監査

AI Expression Copilot／AI client／ObjectDetector API／ColorGrading 基盤と、AI の説明カタログに background removal 等の説明は確認した。一方、Auto-Reframe の追従実装、Background Removal の実マット生成、一括処理、Style Transfer、複数ショットの Auto Color Match、Auto-Rigging の実装は確認できない。したがって現状表の「完成」は既存の関連基盤・入口を指す範囲に限定し、制作補助機能全体は未完了・runtime未検証とする。

## 2026-08-15 現行コード監査

- `Core.AI.ToolBridge`、`ToolExecutor`、`PromptGenerator`、Tiered AI の local/cloud 経路を確認。AI ツールのスキーマ生成・tool call 正規化・実行入口は存在する。
- Expression Copilot は式の生成／プレビューと基本的な UI 導線があるが、完全なレイヤーコンテキスト・全関数・runtime 適用確認は未完了。
- ObjectDetector、画像解析、ColorGrading の基盤は存在するが、Auto-Reframe の追従アニメーション、Background Removal のマット生成、Style Transfer、Auto Color Match、Auto-Rigging の制作フロー接続は確認できない。
- 旧表の「AI Image Generation 完成」は関連クライアント／入口の存在を示す範囲に留め、画像生成の実サービス応答と成果物適用は runtime 未検証として扱う。

判定: **AI 基盤と Expression／Tool の入口は実装済み。列挙された制作自動化機能の大半は未接続または未実装で、runtime 検証も pending。**

## Update 2026-08-15

現行コードを追加確認した。`Core.AI.ToolBridge`、`ToolExecutor`、`PromptGenerator`、Tiered AIのlocal／cloud経路、Expression Copilotの生成・preview UI、ObjectDetector／画像解析／ColorGradingの関連基盤は存在する。AI tool schema、tool call正規化、実行入口は整備されている。

一方、Auto-Reframeの追従アニメーション、Background Removalの実マット生成、Style Transfer、複数ショットのAuto Color Match、Auto-Riggingの制作workflowは未接続または未実装。画像生成も実サービス応答と成果物適用はruntime未検証であり、AI基盤以外の制作自動化は pending とする。

---

## 機能

### 1. Auto-Reframe
- 異なるアスペクト比に自動でコンテンツをリフレーム
- サブジェクト検出で被写体を追従
- ソーシャルメディア向け (9:16, 1:1, 4:5) の自動適応

### 2. Background Removal
- ワンクリックで背景除去
- マットとして自動エクスポート
- シーケンス全体の一括処理

### 3. Style Transfer
- 参照画像のスタイルをターゲットに適用
- アニメ風 / 油絵風 / スケッチ風
- リアルタイムプレビュー

### 4. Auto Color Match
- 複数ショットの色を自動マッチング
- リファレンスショットの色調に他ショットを合わせる

---

## Implementation

### Auto-Reframe:
```
1. フレームからサブジェクトを検出 (ObjectDetector を使用)
2. サブジェクトのバウンディングボックスを追跡
3. ターゲットアスペクト比でクロップ位置を計算
4. スムーズなパンアニメーションを生成
```

### Background Removal:
```
1. セグメンテーションモデルで前景/背景を分離
2. アルファマットを生成
3. マットレイヤーとして出力
```

### Style Transfer:
```
1. 参照画像のスタイル特徴量を抽出
2. ターゲット画像にスタイルを適用
3. パラメータで強度を調整
```

---

## 見積

| タスク | 見積 |
|--------|------|
| Auto-Reframe (サブジェクト追従) | 6h |
| Background Removal | 4h |
| Style Transfer | 4h |
| Auto Color Match | 3h |
| AI モデル統合レイヤー | 2h |

**総見積: ~19h**

---

## 関連ファイル

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/src/AI/ObjectDetector.cppm` | オブジェクト検出 |
| `Artifact/src/AI/AIClient.cppm` | AI クライアント |
| `ArtifactCore/include/AI/ObjectDetector.ixx` | 検出 API |
