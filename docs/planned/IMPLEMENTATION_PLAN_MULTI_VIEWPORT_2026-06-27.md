# Implementation Plan: Multi-Viewport Layout System (M-VP-1)

更新日: 2026-06-27
元ドキュメント: `docs/planned/MILESTONE_MULTI_VIEWPORT_LAYOUT_2026-06-01.md` / `docs/planned/IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-02.md`

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **新規** | `ArtifactCore/include/Viewport/ViewportLayoutManager.ixx` | レイアウト管理 core |
| **新規** | `ArtifactCore/src/Viewport/ViewportLayoutManager.cppm` | 実装 |
| **新規** | `ArtifactCore/include/Event/ViewportLayoutChangedEvent.ixx` | EventBus イベント |
| **新規** | `Artifact/src/Widgets/Render/ArtifactMultiViewportContainer.cppm` | 複数ペイン親 widget |
| **新規** | `Artifact/include/Widgets/Render/ArtifactMultiViewportContainer.ixx` | ヘッダー |
| **変更** | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 複数インスタンス前提へ分離 |
| **変更** | `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx` | ViewportInstanceID 追加 |
| **変更** | `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | 単一→親責務へ |
| **変更** | `Artifact/include/Widgets/Render/ArtifactCompositionEditor.ixx` | 公開 API 整理 |
| **変更** | `Artifact/src/AppMain.cppm` | 中央ドック差し替え |
| **変更** | `Artifact/src/Widgets/ArtifactViewMenu.cppm` | レイアウト切替メニュー追加 |

---

## User Review Required

> [!IMPORTANT]
> **Diligent Rendering Serialization**:
> Diligent's `IDeviceContext` is currently used for direct rendering in `ArtifactCompositionRenderController`. When multiple controllers render simultaneously (across multiple viewports), context rendering collision can occur. 
> To mitigate this safely for the MVP, we propose wrapping the render submission in a `QMutex` to serialize drawing commands, coupled with a late-update polling mechanism (100ms timer) for non-active viewports. We will replace this with a proper `ArtifactRenderScheduler` deduplication queue in a future milestone.

### 2026-07-12 RenderScheduler foundation progress

- `RenderTask` に明示的な `deduplicationKey` を追加した。
- key未設定Taskは従来どおり常に投入し、既存挙動を維持する。
- 同一keyのpending / in-flight Taskは後着をcancel扱いにして重複実行しない。
- 完了、pending cancel、cancel-allの各経路でkeyを解放する。
- `BatchRenderer` のframe taskはframe単位のkeyを設定する。
- Source/diff checked only. Build / runtime verification is intentionally deferred.

## Open Questions

> [!NOTE]
> 1. **Default camera behaviors**: Viewports assigned with "None" or "(Default Camera)" will render using the standard view camera. Is this behavior sufficient, or should we pre-assign specific angles (e.g., Top, Left, Front) to specific viewports in the 4-Up layout?
> 2. **Render Performance Safety**: Running 4 viewports concurrently will double or quadruple composition evaluation cost. Are there specific low-end target hardware profiles we should warn or restrict from using high-density layouts, or is a warning HUD sufficient?

---

## 変更詳細

### 1. ViewportLayoutManager (Core 新規)

**ファイル**: `ArtifactCore/include/Viewport/ViewportLayoutManager.ixx`

```cpp
export module Viewport.ViewportLayoutManager;

export enum class ViewportLayout {
    Single,
    HorizontalSplit,
    FourUp,
    OnePlusThree
};

export struct ViewportAssignment {
    // ペイン識別子 (0=primary, 1=secondary, 2-3=sub)
    int paneId = 0;
    // 割り当てカメラレイヤー（empty=default camera）
    std::optional<CameraLayerID> cameraLayerId;
    // ペインごとの zoom/pan 状態
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
};

export struct ViewportLayoutState {
    ViewportLayout layout = ViewportLayout::Single;
    std::vector<ViewportAssignment> assignments;
    int activePaneId = 0;
};

export class ViewportLayoutManager {
public:
    static ViewportLayoutManager& instance();

    ViewportLayoutState current() const;
    void setLayout(ViewportLayout layout);
    void assignCamera(int paneId, CameraLayerID cameraId);
    void setActivePane(int paneId);
    void saveState();
    void restoreState();

private:
    ViewportLayoutManager() = default;
    ViewportLayoutState state_;
};
```

**実装ファイル:** `ArtifactCore/src/Viewport/ViewportLayoutManager.cppm`

- `saveState()/restoreState()` は `FastSettingsStore` にキー `viewport/layout` で保存
- `setLayout()` 呼び出し時に `ViewportLayoutChangedEvent` を EventBus へ発行

---

**ファイル**: `ArtifactCore/include/Event/ViewportLayoutChangedEvent.ixx`

```cpp
export module Event.ViewportLayoutChangedEvent;

import Core.EventBus.Event;

export struct ViewportLayoutChangedEvent : Event {
    ViewportLayout layout;
    std::vector<ViewportAssignment> assignments;
    int activePaneId;
};
```

---

### 2. ArtifactMultiViewportContainer (新規)

**ファイル**: `Artifact/include/Widgets/Render/ArtifactMultiViewportContainer.ixx`

```cpp
export module Widget.Render.MultiViewportContainer;

import Qt6.QtWidgets;
import Qt6.QtCore;
import Render.CompositionRenderController;
import Layer.Camera;
import Viewport.ViewportLayoutManager;
import Core.EventBus;

export class ArtifactMultiViewportContainer : public QWidget {
    W_OBJECT(ArtifactMultiViewportContainer)

public:
    explicit ArtifactMultiViewportContainer(QWidget* parent = nullptr);
    ~ArtifactMultiViewportContainer();

    void setComposition(CompositionID compId);
    CompositionID currentComposition() const;

public slots:
    void onLayoutChanged(const ViewportLayoutChangedEvent& evt);

signals:
    void activePaneChanged(int paneId);
    void cameraAssigned(int paneId, CameraLayerID cameraId);

private slots:
    void rebuildPanes();
    void onPaneActivated(int paneId);
    void onCameraSelectionChanged(int paneId, CameraLayerID cameraId);

private:
    struct Pane {
        QWidget* container = nullptr;
        ArtifactCompositionRenderWidget* renderWidget = nullptr;
        std::unique_ptr<ArtifactCompositionRenderController> controller;
        ViewportAssignment assignment;
        QComboBox* cameraSelector = nullptr;
    };

    void setupPane(Pane& pane, int paneId);
    void teardownPane(Pane& pane);
    void distributeFrameEvent(const FrameChangedEvent& evt);

    CompositionID currentComp_;
    std::vector<Pane> panes_;
    ViewportLayout currentLayout_;
    int activePaneId_ = 0;
    QTimer* lowHzPollTimer_ = nullptr; // 非アクティブペイン用
};
```

**実装ポイント:**

- `rebuildPanes()`: `ViewportLayoutManager::current()` を読んでレイアウトに応じて `panes_` を再構築。`Single` のときは 1 ペイン、`FourUp` のときは 4 ペイン。`QGridLayout` を使い、レイアウト種別で row/column を切り替える。
- 各ペインの `renderWidget` は `ArtifactCompositionRenderWidget` を heap 確保し、`container` のレイアウトへ追加。
- controller は各ペインごとに `std::make_unique<ArtifactCompositionRenderController>(paneId)` で生成。
- `onLayoutChanged()`: `ViewportLayoutManager` から発行された event を受け取り、`rebuildPanes()` を呼ぶ。
- `distributeFrameEvent()`: `FrameChangedEvent` を受信したら全 `controller` へ伝達。ただし非アクティブペインは `lowHzPollTimer_`（100ms 間隔）で late update する。
- 各ペイン左上に `cameraSelector`（`QComboBox`）を置き、カメラレイヤー一覧から選択。切替時に `ViewportLayoutManager::assignCamera(paneId, cameraId)` を呼び、さらに `ArtifactCompositionRenderController::setViewportCamera(cameraLayerId)` を呼ぶ。

---

### 3. ArtifactCompositionRenderController 変更

**ヘッダー変更** (`Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`):

- 新規コンストラクタ: `explicit ArtifactCompositionRenderController(int viewportInstanceId, QObject* parent = nullptr);`
- 新規メソッド: `void setViewportCamera(std::optional<CameraLayerID> cameraId);`
- 新規メソッド: `void setQualityPreset(PreviewQualityPreset preset);`
- 既存の static/shared 状態（global dirty flag 等）を `viewportInstanceId_` スコープに分離。static を維持しつつ、key に `viewportInstanceId_` を加える。

**実装変更** (`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`):

- コンストラクタ引数 `viewportInstanceId` をメンバに保存。
- `renderFrame()` / `evaluateLayer()` 内部で使用する dirty flag を `static std::unordered_map<int, bool>` に移行（既存の1個前提をMap化）。
- `setViewportCamera()`: `ArtifactViewportCamera` を持ち、orthographic/perspective の projection matrix を即時再計算。`ArtifactCameraLayer` を読み取り。
- `setActive(bool)`: 外部から呼ばれ、非アクティブ時はデバッグ描画を省略するフラグを立てる（`lowHzPollTimer_` の動作と連動）。

**既存の制約への対応:**
- Diligent `IDeviceContext` は内部で1つ使っている。複数 controller が同一フレームで submit すると競合する。`ArtifactRenderScheduler` / `RenderCommandBuffer` 経路で直列化する（当面は `QMutex` で critical section）。本来は RenderQueue の serialization に任せるが、MVP では mutex で先に潰す。

---

### 4. ArtifactCompositionEditor 変更

**ヘッダー変更** (`Artifact/include/Widgets/Render/ArtifactCompositionEditor.ixx`):

- 子 widget の `ArtifactCompositionRenderWidget` を所持せず、`ArtifactMultiViewportContainer* viewportContainer_` を所有するよう変更。
- `setComposition()` は `viewportContainer_->setComposition()` へ委譲。
- ツール操作 / キーボード入力は `viewportContainer_->activeRenderWidget()` を通して行う。

**実装変更** (`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`):

- コンストラクタで `ArtifactMultiViewportContainer` を生成し setCentralWidget する。
- フッターの `ArtifactCompositionViewerFooter` は active pane の renderWidget と同期。
- オーバーレイ (`ArtifactCompositionRenderOverlay`) は active ペインにのみ描画するモードを追加（`overlayTargetPaneId_` を管理）。

---

### 5. AppMain.cppm の変更

**対象行**: 中央ドック登録部（`Composition Viewer` 登録箇所）

変更前:
```cpp
auto* editor = new ArtifactCompositionEditor();
mainWindow->addDockedWidget("Composition Viewer", ... , editor);
```

変更後:
```cpp
auto* multiViewport = new ArtifactMultiViewportContainer();
multiViewport->setComposition(currentCompId);
mainWindow->addDockedWidget("Composition Viewer", ... , multiViewport);
```

---

### 6. ArtifactViewMenu の変更

**ファイル**: `Artifact/src/Widgets/ArtifactViewMenu.cppm`

`View` メニューに以下を追加:

```
View →
  Single View
  2-Up Horizontal
  4-Up
  1+3 Sub-views
  ─────────────
  Assign Camera to Active Pane →
    (Camera Layer 一覧)
```

- `ViewportLayoutManager::instance().setLayout(...)` を呼ぶ。
- `Assign Camera to Active Pane` は現在の `activePaneId` を使い、`ViewportLayoutManager::assignCamera(activePaneId, cameraId)` を実行。

---

## View メメニュー層の設計詳細（`ArtifactViewMenu.cppm`）

### 新設する private メソッド群

```cpp
private:
    void setupViewportLayoutMenu();
    void rebuildCameraAssignmentMenu();
    void onViewportLayoutActionTriggered(ViewportLayout layout);
    void onCameraAssignedToPane(CameraLayerID cameraId);

    QActionGroup* viewportLayoutGroup_ = nullptr;
    QMenu* cameraAssignmentMenu_ = nullptr;
```

### レイアウト切替UI

`setupViewportLayoutMenu()` で View メニューの先頭に以下を挿入:

```cpp
QMenu* viewportMenu = addSubMenu(tr("Viewport Layout"));
viewportLayoutGroup_ = new QActionGroup(this);

auto addLayoutAction = [&](ViewportLayout layout, const QString& label) {
    QAction* a = viewportMenu->addAction(label);
    a->setCheckable(true);
    a->setActionGroup(viewportLayoutGroup_);
    a->setData(QVariant::fromValue(static_cast<int>(layout)));
    connect(a, &QAction::triggered, this, [this, layout]() {
        onViewportLayoutActionTriggered(layout);
    });
};

addLayoutAction(ViewportLayout::Single, tr("Single View"));
addLayoutAction(ViewportLayout::HorizontalSplit, tr("2-Up Horizontal"));
addLayoutAction(ViewportLayout::FourUp, tr("4-Up"));
addLayoutAction(ViewportLayout::OnePlusThree, tr("1+3 Sub-views"));
```

### カメラ割り当てUI

`rebuildCameraAssignmentMenu()` で毎回以下の要素を生成:

```cpp
int paneCount = ViewportLayoutManager::instance().current().assignments.size();

cameraAssignmentMenu_->clear();
for (int i = 0; i < paneCount; ++i) {
    QMenu* sub = cameraAssignmentMenu_->addMenu(
        tr("Pane %1").arg(i + 1)
    );
    QAction* noneAct = sub->addAction(tr("(Default Camera)"));
    noneAct->setData(QVariant::fromValue(std::make_pair(i, CameraLayerID{})));
    connect(noneAct, &QAction::triggered, this, [this, i]() {
        ViewportLayoutManager::instance().assignCamera(i, {});
    });

    for (const auto& [camId, camName] : ArtifactProjectService::instance()
                                             .currentComposition()
                                             ->cameraLayerNames()) {
        QAction* act = sub->addAction(QString::fromStdString(camName));
        act->setData(QVariant::fromValue(std::make_pair(i, camId)));
        connect(act, &QAction::triggered, this, [this, camId, i]() {
            ViewportLayoutManager::instance().assignCamera(i, camId);
        });
    }
}
```

### ハンドラ実装

```cpp
void ArtifactViewMenu::onViewportLayoutActionTriggered(ViewportLayout layout) {
    ViewportLayoutManager::instance().setLayout(layout);
    rebuildCameraAssignmentMenu();
}
```

### EventBus との連携

`ArtifactViewMenu` は constructor で以下を登録:

```cpp
EventBus::subscribe<ViewportLayoutChangedEvent>(
    this,
    [this](const ViewportLayoutChangedEvent& evt) {
        for (QAction* a : viewportLayoutGroup_->actions()) {
            if (static_cast<ViewportLayout>(a->data().toInt()) == evt.layout) {
                a->setChecked(true);
                break;
            }
        }
        rebuildCameraAssignmentMenu();
    }
);
```

---

## 実装順番

1. `ViewportLayoutManager` + `ViewportLayoutChangedEvent` (Core)
2. `ArtifactMultiViewportContainer` — Single と HorizontalSplit のみ先に実装
3. `ArtifactCompositionRenderController` の複数インスタンス対応
4. `ArtifactCompositionEditor` → `ArtifactMultiViewportContainer` 切替
5. `AppMain.cppm` のドック登録差し替え
6. `ArtifactViewMenu` に Viewport Layout + Assign Camera メニュー追加
7. `FourUp` / `OnePlusThree` レイアウトの builder
8. 非アクティブペイン低Hzポーリング
9. カメラ orthographic パラメータの各ペイン反映（M-CP-1 統合）
10. レイアウト状態の FastSettingsStore 永続化

---

## 見積もり

| Step | 内容 | 見積 |
|---|---|---|
| 1 | Core: ViewportLayoutManager + Event | 4–6h |
| 2 | MultiViewportContainer (Single/H-Split) | 8–12h |
| 3 | RenderController 複数インスタンス対応 | 6–10h |
| 4 | CompositionEditor 切替 | 4–6h |
| 5 | AppMain ドック差し替え | 2–4h |
| 6 | ViewMenu 実装 | 4–6h |
| 7 | FourUp / 1+3 レイアウト | 4–6h |
| 8 | 非アクティブ低Hzポーリング | 3–4h |
| 9 | Orthographic 反映 | 4–6h |
| 10 | FastSettingsStore 永続化 | 2–3h |

合計: 41–57h

---

## 注意事項

- **Diligent のマルチ submit**: 複数ペインが同じ Composition を評価すると同一フレームで複数回 full render が走る。MVP では `QMutex` + `late update` で先に潰す。後日 `ArtifactRenderScheduler` による適切な dedup へ置き換え。
- **View メニュー巻き戻し対応**: `ArtifactViewMenu.cppm` は enum を QActionGroup で管理するため、View メニュー項目の checked/unchecked 制御を忘れずに。
- **Central widget 差し替え**: 既存 of `ArtifactCompositionEditor` を直接 delete せず、`ArtifactMultiViewportContainer` へ ownership を移譲する経路を取る。
- **新規グローバル signal 禁止への対応**: `ViewportLayoutChangedEvent` は EventBus 公開型として追加する。
