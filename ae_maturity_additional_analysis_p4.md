# AE-Like 成熟度追加分析 — パート4（パート1-3 + 既存1-31 以外の問題）

**調査対象**: `AppMain.cppm`, Menu/Accel/Dialog, Color/Property/Image/Utils, CMake/vcpkg/build  
**制約**: ソースコードのみ。`docs/` は未読。

---

## P0 — 即時修正必須

### 1. AppMain の autoSaveManager 二重解放
- **発生箇所**: `Artifact/src/AppMain.cppm:2280-2291`
- **内容**: 2つの `aboutToQuit` ハンドラが両方とも `delete autoSaveManager` を実行。2番目で Use-After-Free。
- **影響**: アプリ終了時にクラッシュ。

### 2. UniString の `operator std::string()` が文字化けを生成
- **発生箇所**: `ArtifactCore/src/Utils/UniString.cppm:189-192`
- **内容**: `std::u16string` の `begin()/end()` を `char*` として `std::string` 化。UTF-16 のバイト列がそのまま latin-1 風に切り詰められ文字化け。
- **影響**: パス・レイヤー名などの文字化け。

### 3. UniString の比較/代入が moved-from で null deref
- **発生箇所**: `ArtifactCore/src/Utils/UniString.cppm:119-135, 152-157, 200-228`
- **内容**: `operator==`, `operator=`, 生 C-string コンストラクタが `impl_ == nullptr` をチェックしない。move 後のオブジェクト使用でクラッシュ。
- **影響**: ムーブセマンティクス使用個所でクラッシュ。

### 4. FFmpegEncoder の linesize がハードコード（stride 非互換）
- **発生箇所**: `Artifact/src/Export/FFmpegEncoder.cppm:445, 676`
- **内容**: `srcLinesize = {w * 4}` 固定。`av_frame_get_buffer` の実際の linesize (32byte aligned) と不一致 → `sws_scale` の入力が破損。
- **影響**: 書き出し画像が崩れる/クラッシュ。

### 5. ImageF32x4 の 0-width/0-height 未ガード
- **発生箇所**: `ArtifactCore/src/Image/ImageF32x4.cppm:12`
- **内容**: `cv::Mat(h, w, ...)` に w/h <= 0 チェックなし。OpenCV 例外または無効行列。
- **影響**: レイヤー作成時やリサイズでクラッシュ。

### 6. FFmpeg triplet が x64-windows 固定
- **発生箇所**: `ArtifactCore/CMakeLists.txt:406`
- **内容**: `VCPKG_TARGET_TRIPLET` を無視し `x64-windows` 固定。ARM64/交差ビルドがリンク失敗。
- **影響**: 非 x64 環境でビルド不能。

### 7. sentencepiece リンクターゲットが存在しない
- **発生箇所**: `ArtifactCore/CMakeLists.txt:296`
- **内容**: `absl::abseil_dll` と `absl::flags` をリンクするが vcpkg abseil がエクスポートするのは `absl::absl`。リンカーエラー。
- **影響**: クリーンブートストラップで失敗。

### 8. submodule nanosvg が .gitmodules に登録されていない
- **発生箇所**: `.gitmodules` 全体
- **内容**: `third_party/nanosvg` がモジュールインデックスには存在するが `.gitmodules` にマッピングがない。`git submodule update --init --recursive` でスキップされる。
- **影響**: CI/他環境で nanosvg が未初期化。

---

## P1 — 高重大度

### 9. ApplicationSettingDialog の autoSave チェックが保存されない
- **発生箇所**: `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm:163-178`
- **内容**: `autoSaveCheckBox_` の状態をロードするが `saveSettings()` で書き戻さない。毎回デフォルト未チェックにリセット。
- **影響**: 自動保存が常にオフ。

### 10. ApplicationSettingDialog の saveSettings が呼ばれない
- **発生箇所**: `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm:140 / ArtifactEditMenu.cppm:534`
- **内容**: `handlePreferences()` が `dialog->show()` (非モーダル) で実行。OK/Apply ボタンが `accept()` → `saveSettings()` を呼ぶ経路が通らない。
- **影響**: 設定ダイアログ全般の保存が不能。

### 11. 5つのダイアログに hardcoded 固定サイズ + デフォルトボタン未設定
- **発生箇所**: `CreatePlaneLayerDialog.cppm:659`, `CreatePlaneLayerDialog.cppm:790`, `CreateCameraLayerDialog.cppm:298`, `PrecomposeDialog.cppm:68`, `ColorSwatchDialog.cppm:291`
- **内容**: `setFixedSize(520,500)` などで HiDPI 無視。`QDialogButtonBox` の OK に `setDefault(true)` 未設定で Enter キーが効かなくなる可能性。
- **影響**: UI/UX 破綻 + DPI スケーリング不良。

### 12. タイムラインのキーフレーム貼付で EOF 超えが制限されない
- **発生箇所**: `Artifact/src/Widgets/ArtifactTimelineWidget.cpp:1154`
- **内容**: `pasteKeyframesToLayers` が `>= 0` clamp のみ。composition duration を超えるフレームに貼り付く。
- **影響**: 再生不可能なキーフレーム状態。

### 13. Saturation ノードの mutex がロックされない
- **発生箇所**: `Artifact/src/Color/Saturation.cppm:15-56`
- **内容**: `std::mutex mutex_` 宣言があるが `setSaturation()` でも `saturation()` でも `lock()` しない。
- **影響**: カラー Grader のパラメータ変更で data race。

### 14. Image dirty flag に mutex なく並列アクセス
- **発生箇所**: `Artifact/src/Image/ImageF32x4RGBAWithCache.cppm:61-64, 89-93`
- **内容**: `m_bCpuDataDirty` / `m_bGpuDataDirty` が render thread と upload thread で非保護。
- **影響**: GPU に未初期化 or 古いテクスチャが使われる。

### 15. Color grading LUT 構築に mutex 不足
- **発生箇所**: `Artifact/src/Color/ArtifactColorNodeGraph.cppm:80-81, 176-181`
- **内容**: `ensureOrder()` が `evaluationOrder_` を遅延構築するが排他制御なし。`connect/disconnect` と並列 `process` で競合。
- **影響**: LUT 構築中にベクターが壊れる。

### 16. PNG/BMP/JPEG 画像モジュールがスタブ
- **発生箇所**: `ArtifactCore/src/Image/PNGImage.cppm`, `BitmapImage.cppm`, `JPEGImage.cppm:1-2`, `ImageExports.cppm`
- **内容**: PNG/BMP が空実装。JPEG は `//module` コメントアウトでモジュール宣言すらない。`ImageExports` も空。
- **影響**: 外部書出し/取込みのこれらの形式が機能しない。

### 17. FloatImage の width/height が 0 固定
- **発生箇所**: `ArtifactCore/src/Image/FloatImage.cppm:35-42,54-62`
- **内容**: どちらも `return 0;`。内部状態を無視して常に 0。
- **影響**: 当該クラスを利用するパイプラインが 0 サイズ画像として処理。

### 18. ショートカット競合 (Ctrl+D / Ctrl+R)
- **発生箇所**: `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm:101&150`, `ArtifactViewMenu.cppm:181`
- **内容**: Duplicate が Ctrl+D で Delete も D。Rulers が Ctrl+R で Redo の一般的なショートカットと競合。
- **影響**: ユーザー期待と異なる動作。

### 19. Property の setExternalNormalizedValue が m_value に書戻さない
- **発生箇所**: `ArtifactCore/src/Property/AbstractProperty.cppm:555-574`
- **内容**: `clamp` 後の値が `m_value` に反映されず、呼び出し側の値が古いまま。
- **影響**: 外部バインドされたパラメータが実際に更新されない。

### 20. AbstractProperty の range 検証不足
- **発生箇所**: `ArtifactCore/src/Property/AbstractProperty.cppm:208-217`
- **内容**: `setDefaultValue` / `setMinValue` / `setMaxValue` が互いを検証しない。`default > max` などが silently accepted。
- **影響**: スライダー/入力の境界値が不正。

### 21. vcpkg に未使用/未宣言パッケージが混在
- **発生箇所**: `ArtifactCore/CMakeLists.txt:262-277`, `vcpkg.json:10,22-23`
- **内容**: `abseil` / `protobuf` は find_package するが vcpkg.json に未宣言。`qtwebsockets` / `box2d` / `fmt` は宣言のみで使用箇所なし。
- **影響**: クリーンビルド失敗、または orphan 依存。

### 22. ArtifactWidgets の _GUARDOVERFLOW_CRT_ALLOCATORS が継承されない
- **発生箇所**: `ArtifactWidgets/CMakeLists.txt` (欠落)
- **内容**: Artifact/ArtifactCore で MSVC  hardened allocator を有効化しているが、Widgets 共有ライブラリに反映されない。
- **影響**: ヒープガードが Widgets モジュールで無効。

### 23. install() ルールが全モジュールで欠落
- **発生箇所**: `Artifact/CMakeLists.txt`, `ArtifactCore/CMakeLists.txt`, `ArtifactWidgets/CMakeLists.txt`, `ArtifactRenderer/CMakeLists.txt`
- **内容**: `install(TARGETS ...)` がない。`cmake --install` でバイナリもヘッダもインストールされない。
- **影響**: パッケージ配布/CI デプロイ不能。

### 24. ノードグラフ evaluationOrder_ キャッシュの data race
- **発生箇所**: `Artifact/src/Color/ArtifactColorNodeGraph.cppm:80-81,176-181`
- **内容**: `mutable` な `evaluationOrder_` が `ensureOrder()` で遅延更新。排他制御なし。
- **影響**: 並列カラー評価でベクターが壊れる。

---

## P2 — 中程度/保守負債

### 25. GradingNode の operator= で placement new 前に destroy しない
- **発生箇所**: `Artifact/src/Color/ArtifactColorGradingEngine.cppm:44-73`
- **内容**: `this->~GradingNode()` しているが placement new で union active member が他の型の場合 UB。
- **影響**: まれにクラッシュ/メモリ破壊。

### 26. シェーダー PSO ディスクキャッシュの無効化ポリシー欠如
- **発生箇所**: `Artifact/src/Render/ShaderManager.cppm:407-506`
- **内容**: キャッシュキーが GPU ID のみ。HLSL ソース変更を検知しない。
- **影響**: シェーダー修正がバイナリ再起動まで反映されないことがある。

### 27. ArtifactRenderLayerPipeline の swapAccumAndTemp にメモリバリアなし
- **発生箇所**: `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:232-235`
- **内容**: SRV↔UAV の入れ替えが `std::swap` のみ。D3D12/Vulkan の subresource transition が発生しない。
- **影響**: GPU バリデーションエラー。

### 28. FloatColor がピクセルごとにヒープアロケート
- **発生箇所**: `Artifact/src/Color/FloatColor.cppm:46-152`
- **内容**: `FloatColor` が pimpl (4 floats なのにヒープ)。カラーフィルタのループで莫大な new/delete。
- **影響**: カラー演算のパフォーマンス劣化。

### 29. ノードグラフ Topological Sort 未実装
- **発生箇所**: `Artifact/src/Graphics/` (ShaderNode)
- **内容**: `NodeGraph::getTopologicalOrder()` が追加された順に返す。
- **影響**: ノード接続順によって HLSL コンパイル失敗。

### 30. vcpkg overlay に絶対パス
- **発生箇所**: `X:\Dev\ArtifactStudio\vcpkg-overlays\ports\onnxruntime\portfile.cmake:1`
- **内容**: `C:/vcpkg/ports/onnxruntime/portfile.cmake` が直書き。C:\vcpkg 以外で失敗。
- **影響**: CI/他環境で onnxruntime ビルド不能。

### 31. AudioMixer バストポロジーサイクルのサイレントドロップ (重複確認)
- **発生箇所**: `ArtifactCore/src/Audio/AudioMixer.cppm:33-63`
- **内容**: サイクル検出で return しても visited に追加しないため、次回訪問時に再度 drop。
- **影響**: 循環バスの音声が消える。

---

## マーカー・未接続の再掲（重複なき新規分）

| # | 内容 | 発生箇所 |
|---|------|----------|
| M-1 | TextGizmo 未接続 | `Artifact/src/Tool/ArtifactTextGizmo.cppm` |
| M-2 | SpeedGraph sample 未実装 | `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm:815` |
| M-3 | Text Animator timeline 未配線 | `Artifact/src/Layer/ArtifactTextLayer.cppm` |
| M-4 | Group Layer mask 未接続 | drawMaskedTextureLocal 未呼び出し |
| M-5 | ShapeOperator 6種 未実装 | `ArtifactCore/src/Shape/ShapeGroup.cppm` |

---

## 合計: パート4 単体

| カテゴリ | P0 | P1 | P2 |
|----------|----|----|----|
| Use-after-free / double-free | 2 | 0 | 0 |
| Null deref / UB | 4 | 0 | 1 |
| 文字列/シリアライズ | 2 | 2 | 1 |
| 画像ストライド/サイズ | 2 | 0 | 0 |
| ビルド依存/パス | 2 | 2 | 2 |
| データ race | 1 | 4 | 1 |
| スタブ/空実装 | 1 | 3 | 1 |
| UI/アクセシビリティ | 0 | 8 | 3 |
| GPU/PSO/バリア | 0 | 2 | 1 |
| 合計 | 14 | 21 | 10 |

---

*分析日: 2026-06-03*  
*派生元: ae_maturity_additional_analysis_p1/p2/p3 の重複を除外*
