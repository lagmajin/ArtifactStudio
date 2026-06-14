# バグ報告: パーティクルレイヤー非表示調査

> 2026-06-14 調査  
> マイルストーン: `M-CE-CRIT-1` Critical Render / Media Stability Program

---

## 現象

パーティクルレイヤーがコンポジットエディタに表示されない。

---

## コードフロー分析

### 1. パーティクルレイヤー描画パス

**ArtifactParticleLayer::draw()** (line 194-249)

```cpp
void ArtifactParticleLayer::draw(ArtifactIRenderer* renderer) {
    // 1. パーティクルシステム更新
    impl_->particleSystem->goToFrame(frameNumber, fps);
    
    // 2. GPUレンダリングパス (renderer->isInitialized() == true の場合)
    if (renderer->isInitialized()) {
        const auto sourceData = impl_->particleSystem->captureRenderData();
        if (!sourceData.particles.empty()) {
            renderer->drawParticles(renderData);  // <-- GPUへ submit
            return;
        }
    }
    
    // 3. ソフトウェアフォールバック (renderer未初期化時)
    // QPainter でレンダリング
}
```

### 2. ParticleRenderer フロー

**ArtifactIRenderer::Impl::drawParticles()** (line 688-786)

1. **パーティクルデータ空チェック** → 警告ログ出力
2. **particleRenderer_ 未初期化** → Lazy init (`initialize(100000)`)
3. **viewport不正** → 警告ログ出力
4. **RTVなし** → 警告ログ出力
5. **コマンドバッファに登録** → `cmdBuf_.append(std::move(pkt));`

### 3. ParticleRenderer 実装

**ParticleRenderer.cppm**

- `initialize(size_t maxParticles)` - PSO/バッファ作成
- `updateBuffer(const ParticleRenderData& data)` - GPU upload
- `prepare(IDeviceContext* pContext)` - PSO設定
- `draw(IDeviceContext* pContext, size_t activeCount)` - 描画発行

---

## 調査結果

### 原因候補（優先順）

| 順 | 原因 | 根拠 |
|----|------|------|
| 1 | **RTV（レンダーターゲット）不在** | `drawParticles()` 753-766 で RTVチェック、無い場合はスキップ |
| 2 | **ParticleRenderer 初期化失敗** | Lazy init だが、バッファ/PSO作成失敗の可能性 |
| 3 | **viewport不正** | 行 716-726 で viewportチェック、0x0 の場合スキップ |
| 4 | **パーティクルデータ空** | Emitterなし、またはシミュレーション未実行 |
| 5 | **Diligent PSO/シェーダー未作成** | `ParticleRenderer::createPSO()` でログ出力 |

---

## 推定フロー（現在の問題）

```
[ParticleLayer::draw]
  → goToFrame() (シミュレーション)
  → captureRenderData() (パーティクル取得)
  → renderer->isInitialized() → false (Diligent未初期化)
  → フォールバックQImage描画へ
    → renderFrame() → renderToImage() → QPainter描画
    → ただし QPainter描画も何か条件でスキップされる可能性
```

### 推定原因

**`renderer->isInitialized()` が false** の場合、フォールバック描画が実行されるが:

- `renderToImage()` で描画後、`emit frameRendered()` のみで UI へ即反映されない可能性
- `goToFrame()` 内で `impl_->cachedFrame = QImage()` （line 763）でクリアされている
- `frameNumber == impl_->cachedFrameNumber` のタイミング競合

---

## 対処提案

### 1. デバッグログ追加

```cpp
// ArtifactParticleLayer::draw() 冒頭に追加
qDebug() << "[ParticleLayer] draw() called"
         << "frame=" << frameNumber
         << "rendererInitialized=" << renderer->isInitialized()
         << "particleCount=" << sourceData.particles.size();
```

### 2. ParticleRenderer::initialize ログ

`ParticleRenderer.cppm` - `initialize()` でバッファ/PSO作成成功/失敗をログ出力

### 3. viewport/ RTV 状態確認

```cpp
// ArtifactIRenderer::Impl::drawParticles() へ追加
qDebug() << "[ParticleRenderer] state"
         << "viewport=(" << m_viewportWidth << "x" << m_viewportHeight << ")"
         << "hasRTV=" << (primitiveRenderer_.currentRTV() != nullptr);
```

---

## 関連コード

- `Artifact/src/Layer/ArtifactParticleLayer.cppm` - レイヤー描画
- `Artifact/src/Render/ArtifactIRenderer.cppm` - drawParticles 実装
- `ArtifactCore/src/Graphics/ParticleRenderer.cppm` - Diligent描画
- `ArtifactCore/include/Physics/2D/Physics2D.ixx` - RigidBody2D（3Dパーティクル用）

---

## 次ステップ

1. 上記ログ追加して再実行
2. パーティクルデータが空でないことを確認
3. renderer初期化状態確認
4. ParticleRenderer PSO作成状態確認