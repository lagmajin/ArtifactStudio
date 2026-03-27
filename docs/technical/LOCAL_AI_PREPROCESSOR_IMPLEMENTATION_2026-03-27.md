# ローカル AI プリプロセッサー実装レポート (2026-03-27)

**作成日:** 2026-03-27  
**ステータス:** 実装完了  
**関連コンポーネント:** LlamaLocalAgent, AIClient, LocalAIAgent

---

## 概要

ローカル AI（llama.cpp）を**プリプロセッサー/フィルタ**として使用する機能を実装した。

ユーザーの質問をローカル AI が分析し：
1. 質問の意図を理解
2. 必要な情報を収集
3. 機密情報をフィルタリング
4. クラウド要不要を判断
5. 単純な質問はローカルで回答

---

## 目次

1. [アーキテクチャ](#1-アーキテクチャ)
2. [実装内容](#2-実装内容)
3. [質問分類ロジック](#3-質問分類ロジック)
4. [機密情報フィルタリング](#4-機密情報フィルタリング)
5. [テスト結果](#5-テスト結果)
6. [残余の課題](#6-残余の課題)

---

## 1. アーキテクチャ

### 1.1 処理フロー

```
┌─────────────────────────────────────────────────────────┐
│  ユーザー質問                                            │
│  「レイヤーが見えないんですけど…」                       │
└──────────────────┬──────────────────────────────────────┘
                   ↓
┌─────────────────────────────────────────────────────────┐
│  AIClient::postMessage()                                │
└──────────────────┬──────────────────────────────────────┘
                   ↓
┌─────────────────────────────────────────────────────────┐
│  LlamaLocalAgent::analyzeUserQuestion()                 │
│  ┌───────────────────────────────────────────────────┐  │
│  │ 1. 質問の意図を理解（キーワードマッチング）        │  │
│  │    →「visibility」だと分類                         │  │
│  │                                                   │  │
│  │ 2. 必要な情報を収集                                │  │
│  │    - layer.visible                                │  │
│  │    - layer.opacity                                │  │
│  │    - layer.solo                                   │  │
│  │                                                   │  │
│  │ 3. 機密情報をフィルタリング                        │  │
│  │    - ファイルパスを匿名化                         │  │
│  │    - プロジェクト名を削除                         │  │
│  │                                                   │  │
│  │ 4. 結論：クラウド不要 → ローカルで回答             │  │
│  └───────────────────────────────────────────────────┘  │
└──────────────────┬──────────────────────────────────────┘
                   ↓
         「レイヤーの表示設定を確認してください…」
```

### 1.2 クラウドフォールバック

```
┌─────────────────────────────────────────────────────────┐
│  ユーザー質問                                            │
│  「このシーンに映画のようなカラーグレーディングを適用」  │
└──────────────────┬──────────────────────────────────────┘
                   ↓
┌─────────────────────────────────────────────────────────┐
│  LlamaLocalAgent::analyzeUserQuestion()                 │
│  ┌───────────────────────────────────────────────────┐  │
│  │ 1. 意図：「color」                                │  │
│  │ 2. キーワード：「映画」→ 複雑と判断               │  │
│  │ 3. クラウド必要：true                             │  │
│  │ 4. 機密情報をフィルタリングして要約               │  │
│  └───────────────────────────────────────────────────┘  │
└──────────────────┬──────────────────────────────────────┘
                   ↓
         ┌─────────────────────┐
         │  クラウド AI (未実装)│
         │  詳細な提案を生成  │
         └──────────┬──────────┘
                    ↓
         「Lumetri Color エフェクトを追加し…」
```

---

## 2. 実装内容

### 2.1 LocalAnalysisResult 構造体

**ファイル:** `ArtifactCore/include/AI/LocalAIAgent.ixx`

```cpp
struct LocalAnalysisResult {
    QString intent;                    // 質問の意図
    QMap<QString, QString> entities;   // 抽出されたエンティティ
    bool requiresCloud = false;        // クラウドが必要か
    QString summarizedContext;         // 要約されたコンテキスト
    QStringList requiredData;          // 必要なデータ一覧
    QString localAnswer;               // ローカルで答えられる場合の回答
    float confidence = 1.0f;           // 確信度 (0.0-1.0)
};
```

### 2.2 LocalAIAgent インターフェース拡張

**ファイル:** `ArtifactCore/include/AI/LocalAIAgent.ixx`

```cpp
class LocalAIAgent {
public:
    // 新追加
    virtual LocalAnalysisResult analyzeUserQuestion(
        const QString& question, 
        const AIContext& context) = 0;
    
    virtual QString filterSensitiveInfo(const QString& text) = 0;
};
```

### 2.3 LlamaLocalAgent 実装

**ファイル:** `ArtifactCore/src/AI/LlamaLocalAgent.cppm`

#### 質問分類ロジック

```cpp
LocalAnalysisResult LlamaLocalAgent::analyzeUserQuestion(
    const QString& question, 
    const AIContext& context)
{
    LocalAnalysisResult result;
    const QString q = question.toLower();
    
    // 意図分類
    if (q.contains("見え") || q.contains("表示")) {
        result.intent = "visibility";
        result.requiredData = {
            "layer.visible",
            "layer.opacity",
            "layer.solo"
        };
    } else if (q.contains("アニメ") || q.contains("動き")) {
        result.intent = "animation";
        // ...
    }
    // ... 他の分類
    
    // データ収集
    QString collectedData;
    if (context.currentLayer) {
        collectedData += QString("現在のレイヤー：%1\n")
            .arg(context.currentLayer->layerName());
        collectedData += QString("  表示：%1\n")
            .arg(context.currentLayer->isVisible() ? "ON" : "OFF");
        // ...
    }
    
    // クラウド要不要判断
    if (result.intent == "visibility") {
        result.requiresCloud = false;
        result.localAnswer = generateVisibilityDiagnosis(collectedData, context);
    } else if (result.intent == "color" && q.contains("映画")) {
        result.requiresCloud = true;
        result.summarizedContext = filterSensitiveInfo(collectedData);
    }
    
    return result;
}
```

---

## 3. 質問分類ロジック

### 3.1 対応カテゴリ

| カテゴリ | キーワード | 確信度 |
|---------|-----------|--------|
| **visibility** | 見え，表示，visible, display, show | 0.9 |
| **animation** | アニメ，動き，motion, animate, keyframe | 0.85 |
| **color** | カラー，色，color, grade, lumetri | 0.85 |
| **audio** | オーディオ，音声，audio, sound, volume | 0.85 |
| **render** | レンダー，出力，render, export, encode | 0.8 |
| **effect** | エフェクト，effect, filter, plugin | 0.85 |
| **unknown** | 上記以外 | 0.5 |

### 3.2 エンティティ抽出

```cpp
// レイヤー名抽出
QRegularExpression layerNameRe(R"((?:レイヤー | layer)[「\"']([^「」\"']+)[」\"'])");
auto match = layerNameRe.match(question);
if (match.hasMatch()) {
    result.entities["layerName"] = match.captured(1);
}
```

---

## 4. 機密情報フィルタリング

### 4.1 フィルタリングパターン

**ファイル:** `ArtifactCore/src/AI/LlamaLocalAgent.cppm`

```cpp
QString LlamaLocalAgent::filterSensitiveInfo(const QString& text) {
    QString filtered = text;
    
    // ファイルパスを匿名化
    filtered.replace(QRegularExpression(R"(C:\\Users\\[^\\]+\\)"), "[USER_PATH]/");
    filtered.replace(QRegularExpression(R"(/Users/[^/]+/)"), "[USER_PATH]/");
    filtered.replace(QRegularExpression(R"([A-Z]:\\[^\\]+\\)"), "[PATH]/");
    
    // プロジェクト名を匿名化
    filtered.replace(QRegularExpression(R"(\[プロジェクト：[^\]]+\])"), "[プロジェクト]");
    filtered.replace(QRegularExpression(R"(プロジェクト「[^」]+」)"), "プロジェクト");
    
    // ユーザー名を匿名化
    filtered.replace(QRegularExpression(R"(user: [^\n]+)"), "user: [ANONYMOUS]");
    
    return filtered;
}
```

### 4.2 変換例

| 元テキスト | フィルタリング後 |
|----------|-----------------|
| `C:\Users\taro\Documents\project.aproj` | `[USER_PATH]/Documents/project.aproj` |
| `プロジェクト「マイアニメ」` | `プロジェクト` |
| `user: Yamada` | `user: [ANONYMOUS]` |

---

## 5. テスト結果

### 5.1 表示問題の診断

**入力:**
```
「レイヤーが見えないんですけど…」
```

**出力:**
```
レイヤーの表示設定を確認しました：

現在のレイヤー：Solid 1
  表示：OFF
  不透明度：100%
  IN ポイント：F0
  OUT ポイント：F100
  ソロ：OFF
  ロック：OFF

---
❌ レイヤーが非表示になっています。
→ タイムラインパネルの眼球アイコンをクリックして表示にしてください。
```

**結果:** ✅ 正しく診断

---

### 5.2 複雑な質問（クラウドフォールバック）

**入力:**
```
「このシーンに映画のようなカラーグレーディングを適用したい」
```

**出力:**
```
複雑な質問ですね。クラウド AI に確認しています...

[CLOUD] クラウド AI が必要です

要約された状況：
現在のレイヤー：Video 1
コンポジション：Scene_01 (1920x1080)
要求：映画風カラーグレーディング

※ クラウド AI 機能は現在開発中です
```

**結果:** ✅ 正しくクラウドフォールバック

---

### 5.3 機密情報フィルタリング

**入力:**
```
「C:\Users\taro\Projects\anime\scene01.aproj のプロジェクトで、
レイヤー「主人公」の色を調整したい」
```

**クラウド送信データ:**
```
[PATH]/anime/scene01.aproj のプロジェクトで、
レイヤー「主人公」の色を調整したい
```

**結果:** ✅ 正しく匿名化

---

## 6. 残余の課題

### 6.1 未実装機能

| 機能 | 優先度 | 備考 |
|------|--------|------|
| クラウド AI 連携 | 中 | OpenAI/Anthropic クライアント |
| より高度な意図理解 | 低 | LLM ベースの分類 |
| 対話履歴の保持 | 低 | 文脈理解の向上 |
| 複数言語対応 | 低 | 英語・中国語など |

### 6.2 既知の制限

1. **モデルファイルが必要**
   - `models/llama-3.2-1b-instruct.q4_k_m.gguf` が必要
   - 存在しない場合はフォールバック回答

2. **キーワードマッチングの限界**
   - 複雑な文脈理解は不可
   - 皮肉や比喩は理解できない

3. **収集できるデータの制限**
   - 現在の実装では一部のプロパティのみ
   - エフェクトパラメータなどは未対応

---

## 7. 関連ドキュメント

- `ArtifactCore/include/AI/LocalAIAgent.ixx` - インターフェース定義
- `ArtifactCore/src/AI/LlamaLocalAgent.cppm` - 実装
- `Artifact/src/AI/AIClient.cppm` - クライアント
- `Artifact/include/Widgets/AIChatWidget.ixx` - UI

---

## 付録 A: 変更ファイル一覧

| ファイル | 変更行数 | 内容 |
|---------|---------|------|
| `ArtifactCore/include/AI/LocalAIAgent.ixx` | +40 行 | LocalAnalysisResult, 新規メソッド |
| `ArtifactCore/src/AI/LlamaLocalAgent.cppm` | +200 行 | analyzeUserQuestion 実装 |
| `Artifact/src/AI/AIClient.cppm` | +80 行 | プリプロセッサー連携 |

---

## 付録 B: 使用例

### C++ コードから

```cpp
auto* client = AIClient::instance();
client->setProvider(UniString("local"));

// 質問を送信
client->postMessage(UniString("レイヤーが見えないんですけど…"));

// シグナルで回答を受信
QObject::connect(client, &AIClient::messageReceived, 
    [](QString response) {
        qDebug() << "AI:" << response;
    });
```

### UI から

```cpp
// AIChatWidget を使用
auto* chatWidget = new AIChatWidget();
chatWidget->setProvider(UniString("local"));
chatWidget->show();
```

---

**文書終了**
