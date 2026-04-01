# Milestone: Face Detection & Auto-Mosaic (2026-04-01)

**Status:** Not Started
**Goal:** OpenCV による顔認識 → 自動モザイク/ぼかしエフェクト

---

## 現状

| 機能 | 状態 |
|------|------|
| OpenCV 連携基盤 (`CvUtils`) | ✅ 実装済み |
| QImage ↔ cv::Mat 変換 | ✅ 実装済み |
| OpenCV ベースエフェクト群 | ✅ 多数実装済み |
| 顔認識 (Haar Cascade / DNN) | ❌ 未実装 |
| 自動モザイク/ぼかし | ❌ 未実装 |
| 追従トラッキング | ❌ 未実装 |

---

## Architecture

```
FaceDetectionEngine (新規)
  ├── Haar Cascade / DNN による顔検出
  ├── 検出結果: 顔の矩形リスト + 信頼度
  └── トラッキング: 前フレームからの追従

AutoMosaicEffect (新規エフェクト)
  ├── 入力: 顔検出結果
  ├── 処理: 検出領域にモザイク/ぼかし適用
  ├── パラメータ: 強度、フェード、除外リスト
  └── 出力: 処理済み画像
```

---

## Phase 1: Face Detection Engine

### 実装内容
- `FaceDetectionEngine` クラス (ArtifactCore)
- Haar Cascade による高速顔検出
- DNN (OpenCV DNN) による高精度顔検出（オプション）
- 検出結果のキャッシュ（フレーム間）

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/ImageProcessing/OpenCV/FaceDetectionEngine.ixx` | ヘッダー |
| `ArtifactCore/src/ImageProcessing/OpenCV/FaceDetectionEngine.cppm` | 実装 |
| `ArtifactCore/resources/haarcascade_frontalface_default.xml` | カスケードファイル |

### 見積: 4h

---

## Phase 2: Auto Mosaic Effect

### 実装内容
- `AutoMosaicEffect` クラス (Artifact エフェクト)
- 検出領域へのモザイク/ぼかし適用
- パラメータ: 強度（ピクセルサイズ）、フェード、形状（矩形/円）

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `Artifact/include/Effects/AutoMosaicEffect.ixx` | ヘッダー |
| `Artifact/src/Effects/AutoMosaicEffect.cppm` | 実装 |

### 見積: 4h

---

## Phase 3: Tracking & Smoothing

### 実装内容
- 顔の追従（前フレームからの位置予測）
- 検出結果のスムージング（チラつき防止）
- 一時的な見失いに対する補間

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/ImageProcessing/OpenCV/FaceTracker.ixx` | ヘッダー |
| `ArtifactCore/src/ImageProcessing/OpenCV/FaceTracker.cppm` | 実装 |

### 見積: 4h

---

## Phase 4: Inspector UI & Integration

### 実装内容
- インスペクタでのパラメータ編集
- 検出結果のプレビュー表示
- 除外顔の選択（モザイクをかけない顔の指定）

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `ArtifactWidgets/src/Effect/AutoMosaicEditor.cppm` | エディタUI |

### 見積: 3h

---

## Recommended Order

| 順序 | フェーズ | 見積 |
|---|---|---|
| 1 | **Phase 1: Face Detection Engine** | 4h |
| 2 | **Phase 2: Auto Mosaic Effect** | 4h |
| 3 | **Phase 3: Tracking & Smoothing** | 4h |
| 4 | **Phase 4: Inspector UI** | 3h |

**総見積: ~15h**

---

## 既存の関連ファイル

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/ImageProcessing/OpenCV/CvUtils.ixx` | QImage ↔ cv::Mat 変換 |
| `ArtifactCore/include/ImageProcessing/OpenCV/BlurGAPI.ixx` | G-API ベースぼかし |
| `ArtifactCore/include/ImageProcessing/SharpenDirectionalBlur.ixx` | ぼかしエフェクト参考 |
| `ArtifactCore/include/ImageProcessing/OpenCV/Glow.ixx` | OpenCV エフェクト参考 |
| `Artifact/src/Effects/ArtifactAbstractEffect.ixx` | エフェクト基底クラス |

---

## 技術的注意点

1. **Haar Cascade vs DNN**
   - Haar Cascade: 高速だが精度は中程度
   - DNN (ResNet-10 / SSD): 高精度だが重い
   - プレビュー時は Haar Cascade、レンダリング時は DNN などの切り替えを検討

2. **カスケードファイルの配置**
   - OpenCV の `haarcascade_frontalface_default.xml` をプロジェクトリソースに含める
   - vcpkg で OpenCV をインストールしている場合、`share/opencv4/haarcascades/` に存在

3. **パフォーマンス**
   - 毎フレーム全検出は重いため、キーフレームで検出 → 間はトラッキング
   - ダウンサンプリングして検出 → 座標をスケールバック
