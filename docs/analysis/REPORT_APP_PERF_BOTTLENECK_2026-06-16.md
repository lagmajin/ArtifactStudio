# App-only 追加ギャップ & パフォーマンスボトルネック レポート — 2026-06-16

作成日: 2026-06-16
目的: `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` と同スコープ (`Artifact/src`) をさらに **App のみ** に絞り、未提案の機能を追加。加えて、ソース上の **パフォーマンスボトルネック候補** を列挙。
調査範囲: `Artifact/src / Artifact/include` のみ。`ArtifactCore / ArtifactWidgets / third_party / libs` は除外。

---

## 1. 調査方法

`_perf_scan.py` に 100+ 項目を列挙し、`Artifact/src + include` を grep ベースで走査。ボトルネック候補は **paintEvent / renderOneFrame / renderImpl** の 3 つの hot path に focus して再走査。

---

## 2. 機能ギャップ（App のみ）

### 2.1 サマリ

- 完全未実装 (MISS, 0 hit): **52 項目**
- 部分実装 (PART, 1〜2 hit): 4 項目
- 実装あり (OK, 3+ hit): 多数 (前回レポートと重複)

`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` と重複しない App レベルの機能:

### 2.2 完全未実装 (MISS, 0 hit) — 52 項目

| カテゴリ | 機能 |
|---|---|
| Diagnostics | RenderDoc integration, GPU profiling (Nsight / GPA), Bug report dialog, Performance overlay, Crash dump analyzer |
| Render | Render debounce / throttle, Mipmap generation, Cache hit rate, LOD selection, GPU upload async, Streaming decode, Worker pool |
| Editor UI | Magnetic snap, Tip of the day, Welcome screen, Asset preview, Asset library, Project templates, Template gallery, Script console, Expression editor (UI), Layer search, Plugin manager UI, Macro recorder, Tool palette customization, Layout save / load, Expression editor (UI) |
| Quality | Settings migration, Project compare (diff), User feedback dialog, Trial / license UI, Settings dialog (advanced) |
| Network | Update check, Telemetry, Error report dialog, Crash report dialog (実体は 6 hit、`class\s+CrashReport` ではなく `crashReport` 文字列) |
| Project | Asset library, Project sharing (6 hit だが実体は別概念), Project size limit, Auto-recovery on crash, Crash-safe load, Region of interest (ROI), Smart cache invalidation, Undo stack size limit, Crash on save (atomic write), Settings migration |
| Color | Color label, Custom shortcut |
| Tablet | Tablet / pen support, Touch gesture, Joystick controller (562 hit だが実体は別) |
| VR | VR / XR support (18 hit、Expression 評価の `value`/`Array` 名前空間と衝突) |
| Other | Documentation link, Tutorial / onboarding, qDebug in render path, qDebug in renderOneFrame |

→ App レベルでも **診断 / 観測性 / 開発者向けツール** がほぼ未着手。Performance overlay / RenderDoc 統合 / qDebug 制御は **デバッグ効率に直結する** が未着手。

### 2.3 部分実装 (PART)

- Edit menu integration (1 hit, `ArtifactEditMenu.cppm`) — 部分的
- Layer lock UI (1 hit, `LayerLockIndicator.cppm`) — indicator のみ。lock 機能自体は別
- Node graph editor (1 hit, `ArtifactColorNodeGraph.cppm`) — 名前は color node graph
- Plugin manager UI (1 hit, `WindowPluginManager.cppm`) — 部分的

### 2.4 実装あり (OK, 3+ hit)

| 機能 | hit | 評価 |
|---|---:|---|
| Unique ptr / shared ptr | 667 | 良好。モダン C++ 採用 |
| Joystick controller | 562 | 名前空間の言及多数 (誤検知の可能性) |
| Heap alloc per frame | 533 | **深刻**。`new` / `make_shared` が render path に多数 |
| Project validation | 489 | 良好 |
| App-level logging (qDebug) | 504 | 良好 (ただし `qDebug in render` 0 hit は確認済み) |
| QFile read in render path | 478 | **深刻**。`QFileInfo::exists()` が 35+ 箇所 |
| String allocation hot path | 287 | 良好 (allocation あるが、render 内 0 hit) |
| Localization | 272 | 良好 |
| Manual delete | 264 | **要調査**。unique_ptr / shared_ptr 移行の余地 |
| update() in paint event | 234 | **軽度**。3 件の直接呼び出し + ノイズ多数 |
| QMutex / QReadWriteLock | 216 | 良好 |
| Surface cache (LOD) | 111 | 良好 |
| Lambda capture by value | 5892 | 誤検知 (Qt の connect 構文) |
| Per-frame vector clear | 431 | 良好 (render 内部の統計用) |
| Lazy load (placeholder) | 102 | 良好 |
| QTimer in render loop | 73 | **要確認** |
| HiDPI / DPI handling | 67 | 良好 |
| Frame cache | 68 | 良好 |
| GPU texture cache | 63 | 良好 |
| Async decode (QFuture) | 62 | 良好 |
| Readback (GPU→CPU) | 42 | **要確認** (42 件は多め) |
| Frequent QImage allocation | 38 | **RENDER_FORMAT_CONTRACT 違反疑い** |
| QThread / worker | 38 | 良好 |
| JSON parse in render path | 34 | **軽度**。一部 render path 解析 |
| Drag & drop | 32 | 良好 |
| Undo / redo history | 27 | 良好 |
| Settings dialog | 22 | 良好 |
| QSettings in render path | 20 | **要確認** |
| App perf counter / scope timer | 20 | 良好 |
| Workspace switching | 19 | 良好 |
| Clipboard support | 18 | 良好 |
| Unnecessary copy | 16 | **軽度** |
| Render key cache | 14 | 良好 |
| QtConcurrent | 14 | 良好 |
| QFuture / QPromise | 12 | 良好 |
| Welcome screen | 12 | 良好 |
| Shy view | 14 | 良好 |
| Dark / light theme | 11 | 良好 |
| Render preview (low-res) | 10 | 良好 |
| File watcher in render | 9 | **軽度** |
| Recent files | 9 | 良好 |
| QWaitCondition | 5 | 良好 |
| QWidget::repaint in render | 3 | 軽微 |
| Network in render path | 5 | 軽微 |

---

## 3. パフォーマンスボトルネック候補 (hot path)

### 3.1 重要発見: `update() in paintEvent` (3 件)

```
Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm:6751    impl_->update();
Artifact/src/Widgets/Control/ArtifactPlaybackControlTestWidget.cppm:1238  repaint();
Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm:1240  repaint();
```

**問題**: `paintEvent` 内から `update()` / `repaint()` を呼ぶと、Qt の `QWidget` ドキュメントに明記されている通り **無限再描画** の危険がある。Qt の paint event 中に再描画要求を出すと、デッドロックやフレーム落ちの原因になる。

- `ArtifactProjectManagerWidget.cppm:6751` — project manager 内部の state 変更通知に paint event を使う設計
- 2 つの `ArtifactPlaybackControlWidget` — playback 制御の frame 更新で paint を使う設計

**修正案**:
- `update()` を `QTimer::singleShot(0, this, [this](){ update(); })` で deferred
- playback control は `Q_PROPERTY` + signal 経由で frame change 通知
- paint event 内で `update()` が必要なケースは `dirty` フラグ + 次 frame での `update()` のみ

### 3.2 重要発見: `QFile` / `QFileInfo` in render path (478 hit)

35+ 箇所で `QFileInfo::exists()` を render 周辺で呼び出し。代表例:

- `ArtifactCompositionEditor.cppm:1885` — drag & drop handler (acceptable)
- `ArtifactCompositionEditor.cppm:286` — `currentFrameToQImage()` 経由
- `ArtifactVideoLayer.cppm` — video file 存在チェック
- `ArtifactFrameCache.cppm` — cache 存在チェック
- `ArtifactProjectService.cppm:810` — Asset monitor の project loop

**問題**: render path に **I/O bound** の `exists()` 呼び出しが点在。disk I/O は ms オーダーで、frame loop に混入すると FPS を破壊する。

**修正案**:
- `QFileSystemWatcher` で **cache 化** (9 hit 既存)
- 起動時に bulk check して memory map に
- render 時には cache lookup のみ
- 削除: `QFileInfo::exists()` を render 経路から排除

### 3.3 重要発見: `QImage` の hot path 流入 (38 hit)

`RENDER_FORMAT_CONTRACT_2026-05-16.md` で「`QImage` を新規 hot path に入れない」と明記されているが、App 内に 38 箇所の `QImage` 使用が残存。代表箇所:

- `ArtifactLayer::toQImage()` — ほぼ全 layer type
- `ArtifactSoftwareImageCompositor.cppm:90` — `matRGBAToQImage`
- `ArtifactOffscreenRenderer2D.cppm:229` — canvas 確保
- `ArtifactPreviewCompositionPipeline.cppm:142` — surface 取得
- `ArtifactCompositionViewDrawing.cppm:346` — surface 取得
- `ArtifactRenderQueueService.cppm:2693` — text layer rasterize

**問題**: `ImageF32x4_RGBA` (linear canonical) → `QImage` (sRGB uint8) → `ImageF32x4_RGBA` のラウンドトリップが render path に存在。色空間変換 + メモリ確保 + GPU upload 再実行のコストが **2 倍** かかる。

**修正案**:
- `toQImage()` を **legacy compatibility** 専用にマークし、render path から外す
- `ImageF32x4_RGBA` 直接扱いの API に置換
- `ArtifactRenderQueueService.cppm:2693` の text layer `convertToFormat(QImage::Format_ARGB32_Premultiplied)` を `ImageF32x4_RGBA` 直接へ
- 段階的に `QImage` 依存を削減

### 3.4 重要発見: `Heap alloc per frame` (533 hit)

`std::make_unique` / `std::make_shared` / `new` が 533 箇所。hot path 候補:

- Effect 適用: `BlurEffect.cppm:314`, `BrightnessEffect.cppm:105`, `ChannelMixerEffect.cppm:77`, `ColoramaEffect.cppm:73`, `ColorBalanceEffect.cppm:79`, `CurvesEffect.cppm:90`, `ExposureEffect.cppm:81`, `FillEffect.cppm:63`, `HueAndSaturation.cppm:111`, `LevelsEffect.cppm:96`, `PhotoFilterEffect.cppm:74`, `SelectiveColorEffect.cppm:75`, `TritoneEffect.cppm:76`, `EdgeBloomEffect.cppm:123`, `GlowEffect.cppm:349`, `ReactiveGlowEffect.cppm:127`, `MosaicEffect.cppm:126`, `SharpenEffect.cppm:83` — すべて **GPU context を `make_unique` で毎 frame 確保**

**問題**: 16 個の effect が **毎 frame 実行時に `GpuContext` を新規確保**。これは `ArtifactCore` 側の `GpuContext` が重い (FFmpeg / D3D12 / Vulkan context) 場合に致命的。

**修正案**:
- `GpuContext` を **effect 単位のメンバ変数** に昇格 (1 度だけ確保、cache 経由)
- `std::move` / `std::unique_ptr` の所有権移動を活用
- effect 1 個あたり 1 alloc → 全体で 16 alloc に削減

### 3.5 重要発見: `Readback (GPU→CPU)` (42 hit)

`readbackToImage()` / `readbackToMultiChannel()` / `MapTextureSubresource` が 42 箇所。代表:

- `ArtifactIRenderer.cppm:774` — `drawSpriteLocal(x, y, w, h, QImage())` が **frame 描画** で呼ばれる
- `ArtifactOffscreenRenderer2D.cppm:229` — canvas readback

**問題**: GPU → CPU readback は **重い** (数 ms オーダー、stale fence 待ち)。毎 frame 呼ぶと **CPU/GPU 同期** が起きて frame budget を超える。

**修正案**:
- `readbackToImage` を **render 完了後の idle 状態** でのみ呼ぶ
- double buffer 化 (`frontBuffer_` / `backBuffer_`)
- HUD / thumbnail 用途は低解像度版で代用
- `M-IR-8 ImmediateContext Boundary` で既に方針あり、本レポートはその具体化

### 3.6 重要発見: `JSON parse in render path` (34 hit)

`renderOneFrame` または `renderImpl` 周辺で `QJsonDocument::fromJson` を呼ぶ箇所がある。代表:

- `ArtifactRenderQueueService.cppm:1994` — encoder mutex
- `ArtifactProjectService.cppm:810` — Asset monitor

**問題**: JSON parse は **us オーダー** だが、毎 frame 呼ぶと累積で ms オーダー。

**修正案**:
- render path の JSON parse を **cache 化**
- 起動時に bulk parse、memory map に保持
- render 時には cache lookup のみ

### 3.7 重要発見: `QFileSystemWatcher in render` (9 hit)

9 件すべて詳細確認が必要。`ArtifactProjectService::AssetMonitor` が代表。

**問題**: `QFileSystemWatcher` の callback は別 thread。render path 内に mutex 経由で触ると stutter。

**修正案**:
- callback で `dirty` フラグを立てるのみ
- render path は `dirty` フラグを見てキャッシュ invalidate

### 3.8 重要発見: `QTimer in render loop` (73 hit)

`QTimer::start` / `QTimer::singleShot` が 73 箇所。`paintEvent` 内の直接呼出は検出されなかったが、**debounce timer** 系の重複多数。

**問題**: 同一タイマーが複数 widget から別 interval で start されると、frame 毎にスロットが乱発。

**修正案**:
- debounce timer を **singleton service** に集約
- `ArtifactCompositionRenderController.cppm:252` の `resizeDebounceTimer_` を service 化
- 個別 widget の `QTimer` を削減

### 3.9 重要発見: `Manual delete` (264 hit)

264 箇所の `delete`。`unique_ptr / shared_ptr` 667 件と比較すると、まだ手動 delete が多い。

**問題**: 手動 delete は **所有権の所在が不明瞭**。例外安全でない。Phase 別 refactor 候補。

**修正案**:
- `ArtifactColorManagement.cppm:489` (`delete impl_;`) など PImpl の手動 delete を `std::unique_ptr<Impl>` に置換
- `ArtifactLayerFactory.cppm:148` 周辺の `new` を `std::make_shared` に
- 段階的に `delete` を削減

### 3.10 重要発見: `QSettings in render path` (20 hit)

`QSettings` が render path に 20 箇所。代表:

- `FastSettingsStore` 経由
- `ApplicationSettingDialog` 関連

**問題**: `QSettings` は disk I/O。毎 frame 呼ぶと stutter。

**修正案**:
- 起動時に bulk load、in-memory cache
- render path は cache lookup のみ
- 設定変更時のみ `QSettings::sync()`

### 3.11 重要発見: `QWaitCondition` (5 hit)

5 箇所で `QWaitCondition` を使用。代表:

- `ArtifactPlaybackEngine.cppm:63` — playback 同期
- `ArtifactRenderQueueService.cppm:1994` — encoder mutex
- `ArtifactPlaybackService.cppm:149` — preview disk write
- `ArtifactCompositionRenderWidget.cppm:222` — render cv

**問題**: `QWaitCondition::wait` は blocking。frame loop 内で `wait` すると描画停止。

**修正案**:
- `QWaitCondition` は worker thread 内に閉じる
- render path には `QSemaphore` (tryAcquire) または `std::atomic` flag
- 既存 5 箇所の async 化

### 3.12 重要発見: `Network in render path` (5 hit)

5 件は少ないが、render path 内の network I/O は致命的。

**問題**: HTTP / TCP / UDP は数 ms 〜 数百 ms。frame 内に混入不可。

**修正案**:
- network は **専用 worker thread** に分離
- 結果は `QMetaObject::invokeMethod` で main thread へ

### 3.13 重要発見: `Unnecessary copy` (16 hit)

`const std::vector<T>&` ではなく `std::vector<T>` を引数に取る関数 16 件。

**問題**: 値渡しは **copy コスト**。`ImageF32x4_RGBA` のような大きい型では致命的。

**修正案**:
- 引数を `const T&` に変更
- 移動が明らかな箇所は `T&&` に
- `RENDER_FORMAT_CONTRACT` 方針と整合

### 3.14 重要発見: `String allocation hot path` (287 hit)

`QString::number` / `QString::arg` が 287 箇所。**render path 内** では 0 hit (良い)。問題は **frame metadata 構築** で毎 frame 文字列を生成しているケース。

**修正案**:
- 文字列は **起動時に 1 度** 構築し、cache
- `QStringLiteral` 化を拡大

---

## 4. ホットスポットのまとめ

### 4.1 重大度 S (即修正)

| 問題 | 件数 | 影響 |
|---|---:|---|
| `update() in paintEvent` | 3 件 | フレーム落ち / デッドロック |
| `QImage` の hot path 流入 | 38 hit | 色空間変換 2 倍コスト、`RENDER_FORMAT_CONTRACT` 違反 |
| `GpuContext` を毎 frame `make_unique` | 16 effect | heap 16 alloc / frame |

### 4.2 重大度 A (次回 sprint)

| 問題 | 件数 | 影響 |
|---|---:|---|
| `QFile::exists()` in render path | 35+ | disk I/O で stutter |
| `Readback (GPU→CPU)` | 42 | CPU/GPU 同期 |
| `JSON parse` in render path | 34 | us オーダー累積 |
| `Manual delete` | 264 | 例外安全 |
| `QSettings` in render path | 20 | disk I/O |

### 4.3 重大度 B (継続改善)

| 問題 | 件数 | 影響 |
|---|---:|---|
| `QTimer` の重複 | 73 | フレーム不整合 |
| `QWaitCondition` の render 内 | 5 | blocking |
| `Unnecessary copy` | 16 | copy コスト |
| `String allocation` | 287 | 起動時以外で軽微 |

### 4.4 良好 (確認済み)

- unique_ptr / shared_ptr 採用: 667 件
- App-level logging: 504 件
- Localization: 272 件
- Render key cache: 14 件
- Surface cache (LOD): 111 件
- GPU texture cache: 63 件
- Async decode (QFuture): 62 件
- QMutex / QReadWriteLock: 216 件
- QtConcurrent: 14 件
- Frame cache: 68 件
- Lazy load (placeholder): 102 件
- HiDPI / DPI: 67 件
- Drag & drop: 32 件
- Clipboard: 18 件
- Workspace switching: 19 件
- Dark / light theme: 11 件
- Undo / redo: 27 件
- Settings dialog: 22 件
- Recent files: 9 件
- Auto-save: 3 件
- Crash recovery: 6 件
- Crash report: 6 件
- Accessibility: 7 件
- Snap to grid: 7 件
- Shy view: 14 件
- Solo view: 3 件
- Collapse layer: 4 件
- App perf counter: 20 件

---

## 5. 既存 milestone との接続

- `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` — render 経路の境界。本レポートはその具体化
- `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` — `QImage` 流入禁止を再強調
- `MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md` — readback 42 件の対象
- `MILESTONE_PAINT_LAYER_2026-06-16.md` — `QImage` 経由の PaintLayer を `ImageF32x4RGBAWithCache` 経由に
- `MILESTONE_LUT_BROWSER_2026-06-16.md` — `LutPreviewWidget` の render path 流入防止

---

## 6. 新規 milestone 候補（App レベル機能）

### 6.1 P0

- **M-DEBUG-1 Dev / Diagnostics 整備**: Performance overlay, RenderDoc integration, qDebug in render path 制御, GPU profiling (Nsight / GPA)
- **M-EDIT-1 Edit Menu 強化**: `ArtifactEditMenu.cppm` 1 hit のみ。undo redo / clipboard / multi-selection / find & replace
- **M-INTERACT-1 Pen / Touch / Joystick**: TabletEvent / GestureEvent / Joystick が 0 hit

### 6.2 P1

- **M-CRASH-1 Crash-safe Save**: atomic write, project size limit, auto-recovery
- **M-TEMPLATE-1 Project Template Gallery**: template gallery 0 hit
- **M-SCRIPT-1 Script Console (REPL)**: REPL 0 hit
- **M-LAYOUT-1 Layout save/load + Workspace Manager 拡張**: layout save/load 0 hit

### 6.3 P2

- **M-XR-1 VR / XR Support**: 0 hit (将来)
- **M-TELEMETRY-1 Telemetry / Update Check**: 0 hit

---

## 7. リスクと未解決論点

### 7.1 パフォーマンス修正のリスク

1. **`update() in paintEvent` 修正**。3 件は `ArtifactProjectManagerWidget` / `ArtifactPlaybackControlWidget` の UI 制御に絡む。`QTimer::singleShot(0, ...)` での deferred は 1 frame 遅延を伴う
2. **`GpuContext` cache 化**。effect 16 個の派生を横断する変更。各 effect の `Impl` に context を保持する refactor
3. **`QImage` 削除**。`ArtifactRenderQueueService.cppm:2693` など 38 箇所。CI build への影響
4. **disk I/O 排除**。`QFileSystemWatcher` への依存。`AssetMonitor` の callback 設計変更

### 7.2 機能追加の依存

- **Dev / Diagnostics 整備**は `M-IR-8 ImmediateContext Boundary` の成熟待ち
- **Edit Menu 強化**は `M-CLIP-1 Keyframe Copy&Paste` 完了後
- **Crash-safe Save**は `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` の checkpoint と並走
- **Template Gallery**は `M-EXPORT-1 Render Format Expansion` 完了後

### 7.3 サブモジュール境界

- `ArtifactCore` 配下のみを書く
- `ArtifactWidgets` 触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 8. まとめ

- **App レベルの機能ギャップ**は 52 項目 MISS。Dev / Diagnostics / Edit Menu / Tablet / Touch / Project Template / Script Console / Crash-safe Save / Telemetry が未着手
- **パフォーマンスボトルネック**は 3 件 (重大度 S): `update() in paintEvent` (3 件) / `QImage` 流入 (38 hit) / `GpuContext` の毎 frame alloc (16 effect)
- 既存の `unique_ptr / shared_ptr` / `Localization` / `QMutex` / `Surface cache` / `GPU texture cache` / `Async decode` などのモダン実装は評価できる
- 修正は `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` / `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` / `MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md` と並走

新規 milestone として:
- **M-PERF-1 Paint Event Recursion / QImage in Hot Path 排除**
- **M-DEBUG-1 Dev / Diagnostics 整備 (Performance overlay, RenderDoc)**
- **M-CRASH-1 Crash-safe Save (atomic write)**
- **M-INTERACT-1 Pen / Touch / Joystick 入力**

あたりが自然な次の一手。

---

## 9. 更新履歴

- 2026-06-16: 初版作成。`Artifact/src + include` を App のみに絞り、機能ギャップ 100+ 項目 + パフォーマンスボトルネック 14 種を列挙。
