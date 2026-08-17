# MILESTONE: 単一画像レイヤー化（Single Image Layerization）

**最終更新:** 2026-08-17

**ステータス:** In Progress（standalone prototype implemented; app integration not started）

**識別子:** M-IMG-LAYER-1

## 目的

一枚の静止画を解析し、前景オブジェクト・背景・必要な補助要素を、ArtifactStudioで個別に編集できる通常レイヤーへ変換する。

ここでいう「レイヤー化」は、元画像から完全な3D構造を復元することではない。可視画素からマスクを推定し、切り抜きレイヤーを作成し、マスクで隠れた背景を必要な範囲だけ補完する、制作向けの2D／2.5D変換と定義する。

## 到達イメージ

1. 静止画レイヤーを選択する。
2. Composition Editor の専用レイヤー化導線から解析を開始する。
3. 解析結果を候補オブジェクト、背景、信頼度、境界プレビューとして確認する。
4. 採用する候補と背景補完方法を選択する。
5. 一括 Undo 可能な一回の確定で、複数の画像レイヤーと必要なマスクを作成する。
6. 作成後は通常の Composition Editor、Timeline、Inspector、Mask 編集で個別に編集できる。

解析・生成の入口は `ArtifactCompositionEditor`／`ArtifactCompositionRenderWidget` の編集責務に置く。`ArtifactPropertyWidget` は生成結果の通常プロパティ編集だけを担当し、AI解析設定や候補一覧を汎用Propertyへ混在させない。解析失敗や候補の検査情報は既存の診断面へ接続する。

## 現行基盤とギャップ

### 再利用できる基盤

- `ArtifactImageLayer` の `ImageF32x4_RGBA` による静止画バッファ、source identity、保存／再読込、GPU／CPU表示経路。
- `LayerMask`、`MaskPath`、`MaskPath::fromAlphaMask()` によるアルファマスクとベジェパス化。
- `OpenCVRotoBrushEngine` のマスク推定、精緻化、既存フレームへの伝播補助。
- ONNX／DirectML の汎用AI基盤と `MaskCutoutPipeline` のGPU cutout経路。
- Composition Editor の `Modal.Mask`／`Modal.Pen` と既存のマスク編集・Undo経路。

### 今回新たに必要な責務

- 一枚の入力から複数オブジェクト候補を返すセグメンテーション契約。
- 候補マスクの重複除去、信頼度、境界品質、前後順の管理。
- マスク付き画素を独立した画像レイヤーへ変換する生成オーケストレーション。
- 背景の未観測領域を補完した場合の明示的な不確実性表示と保存情報。
- 解析結果を再利用・再編集・再読込できる生成メタデータ。

既存のRotoBrush／SAM計画の時間伝播や動画対応を本マイルストーンへ取り込まない。単一の静止画を対象に、まず制作へ使える変換結果を成立させる。

## 実装フェーズ

### Phase 0A — Standalone prototype（アプリ非統合）

- `tools/single_image_layerizer/layerize.py` を独立CLIとして実装する。
- `--mask`、`--bbox`、OpenCV GrabCutの3入力方式を切り替えられるようにする。
- 候補マスク、切り抜きRGBA、背景、preview、versioned `manifest.json` を出力する。
- `source identity`、`bounds`、`confidence`、`provenance`、推定画素の警告を、アプリとは独立に確認できるようにする。
- モデル推論・Qt・Composition Editor・Timeline・GPU backendには接続しない。

受入条件:

- [x] アプリを起動せずに、入力画像からlayer packを生成できるCLIがある。
- [x] マスク入力を使って、切り抜きRGBAと背景を再現可能に出力できる。
- [x] `manifest.json` にsource hash、候補bounds、provenance、hidden pixelの警告を保存する。
- [ ] 実素材での出力品質、OpenCV環境、モデル候補の比較を確認する。

### Phase 0 — 仕様・受入素材・モデル境界の固定

- 「前景1個」「複数オブジェクト」「単純背景」「複雑背景」の代表静止画を固定する。
- 結果の最小単位を `candidate mask + bounds + confidence + order hint + source identity` として定義する。
- 自動解析、ユーザーのクリック／矩形補助、手動マスク修正の責務を分離する。
- ローカル推論モデルを必須にするか、モデル未配置時に手動／既存OpenCV補助へフォールバックするかを決める。
- hidden pixel の復元は推定であり、元画像から証明できないことをUIと保存メタデータで明示する。

### Phase 1 — 単一オブジェクトの切り抜きMVP

- 画像クリックまたは矩形指定から、前景候補を1つ生成する。
- 推論結果を半透明マスク、輪郭、除外領域としてComposition Editor上に表示する。
- 既存の `ImageF32x4_RGBA` とマスク経路を使い、切り抜き画像レイヤーを生成する。
- 生成前プレビュー、再解析、採用、キャンセルを実装する。
- 生成操作を一つのUndo単位にし、保存／再読込後もsource identity、mask、bounds、生成元を保持する。

完了時点では、まず「人物または主オブジェクトを一枚の画像から切り抜いて編集できる」ことを受入条件とする。

### Phase 2 — 複数候補のレイヤー化

- 画像全体から複数候補を生成する、またはユーザーが複数の対象を指定する。
- 小さすぎる候補、重複候補、ほぼ同一の候補を信頼度と面積で整理する。
- 各候補に仮名称、bounds、confidence、mask preview、採否チェックを付ける。
- 候補ごとに通常の画像レイヤーを作成し、Timeline上で個別選択・非表示・並べ替えできるようにする。
- 生成順序は推定値として扱い、ユーザーが確定前に並べ替えられるようにする。
- 元の入力レイヤーは破壊せず、参照用に残すか、ユーザーが明示した場合だけ置換する。

### Phase 3 — 背景再構成と境界品質

- 前景を除去した背景の穴を、既存の画像処理基盤または承認済みの補完エンジンで埋める。
- 最初は小さな穴・単純背景を対象にし、複雑な遮蔽物の裏側を無条件に復元しない。
- 補完領域を「推定画素」として記録し、元画像由来の画素と区別できる診断情報を持つ。
- 髪、半透明、細い輪郭、影、反射などの境界ケースに対して、収縮・拡張・feather・manual refine を適用できるようにする。
- 背景補完に失敗した場合は透明背景または元画像保持へ安全に戻り、切り抜きレイヤー自体を失わない。

### Phase 4 — 制作向け修正導線

- 解析結果の前景追加／除外を既存の `RotoBrush`、Brush、Pen、Mask 編集へ渡す。
- 頂点編集、ブラシ修正、マスクの有効／無効、再解析、候補の再生成を一貫したUndo経路にする。
- AI結果を確定せずに通常のマスクとして編集できる暫定状態を持つ。
- 失敗理由を `model`、`decode`、`mask`、`cutout`、`background`、`composite` の段階で表示する。
- 解析中のキャンセル、対象レイヤー変更、Composition切替で古い結果が新しい対象へ適用されないよう世代管理する。

### Phase 5 — Preview／Render Queue／保存の一致

- Composition View、Software Preview、Render Queue の切り抜き結果と背景補完結果を比較する。
- source version、mask revision、候補設定、モデル識別子、補完設定をcache keyと保存メタデータへ反映する。
- source更新、relink、missing、project save／reload、GPU device再初期化後にstaleな生成結果が残らないことを確認する。
- GPU経路が利用できない場合は、明示的なCPU fallbackと失敗診断を行う。
- 受入素材で、元画像との差分、アルファ境界、レイヤー順、再読込後の結果を記録する。

### Phase 6 — 2.5D配置（任意の拡張）

- 深度推定を使った前後配置、視差、軽微なカメラ移動を追加する。
- 深度は測定値ではなく推定値として保存し、奥行きの編集と再計算を分離する。
- Phase 1〜5の2Dレイヤー化と受入が完了するまで着手しない。

## データ・保存契約

生成結果は、少なくとも次の情報を持つ。

| 項目 | 内容 |
|---|---|
| source identity | 元画像のasset ID、source version、path参照 |
| result identity | 生成結果の安定IDと生成日時 |
| candidate | 仮名称、mask、bounds、confidence、採否、order hint |
| inference | モデル識別子、モデルバージョン、入力サイズ、設定値 |
| background | 未変更、透明化、補完済み、補完範囲、補完方法 |
| provenance | 元画像由来／推定／手動修正の区分 |
| revision | マスク、補完、手動修正のrevision |

生成画像を外部一時ファイルだけに依存させない。再読込に必要なmask、設定、source参照、生成バッファまたは再生成可能な素材契約を、既存のproject／asset保存境界に合わせて決める。

## 実装制約

- 新しいレイヤー合成は `ImageF32x4_RGBA`、既存GPU経路、または専用CPU合成を使い、新規の `QImage`／`QPainter`／Qt `CompositionMode` を主経路へ追加しない。
- QtCSS、`QColorDialog`、新規のグローバルシグナル／スロットは追加しない。既存の操作・サービス・診断経路を再利用する。
- `.ixx` の新規依存は最小化し、ポインタ／参照で済む型は前方宣言を優先する。新規PImplの所有には既定の `Impl*` 方針を使う。
- Diligent／DX12 backendを推測で広範囲に変更しない。GPU cutout、cache、CPU fallbackの境界を分けて確認する。
- `ReactiveEvents` は変更しない。
- 動画、連番、時間方向のマスク伝播、完全な隠し領域復元、画像からの完全なベクター化は対象外とする。

## 完了条件

- [ ] 代表的な単純静止画から主オブジェクトを切り抜き、通常の画像レイヤーとして編集できる。
- [ ] 複数候補を採否・名称・順序付きで確認し、一括Undoでレイヤー化できる。
- [ ] 元画像を保持したまま生成レイヤーを作成でき、source identityと生成元が保存／再読込される。
- [ ] マスク境界を既存のBrush／RotoBrush／Pen／Mask編集で修正できる。
- [ ] 背景補完の推定領域が明示され、補完失敗時に安全なfallbackがある。
- [ ] Composition View、Software Preview、Render Queueでアルファ境界とレイヤー順が一致する。
- [ ] source更新、missing、relink、キャンセル、対象切替でstale結果やproject data破損が起きない。
- [ ] モデル未配置、推論失敗、巨大画像、非対応画像で、段階付き診断と安全な未適用状態へ遷移する。
- [ ] 実素材の受入記録があり、「元画像から直接確認できる画素」と「推定画素」が区別されている。

## 関連文書

- [`../../tools/single_image_layerizer/README.md`](../../tools/single_image_layerizer/README.md)

- [`MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`](MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md)
- [`MILESTONE_ROTOBRUSH_AI_MASK_2026-08-01.md`](MILESTONE_ROTOBRUSH_AI_MASK_2026-08-01.md)
- [`MILESTONE_PHOTOSHOP_LIKE_IMAGE_EDITING_2026-04-11.md`](MILESTONE_PHOTOSHOP_LIKE_IMAGE_EDITING_2026-04-11.md)
- [`../analysis/STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX_2026-08-08.md`](../analysis/STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX_2026-08-08.md)
- [`../spec/SPEC_MASK_OPERATIONS_REQUIREMENTS_2026-07-31.md`](../spec/SPEC_MASK_OPERATIONS_REQUIREMENTS_2026-07-31.md)
- [`../../Artifact/docs/LAYER_CREATION_SPEC_2026-03-15.md`](../../Artifact/docs/LAYER_CREATION_SPEC_2026-03-15.md)

## 次の実装単位

まずPhase 0として、単純背景・人物／物体・複数候補の最小受入素材を固定し、既存 `OpenCVRotoBrushEngine`／ONNX基盤／`LayerMask`／画像レイヤー生成経路の接続点を静的に確認する。モデル選定と新規モジュール追加は、その境界確認後に最小範囲で決める。ビルド・テスト・runtime検証は、実行許可を得てから行う。
