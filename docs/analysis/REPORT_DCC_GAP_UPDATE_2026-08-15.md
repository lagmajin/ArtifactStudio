# AE・他 DCC 機能差ギャップ最新レポート — 2026-08-15

**最終更新:** 2026-08-18
**調査対象:** ArtifactStudio 親リポジトリ、`Artifact`、`ArtifactCore` の現行ソース
**調査方法:** 既存の 2026-07-28 DCC レポートおよび 2026-08-08 3D レポートを基準に、現行ファイル・シンボル・直近コミットを再照合。ビルド、CMake、テスト、実ランタイム検証は未実施。

## 結論

7/28 時点の主要ギャップのうち、連番画像の時間駆動接続は現行コードで前進しており、「未接続」とは判定しない。`ImageSequenceSource`、`ArtifactImageLayer`、Project Service、Composition Render Controller の間に、読み込み・フレーム選択・描画経路が存在する。

現在の制作上の最大ギャップは、次の順である。

1. **静止画／連番／シェイプの実ランタイム受入れ確認**（コード接続は進んだが、欠番・フレームレート・再読込・保存復元を未確認）
2. **3D lookdev の実用化**（Material 編集、追加 Principled 値、shadow path、Environment Map data はあるが、IBL render binding と環境反射が不足）
3. **合成仕上げの完成**（3D の実ライティング、レイヤー横断の velocity 合成、Keyer の runtime 受入れ）
4. **DCC 間受け渡しと専用 UI**（AEP の実入力、字幕 UI）

## 前回から更新された判定

| 領域 | 現行判定 | 根拠 | 残る問題 |
|---|---|---|---|
| 連番画像 | 🟡 接続済み・受入れ未確認 | `Media.ImageSequenceSource`、`ArtifactImageLayer::setImageSequence`、Project Service、`ArtifactCompositionRenderController` | 欠番、開始フレーム、fps、seek、保存復元、キャッシュの一連の確認が未実施 |
| Track Matte | 🟡 UI・編集・評価あり | `LayerMatte`のstack/JSON/alpha・luma評価、`ArtifactLayerPanelWidget`のAlt-drag link、cycle guard、Undo/tooltip | 実コンポジションでのstack順・複数matte受入れが未確認 |
| 3D Material | 🟡 編集・描画接続あり | 3D Layer の Material property group、JSON、`ArtifactIRenderer::drawMesh`、`MeshRenderer::setPbrFactors/setPrincipledFactors` | IBL/environment map の render binding、反射プローブ、runtime受入れが未確認 |
| Environment Map | 🟡 render接続済み・runtime未確認 | `ArtifactEnvironmentMapLayer` の HDRI path / intensity / rotation / background visibility、Composition Render Controller、`ArtifactIRenderer`、`MeshRenderer` の IBL／Skybox binding | GPU prefilter、Inspector/preset UI、runtime受入れが未確認 |
| 3D Shadow | 🟡 render path あり | `MeshRenderer::prepareShadow/drawShadow/setShadowMap`、3×3 PCF softness、`ArtifactIRenderer` shadow pass | 複数ライト・品質設定・実シーンでの受入れが未確認 |
| Lottie | 🟡 実装痕跡あり | `Export/Lottie/LottieTypes.ixx`、`LottieExporter.ixx`、`LottieRigExporter.ixx` | 対応範囲と round-trip は未検証 |
| PSD | 🟡 実装あり | `PSDDocument`、`ArtifactProjectService::importPsdLayersToCurrentComposition`、Project View の PSD action | 複雑な adjustment layer／スマートオブジェクト等の互換範囲は未確認 |
| AEP | ❌ importer 本体なし | `AEPImporterDescription` は AI description のみ。AEP拡張子アイコンは存在するが機能実装ではない | parser、変換、UI 入口が未実装 |
| Planar Tracker | 🟡 UI・適用経路あり | `ArtifactCore/src/Tracking/PlanarTracker.cppm`、`CompositionRenderController::trackerUsePlanarMode/trackerTrackAll/trackerApplyToPosition`、`ArtifactPointTrackerGizmo` | 実データでの品質、失敗時復旧、4点ROIの編集受入れが未確認 |
| Keyer | 🟡 パス＋Inspector編集あり | `IBKKeyer` の `despillStrength` / `edgeMatteSoftness`、GPU constant buffer、`IBKKeyerEffect::getProperties/setPropertyValue`、共通Inspector | GPU/CPU差、clean plate workflow、実データ受入れが未確認 |
| Motion Blur | 🟡 Composition pass接続あり | `TimeRemapEffect::applyMotionBlur`、`MotionBlurPass`、timeline settings、pipeline velocity SRV/channel | 実フレームでの速度境界・深度遮蔽・runtime受入れが未確認 |
| Subtitle | 🟡 SRT/WebVTT↔TextLayer導線あり | `OtioAdapter::importSrt/importWebVtt/exportSrt/exportWebVtt`、CompositionEditorのSRT/WebVTT import／Text keyframe生成／選択Text Layer export | Caption Panel、cue編集、NLE Store永続化、独立Caption track exportが未実装 |
| Loudness | 🟡 Core＋meter UIあり | `AudioSpectrum` の momentary/integrated LUFS、true peak、target normalization、`AudioMixerWidget`のLUFS/TP表示 | BS.1770相当の実測確認、integrated運用、校正が未確認 |
| Keyframe workflow | 🟡 UI・保存・評価あり | TimelineのRoving操作、Source Text keyframe操作、`ArtifactTextLayer`のJSON保存/復元・current-frame評価 | runtime受入れと複雑な補間ケースが未確認 |

## 優先ギャップ

### 1. 連番画像の受入れ契約を固める

**カテゴリ:** 品質・制作導線　**規模:** 中　**確度:** 高

`ImageSequenceSource` は frame path の展開、source frame と sequence index の変換、LRU キャッシュ、prefetch、seek を持つ。Project Service は sequence frame rate を `ArtifactImageLayer` に設定し、Render Controller には image sequence 判定がある。したがって、旧レポートの「ImageSequenceSource → ImageLayer::draw が未接続」は現行ソースでは解消扱いに更新する。

ただし、コードの存在だけでは DCC の footage 解釈として十分とは言えない。欠番、開始番号、異なるサイズ、fps 変更、保存→再読込、キャッシュ無効化、フレーム外参照の仕様を受入れ条件として固定する必要がある。

**次に確認:** 既存の sequence import / render path を対象に、最小の静止画・欠番・保存復元ケースを runtime で確認する。

### 2. 3D Material Editor とライティングの縦統合

**カテゴリ:** 新機能・UX　**規模:** 大　**確度:** 高

8/8 以降、`Material` は DCC 標準に近い Principled パラメータへ拡張された。これは 3D layer のデータ基盤として前進し、Material property group→`ArtifactIRenderer::drawMesh`→`MeshRenderer`のsetter／shader定数、shadow map pathまで接続されている。一方、Material Editorの専用surface、IBL/environment map、reflection probeの縦統合までは確認できない。

そのため「値を持てる」状態と「lookdev できる」状態の間にギャップがある。UI を先に増やすのではなく、material 値→render pass→viewport preview→save/load の一本の契約を作るのが優先である。

**次に確認:** PBR material の setter が実際の shader/render path に反映されるか、環境光なし／あり、影あり／なしの比較を runtime で確認する。

### 3. Matte / Keyer / Motion Blur の制作操作を完成させる

**カテゴリ:** 合成・UX　**規模:** 中〜大　**確度:** 高

Matte は Core 側に `MatteStack`、JSON化、alpha/luma評価があり、Timeline側にもAlt-drag link、cycle guard、Undo、tooltipがある。KeyerはIBKのGPU/CPU処理、Inspector編集、despill／edge softnessまで存在する。Motion Blurもshutter設定、Compositionの`MotionBlurPass`、velocity SRV、depth入力まで接続されている。残るのは実コンポジション・実フレームでの受入れ確認と、複数matte／clean plate／速度境界の品質確認である。

**推奨順:** Track Matte の既存タイムライン導線 → Keyer の UI と spill/edge → velocity pass。

### 4. 受け渡し機能の旧判定を再分類する

**カテゴリ:** ドキュメント整合　**規模:** 小〜中　**確度:** 高

旧レポートの「AEP / PSD / Lottie import ❌」は、現行ソースと一致しない部分がある。Lottieは型・import/export API・rig exporterが存在し、PSDはレイヤーimportとProject View入口が存在するため、いずれも互換範囲・runtime・round-trip未検証と記述するべきである。AEPのみ、今回のソース走査では同等のimporter本体を確認できなかった。

Planar Tracker も「未実装」ではなく、Core・専用UI・適用workflowまで現行ソースに存在する。残りは実データでの品質、失敗時復旧、4点ROI編集の受入れ確認である。

## 継続して未実装または未確認の大きな差分

CEA-708、AEP import、Motion Sketch、Resolve 相当の Qualifier / Power Window、Nuke 的 deep data / pass merge、チーム制作基盤は、今回の現行ソース走査でも完成実装を確認できなかった。OFXはホスト状態表示、再スキャン、読み込み済みプラグイン一覧のツールチップまで追加したが、enable/disable、検索、詳細メタデータ編集を含む完全なManager UIではない。Planar Trackerは専用UIと適用経路まで存在するが、実データ受入れが未確認。PSD と Lottie は実装経路があるが互換範囲未確認。SRT/WebVTT はCore入出力のみでアプリUI未接続、LUFSはmeter UIまで追加済み、RovingとSource Text KeyframeはUI・保存・評価まで存在するため、「未実装」ではなく残る責務とruntime受入れを分けて扱う。

## 調査上の注意

### Subtitle UI の統合境界

字幕UIを追加する場合は、`NLEProjectStore`を一時的なダイアログ所有にせず、Project／Composition寿命に紐づくStoreとして保持する必要がある。最低限、(1) active sequenceの選択、(2) SRT/WebVTT import時のcue検証と警告表示、(3) cueの追加・更新・削除とUndo、(4) Project保存・再読込へのStore JSON組み込み、(5) Composition／Timeline表示との同期、(6) SRT/WebVTT exportの出力先選択を同じ責務境界で定義する。現行Artifact側にはこのStore所有者とComposition同期契約が存在しないため、単独の字幕ダイアログを追加して完了扱いにはしない。

- `Artifact` と `ArtifactCore` は 2026-08-15 時点で Principled material 関連コミットが最新に入っているため、旧レポートの 3D material 判定をそのまま流用しない。
- ドキュメント、git commit 名、型定義だけでは runtime 完成とは判定しない。
- キーワード検索で hit がない場合でも、`OtioAdapter`、`AudioSpectrum`、`TimeRemapEffect`、Timeline の操作コードのように、名称が異なる既存経路を先に確認する。
- 今回はユーザー指示により build / test / CMake を実行していないため、「実装あり」は静的なソース確認、「完成」は未判定である。
