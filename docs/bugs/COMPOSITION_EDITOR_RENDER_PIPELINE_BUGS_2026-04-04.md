# コンポジションエディター レンダリングパイプライン バグ報告

**対象ファイル:** `ArtifactCompositionRenderController.cppm`, `ArtifactCompositionEditor.cppm`

---

## Bug 1: コンポジション領域が塗りつぶされない

### 症状
GPU ブレンドパス（CS ブレンド有効）使用時、コンポジション背景色（`bgColor`）が正しく表示されず、前フレームのゴミデータや透明な黒で上書きされる。

### 根本原因
GPU ブレンドパスの「Bug B fix」と呼ばれるリソースバリア（`UNORDERED_ACCESS → SHADER_RESOURCE` 遷移）が、**ブレンドが一度も実行されなかったフレーム**でも無条件に適用されていた。

```cpp
// 修正前（常に実行）
accumBarrier.OldState = RESOURCE_STATE_UNORDERED_ACCESS;  // ← ブレンド未実行時は誤り
accumBarrier.NewState = RESOURCE_STATE_SHADER_RESOURCE;
ctx->TransitionResourceStates(1, &accumBarrier);
```

accum テクスチャは `setOverrideRTV` + `clear()` の後 `RENDER_TARGET` 状態にある。CS ブレンドディスパッチが一度も実行されない場合（`frameOutOfRange=true` など）、`RENDER_TARGET` 状態のまま上記バリアが発行されると D3D12 未定義動作が発生し、GPU が前フレームの UAV キャッシュから alpha=1 のデータを読み取り、bgColor 矩形を覆い隠す。

### 修正内容
`bool blendPerformed = false;` フラグを追加し、CS ブレンドが実際に実行された場合のみ `true` に設定。バリアを `if (blendPerformed)` で保護することで、ブレンド未実行時は Diligent の `CommitShaderResources(TRANSITION)` による自動遷移に委ねる。

```cpp
bool blendPerformed = false;
// ... layer loop ...
if (blendOk) {
    renderPipeline_.swapAccumAndTemp();
    blendPerformed = true;
    // ...
}

// バリアはブレンド実行時のみ
if (blendPerformed) {
    accumBarrier.OldState = RESOURCE_STATE_UNORDERED_ACCESS;
    ctx->TransitionResourceStates(1, &accumBarrier);
}
```

---

## Bug D: 回転ギズモがフラットなリングではなくトーラスであるべき

### 症状
回転ギズモが LINE_LIST ベースの平面的なリング表示だった。業界標準では各軸にトーラス（チューブ状リング）、外側に自由回転用グレーリングが表示される。

### 修正内容

#### PrimitiveRenderer3D に `drawGizmoTorus()` を追加
トーラスジオメトリ（48 リングセグメント × 12 チューブセグメント = 3456 頂点/トーラス）を TRIANGLE_LIST で生成。

```
メジャーサークル: ringSegments=48 周、マイナーサークル: tubeSegments=12
各 quad → 2 三角形 → 6 頂点（法線は中心→頂点方向で擬似ライティング）
```

#### ArtifactIRenderer に `drawGizmoTorus()` を追加
`Detail::float3` → `QVector3D` 変換ブリッジを ArtifactIRenderer に実装。

#### Artifact3DGizmo の Rotate モード更新
- X/Y/Z 軸: `drawAxisRing` → `drawGizmoTorus`（シャドウ+コア 2 パス描画維持）
- 外側: グレー自由回転トーラスリング追加（`GizmoAxis::Screen`）
- majorRadius = `s * 0.54f`（軸リング）/ `s * 0.64f`（外側）
- minorRadius = `s * 0.025f`（コア）/ `s * 0.030f`（シャドウ）

---

## Bug E: スケールギズモの先端がキューブであるべき

### 症状
スケールギズモのシャフト先端が移動ギズモと同じピラミッド（コーン）で、視覚的に区別できなかった。

### 修正内容

#### PrimitiveRenderer3D に `drawGizmoCube()` を追加
キューブジオメトリ（6 面 × 2 三角形 = 36 頂点）を TRIANGLE_LIST で生成。法線ベースの擬似ライティング付き。

#### Artifact3DGizmo の Scale モード更新
- `drawAxisArrow` (ピラミッド先端) → `drawScaleAxis` ラムダ（ライン + キューブ先端）
- cubeHalf = `s * 0.065f`
- シャドウ（`cubeHalf * 1.12f`、暗色）+ コア（`cubeHalf`、明色）の 2 パス描画
- 中央均等スケールハンドル（白リング `s * 0.60f` + 半透明内リング `s * 0.18f`）は維持

---

## Bug F: オーディオミキサーウィジェットが再生に連動しない

### 症状
オーディオレイヤーを再生しても、オーディオミキサーウィジェットのメーターが動かない。

### 根本原因
1. `AudioRenderer` → `PlaybackEngine` → `PlaybackService` → `MixerWidget` → `masterBus.updateLevels()` の**マスターバス**レベル伝達チェーンは存在していた
2. しかし **チャンネルストリップ**は `refreshDerivedLevels()` 経由でフェーダー位置からの dB 計算値のみ表示（`volumeToMeterDb(strip->volume(), strip->isMuted())`）
3. リアルタイムオーディオレベルがチャンネルストリップに分配されていなかった

### 修正内容
1. `AudioMixer::Impl::refreshPlaybackLevels()` を拡張:
   - マスターバスに加え、全チャンネルストリップにもレベルを分配
   - 各ストリップのレベル = `masterRms + volumeToMeterDb(strip->volume())` で近似
   - ミュートストリップは `-60.0f`（サイレンス）

2. `AudioMixer` に public メソッド `updatePlaybackLevels(float leftRms, float rightRms)` を追加

3. `ArtifactCompositionAudioMixerWidget` の `audioLevelChanged` ハンドラを変更:
   ```cpp
   // 旧: masterBus->updateLevels(leftRms, rightRms) のみ
   // 新: mixer_->updatePlaybackLevels(leftRms, rightRms)
   //     → マスターバス + 全チャンネルストリップに分配
   ```

---

## 追加変更ファイル（第3フェーズ）

| ファイル | 変更内容 |
|---|---|
| `Artifact/src/Render/PrimitiveRenderer3D.cppm` | Bug D: drawGizmoTorus 追加、Bug E: drawGizmoCube 追加 |
| `Artifact/include/Render/PrimitiveRenderer3D.ixx` | Bug D/E: draw3DTorus, draw3DCube 宣言追加 |
| `Artifact/include/Render/ArtifactIRenderer.ixx` | Bug D/E: drawGizmoTorus, drawGizmoCube 宣言追加 |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | Bug D/E: float3→QVector3D ブリッジ実装追加 |
| `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm` | Bug D: Rotate モードのトーラス化、Bug E: Scale モードのキューブ先端化 |
| `Artifact/src/Audio/ArtifactAudioMixer.cppm` | Bug F: refreshPlaybackLevels 拡張、updatePlaybackLevels 追加 |
| `Artifact/include/Audio/ArtifactAudioMixer.ixx` | Bug F: updatePlaybackLevels 宣言追加 |
| `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm` | Bug F: audioLevelChanged ハンドラ変更 |

## Bug 2: コンポジションエディター下部/右部に約200pxの描画空白領域がある

### 症状
コンポジションエディターの下端・右端付近に、背景色で塗りつぶされるがギズモもレイヤーも表示されない「デッドゾーン」が生じる。DPI スケール（`devicePixelRatio`）が 1.0 より大きい場合のみ発生（例：125% スケール環境で 800論理px ウィジェット → 1000物理px スワップチェーン → 200px デッドゾーン）。

### 根本原因
スワップチェーンは `widget->width() * devicePixelRatio()` で物理ピクセルサイズで作成されるが、`hostWidth_/hostHeight_` の保存と `setViewportSize()` への引き渡しが論理ピクセルのままだった。

```
スワップチェーン幅: 1000px (物理)
hostWidth_ : 800px (論理) ← ここがズレの原因
DX12 viewport: 0..800 (論理領域のみ)
clear(): 0..1000 全体をクリア
draw calls: 0..800 のみ → 800..1000 がデッドゾーン
```

また、マウスイベント座標・ズーム/パン操作なども論理ピクセルで渡されていたため、DPR ≠ 1 環境ではヒットテストや操作位置が全体的にズレていた。

### 修正内容
1. `Impl` に `float devicePixelRatio_ = 1.0f;` を追加
2. `initialize()` と `setViewportSize()` で DPR を読み取り、論理ピクセルを物理ピクセルへ変換してから `hostWidth_/hostHeight_` と `renderer_->setViewportSize()` に渡す
3. `setViewportSize()` 内で `hostWidget_->devicePixelRatio()` を毎回再読み取り（モニター移動時の DPR 変化に対応）
4. 自動サイズ同期比較（`renderOneFrameImpl`）を `host->width() * devicePixelRatio_` vs `hostWidth_` に修正
5. 以下の関数で Qt 論理ピクセル座標を DPR 倍して物理ピクセルに変換：
   - `handleMousePress()` — `event->position() * devicePixelRatio_`
   - `handleMouseMove()` — 引数を `viewportPosLogical` にリネームしてスケール適用
   - `zoomInAt()` / `zoomOutAt()` — `viewportPos * devicePixelRatio_`
   - `panBy()` — デルタを `devicePixelRatio_` 倍
   - `layerAtViewportPos()` — `viewportPos * devicePixelRatio_`
   - `cursorShapeForViewportPos()` — `viewportPos * devicePixelRatio_`
6. `ArtifactCompositionEditor.cppm` の `updateViewportCursor()` 内にある直接ギズモ呼び出し（`handleAtViewportPos`）でも `pos * devicePixelRatio()` を適用

---

## Bug 3: CS ブレンド On/Off 切替でオブジェクトの見える位置が変わる

### 症状
GPU ブレンド（CS）を有効/無効で切り替えると、レイヤーオブジェクトが画面上で異なる位置に見える。

### 根本原因
Bug 1 の派生症状。CS ブレンド ON 時にリソースバリア誤発行で accum テクスチャに前フレームのゴミデータ（alpha=1）が読み込まれ、bgColor 矩形と合成領域境界が正しく描画されない。これにより「コンポジション領域がどこに表示されるか」が CS ON/OFF で異なって見えていた。

数学的には両パス（GPU ブレンド / フォールバック）で NDC 変換式は完全に一致しており、Bug 1 修正により本バグも同時に解消する。

### 修正内容
Bug 1 の修正（`blendPerformed` フラグによるバリア保護）によって解消。

---

## 変更ファイル

| ファイル | 変更内容 |
|---|---|
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | Bug 1: blendPerformed フラグ追加・バリア保護。Bug 2: devicePixelRatio_ 追加、全座標系変換。Bug A: bgColor 早期描画。Bug B: fitToViewport(0.0f)・zoom100 センタリング |
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | Bug 2: updateViewportCursor の直接ギズモ呼び出しに physPos スケール追加 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` | Bug B: zoomFit margin 0 化・zoom100 センタリング追加 |
| `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` | Bug C: frameChanged 時に painterTrackView_->update() 追加 |

---

## Bug A (第2フェーズ): コンポジション背景色が塗りつぶされない（継続バグ）

### 症状
コンポジション領域が `bgColor`（`comp->backgroundColor()`）で塗りつぶされない。レイヤーブレンドには影響していないが、背景矩形自体が画面に表示されない。

### 根本原因
GPU パスと fallback パス それぞれで `drawSolidRect(bgColor)` を描画していたが、GPU パスでは CS ディスパッチ後に D3D12 グラフィクス PSO へ切り替えるタイミングで `SetViewports` が不完全な状態になるケースがあった。特に初回フレームや CS ブレンドが介在した後では、D3D12 内部の viewport ステートが未初期化のまま `drawSolidRect` が呼ばれる可能性があった。

### 修正内容（第3フェーズ）
bgColor 描画を GPU パス内の「オフスクリーン描画後・viewport 復元直後・accum blit 直前」に配置。この位置では `ctx->SetViewports()` が確実に発行済みのため D3D12 viewport が有効。

```cpp
// GPU パス: ホスト viewport 復元後、accum blit 前
renderer_->setViewportSize(origViewW, origViewH);
ctx->SetViewports(1, &hostVP, ...);  // ← 確実に発行

// bgColor を Composition Space 座標で描画
renderer_->setCanvasSize(cw, ch);
renderer_->setZoom(origZoom);
renderer_->setPan(origPanX, origPanY);
renderer_->drawSolidRect(0.0f, 0.0f, cw, ch, bgColor, 1.0f);

// accum blit (SRC_ALPHA) → 透明ピクセル領域で bgColor が透過
renderer_->setCanvasSize(origViewW, origViewH);
renderer_->setZoom(1.0f);
renderer_->setPan(0.0f, 0.0f);
renderer_->drawSpriteTransformed(0, 0, origViewW, origViewH, identity, accumSRV, 1.0f);
```

**レイヤーブレンドへの影響がない理由:**
- レイヤーブレンドは accum RT 内で完結（bgColor は swap chain 上に描画）
- accum の SRC_ALPHA blit: `result = src * src.a + dst * (1 - src.a)`
  - 透明ピクセル(a=0): `result = 0 + bgColor * 1 = bgColor` ✓
  - 不透明ピクセル(a=1): `result = layer.rgb * 1 + 0 = layer.rgb` ✓

fallback パスも同様に SetViewports 後に bgColor を描画（line 3042-3074）。

---

## Bug B (第2フェーズ): Fit/Fill/100% でコンポジションが端まで表示されない

### 症状
「Fit」「Fill」「100%」ズームボタンを押してもコンポジション境界がビューポートの端まで表示されず、約 50px の余白が生じる。また 100% 時にキャンバスが左上に寄って表示される。

### 根本原因
1. `ArtifactIRenderer::fitToViewport(float margin = 50.0f)` のデフォルト引数が 50px。  
   `CompositionRenderController::zoomFit()` が引数なしで呼び出しており、常に 50px マージンが適用されていた。
2. `zoom100()` は `setZoom(1.0f)` を呼ぶだけで、pan のセンタリングを行っていなかった。

### 修正内容
1. `zoomFit()` の呼び出しを `fitToViewport(0.0f)` に変更（`CompositionRenderController` および `ArtifactCompositionRenderWidget` の両方）。
2. `zoom100()` に pan センタリングを追加:
   ```cpp
   const float panX = (hostWidth_  - lastCanvasWidth_)  * 0.5f;
   const float panY = (hostHeight_ - lastCanvasHeight_) * 0.5f;
   renderer_->setPan(panX, panY);
   ```
   `ArtifactCompositionRenderWidget::zoom100()` も同様（コンポジションサイズは `previewPipeline_.composition()->settings().compositionSize()` から取得）。

---

## Bug C: タイムライン右ペイン再生中に下部ウィンドウにゴミが表示される

### 症状
タイムライン右ペインでプレビュー再生中、プレイヘッドが動くとタイムライントラック表示エリア（`ArtifactTimelineTrackPainterView`）の一部に再描画されないゴミ（前フレームの残像）が表示される。

### 根本原因
`ArtifactPlaybackService::frameChanged` シグナルのハンドラ内で `painterTrackView_->setCurrentFrame(...)` を呼び出した後、`update()` が呼ばれていなかった。このため Qt はプレイヘッド位置変化による再描画要求を受け取らず、古い内容が画面に残り続けた。

`ArtifactTimelineWidget.cpp` (旧 line 1457–1460):
```cpp
if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setCurrentFrame(
        static_cast<double>(frame.framePosition()));
    // update() が抜けていた
}
```

### 修正内容
```cpp
if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setCurrentFrame(
        static_cast<double>(frame.framePosition()));
    impl_->painterTrackView_->update();  // ← 追加
}
```

