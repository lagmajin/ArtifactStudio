# M-INTERACT-1 Pen / Touch / Joystick 入力 Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`,
      `Artifact/src/Tool/ArtifactToolManager.cppm`,
      `Artifact/src/Tool/ArtifactTool.cppm`,
      `Artifact/src/Widgets/ArtifactMainWindow.cppm`,
      `Artifact/src/Input/*`,
      `Artifact/src/Service/ApplicationService.cppm`
位置づけ: `REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` で 0 hit だった **Tablet / Touch / Joystick** 入力に対応する。
参照:
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- `docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md`
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`
- `docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md` (rig / FK)

---

## 1. 目的

`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2:

> - Tablet / pen support: 0 hit
> - Touch gesture: 0 hit
> - Joystick controller: 562 hit (実体は別概念)

プロ用動画制作では **Wacom Cintiq などのペンタブレット** と **3D マウス (SpaceMouse) などのジョイスティック** が必須。現状は **マウス + キーボード** のみ。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理

### 2.1 既存資産

- `ArtifactToolManager` — tool ownership
- `ArtifactTool` — 既存 tool の基底
- `MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md` — toolbar

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Pen pressure | 0 hit | 筆圧が constant 1.0 |
| Pen tilt | 0 hit | 傾き取り込みなし |
| Touch gesture (pinch / swipe) | 0 hit | トラックパッド / タッチパネル未対応 |
| Joystick (3D mouse) | 0 hit | SpaceMouse 等の入力機器未対応 |
| Multi-touch | 0 hit | 1 pointer のみ |

---

## 3. 設計の柱

### 3.1 Pen / Tablet 入力

`Artifact/src/Input/ArtifactPenInput.cppm` を新規追加:

```cpp
class ArtifactPenInput : public QObject {
public:
    static ArtifactPenInput& instance();

    // 現在の pen 状態
    bool isActive() const;
    QPointF position() const;
    float pressure() const;       // 0.0 .. 1.0
    float tiltX() const;          // -1.0 .. 1.0
    float tiltY() const;
    float rotation() const;       // 0.0 .. 360.0
    Qt::MouseButton button() const;

signals:
    void penPressed(const QPointF& pos, float pressure);
    void penMoved(const QPointF& pos, float pressure, float tiltX, float tiltY);
    void penReleased(const QPointF& pos);
};
```

- `QTabletEvent` ベース
- `CompositionEditor` の `tabletEvent` で受信
- 既存 mouse event と **並走** (pen → tabletEvent、mouse → mouseEvent)

### 3.2 Touch Gesture

`Artifact/src/Input/ArtifactTouchGesture.cppm`:

```cpp
class ArtifactTouchGesture : public QObject {
public:
    enum class Gesture {
        Pinch,            // ズーム
        Pan,              // 平行移動
        Swipe,            // ページ切替
        Rotate,           // 回転
        Tap,              // クリック代替
        LongPress,        // 右クリック代替
    };

    // 検出
    bool recognize(QGestureEvent* event, Gesture* outGesture);

signals:
    void gestureStarted(Gesture g);
    void gestureUpdated(Gesture g, const QPointF& pos, float scale, float rotation);
    void gestureFinished(Gesture g);
};
```

- `QGestureRecognizer` ベース
- トラックパッド / タッチパネル両対応
- `QPanGesture` / `QPinchGesture` / `QSwipeGesture` 統合

### 3.3 Joystick (3D mouse / SpaceMouse)

`Artifact/src/Input/ArtifactJoystickInput.cppm`:

```cpp
class ArtifactJoystickInput : public QObject {
public:
    static ArtifactJoystickInput& instance();

    // 3D mouse の 6 軸 (tx, ty, tz, rx, ry, rz)
    QVector3D translation() const;
    QVector3D rotation() const;

    // ボタン
    QList<bool> buttons() const;

signals:
    void translated(const QVector3D& delta);
    void rotated(const QVector3D& delta);
    void buttonPressed(int id);
    void buttonReleased(int id);
};
```

- 3DConnexion SpaceMouse 等の **vendor SDK** 経由
- 任意 platform の `RawInput` も考慮
- 依存 SDK は **optional**

### 3.4 Multi-touch

`ArtifactCompositionEditor` の `touchEvent` で 10 pointer まで同時対応:

- `QTouchDevice` の取得
- 各 pointer の `position` / `pressure` / `area`
- 既存 `mouseEvent` 経路と並走

### 3.5 Tool 統合

`ArtifactToolManager` に新規 tool を登録:

- `ToolType::TabletPen` — 筆圧対応 brush
- `ToolType::Touch` — multi-touch pan / zoom / rotate
- `ToolType::Joystick` — 3D camera 操作 (将来)

### 3.6 Composition Editor 統合

`ArtifactCompositionEditor` の既存 event ハンドラに `tabletEvent` / `touchEvent` を追加:

```cpp
class ArtifactCompositionEditor {
protected:
    void tabletEvent(QTabletEvent* event) override;
    void touchEvent(QTouchEvent* event) override;
    bool gestureEvent(QGestureEvent* event) override;
};
```

- mouseEvent と **並走** (既存動作を破壊しない)
- pen pressure は `ArtifactPenInput` 経由で取得
- 既存 `mousePressEvent` / `mouseMoveEvent` は温存

### 3.7 Timeline 統合

`ArtifactTimelineWidget` の `tabletEvent`:

- ペンのみで scrub (mouse wheel と並走)
- 筆圧で playback speed 調整 (任意)

### 3.8 Shortcut 統合

`MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` の `Global` context に:

- ペン button 2 (barrel) → right click
- ペン button 1 + Ctrl → right click
- 3D mouse button → tool 切替

### 3.9 Project 保存

- pen / touch の **ユーザ設定** (筆圧カーブ、ボタン割当) は `ApplicationSettingDialog` に保存
- 個別 project には保存しない (ユーザグローバル)

### 3.10 不変条件 (Guardrails)

- `ArtifactWidgets` 触らない
- mouse event は **温存** (既存ユーザを破壊しない)
- 新規 signal-slot 接続は `penMoved / touchGestureStarted / joystickTranslated` の 3 個に限定
- `QImage` / `setStyleSheet` 流入禁止
- 3D mouse vendor SDK は **optional** (検出失敗で silent fallback)
- 既存 `QShortcut` / `InputOperator` を破壊しない

### 3.11 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `input.tablet.unavailable` (severity=info, Wacom 等の SDK 不在)
- `input.joystick.vendor-missing` (severity=info, 3D mouse vendor SDK 不在)
- `input.touch.unsupported-platform` (severity=info, multi-touch 未対応 platform)

---

## 4. フェーズ計画

### Phase 1: Pen / Tablet 入力 (P0, 1〜2 セッション)

- `ArtifactPenInput` 実装
- `CompositionEditor::tabletEvent` 追加
- 既存 mouse event と並走

**Done criteria:**
- Wacom 等で `pressure() / tiltX() / tiltY()` が取得
- 既存 mouse event が破壊されない
- `BrushTool` (将来 M-PAINT-1) で筆圧利用可能

### Phase 2: Touch Gesture (P0, 1〜2 セッション)

- `ArtifactTouchGesture` 実装
- `CompositionEditor::touchEvent` 追加
- `QGestureRecognizer` 統合

**Done criteria:**
- Pinch (ズーム) / Pan (平行移動) / Swipe / Rotate 検出
- トラックパッドで zoom
- タッチパネルで pinch

### Phase 3: 3D mouse / Joystick (P0, 1〜2 セッション)

- `ArtifactJoystickInput` 実装
- 3DConnexion SDK optional 検出
- 6 軸 (tx, ty, tz, rx, ry, rz) 取得

**Done criteria:**
- SpaceMouse で viewport の pan / orbit
- vendor SDK 不在時に silent fallback
- ボタン割当可能

### Phase 4: Multi-touch (P1, 1 セッション)

- `CompositionEditor::touchEvent` で 10 pointer 対応
- 既存 touch event と並走

**Done criteria:**
- 10 pointer 同時検出
- 既存 single touch 動作を破壊しない

### Phase 5: Shortcut + 設定 UI (P1, 1 セッション)

- ApplicationSettingDialog に「Input」ページ追加
- 筆圧カーブ / ボタン割当

**Done criteria:**
- 設定ダイアログから筆圧カーブ変更
- 設定は `FastSettingsStore` に保存

### Phase 6: Diagnostics + Project 保存 (P1, 1 セッション)

- Problem View への `input.*` 健全性 contribution

**Done criteria:**
- 設定がプロジェクトに永続化
- 旧プロジェクトが tablet 設定欠落で開ける

### Phase 7: Brush pressure 統合 (P2, 別 milestone 推奨)

- `M-PAINT-1 Paint Layer` との統合
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_BRUSH_PRESSURE_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md` | toolbar。本 milestone は新規 tool 追加。 |
| `MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` | shortcut 登録先。 |
| `MILESTONE_PAINT_LAYER_2026-06-16.md` | 将来、brush pressure を利用。 |

---

## 6. リスクと未解決論点

### 6.1 入力リスク

1. **`QTabletEvent` の platform 差**。Windows / macOS / Linux で SDK 差
2. **3D mouse vendor SDK の依存**。3DConnexion SDK は proprietary。CMake `find_package` optional
3. **Touch gesture の誤認識**。2 finger swipe を誤検出
4. **既存 mouse event との競合**。tabletEvent が mouse event も発火させる platform あり
5. **Wintab / Wacom driver 互換**。Phase 1 で実機確認

### 6.2 設計未解決

- **筆圧カーブ**。linear / ease-in / custom の 3 種。Phase 5 で UI
- **3D mouse ボタン割当**。tool 切替 vs 機能呼び出し。Phase 3 で決定
- **Touch gesture の modifier 連携**。Ctrl + pinch = zoom precision 等。Phase 2 で決定
- **Tablet キャリブレーション**。座標系の補正。Phase 5 以降

### 6.3 サブモジュール境界

- `ArtifactCore` 配下のみを書く (該当箇所のみ)
- `ArtifactWidgets` 触らない
- 3D mouse vendor SDK は `vcpkg` または optional
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- Wacom / ペンタブレットで `pressure / tilt / rotation` が取得
- Touch gesture (Pinch / Pan / Swipe / Rotate) が動作
- 3D mouse (SpaceMouse) で viewport pan / orbit
- Multi-touch で 10 pointer 同時検出
- 既存 mouse / keyboard 入力が破壊されない
- ApplicationSettingDialog に Input ページ
- 設定が `FastSettingsStore` に保存
- Problem View に `input.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2 を正式 milestone に起こした。

## 2026-07-25 現状確認

専用の `ArtifactPenInput`、`ArtifactTouchGesture`、`ArtifactJoystickInput`、`QTabletEvent`／`QTouchEvent`／gesture handler、SpaceMouse／RawInput adapter は現行ソースから確認できなかった。`ArtifactBrushTool` は存在するが、入力値は radius／opacity／eraser の設定が中心で、pressure／tiltを受け取る経路はない。

したがって本マイルストーンは、設計のみで実装未着手に近い状態。ペン筆圧・傾き、multi-touch、pinch／pan／rotate、Joystick 6軸、Timeline／Composition Editorへの安全な入力統合、optional SDK、実機検証が未完了である。
