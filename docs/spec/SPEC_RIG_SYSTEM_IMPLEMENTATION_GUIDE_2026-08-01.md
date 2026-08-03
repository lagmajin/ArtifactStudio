# リグシステム UI 実装手順書

**日付**: 2026-08-01
**前提**: `Rig2D.ixx` / `Rig2D.cppm` / `CompositionRenderController.cppm` に SmartBone, SkinMesh, Pose, 描画関数 実装済み

---

## 凡例

各ステップは `[ファイル] → [操作]` の形式。ステップ番号が若い順に実行する。

---

## Phase 0: 準備（既存コードへの最小追加）

### Step 0.1 — ToolType に RigSelect, RigWeight 追加
**ファイル**: `Artifact/include/Tool/ArtifactToolManager.ixx`

```cpp
// enum class ToolType { ... の末尾に追加:
    RigSelect,
    RigWeight
```

**ファイル**: `Artifact/src/Tool/ArtifactToolManager.cppm`

`toolName()` の switch に追加:
```cpp
case ToolType::RigSelect: return QStringLiteral("Rig Select");
case ToolType::RigWeight: return QStringLiteral("Weight Paint");
```

---

### Step 0.2 — Impl にリグ関連メンバー追加
**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`class CompositionRenderController::Impl` 内のメンバー変数エリアに追加:
```cpp
// ── Rig editing ──
bool showRigOverlay_ = false;
bool rigEditActive_ = false;
ArtifactCore::Rig2D* activeRig_ = nullptr;
LayerID riggedLayerId_;
Id selectedBoneId_;
Id draggingBoneId_;
Id draggingControlId_;
QPointF dragStartViewportPos_;
float dragStartBoneRotation_ = 0.0f;
// Weight paint
bool weightPaintActive_ = false;
int weightPaintBoneIndex_ = 0;
float weightPaintRadius_ = 24.0f;
float weightPaintOpacity_ = 0.5f;
```

---

### Step 0.3 — 公開 API 追加
**ファイル**: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`

```cpp
void setShowRigOverlay(bool show);
bool showRigOverlay() const;
void setActiveRig(ArtifactCore::Rig2D* rig, const LayerID& layerId);
ArtifactCore::Rig2D* activeRig() const;
void setRigEditActive(bool active);
bool rigEditActive() const;
```

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

```cpp
void CompositionRenderController::setShowRigOverlay(bool show) {
    impl_->showRigOverlay_ = show;
    impl_->invalidateOverlayComposite();
}
bool CompositionRenderController::showRigOverlay() const {
    return impl_->showRigOverlay_;
}
void CompositionRenderController::setActiveRig(ArtifactCore::Rig2D* rig, const LayerID& layerId) {
    impl_->activeRig_ = rig;
    impl_->riggedLayerId_ = layerId;
    impl_->invalidateOverlayComposite();
}
ArtifactCore::Rig2D* CompositionRenderController::activeRig() const {
    return impl_->activeRig_;
}
void CompositionRenderController::setRigEditActive(bool active) {
    impl_->rigEditActive_ = active;
}
bool CompositionRenderController::rigEditActive() const {
    return impl_->rigEditActive_;
}
```

---

## Phase 1: ボーン・コントロールの表示（まず見えるようにする）

### Step 1.1 — 描画呼び出しを drawViewportCanvasOverlay に追加
**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`drawViewportCanvasOverlay` の末尾（grid 描画の前後どこでもOK）に追加:

```cpp
// ── Rig2D overlay ──
if (showRigOverlay_ && activeRig_) {
    auto comp = previewPipeline_.composition();
    auto rigLayer = comp ? comp->layerById(riggedLayerId_) : ArtifactAbstractLayerPtr{};
    if (rigLayer) {
        const QTransform globalTx = rigLayer->getGlobalTransform();
        const float zoom = renderer_->getZoom();
        const FloatColor boneColor(0.3f, 0.8f, 1.0f, 0.9f);   // 青
        const FloatColor wireColor(0.5f, 1.0f, 0.5f, 0.5f);     // 緑（薄）

        // ボーン描画
        if (activeRig_->rootBone()) {
            drawRigBone(renderer_.get(), activeRig_->rootBone(), globalTx,
                        boneColor, 2.5f, zoom);
        }
        // コントロール描画
        for (auto* ctrl : activeRig_->controls()) {
            drawRigControl(renderer_.get(), ctrl, globalTx);
        }
        // スキンメッシュのワイヤーフレーム（変形後）
        if (auto* mesh = activeRig_->skinMesh()) {
            std::vector<QVector2D> deformed;
            mesh->deform(activeRig_, deformed);
            // drawRigSkinWireframe の代わりに deform後の頂点で描画
            const auto& tris = mesh->triangles();
            for (size_t i = 0; i + 2 < tris.size(); i += 3) {
                for (int e = 0; e < 3; ++e) {
                    auto a = tris[i + e], b = tris[i + (e + 1) % 3];
                    if (a >= deformed.size() || b >= deformed.size()) continue;
                    QPointF pa = globalTx.map(QPointF(deformed[a].x(), deformed[a].y()));
                    QPointF pb = globalTx.map(QPointF(deformed[b].x(), deformed[b].y()));
                    renderer_->drawSolidLine(
                        {(float)pa.x(), (float)pa.y()}, {(float)pb.x(), (float)pb.y()},
                        wireColor, 0.7f);
                }
            }
        }
    }
}
```

検証: リグを作成し `setShowRigOverlay(true)` → VP上にボーンが表示されるか確認。

---

## Phase 2: ボーン・コントロールのピックとドラッグ

### Step 2.1 — ボーンヒットテスト関数
**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`drawViewportCanvasOverlay` の前（同じ名前空間か Impl の private メソッドとして）:

```cpp
Id hitTestRigBone(ArtifactCore::Rig2D* rig, Bone2D* bone,
                  const QPointF& canvasPos, const QTransform& globalTx,
                  float threshold) {
    if (!rig || !bone) return Id();
    QPointF bp = globalTx.map(QPointF(bone->resolvedTransform().position.x(),
                                       bone->resolvedTransform().position.y()));
    float len = bone->length();
    float rad = bone->resolvedTransform().rotation * (3.14159265f / 180.0f);
    QPointF tip(bp.x() + std::sin(rad) * len, bp.y() - std::cos(rad) * len);

    // 線分と点の距離
    QPointF ab = tip - bp;
    QPointF ap = canvasPos - bp;
    float abLenSq = ab.x()*ab.x() + ab.y()*ab.y();
    float t = abLenSq > 1e-6f ? std::clamp(
        static_cast<float>(ap.x()*ab.x() + ap.y()*ab.y()) / abLenSq, 0.0f, 1.0f) : 0.0f;
    QPointF proj = bp + ab * t;
    float distSq = (canvasPos - proj).x()*(canvasPos - proj).x() +
                   (canvasPos - proj).y()*(canvasPos - proj).y();

    float jointDistSq = (canvasPos - bp).x()*(canvasPos - bp).x() +
                         (canvasPos - bp).y()*(canvasPos - bp).y();
    if (jointDistSq < threshold * threshold) return bone->id();

    if (distSq < threshold * threshold) return bone->id();

    // 子を再帰探索
    for (auto* child : bone->children()) {
        Id hit = hitTestRigBone(rig, child, canvasPos, globalTx, threshold);
        if (!hit.isNil()) return hit;
    }
    return Id();
}
```

### Step 2.2 — コントロールヒットテスト
```cpp
Id hitTestRigControl(ArtifactCore::Rig2D* rig, const QPointF& canvasPos,
                     const QTransform& globalTx, float threshold) {
    if (!rig) return Id();
    for (auto* ctrl : rig->controls()) {
        if (!ctrl || !ctrl->enabled()) continue;
        QPointF cPos;
        if (ctrl->kind() == RigControlKind::Point) {
            QVector2D pt = ctrl->value().value<QVector2D>();
            cPos = globalTx.map(QPointF(pt.x(), pt.y()));
        } else {
            cPos = globalTx.map(QPointF(0, 0));
        }
        float distSq = (canvasPos.x() - cPos.x())*(canvasPos.x() - cPos.x()) +
                        (canvasPos.y() - cPos.y())*(canvasPos.y() - cPos.y());
        if (distSq < threshold * threshold) return ctrl->id();
    }
    return Id();
}
```

### Step 2.3 — handleMousePress に Rig ドラッグ開始を追加
**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`handleMousePress` の先頭近く（MotionSketch / Puppet の後、Pen tool の前あたり）に追加:

```cpp
// RigEdit tools
if (rigEditActive_ && activeRig_ && (activeTool == ToolType::RigSelect ||
                                      activeTool == ToolType::RigWeight)) {
    const auto cPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    const QPointF canvasPt(cPos.x, cPos.y);

    auto comp = impl_->previewPipeline_.composition();
    auto rigLayer = comp ? comp->layerById(riggedLayerId_) : ArtifactAbstractLayerPtr{};
    if (rigLayer) {
        const QTransform globalTx = rigLayer->getGlobalTransform();
        bool invertible = false;
        const QTransform invTx = globalTx.inverted(&invertible);
        if (invertible) {
            QPointF localPt = invTx.map(canvasPt);
            float threshold = 14.0f / impl_->renderer_->getZoom();

            // 1. コントロールを先にテスト
            Id hitCtrl = hitTestRigControl(activeRig_, localPt, QTransform(), threshold);
            if (!hitCtrl.isNil()) {
                draggingControlId_ = hitCtrl;
                selectedBoneId_ = Id(); // ボーン選択解除
                dragStartViewportPos_ = viewportPos;
                notifyViewportInteractionActivity();
                impl_->invalidateOverlayComposite();
                markRenderDirty();
                event->accept();
                return;
            }

            // 2. ボーンテスト
            if (activeTool == ToolType::RigSelect && activeRig_->rootBone()) {
                Id hitBone = hitTestRigBone(activeRig_, activeRig_->rootBone(),
                                             localPt, QTransform(), threshold);
                if (!hitBone.isNil()) {
                    selectedBoneId_ = hitBone;
                    draggingBoneId_ = hitBone;
                    Bone2D* bone = activeRig_->findBone(hitBone);
                    dragStartViewportPos_ = viewportPos;
                    dragStartBoneRotation_ = bone ? bone->localTransform().rotation : 0.0f;
                    notifyViewportInteractionActivity();
                    impl_->invalidateOverlayComposite();
                    markRenderDirty();
                    event->accept();
                    return;
                }
            }
        }
    }
}
```

### Step 2.4 — handleMouseMove に Rig ドラッグ更新を追加
`handleMouseMove` の先頭近く:

```cpp
// Rig bone drag
if (rigEditActive_ && !draggingBoneId_.isNil() && activeRig_) {
    Bone2D* bone = activeRig_->findBone(draggingBoneId_);
    if (bone) {
        float delta = static_cast<float>(viewportPos.x() - dragStartViewportPos_.x());
        float newRot = dragStartBoneRotation_ + delta * 0.5f;  // 感度調整
        bone->setLocalRotation(newRot);
        activeRig_->update();
        impl_->invalidateOverlayComposite();
        markRenderDirty();
    }
    return;
}

// Rig control drag
if (rigEditActive_ && !draggingControlId_.isNil() && activeRig_) {
    RigControl2D* ctrl = activeRig_->findControl(draggingControlId_);
    if (ctrl) {
        if (ctrl->kind() == RigControlKind::Slider) {
            float delta = static_cast<float>(viewportPos.x() - dragStartViewportPos_.x());
            float newVal = std::clamp(ctrl->value().toFloat() + delta * 0.005f, 0.0f, 1.0f);
            ctrl->setValue(newVal);
        } else if (ctrl->kind() == RigControlKind::Point) {
            // ビューポート→キャンバス変換でPointを更新
            const auto cPos = impl_->renderer_->viewportToCanvas(
                {(float)viewportPos.x(), (float)viewportPos.y()});
            ctrl->setValue(QVariant::fromValue(QVector2D((float)cPos.x, (float)cPos.y)));
        }
        activeRig_->update();
        impl_->invalidateOverlayComposite();
        markRenderDirty();
    }
    return;
}
```

### Step 2.5 — handleMouseRelease に Rig ドラッグ終了を追加
`handleMouseRelease` 内、gizmo release の前後:

```cpp
if (rigEditActive_ && (!draggingBoneId_.isNil() || !draggingControlId_.isNil())) {
    // commit to undo if needed
    draggingBoneId_ = Id();
    draggingControlId_ = Id();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
}
```

---

## Phase 3: ウェイトペイント

### Step 3.1 — ウェイトペイントのマウス処理
`handleMouseMove` に追加（Rig ドラッグ処理の後）:

```cpp
// Weight paint
if (rigEditActive_ && activeTool == ToolType::RigWeight &&
    (event->buttons() & Qt::LeftButton) && activeRig_->skinMesh()) {
    auto comp = impl_->previewPipeline_.composition();
    auto rigLayer = comp ? comp->layerById(riggedLayerId_) : ArtifactAbstractLayerPtr{};
    if (rigLayer) {
        const QTransform globalTx = rigLayer->getGlobalTransform();
        const auto cPos = impl_->renderer_->viewportToCanvas(
            {(float)viewportPos.x(), (float)viewportPos.y()});
        bool invertible = false;
        const QTransform invTx = globalTx.inverted(&invertible);
        if (invertible) {
            QPointF localPt = invTx.map(QPointF(cPos.x, cPos.y));

            // find last brush position for delta-based continuous stroke
            static QPointF lastBrushPos;
            float brushR = weightPaintRadius_ / impl_->renderer_->getZoom();
            float brushRSq = brushR * brushR;

            auto& verts = const_cast<std::vector<SkinVertex>&>(activeRig_->skinMesh()->vertices());
            for (auto& v : verts) {
                if (v.boneIndices[0] < 0) continue;
                float dx = v.position.x() - localPt.x();
                float dy = v.position.y() - localPt.y();
                if (dx*dx + dy*dy > brushRSq) continue;

                // find slot for selected bone
                for (int w = 0; w < 4; ++w) {
                    if (v.boneIndices[w] == weightPaintBoneIndex_) {
                        float falloff = 1.0f - std::sqrt(dx*dx + dy*dy) / brushR;
                        v.weights[w] = std::clamp(v.weights[w] + falloff * weightPaintOpacity_ * 0.1f, 0.0f, 1.0f);
                        break;
                    }
                }
                // normalize
                float sum = 0.0f;
                for (int w = 0; w < 4; ++w) sum += v.weights[w];
                if (sum > 1e-6f) for (int w = 0; w < 4; ++w) v.weights[w] /= sum;
            }
            lastBrushPos = localPt;
            activeRig_->update();
            impl_->invalidateOverlayComposite();
            markRenderDirty();
        }
    }
}
```

---

## Phase 4: リグレイヤー作成と描画

### Step 4.1 — ArtifactAbstractLayer に Rig2D 所有を追加
**ファイル**: `Artifact/include/Layer/ArtifactAbstractLayer.ixx`

```cpp
// public メソッドとして追加:
virtual ArtifactCore::Rig2D* rig() const { return nullptr; }
virtual void setRig(std::unique_ptr<ArtifactCore::Rig2D> rig) {}
virtual bool hasRig() const { return false; }
```

**ファイル**: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

（基底は abstract virtual で定義のみ、実際の実装は派生クラスまたは Impl に追加）

### Step 4.2 — Impl に Rig2D 所有を追加
**ファイル**: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

`ArtifactAbstractLayer::Impl` に追加:
```cpp
std::unique_ptr<ArtifactCore::Rig2D> rig_;
```

`ArtifactAbstractLayer::rig()` 実装:
```cpp
ArtifactCore::Rig2D* ArtifactAbstractLayer::rig() const { return impl_->rig_.get(); }
void ArtifactAbstractLayer::setRig(std::unique_ptr<ArtifactCore::Rig2D> r) { impl_->rig_ = std::move(r); }
bool ArtifactAbstractLayer::hasRig() const { return impl_->rig_ != nullptr; }
```

### Step 4.3 — リグレイヤー描画を drawLayerForCompositionView に追加
**ファイル**: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`

`drawLayerForCompositionView` の fallback `layer->draw(renderer)` の前に追加:

```cpp
// Rig layer: draw skinned mesh
if (auto* rig = layer->rig()) {
    if (auto* mesh = rig->skinMesh()) {
        const RationalTime rt(layer->currentFrame(), 30);
        rig->evaluate(rt);
        std::vector<QVector2D> deformed;
        mesh->deform(rig, deformed);
        const auto& tris = mesh->triangles();
        // draw textured triangles via renderer
        for (size_t i = 0; i + 2 < tris.size(); i += 3) {
            // ... basic triangle draw for now ...
        }
        return;
    }
}
```

---

## Phase 5: ポーズライブラリパネル

### Step 5.1 — ヘッダ作成
**ファイル**: `Artifact/include/Widgets/Rig/ArtifactPoseLibraryPanel.ixx` (新規)

```cpp
module;
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSlider>
#include <memory>
#include <vector>
export module Artifact.Widgets.Rig.PoseLibraryPanel;

import ArtifactCore.Rig2D;

export namespace Artifact {

class ArtifactPoseLibraryPanel : public QWidget {
    Q_OBJECT
public:
    explicit ArtifactPoseLibraryPanel(QWidget* parent = nullptr);
    
    void setRig(ArtifactCore::Rig2D* rig);
    void captureCurrentPose();
    void applyPose(int index, float blendWeight = 1.0f);
    int poseCount() const;
    
signals:
    void poseSelected(int index, float blendWeight);
    void poseCaptured();
    
private slots:
    void onCaptureClicked();
    void onSelectionChanged();
    void onBlendSliderChanged(int value);
    
private:
    void refreshList();
    
    ArtifactCore::Rig2D* rig_ = nullptr;
    QListWidget* poseList_ = nullptr;
    QPushButton* captureBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QSlider* blendSlider_ = nullptr;
    std::vector<ArtifactCore::PoseSnapshot> poses_;
};

} // namespace Artifact
```

### Step 5.2 — 実装
**ファイル**: `Artifact/src/Widgets/Rig/ArtifactPoseLibraryPanel.cppm` (新規)

```cpp
module;
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>

module Artifact.Widgets.Rig.PoseLibraryPanel;

import ArtifactCore.Rig2D;

namespace Artifact {

ArtifactPoseLibraryPanel::ArtifactPoseLibraryPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    auto* toolbar = new QHBoxLayout();
    captureBtn_ = new QPushButton("Capture");
    deleteBtn_ = new QPushButton("Delete");
    toolbar->addWidget(captureBtn_);
    toolbar->addWidget(deleteBtn_);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    poseList_ = new QListWidget();
    poseList_->setViewMode(QListWidget::IconMode);
    poseList_->setIconSize(QSize(64, 64));
    poseList_->setResizeMode(QListWidget::Adjust);
    layout->addWidget(poseList_);

    auto* blendRow = new QHBoxLayout();
    blendRow->addWidget(new QLabel("Blend:"));
    blendSlider_ = new QSlider(Qt::Horizontal);
    blendSlider_->setRange(0, 100);
    blendSlider_->setValue(100);
    blendRow->addWidget(blendSlider_);
    layout->addLayout(blendRow);

    connect(captureBtn_, &QPushButton::clicked, this, &ArtifactPoseLibraryPanel::onCaptureClicked);
    connect(poseList_, &QListWidget::currentRowChanged, this, &ArtifactPoseLibraryPanel::onSelectionChanged);
    connect(blendSlider_, &QSlider::valueChanged, this, &ArtifactPoseLibraryPanel::onBlendSliderChanged);
    connect(deleteBtn_, &QPushButton::clicked, [this]() {
        int row = poseList_->currentRow();
        if (row >= 0 && row < (int)poses_.size()) {
            poses_.erase(poses_.begin() + row);
            refreshList();
        }
    });
}

void ArtifactPoseLibraryPanel::setRig(ArtifactCore::Rig2D* rig) {
    rig_ = rig;
}

void ArtifactPoseLibraryPanel::captureCurrentPose() {
    if (rig_) {
        ArtifactCore::PoseSnapshot pose = ArtifactCore::capturePose(*rig_);
        bool ok;
        QString name = QInputDialog::getText(this, "Pose Name", "Name:", QLineEdit::Normal, "Pose", &ok);
        if (ok && !name.isEmpty()) {
            pose.name = name;
            poses_.push_back(pose);
            refreshList();
            emit poseCaptured();
        }
    }
}

void ArtifactPoseLibraryPanel::applyPose(int index, float blendWeight) {
    if (rig_ && index >= 0 && index < (int)poses_.size()) {
        ArtifactCore::applyPose(*rig_, poses_[index], blendWeight);
        rig_->update();
    }
}

int ArtifactPoseLibraryPanel::poseCount() const {
    return (int)poses_.size();
}

void ArtifactPoseLibraryPanel::refreshList() {
    poseList_->clear();
    for (const auto& pose : poses_) {
        auto* item = new QListWidgetItem(pose.name);
        item->setToolTip(pose.name);
        poseList_->addItem(item);
    }
}

void ArtifactPoseLibraryPanel::onCaptureClicked() {
    captureCurrentPose();
}

void ArtifactPoseLibraryPanel::onSelectionChanged() {
    int row = poseList_->currentRow();
    emit poseSelected(row, blendSlider_->value() / 100.0f);
}

void ArtifactPoseLibraryPanel::onBlendSliderChanged(int value) {
    int row = poseList_->currentRow();
    if (row >= 0) applyPose(row, value / 100.0f);
}

} // namespace Artifact
```

### Step 5.3 — ArtifactMainWindow に Dock 登録（Step 9 参照）

---

## Phase 6: リグ階層パネル

### Step 6.1 — ヘッダ
**ファイル**: `Artifact/include/Widgets/Rig/ArtifactRigHierarchyPanel.ixx` (新規)

```cpp
module;
#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
export module Artifact.Widgets.Rig.HierarchyPanel;

import ArtifactCore.Rig2D;

export namespace Artifact {

class ArtifactRigHierarchyPanel : public QWidget {
    Q_OBJECT
public:
    explicit ArtifactRigHierarchyPanel(QWidget* parent = nullptr);
    void setRig(ArtifactCore::Rig2D* rig);
    void refreshTree();

signals:
    void boneSelected(const Id& boneId);
    void controlSelected(const Id& controlId);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onAddBone();
    void onAddControl();
    void onContextMenu(const QPoint& pos);

private:
    QTreeWidgetItem* addBoneItem(Bone2D* bone, QTreeWidgetItem* parent = nullptr);
    void addControlItems();

    ArtifactCore::Rig2D* rig_ = nullptr;
    QTreeWidget* tree_ = nullptr;
};

} // namespace Artifact
```

### Step 6.2 — 実装
**ファイル**: `Artifact/src/Widgets/Rig/ArtifactRigHierarchyPanel.cppm` (新規)

```cpp
module;
#include <QMenu>
#include <QInputDialog>
#include <QHeaderView>

module Artifact.Widgets.Rig.HierarchyPanel;

import ArtifactCore.Rig2D;

namespace Artifact {

ArtifactRigHierarchyPanel::ArtifactRigHierarchyPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* toolbar = new QHBoxLayout();
    auto* addBoneBtn = new QPushButton("+Bone");
    auto* addCtrlBtn = new QPushButton("+Ctrl");
    toolbar->addWidget(addBoneBtn);
    toolbar->addWidget(addCtrlBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    tree_ = new QTreeWidget();
    tree_->setHeaderLabels({"Name", "Type"});
    tree_->header()->setStretchLastSection(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(tree_);

    connect(addBoneBtn, &QPushButton::clicked, this, &ArtifactRigHierarchyPanel::onAddBone);
    connect(addCtrlBtn, &QPushButton::clicked, this, &ArtifactRigHierarchyPanel::onAddControl);
    connect(tree_, &QTreeWidget::itemClicked, this, &ArtifactRigHierarchyPanel::onItemClicked);
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &ArtifactRigHierarchyPanel::onContextMenu);
}

void ArtifactRigHierarchyPanel::setRig(ArtifactCore::Rig2D* rig) {
    rig_ = rig;
    refreshTree();
}

void ArtifactRigHierarchyPanel::refreshTree() {
    tree_->clear();
    if (!rig_) return;

    auto* bonesRoot = new QTreeWidgetItem(tree_, {"Bones", ""});
    if (rig_->rootBone()) {
        addBoneItem(rig_->rootBone(), bonesRoot);
    }
    bonesRoot->setExpanded(true);

    auto* controlsRoot = new QTreeWidgetItem(tree_, {"Controls", ""});
    addControlItems();
    controlsRoot->setExpanded(true);
}

QTreeWidgetItem* ArtifactRigHierarchyPanel::addBoneItem(Bone2D* bone, QTreeWidgetItem* parent) {
    if (!bone) return nullptr;
    auto* item = new QTreeWidgetItem(parent ? parent : tree_->invisibleRootItem(),
                                      {bone->name(), "Bone"});
    item->setData(0, Qt::UserRole, bone->id().toString());
    for (auto* child : bone->children()) {
        addBoneItem(child, item);
    }
    return item;
}

void ArtifactRigHierarchyPanel::addControlItems() {
    if (!rig_) return;
    auto* root = tree_->topLevelItem(1); // "Controls"
    if (!root) return;
    for (auto* ctrl : rig_->controls()) {
        QString typeName;
        switch (ctrl->kind()) {
        case RigControlKind::Slider: typeName = "Slider"; break;
        case RigControlKind::Point:  typeName = "Point"; break;
        case RigControlKind::Angle:  typeName = "Angle"; break;
        }
        auto* item = new QTreeWidgetItem(root, {ctrl->name(), typeName});
        item->setData(0, Qt::UserRole, ctrl->id().toString());
    }
}

void ArtifactRigHierarchyPanel::onItemClicked(QTreeWidgetItem* item, int) {
    if (!item || !rig_) return;
    QString idStr = item->data(0, Qt::UserRole).toString();
    if (idStr.isEmpty()) return;
    Id id(idStr);
    if (rig_->findBone(id)) {
        emit boneSelected(id);
    } else if (rig_->findControl(id)) {
        emit controlSelected(id);
    }
}

void ArtifactRigHierarchyPanel::onAddBone() {
    if (!rig_) return;
    bool ok;
    QString name = QInputDialog::getText(this, "Add Bone", "Name:", QLineEdit::Normal, "Bone", &ok);
    if (ok && !name.isEmpty()) {
        // Find selected bone as parent
        auto* sel = tree_->currentItem();
        Id parentId;
        if (sel) {
            QString idStr = sel->data(0, Qt::UserRole).toString();
            if (!idStr.isEmpty()) parentId = Id(idStr);
        }
        rig_->addBone(name, parentId);
        refreshTree();
    }
}

void ArtifactRigHierarchyPanel::onAddControl() {
    if (!rig_) return;
    rig_->addSlider("Control", 0.0, 0.0, 1.0);
    refreshTree();
}

void ArtifactRigHierarchyPanel::onContextMenu(const QPoint& pos) {
    auto* item = tree_->itemAt(pos);
    if (!item || !rig_) return;
    QString idStr = item->data(0, Qt::UserRole).toString();
    if (idStr.isEmpty()) return;

    QMenu menu;
    menu.addAction("Rename", [this, item]() {
        bool ok;
        QString name = QInputDialog::getText(this, "Rename", "Name:", QLineEdit::Normal, item->text(0), &ok);
        if (ok && !name.isEmpty()) item->setText(0, name);
    });
    menu.addAction("Delete", [this, idStr]() {
        Id id(idStr);
        rig_->removeBone(id);
        refreshTree();
    });
    menu.exec(tree_->viewport()->mapToGlobal(pos));
}

} // namespace Artifact
```

---

## Phase 7: レイヤーとの連携（リグ作成メニュー）

### Step 7.1 — レイヤーメニューに追加
**ファイル**: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`

```cpp
// "New" サブメニューに追加:
auto* newRigAction = newMenu->addAction("Rig Layer");
connect(newRigAction, &QAction::triggered, [this]() {
    auto comp = currentComposition();
    if (!comp) return;
    auto rigLayer = createLayer(LayerType::Rig); // or Null + Rig2D
    comp->appendLayerTop(rigLayer);
});
```

---

## Phase 8: タイムライン連携

### Step 8.1 — リグレイヤーのトラック表示
**ファイル**: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`

既存のレイヤートラック描画に、リグレイヤーの場合の特別処理を追加:

```cpp
// drawLayerTrack 内:
if (layer->hasRig()) {
    // draw rig control keyframe markers
    auto* rig = layer->rig();
    for (auto* bone : rig->bones()) {
        if (bone && bone->keyFrameCount() > 0) {
            // draw small diamond markers for each keyframe
        }
    }
}
```

（タイムライン連携は複雑なので、初期実装では省略し Phase 3+ で着手）

---

## Phase 9: ArtifactMainWindow への Dock 登録

### Step 9.1 — Dock 追加
**ファイル**: `Artifact/src/Widgets/ArtifactMainWindow.cppm`

```cpp
// setupDockWidgets() 内:
auto* posePanel = new ArtifactPoseLibraryPanel(this);
auto* poseDock = new QDockWidget("Pose Library", this);
poseDock->setWidget(posePanel);
addDockWidget(Qt::RightDockWidgetArea, poseDock);

auto* rigHierarchyPanel = new ArtifactRigHierarchyPanel(this);
auto* rigDock = new QDockWidget("Rig Hierarchy", this);
rigDock->setWidget(rigHierarchyPanel);
addDockWidget(Qt::RightDockWidgetArea, rigDock);
```

---

## 検証チェックリスト

- [ ] `ToolType::RigSelect` / `RigWeight` が追加されている
- [ ] `setShowRigOverlay(true)` + Rig2D でボーンがVPに表示される
- [ ] コントロール（Slider/Point/Angle）が丸で表示される
- [ ] ボーンクリックで選択・ドラッグで回転できる
- [ ] コントロールクリックで選択・ドラッグで値が変わる
- [ ] ウェイトペイントでメッシュ頂点のweightが変わる
- [ ] ポーズパネルで Capture/Apply できる
- [ ] リグ階層パネルで Add/Delete/Rename できる
- [ ] リグ編集中、他のツールと競合しない

---

## 想定所要工数（参考）

| Phase | 内容 | 規模 |
|-------|------|------|
| 0 | 準備（enum, member, API） | 小 |
| 1 | ボーン・コントロール表示 | 小 |
| 2 | ピック＆ドラッグ | 中 |
| 3 | ウェイトペイント | 中 |
| 4 | リグレイヤー作成と描画 | 中 |
| 5 | ポーズライブラリパネル | 小（新規ファイル2） |
| 6 | リグ階層パネル | 中（新規ファイル2） |
| 7 | メニュー統合 | 小 |
| 8 | タイムライン連携 | 大（後回し推奨） |
| 9 | Dock 登録 | 小 |
