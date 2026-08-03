# ハイエンド 2D アニメーションリグシステム 設計

**日付**: 2026-07-31
**現状**: `ArtifactCore/include/Rig/Rig2D.ixx` にボーン階層、コントロール、制約、IK の基本実装あり
**目標**: Spine / Live2D / Moho 水準のプロフェッショナル向け 2D リグシステム

---

## 1. コンセプト

2D アニメーションリグの3本柱：

```
          ┌─────────────┐
          │   Rig2D      │  ← 既存: ボーン階層 + IK + 制約
          │  (Skeleton)  │
          └──────┬──────┘
                 │ drives
    ┌────────────┼────────────┐
    │            │            │
┌───┴───┐  ┌────┴─────┐  ┌──┴──────────┐
│Skin2D │  │Deformer  │  │Animation     │
│(変形) │  │Stack     │  │Layers / Pose│
└───────┘  │(変形器)  │  │(アニメ機構) │
           └──────────┘  └─────────────┘
```

---

## 2. Skin2D — メッシュスキニング

### 2.1 コア型

```cpp
// メッシュ頂点（変形対象）
struct SkinVertex {
    Detail::float2 position;   // rest pose 位置
    Detail::float2 uv;         // テクスチャ座標
    float weights[4];          // 上位4ボーンのウェイト
    uint8_t boneIndices[4];    // 対応するボーン ID インデックス
};

// メッシュ
class SkinMesh {
public:
    void setVertices(const std::vector<SkinVertex>& verts);
    void setTriangles(const std::vector<uint32_t>& indices);
    void setTexture(const ArtifactCore::ImageF32x4_RGBA& image);
    void setImageLayerId(const LayerID& id);  // ソース画像レイヤー

    // バインドポーズの設定
    void bindToRig(Rig2D* rig);
    void autoBind(int maxBonesPerVertex = 4);  // 距離ベースの自動バインド

    // 変形実行（CPU / GPU 両対応）
    void deform(RigEvaluationContext2D& ctx,
                std::vector<Detail::float2>& outPositions);
    void deformGPU(RigEvaluationContext2D& ctx);  // compute shader

    const std::vector<SkinVertex>& vertices() const;
    const std::vector<uint32_t>& triangles() const;

private:
    std::vector<SkinVertex> vertices_;
    std::vector<uint32_t> triangles_;
    std::vector<Detail::float2> restPositions_;  // バインドポーズ
    // GPU バッファハンドル
};
```

### 2.2 変形アルゴリズム

```cpp
enum class SkinningMethod {
    LBS,              // Linear Blend Skinning（標準、高速）
    DQS,              // Dual Quaternion Skinning（ボリューム保持、Spine方式）
    MLS,              // Moving Least Squares（変形点ベース、Live2D方式）
    FFD,              // Free-Form Deformation（格子変形）
    PBD,              // Position-Based Dynamics（物理ベース）
};

class SkinningSolver {
public:
    virtual void solve(const SkinMesh& mesh,
                       RigEvaluationContext2D& ctx,
                       std::vector<Detail::float2>& out) = 0;
    virtual SkinningMethod method() const = 0;
};

class LBSSkinningSolver : public SkinningSolver { ... };
class DQSSkinningSolver : public SkinningSolver { ... };
class MLSSkinningSolver : public SkinningSolver { ... };
```

### 2.3 ウェイトペインティング（VP上）

```cpp
// Weight Paint ツール
class WeightPaintTool {
public:
    // ブラシでウェイトを塗る
    void paintStroke(SkinMesh& mesh, int boneIndex,
                     const std::vector<QPointF>& canvasPoints,
                     float brushRadius, float opacity, float flow);

    // グラデーションツール
    void gradientFill(SkinMesh& mesh, int boneIndex,
                      const QPointF& from, const QPointF& to,
                      float fromWeight, float toWeight);

    // スムージング
    void smooth(SkinMesh& mesh, int iterations = 3);

    // 自動正規化（全ボーンの合計 = 1.0）
    void normalize(SkinMesh& mesh);

    // ミラー（対称ボーンにウェイトをコピー）
    void mirror(SkinMesh& mesh, const std::map<int,int>& boneMirrorMap);

    // ヒートマップ表示（VP オーバーレイでボーンごとに色付け）
    void drawWeightOverlay(ArtifactIRenderer* renderer, const SkinMesh& mesh,
                           int selectedBone);
};
```

---

## 3. Deformer Stack — 変形器チェイン

### 3.1 デフォーマ種類

```cpp
enum class DeformerType {
    Bend,            // 曲げ
    Twist,           // ねじり
    SquashStretch,   // 押しつぶし・引き伸ばし
    Inflate,         // 膨張
    Noise,           // ノイズ変形
    Lattice,         // 格子変形（FFD）
    Curve,           // パス沿い変形
    Wave,            // 波
    Jiggle,          // ジグル（物理）
    Custom           // カスタム（expression）
};

// デフォーマパラメータの基底
struct DeformerParams {
    bool enabled = true;
    float strength = 1.0f;
    std::optional<QRectF> region;  // 変形領域（nullopt = 全体）
    // アニメーション可能なプロパティ
};

struct BendDeformerParams : DeformerParams {
    float angle = 0.0f;       // 曲げ角度
    float curvature = 0.0f;   // 曲率
    QPointF pivot;            // 基準点
    QString axis = "Y";       // 曲げ軸
};

struct TwistDeformerParams : DeformerParams {
    float angle = 0.0f;       // ねじり角度
    QPointF center;
    QString axis = "Y";
};

struct SquashStretchDeformerParams : DeformerParams {
    float factorX = 1.0f;
    float factorY = 1.0f;
    QPointF center;
    float volumePreservation = 0.5f;  // 体積保持（0=なし、1=完全）
};

struct LatticeDeformerParams : DeformerParams {
    int columns = 4;
    int rows = 4;
    std::vector<QPointF> controlPoints;  // 格子制御点
};

struct CurveDeformerParams : DeformerParams {
    std::vector<QPointF> pathPoints;     // 変形パス
    float offset = 0.0f;                 // パスに沿ったオフセット
};

struct JiggleDeformerParams : DeformerParams {
    float mass = 1.0f;
    float stiffness = 100.0f;
    float damping = 10.0f;
    QPointF gravity = {0, 9.8f};
};
```

### 3.2 デフォーマチェイン

```cpp
class DeformerChain {
public:
    void prepend(std::unique_ptr<IDeformer> deformer);
    void append(std::unique_ptr<IDeformer> deformer);
    void insert(int index, std::unique_ptr<IDeformer> deformer);
    void remove(int index);
    void clear();
    int count() const;

    // 全デフォーマを順に適用
    void apply(std::vector<Detail::float2>& positions,
               const QRectF& bounds,
               const RationalTime& time);

    IDeformer* at(int index) const;

private:
    std::vector<std::unique_ptr<IDeformer>> deformers_;
};

class IDeformer {
public:
    virtual ~IDeformer() = default;
    virtual DeformerType type() const = 0;
    virtual void apply(std::vector<Detail::float2>& positions,
                       const QRectF& bounds) = 0;
    virtual void setParams(const DeformerParams& params) = 0;
    virtual const DeformerParams& params() const = 0;
    virtual bool animatableProperties(std::vector<PropertyPath>& out) const = 0;
};
```

---

## 4. Smart Bones（スマートボーン）

Spine の重要な機能。1つのボーン回転が複数のプロパティを同時に駆動する。

```cpp
struct SmartBoneEntry {
    float driverAngle;              // 駆動ボーンの角度
    std::map<Id, BoneTransform> targetTransforms;  // ターゲットボーン → 変換
    std::map<Id, float> meshOffsets;                // メッシュオフセット
};

class SmartBoneController {
public:
    void setDriverBone(const Id& boneId);
    void addKey(float driverAngle,
                const std::map<Id, BoneTransform>& targets);
    void removeKey(float driverAngle);

    // 指定角度での補間値を取得
    void evaluate(float driverAngle,
                  std::map<Id, BoneTransform>& outTargets);

    // カーブエディタで角度-値カーブを編集
    void editCurve(Id targetBoneId, const std::string& channel);

private:
    Id driverBoneId_;
    std::vector<SmartBoneEntry> keys_;  // driverAngle でソート
};
```

---

## 5. Pose Library（ポーズライブラリ）

```cpp
struct PoseSnapshot {
    QString name;
    QString category;                 // "Idle", "Walk", "Attack" など
    std::map<Id, BoneTransform> boneTransforms;
    std::map<Id, QVariant> controlValues;
    QImage thumbnail;
    RationalTime frameReference;
};

class PoseLibrary {
public:
    void addPose(const PoseSnapshot& pose);
    void removePose(const QString& name);
    PoseSnapshot* findPose(const QString& name);

    // ポーズを現在フレームに適用（ブレンド可能）
    void applyPose(Rig2D* rig, const QString& poseName,
                   float blendWeight = 1.0f,
                   const RationalTime& atTime = RationalTime(0, 30));

    // 2つのポーズ間を補間
    PoseSnapshot blend(const PoseSnapshot& a, const PoseSnapshot& b,
                       float t);

    // 現在のリグ状態からスナップショット作成
    PoseSnapshot capture(Rig2D* rig, const RationalTime& time);

    // ファイル入出力
    void save(const QString& filePath);
    void load(const QString& filePath);

    // VP パレット表示
    void drawPosePalette(ArtifactIRenderer* renderer,
                         const QRectF& viewportRect);

private:
    std::vector<PoseSnapshot> poses_;
};
```

---

## 6. Animation Layers（アニメーションレイヤー）

```cpp
enum class LayerBlendMode {
    Override,       // 上書き
    Additive,       // 加算（差分のみ）
    Multiply        // 乗算
};

class AnimationLayer {
public:
    AnimationLayer(const QString& name, LayerBlendMode mode);

    // キーフレーム（このレイヤー固有）
    void setKeyFrame(const Id& boneId, const RationalTime& time,
                     const BoneTransform& transform);
    void setControlKey(const Id& controlId, const RationalTime& time,
                       const QVariant& value);

    // 指定時間の変換を取得
    BoneTransform evaluateBone(const Id& boneId, const RationalTime& time);
    QVariant evaluateControl(const Id& controlId, const RationalTime& time);

    // 重み
    float weight() const;
    void setWeight(float w);

    bool solo() const;
    void setSolo(bool s);

    bool muted() const;
    void setMuted(bool m);

private:
    QString name_;
    LayerBlendMode blendMode_;
    float weight_ = 1.0f;
    bool solo_ = false;
    bool muted_ = false;
    // boneId → (time → transform)
    std::map<Id, AnimatableValueT<BoneTransform>> boneAnimations_;
    std::map<Id, AnimatableValueT<QVariant>> controlAnimations_;
};

class AnimationLayerStack {
public:
    void addLayer(const QString& name, LayerBlendMode mode = LayerBlendMode::Override);
    void removeLayer(int index);
    void moveLayer(int from, int to);
    int layerCount() const;

    // 全レイヤーを合成して最終変換を取得
    BoneTransform evaluateBone(const Id& boneId, const RationalTime& time,
                               const BoneTransform& baseTransform);
    QVariant evaluateControl(const Id& controlId, const RationalTime& time,
                             const QVariant& baseValue);

    AnimationLayer* layer(int index);

private:
    std::vector<AnimationLayer> layers_;
    // baseTransform は Rig2D 自体のキーフレーム（レイヤー0相当）
};
```

---

## 7. Auto Rigging（自動リギング）

```cpp
class AutoRigger2D {
public:
    // 画像のシルエット解析からボーンを自動生成
    struct AutoRigResult {
        std::vector<Bone2D> bones;
        std::vector<std::pair<int, int>> parentMap; // child idx → parent idx
        SkinMesh mesh;
    };

    // 人間型2Dキャラクターの自動リギング
    AutoRigResult rigHumanoid(const ArtifactCore::ImageF32x4_RGBA& image,
                               const HumanoidParams& params);

    // 汎用メッシュの自動ボーン配置
    AutoRigResult rigFromMesh(const SkinMesh& mesh,
                               int maxBones = 30);

    // 対称ボーンの自動検出
    std::map<Id, Id> detectSymmetry(Rig2D* rig);

private:
    // 内部: 距離場ベースのスケルトン抽出 → ボーン配置
    // 内部: クラスタリング → 関節位置推定
};
```

---

## 8. Onion Skinning（オニオンスキニング）

```cpp
class OnionSkinRenderer {
public:
    struct OnionSettings {
        int prevFrames = 2;        // 前フレーム表示数
        int nextFrames = 2;        // 次フレーム表示数
        float prevOpacity = 0.3f;  // 過去フレームの不透明度
        float nextOpacity = 0.3f;  // 未来フレームの不透明度
        FloatColor prevTint = FloatColor(1.0f, 0.3f, 0.3f, 1.0f);  // 赤
        FloatColor nextTint = FloatColor(0.3f, 0.3f, 1.0f, 1.0f);  // 青
        bool showOutlines = true;  // アウトラインのみ表示
    };

    void setSettings(const OnionSettings& settings);
    void draw(ArtifactIRenderer* renderer, Rig2D* rig,
              SkinMesh* mesh, const RationalTime& currentTime);

private:
    OnionSettings settings_;
    // キャッシュ: 過去数フレームの変形済みメッシュ
    struct OnionCacheEntry {
        RationalTime time;
        std::vector<Detail::float2> deformedPositions;
    };
    std::deque<OnionCacheEntry> cache_;
};
```

---

## 9. Space Switching（空間切り替え）

```cpp
enum class ControlSpace {
    World,         // ワールド空間
    Local,         // 親空間
    CustomObject,  // 指定オブジェクト空間
    View           // カメラ空間
};

class ControlSpaceManager {
public:
    void setSpace(Id controlId, ControlSpace space);
    void setCustomSpace(Id controlId, Id referenceObjectId);

    QMatrix4x4 computeSpaceMatrix(Id controlId,
                                   RigEvaluationContext2D& ctx);

    // 親オブジェクトの移動に追従するか
    bool followParent(Id controlId) const;
    void setFollowParent(Id controlId, bool follow);

private:
    struct SpaceEntry {
        ControlSpace space = ControlSpace::Local;
        Id referenceObjectId;  // CustomObject の場合
        bool followParent = true;
    };
    std::map<Id, SpaceEntry> spaceMap_;
};
```

---

## 10. Driven Keys / Set Driven Key

```cpp
struct DriverKey {
    float driverValue;
    float drivenValue;
    InterpolationType interpolation = InterpolationType::Linear;
};

class DrivenKeyRelation {
public:
    void setDriver(Id driverBoneId, const std::string& driverChannel);
    void setDriven(Id drivenBoneId, const std::string& drivenChannel);

    void addKey(float driverValue, float drivenValue);
    void removeKey(int index);
    int keyCount() const;

    float evaluate(float driverValue) const;

private:
    Id driverBoneId_;
    std::string driverChannel_;  // "rotation", "position.x", etc.
    Id drivenBoneId_;
    std::string drivenChannel_;
    std::vector<DriverKey> keys_; // driverValue でソート
};
```

---

## 11. Control Shape System（コントロール形状）

VP 上でのリグコントロールの視覚表現。

```cpp
enum class ControlShapeType {
    Circle, Square, Diamond, Cross,
    Arrow, Cube, Sphere,
    Custom,     // カスタム SVG/画像
    Null         // 非表示
};

struct ControlShape {
    ControlShapeType type = ControlShapeType::Circle;
    float size = 24.0f;
    FloatColor color = FloatColor(1.0f, 0.3f, 0.3f, 1.0f);
    FloatColor selectedColor = FloatColor(1.0f, 0.8f, 0.2f, 1.0f);
    float outlineThickness = 2.0f;
    QString customShapeAsset;  // SVG/PNG パス
};

class ControlShapeRenderer {
public:
    void setShape(Id controlId, const ControlShape& shape);
    void drawAll(ArtifactIRenderer* renderer, Rig2D* rig,
                 const RigEvaluationContext2D& ctx);

    // ピック
    Id pickControl(const QPointF& viewportPos,
                   ArtifactIRenderer* renderer,
                   Rig2D* rig,
                   const RigEvaluationContext2D& ctx);

private:
    std::map<Id, ControlShape> shapes_;
};
```

---

## 12. Retargeting（アニメーション転送）

```cpp
class AnimationRetargeter {
public:
    // ソースリグ → ターゲットリグにアニメーションを転送
    void retarget(Rig2D* sourceRig, Rig2D* targetRig,
                  const std::map<Id, Id>& boneMap,   // srcBone → dstBone
                  const RationalTime& srcTimeStart,
                  const RationalTime& srcTimeEnd,
                  const RationalTime& dstTimeStart);

    // 自動ボーンマッピング（名前ベース）
    std::map<Id, Id> autoMap(Rig2D* sourceRig, Rig2D* targetRig,
                              float nameSimilarityThreshold = 0.7f);

    // リターゲット時のスケール/オフセット補正
    void setScaleCompensation(float factor);
    void setRootOffset(const QVector2D& offset);
};
```

---

## 13. VP 統合

### 13.1 リグ編集モード

```cpp
// CompositionRenderController に追加
class RigEditMode {
public:
    void enter(Rig2D* rig, SkinMesh* mesh);
    void exit();

    // handleMousePress でリグコントロールを優先ピック
    bool handleMousePress(const QPointF& viewportPos,
                          CompositionRenderController* controller);
    bool handleMouseMove(const QPointF& viewportPos,
                         CompositionRenderController* controller);
    bool handleMouseRelease();

    // 描画
    void drawOverlay(ArtifactIRenderer* renderer);
    void drawBones(ArtifactIRenderer* renderer);
    void drawControls(ArtifactIRenderer* renderer);
    void drawMeshWireframe(ArtifactIRenderer* renderer);
    void drawWeights(ArtifactIRenderer* renderer);

    // ツール切替
    void setTool(RigEditTool tool);
    RigEditTool currentTool() const;

private:
    Rig2D* rig_ = nullptr;
    SkinMesh* mesh_ = nullptr;
    RigEditTool tool_ = RigEditTool::Select;
    Id selectedBoneId_;
    Id selectedControlId_;
    bool dragging_ = false;
    ControlShapeRenderer controlRenderer_;
    WeightPaintTool weightPaintTool_;
    OnionSkinRenderer onionSkinRenderer_;
    BoneDrawStyle boneDrawStyle_ = BoneDrawStyle::Filled;
};

enum class RigEditTool {
    Select,          // ボーン/コントロール選択
    Move,            // ボーン/コントロール移動
    Rotate,          // ボーン回転
    CreateBone,      // 新規ボーン作成
    CreateControl,   // 新規コントロール作成
    WeightPaint,     // ウェイトペイント
    Pose,            // ポーズ適用
};
```

---

## 14. ファイル構成

```
ArtifactCore/
├── include/
│   └── Rig/
│       ├── Rig2D.ixx                    ← 既存（拡張）
│       ├── Skin2D.ixx                   ← 新規: SkinMesh, SkinningSolver
│       ├── Deformer2D.ixx               ← 新規: DeformerChain, IDeformer, 各デフォーマ
│       ├── SmartBone2D.ixx              ← 新規: SmartBoneController
│       ├── PoseLibrary2D.ixx            ← 新規
│       ├── AnimationLayer2D.ixx         ← 新規
│       ├── AutoRigger2D.ixx             ← 新規
│       ├── OnionSkin2D.ixx              ← 新規
│       ├── ControlSpace2D.ixx           ← 新規
│       ├── DrivenKey2D.ixx              ← 新規
│       └── AnimationRetargeter2D.ixx    ← 新規
└── src/
    └── Rig/
        ├── Rig2D.cppm                   ← 既存（拡張）
        ├── Skin2D.cppm                  ← 新規
        ├── Deformer2D.cppm              ← 新規
        ├── SmartBone2D.cppm             ← 新規
        ├── PoseLibrary2D.cppm           ← 新規
        ├── AnimationLayer2D.cppm        ← 新規
        ├── AutoRigger2D.cppm            ← 新規
        ├── OnionSkin2D.cppm             ← 新規
        └── AnimationRetargeter2D.cppm   ← 新規

Artifact/
├── include/
│   └── Widgets/Render/
│       └── ControlShapeRenderer.ixx     ← 新規: VP上でのコントロール図形描画
└── src/
    └── Widgets/Render/
        ├── ControlShapeRenderer.cppm    ← 新規
        └── RigEditMode.cppm             ← 新規: VPリグ編集モード
```

---

## 15. 実装優先順位

| 優先度 | モジュール | 理由 |
|--------|-----------|------|
| **P0** | Skin2D (LBS) | メッシュ変形がないとリグが意味をなさない |
| **P0** | ControlShapeRenderer | VP上でコントロールが見えないと操作不能 |
| **P1** | WeightPaintTool | スキニングに不可欠 |
| **P1** | SmartBone2D | Spine互換の最重要機能 |
| **P1** | DeformerChain (Bend, Twist, SquashStretch) | 最も使用頻度の高い変形 |
| **P2** | PoseLibrary | アニメーターの生産性を大幅に向上 |
| **P2** | AnimationLayerStack | ノンリニア編集に必要 |
| **P2** | OnionSkinRenderer | 作画補助に必須 |
| **P3** | DQS / MLS スキニング | LBS の品質限界を超える |
| **P3** | AutoRigger2D | 量産に必須だが複雑 |
| **P3** | AnimationRetargeter | アセット共有に便利 |
| **P4** | DrivenKey, ControlSpace | 細かい制御、プロ向け |
| **P4** | Jiggle, Physics | 二次アニメーション、演出用 |
