# 実装案: M-APP-7 EditMode → ツール自動マッピングの UI 接続

> マイルストーン: `MILESTONES_BACKLOG.md` 1124 行 / `MILESTONE_APP_LAYER_COMPLETENESS.md` 61-66 行  
> 状態: 実装済み・実機未確認
> 作成: 2026-06-13

## 目標

UI の編集モード (EditMode) 選択を `ArtifactToolService` に接続し、ツールバーボタンおよびキーボードショートカットでモード切替が可能にする。  
また、DisplayMode (表示モード) 切替のショートカットを追加する。

## Current Status (2026-07-13)

- 既存`ArtifactToolBar`の単一tool dispatcherから`ArtifactToolService::setEditMode()`を同期するsliceを実装
- Hand / Zoom / Scrub PreviewはView、transform系はTransform、PenはMask、Brush / EraserはPaint、Shape / Rectangle / TextはShapeへ対応
- Brush / Eraserも既存tool serviceのactive toolへ渡すよう統一
- 新しいtoolbar button、signal / slot、QtCSSは追加していない
- 既存`ToolChangedEvent` subscriptionでserviceからtoolbarのchecked stateへ戻す双方向同期を確認
- Brush / Eraserのtool label mappingを補完し、programmatic切替でもchecked stateが追従
- Clone Stampは専用`ToolType`がないため、既存request経路を維持してBrushへ誤表示しない
- `V / T / M / P`をEditMode専用shortcutへ再割当する案は、既存のSelection / Text / mask property / position shortcutと競合するため採用しない
- DisplayMode viewport反映は別マイルストーンM-APP-8として分離
- build / testはユーザー方針により実施しない

---

## 既存実装状況

### ArtifactToolService (実装済み)
- `setEditMode(EditMode mode)` - モード変更時に自動でツールを切り替える
- `setDisplayMode(DisplayMode mode)` - 表示モード変更
- シグナル: `editModeChanged(mode)`, `displayModeChanged(mode)`

### ArtifactToolBar (実装済み)
- ツールアクション（選択、手のひら、ズームなど）あり
- ビューモードアクション（Normal/Grid/Detail）あり
- `setWorkspaceMode(WorkspaceMode)` で workspace 切替 UI あり

### ArtifactMainWindow
- `keyPressEvent` は `Shift+Space` の immersive dock、`Ctrl+1..4` の workspace、feature flag 下の command palette を処理
- V/T/M/P と裸の 1..4 は既存の tool／property／表示操作との競合回避のため EditMode／DisplayMode shortcut には割り当てない
- ツール信号のルーティングは個別ツールのみ (`moveToolRequested` など)

---

## 実装方針

### 方針A: ArtifactToolBar にモード切替アクションを追加

ツール切り替えアクション（選択/手のひら/ペン）の前に、編集モードグループを追加。

```
[ホーム] | [View] [Transform] [Mask] [Paint] | [選択] [手のひら] ...
```

**利点**: 既存ワークフローに自然に組み込める  
**欠点**: ツールバー幅が増える可能性

### 方針B: ツールバーツール選択時に自動判定

選択ツールクリック時、選択中レイヤーの種類により EditMode を自動判定して切り替える。

**利点**: UI を増やさない  
**欠点**: 予期ぬモード切替でユーザー混乱の可能性

### 推奨: 方針A + キーボードショートカット実装

既存の workspace ボタンと同じパターンで、モード切替専用のシンプルなドロップダウンボタンを追加。

---

## 実装詳細

### 1. ArtifactToolBar.ixx への追加

```cpp
// EditMode group actions
QActionGroup *editModeGroup_ = nullptr;
QAction *viewModeAction_ = nullptr;    // V: View mode
QAction *transformModeAction_ = nullptr; // T: Transform mode
QAction *maskModeAction_ = nullptr;    // M: Mask mode
QAction *paintModeAction_ = nullptr;   // P: Paint mode

// DisplayMode shortcuts actions
QAction *colorViewAction_ = nullptr;   // 1: Color
QAction *alphaViewAction_ = nullptr;   // 2: Alpha
QAction *maskViewAction_ = nullptr;    // 3: Mask
QAction *wireframeViewAction_ = nullptr; // 4: Wireframe
```

### 2. ArtifactToolBar.cppm 追加実装

#### (1) EditMode アクション追加

```cpp
impl_->editModeGroup_ = new QActionGroup(this);
impl_->editModeGroup_->setExclusive(true);

impl_->viewModeAction_ = new QAction(this);
impl_->viewModeAction_->setText("V");
impl_->viewModeAction_->setToolTip("View Mode (V)");
impl_->viewModeAction_->setShortcut(QKeySequence(Qt::Key_V));
impl_->viewModeAction_->setCheckable(true);
impl_->viewModeAction_->setChecked(true);
impl_->editModeGroup_->addAction(impl_->viewModeAction_);
addAction(impl_->viewModeAction_);

// Transform, Mask, Paint も同様
// ...

QObject::connect(impl_->editModeGroup_, &QActionGroup::triggered, this,
    [this](QAction* action) {
        if (!action) return;
        EditMode mode = EditMode::View;
        if (action == impl_->transformModeAction_) mode = EditMode::Transform;
        else if (action == impl_->maskModeAction_) mode = EditMode::Mask;
        else if (action == impl_->paintModeAction_) mode = EditMode::Paint;
        setEditMode(mode);
    });
```

#### (2) DisplayMode アクション追加

```cpp
// 1, 2, 3, 4 ショートカット（グローバル）
impl_->colorViewAction_ = new QAction(this);
impl_->colorViewAction_->setShortcut(QKeySequence(Qt::Key_1));
// ...

QObject::connect(impl_->colorViewAction_, &QAction::triggered, this,
    [this]() { setDisplayMode(DisplayMode::Color); });
// ...
```

#### (3) サービス接続

```cpp
void ArtifactToolBar::setEditMode(EditMode mode) {
    if (auto* app = Artifact::ApplicationService::instance()) {
        if (auto* ts = app->toolService()) {
            ts->setEditMode(mode);
        }
    }
}

void ArtifactToolBar::setDisplayMode(DisplayMode mode) {
    if (auto* app = Artifact::ApplicationService::instance()) {
        if (auto* ts = app->toolService()) {
            ts->setDisplayMode(mode);
        }
    }
}
```

### 3. ArtifactMainWindow.cppm への追加

#### (1) keyPressEvent 実装

```cpp
void ArtifactMainWindow::keyPressEvent(QKeyEvent* event) {
    const Qt::Key key = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    
    // EditMode shortcuts (V, T, M, P)
    if (!modifiers || modifiers == Qt::ShiftModifier) {
        switch (key) {
        case Qt::Key_V:
            if (auto* app = Artifact::ApplicationService::instance()) {
                if (auto* ts = app->toolService()) {
                    ts->setEditMode(EditMode::View);
                }
            }
            event->accept();
            return;
        case Qt::Key_T:
            if (auto* app = Artifact::ApplicationService::instance()) {
                if (auto* ts = app->toolService()) {
                    ts->setEditMode(EditMode::Transform);
                }
            }
            event->accept();
            return;
        case Qt::Key_M:
            if (auto* app = Artifact::ApplicationService::instance()) {
                if (auto* ts = app->toolService()) {
                    ts->setEditMode(EditMode::Mask);
                }
            }
            event->accept();
            return;
        case Qt::Key_P:
            if (auto* app = Artifact::ApplicationService::instance()) {
                if (auto* ts = app->toolService()) {
                    ts->setEditMode(EditMode::Paint);
                }
            }
            event->accept();
            return;
        }
    }
    
    // DisplayMode shortcuts (1, 2, 3, 4)
    if (!modifiers) {
        switch (key) {
        case Qt::Key_1:
            if (auto* app = Artifact::ApplicationService::instance()) {
                if (auto* ts = app->toolService()) {
                    ts->setDisplayMode(DisplayMode::Color);
                }
            }
            event->accept();
            return;
        case Qt::Key_2:
            ts->setDisplayMode(DisplayMode::Alpha);
            event->accept();
            return;
        case Qt::Key_3:
            ts->setDisplayMode(DisplayMode::Mask);
            event->accept();
            return;
        case Qt::Key_4:
            ts->setDisplayMode(DisplayMode::Wireframe);
            event->accept();
            return;
        }
    }
    
    QMainWindow::keyPressEvent(event);
}
```

#### (2) ツールバーシグナルルーティング

```cpp
// ArtifactMainWindow コンストラクタに追加
QObject::connect(toolBar, &ArtifactToolBar::viewModeRequested, this,
    [this]() { /* 現在編集中のレイヤーに応じてモード切替 */ });
QObject::connect(toolBar, &ArtifactToolBar::transformModeRequested, this,
    [this]() { impl_->setEditMode(EditMode::Transform); });
// ...
```

### 4. アイコン追加

| ファイル | 用途 |
|----------|------|
| `Artifact/App/Icon/Studio/mode_view.svg` | View モードアイコン |
| `Artifact/App/Icon/Studio/mode_transform.svg` | Transform モードアイコン |
| `Artifact/App/Icon/Studio/mode_mask.svg` | Mask モードアイコン |
| `Artifact/App/Icon/Studio/mode_paint.svg` | Paint モードアイコン |

---

## 作業順

### 実装監査結果（2026-07-30）

- [x] 既存 `ArtifactToolBar` の tool dispatcher から `ArtifactToolService::setEditMode()` へ同期
- [x] `ToolChangedEvent` による service → toolbar checked state 同期
- [x] Brush / Eraser の mapping と Clone Stamp の非誤表示境界
- [x] 競合する V/T/M/P、裸の 1..4 shortcut を追加しない判断
- [x] MainWindow の実在 shortcut（immersive / workspace / command palette）を文書化
- [ ] 実機での tool 切替、checked state、shortcut 競合の受け入れ確認

以下の初期設計案は、現在の責務境界と shortcut 競合を確認するための参考案として保持する。

1. **ArtifactToolBar.ixx** - アクションメンバー追加
2. **ArtifactToolBar.cppm** - EditMode/DisplayMode アクション実装
3. **ArtifactMainWindow.cppm** - keyPressEvent ショートカット実装
4. **アイコン作成** - SVG アイコン4つ追加

---

## 作業工数見積

| タスク | 見積時間 |
|--------|----------|
| ヘッダ編集・アクション追加 | 1h |
| cppm 実装 | 2-3h |
| キーボードショートカット | 1h |
| アイコン作成 | 1h |
| 動作確認・adjust | 1-2h |
| **合計** | 6-8h |

---

## UI 表示例

```
┌─────────────────────────────────────────────────────────┐
│ [ホーム] | [V] [T] [M] [P] | [選択] [手] [ズーム] ...   │
└─────────────────────────────────────────────────────────┘
```

- `[V]` = View (Hand ツール)
- `[T]` = Transform (Selection ツール)  
- `[M]` = Mask (Pen ツール)
- `[P]` = Paint (Shape ツール)

---

## 関連

- `Artifact/include/Tool/Tool.ixx` - EditMode/DisplayMode 定義
- `Artifact/include/Service/ArtifactToolService.ixx` - setEditMode 実装
- `docs/WIDGET_MAP.md` - UI 名称確認
