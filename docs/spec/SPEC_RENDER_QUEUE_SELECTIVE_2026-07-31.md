# Render Queue 選択的レンダリング 要件定義

**日付**: 2026-07-31
**現状**: `ArtifactRenderJob` はフルコンポジションのフルフレームレンダリングのみ対応
**目標**: 「このレイヤーだけ」「この範囲だけ」をキューに送れる仕組み

---

## 1. RenderJob 拡張フィールド

既存の `ArtifactRenderJob` に追加するフィールド:

```cpp
class ArtifactRenderJob {
    // ... existing fields ...

    // === 新規: 範囲指定 ===
    enum class FrameRangeMode {
        Composition,      // コンポジション全体（既存の挙動）
        WorkArea,         // コンポジションのワークエリア
        Custom,           // startFrame/endFrame で指定（既存でも対応済み）
        SelectedFrames,   // タイムラインの選択フレーム
        SingleFrame       // 現在のフレームのみ
    };
    FrameRangeMode frameRangeMode = FrameRangeMode::Composition;

    enum class RegionMode {
        Full,             // コンポジション全体（既存）
        RegionOfInterest, // ROI 矩形でクロップ
        CustomCrop        // cropX/Y/W/H で指定
    };
    RegionMode regionMode = RegionMode::Full;
    int cropX = 0, cropY = 0, cropW = 0, cropH = 0;

    // === 新規: レイヤーフィルタ ===
    enum class LayerFilterMode {
        All,              // 全レイヤー（既存）
        Selected,         // 選択レイヤーのみ
        Solo,             // Solo 状態のレイヤーのみ
        Visible,          // 表示レイヤーのみ（非表示レイヤーをスキップ）
        Custom            // 指定したレイヤーIDリスト
    };
    LayerFilterMode layerFilterMode = LayerFilterMode::All;
    QList<ArtifactCore::LayerID> layerWhitelist;   // Custom モード時に使用
    QList<ArtifactCore::LayerID> layerBlacklist;   // 常に除外するレイヤー（全モード共通）
    bool excludeGuideLayers = false;   // ガイドレイヤー除外
    bool excludeAdjustmentLayers = false; // 調整レイヤー除外

    // === 新規: パス分割 ===
    struct RenderPassConfig {
        QString name;          // "Beauty", "3D Only", "2D Only" など
        LayerFilterMode layerFilter = LayerFilterMode::All;
        QList<ArtifactCore::LayerID> layerIds;
        bool enabled = true;
    };
    bool splitPasses = false;               // レイヤーグループ別に分割出力するか
    QList<RenderPassConfig> renderPasses;   // 分割パスの設定

    // === 新規: 解像度プリセット ===
    enum class ResolutionPreset {
        Custom,           // 任意指定（既存）
        Composition,      // コンポジション解像度
        Half,             // 1/2
        Third,            // 1/3
        Quarter           // 1/4
    };
    ResolutionPreset resolutionPreset = ResolutionPreset::Composition;
};
```

---

## 2. フレーム範囲モード

### 2.1 Composition（既存）
- コンポジション全体（frame 0 〜 コンポジション長）

### 2.2 WorkArea
```cpp
// コンポジションのワークエリア（B/I から N/O）を範囲にする
job.startFrame = composition->workAreaStart();
job.endFrame = composition->workAreaEnd();
job.frameRangeMode = FrameRangeMode::WorkArea;
```

### 2.3 SelectedFrames
- タイムラインで選択中のフレーム範囲
- 複数の非連続区間の場合はそれぞれ別ジョブとしてキューに追加

### 2.4 SingleFrame
- 現在のタイムライン位置のフレームのみ
- スナップショット用途に最適

---

## 3. レイヤーフィルタ

### 3.1 Selected（選択レイヤーのみ）
```cpp
// renderSingleFrameComposition / drawLayerForCompositionView に layerFilter を伝播
// 描画時にフィルタ外のレイヤーをスキップ:
if (layerFilterMode == Selected && !selectedLayerIds.contains(layer->id()))
    continue;
```

### 3.2 Solo
- レイヤーパネルで Solo が ON のレイヤーのみをレンダリング
- AE の標準的な Solo 挙動と同じ

### 3.3 Custom（ホワイトリスト/ブラックリスト）
- `layerWhitelist` が空でなければ、そのリストに含まれるレイヤーのみ
- `layerBlacklist` に含まれるレイヤーは常に除外
- 両方指定可能（例: 「背景以外のテキストレイヤーだけ」）

### 3.4 ガイドレイヤー除外
- コンポジション設定で「ガイドレイヤー」に指定されたレイヤーをスキップ
- AE と同様に、ガイドレイヤーは最終レンダリングではデフォルト非表示

---

## 4. 領域クロップ（Region of Interest）

### 4.1 ROI
- VP上で矩形選択した領域だけをレンダリング
- `cropX, cropY, cropW, cropH` で指定
- 出力解像度は cropW × cropH（またはそこからの倍率指定）

### 4.2 実装
```cpp
// renderSingleFrameComposition 内:
QImage fullFrame = /* render full frame */;
if (job.regionMode == RegionOfInterest) {
    QImage cropped = fullFrame.copy(job.cropX, job.cropY, job.cropW, job.cropH);
    fullFrame = cropped;
}
```

---

## 5. パス分割レンダリング

### 5.1 ユースケース
- 3D レイヤーと 2D レイヤーを別ファイルに書き出して外部コンポジット
- 背景と前景を別々にレンダリング
- マスク済みと非マスクを分離

### 5.2 RenderPassConfig
```cpp
// 例: 2パスに分割
job.splitPasses = true;
job.renderPasses = {
    { "3D Layers",   LayerFilterMode::Custom, {layer3D_id1, layer3D_id2} },
    { "2D Overlay",  LayerFilterMode::Custom, {layer2D_id1, layer2D_id2} },
};
```
出力は `output_3D_Layers_0001.png` と `output_2D_Overlay_0001.png` になる。

---

## 6. 解像度プリセット

### 6.1 プリセット値
| プリセット | 計算 |
|-----------|------|
| Composition | comp->settings().compositionSize() |
| Half | compSize / 2 |
| Third | compSize / 3 |
| Quarter | compSize / 4 |
| Custom | ユーザー指定の width/height |

### 6.2 適用タイミング
```cpp
void applyResolutionPreset(ArtifactRenderJob& job, const ArtifactCompositionPtr& comp) {
    QSize compSize = comp->settings().compositionSize();
    switch (job.resolutionPreset) {
    case Composition: job.resolutionWidth = compSize.width(); job.resolutionHeight = compSize.height(); break;
    case Half:        job.resolutionWidth = compSize.width()/2; job.resolutionHeight = compSize.height()/2; break;
    case Third:       job.resolutionWidth = compSize.width()/3; job.resolutionHeight = compSize.height()/3; break;
    case Quarter:     job.resolutionWidth = compSize.width()/4; job.resolutionHeight = compSize.height()/4; break;
    case Custom: break; // 既存の resolutionWidth/Height をそのまま使う
    }
}
```

---

## 7. キュー追加 UI フロー

### 7.1 メニューエントリ

```
Composition メニュー:
├── Add to Render Queue                       ← 既存（フルコンポジション）
├── Add Selection to Render Queue             ← 新規（選択レイヤーのみ）
│   ├── Current Frame Only                    ← 現在フレーム
│   ├── Work Area                             ← ワークエリア
│   └── Full Composition                      ← フルコンポジション
├── Add Work Area to Render Queue             ← 新規（範囲指定）
└── Add to Render Queue (Advanced)...         ← 新規（ダイアログ）

レイヤー右クリックメニュー:
├── Add Layer to Render Queue                 ← 新規
│   ├── Current Frame
│   ├── Work Area
│   └── Full Composition
```

### 7.2 Advanced ダイアログ（新規）

```
┌─────────────────────────────────────────────┐
│  Add to Render Queue                        │
│                                             │
│  Job Name: [Comp1_3DOnly____________]      │
│                                             │
│  ── Range ──                               │
│  ( ) Full Composition                      │
│  ( ) Work Area   [0 ───── 240]             │
│  ( ) Custom      [___] to [___]            │
│  ( ) Current Frame  [120]                  │
│                                             │
│  ── Layers ──                              │
│  ( ) All Layers                            │
│  (•) Selected Layers (3 selected)          │
│  ( ) Solo Layers                           │
│  ( ) Custom...  [Select Layers]            │
│  [x] Exclude Guide Layers                  │
│  [ ] Exclude Adjustment Layers             │
│                                             │
│  ── Region ──                              │
│  (•) Full Composition                      │
│  ( ) Region of Interest                    │
│      X:[___] Y:[___] W:[___] H:[___]       │
│                                             │
│  ── Resolution ──                          │
│  Preset: [Composition ▼]                   │
│  [ ] Split Passes  [Configure...]          │
│                                             │
│  ── Output ──                              │
│  Format: [MP4 ▼]  Path: [..._____]        │
│                                             │
│            [Cancel]  [Add to Queue]         │
└─────────────────────────────────────────────┘
```

---

## 8. 実装方針

### 8.1 既存コードへの変更範囲

| 変更箇所 | 内容 |
|----------|------|
| `ArtifactRenderJob` | 新規フィールド追加（enum, bool, list） |
| `renderSingleFrameComposition` | レイヤーフィルタ適用の分岐追加 |
| `renderComposition` (GPU) | レイヤーフィルタ + 領域クロップ追加 |
| `addCompositions` | パラメータ拡張 |
| `CompositionMenu` | 新規メニューエントリ追加 |
| `LayerMenu` | 新規メニューエントリ追加 |
| `RenderQueueManagerWidget` | Advanced ダイアログ（新規 Widget） |

### 8.2 後方互換性
- `LayerFilterMode::All` / `FrameRangeMode::Composition` がデフォルト
- 既存の API 呼び出しはそのまま動作

### 8.3 レンダリング時のフィルタ適用

```cpp
bool shouldRenderLayer(const ArtifactAbstractLayerPtr& layer,
                        const ArtifactRenderJob& job) {
    if (!layer) return false;

    // ガイドレイヤー除外
    if (job.excludeGuideLayers && layer->isGuideLayer()) return false;

    // 調整レイヤー除外
    if (job.excludeAdjustmentLayers && layer->isAdjustmentLayer()) return false;

    // ブラックリスト
    if (job.layerBlacklist.contains(layer->id())) return false;

    // モード別フィルタ
    switch (job.layerFilterMode) {
    case All:
        return true;
    case Selected:
        return job.layerWhitelist.contains(layer->id());
    case Solo:
        return layer->isSolo();
    case Visible:
        return layer->isVisible();
    case Custom:
        return job.layerWhitelist.isEmpty() || job.layerWhitelist.contains(layer->id());
    }
    return true;
}
```
