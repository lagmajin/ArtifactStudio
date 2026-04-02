# ボトルネック仮説: レンダリングキューからの出力が異常に遅い

## 症状
Render Queue を使用してコンポジションを出力（レンダリング）した際、処理速度が期待より著しく遅い。リアルタイム再生時と比較して、1フレームあたりのレンダリング時間が極端に長い。

## 調査前提

- 通常の Composition Editor の preview は GPU パイプライン（`pipelineEnabled_`）で rapid に描画されている
- Render Queue は `ArtifactCompositionRenderController::renderOneFrame()` を順次呼び出してフレームを出力していると推測される
- 出力時には `readbackToImage()` または `saveFrame()` による CPU への readback が発生する

## ボトルネック仮説一覧

### H1: GPU-CPU 同期（Readback ブロッキング）

Render Queue は各フレームのレンダリング後に `glReadPixels` / `IDeviceContext::MapBuffer` 等で texture を CPU メモリへ readback している。これが pipeline stall を引き起こし、GPU の描画待ちで CPU がブロックされる。特に `MapBuffer` で `MAP_FLAG_DISCARD` を使わず、`WAIT` ポリシーで读完生する場合、GPU が描画終了するまで CPU が待たされる。

### H2: Surface Cache の常時クリア

`ArtifactCompositionRenderController::bindCompositionChanged()` 内で、composition が変更された際に `surfaceCache_.clear()` と `gpuTextureCacheManager_->clear()` が呼ばれる。Render Queue ジョブ開始時に composition が (通常は変更されないが) バインドされ直すと、**全レイヤーの GPU texture cache がクリア**され、その後 each frame で static layer の texture を再アップロードする。画像・動画レイヤーが多い場合、この再生成コストは甚大。

```cpp
// ArtifactCompositionRenderController.cppm:1159-1168
compositionChangedConnection_ = QObject::connect(
    composition.get(), &ArtifactAbstractComposition::changed, owner,
    [this, owner, composition]() {
      surfaceCache_.clear();
      if (gpuTextureCacheManager_) {
        gpuTextureCacheManager_->clear();
      }
      invalidateBaseComposite();
      applyCompositionState(composition);
      owner->renderOneFrame();
    });
```

Render Queue 実行前に composition が "changed" を発火させていないか注意が必要。また、Render Queue の開始時に composition を既存のものから再設定するロジックがある場合、意図せず cache をクリアしている可能性がある。

### H3: ダウンサンプル無効（フル解像度レンダリング）

Preview 時には `effectivePreviewDownsample` が算出され（行 2545-2549）、`rcw/rch` がダウンサンプルされたオフスクリーン RT サイズになる。しかし Render Queue では `pipelineEnabled_` 状態でも **ダウンサンプルが適用されない**可能性がある。確認すべきは `renderOneFrameImpl` の Computed `rcw`/`rch` が full composition 解像度 (`cw`,`ch`) と同じかどうか。full 解像度では pixel 数が多く、シェーダ負荷と bandwidth 消費が増大する。

### H4: シングルスレッド・シーケンシャル処理

現在の Render Queue は1つのスレッドでフレームを順次 `renderOneFrame()` → `saveFrame()` している。GPU 描画は非同期だが、CPU 側の readback と保存が直列化され、GPU の pipeline depth を活用できていない。並列化（例えば N フレームを batch render してまとめて readback）がなされていない。

### H5: レイヤーごとのエフェクト適用コスト

一部のエフェクト（C人称の blur や heavy effect）が GPU で未実装（CPU fallback）の場合、`drawLayerForPreviewView` 内で `drawWithClonerEffect` が Software パス（`QImage` 操作）を使う。これがボトルネックになる。

### H6: 頻発する `changed` シグナル

composition または layer の `changed` がレンダリング中に頻発すると、`invalidateBaseComposite()` が呼ばれ、パイプラインが every frame で再構築される可能性がある。Render Queue 中は composition が不変であることを保証し、シグナルをブロックする必要がある。

## 確認方法

1. **フェーズ計測の強化**  
   `renderOneFrameImpl` 内で以下の時間を計測しログ出力：
   - Base background pass
   - Layer draw pass
   - Overlay pass
   - Readback / save
   既に `basePassMs`, `layerPassMs` は存在するが、Render Queue 専用の計測ポイントが必要。

2. **ダウンサンプル確認**  
   ```cpp
   qDebug() << "rcw=" << rcw << "rch=" << rch << "cw=" << cw << "ch=" << ch;
   ```
   を `renderOneFrameImpl` に追加し、Render Queue 実行時のオフスクリーン RT サイズが preview 時と同等か確認。

3. **Cache 有無の影響測定**  
   Render Queue 開始前に `surfaceCache_.clear()` を意図的に呼び出さないようにし、キャッシュを温存した状態での速度を計る。

4. **CPU/GPU プロファイル**  
   - GPU: RenderDoc 等で shader 命令数、帯域幅、pipeline stall を確認
   - CPU: 各フレームの time spent in `renderOneFrame`, `readbackToImage`, `saveFrame` を測定

## 対策方向

| 仮説 | 対策 |
|------|------|
| H1 Readback blocking | PBO (Pixel Buffer Object) で非同期 readback、または `MapBuffer` に `MAP_FLAG_DISCARD` と `WAIT` 無し |
| H2 Cache clear | Render Queue 開始前に composition をロード後、`surfaceCache_` と `gpuTextureCacheManager_` を温存。composition->setChanged() が起こらないようロックする |
| H3 Downsample | Render Queue 用の `downsampleFactor` 設定を追加し、preview 相当の `effectivePreviewDownsample` を適用 |
| H4 Single-thread | フレームを batch 化。例: 4 フレームを GPU に submit → 4 フレーム分の結果を一括 readback → 別スレッドで保存 |
| H5 CPU fallback | エフェクトの GPU 実装を完成させる（`M-FX-6` 関連） |
| H6 Changed spam | Render Queue 実行中は composition の `changed` シグナルを一時ブロック（`QSignalBlocker` 使用） |

## 優先すべき検証

1. **ダウンサンプルと readback の計時**  
   `renderOneFrameImpl` に以下の計測を追加：
   ```cpp
   auto t0 = std::chrono::high_resolution_clock::now();
   // ... レンダリング
   auto t1 = std::chrono::high_resolution_clock::now();
   QImage img = readbackToImage();
   auto t2 = std::chrono::high_resolution_clock::now();
   qDebug() << "GPU time:" << ... << "readback time:" << ...
   ```

2. **Cache クリアの Dewpoint**  
   Render Queue 開始前後で `surfaceCache_.size()` と `gpuTextureCacheManager_->size()` をログ出力。

3. **右上の `kMaxFrame/8` ダウンサンプル**  
   `effectivePreviewDownsample` の計算ロジック (`getEffectivePreviewDownsample`) を見直し、Render Queue 時にも同値が適用されるか確認。

## 関連ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/CompositionRenderer.cppm` / `ArtifactPreviewCompositionPipeline.cppm`
- `Artifact/src/Widgets/Render/ArtifactIRenderer.cppm`
- Render Queue Manager（存在する場合）

---

この仮説に基づき、`renderOneFrameImpl` に詳細な計測ポイントを追加して、まずは哪个フェーズが支配的かを特定することが望ましい。
