# ビューポート強化案 (Viewport Enhancement Proposal)

> 作成: 2026-07-08 / 状態: Draft（提案・未着手）
> スコープ: Composition Editor ビューポート（`ArtifactCompositionRenderController` + `ArtifactIRenderer` + `DiligentViewportWidget`）
> 目的: 既存の `MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` の不足リストと M-VP 系計画を**再列挙せず**、そこから優先順位を絞り込み、アーキテクチャに根ざした強化ロードマップと基盤リファクタ案を提示する。

---

## 0. 今回の着手対象

この提案のうち、今回の「やれそうなマイルストーン」としては **W0 (基盤)** を優先候補にする。

- `D-1 RenderScheduler`
- `D-2 ViewportState`
- `D-3 Overlay compositor`

理由:

- 追加 UI の前に、状態と描画の責務を先に切れる
- `A` / `B` / `C` の派生案を同じ土台に載せられる
- 既存の機能を増やすより、共有基盤の整理に寄せたほうが依存を読みやすい

今回のセッションでは、機能の本体実装ではなく、W0 を次の実行単位として固定することを成果物にする。

---

## 1. 現状アーキテクチャ（責務のおさらい）

| コンポーネント | 責務 | ファイル |
|---|---|---|
| `CompositionRenderController` | ビューポートの単一エントリ。ズーム/パン/回転、オーバーレイ、ギズモ、チャンネル表示、比較モード、オニオンスキン等を集約。4500+ 行。 | `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` |
| `ArtifactIRenderer` | Diligent/D3D12 バックエンド抽象。描画プリミティブ・マルチチャンネル・オフスクリーン RT・GPU ブレンド。 | `Artifact/include/Render/ArtifactIRenderer.ixx` |
| `ViewportTransformer` | キャンバス↔ビューポート座標変換・フィット/フィル・ズーム。 | `ArtifactCore/include/Transform/ViewportTransformer.ixx` |
| `ViewOrientationNavigator` | 3D 視点（hotspot/quaternion）管理。 | `UI.View.Orientation.Navigator` |
| `ArtifactCompositionRenderOverlay` | オーバーレイ描画群（HUD/コンテキストメニュー/パイメニュー/ワークカーソル等）。 | `Artifact/include/Widgets/Render/ArtifactCompositionRenderOverlay.ixx` |
| `DiligentViewportWidget` | QWidget ホスト。swapchain 管理。 | `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx` |

**構造的ボトルネック**:
1. `CompositionRenderController` が「状態・相互作用・描画指示・オーバーレイ」を一身に背負い、単一インスタンス前提。複数ビューポート化（M-VP-1）の際に分割が必須。
2. 描画提交は `IDeviceContext` を直接使っており、マルチビューポート時に競合する（`IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-27.md` の "User Review Required" が言及）。
3. チャンネル分離表示・ROI・HUD 等の「表示モード」が controller の bool/enum フィールドとして散在し、拡張のたびに controller が肥大化する。

---

## 2. 既存計画との関係（重複回避マトリクス）

本提案は以下を「既にカバー済み」として再提案**しない**。これらは設計監査または M-VP 系に含まれる。

| 項目 | 既存所在 | 本提案での扱い |
|---|---|---|
| マルチビューポートレイアウト | M-VP-1/2、IMPLEMENTATION_PLAN_MULTI_VIEWPORT_* | §4 の「RenderScheduler」で直列化課題のみ補完 |
| キャンバス回転 / 動的解像度 / ブックマーク | M-VP-4 / M-VP-5 / M-VP-8 | 基盤（`ViewportState` 構造化）で吸収想定 |
| チャンネル分離表示（RGBA/法線等） | enum `ViewportChannelDisplayMode` 既存、`CompositionRenderController` に実体あり | 拡張 API の「DisplayFilter」として統合提案 |
| オニオンスキン / モーションパス | 実装済み | 参照のみ |
| 3D Orbit/Pan/Preview | M-3D-2 | ナビゲーション統一構文として §5 で補足 |

---

## 3. 強化の 4 本柱と優先順位

既存不足リストを「実装コスト × 頻出度 × 基盤効果」で整理し、4 本柱に束ねる。

- **A. 表示・デバッグ表現の拡張**（最も高頻出・低リスク）
- **B. 比較・レビュー支援**（クライアント確認ワークフロー直結）
- **C. ナビゲーション・インタラクション統一**（操作性の底上げ）
- **D. レンダリング基盤の堅牢化**（マルチビューポートを見据えた必須工事）

優先順位: **D（基盤）を前提**に、**A → C → B** の順で価値が出る。

---

## 4. 基盤リファクタ案（D 本柱・最優先）

### D-1. `ArtifactRenderScheduler`（描画提交の直列化・非同期化）
- **課題**: `CompositionRenderController.cppm:8343` の `finalizeGpuRenderToViewport` や各 draw 呼び出しが `IDeviceContext` を直接叩く。M-VP-1 では `QMutex` でラップする暫定案が提案されているが、これは本質解決ではない。
- **提案**: 描画コマンドを「フレーム単位のコマンドバッチ」として `ArtifactRenderScheduler` の重複排除キューに積み、単一の `IDeviceContext` 提交スレッドが消費する。各 `CompositionRenderController` は「自分のフレームをスケジュールする」だけになる。
- **対象**: 新規 `ArtifactCore/include/Render/RenderScheduler.ixx` + `Artifact/src/...`。`ArtifactIRenderer` は `submitFrame(CommandBatch)` を追加。
- **制約**: Diligent/D3D12 パスは推測で広く触らない（`ArtifactIRenderer.ixx:3-7` の保守ルール）。コマンドバッチ化の境界だけを慎重に追加。

### D-2. `ViewportState` の構造化（M-VP-4/5/8 の統合受け口）
- 現在 `pan`/`zoom`/`orientation` が controller の散在フィールド。これを `ViewportTransformer::ViewportCB`（`ViewportTransformer.ixx:89`）に回転・解像度スケール・DPR を含む構造体へ統合し、ブックマーク/回転/動的解像度が共通で読み書きできるようにする。
- `CompositionRenderController` から zoom/pan/rotate の「状態保持」を追い出し、状態は `ViewportState` が持つ。

### D-3. Overlay 合成の分離（Compositor）
- `ArtifactCompositionRenderOverlay` の各 `drawViewport*Overlay` を、controller から独立した「OverlayLayer リスト」として compositor が管理。
- 追加オーバーレイ（HUD、ROI、サンプル点、ゼブラ等）が controller を触らず `registerOverlay(std::unique_ptr<OverlayLayer>)` で差し込めるようになる。
- **制約**: QtCSS / `QColorDialog` / `QPainter` 合成は禁止（`AGENTS.md`）。既存の `FloatColorPicker` + owner-draw + `ArtifactIRenderer` プリミティブで描く。

---

## 5. 表示・デバッグ表現の拡張（A 本柱）

### A-1. DisplayFilter フレームワーク（チャンネル表示の統合）
- `ViewportChannelDisplayMode` enum は既存だが、個別チャンネルの ON/OFF と「PBR チャンネル分離」「Buffer Visualization」がばらばら。
- `IRenderer::setChannelEnabled(ChannelType, bool)`（`ArtifactIRenderer.ixx:148`）をフックに、`DisplayFilter` としてまとめ、オーバーレイ上で切替パネルを出す。
- 追加価値: Zebra（クリッピング可視化）、Sample Points（RGBA 常時表示）、Buffer Visualization（Nuke 系）を同一フレームワークで提供。

### A-2. ROI (Region of Interest) の実用化
- `ArtifactRenderROI` 構造体は存在するが debug draw がコメントアウト。
- ビューポート上で矩形を引いて「プレビュー範囲」を視覚指定 → `RenderScheduler` がその範囲だけ高品質レンダリング（C4D の IRR / Nuke の Pre-render Region 相当）。
- ヘビー comp の部分確認に必須。既存の `markRenderDirty()` 経路と組み合わせ。

### A-3. カスタマイズ可能 HUD
- 現在 `stateLabel` 等で部分的出力。`HUDConfig`（poly count / fps / camera name / zoom / frame 等のトグルリスト）を導入し、OverlayLayer として描画。
- 既存 `lastFrameTimeMs()/averageFrameTimeMs()`（`CompositionRenderController.ixx:256-257`）を流用。

### A-4. Show Flags / Display Tags（レイヤー単位表示上書き）
- C4D の Display Tags / UE の Show Flags 相当。レイヤーごとに「ワイヤーフレーム/シェード/非表示」をビューポート上で上書き。
- `LayerID` 単位のオーバーライドマップを `CompositionRenderController` の描画ループ（`drawLayerForCompositionView`）に適用。

---

## 6. 比較・レビュー支援（B 本柱）

### B-1. ビューポート内 Wipe / Split 比較
- 既存 `CompositionCompareMode`（Off/A/B/Diff）は実体あり。これに「ドラッグで境界を動かす Wipe ライン」を追加（Resolve/Fusion/Nuke 系）。
- 既存 `setReferenceOverlayImage` / `isReferencePinned` 経路を活用し、Composition Viewport 内での A/B ワイプを完結。

### B-2. Sample Points / Color Sampler 強化
- `setShowColorSamplerOverlay` は既存。これを「永続サンプル点」化し、各点の RGBA を HUD に常時表示（Nuke Sample Points）。
- `FloatColorPicker` と連携してカラーマッチング用途へ。

### B-3. Playblast / Contact Sheet（将来拡張）
- `captureCurrentFrameImage()`（`CompositionRenderController.ixx:254`）＋ `readbackToImageAsync`（`ArtifactIRenderer.ixx:113`）を使い、連番/動画の簡易書き出し（Maya Playblast / Nuke Flipbook / Contact Sheet）をビューポートから直接。
- 出力は `QImage` 新規採用禁止の例外枠（`ArtifactIRenderer` の readback は既存 API なのでそのまま利用）。

---

## 7. ナビゲーション・インタラクション統一（C 本柱）

### C-1. 共通ナビゲーション構文
- Blender 風 `Alt+Left Drag = Orbit` / `Middle Drag = Pan` / `Wheel = Zoom` を 2D/3D 両ビューポートで統一。
- `handleMousePress/Move/Release`（`CompositionRenderController.ixx:259-261`）を `ViewportNavigator` に委譲し、ツール切替を挟まず視点操作可能に。

### C-2. Preview Orbit Mode（実カメラと preview view の分離）
- カメラレイヤー編集か、視点だけの preview かを `ViewOrientationNavigator` の状態で明示。誤操作を防ぐ。

### C-3. Construction Plane / Guide Geometry（将来拡張）
- Houdini 由来。任意平面上での変形・スナップ、およびレンダリングされないガイドジオメトリを OverlayLayer として提供。

---

## 8. マイルストーン分割案（提案）

| Wave | 内容 | 依存 | 想定規模 |
|---|---|---|---|
| **W0 (基盤)** | D-1 RenderScheduler、D-2 ViewportState、D-3 Overlay compositor | なし | 大（C++20 modules 影響大） |
| **W1** | A-3 カスタム HUD、A-1 DisplayFilter 統合、A-4 Show Flags | W0 | 中 |
| **W2** | A-2 ROI 実用化、B-1 Wipe 比較、B-2 Sample Points | W0 | 中 |
| **W3** | C-1/C-2 ナビゲーション統一 | W0, D-2 | 中 |
| **W4 (拡張)** | B-3 Playblast、C-3 Construction Plane、M-VP 系との統合検証 | W1-W3 | 大 |

---

## 9. リスク・制約

- **Diligent/D3D12 直接変更禁止**: `ArtifactIRenderer` の低レベルは推測で広く触らない。境界（コマンドバッチ化・チャンネル hook）のみ追加。
- **C++20 modules 循環参照**: 新規 `RenderScheduler` / `ViewportState` / `OverlayLayer` は `.ixx` に不要な `import` を入れず、値保持しない型は前方宣言。影響は `check_module_hygiene` で検査。
- **子リポジトリ非変更**: `ArtifactCore` 側へ新規モジュールを足す場合のみ編集し、`Artifact`/`ArtifactWidgets` は基本触らない（AGENTS.md の submodule ルール）。
- **UI 表現の制約**: QtCSS / `QColorDialog` / `QPainter` 合成 / `QImage` 新規採用は禁止。既存 `ArtifactIRenderer` プリミティブ + owner-draw + `FloatColorPicker` で完結。
- **CRLF 維持**: 既存ファイル編集は `edit` ツール使用。`write` は新規ファイルのみ。

---

## 10. 未確認事項（ユーザ確認待ち）

1. **W0 基盤を今回やるか、M-VP-1 の `QMutex` 暫定で進めるか** — 本提案は「いつか正規解」を推奨するが、短期リリースが必要なら暫定併用もあり。
2. **A 本柱のどの項目を最初に欲しいか** — HUD/ROI/Wipe の優先度。
3. **B-3 Playblast の出力フォーマット・コーデック要件** — 既存エンコード基盤の有無確認が必要。
4. **DisplayFilter と既存 `ViewportChannelDisplayMode` の統合粒度** — enum 拡張か別フレームワークか。

---

## 11. Next Step

W0 を実装に進める前の最初の作業順は次の通り。

1. `ViewportState` の保持場所を `CompositionRenderController` の散在フィールドから切り出す
2. `ViewportOverlayCompositor` の境界だけを先に作る
3. `DisplayFilterSet` は既存 enum の下位互換を保ったまま導入する
4. 低レベルの Diligent / D3D12 パスには触らない

この順で進めると、後続の HUD / ROI / Wipe / DisplayFilter 拡張を同じ入口に揃えやすい。

---

## 12. W0 実行チェックリスト

W0 を「着手した」と言える最低ラインは次の 3 点。

1. `ViewportState` の責務境界が文書上で確定している
2. `ViewportOverlayCompositor` と `DisplayFilterSet` の API 形が決まっている
3. `CompositionRenderController` 側の既存フィールドからの移し替え順が 1 本に定まっている

この 3 点がそろったら、初回実装は `ViewportState` から入る。

---

## 11. 参照ドキュメント

- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`（不足機能網羅・C4D/Maya/Houdini 比較）
- `docs/planned/IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-27.md`（M-VP-1 / Diligent 直列化課題）
- `docs/planned/IMPLEMENTATION_PLAN_VIEWPORT_PANE_MANAGER_2026-06-28.md`（M-VP-2）
- `docs/planned/MILESTONE_VIEWPORT_DYNAMIC_RESOLUTION_2026-06-27.md`（M-VP-5）
- `docs/planned/MILESTONE_VIEWPORT_BOOKMARKS_2026-06-27.md`（M-VP-8）
- `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`（M-3D-2）

---

## 12. コードレベル具体化（追加調査ベース）

> 以下は §4–§7 の機能を実際のコード差し込み位置まで落とし込んだもの。
> 調査根拠: `ArtifactCompositionRenderController.cppm:18903`（`renderOneFrame`→`renderOneFrameImpl`）、同 `:8343`（`finalizeGpuRenderToViewport`）、`ArtifactIRenderer.ixx:119-189`（既存描画 API）、`ViewportTransformer.ixx:89`（`ViewportCB`）。

### 12.1 D-1 `ArtifactRenderScheduler`（描画提交の直列化）

**新規モジュール**: `ArtifactCore/include/Render/RenderScheduler.ixx`

```cpp
export module Core.Render.Scheduler;
import Core.EventBus.Event;

export struct RenderCommandBatch {
  int viewportInstanceId = 0;
  std::function<void(ArtifactIRenderer&)> submit;  // 既存 renderer の draw 呼び出しをクロージャ化
  uint64_t frameTag = 0;
};

export class RenderScheduler {
public:
  static RenderScheduler& instance();
  // 重複排除: 同一 viewportInstanceId の未処理バッチは上書き（最新フレームのみ描画）
  void enqueue(RenderCommandBatch batch);
  // 単一 IDeviceContext 提交スレッドが消費
  void pump();
  void setDevice(Diligent::RefCntAutoPtr<Diligent::IDeviceContext> ctx);
private:
  RenderScheduler() = default;
};
```

**統合箇所**: `CompositionRenderController::renderOneFrame()`（`cppm:18903`）の `impl_->renderOneFrameImpl(this)` 呼び出しを、クロージャ化して `RenderScheduler::enqueue()` へ回す。controller は「自分のフレームをスケジュールする」だけになり、M-VP-1 の `QMutex` 暫定案が不要になる。
**制約**: `ArtifactIRenderer` の低レベル（D3D12 パス）は変更しない。既存 `setViewportRect/setZoom/setPan/setCanvasSize`（`ArtifactIRenderer.ixx:119-167`）をクロージャ内でそのまま呼ぶ。

### 12.2 D-2 `ViewportState`（ViewportCB の拡張）

`ViewportTransformer::ViewportCB`（`ViewportTransformer.ixx:89`）を以下に拡張。M-VP-4（回転）/ M-VP-5（動的解像度）の受け口を兼ねる。

```cpp
struct ViewportCB {
  float2 offset;            // パン位置
  float2 scale;             // スケール
  float2 screenSize;        // ビューポート解像度
  float  zoom;
  float  rotationRad;       // M-VP-4: キャンバス回転
  float  resolutionScale;   // M-VP-5: 動的解像度スケール (0.5–1.0)
  float  dpr;               // device pixel ratio
  float  _pad;
};
```

**移行**: `CompositionRenderController` の `viewportPan()` / `viewportZoom()` / `viewportOrientationQuaternion()` 等（`.ixx:119-283`）の状態保持を `ViewportState` へ追い出し。`setViewportOrientation*` は quaternion → `rotationRad`（2D 表現）または 3x3 行列へ投影。

### 12.3 D-3 `OverlayLayer` compositor 分離

**新規モジュール**: `Artifact/include/Widgets/Render/ViewportOverlay.ixx`

```cpp
export module Widget.Render.ViewportOverlay;
import Artifact.Render.IRenderer;

export class ViewportOverlayLayer {
public:
  virtual ~ViewportOverlayLayer() = default;
  virtual void draw(ArtifactIRenderer& renderer, const ViewportState& vs) = 0;
  virtual int hitTest(const QPointF& viewportPos) const { return -1; }  // 既存 viewportOverlayItemAt 相当
};

export class ViewportOverlayCompositor {
public:
  void registerOverlay(std::unique_ptr<ViewportOverlayLayer> layer);
  void unregisterOverlay(const ViewportOverlayLayer* layer);
  void drawAll(ArtifactIRenderer& renderer, const ViewportState& vs);
  int hitTestAll(const QPointF& viewportPos) const;
};
```

**移行**: `ArtifactCompositionRenderOverlay.ixx` の各 `drawViewport*Overlay(renderer, ...)` を `ViewportOverlayLayer` のサブクラス（InfoOverlay / ContextMenuOverlay / PieMenuOverlay / WorkCursorOverlay 等）へ移行。controller は `compositor_.drawAll(*renderer_, state_)` のみ呼ぶ。`hideViewportOverlay()` / `isViewportOverlayVisible()`（`.ixx:218-219`）は compositor のフラグへ委譲。
**制約**: `QPainter`/`QColorDialog`/QtCSS 不使用。描画は既存 `ArtifactIRenderer` プリミティブ（`drawRoundedPanel`/`drawText`/`drawSolidLine` 等）で行う。

### 12.4 A-1 `DisplayFilter`（チャンネル表示の統合）

既存 `ArtifactIRenderer::setChannelEnabled(ChannelType, bool)`（`ArtifactIRenderer.ixx:148`）＋ `ViewportChannelDisplayMode`（`.ixx:55`）をフックに統合。

```cpp
export enum class DisplayFilterId {
  Color, Alpha, RGBA, Red, Green, Blue,
  NormalXYZ, VelocityXY, Depth, Emission, ObjectId, MaterialId, AlbedoRGB,
  PbrBaseColor, PbrRoughness, PbrMetallic, PbrNormal, PbrHeight,   // 3D 系
  Zebra,                  // クリッピング可視化
  BufferVisualization     // Nuke 系: GPU バッファ直接表示
};

export class DisplayFilterSet {
public:
  void setEnabled(DisplayFilterId id, bool on);
  bool isEnabled(DisplayFilterId id) const;
  // 既存 setChannelEnabled へ変換して renderer に適用
  void applyTo(ArtifactIRenderer& renderer) const;
};
```

**統合箇所**: `setViewportChannelDisplayMode()`（`.ixx:168`）の内部で `DisplayFilterSet::applyTo(*renderer_)` を呼ぶ。既存 enum を維持しつつ、`Zebra`/`BufferVisualization` を後ろに追加。

### 12.5 A-3 カスタム HUD（`HUDConfig`）

`lastFrameTimeMs()` / `averageFrameTimeMs()`（`.ixx:256-257`）＋ `frameDebugSnapshot()` をソースにする。

```cpp
export struct HUDConfig {
  bool showFps = true;
  bool showFrameMs = false;
  bool showPolyCount = false;
  bool showCameraName = true;
  bool showZoom = true;
  bool showResolutionScale = false;
};
```

`HUDOverlayLayer : ViewportOverlayLayer` として §12.3 の compositor に登録。`stateLabel` の既存出力を置換。

### 12.6 B-1 ビューポート内 Wipe 比較

既存 `CompositionCompareMode`（`.ixx:41` Off/A/B/Diff）に以下を追加:

```cpp
enum class CompositionCompareMode {
  Off, A, B, Diff,
  WipeHorizontal,   // 水平ワイプ境界
  WipeVertical,     // 垂直ワイプ境界
  WipeFree          // ドラッグで境界移動
};
```

**差し込み**: `finalizeGpuRenderToViewport`（`:8343`）の背景/コンポジション描画後に、`drawComparisonWipe(renderer_, mode, boundaryT)` を追加。`setReferenceOverlayImage` / `isReferencePinned` 経路（`.ixx:159-163`）をリファレンス側として流用。境界位置 `boundaryT` は `handleMouseMove`（`:260`）で更新。

### 12.7 影響まとめ

| 変更 | 新規モジュール | 変更ファイル | 循環参照リスク |
|---|---|---|---|
| D-1 RenderScheduler | `Core.Render.Scheduler` | `CompositionRenderController.cppm` | 低（`ArtifactIRenderer&` のみ参照） |
| D-2 ViewportState | `Core.Transform.Viewport`（拡張） | `ViewportTransformer.ixx`, `CompositionRenderController.*` | 中（controller の状態削減で減少） |
| D-3 Overlay compositor | `Widget.Render.ViewportOverlay` | `ArtifactCompositionRenderOverlay.*`, controller | 中（overlay 既存 import を引き継ぎ） |
| A-1 DisplayFilter | `Widget.Render.DisplayFilter` | `CompositionRenderController.*` | 低 |
| A-3 HUD | D-3 に内含 | — | 低 |
| B-1 Wipe | — | `CompositionRenderController.*` enum + draw | 低 |

**C++20 modules 注意**: 新規 `.ixx` は値保持しない型（前方宣言可）と既存 `ArtifactIRenderer&` 参照のみに留め、実装 `.cppm` へ依存を閉じ込める。`check_module_hygiene` で検査。
