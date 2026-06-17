# M-PERF-1 Performance Hot Path 修正 Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`,
      `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`,
      `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm`,
      `Artifact/src/Widgets/Control/ArtifactPlaybackControlTestWidget.cppm`,
      `Artifact/src/Render/ArtifactIRenderer.cppm`,
      `Artifact/src/Render/PrimitiveRenderer2D.cppm`,
      `Artifact/src/Render/ArtifactOffscreenRenderer2D.cppm`,
      `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`,
      `Artifact/src/Effects/*`,
      `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`,
      `Artifact/src/Service/ArtifactRenderQueueService.cppm`
位置づけ: `REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` で列挙した **重大度 S / A のボトルネック** を修正する。
参照:
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md`
- `docs/analysis/REPORT_CE_RENDER_ROI_2026-06-16.md`
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`
- `docs/planned/MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md`
- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`

---

## 1. 目的

`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §4.1 で抽出した重大度 S / A の問題を修正する:

- **S**: `update() in paintEvent` (3 件) / `QImage` の hot path 流入 (38 hit) / `GpuContext` 毎 frame alloc (16 effect)
- **A**: `QFile::exists()` in render path / `Readback (GPU→CPU)` 42 件 / `JSON parse` in render path / `Manual delete` 264 / `QSettings` 20

> 重要: 機能追加なし。**既存コードの修正のみ**。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 重大度 S (3 件)

| 問題 | 場所 | 件数 |
|---|---|---:|
| `update() in paintEvent` | `ArtifactProjectManagerWidget:6751` / `ArtifactPlaybackControlTestWidget:1238` / `ArtifactPlaybackControlWidget:1240` | 3 |
| `QImage` の hot path 流入 | 38 hit (各 layer / renderer) | 38 |
| `GpuContext` 毎 frame `make_unique` | Blur/Brightness/ChannelMixer/Colorama/ColorBalance/Curves/Exposure/Fill/Hue/Levels/PhotoFilter/Selective/Tritone/EdgeBloom/Glow/ReactiveGlow/Mosaic/Sharpen | 16 effect |

### 2.2 重大度 A

| 問題 | 件数 |
|---|---:|
| `QFile::exists()` in render path | 35+ |
| `Readback (GPU→CPU)` | 42 |
| `JSON parse` in render path | 34 |
| `Manual delete` | 264 |
| `QSettings` in render path | 20 |

---

## 3. 設計の柱

### 3.1 Paint Event 再帰の解消

3 件すべて `QTimer::singleShot(0, ...)` で deferred:

```cpp
// before
void Impl::paintEvent(QPaintEvent*) {
    // ...
    impl_->update();  // ← 再描画要求
}

// after
void Impl::paintEvent(QPaintEvent*) {
    // ...
    if (impl_->dirty_) {
        impl_->dirty_ = false;
        QTimer::singleShot(0, this, [this](){ update(); });
    }
}
```

- 即時再描画が必要な場合は `dirty` フラグで次 frame に持ち越し
- `ArtifactPlaybackControlWidget` 系は `frameChanged` signal + `Q_PROPERTY` で frame 状態通知

### 3.2 QImage 排除 (RENDER_FORMAT_CONTRACT 整合)

38 hit を **3 段階** で削減:

1. **Phase A**: `toQImage()` を legacy compatibility 専用にマーク (deprecation コメント)
2. **Phase B**: render path の **readback / surface 取得** で `QImage` を使わない API 追加
3. **Phase C**: `ArtifactRenderQueueService.cppm:2693` の text layer `convertToFormat` を `ImageF32x4_RGBA` 直接へ

代表修正:
- `ArtifactCompositionViewDrawing.cppm:346` — `buffer.toQImage()` を `buffer.image()` に
- `ArtifactPreviewCompositionPipeline.cppm:142` — `current.image().toQImage()` を `current.image()` に
- `ArtifactRenderQueueService.cppm:2693` — text layer `convertToFormat` を `ImageF32x4_RGBA` 直渡し
- `ArtifactSoftwareImageCompositor.cppm:90` — `matRGBAToQImage` を optional に
- `ArtifactOffscreenRenderer2D.cppm:229` — canvas 確保を `ImageF32x4_RGBA` で

### 3.3 GpuContext Cache 化 (16 effect)

各 effect の `Impl` に `std::unique_ptr<ArtifactCore::GpuContext>` を昇格:

```cpp
class BlurEffect::Impl {
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;  // 1 度だけ確保
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
public:
    void ensureContext(IRenderDevice* device, IDeviceContext* ctx) {
        if (!gpuContext_ || gpuContext_->device() != device) {
            gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device, ctx);
        }
    }
};
```

- 16 effect × `make_unique` / frame → 16 alloc / frame が **起動時 1 回** に
- `device` 切替時の再確保のみ

### 3.4 disk I/O 排除 (QFile::exists)

render path 内の `QFileInfo::exists()` を **すべて** `QFileSystemWatcher` cache に置換:

```cpp
class AssetExistenceCache {
public:
    static AssetExistenceCache& instance();
    bool exists(const QString& path);
    void refresh();  // 起動時に bulk
    void onFileChanged(const QString& path, bool exists);
};
```

- 起動時に全 asset に対して `exists()` 実行、cache 化
- `QFileSystemWatcher` で callback、dirty フラグ更新
- render path は `cache.exists(path)` のみ

### 3.5 Readback (GPU→CPU) 削減

`ArtifactIRenderer.cppm:774` の `drawSpriteLocal(x, y, w, h, QImage())` を **double buffer** 化:

```cpp
class ArtifactIRenderer {
    // before: 毎 frame readback
    // after: idle 時に 1 度だけ readback
    QImage readbackSprite(float2 pos, float2 size, int frame);  // idle only
};
```

- frame 描画中は `texture_` を直接使用
- HUD / thumbnail 用途は低解像度版 (1/4 サイズ) でキャッシュ
- `M-IR-8 ImmediateContext Boundary` と整合

### 3.6 JSON parse in render path 排除

`renderOneFrame` 周辺 34 hit のうち、**実 hot path** で 呼ぶ箇所を特定:

- `ArtifactRenderQueueService.cppm:1994` — encoder mutex 内部
- `ArtifactProjectService.cppm:810` — Asset monitor loop

→ 起動時に bulk parse → memory map に保持。render 時は cache lookup。

### 3.7 Manual delete → unique_ptr 移行 (264 → ~0)

代表箇所から段階的に:

- `ArtifactColorManagement.cppm:489` — `delete impl_` → `std::unique_ptr<Impl>`
- `ArtifactLayerFactory.cppm:148` — `new ArtifactXxxLayer` → `std::make_shared`
- `ArtifactPlaybackEngine.cppm:106` — `workerThread_ = new QThread()` → `std::unique_ptr<QThread>`

---

## 4. フェーズ計画

### Phase 1: Paint Event 再帰解消 (P0, 1 セッション)

- `ArtifactProjectManagerWidget:6751` 修正
- `ArtifactPlaybackControlTestWidget:1238` 修正
- `ArtifactPlaybackControlWidget:1240` 修正

**Done criteria:**
- 3 件すべて `QTimer::singleShot(0, ...)` 化
- 無限再描画の再現テストで停止

### Phase 2: GpuContext cache 化 (P0, 2〜3 セッション)

- 16 effect の `Impl` に `gpuContext_` 昇格
- 16 alloc / frame → 16 alloc / device lifetime

**Done criteria:**
- 16 effect で `make_unique` 呼出が frame 内に 0
- 起動時に 16 context 確保
- render benchmark で frame time 改善

### Phase 3: QImage hot path 排除 (P0, 2〜3 セッション)

- `toQImage()` を deprecation マーク
- `ArtifactCompositionViewDrawing.cppm:346` 修正
- `ArtifactPreviewCompositionPipeline.cppm:142` 修正
- `ArtifactRenderQueueService.cppm:2693` 修正
- `ArtifactSoftwareImageCompositor.cppm:90` 修正
- `ArtifactOffscreenRenderer2D.cppm:229` 修正
- `RENDER_FORMAT_CONTRACT` 整合テスト

**Done criteria:**
- hot path に `QImage` 構築が 0
- `RENDER_FORMAT_CONTRACT_2026-05-16.md` の linear canonical 経路
- legacy `toQImage()` は deprecation 警告のみ

### Phase 4: disk I/O 排除 (P1, 1〜2 セッション)

- `AssetExistenceCache` 実装
- `QFileSystemWatcher` 連携
- render path の `QFileInfo::exists()` を 35+ 箇所すべて置換

**Done criteria:**
- render path に `QFile::exists()` が 0
- 起動時に bulk cache
- file 変更時に callback で cache 更新

### Phase 5: Readback 削減 (P1, 2 セッション)

- `ArtifactIRenderer.cppm:774` を double buffer 化
- HUD / thumbnail 用途は低解像度版
- `M-IR-8 ImmediateContext Boundary` 整合

**Done criteria:**
- hot path に GPU→CPU readback が 0
- HUD は前回 frame の低解像度版
- frame time 改善

### Phase 6: JSON parse 排除 + Manual delete 移行 (P1, 1〜2 セッション)

- render path の JSON parse を bulk cache 化
- 264 hit の `delete` を段階的に `unique_ptr` へ

**Done criteria:**
- render path に `QJsonDocument::fromJson` が 0
- `delete` 文が 80% 以上削減
- 例外安全向上

### Phase 7: 計測 + Diagnostics (P1, 1 セッション)

- 修正前後の frame time を `M-DEBUG-1` の Performance overlay で比較
- Problem View に perf 健全性 contribution

**Done criteria:**
- 修正前: frame time X ms
- 修正後: frame time X / 2 ms
- Diagnostics に perf regression 検出

---

## 5. 不変条件 (Guardrails)

- 機能追加なし。**既存 API は温存**
- `QImage` 排除は **hot path のみ**。legacy compatibility は deprecation コメントのみ
- `GpuContext` cache は **device 寿命内** のみ。再確保しない
- `unique_ptr` 移行は **段階的**。既存コードを破壊しない
- 新規 signal-slot 接続は 0
- `QImage` / `setStyleSheet` 流入禁止 (taste 維持)
- 既存 milestone の Done Criteria を **破壊しない**

---

## 6. リスクと未解決論点

### 6.1 修正リスク

1. **`QTimer::singleShot(0, ...)` の 1 frame 遅延**。UI 応答性影響。Phase 1 で実機確認
2. **`GpuContext` cache**。device 切替時の再確保が抜ける可能性。ensureContext で device 比較
3. **`QImage` 排除**。既存テストが `toQImage()` 経由の pixel compare。deprecation コメントのみで当面維持
4. **Manual delete 移行**。既存テストが `delete` 想定。リファクタ範囲を限定

### 6.2 計測

- 修正前後の frame time を `M-DEBUG-1` の Performance overlay で計測
- 32 件すべて修正後に **平均 30% 改善** を目標

### 6.3 サブモジュール境界

- `ArtifactCore` 配下のみを書く (修正対象が Core の場合)
- `ArtifactWidgets` 触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- 3 件の `update() in paintEvent` が解消
- 16 effect で `GpuContext` cache 化
- render path に `QImage` 構築が 0
- render path に `QFile::exists()` が 0
- render path に GPU→CPU readback が 0
- render path に `QJsonDocument::fromJson` が 0
- `delete` 文が 80% 以上削減
- frame time が 30% 以上改善
- 既存 milestone の Done Criteria を破壊していない
- 新規 `QImage` / `setStyleSheet` / signal-slot が増えていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §4.1 / §4.2 の重大度 S / A を正式 milestone に起こした。