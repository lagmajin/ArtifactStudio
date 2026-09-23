# 2D デフォーマ統合計画

**最終更新:** 2026-09-23
**ステータス:** In Progress

## 目的

「Puppet Pin」を単独ツールとして完成させるのではなく、静止画・連番画像・シェイプを対象とする非破壊の 2D デフォーマへ拡張する。ピン、格子、曲線を同じ変形データと描画経路で扱い、時間評価、保存、Undo、GPU プレビューを共有する。

## 現状と前提

- `ArtifactPuppetTool` は静止画向けにピン／格子制御点の追加・移動・属性変更、オーバーレイ、Undo 導線を持つ。キー対象の x/y 制御点は既存プロパティキーフレームから評価する初期接続を追加。格子密度変更は既存頂点から補間する。
- `OpenCVPuppetEngine` はメッシュ生成と MLS 変形の基礎を持つ。一方、TPS／ARAP、GPU 描画、永続化、実機での品質と性能は完了扱いにしない。
- 旧来の `deformLayer()` は `toQImage()` → OpenCV → `setFromQImage()` で画像レイヤーを書き換えていた。静止画の通常描画は GPU 三角形描画へ非破壊接続済みだが、他の合成経路は引き続き未統合。
- 既存の Mesh Warp／Liquify、Layer Modifier、Rig2D は関連するが、エフェクト、変形データ、骨操作の責務を混同しない。

## 利用者の操作像

1. レイヤーを選び「2D デフォーマ」を追加する。元画像／形状は保持する。
2. 同じデフォーマ内で「ピン」「格子」を選び、表示中のワイヤーと制御点を編集する。静止画向けの初期ピン／格子編集が実装済み。
3. 変形はレイヤーローカル座標で保存し、時間位置に応じて制御点を補間する。通常のレイヤー変換、マスク、エフェクトとの適用順を明示する。
4. デフォーマの有効／無効、リセット、メッシュ再生成をInspectorまたは専用編集面から行う。通常のProperty Widgetにコンポーネント詳細を露出させない。有効／無効の保存・Undo導線はTool Optionsへ追加済み。

## データと評価の設計

- レイヤーに持続する `Deformer2D` 記述子を置く。ID、方式、基準領域、メッシュ生成設定、制御点、アニメーション、版番号を保存する。ツールは編集 UI のみ所有する。
- 共通メッシュは基準頂点、UV、三角形 index、変形後頂点を分ける。トポロジー変更は編集確定時に行い、通常フレームでは再生成しない。
- ピンと格子はまず別の制御方式として同じメッシュ出力契約に合わせる。曲線／ケージ、複数デフォーマの順序付け、Rig2D 連携は後段で検討する。
- ソース画像を不変とし、フレーム評価では制御点 → 変形頂点 → GPU の UV メッシュ描画へ進む。CPU 画像ワープは最低限の互換フォールバックに限定する。
- 座標系、範囲外制御点、退化三角形、アルファ端のにじみ、マスク・エフェクトとの順序を設計レビューで確定する。
- ホットパスでは事前確保したバッファを再利用し、`QImage` 変換、画像の深いコピー、メッシュ再生成、無制限のコンテナ拡張を行わない。

## 段階と完了条件

| 段階 | 作業 | 完了条件 |
|---|---|---|
| 0. 境界確定 | 現行 Puppet／Mesh Warp／Modifier／renderer の呼び出しと保存経路を監査。適用順と対象レイヤーを決定 | 所有者、座標系、GPU 描画への接続点、既存プロジェクトの扱いが文書化される |
| 1. 非破壊のピン | ピン状態をレイヤー側の永続データへ移し、ソースを保持したまま評価。既存操作と Undo を移行 | 初期コード済み。未ビルド・未実機、Undo 往復・保存再読込の確認待ち |
| 2. GPU メッシュ表示 | Diligent の既存描画経路に UV 付き変形メッシュを渡す。ワイヤー表示と選択表示を編集時に提供 | 静止画の一部経路へ接続済み。GPU プレビュー／通常描画／書き出し一致の確認待ち |
| 3. アニメーション | ピン位置・回転・重みを既存のプロパティ／キーフレーム経路に接続。トポロジーはアニメーション中固定 | x/y/回転/weightのコード接続と左Timelineの専用行を追加。再生・スクラブ・保存／再読込・Undo／Redoの一貫性確認が残る |
| 4. 格子方式 | NxM の持続メッシュと頂点／領域編集を共通出力へ追加。既存 Liquify エフェクトとは用途と導線を整理 | NxM 直接頂点編集・保存・密度変更補間の初期コード済み。領域編集と統合確認待ち |
| 5. 拡張判定 | 曲線／ケージ、局所剛性、複数メッシュ、ARAP／TPS、Rig2D 連携の需要と性能を評価 | 品質上必要な方式だけを別マイルストーンに切り出す |

## 実装順

Phase 0 の監査後、「静止画レイヤー + ピン + 非破壊評価 + 保存／Undo」を実装し、静止画用格子、連番の同フレームImageF32テクスチャ描画とフレーム単位の制御キー操作を接続した。連番は後続フレームのシルエット移動を覆うため、初回bindで全フレーム矩形のトポロジーを作る。通常GPUベクター描画のShapeにも同じ制御点状態とローカル点写像（PinsはMLS、Gridは双線形）を接続した。画像の通常GPU描画ではsource cropの矩形・UV・表示回転をメッシュに接続した。次はShapeのmask／非対応effect surfaceと画像のmask／effect surfaceを共通変形出力へ接続し、連番・Shape・cropの再生、Undo、保存往復を確認する。

## 実装進捗 (2026-09-23)

- `ArtifactAbstractLayer` に `deformation2D` JSON の保持・レイヤー保存／読込経路を追加した。
- Puppet pin の id、基準位置、現在位置、種別、回転、weight、depth をレイヤーローカル座標で保存し、選択レイヤーの表示／ヒットテストで遅延復元する経路を追加した。
- ピン追加・削除・属性編集・ドラッグ確定・Undo 適用でデータを同期する。
- OpenCV MLS のメッシュ出力を既存のテクスチャ付き三角形描画へ接続し、対象画像の `setFromQImage()` 書き戻しを除去した。MLS 更新では頂点ごとの動的ウェイト配列をなくし、描画時は更新済みメッシュを参照する。
- Puppet ツールのオプション面に「ピン／格子」と行列数を追加し、既存 `ToolOptionChangedEvent` 経路で静止画レイヤーの状態を切り替える。格子の制御点は行優先 NxM 配列として保存し、格子線と頂点を表示する。
- `OpenCVPuppetEngine` に規則格子から alpha-aware メッシュ頂点へ双線形に写す経路を追加した。方式切替時は同一モード内の制御点を初期化し、Pins と Grid の別データはプロジェクト内で保持する。
- `deformation2D.<controlId>.x/y` をレイヤーの動的プロパティとして Timeline Keyframe Model と Undo 復元経路から解決し、ピン／格子制御点位置をCompositionフレームで評価する。ドラッグ中はプレビュー位置を更新し、マウス解放でキーを確定する。
- ピンの `rotation/weight` も動的プロパティ化し、JSON保存、Timeline行、キーフレームUndo／Redo、同一フレームでの再評価に接続した。位置と属性キーには補間、Bezier接線、roving、anchor、color labelを保存する。
- Deformer制御点は通常の Property Widget グループに追加せず、Timeline内の折りたたみ可能な専用グループへ表示する。Dope Sheetキー収集、選択編集、レイヤー時間シフト、キー属性変更、Undo復元は動的プロパティを解決する。
- **未完了:** 左Timeline行と位置／属性キー編集のコードは追加したが未ビルド・未実機で、保存再読込、Undo／Redo、カーブ編集、クリップ時間シフトの動作は未確認。Tool OptionsにDeformer有効／無効を追加し、`deformation2D.enabled`へ保存して状態Undoへ接続した（欠落値はtrueとして後方互換）。GPUメッシュ経路は静止画と連番の通常画像描画に接続した。source cropはImageLayerから描画矩形・source pixel矩形・回転をまとめたlayoutを取得し、mesh UVを元フレームへ戻して通常GPU描画に接続した。Sequenceは初回bind時に矩形トポロジーを作り、同解像度の各フレームImageF32バッファをテクスチャとして描く。source cropもcrop矩形が変わったときだけトポロジーを再bindする。crop回転を制御点表示・ヒットテスト・ドラッグにも反映する。いずれもレイヤーマスク／ラスタライズエフェクトなしの通常画像GPU分岐に限る。Grid切替、ドラッグキー確定、キーUndo復元からsequence限定拒否を除去し、通常画像と同じComposition-frame評価へ接続した。矩形メッシュ品質・性能、effect mask／非対応effect、レイヤーマスク合成、書き出し一致は未実施。Shape通常GPU vector drawはPins/Gridを接続したが、レイヤーマスクまたはGPU plan非対応effectでは未適用。2026-09-23の静的監査で、`restoreLayerData()` はUndo対象control IDのキャッシュ済みdynamic propertiesを消してからJSON状態を再水和する必要があり、その順序を追加した。未ビルド。
- **格子の制約:** 初版は規則格子頂点を直接ドラッグする方式で、領域選択、滑らかさ制約は未実装。密度変更では旧制御点から双線形補間する。方式・密度変更と制御点の追加／移動／削除は状態 Undo に接続したが、未実機確認。
- **Shape接続 (2026-09-23):** 通常のComposition GPUベクター経路から Shape の描画へ制御点写像コールバックを渡し、塗り三角形とストローク点を共通のローカル座標写像で変形する。Pinsは既存OpenCV MLSと同じ制約点生成／剛体MLS式、Gridは格子制御点の双線形評価を使う。変形無効時および制御点なしでは通常描画へ戻す。C++モジュール依存を避けるため、Shape公開APIは関数ポインタのコールバック契約を使う。GPU effect planに適合しレイヤーマスクが無い場合は後段のGPU pointwise/spatial effectとGPU matteを維持する。レイヤーマスクまたはGPU plan非対応のeffect（CPU effect、effect mask/region、mix等）はCPU surface経路になりDeformer未適用。他のレイヤー内蔵物理格子との同時使用、再生・実機品質・性能は未確認。未ビルド。

## 2026-09-23 静的監査メモ

- Composition側のGPU描画フックは `ArtifactCompositionRenderController` の「ラスターエフェクト／マスクなし・current frame bufferあり」の画像分岐内にある。ImageLayerからsource cropのsource pixel rect、output local rect、回転transformをまとめたlayoutを取得し、crop矩形をトポロジー、元フレーム全体に対するUVをテクスチャ参照として渡す。ImageLayer直接描画も同じlayoutを使う。crop rect変更時はcold bindする。crop回転はoverlay制御点表示とヒットテストにも適用する。フレーム内 `QImage` 変換やCPU warpは追加しない。
- Sequenceの通常描画は `refreshSequenceFrameForCurrentTime()` と `currentFrameBuffer()` を使い、同解像度でないフレームを取り込まず、resolved frame index とcontent keyをGPU texture cache identityに含める。Deformer GPU pathも同フレームImageF32をテクスチャに使う。静止画は初回の8-bit alpha輪郭をメッシュ化し、Sequenceは同解像度のフレーム間でシルエットが移動しても覆えるよう初回bind時に全矩形を不透明にしたトポロジーを作る。いずれも同寸法中はメッシュを再生成しない。Sequence矩形メッシュの品質・性能は実素材で未確認。
- shapeの通常描画は `ArtifactShapeLayer::draw()`、surface合成は `drawLayerForCompositionView()` を通る。通常GPUベクター描画ではShape Layerの平坦化された三角形／ストローク点へDeformerのローカル点写像を適用する。GPU effect planが成立しレイヤーマスクが無い場合は、この後GPU effect/matteへ進む。レイヤーマスクまたはGPU plan非対応のeffectはQImage surface経路へ切り替わり、Deformerは未適用。現方式はパス頂点単位の評価で、画像用のUVメッシュとは別に描画頂点を写す。実機品質・性能は未確認。単純な `toQImage()` 変形統合は採用しない。
- `restoreLayerData()` はJSONのcontrol状態を適用した後、control IDごとの永続property cacheを破棄し、制御点JSONから再水和する。これによりUndo/Redo時の古いキーキャッシュ再利用を防ぐ。実行時の往復確認は未実施。

## 受け入れ確認

- ピンを動かしてもソース画像の画素とプロジェクト内の元データは不変。
- 複数フレームを往復しても結果が決定的で、保存／再読込後も同じ。
- 通常変換、マスク、エフェクト、アルファ境界で表示と書き出しが一致する。
- ピン追加・移動・削除、メッシュ再生成、方式変更が Undo／Redo と選択復元に対応する。
- ドラッグと再生でフレームごとの画像変換／大容量確保がなく、GPU 不可時のフォールバック範囲が明示される。

## 関連計画

- [Puppet Engine](MILESTONE_PUPPET_ENGINE_2026-03-29.md): ピン方式の既存基盤。旧来の単独完了条件はこの統合計画で再評価する。
- [Mesh Warp / Liquify](MILESTONE_MESH_WARP_LIQUIFY_2026-06-02.md): 格子方式とブラシエフェクトの境界を Phase 4 で整理する。
- [Layer Modifier System](MILESTONE_LAYER_MODIFIER_SYSTEM_2026-06-13.md): 既存スタックを再利用できる範囲を Phase 0 で確かめる。
