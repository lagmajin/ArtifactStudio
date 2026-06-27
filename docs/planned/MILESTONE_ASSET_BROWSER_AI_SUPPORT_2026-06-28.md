# Milestone: Asset Browser AI Support (M-AB-15)

**マイルストーンID**: M-AB-15
**作成日**: 2026-06-28
**優先度**: P3 (Low)
**推定工数**: 3-5日
**カテゴリ**: Asset Browser / AI / Automation
**状態**: Planned
**依存**: M-AB (Asset Browser base), M-AB-4 (Hover Preview), M-AB-12 (Tag System)

---

## 目的

アセットブラウザーにAI支援機能を実装する。自動タグ付け、類似アセット検索、コンテンツ推奨などのAI機能を提供。

---

## 背景

### 現状
- AI機能は未実装
- OpenCV、FFmpegなどの画像/動画処理ライブラリは既に統合済み
- 画像の特徴抽出、類似度計算などの基盤技術は利用可能
- 既存のAI関連マイルストーンは存在するが、Asset Browser固有のAI支援は未実装

### 要件
- **AI Auto-Tagging**: AIが画像/動画の内容を解析し、自動的にタグを提案
- **Similar Asset Search**: 類似のアセットを検索（ビジュアル類似性、メタデータ類似性）
- **Content-Based Recommendation**: 選択したアセットに基づいて関連アセットを推奨
- **Smart Filtering**: 自然言語クエリによるフィルタリング
- **Batch Processing**: 複数のアセットに対する一括AI処理

### ユースケース
1. 大量の未整理の画像に自動でタグを付与
2. 特定のアセットに似た画像/動画を検索
3. 選択したアセットに基づいて関連するアセットを提案
4. 自然言語で「夕焼けの画像を全て表示」などのクエリを実行

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **新規** | `ArtifactCore/include/AI/AssetAnalyzer.ixx/cppm` | アセット解析基盤 |
| **新規** | `ArtifactCore/include/AI/AutoTagger.ixx/cppm` | 自動タグ付け |
| **新規** | `ArtifactCore/include/AI/SimilaritySearcher.ixx/cppm` | 類似性検索 |
| **新規** | `ArtifactCore/include/AI/ContentRecommender.ixx/cppm` | コンテンツ推奨 |
| **新規** | `Artifact/src/Widgets/AI/AIAssistantPanel.ixx/cppm` | AIアシスタントパネル |
| **新規** | `Artifact/src/Widgets/AI/AIAutoTagDialog.ixx/cppm` | 自動タグ付けダイアログ |
| **新規** | `Artifact/src/Widgets/AI/SimilarAssetsWidget.ixx/cppm` | 類似アセットウィジェット |
| **新規** | `Artifact/src/Widgets/AI/AISettingsDialog.ixx/cppm` | AI設定ダイアログ |
| **変更** | `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | AI機能統合 |
| **新規** | `ArtifactCore/include/Event/AIAnalysisCompletedEvent.ixx` | AI解析完了イベント |

---

## 変更詳細

### 1. AssetAnalyzer - アセット解析基盤

**特徴**:
- OpenCVを使用した画像解析
- 色ヒストグラム、エッジ検出、テクスチャ特徴の抽出
- 非同期処理対応
- 解析結果のキャッシュ
- サムネイル生成

**主なメソッド**:
- `analyzeImage()` - 画像を解析
- `analyzeImageAsync()` - 非同期解析
- `analyzeVideoKeyframes()` - 動画のキーフレーム解析
- `compareImages()` - 画像の類似度計算
- `findSimilarAssets()` - 類似アセット検索

### 2. AutoTagger - 自動タグ付け

**特徴**:
- ルールベース + MLベースのハイブリッドタグ付け
- 信頼度に基づくタグ提案
- カラー、カテゴリ、オブジェクトなどのタグ
- 非同期処理対応

**主なメソッド**:
- `tagAsset()` - アセットにタグ付け
- `tagAssetAsync()` - 非同期タグ付け
- `getTagSuggestions()` - タグ提案取得

### 3. SimilaritySearcher - 類似性検索

**特徴**:
- 視覚的特徴、タグ、メタデータに基づく類似度計算
- 加重平均による柔軟な類似度計算
- インデックス機能による高速検索

**主なメソッド**:
- `findSimilarAssets()` - 類似アセット検索
- `calculateSimilarity()` - 類似度計算
- `buildIndex()` - インデックス構築

### 4. ContentRecommender - コンテンツ推奨

**特徴**:
- 類似性、タグ、使用履歴に基づく推奨
- 推奨理由の生成
- コンテキストを考慮した推奨

**主なメソッド**:
- `recommendForAsset()` - アセットに基づく推奨
- `recommendForTags()` - タグに基づく推奨
- `recommendBasedOnHistory()` - 履歴に基づく推奨

### 5. UIコンポーネント

**AIAssistantPanel**: AI機能のメインパネル
- モード切り替え（自動タグ付け、類似検索、推奨、設定）
- 進行状況表示
- 結果表示

**AIAutoTagDialog**: 自動タグ付けダイアログ
- タグ提案表示
- 信頼度表示
- タグ選択・適用

**SimilarAssetsWidget**: 類似アセット表示ウィジェット
- 類似度表示
- 並べ替え機能

**AISettingsDialog**: AI設定ダイアログ
- モデル選択
- しきい値設定
- GPU使用の有無

### 6. AssetBrowser への統合

**コンテキストメニューに追加**:
- "AI Auto-Tagging..." - 自動タグ付け
- "Find Similar Assets..." - 類似アセット検索
- "Get Recommendations..." - 推奨取得
- "AI Settings..." - AI設定

---

## タスク分割 (優先度付き)

### 優先度レベル
- **P0 (Critical)**: コア機能、なければ動作しない
- **P1 (High)**: 主要機能、ないと使い勝手が悪い
- **P2 (Medium)**: 便利機能、あってもなくても動作する
- **P3 (Low)**: 見栄え/UX向上、なくても機能する

### Phase 1: AI解析基盤 (2日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AssetAnalyzer` クラス設計 | P0 | 1h | なし | ✅ |
| 画像解析実装 (OpenCV) | P0 | 4h | 上記 | ✅ |
| 特徴抽出実装 | P0 | 4h | 上記 | ✅ |
| 画像比較実装 | P0 | 2h | 上記 | ✅ |
| 非同期解析 | P0 | 2h | 上記 | ✅ |
| キャッシュ機能 | P0 | 2h | 上記 | ✅ |

### Phase 2: 自動タグ付け (1日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AutoTagger` クラス設計 | P1 | 1h | Phase 1 | ✅ |
| ルールベースタグ付け | P1 | 3h | 上記 | ✅ |
| 色ベースタグ付け | P1 | 2h | 上記 | ✅ |
| 非同期タグ付け | P1 | 2h | 上記 | ✅ |
| シグナル/スロット | P1 | 1h | 上記 | ✅ |

### Phase 3: 類似性検索 (1日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `SimilaritySearcher` クラス設計 | P1 | 1h | Phase 1 | ✅ |
| 類似度計算実装 | P1 | 3h | 上記 | ✅ |
| タグ類似度 | P1 | 1h | 上記 | ✅ |
| メタデータ類似度 | P1 | 1h | 上記 | ✅ |
| 非同期検索 | P1 | 2h | 上記 | ✅ |
| インデックス機能 | P2 | 2h | 上記 | ✅ |

### Phase 4: コンテンツ推奨 (0.5日) - **P2**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `ContentRecommender` クラス設計 | P2 | 1h | Phase 3 | ✅ |
| 推奨アルゴリズム | P2 | 2h | 上記 | ✅ |
| 理由生成 | P2 | 1h | 上記 | ✅ |

### Phase 5: UIコンポーネント (1日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AIAssistantPanel` | P1 | 3h | Phase 4 | ✅ |
| `AIAutoTagDialog` | P1 | 2h | Phase 2 | ✅ |
| `SimilarAssetsWidget` | P1 | 2h | Phase 3 | ✅ |
| `AISettingsDialog` | P2 | 2h | Phase 4 | ✅ |

### Phase 6: AssetBrowser 統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| AI機能のUI統合 | P1 | 2h | Phase 5 | ❌ (UIスレッド) |
| コンテキストメニュー追加 | P1 | 1h | Phase 5 | ❌ (UIスレッド) |
| タグ提案表示 | P1 | 1h | Phase 5 | ❌ (UIスレッド) |

### 並行作業可能性
- **Phase 1-4 (Core)**: 独立して並行可能
- **Phase 5 (UI)**: Phase 4完了後、独立して並行可能
- **Phase 6 (Integration)**: UIスレッド依存

---

## テスト戦略

### 1. 自動テスト (Unit Tests)

#### P0 - 必須テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `AssetAnalyzer::analyzeImage` | 自動 | 画像が正しく解析 | 2h |
| `AssetAnalyzer::compareImages` | 自動 | 類似度が正しく計算 | 1h |
| 特徴抽出 (色ヒストグラム) | 自動 | 特徴が正しく抽出 | 1h |

#### P1 - 主要テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `AutoTagger::tagAsset` | 自動 | タグが正しく付与 | 2h |
| `SimilaritySearcher::calculateSimilarity` | 自動 | 類似度が正しい | 2h |
| `SimilaritySearcher::findSimilarAssets` | 自動 | 類似アセットが見つかる | 2h |
| `ContentRecommender::recommendForAsset` | 自動 | 推奨が正しい | 2h |

#### P2 - 統合テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| AI自動タグ付け | 手動 | タグが適切に付与 | 2h |
| 類似アセット検索 | 手動 | 類似アセットが見つかる | 2h |
| アセット推奨 | 手動 | 推奨が適切 | 2h |
| AIパネル操作 | 手動 | UIが使いやすい | 2h |

### 2. 手動テスト (Manual Tests)

#### UXテスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| AI自動タグ付けの操作性 | 直感的で分かりやすい | 2h |
| 類似検索の使いやすさ | 分かりやすい | 2h |
| 推奨機能の使いやすさ | 分かりやすい | 2h |
| AI設定の操作性 | 分かりやすい | 1h |

#### 視覚的テスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| 類似アセット表示 | 正しく表示 | 1h |
| 推奨アセット表示 | 正しく表示 | 1h |
| AIパネルのレイアウト | 見やすい | 1h |

### 3. テスト実行計画

#### Phase 1-2: Unit Tests (Core)
```
日数: 2日
対象: AssetAnalyzer, AutoTagger
方法: Google Test / Catch2
実行: CIパイプライン
```

#### Phase 3-4: Unit Tests (Search + Recommend)
```
日数: 1日
対象: SimilaritySearcher, ContentRecommender
方法: Google Test / Catch2
実行: CIパイプライン
```

#### Phase 5: UI Tests
```
日数: 1日
対象: AI UIコンポーネント
方法: 手動テスト
実行: QAチーム
```

#### Phase 6: Integration Tests
```
日数: 1日
対象: AssetBrowser 統合
方法: 手動テスト
実行: QAチーム
```

### 4. テスト完了基準
- [ ] P0全てのテストがパス
- [ ] P1の80%以上のテストがパス
- [ ] 重大なバグなし
- [ ] 自動タグ付けが正しく動作
- [ ] 類似性検索が正しく動作
- [ ] コンテンツ推奨が正しく動作
- [ ] UIが直感的で使いやすい
- [ ] 全てのテスト項目がパス

---

## 成果物

1. **コア機能**: AI支援機能（自動タグ付け、類似検索、推奨）
2. **API**: `AssetAnalyzer`, `AutoTagger`, `SimilaritySearcher`, `ContentRecommender`
3. **UIコンポーネント**: `AIAssistantPanel`, `AIAutoTagDialog`, `SimilarAssetsWidget`, `AISettingsDialog`
4. **統合**: AssetBrowser への完全統合
5. **イベントシステム**: AI関連イベント

---

## 依存関係

### 必要な前提条件
- **M-AB (Asset Browser 基盤)**
- **M-AB-4 (Hover Preview)**
- **M-AB-12 (Tag System)**
- **OpenCV** - 画像処理ライブラリ

### 連携する機能
- `AssetAnalyzer` - 画像解析
- `AutoTagger` - 自動タグ付け
- `SimilaritySearcher` - 類似性検索
- `ContentRecommender` - コンテンツ推奨
- `TagManager` - タグ管理

---

## リスクと対策

### リスク1: 処理時間
**内容**: AI解析に時間がかかりすぎる
**対策**:
- 非同期処理を実装
- キャッシュを活用
- 解析品質を調整可能に
- バックグラウンド処理

### リスク2: メモリ使用量
**内容**: 特徴ベクトルの保持によるメモリ消費
**対策**:
- 特徴ベクトルの次元を制限
- LRUキャッシュを実装
- 量子化（浮動小数点数の圧縮）

### リスク3: 精度
**内容**: AIのタグ付け精度が低い
**対策**:
- ルールベースとMLベースを組み合わせ
- 信頼度を表示
- ユーザーに確認を促す
- 既存のタグ付けから学習（将来的な拡張）

### リスク4: 依存ライブラリ
**内容**: OpenCV、MLライブラリへの依存
**対策**:
- オプショナルな機能として実装
- 依存ライブラリが存在しない場合は機能を無効化
- 落ち度の低い実装を提供

---

## テスト項目

- [ ] 画像解析が正しく動作
- [ ] 自動タグ付けが正しく動作
- [ ] 類似アセット検索が正しく動作
- [ ] コンテンツ推奨が正しく動作
- [ ] 非同期処理が正しく動作
- [ ] キャッシュ機能が正しく動作
- [ ] UIが直感的で使いやすい
- [ ] パフォーマンステスト（1000+アセット）

---

## 完了基準

- [ ] 画像解析が正しく動作
- [ ] 自動タグ付けが正しく動作
- [ ] 類似アセット検索が正しく動作
- [ ] コンテンツ推奨が正しく動作
- [ ] UIが直感的で使いやすい
- [ ] パフォーマンスに問題なし
- [ ] 全てのテスト項目がパス
- [ ] UXテストで問題なし
- [ ] ドキュメントが更新

---

## 関連文書

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md`
- `docs/planned/MILESTONES_BACKLOG.md`
- `docs/planned/MILESTONE_AI_*` - AI関連マイルストーン

---

## メモ

- AI支援機能はアセット管理を自動化・効率化
- 現段階ではルールベースの簡易実装、将来的にはMLモデルを統合
- After EffectsのAdobe Senseiと類似の機能を目指す
- 将来的な拡張：画像のオブジェクト検出、シーン分類、顔認識、OCR、3Dモデル解析、音声解析、クラウドAIサービス統合
- GPU加速を実装
- 実行環境の検出（GPU/CPU）と最適化

---

## 技術的注意事項

### OpenCV依存
```cpp
// 画像特徴抽出例
std::vector<float> extractColorHistogram(const cv::Mat& image, int bins = 64) {
    cv::Mat hsvImage;
    cv::cvtColor(image, hsvImage, cv::COLOR_BGR2HSV);
    
    int histSize[] = {bins, bins, bins};
    float hranges[] = {0, 180};
    float sranges[] = {0, 256};
    float vranges[] = {0, 256};
    const float* ranges[] = {hranges, sranges, vranges};
    int channels[] = {0, 1, 2};
    
    cv::Mat hist;
    cv::calcHist(&hsvImage, 1, channels, cv::Mat(), hist, 3, histSize, ranges);
    cv::normalize(hist, hist, 0, 1, cv::NORM_MINMAX);
    
    // 1次元に変換
    std::vector<float> result;
    result.resize(bins * bins * bins);
    std::memcpy(result.data(), hist.ptr<float>(), result.size() * sizeof(float));
    return result;
}
```

### 類似度計算（コサイン類似度）
```cpp
float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    
    float dotProduct = 0.0f;
    float norm1 = 0.0f, norm2 = 0.0f;
    
    for (size_t i = 0; i < a.size(); ++i) {
        dotProduct += a[i] * b[i];
        norm1 += a[i] * a[i];
        norm2 += b[i] * b[i];
    }
    
    norm1 = std::sqrt(norm1);
    norm2 = std::sqrt(norm2);
    
    if (norm1 == 0 || norm2 == 0) return 0.0f;
    return (dotProduct / (norm1 * norm2) + 1.0f) / 2.0f; // 0-1に正規化
}
```

### 非同期処理パターン
```cpp
void analyzeImageAsync(const QString& path, std::function<void(AnalysisResult)> callback) {
    QtConcurrent::run([this, path, callback]() {
        AnalysisResult result = analyzeImage(path);
        if (callback) callback(result);
    });
}
```
