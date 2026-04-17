# ArtifactIRenderer インターフェース & 実装 分析レポート
**作成日**: 2026-04-17  
**対象プロジェクト**: Artifact  
**対象モジュール**: Artifact.Render

---

## 📋 1. インターフェース定義

### 宣言ファイル
```
Artifact/include/Rend/ArtifactIRenderer.ixx:55
```

### クラス定義
```cpp
export class ArtifactIRenderer {
public:
    class Impl;  // Pimpl イディオム

    ArtifactIRenderer();
    explicit ArtifactIRenderer(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice,
                               Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext,
                               QWidget* widget);
    ~ArtifactIRenderer();

    // 初期化・破棄
    void initialize(QWidget* widget);
    void initializeHeadless(int width, int height);
    void createSwapChain(QWidget* widget);
    void recreateSwapChain(QWidget* widget);
    void destroy();
    void present();

    // クリア・フラッシュ
    void clear();
    void flush();
    void flushAndWait();

    // 状態確認
    bool isInitialized() const;
    bool hasSwapChain() const;

    // リードバック
    QImage readbackToImage() const;
    using ReadbackCallback = std::function<void(const QImage&)>;
    void readbackToImageAsync(ReadbackCallback callback) const;

    // ビューポート・カメラ制御
    void setViewportSize(float w, float h);
    void setCanvasSize(float w, float h);
    void setPan(float x, float y);
    void getPan(float& x, float& y) const;
    void setZoom(float zoom);
    float getZoom() const;
    void panBy(float dx, float dy);
    void resetView();
    void fitToViewport(float margin = 50.0f);
    void fillToViewport(float margin = 0.0f);
    void setViewMatrix(const QMatrix4x4& view);
    void setProjectionMatrix(const QMatrix4x4& proj);
    void setUseExternalMatrices(bool use);
    void setGizmoCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj);
    void resetGizmoCameraMatrices();
    void set3DCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj);
    void reset3DCameraMatrices();
    QMatrix4x4 getViewMatrix() const;
    QMatrix4x4 getProjectionMatrix() const;
    void zoomAroundViewportPoint(Detail::float2 viewportPos, float newZoom);
    Detail::float2 canvasToViewport(Detail::float2 pos) const;
    Detail::float2 viewportToCanvas(Detail::float2 pos) const;

    // LOD (Level of Detail)
    void setDetailLevel(LODManager::DetailLevel lod);
    LODManager::DetailLevel detailLevel() const;

    // 2D描画プリミティブ
    void drawRectOutline(float x, float y, float w, float h, const FloatColor& color);
    void drawRectOutline(Detail::float2 pos, Detail::float2 size, const FloatColor& color);
    void drawSolidLine(Detail::float2 start, Detail::float2 end, const FloatColor& color, float thickness);
    void drawPolyline(const std::vector<Detail::float2>& points, const FloatColor& color, float thickness);
    void drawQuadLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, Detail::float2 p3, const FloatColor& color);
    void drawSolidRect(float x, float y, float w, float h);
    void drawSolidRect(float x, float y, float w, float h, const FloatColor& color, float opacity = 1.0f);
    void drawSolidRect(Detail::float2 pos, Detail::float2 size, const FloatColor& color, float opacity = 1.0f);
    void drawPoint(float x, float y, float size, const FloatColor& color);
    void drawSolidRectTransformed(float x, float y, float w, float h, const QTransform& transform, const FloatColor& color, float opacity = 1.0f);
    void drawSolidRectTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const FloatColor& color, float opacity = 1.0f);
    void drawRectLocal(float x, float y, float w, float h, const FloatColor& color, float opacity = 1.0f);
    void drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color);
    void drawThickLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color);
    void drawDotLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, float spacing, const FloatColor& color);
    void drawDashedLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, float dashLength, float gapLength, const FloatColor& color);
    void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color);
    void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, Detail::float2 p3, float thickness, const FloatColor& color);
    void drawSolidTriangleLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, const FloatColor& color);
    void drawCircle(float x, float y, float radius, const FloatColor& color, float thickness = 1.0f, bool fill = false);
    void drawCrosshair(float x, float y, float size, const FloatColor& color);
    void drawCheckerboard(float x, float y, float w, float h, float tileSize, const FloatColor& c1, const FloatColor& c2);
    void drawGrid(float x, float y, float w, float h, float spacing, float thickness, const FloatColor& color);

    // スプライト・テクスチャ描画
    void drawSprite(float x, float y, float w, float h);
    void drawSprite(Detail::float2 pos, Detail::float2 size);
    void drawSprite(float x, float y, float w, float h, Diligent::ITextureView* pSRV, float opacity = 1.0f);
    void drawSprite(float x, float y, float w, float h, const QImage& image, float opacity = 1.0f);
    void drawSpriteTransformed(float x, float y, float w, float h, const QTransform& transform, const QImage& image, float opacity = 1.0f);
    void drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const QImage& image, float opacity = 1.0f);
    void drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, Diligent::ITextureView* texture, float opacity = 1.0f);
    void drawMaskedTextureLocal(float x, float y, float w, float h, Diligent::ITextureView* sceneTexture, const QImage& maskImage, float opacity = 1.0f);

    // パーティクル描画
    void drawParticles(const ArtifactCore::ParticleRenderData& data);

    // 3Dギズモ描画 (ImGuizmo 等で使用)
    void drawGizmoLine(Detail::float3 start, Detail::float3 end, const FloatColor& color, float thickness = 1.0f);
    void drawGizmoArrow(Detail::float3 start, Detail::float3 end, const FloatColor& color, float size = 1.0f);
    void drawGizmoRing(Detail::float3 center, Detail::float3 normal, float radius, const FloatColor& color, float thickness = 1.0f);
    void drawGizmoTorus(Detail::float3 center, Detail::float3 normal, float majorRadius, float minorRadius, const FloatColor& color);
    void drawGizmoCube(Detail::float3 center, float halfExtent, const FloatColor& color);
    void flushGizmo3D();
    void draw3DLine(Detail::float3 start, Detail::float3 end, const FloatColor& color, float thickness = 1.0f);
    void draw3DArrow(Detail::float3 start, Detail::float3 end, const FloatColor& color, float size = 1.0f);
    void draw3DCircle(Detail::float3 center, Detail::float3 normal, float radius, const FloatColor& color, float thickness = 1.0f);
    void draw3DQuad(Detail::float3 v0, Detail::float3 v1, Detail::float3 v2, Detail::float3 v3, const FloatColor& color);

    // オフスクリーンレンダリング
    void* createOffscreenTexture(int width, int height);
    void destroyOffscreenTexture(void* textureView);
    void pushRenderTarget(void* textureView);
    void popRenderTarget();
    void clearRenderTarget(const FloatColor& color);
    void drawOffscreenTexture(void* textureView, const QRectF& bounds, float opacity = 1.0f);

    // アップスケール設定
    void setUpscaleConfig(bool enable, float sharpness);

    // ライティング
    void setSceneLights(const std::vector<ArtifactCore::Light>& lights);
    const std::vector<ArtifactCore::Light>& getSceneLights() const;

    // DiligentEngine アクセサ
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device() const;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> immediateContext() const;
    Diligent::ITextureView* layerTextureView() const;
    Diligent::ITextureView* layerRenderTargetView() const;
    ArtifactCore::IRayTracingManager* rayTracingManager() const;
    void setOverrideRTV(Diligent::ITextureView* rtv);

private:
    std::unique_ptr<Impl> impl_;
};
```

---

## 📋 2. 実装クラス

### ArtifactIRenderer::Impl
**ファイル**: `Artifact/src/Render/ArtifactIRenderer.cppm:65`

```cpp
class ArtifactIRenderer::Impl {
public:
    // 依存コンポーネント
    DiligentDeviceManager     deviceManager_;
    ShaderManager             shaderManager_;
    PrimitiveRenderer2D       primitiveRenderer_;
    PrimitiveRenderer3D       primitiveRenderer3D_;
    std::unique_ptr<ArtifactCore::IRayTracingManager> rayTracingManager_;
    std::unique_ptr<ArtifactCore::GpuContext>         gpuContext_;
    std::unique_ptr<ArtifactCore::ParticleRenderer>   particleRenderer_;

    mutable DiligentImmediateSubmitter submitter_;
    mutable RenderCommandBuffer        cmdBuf_;

    // レンダーターゲット
    RefCntAutoPtr<ITexture> m_layerRT;
    Uint32 m_layerRTWidth = 0;
    Uint32 m_layerRTHeight = 0;

    // リードバック用ステージング
    mutable RefCntAutoPtr<ITexture>  m_readbackStagingTex;
    mutable TEXTURE_FORMAT           m_readbackStagingFormat = TEX_FORMAT_UNKNOWN;
    mutable RefCntAutoPtr<IFence>    m_readbackFence;
    mutable Uint32                   m_readbackStagingWidth = 0;
    mutable Uint32                   m_readbackStagingHeight = 0;
    mutable Uint64                   m_readbackFenceValue = 0;
    mutable std::mutex               m_readbackMutex;

    QWidget* widget_ = nullptr;

    // プロファイリング
    bool m_initialized = false;
    bool m_frameQueryInitialized = false;
    double m_lastGpuFrameTimeMs = 0.0;
    Uint32 m_frameQueryIndex = 0;
    static constexpr Uint32 FrameQueryCount = 2;
    std::array<RefCntAutoPtr<IQuery>, FrameQueryCount> m_frameQueries;

    // ヘッドレスモード
    int m_offlineWidth  = 0;
    int m_offlineHeight = 0;

    // ビュー制御
    float m_viewportWidth  = 0.0f;
    float m_viewportHeight = 0.0f;

    // 描画状態
    FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };
    std::vector<ArtifactCore::Light> m_sceneLights;
    LODManager::DetailLevel detailLevel_ = LODManager::DetailLevel::High;

    // ...
};
```

---

## 📋 3. 主要メソッド一覧

### 初期化・ライフサイクル
| メソッド | ファイル位置 | 責務 |
|---------|-------------|------|
| `ArtifactIRenderer()` | ArtifactIRenderer.cppm:921 | デフォルトコンストラクタ |
| `ArtifactIRenderer(device, context, widget)` | ArtifactIRenderer.cppm:914 | Diligentデバイス指定コンストラクタ |
| `initialize(widget)` | ArtifactIRenderer.cppm:928 / Impl::294 | ウィジェットベース初期化 |
| `initializeHeadless(w, h)` | ArtifactIRenderer.cppm:929 / Impl::347 | ヘッドレス（非ウィンドウ）初期化 |
| `createSwapChain(widget)` | ArtifactIRenderer.cppm:930 / Impl::719 | スワップチェーン作成 |
| `recreateSwapChain(widget)` | ArtifactIRenderer.cppm:931 / Impl::747 | スワップチェーン再作成（リサイズ等） |
| `destroy()` | ArtifactIRenderer.cppm:936 / Impl::853 | 全リソース解放 |

### レンダリング制御
| メソッド | ファイル位置 | 備考 |
|---------|-------------|------|
| `clear()` | ArtifactIRenderer.cppm:933 / Impl::820 | クリアカラーでバッファクリア |
| `flush()` | ArtifactIRenderer.cppm:934 / Impl::830 | GPUコマンドフラッシュ（非同期） |
| `flushAndWait()` | ArtifactIRenderer.cppm:935 / Impl::836 | GPU完了待機（同期） |
| `present()` | ArtifactIRenderer.cppm:945 / Impl:874 | スワップチェーンPresent + 損失時再作成 |

### ビューポート・変換
| メソッド | ファイル位置 | 説明 |
|---------|-------------|------|
| `setViewportSize(w, h)` | Impl::136 | ビューポートサイズ設定 |
| `setCanvasSize(w, h)` | Impl::137 | キャンバスサイズ設定 |
| `setPan(x, y)` / `getPan()` | Impl::138-139 | パン位置設定/取得 |
| `setZoom(zoom)` / `getZoom()` | Impl::140-141 | ズームレベル設定/取得 |
| `panBy(dx, dy)` | Impl::142 | 相対パン移動 |
| `resetView()` | Impl::143 | ビューリセット |
| `fitToViewport(margin)` | Impl::144 | コンテンツをビューポートに合わせる |
| `fillToViewport(margin)` | Impl::149 | コンテンツでビューポートを埋める |
| `setViewMatrix(view)` | Impl::150 | ビュー行列設定（2D+3D） |
| `setProjectionMatrix(proj)` | Impl::151 | 射影行列設定（2D+3D） |
| `zoomAroundViewportPoint(pos, newZoom)` | ArtifactIRenderer.cppm:977 / Impl::157 | 指定ポイント中心でズーム |

### リードバック
| メソッド | ファイル位置 | 説明 |
|---------|-------------|------|
| `readbackToImage()` | ArtifactIRenderer.cppm:940 / Impl::401 | 同期Readback（QImage取得） |
| `readbackToImageAsync(callback)` | ArtifactIRenderer.cppm:941 / Impl::544 | 非同期Readback（バックグラウンドスレッド） |

### 2D描画
| メソッド | 実体 | 説明 |
|---------|------|------|
| `drawSolidRect()` | PrimitiveRenderer2D::drawRectLocal | 塗りつぶし矩形 |
| `drawRectOutline()` | PrimitiveRenderer2D::drawRectOutlineLocal | 矩形アウトライン |
| `drawSolidLine()` | PrimitiveRenderer2D::drawThickLineLocal | 太線 |
| `drawPolyline()` | Impl::181-186（forループ） | ポリライン（線の連続） |
| `drawThickLineLocal()` | PrimitiveRenderer2D::drawThickLineLocal | 厚い線 |
| `drawDotLineLocal()` | PrimitiveRenderer2D::drawDotLineLocal | 点線 |
| `drawDashedLineLocal()` | PrimitiveRenderer2D::drawDashedLineLocal | 破線 |
| `drawBezierLocal()` | PrimitiveRenderer2D::drawBezierLocal | ベジェ曲線（2次/3次） |
| `drawSolidTriangleLocal()` | PrimitiveRenderer2D::drawSolidTriangleLocal | 塗りつぶし三角形 |
| `drawCircle()` | PrimitiveRenderer2D::drawCircle | 円（塗り/線） |
| `drawCrosshair()` | PrimitiveRenderer2D::drawCrosshair | 十字カーソル |
| `drawCheckerboard()` | PrimitiveRenderer2D::drawCheckerboard | チェックボード |
| `drawGrid()` | PrimitiveRenderer2D::drawGrid | グリッド |
| `drawPoint()` | PrimitiveRenderer2D::drawPoint | 点 |

### スプライト・テクスチャ
| メソッド | 実体 | 説明 |
|---------|------|------|
| `drawSprite()` | PrimitiveRenderer2D::drawSpriteLocal | スプライト（QImage） |
| `drawSprite(pos, size)` | PrimitiveRenderer2D::drawSpriteLocal | スプライト（位置・サイズ） |
| `drawSprite(x, y, w, h, pSRV)` | PrimitiveRenderer2D::drawTextureLocal | スプライト（ITextureView） |
| `drawSprite(x, y, w, h, QImage)` | PrimitiveRenderer2D::drawSpriteLocal | スプライト（QImage版） |
| `drawSpriteTransformed()` | PrimitiveRenderer2D::drawSpriteTransformed | 変換（QTransform/QMatrix4x4/QImage/Texture） |
| `drawMaskedTextureLocal()` | PrimitiveRenderer2D::drawMaskedTextureLocal | マスク付きテクスチャ |

### 3Dギズモ
| メソッド | 実体 | 説明 |
|---------|------|------|
| `drawGizmoLine()` | PrimitiveRenderer3D::draw3DLine | 3Dライン（ワールド座標） |
| `drawGizmoArrow()` | PrimitiveRenderer3D::draw3DArrow | 3D矢印 |
| `drawGizmoRing()` | PrimitiveRenderer3D::draw3DCircle | 3D円環 |
| `drawGizmoTorus()` | PrimitiveRenderer3D::draw3DTorus | 3Dトーラス |
| `drawGizmoCube()` | PrimitiveRenderer3D::draw3DCube | 3D立方体 |
| `draw3DQuad()` | PrimitiveRenderer3D::draw3DQuad | 3Dクワッド |
| `flushGizmo3D()` | PrimitiveRenderer3D::flushGizmo3D | 3Dジオメトリバッチ送信 |

### パーティクル
| メソッド | ファイル位置 | 説明 |
|---------|-------------|------|
| `drawParticles()` | ArtifactIRenderer.cppm:222 / Impl::222 | パーティクル描画（遅延初期化） |

### オフスクリーン
| メソッド | ファイル位置 | 説明 |
|---------|-------------|------|
| `createOffscreenTexture()` | ArtifactIRenderer.cppm:166 | オフスクリーンテクスチャ作成 |
| `destroyOffscreenTexture()` | ArtifactIRenderer.cppm:167 | オフスクリーンテクスチャ破棄 |
| `pushRenderTarget()` | ArtifactIRenderer.cppm:168 | RTVをオフスクリーンに切り替え |
| `popRenderTarget()` | ArtifactIRenderer.cppm:169 | RTVを戻す |
| `clearRenderTarget()` | ArtifactIRenderer.cppm:170 | オフスクリーンクリア |
| `drawOffscreenTexture()` | ArtifactIRenderer.cppm:171 | オフスクリーンテクスチャ描画 |

### アクセサ
| メソッド | 説明 |
|---------|------|
| `device()` | IRenderDevice 取得 |
| `immediateContext()` | IDeviceContext 取得 |
| `layerTextureView()` | レイヤーテクスチャSRV取得 |
| `layerRenderTargetView()` | レイヤーテクスチャRTV取得 |
| `rayTracingManager()` | IRayTracingManager 取得 |
| `setOverrideRTV()` | 強制RTV上書き（ヘッドレス等） |

---

## 📋 4. DiligentEngine 統合

### DiligentEngine の役割
`ArtifactIRenderer` は **DiligentEngine の高レベルラッパー** として機能します。

```
DiligentEngine 抽象化レイヤー
├─ バックエンド: Direct3D12 (Windows)
├─ バックエンド: Vulkan (Cross-platform)
└─ 共通インターフェース: IRenderDevice, IDeviceContext, ISwapChain, ITexture, IBuffer, ...
```

### 統合ポイント

#### 1. **DiligentDeviceManager** (デバイス管理の単一責任)
```cpp
// ファイル: Artifact/include/Render/DiligentDeviceManager.ixx
class DiligentDeviceManager {
    void initialize(QWidget* widget);           // D3D12/Vulkan 初期化
    void initializeHeadless();                  // ヘッドレス初期化
    void createSwapChain(QWidget* widget);       // スワップチェーン作成
    RefCntAutoPtr<IRenderDevice> device() const; // デバイス取得
    RefCntAutoPtr<IDeviceContext> immediateContext() const; // 即時コンテキスト
    RefCntAutoPtr<ISwapChain> swapChain() const; // スワップチェーン
    bool isRayTracingSupported() const;         // レイトレーシング対応確認
};
```
- **バックエンド選択**: `acquireSharedRenderDeviceForCurrentBackend()` で D3D12/Vulkan を選択
- **デバイス共有**: アプリケーション全体で単一の `IRenderDevice` を共有
- **コンテキスト管理**: 即時コンテキスト（Immediate Context）と遅延コンテキスト（Deferred Context）

#### 2. **ShaderManager** (シェーダ & PSO 管理)
```cpp
class ShaderManager {
    void initialize(IRenderDevice* device, TEXTURE_FORMAT rtvFormat);
    void createShaders();  // 頂点/ピクセルシェーダコンパイル
    void createPSOs();     // Pipeline State Object 作成
    RenderShaderPair lineShaders() const;
    PSOAndSRB solidRectPsoAndSrb() const;
    // ...
};
```
- **シェーダ**: HLSL で記述、Diligent のシェーダコンパイラを通じてバイトコード生成
- **PSO**: 各プリミティブタイプごとに PSO（Pipeline State Object）を事前作成
- **SRB**: Shader Resource Binding（定数バッファ、テクスチャバインド）

#### 3. **DiligentImmediateSubmitter** (コマンド送信)
```cpp
class DiligentImmediateSubmitter : public IRenderSubmitter {
    void submit(RenderCommandBuffer& buf, IDeviceContext* ctx) override;
    // 内部: バーテックスバッファ/インデックスバッファ/定数バッファバインド
    //       各 Draw コールを IDeviceContext::DrawIndexed() で実行
};
```
- **バッファ管理**: 頂点/インデックス/定数バッファを事前作成
- **PSO/SRB キャッシュ**: `ShaderManager` から取得した PSO/SRB を保持
- **パケット解釈**: `RenderCommandBuffer` の `std::variant<DrawPacket>` を型安全に解釈して送信

#### 4. **リードバック処理**
```cpp
QImage ArtifactIRenderer::Impl::readbackToImage() const {
    // 1. ソーステクスチャ優先順位: m_layerRT > swapChain back buffer
    // 2. ステージングテクスチャ作成 (CPU_ACCESS_READ, USAGE_STAGING)
    // 3. CopyTexture で GPU→CPU 転送
    // 4. Fence で GPU 終了待機
    // 5. MapTextureSubresource で CPU メモリマップ
    // 6. フォーマット変換: TEX_FORMAT_RGBA16_FLOAT → QImage::Format_RGBA8888
    //    (half-float → 8-bit sRGB、ガンマ補正 apply)
}
```
- **非同期版**: `QtConcurrent::run` でバックグラウンドスレッドで fence wait + 変換
- **フォーマット**: HDR パス（RGBA16_FLOAT）と LDR パス（RGBA8_UNORM）を自動判別

---

## 📋 5. 責務範囲

### ArtifactIRenderer の責務
| カテゴリ | 内容 |
|---------|------|
| **GPU レンダリング** | DiligentEngine 経由のハードウェアアクセラレーション（D3D12/Vulkan） |
| **ソフトウェアレンダリング** | 提供しない。DiligentEngine が GPU 必須のため |
| **レンダリング抽象化** | バックエンド（D3D12/Vulkan）の差異を隱蔽した統一API |
| **2Dプリミティブ** | 矩形、線、円、スプライト、テクスチャ等の2D描画 |
| **3Dプリミティブ（ギズモ）** | 3Dライン、矢印、円環、トーラス、立方体（ワールド座標系） |
| **パーティクル** | GPU パーティクル描画（`ParticleRenderer` 委譲） |
| **レイトレーシング** | `IRayTracingManager` 経由で RT API 提供（未実装或いはスタブ） |
| **オフスクリーン** | レイヤーごとのRenderTarget管理、マスク合成等 |
| **ビューポート制御** | パン、ズーム、ビュー/射影行列管理 |
| **LOD** | LODManager 連携による詳細レベル制御 |
| **リードバック** | GPU→CPU テクスチャreadback（同期/非同期） |
| **リソース管理** | レンダーターゲットテクスチャの作成・破棄 |
| **プロファイリング** | GPU フレーム時間計測（Query 使用） |

### ソフトウェアレンダリングについて
**`ArtifactIRenderer` はソフトウェアレンダリングを提供しません**。  
- DiligentEngine は GPU API 抽象化レイヤーのため、CPUのみのフォールバックは非対応
- 過去の `QImage` ベースのソフトウェアレンダリング（Videoレイヤー等）は別システム (`Render.Composition` 内) で継続
- ヘッドレスモード (`initializeHeadless`) も GPU 必須（Offscreen RTV 作成）

---

## 📋 6. 依存関係マップ

### ArtifactIRenderer の依存関係

```mermaid
graph TD
    A[ArtifactIRenderer] --> B[Impl (Pimpl)]

    B --> C[DiligentDeviceManager]
    B --> D[ShaderManager]
    B --> E[PrimitiveRenderer2D]
    B --> F[PrimitiveRenderer3D]
    B --> G[DiligentImmediateSubmitter]
    B --> H[RenderCommandBuffer]
    B --> I[IRayTracingManager]
    B --> J[ParticleRenderer]
    B --> K[GpuContext]

    C --> L[DiligentEngine<br/>IRenderDevice, IDeviceContext, ISwapChain]
    D --> L
    E --> L
    F --> L
    G --> L

    M[CompositionRenderController] --> A
    N[ArtifactSoftwareRenderTestWidget] --> A
    O[ArtifactPreviewCompositionPipeline] --> A
```

### 直接依存（Impl メンバー変数）
| 依存先クラス | 依存ファイル | 責務 |
|------------|------------|------|
| `DiligentDeviceManager` | `include/Render/DiligentDeviceManager.ixx` | Diligent デバイス/コンテキスト/スワップチェーン管理 |
| `ShaderManager` | `include/Render/ShaderManager.ixx` | シェーダコンパイル & PSO 作成 |
| `PrimitiveRenderer2D` | `include/Render/PrimitiveRenderer2D.ixx` | 2D描画実装（矩形、線、スプライト等） |
| `PrimitiveRenderer3D` | `include/Render/PrimitiveRenderer3D.ixx` | 3Dギズモ描画実装 |
| `DiligentImmediateSubmitter` | `include/Render/DiligentImmediateSubmitter.ixx` | コマンドバッファ→GPU送信 |
| `RenderCommandBuffer` | `include/Render/RenderCommandBuffer.ixx` | 描画パケットキュー |
| `IRayTracingManager` | `Graphics/RayTracingManager` (ArtifactCore) | レイトレーシングAPI |
| `ParticleRenderer` | `Graphics/ParticleRenderer` (ArtifactCore) | GPUパーティクル |
| `GpuContext` | `Core/GpuContext` (ArtifactCore) | GPU共通コンテキスト |

### 間接受入（インポートモジュール）
| インポートモジュール | 提供元 | 内容 |
|-------------------|--------|------|
| `Graphics` | ArtifactCore | `FloatColor`, `ImageF32x4`, 等 |
| `Color.Float` | Artifact | `FloatColor` 定義 |
| `Artifact.Render.DiligentDeviceManager` | Artifact | 上記 |
| `Artifact.Render.ShaderManager` | Artifact | 上記 |
| `Artifact.Render.PrimitiveRenderer2D` | Artifact | 上記 |
| `Artifact.Render.PrimitiveRenderer3D` | Artifact | 上記 |
| `Artifact.Render.Config` | Artifact | `RenderConfig::MainRTVFormat` |
| `Artifact.LOD.Manager` | Artifact | LOD 制御 |
| `std` | C++標準 | `vector`, `unique_ptr`, `function`, `mutex` |
| `QImage`, `QTransform`, `QMatrix4x4` | Qt | 画像・変換行列 |
| `DiligentCore/...` | DiligentEngine | `IRenderDevice`, `IDeviceContext`, `Texture`, `Buffer`, `Query`, `Fence`, 等 |

### DiligentEngine サブモジュール依存
`libs/DiligentEngine/` 内の以下のヘッダーを直接インクルード：
- `Graphics/GraphicsEngine/interface/RenderDevice.h`
- `Graphics/GraphicsEngine/interface/DeviceContext.h`
- `Graphics/GraphicsEngine/interface/Query.h`
- `Graphics/GraphicsEngine/interface/SwapChain.h`
- `Graphics/GraphicsEngine/interface/Texture.h`
- `Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h` (Windows)
- `Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h` (Vulkan)
- `Common/interface/RefCntAutoPtr.hpp` (スマートポインタ)
- `Common/interface/BasicMath.hpp` (float2, float3, float4)
- `Common/interface/Float16.hpp` (half-float変換)

---

## 📋 7. レンダリングアーキテクチャ概要

```
[Qt Widget] 
    ↓ (host HWND)
[CompositionRenderController] -- uses --> [ArtifactIRenderer]
                                         ↓
                              [ArtifactIRenderer::Impl]
                                         ↓
                    ┌────────────────────┼────────────────────┐
                    ↓                    ↓                    ↓
         [DiligentDeviceManager] [ShaderManager]  [PrimitiveRenderer2D]
                    ↓                    ↓                    ↓
         [DiligentEngine]      [PSO/SRVキャッシュ]      [2Dバッチ処理]
                    ↓                    ↓                    ↓
         [D3D12/Vulkan]        [シェーダバイナリ]       [頂点バッファ]
                                                       ↓
                                         [DiligentImmediateSubmitter]
                                                       ↓
                                         [RenderCommandBuffer]
                                                       ↓
                                         [GPU Command Queue]
```

### 描画フロー（1フレーム）
1. **CompositionRenderController::renderOneFrame()** 呼び出し
2. **ArtifactIRenderer::clear()** → `PrimitiveRenderer2D::clear()` で RTV クリア
3. **各種 drawXXX()** → `PrimitiveRenderer2D::drawXXXLocal()` で `RenderCommandBuffer` にパケット追加
4. **3Dギズモ drawXXX()** → `PrimitiveRenderer3D::draw3DXXX()` でジオメトリ蓄積
5. **flushGizmo3D()** → `PrimitiveRenderer3D::flushGizmo3D()` で3Dバッチ送信
6. **ArtifactIRenderer::present()**
   - `DiligentImmediateSubmitter::submit(cmdBuf_, ctx)` で全パケット実行
   - `swapChain->Present()` で画面反映
7. **GPUプロファイリング**: Query でフレーム時間計測

---

## 📋 8. バックエンド実装の現状

### 現在の実装
- **`ArtifactIRenderer` 自体は単一実装**（Pimpl で DiligentEngine ラップ）
- **ソフトウェアレンダリングクラス**: 存在しない
  - `ArtifactSoftwareRender` というクラスは見つからない
  - `ArtifactSoftwareRenderTestWidget` はテスト用ウィジェット（Qt 描画）
- **OpenGL レンダラ**: 存在しない。DiligentEngine が D3D12/Vulkan のみサポート
- `ArtifactCompositionRenderController` は `ArtifactIRenderer` **インスタンスを所有**し、Composition Editor の描画を統御

### バックエンド選択メカニズム
```cpp
// DiligentDeviceManager 内部（Impl は .cpp 内に定義）
bool acquireSharedRenderDeviceForCurrentBackend(
    RefCntAutoPtr<IRenderDevice>& outDevice,
    RefCntAutoPtr<IDeviceContext>& outImmediateContext);
```
- **Windows**: `EngineFactoryD3D12::CreateDevice()` で D3D12 デバイス作成
- **Linux/macOS**: `EngineFactoryVk::CreateDevice()` で Vulkan デバイス作成
- **選択ロジック**: ビルド時設定 or 実行時 OS 検出（詳細は実装ファイル要確認）

---

## 📋 9. 注意点・設計思想

### 設計原則（コメントより引用）
```cpp
// ArtifactIRenderer.ixx:3-6
// ArtifactIRenderer maintenance rule:
// Do not rewrite the existing D3D12-specific path by guesswork.
// Do not replace this renderer with a Qt-only implementation.
// Extend backends carefully while preserving the current Diligent/D3D12 architecture.
```

### Pimpl イディオム採用理由
- **ABI 安定性**: 実装変更時にヘッダー再コンパイル防​​止
- **カプセル化**: DiligentEngine 依存を .cpp 内に閉じ込め
- **コンパイル時間短縮**: ヘッダー依存削減

### スレッド安全性
- `readbackToImageAsync()` は `QtConcurrent::run` でバックグラウンド実行
- `m_readbackMutex` でリードバックリソース保護
- 描画自体は単一スレッド（Qt GUI スレ）前提

### GPU/CPU 同期
- **Fence 使用**: ` flushAndWait()`, `readbackToImage()` で GPU 完了待機
- **Query 使用**: GPU フレーム時間計測に `IQuery` (QUERY_TYPE_DURATION)
- **マップ時**: `MAP_FLAG_DO_NOT_WAIT` で待機排除（fence で確実に終了済み保証）

### レンダリングモード
| モード | 初期化関数 | レンダーターゲット |
|-------|-----------|------------------|
| **通常（ウィンドウ）** | `initialize(widget)` | スワップチェーンバックバッファ + m_layerRT（レイヤー合成用） |
| **ヘッドレス** | `initializeHeadless(w, h)` | m_layerRT（オフラインRTV）のみ |
| **オフスクリーン** | `pushRenderTarget(texture)` | 任意の `ITextureView` |

---

## 📋 10. その他関連クラス

### RenderCommandBuffer（コマンドバッファ）
- 各描画プリミティブを `std::variant<DrawPacket>` でキューイング
- `DiligentImmediateSubmitter::submit()` で変数访问して GPU 送信
- バッチ処理により API 呼び出し回数を削減

### DiligentImmediateSubmitter（送信者）
- `RenderCommandBuffer` の内容を実際の Diligent API に変換
- バーテックス/インデックス/定数バッファを事前バインド
- PSO と SRB を設定して `DrawIndexed()` 実行

### ShaderManager（シェーダ管理）
- 各プリミティブ用の頂点シェーダ・ピクセルシェーダを事前コンパイル
- `RenderShaderPair` 構造体で VS/PS のペアを保持
- `PSOAndSRB` 構造体で PipelineState + ShaderResourceBinding を保持

### PrimitiveRenderer2D（2D描画エンジン）
- 実際の描画ロジックのほとんどを `PrimitiveRenderer2D` が担当
- 座標変換（canvas↔viewport）も内部で管理
- `ArtifactIRenderer` は単に Impl のメソッドを委譲しているだけ

### PrimitiveRenderer3D（3Dギズモエンジン）
- 3D ライン、円、矢印、トーラス等をワールド座標で描画
- ImGuizmo などの3D操作ウィジェットで使用
- ビュー/射影行列は `setCameraMatrices()` で設定

---

## 📋 11. ファイルパス総まとめ

### 核心ファイル
| ファイル | 役割 |
|---------|------|
| `Artifact/include/Render/ArtifactIRenderer.ixx` | インターフェース宣言 |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 実装（Impl クラス） |
| `Artifact/include/Render/DiligentDeviceManager.ixx` | Diligent デバイス管理 |
| `Artifact/src/Render/DiligentDeviceManager.cppm` | DiligentDeviceManager 実装 |
| `Artifact/include/Render/PrimitiveRenderer2D.ixx` | 2D描画エンジン |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | PrimitiveRenderer2D 実装 |
| `Artifact/include/Render/PrimitiveRenderer3D.ixx` | 3Dギズモ描画エンジン |
| `Artifact/src/Render/PrimitiveRenderer3D.cppm` | PrimitiveRenderer3D 実装 |
| `Artifact/include/Render/ShaderManager.ixx` | シェーダ/PSO 管理 |
| `Artifact/src/Render/ShaderManager.cppm` | ShaderManager 実装 |
| `Artifact/include/Render/DiligentImmediateSubmitter.ixx` | コマンド送信 |
| `Artifact/src/Render/DiligentImmediateSubmitter.cppm` | 実装 |
| `Artifact/include/Render/RenderCommandBuffer.ixx` | コマンドバッファ型定義 |
| `Artifact/src/Render/RenderCommandBuffer.cppm` | 実装 |

### 使用侧ファイル
| ファイル | 役割 |
|---------|------|
| `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx` | Composition Editor コントローラ（ArtifactIRenderer を使用） |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 実装 |
| `Artifact/src/Widgets/Render/ArtifactSoftwareRenderTestWidget.cppm` | テスト用ウィジェット |

---

## 📋 12. 調査で確認できなかった点

| 項目 | 状況 | 備考 |
|-----|------|------|
| `ArtifactSoftwareRender` クラス | **存在しない** | `ArtifactSoftwareRenderTestWidget` は Qt Widget テスト用 |
| `ArtifactOpenGLRender` クラス | **存在しない** | OpenGL バックエンド未実装 |
| ソフトウェアレンダリング実装 | **DiligentEngine 依存のため未提供** | ヘッドレスモードも GPU 必須 |
| `ArtifactSoftwareRenderInspectors` クラス | **見つからない** | 名称が違う可能性あり |
| `ArtifactIRenderer` が純粋仮想インターフェースか | **否** | 具体的クラス（Pimpl で実装） |
| `Render.Composition` モジュール依存 | **`CompositionRenderController` が依存** | `import Artifact.Render.IRenderer;` |

---

## ✅ 結論

1. **`ArtifactIRenderer` は具体クラス**: Pimpl イディオムで実装された具象クラス（インターフェースではない）
2. **実装は単一**: `Artifact/src/Render/ArtifactIRenderer.cppm` の `Impl` クラスのみ
3. **DiligentEngine 統合**: `DiligentDeviceManager` 経由で D3D12/Vulkan を抽象化して使用
4. **責務**: GPUレンダリングのみ（ソフトウェアレンダリングなし）、2D/3D描画、リードバック、LOD制御等
5. **依存**: `DiligentEngine` 必須、`Render.Composition` は `CompositionRenderController` が間接参照
6. **バックエンド**: Direct3D12（Windows）と Vulkan（クロスプラットフォーム）の2択、実行時選択

---

**分析完了**  
※ 本レポートは `Artifact` リポジトリの 2026-04-17 時点のコードベースに基づく
