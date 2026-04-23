# Bug Fix: Composition Background & Clear Color (2026-04-10)

## 対象バグ

1. **COMPOSITION_FILL_NOT_VISIBLE** — コンポジション領域が背景色で塗りつぶされない
2. **COMPOSITION_BACKGROUND_CONFUSION** — コンポジットエディタの背景色が薄いグレー（`#F0F0F0`）のまま
3. **MAIN_WINDOW_PANEL_LAYOUT_ISSUES** — メインウィンドウのパネルレイアウト問題（部分対応）

## 仮説と検証

### 仮説 1: Win32 STATIC ウィンドウクラスの GDI 干渉（✅ 確定）

**根拠**: `renderHwnd_` は Win32 の `"STATIC"` クラスで作成されていた。
STATIC コントロールは `WM_ERASEBKGND` / `WM_PAINT` を独自に処理し、
デフォルトのシステム背景色（`COLOR_BTNFACE` = `#F0F0F0`、薄いグレー）で塗りつぶす。

DX12 のレンダリング結果（Present 後）は DXGI flip model で保持されるが、
イベント駆動レンダリング（連続描画なし）のため、フレーム間で GDI の
`WM_ERASEBKGND` が発火すると薄いグレーが一瞬見える。特に初回表示時、
リサイズ時、他ウィンドウとの重なり解消時に顕著。

**対応**: `"STATIC"` → カスタムウィンドウクラス `DiligentRenderSurface` に置換。
- `WM_ERASEBKGND` → `return 1`（描画せず処理済みを返す）
- `WM_PAINT` → `BeginPaint/EndPaint` のみ（領域バリデーションだけ）
- `hbrBackground = nullptr`（背景ブラシなし）

### 仮説 2: 0×0 ウィジェットでの初期化失敗＋リカバリ不在（✅ 確定）

**根拠**: `CompositionViewport::showEvent` → `singleShot(0)` のタイミングで
ウィジェットがまだ 0×0（レイアウト未確定）の場合：

1. `DiligentDeviceManager::initialize()` → デバイス作成成功
2. `createSwapChainForBackend(hwnd, 0, 0)` → **失敗**（D3D12 は 0×0 拒否）
3. `initialized_ = false`（スワップチェーン後に設定されるため）
4. `ArtifactIRenderer::initialize()` → `!deviceManager_.isInitialized()` で早期リターン
5. `CompositionRenderController::initialize()` → renderer をリセット＋リターン
6. 以降のリサイズで `recreateSwapChain()` が呼ばれるが、`!swapChain_` ガードで即リターン
7. **リカバリパスなし** — ビューアは永久に描画不能

**対応**:
- `initialized_ = true` をデバイス作成直後（スワップチェーン前）に移動
- `recreateSwapChain()` 内で `!swapChain_` 時に `createSwapChain()` へフォールバック
- `renderOneFrameImpl()` にスワップチェーン存在チェックを追加

### 仮説 3: drawSolidRect の PSO が null（❌ 未確定だが防御済み）

**根拠**: `drawRectLocal()` の L445 に `!m_draw_solid_rect_pso_and_srb.pPSO` ガードがあり、
PSO が null なら全ての solid rect 描画がサイレントに no-op になる。
レイヤーは別の PSO（sprite/texture）を使うため、レイヤーだけ表示される可能性がある。

**結果**: PSO は `ShaderManager::createPSOs()` で正常に作成されることを確認。
デバイスが有効であれば PSO も有効。仮説 2 の修正によりデバイス初期化が
確実に成功するようになったため、PSO も確実に作成される。

## 変更ファイル

### `Artifact/src/Render/DiligentDeviceManager.cppm`
- **カスタムウィンドウクラス追加**: `RenderHwndProc` + `ensureRenderWindowClass()`
  - `WM_ERASEBKGND` → `return 1`
  - `WM_PAINT` → `BeginPaint/EndPaint` のみ
  - `hbrBackground = nullptr`
- **`initialize()`**: `initialized_ = true` をデバイス作成直後に移動。
  スワップチェーン失敗時も `return` するが `initialized_` は `true` のまま。
  HWND サイズは `std::max(width, 1)` で 0 防止。
- **`createSwapChain()`**: STATIC → カスタムクラス使用。
  既存 HWND のサイズ同期追加。0×0 サイズの場合は作成を延期。
- **`recreateSwapChain()`**: `!swapChain_` 時に `createSwapChain()` へフォールバック。
- `<algorithm>` を GMF に追加（`std::max` 用）。

### `Artifact/include/Render/ArtifactIRenderer.ixx`
- `hasSwapChain()` アクセサ追加。

### `Artifact/src/Render/ArtifactIRenderer.cppm`
- `hasSwapChain()` 実装追加。
- `recreateSwapChain()`: `!swapChain` 時に `createSwapChain()` へフォールバック。

### `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `recreateSwapChain()`: `!hasSwapChain()` 時に `createSwapChain()` を呼ぶ。
- `renderOneFrameImpl()`: スワップチェーン未作成時の早期リターンを追加。

## 期待される効果

1. **薄いグレー背景の解消**: STATIC → カスタムウィンドウクラスにより
   GDI が DX12 のコンテンツを上書きしなくなる。
2. **0×0 初期化からのリカバリ**: デバイス初期化を成功扱いにし、
   最初のリサイズで自動的にスワップチェーンが作成される。
3. **コンポジション背景色の表示**: 上記 2 つの修正により、
   `ClearRenderTarget(clearColor)` と `drawSolidRect(bgColor)` が確実に実行される。

## 未対応・残課題

- **ビルド未検証**: 環境に `pwsh.exe` がなく、セッション内ビルド不可。
  ユーザーによる手動ビルドが必要。
- **MAIN_WINDOW_PANEL_LAYOUT**: 本修正の範囲外。別途対応予定。
