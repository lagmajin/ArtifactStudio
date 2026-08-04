# プレーナートラッカー 実装マイルストーン

**日付**: 2026-08-01
**ベース**: Mocha Pro / Nuke PlanarTracker / AE 3D Camera Tracker
**現状**: `MotionTracker` に `TrackerType::Planar` と `TrackingMethod::OpticalFlow` の enum 定義あり。`NccTracker`（NCCテンプレートマッチング）実装済み。プレーナートラッキングの本格的な実装は不在。
**狙い**: 平面（ビルボード、壁、床など）を4点で囲み、その変形をトラッキングしてホモグラフィ行列を出力する

---

## プレーナートラッカーとは

Mocha の Planar Tracker:
1. ユーザーが追跡対象の平面をスプラインツールで囲む
2. 平面上のテクスチャ特徴を追跡（オプティカルフロー + 特徴点）
3. フレーム間の平面射影変換（ホモグラフィ H 行列）を計算
4. 結果を CornerPin エフェクトや3Dカメラソルバーに渡す

出力: `H(t) = 3×3 homography matrix per frame`

---

## Phase 1: 平面トラッキングコア

### Step 1.1 — 特徴点検出 + マッチング
既存の `MotionTracker` に Planar モードの実装を追加。

**変更**: `ArtifactCore/src/Tracking/MotionTracker.cppm`

```
PlanarTracker::track() の流れ:

1. 初期化（1フレーム目）
   a. ユーザー指定の4点 (p0,p1,p2,p3) が囲む領域を取得
   b. 領域内で GoodFeaturesToTrack (Shi-Tomasi) で特徴点検出
   c. 特徴点座標をバウンディングボックス正規化空間に変換
   d. 各点の SIFT/ORB 特徴量を計算（後で再マッチング用）

2. フレーム間追跡（t → t+1）
   a. calcOpticalFlowPyrLK() で特徴点を追跡
   b. RANSAC で外れ値を除去しホモグラフィ H を推定
   c. 特徴点数が閾値以下になったら特徴点を再検出 + 再初期化
   d. H を出力

3. 再検出フェイルセーフ
   a. 追跡に失敗したら t-1 の特徴量で現在フレームをテンプレートマッチング
   b. マッチしたら RANSAC で H を再推定
   c. それでもダメならユーザーに通知 / 次のフレームまで補間
```

### Step 1.2 — PlanarTrackResult 型
```cpp
struct PlanarTrackResult {
    int frame;
    QMatrix4x4 homography;  // 3×3（4×4の左上3×3に格納）
    float confidence;       // 0-1
    int inlierCount;        // RANSAC インライア数
    int totalFeatures;      // 追跡に使った特徴点数
    bool needsReinit;       // 再初期化推奨フラグ
};

struct PlanarTrackSession {
    // 入力
    std::vector<QPointF> roiCorners;  // 4点（時計回り）
    int startFrame, endFrame;
    TrackingQuality quality;
    float minFeatureConfidence = 0.7f;

    // 出力
    std::map<int, PlanarTrackResult> frameResults; // frame → result
    PlanarTrackResult* resultAtFrame(int frame);

    // 特徴点キャッシュ
    std::vector<cv::Point2f> lastTrackedPoints;
    cv::Mat lastDescriptors;  // ORB or SIFT
    cv::Mat referenceImage;   // 初期フレームのROI領域
};
```

---

## Phase 2: ホモグラフィの応用

**2026-08-04 実装済み**: Planar モードの4点＋ROI登録、厳格なホモグラフィ追跡、結果の Corner Pin キーフレーム書き出し、TrackPoint コンテキストメニューからの Planar 切り替えを接続。

### Step 2.1 — CornerPin との連携
既存の `CornerPinEffect` と接続:

```cpp
// PlanarTracker → CornerPin エフェクトにキーフレーム書き出し
void applyPlanarTrackToCornerPin(
    ArtifactCornerPinEffect* effect,
    const std::map<int, PlanarTrackResult>& results,
    double fps
);
```

### Step 2.2 — Insert（差し込み合成）
トラッキング結果から「映像を壁に貼り付ける」操作:

```
1. ソース画像を H で変形
2. 変形画像を背景にアルファ合成
3. 必要に応じてブレンド（照明変化の補正）
```

### Step 2.3 — Remove（オブジェクト除去）
トラッキング領域の周囲から背景を補完し、オブジェクトを除去:

```
1. トラッキング領域を中心にしたクリーンプレート生成
2. 領域外のテクスチャでインペインティング（OpenCV inpaint）
3. H の逆行列で元の位置に戻す
```

---

## Phase 3: スプラインベースの ROI 指定

### Step 3.1 — VP 上の ROI 描画ツール
**変更**: `ArtifactCompositionRenderController.cppm`

```
PlanarTrackerTool:
- 4点以上の閉じたスプラインをVP上で描画
- 頂点はドラッグ可能
- トラッキング範囲のプレビュー（特徴点密度のヒートマップ表示）
- トラック結果のプレビュー（4点の軌跡 + ワイヤーフレーム）
```

### Step 3.2 — 自動 ROI 提案
エッジ検出 + 直線検出（HoughLines）で長方形領域を自動提案:

```cpp
std::vector<std::vector<QPointF>> autoDetectPlanes(
    const ImageF32x4_RGBA& frame,
    int maxPlanes = 5
);
```

---

## Phase 4: 3D カメラソルバー連携

### Step 4.1 — 複数平面からのカメラ推定
複数の PlanarTrackResult から3Dカメラの動きを復元:

```
1. 各平面の H 行列から消失点を抽出
2. 2つ以上の平面でカメラ内部パラメータ（焦点距離）を推定
3. 複数フレームの平面の動きからカメラ外部パラメータ（R|t）を推定
4. ArtifactCameraLayer にキーフレーム書き出し
```

---

## Phase 5: パフォーマンス最適化

### Step 5.1 — ピラミッド処理
低解像度（1/4）で粗く追跡 → フル解像度でリファイン。

### Step 5.2 — バックグラウンド追跡
`BackgroundTaskWorkerPool` で全フレームの追跡を非同期実行。
VP操作をブロックしない。

### Step 5.3 — GPU オプティカルフロー
既存の `TrackingMethod::OpticalFlow` の OpenCV 実装を Diligent GPU コンピュートに置き換え。
→ フレーム間追跡をGPUで実行（1000特徴点でも <2ms）

---

## ファイル一覧

| フェーズ | ファイル | 新規/変更 | 内容 |
|---------|----------|----------|------|
| P1 | `ArtifactCore/include/Tracking/PlanarTracker.ixx` | 新規 | PlanarTracker インターフェース |
| P1 | `ArtifactCore/src/Tracking/PlanarTracker.cppm` | 新規 | コア追跡アルゴリズム |
| P1 | `ArtifactCore/Tracking/MotionTracker.cppm` | 変更 | Planar モード実装追加 |
| P2 | `Artifact/include/Effects/PlanarTrackEffect.ixx` | 新規 | トラック→CornerPin エフェクト |
| P2 | `Artifact/src/Effects/PlanarTrackEffect.cppm` | 新規 | 実装 |
| P2 | `ArtifactCornerPinEffect.cppm` | 変更 | H行列受取口追加 + バグ修正 |
| P3 | `ArtifactCompositionRenderController.cppm` | 変更 | PlanarTracker ツール + VP UI |
| P3 | `ArtifactCore/Tracking/PlaneDetector.cppm` | 新規 | 自動平面検出（HoughLines） |
| P4 | `ArtifactCore/include/Tracking/CameraSolver.ixx` | 新規 | 3Dカメラ復元 |
| P4 | `ArtifactCore/src/Tracking/CameraSolver.cppm` | 新規 | 実装 |
| P5 | `ArtifactCore/src/Tracking/PlanarTrackerGPU.cppm` | 新規 | GPU高速化 |

---

## 検証チェックリスト

- [ ] 単純な平行移動シーンで4点トラッキングが動作（誤差 <1px）
- [ ] スケール変化（ズームイン）で追跡が維持される
- [ ] 回転（45°）で追跡が維持される
- [ ] 斜めからのパース変化で追跡が維持される
- [ ] 特徴点喪失時に再初期化が自動実行される
- [ ] RANSAC外れ値除去が照明変化・隠蔽に強い
- [ ] 出力ホモグラフィをCornerPinに適用して映像が壁に貼り付けられる
- [ ] 2平面から3Dカメラが復元される（焦点距離既知の場合）
- [ ] Mocha Pro の Planar Tracker と同程度の精度（NCC >0.95）
- [ ] バックグラウンド追跡中もVP操作が可能
