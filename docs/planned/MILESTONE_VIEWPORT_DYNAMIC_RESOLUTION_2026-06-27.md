> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_VIEWPORT_BOOKMARKS_2026-06-27.md](MILESTONE_VIEWPORT_BOOKMARKS_2026-06-27.md)

# Milestone: Viewport Dynamic Resolution Switching (M-VP-5)

**マイルストーンID**: M-VP-5
**作成日**: 2026-06-27
**優先度**: Medium
**推定工数**: 2-3日
**カテゴリ**: Composition Editor / Viewport / Performance
**最終更新**: 2026-08-15
**状態**: 部分実装（固定品質プリセットと操作中 downsample は実装済み。負荷連動の自動スケールは未完了）

---

## 目的

ビューポートの表示解像度をリアルタイムで切り替える機能を実装する。ユーザーはパフォーマンスと品質のトレードオフをコントロールでき、高解像度ディスプレイやリモートデスクトップ環境でも快適に作業できる。

---

## 背景

### 現状
- Composition Editor に Full／Half／Quarter の preview quality 切替があり、`PreviewQualityPreset` から renderer の downsample factor（1／2／4）へ反映される。
- 操作中は `interactivePreviewDownsampleFloor_` と `effectivePreviewDownsample` により一時的に低解像度化する経路がある。
- 表示負荷や目標フレームレートを計測して段階的に自動調整する制御は未確認。
- HiDPI対応が不十分で、高DPIディスプレイで100%表示時の位置ずれバグあり
- `devicePixelRatio` は部分的に考慮されているが、表示解像度スケールとは分離されていない

### 要件
- 表示解像度を段階的に切り替え（例: 25%/50%/75%/100%/150%/200%）
- 解像度切替はリアルタイム（1フレーム以内）で反映
- DPR（Device Pixel Ratio）との適切な連動
- パフォーマンスモードと品質モードの自動切替
- 状態はプロジェクトに保存

### ユースケース
1. 古いPCやリモートデスクトップで作業する際のパフォーマンス最適化
2. 4K/8Kディスプレイでの作業
3. 高解像度プレビューの確認
4. リアルタイムプレビューの品質調整
5. テストレンダリングの解像度設定

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **変更** | `ArtifactCore/include/Transform/ViewportTransformer.ixx` | 解像度スケールフィールド追加 |
| **変更** | `ArtifactCore/src/Transform/ViewportTransformer.cppm` | 解像度スケールロジック |
| **変更** | `Artifact/include/Render/ArtifactIRenderer.ixx` | 解像度操作API追加 |
| **変更** | `Artifact/src/Render/ArtifactIRenderer.cppm` | 解像度状態管理 |
| **変更** | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 解像度制御 |
| **変更** | `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | UI統合 |
| **変更** | `Artifact/src/Widgets/ArtifactViewMenu.cppm` | 解像度切替メニュー |
| **新規** | `ArtifactCore/include/Event/ViewportResolutionChangedEvent.ixx` | 解像度変更イベント |

---

## 変更詳細

### 1. ViewportTransformer への解像度スケール追加

**ファイル**: `ArtifactCore/include/Transform/ViewportTransformer.ixx`

```cpp
export class ViewportTransformer {
public:
    // 解像度スケール操作
    void SetDisplayScale(float scale);
    float GetDisplayScale() const;
    
    // 定数バッファ拡張
    struct ViewportCB {
        float2 offset;      // パン位置
        float2 scale;       // DPR × 解像度スケール
        float2 screenSize;  // ビューポート解像度（論理）
        float  zoom;        // ズーム倍率
        float  rotation;    // 回転角度（ラジアン）
        float  padding;
    };
};
```

**ファイル**: `ArtifactCore/src/Transform/ViewportTransformer.cppm`

```cpp
class ViewportTransformer::Impl {
public:
    float2 viewportSize = {1920, 1080};
    float2 canvasSize = {1920, 1080};
    float2 pan = {0, 0};
    float zoom = 1.0f;
    float rotation = 0.0f;
    float displayScale = 1.0f;  // ⭐ 新規: 表示解像度スケール（1.0 = 100%）
    float devicePixelRatio = 1.0f;  // ⭐ 新規: DPR

    float2 CanvasToViewport(float2 canvasPos) const {
        // 1. 回転変換（既存）
        // ... 回転ロジック
        
        // 2. ズームとパン（解像度スケール考慮）
        float effectiveZoom = zoom * displayScale;
        return {
            (rotated.x + center.x) * effectiveZoom + pan.x,
            (rotated.y + center.y) * effectiveZoom + pan.y
        };
    }

    ViewportCB GetViewportCB() const {
        // DPR × 解像度スケールを考慮
        float effectiveScale = devicePixelRatio * displayScale;
        return {
            pan,
            {effectiveScale, effectiveScale},
            viewportSize,
            zoom,
            rotation * M_PI / 180.0f,
            0.0f
        };
    }
};

// 新規メソッド実装
void ViewportTransformer::SetDisplayScale(float scale) {
    impl_->displayScale = std::clamp(scale, 0.1f, 4.0f);  // 10%-400%に制限
}

float ViewportTransformer::GetDisplayScale() const {
    return impl_->displayScale;
}

void ViewportTransformer::SetDevicePixelRatio(float dpr) {
    impl_->devicePixelRatio = std::max(1.0f, dpr);
}

float ViewportTransformer::GetDevicePixelRatio() const {
    return impl_->devicePixelRatio;
}
```

### 2. ArtifactIRenderer へのAPI追加

**ファイル**: `Artifact/include/Render/ArtifactIRenderer.ixx`

```cpp
export class ArtifactIRenderer {
public:
    // 解像度操作
    enum class ResolutionPreset {
        Auto,       // 自動（DPR × 100%）
        Quarter,    // 25%
        Half,       // 50%
        ThreeQuarter, // 75%
        Full,       // 100%
        OneFifty,    // 150%
        Double,     // 200%
        Custom      // カスタム
    };
    
    void setResolutionPreset(ResolutionPreset preset);
    void setResolutionScale(float scale);  // 1.0 = 100%
    float resolutionScale() const;
    ResolutionPreset currentResolutionPreset() const;
    void setDevicePixelRatio(float dpr);
    float devicePixelRatio() const;
};
```

**ファイル**: `Artifact/src/Render/ArtifactIRenderer.cppm`

```cpp
// 定数定義
const std::map<ArtifactIRenderer::ResolutionPreset, float> kResolutionPresetScales = {
    {ResolutionPreset::Quarter,     0.25f},
    {ResolutionPreset::Half,       0.5f},
    {ResolutionPreset::ThreeQuarter, 0.75f},
    {ResolutionPreset::Full,       1.0f},
    {ResolutionPreset::OneFifty,   1.5f},
    {ResolutionPreset::Double,     2.0f}
};

// Impl クラスに追加
void setResolutionPreset(ResolutionPreset preset) {
    auto it = kResolutionPresetScales.find(preset);
    if (it != kResolutionPresetScales.end()) {
        setResolutionScale(it->second);
    }
}

void setResolutionScale(float scale) {
    scale = std::clamp(scale, 0.1f, 4.0f);
    m_resolutionScale = scale;
    primitiveRenderer_.setDisplayScale(scale * m_devicePixelRatio);
    recreateRenderTargets();  // レンダーターゲットを再作成
}

void setDevicePixelRatio(float dpr) {
    m_devicePixelRatio = std::max(1.0f, dpr);
    // DPR変更時は再計算
    primitiveRenderer_.setDisplayScale(m_resolutionScale * m_devicePixelRatio);
}

// メンバー変数追加
float m_resolutionScale = 1.0f;      // 解像度スケール（1.0 = 100%）
float m_devicePixelRatio = 1.0f;    // DPR
ResolutionPreset m_preset = ResolutionPreset::Full;
```

### 3. CompositionRenderController への統合

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

```cpp
// 解像度操作をレンダラーに転送
void CompositionRenderController::setResolutionScale(float scale) {
    impl_->renderer_->setResolutionScale(scale);
    impl_->resolutionScale_ = scale;
    
    // 解像度変更イベント発行
    ViewportResolutionChangedEvent evt{
        .scale = scale,
        .previousScale = impl_->previousResolutionScale_,
        .devicePixelRatio = impl_->renderer_->devicePixelRatio()
    };
    EventBus::publish(evt);
    impl_->previousResolutionScale_ = scale;
}

void CompositionRenderController::setResolutionPreset(
    ArtifactIRenderer::ResolutionPreset preset) {
    impl_->renderer_->setResolutionPreset(preset);
    impl_->preset_ = preset;
}

// 状態保存/復元に解像度を追加
void CompositionRenderController::saveViewportState(ViewportState& state) const {
    // ... 既存コード
    state.resolutionScale = impl_->resolutionScale_;
    state.resolutionPreset = static_cast<int>(impl_->preset_);
}

void CompositionRenderController::restoreViewportState(const ViewportState& state) {
    // ... 既存コード
    setResolutionScale(state.resolutionScale);
    impl_->preset_ = static_cast<ArtifactIRenderer::ResolutionPreset>(state.resolutionPreset);
}
```

### 4. CompositionEditor へのUI統合

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

```cpp
//ホイールイベントに解像度変更操作追加
void ArtifactCompositionEditor::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() == Qt::ControlModifier) {
        // Ctrl+ホイール: 解像度変更
        float delta = event->angleDelta().y() / 120.0f;
        float currentScale = impl_->renderController_->resolutionScale();
        
        // 10%単位で変更
        if (delta > 0) {
            currentScale = std::min(4.0f, currentScale + 0.1f);
        } else {
            currentScale = std::max(0.1f, currentScale - 0.1f);
        }
        
        impl_->renderController_->setResolutionScale(currentScale);
        update();
        return;
    }
    // ... 既存ズーム処理
}
```

### 5. ViewMenu へのメニュー追加

**ファイル**: `Artifact/src/Widgets/ArtifactViewMenu.cppm`

```cpp
// 解像度切替メニュー追加
void ArtifactViewMenu::setupViewportMenu() {
    // ... 既存コード
    
    // 解像度サブメニュー
    QMenu* resolutionMenu = viewportMenu->addMenu(tr("Resolution"));
    
    QActionGroup* resolutionGroup = new QActionGroup(this);
    
    // プリセット
    QAction* autoAction = resolutionMenu->addAction(tr("Auto"));
    autoAction->setCheckable(true);
    autoAction->setActionGroup(resolutionGroup);
    connect(autoAction, &QAction::triggered, [this]() {
        emit resolutionPresetChanged(ArtifactIRenderer::ResolutionPreset::Auto);
    });
    
    QAction* quarterAction = resolutionMenu->addAction(tr("25%"));
    quarterAction->setCheckable(true);
    quarterAction->setActionGroup(resolutionGroup);
    connect(quarterAction, &QAction::triggered, [this]() {
        emit resolutionPresetChanged(ArtifactIRenderer::ResolutionPreset::Quarter);
    });
    
    QAction* halfAction = resolutionMenu->addAction(tr("50%"));
    halfAction->setCheckable(true);
    halfAction->setActionGroup(resolutionGroup);
    connect(halfAction, &QAction::triggered, [this]() {
        emit resolutionPresetChanged(ArtifactIRenderer::ResolutionPreset::Half);
    });
    
    QAction* threeQuarterAction = resolutionMenu->addAction(tr("75%"));
    threeQuarterAction->setCheckable(true);
    threeQuarterAction->setActionGroup(resolutionGroup);
    connect(threeQuarterAction, &QAction::triggered, [this]() {
        emit resolutionPresetChanged(ArtifactIRenderer::ResolutionPreset::ThreeQuarter);
    });
    
    QAction* fullAction = resolutionMenu->addAction(tr("100%"));
    fullAction->setCheckable(true);
    fullAction->setChecked(true);  // デフォルト
    fullAction->setActionGroup(resolutionGroup);
    connect(fullAction, &QAction::triggered, [this]() {
        emit resolutionPresetChanged(ArtifactIRenderer::ResolutionPreset::Full);
    });
    
    resolutionMenu->addSeparator();
    
    QAction* customAction = resolutionMenu->addAction(tr("Custom..."));
    connect(customAction, &QAction::triggered, [this]() {
        showCustomResolutionDialog();
    });
}

// 解像度メニューの状態を更新
void ArtifactViewMenu::updateResolutionMenu(ArtifactIRenderer::ResolutionPreset preset) {
    // メニューのチェック状態を更新
    // ... 実装
}

void ArtifactViewMenu::showCustomResolutionDialog() {
    bool ok;
    float scale = QInputDialog::getDouble(
        this, tr("Custom Resolution"), tr("Scale (%):"),
        impl_->renderController->resolutionScale() * 100.0f,
        10.0, 400.0, 10.0, &ok
    );
    
    if (ok) {
        emit resolutionScaleChanged(scale / 100.0f);
    }
}
```

### 6. 解像度変更イベント

**新規ファイル**: `ArtifactCore/include/Event/ViewportResolutionChangedEvent.ixx`

```cpp
module;
#include <cstdint>

export module Event.ViewportResolutionChangedEvent;

import Core.EventBus.Event;

export struct ViewportResolutionChangedEvent : Event {
    float scale;              // 解像度スケール（1.0 = 100%）
    float previousScale;     // 前回の解像度スケール
    float devicePixelRatio;  // DPR
};
```

---

## タスク分割 (優先度付き)

### 优先度レベル
- **P0 (Critical)**: コア機能、なければ動作しない
- **P1 (High)**: 主要機能、ないと使い勝手が悪い
- **P2 (Medium)**: 便利機能、あってもなくても動作する
- **P3 (Low)**: 見栄え/UX向上、なくても機能する

### Phase 1: Core 実装 (1日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `ViewportTransformer::Impl` に `displayScale` フィールド追加 | P0 | 0.5h | なし | ✅ |
| `ViewportTransformer::Impl` に `devicePixelRatio` フィールド追加 | P0 | 0.5h | なし | ✅ |
| `SetDisplayScale`/`GetDisplayScale` メソッド実装 | P0 | 1h | 上記 | ✅ |
| `SetDevicePixelRatio`/`GetDevicePixelRatio` メソッド実装 | P0 | 1h | 上記 | ✅ |
| `ViewportCB` に `scale` (DPR × displayScale) 統合 | P0 | 1h | 上記 | ✅ |
| `CanvasToViewport` に解像度スケール考慮 | P0 | 1h | 上記 | ✅ |

### Phase 2: Renderer 統合 (1日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `ResolutionPreset` enum 定義 | P0 | 0.5h | Phase 1 | ✅ |
| `ArtifactIRenderer::Impl` に `m_resolutionScale` フィールド追加 | P0 | 0.5h | Phase 1 | ✅ |
| `ArtifactIRenderer::Impl` に `m_devicePixelRatio` フィールド追加 | P0 | 0.5h | Phase 1 | ✅ |
| 解像度操作API (`setResolutionPreset`, `setResolutionScale`, `resolutionScale`, `devicePixelRatio`) 追加 | P0 | 2h | 上記 | ✅ |
| `recreateRenderTargets()` に解像度考慮 | P0 | 2h | 上記 | ✅ |
| DPR変更時の自動再計算 | P0 | 1h | 上記 | ✅ |

### Phase 3: Controller 統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `CompositionRenderController::Impl` に `resolutionScale_` フィールド追加 | P1 | 0.5h | Phase 2 | ✅ |
| `CompositionRenderController::Impl` に `preset_` フィールド追加 | P1 | 0.5h | Phase 2 | ✅ |
| 解像度操作メソッド (`setResolutionScale`, `setResolutionPreset`) 実装 | P1 | 1h | Phase 2 | ✅ |
| 状態保存/復元に解像度情報追加 | P1 | 1h | 上記 | ✅ |
| `ViewportResolutionChangedEvent` 構造体定義 | P1 | 0.5h | Phase 2 | ✅ |
| イベント発行ロジック実装 | P1 | 0.5h | 上記 | ✅ |

### Phase 4: UI 統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `ViewMenu` への解像度切替メニュー追加 | P1 | 2h | Phase 3 | ❌ (UIスレッド) |
| 解像度プリセットアクション（Auto/25%/50%/75%/100%/150%/200%） | P1 | 1h | 上記 | ❌ (UIスレッド) |
| カスタム解像度ダイアログ | P1 | 1h | 上記 | ❌ (UIスレッド) |
| `CompositionEditor` へのCtrl+ホイール解像度操作 | P1 | 1h | 上記 | ❌ (UIスレッド) |
| メニュー状態の同期 | P2 | 0.5h | 上記 | ❌ (UIスレッド) |
| 解像度変更時の視覚的フィードバック | P3 | 0.5h | 上記 | ❌ (UIスレッド) |

### 並行作業可能性
- **Phase 1 (Core)**: 独立して並行可能
- **Phase 2 (Renderer)**: Phase 1完了後、独立して並行可能
- **Phase 3 (Controller)**: Phase 2完了後、独立して並行可能
- **Phase 4 (UI)**: UIスレッド依存のため、基本的には直列実施

---

## テスト戦略

### 1. 自動テスト (Unit Tests)

#### P0 - 必須テスト (全てパスすること)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| 100%解像度時の座標変換 | 自動 | 通常の座標変換と同一 | 1h |
| 50%解像度時の座標変換 | 自動 | 全ての座標が半分スケール | 1h |
| 200%解像度時の座標変換 | 自動 | 全ての座標が2倍スケール | 1h |
| DPR=2.0時のスケール | 自動 | `scale = 2.0 * displayScale` | 1h |
| `SetDisplayScale`/`GetDisplayScale` | 自動 | 設定値 = 取得値 | 0.5h |
| `SetDevicePixelRatio`/`GetDevicePixelRatio` | 自動 | 設定値 = 取得値 | 0.5h |
| 解像度スケールのクランプ | 自動 | 0.1-4.0の範囲内 | 0.5h |
| DPRの最小値 | 自動 | 1.0以上 | 0.5h |
| 解像度プリセットの変換 | 自動 | `Quarter=0.25`, `Half=0.5`, `Full=1.0`, `Double=2.0` | 1h |

#### P1 - 主要テスト (可能な限りパス)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| 解像度変更 + パンの組み合わせ | 自動 | 座標変換が正しい | 1h |
| 解像度変更 + ズームの組み合わせ | 自動 | 座標変換が正しい | 1h |
| 解像度変更 + 回転の組み合わせ | 自動 | 座標変換が正しい | 1h |
| 状態保存/復元 | 自動 | 保存前と復元後の状態が同一 | 1h |
| DPR変更時の自動再計算 | 自動 | 解像度スケールが正しく再計算 | 1h |
| `recreateRenderTargets()` 呼び出し確認 | 自動 | 解像度変更時に呼び出される | 0.5h |

#### P2 - 統合テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| 全ビューポート操作の組み合わせ | 手動 | 視覚的確認 | 2h |
| 低解像度時の画質 | 手動 | アップスケールが自然 | 1h |
| 高解像度時の画質 | 手動 | 通常と同じ品質 | 1h |
| 解像度変更時のレスポンス | 手動 | 1フレーム以内で反映 | 1h |

### 2. 手動テスト (Manual Tests)

#### UXテスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| Ctrl+ホイールの感度 | 直感的で自然な解像度変更 | 1h |
| メニューからの解像度切替 | 即座に反映 | 0.5h |
| カスタム解像度ダイアログ | 入力しやすい | 0.5h |
| 解像度切替時のフィードバック | 明確で分かりやすい | 0.5h |

#### 視覚的テスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| 各解像度プリセットの画質 | 期待通りの品質 | 2h |
| 解像度変更時の滑らかさ | 切り替えがなめらか | 1h |
| DPR変更時の自動調整 | 画質が維持される | 1h |
| 低解像度時のパフォーマンス | 60fps維持 | 1h |
| 高解像度時のパフォーマンス | 60fps維持 | 1h |

### 3. テスト実行計画

#### Phase 1: Unit Tests (Core)
```
日数: 1日
対象: ViewportTransformer クラス
方法: Google Test / Catch2
実行: CIパイプラインで自動実行
```

#### Phase 2: Integration Tests (Renderer)
```
日数: 1日
対象: ArtifactIRenderer クラス
方法: モックを使用した単体テスト + 手動テスト
実行: CIパイプライン (自動) + QAチーム (手動)
```

#### Phase 3: System Tests (Controller)
```
日数: 0.5日
対象: CompositionRenderController
方法: 手動 + 自動テスト
実行: 手動テストはQAチーム、自動テストはCI
```

#### Phase 4: UX Tests (UI)
```
日数: 0.5日
対象: CompositionEditor + ViewMenu
方法: 手動テスト
実行: UXデザイナー + QAチーム
```

### 4. テスト完了基準
- [ ] P0全てのテストがパス
- [ ] P1の80%以上のテストがパス
- [ ] 重大なバグなし
- [ ] パフォーマンス目標達成 (60fps維持)
- [ ] UXテストで問題なし
- [ ] 全ての解像度プリセットが正常に動作
- [ ] DPRとの連動が正常

---

## 成果物

1. **コア機能**: 解像度の動的切り替え
2. **API**: `setResolutionScale()`/`setResolutionPreset()`/`resolutionScale()`
3. **UI操作**: Ctrl+ホイールで解像度変更、メニューからのプリセット選択
4. **イベントシステム**: 解像度変更時の通知
5. **状態管理**: 解像度状態の保存/復元

---

## 依存関係

### 必要な前提条件
- **HiDPI対応 (M-VP-5に依存)** - DPR管理が必要
- **GPUビューポート制御 (推奨)** - レンダーターゲットの再作成に関連

### 連携する機能
- `ViewportTransformer` - 中核的な変換ロジック
- `ArtifactIRenderer` - レンダラインターフェース
- `CompositionRenderController` - ビューポート制御
- `CompositionEditor` - UIウィジェット
- `ViewMenu` - メニューUI

### 影響を受ける機能
- レンダーターゲットのサイズ
- テクスチャアトラスのサイズ
- リアクションの品質

---

## リスクと対策

### リスク1: メモリ使用量の増加
**内容**: 高解像度モード時のメモリ消費
**対策**:
- 解像度が高い場合は、必要に応じてテクスチャをダウンスケール
- LODシステムと連動
- メモリ不足時は自動で解像度を下げる

### リスク2: パフォーマンス低下
**内容**: 高解像度時のレンダリング負荷
**対策**:
- 解像度が低い場合は、アップスケールレンダリングを使用
- 非アクティブビューポートは低解像度でレンダリング
- GPUメモリの効率的な管理

### リスク3: DPRとの競合
**内容**: DPR変更時の解像度スケールの再計算
**対策**:
- DPR変更を検知し、自動で再計算
- DPR × 解像度スケールを一体で管理
- 状態変更イベントを適切に発行

### リスク4: 状態保存の互換性
**内容**: 解像度状態を保存したプロジェクトは古いバージョンで開けない
**対策**:
- バージョン管理を導入
- 古いバージョンでは解像度を100%にリセット
- 互換性のある保存形式

---

## テスト項目

- [ ] 解像度プリセット切替（25%/50%/75%/100%/150%/200%）
- [ ] Ctrl+ホイールによる解像度変更
- [ ] カスタム解像度ダイアログ
- [ ] DPR変更時の自動再計算
- [ ] 状態保存/復元
- [ ] 解像度変更時のイベント発行
- [ ] 低解像度時の画質確認
- [ ] 高解像度時のメモリ使用量
- [ ] 解像度変更時のレスポンス
- [ ] 既存のパン/ズーム/回転との組み合わせ

---

## 完了基準

- [ ] 全ての解像度プリセットが正常に動作
- [ ] Ctrl+ホイールで直感的に解像度変更
- [ ] DPRとの適切な連動
- [ ] 状態保存/復元が正常に動作
- [ ] パフォーマンスが既存より10%以内の低下
- [ ] 全てのテスト項目がパス
- [ ] ドキュメントが更新

---

## 関連文書

- `docs/analysis/REPORT_CE_RENDER_ROI_2026-06-16.md` - 現在のビューポート機能分析
- `docs/bugs/BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md` - HiDPI関連バグ
- `docs/experiments/RENDER_BUG_INVESTIGATION_2026-04-11.md` - ビューポート初期化バグ

---

## メモ

- 解像度スケールは1.0を基準（100%）とし、0.1-4.0の範囲で動作
- DPR × 解像度スケールを`ViewportCB::scale`に設定
- 解像度変更時はレンダーターゲットを再作成
- 将来的には、解像度をX/Y別に設定できるように拡張
- 解像度プリセットはユーザー設定でカスタマイズ可能

---

## 2026-07-25 現状確認

静的確認では、レンダーコンテキストやプレビュー設定に `resolutionScale` が存在し、出力サイズからスケールを計算する経路、GI の内部解像度スケール、DPR を使ったレンダーターゲット寸法計算もある。ただしこれらは export／内部品質設定／アップスケールの責務であり、本マイルストーンが要求する viewport 用の解像度プリセット API と UI とは別である。

現行コードでは `PreviewQualityPreset` の Full／Half／Quarter、controller の resolution scale、操作中 downsample、resize 時の DPR 更新、品質変更イベント経路とレンダーターゲット再作成を確認できる。一方、25〜200% の細粒度プリセット、Custom、Ctrl+wheel、負荷連動 Auto Scale、viewport状態の保存、runtime受入は未完了または未確認である。したがって本マイルストーンは「固定品質プリセットと操作中 downsample の部分実装」と判定する。

確認範囲: `ArtifactCore/src/Transform/ViewportTransformer.cppm`、`ArtifactCore/src/Preview/PreviewSettings.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。ビルド・実機操作による動作確認は未実施。
