# 優先度高い実装マイルストーン 総括

**作成日:** 2026-03-28
**更新日:** 2026-03-28
**ステータス:** 一部実装済み

---

## 優先度🔴高のマイルストーン（4 件）

| # | マイルストーン | 工数 | 効果 | 依存 | ステータス |
|---|--------------|------|------|------|-----------|
| **1** | **レンダリング性能改善** | 16-23h | フレームレート向上 | なし | ✅ **一部実装済み** |
| **2** | **アプリ層改善** | 29-41h | 必須機能の完成 | なし | ✅ **一部実装済み** |
| **3** | **テスト・QA 基盤** | 76-102h | 品質・信頼性向上 | なし | ❌ 未着手 |
| **4** | **セキュリティ強化** | 64-82h | データ保護 | なし | ❌ 未着手 |
| **5** | **Undo/Redo 統合** | 20-30h | 操作安心感 | なし | ✅ **一部実装済み** |
| **6** | **ASIO スタブバックエンド** | 12-18h | 音声出力 | なし | ✅ **実装済み** |
| **7** | **ROI システム** | 12-18h | 描画最適化 | なし | ✅ **一部実装済み** |

**合計工数:** 185-248 時間（新規：152-200h）

---

## 実装推奨順序

### 第 1 段階: 基盤整備（45-64h）

1. **レンダリング性能改善**（16-23h）
   - 即座にユーザー体験が向上
   - 実装が比較的容易
   - 他機能への影響小

2. **アプリ層改善**（29-41h）
   - 必須機能の完成
   - 動画プロキシ機能
   - プロジェクト管理の安定化

### 第 2 段階: 品質向上（140-184h）

3. **テスト・QA 基盤**（76-102h）
   - 自動テスト環境
   - CI パイプライン
   - ドキュメント整備

4. **セキュリティ強化**（64-82h）
   - 入力検証
   - プロジェクト暗号化
   - 安全な通信

---

## 各マイルストーンの詳細

### 1. レンダリング性能改善

**ドキュメント:** `docs/planned/MILESTONE_RENDERING_PERFORMANCE_2026-03-28.md`

#### 問題点

| 問題 | 影響 | 深刻度 |
|------|------|--------|
| テクスチャキャッシュ非効率 | 毎フレーム GPU 転送 | ★★★ |
| ギズモ描画の過剰呼び出し | 241 回/帧の GPU 呼び出し | ★★★ |
| シグナルストーム | 2-3 回の再レンダリング | ★★ |
| 不要な readback | CPU ブロッキング | ★★ |

#### 実装内容

```cpp
// Phase 1: テクスチャキャッシュ改善
// 修正前：インスタンスベースのキャッシュキー
qint64 cacheKey = image.cacheKey();

// 修正後：レイヤー ID ベース
qint64 cacheKey = qHash(layer->id().toString());

// Phase 2: ギズモ描画最適化
// 修正前：128 回の描画
for (float r = 0.5f; r <= radius; r += 1.0f) {
    drawCircle(x, y, r, color, 1.5f, false);
}

// 修正後：1 回の描画
drawSolidCircle(x, y, radius, color);
```

#### 期待効果

| 指標 | 現在 | 改善後 | 向上率 |
|------|------|--------|--------|
| **フレームレート** | 30-45fps | 60fps | +33-100% |
| **GPU 呼び出し** | 2000+/帧 | 500/帧 | -75% |
| **メモリ帯域** | 1GB/s | 200MB/s | -80% |

#### 実装順序

1. テクスチャキャッシュ改善（4-6h）
2. ギズモ描画最適化（3-4h）
3. シグナルストーム防止（2-3h）
4. 不要な readback 削除（3-4h）

---

### 2. アプリ層改善

**ドキュメント:** `docs/planned/MILESTONE_APP_LAYER_IMPROVEMENTS_2026-03-28.md`

#### 問題点

| 問題 | 場所 | 影響 |
|------|------|------|
| VideoLayer::generateProxy() 未実装 | ArtifactVideoLayer.cppm | 高解像度動画が重い |
| プロジェクト管理 TODO | ArtifactProject.cppm | 削除・ダーティ状態が不完全 |
| WebUI ブリッジ未実装 | ArtifactWebBridge.cppm | Web 制御ができない |
| インスペクター機能不足 | ArtifactInspectorWidget.cppm | 一部レイヤーで表示不良 |

#### 実装内容

```cpp
// Phase 1: VideoLayer Proxy 機能
bool ArtifactVideoLayer::generateProxy(ProxyQuality quality) {
    if (quality == ProxyQuality::None) {
        clearProxy();
        return true;
    }
    
    // FFmpeg でプロキシ生成
    QString ffmpegPath = findFFmpeg();
    QStringList args;
    args << "-i" << impl_->sourcePath_;
    
    switch (quality) {
        case ProxyQuality::Low:
            args << "-vf" << "scale=iw/4:ih/4";
            break;
        case ProxyQuality::Medium:
            args << "-vf" << "scale=iw/2:ih/2";
            break;
    }
    
    args << "-c:v" << "libx264" << "-crf" << "23";
    args << impl_->proxyPath_;
    
    return QProcess::execute(ffmpegPath, args) == 0;
}

// Phase 2: プロジェクト管理改善
// container_.remove() の実装
// ダーティ状態通知の追加
```

#### 期待効果

- 高解像度動画編集が軽量化
- プロジェクト管理の安定化
- WebUI からのフルコントロール

#### 実装順序

1. VideoLayer Proxy 機能（6-8h）
2. プロジェクト管理改善（4-6h）
3. レイヤー追加コマンド（6-8h）
4. WebUI ブリッジ実装（4-6h）
5. インスペクター改善（3-4h）

---

### 3. テスト・QA 基盤

**ドキュメント:** `docs/planned/MILESTONE_TEST_QA_INFRASTRUCTURE_2026-03-28.md`

#### 問題点

| 問題 | 現状 | 影響 |
|------|------|------|
| 単体テスト不足 | カバレッジ<20% | バグの早期発見が困難 |
| UI テスト未整備 | 手動テストのみ | 再現テストが困難 |
| CI 未導入 | 手動ビルド | リリースに時間 |
| ドキュメント不足 | API 文書なし | 新規参入が困難 |

#### 実装内容

```cpp
// Phase 1: 単体テスト基盤
TEST_CASE("VideoLayer loads file", "[VideoLayer]") {
    auto layer = std::make_shared<ArtifactVideoLayer>();
    
    REQUIRE(layer->loadFromPath("test.mp4") == true);
    REQUIRE(layer->isLoaded() == true);
    REQUIRE(layer->sourcePath() == "test.mp4");
}

TEST_CASE("AudioRenderer plays segment", "[Audio]") {
    AudioRenderer renderer;
    REQUIRE(renderer.openDevice(""));
    
    AudioSegment segment;
    renderer.enqueue(segment);
    
    REQUIRE(renderer.isActive());
}

// Phase 4: CI パイプライン
# .github/workflows/ci.yml
name: CI
on: [push, pull_request]
jobs:
  build:
    runs-on: windows-latest
    steps:
    - uses: actions/checkout@v2
    - name: Configure CMake
      run: cmake -B build -S .
    - name: Build
      run: cmake --build build
    - name: Test
      run: ctest --test-dir build --output-on-failure
```

#### 期待効果

| 指標 | 現在 | 改善後 |
|------|------|--------|
| **テストカバレッジ** | <20% | >60% |
| **バグ発見時期** | リリース後 | 開発中 |
| **リリース時間** | 1 週間 | 1 日 |

#### 実装順序

1. 単体テスト基盤（20-30h）
2. CI パイプライン（12-16h）
3. UI テスト（16-20h）
4. パフォーマンステスト（12-16h）
5. ドキュメント整備（16-20h）

---

### 4. セキュリティ強化

**ドキュメント:** `docs/planned/MILESTONE_SECURITY_HARDENING_2026-03-28.md`

#### 問題点

| 問題 | リスク | 影響 |
|------|--------|------|
| 入力検証不足 | バッファオーバーフロー | クラッシュ/脆弱性 |
| 暗号化なし | データ漏洩 | 機密情報流出 |
| 通信保護不足 | 中間者攻撃 | 認証情報窃取 |
| 監査ログなし | 不正操作検出不可 | セキュリティインシデント |

#### 実装内容

```cpp
// Phase 1: 入力検証
QString sanitizeFilePath(const QString& path) {
    // null 文字の除去
    QString sanitized = path.replace(QChar('\0'), "");
    
    // パストラバーサルの防止
    sanitized = QFileInfo(sanitized).canonicalFilePath();
    
    // ベースディレクトリの検証
    if (!sanitized.startsWith(allowedBasePath)) {
        qWarning() << "Invalid path access attempt:" << path;
        return "";
    }
    
    return sanitized;
}

// Phase 2: プロジェクト暗号化
class ProjectEncryption {
public:
    bool encrypt(const QString& inputPath, const QString& outputPath, 
                 const QString& password) {
        // パスワードから鍵を生成（PBKDF2）
        QByteArray key = deriveKey(password, salt_, 100000);
        
        // AES-256 で暗号化
        QByteArray encrypted = aes256Encrypt(data, key, iv_);
        
        // 出力
        QFile output(outputPath);
        output.open(QIODevice::WriteOnly);
        output.write(MAGIC_HEADER);
        output.write(salt_);
        output.write(iv_);
        output.write(encrypted);
        
        return true;
    }
};
```

#### 期待効果

| 指標 | 現在 | 改善後 |
|------|------|--------|
| **暗号化** | なし | AES-256 |
| **入力検証** | 一部 | 完全 |
| **通信保護** | 一部 | TLS 1.3 |

#### 実装順序

1. 入力検証（10-14h）
2. プロジェクト暗号化（12-16h）
3. 安全なネットワーク（10-12h）
4. 監査ログ（8-10h）
5. サンドボックス（16-20h）
6. 脆弱性スキャン（8-10h）

---

## 全体ロードマップ

### 第 1 四半期（1-3 ヶ月）

**目標:** 基盤性能の向上

| 週 | マイルストーン | 工数 |
|----|--------------|------|
| 1-2 | レンダリング性能 - Phase1,2 | 8h |
| 3-4 | レンダリング性能 - Phase3,4 | 8h |
| 5-8 | アプリ層改善 - Phase1,2 | 16h |
| 9-12 | アプリ層改善 - Phase3,4,5 | 16h |

### 第 2 四半期（4-6 ヶ月）

**目標:** 品質基盤の確立

| 週 | マイルストーン | 工数 |
|----|--------------|------|
| 13-18 | テスト・QA 基盤 - Phase1,2 | 40h |
| 19-22 | テスト・QA 基盤 - Phase3,4 | 24h |
| 23-26 | セキュリティ - Phase1,2 | 24h |

### 第 3 四半期（7-9 ヶ月）

**目標:** 完成・リリース準備

| 週 | マイルストーン | 工数 |
|----|--------------|------|
| 27-30 | セキュリティ - Phase3,4 | 20h |
| 31-34 | テスト・QA 基盤 - Phase5 | 16h |
| 35-38 | 全体テスト・バグ修正 | - |

---

## リスクと対策

### リスク 1: 工数超過

**リスク:** 各マイルストーンが予想より時間がかかる

**対策:**
- 段階的な実装
- 優先度の低い Phase は後回し
- 外部リソースの活用

### リスク 2: 依存関係

**リスク:** 他マイルストーンとの競合

**対策:**
- 依存関係の明確化
- 並行開発の調整
- 統合テストの頻繁な実行

### リスク 3: 技術的課題

**リスク:** 予期せぬ技術的問題

**対策:**
- 技術スパイクの実施
- 専門家への相談
- 代替案の準備

---

## 成功基準

### 定量的基準

| 指標 | 目標値 |
|------|--------|
| **フレームレート** | 60fps 維持 |
| **テストカバレッジ** | >60% |
| **クリティカルバグ** | 0 |
| **リリースサイクル** | 1 日 |

### 定性的基準

- ユーザーからの性能向上のフィードバック
- 開発者の生産性向上
- セキュリティ監査の合格

---

## 次のアクション

1. **レンダリング性能改善** から着手
2. 週次で進捗を確認
3. 四半期ごとに目標を見直し

---

**文書終了**
