# Milestone: AI-Assisted Features (2026-03-29)

**Status:** Not Started
**Goal:** AI を活用したアニメーション制作支援。
手動で難しい作業を自動化して生産性を向上。

---

## 現状

| 機能 | 状態 |
|------|------|
| AI Expression Copilot | ✅ 完成 |
| AI Image Generation | ✅ 完成 (Stable Diffusion) |
| AI Prompt Engineering | ⚠️ 基本のみ |
| Auto-Reframe | ❌ 未実装 |
| Background Removal | ❌ 未実装 |
| Style Transfer | ❌ 未実装 |
| Auto-Rigging | ❌ 未実装 |

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
