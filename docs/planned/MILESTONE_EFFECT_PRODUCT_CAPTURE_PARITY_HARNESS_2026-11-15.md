# Effect Product Capture & Parity Harness R&D

**最終更新:** 2026-11-15
**ステータス:** 計画（コード変更・ビルドなし。ユーザー明示許可後に runtime 検証）

## 目的

オフラインの Python 参照モデルで得た知見（Blur / Glow / Transition / DoF / Detail Sharpening / Outline / Keying / Film / Retro）を、**Artifact の実レンダー経路の証拠**へ変換する共通ハーネスを整える。新エフェクトの追加ではなく、既存 11 本の R&D を製品判断に使える状態へ戻すことが目的。

## 既存資産（読解で確認済み）

- `Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cpp`
  - `renderFrame()` → `captureImage()` が `ArtifactCore::ImageF32x4_RGBA`（linear float RGBA）を返す。`readbackToImageF32()` を再利用し、staging texture を毎フレーム生成しない設計。
  - `renderToQImage()` は `readbackToImage(BGRA)` 経由で `QImage` を返す。
  - `saveFrame(path, options)` は `captureImage()` → `ImageExporter` 経由で PNG/JPEG 等を書き出す。ただし RGBA8 へ `value * 255` で丸める実装であり、HDR/linear の保存契約は別途検証が必要。
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp`
  - `readbackPresentedFrameImage()` が completed GPU composition frame を `QImage` で返す。`M-UI-OFFLINE-1` の `ui.compositionCapture` がこれを明示実行する。
- `Artifact/src/Export/ArtifactExportPreRenderPipeline.cpp`
  - `OffscreenCompositionRenderer` を保持しており、export 経路は offscreen renderer を前提にしている。
- `M-UI-OFFLINE-1`（`docs/planned/MILESTONE_OFFLINE_UI_AUTOMATION_2026-11-13.md`）
  - `--ui-headless --ui-mcp-port`、`tools/run_ui_offline.py`、`ui.compositionCapture`、`ui.state` が実装済みだが **runtime 未検証**。
- `ArtifactUiTest` / `tools/capture_ui_test.py`
  - 固定 fixture の起動・UI スクリーンショット・結果ファイルをサポート。`--components-no-window` で不可視 Components 統合確認。

## 未確認・ギャップ（ evidence: 各 R&D 文書の進捗欄）

1. **linear float の保存契約**: `saveFrame()` は RGBA8 丸め。HDR/linear を EXR/PFM で保存する経路の存在と、`ImageExporter` の float 対応は未確認。
2. **alpha/color descriptor の永続化**: `captureImage()` は `ImageF32x4_RGBA` を返すが、premultiplied/straight の association、色空間、露出、フレーム番号、backend/driver、source revision を manifest へ記録する driver がない。
3. **D3D12/Vulkan paired capture**: `OffscreenCompositionRenderer` は device/context を受け取るが、backend 切替と paired capture を自動化するスクリプトはない。
4. **timing/allocation record**: `HOT_PATH_RULES.md` の規則に沿った steady-state allocation 計測の harness がない。Shader Graph の hot-path 疑義（`Material::materialGraphJson()` の値返却 → `UniString` コピー → `toUtf8()`）も未計測。
5. **blind review の集約**: Atmospheric Glow / Blur は A/B パケットと key 隔離まで作ったが、独立判定者の回答取得と集約が未実施。

## フェーズ

### Phase 0 — 能力棚卸し（コード読解のみ、ビルドなし）

- `OffscreenCompositionRenderer` の `captureImage()` / `readbackToImageF32()` の色空間・alpha 契約をソースで確定。
- `ImageExporter` の float/EXR 対応と `ImageF32x4_RGBA` のシリアライズ経路を確認。
- `ArtifactCompositionRenderController::readbackPresentedFrameImage()` の戻り値の `QImage` format と、GPU readback の同期境界を確認。
- 既存 `tests/Artifact/*Main.cpp`（CompositionLayerMask / MotionPath / LayerSurfaceCache / ViewportChannel）の capture パターンを台帳化。

### Phase 1 — float capture driver（設計のみ）

- `captureImage()` を NPY/PFM/EXR へ保存する最小ドライバを設計（Python 側の `numpy`/`PFM` 解析器と整合）。
- run manifest schema: 入力 fixture hash、設定 JSON、backend/driver、解像度、色空間、alpha association、フレーム番号、source revision、capture timestamp。
- `M-UI-OFFLINE-1` の `ui.compositionCapture` と組み合わせ、不可視起動で 1 セッション 1 プロセスの capture を可能にする。

### Phase 2 — backend parity & timing

- D3D12 / Vulkan の paired capture を manifest で比較。
- warm-up / iteration / raw time / median / p95 / peak GPU memory / allocation count を記録。
- Shader Graph の hot-path 計測（同一 JSON の steady-state allocation）を追加。

### Phase 3 — blind review 集約

- Atmospheric Glow / Blur の既存 A/B パケットに判定者回答を添付する集約スクリプト。
- 独立判定者 2 名の回答を manifest に記録し、未回答は「未検証」として残す。

## 確認済みソース証拠（2026-11-15、読解のみ）

- `Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cpp` L100-110: `initializeResources()` で RT format は `TEX_FORMAT_RGBA8_UNORM_SRGB`。よって `captureImage()` → `readbackToImageF32()` は sRGB-encoded 8-bit を float に変換した値を返す。HDR/scene-linear capture には RT format の変更または別 float RT 経路が必要。
- `Artifact/src/Render/ArtifactIRenderer.cpp` L4110-4132: `readbackToImageF32()` は `thread_local std::vector<float> rgba` を再利用し、`readbackTextureViewToFloatBuffer(activeColorView(), rgba, width, height)` で float buffer を取得、`setFromRGBA32F(rgba.data(), width, height)` で raw RGBA → canonical BGRA backing へ変換。RT が RGBA8 (sRGB) の場合は 8-bit → float 経路になる。
- `ArtifactCore/src/IO/Image/ImageExporter.cpp` L207-213, L781: OpenImageIO ベースで FLOAT/EXR をサポート。`write_image(TypeDesc::FLOAT, ...)` で float 書き込み。float capture 保存の下地は存在。
- `Artifact/src/Render/ArtifactIRenderer.cpp` L5186-5195: `activeColorView()` は override RTV → swapchain back buffer → layer RT の順で選択。offscreen 経路では override RTV（RGBA8_UNORM_SRGB）が使われる。

## 非対象

- ArtifactCore / Artifact / ArtifactWidgets の変更（設計文書のみ）
- 新エフェクト、シェーダー、UI、ショートカット、プロジェクト形式の追加
- GPU / Diligent render path の変更
- ビルド、CMake、テスト実行（ユーザーの明示指示を待つ）

## 完了条件

- float capture の色空間・alpha 契約が文書化されている。
- manifest schema が定義され、1 件以上の capture が再現可能な形で記録されている。
- backend parity / timing / allocation の未検証項目が明示されている。
- 既存ラボ（Blur / Glow / DoF / Transition / Detail Sharpening / Outline / Keying / Film / Retro）が本ハーネスを参照可能になっている。

## 関連

- `docs/planned/MILESTONE_OFFLINE_UI_AUTOMATION_2026-11-13.md`
- `docs/technical/HOT_PATH_RULES.md`
- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp`
- `Artifact/src/Export/ArtifactExportPreRenderPipeline.cpp`
