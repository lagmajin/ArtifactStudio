# ビデオプロキシ機能改善 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** ArtifactVideoLayer, FFmpegEncoder, ProxyManager, ArtifactProjectManagerWidget

---

## 概要

既存のビデオプロキシ機能を改善・発展させ、実用的なプロキシワークフローを実現する。

**現状:**
- `ProxyQuality` 列挙型は存在（None, Quarter, Half, Full）
- `generateProxy()` メソッドは存在するが未実装（TODO コメント）
- プロキシファイルのパス管理は実装済み
- プロキシ切り替えロジックは一部実装

---

## 発見された問題点

### ★★★ 問題 1: プロキシ生成の実装がない

**場所:** `Artifact/src/Layer/ArtifactVideoLayer.cppm:696`

```cpp
bool ArtifactVideoLayer::generateProxy(ProxyQuality quality)
{
    // ... 準備 ...
    
    // TODO: Implement actual proxy generation using FFmpeg or OpenCV
    // For now, just return false to indicate it's not implemented
    return false;  // ← 常に失敗
}
```

**影響:**
- プロキシ機能が使えない
- 高解像度動画が重いまま
- ワークフローの効率化ができない

**工数:** 8-12 時間

---

### ★★★ 問題 2: プロキシ切り替えが不完全

**場所:** `Artifact/src/Layer/ArtifactVideoLayer.cppm:647-658`

```cpp
void ArtifactVideoLayer::setProxyQuality(ProxyQuality quality)
{
    impl_->proxyQuality_ = quality;
    
    // Check if proxy file exists
    if (quality != ProxyQuality::None && !impl_->proxyPath_.isEmpty()) {
        if (QFile::exists(impl_->proxyPath_)) {
            // Load proxy instead
            qDebug() << "[VideoLayer] Switching to proxy:" << impl_->proxyPath_;
        }
        // ← ここで実際にプロキシを読み込む処理がない
    }
}
```

**影響:**
- プロキシに切り替わらない
- 高解像度のまま再生

**工数:** 4-6 時間

---

### ★★ 問題 3: プロキシマネージャーの未実装

**場所:** `Artifact/include/Layer/ArtifactVideoLayer.ixx:52`

```cpp
class ProxyManager;  // ← 前方宣言のみで実体がない
```

**影響:**
- 複数動画のプロキシ管理ができない
- プロキシのバッチ生成ができない
- プロキシキャッシュの管理ができない

**工数:** 10-14 時間

---

### ★★ 問題 4: プロキシ生成 UI の不足

**場所:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`

**現状:**
- 「Generate Proxies」ボタンは存在
- プログレスバーは存在
- 設定ダイアログがない（品質、形式、コーデック）

**工数:** 6-8 時間

---

### ★ 問題 5: プロキシキャッシュの管理

**場所:** `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm:1109`

```cpp
QDir(appDataDir).filePath(QStringLiteral("ProxyCache")),
```

**問題:**
- キャッシュの自動削除機能がない
- 容量制限がない
- 使用されていないプロキシの検出がない

**工数:** 6-8 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **プロキシ生成の実装** | 8-12h | 🔴 高 |
| **プロキシ切り替え改善** | 4-6h | 🔴 高 |

### P1（重要）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **プロキシマネージャー** | 10-14h | 🟡 中 |
| **プロキシ生成 UI** | 6-8h | 🟡 中 |

### P2（推奨）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **プロキシキャッシュ管理** | 6-8h | 🟢 低 |

**合計工数:** 34-48 時間

---

## Phase 構成

### Phase 1: プロキシ生成の実装

- 目的:
  - 実際のプロキシ生成を可能に

- 作業項目:
  - FFmpeg を使用したトランスコード
  - 低解像度エンコード
  - 進捗管理

- 完了条件:
  - 1920x1080 動画が 960x540（Half）で生成可能
  - 進捗が表示される

- 実装案:
  ```cpp
  bool ArtifactVideoLayer::generateProxy(ProxyQuality quality)
  {
      if (!impl_->isLoaded_) return false;
      
      // スケール計算
      double scale = 1.0;
      switch (quality) {
          case ProxyQuality::Quarter: scale = 0.25; break;
          case ProxyQuality::Half: scale = 0.5; break;
          default: return false;
      }
      
      int proxyWidth = static_cast<int>(impl_->streamInfo_.width * scale);
      int proxyHeight = static_cast<int>(impl_->streamInfo_.height * scale);
      
      // プロキシパス生成
      QFileInfo srcInfo(impl_->sourcePath_);
      QString proxyDir = srcInfo.absolutePath() + "/.proxy";
      QDir().mkpath(proxyDir);
      QString proxyName = QString("%1_proxy_%2.mp4")
                              .arg(srcInfo.baseName())
                              .arg(static_cast<int>(quality));
      impl_->proxyPath_ = proxyDir + "/" + proxyName;
      
      // FFmpeg でプロキシ生成
      QString ffmpegPath = findFFmpeg();
      if (ffmpegPath.isEmpty()) {
          qWarning() << "[VideoLayer] FFmpeg not found";
          return false;
      }
      
      QStringList args;
      args << "-i" << impl_->sourcePath_;
      args << "-vf" << QString("scale=%1:%2").arg(proxyWidth).arg(proxyHeight);
      args << "-c:v" << "libx264";
      args << "-crf" << "23";  // 画質（0-51、低いほど高画質）
      args << "-preset" << "fast";  // エンコード速度
      args << "-c:a" << "aac";  // オーディオ
      args << "-b:a" << "128k";
      args << "-y";  // 上書き
      args << impl_->proxyPath_;
      
      qDebug() << "[VideoLayer] Generating proxy:" << impl_->proxyPath_
               << "Size:" << proxyWidth << "x" << proxyHeight;
      
      // 実行
      QProcess process;
      process.start(ffmpegPath, args);
      
      if (!process.waitForFinished(-1)) {
          qWarning() << "[VideoLayer] FFmpeg failed:" << process.errorString();
          return false;
      }
      
      if (process.exitCode() != 0) {
          qWarning() << "[VideoLayer] FFmpeg exited with code:" << process.exitCode();
          qWarning() << "stderr:" << process.readAllStandardError();
          return false;
      }
      
      // 成功
      qDebug() << "[VideoLayer] Proxy generated successfully";
      return true;
  }
  ```

### Phase 2: プロキシ切り替え改善

- 目的:
  - プロキシとオリジナルの seamless 切り替え

- 作業項目:
  - プロキシ読み込み
  - フレームデコードの切り替え
  - キャッシュの切り替え

- 完了条件:
  - プロキシ設定で即座に切り替え
  - 再生が途切れない

- 実装案:
  ```cpp
  void ArtifactVideoLayer::setProxyQuality(ProxyQuality quality)
  {
      impl_->proxyQuality_ = quality;
      
      // プロキシ使用時はプロキシを読み込む
      if (quality != ProxyQuality::None && !impl_->proxyPath_.isEmpty()) {
          if (QFile::exists(impl_->proxyPath_)) {
              qDebug() << "[VideoLayer] Switching to proxy:" << impl_->proxyPath_;
              
              // プロキシファイルを開く
              if (impl_->playbackController_->openMediaFile(impl_->proxyPath_)) {
                  qDebug() << "[VideoLayer] Proxy opened successfully";
                  
                  // キャッシュをクリア（プロキシ用に再構築）
                  impl_->frameCache_.clear();
                  
                  // 現在のフレームを再デコード
                  decodeCurrentFrame();
              } else {
                  qWarning() << "[VideoLayer] Failed to open proxy";
              }
          }
      } else {
          // オリジナルに戻る
          if (!impl_->sourcePath_.isEmpty()) {
              qDebug() << "[VideoLayer] Switching back to original";
              impl_->playbackController_->openMediaFile(impl_->sourcePath_);
              impl_->frameCache_.clear();
              decodeCurrentFrame();
          }
      }
  }
  
  QImage ArtifactVideoLayer::currentFrameToQImage() const
  {
      // プロキシ使用時はプロキシから、そうでなければオリジナルから
      return impl_->currentQImage_;
  }
  ```

### Phase 3: プロキシマネージャー

- 目的:
  - 複数動画のプロキシ管理

- 作業項目:
  - プロキシマネージャークラス
  - バッチ生成
  - 状態管理

- 完了条件:
  - 複数動画のプロキシを一括生成
  - 進捗管理

- 実装案:
  ```cpp
  class ProxyManager : public QObject {
      Q_OBJECT
      
  public:
      struct ProxyJob {
          QString sourcePath;
          QString proxyPath;
          ProxyQuality quality;
          bool completed = false;
          bool failed = false;
          int progress = 0;
      };
      
      void addProxyJob(const QString& sourcePath, ProxyQuality quality) {
          ProxyJob job;
          job.sourcePath = sourcePath;
          job.quality = quality;
          job.proxyPath = generateProxyPath(sourcePath, quality);
          jobs_.append(job);
      }
      
      void startBatchGeneration() {
          currentJobIndex_ = 0;
          processNextJob();
      }
      
  signals:
      void jobProgress(int jobIndex, int progress);
      void jobCompleted(int jobIndex);
      void batchCompleted();
      
  private slots:
      void processNextJob() {
          if (currentJobIndex_ >= jobs_.size()) {
              emit batchCompleted();
              return;
          }
          
          ProxyJob& job = jobs_[currentJobIndex_];
          
          // VideoLayer でプロキシ生成
          auto layer = std::make_shared<ArtifactVideoLayer>();
          if (layer->loadFromPath(job.sourcePath)) {
              if (layer->generateProxy(job.quality)) {
                  job.completed = true;
                  emit jobCompleted(currentJobIndex_);
              } else {
                  job.failed = true;
              }
          }
          
          currentJobIndex_++;
          processNextJob();
      }
      
  private:
      QList<ProxyJob> jobs_;
      int currentJobIndex_ = 0;
      
      QString generateProxyPath(const QString& source, ProxyQuality quality) {
          QFileInfo srcInfo(source);
          QString proxyDir = srcInfo.absolutePath() + "/.proxy";
          QDir().mkpath(proxyDir);
          QString proxyName = QString("%1_proxy_%2.mp4")
                                  .arg(srcInfo.baseName())
                                  .arg(static_cast<int>(quality));
          return proxyDir + "/" + proxyName;
      }
  };
  ```

### Phase 4: プロキシ生成 UI

- 目的:
  - ユーザーが簡単にプロキシ生成

- 作業項目:
  - 設定ダイアログ
  - 品質選択
  - 進捗表示

- 完了条件:
  - ワンクリックでプロキシ生成
  - 進捗が可視化

### Phase 5: プロキシキャッシュ管理

- 目的:
  - キャッシュの自動管理

- 作業項目:
  - 容量制限
  - 自動削除
  - 使用状況の監視

- 完了条件:
  - 容量超過で自動削除
  - 未使用プロキシの検出

---

## 技術的課題

### 1. FFmpeg の依存

**課題:**
- FFmpeg が必要
- ビルド設定

**解決案:**
- オプション依存として扱う
- 内蔵バイナリのバンドル
- 代替実装（OpenCV）

### 2. エンコード時間

**課題:**
- プロキシ生成に時間がかかる

**解決案:**
- バッチ処理
- バックグラウンド処理
- 並列エンコード

### 3. ディスク容量

**課題:**
- プロキシファイルで容量を消費

**解決案:**
- 容量制限
- 自動削除
- 圧縮率の調整

---

## 期待される効果

### 性能向上

| 指標 | 現在 | 改善後 | 向上率 |
|------|------|--------|--------|
| **高解像度動画再生** | 重い | 軽い | +200% |
| **スクラブ応答** | 遅い | 速い | +300% |
| **メモリ使用量** | 大 | 小 | -75% |

### ワークフロー

- 4K/8K 動画も軽快に編集
- プロキシとオリジナルを切り替え
- 書き出しはオリジナル品質

---

## 関連ドキュメント

- `docs/planned/MILESTONE_APP_LAYER_IMPROVEMENTS_2026-03-28.md` - アプリ層改善
- `docs/planned/MILESTONE_RENDERING_PERFORMANCE_2026-03-28.md` - レンダリング性能

---

## 実装順序の推奨

1. **Phase 1: プロキシ生成** - 基盤機能
2. **Phase 2: プロキシ切り替え** - 必須機能
3. **Phase 3: プロキシマネージャー** - 効率化
4. **Phase 4: UI** - 使いやすさ
5. **Phase 5: キャッシュ管理** - 保守性

---

**文書終了**
