# Render Recovery & Layer Identity 実装レポート (2026-03-27)

**作成日:** 2026-03-27  
**ステータス:** 実装完了  
**関連コンポーネント:** ArtifactRenderQueueService, ArtifactLayerPanelWidget

---

## 概要

レンダーキューの失敗検出・再レンダリング機能（Render Recovery）と、レイヤータイプ別の視覚的識別機能（Layer Identity）を実装した。

---

## 目次

1. [Render Recovery](#1-render-recovery)
2. [Layer Identity](#2-layer-identity)
3. [テスト結果](#3-テスト結果)
4. [残余の課題](#4-残余の課題)
5. [関連ドキュメント](#5-関連ドキュメント)

---

## 1. Render Recovery

### 1.1 実装内容

**目的:**
- 出力失敗時に「どのフレームが失敗したか」を特定
- 失敗フレームの再レンダリングを可能に
- 連番出力と動画出力の両方に対応

### 1.2 実装詳細

#### FailedFrameInfo 構造体

```cpp
struct FailedFrameInfo {
    int jobId;              // ジョブ ID
    int frameNumber;        // フレーム番号（動画の場合は -1）
    QString errorMessage;   // エラーメッセージ
    qint64 timestamp;       // 検出時刻
};
```

#### FailedFrameDetector クラス

**ファイル:** `Artifact/src/Render/ArtifactRenderQueueService.cppm:647-700`

```cpp
class FailedFrameDetector {
public:
    QList<FailedFrameInfo> detectFailedFrames(const ArtifactRenderJob& job) {
        QList<FailedFrameInfo> failedFrames;
        const int startFrame = job.startFrame;
        const int endFrame = job.endFrame;
        QString outputPath = job.outputPath.trimmed();
        
        // 画像シーケンスかどうかを判定
        const QString ext = QFileInfo(outputPath).suffix().toLower();
        const bool isSequence = (ext == "png" || ext == "exr" ||
                                ext == "tiff" || ext == "tif" ||
                                ext == "jpg" || ext == "jpeg" ||
                                ext == "bmp");
        
        if (isSequence) {
            // シーケンスの場合、各フレームの存在をチェック
            for (int f = startFrame; f <= endFrame; ++f) {
                QString framePath = generateFramePath(outputPath, f);
                if (!QFile::exists(framePath)) {
                    failedFrames.append({job.id, f, "Frame not found", ...});
                } else if (QFileInfo(framePath).size() == 0) {
                    failedFrames.append({job.id, f, "Frame is empty", ...});
                }
            }
        } else {
            // 動画ファイルの場合、ファイルの存在とサイズをチェック
            if (!QFile::exists(outputPath)) {
                failedFrames.append({job.id, -1, "Output file not found", ...});
            } else if (QFileInfo(outputPath).size() == 0) {
                failedFrames.append({job.id, -1, "Output file is empty", ...});
            }
        }
        
        return failedFrames;
    }
};
```

#### 公開 API

**ファイル:** `Artifact/include/Render/ArtifactRenderQueueService.ixx:128-135`

```cpp
// Render Recovery: 失敗フレーム検出
struct FailedFrameInfo {
  int jobId;
  int frameNumber;
  QString errorMessage;
  qint64 timestamp;
};
QList<FailedFrameInfo> detectFailedFrames(int jobIndex) const;
int rerenderFailedFrames(int jobIndex, const QList<int>& frameNumbers);
```

### 1.3 使用方法

```cpp
// 失敗フレームの検出
auto* service = ArtifactRenderQueueService::instance();
QList<ArtifactRenderQueueService::FailedFrameInfo> failed = service->detectFailedFrames(jobIndex);

for (const auto& ff : failed) {
    qDebug() << "Job" << ff.jobId << "Frame" << ff.frameNumber << ":" << ff.errorMessage;
}

// 失敗フレームの再レンダリング
QList<int> frameNumbers;
for (const auto& ff : failed) {
    if (ff.frameNumber >= 0) {
        frameNumbers.append(ff.frameNumber);
    }
}
service->rerenderFailedFrames(jobIndex, frameNumbers);
```

---

## 2. Layer Identity

### 2.1 実装内容

**目的:**
- レイヤータイプを色で視覚的に識別可能に
- タイムライン上のレイヤー確認を容易に
- 編集中のレイヤーを強調表示

### 2.2 実装詳細

#### getLayerTypeColor 関数

**ファイル:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp:105-138`

```cpp
QColor getLayerTypeColor(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) return QColor(128, 128, 128);  // Gray - null
  
  if (auto img = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
   return QColor(100, 149, 237);  // CornflowerBlue - 画像
  }
  if (auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
   return QColor(255, 165, 0);    // Orange - 動画
  }
  if (auto text = std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
   return QColor(50, 205, 50);    // LimeGreen - テキスト
  }
  if (auto solid = std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
   return QColor(219, 112, 147);  // PaleVioletRed - ソリッド
  }
  if (auto svg = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
   return QColor(147, 112, 219);  // MediumPurple - SVG
  }
  if (auto audio = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   return QColor(255, 215, 0);    // Gold - オーディオ
  }
  if (auto camera = std::dynamic_pointer_cast<ArtifactCameraLayer>(layer)) {
   return QColor(0, 255, 255);    // Cyan - カメラ
  }
  if (auto particle = std::dynamic_pointer_cast<ArtifactParticleLayer>(layer)) {
   return QColor(255, 105, 180);  // HotPink - パーティクル
  }
  
  return QColor(128, 128, 128);  // Gray - その他
}
```

#### paintEvent での使用

**ファイル:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp:1841-1849`

```cpp
// レイヤータイプ別の色を取得
QColor layerTypeColor = getLayerTypeColor(l);

// 左端にタイプカラーバーを描画（4px）
p.fillRect(0, y, 4, rowH, layerTypeColor);

if (sel && !isGroupRow) p.fillRect(4, y, width() - 4, rowH, QColor(180, 110, 45));
else if (i == impl_->hoveredLayerIndex) p.fillRect(4, y, width() - 4, rowH, QColor(60, 60, 60));
else p.fillRect(4, y, width() - 4, rowH, (i % 2 == 0) ? QColor(42, 42, 42) : QColor(45, 45, 45));
```

### 2.3 レイヤータイプと色の対応

| レイヤータイプ | 色 | RGB |
|--------------|-----|-----|
| **画像** (Image) | CornflowerBlue | (100, 149, 237) |
| **動画** (Video) | Orange | (255, 165, 0) |
| **テキスト** (Text) | LimeGreen | (50, 205, 50) |
| **ソリッド** (Solid2D) | PaleVioletRed | (219, 112, 147) |
| **SVG** | MediumPurple | (147, 112, 219) |
| **オーディオ** (Audio) | Gold | (255, 215, 0) |
| **カメラ** (Camera) | Cyan | (0, 255, 255) |
| **パーティクル** (Particle) | HotPink | (255, 105, 180) |
| **その他** | Gray | (128, 128, 128) |

---

## 3. テスト結果

### 3.1 Render Recovery テスト

| テスト | 結果 | 備考 |
|--------|------|------|
| 画像シーケンスの欠損検出 | ✅ PASS | フレーム 5/100 が欠落している場合、正しく検出 |
| 空ファイルの検出 | ✅ PASS | 0 バイトのファイルを検出 |
| 動画ファイルの欠損検出 | ✅ PASS | 出力ファイルが存在しない場合を検出 |
| 正常なジョブの判定 | ✅ PASS | 失敗がない場合は空リストを返す |

### 3.2 Layer Identity テスト

| テスト | 結果 | 備考 |
|--------|------|------|
| 画像レイヤーの色 | ✅ PASS | CornflowerBlue が表示 |
| 動画レイヤーの色 | ✅ PASS | Orange が表示 |
| テキストレイヤーの色 | ✅ PASS | LimeGreen が表示 |
| ソリッドレイヤーの色 | ✅ PASS | PaleVioletRed が表示 |
| 複数レイヤーの識別 | ✅ PASS | 各レイヤーが色で区別可能 |
| 選択状態との両立 | ✅ PASS | 選択色とタイプカラーが競合しない |

---

## 4. 残余の課題

### 4.1 Render Recovery

| 課題 | 優先度 | 備考 |
|------|--------|------|
| 失敗フレーム一覧 UI の実装 | 中 | ダイアログまたはドックウィンドウ |
| 再レンダリングの進捗表示 | 中 | 既存の progress バーを流用 |
| バッチ再レンダリング | 低 | 複数ジョブの失敗フレームを一度に再レンダリング |
| エラーログの詳細化 | 低 | エラーコードやスタックトレースの保存 |

### 4.2 Layer Identity

| 課題 | 優先度 | 備考 |
|------|--------|------|
| アイコンの追加 | 中 | カラーバーに加えてアイコンも表示 |
| ユーザーカスタマイズ | 低 | 色の割り当てをユーザーが変更可能に |
| レイヤーグループの色 | 低 | グループ単位での色設定 |
| 検索・フィルタ機能 | 低 | 色でレイヤーをフィルタ |

---

## 5. 関連ドキュメント

- `docs/planned/MILESTONE_RENDER_OUTPUT_FEEL_REFINEMENT_2026-03-27.md` - Render/Output 改善マイルストーン
- `docs/planned/MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md` - Layer Solo View
- `docs/bugs/COMPOSITION_EDITOR_PERFORMANCE_2026-03-26.md` - Composition Editor パフォーマンス

---

## 付録 A: 変更ファイル一覧

| ファイル | 変更行数 | 内容 |
|---------|---------|------|
| `Artifact/include/Render/ArtifactRenderQueueService.ixx` | +12 行 | FailedFrameInfo 構造体と API 宣言 |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | +80 行 | FailedFrameDetector クラスと実装 |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | +40 行 | getLayerTypeColor 関数と paintEvent 修正 |

---

## 付録 B: 今後の拡張

### Render Recovery

1. **UI 実装** (2-3 時間)
   - 失敗フレーム一覧ダイアログ
   - 再レンダリング進捗表示

2. **機能強化** (4-6 時間)
   - エラーログの詳細化
   - 自動リトライ機構
   - 部分再レンダリングの最適化

### Layer Identity

1. **アイコン実装** (2-3 時間)
   - レイヤータイプ別のアイコン
   - カラーバーとアイコンの併用

2. **機能強化** (3-4 時間)
   - ユーザーカスタマイズ
   - 検索・フィルタ機能
   - レイヤーグループの色設定

---

**文書終了**
