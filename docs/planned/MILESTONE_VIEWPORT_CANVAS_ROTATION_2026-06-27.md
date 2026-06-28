# Milestone: Viewport Canvas Rotation System (M-VP-4)

**マイルストーンID**: M-VP-4
**作成日**: 2026-06-27
**優先度**: Medium
**推定工数**: 3-4日
**カテゴリ**: Composition Editor / Viewport
**状態**: Planned

---

## 目的

After Effects風のキャンバス回転機能を実装する。ユーザーはビューポート上でキャンバスを任意の角度で回転させ、直感的なコンポジション操作ができるようになる。

---

## 背景

### 現状
- 現在のビューポートはパン・ズーム・フィット機能は完備
- 回転操作（`Viewport rotation (canvas rotate)`）は **0 hit** — 完全に未実装
- `ViewportTransformer` クラスに回転状態を管理する仕組みがない
- 3Dレイヤー用のカメラ回転は別途実装されているが、2Dキャンバスの回転は未サポート

### 要件
- キャンバスを任意角度（-180°〜+180°）で回転
- 回転中心点はビューポート中央
- マウスジェスチャー（Shift+ドラッグ）で回転
-キーボードショートカット（Rキー等）でリセット
- 回転状態はプロジェクトに保存
- 既存のパン・ズームと調和した操作性

### ユースケース
1. 斜めに撮られた映像素材を水平に調整しながら作業
2. 回転アニメーションのプレビュー
3. 特殊効果（ティルトシフト等）の作業
4. モーショングラフィックスの角度調整

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **変更** | `ArtifactCore/include/Transform/ViewportTransformer.ixx` | 回転フィールドとメソッド追加 |
| **変更** | `ArtifactCore/src/Transform/ViewportTransformer.cppm` | 回転変換ロジック実装 |
| **変更** | `Artifact/include/Render/ArtifactIRenderer.ixx` | 回転操作API追加 |
| **変更** | `Artifact/src/Render/ArtifactIRenderer.cppm` | 回転状態管理 |
| **変更** | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 回転操作ハンドリング |
| **変更** | `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | UI統合・ショートカット |
| **新規** | `ArtifactCore/include/Event/ViewportRotationChangedEvent.ixx` | 回転状態変更イベント |

---

## 変更詳細

### 1. ViewportTransformer への回転サポート追加

**ファイル**: `ArtifactCore/include/Transform/ViewportTransformer.ixx`

```cpp
export class ViewportTransformer {
public:
    // 既存メソッドに追加
    void SetRotation(float degrees);
    float GetRotation() const;
    void ResetRotation();
    
    // 回転を含む座標変換
    float2 CanvasToViewport(float2 canvasPos) const;
    float2 ViewportToCanvas(float2 viewportPos) const;
    
    // 定数バッファ拡張
    struct ViewportCB {
        float2 offset;      // パン位置
        float2 scale;       // スケール
        float2 screenSize;  // ビューポート解像度
        float  zoom;        // ズーム倍率
        float  rotation;    // 回転角度（ラジアン）⭐ 新規
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
    float rotation = 0.0f;  // ⭐ 新規: 回転角度（度）

    float2 CanvasToViewport(float2 canvasPos) const {
        // 1. 回転変換
        float rad = rotation * M_PI / 180.0f;
        float cosR = std::cos(rad);
        float sinR = std::sin(rad);
        
        // 回転中心はキャンバス中心
        float2 center = {canvasSize.x * 0.5f, canvasSize.y * 0.5f};
        float2 relative = {canvasPos.x - center.x, canvasPos.y - center.y};
        
        // 回転適用
        float2 rotated = {
            relative.x * cosR - relative.y * sinR,
            relative.x * sinR + relative.y * cosR
        };
        
        // 2. ズームとパン
        return {
            (rotated.x + center.x) * zoom + pan.x,
            (rotated.y + center.y) * zoom + pan.y
        };
    }

    float2 ViewportToCanvas(float2 viewportPos) const {
        // 逆変換: まずパンとズームを戻す
        float2 zoomed = {
            (viewportPos.x - pan.x) / zoom,
            (viewportPos.y - pan.y) / zoom
        };
        
        // 回転中心
        float2 center = {canvasSize.x * 0.5f, canvasSize.y * 0.5f};
        float2 relative = {zoomed.x - center.x, zoomed.y - center.y};
        
        // 逆回転
        float rad = -rotation * M_PI / 180.0f;
        float cosR = std::cos(rad);
        float sinR = std::sin(rad);
        
        float2 unrotated = {
            relative.x * cosR - relative.y * sinR,
            relative.x * sinR + relative.y * cosR
        };
        
        return {unrotated.x + center.x, unrotated.y + center.y};
    }

    ViewportCB GetViewportCB() const {
        return {
            pan,
            {1.0f, 1.0f},
            viewportSize,
            zoom,
            rotation * M_PI / 180.0f,  // ラジアンに変換
            0.0f
        };
    }
};

// 新規メソッド実装
void ViewportTransformer::SetRotation(float degrees) {
    impl_->rotation = std::fmod(degrees, 360.0f);
    if (impl_->rotation < 0) impl_->rotation += 360.0f;
}

float ViewportTransformer::GetRotation() const {
    return impl_->rotation;
}

void ViewportTransformer::ResetRotation() {
    impl_->rotation = 0.0f;
}
```

### 2. ArtifactIRenderer へのAPI追加

**ファイル**: `Artifact/include/Render/ArtifactIRenderer.ixx`

```cpp
export class ArtifactIRenderer {
public:
    // 回転操作
    void setRotation(float degrees);
    float getRotation() const;
    void resetRotation();
    void rotateBy(float deltaDegrees);
};
```

**ファイル**: `Artifact/src/Render/ArtifactIRenderer.cppm`

```cpp
// Impl クラスに追加
void setRotation(float degrees) {
    primitiveRenderer_.setRotation(degrees);
    m_rotation = degrees;
}

float getRotation() const { return m_rotation; }
void resetRotation() { setRotation(0.0f); }
void rotateBy(float deltaDegrees) { setRotation(m_rotation + deltaDegrees); }

// メンバー変数追加
float m_rotation = 0.0f;  // 回転角度（度）
```

### 3. CompositionRenderController への統合

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

```cpp
// 回転操作をレンダラーに転送
void CompositionRenderController::setRotation(float degrees) {
    impl_->renderer_->setRotation(degrees);
    impl_->rotation_ = degrees;
    updateViewportTransform();
}

float CompositionRenderController::getRotation() const {
    return impl_->rotation_;
}

void CompositionRenderController::rotateBy(float deltaDegrees) {
    setRotation(impl_->rotation_ + deltaDegrees);
}

void CompositionRenderController::resetRotation() {
    setRotation(0.0f);
}

// 状態保存/復元に回転を追加
void CompositionRenderController::saveViewportState(ViewportState& state) const {
    // ... 既存コード
    state.rotation = impl_->rotation_;
}

void CompositionRenderController::restoreViewportState(const ViewportState& state) {
    // ... 既存コード
    setRotation(state.rotation);
}
```

### 4. CompositionEditor へのUI統合

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

```cpp
// マウスイベントハンドラーに回転操作追加
void ArtifactCompositionEditor::mouseMoveEvent(QMouseEvent* event) {
    if (impl_->rotationMode_) {
        QPointF delta = event->position() - impl_->lastMousePos_;
        float rotationDelta = delta.x() * 0.5f;  // 感度調整
        impl_->renderController_->rotateBy(rotationDelta);
        impl_->lastMousePos_ = event->position();
        update();
        return;
    }
    // ... 既存パン/ズーム処理
}

// キーイベントハンドラー
void ArtifactCompositionEditor::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_R:
            impl_->renderController_->resetRotation();
            update();
            break;
        case Qt::Key_Shift:
            // Shiftキーで回転モード開始
            impl_->rotationMode_ = true;
            setCursor(Qt::SizeAllCursor);
            break;
        // ... 既存処理
    }
}

void ArtifactCompositionEditor::keyReleaseEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Shift:
            impl_->rotationMode_ = false;
            unsetCursor();
            break;
        // ... 既存処理
    }
}

// 対応するメンバー変数
// In Impl struct:
bool rotationMode_ = false;
QPointF lastMousePos_;
```

### 5. 回転状態変更イベント

**新規ファイル**: `ArtifactCore/include/Event/ViewportRotationChangedEvent.ixx`

```cpp
module;
#include <cstdint>

export module Event.ViewportRotationChangedEvent;

import Core.EventBus.Event;

export struct ViewportRotationChangedEvent : Event {
    float rotationDegrees;  // 回転角度（度）
    float previousRotationDegrees;  // 前回の回転角度
};
```

** CompositionRenderController.cppm にイベント発行 **:
```cpp
void CompositionRenderController::setRotation(float degrees) {
    float previous = impl_->rotation_;
    impl_->renderer_->setRotation(degrees);
    impl_->rotation_ = degrees;
    updateViewportTransform();
    
    // イベント発行
    ViewportRotationChangedEvent evt{
        .rotationDegrees = degrees,
        .previousRotationDegrees = previous
    };
    EventBus::publish(evt);
}
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
| `ViewportTransformer::Impl` に `rotation` フィールド追加 | P0 | 0.5h | なし | ✅ |
| `CanvasToViewport` 回転変換ロジック実装 | P0 | 2h | 上記 | ✅ |
| `ViewportToCanvas` 逆回転変換ロジック実装 | P0 | 2h | 上記 | ✅ |
| `SetRotation`/`GetRotation`/`ResetRotation` メソッド実装 | P0 | 1h | 上記 | ✅ |
| `ViewportCB` に `rotation` フィールド追加 | P0 | 0.5h | 上記 | ✅ |

### Phase 2: Renderer 統合 (1日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `ArtifactIRenderer::Impl` に `m_rotation` フィールド追加 | P0 | 0.5h | Phase 1 | ✅ |
| `ArtifactIRenderer` に回転API (`setRotation`, `getRotation`, `resetRotation`, `rotateBy`) 追加 | P0 | 2h | Phase 1 | ✅ |
| `PrimitiveRenderer2D` への回転状態伝達 | P0 | 1h | 上記 | ✅ |
| 既存の `setViewportSize`/`setPan`/`setZoom` との整合性確認 | P0 | 0.5h | 上記 | ✅ |

### Phase 3: Controller 統合 (1日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `CompositionRenderController::Impl` に `rotation_` フィールド追加 | P1 | 0.5h | Phase 2 | ✅ |
| 回転操作メソッド (`setRotation`, `getRotation`, `rotateBy`, `resetRotation`) 実装 | P1 | 2h | Phase 2 | ✅ |
| 状態保存/復元に回転情報追加 | P1 | 1h | 上記 | ✅ |
| `ViewportRotationChangedEvent` 構造体定義 | P1 | 0.5h | Phase 2 | ✅ |
| イベント発行ロジック実装 | P1 | 0.5h | 上記 | ✅ |
| `EventBus` へのイベント登録 | P1 | 0.5h | 上記 | ✅ |

### Phase 4: UI 統合 (1日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `CompositionEditor::Impl` に `rotationMode_` フィールド追加 | P1 | 0.5h | Phase 3 | ✅ |
| マウスイベントハンドラーに回転操作追加 | P1 | 2h | 上記 | ❌ (UIスレッド) |
| 回転モードの開始/終了（Shiftキー） | P1 | 1h | 上記 | ❌ (UIスレッド) |
| 回転リセットショートカット（Rキー） | P1 | 0.5h | 上記 | ❌ (UIスレッド) |
| カーソル変更（回転モード時） | P2 | 0.5h | 上記 | ❌ (UIスレッド) |
| 回転中の視覚的フィードバック | P3 | 1h | 上記 | ❌ (UIスレッド) |

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
| 0°回転時の座標変換 | 自動 | `CanvasToViewport(p) == p * zoom + pan` | 1h |
| 90°回転時の座標変換 | 自動 | (x,y) → (-y,x) 回転 + zoom + pan | 1h |
| 180°回転時の座標変換 | 自動 | (x,y) → (-x,-y) 回転 + zoom + pan | 1h |
| 270°回転時の座標変換 | 自動 | (x,y) → (y,-x) 回転 + zoom + pan | 1h |
| 360°回転 = 0°回転 | 自動 | 360°と0°の結果が同一 | 0.5h |
| 負の角度 (-90°) | 自動 | -90° = 270°の結果と同一 | 0.5h |
| 逆変換の一意性 | 自動 | `ViewportToCanvas(CanvasToViewport(p)) == p` | 1h |
| `GetRotation`/`SetRotation` | 自動 | 設定値 = 取得値 | 0.5h |
| `ResetRotation` | 自動 | 0°に戻る | 0.5h |
| `rotateBy` の累積 | 自動 | `rotateBy(90) + rotateBy(90) = 180°` | 0.5h |

#### P1 - 主要テスト (可能な限りパス)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| 回転 + パンの組み合わせ | 自動 | 座標変換が正しい | 1h |
| 回転 + ズームの組み合わせ | 自動 | 座標変換が正しい | 1h |
| 回転中心がキャンバス中央 | 自動 | 回転前後で中心点が同一 | 1h |
| 回転状態の保存/復元 | 自動 | 保存前と復元後の状態が同一 | 1h |

#### P2 - 統合テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| 全ビューポート操作の組み合わせ | 手動 | 視覚的確認 | 2h |
| ギズモ表示の確認 | 手動 | 回転時もギズモが正しく表示 | 1h |
| レイヤー選択の確認 | 手動 | 回転時もマウスピッキングが正しい | 1h |
| 3Dレイヤーとの互換性 | 手動 | 回転が3D表示に影響しない | 1h |

### 2. 手動テスト (Manual Tests)

#### UXテスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| Shift+ドラッグの感度 | 直感的で自然な回転 | 1h |
| Rキーのレスポンス | 即座にリセット | 0.5h |
| 回転モードの開始/終了 | 明確で分かりやすい | 0.5h |
| カーソル変更 | 回転モード中は回転カーソル | 0.5h |

#### 視覚的テスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| 回転時の画質 | 回転前と同じ画質 | 1h |
| 回転時のパフォーマンス | 60fps維持 | 1h |
| 回転+ズーム+パンの組み合わせ | 直感的で予測可能 | 2h |

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
日数: 0.5日
対象: ArtifactIRenderer クラス
方法: モックを使用した単体テスト
実行: CIパイプラインで自動実行
```

#### Phase 3: System Tests (Controller)
```
日数: 1日
対象: CompositionRenderController
方法: 手動 + 自動テスト
実行: 手動テストはQAチーム、自動テストはCI
```

#### Phase 4: UX Tests (UI)
```
日数: 1日
対象: CompositionEditor
方法: 手動テスト
実行: UXデザイナー + QAチーム
```

### 4. テスト完了基準
- [ ] P0全てのテストがパス
- [ ] P1の80%以上のテストがパス
- [ ] 重大なバグなし
- [ ] パフォーマンス目標達成
- [ ] UXテストで問題なし

---

## 成果物

1. **コア機能**: キャンバス回転の完全サポート
2. **API**: `setRotation()`/`getRotation()`/`rotateBy()`/`resetRotation()`
3. **UI操作**: Shift+ドラッグで回転、Rキーでリセット
4. **イベントシステム**: 回転状態変更時の通知
5. **状態管理**: 回転状態の保存/復元

---

## 依存関係

### 必要な前提条件
- なし（独立して実装可能）

### 連携する機能
- `ViewportTransformer` - 中核的な変換ロジック
- `ArtifactIRenderer` - レンダラインターフェース
- `CompositionRenderController` - ビューポート制御
- `CompositionEditor` - UIウィジェット

### 影響を受ける機能
- 毎フレームの座標変換（全レイヤー描画）
- ギズモの表示位置
-ママウスピッキング
- セレクションボックス

---

## リスクと対策

### リスク1: パフォーマンス影響
**内容**: 回転変換の追加で座標変換コストが上昇
**対策**:
- `std::sin`/`std::cos` の呼び出しを最小化（回転角度が変更された時のみ再計算）
- 回転行列をキャッシュ
- シンプルな回転の場合は近似計算を検討

### リスク2: 既存機能との干渉
**内容**: 回転状態がパン/ズームと干渉する可能性
**対策**:
- 回転は常にキャンバス中心を基点とする
- 座標変換の順序を明確化（回転 → ズーム → パン）
- テストで各操作の組み合わせを検証

### リスク3: シェーダーの互換性
**内容**: 回転情報をGPUに渡す必要
**対策**:
- `ViewportCB` に回転情報を追加
- 既存のシェーダーを更新（回転行列を適用）
- 複雑なシェーダーは段階的に更新

### リスク4: 状態保存の互換性
**内容**: 回転状態を保存したプロジェクトは古いバージョンで開けない
**対策**:
- バージョン管理を導入
- 古いバージョンでは回転状態を無視（0にリセット）
- Maiじゃあれる形式で保存

---

## テスト項目

- [ ] 回転操作（Shift+ドラッグ）の動作確認
- [ ] 回転リセット（Rキー）の動作確認
- [ ] 回転 + パンの組み合わせ
- [ ] 回転 + ズームの組み合わせ
- [ ] 回転 + フィットの組み合わせ
- [ ] 状態保存/復元
- [ ] 360°回転（0°と360°が同等）
- [ ] 負の角度
- [ ] マウス感度の調整
- [ ] ギズモ表示の確認
- [ ] レイヤー選択の確認

---

## 完了基準

- [ ] 全ての回転操作が直感的に動作する
- [ ] 既存のパン/ズーム機能との競合がない
- [ ] 状態保存/復元が正常に動作
- [ ] パフォーマンスが既存より5%以内の低下
- [ ] 全てのテスト項目がパス
- [ ] ドキュメントが更新されてる

---

## 関連文書

- `docs/analysis/REPORT_CE_RENDER_ROI_2026-06-16.md` - 現在のビューポート機能分析
- `docs/planned/IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-02.md` - マルチビューポート計画
- `ArtifactCore/src/Transform/ViewportTransformer.cppm` - 中核実装

---

## メモ

- 回転角度は度（degrees）で管理し、シェーダーに渡す際にラジアンに変換
- 回転中心は常にキャンバス中心
- 回転状態はプロジェクトファイルに保存
- 将来的には、回転中心をカスタマイズできるように拡張
