# OCIO 不足機能 メモ

**日付**: 2026-08-01
**現状評価**: 🟢 75% — アーキテクチャは完成。実際の色変換精度とワークフローにギャップあり。

---

## 構造上の不足

### 1. 実 OCIO ライブラリ未統合 🔴
- コード自体のコメントに `When real OCIO library is unavailable, these provide basic transforms` と明記されている
- 現在は `ColorSpaceConverter::getConversionMatrix()` による**3x3 行列近似**のみ
- 実際の OpenColorIO (v2.x) ライブラリを使っていないため、LUT 補間、3D 変換、正確な色変換ができない
- 結果: ACES RRT+ODT の正確なレンダリングは不可能

### 2. 実 OCIO config.ocio ファイルが読めない 🔴
- `loadFromFile()` が **JSON** を読み込む（`QJsonDocument::fromJson`）
- 本物の OCIO config は **YAML** 形式（`.ocio` 拡張子）
- つまり業界標準の ACES 1.3 config.ocio や studio 固有の config を直接ロードできない
- 自前の JSON フォーマットに変換しないと使えない

### 3. GPU カラー変換がない 🟡
- `applyViewTransformToImage()` が **CPU** 上で `Parallel::For` + 行列乗算
- Diligent Engine の GPU パイプラインを使っているのに、色変換が CPU 往復になる
- ビューポート表示のたびに GPU→CPU→GPU の転送が発生 → パフォーマンス劣化
- GPU コンピュートシェーダーで処理すべき

### 4. カラースペース変換が行列のみ 🟡
- `OCIOColorSpaceTransform` が 3x3 matrix + offset のみ
- 実際の OCIO 変換は **1D LUT → 3D LUT → 1D LUT** のパイプライン
- 行列近似では色域変換（gamut mapping）が正確にできない
- トーンマッピング（HDR→SDR）も行列では不可能（PQ/HLG の非線形カーブが必要）

---

## ワークフロー上の不足

### 5. 素材ごとの入力カラースペース指定がない 🟡
- AE/Nuke では各フッテージ素材に個別の Input Color Space を設定できる
- 例: 「この ProRes ファイルは S-Log3」「この EXR は ACEScg」
- 現在の実装はグローバルな working space のみ → 素材ごとの解釈ができない
- `ArtifactOCIOManager` に `perSourceColorSpace_` マップが必要

### 6. プロジェクト単位の OCIO config 割り当てがない 🟡
- 現在はグローバルシングルトン
- AE ではプロジェクト設定で OCIO config を選択
- プロジェクトによって ACES 1.3 と ACES 2.0 を切り替えられない

### 7. ビューアごとの display/view 設定がない 🟡
- 現在はグローバルな display/view
- 複数ビューアで異なる表示（sRGB と Rec.709 など）を同時表示できない
- コンポジションビューアごとに個別設定すべき

### 8. Looks（ルック）が未実装 🔴
- `looks()` の getter/setter はあるが、`applyViewTransformToImage` 内で使われていない
- ACES の Looks はクリエイティブグレーディング（例: "ACES 1.0 Default", "ACES 1.0 Golden"）
- setter から実際の色変換パイプラインまで配線されていない

### 9. トランスファー関数が不完全 🟡
- 現在: `sRGB`, `Gamma22`, `Gamma24`, `Linear` のみ
- 不足:
  - **PQ** (ST.2084 — HDR10, Dolby Vision)
  - **HLG** (Hybrid Log-Gamma — HDR 放送)
  - **S-Log3 / S-Log2** (Sony)
  - **LogC3 / LogC4** (ARRI)
  - **REDlog / REDlogFilm** (RED)
  - **Canon Log / Canon Log 2/3** (Canon)
  - **V-Log** (Panasonic)
  - **N-Log** (Nikon)
  - **Cineon Log**

### 10. 露出・ガンマのビューア調整がない 🟡
- AE/Nuke ではビューアで露出（Exposure）+ ガンマ（Gamma）を一時的に調整できる
- OCIO config 全体を変えずに、見た目だけ明るくする用途
- 現在の実装にはこの UI がない

### 11. 色空間の名前解決が部分一致のみ 🟡
- `mapOCIOColorSpaceToEnum()` が単純な部分文字列マッチ
- 例: `displayName.contains("sRGB")` は "sRGB" も "Linear sRGB" も "Wide Gamut sRGB" もすべてマッチしてしまう
- OCIO のエイリアス解決・ロール解決と統合すべき

---

## 優先順位

| 優先度 | 項目 | 理由 |
|--------|------|------|
| **P0** | 2. 実 OCIO config.ocio 読み込み | 外部設定ファイルが読めないのは致命的 |
| **P0** | 5. 素材ごとの入力カラースペース | 複数ソースがあると色が破綻 |
| **P1** | 1. 実 OCIO ライブラリ統合 | 正確な色変換に必須（vcpkg に OpenColorIO あり） |
| **P1** | 3. GPU カラー変換 | 毎フレームCPU往復はパフォーマンス上問題 |
| **P1** | 8. Looks 配線 | クリエイティブグレーディングパイプラインが死んでいる |
| **P2** | 9. トランスファー関数拡充 | カメラログ素材の読み込み時 |
| **P2** | 6. プロジェクト単位設定 | マルチプロジェクト運用 |
| **P2** | 7. ビューアごと設定 | マルチビューア運用 |
| **P3** | 10. 露出・ガンマ調整 | QoL |
| **P3** | 11. 名前解決の厳密化 | エッジケース |

---

## 2026-08-02 実装進捗

以下を実装済み。ビルド・runtime確認は未実施。

- `vcpkg.json` / `ArtifactCore/CMakeLists.txt` に OpenColorIO 2.5 の依存を明示
- `OCIOConfig::loadFromFile()` で実 `.ocio` config を `OCIO::Config::CreateFromFile()` から読み込み
- roles / color spaces / displays / views / view colorspace を列挙
- `ArtifactOCIOManager` の CPU Processor による Display/View Transform
- Looks を `LookTransform + DisplayViewTransform` の GroupTransform として適用
- 素材ごとの `inputColorSpace` を実OCIO Processorへ接続
- PQ / HLG / ACEScc / ACEScct / Sony S-Log3 などの入力transfer fallback
- project JSONへのOCIO config path、working space、display、view、looks保存・復元
- Viewer Exposure / Gamma のサービス層とproject JSON保存・復元
- Display/View colorspaceを使用した厳密な出力色空間解決

残作業:

- Viewer Exposure / Gamma のUI操作面
- 複数viewerごとの独立設定
- OpenColorIO direct GPU shader / native 1D・3D LUT binding（3D LUT bake + existing GPU LUT pass は実装済み）
- runtimeでの色差・alpha・HDR値保持の検証

### 追加調査: 表示変換の接続状態

- `ArtifactImageLayer` からの入力変換は実際に呼び出されている。
- 一方、`ArtifactOCIOManager::applyViewTransformToImage()` の呼び出し元は現時点でなく、CPU表示変換APIはプレビュー経路へ未接続。
- `ArtifactFinalPostProcess` はGPU LUT用の既存基盤として存在し、3D LUT bake経由の生成・適用まで接続済み。直接 OCIO GPU shader と native 1D/3D resource を使う経路は未接続。
- `ArtifactFinalPostProcess` の所有・適用と ping-pong texture 接続は実装済み。次の実装単位は、直接 OCIO `GPUProcessor` shader を Diligent pass へ渡す経路、または viewer UI の編集導線。
- CPU `ImageF32x4_RGBA` 変換を描画ホットパスへ直接挿入するのは、GPU経路があるため現時点では採用しない。
- オフスクリーン compute texture の `RTV/SRV/UAV` 公開、ping-pong cache、`ArtifactFinalPostProcess::apply()` 接続は実装済み。
- post-process 側は通常の sRGB render target と分離し、`RGBA16_FLOAT + RTV/SRV/UAV` の compute 用 texture を専用生成するようにした。sRGB texture をそのまま UAV 化する backend 制約を避けるため。
- `ArtifactOCIOManager::gpuViewTransformShader()` を追加し、active working/display/view/looks から OCIO 2.5 の HLSL shader fragment を生成できる境界を作った。Diligent 用 compute wrapper、uniform、1D/3D LUT resource binding は引き続き未接続。
- `gpuViewTransformDescriptor()` で shader source に加えて uniform 名、uniform buffer size、1D/2D/3D texture 名・サイズを取得できるようにした。Diligent binding 層が必要とする OCIO resource metadata の入口まで実装済み。
- descriptor に texture 次元と 1D/3D shader binding index も追加し、binding 層が名前推測に依存しない形にした。
- uniform descriptor に型、buffer offset、取得時点の値も追加した。Diligent の constant buffer 更新に必要な OCIO dynamic uniform 情報を渡せる。
- `bakeViewTransformLUT()` を追加し、OCIO CPU Processor から指定解像度の3D LUTを生成できるようにした。既存 `LUT3DGPUComputer` へ渡す実用的なGPU経路の基盤で、毎フレームのCPU画素変換を避けられる。
- `CompositionRenderController` が OCIO LUT を設定変更時に一度だけベイクし、`ArtifactFinalPostProcess` の compute pass と ping-pong texture を通して合成結果へ適用する接続を追加した。runtime確認は未実施。
- LUTベイクに viewer Exposure/Gamma も反映し、両値を再ベイクキーへ含めた。CPU表示変換経路との viewer 調整差を解消した。
- composition cache hit 時も未変換 texture を直接表示せず、同じ post-process 経路を通すようにした。キャッシュ再利用時の表示変換抜けを防止。
- OCIO config のクリア時に前回の GPU LUT が残らないよう、無効 config・空キーで明示的に LUT を解除するようにした。
- 外部 `.ocio` config がない組み込み preset でも、既存 ColorSpace matrix fallback と viewer Exposure/Gamma を GPU LUTへベイクするようにした。
- LUT shader に入力 domain パラメータを追加し、OCIO表示変換は `[0,4]` domain でベイク・適用するようにした。従来の一般LUT利用者は default `[0,1]` のまま。
- `ArtifactFinalPostProcess` のコメントを実装状態に合わせ、現在の view transform が baked LUT 経路で実行されることを明記した。
- `[0,4]` domain は一般的な正の scene-linear HDR入力向けの初期範囲であり、negative値・4超の値・domain外の外挿は未検証。HDR完全対応とは扱わない。

### 実装可否サマリー

| 項目 | 判断 | 現状 |
|---|---|---|
| 実 `.ocio` 読み込み | 実装可能・実装済み | OpenColorIO 2.5 `Config::CreateFromFile()` を使用 |
| 素材入力変換 | 実装可能・実装済み | source color space / transfer function を入力経路へ接続 |
| Display/View/Looks | 実装可能・実装済み | OCIO Processor と 3D LUT bake の両経路 |
| GPU表示変換 | 実装可能・部分実装 | 3D LUT compute pass 接続済み、native GPU shader binding は未完 |
| Project単位設定 | 実装可能・実装済み | project JSONへ保存・復元 |
| Viewer Exposure/Gamma | サービス層実装済み | UI操作面は未実装 |
| 複数viewer設定 | 実装可能 | viewer識別子と独立設定モデルが未実装 |
| HDR/alpha/runtime検証 | 実装後の検証項目 | ビルド・実機確認待ち |

Exposure/Gamma setter は既存 `configChanged` 通知を発火するため、将来の UI・保存・表示更新側は新しい signal 配線なしで変更を検知できる。
working space / display / view / looks の setter も既存 `configChanged` と専用変更通知を揃え、LUT再生成とUI状態更新の通知漏れを防いだ。
