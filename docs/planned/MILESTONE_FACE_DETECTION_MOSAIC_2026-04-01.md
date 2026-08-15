# Milestone: Face Detection & Auto-Mosaic (2026-04-01)

**最終更新:** 2026-08-15
**Status:** FaceDetection／FaceTracker／AutoMosaic の基盤実装済み、モデル運用・UI・runtime parity は未完了
**Goal:** OpenCV による顔認識 → 自動モザイク/ぼかしエフェクト

---

## 現状

| 機能 | 状態 |
|------|------|
| OpenCV 連携基盤 (`CvUtils`) | ✅ 実装済み |
| QImage ↔ cv::Mat 変換 | ✅ 実装済み |
| OpenCV ベースエフェクト群 | ✅ 多数実装済み |
| 顔認識 (Haar Cascade / DNN) | ⚠️ `AutoMosaicEffect` から detector を呼ぶ基盤あり。モデル運用は未完了 |
| 自動モザイク/ぼかし | ✅ `AutoMosaicEffect::apply()` の pixelate／Gaussian／median／feather |
| 追従トラッキング | ❌ FaceTracker接続は未確認 |

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


---

## Static audit follow-up (2026-07-25)

FaceDetectionEngine、FaceTracker、AutoMosaicEffect の実装と include／module 登録を確認した。AutoMosaic は face detection の有効化、検出領域への pixelate／Gaussian／median 処理、feather、強度等の property を持つため、文書冒頭の Not Started は現状と不一致である。

ただし、Haar／DNN の実モデル選択、検出キャッシュと追従平滑化の実運用、除外顔選択 UI、Inspector preview、OpenCV resource 配置、実フレーム検証は未確認である。Phase 1〜2 はソース実装済み、Phase 3 は基盤あり・検証待ち、Phase 4 は UI 統合未確認として更新する。

## 現行コード監査 (2026-08-15)

- `FaceDetectionEngine` は Haar Cascade／OpenCV DNN の設定、model path、`QImage`／`cv::Mat` 入力、検出結果と confidence を提供し、`FaceTracker` は検出矩形の対応付け・平滑化・見失いフレームの保持を実装している。
- `AutoMosaicEffect` は effect service に登録され、face detection の有効化、pixelate／Gaussian／median、strength／feather 等の property と検出領域への適用を持つ。通常の `MosaicEffect` には GPU compute と CPU fallback もあるが、AutoMosaic の顔検出処理は別の CPU／QImage 経路である。
- Haar cascade／DNN のモデル資産配置と AutoMosaic の detector／tracker の実際の runtime 接続、検出結果の frame cache、除外顔選択、Inspector 上の preview UI は静的コードからは受入れ確認できない。
- source から `QImage` 入力の CPU 処理が確認できるため、静止画・連番を優先する場合でも GPU／float buffer の合成本流へ移すか、互換境界として明示する整理が残る。実フレーム品質・追従安定性・performance は未計測。

判定: **顔検出／追従／自動モザイクの実装基盤と effect 登録は完了。モデル資産、UI 導線、cache／runtime 接続、品質・性能受入れは pending。**

## Update 2026-08-15 — Effect Stack 接続

`AutoMosaicEffect::apply()` を追加し、通常の `ArtifactAbstractEffect::applyConfigured()` 経路からも顔検出／手動領域の pixelate・Gaussian・median・feather を `ImageF32x4_RGBA` 上で適用できるようにした。これにより standalone の `QImage` helper だけでなく、layer／composition effect stack でAuto Mosaicを処理できる。

モデル資産、detector／trackerのframe cache接続、除外顔選択、Inspector preview、GPU相当経路、実フレーム品質・性能は未完了または未検証。
