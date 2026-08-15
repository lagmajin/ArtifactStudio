# MILESTONE: 3D Viewport & Rendering Production Hardening

**日付**: 2026-08-15
**現状**: 2D Gizmo と 3D Axis／Projected Frame Gizmo の描画・ヒットテスト・ドラッグ経路は実装済み。RayTracing は mesh の vertex/index buffer、content hash、BLAS create/update、TLAS build に加え、warmup 用 ray-generation／miss／closest-hit shader、PSO、SBT、出力 UAV、`TraceRays()` dispatch まで Core に実装されている。ただし最終描画／lighting 連携と実ジオメトリ用 shader 契約は未確認。CPU レイトレーサーは動いているがエディタ未接続。Volume レンダリングは CPU のみ。照明は shadow map の生成／SRV 接続まで部分実装で、全面的な寄与は未確認。DOF はカメラパラメータと shader 資産が存在するが Diligent の DOF pass は未統合。
**目標**: Frame Gizmo 修正、DXR に実ジオメトリ投入、Volume GPU 化、照明パイプライン接続、DOF 有効化。

## 現行コード監査 (2026-08-15)

Projected frame gizmo は `Artifact3DGizmo` の hit test、local basis、mode 切替、camera matrix、drag 更新、undo 経路が `CompositionRenderController` に統合されており、旧記述の「描画のみ／2D canvas-space 不一致」は現行コードと一致しない。3D axis gizmo と selection overlay も同じ controller 経路で確認できる。RayTracing は `ArtifactIRenderer` から mesh geometry を hash 付きで `createOrUpdateBLAS()` へ渡し、TLAS を再構築する経路が確認できる。`RayTracingManager` には warmup shader の `TraceRay()` と `IDeviceContext::TraceRays()`、RGBA32F output texture／SBT の実装もあるが、renderer の通常描画結果へ接続された最終パスかは未確認である。さらに shadow map は 1024² depth texture／DSV／SRV の生成、caster pass、main render への SRV 接続まで存在し、`shadowMapDebugState()` と Frame Debug の `Shadow Map` resource 診断も追加した。GPU volume、DOF の Diligent 接続、runtime 受入は未検証である。

## 現状マトリクス

| コンポーネント | 状態 | 決定的な穴 |
|---------------|------|-----------|
| 2D TransformGizmo | ✅ 完成 | — |
| 3D Axis Gizmo | ✅ 描画+ヒット+ドラッグ | — |
| **3D Frame Gizmo** | ✅ 描画+ヒット+ドラッグ | runtime 検証は未実施 |
| GPU RayTracing | 🟡 warmup dispatch／acceleration structure 実装 | `TraceRay()` と `TraceRays()` は Core に存在。通常 renderer の最終出力／実ジオメトリ shader 接続は未確認 |
| CPU RayTracer | ✅ 機能 | エディタ未接続。BVH AABB がダミー |
| Volume Rendering | ⚠️ CPU only | GPU パス不在。エディタ未接続 |
| 3D Lighting | 🟡 shadow map 部分実装 | shadow map の生成／SRV 接続あり。全面的な lighting 契約は未完 |
| Depth of Field | ❌ 未着手 | シェーダー資産は存在。深度SRV未作成 |
| Selection Wireframe | ⚠️ 実装済み | ランタイム検証未。エッジキャッシュ未 |
| Ground Grid | ⚠️ データモデル完成 | 3D ビューポート描画未接続 |
| GizmoMode.ixx | ❌ 空 | 4行の空モジュール |

---

## Phase 1: 3D Frame Gizmo 修正

### 1.1 根本原因

`CompositionRenderController` のマウスプレスハンドラ (~line 20602) は、projected frame corner のヒット時に `gizmo3D_->beginDrag(frameAxis, createPickingRay(viewportPos))` を呼ぶ。これは正しい。しかし `gizmo3D_->updateDrag()` が返すデルタは **3D ワールド座標**であり、メインのマウスムーブハンドラ (~line 19640) はこれを直接レイヤーの position に適用している。Frame resize に必要なのは **レイヤー平面上での scaleX/scaleY 変更** であり、ワールド座標の平行移動とは異なる。

### 1.2 修正手順

**Step 1**: `ArtifactProjectedFrameGizmo` クラスを新設（既存 SPEC_3D_FRAME_GIZMO_REQUIREMENTS に従う）

```cpp
// 新規: Artifact/include/Widgets/Render/ArtifactProjectedFrameGizmo.ixx
class ArtifactProjectedFrameGizmo {
public:
    enum class Handle { None, CornerTL, CornerTR, CornerBL, CornerBR,
                              EdgeTop, EdgeBottom, EdgeLeft, EdgeRight,
                              RotateRing, Interior };
    
    // ヒットテスト: ビューポート座標→ハンドル種類
    Handle hitTest(const QPointF& viewportPos,
                   const Artifact3DCamera& camera,
                   const Transform3D& layerTransform,
                   const QRectF& localBounds);
    
    // ドラッグ開始
    bool beginDrag(Handle handle, const QPointF& viewportPos,
                   const Artifact3DCamera& camera);
    
    // ドラッグ中: デルタ→scale/position変更
    struct DragResult {
        float newScaleX;
        float newScaleY;
        QVector3D newPosition;  // fixed-point corner を維持する位置調整
        float newRotation;      // RotateRing 用
    };
    DragResult updateDrag(const QPointF& currentViewportPos,
                          const Artifact3DCamera& camera,
                          const Transform3D& currentTransform);
    
    // ドラッグ終了
    void endDrag();
    
    // 描画
    void draw(QPainter& painter, const Transform3D& transform,
              const QRectF& localBounds, const Artifact3DCamera& camera,
              Handle hoveredHandle, Handle activeHandle);

private:
    // レイとレイヤー平面の交差
    QVector3D intersectLayerPlane(const QPointF& viewportPos,
                                   const Artifact3DCamera& camera,
                                   const Transform3D& layerTransform);
    
    Handle activeHandle_ = Handle::None;
    QVector3D dragStartWorld_;
    float dragStartScaleX_, dragStartScaleY_;
    QVector3D dragStartPosition_;
    QVector3D fixedCorner_;  // resize 時に固定する角のワールド位置
};
```

**Step 2**: コーナードラッグ → scale 変換の実装:

```cpp
ArtifactProjectedFrameGizmo::DragResult
ArtifactProjectedFrameGizmo::updateDrag(
    const QPointF& currentViewportPos,
    const Artifact3DCamera& camera,
    const Transform3D& currentTransform)
{
    QVector3D currentWorld = intersectLayerPlane(currentViewportPos, camera, currentTransform);
    QVector3D delta = currentWorld - dragStartWorld_;
    
    // レイヤーのローカル座標系に変換
    QMatrix4x4 invTransform = currentTransform.inverseMatrix();
    QVector3D localDelta = invTransform.mapVector(delta);
    
    DragResult result;
    result.newPosition = dragStartPosition_;
    result.newScaleX = dragStartScaleX_;
    result.newScaleY = dragStartScaleY_;
    result.newRotation = 0;
    
    switch (activeHandle_) {
    case Handle::CornerTR:
        // 右上コーナー: scaleX と scaleY を同時に変更。左下固定
        result.newScaleX = std::max(1.0f / 1920.0f,
            dragStartScaleX_ + localDelta.x() / localBounds_.width());
        result.newScaleY = std::max(1.0f / 1080.0f,
            dragStartScaleY_ - localDelta.y() / localBounds_.height());
        // fixed corner を維持する位置補正
        result.newPosition = computePositionWithFixedCorner(
            currentTransform, result.newScaleX, result.newScaleY, fixedCorner_);
        break;
    
    case Handle::EdgeRight:
        result.newScaleX = std::max(1.0f / 1920.0f,
            dragStartScaleX_ + localDelta.x() / localBounds_.width());
        break;
    
    case Handle::RotateRing: {
        // レイヤー中心からドラッグ開始点と現在点への角度差
        QVector3D center = currentTransform.position();
        QVector3D startVec = (dragStartWorld_ - center).normalized();
        QVector3D currentVec = (currentWorld - center).normalized();
        result.newRotation = std::atan2(
            QVector3D::crossProduct(startVec, currentVec).length(),
            QVector3D::dotProduct(startVec, currentVec)
        ) * 180.0f / 3.14159265f;
        break;
    }
    
    case Handle::Interior:
        result.newPosition = dragStartPosition_ + delta;
        break;
    }
    
    return result;
}
```

**Step 3**: Controller のマウスハンドラ統合:

```cpp
// CompositionRenderController の mouseMoveEvent 内 (~line 19640)
if (frameGizmo_->isDragging()) {
    auto result = frameGizmo_->updateDrag(event->pos(), currentCamera_, layerTransform);
    layer->transform3D().setScale(time, result.newScaleX, result.newScaleY);
    layer->transform3D().setPosition(time, result.newPosition);
    // undo 用のスナップショット
    updateUndoSnapshot();
}
```

### 1.3 完了条件

- [ ] 3D Frame Gizmo のコーナードラッグでレイヤーの scale が正しく変更される
- [ ] 対角のコーナーが固定されたままアスペクト比を保って拡大縮小
- [ ] エッジハンドル（上下左右中央）のドラッグで一軸拡大縮小
- [ ] RotateRing のドラッグで Z 軸回転
- [ ] 内部ドラッグでレイヤーをレイヤー平面上で移動
- [ ] モード別のハンドル表示フィルタリング（Move=コーナーのみ, Scale=コーナー+エッジ, Rotate=リングのみ）

---

## Phase 2: GPU RayTracing 実ジオメトリ投入

### 2.1 BLAS 構築

`GPURayTracer` の空 BLAS に実際のメッシュジオメトリを投入:

```cpp
// GPURayTracer.cppm の buildBLAS を実装
bool GPURayTracer::buildBLASForLayer(const Artifact3DLayer* layer) {
    const auto& mesh = layer->mesh();
    
    // 1. 頂点バッファとインデックスバッファを GPU にアップロード
    Diligent::BufferDesc vbDesc;
    vbDesc.Name = "BLAS_VertexBuffer";
    vbDesc.Size = mesh.vertexCount() * sizeof(Vertex);
    vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER |
                       Diligent::BIND_RAY_TRACING;
    // ... CreateBuffer
    
    Diligent::BufferDesc ibDesc;
    ibDesc.Name = "BLAS_IndexBuffer";
    ibDesc.Size = mesh.indexCount() * sizeof(uint32_t);
    ibDesc.BindFlags = Diligent::BIND_INDEX_BUFFER |
                       Diligent::BIND_RAY_TRACING;
    
    // 2. BLAS のジオメトリ記述を設定
    Diligent::BLASBuildTriangleData triData;
    triData.GeometryName = layer->name().toStdString();
    triData.pVertexBuffer = vertexBuffer_;
    triData.VertexStride = sizeof(Vertex);
    triData.VertexCount = mesh.vertexCount();
    triData.VertexValueType = Diligent::VT_FLOAT32;
    triData.VertexComponentCount = 3;
    triData.pIndexBuffer = indexBuffer_;
    triData.IndexType = Diligent::VT_UINT32;
    triData.PrimitiveCount = mesh.indexCount() / 3;
    triData.Flags = Diligent::RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    
    // 3. BLAS ビルド
    Diligent::BuildBLASAttribs blasAttribs;
    blasAttribs.pTriangleData = &triData;
    blasAttribs.TriangleDataCount = 1;
    blasAttribs.Flags = Diligent::RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
    
    deviceContext_->BuildBLAS(blasAttribs);
    return true;
}
```

### 2.2 TLAS 構築とインスタンス

全 3D レイヤーの BLAS→TLAS インスタンスを構築:

```cpp
bool GPURayTracer::buildTLAS(const std::vector<Artifact3DLayer*>& layers) {
    std::vector<Diligent::TLASBuildInstanceData> instances;
    
    for (auto* layer : layers) {
        Diligent::TLASBuildInstanceData instance;
        instance.InstanceName = layer->name().toStdString();
        instance.pBLAS = getOrCreateBLAS(layer);  // Phase 2.1
        instance.Transform = layer->transform3D().matrix();
        instance.CustomId = layer->id();
        instance.Mask = 0xFF;
        instance.Flags = Diligent::RAYTRACING_INSTANCE_FLAG_NONE;
        instances.push_back(instance);
    }
    
    // TLAS ビルド
    deviceContext_->BuildTLAS(instances.data(), instances.size(),
                               Diligent::RAYTRACING_BUILD_AS_PREFER_FAST_TRACE |
                               Diligent::RAYTRACING_BUILD_AS_ALLOW_UPDATE);
}
```

### 2.3 レイ生成シェーダーの実装

```hlsl
// RayTrace.hlsl（既存のグラデーション出力を置き換え）
RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> Output : register(u0);

[shader("raygeneration")]
void RayGen() {
    uint2 idx = DispatchRaysIndex().xy;
    float2 uv = (float2(idx) + 0.5) / float2(DispatchRaysDimensions().xy);
    
    RayDesc ray;
    ray.Origin = cameraPosition;
    ray.Direction = normalize(uvToWorldDirection(uv));
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    
    Payload payload;
    payload.hitDistance = -1.0;
    payload.color = float4(0.1, 0.1, 0.15, 1.0); // sky
    
    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    Output[idx] = payload.color;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr) {
    // インスタンスIDからマテリアル情報を取得
    // → PBR 評価 → ライティング
    float3 normal = computeNormal(attr, PrimitiveIndex());
    float3 albedo = getMaterialAlbedo(InstanceID(), PrimitiveIndex());
    
    payload.color = float4(albedo * max(0.0, dot(normal, lightDirection)), 1.0);
    payload.hitDistance = RayTCurrent();
}

[shader("miss")]
void Miss(inout Payload payload) {
    payload.color = float4(0.1, 0.1, 0.15, 1.0);
}
```

### 2.4 完了条件

- [ ] 1つの 3D レイヤー（Cube）が DXR でレイトレースされる
- [ ] ヒットポイントで法線ベースの簡易ライティングが適用される
- [ ] 複数レイヤーの TLAS が正しく構築される（インスタンス変換）
- [ ] レイトレース出力が Composition View の別タブで表示可能
- [ ] カメラ移動でレイトレースビューが追従

---

## Phase 3: 3D 照明パイプライン接続 + Ground Grid 描画

### 3.1 照明データのレンダーパイプラインへの配信

```cpp
// CompositionRenderController のレンダリングループに追加
void collectActiveLights(
    const Composition& comp,
    std::vector<LightUniform>& lights)
{
    for (auto& layer : comp.layers()) {
        if (auto* lightLayer = dynamic_cast<ArtifactLightLayer*>(layer.get())) {
            LightUniform light;
            light.type = static_cast<int>(lightLayer->lightType());
            light.color = lightLayer->color();
            light.intensity = lightLayer->intensity() / 100.0f;
            light.position = lightLayer->transform3D().position();
            light.direction = lightLayer->transform3D().forward();
            light.range = lightLayer->range();
            light.coneAngle = lightLayer->coneAngle();
            light.castsShadows = lightLayer->castsShadows();
            lights.push_back(light);
        }
    }
    
    // 定数バッファとして全シェーダーにバインド
    updateLightBuffer(lights);
}
```

### 3.2 Ground Grid 3D 描画

`computeGroundGridLines()` を呼び出し、結果を `draw3DLine()` でビューポートに描画:

```cpp
// CompositionRenderer の draw パイプラインに追加
void drawGroundGrid(Diligent::IDeviceContext* ctx, const Camera& camera) {
    auto lines = computeGroundGridLines(camera.position(), camera.fov());
    
    for (auto& line : lines) {
        // distance-based alpha fade
        float alpha = line.fadeAlpha;
        QColor color = line.isMajor ? majorGridColor : minorGridColor;
        color.setAlphaF(alpha);
        
        // 3D 空間の直線を描画
        draw3DLine(line.start, line.end, color, 1.0f);
    }
    
    // 原点に軸表示
    draw3DLine(QVector3D(-1, 0, 0), QVector3D(1, 0, 0), Qt::red, 2.0f);    // X
    draw3DLine(QVector3D(0, 0, -1), QVector3D(0, 0, 1), Qt::green, 2.0f);  // Z
}
```

### 3.3 完了条件

- [ ] Point / Spot / Parallel 光源が実際の 3D レイヤーに照明効果を与える
- [ ] Ground Grid がビューポートの床面に描画される
- [ ] カメラ移動に応じて Ground Grid が無限遠まで追従（カメラ相対再配置）
- [ ] Grid toggle がツールバーから操作可能

---

## Phase 4: Volume Rendering GPU 化 + DOF

### 4.1 Volume Rendering GPU パス

既存 CPU `CPUVolumeRenderer` のレイマーチングロジックを HLSL compute shader に移植:

```hlsl
// VolumeRenderCS.hlsl
RWTexture2D<float4> output : register(u0);
Texture3D<float> densityField : register(t1);
Texture3D<float> temperatureField : register(t2);

cbuffer VolumeParams : register(b0) {
    float4x4 invViewProj;
    float3 cameraPos;
    float stepSize;
    float3 volumeMin;
    float volumeScale;
    float4 absorptionColor;
    float4 emissionColor;
    float scattering;
    int maxSteps;
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    float2 uv = (float2(tid.xy) + 0.5) / float2(outputDims);
    
    // レイのワールド空間での開始点と方向を計算
    float3 rayOrigin, rayDirection;
    computeRayFromUV(uv, invViewProj, rayOrigin, rayDirection);
    
    // AABB交差
    float tNear, tFar;
    if (!intersectAABB(rayOrigin, rayDirection, volumeMin, volumeMin + volumeScale, tNear, tFar)) {
        output[tid.xy] = float4(0, 0, 0, 1);
        return;
    }
    
    // レイマーチング
    float4 color = float4(0, 0, 0, 0);
    float t = tNear;
    
    for (int i = 0; i < maxSteps && t < tFar; i++) {
        float3 pos = rayOrigin + rayDirection * t;
        float3 uvVolume = (pos - volumeMin) / volumeScale;
        
        float density = densityField.SampleLevel(linearSampler, uvVolume, 0);
        float temp = temperatureField.SampleLevel(linearSampler, uvVolume, 0);
        
        if (density > 0.001) {
            float4 srcColor = lerp(absorptionColor, emissionColor, temp);
            color.rgb += (1.0 - color.a) * srcColor.rgb * srcColor.a * density * scattering;
            color.a += (1.0 - color.a) * density;
        }
        
        t += stepSize;
        if (color.a > 0.99) break;
    }
    
    output[tid.xy] = color;
}
```

### 4.2 DOF 有効化

Wicked Engine の DOF シェーダー資産（8 compute shader）を Diligent 統合:

1. 深度バッファから `R32_FLOAT` SRV を作成（現在は `BIND_DEPTH_STENCIL` のみ）
2. `depthoffield_prepassCS` + `depthoffield_mainCS`（cheap variant）を Diligent compute に移植
3. カメラパラメータ（aperture, focusDistance, blurAmount）→ CoC ユニフォーム
4. Compute chain dispatch: prepass → main（cheap）→ output

### 4.3 完了条件

- [ ] Volume データ（Pyro 等）が GPU レイマーチングでビューポートに表示
- [ ] 透過・吸収・発光の各モードが正しくレンダリング
- [ ] DOF の cheap パスが動作し、フォーカス外領域にボケが適用される
- [ ] カメラの aperture / focusDistance 変更が DOF に即時反映

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | 新規 `Artifact/include/Widgets/Render/ArtifactProjectedFrameGizmo.ixx` | Frame Gizmo クラス |
| P1 | 新規 `Artifact/src/Widgets/Render/ArtifactProjectedFrameGizmo.cppm` | 交差・ドラッグ変換実装 |
| P1 | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | Frame Gizmo ハンドラ統合 |
| P1 | `Artifact/include/UI/GizmoMode.ixx` | 空→enum定義 |
| P2 | `ArtifactCore/src/Render/GPURayTracer.cppm` | BLAS ジオメトリ投入 + TLAS 構築 |
| P2 | `ArtifactCore/src/Render/Shaders/RayTrace.hlsl` | ray-gen/closest-hit/miss 実装 |
| P3 | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 照明データ収集 + Ground Grid 描画 |
| P4 | 新規 `Artifact/shaders/volumeRenderCS.hlsl` | Volume レイマーチング |
| P4 | `ArtifactCore/src/Render/VolumeRenderer.cppm` | GPU パス追加 |
| P4 | 新規 `Artifact/src/Render/DepthOfFieldPass.cppm` | DOF compute chain |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: Frame Gizmo | **P0** | 中 | 3D 操作の基本。根本原因は特定済みで修正範囲限定 |
| P3: 照明+Grid | **P1** | 小 | データモデルは完了。配線だけ |
| P2: GPU RayTracing | **P2** | 大 | DXR シェーダー実装が主。優先度は中だがインパクト大 |
| P4: Volume+DOF | **P2** | 大 | シェーダー資産は存在。Diligent 統合が主作業 |
