# 実装レポート: GPUテキスト描画修正 & 音声出力実装

**日付**: 2026-04-19  
**対象**: GPU テキスト/シェイプ描画、レンダーキュー音声出力

---

## 問題1: GPUテキスト描画がコンポジットエディタ・ソロビューに表示されない

### 根本原因

`ArtifactCompositionRenderController` および `ArtifactPreviewCompositionPipeline` の
テキストレイヤー処理ブロックにて、**常に `toQImage()` (QPainter CPU パス) を経由**していた。

```cpp
// 修正前 (コンポジットエディタ)
if (auto* textLayer = ...) {
    const QImage img = textLayer->toQImage(); // ← QPainter 常用
    applySurfaceAndDraw(img, localRect, ...);
    return;  // ← GPU パスへ到達しない
}
```

`ArtifactTextLayer::draw()` は内部に GPU テキストパス (`submitGlyphTextTransformed`) を
持つが、コンポジションビューから一切呼ばれていなかった。

### 副原因: バッファオーバーランバグ (事前セッションで修正済み)

`submitGlyphTextTransformed` が `RenderSolidTransform2D` (16 バイト) バッファに
`QMatrix4x4` (64 バイト) を書き込んでいた。  
新 PSO (`glyphQuadTransformPsoAndSrb_`) と 64 バイトバッファを追加して修正済み。

### 修正内容

**`ArtifactCompositionRenderController.cppm`** (~919行):
```cpp
// 修正後
if (auto* textLayer = ...) {
    if (!hasRasterizerEffectsOrMasks(layer)) {
        textLayer->draw(renderer);  // GPU パス直接呼び出し
    } else {
        // エフェクト/マスクあり → toQImage() を維持
        const QImage textImage = textLayer->toQImage();
        if (!textImage.isNull()) applySurfaceAndDraw(textImage, localRect, true);
    }
    return;
}
```

**`ArtifactPreviewCompositionPipeline.cppm`** (~211行): 同様のパターンで修正。

---

## 問題2: シェイプ (Solid2D) 描画 — 調査結果

**コンポジットエディタ**: `drawSolidRectTransformed` で GPU 直接描画 → 既に正常動作  
**プレビューパイプライン**: `drawSolidRectTransformed` または `drawSpriteTransformed` で処理済み  

`ArtifactSolid2DLayer` の GPU 描画パスは両ビューとも実装済みであり、修正不要。

---

## 実装2: レンダーキュー 音声出力サポート

### 既存インフラ (変更なし)

以下は既にバックエンドに実装済み:
- `exportCompositionAudioToWav()` — コンポジション音声を WAV エクスポート
- `muxAudioWithVideo()` — WAV + 動画 → 最終出力に統合
- `addRenderQueueForComposition()` — `comp->hasAudio()` 時に自動で音声統合を有効化

### 修正内容

#### 1. `addRenderQueueWithPreset()` に音声自動有効化を追加

`addRenderQueueForComposition()` にのみ存在していた自動有効化ロジックを、
`addRenderQueueWithPreset()` にも追加:

```cpp
// ArtifactRenderQueueService.cppm
if (comp->hasAudio()) {
    job.integratedRenderEnabled = true;
}
```

#### 2. `ArtifactRenderOutputSettingDialog` に音声 UI を追加

- **Audio チェックボックス**: "Include audio in output" — 音声統合の有効/無効
- **Audio Codec コンボ**: AAC / MP3 / FLAC / Opus
- **Audio Bitrate スピン**: 32〜512 kbps (デフォルト 128 kbps)
- チェックオフ時はコーデック/ビットレートを無効化

```
Bitrate:         [8000 kbps]
Audio:           ☑ Include audio in output
Audio Codec:     [AAC ▼]
Audio Bitrate:   [128 kbps]
```

#### 3. `ArtifactRenderQueueManagerWidget.cpp` に音声設定の読み書きを接続

Format... ボタンからダイアログを開く前後に音声設定を読み書き:
```cpp
// ダイアログ表示前
dialog.setIncludeAudio(service->jobIntegratedRenderEnabledAt(index));
dialog.setAudioCodec(service->jobAudioCodecAt(index));
dialog.setAudioBitrateKbps(service->jobAudioBitrateKbpsAt(index));

// OK 後
service->setJobIntegratedRenderEnabledAt(index, dialog.includeAudio());
service->setJobAudioCodecAt(index, dialog.audioCodec());
service->setJobAudioBitrateKbpsAt(index, dialog.audioBitrateKbps());
```

サマリーラベルにも音声状態を追記:
```
Format: MP4 | Codec: H.264 | Encode: auto | Render: auto | Audio: aac@128kbps
```

### 音声統合フロー

```
1. comp->hasAudio() → integratedRenderEnabled = true (自動)
2. 動画をテンポラリファイルに出力: output.__video_tmp__.mp4
3. コンポジション音声を WAV に出力: output.__audio_tmp__.wav
4. muxAudioWithVideo() で動画 + 音声 → output.mp4
5. テンポラリファイル削除
```

---

## 変更ファイル一覧

| ファイル | 変更内容 |
|---------|---------|
| `Artifact/include/Widgets/Dialog/ArtifactRenderOutputSettingDialog.ixx` | 音声メソッド追加 |
| `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm` | 音声 UI + getter/setter 実装 |
| `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp` | 音声設定の読み書き接続 + サマリー更新 |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | preset パスの音声自動有効化追加 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | テキストレイヤー GPU パス接続 |
| `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm` | テキストレイヤー GPU パス接続 |
| `Artifact/include/Render/ThickLineShaders.ixx` *(事前)* | `g_2DSpriteTransformColorVS` HLSL 追加 |
| `Artifact/src/Render/ShaderManager.cppm` *(事前)* | `glyphQuadTransformPsoAndSrb_` PSO 追加 |
| `Artifact/src/Render/DiligentImmediateSubmitter.cppm` *(事前)* | `submitGlyphTextTransformed` バッファ修正 |
