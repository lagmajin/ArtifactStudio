# ローカル AI プリプロセッサー Milestone

**作成日:** 2026-03-27  
**ステータス:** 実装完了  
**関連コンポーネント:** LlamaLocalAgent, AIClient, AIContext, AIChatWidget

---

## 概要

ローカル AI（llama.cpp またはルールベース）を**プリプロセッサー/フィルタ**として使用する機能を実装する。

ユーザーの質問をローカル AI が分析し：
1. 質問の意図を理解
2. 必要な情報を収集
3. 機密情報をフィルタリング
4. クラウド要不要を判断
5. 単純な質問はローカルで回答

---

## Goal

- ユーザーの自然な質問をローカルで分析
- 表示問題、アニメーション設定などの一般的な質問はローカルで回答
- 複雑な質問（映画的カラーグレーディングなど）はクラウド AI にエスカレーション
- 機密情報（ファイルパス、プロジェクト名）を自動的に匿名化

---

## Scope

- `ArtifactCore/include/AI/LocalAIAgent.ixx`
- `ArtifactCore/include/AI/LlamaLocalAgent.ixx`
- `ArtifactCore/src/AI/LlamaLocalAgent.cppm`
- `ArtifactCore/include/AI/AIContext.ixx`
- `Artifact/src/AI/AIClient.cppm`
- `Artifact/include/Widgets/AIChatWidget.ixx`
- `Artifact/src/Widgets/AIChatWidget.cppm`

---

## Non-Goals

- 大規模言語モデルの完全実装
- 画像生成機能
- 音声認識機能
- 完全な自然言語理解

---

## Background

現在の `AIClient` はダミー実装のみで、実際の AI 機能は存在しない。

この milestone は、ローカル AI を「プリプロセッサー」として位置づけ、以下の利点を実現する：

1. **プライバシー保護** - 機密情報をフィルタリングしてからクラウド送信
2. **コスト削減** - 単純な質問はローカルで処理
3. **速度向上** - ローカル処理は即時回答
4. **オフライン対応** - 一部機能はオフラインで利用可能

---

## Phases

### Phase 1: 基盤実装

- 目的:
  - ローカル AI のインターフェースを定義
  - 質問分析の基本的な構造を実装

- 作業項目:
  - `LocalAnalysisResult` 構造体の定義
  - `LocalAIAgent::analyzeUserQuestion()` インターフェース
  - `LocalAIAgent::filterSensitiveInfo()` インターフェース
  - `AIContext` に `userPrompt`, `systemPrompt` メンバー追加

- 完了条件:
  - インターフェースが定義される
  - AIContext がプロンプトを保持できる

### Phase 2: ルールベース実装

- 目的:
  - llama.cpp に依存しないフォールバック実装
  - キーワードマッチングによる意図分類

- 作業項目:
  - 6 つのカテゴリ分類（visibility, animation, color, audio, render, effect）
  - キーワードマッチングロジック
  - ルールベースの回答生成
  - 機密情報フィルタリング（正規表現）

- 完了条件:
  - llama.cpp なしでビルド可能
  - 基本的な質問に回答可能

- 実装済み機能:
  ```cpp
  // 質問分類
  if (q.contains("見え") || q.contains("表示")) {
      result.intent = "visibility";
  } else if (q.contains("アニメ") || q.contains("動き")) {
      result.intent = "animation";
  }
  // ...
  
  // 機密情報フィルタリング
  filtered.replace(QRegularExpression(R"(C:\\Users\\[^\\]+\\)"), "[USER_PATH]/");
  ```

### Phase 3: AIClient 連携

- 目的:
  - ローカル AI を AIClient に統合
  - クラウドフォールバックの仕組み

- 作業項目:
  - `AIClient::postMessage()` の修正
  - ローカル分析 → クラウドフォールバックフロー
  - ストリーミング表示
  - エラーハンドリング

- 完了条件:
  - AIClient がローカル AI を使用
  - 複雑な質問はクラウドへフォールバック

- 実装済みフロー:
  ```cpp
  auto analysis = impl_->localAgent->analyzeUserQuestion(input, context);
  
  if (!analysis.requiresCloud) {
      // ローカルで回答
      QString response = analysis.localAnswer;
      Q_EMIT this->messageReceived(response);
  } else {
      // クラウドにフォールバック
      Q_EMIT this->partialMessageReceived("複雑な質問ですね。クラウド AI に確認しています...");
      // ...
  }
  ```

### Phase 4: 診断機能

- 目的:
  - 具体的な問題診断機能
  - ステップバイステップの案内

- 作業項目:
  - 表示問題診断（`generateVisibilityDiagnosis()`）
  - 一般的な回答生成（`generateGenericAnswer()`）
  - チェックリスト形式の案内

- 完了条件:
  - 「レイヤーが見えない」などの質問に具体的な手順を回答
  - チェックリスト形式で表示

- 実装済み診断:
  ```cpp
  if (collectedData.contains("(要確認)")) {
      answer += "\n⚠️ 詳細な状態は AIContext から取得できません。\n";
      answer += "以下の手順で手動で確認してください：\n\n";
      answer += "1. タイムラインパネルで眼球アイコンが ON になっているか確認\n";
      answer += "2. プロパティパネルで不透明度が 0% になっていないか確認\n";
      // ...
  }
  ```

### Phase 5: UI 統合

- 目的:
  - AIChatWidget での表示
  - プロバイダー切り替え

- 作業項目:
  - ローカル/クラウド切り替え UI
  - 分析結果の表示
  - ストリーミング表示の改善

- 完了条件:
  - UI からローカル AI を使用可能
  - 回答がストリーミング表示

---

## 質問分類カテゴリ

| カテゴリ | キーワード | 確信度 | クラウド |
|---------|-----------|--------|---------|
| **visibility** | 見え，表示，visible, display | 0.9 | ❌ ローカル |
| **animation** | アニメ，動き，motion, keyframe | 0.85 | ❌ ローカル |
| **color** | カラー，色，color, grade | 0.85 | ⚠️ 条件付き |
| **audio** | オーディオ，音声，audio, volume | 0.85 | ❌ ローカル |
| **render** | レンダー，出力，render, export | 0.8 | ❌ ローカル |
| **effect** | エフェクト，effect, filter | 0.85 | ⚠️ 条件付き |

**クラウドフォールバック条件:**
- `color` + 「映画」「cinematic」→ クラウド
- `effect` + 「作成」「create」「custom」→ クラウド
- `animation` + 「自動」「auto」→ クラウド

---

## 機密情報フィルタリング

### フィルタリングパターン

| パターン | 変換後 |
|---------|--------|
| `C:\Users\taro\...` | `[USER_PATH]/...` |
| `/Users/taro/...` | `[USER_PATH]/...` |
| `プロジェクト「マイアニメ」` | `プロジェクト` |
| `user: Yamada` | `user: [ANONYMOUS]` |

### 実装

```cpp
QString LlamaLocalAgent::filterSensitiveInfo(const QString& text) {
    QString filtered = text;
    
    // ファイルパスを匿名化
    filtered.replace(QRegularExpression(R"(C:\\Users\\[^\\]+\\)"), "[USER_PATH]/");
    filtered.replace(QRegularExpression(R"(/Users/[^/]+/)"), "[USER_PATH]/");
    
    // プロジェクト名を匿名化
    filtered.replace(QRegularExpression(R"(\[プロジェクト：[^\]]+\])"), "[プロジェクト]");
    
    return filtered;
}
```

---

## 使用例

### 表示問題の診断

**入力:**
```
「レイヤーが見えないんですけど…」
```

**出力:**
```
レイヤーの表示設定を確認しました：

選択中のレイヤー：Solid 1
  表示：(要確認)
  不透明度：(要確認)
  ソロ：(要確認)

---
⚠️ 詳細な状態は AIContext から取得できません。
以下の手順で手動で確認してください：

1. タイムラインパネルで眼球アイコンが ON になっているか確認
2. プロパティパネルで不透明度が 0% になっていないか確認
3. ソロ設定（S）が有効になっていないか確認
4. レイヤーがロック（🔒）されていないか確認
5. IN/OUT ポイントの範囲内にプレイヘッドがあるか確認
```

### 複雑な質問（クラウドフォールバック）

**入力:**
```
「このシーンに映画のようなカラーグレーディングを適用したい」
```

**出力:**
```
複雑な質問ですね。クラウド AI に確認しています...

[CLOUD] クラウド AI が必要です

要約された状況：
選択中のレイヤー：Video 1
アクティブコンポジション：Scene_01
要求：映画風カラーグレーディング

※ クラウド AI 機能は現在開発中です
```

---

## 実装状況

### 完了済み

- ✅ `LocalAnalysisResult` 構造体
- ✅ `LocalAIAgent` インターフェース拡張
- ✅ `LlamaLocalAgent::analyzeUserQuestion()` 実装
- ✅ 機密情報フィルタリング
- ✅ `AIClient` 連携
- ✅ クラウドフォールバック

### 未実装

- ⏳ クラウド AI クライアント（OpenAI/Anthropic）
- ✅ ルールベースフォールバック（llama.cpp ビルド問題のため）
- ⏳ 対話履歴の保持
- ⏳ 複数言語対応

---

## 関連ドキュメント

- `docs/technical/LOCAL_AI_PREPROCESSOR_IMPLEMENTATION_2026-03-27.md` - 実装詳細
- `ArtifactCore/include/AI/LocalAIAgent.ixx` - インターフェース
- `ArtifactCore/src/AI/LlamaLocalAgent.cppm` - 実装
- `Artifact/src/AI/AIClient.cppm` - クライアント

---

## 今後の拡張

### Phase 6: llama.cpp 連携

- モデルファイルの読み込み
- GGUF 形式サポート
- GPU 加速（オプション）

### Phase 7: クラウド AI 連携

- OpenAI GPT-4 クライアント
- Anthropic Claude クライアント
- API キー管理

### Phase 8: 対話履歴

- 文脈理解の向上
- 複数ターン対話
- 以前の質問を参照

---

**文書終了**
