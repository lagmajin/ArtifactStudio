# Single Image Layerizer Prototype

ArtifactStudioへ統合しない、静止画レイヤー化の検証用プロトタイプです。入力画像から次の成果物を持つlayer packを生成します。

- `layers/Layer_01.png`: 透明背景の切り抜きRGBA画像
- `masks/Layer_01.png`: 候補マスク
- `background.png`: `transparent`、`original`、または `inpaint` の背景
- `preview.png`: 背景と生成レイヤーを合成した確認画像
- `manifest.json`: source identity、候補bounds、confidence、provenance、設定、警告

## 使い方

Pillowだけで動く決定的なマスク入力（単一候補）:

```powershell
python tools/single_image_layerizer/layerize.py input.png `
  --mask foreground_mask.png `
  --background transparent `
  --output out/layerized
```

複数の disconnected component を別レイヤーへ分ける場合は、上のコマンドへ `--split-components` を追加してください。この分割には OpenCV と NumPy が必要です。

矩形を仮の候補として使う最小確認:

```powershell
python tools/single_image_layerizer/layerize.py input.png `
  --bbox 120,80,640,720 `
  --background original `
  --output out/layerized
```

OpenCV GrabCutを使う自動切り抜き:

```powershell
python tools/single_image_layerizer/layerize.py input.png `
  --auto --bbox 120,80,640,720 `
  --background inpaint `
  --output out/layerized
```

`--auto` と `--background inpaint`、`--split-components` の高度な処理には OpenCV と NumPy が必要です。モデル推論はまだ接続していません。`--mask` を基準入力にすることで、セグメンテーションモデルの比較とレイヤー生成契約を独立に検証できます。

## 設計上の境界

- Qt、ArtifactStudio、Composition Editor、Timeline、GPU backendには依存しません。
- `manifest.json` の schema は `artifact.single-image-layerization/v1` です。
- `confidence` はモデル未接続時には `null` とし、面積やcoverageをconfidenceとして偽装しません。
- 切り抜き画像の画素は元画像由来、背景のinpaint領域は推定画素としてmanifestに記録します。
- 一枚の画像では隠れている画素を確実に復元できないため、結果は「編集可能な推定レイヤー」です。
- 動画、連番、時間方向のマスク伝播、アプリへのレイヤー生成接続は対象外です。

## アプリ統合時の想定

後続のArtifactStudio統合では、`manifest.json` を候補レイヤー生成契約の入力にできます。候補の `bounds`、`files.rgba`、`files.mask`、`provenance`、`source.sha256` を検証してから、既存の画像レイヤー、LayerMask、Undo、project保存経路へ接続します。モデル実装やUIをこのプロトタイプへ混ぜず、まず結果品質とmanifestの安定性を確認します。
