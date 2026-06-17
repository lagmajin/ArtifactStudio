# M-XR-1 VR / XR Support Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Render/ArtifactIRenderer.cppm`,
      `Artifact/src/Composition/ArtifactCameraLayer.cppm`,
      `ArtifactCore/include/Transform/Camera.ixx`,
      `ArtifactCore/src/Transform/Camera.ixx`,
      `ArtifactCore/include/Time/TimeCode.ixx`
位置づけ: VR / XR (OpenXR) 対応のための **foundation**。現状は 0 hit (`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2) で、UI は `Artifact3DModelViewer` のみ。
参照:
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2
- `docs/analysis/MOTION_GRAPHICS_AD_PRODUCTION_THINKING_MEMO_2026-05-28.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`
- `docs/planned/MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md`
- `docs/planned/MILESTONE_CAMERA_PROJECTION_2026-03-31.md`
- `docs/planned/MILESTONE_PEN_TOUCH_JOYSTICK_2026-06-16.md` (3D mouse 経由)

---

## 1. 目的

`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2:

> - VR / XR support: 18 hit (Expression 評価の `value` / `Array` 名前空間と衝突。実際は別概念)

VR / XR での immersive editing は **次の制作環境の標準**。現状は完全に未着手で、3D 編集は `Artifact3DModelViewer` のみ。

本 milestone は **OpenXR foundation** + **stereo camera** + **HMD view matrix** を導入する。フル実装は後段の milestone に分離。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理

### 2.1 既存資産

- `Artifact3DModelViewer.cppm` — 3D model 表示
- `ArtifactCameraLayer.cppm` — camera layer
- `ArtifactCore/include/Transform/Camera.ixx` — camera 抽象
- `MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md` — 3D viewport

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| OpenXR 統合 | 0 hit | VR runtime 不在 |
| Stereo camera | 0 hit | 左目 / 右目の stereo render なし |
| HMD view matrix | 0 hit | pose tracking なし |
| Immersive viewport | 0 hit | VR viewport widget 不在 |
| 6DoF controller | 0 hit | controller 入力 (Touch / Index / Vive) なし |

---

## 3. 設計の柱

### 3.1 OpenXR Foundation

`ArtifactCore/include/XR/OpenXR.ixx` を新規追加 (optional):

```cpp
namespace ArtifactCore {

class OpenXRSession {
public:
    static OpenXRSession& instance();

    bool initialize();     // OpenXR runtime 検出
    void shutdown();

    bool isAvailable() const;

    // HMD
    QMatrix4x4 hmdPose() const;     // 現在の HMD pose
    QMatrix4x4 hmdView(int eye) const;  // eye = 0 (left) / 1 (right)

    // session
    bool beginSession();
    void endSession();
    bool isSessionActive() const;

    // runtime 名
    QString runtimeName() const;  // "SteamVR" / "Oculus" / "OpenXR"
};

} // namespace ArtifactCore
```

- **optional 依存**: OpenXR SDK がある場合のみ
- runtime 不在時は silent fallback (no-op)
- `OpenXR::Context` で `XR_KHR_composition_layer_*` 等を扱えるよう拡張可能

### 3.2 Stereo Camera

`ArtifactCore/include/Transform/StereoCamera.ixx` を新規追加:

```cpp
struct StereoCamera {
    QMatrix4x4 leftEyeView;
    QMatrix4x4 rightEyeView;
    QMatrix4x4 projection;        // 共通
    float ipd;                   // inter-pupillary distance (m)
    float nearPlane;
    float farPlane;

    static StereoCamera fromHmd(QMatrix4x4 hmdPose, float ipd = 0.064f);
};
```

- `ArtifactCameraLayer` に `setStereoMode(StereoMode)` 追加
- `StereoMode::Mono` / `Stereo::TopBottom` / `Stereo::SideBySide`

### 3.3 Render 統合

`ArtifactCompositionRenderController` の render 経路に stereo 分岐:

```cpp
void renderOneFrame(FramePosition f) {
    if (vrMode) {
        // left eye
        setViewMatrix(stereoCamera.leftEyeView);
        renderInternal(f);
        // right eye
        setViewMatrix(stereoCamera.rightEyeView);
        renderInternal(f);
        // submit to OpenXR swap chain
    } else {
        setViewMatrix(monoCameraView);
        renderInternal(f);
    }
}
```

- `swap chain` は OpenXR の `XR_KHR_composition_layer_*` を経由
- 既存 Diligent の `ISwapChain` と並走

### 3.4 XR Viewport Widget

`Artifact/src/Widgets/Render/ArtifactXRViewport.cppm` を新規追加:

```cpp
class ArtifactXRViewport : public QWidget {
public:
    explicit ArtifactXRViewport(QWidget* parent = nullptr);

    void setComposition(ArtifactComposition* comp);

    // stereo / mono 切替
    void setStereoMode(StereoMode mode);

    // OpenXR session
    void startXRSession();
    void stopXRSession();

signals:
    void hmdPoseUpdated(QMatrix4x4 pose);
};
```

- HMD 接続時に fullscreen 表示
- mono 時は通常 viewport (preview 用途)

### 3.5 6DoF Controller 入力

`M-INTERACT-1 Pen/Touch/Joystick` で 3D mouse を追加済み。XR controller はその上に乗る:

- 左手 controller → tool 切替 (Brush / Move / Rotate)
- 右手 controller → gizmo 操作
- トリガー → click
- グリップ → multi-select

### 3.6 Immersive Editor

XR session 中の editor:

- Composition Editor を **HMD 上に全画面表示**
- Timeline は **floating panel** (3D 空間)
- Inspector は **floating panel**
- 3D gizmo は 両手 controller で操作

→ Phase 5 以降。本 milestone は foundation のみ

### 3.7 Project 保存

- `ArtifactComposition` に `vrMode` / `stereoMode` 設定追加
- 旧 project は mono で開く

### 3.8 不変条件 (Guardrails)

- OpenXR は **optional 依存**。runtime / SDK 不在で silent fallback
- 既存 render path を破壊しない
- 新規 signal-slot 接続は `hmdPoseUpdated / xrSessionChanged` の 2 個に限定
- `QImage` / `setStyleSheet` 流入禁止
- `ArtifactWidgets` 触らない

### 3.9 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `xr.runtime-missing` (severity=info, OpenXR runtime 不在)
- `xr.session.failed` (severity=error, session 開始失敗)
- `xr.hmd-disconnected` (severity=warning, HMD 切断)
- `xr.stereo-mismatch` (severity=warning, composition が stereo 未対応)

---

## 4. フェーズ計画

### Phase 1: OpenXR Foundation (P0, 1〜2 セッション)

- `OpenXRSession` 実装
- optional 検出
- HMD pose query

**Done criteria:**
- OpenXR runtime 検出
- `hmdPose()` が matrix を返す
- runtime 不在で silent fallback

### Phase 2: StereoCamera (P0, 1 セッション)

- `StereoCamera` 実装
- `ArtifactCameraLayer::setStereoMode` 追加
- IPD / near / far のパラメータ

**Done criteria:**
- stereo camera 計算
- `StereoMode::Mono / TopBottom / SideBySide` 切替
- `ArtifactCameraLayer` 永続化

### Phase 3: Render 統合 (P0, 1〜2 セッション)

- `ArtifactCompositionRenderController` に stereo render 分岐
- swap chain 統合
- frame rate 維持

**Done criteria:**
- stereo render 動作
- 90 Hz HMD で安定
- 既存 mono 動作を破壊しない

### Phase 4: XR Viewport Widget (P0, 1〜2 セッション)

- `ArtifactXRViewport` 実装
- fullscreen / mono preview 切替

**Done criteria:**
- HMD 接続時に fullscreen
- mono preview 動作
- session 制御

### Phase 5: Controller 入力 (P1, 1 セッション)

- 6DoF controller 統合
- `M-INTERACT-1` と並走

**Done criteria:**
- 左手 / 右手 controller 識別
- tool 切替動作

### Phase 6: Project 保存 + Diagnostics (P1, 1 セッション)

- project JSON に `vrMode / stereoMode`
- Problem View への `xr.*` 健全性 contribution

**Done criteria:**
- project 保存 → 再読込で復元
- 旧 project が mono で開く
- Problem View 表示

### Phase 7: Immersive Editor (P2, 別 milestone 推奨)

- 3D 空間 editor
- floating panel
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_XR_IMMERSIVE_EDITOR_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md` | 3D viewport 基礎。XR はその延長。 |
| `MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md` | material。XR は material の表示面。 |
| `MILESTONE_CAMERA_PROJECTION_2026-03-31.md` | camera。stereo camera は拡張。 |
| `MILESTONE_PEN_TOUCH_JOYSTICK_2026-06-16.md` | 3D mouse / XR controller 接続。 |

---

## 6. リスクと未解決論点

### 6.1 実装リスク

1. **OpenXR SDK 依存**。Khronos OpenXR loader は CMake `find_package(OpenXR)` 経由
2. **Vulkan / D3D12 依存**。OpenXR は graphics API 抽象に依存
3. **stereo render performance**。90 Hz × 2 eye = 180 fps 相当
4. **HMD 接続 / 切断**。動的検出が必要
5. **IPD 設定**。ユーザ個別の調整

### 6.2 設計未解決

- **WebXR 対応**。将来
- **foveated rendering**。Phase 7 以降
- **eye tracking**。Quest Pro 等
- **mixed reality (pass-through)**。Quest 3 等

### 6.3 サブモジュール境界

- `ArtifactCore/include/XR/OpenXR.ixx` を **optional** で追加
- `ArtifactCore/include/Transform/StereoCamera.ixx` を新規追加
- `Artifact/src/Widgets/Render/ArtifactXRViewport.cppm` を新規追加
- `Artifact/src/Composition/ArtifactCameraLayer.cppm` 拡張
- `ArtifactCore/CMakeLists.txt` に optional 登録
- `ArtifactWidgets` 触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- OpenXR runtime 検出
- `hmdPose()` が matrix を返す
- StereoCamera 計算
- `ArtifactCameraLayer` で stereo mode 切替
- Render で stereo 動作
- 90 Hz HMD で安定
- XR Viewport widget 表示
- 6DoF controller 認識
- project 保存 → 再読込で復元
- 旧 project が mono で開く
- Problem View に `xr.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2 を正式 milestone に起こした。VR / XR foundation。