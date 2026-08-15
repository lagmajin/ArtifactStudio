# マイルストーン: FSR / DLSS アップスケーリング基盤（2026-08-05）

**最終更新:** 2026-08-15

**現状:** renderer 内の単純な低解像度描画スケールは実装済み。FSR/DLSS/XeSS の方式選択、temporal frame contract、GPU upscaler adapter、診断・履歴リセットは未実装。

## 現行コード監査 (2026-08-15)

`ArtifactIRenderer` には `setUpscaleConfig()` と 0.25〜1.0 の内部 render scale があり、render target の幅・高さを縮小する経路が存在する。`PreviewSettings` にも resolution scale の基礎設定があるため、単純な spatial downscale の入口はある。また `ArtifactCore/include/FSR/ffx_fsr1.h` は参照ヘッダとして存在する。しかし、FSR の実行 shader／adapter、DLSS／XeSS SDK接続、低解像度入力から表示解像度への明示的な GPU upscale pass、motion vector／depth／jitter／exposure／history reset 契約、実効方式の診断表示は確認できない。現段階は UPS-0 の既存スケール入口のみで、UPS-1以降は未着手と判定する。

## Update 2026-08-15

- `ArtifactIRenderer::setUpscaleConfig()` と 0.25〜1.0 の render scale による低解像度 render target 経路を再確認。これは方式選択付きの upscaler ではなく、既存の spatial scale 入口に留まる。
- FSR／DLSS／XeSS の実行 adapter、明示的な GPU upscale pass、motion／depth／jitter／exposure／history reset 契約、実効方式の診断表示は現行コードで確認できない。
- 判定は UPS-0 の既存スケール入口のみ実装、UPS-1 以降未着手。SDK／Diligent backend の追加変更や build／runtime 検証は行っていない。

## 目的

低解像度でコンポジションを描画し、最終表示解像度へGPUアップスケールする経路を整備する。まずベンダー非依存の契約と安全なフォールバックを完成させ、その上にAMD FSR、NVIDIA DLSS、Intel XeSSを同じ拡張点から接続する。

対象は当面、インタラクティブなComposition View / preview表示とする。静止画シーケンスや最終出力の品質を自動的に変えることは本マイルストーンの対象外とし、明示的な出力設定が整うまで出力経路はnative resolutionを維持する。

## 前提・制約

- 既存の `Preview/ResolutionPercent` を入力解像度の基礎設定として再利用する。
- software backend、非対応GPU、初期化失敗、履歴無効化時は、既存のnative / 簡易空間アップスケールへ確実にフォールバックする。
- アップスケーラーSDKをComposition/Layersへ直接参照させず、Artifactのrender境界にアダプターを置く。
- FSR/DLSSが要求する motion vectors、depth、exposure、jitter、reset条件を、フレーム契約として明示する。値を推測して埋めない。
- SDKの再配布、ドライバー要件、対応GPU判定、ライセンスは実装前に確認し、未確認のSDKバイナリやヘッダをリポジトリへ追加しない。
- Qtの合成、QImage、既存のReactiveEvents、新規のグローバルシグナルは使用しない。

## 完了条件

- preview設定から `Native`、`Spatial`、`FSR`、`DLSS`、`XeSS`、`Auto` を選択できる（利用不可の方式は選択不可または明確なフォールバック表示）。
- 低解像度入力、アップスケール出力、表示用presentのサイズと色空間が一貫している。
- カメラ移動、レイヤー変形、seek、停止・再開、composition切替、resize時に履歴が正しくリセットされ、ゴーストや過去フレームの混入を抑制できる。
- SDK未導入・非対応GPU・初期化失敗でもpreviewが表示され、診断情報から選択方式と実効方式を確認できる。
- FSR/DLSS有効時と無効時で、同一フレームのnative参照画像、フレーム時間、GPUメモリ、フォールバック理由を比較できる。
- 静止画レイヤー、連番画像、シェイプ、マスク、ブレンド、3Dレイヤーのpreviewで、方式切替によるクラッシュ・保存データ破壊がない。

## Work Packages

### UPS-0: 現状監査と契約固定

- DiligentのComposition View / preview render終端、present、既存resolution scaleの責務を特定する。
- `UpscalerMode`、quality preset、input/output extent、jitter、motion/depth/exposureの有無、history reset理由を含む最小フレーム記述を設計する。
- GPU backendだけを対象とし、software previewおよび最終出力経路との境界を文書化する。

### UPS-1: ベンダー非依存アップスケール境界

- `Native` と `Spatial`（まずは既存または小さなGPU空間アップスケール）を共通インターフェースで動かす。
- render target pool、入力/出力extent、resource state、resize時の再生成を整理する。
- SDKなしでも成立する診断、設定保存、フォールバック、A/Bキャプチャを完成させる。

### UPS-2: Temporal入力の整備

- jitter生成とprojectionへの適用、motion vectorの座標系・符号・スケール、depthのnear/farと反転規約を固定する。
- camera/layer transform、animated property、seek、composition変更、dirty/recomposeをhistory resetへ接続する。
- velocityが取得できないレイヤーやソフトウェア合成結果について、未定義のベクトルを生成せず安全な品質低下またはSpatialへ切り替える。

### UPS-3: AMD FSR統合

- FSR 2系を候補とし、SDKの再配布条件・対応API・D3D12/Diligent resource連携を確認する。
- FSR adapter、quality preset、sharpness、HDR/SDR、dynamic resolutionをpreview設定へ接続する。
- FSR初期化失敗、resource再生成、history reset、device loss、SDK不在の復旧を実装する。

### UPS-4: NVIDIA DLSS統合

- DLSS SDK、NVAPI/NGX等の依存と再配布・ランタイム要件を確認し、対応GPU判定を実装する。
- FSRと同一のフレーム契約を使うadapterを追加し、DLSS固有のfeature discoveryとquality mappingを隔離する。
- DLSS unavailable時にFSR、Spatial、Nativeへ決定的にフォールバックする。

### UPS-5: Intel XeSS統合

- XeSS SDKの再配布条件、対応API、D3D12/Diligent resource連携、対応GPUおよびDP4a経路の要件を確認する。
- FSR/DLSSと同一のフレーム契約を使うXeSS adapterを追加し、quality mapping、入力解像度、jitter、motion/depth、history resetを接続する。
- Intel GPUでは専用経路を優先し、非対応GPUやSDK不在時は利用可能な方式、Spatial、Nativeへ決定的にフォールバックする。

### UPS-6: UX・診断・検証

- Preview設定に方式、品質、resolution scale、実効方式、履歴状態を表示する。
- フレーム時間、アップスケール時間、入力/出力解像度、GPUメモリ、fallback reasonを既存診断へ記録する。
- 静止画／連番／シェイプ／マスク／ブレンド／3D、resize、seek、カメラ移動、device再初期化の受入ケースを固定する。

## 推奨順序

1. UPS-0 現状監査とフレーム契約
2. UPS-1 ベンダー非依存境界（Native / Spatial）
3. UPS-2 jitter・motion・depth・history
4. UPS-3 FSR統合
5. UPS-4 DLSS統合
6. UPS-5 XeSS統合
7. UPS-6 UX・診断・受入検証

## 依存・リスク

- M11.1 Hardware Render Integration、`ArtifactIRenderer`、Composition Viewのrender終端が主要な依存候補。
- 最大のリスクは、現在のレイヤー／合成結果から正しいmotion vectorsとdepthを得られないこと。ここを解決せずSDK統合を先行しない。
- FSR、DLSS、XeSSは同じ品質になるとは限らないため、同一preset名を無理に共有せず、共通の目標入力解像度と方式固有の品質マッピングを持つ。
- 実機検証なしで「対応済み」と判定しない。少なくともD3D12の対応GPU、非対応GPU、SDKなし構成を分けて確認する。

## ステータス

計画中。UPS-0の監査とフレーム契約設計から着手する。
