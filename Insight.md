# Insight Log

実装・調査中に得た、将来の改善案、設計上の仮説、再利用できる知見を記録する。

このファイルの内容は仕様や実装指示ではない。採用・優先順位付け・実装の可否は、別途ユーザーまたは設計レビューで判断する。

## 記録ルール

- 事実と推測を分ける。
- 未検証の内容には `未検証` と付ける。
- 依頼外の変更を避けるため、記録だけで実装を始めない。
- 関連ファイルや次の検証方法を残し、後から再開できるようにする。

## Insights

### 2026-08-10 — Playback audio fill の異常フレームレート境界

- 状態: 実装済みの局所防御。実機再生での挙動は未検証。
- 関連: `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- 事実: 音声先読み量は `audioSampleRate / framerate` から計算され、従来は `1e-6` を下限にした値を無制限に `int` 化していた。音量 setter も非有限値を `std::clamp` に渡していた。音声クロック同期にも同じ下限と、補正フレームの無防備な `int64_t` 化が残っていた。音声なしへ遷移する経路では、デバイス停止後のバッファと先読み状態の扱いも分散していた。
- 閃き・仮説: 再生エンジンの入力境界では、異常値を小さな既定値へ黙って変換するより、診断を残して音声補充を停止する方が巨大な要求量や未定義変換を避けやすい。
- 価値・懸念: 低フレームレート・非有限フレームレート・NaN 音量による音声バッファ破壊や長時間ハング、クロック補正値の異常化、音声なし遷移後の stale buffer リスクを下げる。一方、特殊な低速タイムラインを正式対応する場合は別途上限仕様が必要。
- 次の確認: 明示許可後に、異常な frame rate と volume を含む再生ケースでログ、音声停止、通常値への復帰を実機確認する。

### 2026-08-10 — Core AudioVolume の直接利用境界

- 状態: 実装済みの局所防御。Core の各利用箇所を網羅した runtime 検証は未実施。
- 関連: `ArtifactCore/src/Audio/AudioVolume.cppm`
- 事実: サービス経由の音量変更は有限値へ正規化されていたが、`AudioVolume` 自体の constructor、setter、補間、算術演算、dB 変換には `std::clamp` や算術結果を非有限値のまま受け入れる経路があった。
- 閃き・仮説: 境界の正規化を呼び出し側だけに分散させず、値オブジェクトの生成・変更・演算点に置くと、UI・サービス・Core 直利用の経路で同じ不変条件を保ちやすい。
- 価値・懸念: NaN 音量がミュート判定、dB 変換、ミキサー計算へ漏れる可能性を下げる。`-∞ dB` は既存の無音意味を保ち、極端な有限 dB は 0〜2 に飽和させた。
- 次の確認: 明示許可後に、NaN / ±∞ / 極端値 / 0 除算を含む AudioVolume の単体または既存 Core 検証経路で、有限値と既存ミュート semantics を確認する。

### 2026-08-10 — Core AudioDecibels の有限値境界

- 状態: 実装済みの局所防御。変換・演算の runtime 検証は未実施。
- 関連: `ArtifactCore/src/Audio/AudioDecibels.cppm`
- 事実: dB 値オブジェクトの constructor、setter、補間、算術演算は `std::clamp` に非有限値を渡し得た。`fromLinearValue` も NaN / +∞ を明示的に扱っていなかった。
- 閃き・仮説: `AudioVolume` と `AudioDecibels` の相互変換境界で有限性を保証すると、ミキサーや UI がどちらの表現を使っても同じ不変条件を受け取れる。
- 価値・懸念: NaN dB の伝播を抑え、無音（−60 dB）と最大値（20 dB）の意味を維持したまま、極端な線形値を定義済み範囲へ収める。一方、実際の effect chain が例外値を生成しないかは未検証。
- 次の確認: 明示許可後に、AudioVolume↔AudioDecibels の NaN / ±∞ / 0 / 極端値変換と補間を既存 Core 検証経路で確認する。

### 2026-08-10 — AudioBus の PCM overflow とメータリング境界

- 状態: 実装済みの局所防御。実時間 effect chain の負荷・音質確認は未実施。
- 関連: `ArtifactCore/src/Audio/AudioBus.cppm`
- 事実: Bus のゲイン適用は有限な入力でも `float` overflow を起こし得た。overflow 後の soft-clip 近似は `∞/∞` になり得て、RMS 集計も float の二乗加算で非有限化する余地があった。
- 閃き・仮説: PCM の有限性を effect 処理後だけでなく、ゲイン適用直後にも保証し、メータ集計を double に分離すると、出力と表示値が同じブロック内で NaN 化する経路を減らせる。
- 価値・懸念: 異常に大きい入力やゲインでも soft-clip が有限値として動き、メータの非有限値伝播を抑えられる。飽和点付近のメータ表示は実機音量・既存仕様との確認が必要。
- 次の確認: 明示許可後に、最大値近傍の PCM、通常ゲイン、FX 後の極端値で output / peak / RMS が有限であることを確認する。

### 2026-08-10 — AudioPanner の直接 API 入力境界

- 状態: 実装済みの局所防御。各 panning mode の実機・聴感確認は未実施。
- 関連: `ArtifactCore/src/Audio/AudioPanner.cppm`
- 事実: `calculateGain` と `calculateConstantPowerGains` は非有限または範囲外の pan / azimuth を直接計算し、`applyPanning` はゲインと PCM の非有限性を検査していなかった。
- 閃き・仮説: AudioBus 経由だけでなく、公開されている Panner API 自体で pan を正規化すると、別の effect / UI 経路からの NaN 伝播も抑えられる。
- 価値・懸念: 不正な pan で左右ゲインが NaN になったり、直接適用で PCM が壊れたりするリスクを下げる。異常ゲインは無音へ寄せ、overflow は有限の最大値へ飽和させた。
- 次の確認: 明示許可後に、NaN / ±∞ / ±2 の pan と最大 PCM を各公開 API に通し、有限出力と左右定位を確認する。

### 2026-08-10 — AudioSegment の共通 frame 数契約

- 状態: 実装済みの局所防御。既存 caller の不揃いチャンネル入力に対する runtime 互換性は未検証。
- 関連: `ArtifactCore/include/Audio/AudioSegment.ixx`、`ArtifactCore/src/Audio/AudioDownMixer.cppm`、`ArtifactCore/src/Audio/AudioBus.cppm`
- 事実: `frameCount()` は「全チャンネルのサンプル数」というコメントに反して channel 0 の長さだけを返していた。downmix / mix 側には各チャンネル長を個別に clamp する処理もあるが、共通ループ上限としては過大になり得た。
- 閃き・仮説: AudioSegment の共通 frame 数を最短チャンネル長に定義すると、public struct に不揃いデータが入った場合も、上流のループ上限自体が安全側に揃う。
- 価値・懸念: panning、downmix、mixer の out-of-range リスクを下げる。長いチャンネルの余剰サンプルを暗黙に捨てる契約になるため、入力生成側で不揃いが発生していないかは別途確認が必要。
- 次の確認: 明示許可後に、0 / 1 / 不揃い / 負の setFrameCount を含む AudioSegment を既存音声経路へ渡し、共通 frame 数と出力長を確認する。

### 2026-08-10 — AudioDownMixer の出力有限性境界

- 状態: 実装済みの局所防御。極端係数での音量・聴感確認は未実施。
- 関連: `ArtifactCore/src/Audio/AudioDownMixer.cppm`
- 事実: downmix の係数 setter は有限な任意値を許容し、複数チャンネルの加算結果を float のまま出力していた。巨大係数や極端な PCM では overflow、符号の異なる無限大の加算では NaN が起こり得る。
- 閃き・仮説: 各レイアウト分岐の算術を個別に変更せず、pass-through を含む `process()` の出力境界で有限値を保証する方が、変換マトリクスを増やさずに共通契約を守れる。
- 価値・懸念: downmix 後の NaN / ±∞ が mixer や renderer へ漏れるリスクを下げる。NaN は 0、±∞ は float 最大値へ飽和させるため、極端値の聴感は別途確認が必要。
- 次の確認: 明示許可後に、各レイアウト・巨大係数・極端 PCM で downmix 出力と audio bus の有限性を確認する。

### 2026-08-10 — AudioRingBuffer の read 境界と layout 復元

- 状態: 実装済みの局所修正。SPSC の同時実行と hardware callback の runtime 検証は未実施。
- 関連: `ArtifactCore/src/Audio/AudioRingBuffer.cppm`
- 事実: `read(..., 0)` は成功扱いのまま cursor を進めず、読み出し channel 数にかかわらず `AudioChannelLayout::Stereo` を設定していた。
- 閃き・仮説: callback 側の要求量が 0 になる境界では明示的に失敗・空出力を返し、既知の channel 数から layout を復元すると、再試行ループと不要な downmix を避けられる。
- 価値・懸念: zero-length read の曖昧な成功状態と、6 / 8ch の誤った metadata 伝播を防ぐ。sample rate は ring buffer が保持していないため、既存 caller の設定責務は残る。
- 次の確認: 明示許可後に、0 frame / 空 buffer / mono・stereo・5.1・7.1 の read と consumer の layout 判定を確認する。

### 2026-08-10 — AudioParametricEQ の band 計算入口

- 状態: 実装済みの局所防御。極端な EQ 設定での周波数応答は未検証。
- 関連: `ArtifactCore/src/Audio/AudioParametricEQ.cppm`
- 事実: `process()` は band の frequency / gain / Q を `std::clamp` と `std::pow` へ直接渡していた。`setParameterValue()` も非有限値を先に拒否していなかった。
- 閃き・仮説: setter・JSON だけでなく、リアルタイム process の計算直前で値を再正規化すると、内部状態や将来の別入力経路からの NaN も係数計算へ入らない。
- 価値・懸念: EQ biquad の NaN 係数・状態汚染を防ぎ、計算後の fallback だけに依存しない。無効値は frequency 1000Hz、gain 0dB、Q 1 の既定値へ寄せた。
- 次の確認: 明示許可後に、非有限 / 極端な band 設定で係数・state・出力が有限であることを確認する。

### 2026-08-10 — Timeline / Property 編集経路の再計算と未利用基盤を改善候補として記録

- 状態: 改善候補・未実装。実行時の呼び出し頻度と効果は未検証。
- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/PropertyEditor/*`、`ArtifactCore/src/Property/AbstractProperty.cppm`、`ArtifactCore/src/Script/ScriptContext.cppm`、`docs/analysis/PERFORMANCE_ASYNC_GPU_OPTIMIZATION_2026-08-06.md`
- 事実: Timeline の keyframe marker / connection segment 収集、Layer Panel の property search、Timeline の audio waveform 生成、Property の式評価と keyframe 読み出しに、描画・検索・再生の更新単位より広い再計算が残っている箇所がある。`PropertyEditor` には共通 row/editor 基盤があり、旧 Knob 系は監査上 legacy / stub と整理されている。Project open/save には async 経路があるが、File Menu・recent project・recovery・auto-save の利用経路は統一されていない。
- 閃き・仮説: composition、track、selection、visible range、expression text、property group 構造、waveform source の変更をそれぞれキャッシュ無効化条件にすれば、playhead 移動や通常の再描画で同じ構造を作り直さずに済む可能性が高い。既存の async API と `CachedAudioWaveform` を正規経路へ寄せると、機能追加より小さい変更で操作中の停止感を減らせる可能性がある。
- 価値・懸念: Timeline のスクラブ、プロパティ検索、式付きレイヤー、波形表示、大規模プロジェクトの open/save の体感改善につながる。一方、cache invalidation、式編集時の AST 更新、保存中の編集との整合性、thread safety は設計確認が必要であり、場当たり的な import / signal / module 追加は避ける。
- 次の確認: 各経路の実呼び出し頻度を既存 profiler / trace で確認し、最初に marker cache または AST cache のどちらか一つを小さな計測付き slice として比較する。ビルド・runtime 検証は明示許可後に行う。

### 2026-08-10 — 診断・Render Queue・Composition Editor の「発見後の行動」導線を改善候補として記録

- 状態: 改善候補・未実装。UI上の実操作とruntimeの使いやすさは未検証。
- 関連: `Artifact/src/Widgets/ArtifactProjectHealthDashboard.cppm`、`Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`docs/planned/MILESTONES_BACKLOG.md`
- 事実: Project Health の診断行には diagnostic ID、composition ID、layer ID、asset path が保持され、tooltipには `Double-click to inspect` が設定されているが、同ファイル内に診断行のダブルクリック処理は見当たらない。Frame Debug は `goal / now / warning / next` の要約と resource 状態を持つ。Render Queue は retry、failed frames、history、export の部品を持つが、history表示は主に文字列行である。Composition Editor の viewport render output は通常の単一フレーム出力で `readbackToImage()` を直接呼び、Four-Upレイアウトでは未表示ペインの同期初期化を避ける意図がコメントされている。
- 閃き・仮説: 診断を「読むだけの表」から対象へのジャンプ、修復、再試行へつながる作業面にすると、既存の診断データを増やさずに価値を上げられる。Render Historyを構造化すれば、失敗理由とRetry Failed Framesを一体化できる。Frame Debugの`next`を原因別に生成すれば、利用者が次の操作を判断しやすい。
- 価値・懸念: 失敗から復旧までの往復を短縮できる。一方、UI間の既存責務境界、選択同期、Render Queue jobの永続ID、非同期readbackのキャンセルと出力境界を先に確認する必要がある。新規の中央集権イベント配線は追加しない。
- 次の確認: Project Health の診断行に現在接続済みの navigation / selection API があるか、Render Queue serviceがjob identityとfailed frame情報を公開しているか、Four-Upの非表示paneが本当に初期化されるタイミングを静的に追う。ビルド・runtime検証は明示許可後に行う。

### 2026-08-09 — 体積3Dレイヤーの bounding box ギズモを既存3Dギズモへ統合する

- 状態: 実装済み・実機未検証
- 関連: `Artifact/include/Widgets/Render/Artifact3DGizmo.ixx`、`Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: 体積3Dレイヤーの mesh bounding box を既存の3Dギズモへ同期し、8角・12辺中央・6面中央を描画・レイ判定する経路を追加した。角は非均等スケール、辺は2軸スケール、面は1軸スケールとして扱い、反対側のローカル面を固定する。View空間、個別ハイライト、通常ドラッグとモーダル操作の軸拘束、既存のShift/Ctrlによる比例・スナップ、数値HUD表示も既存経路へ接続した。
- 閃き・仮説: 既存の `GizmoAxis` と符号付きスケール拘束を再利用すると、専用の新しいイベント経路を増やさずに面・辺・角ごとの操作を接続できる。BB中心を移動・回転ギズモの基準に使うと、非対称メッシュでも表示位置とヒット位置を揃えられる。
- 価値・懸念: Blender/Maya型のハンドル構成に近づけられる。一方、非対称メッシュ、負スケール、透視投影でのヒット優先順位とサイズ感、回転時の実ピボット補正は未検証。
- 次の確認: ビルド・実機で角／辺／面ハンドルのヒット、反対面固定、View/World/Local切替、Shift/Ctrl、数値入力、Undo/Redo、カメラ回転、非対称メッシュとクリップ境界を確認する。

### 2026-08-03 — RenderControllerのviewport overlay・Rig表示・編集補助を統合する

- 状態: 確認済み
- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`
- 事実: viewport ruler／grid／safe margin／onion skin、Rig overlay／weight map、pose slot、mask操作、3D投影枠、overlay表示切替をRenderControllerとEditor／ViewMenuへ接続した。
- 閃き・仮説: 表示補助の状態をRenderControllerに集約し、Editorの入力とViewMenuの切替を同じAPIへ通すと、表示状態と操作状態の乖離を減らせる。
- 価値・懸念: 2D／3D／Rigの編集補助が同じviewport導線で扱える。変更範囲が大きいため、既存操作との回帰確認が必要。
- 次の確認: ビルド・実機でgrid／ruler／onion／safe margin、Rig pose、mask、3D frame、ViewMenu切替を確認する。

### 2026-08-03 — AIDSLのfilter評価とCommandAction解析を実装する

- 状態: 確認済み
- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`、`Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `and`／`or`を含むfilter式、数値・文字列比較、regex match、FrameExpr解決、符号付き整数、CommandActionの型を追加した。
- 閃き・仮説: DSLのparseとevaluateを分けたまま、式ノードの値解決を共通化すると、property filterを実データへ適用しやすい。
- 価値・懸念: AIDSLコマンドの条件指定とframe参照が実用化する。regex入力や演算子優先順位は追加テストが必要。
- 次の確認: ビルド・実機でand／or、数値／文字列比較、regex、frame context、無効式を確認する。

### 2026-08-03 — ViewportScaleOverlayの目盛り計算と表示要素管理を分離する

- 状態: 確認済み
- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`
- 事実: ズームに応じたnice step、ruler tick、scale bar、grid label、compassのデータ生成と、overlay要素のID／可視性／キャッシュ管理を独立モジュールへ整理した。
- 閃き・仮説: viewport overlayの数値計算を描画側から分離すると、ズーム・canvas範囲・単位表示の検証と再利用がしやすい。
- 価値・懸念: Ruler／scale bar等の表示実装を複数のviewportから共有できる。新規C++ moduleの登録と実描画接続は別途確認が必要。
- 次の確認: ビルドでmodule分類を確認し、実機でズーム端点、負原点、単位ラベル、overlay cacheを確認する。

### 2026-08-03 — CompositionRenderWidgetのズーム・パン操作を滑らかにする

- 状態: 確認済み
- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- 事実: アンカーを維持するsmooth zoom、pan momentum、ツール別cursor、zoom marquee／space hand状態、rotation snap設定を追加した。
- 閃き・仮説: ズーム中心をviewport座標とcanvas座標の組で保持し、アニメーション中も同じアンカーを再計算すると、視点が飛びにくい。
- 価値・懸念: ビュー操作の連続性とツール識別性が上がる。タイマー中のrender負荷とpan停止条件は実機確認が必要。
- 次の確認: ビルド・実機でwheel／button zoom、momentum pan、tool cursor、rotation snap、zoom marqueeを確認する。

### 2026-08-03 — CompositionMenuから選択的なRender Queue投入を追加する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`
- 事実: 現在コンポジション、現在フレーム、ワークエリア、選択レイヤー単位のRender Queue投入と、高度なRender設定画面への導線を追加した。
- 閃き・仮説: よく使う範囲指定をメニューに直接出し、複雑な条件だけRender Managerへ委譲すると、簡易操作と詳細操作を両立できる。
- 価値・懸念: レンダー準備の手数を減らせる。選択レイヤーや範囲設定のjob反映は実機確認が必要。
- 次の確認: ビルド・実機で各投入モード、job設定、詳細画面表示、アクティブコンポジションなしを確認する。

### 2026-08-03 — Keying effectの保存復元を実装クラスへ接続する

- 状態: 確認済み
- 関連: `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- 事実: Chroma／Luma／Difference Keyの識別子（新旧形式）をJSON復元時に各実装クラスへ割り当てるようにした。
- 閃き・仮説: エフェクトの登録・UIカタログだけでなく復元分岐も同じ識別子の別名を受けると、保存済みプロジェクトの互換性を保ちやすい。
- 価値・懸念: Keying effectの保存／再読込で実装固有プロパティを失いにくくなる。旧形式の全識別子は別途棚卸しが必要。
- 次の確認: ビルド・実機で3種Keying effectの保存、旧ID読込、Property復元を確認する。

### 2026-08-03 — Render Queue Managerの選択レンダー設定とFarm状態表示を拡張する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`
- 事実: フレーム範囲、解像度、ROI／crop、layer filter、pass分割、除外設定、選択layer利用のUIと要約表示を追加し、Farm worker／RPC状態も表示するようにした。
- 閃き・仮説: レンダー設定をjob単位で要約して表示すると、キュー投入後の対象範囲と除外条件を再確認しやすく、Farm状態と合わせて実行前の判断材料になる。
- 価値・懸念: 選択的レンダーと分散実行の可視性が上がる。設定UIの密度と既存jobとの復元互換性は実機確認が必要。
- 次の確認: ビルド・実機でselected range、ROI、layer filter、pass分割、Farm表示、既存job復元を確認する。

### 2026-08-03 — ToolOptionsBarのブラシ・消しゴム・Motion Sketch操作を拡張する

- 状態: 確認済み
- 関連: `Artifact/include/Widgets/ArtifactToolOptionsBar.ixx`、`Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`
- 事実: ブラシのflow／spacing／angle／roundness／jitter／筆圧・傾き、Clone時間ずれ、消しゴム詳細、Motion Sketch設定、色選択とTool同期を追加した。
- 閃き・仮説: オプションUIからTool本体へ同期する明示メソッドを持つと、再生成されるUIと永続化されたTool状態の不一致を減らせる。
- 価値・懸念: 編集パラメータの表現力とアクセシビリティが上がる。横幅・表示密度と色pickerの既存テーマ整合性は実機確認が必要。
- 次の確認: ビルド・実機で各spinbox／checkbox、色選択、Tool切替時同期、狭幅レイアウトを確認する。

### 2026-08-03 — Command Paletteからマスク追加をUndo対応で実行する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: Command PaletteのAdd Maskを実装し、選択レイヤー全体の矩形マスクを追加、UndoManager経由の復元、About項目の情報表示を接続した。
- 閃き・仮説: パレットからの編集コマンドも通常メニューと同じUndoCommandへ通すと、導線が増えても編集履歴の一貫性を保てる。
- 価値・懸念: マスク追加の発見性とUndo整合性が上がる。既存マスクのコピー／復元コストは別途確認が必要。
- 次の確認: ビルド・実機で選択レイヤーへのAdd Mask、Undo／Redo、対象なし、空サイズを確認する。

### 2026-08-03 — RenderMenuに一時停止・キャンセル導線と説明を追加する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm`
- 事実: レンダーキューの一時停止／全ジョブキャンセルを既存サービスへ接続し、開始・キュー操作へaccessible descriptionとstatus tipを追加した。
- 閃き・仮説: 長時間処理のメニューは開始だけでなく停止可能性を同じ場所に示すと、操作の可逆性と状態理解を改善できる。
- 価値・懸念: レンダー操作の発見性とアクセシビリティが上がる。pause／cancel後のキュー状態表示は実機確認が必要。
- 次の確認: ビルド・実機で開始／一時停止／再開／全キャンセルと無効化条件を確認する。

### 2026-08-03 — LayerMenuからRigレイヤー作成とTracker導線を接続する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- 事実: Rig Layer作成アクションを追加し、初期root bone／control pointを生成してRig Selectへ切り替える導線を実装した。Motion Tracker作成後はTrack Pointを選択するようにした。
- 閃き・仮説: レイヤー生成直後に対応する編集Toolを選択すると、作成結果を確認・操作するまでの導線が短くなる。
- 価値・懸念: Rig／Tracker機能の発見性と初期操作性が上がる。既存選択状態やroot bone生成失敗時の扱いは実機確認が必要。
- 次の確認: ビルド・実機でRig作成、root操作、Tracker作成済み／新規の両経路を確認する。

### 2026-08-03 — MainWindowのブラシ・モーションスケッチ設定を永続化する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- 事実: Brush／Eraser／Clone Stampのoption変更をBrushToolへ反映し、QSettingsへ保存・起動時復元する経路を追加した。Motion Sketchの主要設定も同様に接続した。
- 閃き・仮説: ToolOptionsBarが再生成されてもTool本体を単一の状態源にすると、UI再構築と設定永続化を分離できる。
- 価値・懸念: ブラシ設定の再利用性が上がる。設定キーの命名と旧版値の単位変換は互換性確認が必要。
- 次の確認: ビルド・実機でブラシ／Clone／Motion Sketchの変更、再起動後復元、異常値補正を確認する。

### 2026-08-03 — 3D選択枠オーバーレイの可視性とハンドルを強化する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`
- 事実: カメラclip空間で選択枠の可視コーナーを判定し、部分可視時の減衰、対角線、辺ハンドル、回転ハンドルを追加した。
- 閃き・仮説: 3D投影下の選択枠は全体が画面内にある前提を置かず、可視性に応じて枠と操作点を描き分けると操作対象を見失いにくい。
- 価値・懸念: 3Dレイヤーの選択・変形操作を認識しやすくできる。clip境界付近と透視投影での見え方は実機確認が必要。
- 次の確認: ビルド・実機で部分的に画面外の枠、辺／回転ハンドル、透視投影を確認する。

### 2026-08-03 — PointTrackerGizmoに特徴領域と信頼度の可視化を追加する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`
- 事実: Feature／Searchサイズ、軌跡上のconfidence色、現在フレームの強調点、平均confidence表示を追加した。
- 閃き・仮説: トラッカーの編集値と解析結果を同じGizmo上で示すと、サイズ調整と結果品質の関係を確認しながら操作できる。
- 価値・懸念: トラッキング状態の把握性が向上する。軌跡点数が多い場合の表示密度と文字位置は実機確認が必要。
- 次の確認: ビルド・実機でサイズ表示、confidence色、現在フレーム強調、長い軌跡の視認性を確認する。

### 2026-08-03 — TransformGizmoの回転スナップと比率維持リサイズを強化する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- 事実: Shiftによる設定可能な回転スナップ、コーナーハンドルのアスペクト比維持、リサイズ倍率表示、アンカー操作HUDを追加した。
- 閃き・仮説: 変形操作では表示中の補助情報と内部累積値を同じスナップ結果へ揃えると、マウス移動の継続時に表示と実値がずれにくい。
- 価値・懸念: 変形の再現性と操作フィードバックが向上する。Shiftの意味を既存操作と統一できているか、実機で確認が必要。
- 次の確認: ビルド・実機で回転スナップ、コーナー比率維持、自由変形、アンカーHUDを確認する。

### 2026-08-03 — TextGizmoの選択状態とプレビュー座標を安定化する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
- 事実: layer切替時にドラッグ状態をリセットし、weight／cluster／line境界とzoomの有限値・下限を補正した。
- 閃き・仮説: Gizmoは選択対象の切替と描画・hitTestが別経路で進むため、対象切替時の状態破棄と座標入力の正規化を同時に行うと残留操作や不定座標を抑えられる。
- 価値・懸念: レイヤー切替後の誤ドラッグとNaNによる表示・hitTest破綻を減らせる。ズーム端点での操作感は実機確認が必要。
- 次の確認: ビルド・実機でText layer切替中のドラッグ、極端なzoom、selector preview表示を確認する。

### 2026-08-03 — Inspectorのエフェクトカタログを実装済み効果へ広げる

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- 事実: Optics Compensation、Turbulent Displace、Luma／Difference Key、Liquify、Stroke系、Blur／Transition／Stylize系などのエフェクトカタログ項目を追加した。
- 閃き・仮説: エフェクト実装とInspectorの検索カタログを同じ識別子・カテゴリ・キーワードで揃えると、既存機能の発見性と追加導線を改善できる。
- 価値・懸念: 未発見だった効果を検索・カテゴリから選べる。カタログ識別子と登録サービスの対応漏れは別途確認が必要。
- 次の確認: ビルド・実機で各項目の検索、カテゴリ表示、追加後のProperty表示を確認する。

### 2026-08-03 — Project Viewの検索・フィルター条件を要約表示する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- 事実: Project Viewのtype filter、unused filter、検索語を要約するラベルを追加し、条件変更時に更新するよう接続した。
- 閃き・仮説: 選択数ではなく現在の絞り込み条件を明示すると、表示件数が変わった理由をユーザーが追いやすい。
- 価値・懸念: 検索・フィルター状態の見落としを減らせる。狭いProject Viewでは高さと情報密度の妥当性を確認する必要がある。
- 次の確認: ビルド・実機でfilter／検索語の変更、ラベル折返し、狭幅レイアウトを確認する。

### 2026-08-03 — ArtifactToolBarのツール導線と名称を拡張する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/ArtifactToolBar.cppm`
- 事実: 楕円、Clone、Rig Select、Rig Weight、Track Pointのツール表示・選択処理を追加し、既存ToolTypeへの切替とアクセシビリティ説明を接続した。
- 閃き・仮説: ToolTypeが先に存在する機能では、ツールバーの表示名・選択状態・ショートカット導線を揃えることが未接続機能の発見性を上げる。
- 価値・懸念: 新しい編集機能へ到達しやすくなる。アイコン資産の既存参照は将来Studio固有SVGへ整理する余地がある。
- 次の確認: ビルド・実機で各ツールの選択、再選択、アクセシビリティ情報、ショートカットを確認する。

### 2026-08-03 — TimelineLayerPanelのvariant名変換とdragEnterの境界を修正する

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`
- 事実: variant名のUniString変換を明示的なArtifactCore変換へ揃え、dragEnterEventのブロック境界を修正した。
- 閃き・仮説: モジュール境界の文字列型変換とイベント分岐の括弧は、暗黙変換や見た目上のインデントに頼らず明示化した方が再スキャン時の不具合を追いやすい。
- 価値・懸念: 型変換の解決とイベント処理の意図しない範囲実行を防げる。タイムラインの各drop形式は実機確認が必要。
- 次の確認: ビルド・実機でパスdropとeffect dropの両方を確認する。

### 2026-08-03 — SurfaceFXのプロパティ入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/SurfaceFX/SurfaceFXEffect.ixx`
- 事実: Surface anchor、feather、element矩形、rotation、intensity、opacity、roughness、時間値に有限値チェックと範囲補正を追加し、`<cmath>`を直接includeした。
- 閃き・仮説: SurfaceFXは複数の要素値を一括編集するため、個別setterがない境界でもproperty dispatch時に正規化すると異常値の混入を防ぎやすい。
- 価値・懸念: エフェクト入力からNaN／無限大が下流へ伝播するリスクを下げられる。時間値のfallbackは既存プリセットとの互換性を確認する必要がある。
- 次の確認: ビルド・実機でSurfaceFXの各property編集とプリセット適用を確認する。

### 2026-08-03 — PaintLayerのブラシ入力・クローンスタンプ・Undoを拡張する

- 状態: 確認済み
- 関連: `Artifact/include/Layer/ArtifactPaintLayer.ixx`、`Artifact/src/Layer/ArtifactPaintLayer.cppm`
- 事実: ブラシのflow／hardness／角度／roundness／各種jitterを追加し、決定的なdab揺らぎ、クローンスタンプ、フレーム単位のUndo、画像復元時のサイズ・有限値検証を実装した。
- 閃き・仮説: CPUペイントでは乱数エンジンを状態保存せず、dab位置とインデックスから決定的に揺らぎを作るとUndo／再適用時の見た目を揃えやすい。
- 価値・懸念: ブラシ表現と編集復元性が向上する一方、既存ストロークの見た目とメモリ使用量（画像Undo／クローン一時バッファ）は確認が必要。
- 次の確認: ビルド・実機でブラシ属性、クローン元／先フレーム、clear後Undo、異常画像復元を確認する。

### 2026-08-03 — LayerMaskのロック状態を実装側へ接続する

- 状態: 確認済み
- 関連: `Artifact/src/Mask/LayerMask.cppm`
- 事実: インターフェースに存在するLayerMaskのロック状態について、実装側の保持領域とgetter／setterを追加した。
- 閃き・仮説: マスク編集可否をレイヤー本体とは別のマスク単位で扱う場合、状態を実装に保持しておくことで後続の編集導線へ接続しやすい。
- 価値・懸念: 宣言だけで失われていた状態を保持できる。保存・UI・編集操作との接続は別途確認が必要。
- 次の確認: ビルドでモジュール境界とリンクを確認し、マスク編集操作がロック状態を参照するか調査する。

### 2026-08-03 — CloneGeneratorの配置入力と分布を安定化する

- 状態: 確認済み
- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: spacing／radius／bounds等の入力を有限値・非負へ補正し、列挙値を検証した。格子・螺旋・ランダム配置へrotationStepとPoisson disk間隔を反映した。
- 閃き・仮説: クローン配置は設定値の異常だけでなく、分布モードごとの変換適用順序が見た目を左右するため、共通の入力正規化と各モードの変換適用を揃える価値がある。
- 価値・懸念: 不正値による配置破綻を減らせる。既存シーンでは回転・Poisson配置の見た目が変わる可能性がある。
- 次の確認: ビルド・実機で各DistributionMode、rotationStep、Poisson diskの配置結果を確認する。

### 2026-08-03 — ParticleGeneratorの発生形状と時間入力を安定化する

- 状態: 確認済み
- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: 発生形状の負値、方向ベクトル、spread、寿命、発生数、delta time、連続発生量を有限値・妥当範囲へ補正した。
- 閃き・仮説: パーティクルは1フレームの不正値が位置・寿命・発生累積へ連鎖するため、発生境界と時間更新境界の両方で正規化するのが有効。
- 価値・懸念: NaN／無限大やゼロ方向によるシミュレーション破綻を抑えられる。発生分布の変更は既存映像との見た目差分を確認する必要がある。
- 次の確認: ビルド・実機で各EmitterShapeの分布、Burst／Continuous、ゼロ方向、異常delta timeを確認する。

### 2026-08-03 — 3Dモデル層のマテリアル復元と入力範囲を補強する

- 状態: 確認済み
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: 3Dモデルのパス検証、形状・マテリアル値の有限値／範囲補正、列挙値検証、発光・法線・遮蔽設定のJSON保存復元を追加した。
- 閃き・仮説: 3Dレイヤーはファイル入力とマテリアル設定が同時に外部化されるため、ロード境界とProperty編集境界を同じ正規化規則に揃えると再現性を保ちやすい。
- 価値・懸念: 不正な保存データや欠落ファイルによる不定状態を減らせる。上限値と既存プロジェクト互換性は実機確認が必要。
- 次の確認: ビルド・実機でモデル再読込、マテリアル保存／復元、異常値入力を確認する。

### 2026-08-03 — CameraLayerの光学・shake入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`
- 事実: FOV、正投影サイズ、clip、IPD、shake各入力とJSON復元値を、有限値と実用的な範囲に補正する経路を追加した。
- 閃き・仮説: カメラ投影と揺れ入力をsetter／復元境界で正規化すると、NaN・無限大・過大値の伝播による表示不安定化を抑えられる。
- 価値・懸念: 実行時の不定値伝播を減らせる一方、採用した上限値は実機・用途別に妥当性を確認する必要がある。
- 次の確認: ビルド・実機でFOV、clip、IPD、shake設定の適用とJSON再読込を確認する。

### 2026-08-03 — ArtifactAudioLayerの音量・再生時間を安全化する

- 状態: 確認済み
- 関連: `Artifact/src/Layer/ArtifactAudioLayer.cppm`
- 事実: layer volume/panは非有限値を補正せず、sample rateが0でもdurationを除算していた。
- 閃き・仮説: 音量・panを有限値化し、sample rateが正の場合だけdurationとloaded状態を有効にすると、音声レイヤーの再生・診断状態を安定させられる。
- 価値・懸念: NaN/Infの音声設定と0除算を抑えられる。無効音源をloaded=falseとする呼び出し側の扱いは未検証。
- 次の確認: ビルド・実機で正常／壊れた音源、volume/pan境界、sample rate 0の挙動を確認する。

### 2026-08-03 — Keying effectのUI分類を揃える

- 状態: 確認済み
- 関連: `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`
- 事実: Chroma Keyはeffect分類・短縮ラベルの対象だったが、追加したLuma/Difference Keyが同じ表示分類に含まれていなかった。
- 閃き・仮説: 3種類のkeying effectを同じK分類へ揃えると、effect paletteや一覧の視覚的なグルーピングを一貫させられる。
- 価値・懸念: キーイング機能の発見性を保てる。Widget名とeffect分類責務の整理は別途未検証。
- 次の確認: ビルド・実機で3種類のeffect表示名、分類、短縮ラベルを確認する。

### 2026-08-03 — BrushToolの入力契約とstroke状態を拡張する

- 状態: 確認済み
- 関連: `Artifact/include/Tool/ArtifactBrushTool.ixx`, `Artifact/src/Tool/ArtifactBrushTool.cppm`
- 事実: ブラシはradius/opacity中心の設定で、flow・hardness・spacing・jitter・pressure/tilt・色・clone/eraser状態やプレビューstroke管理が不足していた。
- 閃き・仮説: 各入力を有限値・範囲制限し、strokeのpreview/cancel/undo状態と追加設定をTool APIへ揃えると、ブラシ編集UIとPaintLayerの操作契約を統一できる。
- 価値・懸念: 静止画ペイントの編集粒度と入力安全性を高められる。筆圧・傾きデバイス、clone stamp、実描画品質は未検証。
- 次の確認: ビルド・実機で通常描画、cancel/undo、筆圧・tilt、eraser mode、clone offset、保存・再読込を確認する。

### 2026-08-03 — PuppetToolのpin属性APIを公開する

- 状態: 確認済み
- 関連: `Artifact/include/Tool/ArtifactPuppetTool.ixx`
- 事実: pinの位置・回転・weight・depthを取得／設定する実装がある一方、公開module宣言にアクセサがなかった。
- 閃き・仮説: pin属性APIを公開すると、rig編集UIや自動化処理が既存PuppetTool経路から状態を読み書きできる。
- 価値・懸念: 2D rig編集の編集粒度を高められる。weight/depthの値域検証とUI接続は未検証。
- 次の確認: ビルド・実機でpin選択、回転・weight・depth編集、保存・再読込を確認する。

### 2026-08-03 — PointTrackerのkeyframe入力と出力範囲を検証する

- 状態: 確認済み
- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: tracker出力の時間・座標・fps・point IDを検証せず、composition範囲外のkeyframeや重複IDを出力する可能性があった。
- 閃き・仮説: finite/範囲検証、composition範囲内への限定、ID重複排除と上限を追加すると、追跡結果の適用を安定させられる。
- 価値・懸念: 異常座標・時間や過大なNull生成を抑え、適用結果をdirty/changedとして通知できる。240fps・1024点上限の用途適合性は未検証。
- 次の確認: ビルド・実機で範囲外keyframe、重複ID、無効fps、空結果、undo/再読込を確認する。

### 2026-08-03 — CameraTrackerの解析範囲と結果を検証する

- 状態: 確認済み
- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: source layerの範囲をそのまま解析し、空画像・過大画像・少数フレーム・非有限pose・無制限のfeature layer生成を許していた。
- 閃き・仮説: composition範囲へクリップし、入力・解結果・生成数を検証すると、追跡処理の過大負荷と不正結果の反映を抑えられる。
- 価値・懸念: 解析の停止条件と結果の妥当性を明確にできる。100000フレーム／1024特徴点上限の用途適合性は未検証。
- 次の確認: ビルド・実機で短尺・長尺・欠損画像・失敗solve・特徴点上限を確認する。

### 2026-08-03 — MotionSketchのフレームレート・入力検証を揃える

- 状態: 確認済み
- 関連: `Artifact/include/Tool/ArtifactMotionSketchTool.ixx`, `Artifact/src/Tool/ArtifactMotionSketchTool.cppm`
- 事実: スケッチのundo/redoが固定24fpsでキー時刻を復元し、開始フレームを保持していなかった。座標・サンプル時刻の有限値検証と表示設定APIも不足していた。
- 閃き・仮説: compositionの実fpsと開始フレームをsnapshotへ使い、入力を検証し、sample rate/wireframe/background設定を公開すると、スケッチ結果とundoの時間軸を一致させられる。
- 価値・懸念: 24fps以外のcompositionでキー位置がずれにくくなる。実時間サンプリングとフレーム丸めの細かな差は未検証。
- 次の確認: ビルド・実機で24/30/60fps、無効座標、undo/redo、表示設定を確認する。

### 2026-08-03 — Luma KeyとDifference Keyをエフェクト登録へ接続する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Keying/LumaKeyEffect.ixx`, `Artifact/include/Effects/Keying/DifferenceKeyEffect.ixx`, `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`, `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`, `Artifact/src/Service/ArtifactEffectService.cppm`
- 事実: Luma/Difference Keyのmodule実装がある一方、Serviceの生成分岐・一覧登録がなく、effect IDから生成できなかった。
- 閃き・仮説: module実装とServiceのalias ID・表示名登録を同時に追加すると、キーイング基盤を通常のエフェクト導線へ接続できる。
- 価値・懸念: 比較資料で未整備だったLuma/Difference Keyを利用可能な登録面へ進められる。実画像のGPU/runtime品質は未検証。
- 次の確認: ビルド・実機で両effectの生成、Property編集、CPU/GPU出力、保存・再読込を確認する。

### 2026-08-03 — AudioServiceの音量・パン入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Service/ArtifactAudioService.cppm`
- 事実: master volume、layer volume、pan、dB変換はclamp前に非有限値を除外していなかった。
- 閃き・仮説: 有限値を確認してから既存範囲へ収め、中立値をフォールバックにすると、音声ミキサーや診断値へのNaN/Inf伝播を防げる。
- 価値・懸念: 音声出力の不定化を抑えられる。無効入力を1.0/0.0へ戻す設計がUI想定と一致するかは未検証。
- 次の確認: ビルド・実機でmaster/layer volume、pan、dB診断の境界値と非有限値を確認する。

### 2026-08-03 — RAM Previewの空範囲と失敗理由を同期する

- 状態: 確認済み
- 関連: `Artifact/src/Render/ArtifactRamPreviewController.cppm`
- 事実: start>=endのpreview rangeでも既存frame state/job queueを保持し、render失敗時にlastErrorを更新していなかった。
- 閃き・仮説: 空範囲を設定した時点で状態をクリアし、失敗フレームの理由をlastErrorへ保存すると、表示状態と診断情報を一致させられる。
- 価値・懸念: 空のRAM Previewが古いジョブやready数を残しにくくなり、失敗原因を後から参照できる。start/endの境界仕様は未検証。
- 次の確認: ビルド・実機で空範囲、単一フレーム、失敗復旧、再ビルドを確認する。

### 2026-08-03 — BatchRendererの逆順rangeと半径入力を防ぐ

- 状態: 確認済み
- 関連: `Artifact/src/Render/ArtifactRenderScheduler.cppm`
- 事実: 逆順FrameRangeではforループが終了せず、renderAroundFrameは負のradiusをそのまま範囲計算に使っていた。
- 閃き・仮説: range境界を先に検証し、フレーム列を終端比較で回し、radiusを0以上へ正規化すると、バッチタスク生成の停止や意図しない範囲を防げる。
- 価値・懸念: 不正範囲による無限ループ・過剰タスクを抑えられる。巨大な正値radiusの上限は未検証。
- 次の確認: ビルド・実機で空／逆順／単一／大半径のバッチレンダーを確認する。

### 2026-08-03 — RenderQueueプリセットのVideo/Audio分類を実装する

- 状態: 確認済み
- 関連: `Artifact/src/Render/ArtifactRenderQueuePresets.cppm`
- 事実: Videoカテゴリは画像シーケンスでないものを広く返し、Audioカテゴリは常にfalseを返していた。
- 閃き・仮説: container/codecを正規化して音声・映像の既知値を分類すると、プリセット選択UIのカテゴリ結果を実データに合わせられる。
- 価値・懸念: 音声プリセットが表示可能になり、音声を動画カテゴリへ誤表示しにくくなる。未知のcontainer/codecの分類は未検証。
- 次の確認: ビルド・実機でwav/mp3、mp4/mov、未知形式、画像シーケンスのカテゴリ表示を確認する。

### 2026-08-03 — OCIO Managerのviewer調整・GPU/LUT APIを公開する

- 状態: 確認済み
- 関連: `Artifact/include/Color/ArtifactOCIOManager.ixx`
- 事実: 実装側にviewer exposure/gamma、GPU view-transform shader/descriptor、3D LUT bakeの定義がある一方、公開module宣言とQVector includeが不足していた。
- 閃き・仮説: API宣言を実装と揃えると、viewer-only調整とGPU/LUT経路をrender側から正規に利用できる。
- 価値・懸念: OCIO v2運用の表示変換・LUT接続面を明示できる。GPU shader descriptorのbackend互換性とHDR受入れは未検証。
- 次の確認: ビルド・実機でviewer exposure/gamma、shader descriptor、LUTサイズ・domainを確認する。

### 2026-08-03 — FinalPostProcessのLUT入力ドメインを公開する

- 状態: 確認済み
- 関連: `Artifact/include/Render/ArtifactFinalPostProcess.ixx`, `Artifact/src/Render/ArtifactFinalPostProcess.cppm`
- 事実: LUT更新APIはあったが、入力ドメインを設定する公開導線がなく、view transformとLUT経路の責務コメントも実装状態と一致していなかった。
- 閃き・仮説: LUT入力ドメインを明示設定できるようにし、GPU経路がactive LUTを使う契約を記述すると、OCIO/LUT接続の境界を明確にできる。
- 価値・懸念: HDRや拡張レンジのLUT入力を後段から指定できる。GPUProcessorによる直接変換は別backendのままで、runtime parityは未検証。
- 次の確認: ビルド・実機でdomain範囲、LUT有無、view transform有無、HDR入力を確認する。

### 2026-08-03 — ArtifactIRendererにcompute用オフスクリーンターゲットを追加する

- 状態: 確認済み
- 関連: `Artifact/include/Render/ArtifactIRenderer.ixx`, `Artifact/src/Render/ArtifactIRenderer.cppm`
- 事実: 既存のオフスクリーン色ターゲットはRTV/SRV用途に限られ、compute post-process向けのRGBA16FターゲットとUAV取得APIがなかった。
- 閃き・仮説: RTV/SRV/UAVを持つRGBA16Fターゲットを生成し、所有テクスチャからUAV viewを取得できるようにすると、GPU後処理の接続面を共通化できる。
- 価値・懸念: CPU/QImage経路へ逃がさずcompute処理を接続できる。backendごとのUAV形式・状態遷移は未検証。
- 次の確認: ビルド・実機でDX12等のUAV作成、view寿命、compute書込後のSRV読込を確認する。

### 2026-08-03 — FrameCacheのstale候補と計測値を堅牢化する

- 状態: 確認済み
- 関連: `Artifact/include/Render/ArtifactFrameCache.ixx`, `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: 無効化後のstale eviction候補、null entry、逆順prefetch範囲、無効な容量・品質・計測値の扱いが不足していた。module宣言もinclude途中にあった。
- 閃き・仮説: live候補のフォールバックと入力検証を追加し、module宣言をinclude後へ移すと、cache更新の停止や不定なFPS計測を抑えられる。
- 価値・懸念: stale bookkeepingでのeviction停滞と異常値の伝播を減らせる。容量0時に例外を返す既存呼び出し側の扱いは未検証。
- 次の確認: ビルド・実機でinvalidate後のeviction、逆順range、容量0、品質境界、FPS計測を確認する。

### 2026-08-03 — GeneratorEffectorの出力保持と生成入力を安定化する

- 状態: 確認済み
- 関連: `Artifact/include/Generator/AbstractGeneratorEffector.ixx`, `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: applyが未実装のログ出力のみで、出力面を保持せず、サイズ・フレーム範囲・生成種別・Noise入力の検証も不足していた。
- 閃き・仮説: 生成結果を保持してoutput()で参照可能にし、入力範囲を正規化、GradientのConicと決定的なNoise生成を実装すると、Generatorの基礎契約を一貫させられる。
- 価値・懸念: 生成結果を後段へ渡せ、無効サイズや極端なNoise設定を抑えられる。既存プリセットとの画質・速度差は未検証。
- 次の確認: ビルド・実機で各Gradient/Noise種別、サイズ変更、フレーム範囲、同一seedの再現性を確認する。

### 2026-08-03 — ArtifactParticleGeneratorのtimeScaleを正規化する

- 状態: 確認済み
- 関連: `Artifact/include/Generator/ArtifactParticleGenerator.ixx`
- 事実: timeScaleを任意のfloatのまま設定でき、負値や非有限値が粒子時間更新へ渡る可能性があった。
- 閃き・仮説: 有限値確認後に0以上へclampし、無効値は1.0へ戻すと、粒子シミュレーションの時間進行を安定させられる。
- 価値・懸念: NaN/Infや逆方向の意図しない時間進行を抑えられる。timeScale=0を停止用途として許可する設計は未検証。
- 次の確認: ビルド・実機で停止、通常、加速、負値、非有限値を確認する。

### 2026-08-03 — ToolManagerへCloneと2D rig編集ツールを追加する

- 状態: 確認済み
- 関連: `Artifact/include/Tool/ArtifactToolManager.ixx`, `Artifact/src/Tool/ArtifactToolManager.cppm`
- 事実: Clone、RigSelect、RigWeightのToolTypeがなく、active tool名へのマッピングも不足していた。
- 閃き・仮説: enumと表示名マッピングを同時に追加すると、ツール選択状態を既存のManager経路で表現できる。
- 価値・懸念: Clone Stampと2D rig編集のツール状態を統一できる。各ツールのUI起動・操作実装との結線は未検証。
- 次の確認: ビルド・実機でツール切替、表示名、ショートカット、未対応操作時の挙動を確認する。

### 2026-08-03 — ArtifactTimelineClockのmodule宣言境界を統一する

- 状態: 確認済み
- 関連: `Artifact/include/Preview/ArtifactTimelineClock.ixx`
- 事実: `export module` 宣言がwobject・Qt includeの途中にあり、global module fragmentとmodule purviewの境界が不統一だった。
- 閃き・仮説: include群の後へmodule宣言を移すと、C++20 moduleのヘッダ解析境界を明確にできる。
- 価値・懸念: MSVCの外部ヘッダ解析不具合を避ける規約へ揃えられる。wobjectヘッダのmodule対応は未検証。
- 次の確認: module hygiene検査とビルドでTimelineClockの宣言・wobject生成を確認する。

### 2026-08-03 — LayerPluginAdapterのABI includeをglobal module fragmentへ移動する

- 状態: 確認済み
- 関連: `Artifact/include/Plugin/LayerPluginAdapter.ixx`
- 事実: `ArtifactPluginABI.h` の通常includeがmodule宣言後のpurview側に置かれていた。
- 閃き・仮説: ABIヘッダをglobal module fragmentのinclude群へ移すと、MSVCが外部ヘッダをmodule purviewとして解析するリスクを下げられる。
- 価値・懸念: C++20 module境界を規約に揃えられる。ABIヘッダ自身のmodule適合性は未検証。
- 次の確認: module hygiene検査とビルドでinclude位置・ABI型の可視性を確認する。

### 2026-08-03 — LayerMaskのlock状態APIを公開宣言へ揃える

- 状態: 確認済み
- 関連: `Artifact/include/Mask/LayerMask.ixx`, `Artifact/src/Mask/LayerMask.cppm`
- 事実: lock状態の実装は存在していたが、公開module interfaceにgetter/setter宣言がなく、利用側から参照・変更できなかった。
- 閃き・仮説: 宣言を実装と揃えると、マスク編集UIや操作経路がロック状態を正規APIで扱える。
- 価値・懸念: ロック中の編集抑止を共通化できる。既存呼び出し側の編集ガードの網羅性は未検証。
- 次の確認: ビルド・module hygiene検査と実機でlock中の編集操作を確認する。

### 2026-08-03 — RenderQueueServiceのselective settings APIを公開宣言へ揃える

- 状態: 確認済み
- 関連: `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- 事実: 実装済みのジョブ単位selective settings getter/setterが公開module宣言に含まれず、利用側がサービスAPIとして参照できなかった。
- 閃き・仮説: `QVariantMap`を用いた宣言と必要なincludeを公開面へ追加すると、WorkspaceAutomationやUIから同じ設定契約を利用できる。
- 価値・懸念: 実装とmodule interfaceの不一致によるコンパイル・利用制限を解消できる。mapキーの仕様は既存実装に依存する。
- 次の確認: ビルド・module hygiene検査で公開宣言と実装の一致を確認する。

### 2026-08-03 — RenderQueueのselective設定をWorkspaceAutomationへ公開する

- 状態: 確認済み
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`
- 事実: RenderQueueServiceとUIにはジョブ単位のselective settings APIが存在する一方、WorkspaceAutomationの操作説明・dispatch・実装一覧に未登録だった。
- 閃き・仮説: get/set操作を既存のAutomation registryへ追加すると、UIと同じ選択的レンダリング設定をAI操作から扱える。
- 価値・懸念: render queue自動化の設定粒度を高められる。QVariantMapのキー検証はサービス側の既存契約に依存し、完全なruntime検証は未実施。
- 次の確認: ビルド・実機で有効／無効レイヤー、範囲指定、無効ジョブindexのget/setを確認する。

### 2026-08-03 — DistributionModesのCatmull-Rom補間を安定化する

- 状態: 確認済み
- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: 補間パラメータの非有限値を除外せず、終端制御点の選択式が不明瞭で、接線を位置基底の値から近似していた。
- 閃き・仮説: 非有限値を既定値へ戻し、終端制御点を明示し、Hermite基底の微分から接線を計算すると、分布曲線の終端と向きを安定させられる。
- 価値・懸念: パーティクル等の分布方向が終端で不連続になりにくい。閉曲線を意図した呼び出し側への影響は未検証。
- 次の確認: ビルド・実機で空・1点・終端・閉曲線・非有限パラメータを確認する。

### 2026-08-03 — ProceduralTextureGeneratorの入力範囲とプロパティ面を整える

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Generator/ProceduralTextureGenerator.ixx`
- 事実: preset・width・heightを無制限に設定でき、GeneratorのProperty一覧とsetPropertyValueが未実装だった。
- 閃き・仮説: presetと解像度を範囲制限し、同じ契約をAbstractPropertyへ公開すると、生成設定の編集と実行時状態を揃えられる。
- 価値・懸念: 不正な解像度やpresetによる生成失敗を抑え、Inspectorから基本設定を編集できる。8192上限とSeedの表示型は未検証。
- 次の確認: ビルド・実機でpreset切替、解像度境界、Seed編集、生成結果を確認する。

### 2026-08-03 — HueAndSaturationの色調整setterを有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/ColorCorrection/HueAndSaturation.ixx`
- 事実: Hue / Saturation / Lightness のsetterはclampのみで、NaN/Infを補正していなかった。
- 閃き・仮説: 有限値を確認してから既存範囲へclampし、無効値には中立値を戻すと、色変換へ異常値が伝播しにくくなる。
- 価値・懸念: 色補正の不定出力を抑えられる。中立値フォールバックと既存プリセットの組み合わせは未検証。
- 次の確認: ビルド・実機で色相・彩度・明度の境界値と非有限値を確認する。

### 2026-08-03 — ArtifactStabilizerのmodule宣言をinclude後へ移動する

- 状態: 確認済み
- 関連: `Artifact/include/Effect/ArtifactStabilizer.ixx`
- 事実: `export module` 宣言が標準・Qt・wobjectヘッダのinclude途中にあり、global module fragmentの構造が他のmoduleインターフェースと不揃いだった。
- 閃き・仮説: include群をglobal module fragmentへまとめ、module宣言をその後へ置くと、C++20 module境界とヘッダ解析の挙動を安定させられる。
- 価値・懸念: MSVCのmoduleスキャン時にincludeがpurviewへ漏れるリスクを下げられる。依存ヘッダの実ビルド確認は未検証。
- 次の確認: ビルド・module hygiene検査でArtifactStabilizerの宣言・include境界を確認する。

### 2026-08-03 — BrushToolをApplicationManagerの公開導線へ接続する

- 状態: 確認済み
- 関連: `Artifact/include/Application/ArtifactApplicationManager.ixx`, `Artifact/src/Application/ArtifactApplicationManager.cppm`
- 事実: BrushToolはアプリケーションManagerの状態・アクセサに含まれず、UIやサービスから既存のManager経由で取得できなかった。
- 閃き・仮説: MotionSketch/Puppetと同じ導線でBrushToolを保持・返却すると、ブラシ操作の所有箇所を一元化できる。
- 価値・懸念: 既存のツール取得パターンを再利用できる。BrushToolの初期化順序やUI接続のruntime確認は未検証。
- 次の確認: ビルド・実機でApplicationManagerからの取得、ツール切替、破棄時の寿命を確認する。

### 2026-08-03 — プロジェクトManagerの同期・非同期パスを正規化する

- 状態: 確認済み
- 関連: `Artifact/src/Project/ArtifactProjectManager.cppm`
- 事実: 同期・非同期の保存／読込でパスのtrimとファイル種別検証がなく、処理途中で元の未正規化パスを混在して使っていた。
- 閃き・仮説: 入口でパスを正規化し、存在・ファイル判定を早期に行って全コールバックとsidecar処理へ同じ値を渡すと、保存状態の不一致を減らせる。
- 価値・懸念: 空白付きパスや不正パスの失敗を早く返し、同期・非同期のcurrent path更新を揃えられる。パスのtrimが意図的な空白を持つ名前へ与える影響は未検証。
- 次の確認: ビルド・実機で同期／非同期の保存・読込、空白付きパス、不正パス、sidecar復元を確認する。

### 2026-08-03 — プロジェクトパッケージ化の検証と保存を堅牢化する

- 状態: 確認済み
- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: パッケージ化前の設定・ツリー検証、外部ファイル存在確認、対象ディレクトリ検証が不足し、project.jsonを直接書き込んでいた。
- 閃き・仮説: 入力と外部ファイルを先に検証し、project.jsonを`QSaveFile`でコミットすると、不完全なパッケージの生成と途中書き込み破損を減らせる。
- 価値・懸念: スタンドアロンパッケージの再現性と失敗理由を改善できる。既存Assetsの置換失敗時に残るファイルの扱いは未検証。
- 次の確認: ビルド・実機で欠損素材、既存ファイル、無効な対象パス、書き込み失敗を確認する。

### 2026-08-03 — プロジェクト書き出し前に設定検証とOCIO情報を保存する

- 状態: 確認済み
- 関連: `Artifact/src/Project/ArtifactProjectExporter.cppm`
- 事実: 書き出し前にプロジェクト設定の検証結果をエラーとして扱う経路がなく、プロジェクト単位のOCIO設定もJSONへ保存していなかった。
- 閃き・仮説: 設定エラーをツリー検証より前に遮断し、OCIO情報をルートへ保存すると、壊れたプロジェクトの出力と色管理設定の欠落を減らせる。
- 価値・懸念: 書き出し失敗の理由を明確化し、旧読込側が無視できる形で色管理情報を保持できる。OCIO managerのruntime状態との整合は未検証。
- 次の確認: ビルド・実機で設定エラー、警告、OCIO有無、旧形式読込との互換性を確認する。

### 2026-08-03 — SourceCrop の保存値と変換パラメータを有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`
- 事実: JSONからNaN/Inf相当の値を受け入れる可能性があり、panは復元後にsetterの制限を通らず、zoom・rotationにも過大値の上限がなかった。
- 閃き・仮説: JSON境界とsetterの双方で有限値・範囲を保証すると、クロップ計算へ異常値が伝播しにくくなる。
- 価値・懸念: 保存プロジェクトの破損値から表示・変換を保護できる。pan/rotation上限の作業用途適合性は未検証。
- 次の確認: ビルド・実機で壊れたJSON、極端なpan/zoom/rotation、通常の再読込を確認する。

### 2026-08-03 — プロジェクト設定の文字列を正規化する

- 状態: 確認済み
- 関連: `Artifact/src/Project/ArtifactProjectSetting.cppm`
- 事実: プロジェクト名・作者名の保存と読込で前後空白を除去しておらず、検証エラーの日本語メッセージに誤記があった。
- 閃き・仮説: JSON境界で文字列をtrimすると、見た目が同じ設定の比較・保存結果を安定させられる。
- 価値・懸念: 不要な空白によるプロジェクト名差異を減らせる。意図的な前後空白を保持する用途は未検証。
- 次の確認: ビルド・実機で空白付き設定の保存・再読込と検証メッセージを確認する。

### 2026-08-03 — フォント使用マニフェストを原子的に保存する

- 状態: 確認済み
- 関連: `Artifact/src/Project/ArtifactProjectStatistics.cppm`
- 事実: JSON/CSVマニフェストを直接対象ファイルへ書き込み、パスのtrimはCSV側だけで行っていた。
- 閃き・仮説: `QSaveFile` と正規化済みパスを使うと、書き込み途中の破損や空白付きパスの不一致を抑えられる。
- 価値・懸念: マニフェスト出力の中断時に既存ファイルを壊しにくくなる。対象ファイルが別プロセスで使用中の場合の挙動は未検証。
- 次の確認: ビルド・実機でJSONのみ、JSON+CSV、空白付きパス、書き込み失敗時の挙動を確認する。

### 2026-08-03 — HexGridEffect のセル・線幅・角度を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/HexGridEffect.cppm`
- 事実: GPU計算側ではセルと線幅の下限だけを補正し、setter側では非有限値や過大値、角度の非有限値を扱っていなかった。
- 閃き・仮説: CPU/GPU双方で同じ下限・上限と有限値契約を持たせると、極端な格子設定による不定計算を抑えられる。
- 価値・懸念: セル分割数と線幅の過大化を制限できる。1024上限の作業用途適合性は未検証。
- 次の確認: ビルド・実機でセル・線幅・角度の境界値とCPU/GPU表示を確認する。

### 2026-08-03 — GaussianBlur のアルファ境界とGPU入力形式を統一する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Blur/GauusianBlur.cppm`
- 事実: CPU/GPUのRGBブラーは透明画素の色を通常平均し、CPUはBGRA入力をそのまま扱い、GPU入力は常にRGBA32Fとしてアップロードしていた。
- 閃き・仮説: premultiplied alpha空間でRGBをぼかし、チャンネル順とGPU upload formatを明示すると、透明端の色漏れと入力形式差を抑えられる。
- 価値・懸念: 透明境界のハローを減らし、16bit float入力にも対応できる。全カラーディスクリプタと実機GPU経路の組合せは未検証。
- 次の確認: ビルド・実機でRGBA/BGRA、透明端、Rgba16Float/Rgba32Floatの出力を比較する。

### 2026-08-03 — LinearWipeEffect のsoftnessをCPU/GPUで共有する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/LinearWipe/LinearWipeEffect.cppm`
- 事実: CPU経路はsoftnessを使わず、GPU定数バッファもangle・featherのみを渡していた。angle・softness・featherには非有限値の補正もなかった。
- 閃き・仮説: softnessを両経路のエッジ計算へ渡し、入力を有限値化すると、ワイプ境界の見た目と編集値の契約を揃えられる。
- 価値・懸念: CPU/GPUの境界幅差を減らせる。既存プロジェクトのsoftness既定値との見た目差は未検証。
- 次の確認: ビルド・実機で角度、softness、featherの境界値とCPU/GPU出力を比較する。

### 2026-08-03 — LiquifyEffect の補間近傍とGPU変位方向を一致させる

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Liquify/LiquifyEffect.cppm`
- 事実: CPU双線形補間の4近傍の行・列インデックスがc10/c01で入れ替わっており、GPU経路の変位方向はCPU経路と逆だった。
- 閃き・仮説: 近傍インデックスとGPUのsource位置を補正すると、補間結果とCPU/GPUのワープ方向が一致しやすくなる。
- 価値・懸念: Liquifyの方向反転や補間アーティファクトを抑えられる。ブラシモードごとの方向仕様は未検証。
- 次の確認: ビルド・実機で各ブラシモード、角度、CPU/GPU経路の出力を比較する。

### 2026-08-03 — RadialShadowEffect の影合成と入力値を安定化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/RadialShadow/RadialShadowEffect.cppm`
- 事実: CPU経路は影色の一時画像を作って単純加算し、元画像の色とアルファを影で置き換えていた。距離・柔らかさ・不透明度・中心値も非有限値を補正していなかった。
- 閃き・仮説: 元画素へ影の寄与を合成し、影色をRGBA順で扱い、入力を有限値化すると、影の重ね合わせと端部の挙動を安定させられる。
- 価値・懸念: 元画像を保持した自然な影合成に近づく。既存の影色・アルファ設計との見た目差は未検証。
- 次の確認: ビルド・実機で透明背景、既存画像、色・不透明度・中心境界を確認する。

### 2026-08-03 — AutoMosaicEffect の対象領域をクリップ・統合する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/AutoMosaicEffect.cppm`
- 事実: 複数の矩形領域を個別に処理し、画像外の矩形や隣接領域の重複を事前整理していなかった。
- 閃き・仮説: 入力領域を画像範囲へクリップし、重複・近接領域を統合してからモザイク処理すると、境界外参照と継ぎ目の不整合を減らせる。
- 価値・懸念: 領域数が多い場合の重複処理を減らせる。統合による意図した隙間の変化は未検証。
- 次の確認: ビルド・実機で画像外・重複・隣接・離間矩形を確認する。

### 2026-08-03 — SatinEffect の色順・アルファ・境界処理を統一する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Satin/SatinEffect.cppm`
- 事実: CPU/GPUのサテン色がBGR順で組み立てられ、GPU境界サンプルは端点へクランプしていた。CPU合成ではサテンアルファへ前景アルファを再度乗算していた。
- 閃き・仮説: RGBA順に統一し、範囲外サンプルを透明として扱い、合成係数をサテンアルファに揃えると、透明端と色の不連続を抑えられる。
- 価値・懸念: CPU/GPU経路の見た目差を減らせる。サテンの既存ブレンド意図との完全一致は未検証。
- 次の確認: ビルド・実機で透明境界、各色、CPU/GPU出力の一致を確認する。

### 2026-08-03 — StrokeEffect の色順・境界サンプリング・入力契約を揃える

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`
- 事実: CPU/GPUの色値組み立てにBGR順の記述が残り、GPU境界サンプルは端点へクランプしていた。また幅・不透明度の入力範囲と無効色の扱いが明示されていなかった。
- 閃き・仮説: RGBA順へ統一し、範囲外サンプルを透明として扱うと、ストロークの縁と色の不連続を抑えられる。
- 価値・懸念: 透明境界のストロークが隣接端のアルファを誤参照しにくくなる。画像のチャンネル記述と実機表示の一致は未検証。
- 次の確認: ビルド・実機で透明端、各色、幅・不透明度の境界値を確認する。

### 2026-08-03 — TurbulentDisplaceEffect のノイズ入力範囲を明示する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm`
- 事実: Amount / Size / Octaves / Seed / Domain Warp はsetterの範囲制限または非有限値補正が不足し、プロパティ範囲も未設定だった。
- 閃き・仮説: 反復回数・シード・ワープ量に上限を設け、同じ範囲をプロパティメタデータへ反映すると、計算負荷と入力契約を安定させられる。
- 価値・懸念: 極端なノイズ設定による過大計算や不定値を抑えられる。上限値が既存作例を狭めないかは未検証。
- 次の確認: ビルド・実機で各パラメータの境界値、プリセット、非有限値を確認する。

### 2026-08-03 — ChromaticReliefEffect のレリーフ入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/ChromaticReliefEffect.cppm`
- 事実: Relief Amount / Chromatic Offset / Direction / Edge Softness / Mix は、setterで非有限値を補正していなかった。
- 閃き・仮説: 有限値確認後に既存範囲へ収め、Directionにもフォールバックを設けると、レリーフ計算の入力契約を安定させられる。
- 価値・懸念: NaN/Infによる不定出力を抑えられる。Directionの単位・許容範囲は既存仕様に依存し、未検証。
- 次の確認: ビルド・実機で各数値の通常値、境界値、非有限値を確認する。

### 2026-08-03 — WhiteBalanceEffect の色調整範囲をプロパティ定義へ反映する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/WhiteBalanceEffect.cppm`
- 事実: Temperature / Tint / Brightness / Preset の想定範囲がプロパティ定義に設定されていなかった。
- 閃き・仮説: 既存の編集契約に対応する最小・最大値をメタデータ化すると、Inspector側で扱える範囲を明示できる。
- 価値・懸念: 色調整の入力範囲がUIと設計上揃う。Presetの0〜6が全プリセットを網羅するかは未検証。
- 次の確認: ビルド・実機で各プロパティの入力範囲とプリセット選択を確認する。

### 2026-08-03 — OpticsCompensationEffect の光学パラメータ契約を揃える

- 状態: 確認済み
- 関連: `Artifact/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm`
- 事実: Center X/Y、FOV はsetter側に範囲制限がある一方、非有限値の補正とプロパティ定義側の範囲メタデータが不足していた。
- 閃き・仮説: 有限値フォールバックとUI範囲を同じ契約として持たせると、入力経路ごとの差異を減らせる。
- 価値・懸念: 光学補正の中心・視野角に不正値が入りにくくなる。既存UIのDirection=-1〜1表示は未検証。
- 次の確認: ビルド・実機で数値入力とDirectionの正負切替を確認する。

### 2026-08-03 — EdgeBloomEffect のプロパティ範囲をUIメタデータへ反映する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Glow/EdgeBloomEffect.cppm`
- 事実: Threshold / Radius / Amount / Edge Boost / Tint Mix の最小値・最大値がプロパティ定義に設定されていなかった。
- 閃き・仮説: 既存setterの想定範囲をプロパティメタデータにも設定すると、編集UIと実装側の入力契約を一致させられる。
- 価値・懸念: UIからの入力範囲が明確になり、異常値入力の機会を減らせる。既存UIが範囲メタデータをどう表示するかは未検証。
- 次の確認: ビルド・実機で各プロパティのスライダー／数値入力範囲とsetterの挙動を確認する。

### 2026-08-03 — DisplacementMapEffect の変位量とチャンネル値を正規化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/DisplacementMap/DisplacementMapEffect.cppm`
- 事実: 最大変位量は任意の float を保持でき、チャンネル列挙値も検証なしで保持していた。
- 閃き・仮説: 有限値確認・変位量の上限・チャンネル列挙範囲をsetterで保証すると、サンプリング位置と配列参照の異常を抑えられる。
- 価値・懸念: 極端な変位や不正なチャンネル値による不定動作を抑制できる。上限4096の表現上の妥当性は未検証。
- 次の確認: ビルド・実機で通常値、符号付き境界値、非有限値、不正列挙値を確認する。

### 2026-08-03 — ApertureShapeBlurEffect の開口入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm`
- 事実: Radius / Rotation / Edge Brightness / Highlight Boost は非有限値をそのまま保持する経路があった。
- 閃き・仮説: setter で有限値を確認し、既存の範囲制限と既定値フォールバックを併用すると、PSF計算へ異常値が伝播しにくくなる。
- 価値・懸念: 開口形状ブラーの不定出力を抑えられる。回転角の極端値とPSF画像経路の相互作用は未検証。
- 次の確認: ビルド・実機で各数値プロパティの境界値と非有限値を確認する。

### 2026-08-03 — AnisotropicFlowBlurEffect のテンソル入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Blur/AnisotropicFlowBlurEffect.cppm`
- 事実: Blur Amount / Tensor Noise Scale / Tensor Integration Scale / Edge Adherence は clamp 前に非有限値を除外していなかった。
- 閃き・仮説: property setter で有限値を確認してから既存範囲へ収めると、異常なテンソル設定がブラー計算へ伝播しにくくなる。
- 価値・懸念: NaN/Inf による不定な出力を抑えられる。境界値の実機挙動は未検証。
- 次の確認: ビルド・実機で各入力の通常値、範囲外値、非有限値を確認する。

### 2026-08-03 — ReactionDiffusionBlurEffect の反応拡散パラメータを有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Blur/ReactionDiffusionBlurEffect.cppm`
- 事実: Blur Radius / Feed / Kill / Pattern Strength / Evolution は property setter で clamp または直接代入し、非有限値を補正していなかった。
- 閃き・仮説: 各反応拡散入力を有限値確認後に既存範囲へ収めると、反復ブラー計算へ異常値が伝播しにくくなる。
- 価値・懸念: 反応拡散の発散や不正パターンを抑えられる。既定値の見た目と反復回数との相互作用は未検証。
- 次の確認: ビルド・実機で Feed / Kill、Iterations、Pattern Strength の境界値を確認する。

### 2026-08-03 — DifferenceMatteEffect の threshold を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/DifferenceMatteEffect.cppm`
- 事実: threshold は clamp のみで、非有限値を明示的に補正していなかった。referenceOffset は既存範囲に制限されている。
- 閃き・仮説: threshold setter で有限値を保証すると、差分マットの比較結果へ異常値が伝播しにくくなる。
- 価値・懸念: 差分マスクの入力安定性を高められる。threshold の既定値は未検証。
- 次の確認: ビルド・実機で referenceOffset と threshold の境界値を確認する。

### 2026-08-03 — RadialBlurEffect の量・中心・モードを有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/RadialBlurEffect.cppm`
- 事実: amount / centerX / centerY / mode は clamp のみで、非有限値を補正していなかった。quality は既存範囲に制限されている。
- 閃き・仮説: 4つの float setter で有限値を保証すると、放射ブラーの中心・量・モード入力を安定化できる。
- 価値・懸念: 放射サンプリングの異常値伝播を抑えられる。mode の float 表現は未検証。
- 次の確認: ビルド・実機で中心、amount、quality、mode の境界値を確認する。

### 2026-08-03 — VoronoiEffect の scale・jitter・mode・seed を境界検証する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/VoronoiEffect.cppm`
- 事実: scale は下限のみ、jitter は clamp のみ、mode は既存範囲内だが seed は直接代入で、非有限値や過大 seed を補正していなかった。
- 閃き・仮説: scale / jitter を有限化し、seed を範囲内へ収めると、セル生成と乱数経路の入力を安定化できる。
- 価値・懸念: 過大なセル密度や未制限 seed による処理差を抑えられる。scale 上限は未検証。
- 次の確認: ビルド・実機で mode、seed、scale / jitter の境界値と再現性を確認する。

### 2026-08-03 — StripesEffect の周波数・角度・厚み・offset を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/StripesEffect.cppm`
- 事実: frequency は下限のみ、angle / offset は直接代入、thickness は clamp のみで、非有限値を補正していなかった。
- 閃き・仮説: 4つの setter で有限値と妥当範囲を保証すると、ストライプ生成の CPU / GPU 同期へ異常値が伝播しにくくなる。
- 価値・懸念: 周波数の過大値と角度・offset の異常値によるパターン破損を抑えられる。frequency 上限は未検証。
- 次の確認: ビルド・実機で周波数、厚み、角度、offset の境界値を確認する。

### 2026-08-03 — HalftoneEffect のドットサイズ・角度・コントラストを有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/HalftoneEffect.cppm`
- 事実: dotSize は下限のみ、angle は直接代入、contrast は clamp のみで、非有限値を補正していなかった。
- 閃き・仮説: 3つの setter で有限値と妥当範囲を保証すると、ハーフトーン格子の生成へ異常値が伝播しにくくなる。
- 価値・懸念: ドット配置とコントラストの入力安定性を高められる。dotSize 上限は未検証。
- 次の確認: ビルド・実機で dotSize、angle、contrast の境界値を確認する。

### 2026-08-03 — GlitchEffect の強度・色ずれ・走査線・seed を境界検証する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/GlitchEffect.cppm`
- 事実: intensity / colorShift / scanlines は clamp のみ、seed は直接代入で、非有限値や過大 seed を補正していなかった。
- 閃き・仮説: 3つの float setter を有限化し、seed を範囲内へ収めると、グリッチ生成の同期先へ異常値が伝播しにくくなる。
- 価値・懸念: 乱数・走査線パラメータの安定性を高められる。seed 上限は未検証。
- 次の確認: ビルド・実機で4項目の境界値と再現性を確認する。

### 2026-08-03 — FilmDamageEffect のフィルム損傷プロパティを有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/FilmDamageEffect.cppm`
- 事実: Grain / Dust / Scratches / Gate Weave / Flicker / Film Burn / Evolution は property setter で clamp または直接代入し、非有限値を補正していなかった。
- 閃き・仮説: 各損傷パラメータを有限値確認後に既存範囲へ収めると、フィルムノイズ生成へ異常値が伝播しにくくなる。
- 価値・懸念: フィルムダメージの入力安定性を高められる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で7項目の異常値入力と Seed 変更を確認する。

### 2026-08-03 — EchoEffect の decay と blendOperator を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/EchoEffect.cppm`
- 事実: decay / blendOperator は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: setter 境界で有限値を保証すると、エコー履歴の蓄積と合成係数へ異常値が伝播しにくくなる。
- 価値・懸念: 残像合成の入力安定性を高められる。blendOperator の既定値の見た目は未検証。
- 次の確認: ビルド・実機で decay / blendOperator の境界値と異常値を確認する。

### 2026-08-03 — VectorFlowGlitchEffect のプロパティ入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/VectorFlowGlitchEffect.cppm`
- 事実: Glitch Amount / Frequency / Chromatic Aberration / Edge Flow Influence / Evolution は property setter で clamp または直接代入し、非有限値を補正していなかった。cmath include も不足していた。
- 閃き・仮説: 各入力を有限値確認後に既存範囲へ収め、標準ヘッダを直接 include すると、ベクトルフローの入力境界を安定化できる。
- 価値・懸念: グリッチ量・色収差・進化値の異常伝播を抑えられる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で5項目の異常値入力とプロパティ再編集を確認する。

### 2026-08-03 — FeedbackEffect の残像量・移動・zoom・回転を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/FeedbackEffect.cppm`
- 事実: amount / decay は clamp のみ、center offset / rotation は直接代入、zoom は下限のみで非有限値や過大値を補正していなかった。
- 閃き・仮説: フィードバックの全数値 setter で有限値と範囲を保証すると、履歴フレーム合成へ異常な移動・拡大値が伝播しにくくなる。
- 価値・懸念: 残像の発散や不安定な変形を抑えられる。上限値の見た目は未検証。
- 次の確認: ビルド・実機で decay、offset、zoom、rotation の境界値を確認する。

### 2026-08-03 — VectorBlurEffect のシャッター角・露出補正を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/VectorBlurEffect.cppm`
- 事実: shutterAngle は clamp のみ、exposureCompensation は非負化のみで、非有限値を補正していなかった。
- 閃き・仮説: 2つの setter で有限値と妥当範囲を保証すると、ベクトルブラーのモーションサンプル計算へ異常値が伝播しにくくなる。
- 価値・懸念: 過大な露出補正・角度による不安定化を抑えられる。上限値の見た目は未検証。
- 次の確認: ビルド・実機で shutterAngle、samples、exposureCompensation の境界値を確認する。

### 2026-08-03 — ChromaticAberrationEffect の色ずれ量と中心を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/ChromaticAberrationEffect.cppm`
- 事実: redShift / blueShift / centerX / centerY は clamp のみで、非有限値を補正していなかった。
- 閃き・仮説: 色収差量と中心座標の setter で有限値を保証すると、ラスタライズ同期へ異常値が伝播しにくくなる。
- 価値・懸念: 色ずれの入力安定性を高められる。GPU のサンプル境界挙動は未検証。
- 次の確認: ビルド・実機で中心移動、色ずれ量の境界、異常値入力を確認する。

### 2026-08-03 — BricksEffect のレンガ寸法・モルタル・offset を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/BricksEffect.cppm`
- 事実: brickWidth / brickHeight / mortarWidth は max のみ、offset は clamp のみで、非有限値を補正していなかった。
- 閃き・仮説: レンガ生成の4 setter で有限値を保証すると、ラスタライズ計算へ異常なセル寸法が伝播しにくくなる。
- 価値・懸念: 不正な寸法・オフセットによるレンガパターン破損を抑えられる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で寸法、モルタル幅、offset の境界値を確認する。

### 2026-08-03 — MosaicEffect の shape mask 平均と cellSize 入力をそろえる

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Mosaic/MosaicEffect.cppm`
- 事実: GPU shape mode はセル全体の平均を作ってから中心形状を出力していたため、形状外サンプルを平均へ含めていた。cellSize は非有限値を補正していなかった。
- 閃き・仮説: shape mode 時は形状内サンプルだけで平均を計算し、cellSize setter で有限値を保証すると、CPU/GPU の形状モザイク結果を整合させやすい。
- 価値・懸念: 形状モードの色滲みと異常 cellSize を抑えられる。GPU のサンプル数増加による性能影響は未検証。
- 次の確認: ビルド・実機で矩形 / shape mode、セル端、cellSize 境界を比較する。

### 2026-08-03 — SharpenEffect の amount・sigma・threshold を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Sharpen/SharpenEffect.cppm`
- 事実: amount / sigma / threshold は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 3つの setter で有限値を保証すると、シャープ化の CPU / GPU 同期へ異常値が伝播しにくくなる。
- 価値・懸念: 輪郭強調の入力安定性を高められる。sigma の既定値と上限の見た目は未検証。
- 次の確認: ビルド・実機で amount / sigma / threshold の境界値を確認する。

### 2026-08-03 — VignetteEffect の画面端半径と CPU/GPU 入力条件をそろえる

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/VignetteEffect.cppm`
- 事実: CPU / GPU の最大半径は中心から原点までの距離だけを使い、center が画面中央以外で端部を覆い切れなかった。setter も非有限値を補正していなかった。
- 閃き・仮説: 各画面端までの最大距離を使い、CPU / GPU の半径計算をそろえ、5つの setter で有限値を保証すると、ビネット境界を安定化できる。
- 価値・懸念: 中心移動時のフェード範囲と異常入力を改善できる。GPU 式の端数差は未検証。
- 次の確認: ビルド・実機で中心位置、画面端、CPU / GPU フェード結果を比較する。

### 2026-08-03 — EdgeEffect / RimLightEffect の rasterizer 入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Rasterizer/EdgeEffect.cppm`
- 事実: Edge の mode / intensity / threshold / invert と RimLight の angle / width / softness / intensity / mix は clamp・fmod のみで、非有限値を補正していなかった。
- 閃き・仮説: 両 rasterizer effect の setter 境界で有限値を保証すると、異常なエッジ・リム光パラメータが同期先へ伝播しにくくなる。
- 価値・懸念: 同一実装ファイル内の2エフェクトの入力安定性をそろえられる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で Edge / RimLight の境界値と異常値入力を確認する。

### 2026-08-03 — DitheringEffect の algorithm enum と量パラメータを境界検証する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Dithering/DitheringEffect.cppm`
- 事実: algorithm は enum 値を直接保持し、amount / patternScale は clamp・max のみで非有限値を補正していなかった。
- 閃き・仮説: algorithm を有効範囲へ制限し、数値 setter で有限値を保証すると、ディザリング同期先の入力条件を安定化できる。
- 価値・懸念: 未定義アルゴリズムと異常量による処理差を抑えられる。enum の有効数は未検証。
- 次の確認: ビルド・実機で algorithm 範囲、amount / patternScale 境界を確認する。

### 2026-08-03 — LiquidGlowEffect の流体グロー入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Glow/LiquidGlowEffect.cppm`
- 事実: threshold / radius / intensity / flowScale / distortion は clamp のみで、phase は直接代入され、非有限値を補正していなかった。
- 閃き・仮説: 流体グローの全数値 setter で有限値を保証すると、フローノイズと発光同期へ異常値が伝播しにくくなる。
- 価値・懸念: Liquid Glow の入力安定性をそろえられる。既定値と phase の周期扱いは未検証。
- 次の確認: ビルド・実機で flowScale / distortion / phase の境界と異常値入力を確認する。

### 2026-08-03 — ResidualGlowEffect の残光パラメータを有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Glow/ResidualGlowEffect.cppm`
- 事実: threshold / radius / intensity / decay / historyMix は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 残光設定の setter で有限値を保証すると、履歴合成へ異常値が伝播しにくくなる。
- 価値・懸念: 残像の蓄積と減衰の入力安定性を高められる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で履歴蓄積、decay / historyMix の境界、異常値入力を確認する。

### 2026-08-03 — PhysicalHalationEffect のプロパティ入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Glow/PhysicalHalationEffect.cppm`
- 事実: Threshold / Spread / Intensity / Red Diffusion / Softness は property setter で clamp するだけで、非有限値を補正していなかった。cmath include も不足していた。
- 閃き・仮説: 各入力を有限値確認後に既存範囲へ収め、標準ヘッダを直接 include すると、halation の入力境界と module 自己完結性を改善できる。
- 価値・懸念: ハレーション設定の異常値伝播を抑えられる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で5項目の異常値入力とプロパティ再編集を確認する。

### 2026-08-03 — LensDistortion CPU パスでも zoom の下限を再確認する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/LensDistortion/LensDistortionEffect.cppm`
- 事実: CPU 適用処理は zoom を直接使っており、API setter の補正状態に依存していた。
- 閃き・仮説: 適用直前にも最小値を適用すると、古い状態や別経路からの設定でもゼロ近傍除算を避けやすくなる。
- 価値・懸念: CPU レンズ歪みの防御境界を実装側にも置ける。GPU パスとの同じ防御範囲は未検証。
- 次の確認: ビルド・実機で zoom 0 / 極小値の CPU / GPU 結果を確認する。

### 2026-08-03 — KuwaharaEffect の radius と sharpness を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Kuwahara/KuwaharaEffect.cppm`
- 事実: radius / sharpness は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: setter 境界で有限値を保証すると、Kuwahara の CPU / GPU 同期へ異常値が伝播しにくくなる。
- 価値・懸念: フィルタ半径と輪郭特性の入力安定性を高められる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で radius / sharpness の境界値と異常値を確認する。

### 2026-08-03 — RadioWavesEffect の波数列挙と時間パラメータを安定化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Generate/RadioWavesEffect.cppm`
- 事実: waveCount は現在時刻までの全ウェーブを列挙し、frequency / lifespan の異常値を前提にせず、各 setter も非有限値を補正していなかった。
- 閃き・仮説: 有効寿命内の first〜last wave だけを列挙し、安全な frequency / lifespan を使うと、不要計算とゼロ除算リスクを減らせる。setter で関連入力も有限化する。
- 価値・懸念: 長時間再生時の波ウェーブ列挙コストと異常入力を抑えられる。波の境界タイミングは未検証。
- 次の確認: ビルド・実機で長時間再生、frequency / lifespan 境界、CPU 出力を確認する。

### 2026-08-03 — SimpleRainEffect の雨パラメータを有限値境界で統一する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Generate/SimpleRainEffect.cppm`
- 事実: Density / Streak Length / Speed / Wind / Opacity / Depth / Splash Amount / Evolution は property setter で clamp または直接代入し、非有限値を補正していなかった。
- 閃き・仮説: 各プロパティ入力を有限値確認後に既存範囲へ収めると、雨生成の同期先へ異常値が伝播しにくくなる。
- 価値・懸念: 雨粒生成のパラメータ破損を抑えられる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で8項目の異常値入力と Seed 変更を確認する。

### 2026-08-03 — LuminescenceCausticsEffect のプロパティ入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Glow/LuminescenceCausticsEffect.cppm`
- 事実: Threshold / Edge Weight / Scale / Intensity / Evolution / Color Shift は property setter で clamp または直接代入するだけで、非有限値を補正していなかった。
- 閃き・仮説: 各プロパティ入力を有限値確認後に既存範囲へ収めると、caustics の同期先へ異常値が伝播しにくくなる。
- 価値・懸念: 発光模様のパラメータ破損を抑えられる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で6項目の異常値入力とプロパティ再編集を確認する。

### 2026-08-03 — Kaleidoscope の補間サンプリング・フェード半径・入力境界を整える

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm`
- 事実: CPU パスは最近傍整数サンプリングと中心依存の fade 半径を使い、center / rotation / zoom / feather は非有限値を補正していなかった。
- 閃き・仮説: bilinear sampling と画面端から求める最大半径を使い、setter で有限値を保証すると、万華鏡の境界品質と異常入力耐性を改善できる。
- 価値・懸念: 回転・拡大時のジャギーと中心外 fade の不整合を抑えられる。補間コストと見た目は未検証。
- 次の確認: ビルド・実機で回転、feather、画面端、CPU / GPU のサンプリング結果を比較する。

### 2026-08-03 — InnerShadow の RGBA 順序・内部合成・入力値を整える

- 状態: 確認済み
- 関連: `Artifact/src/Effects/InnerShadow/InnerShadowEffect.cppm`
- 事実: CPU shadow buffer は BGR 順で書き込み、内部合成は前景 alpha を二重に扱う式になっており、setter は非有限値を補正していなかった。
- 閃き・仮説: RGBA 順へそろえ、shadow factor を前景 alpha と組み合わせた premultiplied 的な式へ整理し、入力 setter で有限値を保証すると、内側影の色・透明度を安定化できる。
- 価値・懸念: DropShadow と同じ色順の不整合を減らし、透明境界の合成を改善できる。合成式の見た目は未検証。
- 次の確認: ビルド・実機で半透明レイヤー、RGBA 色順、softness / opacity 境界を比較する。

### 2026-08-03 — GlowEffect の CPU 輝度マスクと CPU/GPU 入力条件をそろえる

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`
- 事実: CPU マスク生成は BGR 前提の `cvtColor` を使い、入力 setter は直接値を保持していた。GPU/CPU 共通のレイヤー数や sigma 等にも範囲がなかった。
- 閃き・仮説: RGBA チャンネルから明示的な RGB 輝度を計算し、マスクを 1 以下に抑え、両実装の setter 範囲を統一すると、色順と異常入力の差を減らせる。
- 価値・懸念: Glow の CPU / GPU 見た目差と過大パラメータを抑えられる。OpenCV Mat のチャンネル順前提は未検証。
- 次の確認: ビルド・実機で RGB 原色、glowGain、layerCount、sigma の CPU / GPU 結果を比較する。

### 2026-08-03 — DropShadow の RGBA 順序・境界サンプリング・入力値を整える

- 状態: 確認済み
- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`
- 事実: CPU shadow buffer は BGR 順で書き込み、GPU alphaAt は clamp により端点を繰り返し、setter は非有限値を補正していなかった。
- 閃き・仮説: RGBA 順へそろえ、GPU のサンプル範囲外を透明として扱い、softness に応じた半径を明示すると、CPU / GPU の境界挙動を整合させやすい。
- 価値・懸念: 色順・端部の影・異常入力による差を抑えられる。GPU の広いループによる性能影響は未検証。
- 次の確認: ビルド・実機で端部の影、RGBA 色順、softness 境界を比較する。

### 2026-08-03 — BendTransform の変形パラメータを property editor へ公開する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Transform/BendTransform.ixx`
- 事実: angle / direction / size は直接保持し、property group は未実装だった。
- 閃き・仮説: setter で有限範囲を保証し、3項目を min / max 付き property として公開して setter 経由で編集すると、変形設定の UI と内部状態をそろえられる。
- 価値・懸念: Bend の主要設定を通常の property editor から編集できる。module interface 変更と property 名の既存利用状況は未検証。
- 次の確認: ビルド・実機で property 表示、範囲編集、保存・再読込を確認する。

### 2026-08-03 — TwistTransform の Angle プロパティに範囲と有限値境界を付ける

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Transform/TwistTransform.ixx`
- 事実: Angle プロパティは UI 範囲を持たず、property setter も float を直接保持していた。
- 閃き・仮説: -720〜720 度の min / max を提示し、setter で同じ範囲と有限値を保証すると、UI 入力と変形計算の境界をそろえられる。
- 価値・懸念: 非有限・過大角度による変形不安定化を抑えられる。既存の直接 setter 経路の有無は未検証。
- 次の確認: ビルド・実機で範囲外、NaN / infinity、UI 編集結果を確認する。

### 2026-08-03 — WaveEffect の CPU/GPU 波形パラメータ範囲を統一する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Wave/WaveEffect.ixx`
- 事実: CPU / GPU の amplitude / frequency / phase は直接保持し、wave type / orientation も範囲外値を許していた。
- 閃き・仮説: 両実装で同じ有限値・enum 相当の範囲補正を適用すると、波形生成の CPU / GPU パスで入力条件を統一できる。
- 価値・懸念: 異常な波形値や未定義モードによる処理差を抑えられる。振幅・周波数の上限は未検証。
- 次の確認: ビルド・実機で CPU / GPU の波形タイプ、方向、境界値を比較する。

### 2026-08-03 — LiquifyEffect の CPU/GPU brush パラメータと enum を統一する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Liquify/LiquifyEffect.ixx`
- 事実: CPU / GPU の brush type、amount、radius、center、angle、seed、mesh density は直接保持され、enum 範囲や数値境界がなかった。
- 閃き・仮説: 両実装で同じ enum / 有限値 / 範囲補正を適用すると、Liquify の CPU / GPU パスで入力条件を統一できる。
- 価値・懸念: 不正 brush type や過大 mesh 設定による不安定化を抑えられる。範囲値の UX 妥当性は未検証。
- 次の確認: ビルド・実機で brush type、mesh density、異常 float 入力を比較する。

### 2026-08-03 — IESLightEffect の照明値・プロパティ経路・パス入力を整える

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Light/IESLightEffect.ixx`
- 事実: intensity は非負化のみ、temperature は clamp のみで、プロパティ経路は直接代入し、IES パスは trim せず空判定していた。cmath include も不足していた。
- 閃き・仮説: 照明値を有限範囲へ補正し、プロパティを setter 経由に統一し、パスを trim してから保持・判定すると、入力境界を一貫させられる。
- 価値・懸念: IES 照明の異常値と空白付きパスの扱いを安定化できる。実際の IES 解析・LUT 連携は未検証。
- 次の確認: ビルド・実機で照明値、プロパティ編集、空白付き IES パスを確認する。

### 2026-08-03 — ChromaKeyEffect の key color チャンネルを有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Keying/ChromaKeyEffect.ixx`
- 事実: key color は RGBA 値を直接保持し、チャンネルごとの非有限値や範囲外を検査していなかった。
- 閃き・仮説: 各チャンネルを有限・0〜1 に補正すると、キー判定へ異常な色値が伝播しにくくなる。
- 価値・懸念: キーカラー入力の安定性を高められる。CPU/GPU 実装間の setter 共通化は未検証。
- 次の確認: ビルド・実機で透明度を含む key color の境界値を確認する。

### 2026-08-03 — PBRMaterialEffect の材質入力検証と変換項目をそろえる

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Render/PBRMaterialEffect.ixx`
- 事実: 色は無効 QColor をそのまま受け、metallic / roughness / AO / emissiveIntensity は非有限値を補正せず、toMaterial では emissive・occlusion 項目を反映していなかった。
- 閃き・仮説: 色の validity を確認し、数値を有限範囲へ収め、変換時に全 PBR 材質項目を渡すと、編集値とレンダリング材質の境界をそろえられる。
- 価値・懸念: 不正な材質入力と変換漏れを同時に抑えられる。emissive / occlusion の既存 backend 反映は未検証。
- 次の確認: ビルド・実機で色、金属度、粗さ、発光、AO の変換結果を確認する。

### 2026-08-03 — LensDistortionEffect の CPU/GPU パラメータ範囲を統一する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/LensDistortion/LensDistortionEffect.ixx`
- 事実: CPU / GPU の distortion / center / zoom setter は値を直接保持していた。
- 閃き・仮説: 両実装で同じ有限値・範囲補正を適用すると、レンズ歪みの CPU / GPU パスで入力条件を統一できる。
- 価値・懸念: 非有限値や不正 zoom による歪み処理の不安定化を抑えられる。distortion の最大値は未検証。
- 次の確認: ビルド・実機で CPU / GPU の中心、歪み、zoom 境界を比較する。

### 2026-08-03 — SpherizeEffect の CPU/GPU パラメータ境界をそろえる

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Spherize/SpherizeEffect.ixx`
- 事実: CPU / GPU の amount / radius / centerX / centerY setter は値を直接保持していた。
- 閃き・仮説: 両実装で同じ有限値・範囲補正を適用すると、球面化の CPU / GPU パスで入力条件を統一できる。
- 価値・懸念: 異常値や過大値による球面化処理の不安定化を抑えられる。amount の最大値は未検証。
- 次の確認: ビルド・実機で CPU / GPU の境界値・異常値入力を比較する。

### 2026-08-03 — EdgeBloomEffect のしきい値・半径・強度入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Glow/EdgeBloomEffect.ixx`
- 事実: threshold / radius / amount / edgeBoost / tintMix は clamp のみで、非有限値を明示的に補正していなかった。cmath include も不足していた。
- 閃き・仮説: 公開 setter で有限値を保証し、必要な標準ヘッダを直接 include すると、Edge Bloom の API 境界と module 自己完結性を改善できる。
- 価値・懸念: エッジ発光の入力安定性を高められる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で5項目の異常値入力を確認する。

### 2026-08-03 — ChromaticGlowEffect の光量・分散・角度入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Glow/ChromaticGlowEffect.ixx`
- 事実: threshold / radius / intensity / dispersion / tintMix は clamp のみで、angle は直接代入され、非有限値を明示的に補正していなかった。
- 閃き・仮説: 光彩の全数値 setter で有限値を保証すると、GPU / CPU 同期へ異常値が伝播しにくくなる。
- 価値・懸念: 色収差グローの入力安定性をそろえられる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で半径・分散・角度の異常値入力を確認する。

### 2026-08-03 — SphericalField の中心・半径・falloff と距離評価を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Field/SphericalField.ixx`
- 事実: center は配列を直接保持し、radius / falloffWidth は負値・非有限値を許し、evaluateAt は非有限座標や距離を検査していなかった。
- 閃き・仮説: 中心成分と半径系を有限・非負化し、評価入口と距離結果を検査すると、球形フィールドの影響度計算を安定化できる。
- 価値・懸念: 異常入力による球形影響の破損を抑えられる。falloffWidth と半径の相互関係は未検証。
- 次の確認: ビルド・実機で半径境界、falloff、非有限座標の評価を確認する。

### 2026-08-03 — RadialField の中心・軸・半径と距離評価を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Field/RadialField.ixx`
- 事実: center / axis と inner・outer radius は直接保持し、evaluateAt は非有限座標や垂直距離を検査していなかった。
- 閃き・仮説: ベクトル成分を有限化し、半径を非負化し、評価入口と距離結果を検査すると、放射フィールドの影響度計算を安定化できる。
- 価値・懸念: 異常値による放射影響の破損を抑えられる。axis の正規化と inner ≤ outer の関係は未検証。
- 次の確認: ビルド・実機で軸ゼロ、半径境界、非有限座標の評価を確認する。

### 2026-08-03 — LinearField の端点と投影評価を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Field/LinearField.ixx`
- 事実: start / end position は配列を直接保持し、evaluateAt は非有限座標や投影値を検査していなかった。
- 閃き・仮説: 端点 setter で各成分を有限化し、評価入口と投影結果を検査すると、線形フィールドの再計算・評価経路を安定化できる。
- 価値・懸念: 非有限入力による影響度破損を抑えられる。端点が意図的に無限大である利用法は想定しない。
- 次の確認: ビルド・実機で端点一致、ゼロ長、非有限座標の評価を確認する。

### 2026-08-03 — BoxField の寸法・falloff と評価入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Field/BoxField.ixx`
- 事実: halfExtent / falloffWidth は負値・非有限値をそのまま保持し、evaluateAt も非有限座標や距離の検査なしに計算していた。
- 閃き・仮説: setter で寸法を非負有限化し、評価入口と計算結果を検査すると、フィールド評価の異常値伝播を抑えられる。
- 価値・懸念: 物理・エフェクト評価の安定性を高められる。center の有限値検証は今回の差分範囲外。
- 次の確認: ビルド・実機で境界寸法、非有限座標、距離計算結果を確認する。

### 2026-08-03 — DirectionalGlowEffect の光条パラメータを有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/DirectionalGlowEffect.ixx`
- 事実: threshold / intensity / length / weight / angleOffset は clamp・max・直接代入のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 光条の全数値 setter で有限値を保証すると、GPU / CPU 同期へ異常値が伝播しにくくなる。
- 価値・懸念: Directional Glow の入力安定性を一貫して高められる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で光条長・重み・角度の異常値入力を確認する。

### 2026-08-03 — GaussianBlur の CPU/GPU sigma 入力を同じ範囲へ制限する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/GauusianBlur.ixx`
- 事実: CPU / GPU の sigma setter は値を直接保持しており、非有限値や過大値を境界で補正していなかった。
- 閃き・仮説: 両実装で同じ有限値・0〜64 の範囲を適用すると、カーネル生成と GPU パラメータの入力条件をそろえられる。
- 価値・懸念: 異常 sigma によるカーネル・GPU 設定の破損を抑えられる。既定値と最大値の妥当性は未検証。
- 次の確認: ビルド・実機で CPU / GPU sigma の境界値と異常値を比較する。

### 2026-08-03 — BlurEffect の公開パラメータ setter を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/Blur/BlurEffect.ixx`
- 事実: radius / strength / edgeThreshold は clamp または max のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 公開 module interface の setter で有限値を保証すると、Blur の CPU / GPU 実装同期へ異常値が伝播しにくくなる。
- 価値・懸念: 前回の実装側補正と合わせて API 境界も安定化できる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で radius / strength / edgeThreshold の異常値入力を確認する。

### 2026-08-03 — AutoMosaicEffect の feather 入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/AutoMosaicEffect.ixx`
- 事実: feather は非負化のみで、NaN / infinity を明示的に補正していなかった。
- 閃き・仮説: 公開 setter で有限値を確認してから非負化すると、モザイク境界の同期処理へ異常値が伝播しにくくなる。
- 価値・懸念: フェザー量の入力安定性を高められる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で異常値入力時の境界ぼかしを確認する。

### 2026-08-03 — LiftGammaGainEffect の個別 RGB setter を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/LiftGammaGainEffect.ixx`
- 事実: 個別の lift / gamma / gain RGB setter は clamp のみで、非有限値を明示的に補正していなかった。既存の一括 setter とは入力経路が分かれている。
- 閃き・仮説: 個別 setter でも有限値を保証すると、UI の単項目編集と一括編集でカラー調整状態の境界をそろえられる。
- 価値・懸念: 異常値が GPU / CPU 同期へ伝播する可能性を下げられる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で個別 RGB 編集と一括 setter の結果を比較する。

### 2026-08-03 — WhiteBalanceEffect の温度・色かぶり・明度入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/WhiteBalanceEffect.ixx`
- 事実: temperature / tint / brightness は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 公開 setter で有限値を保証すると、ホワイトバランス設定の同期先へ異常値が伝播しにくくなる。
- 価値・懸念: 色温度と補正量の入力安定性を高められる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で3項目の異常値入力とプリセット適用を確認する。

### 2026-08-03 — HDRDisplayEffect の表示モードと HDR パラメータを境界検証する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/ColorCorrection/HDRDisplayEffect.ixx`
- 事実: mode は enum 値を直接保持し、数値設定は非有限値を補正せず、プロパティ経路も setter を経由していなかった。プロパティの min / max も未設定だった。
- 閃き・仮説: enum を有効範囲へ制限し、数値を有限化し、プロパティ経路を setter へ統一することで、UI・直接 API・GPU 同期の境界をそろえられる。
- 価値・懸念: HDR 表示設定の破損値を抑えられる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で mode 範囲外、NaN / infinity、プロパティ編集を確認する。

### 2026-08-03 — ExposureEffect の露出・オフセット・ガンマ入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/ColorCorrection/ExposureEffect.ixx`
- 事実: exposure / offset / gammaCorrection は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 公開 setter で有限値を保証すると、露出処理の同期先へ異常値が伝播しにくくなる。
- 価値・懸念: HDR/SDR 露出調整の入力安定性を高められる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で3項目の異常値入力を確認する。

### 2026-08-03 — BrightnessEffect の4設定 setter を有限値境界で保護する

- 状態: 確認済み
- 関連: `Artifact/include/Effects/ColorCorrection/BrightnessEffect.ixx`
- 事実: brightness / contrast / highlights / shadows は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 公開 module interface の setter 境界で有限値を保証すると、実装同期前に異常値を遮断できる。
- 価値・懸念: 4つのトーン調整値を一貫して安定化できる。module interface の変更は再スキャン範囲が広がる可能性がある。
- 次の確認: ビルド・実機で4項目の異常値入力を確認する。

### 2026-08-03 — TritoneEffect の階調バランス設定を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/TritoneEffect.cppm`
- 事実: balance / softness / masterStrength / colorMix は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 4つの Custom 設定 setter で有限値を保証すると、階調分離と色混合の同期状態へ異常値が伝播しにくくなる。
- 価値・懸念: Tritone の入力安定性をそろえられる。既定値の見た目は未検証。
- 次の確認: ビルド・実機で4項目の異常値入力とプリセット切替を確認する。

### 2026-08-03 — ChannelMixerEffect の強度と 3x3 行列を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/ChannelMixerEffect.cppm`
- 事実: strength と RGB 3x3 matrix は入力を clamp / 代入するだけで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 行列の対角成分を恒等値、非対角成分をゼロへ戻す境界を設けると、破損した外部行列でも色変換の基準を維持しやすい。
- 価値・懸念: Channel Mixer の GPU / CPU 同期へ NaN / infinity が伝播する可能性を下げられる。行列係数の意図的な極端値は別途許容される。
- 次の確認: ビルド・実機で恒等行列と異常係数入力時の結果を確認する。

### 2026-08-03 — ColorWheelsEffect の RGB ホイールと master 値を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/ColorWheelsEffect.cppm`
- 事実: lift / gamma / gain / offset の RGB 値と master 値は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 4ホイールと4 master setter の境界で有限値を保証すると、色調整状態の同期先へ異常値が伝播しにくくなる。
- 価値・懸念: カラーグレーディング入力の安定性を一貫して高められる。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で RGB 各軸と master の異常値入力を確認する。

### 2026-08-03 — ColorBalanceEffect の RGB バランスと範囲設定を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/ColorBalanceEffect.cppm`
- 事実: shadow / midtone / highlight の RGB 調整値、各 range、masterStrength は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 3帯域の色調整と共通範囲設定を setter 境界で有限化すると、Custom 設定の同期状態を一貫して保てる。
- 価値・懸念: 色バランス入力の異常値伝播を抑えられる。帯域範囲の相互関係は未検証。
- 次の確認: ビルド・実機で3帯域の異常値入力と GPU / CPU 表示を確認する。

### 2026-08-03 — HueAndSaturation の RGB 色空間変換と入力境界をそろえる

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/HueAndSaturation.cppm`
- 事実: CPU 側の HSV 変換が BGR 定数を使っており、プロパティ入力も clamp のみで非有限値を補正していなかった。
- 閃き・仮説: RGBA ベースの処理へ RGB 変換定数を合わせ、Hue / Saturation / Lightness の入力境界で有限値を保証すると、色順と異常入力の不整合を同時に減らせる。
- 価値・懸念: CPU パスの色相処理が入力表現と整合しやすくなる。GPU パスとの完全一致は未検証。
- 次の確認: ビルド・実機で RGB 原色と半透明入力の CPU / GPU 結果を比較する。

### 2026-08-03 — LevelsEffect の入出力レベルを有限値として同期する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/LevelsEffect.cppm`
- 事実: input/output black・white は float を直接 double 設定へ渡し、gamma も非有限値の検査なしに clamp していた。
- 閃き・仮説: 5つの setter で有限値を保証し、gamma だけ既存範囲を維持すると、Levels の設定同期へ異常値が流れにくくなる。
- 価値・懸念: レベル調整の外部入力に対する安定性を高められる。black / white の相互関係は未検証。
- 次の確認: ビルド・実機で非有限値入力と black ≤ white の扱いを確認する。

### 2026-08-03 — GradientRampEffect の座標・opacity 入力を有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/GradientRampEffect.cppm`
- 事実: start / end 座標と opacity は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 座標と opacity の setter 境界で有限値を保証すると、グラデーション設定から同期先へ異常値が伝播しにくくなる。
- 価値・懸念: 描画領域と透明度の入力安定性をそろえられる。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で異常値入力時のグラデーション方向と透明度を確認する。

### 2026-08-03 — ColoramaEffect の色変換設定を有限値境界で統一する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/ColoramaEffect.cppm`
- 事実: phase / spread / strength / saturationBoost / contrast は clamp または max のみで、非有限値を明示的に処理していなかった。
- 閃き・仮説: 同じ Colorama 設定グループの setter で有限値を保証すると、Custom 設定から CPU / GPU 実装へ異常値が伝播しにくくなる。
- 価値・懸念: 色変換の外部入力に対する安定性をそろえられる。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で5項目の異常値入力とプリセット切替を確認する。

### 2026-08-03 — PhotoFilterEffect の主要設定を有限値境界で統一する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/PhotoFilterEffect.cppm`
- 事実: density / brightness / contrast / saturationBoost は clamp のみで、非有限値を明示的に補正していなかった。
- 閃き・仮説: 同じ Photo Filter 設定グループの setter で有限値を保証すると、Custom 設定の同期先へ異常値が伝播しにくくなる。
- 価値・懸念: 色調整の外部入力に対する安定性をそろえられる。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で4項目の異常値入力とプリセット切替を確認する。

### 2026-08-03 — SelectiveColorEffect の調整値を setter 境界で有限化する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/SelectiveColorEffect.cppm`
- 事実: strength と CMYK 各調整値は clamp のみで、非有限値を受け入れる余地があった。
- 閃き・仮説: グループ調整の全入力を setter 境界で有限化すると、Custom 設定と同期先の状態を一貫して保てる。
- 価値・懸念: 色調整パラメータの異常値伝播を抑えられる。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で各グループの異常値入力とプリセット切替を確認する。

### 2026-08-03 — InvertEffect の strength setter で非有限値を遮断する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/InvertEffect.cppm`
- 事実: strength は clamp のみで、NaN / infinity を明示的に補正していなかった。
- 閃き・仮説: setter 境界で有限値を保証することで、反転量の CPU / GPU 同期へ異常値が伝播する可能性を下げられる。
- 価値・懸念: 反転強度が常に有効範囲で維持される。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で異常値入力時の表示と保存結果を確認する。

### 2026-08-03 — CurvesEffect のプリセット生成と setter で有限値を共有する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/CurvesEffect.cppm`
- 事実: S カーブ生成と公開 setter が strength を clamp のみで扱い、非有限値が曲線点へ伝播し得た。
- 閃き・仮説: 曲線生成側と setter の両方で同じ有限値境界を適用すると、直接生成経路と通常編集経路の挙動をそろえられる。
- 価値・懸念: 異常入力による曲線破損を抑えられる。既定値の選択は未検証。
- 次の確認: ビルド・実機でプリセット変更時と直接 setter 呼び出し時の結果を確認する。

### 2026-08-03 — GrayscaleEffect の strength setter で非有限値を補正する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/GrayscaleEffect.cppm`
- 事実: strength は clamp のみで、NaN / infinity を明示的に拒否していなかった。
- 閃き・仮説: エフェクトの公開 setter で有限値を保証すれば、同期される CPU / GPU 実装へ異常値が伝播しにくくなる。
- 価値・懸念: グレースケール量が常に有効な範囲で動作する。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で異常値入力時のレンダリング結果を確認する。

### 2026-08-03 — FillEffect の opacity setter で非有限値を遮断する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/ColorCorrection/FillEffect.cppm`
- 事実: opacity は clamp 前に NaN / infinity を検査していなかった。
- 閃き・仮説: setter 境界で非有限値を既定値へ戻すことで、塗りつぶしの GPU / CPU 同期へ異常値が流れ込む経路を減らせる。
- 価値・懸念: 破損した外部パラメータでも opacity が有効範囲に保たれる。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で異常値入力時の保存・表示結果を確認する。

### 2026-08-03 — FindEdges の GPU 出力は入力アルファを保持する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/FindEdges/FindEdgesEffect.cppm`
- 事実: GPU パスはエッジ強度を RGB に書き込む際、アルファを常に `1` にしていた。また amount setter は非有限値を境界で補正していなかった。
- 閃き・仮説: 中心画素のアルファを出力へ引き継ぎ、入力境界で有限値を保証すると、透明レイヤー上のエッジ処理が CPU パスと整合しやすくなる。
- 価値・懸念: 透明度を壊さずに GPU エッジ検出を利用できる。CPU パスとの完全一致は未検証。
- 次の確認: ビルド・実機で半透明入力の GPU / CPU 結果を比較する。

### 2026-08-03 — BevelEffect の GPU 色順とパラメータ境界を CPU 側に合わせる

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Bevel/BevelEffect.cppm`
- 事実: GPU パラメータ転送で highlight / shadow の RGB 成分が BGR 順になっており、強度・softness は非有限値をそのまま受ける可能性があった。
- 閃き・仮説: 転送値を RGB 順へそろえ、setter 境界で有限値と範囲を保証すると、CPU と GPU の見た目差および異常入力の伝播を同時に抑えられる。
- 価値・懸念: ベベルの色再現と異常入力時の安定性が改善する。既定値の見た目は未検証。
- 次の確認: ビルド・実機で highlight / shadow の色順と NaN / infinity 入力時の結果を確認する。

### 2026-08-03 — BlurEffect の数値入力は有限値を明示的に補正する

- 状態: 確認済み
- 関連: `Artifact/src/Effects/Blur/BlurEffect.cppm`
- 事実: `setRadius` / `setStrength` / `setEdgeThreshold` は従来、NaN や無限大を通常の clamp/max だけでは防げなかった。
- 閃き・仮説: 外部パラメータ境界で `std::isfinite` を先に確認し、各既定値へ戻すことで、後段の GPU/CPU ブラー処理へ非有限値が伝播する経路を減らせる。
- 価値・懸念: エフェクト設定の破損時にも安定した既定動作を維持できる。既定値の妥当性は未検証。
- 次の確認: ビルド・実機で NaN / infinity 入力時の UI とレンダリング結果を確認する。

### 2026-07-28 — 連番シーケンスの再生は ArtifactImageLayer::draw と ImageSequenceSource の接続が次段階

- 状態: 未検証（設計案）
- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`ArtifactCore/src/Media/ImageSequenceSource.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- 事実: 今回の実装で sequence は 1 レイヤーに集約され、`sequencePaths` / `sequenceFrameRate` がレイヤー JSON に永続化されるようになった。ただし表示は代表フレーム（先頭）固定で、`ImageSequenceSource`（bounded LRU キャッシュ・先読み・差し替え検出実装済み）は Artifact 側から未使用のまま。
- 閃き・仮説: `ArtifactImageLayer` の Impl に `ImageSequenceSource` を保持し、コンポジションフレーム→（layer inPoint/startTime と sequenceFrameRate 換算）→ `seekSourceFrame` でフレーム切替するのが最小接続。AssetManager の sourceVersion 更新との二重キャッシュ（フレーム単位 LRU vs 代表パス単位 decodedPayload）の役割分担整理が必要。
- 価値・懸念: 動画デコードに依存せずタイムライン再生の基盤ができる。懸念は draw ホットパスでの同期読込（現行 prefetch は単一画像前提）と、フレーム切替時の GPU テクスチャ共有（`canShareSourceGpuTexture`）の整合。
- 次の確認: ビルド確認後、実機で sequence ドロップ→保存→再読込での関係維持を検証し、そのうえで draw 接続の実装単位を切る。

### 2026-07-28 — 非同期インポートと同期インポートの責務差は統合余地がある

- 状態: 未検証
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`（`importAssetsFromPaths` / `importAssetsFromPathsAsync` / `registerImportedAssets`）
- 事実: 同期版は「検出→フレームレート入力→コピー→登録」、非同期版は今回の対応で「コピー→検出→入力→登録」となり、キャンセル時の振る舞いが異なる（同期は中止、非同期は単体登録へフォールバック）。重複登録は `ArtifactProject::addAssetFromPath` の canonical パス重複排除で防がれる。
- 閃き・仮説: 将来的に同期版を非同期版＋完了待ちに寄せるか、登録部分を `registerImportedAssets` へ共通化すると二重実装を解消できる。
- 次の確認: 同期版の呼び出し元（Asset Browser の明示 import など）でキャンセル時中止の振る舞いが必要かユーザー確認。

### 2026-07-27 — SharedPtr への段階移行は std::shared_ptr との相互運用で進めやすい

- 状態: 調査中
- 関連: `ArtifactCore/src/Memory/SharedPtr.cppm`、`Artifact/src/Service/ArtifactEffectService.cppm`、`Artifact/src/Layer/ArtifactVideoLayer.cppm`
- 事実: `SharedPtr` は `std::shared_ptr` からの暗黙/明示構築と代入を受けられ、`makeShared(T*, deleter)` も備えているため、既存の `std::shared_ptr` 返却点を一気に総置換せずに wrapper 側へ寄せられる。
- 閃き・仮説: effect / video / undo のような高頻度パスは、所有型を `SharedPtr` にしても内部実装の一部に `std::shared_ptr` を残す段階移行が現実的。完全移行より先に、API 境界の direct `std::shared_ptr` を減らす方が安全に波及しやすい。
- 価値・懸念: 置換範囲を小さく保ちながら移行を進めやすい。一方、`makeShared(T*, deleter)` の使い方を雑に広げると、所有責任が見えにくくなるので、release 系の一時橋渡しに限るのがよさそう。
- 次の確認: `ArtifactProjectService` / `ArtifactPropertyWidget` など残る濃いクラスターで、API だけ先に `SharedPtr` 化して内部実装は段階的に追従する方針が通るか確認する。

### 2026-07-25 — テクスチャ画像形式の入口統合

- 状態: 実装済み・要検証
- 関連: `ArtifactCore/src/Asset/AssetImporter.cppm`、`Artifact/src/Asset/AssetDirectoryModel.cppm`、OIIO画像読込
- 事実: `FileTypeDetector` は GIF/HDR/WebP/ICO/DDS/KTX を画像として認識していたが、AssetImporter の対応拡張子一覧と Asset Browser の画像判定が一部一致していなかった。
- 対応: AssetImporter、AssetDirectoryModel、ArtifactAssetBrowser を `FileTypeDetector` の拡張子判定へ統一し、Browserだけが個別に扱っていた JPE/JFIF もDetectorへ移した。
- 価値・懸念: テクスチャ形式のインポート導線が統一される。一方、現行の `loadImageViaOIIO` は UINT8 RGBA へ正規化するため、HDR/float の完全な精度保持は別課題。
- 次の確認: OIIO ビルドで各形式の decode 可否を実ファイルで検証し、必要なら float バッファ経路を追加する。

<!--
テンプレート:

### YYYY-MM-DD — 短い題名

- 状態: 未検証 / 調査中 / 有望 / 採用見送り / 完了
- 関連: `path/to/file`、機能名
- 事実:
- 閃き・仮説:
- 価値・懸念:
- 次の確認:
-->
## 2026-07-26 - Resident Debug Agent boundary

- 状態: 実装済み・要実機検証
- Related files/features: `Artifact/src/AppMain.cppm`, `tools/debug-mcp-server`, playback diagnostics.
- Confirmed fact: a resident app-side agent can publish a lightweight playback snapshot without opening `AppDebuggerWidget`, and can cooperatively pause playback from MCP session state.
- Hypothesis / unverified: the same checkpoint path can be extended to property, render-resource, and buffer health probes without materially perturbing playback if sampling remains bounded.
- Value / concern: this gives the AI a live semantic observation point; arbitrary GPU memory inspection remains outside the current boundary.
- 対応: MCP側にwatchの登録・列挙・削除ツールを追加し、アプリ側のresident bridgeがwatch値をスナップショットへ反映するようにした。break hitには直前8件とresume後8件の有界スナップショットを保存する。重複していた軽量bridge writerは撤去し、完全なDebugBridgeFileWriterだけを常駐経路とした。
- Next check: ライブ再生でwatch登録、break hit、pause、resume後のafterSnapshotsを実機確認する。

### 2026-07-27 — 連番検出の欠番契約が実装と不一致

- 状態: 実装済み・要実機検証
- 関連: `ArtifactCore/include/Asset/AssetSequence.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`、画像連番インポート
- 事実: `Asset.Sequence` のコメントは連続した整数フレームだけを連番化すると説明しているが、`detectSequences()` は prefix / suffix / padding と最低枚数だけでグループ化し、隣接フレーム番号の連続性を検査していなかった。したがって `0001-0016, 0018-0048` も1本のシーケンスとして検出されていた。
- 対応: `MissingFramePolicy` を追加し、既定の `Split` では欠番位置で連番を分割するようにした。`Preserve` を明示指定した呼び出し側には `missingFrames` を返すため、将来の hold/error UI を無理なく追加できる。
- 価値・懸念: 欠番を黙って圧縮して再生フレーム番号がずれる事故を防ぐ。一方、VFX素材で意図的な欠番を1アセットとして扱う場合は、インポートUIから `Preserve` と明示ポリシーを選ぶ導線が今後必要になる。
- 次の確認: `J:\dev\ArtifactStudio_TestSequences\png_missing_frame` を使い、Project Viewでの検出結果、タイムライン配置後のフレーム17、保存・再読込時の挙動を実機確認する。

### 2026-07-27 — 2D/3Dギズモのツールモード同期が未統一

- 状態: 実装済み・要実機検証
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Widgets/Render/Artifact3DGizmo.ixx`
- 事実: ツール変更時の `setGizmoMode()` は `TransformGizmo` のモードだけを更新し、`Artifact3DGizmo` の `GizmoMode` は直接同期していなかった。3Dギズモの既定値は `Move` だった。
- 対応: Controllerに2D/3Dモード変換を集約し、3DメニューもControllerを通すようにした。実装を持たない3D `Full` メニューは撤去し、2DのAll/Anchor/Noneは安全に操作できる3D Moveへ正規化する。
- 価値・懸念: Move / Rotate / Scale の表示と操作を一致させ、無反応なFullモードを排除した。真の統合3Dハンドルは、hit-test優先順位を定義してから別途追加する。
- 次の確認: Move / Rotate / Scale を切り替えた際の3D軸描画、hover、drag operationを実機で確認する。

### 2026-07-27 — LODManagerの変更通知で旧値が失われる

- 状態: 実装済み・要実機検証
- 関連: `Artifact/src/LOD/ArtifactLODManager.cppm`、LOD policy
- 事実: `getDetailLevel()` は`currentLevel_`を`newLevel`へ更新した後、`detailLevelChanged(currentLevel_, newLevel)`を発火していたため、通知されるold/newが同じ値になっていた。現時点の検索では接続先は見つからない。
- 対応: 更新前のDetailLevelをローカル保存してから状態を書き換え、通知には正しいold/newを渡すようにした。
- 価値・懸念: LOD変更時だけresourceを切り替える設計で不要な再生成や切替漏れを防げる。`getDetailLevel()` が状態変更を伴う設計自体は、既存API互換のため維持している。
- 次の確認: 実機でズーム境界を往復し、通知値とrendererの品質遷移を確認する。

### 2026-07-27 — 廃止済みサブモジュールのローカル登録情報

- 状態: ユーザー判断待ち
- 関連: 親リポジトリの `.gitmodules`、ローカル Git 設定、`Artifact_dev_review`
- 事実: `git submodule update --init --recursive` は `.gitmodules` に URL がない `Artifact_dev_review` を検出して失敗する。親indexには古いgitlinkが残るが、同ディレクトリは `Artifact` リポジトリの `codex/review-artifact-fixes` worktree として実在し、独自のGit管理情報を持つ。
- 対応: worktreeとその作業を失う危険があるため、自動削除は行わなかった。
- 価値・懸念: 本来の3サブモジュール更新には影響しないが、汎用的な再帰サブモジュール操作は失敗する。親の不正gitlink除去は有益だが、レビューworktreeの保持要否を先に決める必要がある。
- 次の確認: `Artifact_dev_review` を保持するか破棄するかを決めた後、親indexからgitlinkを外すか、正式なsubmoduleとして復元するかを選ぶ。

### 2026-07-27 — RT callback の SharedPtr 化は 2 スロット切替で逃がせる

- 状態: 実装済み・要検証
- 関連: `ArtifactCore/src/Audio/AudioRenderer.cppm`
- 事実: `std::atomic<std::shared_ptr<...>>` を直接 `SharedPtr` に置き換えるのはできないが、2 スロットの `SharedPtr` を用意して、設定側が非アクティブ側へ書き込み後に atomic index を切り替える方式なら、audio callback 側をロックなしのまま `std::shared_ptr` 露出を外せる。
- 価値・懸念: RT 経路のロックを増やさずに基盤 wrapper を導入できる。一方で、複数 writer の同時設定や slot 初期化順序は再確認が必要。
- 次の確認: 実機で callback 付け替え中の音切れ、停止後の null callback、連続再設定時の race を確認する。

### 2026-07-27 — routing を raw pointer key にすると SharedPtr 依存を減らせる

- 状態: 実装済み・要検証
- 関連: `ArtifactCore/src/Audio/AudioMixer.cppm`
- 事実: `AudioBus` の実体寿命を `SharedPtr` の所有で維持しつつ、routing map のキー/値だけを raw pointer にすると、比較・探索の都合で残っていた `std::shared_ptr` 依存をかなり減らせた。
- 価値・懸念: 内部グラフの保持を wrapper へ寄せやすい。反面、raw pointer から `SharedPtr` を復元するための resolve 経路が必要で、バス削除時の参照掃除を忘れると dangling を招く。
- 次の確認: bus 削除、再接続、serialize/deserialize 後に routing が正しい `SharedPtr` を返すかを確認する。

### 2026-07-27 — `ArtifactArray` の実装が二重化している

- 状態: 完了
- 関連: `ArtifactCore/include/Core/ArtifactArray.ixx`、`ArtifactCore/src/Memory/ArtifactArray.cppm`
- 事実: 同一の `ArtifactCore::ArtifactArray` 名に対して、`Core.ArtifactArray` は `::operator new/delete` を直接使うコンテナを、`Memory.ArtifactArray` は `std::allocator` をテンプレート引数に持つ別実装を公開していた。全ソース検索では前者だけが `Core.ArtifactFoundation` 経由で利用されていた。
- 対応: 未使用の `Memory.ArtifactArray` 実装とCMakeの明示登録を削除し、`Core.ArtifactArray` を唯一の `ArtifactArray` 定義にした。
- 価値・懸念: 型名衝突とアロケータ方針の曖昧さを解消した。今後は用途別確保が必要な箇所で `Memory.ArtifactAllocators` と `std::pmr` コンテナを明示利用する。
- 次の確認: CMake再生成後にモジュール依存スキャンと通常ビルドで未参照モジュール削除を確認する。

### 2026-07-29 — ドキュメント INDEX の詳細 Status 同期と自動生成が衝突する

- 状態: 緩和策実装済み・再生成確認待ち
- 関連: `tools/generate_doc_inventory.py`、`docs/INDEX_GENERATED.md`、2026-07-27 以降の planned milestone 文書
- 事実: 生成スクリプトは新規 Markdown を収集できるが、文書ヘッダの短い Status だけを抽出するため、実装監査に基づき INDEX へ手動同期した詳細な Partial／Not started の説明を再生成時に失う。また、最新 milestone 文書が生成後に追加された場合は INDEX から欠落する。
- 価値・懸念: `tools/generate_doc_inventory.py` に既存 INDEX の詳細 Status 引き継ぎと、`状態:`／`進捗状態:` の抽出を追加した。これにより新規文書の収集と既存監査状態の保持を両立できる見込みだが、実 INDEX 再生成時の差分確認はまだ行っていない。
- 次の確認: INDEX をバックアップ可能な手順で一度再生成し、新規 milestone の収録、既存 Status の保持、件数・分類差分を確認する。

### 2026-07-29 — ドキュメント INDEX dry-run はファイルごとの git log がボトルネックになる

- 状態: 未解決・性能課題
- 関連: `tools/generate_doc_inventory.py::get_git_last_modified`
- 事実: 生成器は Markdown ごとに個別の `git log -1` を実行する。1100 件超の文書を対象に Status 引き継ぎを含む dry-run を行ったところ、長時間経過しても完了せず、処理を停止した。INDEX の書き込みは発生していない。
- 価値・懸念: 文書数が増えるほど生成・検証の反復が遅くなり、Status 同期の安全確認を阻害する。git log の一括取得またはファイル単位の不要な履歴照会削減が必要。
- 次の確認: `git log --name-only` 等の一括履歴マップを作り、現行出力と同じ Modified 日付を保ったまま生成時間を測定する。

### 2026-07-30 — Audio Scrub の再入場時バッファ境界

- 状態: 静的対策実装済み・実機確認待ち
- 関連: `Artifact/src/Audio/ArtifactAudioScrubController.cppm`
- 事実: `stopScrub()` は出力バッファを消去するが、停止通知を経由せず再入場する呼び出しでは前回のキューが残り得る。
- 対応: `startScrub()` でも開始前にバッファを消去し、前回のデバイスオープン失敗状態をリセットする。
- 価値・懸念: 新しいドラッグで古いスクラブ音が再生される可能性を抑えた。実機で stop/start、再入場、デバイス失敗復帰を確認する必要がある。

### 2026-08-01 — フレームギズモの2D／3D操作経路統一

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact3DGizmo`
- 事実: ビューキューブ後は2Dレイヤーにも3Dフレームギズモを描画していたが、入力条件が3Dレイヤー限定だったため、ハンドル操作とドラッグ移動が開始できなかった。
- 対応: フレームギズモの描画・ヒットテスト・ドラッグ更新を3Dギズモへ統一し、2Dレイヤーでは位置・回転・スケールだけ既存の2D Transformへ書き戻す。Text Gizmoは専用責務として維持する。
- 価値・懸念: ビューキューブの有無でギズモ操作経路が分岐しなくなる。2D通常表示、ビューキューブ表示、3Dレイヤーでのハンドル位置と回転／スケールの実機確認が必要。

### 2026-08-01 — BrushTool の共有状態と VP 入力

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Application/ArtifactApplicationManager.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Tool/ArtifactBrushTool.cppm`
- 事実: Brush / Eraser の入力は Viewport Controller から `ArtifactBrushTool` へ渡される。Diameter、Opacity、Hardness、Spacing、Angle、Roundness は Application Manager が所有する共有ツールへ設定し、PaintLayer がストローク単位で参照する構成にした。
- 価値・懸念: Tool Options と Viewport が別インスタンスを参照して設定が反映されない問題を避けられる。一方、途中適用されたストロークの Undo 粒度と筆圧入力は未検証である。
- 次の確認: 実ブラシ描画で設定変更が即時反映されること、連続ストロークの Undo が一筆単位になること、Eraser の透明化が期待どおりであることを確認する。

### 2026-08-01 — 図形の中心作成はプレビューだけでなく確定矩形も中心基準にする

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、Rectangle / Ellipse tool
- 事実: Alt ドラッグは開始点を中心として扱う必要がある。終点だけを開始点から反対側へ延長しても、確定時の `QRectF(start, end)` が中心を反映しないため、プレビューと生成結果の位置がずれる。
- 対応: 中心作成フラグをセッション状態として保持し、プレビューと確定処理の双方で `center - delta` から `center + delta` の矩形を生成する共通計算を使用した。
- 価値・懸念: Alt／Shift+Alt の作成結果と表示が一致する。実機でマウス移動中の修飾キー切替、負方向ドラッグ、マスク／シェイプ双方を確認する必要がある。

### 2026-08-01 — Text ツールの作成候補は Esc で確定前に破棄する

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `ArtifactCompositionRenderController`、`ArtifactCompositionEditor` の Text tool input
- 事実: Text ツールのマウス押下後からリリース前まではレイヤー未作成の候補状態だが、既存の Esc 処理はマスクとブラシに限定されていた。
- 対応: 候補フラグ、ドラッグ状態、始点／終点を消去する `cancelTextToolInteraction()` を追加し、エディタの Esc 処理から呼び出すようにした。
- 価値・懸念: クリック位置を誤った場合に不要な Text レイヤーを作成せずに操作を取り消せる。確定済み Text の編集ダイアログに対する Esc は既存の Qt ダイアログ責務を維持する。

### 2026-08-01 — Point Text の Enter は編集確定、Box Text は改行を維持する

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm::ArtifactTextEditorDialog`
- 事実: 既存の Text 編集ダイアログは通常の Enter を常に `QTextEdit` に渡すため、Point Text でも改行が挿入されていた。Box Text は段落編集のため通常 Enter を改行として扱う仕様である。
- 対応: Point Text の通常 Enter を `accept()` に接続し、Ctrl+Enter の既存確定操作と Esc の破棄操作は維持した。Box Text と修飾キー付き Enter は従来どおり編集欄へ渡す。
- 価値・懸念: Point／Box の編集モデル差をダイアログ側で明示できる。実機で IME 確定キー、Ctrl+Enter、Box Text の改行挿入を確認する必要がある。

### 2026-08-01 — Puppet のピン操作は既存選択状態をキー操作へ接続する

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `ArtifactPuppetTool`、`CompositionRenderController`、`CompositionViewport`
- 事実: Puppet はクリックでピン追加／選択と変形を行っていたが、CtrlクリックのStarch種別指定とDelete/Backspaceによる選択ピン削除が入力経路に接続されていなかった。
- 対応: Ctrlクリックで追加直後のピンをStarch(type=1)に設定し、Puppetツール中のDelete/Backspaceを選択ピン削除へ振り分けた。
- 価値・懸念: ピンの基本ライフサイクルがVP操作だけで完結する。回転ハンドル、Overlap深度、キーフレーム化は未検証・未実装である。

### 2026-08-01 — MotionSketch の速度表示に区間加速度を追加する

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の MotionSketch overlay
- 事実: スケッチ中のオーバーレイはサンプル数と直近区間の速度を表示していた。
- 対応: 3点以上のサンプルがある場合、直近速度と1区間前の速度の差を加速度として計算し、HUDに `v` と `a` を表示する。表示幅も拡張した。
- 価値・懸念: ドラッグの加速／減速を記録中に確認できる。サンプル間隔が一定でない場合は厳密な物理単位ではなく、隣接サンプル差分の表示である。

### 2026-08-01 — Brush は QTabletEvent を既存 Mouse 経路へ合流させる

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`ArtifactCompositionRenderController`、`ArtifactBrushTool`
- 事実: CompositionViewport は QMouseEvent のみを Controller に渡しており、タブレットの pressure が BrushStroke に届かなかった。
- 対応: `WA_TabletTracking` を有効化し、TabletPress/Move/Release を Controller の既存 Brush 経路へ渡す圧力ブリッジを追加した。BrushTool は pressure を 0.05〜1.0 にクランプし、半径と不透明度へ反映する。通常マウスは圧力 1.0 のまま動作する。
- 価値・懸念: 新しい公開シグナルや描画経路を増やさず、タブレットだけ筆圧を利用できる。実機で押下・筆圧変化・離上、マウス復帰、消しゴムモードを確認する必要がある。

### 2026-08-01 — Clone Stamp は同一 Paint フレームのF32サンプルから開始する

- 状態: 基礎実装済み・ビルド／実機確認待ち
- 関連: `ArtifactPaintLayer::applyCloneStampAtFrame`、`ArtifactCompositionRenderController`、`ToolType::Clone`
- 事実: 既存 PaintLayer は `ImageF32x4_RGBA` のCPUバッファを公開しており、QImageを介さずピクセル複製できる。Clone Stampの既存UI欄はあったが、ToolTypeと入力経路が未接続だった。
- 対応: Clone toolを追加し、Alt+クリックでPaintレイヤーのソースを設定、通常ドラッグでソース／描画位置のオフセットを保った円形スタンプを適用する。選択中レイヤーと異なるPaintレイヤーをVP上でAltクリックした場合は、そのレイヤーをサンプル元にする。Cloneオプションのフレームオフセットをサンプル元の現在フレームへ加算し、時間ずれの複製を可能にした。最初の適用だけUndoスナップショットを記録し、ソース領域は事前コピーして重なりによる自己汚染を防いだ。既存の「位置固定」オプションも共有BrushTool経由で接続し、オフ時はソースを固定する。Esc はドラッグ中の操作と設定済みソースをクリアし、設定中はVPにSourceマーカーを表示する。
- 価値・懸念: 静止画Paintレイヤーの基本的なクローン操作が成立する。別レイヤー／時間オフセット／非整列サンプリング、筆圧ごとのスタンプ形状は次段階であり、実機で境界・重なり・Undoを確認する必要がある。
### 2026-08-01 — Render Queue 選択的レンダリングのジョブモデルを拡張

- 状態: 実装済み（ジョブモデルと JSON 保存/復元、実レンダリング分岐・UI・ビルド/実行検証は未完了）
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`, `docs/spec/SPEC_RENDER_QUEUE_SELECTIVE_2026-07-31.md`
- 事実: 既存 `ArtifactRenderJob` は開始/終了フレーム、解像度などの基本値のみを持ち、選択レイヤー、ROI、パス分割、解像度プリセットを保持できなかった。
- 実装: `FrameRangeMode`、`RegionMode`、`LayerFilterMode`、`ResolutionPreset`、`RenderPassConfig` と関連フィールドを追加し、LayerID リストを含む JSON 保存/復元を追加した。未知の enum 値は後方互換の既定値へ戻す。
- 実装追加: ソフトウェア単一フレーム経路にブラックリスト、Selected/Custom/Solo/Visible フィルタ、ROI クロップ、解像度プリセットを接続し、キュー実行時に Composition/WorkArea/SingleFrame の有効範囲を解決する。
- 実装追加: GPU 単一フレーム経路にも同じレイヤーフィルタ、ROI、解像度プリセットを適用した。GPU は readback 後に ROI/出力サイズを適用する。
- 実装追加: キューの通常フレーム処理（`renderSingleFrame`）にも GPU レイヤーフィルタを伝播し、GPU 初期化サイズを解像度プリセットから解決するようにした。
- 実装追加: `ArtifactRenderQueueService` に `jobSelectiveSettingsAt` / `setJobSelectiveSettingsAt` を追加し、範囲・ROI・レイヤー ID リスト・除外設定・パス設定・解像度プリセットを QVariantMap 経由で UI/自動化から編集可能にした。
- 実装追加: `WorkspaceAutomation` に `getRenderQueueJobSelectiveSettingsAt` / `setRenderQueueJobSelectiveSettingsAt` を登録し、同じ設定を自動化呼び出しから読み書きできるようにした。
- 実装追加: `splitPasses` をキュー開始時に有効な `RenderPassConfig` ごとの独立ジョブへ展開する処理を追加した。各ジョブはパス名をジョブ名・出力名へ反映し、パスのフィルタ/ID リストを通常のレンダー設定へ変換する。
- 実装追加: ROI 使用時の Composition/Half/Third/Quarter 解像度計算をコンポジション全体ではなくクロップ矩形サイズ基準へ変更した。
- 実装追加: `SelectedFrames` 用に `selectedFrameRanges` をジョブへ追加し、JSON/VariantMap で保持できるようにした。キュー開始時は非連続区間ごとに Custom 範囲の独立ジョブへ展開し、出力名へ `_f開始-終了` を付ける。
- 懸念: UI 側での非連続フレーム選択編集は未実装。範囲の終了値は既存キュー契約に合わせて排他的に扱う。
- 改善: SelectedFrames 展開前に不正区間を除外し、重複/隣接区間を統合する。全区間が不正な場合は元ジョブを保持して消失を防ぐ。
- 実装追加: Render Queue preflight で ROI の空/範囲外、SelectedFrames の有効区間ゼロ、Render Pass 分割の有効パスゼロを診断する。範囲外 ROI は実行時クリップのため Warning、空設定は Error とした。
- 修正: `Selected` レイヤーフィルタの空ホワイトリストを「全レイヤー」ではなく「描画対象なし」として扱い、preflight で Error にする。Selected Render Pass の空 Layer ID も同様に検出する。
- 修正: `Solo` フィルタを、Solo レイヤーが存在する時だけ Solo レイヤーに限定し、Solo が一つもない場合は通常の可視レイヤーへフォールバックする AE 互換挙動に統一した。
- 確認・実装: `ArtifactAbstractLayer` には既存の `isGuide()` があるため新しい属性を増やさず、`excludeGuideLayers` を CPU/GPU の全レイヤーフィルタへ接続した。
- 実装追加: 外部レンダラー呼び出しにもキュー実行時に解決した start/end frame を渡し、WorkArea/SingleFrame/SelectedFrames 展開と外部経路の範囲を一致させた。
- 実装追加: Composition View の既存グリッド描画に、ズームに応じた 1-2-5 系列の自動主間隔を追加した。固定間隔へ戻す `setGridAutoStepEnabled(false)` も提供し、保存済み `GridSettings` 自体は変更しない。
- 懸念: 自動ステップの目標表示間隔は 100 viewport px 固定で、複数グリッド/極座標/アイソメトリック/ラベル描画は未実装。
- 実装追加: 既存 `GridSettings::showNumbers` を Composition View overlay で利用し、主グリッド線の X/Y 座標ラベルを最大 48 個ずつ描画するようにした。ズームアウト時のラベル密集を避けるため画面間隔 24px 未満では抑制する。
- 実装追加: Composition View に極座標グリッド表示モードを追加し、同心円リング・24方向の放射線・中心マーカーを既存 primitives で描画する。`setGridPolarMode(false)` で従来の矩形グリッドへ戻せる。
- 実装追加: Composition View にアイソメトリックグリッド表示モードを追加し、30°/150°/90° の3方向の平行線ファミリを描画する。極座標/矩形グリッドとは排他的に表示する。
- 改善: 極座標とアイソメトリックの setter は有効化時に相互排他を適用し、両モードの同時描画を防ぐ。
- 実装追加: 極座標モードで `subdivisions` に基づく細分リングも描画し、主リングとの重複を除外する。細分間隔が 4 viewport px 未満の場合は描画を抑制する。
- 実装追加: アイソメトリックモードにも `subdivisions` に基づく3方向の細分線を追加し、細分間隔が 4 viewport px 未満の場合は抑制する。
- 実装追加: `CompositionRenderController::snapCanvasToGrid()` を追加し、矩形は XY 丸め、極座標は半径/15°角度丸め、アイソメトリックは現在の細分間隔で座標丸めを提供する。極座標中心は controller が保持する最新キャンバスサイズから求める。
- 実装追加: `ArtifactCompositionRenderWidget` の Ctrl レイヤードラッグから `snapCanvasToGrid()` を呼び出し、表示グリッドモードと実際の移動スナップを接続した。既存のコンポジション中心スナップは先に適用し、グリッドスナップが最終位置を決める。
- 実装追加: 自動グリッドステップの目標 viewport 間隔を `setGridAutoStepTargetViewportInterval()` で変更可能にし、表示とスナップの両方が同じ設定を使用する。値は 24〜512 px に制限する。
- 懸念: パス展開はキュー開始時に行うため、開始後のジョブ数表示が増える。既存のジョブ ID はインデックスベースなので、UI は開始前に設定する必要がある。
- 懸念: ガイドレイヤーの判定 API、SelectedFrames の非連続区間、GPU レンダーパス、パス別出力、Advanced UI は未接続。現状 `SelectedFrames` は保存済みの custom 範囲へフォールバックする。
- 次に確認: `renderSingleFrameGPU` と外部レンダラーへ同じ範囲・レイヤーフィルタ契約を伝播し、Render Queue UI からこれらの設定を編集可能にする。
- 実装追加: アイソメトリックグリッドのスナップを画面 X/Y の独立丸めから斜交基底の格子座標丸めへ変更した。描画中の斜線ファミリの交点にスナップしやすくなる。描画とスナップの基底値が将来別々に変更されないかは未検証。
- 修正: Composition View の Shift+ホイール横移動は、横チルト入力が存在する場合に `angleDelta().x()` を優先するよう変更した。横デルタを持たないマウスでは従来どおり縦デルタを横移動へフォールバックする。
- 実装追加: Render Queue のジョブ詳細プレビューに、サービスが保持する選択的レンダー設定（範囲、領域、レイヤーフィルタ、解像度、Crop）を表示するようにした。編集 UI そのものではないが、設定が保存されているかをジョブ単位で確認できる導線になる。
- 実装追加: Composition Render Widget に `setRotationSnapDegrees()` / `rotationSnapDegrees()` を追加し、Shift+キャンバス回転の刻みを既定45度から外部設定できるようにした。値は1〜360度へ制限し、15/30/45/90度などのプリセットを利用できる。
- 改善: Shift+回転ドラッグ中は Alt で15度、Ctrl で90度の一時プリセットを選べるようにした。通常は widget に設定された既定刻みを使うため、既存操作との互換性を保つ。
- 改善: Composition View の回転ドラッグ角度を横移動量だけでなく、ビューポート中心からの開始ベクトルと現在ベクトルの角度差から算出するようにした。中心付近だけは角度が不定になるため、従来の横移動フォールバックを残している。
- 改善: 回転ギズモモードでも既存の `drawAnchorCenterOverlay()` を表示するようにし、回転中心を視覚的に確認できるようにした。描画実体は既存のアンカー表示を再利用している。
- 実装追加: タブレット入力の `xTilt/yTilt` を `ArtifactBrushTool` へ転送し、傾きの大きさをブラシ真円度、方向をブラシ先端角へ反映した。筆圧と同様にストロークへ記録し、TabletRelease で傾きを初期化する。
- 実装追加: `ArtifactBrushTool` に `pressureAffectsSize` / `pressureAffectsOpacity` を追加した。既定は両方有効で、筆圧をサイズだけ／不透明度だけへ割り当てる編集面を将来追加できる。
- 修正: タブレット筆圧の下限を0.05から0.0へ変更し、筆圧ゼロを正しく無描画として扱えるようにした。通常のマウス／TabletRelease の復帰値は1.0のまま。
- 実装追加: 3Dギズモのドラッグ中に、Move/Rotate/Scale のモード名と Position・Rotation・Scale の現在値を右上HUDへ表示するようにした。既存の renderer オーバーレイ primitive のみを使用し、操作終了時は自動的に消える。
- 修正: 3Dギズモから2D/3Dレイヤーへスケール値を反映する際、各軸の絶対値を最低0.001にクランプするようにした。反転スケールの符号は維持し、ゼロ化による退化や後続の逆変換不安定化を避ける。
- 改善: 3DギズモのドラッグHUDに、`localBounds` と現在スケールから算出した表示サイズ（px）を追加した。位置・回転・スケールと同じドラッグ時の値として提示する。
- 実装追加: 3Dギズモの Rotate モードで Shift を押している間は回転を15度刻みにスナップし、Ctrl 同時押しではスナップを無効化する。既存のギズモ内部ドラッグ計算後に適用するため、位置・スケールの挙動には影響しない。
- 実装追加: 3Dギズモのドラッグ開始時に Position／Rotation／Scale のスナップショットを取得し、終了時に `GizmoTransformUndoCommand` を1件だけUndoManagerへ登録する経路を追加した。3D投影フレームのコーナードラッグと通常の3D軸ドラッグを対象とし、2D専用TransformUndoCommandとは分離している。
- 改善: 3DギズモUndo/Redoの復元後に既存の `LayerChangedEvent::Modified` を発行するようにし、プレビュー・タイムライン等の購読側へ変更を伝播するようにした。
- 安定化: 3Dギズモのドラッグが成立しなかったマウスリリースでも保留中のUndoスナップショットを破棄するようにし、次回操作へ古い履歴状態が漏れないようにした。
- 改善: 3DギズモのUndo登録前に Position／Rotation／Scale の差分を閾値比較し、実質的な変更がないクリック／ドラッグでは空のUndo項目を作らないようにした。
- 実装追加: Composition View のホイールズームを既存の16msタイマーで約150ms補間するようにした。smoothstep補間とズーム位置のキャンバスアンカー再計算により、マウス位置を保ったまま滑らかに拡大縮小する。連続ホイール入力では目標倍率を更新して追従する。
- 改善: Composition View の `zoomIn()` / `zoomOut()` も同じ smooth zoom 経路へ統一した。ツールバーや既存アクションからの倍率変更でも、ビューポート中央をアンカーに補間される。
- 安定化: Reset View／Fit／100% など即時ビューポート操作では進行中のズーム補間をキャンセルし、タイマーの残りフレームが新しいビュー状態を上書きしないようにした。
- 実装追加: キャンバス回転ドラッグ中の角度を既存Info Overlayへ表示し、マウスリリース時に消去するようにした。スナップ後の実際の角度を1桁小数で表示する。
- 改善: Zoom Tool の通常クリック／Altクリックもホイール・ツールバーと同じsmooth zoom経路へ統一した。クリック位置をアンカーにした拡大／縮小が約150msで補間される。
- 実装追加: Zoom Tool のマーキー領域ズームも、選択領域のキャンバス中心をビューポート中心へ移す補間経路へ変更した。倍率計算は従来どおり領域全体が収まる値を使い、表示遷移だけを滑らかにしている。
- 修正: 回転角度HUDの消去を回転ドラッグ時だけに限定した。Clone／Eraser等、別操作が保持しているInfo Overlayを通常のマウスリリースで消さないようにした。
- 実装追加: `CompositionRenderController::zoomAtFactor()` も150msのsmoothstep補間へ変更した。Editorが直接呼ぶホイール／ズーム操作でも、物理viewportアンカーを維持したまま拡大縮小する。Fit／100%は進行中補間をキャンセルする。
- 安定化: Controller側のズーム補間に世代番号を追加し、連続入力やFit／100%切替後に古い `singleShot` コールバックが新しいズーム状態へ干渉しないようにした。
- 実装追加: Composition Render Widget のパンを、ドラッグ終了時の速度から0.86倍ずつ減衰させる慣性スクロールへ拡張した。既存16msタイマーを共有し、ホイール入力開始時には慣性を停止する。
- 安定化: 慣性パンへ渡す速度を各軸±3px/msに制限し、低頻度イベントや一時的な入力遅延による過大な飛びを防いだ。
- 実装追加: 実際の入力を処理する `ArtifactCompositionEditor` 側にもパン慣性を追加した。RenderWidget側と同じ速度上限・0.86減衰・weak_ptr再スケジュールを使い、Space／中ボタンのパン終了後も自然に継続する。
- 安定化: CompositionEditorで新しいマウスジェスチャーが始まると、前回のパン慣性を即時停止するようにした。選択やギズモ操作中に背後のパンが継続しない。
- 修正: 実際のCompositionEditorのShift+ホイール横移動も横チルト入力を優先するよう変更し、横軸のあるマウスと縦軸のみのマウスの両方で意図どおり動作するようにした。
- 安定化: CompositionEditorのホイール入力開始時にもパン慣性を停止し、ホイールズーム／横移動と残留パンアニメーションが同時に走らないようにした。
- 改善: Controllerの連続ズーム中はView Historyを入力イベントごとに追加せず、アニメーション開始時の1件だけを記録するようにした。`zoomInAt/zoomOutAt` の重複pushも削除した。
- 改善: `CompositionRenderController::panBy()` でも履歴追加を250ms単位にまとめ、パン慣性の各フレームが個別のView History項目にならないようにした。パン入力開始時には進行中のズーム補間もキャンセルする。
- 改善: CompositionEditorでEscapeを押した場合もパン慣性を即時停止するようにし、既存の各種キャンセル操作と同じくビューの継続移動を止められるようにした。
- 安定化: `CompositionRenderController::panBy()` にX/Yの有限値検証を追加し、NaN／無限値を履歴・renderer・ズームキャンセル処理へ流さないようにした。
- 改善: Controller側の各ズーム補間フレームで renderer の `zoomAroundViewportPoint()` を使うようにし、キャンバス回転中でもviewportアンカーがずれないようにした。手計算の `setZoom + setPan` は削除した。
- 安定化: Controller側の再スケジュール式ズームコールバックをweak_ptr＋一時的shared_ptrで保持する方式へ変更し、補間終了後にstd::functionが自己循環して残留しないようにした。
- 安定化: ControllerのReset View／Fit Selection／Fit Visible でもズーム補間の世代を無効化するようにし、即時ビュー変更後に古い補間が再適用されないようにした。
- 安定化: `zoomAtFactor()` に有限値・正値チェックを追加し、外部入力のNaN／無限値／0以下でズーム状態やアニメーションを壊さないようにした。
- 改善: 3D投影フレームの上下左右エッジハンドルを、ドラッグ時にそれぞれY／X軸のスケール経路へ振り分けるようにした。従来はコーナーと同じScreen軸へ流れていたため、単一辺の操作でも両軸が変化する可能性があった。コーナーは既存のScreen軸経路を維持する。
- 実装追加: 3D投影フレームの上側に回転ハンドルを描画し、投影空間でヒットテストするようにした。選択時は既存の3DギズモのZ軸Rotate経路へ振り分け、既存の回転HUD・Shift 15度スナップ・Undoスナップショットを再利用する。ハンドル位置はフレーム上辺からのローカル空間オフセットで決める。
- 改善: 3D投影フレームの回転ハンドル用leader lineを、上辺との接続線ではなくフレーム中心からハンドルまでの線として描画するようにした。投影後のフレーム姿勢に追従するため、回転中の現在方向を視覚的に読み取りやすくなる。
- 実装追加: 3D投影フレームのコーナー／エッジ／回転ハンドルをダブルクリックした際のリセット経路を追加した。回転ハンドルはZ回転を0度、その他のフレームハンドルはXYスケールを1.0へ戻し、GizmoTransformUndoCommandとLayerChangedEventを通してUndo・表示更新へ接続する。
- 改善: 3D投影フレームの上下左右エッジをShiftドラッグした場合、ポインタで駆動した軸の倍率をもう一方にも適用し、ドラッグ開始時のアスペクト比を維持するようにした。Ctrl併用時は自由リサイズを優先し、コーナーと回転ハンドルには影響しない。
- 改善: 3D投影フレームのエッジリサイズで、Ctrlなしの場合は反対側の辺が固定されるよう、ローカル軸方向へサイズ変化の半分だけギズモ位置を補正するようにした。Ctrl時は中心固定として位置補正を行わない。カメラ投影後の見た目ではなく、レイヤーのワールド変換から軸方向を求める。
- 改善: ブラシカーソルプレビューにも現在の筆圧を反映するようにした。Pressure Affects Size時は直径を筆圧で縮小し、Pressure Affects Opacity時は透明度を低下させる。筆圧0でもカーソル自体は視認できる最小径・最小透明度を保つ。
- 改善: ブラシドラッグ中のストロークプレビューにもPressure Affects Opacityを反映し、カーソル輪郭だけでなく描画中の軌跡も現在の筆圧に応じて薄くなるようにした。筆圧0付近でも軌跡が完全には消えない下限を設けている。
- 実装追加: Penでマスク頂点を追加する際、Shift押下中は直前の頂点からの角度を45度刻みにスナップするようにした。既存のpending mask作成と後続のUndo・LayerChangedEventはそのまま利用し、初回頂点や既存頂点編集には影響させない。
- 改善: Penの新規マスク頂点追加でCtrlを押している場合、コンポジション左・右・上・下境界へ10px（ズーム補正済み）の吸着判定を行うようにした。境界スナップは新規頂点作成だけに限定し、既存頂点編集やハンドル操作の座標は変更しない。
- 拡張: Ctrlスナップの候補に、可視レイヤーの既存マスク頂点を追加した。候補はキャンバス座標へ変換して最近傍を選ぶため、レイヤーごとの移動・回転を考慮できる。ロック状態や既存パスの編集経路には介入せず、新規頂点配置だけに適用する。
- 改善: Ctrlスナップが実際に成立した新規マスク頂点位置へ、作成中だけシアン色のクロスマーカーを表示するようにした。pending maskの解除時に表示状態と座標をクリアし、他のマスク編集やツールへ残留しない。
- 修正: マスク新規頂点のCtrlスナップはAlt併用時に無効化するようにした。Ctrlで吸着、Altで吸着解除という仕様上の修飾キー優先順位を明示し、Shiftの45度制約は別途継続する。
- 改善: 既存マスク頂点のShiftドラッグも、新規頂点追加と同じ8方向（45度刻み）へスナップするよう統一した。複数選択頂点の移動では主頂点の補正デルタが従来どおり他頂点へ適用される。
- 修正: 既存マスク頂点のドラッグ中スナップでもAlt併用時は吸着を無効化するようにし、新規頂点追加時のCtrl／Alt修飾キー優先順位と一致させた。
- 改善: 既存頂点ドラッグ中のPenプレビュー位置を、Ctrlスナップ前のポインタ座標ではなく吸着後のキャンバス座標へ更新するようにした。実際の頂点位置と表示マーカーのずれを防ぐ。
- 安定化: 頂点ドラッグのプレビュー座標更新を、実際に適用したShift／Ctrl補正後のローカル座標から再計算するようにした。生ポインタやスナップ前座標が残る経路を避け、複数選択移動時も主頂点の表示位置と実データを一致させる。
- 拡張: 既存マスク頂点のCtrlドラッグでも、同一レイヤー内の別マスク／別頂点を最近傍スナップ候補に追加した。ドラッグ中の頂点自身と同時選択頂点は候補から除外し、選択群の移動が内部頂点へ引き寄せられないようにした。
- 安定化: 最近傍マスク頂点スナップで直接使用するQt型に`QLineF`のincludeをグローバルモジュールフラグメントへ明示追加した。間接includeへの依存を避け、C++20 modulesの依存境界を保つ。
- 改善: EraserのLast Stroke Only／全フレーム消去後に既存の`publishLayerModified`を呼ぶようにした。描画バッファ更新だけでなく、タイムライン・保存・他の購読側にも消去結果を伝播させる。
- 安定化: Last Stroke OnlyのEraser操作では`canUndo()`を確認してから復元・変更通知・再描画を行うようにした。Undo履歴が空のクリックで不要なModified通知を発行しない。
- 実装追加: Anchor Pointツールの既存中心マーカーに、現在のアンカーX/Yを表示する数値HUDを追加した。3Dは現在フレームのアニメーション値、2DはStaticTransform2Dのアンカー値を使い、Anchor表示が有効なときだけ描画する。
- 修正: 2DアンカーHUDの実装時に、2D変換には`anchorXAt()`が存在しないことを確認し、`anchorPointX()`／`anchorPointY()`へ切り替えた。3Dの時系列アンカー評価とは責務を分ける。
- 改善: Anchor HUDを3DレイヤーではX/Y/Z表示へ拡張した。3D時のみパネル幅を広げ、Zは現在フレームの`anchorZAt()`から取得する。2D表示の密度と既存レイアウトは維持する。
- 実装追加: 3DレイヤーでCtrl+ダブルクリックしたとき、AnchorをlocalBounds中央へ戻し、回転・スケールを考慮したPosition補正を同時に行うようにした。AnchorとPositionの前後値を`AnchorPointUndoCommand`へ保存し、Undo/RedoとLayerChangedEventを接続する。2Dレイヤーには適用しない。
- 安全化: 3D Anchor中心リセットのCtrl+ダブルクリック呼び出しをAnchor Pointツールがアクティブな場合に限定した。通常の選択／編集ツール上のCtrl+ダブルクリックで意図せずAnchorを変更しない。
- 実装追加: Anchor PointツールのCtrl+ダブルクリックによるlocalBounds中央リセットを2Dレイヤーにも拡張した。2D回転・スケール後も見た目の位置を維持するため、Anchor移動量を変換してPositionを補正し、専用Undoコマンドで復元できるようにした。2D変換APIの仕様に基づく実装であり、ビルド未検証。
- 実装追加: 3D投影フレームの内部領域をドラッグして、レイヤー面のScreen移動へ接続した。コーナー／エッジ／回転ハンドルを先に判定し、残った投影四辺形内部だけを移動対象にすることで、ハンドル操作との競合を避けている。投影頂点がnear/far範囲外の場合は誤った内部判定を避ける。ビルド未検証。
- 改善: 3D投影フレーム内部の移動中にShiftを押すと、開始時のレイヤー面ローカルX/Y軸のうち移動量が大きい軸へ拘束するようにした。投影後の画面方向ではなくワールド変換後のレイヤー面軸へ射影するため、カメラを斜めにした状態でも軸ロックの意味を維持する。ビルド未検証。
- 改善: 3D投影フレームのコーナーリサイズで、Shift時は初期アスペクト比を維持し、Shiftなしでも反対側コーナーを固定する位置補正を追加した。補正はワールド変換後のレイヤー面X/Y軸とlocalBounds寸法から算出し、Ctrl中心固定時は適用しない。ビルド未検証。
- 実装追加: 3D投影フレームの移動／リサイズ中に、現在の実効幅・高さ、位置X/Y、Z回転、操作種別を表示するHUDを追加した。HUDは既存のCanvas Overlay描画経路に限定し、3D描画行列やQt CSSを変更しない。ビルド未検証。
- 改善: 3Dフレーム操作HUDに位置ZとScale X/Yを追加し、平面移動と軸ギズモ操作の結果を同じ表示で確認できるようにした。表示幅・高さは3D値を含むため拡張した。ビルド未検証。
- 安定化: 3D投影フレーム描画前に、投影四隅をnear/farクリップ範囲へ照合する判定を追加した。全頂点がカメラ背面またはクリップ外の場合は枠・ハンドルを描画せず、部分的に範囲内の場合は既存のGPUクリッピングへ委ねる。`QVector4D`を直接includeし、ビルド未検証。
- 改善: 3D投影フレームの四隅の一部だけがnear/far範囲外にある場合、枠線・ハンドル・回転leaderを半透明化するようにした。全点範囲外時の非表示判定は維持し、部分クリップ時の存在確認を可能にする。ビルド未検証。
- 実装追加: Puppetツールのオーバーレイに、OpenCVPuppetEngineが保持する変形後メッシュの三角形ワイヤーフレームを追加した。インデックス範囲を検証してから辺を描画し、既存のピン色分け・選択表示を変更しない。ビルド未検証。
- 実装追加: Puppetで選択中のピンに回転leaderと回転ハンドルを表示し、Alt+ドラッグでピン回転を編集できる経路を追加した。回転値はPinRecordへ正規化して保存し、既存のdeformLayer／Undo外部経路を壊さない範囲で操作する。ビルド未検証。
- 改善: Puppetツールで選択中のピンをダブルクリックすると回転値を0度へ戻す操作を追加した。Puppetツールがアクティブな場合だけ実行し、通常のレイヤー編集上のダブルクリックには介入しない。ビルド未検証。
- 改善: PuppetのAlt回転ドラッグ中にShiftを押すと15度刻みへスナップするようにした。Shiftなしの自由回転と既存の-180〜180度正規化は維持する。ビルド未検証。
- 拡張: Puppet新規ピン追加時の修飾キーをPosition（通常）、Starch（Ctrl）、Bend（Shift）、Overlap（Alt）へ割り当てた。複数修飾キー時はCtrlのStarchを優先し、既存ピンのAlt回転操作とは追加操作の分岐で分離する。ビルド未検証。
- 拡張: PuppetのStarchピン上で修飾キーなしのホイールを回すと、剛性weightを5%刻みで0〜100%調整できるようにした。表示ラベルにも現在のStarch値を併記し、Shift/Alt/Ctrlの既存ナビゲーション操作とは競合させない。ビルド未検証。
- 修正: PuppetのdeformLayerでOpenCVPuppetEngineへ渡す`PuppetPin.weight`が固定1.0だったため、PinRecordの調整済みweightを0〜1へクランプして渡すようにした。これでStarchホイール編集が実際の変形重みに反映される。ビルド未検証。
- 拡張: PuppetのOverlapピン上でも通常ホイールでdepthを±0.05調整できるようにし、範囲を-1〜1へ制限した。表示ラベルに現在の深度を併記し、既存のStarch weight編集と同じポインタ判定・ナビゲーション除外ルールを再利用する。ビルド未検証。
- 修正: PuppetのBend回転単位をArtifact側のEngine境界で度からラジアンへ変換した。UI／PinRecord／オーバーレイは度数表示を維持し、OpenCVPuppetEngineの`std::sin/cos`呼び出しへだけラジアンを渡す。ArtifactCore側は変更していない。ビルド未検証。
- 改善: MotionSketch中に`[`/`]`でSmoothingを0.1刻み調整できるようにし、既存のSketch HUDへ現在のSmoothingとSample Rateを追加表示した。既存のPenマスクOpacityショートカットはツール分岐で維持する。ビルド未検証。
- 拡張: MotionSketch中のShift+`[`/`]`をSample Rateの5fps刻み調整へ割り当てた。通常の`[`/`]`によるSmoothing調整との役割を分離し、HUD表示値を即時確認できるようにした。ビルド未検証。
- 拡張: MotionSketch中に`W`でShow Wireframeを切り替えられるようにし、Sketch HUDへWF:ON/OFFを追加表示した。設定は既存MotionSketchToolのshowWireframe状態を利用する。ビルド未検証。
- 実装追加: MotionSketchのShow Backgroundを追加し、スケッチ中の`B`で背景表示を切り替えられるようにした。OFF時は既存のCanvas Overlay上へ暗い半透明面を描画して軌跡／ワイヤーフレームを強調し、HUDへBG:ON/OFFを表示する。ビルド未検証。
- 改善: Puppetの通常ピンドラッグ中にShiftを押すと、開始位置から移動量の大きいXまたはY軸へ移動を拘束するようにした。Alt回転ドラッグは別経路を維持し、回転中に位置拘束を適用しない。ビルド未検証。
- 改善: PuppetのBendピンラベルに現在の回転角を度数表示するようにした。UI操作・正規化後のPinRecord値をそのまま表示し、Engine境界でのラジアン変換前後を混同しない表示責務にした。ビルド未検証。
- 修正: Puppetピンのダブルクリックリセットを種別別に整理した。Bendは回転0度、Starchはweight 1.0、Overlapはdepth 0.0へ戻し、Positionピンではイベントを消費せず通常のダブルクリック処理へ通す。ビルド未検証。
- 実装追加: Rectangle／Shape作成中の通常ホイールで角丸半径を2px刻み調整し、作成確定時にArtifactShapeLayerのcornerRadiusへ反映するようにした。半径は短辺の半分でクランプし、作成中HUDへ`R`値を表示する。既存のShift正方形・Alt中心作成・修飾キー付きナビゲーションは維持する。ビルド未検証。
- 改善: Rectangle／Shape作成中の角丸半径をVPプレビューにも反映した。Shape/EllipseShapeでは既存のGPUネイティブ`drawRoundedPanel`を使い、Maskモードや半径0では従来の矩形描画へフォールバックする。ビルド未検証。
- 拡張: Rectangle／Shapeツールで作成済みの選択Shape上でも通常ホイールからcornerRadiusを調整できるようにした。作成中のセッション調整を優先し、作成後は選択ShapeのProperty dirty・変更通知・再描画を通す。ビルド未検証。
- 改善: Rectangle／Shapeツールで選択ShapeをダブルクリックするとcornerRadiusを0へ戻すリセットを追加した。作成中や半径0のShapeでは処理せず、他ツールのダブルクリックへ影響させない。ビルド未検証。
- 改善: 作成済みShapeのcornerRadiusホイール調整とダブルクリックリセットを`ShapeCornerRadiusUndoCommand`へ接続した。前後値を保存し、Undo/Redo時もProperty dirty・LayerChangedEvent・UndoManager通知を再実行する。ビルド未検証。
- 改善: Brush／Eraserツール中の`[`/`]`でブラシ径を2px刻み調整できるようにした。PenのマスクOpacity、MotionSketchのSmoothing／Sample Rateとはツール分岐で競合しない。ビルド未検証。
- 拡張: Brush／Eraserのブラケット操作にShift=Flow±5%、Ctrl=Opacity±5%を追加した。修飾キーなしの径±2pxは維持し、各値はBrushTool既存setterの範囲クランプを利用する。ビルド未検証。
- 改善: Brush／Eraserカーソル横へDiameter、Flow、Opacityの小型HUDを追加した。筆圧によるカーソル径・透明度プレビューとは独立した設定値表示で、描画中のストロークプレビュー処理を変更しない。ビルド未検証。
- 改善: Brush／EraserカーソルHUDへHardness、Roundness、Angleも追加表示した。既存BrushToolの値を読み取るだけに留め、描画パラメータや新規イベント配線は変更していない。ビルド未検証。
- 拡張: Brush／Eraserのブラケット操作にAlt=Hardness±5%、Ctrl+Alt=Roundness±5%、Shift+Alt=Angle±15°を追加した。既存の径・Flow・Opacity操作を維持し、BrushToolのsetterによる範囲正規化を利用する。ビルド未検証。
- 拡張: Brush／EraserのCtrl+Shift+Alt+ブラケットでSpacingを5%刻み調整できるようにし、カーソルHUDにもSpacingを表示した。ブラシ点の間隔だけを変更し、既存ストロークの再計算は行わない。ビルド未検証。
- 改善: Eraser時のカーソルHUDへPaint／Layer／Lastの現在モードを追加表示した。モード切替の既存設定を読み取るだけで、クリック時の消去処理やUndo経路は変更していない。ビルド未検証。
- 拡張: Brushの筆圧連動をFlowにも適用した。開始・逐次適用・終了の各BrushStrokeで`flow × pressure`を使い、既存のSize／Opacityと同じクランプ前提の経路へ揃えた。連動は`pressureAffectsFlow`で無効化可能で、既定は有効。ビルド未検証。
- UI追加: Brush Optionsへ`Pressure Flow`チェックボックスを追加し、筆圧によるFlow変調をユーザーが切り替えられるようにした。既存のoptionChanged経路からBrushToolへ反映し、新規シグナルは追加していない。ビルド未検証。
- UI追加: Brush Optionsへ`Pressure Size`／`Pressure Opacity`切替を追加した。既存BrushToolの筆圧フラグへ接続し、Flowを含む3つの筆圧連動を個別に無効化できるようにした。ビルド未検証。
- UI追加: Brush Optionsへ`Tilt Angle`／`Tilt Roundness`切替を追加した。BrushStroke生成時のペン傾きによる先端変形を個別に無効化でき、Angle／Roundnessの手動設定は維持される。ビルド未検証。
- 表示改善: Brush／Eraserカーソル輪郭にも現在のTilt Angle／Roundnessを反映した。描画時のBrushStroke生成と同じ傾き補正をプレビューへ適用し、設定値だけでなく実効先端形状を確認できるようにした。ビルド未検証。
- 表示改善: Brush／Eraserのカーソル輪郭と描画中ストロークプレビューの透明度へ実効Flowを反映した。Pressure Flow有効時はFlow×Pressure、無効時は基準Flowを使い、プレビューだけが実描画より濃く見える不一致を抑えた。ビルド未検証。
- 表示改善: Brush／Eraser HUDを2行化し、基準設定に加えてPressure、実効Flow、Tilt X/Yを表示した。筆圧・傾きによる実効値を描画前に確認できるが、HUDは読み取り専用のまま維持する。ビルド未検証。
- 拡張: BrushStrokeへSize Jitter／Opacity Jitterを追加し、PaintLayerの各ダブへ座標と点番号から決定的な変動を適用した。Undo／逐次適用で見た目が変わらないよう乱数エンジンは保持せず、Brush Optionsから0〜100%を設定できる。ビルド未検証。
- 拡張: BrushStrokeへScatterを追加し、各ダブを半径内の決定的な方向・距離へ散らせるようにした。Brush Optionsから0〜100%を設定でき、Size／Opacity Jitterと同じく再適用時の再現性を保つ。ビルド未検証。
- 表示改善: Brush／Eraser HUDの動的値行へSize Jitter／Opacity Jitter／Scatterを追加表示した。Pressure・実効Flow・Tiltの表示は維持し、値の編集経路はBrush Optionsに限定する。ビルド未検証。
- 拡張: BrushStrokeへAngle Jitter／Roundness Jitterを追加し、各ダブへ決定的な角度±180度・真円度変動を適用した。Brush Optionsから個別設定でき、既存のTilt補正後の値を基準にする。ビルド未検証。
- 表示改善: Brush／Eraser HUDのダイナミクス行へAngle／Roundness Jitterも追加し、Size／Opacity／Angle／Roundness JitterとScatterを一目で確認できるようにした。ビルド未検証。
- 拡張: BrushStrokeへFlow Jitterを追加し、流量係数へ独立した決定的変動を適用した。Brush OptionsとHUDから設定値を確認でき、Opacity Jitterとは別に扱う。ビルド未検証。
- 表示改善: Brush／EraserカーソルへScatter最大範囲の薄いリングを追加した。先端輪郭と散布範囲を分離して表示し、描画中のストロークプレビューやPaintLayerの計算は変更していない。ビルド未検証。
- 拡張: Brush Optionsで変更した共有BrushTool設定をQSettingsへ保存し、MainWindow起動時に復元するようにした。CoreのAppSettings契約は変更せず、既存のoptionChanged経路を保存フックとして再利用する。ビルド未検証。
- 改善: MainWindow起動時に復元したBrushToolの全設定をBrush OptionsへSignalBlocker付きで同期する`syncBrushOptionsFromTool()`を追加した。永続化値・描画値・UI表示値の不一致を防ぎ、同期時のoptionChanged再発火も抑制する。ビルド未検証。
- 改善: Brush ColorのQSettings復元とカラーボタンのパレット同期を追加した。保存されたRGBA文字列を範囲クランプしてBrushToolへ戻し、明度に応じたボタン文字色も再計算する。ビルド未検証。
- 改善: QSettings復元へEraserのmode（Paint／Layer／Last）とLast Stroke Onlyも追加した。optionChangedの共有保存キーを優先し、旧形式の`eraser/*`キーもフォールバックとして読めるようにした。ビルド未検証。
- 改善: `syncBrushOptionsFromTool()`でEraser Optionsのサイズ、強さ、Hardness、Angle、Roundness、Last Stroke Only、モード選択も同期するようにした。起動直後のBrush／Eraser UIが同じ共有BrushTool状態を表示する。ビルド未検証。
- 改善: Clone Stampのradius／Aligned／Time OffsetもQSettingsから復元するようにした。radiusはBrush Size保存値を優先して共有状態の上書きを避け、既存のBrushTool契約だけを利用する。ビルド未検証。
- 改善: BrushTool復元同期へClone OptionsのRadius／Aligned／Time Offset表示も追加した。`QSignalBlocker`で同期時のoptionChanged再発火を抑え、Clone UIと共有ツール状態を一致させる。ビルド未検証。
- UI追加: Eraser OptionsへHardness設定を追加し、BrushToolの共通hardnessへ反映するようにした。Eraser Strength（opacity）とは別のパラメータとして扱い、既存の消去モード・Undo処理は変更していない。ビルド未検証。
- UI追加: Eraser Optionsへ共通ブラシ先端のAngle／Roundnessを追加した。消しゴム専用の形状状態を増やさず、BrushToolの既存setterへ接続してBrushと同じ先端形状を共有する。ビルド未検証。
- 拡張: TrackPointツールのブラケット操作でFeature Size（通常）とSearch Size（Shift）を2px刻み調整できるようにした。Inner領域がOuter領域を越えないよう上下限を保ち、既存Tracker Gizmoの状態・描画を更新する。ビルド未検証。
- 表示改善: TrackPoint GizmoにFeature／Searchの現在幅・高さを表示するHUDを追加した。サイズ変更の結果をVP上で即時確認でき、NCC／MotionTrackerの計算経路は変更していない。ビルド未検証。
- 表示改善: 3D投影フレームへ薄い対角線リファレンスマークを追加した。投影後のコーナー位置とアスペクト変化を読み取りやすくする補助表示で、フレームのヒットテストやリサイズ計算は変更していない。ビルド未検証。
- 改善: MotionSketchのSmoothing／Sample Rate／WireframeをQSettingsへ保存・復元し、起動時にOptions UIへ同期する処理を追加した。設定値は既存のMotionSketchTool APIだけを通し、ビルド未検証。
- 改善: MotionSketchのBキーによる背景表示切替も`motionSketch/showBackground`へ保存し、起動時に復元するようにした。UI項目がない表示補助状態も既存のツール状態と同じ永続化境界に揃えた。ビルド未検証。
- 改善: MotionSketchのキーボード操作（W、[／]）で変更したWireframe／Smoothing／Sample Rateも即時保存するようにした。Options UI経由と直接操作経由で設定の永続化が分岐しない。ビルド未検証。
- 改善: TrackPointのFeature/Search領域サイズをQSettingsへ保存し、起動時にTracker Gizmoへ復元するようにした。ブラケット操作だけでなく領域ハンドルのドラッグも同じ設定へ反映する。ビルド未検証。
- UI改善: Viewメニューの「定規を表示」をCompositionRenderControllerへ接続し、表示状態をQSettingsへ保存・復元するようにした。既存のViewport Ruler／Scale Overlay描画経路は変更していない。ビルド未検証。
- UI追加: Render Queue Job DetailsへSelective Render設定（Adjustment Layer除外／Split Passes）を追加し、既存の`jobSelectiveSettingsAt`／`setJobSelectiveSettingsAt` APIへ接続した。選択ジョブ変更時のUI同期も実装した。ビルド未検証。
- 表示改善: Render Queueのプレビュー要約へSelective RenderのAdjustment除外／Split Passes状態を追加した。ジョブ詳細を開かなくても出力範囲の追加条件を判別できる。ビルド未検証。
- UI追加: Selective Renderへ`Exclude Guide Layers`も追加し、RenderQueueServiceが既に保持するガイドレイヤー除外設定を編集・復元できるようにした。ビルド未検証。
- UI追加: Selective Renderへ解像度プリセット（Custom／Composition／Half／Third／Quarter）を追加し、ジョブ単位の`resolutionPreset`を既存サービスAPIへ保存・復元するようにした。ビルド未検証。
- UI追加: Selective RenderへFrame Range（Composition／Work Area／Custom／Selected Frames／Single Frame）とRegion（Full／ROI／Custom Crop）のモード選択を追加し、ジョブ単位で保存・復元するようにした。ビルド未検証。
- UI追加: Render QueueのROI／Custom Crop用にCrop X/Y/Width/Height編集欄を追加し、`cropX/cropY/cropW/cropH`をジョブ単位で保存・復元するようにした。ビルド未検証。
- UI追加: Selective Renderへレイヤーフィルタ（All／Selected／Solo／Visible／Custom Layers）の選択を追加し、`layerFilterMode`をジョブ単位で保存・復元するようにした。ビルド未検証。
- 改善: Render QueueでSplit Passesを初めて有効化した際、renderPassesが空ならBeautyパスを自動生成するようにした。既存の明示パスは保持し、空定義によるpreflightエラーを避ける。ビルド未検証。
- 表示改善: Split Passes有効時、プレビュー要約へ有効なRenderPassConfig名を`Passes:`として表示するようにした。Beauty自動生成後の実際の出力分割内容をジョブ詳細外から確認できる。ビルド未検証。
- UI改善: RegionがFullのときCrop X/Y/Width/Heightを無効化し、ROIまたはCustom Crop選択時だけ編集可能にした。入力値自体は保持し、モード切替で破棄しない。ビルド未検証。
- UI追加: Split Passesの`Configure Passes…`を追加し、カンマ区切りの名前入力から有効なRenderPassConfig一覧を保存できるようにした。重複名は除外し、空入力では既存設定を保持する。ビルド未検証。
- UI追加: Selective RenderのCustom Layersへ現在のレイヤー選択を取り込む`Use Current Selection`ボタンを追加した。選択レイヤーIDを`layerWhitelist`へ保存し、フィルタモードをCustomへ切り替える。空選択では既存設定を保持する。ビルド未検証。
- UI追加: 現在のレイヤー選択をSelective Renderの`layerBlacklist`へ追加する`Exclude Current Selection`を追加した。既存のWhitelistやレイヤーフィルタモードと併用できる。ビルド未検証。
- 表示改善: Render Queueの要約へlayerWhitelist／layerBlacklistの件数を表示し、選択・除外レイヤーの適用状態を確認できるようにした。IDそのものは表示せず、要約の横幅を抑えている。ビルド未検証。
- UI追加: Selective Renderへ`Clear Included`／`Clear Excluded`を追加し、layerWhitelist／layerBlacklistを個別に解除できるようにした。フィルタモードは維持し、対象リストだけを空にする。ビルド未検証。
- UI改善: Whitelist／Blacklistが空のジョブでは対応するクリアボタンを無効化し、ジョブ切替時の設定件数と操作可能状態を一致させた。ビルド未検証。
- 改善: Current SelectionのWhitelist／Blacklist取り込み時に反対側のリストから同じLayer IDを除去するようにした。相互排他的な指定を保ち、同一レイヤーが同時にinclude／excludeされる矛盾を防ぐ。ビルド未検証。
- UI追加: ViewメニューのGrid Settingsへズーム連動ステップ／極座標グリッド／アイソメトリックグリッドの切替を追加した。既存CompositionRenderController APIへ接続し、状態をQSettingsへ保存・復元する。ビルド未検証。
- UI改善: 極座標／アイソメトリックの相互排他をメニュー表示にも反映し、一方を有効化した際にもう一方のチェックを解除するようにした。Controller側の排他契約とUI状態を一致させる。ビルド未検証。
- UI追加: Grid Settingsへ「グリッド数値を表示」を追加し、既存GridSettings::showNumbersとメジャー線ラベル描画をViewメニューから切り替えられるようにした。ビルド未検証。
- UI追加: Grid Settingsへズーム連動ステップの目標ビューポート間隔編集を追加した。24〜512pxに制限し、Controllerへ適用した値をQSettingsへ保存・復元する。ビルド未検証。
- 修正: Viewメニューの「グリッドを表示」「グリッドにスナップ」はActionだけ存在してController接続がなかったため、実際の表示／snapToGrid状態へ接続した。両方をQSettingsへ保存・復元し、メニューのチェック状態も同期する。ビルド未検証。
- 修正: Viewメニューの「ガイドを表示」もControllerへ接続し、showGuides状態をQSettingsへ保存・復元するようにした。既存のガイド生成・描画処理は変更していない。ビルド未検証。
- 実装: 「ガイドにスナップ」をControllerへ追加し、Pen頂点ドラッグ時の既存ガイドスナップ経路へ接続した。Ctrl押下に加えてメニュー状態が有効条件となり、状態はQSettingsへ保存・復元する。ビルド未検証。
- 表示改善: 極座標グリッドで`showNumbers`が有効な場合、同心円の半径ラベル（r値）を表示するようにした。ズームが低すぎる場合はラベルを抑制し、既存の線描画・スナップ計算は変更していない。ビルド未検証。
- 実装: Pen頂点ドラッグ時に未使用だった`snapCanvasToGrid()`を接続し、Grid SettingsのsnapToGridを実際の頂点位置へ適用するようにした。ガイドスナップとは別に適用され、両方の設定を既存のスナップ経路で統合する。ビルド未検証。
- 表示改善: アイソメトリックグリッドでも`showNumbers`有効時に現在の格子間隔を表示する補助ラベルを追加した。極座標の半径ラベルと同じズーム抑制条件を使い、線生成は変更していない。ビルド未検証。
## 2026-08-02 — Brush/Eraser の対象レイヤー自動補完

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Brush/Eraser マウス開始処理。
- 確認できた事実: Paint レイヤー未選択時の自動作成は既に存在したが、画像・シェイプ等の非 Paint レイヤーを選択した状態ではブラシ処理へ進めなかった。
- 実装: 選択レイヤーが存在しても `ArtifactPaintLayer` でない場合は、新しい Paint レイヤーを作成して選択対象に切り替える。作成時には短い情報オーバーレイを表示する。
- 価値: ツール選択後の最初のクリックが無反応になる状態をなくし、Brush/Eraser の「対象 Paint レイヤーがない場合の自動作成」という仕様を実際の編集導線に揃えられる。
- 次に確認すべきこと: 実ランタイムで既存レイヤー選択中の Brush/Eraser 開始、Undo、レイヤー順序を確認する（未検証）。
## 2026-08-02 — Text ツールのダブルクリック編集入口

- 関連: `ArtifactCompositionRenderWidget` / `CompositionRenderController` / `ArtifactTextLayer`。
- 実装: ビューポートのダブルクリック時、選択中レイヤーが Text なら既存の Qt 入力ダイアログで本文を編集し、変更後にレイヤー更新・GPU/オーバーレイ無効化・再描画を行う。
- 方針: 新規シグナル／スロット配線や別のテキスト編集モデルは導入せず、既存の選択状態と `setText()` を利用した最小の編集導線とした。
- 未検証: 実ランタイムのダブルクリック判定、Undo 履歴、マルチライン入力の表示品質。
## 2026-08-02 — Text 内容変更の Undo 対応

- 関連: `ArtifactCompositionRenderController.cppm` の `TextContentUndoCommand`。
- 実装: ダブルクリック編集で本文を変更した場合、変更前後の文字列を既存 `UndoManager` に登録するようにした。Undo/Redo は `ArtifactTextLayer::setText()`、`changed()`、LayerChangedEvent 通知を通る。
- 価値: Text ツールの直接編集が、単なる即時反映ではなく既存編集履歴へ統合された。
- 未検証: 実ランタイムでの Undo/Redo と再描画、空文字列入力、レイヤー削除後の履歴実行。
## 2026-08-02 — Text ダブルクリック対象のレイヤーヒット解決

- 関連: `ArtifactCompositionRenderController::editTextAtViewport()`。
- 実装: 選択中レイヤーが Text でない場合でも、ダブルクリック位置を viewport→canvas→layer local へ変換し、可視・アクティブな Text レイヤーを上位から検索して編集対象にする。
- 価値: 先にレイヤーを選択していなくても、キャンバス上のテキストを直接編集できる導線になった。
- 未検証: 変形・回転した Text、重なった Text、ロックレイヤーのヒット順。
## 2026-08-02 — Text 編集ヒットテストの安全化

- 関連: `CompositionRenderController::editTextAtViewport()`。
- 実装: 選択中 Text でもダブルクリック位置がレイヤー境界外なら編集を開始しないようにし、ロック／選択ロックされた Text は位置検索から除外した。
- 価値: 意図しない編集ダイアログ表示とロックレイヤーの編集を防ぐ。
- 未検証: 複雑な非矩形テキスト形状、親子レイヤー変換、実ランタイムのロック状態。
## 2026-08-02 — 3D Frame Gizmo のダブルクリックリセット

- 関連: `CompositionRenderController::resetSelected3DTransform()`。
- 実装: 選択中の 3D レイヤーをビューポートでダブルクリックすると、位置 X/Y・回転・XY スケールを初期値へ戻す。Z 位置は保持する。
- Undo: 既存 `GizmoTransformUndoCommand` を利用し、変更前後の TransformSnapshot を履歴へ登録する。
- 安全性: ロック／選択ロック中のレイヤーは対象外。
- 未検証: 実ランタイムでの 3D カメラ投影、キーフレーム時の値、Undo/Redo 表示。
## 2026-08-02 — 3D 投影フレームの最小サイズクランプ

- 関連: `CompositionRenderController::handleMouseMove()` の projected frame resize。
- 実装: コーナー／エッジリサイズ後の XY スケールを、ローカル境界の 1 px 未満にならないようにクランプする。Shift の均等スケールや Ctrl の中心固定計算後に適用する。
- 価値: 極端なドラッグでフレームがゼロ化し、ハンドルや選択枠が消える状態を防ぐ。
- 未検証: 負スケール反転を意図的に使うケース、非均一な 3D 親変換、実ランタイムの境界表示。
## 2026-08-02 — Text ダブルクリック時の選択同期

- 関連: `CompositionRenderController::editTextAtViewport()`。
- 実装: 選択状態とは別の Text レイヤーを位置ヒットで見つけた場合、編集ダイアログを開く前に `setSelectedLayerId()` で選択・プレビュー・ギズモ状態を同期する。
- 価値: 編集後もプロパティ／ギズモ／レイヤー選択がクリック対象と一致する。
- 未検証: 複数選択状態と親子レイヤーの選択表示。
## 2026-08-02 — Puppet ピンドラッグの Undo 統合

- 関連: `CompositionRenderController` / `ArtifactPuppetTool`。
- 実装: ピンドラッグ開始時に位置・回転を保存し、マウスリリース時に変更があれば `PuppetPinUndoCommand` を UndoManager へ登録する。Undo/Redo ではピン状態を復元し、メッシュ変形も再適用する。
- 価値: Position/Bend/Overlap 等のピン操作を、既存の編集履歴と一貫して取り消せる。
- 未検証: PuppetTool のライフタイム、ピン削除後の履歴、実ランタイムでの画像再変形。
## 2026-08-02 — Puppet Starch/Overlap パラメータの Undo 対応

- 関連: `adjustSelectedPuppetPinWeightAt()` / `adjustSelectedPuppetPinDepthAt()`。
- 実装: ホイール操作で変更する Starch Weight と Overlap Depth の変更前後を `PuppetPinScalarUndoCommand` に登録した。
- 価値: ピン移動・回転に加え、ピン属性の微調整も編集履歴から個別に戻せる。
- 未検証: 連続ホイール操作の履歴粒度、ピン削除後の履歴実行、実ランタイムの再描画。
## 2026-08-02 — TrackPoint 解析範囲と結果 HUD の改善

- 関連: `trackerTrackForward()` / `trackerTrackBackward()` / `trackerTrackAll()`。
- 実装: Forward/Backward の固定 30 フレーム処理を廃止し、現在フレームからコンポジションの `FrameRange` 終端／始端まで解析するようにした。解析後は対象範囲と平均 Confidence を情報 HUD に表示する。
- 価値: 長いコンポジションでも追跡範囲が実際のタイムラインに一致し、結果品質をその場で確認できる。
- 未検証: 非ゼロ開始フレーム、巨大なフレーム範囲、解析中の UI 応答性。
## 2026-08-02 — TrackPoint 解析開始 HUD

- 関連: `trackerTrackForward()` / `trackerTrackBackward()` / `trackerTrackAll()`。
- 実装: 同期的なフレーム走査を開始する直前に解析範囲を情報オーバーレイへ表示し、完了時の Confidence HUD へ自然につなげた。
- 価値: 長い解析範囲でも、操作が受け付けられたことと対象範囲をユーザーが確認できる。
- 未検証: UI スレッド上の同期解析中にオーバーレイが描画されるタイミング。
## 2026-08-02 — Ellipse ツール名称の明確化

- 関連: `Artifact/src/Widgets/ArtifactToolBar.cppm`。
- 実装: `ToolType::Ellipse` のツールバー表示名を曖昧な「シェイプ」から「楕円」へ変更した。
- 価値: Shape と Ellipse の責務がツールバー上でも区別でき、仕様上のツール名と UI 表示を一致させる。
- 未検証: ローカライズ資産やアクセシビリティ読み上げ文言の追加箇所。
## 2026-08-02 — Track All の非ゼロ開始フレーム修正

- 関連: `trackerTrackAll()`。
- 確認・修正: 完了 HUD が参照する `range` と走査範囲が一致しておらず、`duration()` を 0 始点として使っていた。`FrameRange::start()/end()/frameCount()` を使い、実際の開始フレームから終了フレームまで走査するよう修正した。
- 価値: 非ゼロ開始のコンポジションでも Track All の画像フレームと Confidence 表示が一致する。
- 未検証: 非ゼロ開始フレームの実ランタイム解析。
## 2026-08-02 — MotionSketch 終了時の再生状態復元

- 関連: `CompositionRenderController::handleMouseRelease()`。
- 修正: 自動再生を開始した MotionSketch がサンプル不足で終了しても、元々停止中だった場合は Playback を確実に pause し、保存していた `motionSketchWasPlaying_` をクリアする。
- 価値: クリックだけで終了したケースや短いスケッチ後に、意図せず再生が継続する状態を防ぐ。
- 未検証: 実ランタイムの再生中開始／停止中開始の両ケース。
## 2026-08-02 — MotionSketch の早期終了バグ修正

- 関連: `CompositionRenderController::handleMouseMove()` / `handleMouseRelease()`。
- 修正: `handleMouseMove()` に重複していた `finishSketch()` 処理を削除し、MotionSketch の終了を `handleMouseRelease()` のみに統一した。
- 影響: 最初のマウス移動でスケッチが確定してしまう状態を解消し、ドラッグ中の全サンプルを収集できるようにした。自動再生の停止処理もリリース時に維持される。
- 未検証: 実ランタイムでの長いドラッグ、キャンセル、再生中開始。
## 2026-08-02 — MotionSketch 終端サンプル補完

- 関連: MotionSketch の press/move/release 経路。
- 実装: 最後に通過したキャンバス座標を保持し、リリース時に Sample Rate のスロットルを尊重しながら最終サンプルを追加試行する。終了・キャンセル時は座標をクリアする。
- 価値: ドラッグ終端の位置がキーフレームへ反映されず、動きが途中で止まって見えるケースを減らす。
- 未検証: リリース直後の短いドラッグ、低い Sample Rate、キャンセル操作。
## 2026-08-02 — TrackPoint Apply 結果 HUD

- 関連: `trackerApplyToPosition()` / `trackerApplyToAnchor()`。
- 実装: Position Apply 後に選択レイヤー適用か新規 Null 作成かを表示し、Anchor Apply 後には選択レイヤーへの書き込み状態を表示する。
- 価値: 解析結果の適用操作が無反応に見える状態を減らし、次に必要な選択操作を明確にする。
- 未検証: 実ランタイムの Null 作成、既存レイヤーへのキーフレーム反映。
## 2026-08-02 — TrackPoint FrameRange 入力検証

- 関連: `trackerTrackForward()` / `trackerTrackBackward()` / `trackerTrackAll()`。
- 実装: 解析開始前に `FrameRange::isValid()` を確認し、不正な範囲で clamp／フレーム走査へ進まないようにした。start=end の単一フレーム範囲は有効な入力として保持する。
- 価値: 空のプロジェクトや未確定範囲で不正な解析範囲・無限ループを避ける。
- 未検証: 実ランタイムでの空範囲生成条件。
## 2026-08-02 — TrackPoint Null 適用後の選択同期

- 関連: `trackerApplyToPosition()` / `ArtifactPointTrackerTool::applyTrackingResult()`。
- 実装: 新規 Null レイヤーへ追跡結果を書き出した場合、作成された最上位レイヤーを選択状態へ同期する。Apply の戻り値も確認し、キーフレームが無い場合は失敗 HUD を表示する。
- 価値: Apply 後に結果レイヤーを探し直す必要がなく、空結果を成功と誤認しない。
- 未検証: 複数同時適用、レイヤー追加失敗、実ランタイムの選択同期。
## 2026-08-02 — TrackPoint 複数ポイント一括 Apply

- 関連: `ArtifactPointTrackerTool::applyAllTrackingPoints()` / TrackPoint コンテキストメニュー。
- 実装: Core 側に存在していた全ポイント適用 API を Controller へ公開し、各トラッキングポイントを個別 Null レイヤーへ一括書き出しするメニュー項目を追加した。
- 価値: 複数点トラッキング結果を 1 点ずつ適用する手作業をなくし、複数点ワークフローを UI から完結できる。
- 未検証: 複数点結果の実ランタイム適用、レイヤー順序、Undo 粒度。
## 2026-08-02 — TrackPoint 複数 ID の重複除去

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm` の `applyAllTrackingPoints()`。
- 修正: フレームごとの並び順に依存した直前 ID 比較を廃止し、`std::set` で全結果中の有効ポイント ID を一意化した。
- 価値: 複数フレームに同じポイントが現れる通常のトラッキング結果でも、ポイントごとに Null レイヤーを 1 つだけ作成できる。
- 未検証: 実ランタイムでの複数点結果、ポイント ID の順序が変わるトラッカー。
## 2026-08-02 — Ellipse ツールの直接選択導線

- 関連: `Artifact/src/Widgets/ArtifactToolBar.cppm`。
- 実装: メインツールバーに Shape とは独立した「楕円」アクションを追加し、`Shift+Q` で `ToolType::Ellipse` を直接選択できるようにした。既存 Shape アイコンをフォールバックとして利用する。
- 価値: Ellipse の実装済みキャンバス作成経路へ、通常の UI から到達できる。
- 未検証: Qt のショートカット競合、ツールバーのコンパクト表示、実ランタイムの Ellipse 作成。
## 2026-08-02 — MotionSketch 開始フレーム固定

- 関連: `ArtifactMotionSketchTool::beginSketch()` / `finishSketch()`。
- 修正: 自動再生中に終了時の現在フレームを基準にしていた処理を改め、スケッチ開始時のフレームを保存してキーフレーム生成へ使用するようにした。大きなフレーム番号には `int64_t` を使う。
- 価値: 再生しながら描いても、スケッチの最初の位置から正しい時間範囲へ記録される。
- 未検証: 再生中の長時間スケッチ、非ゼロ開始フレーム、Undo/Redo。
## 2026-08-02 — Ellipse 選択時の Tool Options 同期

- 関連: `ArtifactToolOptionsBar::setCurrentTool()`。
- 修正: ツールバーの表示名を「楕円」に分離したことに合わせ、楕円選択時も Shape オプションフレームを表示するよう判定を拡張した。
- 価値: Ellipse アクション追加後も、Shape のサイズ・塗り・ストローク等のオプション編集を失わない。
- 未検証: 実ランタイムでのツール切り替えとオプションバー表示。
## 2026-08-02 — Ellipse 作成 HUD の明確化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の矩形／楕円プレビュー HUD。
- 修正: Ellipse モードではサイズ表示を `Ellipse 幅 x 高さ` に切り替え、矩形／角丸矩形だけに意味のある角丸半径 `R` 表示を出さないようにした。
- 価値: 楕円作成中に、実際に編集できる形状と HUD の情報が一致する。
- 未検証: 実ランタイムでの EllipseMask / EllipseShape 両モードの表示、ローカライズ済みフォント幅。
## 2026-08-02 — Viewport 定規のパン／ズーム追従

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`。
- 修正: 定規の目盛り計算へ renderer の pan と zoom から求めた可視キャンバス原点・範囲を渡し、定規本体とスケールバーは変形後のキャンバス端ではなくビューポート端へ固定した。
- 価値: キャンバスをパンして原点が画面外へ移動しても、表示中の範囲に対応する目盛りとスケールバーを確認できる。
- 未検証: 回転ビュー、DPI スケーリング、実ランタイムでの大きなパン値。
## 2026-08-02 — Viewport 定規の SubMinor 目盛り

- 関連: `Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`、定規 HUD 描画。
- 修正: 5 分割以上のステップでは `Major` / `Minor` / `SubMinor` を生成し、描画長を 10 / 6 / 3 px に分けた。従来は補助目盛りがすべて同じ長さだった。
- 価値: ズームに応じた 1-2-5 系列の細かい間隔を視覚的に読み取りやすくする。
- 未検証: 低ズームでの目盛り密度、回転ビュー、実ランタイムのアンチエイリアス。
## 2026-08-02 — Viewport 定規ラベルを実座標化

- 関連: `Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: メジャー目盛りのラベルを固定ステップ名から各目盛りのキャンバス座標へ変更した（例: `0 px`, `80 px`, `160 px`）。小数ステップでは精度に応じて小数桁も調整する。
- 価値: パン・ズーム中でも、目盛り位置の実際の座標を読み取れる。
- 未検証: 単位名が空文字列の場合、長いラベルの重なり、実ランタイム表示。
## 2026-08-02 — Viewport 定規の目盛りキャッシュ

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 修正: pan、zoom、ビューポートサイズ、キャンバスサイズをキャッシュキーとして保持し、いずれかが変化した場合だけ水平／垂直目盛りを再生成するようにした。
- 価値: 内容が静止した再描画で目盛り計算とベクター生成を繰り返さず、オーバーレイ描画の負荷を抑えられる。
- 未検証: 高頻度 pan／zoom 操作時のキャッシュ更新、キャンバスの動的リサイズ、実ランタイム性能。
## 2026-08-02 — Text 作成直後の編集開始

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Text ツール release 処理。
- 修正: Text レイヤーの作成成功を確認した後、既存の `editTextAtViewport()` を呼び出して即時入力へ遷移するようにした。編集内容は既存の `TextContentUndoCommand` 経路を再利用する。
- 価値: クリック作成・ドラッグ作成のどちらでも、作成後に別操作を挟まずテキストを入力できる。
- 未検証: モーダル編集ダイアログのキャンセル時の UX、IME 入力、段落テキストの長文入力。
## 2026-08-02 — Paint ストロークの Undo ショートカット接続

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`。
- 修正: Brush／Eraser がアクティブなとき、`Ctrl+Z` 相当の Undo が選択中 `ArtifactPaintLayer` の `undoLastStroke()` を優先して実行する公開経路を追加した。描画キャッシュ・変更通知・情報 HUD も更新する。
- 価値: Paint レイヤーの内部ストローク履歴を通常の編集操作から戻せる。
- 未検証: グローバル Undo 履歴との混在、逐次適用された長いストローク、Redo との組み合わせ。
## 2026-08-02 — Paint Undo のキー処理順序を固定

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` の `keyPressEvent()`。
- 修正: `InputOperator` がキーを先に消費する前に、Brush／Eraser の Undo を判定するようにした。Paint ストロークを戻せた場合はイベントをそこで完了させる。
- 価値: 入力コンテキスト設定に依存せず、Viewport 上の Paint Undo が確実に専用履歴へ到達する。
- 未検証: ショートカット設定を変更した環境、IME 中の Ctrl+Z、Redo との組み合わせ。
## 2026-08-02 — Pen のマスクキーボード操作を接続

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、マスク操作 Controller API。
- 修正: Pen アクティブ時に `Esc` で作成中マスクをキャンセル、`Delete / Backspace` で選択頂点またはホバー頂点を削除、`Ctrl+A` で編集可能な頂点を全選択できるようにした。入力コンテキストより前に処理する。
- 価値: 仕様にあるマスク編集の基本ショートカットが、既存の Undo 対応 Controller 操作へ到達する。
- 未検証: IME／アクセシビリティキー設定、複数パス削除後の選択状態、実ランタイムのキーフォーカス。
## 2026-08-02 — Pen のマスク複製ショートカット

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`CompositionRenderController::duplicateHoveredMask()`。
- 修正: Pen アクティブ時の `Ctrl+D` を、ホバー中マスクの既存複製・Undo 経路へ接続した。
- 価値: マスク仕様の複製操作を、コンテキスト UI に依存せず Viewport から実行できる。
- 未検証: ホバーなしでの選択マスク複製、複数マスクの連続複製、実ランタイムのショートカット競合。
## 2026-08-02 — マスクのコピー／並び順操作を接続

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、マスク Controller の clipboard／reorder API。
- 修正: Pen アクティブ時の `Ctrl+C / Ctrl+V` をマスクのコピー／ペーストへ、上下矢印をホバー中マスクの合成順移動へ接続した。
- 価値: マスクを同一レイヤーまたは別レイヤーへ再利用し、合成順をキーボードから調整できる。
- 未検証: OS クリップボードとの共存、ホバーなしの選択マスク操作、上下矢印とレイヤーナッジの競合。
## 2026-08-02 — Composition Editor 側にもマスクキー操作を接続

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- 修正: Viewport Widget だけでなく Composition Editor のキーイベント経路にも、マスクの `Ctrl+C / Ctrl+V` と上下矢印の並び順変更を追加した。フォーカスが親 Editor 側にある場合も同じ Controller API を使う。
- 価値: フォーカス位置によるショートカット取りこぼしを減らし、マスク編集操作を一貫させる。
- 未検証: 親子 Widget のイベント伝播時に同一操作が二重適用されないこと、実ランタイムのフォーカス遷移。
## 2026-08-02 — 3D フレームハンドルのカメラクリッピング

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` の `drawSelectionFrameOverlay()`。
- 修正: 3D カメラ行列が有効な場合、コーナー・エッジ・回転ハンドルごとに clip-space の Z と W を検査し、背面／near-far clip 外のハンドルを描画しないようにした。
- 価値: レイヤーがカメラ背面へ回り込んだとき、操作対象ではないハンドルが画面に残る誤認を防ぐ。
- 未検証: クリップ面をまたぐフレーム、極端な透視投影、実ランタイムのハンドル可視性。
## 2026-08-02 — 3D Text フレーム HUD のボックス寸法表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の projected frame HUD。
- 修正: 3D Text レイヤーをリサイズ中、既存の幅・高さ・位置・スケール情報に `maxWidth` と `boxHeight` に基づく `TextBox W x H` を追加表示する。
- 価値: 見た目の投影サイズと、段落テキストが実際に折り返すボックス寸法を区別して確認できる。
- 未検証: Point Text、アニメーション中のボックス値、HUD の高さが増えた場合の表示領域。
## 2026-08-02 — 3D フレーム HUD の行数追従

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の projected frame HUD。
- 修正: 表示文字列の改行数から HUD 高さを算出し、TextBox 情報を追加した場合も固定高さで切れないようにした。幅も長い数値列に合わせて拡張した。
- 価値: Text レイヤーのリサイズ中に、追加されたボックス寸法情報まで常に読める。
- 未検証: 極端に長い数値、DPI スケーリング、画面下端付近での HUD 配置。
## 2026-08-02 — 3D フレーム HUD の投影中心追従

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の projected frame HUD。
- 修正: カメラ行列が有効な場合、レイヤーのローカル中心を投影し、その近傍へ HUD を配置するようにした。画面端では viewport 内へクランプする。
- 価値: リサイズ対象と数値情報の距離が近くなり、複数の 3D レイヤー操作でも視線移動を減らせる。
- 未検証: 透視投影で中心が画面外にある場合、DPI スケーリング、実ランタイムの HUD 重なり。
## 2026-08-02 — HUD 行数計算の MSVC 型整合

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 修正: `QString::count()` の `qsizetype` を HUD 行数計算前に `int` へ明示変換し、`std::max` の型推論に依存しないようにした。
- 価値: MSVC／C++20 modules 環境でのテンプレート推論エラーを避け、TextBox 追加後の動的 HUD 高さ計算を成立させる。
- 未検証: 極端な改行数による int 範囲超過（通常の HUD 文字列では発生しない）。
## 2026-08-02 — Render Queue SingleFrame の終端フレーム保護

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` のフレーム範囲解決。
- 修正: `SingleFrame` がコンポジション範囲の最終フレームを指す場合に、`std::clamp` の下限と上限が逆転しないよう、最小終端を `startF + 1` として正規化した。
- 価値: 終端フレームのスナップショットが無効範囲や不正なクランプ計算にならず、1 フレームのジョブとして維持される。
- 未検証: 空の FrameRange、負のフレーム番号、外部レンダラーの終端フレーム契約。
## 2026-08-02 — Render Queue SelectedFrames の初期範囲

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の選択的レンダー設定。
- 修正: `Selected Frames` モードへ切り替えた際、保存済みの選択区間が空なら現在ジョブの有効な start/end を 1 区間の初期値として保存する。
- 価値: UI でモードを選んだだけのジョブが、空の範囲による preflight エラーで停止しない。
- 未検証: 非連続なタイムライン選択を提供する将来 UI との統合、ワークエリア由来ジョブの初期範囲、実ランタイム。
## 2026-08-02 — SelectedFrames の負フレーム対応

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` の `expandSelectedFrameRanges()`。
- 修正: 選択区間の検証から `start < 0` の排除を外し、`end > start` の半開区間だけを検証するようにした。実際のコンポジション範囲へのクランプは後段へ委ねる。
- 価値: 開始フレームが負値のタイムラインでも、SelectedFrames の区間を正しくキュー展開できる。
- 未検証: 負フレームを含むコンポジションの外部エンコーダー出力名、複数区間の重複統合。
## 2026-08-02 — SelectedFrames の区間数表示

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` のジョブ概要。
- 修正: FrameRangeMode が SelectedFrames の場合、保存されている非連続区間の数を `Selected ranges: N` として概要へ追加した。
- 価値: 複数区間が別ジョブへ展開される設定かどうかを、詳細欄を開かず確認できる。
- 未検証: 区間数が多い場合の概要幅、展開後ジョブ一覧との表示同期。
## 2026-08-02 — SelectedFrames の範囲編集 UI

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 実装: Selected Frames 用に開始／終了フレームのスピンボックスを追加し、確定時に半開区間 `[start, end)` を `selectedFrameRanges` へ保存する。既存ジョブに範囲がない場合はジョブの start/end を初期表示する。
- 価値: SelectedFrames が単なるモード表示ではなく、Render Queue Inspector から実際に区間を編集できる。
- 未検証: 非連続区間を複数入力する UI、負フレーム、実ランタイムの選択モード切替。
## 2026-08-02 — SelectedFrames の非連続区間編集

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 実装: `Add Range` で現在の `[start, end)` 入力を既存の `selectedFrameRanges` へ追加し、`Clear Ranges` で全区間を消去できるようにした。Inspector 同期時にボタンの有効状態も更新する。
- 価値: 非連続区間を複数登録し、Render Queue のジョブ分割仕様へ UI から直接渡せる。
- 未検証: 重複区間の自動統合（サービスの展開段階で処理）、区間数が多い場合の UI 幅、空リストでの preflight 表示。
## 2026-08-02 — SelectedFrames 区間一覧の可視化

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 修正: Inspector に登録済みの `[start, end)` 区間一覧を表示するラベルを追加し、ジョブ同期時に `Ranges: [a, b), ...` を再生成するようにした。
- 価値: 非連続区間の登録結果を、キュー投入前にその場で確認できる。
- 未検証: 区間数が非常に多い場合のラベル折り返し、重複統合後の表示差、実ランタイムの再選択。
## 2026-08-02 — Flexible Grid のズームフェード

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のグリッド描画。
- 修正: ズーム値に応じて major／minor／axis グリッド色のアルファを `0.25〜1.0` の範囲で縮退させ、矩形・極座標・アイソメトリック表示へ適用した。
- 価値: 大きくズームアウトしたときの線密度による視覚ノイズを抑え、キャンバス内容を読みやすくする。
- 未検証: 極端なズーム値、透過色設定との組み合わせ、実ランタイムの表示バランス。
## 2026-08-02 — Grid ラベルのズームフェード整合

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の極座標／アイソメトリック／矩形グリッドラベル。
- 修正: グリッド線と同じ `gridFade` を数値ラベルのアルファにも適用した。
- 価値: ズームアウト時に線だけ薄くなってラベルだけが残る不整合を防ぐ。
- 未検証: 低ズーム時のラベル可読性、DPI スケーリング、実ランタイム表示。
## 2026-08-02 — SelectedFrames 未選択状態のリセット

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の `syncDetailEditorsFromJob()`。
- 修正: ジョブが未選択または無効な場合、区間一覧を `Ranges: none` に戻し、開始／終了入力と Add/Clear ボタンを無効化する。
- 価値: 前のジョブの SelectedFrames 情報が Inspector に残って見える状態を防ぐ。
- 未検証: ジョブ削除・一括削除直後の再描画順序、実ランタイムのフォーカス状態。
## 2026-08-02 — Text 作成キャンセル時の仮レイヤー破棄

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Text ツール mouseRelease 処理。
- 事実: 新規 Text レイヤー作成後に編集ダイアログを開くため、入力をキャンセルすると初期値だけのレイヤーが残る経路があった。
- 修正: ダイアログが未承認の場合、作成直後のレイヤー ID を使って仮レイヤーを削除し、選択状態と合成キャッシュを更新するようにした。
- 価値: 作成操作のキャンセルが「何も作成しなかった」状態になり、空の Text レイヤーがタイムラインへ残らない。
- 未検証: 実ランタイムでのダイアログキャンセル、Undo 履歴との組み合わせ、サービス削除後の選択同期。
## 2026-08-02 — SelectedFrames 区間の重複登録防止と順序正規化

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の SelectedFrames UI。
- 修正: 同一の `[start, end)` 区間を二重登録せず、追加後の区間一覧を開始フレーム・終了フレーム順に並べるようにした。
- 価値: 非連続区間の表示順とキュー展開順の予測可能性を高め、同一区間の重複ジョブ生成を抑える。
- 未検証: 重なり合うが完全一致しない区間の統合方針、サービス側での追加正規化。
## 2026-08-02 — Render Queue ROI 出力サイズの実クロップ整合

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` の software/GPU 出力サイズ計算。
- 事実: コンポジション外へはみ出す crop は描画時にコンポジション矩形へ交差処理される一方、解像度プリセットの基準サイズが要求された `cropW/cropH` のままだった。
- 修正: 実際に生成されたクロップ画像、またはコンポジション矩形との交差矩形を基準にプリセット解像度を計算するようにした。
- 価値: ROI が境界外へはみ出すケースでも、出力ピクセルサイズと実画像の内容が一致しやすくなる。
- 未検証: ROI が完全にコンポジション外の場合の事前診断・バックエンド別エンコーダー挙動。
## 2026-08-02 — 完全境界外 ROI の事前エラー化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` の render preflight。
- 修正: ROI/custom crop とコンポジションの交差矩形が空の場合、従来の境界外警告ではなくキュー投入を止める設定エラーとして報告するようにした。部分的なはみ出しは従来どおり警告とクリップを維持する。
- 価値: 完全外側の ROI が意図せずフルフレームへフォールバックする曖昧な挙動を防ぐ。
- 未検証: UI の preflight 表示更新タイミング、保存済み旧ジョブの再読込時の診断表示。
## 2026-08-02 — SelectedFrames UI とサービス展開ルールの統一

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 修正: UI で区間を追加した後、開始順に並べ、重複・接続する区間を表示段階でマージするようにした。
- 価値: Inspector の区間一覧と、キュー実行時に生成される個別ジョブの区間境界を一致させる。
- 未検証: 既存保存データに含まれる不正な区間、極端に多数の区間を登録した場合の UI 負荷。
## 2026-08-02 — Selective Render レイヤーリスト要約

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の Selective Render Inspector。
- 修正: 保存済み whitelist／blacklist の先頭 ID を短縮表示し、残数と Included／Excluded の区別を常時表示するラベルを追加した。ジョブ未選択時は `none` に戻す。
- 価値: レイヤーフィルタの実体を件数だけでなく確認でき、Custom／Selected 設定の誤投入を減らす。
- 未検証: UUID 以外の ID 表現、非常に狭い Inspector 幅での折り返し。
## 2026-08-02 — レイヤーフィルタモードの明示

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 修正: Included／Excluded の ID 要約に `All / Selected / Solo / Visible / Custom` の現在モードを追加し、ジョブ未選択時の初期表示も `Mode: All` に統一した。
- 価値: ID リストが空でも、フィルタモード自体が Selected／Solo などへ切り替わっている状態を見落としにくくする。
- 未検証: 古い保存値が範囲外のモード番号を持つ場合の表示。
## 2026-08-02 — Render Pass 有効数の Inspector 表示

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の Configure Passes ボタン。
- 修正: 有効かつ名称付きの render pass 数を数え、`Configure Passes (N)…` と表示するようにした。Split Passes が無効、またはジョブ未選択時はボタンを無効化する。
- 価値: パス分割設定の存在と設定数を、ダイアログを開かずに確認できる。
- 未検証: pass 名変更直後の Inspector 更新順序、非常に長いパス名との併用。
## 2026-08-02 — Layer Eraser 全フレーム dirty 通知

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm` の `clearAllFrames()`。
- 調査: `markDirty()` が現状 no-op であることを確認し、全フレームへ通知するだけの変更は実効性がないため撤回した。
- 現状: `clearAllFrames()` 後の `changed()` と呼び出し側の `publishLayerModified()` がレイヤー全体の再描画経路を担う。フレーム単位 dirty API の実装は別課題として残る。
- 価値: 効果のないループを追加せず、キャッシュ無効化の責務を実際の revision／publish 経路に残せた。
- 未検証: フレーム単位 GPU キャッシュを本当に導入する場合の世代キー設計。
## 2026-08-02 — 楕円シェイプの自動レイヤー名

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Rectangle/Ellipse 作成処理。
- 修正: EllipseShape モードでは `Ellipse` を基底名にし、RectangleShape モードでは `Rectangle` を維持するようにした。
- 価値: タイムライン上で作成したシェイプ種別を名前から識別できる。
- 未検証: 既存プロジェクトの命名規則、ローカライズ表示名との整合。
## 2026-08-02 — Render Pass 出力ファイル名の衝突回避

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` の `expandEnabledRenderPasses()`。
- 事実: 異なる pass 名でもサニタイズ後に同じ文字列になると、同一の出力パスへ複数ジョブが書き込む可能性があった。
- 修正: キュー内の通常ジョブ出力を先に予約し、展開済み pass 出力も大文字小文字を畳み込んで追跡する。衝突時は `_2`, `_3` の連番を付与する。
- 価値: `A/B` と `A:B` のような pass 名だけでなく、別ジョブの既存出力との上書きも避けやすくする。
- 未検証: 外部プロセスが同時に同じパスへ書き込むケース、既存キュー内の意図的な同一出力設定。
## 2026-08-02 — Renderer 非依存 ViewportOverlayManager

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: 複数の水平／垂直ルーラーとスケールバーを ID で登録・削除・表示切替できるデータ管理クラスを追加した。個別設定から既存の tick／scale-bar factory を呼び出し、描画用中間データだけを返す。
- 価値: レンダラーや描画 widget に依存せず、将来の複数単位・複数位置オーバーレイへ拡張できる。
- 未検証: 実 widget からの複数 overlay 接続、キャッシュ最適化、3D compass／距離計測 overlay。
## 2026-08-02 — Viewport Overlay runtime 設定更新

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: 登録済み ID に対してルーラー／スケールバー設定を差し替える `configureRuler()` と `configureScaleBar()` を追加した。存在しない ID は `false` を返す。
- 価値: ズーム単位、目標ピクセル間隔、水平／垂直方向、ラベル表示を overlay 再登録なしで変更できる。
- 未検証: runtime 設定変更と外部キャッシュの同期、同一 ID の異種 overlay 競合。
## 2026-08-02 — Viewport Overlay tick cache

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: ルーラーごとに zoom／viewport origin／viewport size／canvas size と生成 tick を保持し、入力が同じ場合は再計算を省略する。設定変更と `invalidateCache()` で明示的に破棄する。
- 価値: overlay 描画が毎フレーム同じ tick 計算を繰り返す負荷を抑え、pan／zoom 更新時だけ再生成できる。
- 未検証: 多数 overlay のメモリ量、浮動小数点の微小変動によるキャッシュヒット率、実 renderer 統合後の invalidation 契約。
## 2026-08-02 — Scale Bar 配置アンカー

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: Scale Bar に BottomLeft／BottomRight／TopLeft／TopRight の anchor と X/Y margin を追加し、生成データの viewport 座標へ反映した。デフォルトは従来の左下配置を維持する。
- 価値: 同一 viewport に複数単位のスケールバーを異なる隅へ配置する基盤になる。
- 未検証: ラベル矩形との重なり回避、極小 viewport、DPI スケーリング。
## 2026-08-02 — Ruler 基準アンカー

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: Ruler に Start／Center／End の anchor を追加し、Center はキャンバス中央、End はキャンバス終端を基準に tick 範囲を生成するようにした。Start は従来の viewport origin 基準を維持する。
- 価値: 画面追従ルーラーと、キャンバス中央・終端基準の補助ルーラーを同じ manager で扱える。
- 未検証: viewport が canvas より大きい場合の表示、anchor 切替時の外部 cache invalidate、垂直軸の実描画位置。
## 2026-08-02 — Renderer 非依存 Grid Label データ

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: Grid Label の単位・目標ピクセル間隔・表示位置を登録できる設定と、ズームから現在の間隔／ラベルを生成する API を追加した。表示切替と設定更新は他の overlay と同じ ID 管理で扱う。
- 価値: グリッド描画本体から、間隔ラベルの計算責務を分離できる。
- 未検証: 実グリッド描画への接続、複数単位ラベルの重なり回避。
## 2026-08-02 — Renderer 非依存 Viewport Compass データ

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: Compass の登録・設定・表示切替と、yaw から X/Y 軸終点を計算するデータ生成 API を追加した。座標とサイズだけを返し、実描画は renderer 側へ分離している。
- 価値: 3D viewport の方向表示を既存の overlay 管理へ追加できる。
- 修正補足: 画面座標系では yaw=0 の Y 軸を上向き（負の画面 Y）として生成するようにした。X 軸は右向きを維持する。
- 未検証: カメラの pitch／roll、実 3D gizmo との方位同期。
## 2026-08-02 — Viewport tick cache の許容誤差比較

- 関連: `Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: cache hit 判定の zoom、origin、viewport／canvas size を厳密一致から相対許容誤差比較へ変更した。微小な浮動小数点揺れでは tick を再生成しない。
- 価値: フレームごとのカメラ行列・DPI 計算による僅かな数値差でキャッシュが無効化されるケースを抑える。
- 未検証: 極端に大きい canvas 座標、意図的に非常に小さい pan 差を即時反映したいケース。
## 2026-08-02 — Viewport Overlay 1フレーム一括生成

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: Ruler／Scale Bar／Grid Label／Compass の可視データを `ViewportOverlayFrameData` にまとめる `generateAll()` を追加した。各 ID の個別生成 API と tick cache はそのまま再利用する。
- 価値: renderer adapter が overlay 種別ごとに manager を反復呼び出しせず、1 フレーム単位の中間表現を受け取れる。
- 未検証: overlay 数が非常に多い場合のコピー量、描画 backend での ID 順序依存、frame data の再利用。
## 2026-08-02 — Viewport Overlay lifecycle clear

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: 全 overlay エントリと ruler tick cache を一括破棄する `clear()` を追加した。manager 寿命中の ID 採番はリセットせず、旧 frame data と新 overlay の ID 衝突を避ける。
- 価値: composition／workspace 切替時に旧 overlay が残る状態を、manager の単一操作でリセットできる。
- 未検証: 外部 renderer が保持する旧 frame data の寿命、長時間運用時の ID 上限。
## 2026-08-02 — AnchorPoint Ctrl+ダブルクリック中央リセット

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 修正: AnchorPoint ツールがアクティブな場合だけ viewport 上の Ctrl+左ダブルクリックを、既存の `resetSelected2DAnchorToCenter()` へ接続した。アンカー移動と位置補正は既存の `AnchorPoint2DUndoCommand` を通る。
- 価値: AnchorPoint ツールでレイヤー中心へ戻す操作を、Inspector や別 UI を開かずに実行できる。
- 未検証: Text／3D レイヤー選択中の優先順位、複数選択時の対象、実ランタイムの modifier 判定。
## 2026-08-02 — 2D回転のShiftスナップ

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、TransformGizmo の Rotate 操作。
- 実装: Shift 押下中の2D回転角を45度単位へスナップし、確定角度から累積ドラッグ値を再同期するようにした。これにより、回転リングと HUD の表示、後続のマウス差分、位置補正が同じ角度を参照する。
- 価値: ビューポート上で水平・垂直・斜め方向へ正確に配置しやすくなる。
- 未検証: Shift の押下／解除をドラッグ途中で切り替えた場合の操作感、複数選択時の各レイヤー補正、実ランタイムの角度表示。
## 2026-08-02 — AnchorPoint 数値HUD

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、AnchorPoint ツール。
- 実装: アンカーポイントをドラッグしている間、現在のローカル座標を `OVR:ANCHOR X/Y` としてアンカー付近に表示する。表示は既存の overlay panel／renderer 経路を使用する。
- 価値: 視覚的な中心合わせだけでなく、レイヤー座標を確認しながら精密にアンカーを配置できる。
- 未検証: キャンバス端でのラベルの画面外クリップ、極端なズーム時のラベル密度、複数選択時の代表値の妥当性。
## 2026-08-02 — 回転スナップ刻みの設定化

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、Viewport の Rotate 操作。
- 実装: `ArtifactStudio/Viewport/RotationSnapDegrees` を読み取り、15／30／45／90度の最近傍値へ正規化してShiftスナップに使う。未設定時は45度を既定値とする。
- 価値: 固定45度だけでなく、精密配置や粗い方向合わせに適した刻みへ切り替えられる。既存の設定保存方式と互換性がある。
- 未検証: 設定UIからの書き込み導線、設定変更中のドラッグ、複数アプリ設定スコープ。
## 2026-08-02 — Scale Center の倍率HUD

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、Scale Center ハンドル。
- 実装: 中心基準の均等スケール中に、既存の resize badge へ開始倍率に対する現在倍率をパーセント表示する。
- 価値: サイズ変化を目視だけでなく「150.0%」のような数値で確認できる。既存のoverlay描画とズーム追従を再利用する。
- 未検証: 初期倍率が負値／極端値のレイヤー、他のScaleハンドルとの表示形式統一、複数選択時の代表倍率。
## 2026-08-02 — Scale コーナーの均等倍率既定

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、Scale の四隅ハンドル。
- 実装: 四隅ドラッグはShiftなしで開始矩形のアスペクト比を維持し、Shift押下時だけ自由な縦横比変更を許可する。固定点は反対側のコーナーとして、位置補正の既存経路へ渡す。
- 価値: Scaleツールの既定操作を均等スケールへ揃え、意図しない画像・シェイプの変形を抑える。
- 未検証: ドラッグ途中のShift切替、負方向への反転、テキスト／シェイプ固有サイズ編集との組み合わせ。
## 2026-08-02 — 3D軸ギズモの変形HUD統合

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、3Dフレーム／軸ギズモのオーバーレイ。
- 実装: 投影フレームの移動・リサイズだけでなく、`Artifact3DGizmo` の軸ドラッグ中も同じ位置・サイズ・回転HUDを表示する。操作種別は `GIZMO` として区別する。
- 価値: 3D操作で現在値が見えなくなる経路をなくし、フレームギズモと軸ギズモのフィードバックを統一する。
- 未検証: 軸ギズモの各モードでのパネル位置、カメラ行列が無効な場合のフォールバック、HUDと軸ラベルの重なり。
## 2026-08-02 — 3D HUD 操作モードの具体化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、3D変形HUD。
- 修正: 軸ギズモ操作時の汎用的な `GIZMO` 表示を、実際の `MOVE`／`ROTATE`／`SCALE` モード名へ置き換えた。投影フレームの移動・リサイズ表示は従来どおり優先する。
- 価値: 数値がどの操作によって変化しているかをHUDだけで判別できる。
- 未検証: `Full` モード時の表示名、軸ごとの操作名表示、モード切替中のドラッグ状態。
## 2026-08-02 — 3D HUD の軸／平面表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact3DGizmo` のHUD。
- 実装: 軸ギズモのアクティブハンドルを `X`／`Y`／`Z`／`XY`／`YZ`／`XZ`／`SCREEN` としてモード名へ付加する。投影フレーム操作時は従来の `MOVE`／`RESIZE` 表示を優先する。
- 価値: 同じRotate／Scaleモードでも、どの軸・平面を操作しているかを即時に把握できる。
- 未検証: `Full` モードのNone軸、軸ラベルの長さによるHUD幅、画面平面ハンドルの実操作名。
## 2026-08-02 — Mask tangent ダブルクリックリセット

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 実装: Penツール中のダブルクリックで、現在hover中のIn／Out tangentを既存の`resetHoveredMaskTangent()`経由でゼロへ戻す。編集トランザクションとUndo経路は既存実装を再利用する。
- 価値: ベジェハンドルを正確に原点へ戻す操作を、Inspectorや手動ドラッグなしで実行できる。
- 未検証: 頂点本体上のCtrlクリックとの競合、hover更新が間に合わない高速ダブルクリック、ロック済みマスク。
## 2026-08-02 — 3D HUD の実操作優先表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact3DGizmo::activeOperation()`。
- 修正: HUDの操作名を表示モードだけでなくアクティブ操作から決定するようにした。FullモードでTranslate／Rotate／Scaleのどれを掴んだかが正しく表示され、操作未確定時は従来のモードをフォールバックに使う。
- 価値: Fullモードや複合ギズモでのフィードバックが実際の入力状態と一致する。
- 未検証: activeOperation がドラッグ開始直後に更新されるタイミング、Noneからモードへ戻る瞬間、既存の軸ラベルとの組み合わせ。
## 2026-08-02 — 3D HUD 幅の内容適応

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、3D変形HUD。
- 実装: HUD各行のフォント幅からパネル幅を計算し、238〜360pxの範囲で調整する。軸／平面ラベルやTextBox情報が長い場合も、固定幅による切れを抑える。
- 価値: 操作状態の追加情報を表示しても、既存の数値行を読みやすく保てる。
- 未検証: 非常に長いフォント名・テキスト情報、狭いビューポートでの左右余白、DPI倍率ごとの見た目。
## 2026-08-02 — 常時3D HUDの操作／軸統一

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、3Dギズモ常時HUD。
- 修正: 右上の常時HUDもactiveOperationを優先し、`3D Rotate Axis Z`のように操作種別と軸を表示する。固定幅を276pxへ拡張して、位置・回転・サイズ・スケール行の可読性を確保した。
- 価値: 画面内に複数の3Dフィードバックが存在しても、操作状態の表記ルールが揃う。
- 未検証: 極端に狭いビューポート、Screen／複合平面の表記、2つのHUDが同時表示される場合の重なり。
## 2026-08-02 — 常時3D HUDの狭幅対応

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、常時3DギズモHUD。
- 修正: パネル幅をビューポート幅から算出し、176〜276pxへ制限した。右端位置も同じ幅を使って再計算し、通常サイズでは既存の余白を保つ。
- 価値: 小さいプレビュー領域やドッキング状態でも、HUDが右側へ大きくはみ出すケースを抑える。
- 未検証: 176px未満の極小領域、DPIスケーリング、他オーバーレイとの重なり。
## 2026-08-02 — 3D 常時HUDの平面軸表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、常時3DギズモHUD。
- 修正: XY／YZ／XZ平面ハンドルを一律の`Plane`ではなく、それぞれの平面名として表示する。X／Y／Z／Screenの既存表示も維持する。
- 価値: 平面移動・平面回転・平面スケールで、拘束方向をHUDだけから判別できる。
- 未検証: `GizmoAxis::None`からの一瞬の表示、Fullモードの平面ハンドル、狭幅時の文字収まり。
## 2026-08-02 — Mask 頂点Ctrlクリックのタンジェントリセット

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 実装: Penツールで頂点をCtrlクリックした際、In／Out両タンジェントをゼロへ戻す専用APIを追加し、既存のマスク編集トランザクションへ接続した。既にゼロの場合は従来のハンドル操作へフォールバックする。
- 価値: 仕様のCtrl+クリックリセットを実装し、ベジェ頂点を直線化する操作を明示的に行える。
- 未検証: Ctrl押下のままドラッグした場合の従来操作との互換性、既にゼロの頂点、アニメーション中のマスクパス。
## 2026-08-02 — Mask 頂点ダブルクリックのリセット導線

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、Mask Pen操作。
- 修正: Penのダブルクリック処理で、まずhover中のIn／Outハンドルをリセットし、ハンドルがない場合はhover中の頂点の両タンジェントをリセットするフォールバックを追加した。
- 価値: ハンドルの正確なヒットが難しいズーム状態でも、頂点本体からベジェを直線化できる。
- 未検証: 近接する複数頂点のhover優先順位、閉じたパスの共有接線、ダブルクリック時のパス編集開始との競合。
## 2026-08-02 — Brush主要設定のアクセシビリティ補完

- 関連: `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`、Brush Tool Options。
- 修正: Diameter／Hardness／Opacity／Flow／Spacing／Angle／Roundness の各spin boxへ英語のaccessible nameと操作説明tooltipを追加した。既存の値範囲・信号接続・レイアウトは変更していない。
- 価値: 数値コントロールが視覚的な単位だけでも、スクリーンリーダーやキーボード操作で意味を識別できる。
- 未検証: 日本語ロケールでの読み上げ、Eraser側共有コントロールとの命名整合、狭いTool Options幅。
## 2026-08-02 — Eraser主要設定のアクセシビリティ補完

- 関連: `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`、Eraser Tool Options。
- 修正: Eraser diameter／opacityにもaccessible nameとtooltipを追加し、既存のhardness／angle／roundness／strength／modeと命名粒度を揃えた。
- 価値: Eraserの主要数値設定を支援技術から識別しやすくする。
- 未検証: BrushとEraserの共有設定を切り替えた際の読み上げ順、狭幅レイアウト。
## 2026-08-02 — Last Stroke Only のアクセシビリティ補完

- 関連: `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`、Eraser Tool Options。
- 修正: Last Stroke Onlyチェックボックスにaccessible name／descriptionを追加し、単なる表示ラベルではなく「直前のストロークだけを消去する」意味を支援技術へ伝える。
- 価値: Layer EraserやPaint Eraserとの違いを、視覚に依存せず判別できる。
- 未検証: mode comboとの二重表示時の読み上げ順、ローカライズ文字列。
## 2026-08-02 — Image Sequence 端点フレーム保持

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`refreshSequenceFrame()`。
- 修正: レイヤー相対フレームが連番枚数の範囲外になった場合、最初／最後の有効フレームへクランプして表示を保持する。空のフレームを返すのはシーケンス自体が空、読み込み失敗、画像不正の場合に限定する。
- 価値: レイヤーのout pointや長い表示範囲で、末尾到達時に画像が突然消える不安定さを抑える。
- 未検証: 意図的なblank tailを必要とする素材、ホールド以外のloop／ping-pong設定、シーケンスFPSとコンポFPSの差。
## 2026-08-02 — Image Sequence FPS時間変換

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`ArtifactImageLayer::draw()`。
- 実装: コンポジションFPSと連番素材FPSが異なる場合、レイヤー相対フレームを秒へ変換してからシーケンスフレームへ丸める。FPS情報が無い場合は従来の1:1フレーム対応へフォールバックする。
- 価値: 24fps素材を30fpsコンポジションで再生する場合などに、フレームの進み方が時間基準で安定する。
- 未検証: 非整数FPS、負の開始時刻、ドロップフレーム表現、loop／ping-pong設定との組み合わせ。
## 2026-08-02 — Renderメニューのアクセシブル説明

- 関連: `Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm`、Render menu actions。
- 修正: 現在のコンポジション追加、全コンポジション追加、キュー表示、管理、出力設定、開始、全削除の各 QAction に、操作内容を説明するaccessible descriptionを追加した。既存のショートカット・有効状態・実行経路は変更していない。
- 価値: 日本語の短い表示名だけでは意味が曖昧なアクションでも、支援技術から目的を識別できる。
- 未検証: QtプラットフォームごとのQAction説明読み上げ、メニュー再構築時の保持、ローカライズ。
## 2026-08-02 — Renderメニューのステータス説明

- 関連: `Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm`、Render menu actions。
- 修正: accessible descriptionと同じ目的文をQActionのstatus tipにも設定した。メニュー選択時のステータスバー表示と支援技術向け説明を同じ語彙に揃えた。
- 価値: アクションを実行する前に、現在の操作対象をステータスバーで確認できる。
- 未検証: メインウィンドウ側のstatus bar受信、メニュー再構築後の表示、翻訳方針。
## 2026-08-02 — Project Settings 検証のエクスポート接続

- 関連: `Artifact/src/Project/ArtifactProjectExporter.cppm`、`ArtifactProjectSettings::validate()`。
- 事実: エクスポート前はプロジェクトツリー検証のみで、プロジェクト名の禁止文字など設定モデルの検証結果を評価していなかった。
- 修正: エクスポート前に設定を検証し、Error は書き出しを中止、Warning／Info はログへ通知するようにした。
- 価値: まだ専用のプロジェクト設定ダイアログがない状態でも、不正なメタデータを成果物へ流し込む経路を減らせる。
- 未検証: GUIからの保存経路がExporterを必ず通るか、既存プロジェクトの不正名を開いて再保存する場合のユーザー通知。
## 2026-08-02 — ビューポートホイールの高精細入力対応

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`wheelEvent()`。
- 事実: 従来は `angleDelta()` のみを読み、トラックパッド等の `pixelDelta()` だけを返す入力では垂直デルタがゼロでも固定の縮小分岐へ進んでいた。
- 修正: pixel deltaを正規化して連続ズーム量へ変換し、Shift水平パンでもpixel deltaを利用するようにした。従来のマウスホイールの120単位経路は維持した。
- 価値: 高精細ホイール／トラックパッドでのズーム方向誤判定と、入力粒度を無視した飛び飛びの操作を抑える。
- 未検証: 各OS・QtプラットフォームのpixelDeltaスケール、自然スクロール設定、長時間の高頻度イベント時の描画負荷。
## 2026-08-02 — パッケージ化経路のプロジェクト検証

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`、`collectAndPackage()`。
- 事実: パッケージ化は `project->toJson()` を直接呼び、通常Exporterの設定／ツリー検証を経由していなかった。
- 修正: アセットコピー開始前にプロジェクト検証を実行し、設定Errorまたはツリー不整合があれば処理を中止する。Warning／Infoはログへ通知する。
- 価値: 不正なメタデータや壊れた参照を含むスタンドアロンパッケージの生成を防ぎ、通常保存との検証基準を揃える。
- 未検証: 既存の不完全プロジェクトを救済目的でパッケージ化する運用、コピー途中の失敗時に作成済みAssetsをロールバックする仕様。
## 2026-08-02 — パッケージJSONの原子的書き込み

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`、`collectAndPackage()`。
- 事実: パッケージの `project.json` は通常の `QFile` へ直接書き込み、書き込みサイズと完了状態を確認していなかった。
- 修正: `QSaveFile` を使い、完全な書き込みとcommitに成功した場合だけ最終ファイルへ置換する。Assetsディレクトリ作成の失敗も明示的に扱う。
- 価値: パッケージ生成中のディスク容量不足やI/O障害で、既存の `project.json` を壊したり不完全なJSONを残したりするリスクを下げる。
- 未検証: アセットコピー途中の失敗時に残るファイルのクリーンアップ方針、ネットワークドライブ上のQSaveFile置換動作。
## 2026-08-02 — Rig編集ツール型の基盤追加

- 関連: `Artifact/include/Tool/ArtifactToolManager.ixx`、`Artifact/src/Tool/ArtifactToolManager.cppm`、`Artifact/src/Widgets/ArtifactToolBar.cppm`。
- 事実: リグ描画ヘルパーは存在する一方、仕様が参照するRigSelect／RigWeightのToolTypeが未定義だった。
- 修正: `RigSelect` と `RigWeight` をToolTypeへ追加し、toolNameとツールラベルを登録した。既存の選択・描画・イベント経路はまだ変更していない。
- 価値: 後続のリグピック／ウェイトペイント実装が、既存ツール管理契約上の明確な型を利用できる。
- 未検証: ツールバーへの専用ボタン、ショートカット、実際のリグ選択・ウェイト操作。
## 2026-08-02 — Rigツールバー登録

- 関連: `Artifact/src/Widgets/ArtifactToolBar.cppm`。
- 修正: 既存のActionGroup／既存lambdaを利用し、RigSelectとRigWeightのツールボタンを追加した。専用の新規signal/slot接続は追加していない。
- 価値: リグ編集ツールを内部APIだけでなく、ユーザーが選択できるUI状態へ進めた。
- 未検証: 実際のボーンピック、ウェイトペイント、アイコン候補の配布環境での表示。
## 2026-08-02 — Rigオーバーレイ表示の接続

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 修正: `showRigOverlay` の公開設定を追加し、RigSelect／RigWeight選択時または明示フラグ有効時に、選択中の `ArtifactAbstract2DLayer` のボーン、コントロール、スキンメッシュ線を既存描画ヘルパーで表示するようにした。
- 価値: リグ編集UIの最初のフィードバックとして、現在対象の骨格とコントロールをVP上で確認できる。
- 未検証: 変形後メッシュ表示、カメラ変換を含む座標一致、選択ボーンの強調、ボーン／コントロールのピック操作。
## 2026-08-02 — RigSelectのボーン／コントロールヒットテスト

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`handleMousePress()`／`handleMouseRelease()`。
- 修正: RigSelectの左クリックで、選択中の2Dレイヤーを対象にコントロールを優先してヒットテストし、次にボーン線分を再帰探索するようにした。レイヤーのglobal transformを逆変換してローカル座標で距離判定し、ズームに応じてヒット閾値を補正する。
- 状態: 選択したボーン／コントロールIDをController内に保持し、マウスリリースまでの一時ドラッグ状態を管理する。実際の回転・値変更は次段階に残している。
- 価値: リグオーバーレイが単なる表示から、編集対象を識別できるインタラクション基盤へ進んだ。
- 未検証: ボーンの親子座標がresolvedTransformで表現される場合の線分位置、選択ハイライト描画、ドラッグ中のポーズ更新とUndo。
- 追加項目: RigSelectボーン回転ドラッグ。
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、RigSelectのmouse press／move経路。
- 修正: 選択したボーンをドラッグすると、ボーン原点を基準にローカル座標の角度差を計算し、`setRigBoneLocalTransform()` でrotationを更新する。Shift押下時は15度刻みにスナップし、階層を再計算して描画・レイヤー変更通知を行う。
- 価値: リグオーバーレイとヒットテストが、実際のポーズ編集操作へ接続された。
- 未検証: Undoコマンド化、キーフレーム自動追加、制約／SmartBone評価との同時利用、親ボーン回転時の表示座標。
- RigControl操作を追加。Pointはローカル位置、SliderはXドラッグ量をmin/maxへ正規化、Angleは原点周りの角度として更新する。リリース時に値変更を専用UndoCommandへまとめる。
- 未検証: コントロール値に紐づくRigPropertyBinding／制約の即時評価、Pointのドラッグ基準、スライダー感度のUI設定。
- Rigコントロール変更後にレイヤーの通常フレーム評価経路を呼び、既存のRigPropertyBinding／制約／値クランプを再利用する。選択中のボーン原点とコントロールに黄色のリングを描画する。
- 未検証: 高頻度ドラッグ中の評価コスト、Angle／Sliderコントロールの表示原点、キーフレーム値との競合。
- RigSelect／RigWeightのVP下部に、現在モードと選択中のボーン／コントロール名を表示するHUDを追加した。表示ハイライトと同じ状態を使うため、選択対象の確認経路が二重化される。
- 未検証: 小さいビューポートでのHUD重なり、ローカライズ、Weightモードの実編集との表示整合。
- EscapeキーでRigSelectの選択とドラッグ状態を解除するController APIを追加し、既存のCompositionEditorキー処理へ接続した。マスク／テキストのキャンセルを先に評価する順序は維持している。
- 未検証: RigWeightのブラシ状態解除、フォーカスが子ウィジェットにある場合のキー伝播。
- ボーン描画とヒットテストの座標を、各ボーンのlocal/resolved positionから`globalMatrix()`基準へ統一した。親子階層の子ボーンでも原点と先端が同じ階層変換を通り、描画と選択判定の不一致を減らせる。
- 未検証: 非一様スケールを持つ親子ボーン、スキンメッシュのdeform座標との一致、カメラ投影下のruntime表示。
- RigWeightモードで、既存SkinMeshの読み取り専用頂点ウェイトを選択ボーンへ投影し、deform後頂点を青→緑→赤で可視化する表示を追加した。Coreの頂点書き込みAPIは変更していない。
- 未検証: 4ウェイトスロット以外の表現、ボーン選択前の対象インデックス、密なメッシュでの描画負荷。
- RigWeightでも同じボーンヒットテストを使って対象ボーンを選択できるようにした。RigWeightでのクリックは対象選択だけを行い、RigSelectのような回転ドラッグは開始しない。
- 未検証: Weight Paintブラシによる書き込み、複数ボーン選択、選択状態の別パネルとの同期。
- RigSelect回転のUndoを追加。ドラッグ開始時のBoneTransformを保持し、リリース時に変更があれば専用UndoCommandを1件だけ積む。undo/redoでは同じリグAPIと階層更新を利用する。
- 未検証: 複数ボーン選択、キーフレーム化、制約評価との組み合わせ。
- RigSelect／RigWeight QActionにaccessible name／descriptionを追加し、選択操作とウェイトヒート表示の違いを支援技術へ明示した。
- 未検証: QtプラットフォームごとのQAction読み上げ、ローカライズ、ツールチップとaccessible descriptionの表示順。
- ControllerのtoolTypeToOverlayLabelへRigSelect／RigWeightを追加し、既存HUDや診断表示で一般名`Tool`へフォールバックしないようにした。
- 未検証: 全オーバーレイの表示箇所、翻訳、狭幅ビューでの文字切り詰め。
- RigSelect／RigWeightに既存のselectカーソル資産を割り当て、通常Selectionと同じArrowへ落ちず、リグ編集モード中であることをカーソルでも示すようにした。新規アイコンは追加していない。
- 未検証: ウェイトブラシ実装後のブラシ形状カーソル、DPI別のカーソル可読性。
- Rig PointコントロールのShiftドラッグを水平／垂直軸ロック、AngleコントロールのShift操作を15度スナップとして実装した。ボーン回転と同じ修飾キー規則をコントロールにも適用する。
- 未検証: RTL座標系、細かい角度範囲、Pointの親階層変換。
- Rig HUDの選択表示へ、ボーン回転角、PointのX/Y、Slider／Angleの現在値を追加した。ドラッグ中の操作結果を別パネルへ移動せず確認できる。
- 未検証: 長い日本語名と数値の横幅、狭いビューポートでの切り詰め、単位表記のローカライズ。
- Rigオーバーレイの明示表示設定を`ArtifactStudio/RigOverlayVisible`へ保存し、Controller生成時に復元するようにした。ツール選択による一時表示は従来どおり自動で行う。
- 未検証: 複数Controller生成時の設定共有、Workspace別設定の分離、設定移行時のキー互換。
# 2026-08-02: Rig表示をラインデバッグ項目へ分離

- 関連: `ArtifactCompositionRenderController` / Rig overlay
- 事実: Rig の骨、コントロール、スキン表示は従来、Rig overlay が有効なら一括描画されていた。
- 変更: `LineDebugKind::RigBone`、`RigControl`、`RigSkin` を追加し、既存のライン表示配列から個別に描画を抑制できるようにした。既定値は骨・コントロールを表示、スキンを非表示とした。
- 価値: Rig Select と Weight の作業時に、表示ノイズを既存のデバッグ表示経路で段階的に減らせる。
- 未検証: UI 側で新しい3項目を公開するメニュー接続はまだ追加していない。現時点ではコントローラ内部の表示状態を利用する実装段階。
- 次に確認: ライン表示設定の既存UIへ Rig 3項目を追加する際、既存メニューの責務と保存キーを確認する。

# 2026-08-02: Rigライン表示の設定を保存

- 関連: `ArtifactCompositionRenderController::setLineDebugKindVisible`
- 変更: RigBone / RigControl / RigSkin の表示状態を `QSettings` に保存し、再起動時に復元するようにした。
- 判断: 既存の全ラインデバッグ項目へ一括で保存仕様を広げず、今回追加したRig表示項目に限定した。
- 未検証: ビルド・実行による設定復元確認は未実施。

# 2026-08-02: Rig WeightのVPペイント入力を追加

- 関連: `CompositionRenderController::handleMousePress/Move/Release`
- 変更: RigWeight選択時にメッシュ上をドラッグすると、選択ボーンのウェイトを距離フォールオフ付きで加算する処理を追加した。Ctrlドラッグでは減算し、4スロット内に対象ボーンがない頂点は最小ウェイトスロットを再利用する。更新後は全スロットを正規化する。
- 制約: CoreのSkinMesh APIを変更せず、公開済みの `vertices()` と `setVertices()` のみを利用している。
- 未検証: Undo復元、ミラー、ブラシ設定UI、ビルド・実行確認は未実施。

# 2026-08-02: Rig Weightのドラッグ単位Undo

- 関連: `RigSkinWeightsUndoCommand`
- 変更: Weightペイント開始時の全頂点スナップショットと、リリース時の結果を保存し、1回のドラッグを1つのUndo操作として登録するようにした。
- 制約: Coreの型やUndo基盤を変更せず、既存の `UndoCommand` と `SkinMesh::setVertices()` を利用した。
- 未検証: 実行時のUndo/Redo、巨大メッシュでのスナップショット負荷、ビルド確認は未実施。

# 2026-08-02: Rig Weightブラシの可視フィードバック

- 関連: `drawViewportCanvasOverlay` / RigWeight
- 変更: ウェイトブラシの現在位置に円形カーソルを描画し、Ctrl押下時は減算色へ切り替えた。画面下部に ADD/SUB、半径、Opacityを表示するHUDも追加した。
- 判断: 既存の `ArtifactIRenderer` 描画経路を使用し、Qtの新規合成やQImage化は行っていない。
- 未検証: 高DPI・極端なズーム時の見かけの半径、実行時表示確認は未実施。

# 2026-08-02: Rig Weightブラシ設定のキーボード調整

- 関連: `ArtifactCompositionEditor::keyPressEvent` / `adjustRigWeightBrush`
- 変更: RigWeight中の `[` / `]` でブラシ半径を調整し、Ctrl併用でOpacityを調整できるようにした。値は `QSettings` に保存して次回起動へ引き継ぐ。
- 初期値: 半径36、Opacity0.35。半径は2〜500、Opacityは0.01〜1.0にクランプする。
- 未検証: 実行時のキー入力と設定復元、ビルド確認は未実施。

# 2026-08-02: Rig Weight正規化コマンド

- 関連: `normalizeRigWeights`
- 変更: RigWeight中に `N` を押すと、各頂点の4スロットのウェイトを合計1.0へ正規化する。変更全体を `RigSkinWeightsUndoCommand` へ登録する。
- 判断: 無ウェイト頂点は勝手にボーンへ割り当てず、そのまま維持する。
- 未検証: キー入力、Undo/Redo、非正規化データの実行確認とビルドは未実施。

# 2026-08-02: Rig Weightスムージング

- 関連: `smoothRigWeights`
- 変更: RigWeight中に `S` を押すと、選択ボーンのウェイトをブラシ半径内の近傍頂点平均へ寄せ、全スロットを再正規化する。処理全体をUndo可能にした。
- 制約: 現在は頂点数に対して単純な近傍走査を行うため、大規模メッシュでは計算量が増える。必要になった段階で空間インデックス化を検討する。
- 未検証: 実行時の平滑化結果、Undo/Redo、性能、ビルドは未確認。

# 2026-08-02: Rig Weightミラー

- 関連: `mirrorRigWeights`
- 変更: RigWeight中に `M` を押すと、選択ボーンのウェイトを各頂点のX軸対称位置に最も近い頂点からコピーする。コピー後は全ウェイトを再正規化し、Undo可能にした。
- 制約: 左右ボーン名の対応情報がCoreにないため、現段階では同一ボーンの幾何学的ミラーとして実装している。将来の左右ボーン対応時に置換する。
- 未検証: 実行時結果、非対称メッシュ、大規模メッシュ性能、ビルドは未確認。

# 2026-08-02: ミラー先の未割当スロット対応

- 関連: `mirrorRigWeights`
- 変更: ミラー先の頂点に選択ボーンのスロットがない場合も、最小ウェイトスロットへ選択ボーンを割り当ててコピーするよう修正した。これにより片側だけに存在するウェイトも反対側へ伝播できる。
- 未検証: 非対称メッシュでの最適な対応頂点判定、実行時結果、ビルドは未確認。

# 2026-08-02: Rig Weight Flow

- 関連: RigWeightブラシ入力・HUD
- 変更: Flowパラメータを追加し、塗布量を `Opacity × Flow × 距離フォールオフ` で計算するようにした。Shift＋`[` / `]` でFlowを調整し、値はQSettingsへ保存する。
- 未検証: キー入力、設定復元、描画HUD、ビルドは未確認。

# 2026-08-02: Rigポーズの一時キャプチャ／適用

- 関連: `captureRigPose` / `applyCapturedRigPose` / `RigPoseUndoCommand`
- 変更: RigSelectまたはRigWeight中に Ctrl+Shift+C で現在のボーン・コントロール状態をキャプチャし、Ctrl+Shift+Vで選択リグへ適用できるようにした。適用はUndo/Redo可能。
- 制約: 現時点ではセッション内クリップボードであり、ポーズライブラリへの永続保存やUIパネルは未実装。
- 未検証: 実行時のポーズ適用、ブレンド、Undo/Redo、ビルドは未確認。

# 2026-08-02: Rigポーズのブレンド貼り付け

- 関連: `ArtifactCompositionEditor::keyPressEvent`
- 変更: Ctrl+Shift+B でキャプチャ済みポーズを50%ブレンド適用できるようにした。通常のCtrl+Shift+Vは100%適用のまま維持する。
- 未検証: 実行時ブレンド、Undo/Redo、ショートカット競合、ビルドは未確認。

# 2026-08-02: Rigポーズキャプチャ状態のHUD表示

- 関連: `drawViewportCanvasOverlay`
- 変更: ポーズをキャプチャ済みの場合、Rig HUDへ `POSE:READY` を表示するようにした。
- 価値: Ctrl+Shift+V / B が有効な状態を視覚的に確認でき、未キャプチャ状態での誤操作を減らせる。
- 未検証: 長いレイヤー名や高DPI時のHUD幅、実行時表示、ビルドは未確認。

# 2026-08-02: Rigポーズスロット永続化

- 関連: `saveRigPoseSlot` / `applyRigPoseSlot`
- 変更: Ctrl+Shift+1/2で現在のポーズを各スロットへ保存し、Ctrl+Shift+Alt+1/2で対応スロットを読み込み適用する。現在は1〜9スロットに対応する保存形式を用意し、ボーン変換とコントロール値をQSettingsへ格納する。
- 制約: ショートカットUIはまず1/2のみ公開。名前付きライブラリやサムネイルは次段階とする。
- 未検証: QVector2D等のコントロール値のQSettings往復、再起動後の適用、Undo/Redo、ビルドは未確認。

# 2026-08-02: ポーズスロットのPoint値を明示シリアライズ

- 関連: `rigPoseToVariantMap` / `rigPoseFromVariantMap`
- 変更: Pointコントロールの`QVector2D`をtype/x/y形式で保存し、読み込み時に明示的に再構成するようにした。Scalar値もtype/value形式へ統一した。
- 価値: QSettingsのQt型登録状態に依存せず、ポーズスロットのコントロール値を往復できる。
- 未検証: 実際の再起動後復元、Slider/Angleの型差、ビルドは未確認。

# 2026-08-02: Rigポーズスロット1〜9の入力公開

- 関連: `ArtifactCompositionEditor::keyPressEvent`
- 変更: Ctrl+Shift+1〜9で対応スロットへ保存し、Ctrl+Shift+Alt+1〜9で読み込み適用できるようにした。保存形式側の1〜9対応と入力範囲を一致させた。
- 未検証: 数字キー配列、Alt/Shift組み合わせ、再起動後の各スロット復元、ビルドは未確認。

# 2026-08-02: Rigポーズスロット全消去

- 関連: `clearRigPoseSlots`
- 変更: Ctrl+Shift+Alt+Backspaceで永続化済みのRigポーズスロット1〜9を一括削除できるようにした。
- 制約: 個別スロット削除UIは未実装。誤操作を避けるため、RigSelect / RigWeight中だけ有効にしている。
- 未検証: 実行時ショートカット、設定削除後の再起動確認、ビルドは未確認。

# 2026-08-02: ポーズ全消去時の一時キャプチャ無効化

- 関連: `clearRigPoseSlots`
- 変更: 永続スロットを全消去した際、セッション内のPoseSnapshotと`POSE:READY`状態も同時にクリアするようにした。
- 価値: 削除後にCtrl+Shift+Vで古い一時ポーズが適用される不整合を防ぐ。
- 未検証: 実行時のHUD更新、ショートカット後の適用拒否、ビルドは未確認。

# 2026-08-02: Viewport内Rig階層リスト

- 関連: `drawViewportCanvasOverlay`
- 変更: Rig overlay表示中に、右上へ読み取り専用のRig HIERARCHYリストを描画するようにした。ボーンの子階層、選択状態、コントロール一覧を表示する。
- 判断: まずDockや新規シグナルを増やさず、既存のArtifactIRenderer描画経路で階層の可視化を提供した。専用Dockパネルは別段階とする。
- 未検証: 深い階層・大量コントロール時の省略表示、高DPI、実行時描画、ビルドは未確認。

# 2026-08-02: Rig階層リストの省略表示

- 関連: Viewport内Rig HIERARCHY
- 変更: 表示行数を超えたボーン／コントロールがある場合、末尾に`… more`を表示して省略状態を明示するようにした。
- 価値: 深い階層や大量コントロールで、表示されていない項目を「存在しない」と誤認しにくくする。
- 未検証: 高DPI、長い名前との重なり、実行時描画、ビルドは未確認。

# 2026-08-02: Rig Weightミラー対応距離制限

- 関連: `mirrorRigWeights`
- 変更: X軸対称点の最近傍距離がブラシ半径の2倍を超える場合、その頂点はミラー対象から除外するようにした。
- 価値: 穴や大きな非対称を持つメッシュで、遠い無関係な頂点へウェイトが誤コピーされるのを防ぐ。
- 未検証: 非対称メッシュの実行結果、閾値のUX、ビルドは未確認。

# 2026-08-02: Ctrl+Tabリグ編集モード切替

- 関連: `ArtifactCompositionEditor::keyPressEvent`
- 変更: Ctrl+Tabで通常のSelectionとRigSelectを切り替えるようにした。RigSelect / RigWeight中はSelectionへ戻り、それ以外からはRigSelectへ入る。
- 判断: 既存のToolManager `setActiveTool` を利用し、ツールバー側の既存同期経路を再利用した。
- 未検証: OS/QtのCtrl+Tab予約キーとの競合、ツールバー同期、実行時表示、ビルドは未確認。

# 2026-08-02: RigSelectのキーボード回転

- 関連: `nudgeSelectedRigBoneRotation`
- 変更: RigSelect中に選択ボーンがある場合、`E`で+15度、`Shift+E`で-15度回転する。マウス操作と同じRigBoneTransformUndoCommandへ登録する。
- 価値: キーボードだけでポーズの微調整ができ、既存のUndo履歴と一貫する。
- 未検証: 既存Eショートカットとの競合、実行時回転、ビルドは未確認。

# 2026-08-02: RigSelectのPointコントロールキーボード移動

- 関連: `nudgeSelectedRigControl`
- 変更: RigSelect中にShift＋矢印キーで選択中のPointコントロールを1単位ずつ移動できるようにした。ControlValueUndoCommandへ登録する。
- 制約: Slider / Angleコントロールには適用せず、既存のマスク頂点移動より先にRigSelect条件で処理する。
- 未検証: 矢印キー競合、実行時移動、Undo/Redo、ビルドは未確認。

# 2026-08-02: Pointコントロール粗調整

- 関連: RigSelect Point keyboard nudge
- 変更: Shift＋矢印の1単位移動に加え、Ctrl+Shift＋矢印で10単位移動できるようにした。
- 未検証: ショートカット競合、実行時移動、Undo/Redo、ビルドは未確認。

# 2026-08-02: Rigモード終了時の選択状態クリア

- 関連: Ctrl+Tab Rigモード切替
- 変更: RigモードからSelectionへ戻る際、選択ボーン／コントロールとドラッグ状態を`clearRigSelection()`でクリアするようにした。
- 価値: 再入場時に古いRig選択が意図せず復活する状態を防ぐ。
- 未検証: 実行時切替、オーバーレイ状態、ビルドは未確認。

# 2026-08-02: Rigポーズブレンドのコントロール値対応

- 関連: `applyCapturedRigPose`
- 変更: 50%ブレンド適用時、ボーンだけでなくPointコントロールの`QVector2D`も線形補間するようにした。非数値コントロールはブレンド率50%以上で目標値へ切り替える。
- 価値: 「Pose blended 50%」の表示と実際のコントロール挙動を一致させる。
- 未検証: Slider / Angle / Pointの実行時ブレンド、Undo/Redo、ビルドは未確認。

# 2026-08-02: Rig Weight設定の起動時クランプ

- 関連: `CompositionRenderController` コンストラクタ
- 変更: QSettingsから復元した半径・Opacity・Flowにも編集時と同じ範囲制限を適用した。
- 価値: 手動編集された設定値や旧バージョンの異常値で、ブラシ描画が極端な値にならない。
- 未検証: 異常設定からの起動、ビルドは未確認。

# 2026-08-02: Rig Weightストローク補間

- 関連: RigWeight `handleMouseMove`
- 変更: 前回のマウス位置から現在位置までを最大64サンプルで補間し、ストローク上の最小距離を使ってウェイトを塗るようにした。
- 価値: 高速ドラッグ時の塗り抜けを減らし、ブラシ半径に応じた連続ストロークを実現する。
- 未検証: 大規模メッシュ・高速入力時の性能、実行時の塗布結果、ビルドは未確認。

# 2026-08-02: RigWeight時のスキン表示保証

- 関連: `drawViewportCanvasOverlay`
- 変更: RigWeightアクティブ中はLineDebugKind::RigSkinの既定設定に関係なくスキン／ウェイト表示を有効化するようにした。
- 価値: Weight Paintへ切り替えた直後にヒートマップが見えない状態を防ぐ。
- 未検証: 実行時の表示切替、手動非表示とのUX整合、ビルドは未確認。

# 2026-08-02: Rig Weightクリックダブ

- 関連: RigWeight `handleMousePress`
- 変更: メッシュ上のクリック時に初回ブラシダブを即時適用するようにした。移動イベントが発生しない単一点のウェイト編集も可能になった。
- 未検証: 高DPI座標変換、クリックとドラッグのUndo単位、実行時結果、ビルドは未確認。

# 2026-08-02: Rig Weightの選択クリックと塗布クリックを分離

- 関連: RigWeight `handleMousePress`
- 変更: ボーンまたはコントロール上のクリックはペイントを開始せず、メッシュ上のクリック／ドラッグだけがウェイト編集を開始するようにした。
- 価値: ペイント対象ボーンを選択する操作で意図せずウェイトが変わる事故を防ぐ。
- 未検証: コントロール重なり時のヒット順、実行時入力、ビルドは未確認。

# 2026-08-02: Rigコンテキストメニューの操作導線

- 関連: `ArtifactCompositionEditor::showViewportContextMenu`
- 変更: リグを持つ2Dレイヤー上の右クリックメニューに、Rig Overlay表示切替、ウェイトのNormalize/Smooth/Mirror、Pose Capture、50% Blend適用を追加した。
- 価値: 専用Dockや新規シグナルを増やさず、既存Controller APIを再利用してRig操作へ到達できる。
- 未検証: 右クリック対象レイヤーの判定、メニュー実行時のUndo・ポーズ適用、ビルドは未確認。
- 追加: Pose Slot 1への保存・50%適用・全Slot消去も同じメニューから呼び出せるようにした。

# 2026-08-02: ViewメニューからRig Overlayを切り替え

- 関連: `ArtifactViewMenu`, `CompositionRenderController`
- 変更: Viewメニューにチェック式の「リグオーバーレイを表示」を追加し、現在のComposition EditorのRig Overlay状態と同期させた。
- 価値: Rigツールを選択していない状態でも、ボーン・コントロール・スキン表示を明示的に確認できる。
- 未検証: メニュー表示時の状態同期、対象レイヤー未選択時の表示、ビルドは未確認。

# 2026-08-02: ウェイトマップ表示をViewメニューから分離制御

- 関連: `ArtifactViewMenu`, `drawViewportCanvasOverlay`
- 変更: `LineDebugKind::RigSkin`をViewメニューのチェック項目へ公開し、RigWeightツール選択時の強制表示を解除した。既定値は既存設定どおり有効。
- 価値: ウェイトマップを表示だけ切り替えたい場合や、Weight Paint中に骨格・コントロールだけ確認したい場合に対応できる。
- 未検証: 既存設定との互換性、実行時の表示切替、ビルドは未確認。

# 2026-08-02: Viewメニューの制作補助オーバーレイ拡張

- 関連: `ArtifactViewMenu`
- 変更: オニオンスキンとセーフマージンをViewメニューのチェック項目として追加し、既存Controller APIと状態同期させた。
- 価値: Composition Editorを操作中に、ツール切替なしで主要な確認用オーバーレイを管理できる。
- 未検証: オニオンスキンのキャプチャ更新、セーフマージン描画、ビルドは未確認。

# 2026-08-02: ViewメニューからRig Poseを再利用

- 関連: `ArtifactViewMenu`
- 変更: 現在のRigポーズCaptureとPose Slot 1の50%適用をViewメニューへ追加し、既存Controller APIへ接続した。
- 価値: コンテキストメニューを開かずに、ポーズの保存・ブレンド適用を実行できる。
- 未検証: Rig未選択時の無効化粒度、実行時のCapture／Apply、ビルドは未確認。

# 2026-08-02: ViewメニューのRig Pose Slot管理

- 関連: `ArtifactViewMenu`
- 変更: Pose Slot 1への保存と全Pose Slot消去を追加し、Capture／50%適用と合わせてメニューから一連の管理を可能にした。
- 価値: ポーズの一時保存・再利用・リセットを同一のViewメニューで完結できる。
- 未検証: Slotデータの永続化確認、Rig未選択時のUX、ビルドは未確認。

# 2026-08-02: Rig Viewメニューのアクセシビリティ情報

- 関連: `ArtifactViewMenu`
- 変更: Rig Overlay、ウェイトマップ、オニオンスキン、セーフマージン、Pose操作へAccessible Name/Descriptionを追加した。
- 価値: 日本語表示だけに依存せず、支援技術や自動UI操作から各操作の目的を識別できる。
- 未検証: Qtアクセシビリティツリー上の読み上げ結果、ビルドは未確認。

# 2026-08-02: EraserのLast Stroke Only操作

- 関連: `ArtifactCompositionEditor::keyPressEvent`, `ArtifactPaintLayer::undoLastStroke`
- 変更: Eraserツール中の`Ctrl+Alt+Z`で、選択中Paintレイヤーの直前ストロークだけを戻す操作を追加した。変更イベントと再描画も既存経路へ通知する。
- 価値: 消し過ぎずに直前の消去／描画だけを取り消す、仕様のLast Stroke Only操作をキーボードから利用できる。
- 未検証: UndoManagerとの履歴順序、空履歴時の挙動、実行時入力、ビルドは未確認。

# 2026-08-02: EraserのLayer Eraser導線

- 関連: `CompositionViewport::showViewportContextMenu`
- 変更: Eraserツール中にPaintレイヤーを右クリックすると、確認付きで全フレームを消去できる項目を追加した。既存`clearAllFrames()`、LayerChanged通知、再描画を利用する。
- 価値: ストローク単位の消去とレイヤー全体消去を明確に分け、破壊的操作には確認を挟める。
- 未検証: 確認ダイアログの実行時表示、保存状態、ビルドは未確認。

# 2026-08-02: Layerメニューから初期Rigレイヤーを生成

- 関連: `ArtifactLayerMenu::handleCreateRig`
- 変更: コンポジションサイズのSolid 2Dレイヤーを作成し、`root`ボーンと`root_ctrl` Pointコントロールを初期化する「リグレイヤー」項目を追加した。
- 価値: Rig編集を開始するための空レイヤー準備を短縮し、既存の`ArtifactAbstract2DLayer::rig2D()`所有モデルをそのまま利用できる。
- 未検証: 作成直後の選択同期、root位置／描画、保存・Undo、ビルドは未確認。
- 追加: rootをコンポジション中央へ配置し、階層更新後に`LayerChangedEvent::Modified`を発行するよう補強した。
- 追加: 作成完了後は既存ToolManagerをRigSelectへ切り替え、生成直後からボーン／コントロールを編集できる状態にした。
- 追加: 編集用SolidのOpacityを0.18に設定し、Rig下地がオーバーレイを覆わないようにした。

# 2026-08-02: RigSelectからBキーでWeight Paintへ移行

- 関連: `ArtifactCompositionEditor::keyPressEvent`
- 変更: `RigSelect`中の単独`B`キーで既存`ToolType::RigWeight`へ切り替えるショートカットを追加した。MotionSketch中の既存Bキー挙動は維持。
- 価値: Rig作業中の選択・ウェイト編集の切り替えをキーボードだけで行える。
- 未検証: Rigレイヤー未選択時の挙動、実行時ツール切替、ビルドは未確認。

# 2026-08-02: Compositionメニューからレンダーキューへ追加

- 関連: `ArtifactCompositionMenu::addCurrentToRenderQueue`
- 変更: 現在のコンポジションを既存`ArtifactRenderQueueService::addRenderQueueForComposition`へ送るメニュー項目を追加した。
- 価値: Compositionメニューからレンダーキュー登録までの導線を補完し、仕様監査の「レンダーキューに追加」を実UIへ接続した。
- 未検証: Queue登録後のUI更新、重複ジョブ方針、ビルドは未確認。

# 2026-08-02: 選択的レンダー範囲のCompositionメニュー導線

- 関連: `ArtifactCompositionMenu`
- 変更: 「現在フレーム」「ワークエリア」をそれぞれRender Queueへ追加し、追加直後のジョブへ`frameRangeMode`とフレーム範囲を設定するようにした。
- 価値: Render Queue Managerを開いてから範囲を手入力する手順を短縮し、既存のSelective Settings形式と整合させた。
- 未検証: ワークエリア終端の包含規約、Queue UIの再読込、ビルドは未確認。

# 2026-08-02: Renderメニューから一時停止

- 関連: `ArtifactRenderMenu::pauseRender`
- 変更: 既存`ArtifactRenderQueueService::pauseAllJobs`を呼ぶ「レンダリングを一時停止」項目を追加し、キューにジョブがある場合だけ有効化した。
- 価値: Render Queue Managerを開かずに全ジョブを停止できる。
- 未検証: 再開操作とのUI関係、実行中ジョブの状態遷移、ビルドは未確認。
- 追加: 開始アクションの表示を「開始／再開」に統一し、一時停止後も同じ既存開始経路で復帰できることをUI上で明示した。

# 2026-08-02: Renderメニューの全ジョブキャンセル

- 関連: `ArtifactRenderMenu::cancelRender`
- 変更: 既存`ArtifactRenderQueueService::cancelAllJobs`を呼ぶ「全ジョブをキャンセル」を追加し、ジョブが存在するときだけ有効化した。
- 価値: Queue Managerを開かずに実行中・保留中ジョブをまとめて停止できる。
- 未検証: キャンセル後の履歴表示、確認ダイアログの要否、ビルドは未確認。

# 2026-08-02: CompositionメニューのVariant依存を明示化

- 関連: `ArtifactCompositionMenu.cppm`
- 変更: 選択的レンダー設定で直接利用する`QVariantMap`をグローバルモジュールフラグメントから明示includeした。
- 価値: Qt型の間接include依存を減らし、C++20 modulesの実装単位を自己完結させる。
- 未検証: モジュール全体のビルド、依存スキャンは未確認。

# 2026-08-02: Work Area APIの型照合

- 関連: `ArtifactCompositionMenu::addWorkAreaToRenderQueue`
- 変更: `ArtifactAbstractComposition::workAreaRange()`の戻り値に合わせ、範囲取得を`start()`／`end()`へ統一した。
- 価値: Fileメニューの既存利用箇所と同じAPI契約に揃え、フィールドアクセスによるコンパイル不整合を回避する。
- 未検証: ビルドは未確認。
# 2026-08-02: 選択レイヤーのみのレンダーキュー登録

- 関連: `ArtifactCompositionMenu::addSelectedLayersToRenderQueue`
- 変更: 選択中レイヤーのIDを`layerWhitelist`へ格納し、既存Selective Settingsの`layerFilterMode=4`でキュー登録する項目を追加した。
- 価値: レンダーキュー管理画面を開いてから手動でレイヤーを指定せず、現在の選択をそのまま限定レンダーへ送れる。
- 未検証: 選択順・レイヤー削除後のWhitelist、実行時出力、ビルドは未確認。

# 2026-08-02: 選択レイヤーレンダーの範囲バリエーション

- 関連: `ArtifactCompositionMenu::addSelectedLayersToRenderQueue`
- 変更: 選択レイヤー限定キュー登録を、全範囲・ワークエリア・現在フレームの3モードへ拡張した。
- 価値: レイヤー選択とフレーム範囲を同時に指定でき、仕様の「選択範囲をレンダーキューに追加」に近い操作導線になった。
- 未検証: 終端フレームの包含規約、選択状態変化後のWhitelist、ビルドは未確認。

# 2026-08-02: Viewport右クリックから単一レイヤーをキュー登録

- 関連: `CompositionViewport::showViewportContextMenu`
- 変更: 右クリック対象レイヤーを単一WhitelistとしてRender Queueへ追加する項目を実装した。
- 価値: レイヤーパネルで選択状態を変更せず、Viewport上で対象を確認したまま限定レンダーを作成できる。
- 未検証: 右クリック対象と現在選択の差異、キュー登録後のSelective Settings表示、ビルドは未確認。
- 追加: 同じ単一レイヤーWhitelistを全範囲・現在フレーム・ワークエリアの3範囲で登録できるよう共通化した。

# 2026-08-02: Compositionレンダー登録項目のアクセシビリティ補完

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`
- 変更: 全コンポジション・現在フレーム・ワークエリアのキュー登録アクションにAccessible Name/Descriptionを追加した。
- 価値: 選択レイヤー用項目と同じ読み上げ情報を持たせ、メニュー操作の意味と対象範囲を支援技術から判別できる。
- 未検証: 実機の読み上げ結果、ビルドは未確認。

# 2026-08-02: Textフォント選択のリアルタイムプレビュー

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: フォント選択コンボボックスのイベントを既存イベントフィルタで監視し、プレビューを再描画するようにした。描画時には選択中のフォントファミリーを一時QFontへ反映する。
- 価値: ダイアログを確定する前にフォント変更の見た目を確認できる。
- 未検証: フォントフォールバック時の表示、各OSのフォント一覧更新、ビルドは未確認。
- 追加: サイズ、Bold、Italicの変更も同じイベントフィルタでプレビューへ反映するようにした。
- 追加: All Caps、Underline、Alignmentもプレビューへ反映し、入力コントロール変更時に再描画する対象へ追加した。
- 追加: TrackingとStretchもプレビュー用QFontへ反映し、文字間隔・横幅変更を確定前に確認できるようにした。
- 追加: Stroke／Stroke WidthとShadow／Blurもプレビュー描画へ反映し、関連コントロール変更時に即時更新するようにした。
- 追加: TextLayerのText／Stroke／Shadow Colorをプレビューへ反映し、固定色との差異をなくした。
- 追加: 固定サンプル文ではなく現在のTextLayer内容をプレビューし、長文は180文字で省略するようにした。

# 2026-08-02: CompositionからAdvanced Render Managerへ直接到達

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`
- 変更: Compositionメニューに「高度なレンダー設定を開く…」を追加し、既存の`ArtifactRenderCenterWindow`を再利用してRender Managerを表示するようにした。
- 価値: 仕様書のAdvanced設定（フレーム範囲、レイヤーフィルター、ROI、出力設定）へ、レンダーキュー登録後だけでなくComposition画面から直接到達できる。
- 未検証: 既存ウィンドウ再利用時の親子階層、実機表示、ビルドは未確認。
- 追加: Advanced項目を開く前に現在コンポジションを既定設定でキューへ追加し、登録済みジョブをRender Managerで調整できる導線にした。

# 2026-08-02: 3Dレイヤーのコンテキストメニューリセット

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: 選択中3Dレイヤーの右クリックメニューに`Reset 3D Transform`を追加し、既存のUndo対応コントローラ処理を呼び出すようにした。
- 価値: ダブルクリック操作を知らない利用者でも、位置・回転・スケールの初期化へ明示的に到達できる。
- 未検証: 右クリック対象と選択レイヤーが異なる場合の表示条件、実機操作、ビルドは未確認。

# 2026-08-02: 3D Transformの数値入力

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: 選択中3Dレイヤーの右クリックメニューから位置XYZ・回転XYZ・スケールXYを順番に入力し、既存の`GizmoTransformUndoCommand`で一括適用できるようにした。スケールは最小値0.001でクランプする。
- 価値: 3Dフレーム仕様の「数値入力」へ到達する実装導線を追加し、ドラッグ操作だけに依存せず正確な変形値を設定できる。
- 未検証: モジュール再スキャン後のビルド、入力途中キャンセル時のUX、キーフレーム時の値適用は未確認。

# 2026-08-02: 3D Transformのコピー／ペースト

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: 右クリックメニューから3Dレイヤーの位置・回転・スケールを`QSettings`の一時クリップボードへ保存し、別レイヤーへ既存の一括適用APIでペーストできるようにした。値がない場合はペースト項目を無効化する。
- 価値: 数値入力を繰り返さず、複数3Dレイヤーへ同じ姿勢・配置を再利用できる。
- 未検証: アプリ再起動後の設定残存、異なるコンポジション間の値適用、実機操作、ビルドは未確認。

# 2026-08-02: 3D Transformクリップボードの明示消去

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: 右クリックメニューに保存済み3D変形値の消去項目を追加し、値が存在する場合のみ有効化した。
- 価値: `QSettings`に残った古い変形値を意図せず再利用する事故を防げる。
- 未検証: 実機メニュー表示、ビルドは未確認。

# 2026-08-02: 3D Transform貼り付け値の有限値検証

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: `QSettings`から読み込んだ3D変形値を適用する前に、位置・回転・スケール全成分が有限値か検証するようにした。
- 価値: 設定破損や手動編集によるNaN／無限大がレンダー・Undo経路へ流入するのを防ぐ。
- 未検証: 異常値設定の実機挙動、ビルドは未確認。

# 2026-08-02: Text Animatorプリセット選択UI

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: テキスト編集ダイアログにText Animatorのプリセット選択を追加し、Typewriter／Slide Up／Scale In／Rotation In／Tracking Fade／Wiggly Position／Blur Reveal／Noneを既存の`text.animatorPreset`プロパティ経由で適用できるようにした。Keep currentでは既存値を維持する。
- 価値: Core側に存在するRange／Wiggly Selector実装へ、テキスト編集画面から直接到達できる。
- 未検証: プリセット適用後のプレビュー、Animator数との同時指定、ビルドは未確認。

# 2026-08-02: Text Animator操作のアクセシビリティ情報

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: Animator数とプリセット選択にAccessible Name/Descriptionを追加した。
- 価値: 読み上げ環境でも、数値がAnimatorスタック数であることと、プリセット適用タイミングを判別できる。
- 未検証: 実機の読み上げ結果、ビルドは未確認。

# 2026-08-02: OCIO表示変換はcache hit経路もpost-process対象

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Color/ArtifactOCIOManager.cppm`, `docs/memo/OCIO_MISSING_FEATURES_2026-08-01.md`
- 事実: composition cacheを再利用する分岐は、cache生成分岐と別に表示処理を持つ。表示変換を生成時だけに置くと、cache hit時に未変換SRVを直接表示する経路が残るため、両分岐を同じpost-process順序に揃えた。
- 価値: キャッシュ有無による色味の不一致と、config解除後に前回LUTが残る表示状態を防げる。
- 未検証: cache hit/miss切替、config解除、Exposure/Gamma変更を含む実機表示とビルドは未確認。

# 2026-08-02: OCIO GPU経路はnative shaderとLUT bakeを分離

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`, `Artifact/src/Render/ArtifactFinalPostProcess.cppm`
- 事実: OCIO GPU shader descriptor/resource metadataの取得APIを追加する一方、実表示経路はOCIO CPU Processorからベイクした3D LUTを既存のDiligent LUT compute passへ渡している。
- 仮説: 既存GPU passを再利用するLUT bake経路は接続範囲を限定できるが、HDRの広い入力範囲や動的uniformを完全に扱うにはnative GPU shader bindingが必要。
- 次に確認: LUT domain設計、HDR値の保持、OCIO GPU shaderのHLSL wrapperと1D/3D resource bindingをruntimeで比較する。

# 2026-08-02: キーイング拡張は既存EffectServiceへ閉じ込められる

- 関連: `Artifact/include/Effects/Keying/LumaKeyEffect.ixx`, `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`, `Artifact/include/Effects/Keying/DifferenceKeyEffect.ixx`, `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`
- 事実: Chroma Keyと同じ`ArtifactAbstractEffect`／CPU実装経路を使うことで、Luma KeyとDifference KeyをInspectorカタログ・EffectServiceへ追加できた。新規シグナルやQt合成は不要だった。
- 価値: キーイング機能の不足を、Diligent低レベル経路やReactiveEventsへ波及させず補完できる。
- 未検証: モジュールビルド、GPU経路との併用、Property Editorでの色／閾値編集、実画像のマット品質。
- 次に確認: 代表的な緑幕・明暗分離・参照色差分画像でCPU結果を比較し、必要なら既存GPUキーイング経路へ段階的に移す。

# 2026-08-02: Composition Graphは既存Widgetの登録だけで導入可能

- 関連: `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`, `Artifact/src/AppMain.cppm`
- 事実: Composition Graph Widgetは既にレイヤー検索、親子リンク、ダブルクリック選択、EventBus更新を持っていた。AppMainのProjectタブ群へ登録するだけで、既存のUI責務を保ったまま利用可能になった。
- 価値: ノードグラフ比較の最初の段階を、新しい中央イベント配線や描画基盤なしで実現できる。
- 仮説: 現状はレイヤー関係グラフであり、Nuke/Houdini相当のエフェクト接続グラフとは責務が異なる。エフェクトグラフ化は別設計として切り分けるべき。
- 未検証: 起動時レイアウト、タブ復元、プロジェクト切替時の表示更新、ビルド。

# 2026-08-02: Composition Graphのレイヤー操作はProjectServiceへ委譲する

- 関連: `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`
- 事実: 選択、削除、複製、名前変更、Visible/Locked/Solo/Shy、親設定、親解除、スタック順変更を、既存`ArtifactProjectService` APIへ委譲してグラフのコンテキストメニューへ追加した。
- 価値: Graph WidgetがレイヤーデータやUndo・選択同期を直接所有せず、既存のサービス責務とEventBus更新を維持できる。
- 仮説: Nuke/Houdini相当のエフェクト接続グラフを追加する場合も、同じWidgetへ直接ロジックを詰め込まず、EffectService／Graphモデルの専用境界を先に設けるべき。
- 未検証: 実機での各操作、Undo履歴、循環親設定の拒否表示、ビルド。

# 2026-08-02: Composition Graphのノード配置はUI設定として保存する

- 関連: `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`
- 事実: レイヤーノードの移動を`QSettings`へ保存し、Graph再構築時にLayer ID単位で位置を復元するようにした。エフェクトノードはレイヤーノード位置から再配置される。
- 価値: EventBus更新やProject保存形式を変更せず、Composition Graphの作業レイアウトだけを維持できる。
- 仮説: 将来のGraph専用保存を導入する場合は、UI配置とEffect接続／評価データを別キー・別バージョンで管理する必要がある。
- 未検証: 実機ドラッグ後の再起動復元、Layer ID再生成時の古い設定、設定削除／レイアウトリセット、ビルド。

# 2026-08-02: DAGのステージ数とGraphのイベント経路は単一の契約にする

- 関連: `Artifact/include/Effects/ArtifactAbstractEffect.ixx`, `Artifact/include/Engine/DAG/Node.ixx`, `Artifact/include/Engine/DAG/LayerGraphBuilder.ixx`, `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`
- 事実: `EffectPipelineStage` は PreProcess を含む6段階であり、DAGの配列・ループ・UI色分けを同じ段階数へ揃えた。Graphの子アイテムはViewのカスタムコンテキストメニューで直接検出できないため、親アイテム解決と共通メニュー入口が必要だった。
- 価値: LayerTransformの接続漏れと、ラベル上の右クリックが編集メニューを失う不整合を防げる。
- 未検証: モジュールビルド、全ステージのポート互換性、実機での子アイテム右クリックとDAG評価。
- 次に確認: Generator／Geometry／Materialのポート契約を実評価へ広げる前に、各ステージの入力・出力型と実装済みエフェクトを対応表にする。

# 2026-08-02: 既存の歪みエフェクトは実装済みでもInspectorカタログが別管理になる

- 関連: `Artifact/src/Service/ArtifactEffectService.cppm`, `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- 事実: `Optics Compensation`、`Turbulent Displace`、`Liquify` はEffectServiceの生成・一覧に存在する一方、Inspectorの検索カタログには一部が登録されていなかった。
- 価値: 既存機能を再実装せず、検索・カテゴリ経由の発見性だけを補完できる。
- 仮説: エフェクト追加時はServiceの生成一覧とInspectorカタログを同じ変更単位で確認するチェックが有効。
- 未検証: 起動後の検索結果、追加操作、プロパティ編集、ビルド。

# 2026-08-02: TrackPointは実装済みでもメインツールバーの導線が欠ける

- 関連: `Artifact/include/Tool/ArtifactToolManager.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: `ToolType::TrackPoint`、Tracker Gizmo、前後／全フレーム追跡、位置・アンカー適用は存在していたが、メインツールバーのアクションとツール名表示が登録されていなかった。
- 価値: 既存のトラッキング実装へ、ツールバーから到達できる導線を追加できる。
- 仮説: ToolType追加時はツールバー、オーバーレイ表示、ショートカットの3箇所を同時に確認する必要がある。
- 未検証: 実機でのアクション表示、Gizmo選択、追跡開始、ビルド。

# 2026-08-02: TrackPointの導線は表示名レジストリも揃える必要がある

- 関連: `Artifact/src/Tool/ArtifactToolManager.cppm`
- 事実: ツールバーから`ToolType::TrackPoint`を選択できても、`ArtifactToolManager::toolName()`に対応ケースがなく、サービス経由の名称が`Unknown`になっていた。
- 価値: UI表示とサービス／自動化側のツール名称を一致させられる。
- 未検証: ツールチップ、サービス名称取得、ビルド。

# 2026-08-02: Viewerツールオーバーレイの表示名はToolType追加時に拡張が必要

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: Brush、Clone、Eraser、ScrubPreviewは入力処理とツールバー導線が存在したが、オーバーレイ名のswitchにケースがなかった。
- 価値: 選択中ツールの状態をViewer上で一貫して確認できる。
- 未検証: 実機での各ツール切替表示、ビルド。

# 2026-08-02: Motion Tracker作成後はTrackPointを自動選択すると導線が連続する

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- 事実: レイヤーメニューからトラッカーを作成しても、作成完了後のアクティブツールは変更されていなかった。
- 価値: 動画レイヤー選択→トラッカー作成→ポイント配置の操作を途切れずに進められる。
- 未検証: 実機での選択状態、既存トラッカー時の挙動、ビルド。

# 2026-08-02: Composition Editorのツール表示もToolType追加時の同期対象

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: Viewerオーバーレイとツールバーで表示可能なTrackPoint等が、Composition Editorのモードボタンでは汎用の`Tool`表示になっていた。
- 価値: エディタ上部のモード表示と実際のアクティブツールを一致させられる。
- 未検証: 実機での各ツール切替、ビルド。

# 2026-08-02: リグ系ツールもComposition Editorのモード表示へ反映する

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: RigSelect、RigWeight、Puppet、MotionSketchはToolTypeと入力処理が存在する一方、Composition Editorのモードボタンでは汎用表示にフォールバックしていた。
- 価値: リグ編集ツールの選択状態をエディタ上部で識別できる。
- 未検証: 実機での表示、ビルド。

# 2026-08-02: Render QueueのFarm設定はサービスにあるため状態表示を再利用できる

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`, `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- 事実: RenderQueueServiceはFarm有効状態とワーカー数を公開していたが、Queue Managerのサマリーには表示されていなかった。
- 価値: ネットワーク／Farmレンダーの設定状態を、キュー画面を離れず確認できる。
- 未検証: Farm有効時の表示、ワーカー数変更後の更新、ビルド。

# 2026-08-02: Render FarmのRPC稼働状態も既存Service APIから表示できる

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`, `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- 事実: Queue Serviceは`isFarmRpcServerRunning()`を公開していたが、Queue Managerの状態表示ではワーカー数のみだった。
- 価値: Farmが有効でもRPC待受が停止している状態を、キュー画面で区別できる。
- 未検証: RPC起動／停止時の表示更新、ビルド。

# 2026-08-02: EffectServiceとInspectorカタログの差分監査で12件の登録漏れを発見

- 関連: `Artifact/src/Service/ArtifactEffectService.cppm`, `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- 事実: Serviceの利用可能エフェクト一覧に存在するStroke、Inner Shadow、Bevel、Satin、Rim Light、Radial Blur、Linear Wipe、Kaleidoscope、Dithering、Kuwahara、Radio Waves、GrayscaleがInspectorカタログから漏れていた。
- 価値: 既存エフェクトを再実装せず、検索・カテゴリ・追加UIから発見できるようにした。
- 未検証: 各項目の検索結果、追加操作、プロパティ表示、ビルド。

# 2026-08-02: 比較表の段落・ステンシル評価を現行コードへ同期

- 関連: `docs/analysis/CROSS_APP_COMPARISON_2026-08-01.md`, `Artifact/src/Layer/ArtifactTextLayer.cppm`, `ArtifactCore/include/Layer/BlendModeInfo.ixx`
- 事実: TextLayerは水平／垂直揃え、Wrap、Leading、Paragraph Spacingを保存・Property Group・描画へ反映し、BlendModeはStencil Alpha/LumaとSilhouette Alpha/LumaをCPU/GPU経路へ持っていた。
- 価値: 未着手扱いの比較表が実装状況を過小評価しないようにした。
- 未検証: 実機での各UI操作とビルド。

# 2026-08-02: 編集・変形ToolTypeもComposition Editorの表示マッピングが必要

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: Move、Scale、Rotation、Zoom、Ripple、Rolling、Slip、Slideはツールとして定義されていたが、モードボタンの表示switchに未登録だった。
- 価値: タイムライン編集系とビューポート変形系の現在状態を同じUIで確認できる。
- 未検証: 実機での表示、ビルド。

# 2026-08-02: 比較表の2Dポイントトラッカー評価を現行実装へ同期

- 関連: `docs/analysis/CROSS_APP_COMPARISON_2026-08-01.md`, `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: 2DポイントトラッカーはNCCベースの前後追跡、TrackPointツール表示、追跡結果の適用導線まで実装済みであり、比較表の評価を★★★★☆へ更新した。総合トラッキング評価も★★☆☆☆から★★★☆☆へ更新した。
- 価値: 実装済みのUI・追跡操作を比較表上で未実装扱いしない。
- 未検証: 実機での追跡精度、結果適用、ビルド。

# 2026-08-02: Stabilizerの特徴点応答式を修正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: 旧式は水平・垂直勾配の二乗和だけを使い、`dx * dy - (dx + dy)^2` を判定していたため、正のコーナー応答を得られなかった。局所輝度勾配の構造テンソルからHarris応答を計算する形へ置き換えた。
- 価値: 既存Stabilizerの特徴点検出が実際に候補点を返せる状態になる。
- 未検証: 実映像での検出数、追跡精度、ビルド。

# 2026-08-02: Stabilizerの追跡途中トラック無効化を修正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: フレーム総数を保持する`processedFrames_`と、追跡途中の位置数を比較していたため、全フレーム処理前にトラックが無効化されていた。追跡完了後に全フレーム分の位置が揃ったトラックだけを有効化するよう変更した。
- 価値: 修正した特徴点検出結果がモーション推定まで到達できる。
- 未検証: 実映像での連続追跡、モーション推定、ビルド。

# 2026-08-02: Stabilizerのモーション推定を平行移動だけからSimilarity変換へ拡張

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`, `ArtifactCore/include/Video/Stabilizer.ixx`
- 事実: 旧実装は特徴点の平均移動量だけを使い、`rotation` と `scale` を常に初期値のまま返していた。中心化した点対から回転・スケール・平行移動を推定し、既存パラメータの有効／無効設定を反映するようにした。
- 価値: 回転や拡大縮小を含むカメラ揺れを、既存のフレーム処理経路へ渡せる。
- 未検証: 実映像での推定方向、境界補間、ビルド。

# 2026-08-02: Stabilizer再実行時の平滑化結果蓄積を防止

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: `stabilize()`／`smoothMotions()`の再実行時に`smoothedMotions_`を初期化していなかったため、前回結果が後ろに蓄積していた。
- 価値: 同じ入力を再処理した際に、フレーム番号と平滑化結果の対応が崩れない。
- 未検証: 再実行操作、ビルド。

# 2026-08-02: Stabilizer特徴点検出でmaxFeatures/minDistanceを適用

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`, `ArtifactCore/include/Video/Stabilizer.ixx`
- 事実: 検出候補を応答値でソートし、`FeatureDetectionParams.maxFeatures` と `minDistance` による非最大抑制を行うようにした。従来は候補点数と密集度が設定値に制限されていなかった。
- 価値: 特徴点追跡の計算量と点分布を既存パラメータから制御できる。
- 未検証: 実画像での検出数、追跡速度、ビルド。

# 2026-08-02: Stabilizer追跡結果の座標参照先を修正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: マッチング処理は最良座標を検出候補配列の末尾へ追加していたが、トラック更新側は候補配列の先頭を参照していた。追加されたマッチ結果領域を使うように修正した。
- 価値: モーション推定へ渡る各トラックが、実際にマッチしたフレーム位置を保持する。
- 未検証: 実映像での追跡精度、ビルド。

# 2026-08-02: Stabilizerの成功判定を実推定結果に同期

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: 特徴点やフレームモーションが空でも`stabilize()`が完了扱いになり得た。特徴点・モーションが存在しない場合は失敗を返し、`totalFeatures_`も有効トラック数へ更新するようにした。
- 価値: 呼び出し側が空の安定化結果を成功と誤認しない。
- 未検証: 特徴点不足時のUI表示、実映像、ビルド。

# 2026-08-02: AutoMosaicの画像端領域クリップを修正

- 関連: `Artifact/src/Effects/AutoMosaicEffect.cppm`
- 事実: 画像外へはみ出す手動／顔検出領域を、x/yを0へ丸めるだけで幅・高さを計算していたため、負座標領域の右端・下端が過剰に処理され得た。QRectの画像矩形との交差領域をOpenCV ROIへ渡すようにした。
- 価値: 画像端のモザイク領域が指定範囲どおりに適用され、ROI範囲外アクセスのリスクを下げる。
- 未検証: 画像端・負座標の手動領域、ビルド。

# 2026-08-02: Linear WipeのSoftnessをCPU/GPU経路へ反映

- 関連: `Artifact/src/Effects/LinearWipe/LinearWipeEffect.cppm`
- 事実: `Softness`プロパティは保持・公開されていたが、CPU実装は固定の境界式、GPU実装はangleとfeatherのみを使用していた。両経路でSoftnessを境界遷移幅として使うようにした。
- 価値: CPUフォールバックとGPU処理でワイプ境界の調整結果が一致し、既存プロパティが実際に機能する。
- 未検証: CPU/GPU境界一致、実機表示、ビルド。

# 2026-08-02: MosaicのShape Mode平均領域をCPU/GPUで同期

- 関連: `Artifact/src/Effects/Mosaic/MosaicEffect.cppm`
- 事実: CPUはセル内のダイヤモンド領域だけを平均していたが、GPUはセル全体の平均色をダイヤモンドへ適用していた。GPUも同じダイヤモンド領域を平均対象にした。
- 価値: Compute経路とCPUフォールバックでShape Modeの見た目が一致する。
- 未検証: CPU/GPU比較、実機表示、ビルド。

# 2026-08-02: Stroke GPU経路の色チャンネル順を修正

- 関連: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`
- 事実: GPU経路はRGBAテクスチャへ直接出力する一方、パラメータ色をBGR順で渡していた。GPU用のストローク色をR/G/B/A順に修正した。
- 価値: GPU処理で赤・青が入れ替わる問題を防ぎ、表示色を指定色に一致させる。
- 未検証: CPU/GPU色比較、実機表示、ビルド。

# 2026-08-02: Bevel/Satin GPU経路の色チャンネル順を修正

- 関連: `Artifact/src/Effects/Bevel/BevelEffect.cppm`, `Artifact/src/Effects/Satin/SatinEffect.cppm`
- 事実: GPUのRGBAテクスチャへ渡すハイライト／シャドウ／サテン色がBGR順だった。GPUパラメータをR/G/B/A順へ揃えた。DropShadow等の既にRGBA順だった経路は変更していない。
- 価値: BevelとSatinでも指定色とGPU出力色が一致する。
- 未検証: CPU/GPU色比較、実機表示、ビルド。

# 2026-08-02: Stabilizerの退化スケールを下限保護

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: 対応点が同一点へ潰れるケースではSimilarity推定のスケールが0になり、既存の逆変換でゼロ除算し得た。推定スケールを正の下限でクランプした。
- 価値: 退化した追跡結果でも処理経路をNaN／無限大へ進めない。
- 未検証: 退化入力、実映像、ビルド。

# 2026-08-02: Drop Shadow GPUぼかし半径をSoftnessへ同期

- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`
- 事実: CPU経路は`ceil(softness * 2.5)`相当のカーネルを使う一方、GPU経路は常に±4ピクセルの9×9サンプルだった。GPUもSoftnessから最大サンプル半径を計算するようにした。
- 価値: 大きなSoftness設定がGPU経路でも実際のぼかし範囲へ反映される。
- 未検証: GPU性能、CPU/GPU見た目比較、ビルド。

# 2026-08-02: AutoMosaicの重複領域を統合

- 関連: `Artifact/src/Effects/AutoMosaicEffect.cppm`
- 事実: 顔検出領域と手動領域、または複数の検出領域が重なる場合に同じ画素へモザイク処理を複数回適用していた。画像内へクリップした領域を重なり／隣接単位で統合してから一度だけ処理するようにした。
- 価値: 重複領域の過剰処理を避け、処理量と結果の不安定さを抑える。
- 未検証: 複数顔・手動領域の重なり、ビルド。

# 2026-08-02: AutoMosaic領域統合を推移的に完了

- 関連: `Artifact/src/Effects/AutoMosaicEffect.cppm`
- 事実: 領域追加時の一段階統合だけでは、第三領域が複数の既存領域を橋渡しするケースで重なりが残り得た。統合後も重なり／隣接がなくなるまで再走査するようにした。
- 価値: 顔検出と手動領域が複雑に連結する場合も一つの処理領域へまとめられる。
- 未検証: 複雑な重なりパターン、ビルド。

# 2026-08-02: PuppetToolのPinRecordへweightフィールドを追加

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: `pinWeight()`、`setPinWeight()`、エンジンへのピン変換、表示処理が`PinRecord::weight`を参照していたが、レコード定義にフィールドが存在しなかった。
- 価値: Puppet Toolの既存weight APIが型定義と一致し、コンパイル可能なデータモデルになる。
- 未検証: ビルド、weight変更の実機反映。

# 2026-08-02: PuppetToolのピン削除をエンジンへ同期

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: `removePin()`はUI側の`LayerPins::pins`だけを削除し、OpenCV Puppet Engine内のピンを残していた。削除時にエンジンをリセットし、次回変形で残存ピンを再登録しないようにした。
- 価値: 削除済みピンが変形結果へ影響し続ける問題を防ぐ。
- 未検証: ピン削除後の再変形、ビルド。

# 2026-08-02: PuppetToolの再バインド失敗を再試行可能に修正

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: `toQImage()`またはRGBA変換が失敗しても`needsRebind`を解除していたため、以後の変形で再バインドを試行しなかった。画像バインド成功時だけフラグを解除するようにした。
- 価値: 一時的な画像取得失敗から、次回変形で自動復旧できる。
- 未検証: 画像取得失敗／復旧、ビルド。

# 2026-08-02: MotionSketchのUndoフレームレートをコンポジションへ同期

- 関連: `Artifact/src/Tool/ArtifactMotionSketchTool.cppm`
- 事実: Undoスナップショットのフレーム番号と復元用`RationalTime`が常に24fpsで処理されていた。コンポジションの実fpsで保存・復元するようにした。
- 価値: 24fps以外のコンポジションでMotion SketchをUndo/Redoしても、キーフレーム位置がずれない。
- 未検証: 30/60fpsでのUndo/Redo、ビルド。

# 2026-08-02: Command PaletteのAdd Maskスタブを実装

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: `palette.dummy.addMask`は実行ログを出すだけだった。選択中レイヤーのsourceSizeに合わせた矩形MaskPathを作成し、LayerMaskを追加して既存Undo変更通知を呼ぶ処理へ置き換えた。
- 価値: コマンドパレットから選択レイヤーへフルソース矩形マスクを追加できる。
- 未検証: UI実行、Undo復元、ビルド。

# 2026-08-02: Command PaletteのAdd MaskへUndo復元を追加

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: 追加処理を専用`UndoCommand`へ移し、既存マスク一覧をbefore/afterとして保存してUndo/Redoできるようにした。UndoManagerがない場合は直接適用する。
- 価値: コマンドパレット操作が既存のマスク編集履歴と同じく可逆になる。
- 未検証: UI実行、Undo/Redo、ビルド。

# 2026-08-02: Command PaletteのAdd MaskコマンドIDを正式化

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: 実処理化後も`palette.dummy.addMask`という仮IDを使っていたため、`palette.layer.addMask`へ整理した。
- 価値: コマンド一覧・ログ・将来のショートカット連携で、実機能として識別できる。
- 未検証: 既存設定からのID移行、ビルド。

# 2026-08-02: Command PaletteのAboutダミーを情報ダイアログ化

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: `About Command Palette`は実行ログだけを出すダミーだった。パレットの用途を説明する情報ダイアログを表示する正式コマンドへ変更した。
- 価値: パレット自身の機能説明へユーザーが到達できる。
- 未検証: UI表示、ビルド。

# 2026-08-02: Point Tracker結果適用後のTransform通知を追加

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: 追跡キーフレームを書き込んだ後に`setDirty(LayerDirtyFlag::Transform)`と`changed()`を呼んでいなかった。Nullレイヤー生成時も選択レイヤー書き出し時も、適用後にTransform変更を通知するようにした。
- 価値: UI再描画・保存判定など既存の変更監視経路へ追跡結果が届く。
- 未検証: 適用後の表示・保存、ビルド。

# 2026-08-02: Point Tracker適用時の非有限値を遮断

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`, `ArtifactCore/src/Tracking/MotionTracker.cppm`
- 事実: Coreのexport結果を変更せず、Artifact側で時間・X/Yが有限値のキーフレームだけを採用するようにした。
- 価値: 追跡失敗や壊れた入力がNaN／無限大のTransformキーフレームとして保存されるのを防ぐ。
- 未検証: 不正結果の適用、ビルド。

# 2026-08-02: ImageLayerのtoQImage連番フレーム更新を追加

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 連番画像のキャッシュ更新は`draw()`経路に限られており、`toQImage()`を直接呼ぶ処理では前フレームの画像を返し得た。現在フレームと連番fpsから対象フレームを解決し、`toQImage()`でも更新するようにした。
- 価値: 連番画像を使うサムネイル・編集ツール・変形処理が現在フレームを参照する。
- 未検証: 連番サムネイル、Puppet／変形経路、ビルド。

# 2026-08-02: ImageLayerのcurrentFrameBuffer連番同期を追加

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: GPU／エフェクト経路が`currentFrameBuffer()`を直接参照すると、`draw()`や`toQImage()`を経由せず前フレームの連番バッファを使う可能性があった。連番時だけ既存の`toQImage()`によるフレーム解決を通すようにした。
- 価値: CPU画像経路とバッファ経路で連番の現在フレームがずれるリスクを減らす。
- 未検証: GPUエフェクト適用中の連番切替、ビルド。

# 2026-08-02: Corner PinのCPUホモグラフィワープを実装

- 関連: `Artifact/src/Effect/ArtifactCornerPinEffect.cppm`
- 事実: 4点からホモグラフィを計算していたが、出力画像へ適用していなかった。既存のRGBA32FバッファをOpenCVの`warpPerspective`で逆向きサンプリングし、透明境界で結果を生成するようにした。
- 価値: Corner Pinの8点プロパティが実際の画像変形へ反映される。
- 未検証: 極端な四辺形・退化した4点、GPU経路、ビルド。

# 2026-08-02: Noise Generatorの多層ノイズ生成を実装

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: 生成時に`scale`、`octaves`、`frameNumber`を使わず、毎回の単純乱数をRGBA各チャンネルへ生成していた。フレーム依存の決定的な乱数グリッドを複数解像度で補間・合成し、グレースケールノイズとして出力するようにした。
- 価値: 同じフレームの再生成結果が安定し、スケールとオクターブ設定がノイズの粒度・複雑さへ反映される。
- 未検証: 実UIからのパラメータ接続、Perlin／Simplex固有の品質、ビルド。

# 2026-08-02: Noise Generatorの内部グリッド上限を追加

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: 多層化により`scale`とオクターブ数の組み合わせ次第で、出力解像度を超える巨大な乱数グリッドを確保し得た。各オクターブの内部グリッド寸法を最大512に制限した。
- 価値: 高解像度出力や高オクターブ設定でも、不要なメモリ急増を防ぐ。
- 未検証: 高負荷設定での画質と実行時間、ビルド。

# 2026-08-02: Gradient／Shape Generatorの入力境界を補強

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: 無効な出力サイズではOpenCV行列生成前に処理を止め、Gradientの半径除算は最小値を持たせ、Shapeの負のサイズ設定は0へクランプするようにした。
- 価値: 空画像や異常なUI入力での除算・不正サイズ・負領域描画を防ぐ。
- 未検証: 無効値を直接渡す呼び出し経路、ビルド。

# 2026-08-02: Radio Wavesの存続波だけを走査

- 関連: `Artifact/src/Effects/Generate/RadioWavesEffect.cppm`
- 事実: これまでは現在時刻までに発生した全ウェーブを走査し、寿命切れを内側で破棄していた。現在時刻と寿命から存続可能な発生番号を先に絞り、周波数・寿命の安全値も使うようにした。
- 価値: 長時間プレビューでも、寿命切れウェーブの累積走査による計算量増加を防ぐ。
- 未検証: 高周波・長時間プレビュー、ビルド。

# 2026-08-02: Liquifyのバイリニア座標順を修正

- 関連: `Artifact/src/Effects/Liquify/LiquifyEffect.cppm`
- 事実: CPUサンプラーでx方向隣接点とy方向隣接点が逆に割り当てられ、補間係数と異なる軸の画素を混合していた。`c10`を(x1,y0)、`c01`を(x0,y1)へ修正した。
- 価値: Push／Pinch／BloatなどCPU Liquifyの境界補間が意図した二次元座標に一致する。
- 未検証: 各ブラシ種別の画質、GPU Pushとの一致、ビルド。

# 2026-08-02: AIDSLのselect filterをASTへ保持

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `parseFilter()`が存在していたが、`select layers ... where`の解析結果では常に`nullptr`を代入していた。where以降のトークンがある場合に既存パーサーを呼び出し、`SelectLayersCommand::filter`へ保存するようにした。
- 価値: レイヤー選択DSLの条件式が解析段階で失われず、後続のcompile／execute実装が利用できる。
- 未検証: 複数条件の実行評価、compile／executeの未実装部分、ビルド。

# 2026-08-02: AIDSLの比較式評価を実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `PropertyRef`の存在確認と`Literal`の常時trueだけで、`BinaryExpr`は常にtrueを返していた。プロパティ／リテラル値を解決し、数値の型差を吸収した比較、文字列比較、正規表現一致、無効な式のfalse判定を追加した。
- 価値: 解析されたselect filterが条件式として評価可能になる。既存のネスト表現による複数条件も比較結果のAND相当として評価できる。
- 未検証: DSL実行経路、正規表現の性能、ビルド。

# 2026-08-02: Liquify GPU Pushの変位方向をCPUへ同期

- 関連: `Artifact/src/Effects/Liquify/LiquifyEffect.cppm`
- 事実: CPUは`q + direction * strength`をサンプリングしていたが、GPU HLSLは`q - direction * strength`を使っていた。GPUのPush方向をCPUと同じ加算方向へ修正した。
- 価値: GPU／CPUフォールバックでPushブラシの見た目が反転しない。
- 未検証: GPU実機での方向一致、他ブラシ種別、ビルド。

# 2026-08-02: Lens DistortionのCPUズーム境界を補強

- 関連: `Artifact/src/Effects/LensDistortion/LensDistortionEffect.cppm`
- 事実: ズームsetterは0以下を受け入れ、CPU経路ではその値で除算していた。GPU経路と同じ`0.001`を下限としてCPU計算にも適用した。
- 価値: CPUフォールバック時の無限値・NaN生成を防ぎ、GPU／CPUで最低ズーム条件を一致させる。
- 未検証: ズーム0以下のUI入力、ビルド。

# 2026-08-02: KaleidoscopeのCPUサンプリングをGPUへ同期

- 関連: `Artifact/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm`
- 事実: CPU経路は変形後座標を最近傍丸めしていたが、GPU経路はバイリニア補間を使っていた。既存の`sampleBilinear`でCPUも補間するようにした。
- 価値: CPUフォールバックとGPU実行で、Kaleidoscopeのエッジ品質・縮小時の見た目が一致する。
- 未検証: GPU／CPU画質の実機比較、ビルド。

# 2026-08-02: KaleidoscopeのFeather半径をGPUへ同期

- 関連: `Artifact/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm`
- 事実: CPUは中心からの距離の1.5倍をFeather基準にしていたが、GPUは中心から画像四隅までの最大距離を基準にしていた。CPUも画像範囲から最大距離を算出するようにした。
- 価値: Feather境界の位置がCPU／GPU経路で一致する。
- 未検証: 非中央中心点・高Feather値での実機比較、ビルド。

# 2026-08-02: Find Edges GPUのアルファ保持を追加

- 関連: `Artifact/src/Effects/FindEdges/FindEdgesEffect.cppm`
- 事実: GPU HLSLはエッジ値をRGBへ出力する際にアルファを常に1へ設定していた。入力中心画素のアルファを保持し、CPU経路と同じ透明度扱いにした。
- 価値: 透明レイヤーへFind Edgesを適用しても、GPU経路で不透明化しない。
- 未検証: GPU／CPUのエッジ強度差、実機描画、ビルド。

# 2026-08-02: Find EdgesのCPU／GPU強度差を把握

- 関連: `Artifact/src/Effects/FindEdges/FindEdgesEffect.cppm`
- 事実: CPUはLaplacian＋画像全体の正規化＋元画像とのブレンド、GPUはSobel強度の飽和出力であり、アルファ修正後もエッジ強度のアルゴリズム差は残る。
- 価値: 今後のGPU parity対応で、単なる係数調整では解消できない差分を明確化できる。
- 未検証: 実画像での差分量、GPU側の二段階正規化設計。

# 2026-08-02: HexGrid GPUの入力クランプをCPUへ同期

- 関連: `Artifact/src/Effects/Rasterizer/HexGridEffect.cppm`
- 事実: CPUはセルサイズ4、線幅0.5を下限としていたが、GPU HLSLは受け取った値をそのまま除算・判定に使っていた。GPU側でも同じ下限を適用した。
- 価値: 異常値や未同期パラメータでも、CPUフォールバックと同じグリッド形状になり、ゼロ除算リスクを抑える。
- 未検証: GPU実機での異常値入力、ビルド。

# 2026-08-02: Vignetteの偏心中心向け半径を補正

- 関連: `Artifact/src/Effects/Rasterizer/VignetteEffect.cppm`
- 事実: 半径基準を常に中心から原点までの距離としていたため、中心点を右下へ移動すると反対側の画面領域を十分に覆えなかった。CPU／GPU双方で中心から四隅までの最大距離を使うようにした。
- 価値: 偏心したVignetteでも画面全体に一貫したフェザー境界を生成する。
- 未検証: 中心点端部・半径0・高Feather値、ビルド。

# 2026-08-02: Radial ShadowのCPU合成をGPUへ同期

- 関連: `Artifact/src/Effects/RadialShadow/RadialShadowEffect.cppm`
- 事実: CPUはRGBAへ`cv::addWeighted`を適用し、色チャンネル順もGPUと異なっていた。CPUも入力RGBへ影色×アルファを加算し、入力アルファへ影アルファを加算して0〜1へクランプするようにした。
- 価値: CPUフォールバック時の色反転・アルファの過剰加算を防ぎ、GPU経路と結果の定義を揃える。
- 未検証: 透明画像・高不透明度での実機比較、ビルド。

# 2026-08-02: Inner Shadowのアルファ合成を修正

- 関連: `Artifact/src/Effects/InnerShadow/InnerShadowEffect.cppm`
- 事実: 影係数に`(1 - source alpha)`を掛けていたため、不透明なソース内部では影が消え、実質的に外側へ寄った合成になっていた。ソースアルファ内で影色を補間し、出力アルファは元のアルファを保持するようにした。
- 価値: Inner Shadowが不透明レイヤーの内側へ正しく表示される。
- 未検証: 透明境界・ぼかし量・GPUフォールバックでの実機表示、ビルド。

# 2026-08-02: Drop ShadowのCPU影色チャンネル順を修正

- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`
- 事実: RGBA32F画像の`cv::Mat`へ影色を`B,G,R,A`順で書き込んでいたため、CPU経路だけ赤青が入れ替わっていた。RGBA順で影マットを生成するようにした。
- 価値: CPUフォールバックとGPU経路で影色が一致する。
- 未検証: GPU／CPU実機比較、半透明影、ビルド。

# 2026-08-02: Drop Shadow GPUの画像外サンプルを透明化

- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`
- 事実: CPUのオフセット画像は範囲外を0で初期化していたが、GPUの`alphaAt`は端画素をクランプしていた。GPUも範囲外をアルファ0として扱うようにした。
- 価値: 影が画像端で不自然に反復する問題を抑え、CPU経路と境界挙動を一致させる。
- 未検証: 大きなオフセット・ぼかし半径での実機比較、ビルド。

# 2026-08-02: Inner Shadowの影色チャンネル順を修正

- 関連: `Artifact/src/Effects/InnerShadow/InnerShadowEffect.cppm`
- 事実: RGBA32Fの`cv::Mat`ビューへ影色をBGR順で格納していたため、CPU経路だけ赤青が入れ替わっていた。影マットをRGBA順で生成するようにした。
- 価値: Inner ShadowのCPUフォールバックで指定色が正しく表示される。
- 未検証: 半透明境界・各色での実機表示、ビルド。

# 2026-08-02: SatinのCPU色順と合成係数をGPUへ同期

- 関連: `Artifact/src/Effects/Satin/SatinEffect.cppm`
- 事実: CPUサテンマットがBGR順で、さらにサテンアルファへソースアルファを事前乗算していたため、色が反転しGPUより弱くなっていた。RGBA順へ修正し、合成時のアルファ係数をGPUと同じにした。
- 価値: CPUフォールバック時の色とサテン強度がGPU経路と一致する。
- 未検証: 半透明ソース・反転・ぼかし量での実機比較、ビルド。

# 2026-08-02: StrokeのCPU影色チャンネル順を修正

- 関連: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`
- 事実: RGBA32FのStrokeマットへ影色をBGR順で格納していたため、CPU経路だけ赤青が入れ替わっていた。RGBA順でマットを生成するようにした。
- 価値: StrokeのCPUフォールバックで指定色がGPU経路と一致する。
- 未検証: 各色・半透明ソースでの実機表示、ビルド。

# 2026-08-02: Stroke／Satin GPUの境界サンプルを透明化

- 関連: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`, `Artifact/src/Effects/Satin/SatinEffect.cppm`
- 事実: CPU側の膨張・オフセット用アルファ画像は範囲外を0としていたが、GPUの`alphaAt`は端画素をクランプしていた。両シェーダーで範囲外をアルファ0にした。
- 価値: 画像端でストローク／サテンが不自然に反復する差異を抑える。
- 未検証: 大きな幅・距離・ソフトネスでの実機比較、ビルド。

# 2026-08-02: Glow CPUのRGBA輝度係数を修正

- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`
- 事実: RGBA32FのRGBチャンネルを`cv::COLOR_BGR2GRAY`へ渡していたため、赤と青の輝度寄与が逆になっていた。RGBA順の0.299／0.587／0.114係数で直接輝度を計算するようにした。
- 価値: CPUフォールバック時のGlow抽出マスクがGPU・標準的なRGBA輝度定義と一致する。
- 未検証: 赤青単色入力でのGPU／CPU比較、ビルド。

# 2026-08-02: Glow CPUのハイライトマスクをGPUへ同期

- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`
- 事実: CPUは閾値を引いた後に`1/(1-threshold)`で正規化していたが、GPUは閾値を超えた輝度へGainを直接適用していた。CPUも閾値超過値へGainを適用し、0〜1へ飽和させるようにした。
- 価値: CPUフォールバックとGPUのGlow発光量が同じ定義になる。
- 未検証: Gain・低輝度入力での実機比較、ビルド。

# 2026-08-02: GlowマスクのOpenCVクランプ型を明示

- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`
- 事実: ハイライトマスクの上限処理をOpenCVの`Scalar`形式へ揃え、既存の行列演算と同じ型経路で明示した。
- 価値: OpenCVオーバーロードの暗黙変換に依存せず、RGBA32Fマスクの上限処理を安定させる。
- 未検証: ビルド。

# 2026-08-02: Luma Keyの適用時閾値をクランプ

- 関連: `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`
- 事実: UI経由のproperty setter以外から閾値が設定されると、適用時のluma判定範囲が0〜1を外れる可能性があった。low／highを並べ替えた後、適用直前にも0〜1へクランプするようにした。
- 価値: 直接API利用や復元データに異常値があっても、キーアルファ計算が安定する。
- 未検証: 不正閾値の復元・実画像、ビルド。

# 2026-08-02: Chroma Keyの適用時パラメータを正規化

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`
- 事実: 直接APIや復元データからsimilarity／smoothness／spillReductionへ範囲外値が入ると、距離判定やスピル係数が不安定になり得た。適用時にRGB最大距離`√3`、最小softness、spill 0〜1へクランプするようにした。
- 価値: UI外からの不正入力でもキーアルファとスピル除去が安定する。
- 未検証: 範囲外値の復元・実画像、ビルド。

# 2026-08-02: Hue/SaturationのCPU色空間変換をRGBAへ修正

- 関連: `Artifact/src/Effects/ColorCorrection/HueAndSaturation.cppm`
- 事実: RGBA32FのRGBチャンネルをBGR用のHSV変換へ渡していたため、CPU経路で赤青が入れ替わって色相・彩度が変化していた。RGB2HSV／HSV2RGBへ変更した。
- 価値: CPUフォールバック時の色相・彩度結果が入力のRGBA順とGPU経路に一致する。
- 未検証: 赤青単色・色相ラップ・Colorizeでの実機比較、ビルド。

# 2026-08-02: Displacement Mapのクランプ補間を修正

- 関連: `Artifact/src/Effects/DisplacementMap/DisplacementMapEffect.cppm`
- 事実: 画像外座標で整数インデックスだけをクランプし、補間係数は負値・1超過のままだったため、端画素から外側へ外挿していた。サンプリング座標全体を画像範囲へクランプしてから補間するようにした。
- 価値: 非Wrapモードの強い変位でも、境界が安定して端画素へクランプされる。
- 未検証: 大きな負方向・正方向変位、Wrapモード、ビルド。

# 2026-08-02: Render QueueプリセットのAudio分類を実装

- 関連: `Artifact/src/Render/ArtifactRenderQueuePresets.cppm`
- 事実: `Audio`カテゴリが常に空を返し、カスタムのWAV／MP3／AAC／PCMプリセットも分類できなかった。コンテナとコーデックを正規化して音声形式を判定し、Videoも既知の動画形式だけを返すようにした。
- 価値: プリセット選択UIで音声プリセットを正しく絞り込め、CSS／HTMLなどを動画として誤分類しない。
- 未検証: 音声プリセットUI、FFmpeg書き出しとの接続、ビルド。

# 2026-08-02: Generator基底の出力サイズ・フレーム範囲を正規化

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: `setOutputSize()` が0以下のサイズを保持し、`setFrameRange()` が逆順範囲をそのまま保持していた。サイズを最小1へ、範囲を昇順へ正規化した。
- 価値: 生成処理へ渡る基本パラメータが不正にならず、後続のバッファ確保・フレーム判定が安定する。
- 未検証: UI外からの不正値入力、ビルド。

# 2026-08-02: Generator固有パラメータの入力正規化

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: Noiseのscale／octaves、Shapeのsizeがsetter経由では無制限で、NaNや極端な値が内部に保持され得た。生成処理の想定範囲に合わせて有限値・範囲を正規化した。
- 価値: UI外の設定復元やスクリプト入力でも、生成処理のグリッド計算・形状サイズが安定する。
- 未検証: NaN／無限値の入力、実画像生成、ビルド。

# 2026-08-02: Noise GeneratorのWhiteNoiseモードを接続

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: `noiseType_`はログ出力以外で参照されず、WhiteNoiseを選んでも補間済みの低周波ノイズになっていた。WhiteNoiseでは出力解像度の乱数場を直接使うようにした。
- 価値: UI上のWhiteNoise選択が実際の生成結果へ反映される。乱数seedは従来同様フレームとoctaveから決定論的に生成する。
- 未検証: 各NoiseTypeの実画像比較、GPU連携、ビルド。

# 2026-08-02: Gradient GeneratorのConicモードを接続

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: `GradientType::Conic`がRadialと同じ分岐へ入り、角度方向のグラデーションを生成していなかった。中心周りの角度を0〜1へ正規化する処理を追加した。
- 価値: UIで選択できるConicグラデーションが実際の出力へ反映される。
- 未検証: 角度の開始位置・境界、GPU連携、ビルド。

# 2026-08-02: Generatorの列挙型パラメータを正規化

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: Gradient／Noise／Shapeの各setterが外部から不正な列挙値をそのまま保持していた。列挙範囲へクランプしてから内部状態へ格納するようにした。
- 価値: 復元データやスクリプト由来の不正値で、未定義分岐やログ用配列の範囲外アクセスが起きる可能性を下げる。
- 未検証: 不正enum入力、ビルド。

# 2026-08-02: Clone Generatorの配置パラメータを正規化

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: spacing／radius／grid spacing／spiral rotations／rotation stepがNaNや無限値を保持でき、radiusは負値も許容していた。有限値検証とradiusの非負化を追加した。
- 価値: クローン変換行列の生成へ不正な浮動小数値が伝播する可能性を下げる。
- 未検証: 不正値からの復元、各分布モードの実配置、ビルド。

# 2026-08-02: Clone Generatorの分岐・bounds入力を正規化

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: distribution／transform modeが不正enum値を保持でき、boundsも負値・NaN・無限値を配置計算へ渡せた。列挙範囲と非負有限値へ正規化した。
- 価値: 外部入力による不正分岐やクローン配置の数値破綻を防ぐ。
- 未検証: 不正enum・bounds復元、実配置、ビルド。

# 2026-08-02: Clone GeneratorのSpiralオフセットを反映

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Spiral分布だけoffsetのX/Yを適用せず、Zのみ反映していた。螺旋配置後にX/Yオフセットを適用するよう修正した。
- 価値: 他の分布モードと同様に、Spiralでも全軸の配置オフセットが機能する。
- 未検証: 3D変換順序、各回転設定、ビルド。

# 2026-08-02: Clone GeneratorのPoisson Disk配置を接続

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Random分布の`usePoissonDisk`設定が無視され、通常の独立乱数配置になっていた。指定時は最大64回の候補再試行で、spacingを最小距離として既存位置との重なりを抑えるようにした。
- 価値: Random配置でクローンが局所的に密集する問題を軽減できる。候補が見つからない場合は通常の乱数候補へフォールバックする。
- 未検証: 高密度配置、3D距離、実表示、ビルド。

# 2026-08-02: Clone GeneratorのGrid回転ステップを接続

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Grid2D／Grid3Dでは`rotationStep`を参照せず、設定しても全クローンが同じ向きだった。生成順インデックスに応じたZ回転を追加した。
- 価値: Grid分布でも既存の回転ステップ設定を利用でき、分布モード間の設定挙動が揃う。
- 未検証: 3D変換順序、Grid表示、ビルド。

# 2026-08-02: Clone GeneratorのHexagonal回転ステップを接続

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Hexagonal分布でも`rotationStep`が無視されていた。生成順インデックスに応じた回転を追加した。
- 価値: Hexagonal／Grid／Linearなどで回転ステップ設定の挙動が揃う。
- 未検証: 六角形配置の変換順序、実表示、ビルド。

# 2026-08-02: Clone GeneratorのSplineフォールバック回転を接続

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Spline未設定時のLinearフォールバックでは`rotationStep`が適用されず、Spline設定時と結果が一致しなかった。フォールバックにも生成順回転を追加した。
- 価値: Splineの有無で回転設定が突然失われず、フォールバック時の挙動が通常のLinear分布と揃う。
- 未検証: Spline未設定時の表示、変換順序、ビルド。

# 2026-08-02: Particle EmitterのdeltaTime入力を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: `ParticleEmitter::update()`が負値・NaN・無限値のdeltaTimeを受けると、内部時刻や粒子寿命・速度計算へ不正値が伝播し得た。非有限値または0以下の更新を早期無視するようにした。
- 価値: 再生停止・外部制御・フレーム時間異常時に粒子シミュレーションが壊れにくくなる。
- 未検証: 可変フレームレート、決定論モード、ビルド。

# 2026-08-02: Particle Emitterの発生数境界を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: `emitParticles()`が負のcountや0以下のmaxParticlesを受けても発生可能数を計算していた。早期終了とavailable値の非負化を追加した。
- 価値: 不正なEmission設定で負数の発生処理や容量計算が発生せず、Burst／Continuous双方の挙動が安定する。
- 未検証: 不正なmaxParticles・burstCount、補助粒子、ビルド。

# 2026-08-02: Particle Emitterのゼロ方向フォールバックを追加

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: directionがゼロベクトルの場合、正規化後もゼロのままで発生粒子の速度方向が失われていた。発生方向を既定のY軸へフォールバックするようにした。
- 価値: 無効な方向設定でも、速度やdirection spreadがゼロへ退化しない。
- 未検証: ゼロ方向、spread、ビルド。

# 2026-08-02: Particle Emitterのspread後方向を正規化

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: direction spread適用後のベクトルをそのまま返していたため、spread角度によって方向ベクトル長が変化し、速度スケールまで変動し得た。返却前に正規化した。
- 価値: spreadは方向だけを変え、初速の大きさは既存設定に委ねられる。
- 未検証: 大きなspread、ゼロ方向、ビルド。

# 2026-08-02: Particle Emitterの発生形状寸法を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: Sphere／CircleのradiusとBox／Rectangle／Lineの寸法が負値のまま発生位置計算へ使われ得た。形状ごとの発生位置計算時に非負化した。
- 価値: 不正な寸法でも発生領域が反転せず、設定値異常による予期しない分布を抑える。
- 未検証: 負寸法、各EmitterShape、ビルド。

# 2026-08-02: Particle EmitterのdirectionSpreadを正規化

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: directionSpreadがNaN・無限値・180度超でも角度計算へ渡せた。有限値を0〜180度へクランプしてからspreadを適用するようにした。
- 価値: 異常な入力による不安定な三角関数計算や意図しない反転を防ぐ。
- 未検証: 範囲外spread、ゼロ方向、ビルド。

# 2026-08-02: Particle寿命計算のゼロ除算を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: `updateParticle()`がmaxLifeを直接除数に使っており、0以下・非有限値の粒子ではlifeが不正化し得た。更新時に最小0.001秒の有限寿命へ正規化した。
- 価値: 不正な寿命設定でもNaNがシミュレーションへ伝播せず、粒子の死亡判定が安定する。
- 未検証: maxLife異常値、補助粒子、ビルド。

# 2026-08-02: Particle Continuous発生率を正規化

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: Continuous emissionのrateが負値・非有限値でも発生累積へ加算され、NaN時は粒子数変換へ不正値が伝播し得た。rateを有限非負値へ正規化し、累積値も有限性を確認するようにした。
- 価値: 外部設定異常でContinuous発生が停止・暴走する可能性を抑える。
- 未検証: 極端なrate、maxParticles到達時、ビルド。

# 2026-08-02: Sphere Emitterの一様サンプリングを修正

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: 球体発生の極角を一様サンプリングしていたため、球面密度が極付近へ偏っていた。`cos(phi)`を一様にし、半径も立方根サンプリングする既存処理と組み合わせた。
- 価値: Sphere emitterの発生位置が体積内で一様になる。
- 未検証: 球体分布の統計比較、他EmitterShape、ビルド。

# 2026-08-02: Particle Burst間隔の異常値を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: BurstのintervalがNaNだと比較が成立せず、Burstが発生しなくなっていた。intervalを有限非負値へ正規化し、既存の0秒間隔は維持した。
- 価値: 不正なintervalでもBurstが永久停止せず、設定意図に近いフォールバック動作になる。
- 未検証: NaN／負interval、0秒Burst、ビルド。

# 2026-08-02: Box Fieldの寸法入力を正規化

- 関連: `Artifact/include/Effects/Field/BoxField.ixx`
- 事実: halfExtentとfalloffWidthが負値・非有限値を保持でき、SDF距離・フォールオフ評価へ不正値が入る可能性があった。setterで非負有限値へ正規化した。
- 価値: Box FieldのCPU評価が異常な外部入力でも安定する。
- 未検証: 不正寸法、falloff境界、ビルド。

# 2026-08-02: Linear Fieldの座標入力を正規化

- 関連: `Artifact/include/Effects/Field/LinearField.ixx`
- 事実: startPos／endPosが非有限値を保持でき、direction・length・influence計算へNaNが伝播し得た。setterで各軸を有限値へ正規化した。
- 価値: Linear Fieldの勾配評価が異常な外部入力でも安定する。
- 未検証: NaN／無限座標、ゼロ長区間、ビルド。

# 2026-08-02: Radial Fieldの入力を正規化

- 関連: `Artifact/include/Effects/Field/RadialField.ixx`
- 事実: center／axis／innerRadius／outerRadiusが非有限値や負半径を保持でき、軸投影・半径フォールオフへ不正値が伝播し得た。座標・軸を有限値、半径を非負有限値へ正規化した。
- 価値: Radial FieldのCPU評価が異常な外部入力でも安定する。
- 未検証: ゼロ軸、inner>outer、範囲外値、ビルド。

# 2026-08-02: Spherical Fieldの入力を正規化

- 関連: `Artifact/include/Effects/Field/SphericalField.ixx`
- 事実: center／radius／falloffWidthが非有限値や負値を保持でき、距離・減衰評価へ不正値が伝播し得た。座標を有限値、半径と減衰幅を非負有限値へ正規化した。
- 価値: Spherical FieldのCPU評価が異常な外部入力でも安定する。
- 未検証: 範囲外値、falloff境界、ビルド。

# 2026-08-02: AIDSLのAdd Key構文を実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `add key at 12f opacity = 0`の解析分岐がproperty／valueを格納せず、実質的に空のAddKeyCommandを生成していた。正しいトークン位置を検証し、複数トークンの値も再構成して格納するようにした。
- 価値: AIDSLスクリプトからキーフレーム追加コマンドの入力情報が失われない。
- 未検証: compile実装、複合値、実行経路、ビルド。

# 2026-08-02: AIDSLのRename/Delete/Group構文を接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`, `docs/planned/AI_TOOL_DSL_IMPLEMENTATION_GUIDE_2026-04-05.md`
- 事実: 仕様に記載されたrename selected／delete selected／group layers into構文がスタブで、ASTへ何も追加していなかった。Selectedターゲットとテンプレート／グループ名を各Commandへ格納するようにした。
- 価値: AIDSLスクリプトのトランザクション内外で、これらの編集意図を後段compileへ渡せる。
- 未検証: compile／実行／undo接続、ターゲット範囲、ビルド。

# 2026-08-02: AIDSLの基本Query構文を接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`, `docs/planned/AI_TOOL_DSL_IMPLEMENTATION_GUIDE_2026-04-05.md`
- 事実: `query selected_layers`／`query comp_size`／`query list properties of selected`がスタブで、クエリ配列へ追加されていなかった。対応するQueryNodeを生成し、comp_sizeの任意IDも保持するようにした。
- 価値: DSL解析結果を既存のQuery実行経路へ渡せる。
- 未検証: 実データ取得、選択状態・プロパティ一覧の完全実装、ビルド。

# 2026-08-02: AIDSLのFind/Describe Query構文を接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`, `docs/planned/AI_TOOL_DSL_IMPLEMENTATION_GUIDE_2026-04-05.md`
- 事実: `query find layers where ...`と`query describe layer ...`が未処理で、未知コマンド扱いになっていた。既存のfilter parserとQueryNodeへ接続した。
- 価値: レイヤー検索・記述クエリの解析結果を既存Query実行経路へ渡せる。
- 未検証: filter評価、レイヤー名からIDへの解決、実データ取得、ビルド。

# 2026-08-02: AIDSLのActive Composition Queryを接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: QueryActiveCompの実行ノードは存在したが、`query active_comp`がparserで生成されず未知構文になっていた。既存ノードへ接続した。
- 価値: DSLから現在のComposition情報を問い合わせる導線が成立する。
- 未検証: active compositionの実ホスト同期、実行結果、ビルド。

# 2026-08-02: AIDSLのComp Size省略IDを補完

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `query comp_size`は空IDを照合して常にunknownになっていた。ID省略時は既存comp lookupの先頭IDを対象にするようにした。
- 価値: 仕様どおり引数なしのcomp_size queryでも既知Compositionを対象にできる。
- 未検証: active compの厳密な選択、実寸法取得、ビルド。

# 2026-08-02: AIDSL Transaction compileを実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: TransactionCommand::compileが常にnullptrを返し、トランザクション内の子CommandをActionへ束ねていなかった。TransactionActionを生成し、子compile結果を順序どおり収集するようにした。
- 価値: 個別Commandのcompile実装が追加された際に、transaction単位のActionへ自然に集約できる。
- 未検証: 子Commandのcompile実装、実行・undo、ビルド。

# 2026-08-02: AIDSL filterのAND結合を修正

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: 複数filter条件の`and`をEqで代用していたため、両条件がfalseでも結合結果がtrueになり得た。BinOp::Andを追加し、左から短絡評価する明示的なANDへ変更した。
- 価値: `where a == x and b == y`が期待どおり両条件成立時だけtrueになる。
- 未検証: 複数条件、ネスト、ビルド。

# 2026-08-02: AIDSL値パーサーの境界を修正

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: 空値で`front()`を参照する可能性があり、負の整数も文字列として解析されていた。空値を安全に扱い、符号付き整数を数値として認識するようにした。
- 価値: 不完全な入力でのクラッシュリスクを下げ、`-12`などのproperty valueを正しく扱える。
- 未検証: 不完全構文、符号付き値、ビルド。

# 2026-08-02: AIDSL filterのOR結合を実装

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: filter parserは`and`しか扱えず、条件結合時の演算子も固定だった。BinOp::Orと結合演算子列を追加し、and／orを記述順に評価できるようにした。
- 価値: `where type == "text" or type == "shape"`のような検索条件をASTへ保持できる。
- 未検証: 演算子優先順位、括弧、複合filter、ビルド。

# 2026-08-02: AIDSL FrameExpr::resolveを実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: FrameExpr::resolveの宣言に対する実装がなく、フレーム式を解決できなかった。数値、コンテキスト名、数値文字列を処理し、未解決値は0へフォールバックするようにした。
- 価値: AddKeyなどのフレーム指定を後段compileで解決できる基盤ができる。
- 未検証: コンテキスト式、負フレーム、ビルド。

# 2026-08-02: Generator基底のapply/outputを実装

- 関連: `Artifact/include/Generator/AbstractGeneratorEffector.ixx`, `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: AbstractGeneratorEffector::apply()がログ出力だけで生成処理を呼ばず、生成結果を保持するAPIもなかった。現在フレーム・出力サイズでgenerateContentを呼び、内部バッファをoutput()から参照できるようにした。
- 価値: Solid／Gradient／Noise／Shape Generatorを基底API経由で実際に生成・取得できる。
- 未検証: applyToLayer接続、バッファ再利用、ビルド。

# 2026-08-02: Generator applyへフレーム範囲を反映

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: startFrame／endFrameを保持していたが、範囲外のcurrentFrameでもgenerateContentが実行されていた。apply時に範囲外を早期終了するようにした。
- 価値: Generatorのフレーム範囲設定が実際の生成タイミングへ反映される。
- 未検証: 範囲境界、無効化後のoutput保持、ビルド。

# 2026-08-02: AIDSL InterpreterのTransaction compileを接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: AIDSLInterpreter::compileTransactionも子Commandのcompile呼び出しがコメントアウトされ、常に空のTransactionActionを返していた。子Actionを生成・収集するようにした。
- 価値: Interpreter経由のtransaction compileでも、個別CommandのActionを失わずに保持できる。
- 未検証: 個別Command compile、実行・undo、ビルド。

# 2026-08-02: AIDSLのUseComp compileを実装

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: UseCompCommand::compileが常にnullptrで、名前／IDのComposition解決結果をActionへ渡せなかった。lookupまたは`#` IDを解決し、CommandActionとして返すようにした。
- 価値: Transaction compileからComposition切替意図を保持できる。
- 未検証: ホスト側実行、未解決名、ビルド。

# 2026-08-02: AIDSL SetProperty compileを実装

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: SetPropertyCommand::compileが常にnullptrで、property pathと値を後段へ渡せなかった。空pathを拒否し、CommandActionへpropertyとValueの文字列表現を格納するようにした。
- 価値: Transaction compileからset操作の意図と値を保持できる。
- 未検証: 実ホストのproperty適用、型変換精度、ビルド。

# 2026-08-02: AIDSL AddKey compileを実装

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: AddKeyCommand::compileが常にnullptrで、frame／property／valueが後段へ渡らなかった。CommandActionへ3要素を保持するようにした。
- 価値: Transaction compileからキーフレーム追加意図を失わずに引き渡せる。
- 未検証: 実ホストのkeyframe適用、frame解決、型変換、ビルド。

# 2026-08-02: AIDSL Rename/Delete/Group compileを実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: Rename／Delete／Groupのcompileが常にnullptrで、テンプレート・グループ名・ターゲット情報を後段へ渡せなかった。CommandActionへ各情報を格納するようにした。
- 価値: AIDSL transactionから編集操作の意図を失わずに引き渡せる。
- 未検証: 実レイヤー操作、Specificターゲット、undo、ビルド。

# 2026-08-02: AIDSL SelectLayers compileを実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: SelectLayersCommand::compileが常にnullptrで、filter有無やlookup上の対象数を後段へ渡せなかった。CommandActionへ選択モードと対象数を格納するようにした。
- 価値: Transaction compileでselect操作を表現できる。
- 未検証: filter実評価、実選択状態の同期、ビルド。

# 2026-08-02: AIDSL executeのCommand compile段階を接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: AIDSLInterpreter::execute()はQueryだけを処理し、Commandのcompile結果を確認していなかった。全Commandをlookup付きでcompileし、生成できたAction数を結果へ含めるようにした。
- 価値: 実ホスト適用前でも、スクリプト内Commandの解決可否を実行結果から確認できる。
- 未検証: ホストAction dispatch、実編集、undo、ビルド。

# 2026-08-02: AIDSL dryRunのcompile可否を可視化

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: dryRunは構文サマリーのみで、Commandがcompile可能か確認できなかった。lookup付きcompileを実行せずに行い、compiledActionCountを返すようにした。
- 価値: スクリプト適用前に、未解決Compositionや不正pathによるcompile失敗を把握できる。
- 未検証: 実ホスト適用、型検証、ビルド。

# 2026-08-02: Generator outputキャッシュのサイズ変更を反映

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: setOutputSize()後もoutput()が旧サイズの生成バッファを保持し、再生成前に古い結果を返す可能性があった。サイズ変更時に出力キャッシュを無効化するようにした。
- 価値: output()の内容と現在設定された出力サイズの不一致を防ぐ。
- 未検証: サイズ変更後の再生成、バッファ再利用、ビルド。

# 2026-08-02: Generatorのレイヤー適用経路は既存API未接続

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`, `Artifact/include/Layer/ArtifactAbstractLayer.ixx`
- 事実: `ArtifactAbstractLayer`にはgeneratorを受け取って生成バッファをレイヤーソースへ適用する既存APIがなく、`layerGenerators()`は設定・シリアライズ用の保持領域だった。
- 価値または懸念: concrete layerへの依存や新規イベント経路を追加して接続すると、Generatorの責務を越えて画像バッファ変換・レイヤー所有権まで広げることになる。
- 次に確認すべきこと: Generator出力をレイヤーソースへ渡す正式なRender/Asset APIが定義された段階で、`applyToLayer()`をそのAPI経由で実装する。
- 未検証: 将来の正式API、ビルド。

# 2026-08-02: OCIO入力変換からviewer補正を分離

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: `applyInputTransformToWorkingImage()`のレガシーフォールバックが、source transfer decode／色空間変換後にviewer exposure・gammaを適用していた。viewer補正は表示変換専用の設定であり、入力素材のworking space変換へ混入させるべきではない。
- 価値: OCIOライブラリが使えない環境でも、入力変換が表示状態に依存せず決定的になる。
- 未検証: 実OCIO config、HDR素材、ビルド。

# 2026-08-02: OCIO viewer設定の非有限値を正規化

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: viewer exposure/gamma の setter と JSON復元が `std::clamp` のみで、NaN/Inf入力を有効値として保持する可能性があった。
- 価値: 表示変換時の `pow()` に不正値を渡さず、設定復元後も有限なviewer状態を保証する。
- 未検証: 壊れたJSON、UI入力、ビルド。

# 2026-08-02: TextLayerのスタイル数値を有限値へ正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: font size、stroke width、shadow、tracking、stretch、leading の setter がNaN/Infをそのまま保持し得た。
- 価値: Text Toolの入力・JSON復元経路から不正なレイアウト値がシェーピングや描画へ流れることを防ぐ。
- 未検証: 異常値を含む既存プロジェクト、フォント依存レイアウト、ビルド。

# 2026-08-02: Keyingパラメータの非有限値を防止

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`, `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`
- 事実: Chroma/Luma Keyのプロパティ入力は `std::clamp` 前にNaN/Infを検査しておらず、不正値がキー判定へ流れる可能性があった。
- 価値: キーイングのCPU経路が入力異常時にも有限な閾値で評価される。
- 未検証: 実画像品質、GPU/runtime経路、ビルド。

# 2026-08-02: BatchRendererの範囲境界を正規化

- 関連: `Artifact/src/Render/ArtifactRenderScheduler.cppm`
- 事実: `renderRange()` は逆順FrameRangeでも開始通知・タスク投入へ進み、`renderAroundFrame()` は負のradiusで範囲が反転し得た。
- 価値: バッチ投入前に不正範囲を拒否し、周辺フレーム要求を常に非負radiusとして扱う。
- 未検証: scheduler runtime、巨大なframe番号、ビルド。

# 2026-08-02: BatchRendererの終端フレームオーバーフローを防止

- 関連: `Artifact/src/Render/ArtifactRenderScheduler.cppm`
- 事実: 終端が整数最大値のFrameRangeでは、`f <= lastFrame` の後の `++f` がオーバーフローし、バッチ投入ループが終了しない可能性があった。
- 価値: 終端フレーム投入後に明示的にbreakし、最大値を含む範囲でも有限回で終了する。
- 未検証: 最大値付近の実行、scheduler runtime、ビルド。

# 2026-08-02: RAM Preview失敗理由を状態へ反映

- 関連: `Artifact/src/Render/ArtifactRamPreviewController.cppm`
- 事実: render callbackがfalseを返した際、`frameFailed`シグナルだけが通知され、`RamPreviewState::lastErrorMessage`へ失敗情報が保存されていなかった。
- 価値: UIや診断側がシグナルを取り逃しても、最後の失敗フレームと理由を状態スナップショットから取得できる。
- 未検証: callback失敗、再ビルド、ビルド。

# 2026-08-02: RAM Previewの空範囲設定で旧状態を破棄

- 関連: `Artifact/src/Render/ArtifactRamPreviewController.cppm`
- 事実: `setPreviewRange(start >= end)` は新しい範囲のstateを作らず、以前のframeStatesとqueueを保持していたため、空範囲なのに旧フレームが問い合わせ可能になる可能性があった。
- 価値: 空／逆順範囲を設定した時点でstate、queue、カウンタ、エラーを一貫してリセットする。
- 未検証: 範囲変更中のbuild、再設定、ビルド。

# 2026-08-02: Layer dirty時にthumbnail cacheを無効化

- 関連: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 事実: `setDirty()` はrevisionとdirty flagだけを更新し、`getThumbnail()`のキャッシュを無効化していなかった。
- 価値: レイヤー変更後に旧サムネイルを返さず、将来の実コンテンツthumbnail生成へ正しい無効化境界を提供する。
- 未検証: 各layer mutation、thumbnail renderer、ビルド。

# 2026-08-02: FrameCache容量ゼロとprefetch終端を修正

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: maxFrameCount=0でも`put()`がentryを追加でき、prefetchRange/cancelPrefetchは終端が整数最大値の場合にincrement overflowの可能性があった。
- 価値: 設定した容量上限を厳密に守り、最大フレームを含むprefetch範囲でも有限回で処理を終了する。
- 未検証: cache eviction、prefetch worker、ビルド。

# 2026-08-02: FrameCache eviction候補枯渇時のフォールバック

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: LRU/LFU/FIFO/Sizeの候補キューが無効化後のstale recordだけになった場合、`evictOne()`はentryを削除せず、`evictToFit()`が終了しない可能性があった。
- 価値: live candidateが見つからない場合もentries mapから1件を削除し、容量調整を有限時間で完了させる。
- 未検証: 各eviction policy、長時間運用、ビルド。

# 2026-08-02: FrameCacheのnull entry取得をmiss扱い

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: entries mapにnull SharedPtrが残った場合、`get()`がhitCountを増やしてnullを返していた。
- 価値: null entryを除去し、返却値とhit/miss統計の意味を一致させる。
- 未検証: 破損状態の復旧、統計表示、ビルド。

# 2026-08-02: RenderPerformanceMonitorの計測値境界を修正

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: FPS=0/NaNでframe budgetを計算でき、frame timeのNaN/負値もメトリクスへ蓄積できた。
- 価値: performance monitorがゼロ除算や不正な平均値を生成せず、有限な計測値だけを集計する。
- 未検証: runtime FPS表示、異常入力、ビルド。

# 2026-08-02: PerformanceMonitor reset時にFPS窓を再初期化

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: `reset()` は履歴とFPSカウンタを消去していたが、`QElapsedTimer`を再起動していなかった。
- 価値: reset後の最初のFPS集計が、reset前の経過時間を分母に含めず新しい計測窓から始まる。
- 未検証: runtime FPS表示、長時間停止後のreset、ビルド。

# 2026-08-02: ProgressiveRendererのdownsample境界を正規化

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: draft/preview qualityのdownsampling値をそのまま保持しており、0以下の値が後段の解像度計算へ渡る可能性があった。
- 価値: downsampleを常に1以上に保ち、品質変更時のゼロ除算・不正サイズを防ぐ。
- 未検証: 実レンダー品質切替、UI入力、ビルド。

# 2026-08-02: FrameCache module interfaceのinclude位置を修正

- 関連: `Artifact/include/Render/ArtifactFrameCache.ixx`
- 事実: `export module Artifact.Render.FrameCache;` の後にQtヘッダをincludeしており、module purview内のinclude禁止ルールに違反していた。
- 価値: Qtヘッダをglobal module fragmentへ移し、MSVC/Ninja dyndepのモジュール解析条件を満たす。
- 未検証: module scan、ビルド。

# 2026-08-02: 公開module interfaceのinclude位置を追加修正

- 関連: `Artifact/include/Effect/ArtifactStabilizer.ixx`, `Artifact/include/Preview/ArtifactTimelineClock.ixx`, `Artifact/include/Plugin/LayerPluginAdapter.ixx`
- 事実: 3つの公開interfaceでも`export module`後にQt/wobject/ABIヘッダをincludeしていた。
- 価値: module purview内のincludeをなくし、C++20 moduleスキャン時の依存解釈を一貫させる。
- 未検証: module scan、Stabilizer/Timeline/Pluginのビルド。

# 2026-08-02: AIDSL undoの未適用成功を防止

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: undo stackにActionがあれば、apply/revert APIが存在しないにもかかわらず`undo()`がtrueを返していた。
- 価値: ホスト状態を変更していないのにundo成功と報告する誤認を防ぐ。
- 次に確認すべきこと: host actionへapply/revert契約を追加する設計時に、undo/redoを実装する。
- 未検証: host integration、ビルド。

# 2026-08-02: AIDSL find layersのlookup条件を実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `QueryFindLayers` はfilter ASTを保持していたが、実行時は条件を評価せず全layer IDを返していた。lookupから得られるlayer IDとグループ名を`id`/`name`として条件評価するようにした。
- 価値: `find layers where id ...` / `name ...` の基本的な検索が、ホストの全プロパティAPIなしで機能する。
- 制限: transformやeffect等の実レイヤープロパティはlookupに存在しないため、未対応のまま。
- 未検証: 複合条件、ホスト連携、ビルド。

# 2026-08-02: SimpleSpline終端制御点の参照を修正

- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: Catmull-Rom補間の最終セグメントで、次の制御点が`i + 1`ではなくセグメント数に依存する誤ったindex式になっていた。
- 価値: 終端付近のSpline位置・接線が最後の制御点を基準に計算される。
- 未検証: Spline分布の実座標、単一点／二点入力、ビルド。

# 2026-08-02: SimpleSpline接線をHermite微分へ修正

- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: 接線がHermite位置basis (`h2`,`h3`) の線形和になっており、曲線位置の微分ではなかった。
- 価値: Spline上の向き・回転に使う接線が、Catmull-Rom/Hermite曲線の実際の微分方向になる。
- 未検証: 接線方向のruntime表示、Spline分布、ビルド。

# 2026-08-02: SimpleSplineの非有限パラメータを防止

- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: `getPoint()` はNaN/Infの`t`を`std::clamp`へ渡し、NaNのままsegment indexへ変換する可能性があった。
- 価値: 補間入力が常に有限範囲となり、未定義なindex変換を防ぐ。
- 未検証: 異常入力、Spline分布、ビルド。

# 2026-08-02: CloneGenerator分布パラメータを正規化

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: variationのNaNとgrid spacingの負値がsetterから保持され、Random/Grid系の生成式へ流れる可能性があった。
- 価値: variationを0〜1、grid spacingを0以上に統一し、分布生成の入力前提を守る。
- 未検証: 各分布モード、異常入力、ビルド。

# 2026-08-02: Particle Generator補間入力を有限値へ正規化

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: 三段階色／値補間のmid position・timeと速度由来のstretch計算がNaN時に`std::clamp`後も不正値を保持し得た。
- 価値: パーティクルの色補間・flipbook/stretch表示で不正な計算値が伝播するのを防ぐ。
- 未検証: 異常なパラメータ、ソフトウェア描画、ビルド。

# 2026-08-02: Particle SystemのtimeScaleを正規化

- 関連: `Artifact/include/Generator/ArtifactParticleGenerator.ixx`
- 事実: `setTimeScale()` がNaN/Infや負値をそのまま保持し、更新deltaTimeへ乗算していた。
- 価値: シミュレーション時間を有限・非負に保ち、逆向き／非有限時間による状態破壊を防ぐ。
- 未検証: 時間倍率変更、停止・再開、ビルド。

# 2026-08-02: Stabilizerの変換値境界を補正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: processFrame() が不正なoutput sizeや、scale=0/NaN、translation/rotationの非有限値をそのまま画像変換へ渡す可能性があった。
- 価値: 変換時のゼロ除算・不正座標・不正画像サイズを防ぎ、異常なmotionでも元画像に近い安全なフォールバックを使う。
- 未検証: 実映像スタビライズ、境界塗り、ビルド。

# 2026-08-02: LiveStabilizerの履歴サイズを正規化

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: `setMaxHistorySize()` が0以下の値を保持し、履歴削除時のerase範囲計算を破壊する可能性があった。
- 価値: 履歴数を最低1件に保ち、負値由来の範囲外削除を防ぐ。
- 未検証: 履歴サイズ変更中のruntime、ビルド。

# 2026-08-02: Stabilizerの空フレーム入力を早期処理

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: null/0サイズ画像でも変換ループへ進み、空画像に対するborder座標clampが不正範囲になる可能性があった。
- 価値: 空フレームを安全にそのまま返し、無効な画素座標計算を防ぐ。
- 未検証: 空画像、境界塗り、ビルド。

# 2026-08-02: Stabilizer特徴検出・平滑化の境界を修正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: feature block size、quality/min distance、smoothing windowを未検証のまま使用し、負値で画素アクセスや空の平滑化結果になる可能性があった。
- 価値: 特徴検出の近傍が画像内に収まり、平滑化結果が少なくとも各motion frameに対応する。
- 未検証: 小画像、異常パラメータ、実映像、ビルド。

# 2026-08-02: Stabilizer block size検証の整数overflowを防止

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: block sizeと画像サイズの比較で、巨大な設定値に対する`blockSize * 2`の整数overflow余地があった。
- 価値: 乗算前の安全な除算比較により、異常に大きいblock sizeでも未定義な範囲判定を防ぐ。
- 未検証: 極端な設定値、小画像、ビルド。

# 2026-08-02: Stabilizer feature track座標を検証

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: trackFeatures() が外部由来のQPointFを直接intへ変換しており、NaN座標や空画像で探索へ進む可能性があった。
- 価値: 空画像を早期終了し、非有限のfeature座標を除外して画素アクセスを安全にする。
- 未検証: 外部track入力、空画像、ビルド。

# 2026-08-02: TextGizmo hit testのzoom境界を修正

- 関連: `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
- 事実: hitTest() がzoom=0/NaNのとき直接除算し、無限のhit thresholdで広範囲をText handleとして扱う可能性があった。
- 価値: zoomが不正でも有限の閾値でText Toolの選択判定を継続する。
- 未検証: zoom変更、viewport hit test、ビルド。

# 2026-08-02: TextLayer boxサイズの非有限値を防止

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: setMaxWidth/setBoxHeight は0以下だけを判定し、NaN/Infをlayoutへ保持し得た。
- 価値: Text Tool resize、property、JSON復元の全経路でbox layout値を有限値に保つ。
- 未検証: 異常JSON、resize操作、フォントレイアウト、ビルド。

# 2026-08-02: TextLayer ruby scaleの非有限値を防止

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: setRubyScale() がNaN/Infを`std::clamp`後も保持し得た。
- 価値: ルビ文字レイアウトの倍率を常に0.1〜1.0の有限値へ保つ。
- 未検証: ルビ表示、異常JSON、ビルド。

# 2026-08-02: TextLayer paragraph spacingの非有限値を防止

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: setParagraphSpacing() は負値だけを補正し、NaN/Infを保持し得た。
- 価値: 段落レイアウトへ不正なspacingを渡さず、Text ToolとJSON/property経路を安定させる。
- 未検証: 段落レイアウト、異常入力、ビルド。

# 2026-08-02: TextAnimator property更新の数値入力を正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: animator property path更新がQVariantのNaN/Infを各range・wiggly・transform値へ直接渡していた。
- 価値: Text Tool／AIDSL／property編集からのアニメータ数値を有限値に統一し、selector・layout計算の不正値伝播を防ぐ。
- 未検証: animator編集、JSON/QVariant異常入力、ビルド。

# 2026-08-02: TextGizmoレイヤー切替時のdrag状態をリセット

- 関連: `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
- 事実: setLayer() がlayerポインタだけを更新し、旧レイヤーのdrag/handle状態を保持していた。
- 価値: レイヤー切替後に旧操作が新レイヤーへ誤適用される可能性を防ぐ。
- 未検証: 選択切替中のdrag、Text Tool runtime、ビルド。

# 2026-08-02: TextGizmo selector overlayの非有限値を補正

- 関連: `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
- 事実: selector weight/cluster boundary/line boundaryを`std::clamp`のみで描画座標へ渡していた。
- 価値: NaN/Inf selector値がオーバーレイ矩形や境界線のgeometryへ伝播するのを防ぐ。
- 未検証: selector preview、異常アニメータ値、ビルド。

# 2026-08-02: Stabilizer未マッチtrackの伝播を停止

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: updateFeatureTracks() が新フレームで未マッチのtrackをvalidのまま残し、次の探索で古い位置を再利用する可能性があった。
- 価値: 各段階で実際にマッチしたtrackだけを次フレームへ伝播し、motion推定に stale feature を混入させない。
- 未検証: 追跡精度、欠落特徴点、実映像、ビルド。

# 2026-08-02: Stabilizerへの空フレーム追加を拒否

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: `addFrame()` がnull/0サイズ画像をframesへ追加でき、後続の特徴検出やoutput size設定へ無効画像が混入し得た。
- 価値: Stabilizer内部のフレーム列を有効画像だけに保ち、空入力由来の失敗を早期に防ぐ。
- 未検証: 空フレーム追加、フレーム位置整合、ビルド。

# 2026-08-02: Stabilizer clearFramesの統計状態をリセット

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: clearFrames() はフレームとmotionを消去していたが、totalFeaturesとprocessingTimeを保持していた。
- 価値: 新しい入力シーケンス開始時に前回の特徴数・処理時間が混入しない。
- 未検証: UI統計表示、再利用、ビルド。

# 2026-08-02: Stabilizer外部track差し替え時の特徴数を同期

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: setFeatureTracks() はtrack配列を差し替えてもtotalFeaturesを更新していなかった。
- 価値: 外部検出結果を設定した直後も、valid track数と統計表示が一致する。
- 未検証: 外部track入力、統計UI、ビルド。

# 2026-08-02: AIDSL layer filterのproperty aliasを追加

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: find layersのlookup filterは短縮名id/nameだけを提供していた。
- 価値: `id`/`name`に加えて`layer.id`/`layer.name`も同じlookup値で評価し、既存のproperty path記法と整合させる。
- 未検証: DSL実行、複合条件、ビルド。

# 2026-08-02: LiveStabilizerの空フレーム履歴混入を防止

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: LiveStabilizer::processFrame() がnull/0サイズ画像をhistoryへ追加していた。
- 価値: Live履歴を有効画像だけに保ち、空フレームで履歴長や初期化状態が壊れるのを防ぐ。
- 未検証: 空入力、停止・再開、ビルド。

# 2026-08-02: Stabilizer可視化の不正geometryを除外

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: visualizeFeatures/visualizeMotionVectors() が非有限QPointFをQPainterへ渡す可能性があった。
- 価値: デバッグ描画でNaN/Inf座標を描画せず、可視化経路の不正geometry伝播を防ぐ。
- 未検証: 異常track表示、QPainter runtime、ビルド。

# 2026-08-02: Stabilizer単一フレームを恒等変換で処理

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: framesが1枚の場合、フレーム間追跡が実行されずfeatureTracksが空になり、安定化処理が失敗していた。
- 価値: 単一フレームを有効な恒等motionとして扱い、プレビューや静止画入力でもStabilizerを利用できる。
- 未検証: FrameMotionの既定値、単一画像、ビルド。

# 2026-08-02: Stabilizer入力変更時に旧motionを破棄

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: setFeatureTracks/addFrame後も旧frameMotions・smoothedMotions・stabilizedフラグが残り、入力と結果が不一致になる可能性があった。
- 価値: tracksまたはフレーム追加後は再安定化を要求し、旧結果の再利用を防ぐ。
- 未検証: tracks差し替え、フレーム追加後の再実行、ビルド。

# 2026-08-02: Stabilizer設定変更時に旧結果を無効化

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: `setParams()` がパラメータだけを差し替え、旧motionとstabilizedフラグを保持していた。
- 価値: smoothing／feature／transform設定変更後に、変更前の安定化結果を返さず再計算を要求する。
- 未検証: 設定変更後の再実行、runtime、ビルド。

# 2026-08-02: Live/Batch Stabilizer設定変更時の状態整合

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: LiveStabilizerは設定変更後も過去の履歴を保持し、BatchStabilizerは未処理時の進捗カウンタを保持していた。
- 価値: 設定変更後のLive履歴を再構築し、未実行Batchの進捗表示を新設定に合わせて初期化する。
- 未検証: 実時間入力、batch UI、ビルド。

# 2026-08-02: Stabilizer出力でalphaを保持

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: processFrame() が`Format_RGB32`で結果を作成しており、入力alphaを補間しても出力形式で保持できなかった。
- 価値: ARGB32 premultipliedへ変更し、透明背景を含む素材のスタビライズ結果でalphaを維持する。
- 未検証: premultiplied alphaの補間品質、実映像、ビルド。

# 2026-08-02: CloneGenerator spacingの負値を防止

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: 基本spacingは有限値のみを確認し、負値を保持していた。
- 価値: Linear/Grid等の配置間隔を常に非負に保ち、逆向き間隔による意図しない配置を防ぐ。
- 未検証: 各分布モード、異常入力、ビルド。

# 2026-08-02: DistributionModesの標準型includeを自己完結化

- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: `SimpleSpline` が使用する `std::clamp`、`std::isfinite`、`std::vector` の直接includeが不足していた。
- 価値: 他moduleの推移的includeに依存せず、公開interface単体で宣言・実装を解析できる。
- 未検証: module scan、Spline分布、ビルド。

# 2026-08-02: Difference Keyの閾値入力を有限値へ正規化

- 関連: `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`
- 事実: threshold/softness のプロパティ変更がNaN/Infを `std::clamp` に渡し、そのまま判定へ保持する可能性があった。
- 価値: Chroma/Lumaと同じくDifference Keyでも異常入力時のアルファ計算を安定させる。
- 未検証: 実画像品質、GPU/runtime経路、ビルド。
# 2026-08-02: 連番フレーム番号の安全な変換

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm` の連番画像フレーム選択。
- 事実: composition FPS と sequence FPS を掛けた結果を `qint64` に直接キャストする処理が `draw()` と `toQImage()` に重複していた。
- 対応: 共通の `resolveSequenceFrame()` に集約し、非有限値・非正の FPS・`qint64` 上限超過を扱ってからキャストするようにした。
- 価値: 極端な時間値や設定値での未定義動作を避け、描画経路と Qt 画像取得経路のフレーム解決を一致させる。
- 次に確認: 実フレーム番号が `qint64` 上限付近になるケースを含む連番再生の runtime 受入れ。ビルド・テストは未実施。
# 2026-08-02: Liquid Glow の非有限パラメータ防止

- 関連: `Artifact/src/Effects/Glow/LiquidGlowEffect.cppm`。
- 事実: setter が `std::clamp` のみを使っており、NaN 入力が内部実装へ伝播し得た。
- 対応: threshold/radius/intensity/flowScale/distortion/phase を有限値確認後にクランプし、異常値は各デフォルトへ戻すようにした。
- 価値: Gaussian blur と flow map 生成で NaN が広がる経路を遮断する。
- 次に確認: 実画像での glow 品質と GPU/runtime 経路。ビルド・テストは未実施。
# 2026-08-02: 基本エフェクト setter の有限値検証

- 関連: `DitheringEffect`、`AddNoiseEffect`、`BevelEffect`。
- 事実: 一部の setter が clamp/max のみで、NaN や infinity を CPU/GPU 実装へ渡し得た。
- 対応: amount/scale/size/strength/softness を有限値確認後に制約し、異常値は既定値へ復帰させた。
- 価値: ノイズ量・パターン尺度・ベベル計算に非有限値が混入する経路を減らす。
- 次に確認: GPU と CPU の実画像一致、ビルド・runtime受入れ。
# 2026-08-02: Luma Key の直接実装値ガード

- 関連: `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`。
- 事実: property 経路では有限値化済みだが、CPU 実装は内部値が直接非有限になった場合を想定していなかった。
- 対応: low/high/softness を apply 境界でも有限値化し、既定値と制約値を適用した。
- 価値: alpha matte 計算の NaN 伝播を property 経路外からも防ぐ。
- 次に確認: 実画像での閾値境界、CPU/runtime 受入れ。ビルド・テストは未実施。
# 2026-08-02: White Balance の色補正入力ガード

- 関連: `Artifact/include/Effects/WhiteBalanceEffect.ixx`。
- 事実: temperature/tint/brightness の setter が clamp のみで、NaN がそのまま内部値に残り得た。温度変換・指数計算・GPU定数へ影響する。
- 対応: 有限値確認後に既存範囲へクランプし、異常値は 6500K / 0 / 0 に復帰させた。
- 価値: CPU/GPU の色補正係数へ非有限値が伝播する経路を塞ぐ。
- 次に確認: CPU/GPUの色補正一致とHDR入力。ビルド・テストは未実施。
# 2026-08-02: Colorama のパラメータ入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/ColoramaEffect.cppm`。
- 事実: phase/spread/strength/saturationBoost/contrast が clamp/max のみで、非有限値を内部設定へ保持し得た。
- 対応: 有限値を確認してから既存範囲へ制約し、異常値は既定設定へ戻した。
- 価値: CPU/GPU のパレット補間・色変換に非有限値が流入する経路を減らす。
- 次に確認: パレット境界と CPU/GPU の実画像一致。ビルド・テストは未実施。
# 2026-08-02: Color Balance の入力値ガード

- 関連: `Artifact/src/Effects/ColorCorrection/ColorBalanceEffect.cppm`。
- 事実: shadow/midtone/highlight の各バランス値と range/strength が clamp のみだった。
- 対応: 各 setter で有限値を確認し、異常値は中立値または既定 range/strength に復帰させた。
- 価値: CPU/GPU のトーンウェイトと色差分に非有限値が混入する経路を防ぐ。
- 次に確認: range 境界・preserve luma・CPU/GPU の実画像一致。ビルド・テストは未実施。
# 2026-08-02: Fill の opacity 入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/FillEffect.cppm`。
- 事実: opacity setter が clamp のみで、非有限値を CPU/GPU の fill 設定へ渡し得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常値は既定 opacity 1.0 に戻した。
- 価値: 塗りつぶしの alpha 合成係数への異常値伝播を防ぐ。
- 次に確認: preserve alpha と GPU fallback の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Gradient Ramp の座標・opacity入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/GradientRampEffect.cppm`。
- 事実: start/end point と opacity の setter が clamp のみで、非有限値が設定に残り得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常な座標は start=(0,0)、end=(1,1)、opacity は 1.0 に復帰。
- 価値: CPU/GPU の勾配方向・alpha計算への非有限値伝播を防ぐ。
- 次に確認: 各 preset の勾配方向と preserve alpha。ビルド・テストは未実施。
# 2026-08-02: Selective Color の調整値ガード

- 関連: `Artifact/src/Effects/ColorCorrection/SelectiveColorEffect.cppm`。
- 事実: strength と各色域の CMYK 調整値が clamp のみで、非有限値が設定に残り得た。
- 対応: 有限値確認後に strength は 0..1、調整値は -1..1 に制約し、異常値は既定値へ復帰させた。
- 価値: 色域別補正の CPU/GPU 計算へ非有限値が伝播する経路を防ぐ。
- 次に確認: relative mode と preserve luma の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Tritone のトーンパラメータガード

- 関連: `Artifact/src/Effects/ColorCorrection/TritoneEffect.cppm`。
- 事実: balance/softness/masterStrength/colorMix が clamp のみで、非有限値を保持し得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常値は TritoneSettings の既定値へ復帰させた。
- 価値: CPU/GPU の三色トーン補間に非有限値が流入する経路を防ぐ。
- 次に確認: preserve luma とプリセット切替時の実画像結果。ビルド・テストは未実施。
# 2026-08-02: Color Wheels の中立値フォールバック

- 関連: `Artifact/src/Effects/ColorCorrection/ColorWheelsEffect.cppm`。
- 事実: lift/gamma/gain/offset の setter が clamp のみで、非有限値が設定に残り得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は lift/offset=0、gamma/gain=1 の中立値へ復帰させた。
- 価値: wheel 計算と GPU パラメータへの非有限値伝播を防ぐ。
- 次に確認: master と RGB 個別調整の CPU/GPU 実画像一致。ビルド・テストは未実施。
# 2026-08-02: Photo Filter の入力値ガード

- 関連: `Artifact/src/Effects/ColorCorrection/PhotoFilterEffect.cppm`。
- 事実: density/brightness/contrast/saturationBoost が clamp のみで、非有限値が設定に残り得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は PhotoFilterSettings の既定値へ復帰させた。
- 価値: CPU/GPU のフィルタ色・明度・彩度計算への非有限値伝播を防ぐ。
- 次に確認: プリセット切替と preserve luma の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Channel Mixer の行列入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/ChannelMixerEffect.cppm`。
- 事実: strength は clamp のみ、9要素の mixer 行列は有限値確認なしで設定されていた。
- 対応: strength を既定値へ復帰し、行列要素の非有限値を単位行列相当の要素へ置換した。
- 価値: CPU/GPU のチャンネル混合と shader 定数へ NaN/infinity が伝播する経路を防ぐ。
- 次に確認: モノクロ・preserve luma とプリセット切替の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Hue and Saturation の入力ガード

- 関連: `Artifact/include/Effects/ColorCorrection/HueAndSaturation.ixx`、`Artifact/src/Effects/ColorCorrection/HueAndSaturation.cppm`。
- 事実: 公開 setter と property 経路の両方が clamp のみで、非有限値を内部へ保持し得た。
- 対応: hue/saturation/lightness を有限値確認後に既存範囲へ制約し、異常値は 0/1/0 の中立値へ復帰させた。
- 価値: CPU/GPU の HSV/HSL 計算への非有限値伝播を防ぐ。
- 次に確認: colorize と hue wrap の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Levels のレベル値・ガンマ入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/LevelsEffect.cppm`。
- 事実: input/output level は値を直接保持し、input gamma も clamp のみだったため、非有限値が正規化・指数計算へ到達し得た。
- 対応: 各 setter で有限値を確認し、異常値は inputBlack/outputBlack=0、inputWhite/outputWhite=255、gamma=1 に復帰させた。
- 価値: レベル計算の分母・ガンマ指数と CPU/GPU 設定への非有限値伝播を防ぐ。
- 次に確認: per-channel と input black/white の境界受入れ。ビルド・テストは未実施。
# 2026-08-02: Grayscale の strength 入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/GrayscaleEffect.cppm`。
- 事実: strength setter が clamp のみで、非有限値が CPU/GPU のグレースケール混合へ流入し得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常値は既定 strength 1.0 に復帰させた。
- 価値: モノクロ化と元画像の混合係数への NaN/infinity 伝播を防ぐ。
- 次に確認: 3種の mode と GPU fallback の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Brightness/Contrast の入力ガード

- 関連: `Artifact/include/Effects/ColorCorrection/BrightnessEffect.ixx`。
- 事実: brightness/contrast/highlights/shadows の公開 setter が clamp のみだった。
- 対応: 有限値確認後に -1..1 へ制約し、異常値は中立値 0 に復帰させた。
- 価値: CPU/GPU の明度・コントラスト係数へ非有限値が伝播する経路を防ぐ。
- 次に確認: highlights/shadows の実画像結果と GPU fallback。ビルド・テストは未実施。
# 2026-08-02: Invert の strength 入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/InvertEffect.cppm`。
- 事実: strength setter が clamp のみで、非有限値が CPU/GPU の反転混合係数へ流入し得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常値は既定 strength 1.0 に復帰させた。
- 価値: RGB/チャンネル反転の混合計算を非有限値から保護する。
- 次に確認: RGB/単一チャンネル/Alpha の実画像結果。ビルド・テストは未実施。
# 2026-08-02: Curves のカーブ強度ガード

- 関連: `Artifact/src/Effects/ColorCorrection/CurvesEffect.cppm`。
- 事実: strength が clamp のみで、S-curve の制御点生成と LUT 計算へ非有限値が到達し得た。
- 対応: setter と S-curve 点生成の両方で有限値を確認し、異常値を strength 0 に復帰させた。
- 価値: CPU/GPU のカーブ/LUT生成を非有限入力から保護する。
- 次に確認: 各 preset と posterize levels の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Exposure の指数計算入力ガード

- 関連: `Artifact/include/Effects/ColorCorrection/ExposureEffect.ixx`。
- 事実: exposure/offset/gamma の公開 setter が clamp のみで、gamma は逆数・pow 計算へ直結していた。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は exposure/offset=0、gamma=1 に復帰させた。
- 価値: CPU/GPU の露光・ガンマ計算への非有限値伝播を防ぐ。
- 次に確認: EV境界、offset、gamma の CPU/GPU 実画像一致。ビルド・テストは未実施。
# 2026-08-02: Kuwahara のカーネル入力ガード

- 関連: `Artifact/src/Effects/Kuwahara/KuwaharaEffect.cppm`。
- 事実: radius/sharpness が clamp のみで、radius はカーネルサイズ計算へ直結していた。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は radius=5、sharpness=0.5 に復帰させた。
- 価値: CPU/GPU の局所統計計算とカーネル生成を非有限入力から保護する。
- 次に確認: anisotropic モードを含む実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Aperture Shape Blur の入力ガード

- 関連: `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm`。
- 事実: radius/rotation/edge brightness/highlight boost の property 入力が非有限値を保持し得た。radius はカーネルサイズ計算へ直結する。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は radius=18、rotation=0、edge=0.2、boost=0.35 に復帰させた。
- 価値: aperture blur のカーネル生成・PSF計算への異常値伝播を防ぐ。
- 次に確認: PSF画像経路と shape/rotation の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Anisotropic Flow Blur の入力ガード

- 関連: `Artifact/src/Effects/Blur/AnisotropicFlowBlurEffect.cppm`。
- 事実: blur amount、tensor scale、edge adherence の property 入力が clamp のみだった。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は既定値へ復帰させた。
- 価値: ベクトル場・積分スケール・ぼかし量の計算を非有限入力から保護する。
- 次に確認: tensor integration と edge adherence の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Blur の radius/strength 入力ガード

- 関連: `Artifact/include/Effects/Blur/BlurEffect.ixx`、`Artifact/src/Effects/Blur/BlurEffect.cppm`。
- 事実: radius は max のみ、strength/edge threshold は clamp のみで、公開 setter と CPU 実装の双方に非有限値の余地があった。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は radius=10、strength=1、edge threshold=0.1 に復帰させた。
- 価値: Gaussian/edge blur のカーネル・混合計算を非有限入力から保護する。
- 次に確認: blur mode、premultiplied alpha、GPU fallback の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Reaction Diffusion Blur の入力ガード

- 関連: `Artifact/src/Effects/Blur/ReactionDiffusionBlurEffect.cppm`。
- 事実: blur radius/feed/kill/pattern strength/evolution の property 入力に非有限値の余地があり、iterations は有限でない整数変換以外の問題はなかった。
- 対応: 浮動小数値を有限値確認後に既存範囲へ制約し、異常値は各既定値へ復帰させた。
- 価値: 反応拡散の反復・模様生成・ぼかし係数への非有限値伝播を防ぐ。
- 次に確認: iterations と evolution の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Gaussian Blur の sigma 入力ガード

- 関連: `Artifact/include/Effects/GauusianBlur.ixx`。
- 事実: CPU/GPU 実装の sigma setter が値を直接保持し、カーネルサイズ・blur計算へ非有限値が到達し得た。
- 対応: 有限値確認後に 0..64 へ制約し、異常値は既定 sigma 5.0 に復帰させた。
- 価値: CPU/GPU の Gaussian kernel と ROI計算を非有限入力から保護する。
- 次に確認: sigma=0 の無効化動作と GPU fallback。ビルド・テストは未実施。
# 2026-08-02: Sharpen の強調パラメータガード

- 関連: `Artifact/src/Effects/Sharpen/SharpenEffect.cppm`。
- 事実: amount/sigma/threshold が clamp のみで、sigma は Gaussian kernel と blur sigma に直結していた。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は amount=1、sigma=1、threshold=0 に復帰させた。
- 価値: シャープ化のカーネル生成と閾値判定を非有限入力から保護する。
- 次に確認: sigma=0 と threshold 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Glow の多層パラメータガード

- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`。
- 事実: CPU/GPU の glow gain、layer count、sigma、growth、alpha 系 setter が値を直接保持していた。
- 対応: 両実装の setter を有限値確認・範囲制約付きにし、多層数を 1..16 に制限した。
- 価値: 多段 Gaussian のカーネル・alpha 正規化・GPU定数への異常値伝播と過大反復を防ぐ。
- 次に確認: 多層 glow の ROI と CPU/GPU 実画像一致。ビルド・テストは未実施。
# 2026-08-02: Drop Shadow の影パラメータガード

- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`。
- 事実: distance/angle/softness/opacity の setter が max/clamp または直接代入で、非有限値が影の三角関数・blur・alpha計算へ到達し得た。
- 対応: 有限値確認後に既存制約を適用し、異常値は distance=5、angle=135、softness=8、opacity=75 に復帰させた。
- 価値: CPU/GPU の影位置・ぼかし・不透明度計算を非有限入力から保護する。
- 次に確認: GPU fallback と shadow color / premultiplied alpha の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Inner Shadow の影パラメータガード

- 関連: `Artifact/src/Effects/InnerShadow/InnerShadowEffect.cppm`。
- 事実: distance/angle/softness/opacity の setter が max/clamp または直接代入で、非有限値が影位置・blur・alpha計算へ到達し得た。
- 対応: 有限値確認後に既存制約を適用し、異常値は distance=5、angle=135、softness=8、opacity=75 に復帰させた。
- 価値: 内側影のオフセット・カーネル・不透明度計算を非有限入力から保護する。
- 次に確認: CPU/GPU fallback と境界 alpha の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Find Edges の amount 入力ガード

- 関連: `Artifact/src/Effects/FindEdges/FindEdgesEffect.cppm`。
- 事実: amount setter が clamp のみで、非有限値が CPU の edge/color 混合と GPU 定数へ流入し得た。
- 対応: 有限値確認後に 0..5 へ制約し、異常値は既定 amount 1.0 に復帰させた。
- 価値: エッジ強調と元画像の混合計算を非有限入力から保護する。
- 次に確認: invert モードと amount 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Directional Glow のストリーク入力ガード

- 関連: `Artifact/include/Effects/DirectionalGlowEffect.ixx`。
- 事実: threshold/intensity/length/weight/angle の setter が max/clamp または直接代入で、非有限値がストリーク計算へ到達し得た。
- 対応: 有限値確認後に既存制約を適用し、異常値は既定の threshold=0.8、length=64/128、weight=0.6/0.4 などへ復帰させた。
- 価値: streak 半径・重み・GPU定数・角度計算を非有限入力から保護する。
- 次に確認: pattern 切替と custom angle の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Chromatic Glow の色収差入力ガード

- 関連: `Artifact/include/Effects/Glow/ChromaticGlowEffect.ixx`。
- 事実: threshold/radius/intensity/dispersion/tintMix は clamp のみ、angle は直接代入で、非有限値がサンプリング座標・GPU定数へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は既定値へ復帰させた。
- 価値: 色収差の半径・強度・角度・サンプリング計算を非有限入力から保護する。
- 次に確認: 色収差角度と tint mix の CPU/GPU 実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Satin のハイライト入力ガード

- 関連: `Artifact/src/Effects/Satin/SatinEffect.cppm`。
- 事実: distance/angle/softness/opacity の setter が max/clamp または直接代入で、非有限値が satin のオフセット・三角関数・blur・alpha計算へ到達し得た。
- 対応: 有限値確認後に既存制約を適用し、異常値は distance=0、angle=0、softness=5、opacity=50 に復帰させた。
- 価値: CPU/GPU の satin ハイライト計算を非有限入力から保護する。
- 次に確認: invert と色・alpha境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Radial Shadow の中心・距離入力ガード

- 関連: `Artifact/src/Effects/RadialShadow/RadialShadowEffect.cppm`。
- 事実: distance/softness/opacity/centerX/centerY が max/clamp のみで、非有限値が距離・alpha・GPU定数へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は distance=10、softness=8、opacity=0.75、center=(0.5,0.5) に復帰させた。
- 価値: 放射状影の中心距離・blur・不透明度計算を非有限入力から保護する。
- 次に確認: 色と中心位置境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Vignette の画面係数入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/VignetteEffect.cppm`。
- 事実: amount/radius/feather/center が clamp のみで、非有限値が距離・マスク計算へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は amount=.7、radius=.8、feather=.4、center=(.5,.5) に復帰させた。
- 価値: CPU/GPU の画面周辺減光マスクを非有限入力から保護する。
- 次に確認: center と feather 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Film Damage のアナログ効果入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/FilmDamageEffect.cppm`。
- 事実: grain/dust/scratches/gate weave/flicker/film burn は clamp のみ、evolution は直接代入だった。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は各既定値へ復帰させた。
- 価値: ノイズ・傷・揺れ・焼けとフレーム進行の計算を非有限入力から保護する。
- 次に確認: seed と evolution を含むフレーム間再現性。ビルド・テストは未実施。
# 2026-08-02: Mosaic のセルサイズ入力ガード

- 関連: `Artifact/src/Effects/Mosaic/MosaicEffect.cppm`。
- 事実: cell size setter が max のみで、非有限値がブロックサイズから整数化され得た。
- 対応: 有限値確認後に 1 以上へ制約し、異常値は既定 cell size 8 に復帰させた。
- 価値: CPU/GPU のモザイクブロック計算とサンプリングを非有限入力から保護する。
- 次に確認: shape mode と大きな画像サイズでの実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Linear Wipe の境界入力ガード

- 関連: `Artifact/src/Effects/LinearWipe/LinearWipeEffect.cppm`。
- 事実: angle は fmod へ直接渡され、softness/feather は clamp のみだったため、非有限値が wipe の投影・境界計算へ到達し得た。
- 対応: 有限値確認後に angle を 0..360 相当に正規化し、softness/feather を既存範囲へ制約した。
- 価値: CPU/GPU の wipe マスク境界と除算を非有限入力から保護する。
- 次に確認: 角度 wrap と feather 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Bricks のタイル寸法入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/BricksEffect.cppm`。
- 事実: brick width/height、mortar、offset が max/clamp のみで、非有限値が除算・タイル配置へ到達し得た。
- 対応: 有限値確認後に既存下限・範囲を適用し、異常値は width=64、height=32、mortar=3、offset=.5 に復帰させた。
- 価値: タイル行列・モルタル計算を非有限入力から保護する。
- 次に確認: offset の周期境界と大画像での実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Chromatic Aberration の座標入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/ChromaticAberrationEffect.cppm`。
- 事実: red/blue shift と center が clamp のみで、非有限値が画素オフセット・中心距離計算へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は shift=2、center=(.5,.5) に復帰させた。
- 価値: CPU/GPU の色チャンネルサンプリング座標を非有限入力から保護する。
- 次に確認: 画像端・中心ピクセル・GPU fallback の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Kaleidoscope の幾何入力ガード

- 関連: `Artifact/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm`。
- 事実: center/rotation/zoom/feather が clamp/max/fmod または直接計算へ渡され、非有限値が分割角度・半径・フェード計算へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は center=.5、rotation=0、zoom=1、feather=0 に復帰させた。
- 価値: CPU/GPU の万華鏡投影と境界フェードを非有限入力から保護する。
- 次に確認: segments、mirror、rotation wrap の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Lens Distortion の投影入力ガード

- 関連: `Artifact/include/Effects/LensDistortion/LensDistortionEffect.ixx`。
- 事実: CPU/GPU の distortion/center/zoom setter が値を直接保持していた。
- 対応: distortion を -100..100、center を 0..1、zoom を 0.01 以上に有限値確認付きで制約した。
- 価値: CPU/GPU の投影座標・半径・除算計算を非有限入力から保護する。
- 次に確認: invert distortion と中心境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Radio Waves の時間・発生パラメータガード

- 関連: `Artifact/src/Effects/Generate/RadioWavesEffect.cppm`。
- 事実: origin/frequency/expansion/lifespan/stroke/opacity/currentTime が max/clamp のみだった。
- 対応: 有限値確認後に既存下限・範囲を適用し、異常値は各既定値へ復帰させた。
- 価値: 波の発生間隔・半径・寿命・アルファ・時間進行計算を非有限入力から保護する。
- 次に確認: currentTime と frequency 境界のフレーム再現性。ビルド・テストは未実施。
# 2026-08-02: Auto Mosaic の境界フェザー入力ガード

- 関連: `Artifact/include/Effects/AutoMosaicEffect.ixx`。
- 事実: feather setter が max のみで、非有限値が自動モザイク領域の境界処理へ到達し得た。
- 対応: 有限値確認後に 0 以上へ制約し、異常値は既定 feather 0 に復帰させた。
- 価値: 顔検出・カスタム領域どちらの境界フェードも非有限入力から保護する。
- 次に確認: 顔検出とカスタム領域の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Edge/Rim Light の入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/EdgeEffect.cppm`。
- 事実: Edge と Rim Light の mode/intensity/threshold、角度・幅・softness・mix が clamp/fmod または直接計算へ非有限値を渡し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は各エフェクトの既定値へ復帰させた。
- 価値: エッジ判定・リム角度・線幅・alpha混合計算を非有限入力から保護する。
- 次に確認: Edge/Rim の CPU/GPU 実画像一致と角度 wrap。ビルド・テストは未実施。
# 2026-08-02: Simple Rain の雨滴入力ガード

- 関連: `Artifact/src/Effects/Generate/SimpleRainEffect.cppm`。
- 事実: density/streak/speed/wind/opacity/depth/splash は clamp のみ、evolution は直接代入だった。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は各既定値へ復帰させた。
- 価値: 雨滴数・位置・速度・飛沫・alpha・フレーム進行計算を非有限入力から保護する。
- 次に確認: evolution と seed を含むフレーム間再現性。ビルド・テストは未実施。
# 2026-08-02: Liquify の変形入力ガード

- 関連: `Artifact/include/Effects/Liquify/LiquifyEffect.ixx`。
- 事実: CPU/GPU の amount/radius/center/angle/mesh density setter が値を直接保持していた。
- 対応: amount/radius/center/angle を有限値確認・範囲制約付きにし、mesh density を 4..128 に制限した。
- 価値: ブラシ変形・座標投影・メッシュ計算への非有限値と過大反復を防ぐ。
- 次に確認: 各ブラシ種別と mesh density 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Displacement Map の変位入力ガード

- 関連ファイル: `Artifact/src/Effects/DisplacementMap/DisplacementMapEffect.cppm`
- 気づき: 水平・垂直変位量は自己サンプリング座標へ直接加算されるため、非有限値が入ると画素サンプリング全体が壊れる。チャンネル enum も property 経由では任意の整数に変換できる。
- 対応: 変位量を有限値かつ ±4096 に制限し、非有限値は既定値 20 に戻した。チャンネル値は Luminance〜Alpha の範囲へ正規化した。
- 価値または懸念: 不正なプロパティ入力で NaN/Inf 座標や未定義チャンネルが伝播するのを防ぐ。±4096 の上限は実装上の安全上限であり、仕様上の最大値としては未検証。
- 次に確認すべきこと: UI 側で変位量の許容範囲を定義する際に、この上限と負値による反転方向を整合させる。
# 2026-08-02: Spherize の球面変形入力ガード

- 関連ファイル: `Artifact/include/Effects/Spherize/SpherizeEffect.ixx`
- 気づき: Spherize は amount、radius、中心座標を CPU/GPU の両実装へ共有し、radius はゼロ除算、座標は画面外の極端なサンプリングへ直結する。
- 対応: amount を有限値かつ -100〜100、radius と中心座標を有限値かつ 0〜1 に制限し、非有限値は既定値へ戻した。CPU/GPU 実装の setter を同じ規則に統一した。
- 価値または懸念: property 経由の NaN/Inf や範囲外値が計算・GPU 定数へ流れるのを防ぐ。
- 次に確認すべきこと: 半径 0 を UI で許容する仕様が必要か、既定の最小半径を設けるべきかを確認する。
# 2026-08-02: Feedback の履歴変形入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/FeedbackEffect.cppm`
- 気づき: Feedback は前フレームのサンプリング座標を zoom・回転・中心オフセットから算出するため、非有限値が入ると履歴参照が破綻する。
- 対応: amount、decay、中心オフセット、zoom、rotation を有限値として検証し、既存 property 範囲に合わせてクランプした。非有限値は各既定値へ戻した。
- 価値または懸念: property 経由の NaN/Inf が座標計算や重み計算へ伝播するのを防ぐ。rotation の許容範囲は既存 UI 定義（±180）に合わせた。
- 次に確認すべきこと: 履歴サンプラーが異なる解像度を返す場合の座標スケーリングは別課題として確認する。
# 2026-08-02: Chromatic Relief の入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/ChromaticReliefEffect.cppm`
- 気づき: Chromatic Relief は方向からオフセットを計算し、edge softness は GaussianBlur の sigma に使うため、非有限値がそのまま画像処理へ流れると不正な処理条件になる。
- 対応: Relief Amount、Chromatic Offset、Direction、Edge Softness、Mix を有限値として検証し、既存の範囲制限と既定値フォールバックを適用した。
- 価値または懸念: property 入力由来の NaN/Inf による OpenCV パラメータや色ブレンド値の破綻を防ぐ。
- 次に確認すべきこと: Direction の UI 表示範囲を決める場合は、内部の任意角度対応と整合させる。
# 2026-08-02: Echo の残像重み入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/EchoEffect.cppm`
- 気づき: Echo の decay と blend operator は複数フレームの重み計算へ直接使われ、非有限値が入ると全エコーの合成結果が壊れる。
- 対応: 両値を有限値として検証してから 0〜1 にクランプし、非有限値は既定値へ戻した。
- 価値または懸念: property 入力由来の NaN/Inf の伝播を防ぐ。echoCount の整数値は既存の 1〜16 制限を維持した。
- 次に確認すべきこと: blend operator の値と実際の合成モードの対応表は別途仕様確認する。
# 2026-08-02: Difference Matte のしきい値入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/DifferenceMatteEffect.cppm`
- 気づき: Difference Matte の threshold はフレーム間差分を二値化する境界値であり、非有限値が入ると比較結果が不定になる。
- 対応: threshold を有限値として検証してから 0〜1 にクランプし、非有限値は既定値 0 に戻した。
- 価値または懸念: property 経由の NaN/Inf がマット生成へ伝播するのを防ぐ。参照フレーム offset の既存 1〜60 制限は維持した。
- 次に確認すべきこと: 異なる解像度の参照フレームを座標スケールするかは別仕様として確認する。
# 2026-08-02: Rasterizer 生成系の入力ガード拡張

- 関連ファイル: `Artifact/src/Effects/Rasterizer/RadialBlurEffect.cppm`, `HalftoneEffect.cppm`, `StripesEffect.cppm`, `HexGridEffect.cppm`
- 気づき: これらの静止画エフェクトは半径・セルサイズ・周波数・角度などを直接ピクセル座標や除算へ使うため、既存の clamp だけでは NaN/Inf が残る。
- 対応: 各 setter で有限値を検証し、既存の下限・UI 想定範囲に加えて安全上限を適用した。非有限値は各エフェクトの既定値へ戻した。
- 価値または懸念: 不正入力による座標計算・除算・GPU/CPU 実装同期の破綻を防ぐ。安全上限は実装上の上限であり、仕様上の最大値としては未検証。
- 次に確認すべきこと: 各 UI の property min/max を setter の安全上限と明示的に揃える。
# 2026-08-02: Voronoi のセル生成入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/VoronoiEffect.cppm`
- 気づき: Voronoi の scale はセル座標の除数、jitter はセル位置の乱数オフセットとして使われ、seed は擬似乱数計算へ渡る。
- 対応: scale を有限値かつ 1〜200、jitter を有限値かつ 0〜2、seed を 0〜9999 に制限し、非有限値の float は既定値へ戻した。
- 価値または懸念: 不正入力でセル分割の除算や乱数座標が破綻するのを防ぐ。上限は既存 property 定義に合わせた。
- 次に確認すべきこと: mode の各値が表示する距離指標と UI 表示名で一致しているか確認する。
# 2026-08-02: Vector Flow Glitch の入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/VectorFlowGlitchEffect.cppm`
- 気づき: Glitch Amount、Frequency、Chromatic Aberration、Edge Flow Influence、Evolution は画像処理設定へ直接渡され、既存の clamp だけでは非有限値を除外できない。
- 対応: 各 property 値を有限値として検証し、既存範囲へクランプした。非有限値はヘッダ既定値に合わせてフォールバックした。
- 価値または懸念: 不正な property 入力が VectorFlowGlitch の内部計算へ伝播するのを防ぐ。
- 次に確認すべきこと: `VectorFlowGlitchSettings` の各範囲定義と UI の min/max を別途揃える。
# 2026-08-02: Vector Blur のモーション入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/VectorBlurEffect.cppm`
- 気づき: shutter angle と exposure compensation はベクトル移動量・合成重みへ使われ、CPU/GPU の両実装へ同期される。
- 対応: 両 float 値を有限値として検証し、既存 property 範囲（角度 0〜720、露出 0〜4）へクランプした。非有限値は既定値へ戻した。
- 価値または懸念: NaN/Inf がモーションブラーのサンプリングや GPU 定数へ伝播するのを防ぐ。samples の既存 2〜32 制限は維持した。
- 次に確認すべきこと: 速度探索の固定範囲（±16 px）が高解像度素材で十分かは別途確認する。
# 2026-08-02: Glitch の乱数・強度入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/GlitchEffect.cppm`
- 気づき: Glitch の intensity、color shift、scanlines は画素変形と合成量に使われ、seed はフレーム番号と結合して乱数初期化へ使われる。
- 対応: float 値を有限値として検証して 0〜1 にクランプし、seed を既存 property 範囲の 0〜9999 に制限した。非有限値は既定値へ戻した。
- 価値または懸念: 不正入力による画素処理・乱数系列の不安定化を防ぐ。
- 次に確認すべきこと: フレーム番号と seed の加算が整数オーバーフローしないよう、長時間タイムラインの型変換を確認する。
# 2026-08-02: Lift Gamma Gain の色調入力ガード

- 関連ファイル: `Artifact/include/Effects/LiftGammaGainEffect.ixx`
- 気づき: Lift/Gamma/Gain は GPU の pow と乗算へ直接渡され、既存の clamp だけでは NaN/Inf を除外できない。
- 対応: RGB 各 setter で有限値を検証し、Lift は -1〜1、Gamma は 0.1〜5、Gain は 0〜4 にクランプした。非有限値は中立値へ戻した。
- 価値または懸念: CPU/GPU の色調計算へ不正値が伝播するのを防ぐ。Gamma と Gain の非有限値はそれぞれ 1 を中立値とした。
- 次に確認すべきこと: Lift の符号と GPU 側の RGB/BGR 変換が UI のチャンネル表記と一致するか確認する。
# 2026-08-02: Dithering のアルゴリズム enum 入力ガード

- 関連ファイル: `Artifact/src/Effects/Dithering/DitheringEffect.cppm`
- 気づき: Dithering の Algorithm は property から整数を enum へ直接変換しており、範囲外の値が CPU/GPU のアルゴリズム分岐へ渡る可能性があった。
- 対応: Bayer〜Stucki の 8 種類に対応する 0〜7 へ正規化してから enum を保持する。
- 価値または懸念: 未定義 enum 値による処理分岐の不整合を防ぐ。color count / amount / pattern scale は既存の入力ガードを維持した。
- 次に確認すべきこと: Algorithm の property UI が enum 名を表示できる場合は、整数表示から名称表示へ改善する。
# 2026-08-02: Procedural Texture Generator の property 導線

- 関連ファイル: `Artifact/include/Effects/Generator/ProceduralTextureGenerator.ixx`
- 気づき: Generator は preset を生成できる一方、getProperties() が空で、幅・高さ・seed・preset を通常の property 導線から編集できなかった。
- 対応: 4 項目の property を公開し、setPropertyValue() を追加した。preset を 0〜5、サイズを 1〜8192 に正規化した。
- 価値または懸念: UI/保存経路から生成設定を編集でき、極端なサイズによる過大なメモリ要求を一定範囲で防げる。プリセット enum の 0〜5 は ArtifactCore の定義に依存する。
- 次に確認すべきこと: Generator の property 名を既存の生成 UI と統一し、seamless/outputFormat などの設定を公開するか判断する。
# 2026-08-02: Procedural Texture の一括設定経路を正規化

- 関連ファイル: `Artifact/include/Effects/Generator/ProceduralTextureGenerator.ixx`
- 気づき: 個別 setter にはサイズ制限を追加したが、setSettings() は設定構造体をそのまま受け入れるため、別経路から範囲外サイズが残る可能性があった。
- 対応: 一括設定後にも width/height を 1〜8192 へ正規化するようにした。
- 価値または懸念: UI 以外の設定復元・プリセット経路でも、生成サイズの安全条件を一貫して適用できる。
- 次に確認すべきこと: settings 内の各 float パラメータにも同様の正規化が必要か、ArtifactCore 側の生成関数の責務と分けて確認する。
# 2026-08-02: Add Noise のサイズ・seed 入力ガード

- 関連ファイル: `Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm`
- 気づき: Add Noise の size は CPU ノイズ座標のスケールとして使われ、seed は CPU/GPU の乱数初期化へ渡る。size の下限だけでは極端な値を許していた。
- 対応: size を有限値かつ 0.1〜64、seed を 0〜9999 に制限した。非有限値は既定値へ戻した。
- 価値または懸念: 過大なノイズスケールや未制限 seed による処理の不安定化を防ぐ。64 の上限は実装上の安全上限であり、仕様上の最大値としては未検証。
- 次に確認すべきこと: GPU 経路で size を未使用のままにするか、CPU と同じノイズスケールを GPU にも実装するか確認する。
# 2026-08-02: Turbulent Displace の変位入力ガード

- 関連ファイル: `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm`
- 気づき: amount、size、octaves、domain warp はノイズ座標・反復・変位量へ直接使われ、size は除算、octaves はループ回数に使われる。
- 対応: amount 0〜1000、size 1〜1024、octaves 1〜12、seed 0〜9999、domain warp 0〜100 に正規化した。float の非有限値は既定値へ戻した。
- 価値または懸念: NaN/Inf、過大な反復回数、極端な変位量による処理時間・座標計算の破綻を防ぐ。上限の一部は実装上の安全上限で未検証。
- 次に確認すべきこと: UI property に min/max を設定し、setter と表示範囲を一致させる。
# 2026-08-02: Wave の波形入力ガード

- 関連ファイル: `Artifact/include/Effects/Wave/WaveEffect.ixx`
- 気づき: Wave の amplitude と frequency はピクセル座標へ加える変位の計算に、phase は三角関数へ、waveType/orientation は分岐へ直接使われる。
- 対応: CPU/GPU 実装の setter を統一し、amplitude を ±4096、frequency を ±10 に制限、非有限値を既定値へ戻した。phase も有限値を確認し、enum 相当の整数は 0/1 に正規化した。
- 価値または懸念: NaN/Inf や範囲外の整数が座標計算・GPU 定数・分岐へ伝播するのを防ぐ。上限は実装上の安全上限として未検証。
- 次に確認すべきこと: Wave の UI property に min/max を設定し、負の frequency/amplitude を仕様として許容するか確認する。
# 2026-08-02: Liquify のブラシ enum・seed 入力ガード

- 関連ファイル: `Artifact/include/Effects/Liquify/LiquifyEffect.ixx`
- 気づき: Liquify の brush type は property 整数から enum へ変換され、turbulence seed はノイズ関数へ直接渡る。既存の float setter にはガードがあるが、この 2 値は未制限だった。
- 対応: CPU/GPU 実装の brush type を Push〜Pucker（0〜5）へ正規化し、seed を 0〜9999 に制限した。
- 価値または懸念: 未定義 brush 分岐や極端な seed の伝播を防ぐ。既存の amount/radius/center/angle/mesh density のガードは維持した。
- 次に確認すべきこと: UI の brush type 表示が enum 名と整数値のどちらを想定しているか確認する。
# 2026-08-02: Bevel の softness 上限ガード

- 関連ファイル: `Artifact/src/Effects/Bevel/BevelEffect.cppm`
- 気づき: Bevel の softness は CPU 側の Gaussian kernel サイズへ変換されるため、下限だけの制限では極端な入力が巨大なカーネルを生成し得る。
- 対応: softness を有限値かつ 0〜64 にクランプし、非有限値は既定値 2 に戻した。
- 価値または懸念: 異常な property 入力による CPU 処理時間・メモリ負荷の急増を防ぐ。64 は実装上の安全上限として未検証。
- 次に確認すべきこと: UI の Softness 最大値を setter の上限と一致させる。
# 2026-08-02: Optics Compensation の投影入力ガード

- 関連ファイル: `Artifact/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm`
- 気づき: Center X/Y は歪み中心、FOV は投影係数へ使われるため、既存の範囲 clamp だけでは NaN/Inf を除外できない。
- 対応: center X/Y と FOV を有限値として検証し、既存範囲へクランプした。非有限値は中心 0.5、FOV 45 の既定値へ戻した。
- 価値または懸念: property 入力由来の不正値が歪み座標・投影計算へ伝播するのを防ぐ。Direction の符号正規化は既存動作を維持した。
- 次に確認すべきこと: FOV 1 度付近の投影計算が意図した補正強度になるか確認する。
# 2026-08-02: Add Noise の Size を CPU/GPU で統一

- 関連ファイル: `Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm`
- 気づき: Size property は CPU setter と property には存在したが、CPU の noiseAt 座標にも GPU shader の乱数座標にも反映されておらず、実質未実装だった。
- 対応: Size でピクセル座標を量子化してノイズ粒度を変える処理を CPU に追加し、同じ値を GPU constant buffer と HLSL へ渡して同じ座標規則を適用した。
- 価値または懸念: Size 設定が実際のノイズ粒度へ反映され、CPU fallback と GPU 経路の挙動差を縮小できる。GPU/CPU の乱数実装自体は既存どおり別実装である。
- 次に確認すべきこと: 実画像で Size 1 未満・大きい Size の見た目と CPU/GPU の粒度差を確認する。
# 2026-08-02: Physical Halation の光学入力ガード

- 関連ファイル: `Artifact/src/Effects/Glow/PhysicalHalationEffect.cppm`
- 気づき: Threshold、Spread、Intensity、Red Diffusion、Softness は Halation の光量・拡散処理へ直接渡され、既存 clamp だけでは非有限値を除外できない。
- 対応: 各 property を有限値として検証し、既存範囲へクランプした。非有限値はヘッダ既定値へ戻した。
- 価値または懸念: NaN/Inf が光学処理設定へ伝播するのを防ぐ。既存の CPU-only 経路は変更していない。
- 次に確認すべきこと: ArtifactCore 側の Halation::Settings の想定範囲と UI property の min/max を揃える。
# 2026-08-02: Residual Glow の履歴入力ガード

- 関連ファイル: `Artifact/src/Effects/Glow/ResidualGlowEffect.cppm`
- 気づき: Residual Glow の radius は kernel サイズへ、decay/history mix は履歴合成の重みへ直接使われる。既存 clamp だけでは非有限値が残る。
- 対応: threshold、radius、intensity、decay、history mix を有限値として検証し、既存範囲へクランプした。非有限値は各既定値へ戻した。
- 価値または懸念: 不正入力による kernel 計算・履歴合成の破綻を防ぐ。履歴サンプリング経路自体は変更していない。
- 次に確認すべきこと: 長時間履歴の decay 上限 0.995 がメモリ保持・残像量の想定と合うか確認する。
# 2026-08-02: Luminescence Caustics の入力ガード

- 関連ファイル: `Artifact/src/Effects/Glow/LuminescenceCausticsEffect.cppm`
- 気づき: Scale は逆数、Evolution は位相、Edge Weight/Intensity/Color Shift はハイライト合成へ直接使われる。既存 clamp だけでは非有限値が残る。
- 対応: 6 つの property を有限値として検証し、既存範囲へクランプした。非有限値は各既定値へ戻した。
- 価値または懸念: 不正入力による逆数・三角関数・光量合成の破綻を防ぐ。既存の CPU 実装と property 名は変更していない。
- 次に確認すべきこと: Evolution の単位（度）を UI 表示やアニメーション側と統一する。
# 2026-08-02: White Balance property 範囲の明示

- 関連ファイル: `Artifact/src/Effects/WhiteBalanceEffect.cppm`
- 気づき: setter 側では温度・Tint・Brightness の範囲を正規化していたが、getProperties() に min/max がなく、編集 UI が入力範囲を認識できなかった。
- 対応: Temperature 1000〜20000 K、Tint/Brightness -1〜1、Preset 0〜6 を property metadata に設定した。
- 価値または懸念: UI と保存・編集経路が setter の既存仕様を共有しやすくなる。Preset の整数表示は既存仕様を維持した。
- 次に確認すべきこと: Preset を整数ではなく選択肢名として扱える property API があるか確認する。
# 2026-08-02: Turbulent Displace の property 範囲同期

- 関連ファイル: `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm`
- 気づき: setter には安全上限を追加済みだったが、getProperties() に min/max がなく、UI からは上限が見えなかった。
- 対応: Amount、Size、Octaves、Seed、Domain Warp の property metadata を setter の範囲と同期した。
- 価値または懸念: UI と復元経路が同じ入力範囲を共有し、過大な変位・反復数を事前に抑えられる。
- 次に確認すべきこと: 同様に metadata が未設定の静止画エフェクトを順次揃える。
# 2026-08-02: Optics Compensation property 範囲同期

- 関連ファイル: `Artifact/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm`
- 気づき: center/FOV の setter には範囲制限がある一方、getProperties() に min/max がなく、UI が制約を事前に表示できなかった。
- 対応: Center X/Y 0〜1、FOV 1〜180、Direction -1〜1 の property metadata を追加した。
- 価値または懸念: UI と setter の範囲が一致し、不正な投影入力を編集段階で抑えやすくなる。
- 次に確認すべきこと: Direction を二択 enum として表示できる property 表現があるか確認する。
# 2026-08-02: Stroke の輪郭入力ガード

- 関連ファイル: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`
- 気づき: Width は CPU の Gaussian kernel と GPU の探索半径へ使われ、Opacity はアルファ合成へ直接使われる。既存の下限・clamp だけでは非有限値や GPU 上限超過を防げなかった。
- 対応: 色は有効な QColor のみ受け入れ、Width を有限値かつ 0〜64、Opacity を有限値かつ 0〜100 に制限した。property metadata も同じ範囲へ同期した。
- 価値または懸念: 不正入力による巨大 kernel・探索範囲や合成値の破綻を防ぐ。64 は GPU 探索実装の上限に合わせた安全上限。
- 次に確認すべきこと: Stroke Color の property picker が不正 QColor を返す場合の UI 側挙動を確認する。
# 2026-08-02: Edge Bloom の光量入力ガード

- 関連ファイル: `Artifact/include/Effects/Glow/EdgeBloomEffect.ixx`
- 気づき: Edge Bloom の radius は CPU の kernel サイズ、threshold/edge boost/amount/tint mix はハイライト抽出と合成へ使われる。既存 clamp だけでは非有限値が残る。
- 対応: 5 つの setter で有限値を検証し、既存範囲へクランプした。非有限値は各既定値へ戻した。
- 価値または懸念: 不正入力による kernel・閾値・光量計算の破綻を防ぐ。CPU/GPU 共通の setter で同じ条件を適用する。
- 次に確認すべきこと: 実画像で radius 上限 32 と CPU/GPU の bloom 広がりが十分か確認する。
# 2026-08-02: Twist Transform の角度入力ガード

- 関連ファイル: `Artifact/include/Effects/Transform/TwistTransform.ixx`
- 気づき: Twist Transform の Angle は property から直接 float へ代入され、非有限値や極端な角度をそのまま保持していた。
- 対応: Angle を有限値かつ ±720 度に制限し、非有限値は既定値 45 度へ戻した。property metadata にも同じ範囲を追加した。
- 価値または懸念: 変形係数へ不正値が伝播するのを防ぐ。実際の field 適用・再描画通知は既存コメントどおり未実装であり、今回の範囲では変更していない。
- 次に確認すべきこと: GeometryTransform の field 適用契約と再描画経路を確認してから、変形本体を実装する。
# 2026-08-02: Bend Transform の property 導線

- 関連ファイル: `Artifact/include/Effects/Transform/BendTransform.ixx`
- 気づき: Bend Transform は angle/direction/size の setter だけがあり、property 公開と setPropertyValue() が未実装だった。
- 対応: 3 パラメータの有限値・範囲ガード、getProperties()、setPropertyValue()、min/max metadata を追加した。
- 価値または懸念: GeometryTransform の設定を既存 property 編集・保存経路へ載せられる。field 適用と実際の変形本体は既存の未実装範囲であり、今回も変更していない。
- 次に確認すべきこと: Bend の field 適用契約と再描画経路を確認してから変形本体を実装する。
# 2026-08-02: SurfaceFX の有限値正規化

- 関連ファイル: `Artifact/include/Effects/SurfaceFX/SurfaceFXEffect.ixx`
- 気づき: SurfaceFX は矩形・要素の property を既に正規化していたが、std::clamp は NaN/Inf を排除しないため、anchor/element 座標や時間値が不正状態になり得た。
- 対応: property setter 内に有限値付き clamp を追加し、座標・サイズ・feather・強度・opacity・roughness・rotation・in/out time を補正した。既存の矩形境界・時間順序の正規化は維持した。
- 価値または懸念: SurfaceFX の設定復元・編集経路で非有限値がレンダリング設定へ残るのを防ぐ。out time の非有限値は「未指定」を表す -1 に戻した。
- 次に確認すべきこと: SurfaceFXData のデフォルト値と property fallback の中立値を完全に揃える。
# 2026-08-02: Corner Pin の座標入力ガード

- 関連ファイル: `Artifact/src/Effect/ArtifactCornerPinEffect.cppm`
- 気づき: Corner Pin の 8 点座標は property から double を直接保持し、非有限値や property metadata の上限超過をそのまま homography 計算へ渡していた。
- 対応: 入力を有限値として検証し、既存 property の ±100000 範囲へクランプした。非有限値は 0 に戻した。
- 価値または懸念: OpenCV の homography/warpPerspective へ NaN/Inf が伝播するのを防ぐ。座標系（ピクセル値か正規化値か）は既存仕様を維持し、今回変更していない。
- 次に確認すべきこと: 退化四辺形で computeHomography が有限だが不安定な行列を返す場合の判定基準を確認する。
# 2026-08-02: PBR Material の材質入力ガード

- 関連ファイル: `Artifact/include/Effects/Render/PBRMaterialEffect.ixx`
- 気づき: PBR 材質の QColor は無効値を受け入れ、float 値は std::clamp/max だけで NaN/Inf を除外していなかった。Emissive Intensity は property metadata の上限 100 と setter が不一致だった。
- 対応: 色は有効な QColor のみ受け入れ、Metallic/Roughness/AO/Emissive Intensity を有限値として検証した。Emissive Intensity を 0〜100 に揃えた。
- 価値または懸念: レンダリング材質へ不正な色・係数が流れるのを防ぎ、property metadata と setter の範囲を一致させる。
- 次に確認すべきこと: toMaterial() が emissiveColor/AO/emissiveIntensity を Material へ反映できる API を確認する。
# 2026-08-02: IES Light の property setter 統一

- 関連ファイル: `Artifact/include/Effects/Light/IESLightEffect.ixx`
- 気づき: IES Light は setter に範囲処理がある一方、setPropertyValue() がメンバーへ直接代入しており、範囲・有限値検証を迂回していた。IES path も空白だけの値をそのまま保持していた。
- 対応: Intensity/Temperature を有限値付きの setter 経由へ統一し、property min/max を追加した。UseTemperature も setter 経由にし、IES path は trim してから load するようにした。
- 価値または懸念: UI・保存復元経路でも光源係数と温度の入力条件が一貫する。Intensity の 1000 上限は実装上の安全上限として未検証。
- 次に確認すべきこと: loadIES() の実ファイル存在・LM-63 parse 結果を返す責務を render pipeline と整理する。
# 2026-08-02: HDR Display の出力設定入力ガード

- 関連ファイル: `Artifact/include/Effects/ColorCorrection/HDRDisplayEffect.ixx`
- 気づき: HDR Display は property setter を迂回して係数を直接代入し、DisplayMode も任意整数を enum として保持できた。
- 対応: Peak Nits、Paper White、Saturation Boost を有限値付き setter 経由へ統一し、DisplayMode を 0〜3 に正規化した。property metadata にも範囲を追加した。
- 価値または懸念: HDR 出力係数・モードの不正値が後段のトーンマッピングへ伝播するのを防ぐ。
- 次に確認すべきこと: HDR10/HLG/scRGB の実際の出力変換実装がどの層にあるか確認する。
# 2026-08-02: Linear Field の評価入力ガード

- 関連ファイル: `Artifact/include/Effects/Field/LinearField.ixx`
- 気づき: start/end は有限値へ正規化されていたが、evaluateAt() の worldPos は未検証で、非有限座標が influence へ伝播する可能性があった。
- 対応: worldPos の各成分と投影 t を有限値検証し、不正値は influence 0 にフォールバックした。
- 価値または懸念: Field 評価結果へ NaN/Inf が残るのを防ぐ。GPU バッファ生成 TODO は API 契約未確認のため変更していない。
- 次に確認すべきこと: ArtifactAbstractField の GPU データ契約を確認し、CPU と同じ LinearField 情報を安全に公開できるか判断する。
# 2026-08-02: Field 評価座標の有限値ガード拡張

- 関連ファイル: `Artifact/include/Effects/Field/RadialField.ixx`, `SphericalField.ixx`, `BoxField.ixx`
- 気づき: 各 Field の設定 setter は一部の値を正規化していたが、evaluateAt() の worldPos と距離計算結果は未検証だった。
- 対応: 3 種類の Field で worldPos 各成分と算出距離を有限値検証し、不正値は influence 0 にフォールバックした。
- 価値または懸念: Field 評価結果の NaN/Inf 伝播を防ぐ。GPU バッファ生成 TODO は引き続き API 契約未確認のため未変更。
- 次に確認すべきこと: ArtifactAbstractField の評価呼び出し側で不正座標を早期除外できるか確認する。
# 2026-08-02: PBR Material の材質反映漏れを補完

- 関連ファイル: `Artifact/include/Effects/Render/PBRMaterialEffect.ixx`, `ArtifactCore/include/Material/Material.ixx`
- 気づき: PBRMaterialEffect は albedo/metallic/roughness だけを toMaterial() へ反映し、既に保持していた emissive color/intensity と ambient occlusion が Material へ渡っていなかった。
- 対応: 既存 Material API の setEmissionColor、setEmissionStrength、setOcclusionStrength を使い、3 値を toMaterial() へ反映した。
- 価値または懸念: property 編集した発光・AO 設定が実際の材質変換結果にも反映される。
- 次に確認すべきこと: MaterialRender 側が emission/occlusion を利用する経路と、テクスチャ未設定時の fallback を確認する。
# 2026-08-02: FrameCache の eviction policy 正規化

- 関連ファイル: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 気づき: CachePolicy は 5 種類の enum だが、setter は任意の enum 値をそのまま保持でき、evictOne() の default 分岐へ意図せず落ちる可能性があった。
- 対応: setPolicy() で LRU〜Size の 0〜4 に正規化してから候補キューを再構築するようにした。
- 価値または懸念: キャッシュ eviction の挙動を定義済み policy に限定できる。policy の UI/property 導線自体は別途確認が必要。
- 次に確認すべきこと: キャッシュ設定の保存・復元経路で CachePolicy をどの形式で扱っているか確認する。
# 2026-08-02: Progressive Renderer の品質設定正規化

- 関連ファイル: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 気づき: RenderQuality は 4 種類だが setQuality() が任意 enum を保持でき、downsampling も下限だけで過大値を許していた。
- 対応: Quality を Draft〜Custom の 0〜3 に正規化し、draft/preview downsampling を 1〜64 に制限した。qualityChanged は正規化後の値を通知する。
- 価値または懸念: 未定義品質分岐や極端なプレビュー縮小率を防ぐ。64 の上限は実装上の安全上限として未検証。
- 次に確認すべきこと: downsampling の UI 最大値と、Custom 品質の実際の扱いを確認する。
# 2026-08-02: RenderPerformanceMonitor の FPS・空統計ガード

- 関連ファイル: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 気づき: 平均フレーム時間が 0 の初期状態やゼロ時間フレーム時に、fpsChanged の平均 FPS 計算が 0 除算になる可能性があった。また未計測状態を performance acceptable と判定していた。
- 対応: 平均フレーム時間が正の場合のみ FPS を計算し、それ以外は 0 を通知する。未計測状態は acceptable を false とした。
- 価値または懸念: 初期状態・ゼロ時間計測で Inf/NaN が UI へ流れるのを防ぐ。ゼロ時間フレームを許容する既存 recordFrameRender の仕様は維持した。
- 次に確認すべきこと: performance monitor の初期表示で未計測状態を別ステータスとして表示するか検討する。
# 2026-08-02: Luma Key setter の入力検証統一

- 関連: `Artifact/include/Effects/Keying/LumaKeyEffect.ixx`, `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`
- 事実: プロパティ経由では閾値と softness を補正していたが、CPU 実装の setter 直接呼び出しは未検証だった。
- 対応: low/high threshold と softness に有限値・範囲補正を追加し、プロパティの hard range も 0..1 に統一した。
- 価値: UI 経由と内部 API 経由で異常値の扱いが分岐せず、Luma Key のアルファ計算を安定させられる。
- 次に確認: GPU/runtime 側の keying 実装が追加される場合は同じ制約を共有する。
# 2026-08-02: Difference Key setter の入力検証統一

- 関連: `Artifact/include/Effects/Keying/DifferenceKeyEffect.ixx`, `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`
- 事実: プロパティ経由では threshold と softness を補正していたが、CPU 実装の直接 setter は未検証だった。
- 対応: 両 setter に有限値・範囲補正を追加し、プロパティの hard range を soft range と一致させた。
- 価値: Difference Key の距離計算で異常な閾値や softness が内部 API から混入する経路を減らした。
- 次に確認: GPU/runtime 側の keying 実装が追加される場合は同じ制約を共有する。
# 2026-08-02: Chroma Key property range の明示化

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`
- 事実: setter 側では similarity、smoothness、spill reduction の範囲補正が存在したが、プロパティ metadata に範囲が設定されていなかった。
- 対応: 3 プロパティの soft/hard range を setter の制約と一致させ、無効な QColor を無視するようにした。
- 価値: UI 側の入力制御と実行時の制約が一致し、無効色による意図しないキー色変更を防ぐ。
- 次に確認: GPU/runtime keying の実装時には CPU と同じ色空間・距離定義を受け入れ条件として整理する。
# 2026-08-02: Keying CPU の非有限画素伝播防止

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`, `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`
- 事実: 画素値が NaN/Inf の場合、色距離計算からアルファへ非有限値が伝播する可能性があった。
- 対応: 色距離または入力アルファが非有限の場合は、その画素の出力アルファを 0 にして判定を継続する。
- 価値: 不正な入力画素がキーイング結果全体へ NaN を広げることを防ぐ。
- 次に確認: GPU/runtime 実装では shader 側でも同等の非有限値ポリシーを定義する。
# 2026-08-02: Text Tool 作成入口の堅牢化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: Text Tool は既に point/box text の作成動線を持っていたが、レイヤー名がレイヤー数依存で削除後の重複を防げず、座標・サイズの非有限値も入口で拒否していなかった。
- 対応: 既存の `uniqueLayerNameForCurrentComposition` を利用し、キャンバス座標と box size の有限値を検証してから Text Layer を作成するようにした。
- 価値: Text Tool の作成操作が既存レイヤー名と衝突せず、異常な入力で壊れたテキストレイヤーを生成しにくくなる。
- 次に確認: 実機 UI で point text と box text の編集開始・undo を確認する（ビルド未実行）。
# 2026-08-02: ディスクプレビュー容量変更の即時 eviction

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: ディスクキャッシュ容量の変更は上限値を更新するだけで、既存キャッシュの超過分は次回フレーム書き込みまで残っていた。
- 対応: 容量設定変更時に既存の global budget enforcement を再利用し、削除された現在キャッシュのフレーム状態も更新するようにした。
- 価値: 設定変更直後からディスク使用量を新しい上限へ近づけられる。
- 次に確認: ビルド後、容量を下げた際の eviction と manifest 再生成を実機で確認する。
# 2026-08-02: OCIO working/display/view setter の入力検証

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: working space、display、view の setter は任意の文字列を状態と `OCIOConfig` に渡していた。
- 対応: trim 後に有効な config の候補一覧で検証し、存在しない値は無視するようにした。
- 価値: 不正な色空間や表示 view が transform へ流れ、不要な fallback を発生させる経路を減らした。
- 次に確認: カスタム OCIO config の display/view 切り替えと GPU descriptor 再生成を実機で確認する。
# 2026-08-02: OCIO 設定ファイルパスの正規化

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: `loadConfigFile` は入力パスをそのまま Core/OCIO に渡しており、空白や存在しないパスを明示的に拒否していなかった。
- 対応: trim 後に絶対パス化し、存在確認を通過したパスだけを Core と OCIO に渡すようにした。
- 価値: 設定読み込み失敗を早期に判定し、Core と OCIO で異なるパス解決になる可能性を減らした。
- 次に確認: カスタム config の相対パス運用が必要な場合は、呼び出し側の基準ディレクトリ仕様を確認する。
# 2026-08-02: Layer Effect Envelope の非有限出力防止

- 関連: `Artifact/src/Animation/ArtifactLayerEffectEnvelope.cppm`
- 事実: envelope の時間補間は clamp 済みだったが、effectStart/effectEnd が NaN/Inf の場合は補間結果へ伝播した。
- 対応: サンプリング時に start/end を有限値へ fallback してから補間するようにした。
- 価値: procedural animation の共通 envelope が異常パラメータから NaN を生成しない。
- 次に確認: エフェクト側の envelope 接続時に start/end の意図した範囲を UI metadata と揃える。
# 2026-08-02: Brush Tool パラメータの有限値検証

- 関連: `Artifact/include/Tool/ArtifactBrushTool.ixx`
- 事実: ブラシの各 setter は clamp を行っていたが、NaN/Inf に対する `std::clamp` は値を保持し得た。
- 対応: radius、opacity、flow、硬さ、spacing、角度、jitter、scatter、pressure、tilt に有限値 fallback を追加した。
- 価値: ブラシ描画へ非有限パラメータが入り、サイズ・間隔・色付け計算が壊れる経路を減らした。
- 次に確認: 実機の筆圧・傾き入力と clone brush のパラメータ同期を確認する。
# 2026-08-02: Brush Tool 色成分の有限値検証

- 関連: `Artifact/include/Tool/ArtifactBrushTool.ixx`
- 事実: ブラシ色の各成分も clamp のみで、NaN/Inf が色状態に残る可能性があった。
- 対応: RGB は 0、alpha は 1 を fallback とする有限値検証を追加した。
- 価値: 異常なブラシ色がペイント処理や合成へ伝播する経路を塞いだ。
- 次に確認: カラーピッカーからの alpha と HDR 色入力の仕様を確認する。
# 2026-08-02: Brush Tool ストローク座標の有限値検証

- 関連: `Artifact/src/Tool/ArtifactBrushTool.cppm`
- 事実: mouse press/move/release の座標を `QLineF` とストローク点へ直接渡していた。
- 対応: press は非有限座標を拒否し、move は無視、release は最後の有効点だけで確定するようにした。
- 価値: 壊れた座標入力がペイントストロークや距離計算へ混入する経路を防ぐ。
- 次に確認: 高 DPI やタブレット入力での座標変換境界を実機確認する。
# 2026-08-02: Motion Sketch 入力の有限値検証

- 関連: `Artifact/src/Tool/ArtifactMotionSketchTool.cppm`
- 事実: スケッチ座標と smoothing/sample rate の setter は clamp 前に非有限値を拒否していなかった。
- 対応: begin/add の座標を検証し、異常座標を拒否。smoothing は 0.5、sample rate は 60fps を非有限値の fallback とした。
- 価値: 不正な座標や時間間隔が keyframe 生成へ伝播する経路を減らした。
- 次に確認: タブレット入力・高 DPI 座標変換後のスケッチ再生を実機確認する。
# 2026-08-02: Puppet Tool 入力の有限値検証

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: pin 座標、rotation、weight、depth、hit-test threshold は clamp/fmod 前に非有限値を拒否していなかった。
- 対応: 異常座標を拒否し、各数値に有限値 fallback と範囲制約を追加した。
- 価値: Puppet pin の位置・変形・ヒットテストが異常入力で壊れる経路を減らした。
- 次に確認: 2D rig の pin 編集と undo/redo を実機確認する。
# 2026-08-02: Puppet engine 同期時の pin 検証

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: engine へ同期する経路では PinRecord の座標・rotation・weight・depth を直接変換していた。
- 対応: 非有限座標の pin を除外し、rotation/weight/depth を有限値・範囲補正してから `PuppetPin` を生成するようにした。
- 価値: setter を経由しない復元・内部状態からの異常値も engine へ伝播しない。
- 次に確認: pin の削除・再 bind 後の engine 状態と描画結果を実機確認する。
# 2026-08-02: Audio Service の音量・パン入力検証

- 関連: `Artifact/src/Service/ArtifactAudioService.cppm`
- 事実: master volume、layer volume、pan は clamp のみで、NaN/Inf が音量・パン状態へ残る可能性があった。
- 対応: 音量は 1.0、pan は 0.0 を有限値 fallback としてから既存範囲へ clamp するようにした。
- 価値: オーディオバスやレイヤーへ非有限値が伝播し、dB 変換やミキサー状態が壊れる経路を減らした。
- 次に確認: ミュート・デバイス切り替えと volume/pan の保存・再読込を実機確認する。
# 2026-08-02: Audio dB 変換の有限値防御

- 関連: `Artifact/src/Service/ArtifactAudioService.cppm`
- 事実: レイヤー値から直接呼ばれる `linearToDecibels` は NaN/Inf を受ける可能性があった。
- 対応: dB 変換前に有限値を検証し、異常値は linear 1.0 に fallback するようにした。
- 価値: UI setter を経由しない音量値でも、ミキサーへ非有限 dB が伝播しない。
- 次に確認: 読み込み済みレイヤーの異常音量を含むプロジェクトで再生状態を確認する。
# 2026-08-02: Motion Sketch 確定前のサンプル列検証

- 関連: `Artifact/src/Tool/ArtifactMotionSketchTool.cppm`
- 事実: begin/add では入力検証しているが、内部 sampledPoints と sampledTimes の不一致・破損を finish 時に検証していなかった。
- 対応: keyframe 生成前に配列長、座標、非負の有限時刻を検証し、異常時は確定を中止するようにした。
- 価値: 壊れたサンプル列から不正な frame keyframe を作成する経路を防いだ。
- 次に確認: 長時間スケッチとキャンセル直後の再開でサンプル列が正しく初期化されることを確認する。
# 2026-08-02: Effect Preset の atomic save

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: effect preset は通常の QFile へ直接書き込み、書き込み途中の失敗や空パスを明示的に扱っていなかった。
- 対応: QSaveFile に変更し、trim 済みパス、完全 write、commit 成功を確認するようにした。
- 価値: プリセット保存中の中断で既存ファイルが壊れる可能性を下げた。
- 次に確認: 保存先フォルダ権限エラーと上書き保存後の再読込を確認する。
# 2026-08-02: Effect Preset 読み込みパスの検証

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: effect preset 読み込みは空パス・空白・不存在ファイルを QFile の open に任せていた。
- 対応: effect の有無、trim 済みパス、ファイル存在を先に検証するようにした。
- 価値: 無効なプリセット入力を早期に拒否し、読み込み失敗の責務を明確にした。
- 次に確認: 互換性のない旧 preset JSON を読み込んだ場合の effect 状態保持を確認する。
# 2026-08-02: Mask Preset の atomic save/load 検証

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: mask preset も通常 QFile へ直接保存し、読み込み時の空パス・不存在確認もなかった。
- 対応: 保存を QSaveFile + 完全 write + commit に変更し、保存/読み込み双方で trim 済みパスを検証するようにした。
- 価値: mask preset の途中書き込みによる破損と無効パスの曖昧な失敗を減らした。
- 次に確認: mask preset の上書き保存後に paths/enabled が保持されることを確認する。
# 2026-08-02: Project Manager 入出力パスの正規化

- 関連: `Artifact/src/Project/ArtifactProjectManager.cppm`
- 事実: 本体プロジェクトの load/save 入口は空白や空パスをそのまま importer/exporter へ渡していた。
- 対応: load は trim・存在・通常ファイルを検証し、save は trim 済みの非空パスを backup/export/hook/状態更新で一貫して使うようにした。
- 価値: 保存先・読み込み元の解決が一貫し、無効パスで既存プロジェクト状態を不必要に変更する経路を減らした。
- 次に確認: 相対パス保存と auto-save/recovery のパス連携を実機確認する。
# 2026-08-02: 非同期 Project load のパス正規化

- 関連: `Artifact/src/Project/ArtifactProjectManager.cppm`
- 事実: `loadFromFileAsync` は入力パスを worker と UI 更新 lambda にそのまま渡していた。
- 対応: 非同期処理開始前に trim・存在・通常ファイルを検証し、以後は normalizedPath を一貫して使用するようにした。
- 価値: 同期/非同期でプロジェクトパスの扱いが分岐せず、無効入力で worker を起動しない。
- 次に確認: 非同期ロード失敗時の callback と UI 状態保持を確認する。
# 2026-08-02: 非同期 Project save のパス正規化

- 関連: `Artifact/src/Project/ArtifactProjectManager.cppm`
- 事実: `saveToFileAsync` は worker、backup、exporter、hook、状態更新へ元の fullpath を個別に渡していた。
- 対応: 開始時に trim 済み非空パスを検証し、以後の全経路で normalizedPath を使うようにした。
- 価値: 同期 save と非同期 save のパス解決差異を減らし、無効パスで worker を起動しない。
- 次に確認: 非同期 save の成功/失敗 callback と backup 生成を確認する。
# 2026-08-02: Project Importer 入力パス検証

- 関連: `Artifact/src/Project/ArtifactProjectImporter.cppm`
- 事実: Manager を経由しない Importer 直接利用では入力パスをそのまま保持し、load 時に QFile の open 結果だけへ依存していた。
- 対応: setter で trim し、JSON 読み込み前に空パス・不存在・ディレクトリを拒否するようにした。
- 価値: Project Importer の直接利用でも同期/非同期 Manager と同じ入力境界になる。
- 次に確認: 相対パスと Unicode パスの import を確認する。
# 2026-08-02: Color Palette Mapping パス検証

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: palette mapping の保存/読み込みは Core manager へ入力パスを直接渡していた。
- 対応: 保存では trim 済み非空パス、読み込みではさらに存在確認を行ってから Core API を呼ぶようにした。
- 価値: effect/mask preset と同じ入力境界を palette mapping にも適用した。
- 次に確認: palette mapping の Unicode パスと上書き保存を確認する。
# 2026-08-02: Shape Layer 数値 setter の有限値検証

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: gradient angle/center/radius、stroke width/taper、corner radius、star inner radius は setter で clamp せず、または NaN/Inf を保持し得た。
- 対応: 各 setter に有限値 fallback と既存の意味に沿った範囲制約を追加した。
- 価値: 静止画・シェイプレイヤーの描画パスへ異常な geometry/style 値が伝播する経路を減らした。
- 次に確認: gradient、rounded shape、star shape の保存/再読込後の描画を確認する。
# 2026-08-02: Shape Layer 色状態の正規化

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Shape Layer の色は描画時に clamp されていたが、fill/stroke/gradient の setter は内部状態へ直接保存していた。
- 対応: RGB/alpha を有限値検証して 0..1 に正規化する共通 helper を追加し、4 系統の色 setter に適用した。
- 価値: 保存・再読込や別 renderer 経由でも異常な色状態が残らない。
- 次に確認: HDR 色入力を Shape Layer に渡す場合の色域仕様を確認する。
# 2026-08-02: Shape Layer カスタム頂点の検証

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: custom polygon/path setter は頂点配列をそのまま内部 geometry へ保存していた。
- 対応: polygon の座標、Bezier vertex の位置と in/out tangent を有限値検証し、異常頂点を除外するようにした。
- 価値: 静止画・シェイプ geometry の path 計算へ NaN/Inf が混入する経路を防いだ。
- 次に確認: 無効頂点を含む custom path の保存/再読込と undo を確認する。
# 2026-08-02: Shape Layer property metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: gradient angle/center/radius と stroke width は setter 側に制約がある一方、Inspector property metadata の範囲が未設定だった。
- 対応: angle、center、radius、stroke width に soft/hard range を追加し、setter の許容範囲と一致させた。
- 価値: Inspector の入力 UI と Shape Layer 実行時の制約が一致する。
- 次に確認: property panel で範囲表示とアニメーション値の編集を確認する。
# 2026-08-02: Shape geometry property metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: width/height、corner radius、star points/inner radius の setter 制約に対し、Inspector metadata の hard/soft range が未設定だった。
- 対応: geometry パラメータへ setter と一致する範囲を追加した。
- 価値: Shape Layer の Inspector から無効なサイズや shape パラメータを入力しにくくなる。
- 次に確認: 大きな canvas size と star/polygon の上限表示を property panel で確認する。
# 2026-08-02: Shape stroke metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: stroke taper と cap/join/align enum は setter 側で範囲制約がある一方、Inspector metadata の hard range が未設定だった。
- 対応: taper を 0..1、cap/join/align を 0..2 として明示した。
- 価値: Shape stroke の UI 入力が実行時の有効範囲から外れない。
- 次に確認: stroke style の property editing と preset 再読込を確認する。
# 2026-08-02: Text Layer 基本 typography metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: font size、tracking、leading の setter は入力を検証していたが、Inspector metadata に範囲がなかった。
- 対応: font size は 1..1000、tracking/leading は -1000..1000 の hard range と実用域の soft range を追加した。
- 価値: Text Layer の typography 編集 UI と実行時の入力制約が一致する。
- 次に確認: point/box text で font size、tracking、leading のアニメーション編集を確認する。
# 2026-08-02: Text Layer stroke/shadow metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: stroke width、shadow offset、shadow blur の setter は有限値・非負制約を持つが、Inspector metadata の範囲が未設定だった。
- 対応: stroke width、shadow offsets、shadow blur に hard/soft range を追加した。
- 価値: Text Layer の描画負荷と表示上の異常値を Inspector から抑制できる。
- 次に確認: shadow blur/offset のアニメーションと preset 再読込を確認する。
# 2026-08-02: Text Layer 色状態の正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: text fill/stroke/shadow の setter は FloatRGBA へ色成分を直接保存していた。
- 対応: RGB/alpha を有限値検証し、0..1 に clamp してから内部状態へ保存するようにした。
- 価値: Text Layer の保存・再読込や別 renderer 経由でも異常な色成分が残らない。
- 次に確認: HDR 色入力を Text Layer に渡す場合の色域仕様を確認する。
# 2026-08-02: Text Layer path text 入力の検証

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: path text の Bezier segments と start/end offset は setter で直接保存されていた。
- 対応: segment の 4 点を有限値検証し、異常 segment を除外。offset は非有限値を 0 に fallback するようにした。
- 価値: path text の文字配置計算へ異常 geometry や非有限 offset が伝播しない。
- 次に確認: open path/closed path の text layout と reverse/offset 編集を確認する。
# 2026-08-02: Text path offset metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: path start/end offset は有限値へ補正されるが、Path Options の Inspector metadata に範囲がなかった。
- 対応: start/end offset に hard -100000..100000、soft -1000..1000 を設定した。
- 価値: path text の offset 編集で過大値を入力しにくくなり、UI と runtime の入力契約が明確になる。
- 次に確認: 長い path と reverse を含む text layout で offset 操作を確認する。
# 2026-08-02: Font Usage manifest の atomic 出力

- 関連: `Artifact/src/Project/ArtifactProjectStatistics.cppm`
- 事実: font usage の JSON/CSV manifest は QFile へ truncate 書き込みしており、途中失敗で既存レポートを壊し得た。
- 対応: QSaveFile に変更し、trim 済みパス、完全 write、commit 成功を確認するようにした。
- 価値: プロジェクト統計出力の破損リスクを下げた。
- 次に確認: JSON のみ、JSON+CSV、書き込み権限エラーの各ケースを確認する。
# 2026-08-02: Project Packager target path validation

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: packager は targetDir の空文字を確認していたが、空白や既存ファイルをディレクトリとして扱うケースを明示していなかった。
- 対応: targetDir を trim し、既存パスが通常ファイルなら packaging を開始しないようにした。
- 価値: Assets 作成や既存ファイル削除へ進む前に、出力先の不整合を検出できる。
- 次に確認: 既存ディレクトリ、未作成ディレクトリ、既存ファイルを target に指定した場合を確認する。
# 2026-08-02: Text Animator preset metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text Animator preset は実装上 0〜7 の ID を扱うが、Inspector metadata に範囲がなかった。
- 対応: animator preset に hard/soft range 0〜7 を追加した。
- 価値: 未定義 preset ID が UI から入力される経路を減らした。
- 次に確認: preset 切り替えと custom animator への復帰を確認する。
# 2026-08-02: Project Packager 欠落アセットの失敗化

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: external file が存在しない場合、copy を skip して元の filePath を残したまま package が成功する可能性があった。
- 対応: 欠落または通常ファイルでない外部ファイルを検出した時点で packaging を失敗させるようにした。
- 価値: 再読込時に参照切れになる不完全 package を生成しない。
- 次に確認: missing asset、permission error、正常 package の各ケースを確認する。
# 2026-08-02: Project Packager 上書き失敗の明示化

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: 既存 Assets ファイルの削除結果を確認せず、copy へ進んでいた。
- 対応: destination の削除に失敗した場合は warning を出して packaging を中断するようにした。
- 価値: ロック・権限エラーで古い asset が残る不完全 package を防ぐ。
- 次に確認: Windows 上の開いている asset と read-only destination の挙動を確認する。
# 2026-08-02: Project Packager の asset preflight

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: 欠落 external file の検出がコピー開始後だったため、後半の欠落で前半だけコピー済みの partial package が残り得た。
- 対応: 全 external file を先に存在・通常ファイル検証してから Assets のコピーを開始するようにした。
- 価値: 欠落アセットで失敗する場合に、packaging の副作用を発生させにくくした。
- 次に確認: 複数アセットのうち一つが欠落するケースで出力先が未変更であることを確認する。
# 2026-08-02: 3Dプリミティブ寸法の非有限値防御

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`、3DプリミティブのJSON復元とProperty Editor入力。
- 事実: 3Dレイヤーの寸法はJSONおよびプロパティ経由で `float` 化され、そのままメッシュ生成に渡り得た。
- 対応: 寸法を有限値・0.01〜100000の範囲へ正規化し、NaN/Infinityは現在値へフォールバックする共通ヘルパーを追加した。
- 価値/懸念: 不正な保存データやUI入力によるメッシュ計算の破綻を局所的に防ぐ。低レベルのDiligent backendやCompositionGraphは変更していない。
- 次に確認: 3D材質値の上限・有限値保証がMaterial側の契約と一致するか、ビルド許可後に確認する。
# 2026-08-02: 3D材質のJSON復元と入力範囲

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- 事実: 3DレイヤーのJSON出力にはベースカラーと材質係数が含まれていたが、復元側ではテクスチャ以外の材質値を反映していなかった。
- 対応: ベースカラー、metallic、roughness、opacityを復元し、材質プロパティの範囲と有限値を入力経路で正規化した。normal/occlusion strengthも範囲を設定した。
- 価値/懸念: 保存→再読込で材質状態が欠落する問題を縮小する。Material側の既定値・上限契約は未検証。
- 次に確認: ビルド許可後に既存プリセットとの互換性と、3Dレンダラーへの材質値伝播を確認する。
# 2026-08-02: キーイング色入力の正規化

- 関連: `Artifact/include/Effects/Keying/ChromaKeyEffect.ixx`、`Artifact/include/Effects/Keying/DifferenceKeyEffect.ixx`、`Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`。
- 事実: QColor経由の不正色をDifference Keyが受け入れる経路があり、直接のFloatRGBA setterも有限値・範囲を保証していなかった。
- 対応: 無効なQColorを無視し、Chroma/Differenceの色チャンネルを有限値かつ0〜1へ正規化した。
- 価値/懸念: キー色の不正値がCPUキー処理へ伝播する可能性を減らす。GPU runtime受入れそのものは未検証。
- 次に確認: ビルド許可後、3種類のKey効果を実画像で比較し、アルファ境界とspillの品質を確認する。
# 2026-08-02: 連番画像のフレームレート境界

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`。
- 事実: 連番画像のフレームレートは有限値チェックのみで、非常に大きい正数をそのまま `ImageSequenceSource` に渡し得た。
- 対応: API入力とJSON復元の両方でフレームレートを `0.001〜1000 FPS` に正規化した。
- 価値/懸念: 異常な保存値によるフレーム番号計算・先読み負荷の暴走を抑える。実際の高FPS素材の運用上限は未検証。
- 次に確認: 実機で連番の保存→再読込→タイムラインフレーム切替を確認する。
# 2026-08-02: 静止画レイヤーのCrop入力境界

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`sourceCrop` のProperty Editor経路。
- 事実: Crop位置・サイズ、pan、zoom、rotation、anchorは `QVariant` から直接double化され、非有限値や極端な値を受け得た。
- 対応: 各入力を有限値チェックと妥当な範囲へ正規化してから `SourceCrop` に渡すようにした。
- 価値/懸念: 静止画・連番画像の編集時に変換行列やCrop計算へ不正値が伝播する可能性を減らす。範囲はUI操作上の安全上限として設定しており、仕様上の最大値は未検証。
- 次に確認: ビルド許可後、極端なCrop/Zoom値を保存・再読込して表示が安定するか確認する。
# 2026-08-02: Text Wiggly Animatorの復元値正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- 事実: Text AnimatorのWiggly設定と変形値はJSONからfloatへ直接変換され、非有限値や範囲外の値が復元され得た。
- 対応: Wigglyの周波数・相関・位相と、Animatorのscale/opacity等を有限値・妥当範囲へ正規化し、Wiggles/SecのPropertyにもhard rangeを追加した。
- 価値/懸念: プロシージャル文字アニメーションの保存→再読込で異常値が評価へ流れる可能性を減らす。実時間品質は未検証。
- 次に確認: ビルド許可後、Wiggly有効状態の保存・再読込と長時間再生を確認する。
# 2026-08-02: 画像素材の色空間メタデータ正規化

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`。
- 事実: `setInputInterpretation()` では色空間名をtrimしていたが、JSON復元では直接代入していた。
- 対応: 画像レイヤーの保存復元でも input color space / transfer function をtrimして、素材解釈の同一性を保つようにした。
- 価値/懸念: UI経由とプロジェクト再読込で色空間名の扱いがずれる可能性を減らす。実OCIO config上の名称存在確認はManager側の責務として残している。
- 次に確認: 複数のOCIO configで素材別解釈を保存・再読込し、変換結果を比較する。
# 2026-08-02: 3Dモデル入力パスの検証

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- 事実: 3Dモデル読み込みは空文字以外をそのままImporterへ渡し、空白付き入力や存在しないファイルでもImporter処理へ進んでいた。
- 対応: 入力をtrimし、通常ファイルの存在を確認してからImporterを呼ぶようにした。既存の読み込み失敗時のfallback挙動は維持した。
- 価値/懸念: 無効パスによるImporter呼び出しと曖昧な失敗を減らす。URI/仮想ファイルパスは未対応のまま。
- 次に確認: ビルド許可後、相対パス・空白付きパス・欠損ファイルの復元挙動を確認する。
# 2026-08-02: Footage解釈サービスのFPS入力境界

- 関連: `Artifact/src/Service/FootageInterpretService.cppm`。
- 事実: Footageのフレームレート変更は `<= 0` のみを検査しており、NaNや極端に大きい値が preflight / 適用処理へ進み得た。
- 対応: 有限値チェックと `0.001〜1000 FPS` の範囲正規化を preflight と適用経路へ追加した。
- 価値/懸念: 素材解釈変更に伴う時間比率計算の異常を抑制する。高FPS素材の上限は未検証。
- 次に確認: ビルド許可後、KeepTime / KeepKeyframes 各モードで境界値を確認する。
# 2026-08-02: 3D材質の保存項目補完

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- 事実: 3D材質のJSONにはベースカラー・metallic・roughness・opacityとテクスチャはあったが、emission color/strength、normal strength、occlusion strengthが出力されていなかった。
- 対応: 上記の材質値をJSONへ追加し、復元時にも有限値・範囲正規化して反映するようにした。
- 価値/懸念: 3Dレイヤーの保存→再読込で材質表現が欠落する範囲を縮小する。既存ファイルには既定値で後方互換する。
- 次に確認: ビルド許可後、材質各値の変更を保存・再読込してレンダー結果を比較する。
# 2026-08-02: 音声レイヤーの音量・パン有限値防御

- 関連: `Artifact/src/Layer/ArtifactAudioLayer.cppm`。
- 事実: 音声レイヤーの `setVolume` / `setPan` は clamp のみで、NaN入力ではNaNが残る可能性があった。JSON復元とProperty入力も同じsetterを通る。
- 対応: 音量・パンを有限値確認後に既存範囲へclampし、無効値はそれぞれ既定値へ戻すようにした。
- 価値/懸念: 音声ミックスとキャッシュ比較に不正値が伝播する可能性を減らす。音声DSP自体は変更していない。
- 次に確認: ビルド許可後、境界値の保存・再読込と再生キャッシュ無効化を確認する。
# 2026-08-02: 音声サンプルレート不正値の防御

- 関連: `Artifact/src/Layer/ArtifactAudioLayer.cppm`。
- 事実: WAVロード後のduration計算はサンプルレートが0でも除算し得た。
- 対応: サンプルレートが正の場合のみdurationを計算し、0以下なら未ロード扱いにした。
- 価値/懸念: 壊れた音声メタデータによる無限値の伝播を防ぐ。デコーダーの検証自体は変更していない。
- 次に確認: ビルド許可後、無効サンプルレートの音声ファイルでロード失敗とUI表示を確認する。
# 2026-08-02: Cameraレイヤーの光学・手ぶれ入力境界

- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- 事実: FOV、正投影サイズ、clip、IPD、手ぶれ減衰・周波数の一部setterは有限値や上限を保証していなかった。
- 対応: Property/JSON/APIの共通setterで有限値確認と既存UI範囲相当のclampを行うようにした。
- 価値/懸念: 投影行列や手ぶれ計算へNaN・極端な値が伝播する可能性を減らす。カメラの実レンダリング挙動は未検証。
- 次に確認: ビルド許可後、Perspective / Orthographic / Stereo各モードで境界値を保存・再読込する。
# 2026-08-02: Camera手ぶれ振幅ベクトルの正規化

- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- 事実: 手ぶれ位置・回転の振幅setterは `QVector3D` をそのまま保持し、JSON復元時の非有限成分を防いでいなかった。
- 対応: 位置振幅を0〜10000、回転振幅を0〜360へ各軸ごとに正規化し、非有限値は0へ戻すようにした。
- 価値/懸念: 手ぶれ計算へNaNが流れる可能性を減らす。shake offset本体は別の外部制御値として維持した。
- 次に確認: ビルド許可後、軸ごとの振幅保存・再読込と手ぶれプレビューを確認する。
# 2026-08-02: Camera手ぶれオフセットの入力防御

- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- 事実: 手ぶれオフセット・回転本体のsetterはベクトル値を直接保持していた。
- 対応: 各軸を有限値確認し、位置は±100000、回転は±360000度に制限した。
- 価値/懸念: view matrix生成へ非有限値が伝播する可能性を減らす。外部制御側が意図的に極端な値を使う契約は未確認。
- 次に確認: ビルド許可後、shake APIとJSON復元の両方で軸別入力を確認する。
# 2026-08-02: レンダーキュー出力FPSの有限値防御

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: 出力設定のFPSは `std::clamp` のみで正規化され、NaN入力では不正値が残る可能性があった。
- 対応: 有限値を確認して1〜240 FPSへclampし、無効値は30 FPSへフォールバックするようにした。
- 価値/懸念: レンダー設定からフレーム時間計算・エンコーダー引数へNaNが伝播する可能性を減らす。実エンコーダー受入れは未検証。
- 次に確認: ビルド許可後、レンダーキュー設定の保存・再読込と境界FPSでの出力を確認する。
# 2026-08-02: レンダーキューOverlay変換値の正規化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: Overlayのoffset・scale・rotation setterはscale以外の有限値確認がなく、scaleもNaNを防げなかった。
- 対応: offsetを±100000、scaleを0.05〜8、rotationを±360000度に正規化し、非有限値は既定値へ戻すようにした。
- 価値/懸念: 最終出力合成へ不正な変換値が伝播する可能性を減らす。出力品質の実機確認は未実施。
- 次に確認: ビルド許可後、Overlay付きジョブの保存・再実行と境界値を確認する。
# 2026-08-02: レンダージョブ一括更新時の設定正規化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: 個別setterには出力設定の範囲制限があったが、`updateJob` はジョブ構造体を直接置換していた。
- 対応: 一括更新経路でも解像度・FPS・ビットレート・音声ビットレート・音声サンプルレートを正規化するようにした。
- 価値/懸念: API経路によるsetter迂回で不正なレンダー設定が残る可能性を減らす。codec等の文字列正規化は既存経路の責務として残した。
- 次に確認: ビルド許可後、個別更新と一括更新で同じ設定結果になるか確認する。
# 2026-08-02: レンダージョブ一括更新時のフレーム範囲整合

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: 個別のフレーム範囲setterは開始0以上・終了が開始以上を保証していたが、`updateJob` の構造体置換はこの整合を迂回していた。
- 対応: 一括更新後にも開始・終了フレームの順序と下限を正規化するようにした。
- 価値/懸念: 不正な範囲による空レンダーや負フレーム指定を減らす。上限値は既存仕様に合わせて未追加。
- 次に確認: ビルド許可後、逆転範囲を含むジョブの保存・再実行を確認する。
# 2026-08-02: Camera JSON復元のsetter迂回補正

- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- 事実: Camera FOV と shake trauma はJSON復元時にメンバーへ直接代入され、setter側で追加した有限値・範囲保証を迂回していた。
- 対応: JSON復元経路にも同じFOV / traumaの正規化を明示的に適用した。
- 価値/懸念: API入力と保存復元でカメラ値の安全性が揃う。manual FOVの状態遷移は既存挙動を維持した。
- 次に確認: ビルド許可後、異常値JSONでPerspectiveカメラを再読込して状態を確認する。
# 2026-08-02: 既存Footage FPSの異常状態からの復旧

- 関連: `Artifact/src/Service/FootageInterpretService.cppm`。
- 事実: 新しいFPSは検証していたが、Footageに既にNaNが入っている場合、旧FPSとの比率計算へ進む可能性が残っていた。
- 対応: currentOverrideで異常FPSを無効値として扱い、変更適用時も旧FPSが有限値でない場合は比率計算を行わず新しいFPSへ置き換えるようにした。
- 価値/懸念: 壊れた既存プロジェクトからの再解釈でNaN比率が広がる可能性を減らす。
- 次に確認: ビルド許可後、異常FPSを含むプロジェクトのFootage再解釈を確認する。
# 2026-08-02: OCIO設定JSONの文字列正規化

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`。
- 事実: 通常のOCIO設定setterはtrimと有効値確認を行う一方、`fromJson` は設定名を直接代入していた。
- 対応: preset / working space / display / view / looks のJSON復元値をtrimして、保存形式の余白差を除去した。
- 価値/懸念: UI設定とプロジェクト復元時の名前不一致を減らす。有効値のconfig照合は既存のconfigロード後状態に依存する。
- 次に確認: ビルド許可後、カスタムOCIO configで余白付き設定の復元を確認する。
# 2026-08-02: OCIO設定復元時の候補整合

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`。
- 事実: JSON復元後のworking space / display / viewは、active configの候補一覧と照合されず無効名が残り得た。
- 対応: config復元後に候補一覧を確認し、無効な値はworking space・display・viewそれぞれの既定候補へ戻してconfigへ反映するようにした。
- 価値/懸念: OCIO実運用で保存された設定とconfig差し替えがあっても無効な表示変換状態を減らす。looksの候補照合は既存APIの制約上未追加。
- 次に確認: ビルド許可後、config差し替え後のJSON復元で表示・view変換が有効になるか確認する。
# 2026-08-02: OCIO Looks入力の正規化

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`。
- 事実: working/display/viewはsetterでtrimしていたが、Looksだけは未加工文字列をconfigへ渡していた。
- 対応: Looksもtrim済みの値を保持・config反映するよう統一した。
- 価値/懸念: Looks指定の余白差による表示変換不一致を減らす。Look名候補の列挙APIは未提供のため照合はしていない。
- 次に確認: ビルド許可後、Looks指定付きOCIO表示変換を保存・再読込する。
# 2026-08-02: プロジェクト設定JSONの文字列正規化

- 関連: `Artifact/src/Project/ArtifactProjectSetting.cppm`。
- 事実: 設定JSONの name / author は前後空白を保持したまま復元され、バリデーション上の表示名と保存値がずれる可能性があった。
- 対応: JSON復元時に両フィールドをtrimした。
- 価値/懸念: プロジェクト設定の再読込で不要な空白が残る問題を減らす。API setterの既存挙動は変更していない。
- 次に確認: ビルド許可後、空白付き設定の保存・再読込とバリデーション表示を確認する。
# 2026-08-02: 一括更新時のOverlay値整合

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: `setJobOverlayTransform` は値を正規化していたが、`updateJob` の構造体置換ではOverlay offset/scale/rotationが未検証だった。
- 対応: 一括更新経路にも個別setterと同じ有限値・範囲正規化を追加した。
- 価値/懸念: API経路による不正なOverlay変換の再侵入を防ぐ。レンダー品質の実機確認は未実施。
- 次に確認: ビルド許可後、一括更新と個別更新の出力変換が一致するか確認する。
# 2026-08-02: レンダージョブ一括更新時の文字列設定正規化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: `updateJob` は output format / codec / audio codec / channel mode を構造体から直接置換していた。
- 対応: 空値を既定値へ戻し、channel modeを許可された値へ正規化した。
- 価値/懸念: setterを迂回する一括更新でもエンコーダー設定が空・未知値になりにくい。codec profileの詳細検証は既存仕様に委ねている。
- 次に確認: ビルド許可後、個別更新と一括更新で出力設定が一致するか確認する。
# 2026-08-02: レンダージョブ一括更新時のパス・名前正規化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: `updateJob` の構造体置換では job name / output path / audio source path の前後空白が残り得た。
- 対応: 一括更新時にも各文字列をtrimし、空のoutput pathは既存の既定出力パス生成へ渡すようにした。
- 価値/懸念: 同じジョブを個別setterで更新した場合との保存結果の差を減らす。
- 次に確認: ビルド許可後、空白付きパスを含むジョブの保存・再実行を確認する。
# 2026-08-02: プロジェクト設定JSON出力のcanonicalize

- 関連: `Artifact/src/Project/ArtifactProjectSetting.cppm`。
- 事実: JSON復元はtrimしていたが、API setterで設定された前後空白はtoJsonでそのまま出力されていた。
- 対応: メモリ上の値を変更せず、JSON出力時のname / authorだけtrimして保存するようにした。
- 価値/懸念: 保存→再読込で設定値が揺れる問題を減らす。UI上の入力中表示は従来どおり。
- 次に確認: ビルド許可後、空白付き入力のメモリ値と保存値の意図した差を確認する。
# 2026-08-02: PaintレイヤーのClone Stamp入力境界

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: Clone Stampの座標・半径・opacity・hardnessを直接計算し、半径NaNではピクセル領域の整数化へ進み得た。default canvasサイズもJSON値をそのまま採用していた。
- 対応: 座標と数値の有限値を確認し、半径を0.5〜10000へ制限、default width/heightを1〜100000へ正規化した。
- 価値/懸念: 静止画ペイント操作と保存復元で不正値がバッファ計算へ流れる可能性を減らす。通常のブラシ描画経路は既存処理を維持した。
- 次に確認: ビルド許可後、Clone Stampの境界値と異常JSON復元を確認する。
# 2026-08-02: Paintブラシストロークの有限値防御

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: ブラシ半径・揺らぎ係数・角度・点座標を描画ループで直接使用していた。
- 対応: ストローク数値の有限値を入口で確認し、各点の非有限座標はスキップするようにした。
- 価値/懸念: NaNによる整数化・ブラシ範囲計算の破綻を防ぐ。無効ストロークは描画せず、既存のundo状態も作成しない。
- 次に確認: ビルド許可後、通常ブラシの異常入力とundo/redo状態を確認する。
# 2026-08-02: PaintフレームJSONのメモリ上限

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: PaintフレームJSONのwidth/heightを検査せず、pixel payloadの検証前に画像をresizeしていた。
- 対応: 寸法を最大16384に制限し、RGBA32F payloadが512 MiBを超える場合は復元を拒否するようにした。
- 価値/懸念: 不正または破損したプロジェクトによる巨大メモリ確保を抑制する。大判ペイント素材の上限は未検証。
- 次に確認: ビルド許可後、大判フレームと破損Base64の復元失敗が安全に扱われるか確認する。
# 2026-08-02: Text Animator selector復元値の正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- 事実: Animatorのrange start/end/offset/easeとpositionはJSONから直接float化され、前回のWiggly正規化対象外だった。
- 対応: selector range・ease・positionを有限値と既存UI想定範囲へ正規化して復元するようにした。
- 価値/懸念: Text Animatorの保存データから異常値が選択範囲・文字変形へ流れる可能性を減らす。selector units/shapeのenum検証は既存仕様に依存する。
- 次に確認: ビルド許可後、範囲Animatorの保存・再読込と選択表示を確認する。
# 2026-08-02: 3Dレイヤーenum復元の検証

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- 事実: fixedGeometry と renderMode はJSON整数を直接enumへcastしていた。
- 対応: 定義済み範囲を確認してから復元し、未知値は現在の安全な状態を維持するようにした。
- 価値/懸念: 破損・将来バージョン由来のenum値がメッシュ生成や表示モードへ流れる可能性を減らす。
- 次に確認: ビルド許可後、未知enumを含む3D JSONの復元結果を確認する。
# 2026-08-02: SourceCrop共通pan値の正規化

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`、画像・動画レイヤーの共有Cropモデル。
- 事実: panはsetterとJSON復元の両方で有限値・上限を保証していなかった。
- 対応: pan各軸を有限値確認後に±1000000へclampし、JSON復元もsetterを通すようにした。
- 価値/懸念: 画像・動画共通のCrop変換で不正なpanが行列計算へ流れる可能性を減らす。既存の動画再生経路は変更していない。
- 次に確認: ビルド許可後、画像・動画双方のCrop保存復元と極端なpanを確認する。
# 2026-08-02: SourceCrop JSONのrect / anchor防御

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`。
- 事実: cropRect・anchorのJSONヘルパーは数値型確認のみで、非有限値をそのままQRectF/QPointFへ渡す可能性があった。
- 対応: rect/pointの有限値確認と、anchorの非有限値フォールバックを追加した。
- 価値/懸念: 画像・動画共通のCrop変換で不正な矩形・anchorが残る可能性を減らす。
- 次に確認: ビルド許可後、異常値を含むCrop JSONの復元と保存結果を確認する。
# 2026-08-02: SourceCrop共通zoom上限

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`。
- 事実: SourceCropのzoomは正値化のみで、画像側Property入力の上限をJSON復元や共有モデルAPIが迂回できた。
- 対応: 共通setterでzoomを0.001〜1000にclampし、画像・動画の復元経路を統一した。
- 価値/懸念: 極端なCrop変換による小数計算・出力範囲の不安定化を抑える。既存の大倍率利用は上限に合わせて制限される。
- 次に確認: ビルド許可後、画像・動画のzoom境界と保存復元を確認する。
# 2026-08-02: SourceCrop共通rotation上限

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`。
- 事実: rotationは非有限値だけを防ぎ、極端な角度は共有API・JSON復元からそのまま保持できた。
- 対応: rotationを±360000度へclampし、画像・動画共通のCrop変換へ適用した。
- 価値/懸念: 極端な角度による三角関数計算や表示変換の不安定化を抑える。通常の回転操作の範囲には影響しない。
- 次に確認: ビルド許可後、Crop rotationの境界値を保存・再読込する。
# 2026-08-02: SourceCrop矩形setterの有限値防御

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`。
- 事実: setCropRectはrect.normalized()のみを行い、APIから非有限矩形を直接保持できた。
- 対応: QRectFの各成分を検証し、非有限値なら空矩形へ戻すようにした。
- 価値/懸念: JSON以外の編集・サービス経路でもCrop変換へ不正矩形が入る可能性を減らす。
- 次に確認: ビルド許可後、無効QRectF入力とclampToSourceの既定矩形復帰を確認する。
# 2026-08-02: Shapeレイヤー寸法上限

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: setSizeは1以上のみを保証し、保存データやAPIから極端なキャンバス寸法を受け入れ得た。
- 対応: width/heightを1〜100000へclampし、shape cache・software描画の過大確保を抑えるようにした。
- 価値/懸念: 破損JSONや誤入力による巨大な描画負荷を減らす。大判シェイプの上限は未検証。
- 次に確認: ビルド許可後、寸法境界の保存復元とcache更新を確認する。
# 2026-08-02: Shape寸法上限をProperty定義へ統一

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: setSizeの上限とShape Propertyのhard rangeが100000 / 16384で不一致だった。
- 対応: setSize側をProperty定義と同じ16384へ揃えた。
- 価値/懸念: UI経由とAPI・JSON経由で許容寸法がずれる問題を解消する。
- 次に確認: ビルド許可後、Property入力とJSON復元の最大寸法が一致することを確認する。
# 2026-08-02: Shapeカスタム頂点のJSON復元フィルタ

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: custom polygon / Bézier pathの頂点はJSON復元時にsetterを通らず直接vectorへ格納されていた。
- 対応: position・inTangent・outTangentの各成分が有限値の頂点だけを復元するようにした。
- 価値/懸念: 破損したShape JSONがポリゴン・パス描画へNaNを流す可能性を減らす。頂点数の上限は既存仕様に委ねている。
- 次に確認: ビルド許可後、異常頂点を含むカスタムShapeの復元と保存を確認する。
# 2026-08-02: Shapeオペレーター復元件数上限

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: shapeOperators JSON配列を件数制限なしでreserveし、破損・悪意あるデータで過大な復元処理へ進み得た。
- 対応: fromJson / restoreOperatorsFromJson の復元件数を最大128へ制限した。未知のoperator typeは従来どおり無視する。
- 価値/懸念: シェイプ復元時のメモリ・処理量の暴走を抑える。128件の仕様上限は未検証。
- 次に確認: ビルド許可後、128件超のオペレーターJSON復元が安全に切り詰められるか確認する。
# 2026-08-02: Shapeカスタム頂点配列の上限

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: custom polygon / custom Bézier pathのJSON配列を件数制限なしでreserveしていた。
- 対応: 各配列の復元件数を最大100000に制限し、巨大JSONによる先行メモリ確保を抑えた。
- 価値/懸念: 既存の有限値フィルタと合わせて、破損Shapeデータの復元負荷を抑制する。上限値は実素材で未検証。
- 次に確認: ビルド許可後、上限超過配列と通常のBezier編集を確認する。
# 2026-08-02: Paintフレーム配列の復元上限

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: Paint JSONのframes配列を件数制限なしで復元していた。
- 対応: 1レイヤーあたりの復元フレーム数を最大10000に制限した。
- 価値/懸念: 破損JSONによる大量フレームのメモリ・復元処理負荷を抑える。正式な長尺ペイント上限は未検証。
- 次に確認: ビルド許可後、10000件超のフレームJSONを安全に切り詰められるか確認する。
# 2026-08-02: Paintピクセルpayloadの有限値正規化

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: Paint JSONのBase64 payloadはサイズ検証があっても、float値そのもののNaN / Infinityを含み得た。
- 対応: RGBA32F復元直後に全チャンネルを走査し、非有限値を0へ置換するようにした。
- 価値/懸念: 破損したペイントデータが描画・合成へ不正浮動小数を流す可能性を減らす。復元時の全画素走査コストは受け入れる。
- 次に確認: ビルド許可後、異常float payloadの復元と通常ペイント画像の再読込を確認する。
# 2026-08-02: Puppetピン数上限

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`。
- 事実: Puppet Toolは各レイヤーのpin配列を無制限に追加できた。
- 対応: 1レイヤーあたり最大1024 pinに制限し、超過追加を拒否するようにした。
- 価値/懸念: PuppetEngineへの過大なpin配列と、誤操作による処理負荷を抑える。1024件のUI上限は未検証。
- 次に確認: ビルド許可後、上限到達時のUI応答と既存pin操作を確認する。
# 2026-08-02: Text Animator配列の復元上限

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- 事実: Property EditorのanimatorCountは最大16だったが、JSONのtext.animators配列は件数無制限でreserve・復元していた。
- 対応: JSON復元も最大16 Animatorへ制限し、UI/APIの上限と一致させた。
- 価値/懸念: 巨大JSONによるText Animatorの過大復元を抑える。既存の16件超データは先頭16件を採用する。
- 次に確認: ビルド許可後、16件超のText Animator JSON復元と通常のselector編集を確認する。
# 2026-08-02: Keying CPU alpha normalization

- 関連: `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`, `ChromaKeyEffect.cppm`, `DifferenceKeyEffect.cppm`
- 事実: CPU キーイング3種で入力の非有限値を無効 alpha にし、最終 alpha を `0..1` に正規化した。
- 価値: 不正な float や過大な元 alpha が後段の合成へ伝播するリスクを抑える。
- 次に確認すべきこと: 実データでキー境界と spill reduction の画質を確認する。
# 2026-08-02: Text style setter range alignment

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: font family の空白入力を既定値へ戻し、tracking/leading の setter を Inspector の hard range（`-1000..1000`）と一致させた。
- 価値: UI、JSON、アニメーション経由で値が入っても、レイアウト計算へ極端な値が流れない。
- 次に確認すべきこと: 多言語・縦書き・長文でのレイアウト実データ確認。
# 2026-08-02: Disk preview cache image integrity check

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: ディスクキャッシュの manifest 照合前に、対象が正のサイズを持つ通常ファイルで、Qt の画像リーダーが読める PNG か検証するようにした。
- 価値: 書き込み途中・破損・非画像ファイルをキャッシュヒットとして扱わず、再レンダリングへ戻せる。
- 次に確認すべきこと: キャッシュ容量削減中の同時読み書きで manifest と frame の整合性を確認する。
# 2026-08-02: Image sequence path restore bound

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 連番画像パスの setter と JSON 復元に最大 `100000` フレームの上限を追加し、JSON 配列内の文字列以外を無視するようにした。
- 価値: 破損・悪意あるプロジェクトで巨大なパス配列を生成してメモリやロード処理を圧迫することを防ぐ。
- 次に確認すべきこと: 非常に長い連番と欠落フレームを含むプロジェクトの再読込挙動。
# 2026-08-02: Source text keyframe restore bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Source Text キーフレームの JSON 復元を最大 `10000` 件に制限し、`timeScale <= 0` の不正な時間値を無視するようにした。
- 価値: 破損プロジェクトによる大量キーフレーム生成や不正な RationalTime の混入を防ぐ。
- 次に確認すべきこと: 大量キーと異常な時間スケールを含むプロジェクトの再読込。
# 2026-08-02: Form particle noise input normalization

- 関連: `Artifact/src/Layer/ArtifactFormParticleLayer.cppm`
- 事実: noise amount/scale/speed/phase の JSON 復元とプロパティ更新で非有限値を拒否し、実用上の上限を適用した。
- 価値: プロシージャル粒子の時間位相や空間周波数が NaN/Inf・極端な値になり、シミュレーションや描画を壊す経路を塞ぐ。
- 次に確認すべきこと: 長時間再生時の noiseSpeed と大きな phase の見た目・性能。
# 2026-08-02: Particle turbulence effector normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: turbulence の frequency/amplitude/evolution/octaves と共通 strength を JSON 復元時に有限値・範囲へ正規化し、追加 API でも同じ制約を適用した。
- 価値: 粒子 effector の NaN/Inf や過大な周波数・反復数による不安定化を防ぐ。
- 次に確認すべきこと: force/vortex/attractor 等の他 effector についても保存復元値と setter の制約を照合する。
# 2026-08-02: Procedural 3D noise restore normalization

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: terrain/path の noise scale・amplitude・evolution を有限値と範囲へ正規化し、terrain octaves を `1..12` に制限した。
- 価値: procedural 3D の JSON 破損値が無限・極小周波数や過大反復として生成処理へ流れるのを防ぐ。
- 次に確認すべきこと: 地形・パスの各ノイズ設定でプリセットと既存シーンの見た目を確認する。
# 2026-08-02: Particle radial effector restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: vortex/attractor/repeller の radius、falloff、angular velocity 等を JSON 復元時に有限値・範囲へ正規化した。
- 価値: 粒子場の半径・速度が NaN/Inf や極端な値となり、近傍探索やシミュレーションを不安定化する経路を減らす。
- 次に確認すべきこと: wind/flocking/kill effector の保存復元値も同じ基準で照合する。
# 2026-08-02: Particle wind and flocking restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: wind の強度・turbulence・周波数・evolution、flocking の近傍半径・重み・最大加速度を JSON 復元時に有限値・範囲へ正規化した。
- 価値: 群集・風場の計算に NaN/Inf や過大な近傍探索値が流入する経路を抑える。
- 次に確認すべきこと: Kill effector の zone type/size と wind direction の異常値復元。
# 2026-08-02: Particle kill-zone restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: Kill zone の type を `0..2` に制限し、各サイズを有限値かつ `0..100000` に正規化した。
- 価値: 破損プロジェクトから未知の zone type や負・無限サイズが衝突判定へ入るのを防ぐ。
- 次に確認すべきこと: 各 zone type の境界上（size=0 を含む）で kill 判定を確認する。
# 2026-08-02: Particle effector vector restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: 全 effector 共通の position/direction と Force の各成分を有限値・`±1000000` の範囲へ正規化してから復元するようにした。
- 価値: 破損したベクトル値が粒子の近傍計算・移流・力計算へ NaN/Inf として伝播するのを防ぐ。
- 次に確認すべきこと: 既存プロジェクトの巨大座標を意図的に保持する必要があるか確認する。
# 2026-08-02: Image input color-space validation

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の入力色空間設定で、アクティブな OCIO config が候補を返す場合は未知の色空間名を空に戻すようにした。config 未ロード時は既存の保存値を保持する。
- 価値: 素材別色空間の指定ミスが無言で変換処理へ流れ、意図しない色変換になるリスクを下げる。
- 次に確認すべきこと: config 切替後に既存レイヤーの入力色空間を再検証する運用を確認する。
# 2026-08-02: Image JSON color-space validation parity

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の JSON 復元でも直接代入をやめ、UI/API と同じ `setInputInterpretation()` を通すようにした。
- 価値: プロジェクト再読込だけが未知の OCIO 色空間名を受け入れる経路をなくした。
- 次に確認すべきこと: config 切替後の再読込と、OCIO config 未ロード時の保存値保持。
# 2026-08-02: Particle wind direction restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: Wind の direction 各成分も共通の有限値・座標範囲検証を通して復元するようにした。
- 価値: 壊れた方向ベクトルが風場計算へ NaN/Inf として伝播する最後の共通ベクトル経路を塞ぐ。
- 次に確認すべきこと: zero direction を許容するか、UI 側で正規化するかを仕様確認する。
# 2026-08-02: Shape procedural operator input normalization

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Shape の Wiggle Paths / Zig Zag operator の amount・frequency 更新で非有限値を拒否し、frequency を `0..10000` に制限した。
- 価値: シェイプのプロシージャル頂点生成へ NaN/Inf や過大周波数が入る経路を抑える。
- 次に確認すべきこと: Wobble と他 operator の setter 範囲を Core 側仕様と照合する。
# 2026-08-02: Shape wobble operator input normalization

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Hand Drawn Wobble の amount・frequency・pressure jitter・gap probability 更新を有限値と範囲内へ正規化した。
- 価値: ランダムな輪郭生成で NaN/Inf、過大周波数、不正な確率値が使われるのを防ぐ。
- 次に確認すべきこと: Wobble の seed と stroke spacing を含む Core 側設定の範囲確認。
# 2026-08-02: Shape repeater input normalization

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Repeater の copies を `1..1000`、start/end opacity を `0..100` に制限し、非有限 opacity は既定値へ戻すようにした。
- 価値: シェイプ複製数による過大な描画負荷と、不正な透明度値の伝播を抑える。
- 次に確認すべきこと: Repeater の位置・scale・rotation のベクトル入力も Core setter の仕様と照合する。
# 2026-08-02: Shape operator JSON normalization parity

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Shape operator の JSON 復元後にも Repeater/Wiggle/ZigZag の値を setter 経由で正規化し、UI 更新時と同じ制約を適用した。
- 価値: Core operator の `fromJson()` が直接受け入れた異常値を、レイヤー境界で吸収できる。
- 次に確認すべきこと: Wobble を含む全 operator の getter/setter API が揃った時点で同じ復元後検証へ拡張する。
# 2026-08-02: Shape Wobble JSON normalization parity

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Hand Drawn Wobble の JSON 復元後にも amount・frequency・pressure jitter・gap probability の範囲検証を適用した。
- 価値: Core operator の復元処理が受け入れた異常値を、レイヤー境界で UI 更新時と同じ基準に補正する。
- 次に確認すべきこと: seed と stroke spacing の復元 API が利用可能になった時点で追加検証する。
# 2026-08-02: Shape operator restore coverage expansion

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: JSON 復元後の operator 正規化を Trim Paths、Offset Paths、Pucker/Bloat、Rounded Corners まで拡張した。
- 価値: 主要なパス加工 operator の有限値・範囲検証をレイヤー境界で一貫させた。
- 次に確認すべきこと: Repeater の point/vector と rotation の有限値検証。
# 2026-08-02: Shape repeater transform restore normalization

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Repeater の anchor/position/scale、offset、rotation を JSON 復元後に有限値・範囲へ正規化した。scale の不正値は `(1,1)` に戻す。
- 価値: Repeater の累積変換で NaN/Inf や極端な指数計算が発生する経路を抑える。
- 次に確認すべきこと: scale のゼロ値を仕様として許容するか確認する。
# 2026-08-02: Procedural 3D mesh density restore bounds

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: Terrain columns/rows を `2..4096`、Path samples を `2..10000`、sides を `3..256` に制限して JSON 復元するようにした。
- 価値: 破損プロジェクトがメッシュ生成時に無制限の頂点数や過大な描画負荷を発生させるのを防ぐ。
- 次に確認すべきこと: UI の品質プリセットが想定する最大密度と上限値の整合。
# 2026-08-02: Render queue restore bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: レンダーキュー JSON 復元を最大 `10000` ジョブ、各ジョブの multi-channel 出力チャンネルを最大 `128` 件に制限した。
- 価値: 破損したキュー定義による大量ジョブ生成や、チャンネル配列の過大なメモリ使用を防ぐ。
- 次に確認すべきこと: selected frame ranges / render passes の復元件数上限も運用値と照合する。
# 2026-08-02: Render queue nested restore bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: 各レンダージョブの selected frame ranges を最大 `10000` 件、render passes を最大 `256` 件に制限して復元するようにした。
- 価値: 巨大なネスト配列による復元時間・メモリ使用の膨張を抑える。
- 次に確認すべきこと: layer whitelist/blacklist の配列上限と、長大な文字列フィールドの扱い。
# 2026-08-02: Render queue layer-filter restore bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: layer whitelist/blacklist の JSON 復元を各最大 `10000` 件に制限し、文字列以外の要素を無視するようにした。
- 価値: 壊れたフィルター配列が大量の LayerID 生成や不要な検索負荷を発生させるのを防ぐ。
- 次に確認すべきこと: 長大な ID 文字列や不正 ID の `LayerID` 構築時の挙動。
# 2026-08-02: Disk preview manifest validation bounds

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: preview manifest の frame 配列を最大 `100000` 件に制限し、非オブジェクト・負の frame 番号を有効エントリとして扱わないようにした。
- 価値: 壊れた manifest による過大な走査や不正な frame ヒットを防ぐ。
- 次に確認すべきこと: manifest の frameCount と実ファイル一覧の整合性を、容量削減中も確認する。
# 2026-08-02: Render queue numeric restore normalization

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: JSON 復元時の解像度を `1..16384`、FPS を有限値かつ `0.001..240`、bitrate を `0..1000000` に制限し、フレーム範囲の負値も補正した。
- 価値: 通常の更新 API を経由しない復元経路でも、出力設定の NaN/Inf・過大値・逆順範囲を防ぐ。
- 次に確認すべきこと: overlay の数値設定と audio bitrate/sample rate の復元値も同じ経路で照合する。
# 2026-08-02: Render queue overlay and audio restore normalization

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: overlay offset/scale/rotation と audio bitrate を JSON 復元時に有限値・範囲へ正規化した。
- 価値: 復元経路から極端なオーバーレイ変換や過大な音声エンコード設定が実行へ流れるのを防ぐ。
- 次に確認すべきこと: 実際の出力フォーマットごとの bitrate/sample rate の許容範囲を受入れ確認する。
# 2026-08-02: Render queue LayerID string bound

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: queue JSON から復元する LayerID 文字列を最大 `1024` 文字に制限した。
- 価値: 巨大な文字列を ID として構築し、フィルター照合へ流す経路を防ぐ。
- 次に確認すべきこと: LayerID の正規フォーマット検証を既存の ID API で実施できるか確認する。
# 2026-08-02: Puppet hit-test threshold bound

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: Puppet pin の hit-test threshold を有限値かつ `0..100000` に制限した。
- 価値: 巨大な threshold の二乗で float overflow が起き、全 pin が誤選択される経路を防ぐ。
- 次に確認すべきこと: zoom 値と pin overlay のスケール計算も同じ overflow 条件で確認する。
# 2026-08-02: Puppet overlay input normalization

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: Puppet overlay 描画で pin 座標の非有限値をスキップし、renderer zoom を有限値かつ `0.001..10000` に制限した。
- 価値: 破損した pin 状態や異常 zoom が overlay の座標・サイズ計算を壊すのを防ぐ。
- 次に確認すべきこと: 変形 mesh vertex の非有限値が返る場合の edge 描画スキップ。
# 2026-08-02: Puppet mesh overlay vertex validation

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: Puppet mesh overlay の edge 描画前に、参照 vertex の x/y が有限値か検証するようにした。
- 価値: 変形エンジンから壊れた vertex が返っても、overlay 全体ではなく該当 edge だけを安全にスキップできる。
- 次に確認すべきこと: deformation 結果そのものの有限値検証と renderer 境界の扱い。
# 2026-08-02: Image sequence frame dimension guard

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 連番フレームを CPU/GPU バッファへ変換する前に、画像寸法を正値かつ最大 `16384x16384` と検証するようにした。
- 価値: レイヤーサイズ未確定時でも、異常に巨大な素材がメモリ確保と変換処理へ流れるのを防ぐ。
- 次に確認すべきこと: 単一画像の `loadFromPath()` 経路にも同じ寸法上限が適用されているか確認する。
# 2026-08-02: Single image dimension guard

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: OIIO の単一画像ヘッダー検査でも寸法を正値かつ最大 `16384x16384` に制限した。
- 価値: 連番以外の単一画像でも、過大な素材を AssetManager とバッファ変換へ渡さない。
- 次に確認すべきこと: 上限超過時に既存画像を保持するか、placeholder を表示するかの UX 方針。
# 2026-08-02: Rig2D restore array bounds

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: Rig2D JSON 復元前に bones `4096`、controls `1024`、constraints/propertyBindings `4096`、smartBones `1024` の上限を適用した。
- 価値: Core 側を変更せず、破損プロジェクトによるリグ要素の大量生成を親レイヤー境界で防ぐ。
- 次に確認すべきこと: 既存プロジェクトの最大リグ規模と上限値の運用整合。
# 2026-08-02: OIIO decode dimension guard

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: OIIO の実デコード関数でも最大 `16384x16384` と正の channel 数を検証してから RGBA 変換・QImage 確保を行うようにした。
- 価値: ヘッダー検査後にファイルが差し替わる競合や prefetch 経路でも、巨大画像の確保を防ぐ。
- 次に確認すべきこと: OIIO のタイル/多解像度画像で spec 寸法と実デコード寸法が一致するか確認する。
# 2026-08-02: Rig2D skin mesh restore validation

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: Rig2D の skin mesh 復元前に、頂点の必須 weight/bone index 配列長・有限値を検証し、頂点を最大 `1000000` 件、triangle index を最大 `3000000` 件へ制限した。
- 価値: Core の malformed JSON 読み込みで配列外アクセスや過大な mesh 生成が起きる経路を親側で防ぐ。
- 次に確認すべきこと: skin mesh の bone index が実際の bone 数以内か、Rig2D 構築後に照合する。
# 2026-08-02: Rig2D skin bone-index validation

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin mesh の bone index を復元対象 bones 数と照合し、範囲外の index を `-1` に補正するようにした。
- 価値: 存在しない bone 参照が skin 評価や pose 更新へ伝播するのを防ぐ。
- 次に確認すべきこと: weight 合計の正規化を Core 復元後に再確認する。
# 2026-08-02: Rig2D skin weight normalization

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin mesh の各頂点 weight を `0..1` に clamp した後、合計 1 へ正規化し、合計 0 の場合は先頭 weight を 1 にするようにした。
- 価値: 不正な weight 合計による未定義・過剰変形を抑え、skin 評価へ安定した入力を渡す。
- 次に確認すべきこと: bone index が `-1` の weight を正規化対象から除外する必要があるか仕様確認する。
# 2026-08-02: Rig2D invalid-bone weight exclusion

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: 範囲外 bone index に対応する weight を `0` として扱い、正規化合計から除外するようにした。
- 価値: 存在しない bone への影響が、残存 weight として変形結果へ混入するのを防ぐ。
- 次に確認すべきこと: 全 bone index が無効な頂点の fallback を、rest position 維持として受け入れ確認する。
# 2026-08-02: Rig2D all-invalid skin fallback

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin vertex の全 bone index が無効な場合、fallback weight を無効 bone に割り当てず全て `0` にするようにした。
- 価値: 存在しない bone への weight 1 が残り、Core の skin 評価へ不正影響が入るのを防ぐ。
- 次に確認すべきこと: 全 weight 0 の頂点を rest position で保持する Core 側挙動を確認する。
# 2026-08-02: Rig2D skin triangle integrity check

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin mesh の triangle index を復元後の頂点数未満に制限し、3要素単位でない末尾 index を除去するようにした。
- 価値: skin 評価時の頂点配列外参照と不完全な三角形生成を防ぐ。
- 次に確認すべきこと: 重複 index や極小面積三角形を許容する既存仕様との整合。
# 2026-08-02: Rig2D skin vertex range normalization

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin mesh の頂点位置を `±1000000`、UV を `±100000` に clamp してから Core 復元するようにした。
- 価値: 異常に大きい座標・UV が skin 変形や描画境界計算を overflow させるリスクを下げる。
- 次に確認すべきこと: 通常の高解像度素材でこの上限が狭すぎないか確認する。
# 2026-08-02: Stabilizer parameter normalization

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: Stabilizer の output size を最大 `16384x16384`、border fill を `0..1`、smoothing window を `1..10000` に正規化した。
- 価値: 既存のスタビライザー処理に極端な画像サイズ・境界値・平滑化窓が流入するのを防ぐ。
- 次に確認すべきこと: Live/BatchStabilizer の setParams も共通正規化へ統一できるか確認する。
# 2026-08-02: Live and batch stabilizer parameter parity

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: Live/BatchStabilizer の setParams にも output size、border fill、smoothing window の正規化を適用し、Live の history size を `1..10000` に制限した。
- 価値: Stabilizer の全実行経路で同じ入力制約を使い、履歴バッファの過大化も防ぐ。
- 次に確認すべきこと: バッチ入力フレーム数と各 frame 寸法の上限を確認する。
# 2026-08-02: Stabilizer frame input bounds

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: StabilizerEffect の蓄積フレームを最大 `10000` 件、Live/Batch 系の入力画像を最大 `16384x16384` に制限した。
- 価値: 長大なバッチや巨大画像が追跡・平滑化処理のメモリ使用量を無制限に増やすのを防ぐ。
- 次に確認すべきこと: 上限到達時に UI へ拒否理由を返す既存通知経路があるか確認する。
# 2026-08-02: Batch stabilizer path normalization

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: BatchStabilizer の入力・出力ファイルパスを setter で trim するようにした。
- 価値: 空白付き・空のパスがバッチ処理へそのまま伝播するのを抑える。
- 次に確認すべきこと: start/stop 処理で入力ファイル存在と出力ディレクトリ書込可否を明示的に返せるか確認する。
# 2026-08-02: Batch stabilizer process preflight

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: BatchStabilizer::process() で入力が実ファイルか、出力がディレクトリでないか、入力と出力が同一パスでないかを事前検証するようにした。
- 価値: 実処理前にダミー成功を返す・入力を上書きする明らかな経路を防ぐ。
- 次に確認すべきこと: ダミー進捗処理を実際のフレーム読み書きへ置き換える段階を別途設計する。
# 2026-08-02: Point tracker apply bounds

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: トラッキング適用時に pointId を非負へ制限し、FPS を有限値かつ `0..240`、時刻・座標を `±1e9` 以内に検証してからキーフレーム化するようにした。
- 価値: 壊れたトラッキング結果が RationalTime や Transform3D への整数 overflow を起こす経路を防ぐ。
- 次に確認すべきこと: MotionTracker の frame range と comp の作業範囲を適用前に照合する。
# 2026-08-02: Point tracker composition-range validation

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: トラッキング結果の frame を composition frame range と照合し、範囲外のキーを除外し、適用キーがゼロなら失敗を返すようにした。
- 価値: composition 外の keyframe 生成と、何も適用していないのに成功扱いになる経路を防ぐ。
- 次に確認すべきこと: work area を適用範囲にする選択肢が必要か確認する。
# 2026-08-02: Point tracker multi-point apply bound

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: 全トラッキングポイントの一括適用で生成対象 point ID を最大 `1024` 件に制限した。
- 価値: 破損した tracker 結果から大量の Null レイヤーが生成されるのを防ぐ。
- 次に確認すべきこと: 上限到達時に UI へ一部適用の状態を通知する経路を確認する。
# 2026-08-02: Camera tracker analysis range bounds

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: Camera Tracker の解析範囲を composition range と交差させ、最大 `100000` フレームに制限し、入力画像を最大 `16384x16384` に制限した。
- 価値: 逆転・過大な in/out や巨大画像が tracker 解析へ入るのを防ぐ。
- 次に確認すべきこと: decode できたフレームが 0 件の場合に tracker.solve() へ進まないことを実データで確認する。
# 2026-08-02: Camera tracker decoded-frame preflight

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: Camera Tracker は decode に成功したフレーム数を数え、0 件なら solve を実行せず失敗を返すようにした。
- 価値: 空の入力で tracker が見かけ上成功し、空のカメラ/Null レイヤーを生成する経路を防ぐ。
- 次に確認すべきこと: solver が要求する最小フレーム数を確認し、必要なら solve 前に明示する。
# 2026-08-02: Camera tracker minimum-frame preflight

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: Camera Tracker の solve 前に decode 成功フレームが最低 2 件あることを要求するようにした。
- 価値: 単一フレーム入力をカメラトラッキング成功として扱う誤判定を防ぐ。
- 次に確認すべきこと: CameraTracker solver の特徴点・フレーム数の最小要件を API 仕様と照合する。
# 2026-08-02: Camera tracker feature-layer bound

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: Camera Tracker の feature point から生成する Null レイヤーを最大 `1024` 件に制限した。
- 価値: 異常な solver 結果による大量レイヤー追加と composition 構造の過負荷を防ぐ。
- 次に確認すべきこと: 上限到達時の feature point 可視化・通知方法を確認する。
# 2026-08-02: Stabilizer direct-frame dimension guard

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: StabilizerEffect::processFrame() の直接入力にも最大 `16384x16384` の寸法検証を追加した。
- 価値: addFrame() を経由しない呼び出しでも、巨大画像を変換処理へ渡さない。
- 次に確認すべきこと: processFrame の outputSize と入力画像のメモリ上限を実運用値と照合する。
# 2026-08-02: Stabilizer interpolation bounds

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: Stabilizer の bilinear 補間係数を有限値・`0..1` に clamp し、補間後 RGBA を `0..255` に丸めた。
- 価値: 境界計算の異常値が QImage の pixel 値へ伝播するのを防ぐ。
- 次に確認すべきこと: borderFill のサンプル座標が常に source bounds 内になることを確認する。
# 2026-08-02: Stabilizer border coordinate validation

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: border fill のサンプル座標を整数化する前に x/y の有限値を確認するようにした。
- 価値: 異常な逆変換座標が NaN のまま境界 clamp・整数化へ進むのを防ぐ。
- 次に確認すべきこと: source が 1 pixel 幅/高さの場合の bilinear 境界分岐を確認する。
# 2026-08-02: Text paragraph setter range alignment

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: maxWidth/boxHeight を `0..100000`、paragraphSpacing を `0..1000` に制限し、Inspector の hard range と setter を一致させた。
- 価値: JSON・UI・アニメーション経路で極端なテキストレイアウト値が流入するのを防ぐ。
- 次に確認すべきこと: 長文・Box text で上限到達時のレイアウト応答を確認する。
# 2026-08-02: Text stroke, shadow, and path range alignment

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: stroke width、shadow offset/blur、path start/end offset の setter を Inspector hard range と一致させた。
- 価値: 描画余白やパスレイアウトへ極端な値が入り、過大なバッファや不安定な配置になるのを防ぐ。
- 次に確認すべきこと: Source Text / ruby の長文入力に対する長さ上限を仕様と照合する。
# 2026-08-02: Text input length bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: 通常テキスト、Source Text キーフレーム API、Source Text JSON 復元の文字列を最大 `1000000` 文字に揃えた。
- 価値: 長大な入力が shaping・layout・glyph cache を無制限に膨張させるのを防ぐ。
- 次に確認すべきこと: 上限超過を UI に通知する既存エラー表示経路があるか確認する。
# 2026-08-02: Ruby text length bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: rubyText の setter も本文・Source Text と同じ最大 `1000000` 文字へ制限した。
- 価値: ruby 注釈だけが shaping/layout 処理を過大化させる経路を防ぐ。
- 次に確認すべきこと: フォント名や rich-text payload の文字列上限を同じポリシーに含めるか確認する。
# 2026-08-02: Text font-family string bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: font family 名を trim 後最大 `1024` 文字に制限し、空文字は既存どおり Arial に戻すようにした。
- 価値: 異常に長い font resolver 入力による探索・キャッシュ処理の膨張を防ぐ。
- 次に確認すべきこと: 複数フォント fallback 名を扱う場合の上限仕様を確認する。
# 2026-08-02: Text animator count range alignment

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text Animator の add/set API を最大 `16` 件に制限し、Inspector の hard range と一致させた。
- 価値: UI 外の API・JSON 操作から animator が無制限に増え、レイアウト評価が膨張するのを防ぐ。
- 次に確認すべきこと: animator JSON 配列復元と preset 適用時も同じ上限を維持することを確認する。
# 2026-08-02: Source text keyframe frame bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Source Text の keyframe setter が負の frame 番号を拒否するようにした。
- 価値: UI 外の API 呼び出しからタイムライン範囲外の不正 keyframe が追加されるのを防ぐ。
- 次に確認すべきこと: comp frame range の上限も setter 側で照合する必要があるか確認する。
# 2026-08-02: Source text JSON frame bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Source Text JSON keyframe の timeValue も `0` 未満を無効として、負の timeScale と合わせて復元しないようにした。
- 価値: API と JSON 復元でタイムライン外キーの扱いを統一する。
- 次に確認すべきこと: composition frame range 上限を超えるキーの扱いを確認する。
# 2026-08-02: Camera tracker solve-result validation

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: CameraTracker の success 結果でも cameraPath が空、または有限 pose が一つもない場合はレイヤー生成を行わないようにした。
- 価値: solver の不完全結果から空・壊れたカメラ構造を composition に追加するのを防ぐ。
- 次に確認すべきこと: 有効 pose の frame 範囲と実際の cameraPath 件数を照合する。
# 2026-08-02: Render queue string restore normalization

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: JSON 復元時の composition/job name、output path、format/codec、encoder backend、audio path/codec/channel mode を trim してから登録するようにした。
- 価値: 空白付き設定が format 判定・出力先検証・backend 選択を不安定化する経路を減らす。
- 次に確認すべきこと: trim 後に空になる必須項目の扱いを既存 queue validation と照合する。
# 2026-08-02: Render queue channel key normalization

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: マルチチャンネル出力の復元・検証時に、チャンネルキーを trim してから既存の canonical key 判定へ渡すようにした。
- 価値: JSON の空白混入で有効な出力チャンネルが無効扱いになる経路を減らす。
- 次に確認すべきこと: renderer channel key の別名変換が既存仕様どおりか確認する。
# 2026-08-02: Text animator restore normalization

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text Animator の JSON 復元で名前を trim・256文字に制限し、空名を既定名へ戻す。Selector の enum 値を定義範囲へ clamp し、正規表現パターンを4096文字に制限する。
- 価値: 不正な enum 値や極端に大きい selector 設定が復元後の評価・UI表示を不安定化する経路を減らす。
- 次に確認すべきこと: Text Animator の評価側で selector pattern の不正な正規表現をどう扱うかを既存仕様と照合する。
# 2026-08-02: Text animator invalid regex fallback

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text Animator の JSON 復元およびプロパティ更新時に、selector pattern を4096文字へ制限し、無効な正規表現なら regex selector を無効化する。
- 価値: 不正な保存値や編集値が TextAnimatorEngine の regex 評価へそのまま流れる経路を減らす。
- 次に確認すべきこと: regex selector の UI で無効化理由を表示する必要があるか、既存のエラー表示方針と照合する。
# 2026-08-02: Text animator property edit bounds

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: UI からの Text Animator プロパティ更新にも、JSON 復元と同じ名前長・enum・範囲・wiggly・変形・stroke/blur の境界値を適用した。
- 価値: 保存時だけでなく、プロパティ編集から極端な値が評価経路へ入る経路を揃えて制限する。
- 次に確認すべきこと: Text Animator の各表示プロパティの UI range とコード側上限が一致しているか確認する。
# 2026-08-02: Image color interpretation restore normalization

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 素材の source path を JSON 復元時に trim し、入力 transfer function 名を trim 後1024文字に制限してから既存の OCIO 入力解釈へ渡すようにした。
- 価値: 保存値の空白や異常に長い識別子が素材再読込・OCIO 適用経路へそのまま入る可能性を減らす。
- 次に確認すべきこと: transfer function の実ライブラリ名検証を OCIO 側 API で行えるか確認する。
# 2026-08-02: Preview disk manifest integrity tightening

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: ディスクプレビューの manifest 読み込みサイズを16 MiB以内に制限し、全 frame entry を object・非負 frame・正の byte 数・単一ファイル名として検証するようにした。
- 価値: 壊れた manifest やパス要素を含む entry を有効なキャッシュとして扱わず、復元時のメモリ消費とファイル参照の曖昧さを抑える。
- 次に確認すべきこと: 大規模キャッシュの manifest 上限が運用上十分か確認する。
# 2026-08-02: Particle emitter restore collection bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: Particle の JSON 復元で emitter と effector を各1024件までに制限し、object 以外の配列要素を無視するようにした。
- 価値: 壊れた／過大な保存配列が復元時に大量の emitter・effector 生成を引き起こす経路を抑える。
- 次に確認すべきこと: emitter/effector enum の定義範囲を確認し、不正な型値を既定値へ戻す。
# 2026-08-02: Particle restore enum validation

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter shape を0..7、emission mode を0..2へ clampし、effector type は0..10以外を復元対象から除外するようにした。
- 価値: 不正な enum 値が未定義の挙動や意図しない effector 分岐へ流れる経路を減らす。
- 次に確認すべきこと: 現在未対応の effector 種別（Drag/Noise/Collision）の実体化方針を別タスクとして確認する。
# 2026-08-02: Particle emitter numeric restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter 復元時の rate、life/speed、burst、frame rate、mass、auxiliary 設定を有限値・範囲・min/max 整合性で正規化し、texture path も trim・長さ制限した。
- 価値: NaN/無限値や極端な設定が粒子生成数・寿命・補助粒子数へ伝播する経路を抑える。
- 次に確認すべきこと: emitter の位置・速度・scale ベクトルにも同じ有限値検証を適用する。
# 2026-08-02: Particle emitter vector restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter 復元時の velocity random、position、rotation、direction を有限値かつ±1,000,000以内に制限し、scale 系の min/max と中間位置も正規化した。
- 価値: 破損したベクトルやスケール値が粒子配置・速度・補間計算へ伝播する経路を抑える。
- 次に確認すべきこと: textureRows/Cols、startFrame/frameCount、auxTrigger の復元範囲を同じ方針で確認する。
# 2026-08-02: Particle texture frame restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter 復元時の texture rows/columns、start frame、frame count、aux trigger を定義範囲へ制限した。
- 価値: 不正なテクスチャ分割や巨大なフレーム範囲がアトラス参照・アニメーション評価へ伝播する経路を抑える。
- 次に確認すべきこと: particle layer のプロパティ編集経路にも同じ frame/texture 上限が揃っているか確認する。
# 2026-08-02: Chroma key property input normalization

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`
- 事実: Chroma Key の similarity、smoothness、spill reduction のプロパティ編集値を有限値として検証し、既存の評価範囲へ clamp してから CPU 実装へ渡すようにした。
- 価値: UI／外部プロパティ経由の NaN・無限値・過大値がキーイング評価へ入る経路を減らす。
- 次に確認すべきこと: Chroma Key の setter 直接呼び出し経路にも同じ範囲が適用されているか確認する。
# 2026-08-02: Particle emitter property edit bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter のプロパティ編集経路にも、shape/mode、位置・回転・方向、rate/burst、texture path・分割数の有限値・範囲制限を適用した。
- 価値: JSON 復元時だけでなく、UI／外部プロパティ編集から極端な値が粒子生成へ入る経路を揃えて制限する。
- 次に確認すべきこと: 残りの emitter 寿命・スケール・aux プロパティ編集も同じ上限へ統一する。
# 2026-08-02: Particle lifetime and speed property bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter の frame rate、mass、life、speed、velocity random、direction spread の編集値を有限値・範囲で正規化し、life/speed の min/max 逆転も補正した。
- 価値: 編集経路から NaN や極端な寿命・速度が粒子シミュレーションへ入る経路を復元処理と同じ基準に揃える。
- 次に確認すべきこと: opacity と scale の編集値にも上限を明示して復元基準と照合する。
# 2026-08-02: Particle scale and opacity property bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: particle の scale/opacity の編集値を有限値・範囲で正規化し、各 min/max の逆転を補正した。
- 価値: 補間途中の NaN や不正な範囲が粒子描画へ伝播する経路を抑える。
- 次に確認すべきこと: gravity/wind/drag と aux 系の編集値も同じ有限値基準へ揃える。
# 2026-08-02: Particle physics and auxiliary property bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: drag、gravity、wind、turbulence、max particles、auxiliary 設定、color position の編集値を有限値・範囲で正規化し、不正な QColor は既存値を維持するようにした。
- 価値: 物理計算・補助粒子生成・色補間へ NaN や極端な値が入る経路を抑える。
- 次に確認すべきこと: particle layer 全体の render 設定にも同じ有限値検証が揃っているか確認する。
# 2026-08-02: Particle render settings bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: particle render の blend/billboard/sort enum を定義範囲へ制限し、soft particle distance と stretch factor を有限値・範囲で復元・編集するようにした。
- 価値: 不正な描画モードや極端な距離・伸長値がレンダー設定へ入る経路を抑える。
- 次に確認すべきこと: render 設定の enum 定義が tooltip の値域と一致しているか確認する。
# 2026-08-02: Procedural3D property edit bounds

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: terrain/path の columns、samples、sides、サイズ、noise、radius、audio gain、source path などの編集値を復元時と同じ有限値・範囲へ揃えた。
- 価値: UI／外部プロパティ編集から過大なメッシュ分割や NaN が Procedural3D 生成へ入る経路を減らす。
- 次に確認すべきこと: path の taper/twist/repeat/scale と material の数値編集も同じ基準で確認する。
# 2026-08-02: Procedural path and material property bounds

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: Procedural3D の material emission strength と path の taper、twist、offset、repeat、scale を有限値・範囲で正規化した。
- 価値: パス生成・マテリアル計算へ NaN や極端な係数が入る経路を抑える。
- 次に確認すべきこと: path の sourceLayerId や base color の入力検証が既存責務と一致しているか確認する。
# 2026-08-02: Procedural3D string and color input normalization

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: path sourceLayerId を trim・1024文字に制限し、Procedural3D の base/emission color は不正な QColor を無視して既存値を維持するようにした。
- 価値: 外部プロパティ入力による曖昧なレイヤー参照や不正色の伝播を抑える。
- 次に確認すべきこと: JSON 復元側の sourceLayerId と色値にも同じ正規化が適用されているか確認する。
# 2026-08-02: Procedural3D JSON restore parity

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: Procedural3D JSON 復元でも terrain/path の source path、sourceLayerId、サイズ、audio gain、radius、taper、twist、offset、repeat、scale を編集経路と同じ基準で正規化した。
- 価値: 保存データ経由だけ異なる値域や非有限値が生成器へ入る不整合を減らす。
- 次に確認すべきこと: JSON 復元とプロパティ編集の共通正規化関数化は、既存モジュール依存を見て別途判断する。
# 2026-08-02: Preview disk manifest duplicate frame rejection

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: manifest の frame entry に同一 frame 番号が重複している場合、キャッシュ全体を無効として扱うようにした。
- 価値: 破損・競合した manifest の一部だけを有効扱いする曖昧さを減らし、frame とファイルの一対一対応を保つ。
- 次に確認すべきこと: manifest 書き込み側が常に重複を生成しないことを確認する。
# 2026-08-02: OCIO input transform argument normalization

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: 素材入力変換の直接呼び出しでも source color space と transfer function を trim してから OCIO processor・legacy fallback へ渡すようにした。
- 価値: レイヤー setter を経由しない呼び出しでも空白混入した識別子が OCIO lookup を不安定化する経路を減らす。
- 次に確認すべきこと: OCIO runtime config に存在しない source color space の fallback 方針を実素材で確認する。
# 2026-08-02: Image source path restore bound

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の単一 source path も JSON 復元時に trim し、最大32768文字へ制限するようにした。
- 価値: sequence path と単一素材 path で復元時の入力境界が異なる不整合を減らす。
- 次に確認すべきこと: loadFromPath 側の既存パス正規化と重複しないか確認する。
# 2026-08-02: Image sequence property edit normalization

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の source path 編集値を trim・32768文字に制限し、sequence frame rate の編集プロパティを追加して0または0.001〜1000fpsへ正規化するようにした。
- 価値: sequence の保存復元・API 設定・プロパティ編集で frame rate の扱いを揃え、既存 sequence source にも変更を即時反映する。
- 次に確認すべきこと: sequence path 配列を UI から編集する導線が必要か、既存の素材管理責務と照合する。
# 2026-08-02: Image sequence frame rate property exposure

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: Image sequence のみ `image.sequenceFrameRate` を Image プロパティグループへ公開し、0.001〜1000fps の hard range と fps 表示を設定した。
- 価値: 追加した sequence frame rate 編集経路を UI の正規プロパティ導線へ接続する。
- 次に確認すべきこと: sequence path 自体の編集責務は Asset 管理 UI と重複しないか確認する。
# 2026-08-02: Image sequence API path bound

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: `setImageSequence()` から登録する各 frame path も trim 後32768文字に制限した。
- 価値: JSON 復元・プロパティ編集・API 登録で sequence path の入力上限を統一する。
- 次に確認すべきこと: ImageSequenceSource 側の path 正規化と責務の重複を確認する。
# 2026-08-02: OCIO settings identifier restore bounds

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: OCIO 設定 JSON の preset name、working space、display、view、looks を trim し、識別子ごとの最大長を設けてから既存の config lookup へ渡すようにした。
- 価値: 設定復元時の異常に長い識別子や空白混入が config 選択を不安定化する経路を減らす。
- 次に確認すべきこと: `ArtifactOCIOConfig::loadFromJson` 側の config path 境界と整合させる。
# 2026-08-02: OCIO config API input bounds

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: preset 名を trim・256文字、外部 config path を trim・32768文字に制限してから既存の preset/path 解決へ渡すようにした。
- 価値: UI や API からの設定変更でも JSON 復元と同じ識別子・パス境界を適用する。
- 次に確認すべきこと: `loadConfig(const OCIOConfig&)` は所有元の config path を直接利用するため、Core 側の境界と責務分担を確認する。
# 2026-08-02: Image input color interpretation property exposure

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の Image プロパティグループへ `image.inputColorSpace` と `image.inputTransferFunction` を追加し、既存の `setInputInterpretation()` を編集経路として利用するようにした。
- 価値: P0 の素材別色空間指定を保存/API だけでなく、通常のプロパティ編集導線から扱えるようにする。
- 次に確認すべきこと: 利用可能な OCIO color space の候補表示を既存 property editor が支援するか確認する。
# 2026-08-02: Image color interpretation property guidance

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: Input Color Space の property tooltip に現在の OCIO working space 候補を表示し、Input Transfer に対応する transfer function 名を案内するようにした。
- 価値: 専用 picker を新設せず、素材別色空間指定の入力ミスを減らす。
- 次に確認すべきこと: property editor が候補選択 UI を提供できる場合は、tooltip より選択式へ移行する。
# 2026-08-02: Image OCIO color space canonicalization

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 入力 color space 名を利用可能な OCIO working space と case-insensitive に照合し、該当時は config の canonical 名へ解決するようにした。不一致は従来どおり空値へ戻す。
- 価値: `sRGB` / `srgb` のような表記差で有効な素材解釈が拒否される問題を減らす。
- 次に確認すべきこと: OCIO config が alias を公開する場合の canonical 名選択を実素材で確認する。
# 2026-08-02: Render queue property string bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: Render Queue の output path を trim・32768文字、job name を trim・256文字に制限する編集経路を追加した。
- 価値: JSON 復元と UI/API からのジョブ更新で文字列境界を統一する。
- 次に確認すべきこと: output format / codec / encoder backend の編集 setter にも同じ正規化が適用されているか確認する。
# 2026-08-02: Render queue codec property bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: output format、codec、codec profile の編集値を trim・256文字、audio source path を trim・32768文字に制限した。
- 価値: Render Queue の JSON・UI/API 更新で encoder 設定文字列の入力境界を統一する。
- 次に確認すべきこと: audio codec と channel mode の setter 長さ・canonical 化を確認する。
# 2026-08-02: Render queue audio property bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: audio codec を trim・256文字に制限し、`updateJob()` の全主要 job/audio 文字列も path 32768文字・識別子256文字へ統一した。
- 価値: 個別 setter だけでなく一括 job 更新経路でも異常に長い文字列が残らないようにする。
- 次に確認すべきこと: audio codec の許可値 canonical 化が既存 encoder 実装の責務か確認する。
# 2026-08-02: Text alignment and wrap enum bounds

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text の horizontal/vertical alignment と wrap mode を JSON 復元・プロパティ編集時に定義範囲へ clamp するようにした。
- 価値: 不正な enum 値が paragraph layout の switch 分岐へ流れる経路を減らす。
- 次に確認すべきこと: Text style の他 enum（font weight/style 等）の復元経路も確認する。
# 2026-08-02: Layer expression restore bound

- 関連: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 事実: effect property の JSON 復元時に expression を trim し、最大16384文字に制限してから既存の serialization bridge へ渡すようにした。
- 価値: 保存データから過大な expression が評価・編集 UI へ流れる経路を抑える。
- 次に確認すべきこと: expression の編集 UI/API 経路にも同じ上限を適用する。
# 2026-08-02: Expression editor input bound

- 関連: `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`
- 事実: Expression Copilot から property へ適用する式を trim・最大16384文字に制限してから既存 apply handler へ渡すようにした。
- 価値: 保存復元と編集 UI の expression 長さ境界を揃える。
- 次に確認すべきこと: Python/API から expression を設定する経路にも同じ上限を適用する。
# 2026-08-02: Image load API path normalization

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: `loadFromPath()` 本体でも path を trim・32768文字に正規化してから sequence 判定、OIIO 読み込み、Asset 登録、保存値へ利用するようにした。
- 価値: API 直接呼び出しでも property/JSON と同じ素材 path 境界を保証する。
- 次に確認すべきこと: OIIO が扱える実パス長の OS 制約と、32768文字上限の妥当性を確認する。
# 2026-08-02: Property widget expression restore bound

- 関連: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- 事実: property widget の serialized expression 復元も trim・最大16384文字へ制限した。
- 価値: layer JSON と widget 側の個別 property 復元で expression 上限が異なる経路を減らす。
- 次に確認すべきこと: project import が widget serialization を経由するか、復元責務を整理する。
# 2026-08-02: Puppet selected pin validation

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: selected pin ID の設定時に trim・256文字制限を適用し、現在の pin 集合に存在しない ID は選択状態へ保持しないようにした。
- 価値: 削除済み・破損した pin 参照が overlay や編集 UI に残る状態を減らす。
- 次に確認すべきこと: pin ID の生成・保存側でも同じ文字列上限を確認する。
# 2026-08-02: Project importer collection bounds

- 関連: `Artifact/src/Project/ArtifactProjectImporter.cppm`
- 事実: project import の AI tags を最大10000件・各256文字、compositions を最大10000件、composition layers を最大100000件に制限した。
- 価値: 破損・過大な project JSON による importer の大量オブジェクト生成を抑える。
- 次に確認すべきこと: projectItems と source registry snapshot の復元側にも同等の上限があるか確認する。
# 2026-08-03: Project item importer collection bound

- 関連: `Artifact/src/Project/ArtifactProjectImporter.cppm`
- 事実: `projectItems` 復元前に配列を最大100000件へ制限するようにした。
- 価値: project JSON の過大な top-level item 配列をそのまま manager へ渡さず、import 時の大量生成を抑える。
- 次に確認すべきこと: manager 側の folder nesting 復元にも深さ・子 item 上限があるか確認する。
# 2026-08-03: Project item tree restore bounds

- 関連: `Artifact/src/Project/ArtifactProject.cppm`
- 事実: project item tree の復元に総数100000件、folder nesting depth 64 の上限を追加した。
- 価値: project JSON の深すぎる／大量の nested folder が再帰処理と owned item allocation を過剰に消費する経路を抑える。
  - 次に確認すべきこと: 上限到達時に import 結果へ警告を返す必要があるか確認する。

# 2026-08-03: Project import file size bound

- 関連: `Artifact/src/Project/ArtifactProjectImporter.cppm`
- 事実: JSON プロジェクトの読み込み前にファイルサイズを 256 MiB 以下へ制限した。
- 価値: `readAll()` と JSON パースが巨大な入力で過剰なメモリを消費する経路を抑える。
- 次に確認すべきこと: 大規模な正当プロジェクトの実ファイルサイズが上限に収まるか確認する。

# 2026-08-03: Project item API restore bounds

- 関連: `Artifact/src/Project/ArtifactProject.cppm`
- 事実: `addProjectItemsFromJson()` に項目総数100000件、folder depth 64、sequence path 100000件の上限を追加した。
- 価値: importer以外のJSON復元入口でも、深いツリーや巨大な連番配列による過剰な所有・文字列確保を抑える。
- 次に確認すべきこと: 上限到達時に呼び出し側へ部分復元を通知するAPIが必要か確認する。

# 2026-08-03: Preset JSON file size bound

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: mask/effect preset の読み込み前に16 MiBのファイルサイズ上限を追加した。
- 価値: preset JSON の `readAll()` とパースが巨大入力で過剰なメモリを消費する経路を抑える。
- 次に確認すべきこと: 既存presetの実サイズ分布を確認し、必要なら上限値を仕様化する。

# 2026-08-03: Locale JSON file size bound

- 関連: `Artifact/src/Translation/TranslationManager.cppm`
- 事実: locale JSON の読み込み前に8 MiBのファイルサイズ上限を追加した。
- 価値: 翻訳ファイル入力の `readAll()` と再帰的な flatten 処理が巨大入力で過剰なメモリを消費する経路を抑える。
- 次に確認すべきこと: 翻訳運用で8 MiBを超えるlocaleが発生しないか確認する。

# 2026-08-03: Batch template JSON file size bound

- 関連: `Artifact/src/Render/ArtifactBatchRenderer.cppm`
- 事実: batch template JSON の列挙・読み込み前に8 MiBのファイルサイズ上限を追加した。
- 価値: template directory 内の巨大JSONが一覧取得時に無制限に読み込まれる経路を抑える。
- 次に確認すべきこと: batch templateの運用ファイルサイズが上限に収まるか確認する。

# 2026-08-03: Effect preset collection input bounds

- 関連: `Artifact/src/Effect/ArtifactEffectPreset.cppm`
- 事実: effect preset collection の読み込みにファイル16 MiB、エントリ100000件の上限を追加し、非object要素を無視するようにした。
- 価値: 外部preset JSONの巨大配列や異常要素が大量のpreset所有を引き起こす経路を抑える。
- 次に確認すべきこと: 上限到達時に読み込み結果へ警告を返す必要があるか確認する。

# 2026-08-03: Color grading preset input bounds

- 関連: `Artifact/src/Color/ArtifactColorGradingEngine.cppm`
- 事実: color grading preset の読み込みにファイル16 MiB、grading node 100000件の上限を追加した。
- 価値: 外部grading presetの巨大JSONやnode配列による過剰なメモリ・処理量を抑える。
- 次に確認すべきこと: 実運用のgrading presetサイズが上限に収まるか確認する。

# 2026-08-03: Workspace JSON file size bound

- 関連: `Artifact/src/Core/ArtifactWorkspaceManager.cppm`
- 事実: workspace session/layout JSON の読み込み前に8 MiBのファイルサイズ上限を追加した。
- 価値: 壊れた、または巨大化したworkspace設定で `readAll()` が過剰なメモリを消費する経路を抑える。
- 次に確認すべきこと: 既存workspace設定の最大サイズを確認する。

# 2026-08-03: Revision storage input bounds

- 関連: `Artifact/src/Project/ArtifactRevisionService.cppm`
- 事実: revision ledgerを16 MiB、snapshot JSONを256 MiB、ledger内revision件数を100000件に制限した。
- 価値: 履歴復元時の巨大JSON読み込みと大量record生成によるメモリ・処理量を抑える。
- 次に確認すべきこと: 大規模プロジェクトのsnapshot実サイズと履歴件数が上限内か確認する。

# 2026-08-03: Color palette input bounds

- 関連: `Artifact/src/Color/ColorPaletteManager.cppm`
- 事実: palette JSONの読み込みにファイル16 MiB、palette 100000件の上限を追加した。
- 価値: 外部paletteの巨大配列や異常なパス指定による過剰な読み込みを抑える。
- 次に確認すべきこと: 実運用paletteのサイズと件数が上限内か確認する。

# 2026-08-03: Animation preset input bound

- 関連: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- 事実: Property Widgetのanimation preset JSON読み込みに16 MiBのファイルサイズ上限を追加した。
- 価値: ユーザー選択ファイルの巨大JSONがUI操作中に無制限に読み込まれる経路を抑える。
- 次に確認すべきこと: 既存animation presetの最大サイズを確認する。

# 2026-08-03: Bundle IPC response bound

- 関連: `Artifact/src/Application/ArtifactProjectBundleIpc.cppm`
- 事実: Bundle IPCのraw responseとJSON payloadを32 MiB以下に制限した。
- 価値: IPC相手からの巨大応答で受信バッファとJSONパースが過剰に膨らむ経路を抑える。
- 次に確認すべきこと: bundle export/importの正当な応答サイズが上限内か確認する。

# 2026-08-03: Locale flatten bounds

- 関連: `Artifact/src/Translation/TranslationManager.cppm`
- 事実: locale JSONのflatten処理に項目100000件、object depth 64の上限を追加した。
- 価値: サイズ上限内でも極端に深い、または多数の翻訳項目を持つJSONによる再帰・map更新の過剰化を抑える。
- 次に確認すべきこと: 翻訳カタログの実項目数とネスト深度を確認する。

# 2026-08-03: External renderer summary bound

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: external renderer summary JSONの読み込みを16 MiB以下に制限した。
- 価値: 外部rendererからの巨大summaryでqueue処理中のJSONバッファが過剰化する経路を抑える。
- 次に確認すべきこと: external rendererが生成するsummaryの実サイズを確認する。

# 2026-08-03: Shortcut preset input bound

- 関連: `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`
- 事実: shortcut preset JSONの読み込みに4 MiBのファイルサイズ上限を追加した。
- 価値: ユーザー選択のshortcut presetが巨大な場合にUI上で無制限に読み込まれる経路を抑える。
- 次に確認すべきこと: 既存shortcut presetの最大サイズを確認する。

# 2026-08-03: Debugger JSON input bounds

- 関連: `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- 事実: debugger bundle/state JSONの読み込みに8 MiBのファイルサイズ上限を追加した。
- 価値: 診断UIのJSON読み込みで巨大なdebug bundleやstateが無制限に展開される経路を抑える。
- 次に確認すべきこと: 通常のdebug bundle/stateサイズが上限内か確認する。

# 2026-08-03: Color grading preset name validation

- 関連: `Artifact/src/Color/ArtifactColorGradingEngine.cppm`
- 事実: grading preset名を空、`.`/`..`、256文字超、ディレクトリ区切りを含む値として保存・読み込みできないようにした。
- 価値: preset名から保存先パスを組み立てる際の意図しないディレクトリ逸脱を防ぐ。
- 次に確認すべきこと: 既存preset名の命名規則がこの制約に適合するか確認する。

# 2026-08-03: Color grading preset atomic save

- 関連: `Artifact/src/Color/ArtifactColorGradingEngine.cppm`
- 事実: grading preset保存をQSaveFile経由にし、全payload書き込み成功時のみcommitするようにした。
- 価値: 保存途中のI/O失敗で既存preset JSONが中途半端な内容に置き換わるリスクを抑える。
- 次に確認すべきこと: 実環境でpreset保存後の再読み込みを確認する。

# 2026-08-03: Workspace atomic save

- 関連: `Artifact/src/Core/ArtifactWorkspaceManager.cppm`
- 事実: workspace session/layout JSONの保存をQSaveFile経由にし、payload全量書き込み成功時のみcommitするようにした。
- 価値: 保存途中のI/O失敗でworkspace設定が空または不完全なJSONに置き換わるリスクを抑える。
- 次に確認すべきこと: 実環境でsession/layout保存後の復元を確認する。

# 2026-08-03: Effect preset atomic save

- 関連: `Artifact/src/Effect/ArtifactEffectPreset.cppm`
- 事実: effect preset collectionの保存をQSaveFile経由にし、payload全量書き込み成功時のみcommitするようにした。
- 価値: 保存途中のI/O失敗でpreset collection JSONが不完全な内容に置き換わるリスクを抑える。
- 次に確認すべきこと: 実環境でpreset保存後の再読み込みを確認する。

# 2026-08-03: Revision snapshot guard and atomic save fix

- 関連: `Artifact/src/Project/ArtifactRevisionService.cppm`
- 事実: latest snapshot hashのサイズ検証条件を成功時に処理する形へ修正し、ledger/snapshot保存をQSaveFile化した。
- 価値: 有効なlatest snapshotを誤って読み飛ばす不具合を直し、履歴保存途中のJSON破損リスクを抑える。
- 次に確認すべきこと: revision作成・再起動後のhead/snapshot復元を確認する。

# 2026-08-03: PSO cache input bound

- 関連: `Artifact/src/Render/ShaderManager.cppm`
- 事実: `ShaderManager` はDiligentの`IPipelineStateCache`を作成・保存・再読込し、各種描画PSOと`MeshRenderer`へ渡している。PSO cacheの読み込みは512 MiB以下のファイルに制限した。
- 価値: 起動時のGPU PSO cache読み込みで異常に巨大なバイナリが無制限にメモリへ展開される経路を抑える。
- 閃き・仮説: Diligent Archiverはこの実行時PSO cacheとは別の事前アーカイブ機構。PSO cacheの実測ヒット率・起動時間が先に確認できるまでは、Archiverの導入を優先しない。
- 次に確認すべきこと: 実GPU環境で生成されるPSO cacheの最大サイズ、cache hit率、初回／二回目起動時間を確認し、必要なら配布用shader/PSO archiveを設計レビューする。

# 2026-08-03: AI model list response bound

- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: model list API responseのJSON読み込みを16 MiB以下に制限した。
- 価値: 外部AI providerからの巨大なmodel list応答でUI側のバッファ・JSONパースが過剰化する経路を抑える。
- 次に確認すべきこと: 接続先providerのmodel list応答サイズが上限内か確認する。
# 2026-08-03: AI model list entry bound

- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: model listの採用件数を10000件、各model idを1024文字に制限し、非object要素を無視するようにした。
- 価値: サイズ上限内でも異常に大量・長大なmodel情報がUIリストへ展開される経路を抑える。
- 次に確認すべきこと: 接続先providerの通常model件数とid長を確認する。

# 2026-08-03: Local AI model list entry bound

- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: ローカルJSONからのmodel list抽出にも件数10000件、ID長1024文字、object型チェックを適用した。
- 価値: API応答とローカル設定の両方でmodel list展開量を一貫して制限する。
- 次に確認すべきこと: ローカルmodel list設定の通常件数とID長を確認する。

# 2026-08-03: Curl AI model list response bound

- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: curl経由のmodel list JSONも16 MiB以下に制限してからパースするようにした。
- 価値: QNetworkReply経路だけでなく、外部curl経路でも巨大stdout応答の展開を抑える。
- 次に確認すべきこと: providerのcurl応答サイズが上限内か確認する。

# 2026-08-03: MCP manual arguments bound

- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: MCP手入力tool arguments JSONを4 MiB以下に制限してからパースするようにした。
- 価値: UIからの巨大arguments入力でJSONパースやtool実行前のバッファ確保が過剰化する経路を抑える。
- 次に確認すべきこと: 通常のMCP tool argumentsサイズが上限内か確認する。

# 2026-08-03: MCP manual argument item bound

- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: MCP手入力argumentsが配列の場合、tool実行へ渡す要素数を10000件に制限した。
- 価値: バイト数内でも極端に細かい大量要素を持つJSONがtool実行へ流れる経路を抑える。
- 次に確認すべきこと: 通常のMCP arguments配列件数が上限内か確認する。

# 2026-08-03: AI model list duplicate suppression

- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: API/local JSONのmodel ID抽出時に重複IDを除外するようにした。
- 価値: providerや設定ファイルの重複項目でmodel selectorが不必要に膨らむことを防ぐ。
- 次に確認すべきこと: providerごとのID表記ゆれ（大文字小文字・別名）を確認する。

# 2026-08-03: OCIO selector string bounds

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: Working Space、Display、View、Looksのsetter入力を4096文字以内へ正規化した。
- 価値: Config未ロード時を含め、外部・UI由来の異常に長いOCIO selectorが状態へ保持される経路を抑える。
- 次に確認すべきこと: 実運用のOCIO selector名が上限内か確認する。

# 2026-08-03: Footage interpretation transfer bounds

- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`
- 事実: FootageからImageParamsへ素材別Color Space/Transfer Functionを渡す境界でも、4096/1024文字の上限を適用した。
- 価値: 古いprojectや別経路で長大な解釈値が保持されていても、画像生成処理へそのまま伝播しないようにする。
- 次に確認すべきこと: FootageInterpretService側の既存入力検証と上限を統一する。
# 2026-08-03: Footage frame-rate validation

- 関連: `Artifact/src/Service/FootageInterpretService.cppm`
- 事実: frame rate変更のpreflight/apply/current overrideで有限値化と0.001〜1000.0の範囲制限を適用した。
- 価値: NaN/Infや極端なframe rateがFootage解釈と保存状態へ伝播する経路を抑える。
- 次に確認すべきこと: UI入力側のframe rate範囲と同じ制約になっているか確認する。
# 2026-08-03: Layer effect envelope finite fallback

- 関連: `Artifact/src/Animation/ArtifactLayerEffectEnvelope.cppm`
- 事実: envelope sampleのeffect start/endが非有限値の場合に安全な既定値へフォールバックするようにした。
- 価値: 異常なanimation値が補間結果へNaN/Infとして伝播する経路を抑える。
- 次に確認すべきこと: envelope入力keyframeの有限値検証と整合するか確認する。
# 2026-08-03: Corner Pin warp implementation

- 関連: `Artifact/src/Effect/ArtifactCornerPinEffect.cppm`
- 事実: Corner Pinのplaceholder処理をhomography計算とOpenCV `warpPerspective`によるRGBA32F CPU warpへ置き換え、入力値・係数の有限値検証を追加した。
- 価値: コーナーピン効果が実際の画像変形を行い、異常な変換値では元画像へ安全にフォールバックする。
- 次に確認すべきこと: 透視変形、退化四辺形、alpha保持のruntime受入れを確認する。
# 2026-08-03: Stabilizer robustness and processing pass

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: Stabilizerの数値・画像サイズ検証、再実行時の状態リセット、完全トラック判定、有限値フォールバック、フレーム変換処理をまとめて整理した。
- 価値: 異常入力や再利用時の古いmotion状態による不安定なstabilization結果を抑える。
- 次に確認すべきこと: 複数フレーム、単一フレーム、特徴点不足、border fillのruntime受入れを確認する。
# 2026-08-03: Add Noise scale and GPU upload alignment

- 関連: `Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm`
- 事実: CPU/GPU両経路でノイズ座標へsizeを適用し、GPU upload bufferの形式・strideを利用するようにした。amount/size/seedも有限値・範囲検証した。
- 価値: CPU/GPUでノイズ粒度が一致し、異常パラメータや入力形式差による不安定な効果結果を抑える。
- 次に確認すべきこと: RGBA16F/RGBA32F、monochrome/color noise、GPU不可時fallbackのruntime受入れを確認する。
# 2026-08-03: Creative effects GPU upload alignment

- 関連: `Artifact/src/Effect/ArtifactCreativeEffects.cppm`
- 事実: Creative effectsのGPU入力をGpuImageUploadBuffer経由にし、RGBA16F/RGBA32F形式・row strideと出力RGBA channel orderを揃えた。
- 価値: GPU経路での入力形式差やstride誤認、出力channel order不整合を抑える。
- 次に確認すべきこと: 16F/32F素材と各creative effectのGPU/fallback結果をruntime確認する。

# 2026-08-03: Image pipeline audit — GPU cache is scaffold only

- 関連: `docs/analysis/IMAGE_PIPELINE_AUDIT_2026-08-02.md`, `ArtifactCore/include/Image/ImageF32x4_With_Cache.ixx`, `ArtifactCore/src/Image/ImageF32x4_With_Cache.cppm`
- 事実: `ImageF32x4RGBAWithCache` には CPU/GPU dirty flag と同期 API の宣言はあるが、GPU texture生成、CPU→GPU同期、GPU→CPU同期、dirty box操作は空実装。`GetGpuTextureUAV()` も CPU dirty 時にGPU更新ではなく逆方向同期を呼ぶ。
- 価値または懸念: 監査のGPU連携評価は「型と契約の存在」と「実動作」を分離して記述する必要がある。現状のままでは static layer cache や GPU effect の正しさをこの型の存在だけから推定できない。
- 次に確認すべきこと: `ArtifactCore` サブモジュール変更の明示承認後、Diligent の device/context 所有境界、upload format、readback policy を確認してから同期実装を設計する。ビルド・runtime検証なしに完了扱いしない。

# 2026-08-03: Flat named-channel EXR already exists beside OpenExr stub

- 関連: `ArtifactCore/src/IO/Image/ImageExporter.cppm`, `ArtifactCore/include/IO/Image/ImageExporter.ixx`, `ArtifactCore/include/Image/OpenEXR.ixx`
- 事実: `OpenExr` facade は空スタブだが、`ImageExporter::writeMultiChannel()` は OIIO `ImageOutput` を使い、MultiChannelImage の named channels、compression、colorspace、string metadata を flat EXRへ書き出す。Cryptomatte用のdraft channel生成も存在する。
- 価値または懸念: 「EXRが未実装」という表現は facade と実用出力経路を分けて記述する必要がある。次の課題は flat AOV writer の新規作成ではなく、既存writerの仕様検証・multi-part/Deep/Cryptomatte 1.3対応である。
- 次に確認すべきこと: OIIO writerの実ファイルを生成してchannel名・metadata・読み戻しを検証する。ビルド・runtime検証はユーザー承認後に実施する。

# 2026-08-03: GPU cache UAV direction correction

- 関連: `ArtifactCore/src/Image/ImageF32x4_With_Cache.cppm`
- 事実: `GetGpuTextureUAV()` は CPU dirty 時にGPU→CPU同期を呼んでいた。また、UAV取得時にSRV viewを返していた。
- 変更: 逆方向同期の呼び出しを除去し、view種別を `TEXTURE_VIEW_UNORDERED_ACCESS` に修正した。CPU→GPU upload自体は device/context 所有契約が未定義のため未実装のまま維持した。
- 次に確認すべきこと: Diligent runtimeでUAV view取得とresource state遷移を確認し、upload APIの所有境界が確定した後に明示同期を実装する。

# 2026-08-03: Planar tracker failure must remain planar

- 関連: `ArtifactCore/src/Tracking/MotionTracker.cppm`, `docs/planned/MILESTONE_PLANAR_TRACKER_2026-08-01.md`
- 事実: `TrackerType::Planar` のホモグラフィ推定に失敗した場合、従来は点トラッキングへフォールバックし、`trackRange()` も `trackForward()` の戻り値を無視していた。
- 変更: Planar モードの失敗を failure frame として記録し、追跡セッションを失敗扱いにするようにした。これにより CornerPin 等へ「ホモグラフィなしの成功結果」を渡さない。
- 次に確認すべきこと: 変換成功時のホモグラフィ、推定失敗時の `TrackResult::isValid`、CornerPin連携を runtime で確認する。OCIO は現行コードで実ライブラリの `.ocio` 読み込み・CPU/GPU shader生成まで実装済みのため、残課題は実配置経路と runtime 検証の確認。

# 2026-08-03: Evaluation-boundary normalization for interactive systems

- 関連: `ArtifactCore/src/Rig/Rig2D.cppm`, `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`, `ArtifactCore/include/Grid/ArtifactGridSystem.ixx`
- 事実: リグの Pose/IK、3D ギズモ、グリッドの各入力経路では、有限値・範囲・スナップを評価境界で正規化することで、保存値や UI 入力を破壊せずに異常値の伝播を抑えられる。
- 価値または懸念: 共有 GPU レンダラーの MFR はスレッド安全性が未確定なため、GPU を無理に並列化せず、まず状態境界と逐次経路を明確に保つ必要がある。
- 次に確認すべきこと: GPU リソース所有境界が確定した後、GPU MFR の並列化単位と同期契約を別途検証する。

# 2026-08-04: Watchpoint polling must not depend on data breakpoints

- 関連: `Artifact/src/AppMain.cppm`, `docs/planned/MILESTONE_MCP_AI_DEBUG_SYSTEM_2026-08-02.md`
- 事実: デバッグポーラーはDataBreakpoint配列が空の場合に早期returnしていたため、Watchpointだけを登録したセッションでは値取得が実行されなかった。
- 変更: DataBreakpointとWatchpointの双方が空の場合だけ早期returnするよう修正し、Watchpointの値を `lastWatchSnapshot` に保存する状態契約を追加した。
- 次に確認すべきこと: runtimeでWatchpoint単独登録後のsnapshot更新間隔、無効化・削除後の停止、古いstate JSONの正規化を確認する。

# 2026-08-05: Serialization registry should follow existing restore APIs

- 関連: `ArtifactCore/include/Serialization/JsonSerializableAdapter.ixx`, `ArtifactCore/include/AI/AIContext.ixx`, `ArtifactCore/include/Graphics/Effect/SurfaceFX.ixx`, `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- 事実: 既存の `toJson()` / `fromJson()` が値型として完結している型は、共通 JSON adapter と migration registry に段階移行できる。一方、callback、PImpl、shared-pointer 戻り値、復元 resolver が必要な型は adapter の汎用契約だけでは安全に復元できない。
- 価値または懸念: 登録数を増やすことより、復元 API と adapter の型契約が一致していることを優先すべき。未検証の型を registry に追加すると、履歴／プロジェクト読み込み時の静かな復元失敗につながる。
- 次に確認すべきこと: 残りの既存型は復元責務を先に定義し、必要なら resolver／snapshot API を設計してから登録する。adapter登録後の実ビルド・typed envelope round-trip はユーザー承認後に検証する。

# 2026-08-05: Undo persistence must validate before factory restoration

- 関連: `Artifact/src/Undo/UndoManager.cppm`, `Artifact/include/Undo/UndoManager.ixx`
- 事実: Undo 履歴と offload ファイルは version、payload／entry 件数、文字列長、JSON object 形状、serialization payload を境界で検証し、Factory生成後は `deserialize()` を先に実行してから `canSerialize()` と memory budget を評価する。
- 価値または懸念: 復元前の未初期化 ID を `canSerialize()` が検査すると、正しい履歴まで拒否する。逆に無制限の JSON／画像 payload を受け入れると、履歴ロードがメモリ圧力や巨大ファイル生成の経路になる。
- 次に確認すべきこと: build/runtimeで履歴 round-trip、旧／未知 version、壊れた offload、single-entry／total budget 超過時の保持状態を確認する。

# 2026-08-05: SerializableCommand can share the serialization identity contract

- 関連: `ArtifactCore/include/Command/SerializableCommand.ixx`, `ArtifactCore/include/Serialization/ISerializable.ixx`
- 事実: Core の `SerializableCommand` は既存の `serialize/deserialize` JSON API を維持したまま `ISerializable` を継承し、`commandType()` を `typeName()`、schemaVersion を 1 として共通 registry 契約へ接続できる。
- 価値または懸念: アプリ層 `UndoCommand` の resolver／memory budget と、Core 層のコラボ command factory は別責務のまま、型識別と schema 契約だけ共有できる。両方を同一履歴スタックへ統合するには別途 ownership／merge policy が必要。
- 次に確認すべきこと: コラボ transport が typed envelope を要求する場合の command factory adapter と、UndoManager との境界を設計レビュー後に追加する。

# 2026-08-05: Registry key types must stay native at enumeration boundaries

- 関連: `ArtifactCore/include/Serialization/SerializationRegistry.ixx`
- 事実: Serialization Registry の map key は `QString` なので、登録型一覧を作る際に `QString::fromStdString()` を使うのは型契約に反していた。登録一覧は key を直接 `QStringList` へ追加する必要がある。
- 価値または懸念: typed registry の列挙 API は診断・migration 監査でも使われるため、境界変換を推測で行わず、コンテナの実型に合わせる必要がある。
- 次に確認すべきこと: registry の重複登録ポリシーを設計し、同一 typeName の初期化順序を診断できるようにする。

# 2026-08-05: Normalize migration type names at the registry boundary

- 関連: `ArtifactCore/include/Serialization/SchemaMigration.ixx`
- 事実: migration registry は内部で `std::string` を使うが、公開 API は `QString` を受け取る。登録と検索で trim 規則を統一し、負の schema version を path 探索から拒否することで、空白差分や不正 version による不一致を防げる。
- 価値または懸念: migration path 探索のグラフ自体は変更せず、入力境界の正規化だけで既存登録の再現性を高められる。
- 次に確認すべきこと: 複数段階 migration の round-trip と、未知／負の version の明示的失敗を検証する。

# 2026-08-05: Array JSON payloads need an object envelope for ISerializable

- 関連: `ArtifactCore/include/Serialization/JsonSerializableAdapter.ixx`, `ArtifactCore/src/Rig/Rig2D.cppm`
- 事実: `ISerializable::serialize()` の共通戻り値は `QJsonObject` だが、`RigControlSet2D` の既存 API は `QJsonArray` を返す。`JsonArraySerializableAdapter` は配列を `items` object member に包み、typed envelope と共存させる。
- 価値または懸念: 既存の配列 JSON 形式を破壊せず Registry／migration の object 契約へ接続できる。adapter は `fromJson(QJsonArray)` の値型だけを対象とし、pointer／polymorphic 戻り値は自動登録しない。
- 次に確認すべきこと: 配列型の typed envelope round-trip と、空／非配列 `items` の明示的失敗を検証する。

# 2026-08-05: GPU effect caches should be audited in two layers

- 関連: `Artifact/src/Effects/`, `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`
- 事実: 多数の GPU エフェクトで output／staging texture の毎フレーム生成が残っていた。一方、Temporal Smear のように履歴テクスチャを持つ実装は単純な output cache と異なるライフサイクルを持つ。また Core の `GPUTexture` は現状メタデータ abstraction であり、Diligent resource ownership の導入は別の大きな設計変更になる。
- 価値または懸念: output／staging の条件付き再利用は局所的な性能改善になるが、device 切替、履歴更新、pipeline／executor の寿命を同時に扱わないと不整合を招く。Core の低レベル ownership をアプリ側の局所修正と混同しないことが重要。
- 次に確認すべきこと: build/runtime で device 再初期化、解像度変更、staging readback、履歴系エフェクトのフレーム連続性を確認し、次段階で Core の `GPUTexture` ownership 方針を設計レビューする。

# 2026-08-06: 3D instance binning should precede mesh-shader-only work

- 関連: `Artifact/App/shaders/ShaderInterop_Renderer.h`, `ArtifactCore/src/Graphics/MeshRenderer.cppm`, `Artifact/src/Render/ArtifactIRenderer.cppm`
- 事実: renderer の共有 shader 定義には meshlet の頂点／三角形上限と indirect draw 用のデータがあり、Diligent 側には indirect draw と mesh-shader feature 判定の経路がある。
- 閃き・仮説（未検証）: まず compute shader で 3D instance を frustum cull し、material／PSO 単位で binning して通常の indexed indirect draw を発行する。meshlet の cone culling／GPU LOD、mesh shader はその上に追加する段階とする。
- 価値または懸念: クローン・多数メッシュ・particle 系の CPU draw submission を減らせる可能性があるが、2D レイヤー合成の overdraw／blend 負荷には効かない。DX12 Work Graphs 専用にすると Vulkan との共通経路を失う。
- 次に確認すべきこと: 代表 3D scene で CPU submit 時間、visible instance 数、material/PSO 切替数、GPU 時間を計測してから対象を選定する。

# 2026-08-08: Shared render device leases need balanced release

- 関連: `Artifact/include/Render/DiligentDeviceManager.ixx`, `Artifact/src/Effects/ColorCorrection/InvertEffect.cppm`, `Artifact/src/Effects/`
- 事実: `acquireSharedRenderDeviceForCurrentBackend()` は内部のmanual refCountを増加させ、対応する`releaseSharedRenderDevice()`を要求する。InvertのGPU経路を含む複数のeffectは取得後にDiligent smart pointerだけを解放し、manual refCountを減らしていない。
- 変更: 今回触れたInvert effectにはscope leaseを追加し、すべての早期returnを含めてreleaseを保証した。
- 価値または懸念: 未解放refCountが蓄積するとshared deviceの破棄・再生成条件が成立せず、device lossやbackend切替時の寿命問題になる可能性がある。他effectを一括変更すると影響範囲が広いため、現依頼では展開していない。
- 次に確認すべきこと: `Artifact/src/Effects/`の全acquire/release対応を静的棚卸しし、共通RAII leaseをDeviceManager APIとして提供する設計を検討する。device lossとbackend切替をruntimeで確認する。

# 2026-08-08: Tight Alignment belongs behind a Diligent buffer opt-in

- 関連: `libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h`, `libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/src/BufferD3D12Impl.cpp`, `Artifact/src/Render/DiligentImmediateSubmitter.cppm`
- 事実: Diligent D3D12 backendは通常のbufferをcommitted resourceとして生成する。Agility SDK 1.619のTight Alignmentはfeature tierを照会し、resource作成時の`D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT`で明示opt-inできる。
- 変更: backend固有のresource作成をアプリから迂回せず、`MISC_BUFFER_FLAG_TIGHT_ALIGNMENT`をDiligentの公開buffer記述に追加し、Tier 1のdefault-heap bufferだけでnative flagへ変換した。最初の対象は小さなimmutable index / vertex bufferに限定した。
- 価値または懸念: dynamic/upload/readback、sparse、textureに同じ最適化を拡張すると既存のallocationやsuballocationの契約を変える。未対応backendはhintを無視するため、共通render pathの契約は変わらない。
- 次に確認すべきこと: 実機でresourceのallocation alignment、VRAM使用量、描画parityを確認し、効果が測定できる小さなdefault-heap bufferだけを追加候補にする。

# 2026-08-08: Internal test runner is a product dependency, not a CTest-only target

- 関連: `Artifact/CMakeLists.txt`, `Artifact/src/AppMain.cppm`, `Artifact/src/Test.cppm`, `Artifact/src/Test/`
- 事実: ルートの `ARTIFACT_BUILD_TESTS` は `tests/` の GTest 登録だけを制御する。一方、通常の `Artifact` target は `src/Test.cppm` と複数の `src/Test/*.cppm` を収集し、`AppMain` が `Artifact.TestRunner` を直接 import している。
- 価値または懸念: 通常ビルドからテストコードを除外するだけでは AppMain のモジュール依存を壊す。開発用の内蔵テストを任意 target 化できれば通常ビルドの規模を減らせる可能性があるが、起動時のUI・CLI導線を含む仕様変更になる。
- 次に確認すべきこと: 内蔵テストランナーの利用導線を整理し、明示的な開発機能フラグまたは別 executable が許容されるかを設計判断する。その後にのみ、test module の target 分離と通常アプリからの import 除去を行う。

# 2026-08-08: 静止画sourceのproject相対path契約

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`ArtifactCore/include/Utils/Path.ixx`、`ArtifactCore/src/Asset/AssetImporter.cppm`。
- 事実: `ArtifactImageLayer`は`image.sourcePath`をそのままJSON保存・`loadFromPath()`へ渡す。`AssetImporter`はsource pathをabsolute pathとして登録しており、layer側にproject rootやproject relocation contextを受け取る経路は確認できない。
- 気づき（未検証）: layer側だけで相対化／再展開するとAssetManagerのsource identityやrelinkとの二重解決になり得る。project serializerまたはAssetManagerが唯一のbase path policyを提供する必要がある。
- 価値／懸念: projectフォルダ移動後のstill image relinkを安定させるには必要だが、個別layerの場当たり実装はshared/localized source契約を壊す可能性がある。
- 次に確認すべきこと: `ArtifactProject`のsave/load境界にsource path resolverを設計し、image/video/sequenceを同じpolicyへ移行できるか確認する。

# 2026-08-08: 静止画レイヤー受入fixtureの不足

- 関連: `docs/analysis/STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX_2026-08-08.md`、`ArtifactCore/include/Generate/GenerateTestImage.ixx`、`Artifact/src/Widgets/Render/ArtifactLayerCompositeTestWidget.cppm`。
- 事実: 既存のtest widgetとRGBA floatのテストパターン生成器は確認できたが、受入表のPNG／TIFF／EXR／PSD／CMYK／orientation素材へ対応付けられた固定fixtureファイルはワークスペース内で確認できない。
- 気づき（未検証）: runtime受入を再現可能にするには、ライセンス確認済みの小さなfixtureセットと期待metadata／期待pixel結果をversion管理する必要がある。
- 価値／懸念: 現状の手動素材だけではpreview／Render Queue比較の回帰検証が再現しにくい。fixture追加は素材ライセンスとテスト実行方針の判断を伴う。
- 次に確認すべきこと: 既存のQA asset保管場所とライセンスを確認し、最小fixtureセットをどのリポジトリ責務で管理するか決める。

# 2026-08-08: Text Animator検証を阻む旧ドライブ参照

- 関連: `build/ArtifactCore/ArtifactCore.vcxproj`、`ArtifactCore/include/Text/TextAnimator.ixx`、Text Animator runtime検証
- 事実: `J:\dev\ArtifactStudio` の既存Visual Studio build構成がコンパイル入力として `X:\Dev\ArtifactStudio` を参照しており、`OperationgSystem.ixx.ifc` を開けずArtifactCoreビルドが停止した。今回のText Animator変更に到達する前の失敗である。
- 価値または懸念: 現在のソースと別ドライブのソースが混在するため、ビルド成功時でも編集内容を正しく検証した保証が弱くなる。構成再生成なしでruntime完了扱いにしない。
- 次に確認すべきこと: ユーザー許可のもと現在の `J:` workspaceからbuild構成を再生成し、Selector Order、Anchor Grouping、Range handleを順にruntime確認する。

# 2026-08-09: Viewport変換キーはAE式ツール切替とBlender式モーダル操作が競合する

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/ArtifactToolBar.cppm`、`ShortcutBindings`。
- 事実: Composition Viewには現在 `W/E/R` による移動・回転・スケールのツール切替があり、別経路では `R` や `G` がAE系のツール／ペン操作として使われる。Blender式の `G/R/S` モーダル変換をそのまま追加すると、既存ツール切替と競合する。
- 気づき（未検証）: Viewportフォーカス時だけ有効なモーダル変換コンテキストを `ShortcutBindings` に追加し、テキスト入力・ペン・Rig等の専用ツールを除外したうえで、既存AE式ツール切替を設定プリセットとして残す構成が必要。
- 価値または懸念: 生のキー判定を追加し続けると、同じキーでもWidgetやOS入力経路によって挙動が変わる。既定プリセットを決めずに既存割当を置換すると利用者の操作を破壊する。
- 次に確認すべきこと: Viewport用Shortcut contextと変換セッションの状態機械を設計し、`G/R/S`、`X/Y/Z`、左クリック／Enter確定、右クリック／Escキャンセルの優先順位を決める。
## 2026-08-09 — Z transform key state lacks symmetric non-key APIs

- **関連:** `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- **確認できた事実:** XY position / scalar rotation / XY scaleにはinitial/current更新とキー削除APIがある一方、Z scaleには`setCurrentScaleZ`相当とZ専用キー削除APIがない。`setScale(time, x, y, z)`はXYZキーを追加する。Z positionもキー削除はXYと分離されていない。
- **価値・懸念:** Artifact側のギズモでXY変形はAuto Key設定に従って非キー更新できるが、Z scaleを実際に変更するとAuto Key無効時でもキー作成を完全には避けられず、取消時にZキーだけを対称に除去できない。
- **次に確認すべきこと:** Core変更が明示的に許可された段階で、Z position/scaleのcurrent/initial setter、has/remove key APIをXYと対称に追加し、3DギズモUndoスナップショットへZキー状態を独立保持する。

# 2026-08-09: Calm theme can reuse the existing palette contract

- **関連:** `ArtifactCore/include/Utils/WindowStyleCSS.ixx`、`Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`、`docs/planned/MILESTONE_NEURODIVERSITY_ACCESSIBILITY_2026-08-08.md`
- **事実:** UIテーマは `DccStylePreset` のトークンを `buildDCCPalette()` へ渡し、`AppMain` が設定変更後に既存ウィジェットへ再適用する経路になっている。Calm はこの経路に低彩度の背景・テキスト・アクセント・選択色を追加すれば、QtCSSや新規シグナルなしで主要UIへ伝播できる。
- **価値または懸念:** N-1のUIクロームは局所的に実装できる一方、ビューポート画像の輝度上限や高速点滅検出はテーマトークンとは別の描画／フレーム解析責務であり、同じ変更に混ぜると検証範囲が広がる。
- **次に確認すべきこと:** Calm選択時のビューポート背景・ギズモ色・オーバーレイ色の適用点を棚卸しし、コンテンツ映像そのものを変更せずUI補助色だけを制御できる境界を決める。

# 2026-08-09: Focus mode should snapshot chrome and ADS dock visibility

- **関連:** `Artifact/include/Widgets/ArtifactMainWindow.ixx`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`、`docs/planned/MILESTONE_NEURODIVERSITY_ACCESSIBILITY_2026-08-08.md`
- **事実:** ArtifactMainWindow は通常の QMainWindow ではなく、QADS の `CDockManager` と独自の root layout でメニューバー、ツールバー、オプションバー、中央ドック、ステータスバーを構成している。既存の `setDockImmersive()` は単一ドック用の可視性保存を持つ。
- **気づき（未検証）:** フォーカスモードはADS全体のレイアウトを再構築せず、クロームと非中央ドックの可視性だけをスナップショットすれば、ユーザーのタブ／splitter／floating配置を壊さずに復元できる。
- **価値または懸念:** QADSの `toggleView()` は floating dock の表示状態にも関係するため、実機で floating／lazy dock、閉じたドック、起動直後のレイアウト復元を確認する必要がある。
- **次に確認すべきこと:** `Ctrl+Shift+F` のグローバルイベント経路、コマンドパレットとの併用、focus mode 中のドック生成・設定変更時の可視性を runtime で確認する。

# 2026-08-09: C++20 module DLL export requires a narrow explicit API boundary

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Color/ArtifactOCIOManager.ixx`、`Artifact/include/Color/ArtifactColorScienceManager.ixx`、`Artifact/include/Color/ColorPaletteManager.ixx`
- **事実:** `WINDOWS_EXPORT_ALL_SYMBOLS` だけでも `ArtifactColor.dll` は生成できたが、C++20 module をまたいで `Artifact` 本体から利用する公開メンバーが不足した。既存の `LIBRARY_DLL_API` を3クラスに限定して付けると、ArtifactColor DLL のビルドと本体リンク時のColor系未解決シンボルが解消した。
- **価値または懸念:** DLL化の境界をクラス単位で明示できる一方、マクロを広範囲へ追加するとABIとBMIの影響範囲が増える。利用側の `dllimport` 方針は、API面を固定してから別途確認する。
- **次に確認すべきこと:** ArtifactColor の安定公開APIを定義し、DLLロードと主要API呼び出しを検証する。次のDLL候補も同じく小さな境界から選ぶ。

# 2026-08-09: Artifact Debug link needs no executable PDB under the current MSVC

- **関連:** `Artifact/CMakeLists.txt`、`out/build/x64-Debug/bin/Debug/Artifact.exe`
- **事実:** MSVC 14.51 rejects the old `/DEBUG:FASTLINK` behavior and falls back to `/DEBUG:FULL`; the monolithic Artifact executable then hits `LNK1140`. Adding `/PDB:NONE` only to the Debug `Artifact` executable allows the full link to complete while preserving embedded object debug information.
- **価値または懸念:** The build is now reproducible with the current toolchain and avoids a multi-gigabyte executable PDB, but executable-level symbol browsing is reduced. A newer toolchain or a smaller target split can restore a full PDB later.
- **次に確認すべきこと:** Keep `/PDB:NONE` scoped to Debug until the executable is split or the toolchain policy changes; do not propagate it to DLLs or library targets.

# 2026-08-09: Effect contract can be split with a small explicit export surface

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/ArtifactAbstractEffect.ixx`、`Artifact/include/Effects/ArtifactEffectImplBase.ixx`、`Artifact/include/Effects/EffectContext.ixx`、`Artifact/include/Effects/ArtifactAbstractField.ixx`、`Artifact/include/Effects/ArtifactEffectFrameSampler.ixx`
- **事実:** `ArtifactEffectContract` は公開モジュール5本・実装3本の小さな契約層で、`ArtifactEffects` と `Artifact` の両方から利用される。`ArtifactAbstractEffect`、`EffectID`、`ArtifactEffectImplBase`、`IEffectFrameSampler`、`ArtifactAbstractField`、`ArtifactEffectFrameSampler` に限定して既存の `LIBRARY_DLL_API` を付けることで、DLL化後の本体リンクが成功した。
- **価値または懸念:** 大規模なEffect実装群をDLL境界へ広げず、契約層だけを分離できる。公開値型を漏らすとリンク時に個別シンボル不足になるため、契約ヘッダの値型も公開表面として棚卸しする必要がある。
- **次に確認すべきこと:** 次候補へ進む前に、契約DLLの利用側に `dllimport` を導入するか、現在の自動公開方式を維持するかをAPI方針として決める。

# 2026-08-09: Composition export bake needs a frame-sequence boundary

- **関連:** `Artifact/src/Export/ArtifactExportPreRenderPipeline.cppm`、`Artifact/src/Export/ArtifactExportLottieWriter.cppm`、`docs/planned/MILESTONE_COMPOSITION_EXPORT_GAME_UI_2026-08-08.md`
- **事実:** マスク・エフェクト・3Dなどネイティブ変換できないレイヤーは、既存のDiligentオフスクリーンレイヤーレンダラーを優先してフレーム列へベイクし、GPU初期化できない場合だけ既存サムネイル経路へフォールバックする。RmlUi／Gameface／Unity／Noesisは画像列を各形式の離散アニメーションへ接続し、Lottieはフレームごとの画像レイヤーへ展開する。
- **気づき（未検証）:** ベイク対象をグループ単位で最適化するには、レイヤー単位の画像列を統合するアトラス仕様と、各ランタイムの画像キャッシュ／読み込み契約が必要になる。現状は正確性を優先して同じフレーム列を各Writerが参照する。
- **価値または懸念:** 現状の出力は静止UI・軽量な2D変換アニメーションに加え、非対応レイヤーの時間変化も保持できるが、複雑エフェクトを含むモーションUIの完全移植とはまだ言えない。全フレームベイクは書き出し時間・容量・ランタイム参照方式を同時に決める必要がある。
- **次に確認すべきこと:** RmlUi／Gameface／Unity／Noesis／Lottieの画像列接続は初期実装済み。現実装はフレームごとのGPU初期化を避けていないため、実機検証前にオフスクリーンコンテキストをWriter単位で再利用できるか確認する。各ランタイムでの画像キャッシュと離散アニメーションの対応も確認する。

# 2026-08-09: Onion skin calculation path is active for paint layers

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`、`ArtifactCore/src/Application/ArtifactAppSettings.cppm`
- **事実:** 既存のキャプチャ方式は残したまま、選択中のペイントレイヤーでは既存の `drawPaintLayerOnionSkinOverlay()` を優先して呼び出し、キャプチャのキュー投入を抑止する経路を接続した。ペイントレイヤー以外は従来のキャプチャ方式へフォールバックする。モーションパス設定の未保存時デフォルトは表示オンに変更した。
- **価値または懸念:** ペイントのフレームバッファを基にした計算方式へ移行できる一方、現実装の計算方式はペイントレイヤー向けであり、他レイヤーの一般化は別設計になる。既存ユーザーが保存したオフ設定は維持される。
- **次に確認すべきこと:** ビルド・実機で、ペイントレイヤーの前後フレーム表示、レイヤー切替時のキャプチャ残像、保存済みモーションパス設定の互換性を確認する。

# 2026-08-09: Selected dock tab used the content background as its fill

- **関連:** `Artifact/src/Widgets/Dock/DockStyleManager.cppm`
- **事実:** 選択中のQADSドックタブは `theme.backgroundColor` を背景に使っており、コンテンツ面と同色になるため、アクティブフレームの塗りつぶしが視認できなかった。選択色トークンを選択中タブの背景に割り当てた。
- **価値または懸念:** 既存のQPalette／テーマ経路のまま、After Effects、Nuke、Default Qt、High Contrastの各テーマで選択タブを識別しやすくできる。実機で文字色とのコントラストは確認が必要。
- **次に確認すべきこと:** ドックの選択切替、浮動ドック、非アクティブウィンドウ、テーマ変更後のタブ再描画を確認する。

# 2026-08-09: Composition export must resolve root parents from the live layer graph

- **関連:** `Artifact/src/Export/ArtifactExportSession.cppm`、`Artifact/src/Export/ArtifactExportRmlUiWriter.cppm`、`Artifact/src/Export/ArtifactExportGamefaceWriter.cppm`、`Artifact/src/Export/ArtifactExportUnityUxmlWriter.cppm`、`Artifact/src/Export/ArtifactExportNoesisXamlWriter.cppm`
- **事実:** Export のツリー構築は `parentLayerId().toString()` をそのまま使うと、未接続の新規レイヤーでルート判定が不安定になる可能性がある。Session では実際の `parentLayer()` が存在する場合だけ親 ID を保存するよう正規化した。
- **価値または懸念:** 各 Writer が共通の空親 ID からルートを辿れるため、新規コンポジションのレイヤー欠落を防げる。親関係が壊れたプロジェクトでは、未接続レイヤーをルートとして扱うため、ランタイム側の修復状態を隠す可能性がある。
- **次に確認すべきこと:** 実機検証で、新規レイヤー、親子レイヤー、親削除後の孤児レイヤーを各出力形式へ書き出し、ルートとネストが一致することを確認する。

# 2026-08-09: Exported UI identifiers need an XML-safe leading character

- **関連:** `Artifact/src/Export/ArtifactExportRmlUiWriter.cppm`、`Artifact/src/Export/ArtifactExportGamefaceWriter.cppm`、`Artifact/src/Export/ArtifactExportUnityUxmlWriter.cppm`、`Artifact/src/Export/ArtifactExportNoesisXamlWriter.cppm`
- **事実:** レイヤー ID は UUID 由来のため数字始まりになり得る。Noesis の `x:Name` などの識別子へそのまま出すと不正またはターゲット参照不能になる可能性があるため、4 Writer の `safeId()` を英数字・アンダースコアへ正規化し、必要時に `layer_` を付けるよう統一した。
- **価値または懸念:** 出力された UI ツリーの名前参照と CSS セレクターが安定する。一方、元レイヤー ID と外部スクリプトが直接結び付いている場合は、サニタイズ後 ID の対応表が別途必要になる。
- **次に確認すべきこと:** 実機検証で数字始まり・記号を含むレイヤー ID を含むコンポジションを各形式へ出力し、参照とアニメーションターゲットが一致することを確認する。

# 2026-08-09: Shared export asset names should be safe before format writers see them

- **関連:** `Artifact/src/Export/ArtifactExportSession.cppm`、RmlUi / Gameface / Unity / Noesis の画像参照
- **事実:** 元画像のファイル名をそのまま相対 URL に使うと、引用符や空白などが HTML/XML/CSS 属性を壊す可能性がある。Session の共有アセット収集でコピー先名を正規化し、同名衝突時は連番を付けるようにした。
- **価値または懸念:** Writer ごとの個別エスケープに依存せず、全形式で同じ安全な参照パスを利用できる。既存の外部スクリプトが元ファイル名を期待する場合は、出力アセット名との対応表が必要になる。
- **次に確認すべきこと:** 実機検証で空白・記号・非 ASCII を含む画像名を含むコンポジションを出力し、コピー先と各形式の参照が一致することを確認する。

# 2026-08-09: Unity UI Toolkit image backgrounds need explicit dimensions

- **関連:** `Artifact/src/Export/ArtifactExportUnityUxmlWriter.cppm`
- **事実:** Unity UI Toolkit の通常画像は `VisualElement` の `background-image` だけでは自然サイズがレイアウトされず、幅・高さが 0 になる可能性がある。コピー後の画像実寸をフォールバックにし、シリアライズ済みの `image.width` / `image.height` を優先して USS に出力するようにした。
- **価値または懸念:** 静止画レイヤーが Unity UI Toolkit 上で可視になる条件を明示できる。実機の UI Toolkit バージョンによる background-image のレイアウト差は残るため、実ランタイム確認が必要。
- **次に確認すべきこと:** Unity UI Toolkit で通常画像、画像列ベイク画像、異なるスケール設定のレイヤーを読み込み、表示サイズと位置を確認する。

# 2026-08-09: Export keyframe joins must carry forward the latest property value

- **関連:** `Artifact/src/Export/ArtifactExportRmlUiWriter.cppm`、`Artifact/src/Export/ArtifactExportGamefaceWriter.cppm`、`Artifact/src/Export/ArtifactExportUnityUxmlWriter.cppm`、`Artifact/src/Export/ArtifactExportNoesisXamlWriter.cppm`
- **事実:** 各 Writer は位置・回転・スケールのキーをまとめて出力する際、別プロパティのキーが存在しないフレームで初期値へフォールバックしていた。これにより、位置だけを変更した後に回転キーを通過すると位置が一時的に戻る出力になり得た。
- **気づき（未検証）:** 同一タイムライン上の異なるプロパティを疎なキー列から構成する場合、各プロパティは指定フレーム以下の最新キーを保持する必要がある。Writer側でこの解決を統一すれば、Core側のアニメーションモデルを変更せずに形式間の挙動を揃えられる。
- **価値／懸念:** CSS、USS、Noesis Storyboard のキーフレーム結合が安定する一方、Bezier補間やイージングはまだ各形式の表現力に依存する。
- **次に確認すべきこと:** 実機検証で位置・回転・スケールを異なるフレームに設定したレイヤーを出力し、各ランタイムで初期値への瞬間的な戻りがないことを確認する。

# 2026-08-09: Export animations need an explicit base frame before the first key

- **関連:** `Artifact/src/Export/ArtifactExportRmlUiWriter.cppm`、`Artifact/src/Export/ArtifactExportGamefaceWriter.cppm`、`Artifact/src/Export/ArtifactExportUnityUxmlWriter.cppm`、`Artifact/src/Export/ArtifactExportNoesisXamlWriter.cppm`、`Artifact/src/Export/ArtifactExportLottieWriter.cppm`
- **事実:** Writer の統合キー列にコンポジション開始フレームが含まれていないと、最初のキーが後ろにあるアニメーションで、ランタイムの fill／最初のキーフレーム解釈に開始時のベース変換を委ねることになる。
- **気づき（未検証）:** 出力形式ごとの fill-mode 差を避けるには、CSS／USS／Storyboard へ開始フレームを追加し、Lottie へもベース値の初期キーを追加する必要がある。
- **価値／懸念:** 遅延開始アニメーションの先行表示を防げる一方、Lottie のキー時刻と各ランタイムのフレーム原点が一致することは実機確認が必要。
- **次に確認すべきこと:** 開始フレーム後に最初のキーを置いたレイヤーを各形式へ出力し、開始時にベース変換が維持されることを確認する。

# 2026-08-09: Export writers must preserve layer in/out visibility

- **関連:** `Artifact/src/Export/ArtifactExportRmlUiWriter.cppm`、`Artifact/src/Export/ArtifactExportGamefaceWriter.cppm`、`Artifact/src/Export/ArtifactExportUnityUxmlWriter.cppm`、`Artifact/src/Export/ArtifactExportNoesisXamlWriter.cppm`
- **事実:** Lottie とベイク画像列にはレイヤーの in/out が出力されていたが、通常変換レイヤーの HTML/CSS・USS・Noesis 経路はレイヤー要素を常時表示していた。Gameface のベイク列も画像差し替えだけで時間範囲を制御していなかった。
- **気づき（未検証）:** 通常変換では変換キーフレームへ opacity の時間列を統合し、ベイク列では CSS／USS の可視アニメーションまたは Gameface のフレーム更新時 opacity、Noesis の opacity storyboard を使うのが最小の共通境界になる。
- **価値／懸念:** レイヤーの時間範囲が形式間で揃う一方、各ランタイムの animation fill と離散キーフレームの境界時刻は実機で確認する必要がある。
- **次に確認すべきこと:** レイヤー in/out がコンポジションの途中にある静止レイヤー、通常アニメーション、ベイク列を各形式で読み込み、開始前と終了後に非表示になることを確認する。

# 2026-08-09: Pre-render scale belongs at the export boundary

- **関連:** `Artifact/src/Export/ArtifactExportDialog.cppm`、`Artifact/src/Export/ArtifactExportPreRenderPipeline.cppm`、5形式の Export Writer
- **事実:** PreRender API は解像度を受け取れたが、Writer と Export Dialog から倍率を指定する契約がなく、実際の出力は常にレイヤーの基準サイズだった。
- **気づき（未検証）:** 1x〜4xの倍率を各Writerへ個別に実装するより、Writer options から `ArtifactExportPreRenderSequenceOptions` へ倍率を渡し、PreRender側で1〜4にクランプする境界が形式間で一貫する。
- **価値／懸念:** 高密度UI向けのベイク品質を選べる一方、倍率を上げるとGPUメモリ・書き出し時間・画像容量が増えるため、実機で許容値を確認する必要がある。
- **次に確認すべきこと:** 同一ベイク対象を1x、2x、4xで出力し、画像寸法、各形式の表示サイズ、ランタイムのメモリ使用量を比較する。

# 2026-08-10: MediaPlaybackController requires fallback FPS for unresolvable video streams

- **関連:** `ArtifactCore/src/Media/MediaPlaybackController.cppm`
- **事実:** 一部動画（MKV/WebM等の特定ストリーム）で FFmpeg の `avg_frame_rate` / `av_guess_frame_rate` / `r_frame_rate` / `nb_frames` の全抽出ルートが失敗し `fps_` が 0.0 のままとなるケースがあった。`fps_ <= 0.0` の場合、`decodeVideoFrameDirectAtFrameRaw` が `invalid state` と判定して全フレームデコードを拒否し、黒画面が発生していた。
- **気づき（未検証）:** `fps_` 確定の直後に `fps_ <= 0.0` 時のデフォルト値（30.0 fps）をセットするフォールバックを入れることで、ヘッダ情報が欠損・特殊なフォーマットでもデコード試行がスキップされずプレビュー画面が表示される。
- **価値／懸念:** 未対応・欠損ヘッダ動画の再生不能（黒画面）を低リスクで回復できる一方、実際の可変フレームレートや異質FPS素材では再生速度のズレが生じる可能性がある。
- **次に確認すべきこと:** ビルド確認後、実機にて FPS ヘッダが取れない動画ファイルを読み込み、黒画面にならずデコード・プレビュー描画が行われるか検証する。

# 2026-08-10: Hidden matte sources must remain in the source-resolution path

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`
- **事実:** Render Queue と preflight は hidden な matte source を参照対象として扱う一方、Composition Editor の GPU source 収集は source 候補を `isLayerEffectivelyVisible()` で先に除外していた。また、CPU evaluator は source が欠けた matte だけを飛ばして残りを部分適用していたが、GPU は欠損時にレイヤーを無加工へ戻す。
- **価値または懸念:** hidden source の matte が preview と Render Queue で一致し、欠損 source による部分マスクで意図しない透明化が起きにくくなる。欠損時は unmasked fallback になるため、preflight／診断で原因を知らせる運用は必要。
- **次に確認すべきこと:** ビルド・実機で hidden source、欠損 source、同一フレームの複数 consumer、source crop 変更を preview と Render Queue で比較する。

# 2026-08-10: GPU matte batches need per-source blend and opacity

- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx`、`ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- **事実:** GPU の3入力 matte shader は共通の stack mode と master opacity しか受け取れず、異なる blend mode／opacity や Difference を逐次 fallback していた。CPU evaluator は source ごとに opacity を適用してから各 blend mode で一つの mask に合成する。
- **価値または懸念:** shader に source ごとの blend mode・opacity を渡し、最大3参照を一回で合成できるようにしたため、混在した matte 設定でも CPU の合成順序に近づく。4参照以上の GPU 経路と実ランタイムの parity は未検証。
- **次に確認すべきこと:** ビルド・実機で Add／Intersect／Subtract／Difference と異なる opacity を2〜3枚組み合わせ、CPU・GPU・Render Queue の結果を比較する。

# 2026-08-10: Composition preview keeps a separate CPU matte evaluator

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- **事実:** Composition Editor の通常描画は共有 `applyLayerMatteReferencesToSurface` ではなく、controller 内の QImage／ImageF32x4 の `applyLayerMatteToSurface` overload を呼び出していた。この経路にも Luma の premultiplied RGB 読み取りと、欠損 source を除外して残りだけ適用する挙動が残っていた。
- **価値または懸念:** preview の CPU 経路も straight-alpha Luma と欠損時 unmasked fallback に揃え、GPU／Render Queue との表示差を減らした。古い未使用 evaluator の整理は別作業として残る。
- **次に確認すべきこと:** ビルド・実機で通常 CPU preview、GPU preview、Render Queue の Luma・透明色・欠損 source を比較する。

# 2026-08-10: GPU preview must not silently miscombine more than three mattes

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- **事実:** UI と CPU evaluator は4枚以上の matte reference を保持・合成できるが、Composition Editor GPU の track-matte pipeline は3 texture slot までである。4枚以上を逐次 shader pass へ送ると、source ごとの blend mode を一つの合成 mask として扱えない。
- **価値または懸念:** GPU preview は4枚以上を誤った部分マスクとして表示せず、警告付きの無加工 fallback にする。GPU preview での4枚以上の完全対応は別途 multi-pass mask 設計が必要。
- **次に確認すべきこと:** 4枚以上の matte を作成し、GPU preview が警告と無加工表示になり、CPU preview／Render Queue は全参照を合成することを実機で確認する。

# 2026-08-10: MatteTrackParams needs a fixed constant-buffer size contract

- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx`、`ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`
- **事実:** CPU の `MatteTrackParams` は HLSL constant buffer へ `memcpy` され、field の追加・並び替えがコンパイル時に GPU layout mismatch として検出される仕組みがなかった。
- **価値または懸念:** 48 bytes（16-byte register 3個）の static assertion を追加し、将来の field 変更で shader 側だけ値がずれる事故を早期検出できる。field の意味・順序そのものは HLSL と手動で保つ必要がある。
- **次に確認すべきこと:** ビルド時に assertion が成立することと、実機で3枚 matte の各 blend／opacity が対応することを確認する。

# 2026-08-10: Matte source layer opacity order remains a specification gap

- **関連:** `ArtifactCore/docs/MILESTONE_TRACK_MATTE_CORE_2026-03-26.md`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`
- **事実:** Core milestone は visibility／opacity／matte の適用順を固定する項目を残している。現行の source resolver は source layer の画像を取得し、参照側 `LayerMatteReference::opacity` は適用するが、source layer 自身の opacity を matte 値へ掛ける明示処理は確認できない。
- **気づき（未検証）:** source layer opacity を matte に含める仕様なら、preview／Render Queue／GPU source upload の全 resolver で同じ順序を実装する必要がある。逆に source opacity を無視する仕様なら、現状を契約として Core 文書へ明記すべき。
- **価値／懸念:** 曖昧なまま局所修正すると backend 間の差を増やすため、実装前に source opacity の期待結果を決める必要がある。
- **次に確認すべきこと:** matte source の opacity を0／0.5／1にしたとき、製品仕様として target mask が変わるべきかを確認する。
# 2026-08-10: Core MatteEvaluator の合成則を MatteStack と一致させた

- **関連:** `ArtifactCore/src/Layer/LayerMatte.cppm` の `MatteEvaluator::combine()`、`ArtifactCore/include/Layer/LayerMatte.ixx` の `evaluateMatteStack()`、アクティブな LayerMatteReference 評価。
- **確認できた事実:** `evaluateMatteStack()` と表示側の LayerMatteReference 評価は Common/Intersect を `min(current, next)`、Subtract を `max(0, current - next)` としている。一方、Core の `MatteEvaluator::combine()` だけが Common を乗算、Subtract を `current * (1 - next)` としていた。
- **対応:** Core の公開 evaluator を既存のスタック評価へ合わせ、同じ MatteStack semantics を共有するようにした。`MatteEvaluator::` の呼び出し箇所はこの散歩時点では検出されていないため、既存利用箇所の動作変更は確認できていない。
- **未確認:** ビルド・テストは AGENTS.md の指示により未実行。将来 `MatteEvaluator` を再利用する場合は、`MatteStackMode` と `MatteBlendMode` の対応を一つの共通実装へ整理できるか検討する。
# 2026-08-10: evaluateMatteStack の初回値と空スタックを中立化

- **関連:** `ArtifactCore/include/Layer/LayerMatte.ixx` の `evaluateMatteStack()`。
- **確認できた事実:** 結果をゼロ初期化したまま最初の有効 source に `Common` / `Subtract` の合成則を適用していたため、最初の matte が常にゼロへ潰れる場合があった。また、有効 node がない stack はゼロ mask を返していたが、`MatteEvaluator::evaluate()` は空入力を1.0として扱っている。
- **対応:** 最初の有効 matte mask はそのまま初期値として採用し、有効 node がない場合は全ピクセルを1.0にした。2枚目以降は既存の Add / Common / Subtract 規則を適用する。
- **未確認:** source が不足する enabled node の fallback は milestone 上も未確定のため、今回変更していない。ビルド・テストも未実行。
# 2026-08-10: evaluateMatteStack の nil source が後続 source の対応をずらす

- **関連:** `ArtifactCore/include/Layer/LayerMatte.ixx` の `evaluateMatteStack()` と `MatteStack::sourceLayerIds()`。
- **確認できた事実:** `sourceLayerIds()` は enabled かつ non-nil の node だけを source 配列へ対応させる一方、`evaluateMatteStack()` は enabled であれば nil source でも `sourceIndex` を消費していた。nil node の後ろに有効 node があると、後続 source が前の node に誤対応する。
- **対応:** stack 評価でも nil source node をスキップし、source 配列の対応規則を `sourceLayerIds()` と揃えた。
- **未確認:** ビルド・テストは未実行。missing な non-nil source の fallback 規則は引き続き未確定。
# 2026-08-10: MatteStack の nil layer ID を cycle と誤判定しない

- **関連:** `ArtifactCore/include/Layer/LayerMatte.ixx` の `MatteStack::hasCycleWithLayer()`。
- **確認できた事実:** nil の `layerId` を渡した場合、enabled な nil source node と `nil == nil` が成立し、実在しないレイヤーを cycle と報告し得た。
- **対応:** nil の引数を即時 false とし、source 側も non-nil の場合だけ比較するようにした。
- **未確認:** この Core helper の実利用箇所は散歩時点で検出されていない。ビルド・テストは未実行。
# 2026-08-10: MatteStack::isEmpty を有効な source 基準へ揃えた

- **関連:** `ArtifactCore/include/Layer/LayerMatte.ixx` の `MatteStack::isEmpty()`、`sourceLayerIds()`、`evaluateMatteStack()`。
- **確認できた事実:** `isEmpty()` だけが enabled かどうかのみを見ており、nil source の node だけでも non-empty を返していた。他の評価・source 列挙 API は nil source を除外している。
- **対応:** enabled かつ non-nil source が存在する場合だけ non-empty と判定するように統一した。
- **未確認:** ビルド・テストは未実行。missing な non-nil source の扱いは別途仕様確定が必要。
# 2026-08-10: 旧 MatteStack CPU helper も初回 mask と nil source を Core と一致させた

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `applyMatteStackToSurface()`。
- **確認できた事実:** この helper は散歩時点で caller を検出していないが、Core の `MatteStack` と同じ `Add / Common / Subtract` を実装していた。初回 mask をゼロから Common 合成し、nil source node でも source index を消費するため、再利用時に Core と結果が分岐する状態だった。
- **対応:** nil source をスキップし、最初の有効 source mask をそのまま採用してから2枚目以降を合成するよう揃えた。
- **未確認:** helper の実 runtime caller、ビルド・テストは未確認・未実行。
# 2026-08-10: MatteNode の欠落 mode は Alpha default を維持する

- **関連:** `ArtifactCore/include/Layer/LayerMatte.ixx` の `MatteNode` constructor / `fromJson()`。
- **確認できた事実:** constructor の既定 mode は `Alpha` だが、JSON に `mode` がない場合も `fromString("")` を通るため `None` へ変わっていた。保存データの欠落フィールドに対する既定値が一致していなかった。
- **対応:** `mode` が存在するときだけ復元し、欠落時は constructor の `Alpha` を保持するようにした。明示された未知・空文字の扱いは既存 parser のまま維持した。
- **未確認:** ビルド・テストは未実行。
# 2026-08-10: 無効な matte 参照では Composition View の cache を bypass しない

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `drawLayerForCompositionView()`。
- **確認できた事実:** `matteSourceImages` が存在し、`matteReferences()` が空でないだけで `hasResolvedMattes` が true になり、disabled／nil 参照しかないレイヤーでも surface／static／GPU texture cache を無効化していた。
- **対応:** enabled かつ non-nil の matte reference が実際にある場合だけ matte 適用と cache bypass を有効にした。描画結果は変えず、不要な再計算を避ける修正。
- **未確認:** runtime 性能とビルド・テストは未確認・未実行。
# 2026-08-10: Composition View の matte fast-path 判定を有効参照基準へ統一

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の effect／CV 境界判定。
- **確認できた事実:** `buildRasterizedSurfaceBuffer()` の2つの overload は、disabled／nil matte だけでも `!matteReferences().empty()` により重い mask／effect 経路を選択していた。実際の matte 適用と Render Queue の source 収集は enabled かつ non-nil を基準にしている。
- **対応:** `hasEnabledMatteReferences()` を追加し、2つの fast-path 判定を有効参照基準へ変更した。無効な参照の描画結果は従来と同じで、不要な処理だけを避ける。
- **未確認:** runtime 性能、ビルド・テストは未確認・未実行。
# 2026-08-10: Render Controller の direct matte 判定を有効参照基準へ統一

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の 3D direct-card、frame-buffer、scene-depth shortcut。
- **確認できた事実:** 6箇所が `matteReferences().empty()` だけで shortcut の可否を決めていたため、disabled／nil 参照だけでも direct path を拒否していた。Composition View 側は既に enabled かつ non-nil を基準にしている。
- **対応:** `layerHasEnabledMatteReferences()` を追加し、6箇所を同じ有効参照判定へ変更した。
- **未確認:** runtime の direct path 選択と性能、ビルド・テストは未確認・未実行。
# 2026-08-10: LayerMatteReference の JSON opacity を有限・範囲内へ正規化

- **関連:** `Artifact/include/Layer/ArtifactLayerMatte.ixx` の `LayerMatteReference::fromJson()`。
- **確認できた事実:** JSON から復元した `opacity` を無検証で float 化しており、範囲外や非有限値が matte evaluator／GPU parameter へ伝播する余地があった。評価側の期待契約は 0〜1。
- **対応:** 復元時に有限値を確認し、有限なら 0〜1 に clamp、非有限なら既定値 1.0 を採用するようにした。
- **未確認:** ビルド・テストは未実行。
# 2026-08-10: LayerMatteReference の JSON enum を範囲検証

- **関連:** `Artifact/include/Layer/ArtifactLayerMatte.ixx` の `LayerMatteReference::fromJson()`。
- **確認できた事実:** `type`、`blendMode`、`fitMode` を無検証で enum cast していた。範囲外値は matte 抽出、合成、fit の switch default へ流れ、入力値によって source が無視されたり意図しない既定経路になったりする。
- **対応:** 各 enum の有効値を4値として検証し、範囲外は既存の Alpha／Add／Stretch default へ戻すようにした。
- **未確認:** ビルド・テストは未実行。
# 2026-08-10: MatteNode の欠落 id は自動生成 ID を維持する

- **関連:** `ArtifactCore/include/Layer/LayerMatte.ixx` の `MatteNode` constructor / `fromJson()`。
- **確認できた事実:** constructor は自動 ID を作るが、JSON の `id` が欠落している場合も空文字へ上書きしていた。Artifact 側の `LayerMatteReference::fromJson()` は欠落時に既存 ID を保持する。
- **対応:** `id` が存在するときだけ復元値で上書きし、欠落時は constructor の自動生成 ID を保持するようにした。
- **未確認:** ビルド・テストは未実行。
# 2026-08-10: MatteNode::order は保存されるが評価順との契約が未確定

- **関連:** `ArtifactCore/include/Layer/LayerMatte.ixx` の `MatteNode::order()`、`MatteStack::toJson()`、`evaluateMatteStack()`。
- **確認できた事実:** `order_` は constructor、getter/setter、JSON serialization に存在するが、現行の stack evaluation と `sourceLayerIds()` は `nodes_` の配列順だけを使い、order 値を参照していない。
- **気づき（未検証）:** `order` が合成順の正規値なら、JSON 復元後や UI reorder 後に評価順が一致しない可能性がある。逆に配列順が正規なら、order は表示・移行用の補助値として文書化が必要。
- **対応:** 仕様根拠がないため今回は実装を変更せず、次に確認すべき契約として記録した。
# 2026-08-10: Render Queue の self-matte 重複診断を抑制

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` の `appendMatteReferenceDiagnostics()`。
- **確認できた事実:** self-reference は専用の Matte エラーとして報告された後、同じ layer を cycle DFS に通すことで CircularDep エラーも追加され、単一の不正参照が2件に重複していた。
- **対応:** cycle DFS の各ノードから self-edge だけを除外し、専用エラー1件に統一した。self-reference と別の複数 layer cycle が併存する場合も、後者の DFS は維持する。
- **未確認:** ビルド・テストは未実行。
# 2026-08-10: Render Queue の self-matte source を runtime map へ入れない

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` の software／GPU matte source 収集。
- **確認できた事実:** preflight は self-reference をエラーにするが、3つの source 収集経路は対象 layer 自身を source map に追加できた。その場合、破損 project JSON では self-matte が実際に適用され、preflight の error 契約と runtime fallback が一致しなかった。
- **対応:** source map への収集時に `matteRef.sourceLayerId == layer->id()` を除外し、既存の missing-source unmasked fallback へ揃えた。
- **未確認:** Composition View の malformed self-reference 実機挙動、ビルド・テストは未確認・未実行。
# 2026-08-10: Composition Render Controller の self-matte source 収集を抑制

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の CPU cache 用 matte source 収集と GPU 事前収集。
- **確認できた事実:** Render Queue 側は self-reference を source map へ入れない一方、Controller の2経路は対象 layer 自身の ID を resolver へ渡し得た。
- **対応:** self-reference を両経路で除外し、missing source 時の既存 unmasked fallback と揃えた。
- **未確認:** ビルド・テストは未確認・未実行。

# 2026-08-10: matte snapshot を Undo memory 見積もりへ反映

- **関連:** `Artifact/src/Undo/UndoManager.cppm` の Add／Remove layer command。
- **確認できた事実:** 依存 matte snapshot を追加した後も `estimatedMemoryBytes()` は command 本体と ID だけを数えていた。
- **対応:** snapshot 内の matte reference 数をメモリ見積もりに加えた。
- **未確認:** 実際の Undo manager eviction／offload 動作は未確認。

# 2026-08-10: Add／Remove matte snapshot の macro 順序を確認

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、Safe Delete／Paste／Quick Layer の `MacroUndoCommand` 利用箇所。
- **確認結果:** snapshot は command 作成時点の同一 composition 内依存だけを保持し、Undo 時に依存 layer が存在する場合だけ復元する。複数削除の逆順 Undo でも detached layer へ参照を戻す経路は無かった。
- **未確認:** 実行時 Undo／Redo は未確認・未実行。

# 2026-08-10: Layer matte JSON 欠落時の clear は partial apply と衝突

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `fromJsonProperties()`、各 layer factory。
- **確認できた事実:** `mattes` は key がある場合だけ置換される。`fromJsonProperties()` は新規 factory だけでなく部分プロパティ適用からも呼ばれるため、key 欠落時の無条件 clear は既存 matte を意図せず消す可能性がある。
- **対応:** 完全復元 caller は新規 layer を生成することを確認し、今回は clear 挙動を変更しない。
- **未確認:** 全 importer／partial property update の実行時網羅は未確認。

# 2026-08-10: App matte 診断で nil source を未接続として扱う

- **関連:** `Artifact/src/Diagnostics/AppValidationRules.cppm` の `ArtifactMatteReferenceRule`。
- **確認できた事実:** App 側は enabled かつ nil の matte reference を空 ID の missing source として報告していたが、Core 診断・描画側は nil を未接続として無視している。
- **対応:** App 診断でも nil source を skip し、診断と描画の active 条件を揃えた。
- **未確認:** 実 Project Health UI／診断実行は未確認・未実行。

# 2026-08-10: cycle 診断の source layer を実際の cycle 内へ修正

- **関連:** `Artifact/src/Diagnostics/AppValidationRules.cppm` の `ArtifactMatteReferenceRule`。
- **確認できた事実:** cycle 検出時の診断 `sourceLayerId` が探索開始 layer に設定され、cycle の外側にある layer を指す場合があった。
- **対応:** 検出された cycle の反復 ID (`cycleId`) を診断 source として設定するようにした。
- **未確認:** Project Health UI での選択／フォーカス動作は未確認。

# 2026-08-10: matte 削除／Undo 修正を Core milestone 要件と照合

- **関連:** `ArtifactCore/docs/MILESTONE_TRACK_MATTE_CORE_2026-03-26.md` の Phase 1／3／4、Composition 削除と `RemoveLayerCommand`。
- **確認結果:** dangling dependency の除去と Undo 復元は source missing／serialization／diagnostics の責務に整合し、matte の評価規則や UI 責務を変更していない。
- **未確認:** 実行時の Undo／Redo とプロジェクト再保存は未確認。

# 2026-08-10: AddLayer Undo でも依存 matte を復元

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm` の `AddLayerCommand`。
- **確認できた事実:** Add の Undo も低レベル `removeLayer()` を通るため、追加 layer を参照する既存 matte があれば削除時に失われ得た。
- **対応:** Remove と同じ依存 snapshot／Undo 復元を追加し、依存 snapshot 付き command は serialize 不可とした。
- **未確認:** ビルド・テストは未確認・未実行。

# 2026-08-10: RemoveLayer Undo で dangling matte 掃除を復元

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm` の `RemoveLayerCommand`。
- **確認できた事実:** Composition の低レベル削除で参照を掃除すると、Undo は削除対象 layer だけを戻し、他 layer の matte 参照を失ったままだった。
- **対応:** 削除前の依存 layer／matte 配列を保持し、Undo 時に復元するようにした。依存スナップショットを持つ command は未対応の永続化を避けるため serialize 不可とした。
- **未確認:** ビルド・テストは未確認・未実行。

# 2026-08-10: Core matte evaluator と実アプリ描画経路を区別

- **関連:** `ArtifactCore/include/Layer/LayerMatte.ixx` の `evaluateMatteStack()`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `evaluateLayerMatteReferences()`。
- **確認できた事実:** リポジトリ内の現行アプリ側呼び出しは後者で、Core の `evaluateMatteStack()` は定義のみだった。
- **価値:** matte 結果の修正時に、Core API を変えても Preview／Render Queue が変わらない経路を誤って対象にしないための境界情報になる。
- **未検証:** 将来の外部利用者や `Artifact_dev_review` 側からの利用有無は未確認。

# 2026-08-10: shared CPU matte の欠損 source fallback を GPU と照合

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `applyLayerMatteReferencesToSurfaceImpl()`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の GPU track-matte 適用。
- **確認できた事実:** CPU 側は active source を全件 preflight し、1件でも欠ければ元 surface を返す。GPU 側も missing source を検出した場合は unmasked layer に戻る。
- **確認結果:** 欠損 source の途中までを部分適用する CPU／GPU の分岐は現行経路では確認されなかった。
- **未確認:** 実 GPU／実フレームでの表示一致は未確認。

# 2026-08-10: GPU matte 適用関数でも self-reference を防御的に除外

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の GPU track-matte 適用関数。
- **確認できた事実:** 通常の source map 構築では self-reference を除外していたが、適用関数内の batch／fallback 収集条件には self 判定が無かった。
- **対応:** 適用関数自身でも target layer と同じ source ID を無視するようにした。
- **未確認:** ビルド・実 GPU 実行は未確認・未実行。

# 2026-08-10: Composition 低レベル削除でも dangling matte を掃除

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` の `Impl::removeLayer()`。
- **確認できた事実:** Project Service 経由の削除には matte 参照掃除があったが、Composition の直接 `removeLayer()`／`removeLayerById()` は parent link の解除だけで、他レイヤーから削除対象への matte 参照を残していた。
- **対応:** 共有の低レベル削除処理でも削除対象 ID を指す matte 参照を除去し、変更を通知するようにした。
- **未確認:** ビルド・テストは未確認・未実行。

# 2026-08-10: shared Composition View と Render Queue の self matte 境界を照合

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`、GPU source map 構築。
- **確認できた事実:** source map 構築、active matte 判定、shared surface 適用入口はいずれも enabled・non-nil・non-self を条件としていた。
- **確認結果:** Render Queue だけが self matte を収集する残存経路は確認されなかった。
- **未確認:** 実フレームでの Preview／Render Queue 画素一致は未確認。

# 2026-08-10: disabled matte をリンク切れ表示から除外

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の matte 行描画。
- **確認できた事実:** disabled な参照も `matteBroken` 検査に入り、無効化済みの nil／削除済み source で警告表示になっていた。
- **対応:** 設定済み参照のバッジは維持しつつ、リンク切れ判定は enabled な参照だけを検査するようにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: matte JSON の空 sourceLayerId と legacy assetId を正規化

- **関連:** `Artifact/include/Layer/ArtifactLayerMatte.ixx` の `LayerMatteReference::fromJson()`。
- **確認できた事実:** `sourceLayerId` キーが存在しても空文字なら、旧 `assetId` が同時に存在する移行途中 JSON を legacy fallback から遮っていた。
- **対応:** 空でない `sourceLayerId` を優先し、空なら `assetId` を fallback、両方なければ明示的に nil へ戻すようにした。既存インスタンス再利用時の stale state も残さない。

# 2026-08-10: Composition Controller の matte source 収集ゲートを active 判定へ統一

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `applySurfaceAndDraw`。
- **確認できた事実:** source 収集ループ自体は disabled・nil・self を除外していたが、入口だけ raw な `matteReferences().size() > 0` 判定だった。
- **対応:** 既存の `layerHasEnabledMatteReferences()` を入口にも使い、実際に収集対象となる matte がある場合だけ resolver を呼ぶようにした。
- **未確認:** ビルド・テストは未確認・未実行。

# 2026-08-10: matte 状態サマリーを active 参照へ統一

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelPresentation.cppm`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- **確認できた事実:** 状態バッジと概要文が、disabled・nil・self のみの matte でも `Matted`／`Matte Linked` と表示していた。
- **対応:** 描画経路と同じ enabled・non-nil・non-self 条件で状態サマリーを判定するようにした。設定自体を編集する matte 行の表示は維持した。
- **未確認:** ビルド・テストは未確認・未実行。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Visual Density の matte 複雑度を active 参照に限定

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の visual density overlay / visual score。
- **確認できた事実:** density weight と visual score が matte 配列の件数をそのまま複雑度へ加算していたため、disabled/nil/self 参照も画面複雑度を水増ししていた。
- **対応:** ヒューリスティックには enabled・非 nil・self 以外の matte 件数だけを使うようにした。診断用の HUD 件数表示は変更していない。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Composition Editor の選択依存表示を active matte に限定

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の selected layer delete safety dialog。
- **確認できた事実:** 外部依存の説明が matte の `enabled` / nil を確認せず、disabled または空 source の参照でも「選択レイヤーを使う」と表示し得た。
- **対応:** enabled かつ非 nil の matte source だけを依存表示の対象にし、`LayerID` の文字列往復も除去した。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Export Session の無効 matte による不要な pre-render を抑制

- **関連:** `Artifact/src/Export/ArtifactExportSession.cppm` の `ArtifactExportSession::build()`。
- **確認できた事実:** `requiresPreRender` と `preRenderReason` が matte 配列の非空だけを見ており、disabled/nil/self 参照でも export pre-render を要求していた。
- **対応:** enabled・非 nil・self 以外の参照だけを active matte として判定し、欠落した別 layer ID は従来どおり active 扱いにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Inspector の matte source 操作にも cycle guard を追加

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` の `setMatteSourceToLayer()` / `addMatteSourceToLayer()`。
- **確認できた事実:** Inspector は self-reference は拒否していたが、source layer から既存 matte chain を辿って対象へ戻る cycle は検査せず、Undo command を生成できた。
- **対応:** Timeline の drag 操作と同じ全 enabled matte edge の到達検査を共通ローカル helper として追加し、source 設定・追加の両経路で拒否するようにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: 実アプリ診断経路にも matte 全参照 DFS を反映

- **関連:** `Artifact/src/Diagnostics/AppValidationRules.cppm` の `ArtifactMatteReferenceRule`、`Artifact/src/Service/ArtifactProjectService.cppm` の診断登録。
- **確認できた事実:** 実際のアプリ／Project Health 経路は Core の `MatteReferenceValidationRule` ではなく Artifact 側の同名ルールを登録していた。Artifact 側も最初の enabled matte source だけを cycle 検査していた。
- **対応:** Artifact 側も全 enabled matte edge を DFS で辿り、self edge は既存の専用診断に任せ、canonical cycle key で重複を抑制するようにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Render Queue の同一 matte cycle 重複診断を抑制

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` の matte preflight DFS。
- **確認できた事実:** A→B→A のような cycle は検出できていたが、A 起点では cycle node A、B 起点では cycle node B となり、`reportedCycleNodes` だけでは同じ cycle を2件報告し得た。
- **対応:** DFS の cycle 部分を node ID の canonical key にして、同一 cycle を開始レイヤーによらず1件へ抑制した。self edge の除外は維持した。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: buildMatteStack の per-reference 情報保持は未検証

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `buildMatteStack()`、`Artifact/include/Layer/ArtifactLayerMatte.ixx` の `toCoreMatteNode()`、`ArtifactCore/include/Layer/LayerMatte.ixx`。
- **確認できた事実:** `LayerMatteReference` の `opacity` と `blendMode` は Core `MatteNode` へ変換されず、Core `MatteStack` は stack 全体の `MatteStackMode` のみを持つ。現時点で `buildMatteStack()` の呼び出し元は検索上見つからない。
- **気づき（未検証）:** 将来この convenience API を render 経路へ接続すると、現行の per-reference matte semantics が失われる可能性がある。
- **対応:** Core 型の拡張や変換仕様の変更は、現在の描画経路に直接影響し得るため今回は実施せず、接続前に契約を確定する課題として記録した。
# 2026-08-10: Core matte cycle diagnostics が最初の参照だけを辿る問題を修正

- **関連:** `ArtifactCore/src/Diagnostics/ValidationRules.cppm` の `MatteReferenceValidationRule::validate()`。
- **確認できた事実:** 旧実装は各 layer から最初の enabled matte source だけを選んで chain を辿っており、2本目以降の matte edge による cycle を検出できなかった。
- **対応:** 全 enabled / 非 nil / 存在する matte edge を DFS で辿り、self edge は専用 Matte 診断に任せ、同一 cycle の重複診断を抑制するようにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Render Layer Widget の self-matte 集計を抑制

- **関連:** `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` の surface inspect 表示と impact 依存集計。
- **確認できた事実:** self-reference は描画 source から除外されている一方、UI の Matte 件数と matte input 影響 ID には残り得た。
- **対応:** self-reference を有効 matte 件数・影響依存の集計から除外した。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: self-matte を高コスト経路の判定からも除外

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の active matte 判定。
- **確認できた事実:** self-reference は source map から除外済みでも、matte が存在するだけで rasterized surface、cache bypass、GPU fast-path 除外の判定を通っていた。
- **対応:** 対象 layer 自身を参照する matte は active matte 判定でも除外し、無効な self-reference が余計な高コスト経路を選ばないようにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Inspector の無効 matte を警告状態から除外

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` の `matteReferenceSummary()`。
- **確認できた事実:** 無効化された参照も missing / nil / self の `hasInvalid` 判定に入り、実際には描画へ影響しない設定が Inspector の警告色・太字表示を誘発し得た。
- **対応:** 参照内容の表示は維持しつつ、`hasInvalid` は enabled な参照だけで更新するようにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: 不正な matte ID で自動生成 ID を壊さない

- **関連:** `Artifact/include/Layer/ArtifactLayerMatte.ixx` の `LayerMatteReference::fromJson()`。
- **確認できた事実:** `LayerMatteReference` は構築時にランダム ID を生成するが、JSON の空値・不正値は `Id` のパース結果が nil となり、操作用 ID を上書きしていた。
- **対応:** 非 nil と確認できた ID だけを復元し、欠落・空値・不正値では構築時の自動 ID を保持するようにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Undo matte JSON の非オブジェクト要素を無視

- **関連:** `Artifact/src/Undo/UndoManager.cppm` の `ChangeLayerMatteReferencesCommand::deserialize()`。
- **確認できた事実:** 通常の layer 復元は `mattes` 配列内の object だけを追加する一方、Undo 復元は配列要素を無条件に `toObject()` して空の参照を追加していた。
- **対応:** Undo 復元も非オブジェクト要素をスキップし、通常保存経路と同じ入力境界に揃えた。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Undo matte JSON の必須配列を検証

- **関連:** `Artifact/src/Undo/UndoManager.cppm` の `ChangeLayerMatteReferencesCommand::deserialize()`。
- **確認できた事実:** `before` / `after` が欠落または非配列でも `QJsonValue::toArray()` が空配列を返し、復元コマンドが有効な layer に空 matte を適用し得た。
- **対応:** 保存形式で必須の両フィールドが配列でない場合は deserialize を失敗させるようにした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: 実適用 matte 経路の self 参照を除外

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **確認できた事実:** Controller の実際に呼ばれる QImage / F32 適用関数と共有 CPU 適用の呼び出し側は、self 参照を source 解決対象に含めていた。self の source map 不在を missing と同じ扱いにするため、他の有効 matte と併存すると matte 全体を未適用へ戻し得た。
- **対応:** 各入口で self 参照を除外し、self 単独は無変更、他の参照は継続評価するようにした。
- **未確認:** ビルド・テストは未確認・未実行。大規模な Render Controller の `diff --check` には既存の trailing whitespace 警告が残る。
# 2026-08-10: Render Controller の matte 件数を実効値へ統一

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の visual density HUD と `FrameDebugSnapshot`。
- **確認できた事実:** Timeline / Render Layer Widget は enabled・非 nil・非 self の matte だけを数える一方、Controller の HUD と debug snapshot は `matteReferences().size()` で設定行数を数えていた。
- **対応:** 既存の描画判定と同じ active 条件を共有する件数ヘルパーを追加し、HUD・snapshot の件数と密度評価を実効状態へ揃えた。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: matte 補助表示の self 参照を除外

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の選択レイヤー補助表示。
- **確認できた事実:** 実描画・件数集計では self matte を無効扱いにしている一方、選択レイヤーの matte source outline は self の bounds も表示対象にしていた。
- **対応:** 補助表示でも self 参照を除外し、実効 matte と同じ表示境界に揃えた。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Render Queue preflight の nil matte 診断を抑制

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` の matte preflight。
- **確認できた事実:** App／Core の matte 診断は enabled でも nil source を未設定として無視する一方、Render Queue preflight は nil source を missing source として報告していた。
- **対応:** Render Queue も enabled・非 nil の参照だけを missing／hidden／cycle 診断の対象にした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Smart Solo の self matte 依存を除外

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm` の `collectSmartSoloLayerIds()`。
- **確認できた事実:** self matte は visited ガードで再帰停止していたが、依存 edge として一度 source lookup・再帰呼び出しに入っていた。
- **対応:** 現在の layer 自身を参照する matte は依存収集対象から明示的に除外し、実効 matte 判定と同じ境界にした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Importer 監査の matte 復元指摘は現行コードでは stale

- **関連:** `docs/CORE_MODULE_QUALITY_AUDIT_2026-08-06.md`、`Artifact/src/Project/ArtifactProjectImporter.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **確認できた事実:** 監査文書は Importer が layer ID・parent・track matte を復元しないと記載しているが、現行 Importer は `ArtifactAbstractComposition::fromJson()` を呼び、canonical factory と `fromJsonProperties()`、parent 解決 pass を通している。layer JSON の `mattes` もこの経路で読み込まれる。
- **対応:** 現在の実装と矛盾する監査文書自体は今回の scope を広げて更新せず、runtime での保存→再読込確認が未実施であることだけを記録した。
- **未確認:** ビルド・テスト・実ファイルの保存再読込は未確認・未実行。
# 2026-08-10: Composition 復元時の layer ID を保持

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **確認できた事実:** layer JSON は `id` を保存しているが、factory 復元後に新規 ID のままだった。そのため parent・matte・clone などの layer ID 参照が保存直後の再読込で解決できなかった。
- **対応:** factory の JSON 復元経路で有効な保存 ID を復元し、Composition 内で重複した場合だけ新規 ID に退避する setter を追加した。
- **未確認:** ビルド・テスト・実ファイルの保存再読込は未確認・未実行。
# 2026-08-10: 不正 layer 要素による parent 復元の添字ずれを修正

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` の `fromJson()` layer 復元。
- **確認できた事実:** layer factory は object 要素と生成成功 layer だけを `loadedLayers` に追加する一方、parent pass は元 JSON 配列の添字を参照していた。不正要素や factory 失敗が前にあると、別 layer の `parentId` を適用し得た。
- **対応:** 生成 layer と対応する JSON object を並行保持し、parent pass は対応ペアを使うようにした。
- **未確認:** ビルド・テスト・実ファイルの保存再読込は未確認・未実行。
# 2026-08-10: クリップ貼り付け時の layer ID 再利用を防止

- **関連:** `Artifact/src/AppMain.cppm` の clip paste 経路。
- **確認できた事実:** 貼り付け時に layer JSON を factory へそのまま渡しており、保存済みの自身の `id` を再利用して同一 Composition 内で ID が衝突し得た。
- **対応:** 貼り付け前に自身の `id` だけを除去し、source layer ID など外部参照は保持したまま新規 layer ID を生成するようにした。
- **未確認:** ビルド・テスト・実 UI 貼り付けは未確認・未実行。
# 2026-08-10: Clipboard / Project Bundle の layer 復元 factory 経路を修正

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Application/ArtifactProjectBundleIpc.cppm`。
- **確認できた事実:** 両貼り付け経路が abstract layer の基底 `fromJson()` を直接呼んでいたが、その実装は nullptr を返すため、JSON layer を生成できなかった。
- **対応:** `ArtifactLayerFactory::createFromJson()` を使用し、貼り付け時は自身の `id` を除去して新規 ID を生成するようにした。
- **未確認:** ビルド・テスト・実 UI 貼り付けは未確認・未実行。
# 2026-08-10: 複数 layer 貼り付けの内部 ID 参照を remap

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Application/ArtifactProjectBundleIpc.cppm`。
- **確認できた事実:** 貼り付け各 layer の自身の ID は新規化しても、選択集合内の parent / matte source は旧 ID のまま残り、新規 layer 間の参照が切れる可能性があった。
- **対応:** 追加前に旧→新 ID を収集し、全 layer 追加後に集合内の parent / matte source だけを新 ID へ置換した。集合外の参照は変更していない。
- **未確認:** ビルド・テスト・複数 layer の実 UI 貼り付けは未確認・未実行。
# 2026-08-10: 複数 layer 貼り付けの clone source も remap

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Application/ArtifactProjectBundleIpc.cppm`、`Artifact/src/Layer/ArtifactCloneLayer.cppm`。
- **確認できた事実:** clone layer は `clone.sourceLayerId` という別の layer ID 参照を持ち、parent / matte だけの remap ではコピー集合内の clone source が旧 layer を指し続けた。
- **対応:** clone settings の source ID も集合内の旧→新マップで置換し、集合外の source は維持した。
- **未確認:** ビルド・テスト・複数 layer の実 UI 貼り付けは未確認・未実行。
# 2026-08-10: Edit Menu の layer 貼り付けを factory 経路へ統一

- **関連:** `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **確認できた事実:** Edit Menu の貼り付けも abstract layer の基底 `fromJson()` を直接呼んでおり、JSON layer を生成できなかった。
- **対応:** `ArtifactLayerFactory::createFromJson()` を使い、貼り付け時の自身の ID は除去して新規 ID を生成するようにした。
- **未検証:** Material Container / Group の legacy embedded child は factory module との循環を避けるため未変更。軽量 deserializer 分離が必要。
# 2026-08-10: 復元用 layer ID setter の接続後変更を防止

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `setId()`、`ArtifactCore/include/Container/MultiIndexContainer.ixx`。
- **確認できた事実:** Composition は layer の ID を `byId_` 索引にも保持するため、接続後に layer 自身の ID だけ変更すると検索索引と実体が不一致になる。
- **対応:** ID 復元 setter は未接続 layer かつ non-nil ID の場合だけ有効にした。
- **未確認:** ビルド・テストは未確認・未実行。
# 2026-08-10: Material Container の embedded layer 復元は production 経路

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm`、`Artifact/src/Layer/ArtifactLayerFactory.cppm`、`Artifact/src/Layer/ArtifactGroupLayer.cppm`。
- **確認できた事実:** Group の `children` は detached legacy payload に限定される一方、Material Container の `materialContainer.slots` は通常の `fromJsonProperties()` で常に読み込まれる。slot layer は基底 `ArtifactAbstractLayer::fromJson()` を呼ぶため、現状は nullptr になり得る。
- **対応:** factory が Material Container を import する一方で Material Container から factory を import すると module 循環になるため、今回の散歩では推測実装を避けた。共通 callback または依存を分離した軽量 layer deserializer が必要。
- **未確認:** ビルド・テスト・Material Container の実データ復元は未確認・未実行。
# 2026-08-10: embedded layer 復元の module 循環を callback 境界で解消

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Layer/ArtifactLayerFactory.cppm`、`Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm`。
- **確認できた事実:** Material Container / legacy Group は nested JSON layer を基底 `fromJson()` へ渡していたが、factory module を直接 import すると Factory→MaterialContainer / Group→Factory の循環になる。
- **対応:** Abstract に thread-safe な deserializer callback 登録点を置き、Factory の呼び出し時に自身を登録した。nested `fromJson()` は登録済み factory へ委譲し、既存 module import の循環を追加しない。
- **未確認:** ビルド・テスト・Material Container / Group の実データ復元は未確認。
# 2026-08-10: Factory 通常生成後の直接 JSON 復元でも callback を登録

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm` の `ArtifactLayerFactory` constructor、`Artifact/src/Test/ArtifactTestAdjustmentLayer.cppm`。
- **確認できた事実:** 既存テストは factory の通常 `createLayer()` 後に基底 `fromJson()` を呼ぶため、JSON factory 関数を直接通らず callback が未登録のままになる経路があった。
- **対応:** Factory constructor でも共通 deserializer を登録し、通常生成→直接復元の順でも callback が利用可能になるようにした。
- **未確認:** ビルド・テストは未確認。
# 2026-08-10: Material Container slot 削除時の stale composition を除去

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm` の `removeMaterialAt()` / `clearMaterials()`。
- **確認できた事実:** slot layer は親 Composition へ接続される一方、slot 削除・JSON 再読込時に pointer を nullptr へ戻さず vector から破棄していた。旧 nested layer が stale な Composition pointer を保持し得た。
- **対応:** slot を erase / clear する前に nested layer を明示的に detach するようにした。
- **未確認:** ビルド・テスト・Material Container の実 UI 編集／再読込は未確認。
# 2026-08-10: Switch Layer child 削除時の stale composition を除去

- **関連:** `Artifact/src/Layer/ArtifactSwitchLayer.cppm` の `removeChildLayer()`。
- **確認できた事実:** Switch child は `addChildLayer()` で親 Composition に接続される一方、削除時は vector から消すだけで Composition pointer を保持し得た。
- **対応:** vector erase 前に child を明示 detach するようにした。
- **未確認:** ビルド・テスト・Switch Layer の実 UI 編集は未確認。
# 2026-08-10: Switch Layer の QObject composition attach 漏れを修正

- **関連:** `Artifact/include/Layer/ArtifactSwitchLayer.ixx`、`Artifact/src/Layer/ArtifactSwitchLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **確認できた事実:** Composition の `owner_` は QObject 派生ポインタで、通常 layer 追加は QObject overload を呼ぶ。Switch Layer は void* overload だけを override していたため、`impl_->composition_` と child attach が更新されなかった。
- **対応:** QObject overload を追加し、既存の void* 実装へ委譲した。
- **未確認:** ビルド・テスト・Switch Layer の実 UI attach / render は未確認。
# 2026-08-10: Paint Layer の QObject composition attach 漏れを修正

- **関連:** `Artifact/include/Layer/ArtifactPaintLayer.ixx`、`Artifact/src/Layer/ArtifactPaintLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **確認できた事実:** Paint Layer は void* overload だけを override し、Composition settings から default size を更新していた。通常の QObject owner attach ではこの更新が呼ばれなかった。
- **対応:** QObject overload を追加し、既存 void* 実装へ委譲した。
- **未確認:** ビルド・テスト・Paint Layer の実 UI attach / 描画は未確認。
# 2026-08-10: composition overload 修正の実装 include を補完

- **関連:** `Artifact/src/Layer/ArtifactPaintLayer.cppm`、`Artifact/src/Layer/ArtifactSwitchLayer.cppm`、`Artifact/src/Layer/ArtifactGroupLayer.cppm`。
- **確認できた事実:** QObject overload の実装ファイルが module interface 経由の QObject 宣言に依存していた。
- **対応:** 各実装ファイルの global module fragment に `QObject` を直接 include した。
- **未確認:** ビルド・テストは未確認。
# 2026-08-10: Adjustable Layer の QObject composition attach 漏れを修正

- **関連:** `Artifact/include/Layer/ArtifactAdjustableLayer.ixx`、`Artifact/src/Layer/ArtifactAdjustableLayer.cppm`。
- **確認できた事実:** Adjustable Layer は Composition size から source size を更新する処理を void* overload にだけ実装していた。
- **対応:** QObject overload を追加し、通常の Composition attach でも source size 更新へ到達するようにした。
- **未確認:** ビルド・テスト・Adjustable Layer の実 UI attach / 描画は未確認。
# 2026-08-10: Adjustable Layer の未使用未初期化 PImpl を除去

- **関連:** `Artifact/include/Layer/ArtifactAdjustableLayer.ixx`、`Artifact/src/Layer/ArtifactAdjustableLayer.cppm`。
- **確認できた事実:** header に `Impl*` が宣言されていたが Impl 定義・初期化・利用・解放がなく、constructor 後に未初期化 pointer が残っていた。
- **対応:** 未使用の PImpl 宣言を削除し、destructor を defaulted definition にした。
- **未確認:** ビルド・テストは未確認。
# 2026-08-10: AppMain clip paste の内部 ID remap を統一

- **関連:** `Artifact/src/AppMain.cppm` の `clipPasteRequested`。
- **確認できた事実:** AppMain の貼り付けは自身の ID だけ除去していたが、複数 layer payload の parent / matte / clone source は旧 ID のままだった。
- **対応:** 追加成功 layer の旧→新 ID を収集し、全追加後に内部参照だけを remap した。append 失敗 layer は選択・マップ対象から除外した。
- **未確認:** ビルド・テスト・実 UI 貼り付けは未確認・未実行。
# 2026-08-10: Edit Menu paste の内部 ID remap を統一

- **関連:** `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`。
- **確認できた事実:** Edit Menu は factory と新規自身 ID までは揃っていたが、複数 layer の parent / matte / clone source は旧 ID のままだった。
- **対応:** 追加成功 layer の旧→新 ID を収集し、追加後に内部参照を remap した。
- **未確認:** ビルド・テスト・実 UI 貼り付けは未確認・未実行。
# 2026-08-10: Paste Undo command の snapshot 時点を修正

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の paste transaction。
- **確認できた事実:** pasted layer を Composition から除去した後に `AddLayerCommand` を生成していたため、親 layer の除去で child parent が先に解除され、Undo snapshot に元 parent が入らなかった。
- **対応:** 各 `AddLayerCommand` を layer 除去前に生成し、parent / matte 依存 snapshot を取得してから一時除去・transaction 追加する順序へ変更した。
- **未確認:** ビルド・テスト・実 UI Undo/Redo は未確認・未実行。
# 2026-08-10: Paste Undo redo の detached child 復元を補完

- **関連:** `Artifact/src/Undo/UndoManager.cppm` の `AddLayerCommand::redo()`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の paste transaction。
- **確認できた事実:** Macro redo は親 layer の Add command 実行時点で child をまだ Composition に戻していない。従来の `containsLayerById()` 条件だけでは parent / matte snapshot が child に戻らず、後続 child 追加後も関係が欠落した。
- **対応:** detached child へ parent / matte snapshot を先行適用し、再追加時に保持されるようにした。別 Composition に接続中の layer は従来どおり除外する。
- **未確認:** ビルド・テスト・実 UI Undo/Redo は未確認・未実行。
# 2026-08-10: Add / Remove Layer Undo の parent 依存を復元

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **確認できた事実:** Composition の layer 削除は対象を親にする child の parent を自動解除するが、Add/Remove Undo は matte 依存しか snapshot していなかった。
- **対応:** 親を削除・再追加する Undo/Redo で child の parent ID も保存し、layer 再追加後に復元するようにした。非シリアライズ可能条件とメモリ見積もりも更新した。
- **未確認:** ビルド・テスト・実 UI Undo/Redo は未確認・未実行。

# 2026-08-10: Layer Factory の旧 layerType 早期 return を修正

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm` の JSON 生成経路。
- **確認できた事実:** `MaterialContainer` / `FormParticle` / `Procedural3D` は `layerType` や専用キーを後段で判定していたが、`type` がない旧 JSON はその前のガードで無条件に破棄されていた。
- **対応:** 認識済みの旧形式だけ早期 return を通過させ、未知形式は従来どおり拒否するようにした。
- **未検証:** ビルド・テスト・旧形式 JSON の実読込は未確認・未実行。

# 2026-08-10: Material Container の layerType / payload 復元を補完

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm`、`Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm`、`docs/planned/MILESTONE_MATERIAL_CONTAINER_LAYER_2026-06-25.md`。
- **確認できた事実:** Phase 1 の計画 JSON は slot を `layerType` + `payload` で表現するが、実装は `layer` オブジェクト形式だけを復元していた。Factory も Image / Shape / Text 等の文字列 layerType を型へ変換していなかった。
- **対応:** 既存の `layer` 形式を維持しつつ、payload に layerType を補って callback factory へ渡す経路を追加し、主要な旧 layerType の型マッピングを追加した。
- **追補:** 計画形式の `layerName` と slot の `id` も、現行の `name` / `slotId` と併用できるようにした。
- **未検証:** ビルド・テスト・Material Container の実データ再読込は未確認・未実行。

# 2026-08-10: Material Container 初回 slot 追加時の exposedIndex を修正

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm` の `insertMaterialAt()`。
- **確認できた事実:** 空 container の `exposedIndex` は 0 だが、index 0 への最初の追加でも既存 slot 挿入と同じシフト処理が走り、index 1 へずれていた。
- **対応:** 既存 slot がある場合だけ exposed index をシフトするようにした。
- **未検証:** ビルド・テスト・Material Container の実 UI 追加／表示は未確認・未実行。

# 2026-08-10: Material Container slot 削除時の exposedIndex を追従

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm` の `removeMaterialAt()`。
- **確認できた事実:** 表示中 slot より前の slot を削除しても exposed index を減算しておらず、同じ slot を表示し続けられなかった。
- **対応:** 削除位置より後ろの exposed index を 1 つ戻し、末尾削除時の clamp も維持した。
- **未検証:** ビルド・テスト・Material Container の実 UI 削除／表示は未確認・未実行。

# 2026-08-10: Switch Layer child 削除時の activeIndex を追従

- **関連:** `Artifact/src/Layer/ArtifactSwitchLayer.cppm` の `removeChildLayer()`。
- **確認できた事実:** active child より前の child を削除しても active index を減算しておらず、意図した child から選択がずれた。
- **対応:** 削除位置より後ろの active index を 1 つ戻し、active child／末尾削除時の既存 clamp は維持した。
- **未検証:** ビルド・テスト・Switch Layer の実 UI 削除／表示は未確認・未実行。

# 2026-08-10: Switch Layer child の JSON 保存・復元を実体化

- **関連:** `Artifact/src/Layer/ArtifactSwitchLayer.cppm` の `toJson()` / `fromJson()`。
- **確認できた事実:** child の ID と名前だけを保存し、復元時に child JSON を生成していなかったため、Switch Layer の再読込で child 実体が失われていた。
- **対応:** child の完全 JSON を保存し、登録済み layer factory callback で復元するようにした。復元後に active index と timeline frame 数も child 数へ整列する。
- **未検証:** ビルド・テスト・Switch Layer の実保存／再読込は未確認・未実行。

# 2026-08-10: Switch Layer を通常の Factory 保存復元経路へ接続

- **関連:** `Artifact/include/Layer/ArtifactSwitchLayer.ixx`、`Artifact/src/Layer/ArtifactSwitchLayer.cppm`、`Artifact/src/Layer/ArtifactLayerFactory.cppm`。
- **確認できた事実:** Switch は Factory case と legacy type mapping がなく、`toJson()` に基底 metadata がなく、custom `fromJson()` は Factory の `fromJsonProperties()` から呼ばれていなかった。
- **対応:** Switch の生成 case、`type` / `layerType`、基底 JSON、`fromJsonProperties()` override を追加し、通常の JSON layer 復元経路へ接続した。
- **未検証:** ビルド・テスト・Switch Layer の実保存／再読込は未確認・未実行。

# 2026-08-10: Video Layer の共通 JSON metadata 欠落を補完

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm` の `toJson()`。
- **確認できた事実:** Video の独自 JSON は `type` や source 情報を保存していたが、基底 `ArtifactAbstractLayer::toJson()` を呼ばず、ID・親・matte・共通プロパティを保存していなかった。
- **対応:** 既存の Video 固有キーを維持しつつ、基底 JSON を初期値として合成するようにした。デコード処理は変更していない。
- **未検証:** ビルド・テスト・Video の実保存／再読込は未確認・未実行。

# 2026-08-10: Particle Layer の Factory 復元漏れを補完

- **関連:** `Artifact/include/Layer/ArtifactParticleLayer.ixx`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`。
- **確認できた事実:** Particle は専用 `fromJson()` では render settings／emitters を復元するが、Factory の通常経路が呼ぶ `fromJsonProperties()` override を持っていなかった。
- **対応:** 既存の `applyPropertiesFromJson()` を再利用する `fromJsonProperties()` override を追加した。
- **未検証:** ビルド・テスト・Particle の実保存／再読込は未確認・未実行。

# 2026-08-10: Shape Layer の Factory 復元漏れを補完

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- **確認できた事実:** Shape は専用 `fromJson()` で shape style／custom path／operators を復元するが、Factory の generic 経路は基底 property 復元だけで専用処理を呼んでいなかった。
- **対応:** SVG source の既存分岐を維持しつつ、通常 Shape は専用 `fromJson()` へ接続した。
- **未検証:** ビルド・テスト・Shape の実保存／再読込は未確認・未実行。

# 2026-08-10: Parametric Composition Layer の instance 復元を補完

- **関連:** `Artifact/include/Layer/ArtifactParametricCompositionLayer.ixx`、`Artifact/src/Layer/ArtifactParametricCompositionLayer.cppm`。
- **確認できた事実:** `parametric.instance` に保存された input bindings／parameter overrides／data row を、Factory の generic 復元経路が読み戻していなかった。
- **対応:** `ParametricCompositionInstance::fromJson()` を使う `fromJsonProperties()` override を追加した。definition の外部解決責務は変更していない。
- **未検証:** ビルド・テスト・Parametric Composition の実保存／再読込は未確認・未実行。

# 2026-08-10: Material Container slot child の親階層を切断

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm` の JSON 復元。
- **確認できた事実:** slot child の JSON に残った `parentId` を復元後、そのまま Composition に接続していた。slot は通常 hierarchy の child ではないため、外部 parent へ誤接続し得た。
- **対応:** slot child を Composition へ接続する際に `clearParent()` を適用し、通常の `insertMaterialAt()` と同じ責務境界に揃えた。
- **未検証:** ビルド・テスト・Material Container の実保存／再読込は未確認・未実行。

# 2026-08-10: Material Container slot detach 時の parent 残留を除去

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm` の `removeMaterialAt()` / `clearMaterials()`。
- **確認できた事実:** slot child の削除・全消去では Composition pointer だけを detach しており、過去に設定された parent が残る可能性があった。
- **対応:** Composition detach 前に `clearParent()` を行い、slot が常に非階層 child として破棄されるようにした。
- **未検証:** ビルド・テスト・Material Container の実 UI 削除／再読込は未確認・未実行。

# 2026-08-10: Switch child の parent 残留を除去

- **関連:** `Artifact/src/Layer/ArtifactSwitchLayer.cppm` の `addChildLayer()` / `removeChildLayer()`。
- **確認できた事実:** Switch child は通常 hierarchy 外だが、追加時に JSON 由来の parent を消さず、削除時にも parent を解除していなかった。
- **対応:** child の追加・削除時に `clearParent()` を行い、Material Container と同じ非階層 child の責務境界に揃えた。
- **未検証:** ビルド・テスト・Switch Layer の実 UI 追加／削除／再読込は未確認・未実行。

# 2026-08-10: nested non-hierarchical child attach 時の parent を正規化

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm`、`Artifact/src/Layer/ArtifactSwitchLayer.cppm` の `setComposition()`。
- **確認できた事実:** attach 時に保持 child の Composition pointer は更新していたが、stale parent を同時に除去していなかった。
- **対応:** container / switch の attach 処理でも `clearParent()` を先に行い、非階層 child の責務を lifecycle 全体で維持した。
- **未検証:** ビルド・テスト・nested layer の実 UI attach / detach は未確認・未実行。

# 2026-08-10: nested child paste 時の ID 衝突を回避

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm`、`Artifact/src/Layer/ArtifactSwitchLayer.cppm`。
- **確認できた事実:** paste 経路は外側 Layer の `id` を除去して新規化するが、nested child JSON の ID は保持したままだった。複数回貼り付けで内部 child ID が衝突し得た。
- **対応:** canonical 保存で外側 `id` がある場合は nested ID を維持し、外側 `id` がない detached payload の復元時だけ nested child ID を再生成する。
- **未検証:** ビルド・テスト・nested Switch / Material Container の実貼り付けは未確認・未実行。

# 2026-08-10: Material Container disabled slot の描画を抑止

- **関連:** `Artifact/src/Layer/ArtifactMaterialContainerLayer.cppm` の `exposedLayer()`。
- **確認できた事実:** slot の `enabled` は JSON 保存・復元されていたが、露出 layer の解決で無視され、disabled slot も通常描画されていた。
- **対応:** 露出 index の slot が disabled または無効なら空 layer を返し、既存の exposed slot 単一評価を維持した。
- **未検証:** ビルド・テスト・disabled slot の実 UI 表示は未確認・未実行。

# 2026-08-10: 旧 layerName の共通復元 fallback を追加

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `fromJsonProperties()`。
- **確認できた事実:** 専用 static `fromJson()` が直接基底復元を呼ぶ Layer では、Factory 側の `layerName` fallback が届かず、旧 JSON の表示名が失われていた。
- **対応:** 共通 `name` を優先し、未存在時だけ `layerName` を表示名へ復元するようにした。
- **未検証:** ビルド・テスト・旧 JSON の実読込は未確認・未実行。

# 2026-08-10: Composition 重複 Layer ID の再生成を衝突検査付きに補強

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` の JSON layer 復元。
- **確認できた事実:** serialized ID が既存 layer と重複した場合、再生成を1回だけ行っていた。
- **対応:** 新 ID が既存索引と衝突しなくなるまで再生成する loop に変更し、by-ID 索引の一意性を明示的に守った。
- **未検証:** ビルド・テスト・重複 ID JSON の実読込は未確認・未実行。
# 2026-08-10: Video Factory 復元の sourcePath 依存を解消

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm`, `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- **確認できた事実:** `ArtifactVideoLayer::fromJson()` は source path の有無に関係なく、再生速度、ループ、音量、Crop、Proxy、トラッキング設定などを復元する。一方、Factory の専用経路は `video.sourcePath` または `sourcePath` が存在する場合に限定されていた。
- **変更:** `LayerType::Video` および旧 `VideoLayer` JSON は常に `ArtifactVideoLayer::fromJson()` を使うようにした。
- **価値:** source path が未設定・空の動画レイヤーでも、保存済みの動画固有設定が共通プロパティ復元で失われない。
- **次に確認:** 実ファイル読み込みを伴わない動画 JSON の保存・復元ケースで、設定値が保持されることを確認する（ビルド・テスト未実行）。

# 2026-08-10: Factory の SVG 判定を Shape 限定に補正

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm` の SVG 復元分岐。
- **確認できた事実:** 分岐条件が `sourcePath` の存在だけだったため、Image など他の source 系 JSON が SVG 初期化へ入る可能性があった。
- **変更:** `svg.sourcePath` が明示された場合、または `LayerType::Shape` の旧 `sourcePath` の場合だけ SVG 経路を使うようにした。
- **価値:** Image / Video / 3D などの source path を SVG と誤判定せず、各レイヤーの Factory 経路へ到達できる。
- **次に確認:** 各 source 系 layer type の最小 JSON を使った Factory 分岐確認（ビルド・テスト未実行）。

# 2026-08-10: Factory の文字列 type 互換を補強

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm` の JSON 型判定。
- **確認できた事実:** `type` が数値ではなく旧形式の文字列だった場合、`QJsonValue::toInt()` により `LayerType::Unknown` 相当へ落ち、`layerType` にしか存在しない互換変換を通らなかった。
- **変更:** Factory が扱う全 enum 系（Null / Solid / Image / Adjustment / Text / Shape / Precomp / Audio / Video / Camera / Light / Group / Particle / Clone / SDF / Model3D / Construction / CompositionBackground / MaterialContainer / FormParticle / Procedural3D / SandSim2D / ParametricComposition / EnvironmentMap / Switch / Paint）の文字列名と Layer 接尾辞を認識するようにした。
- **価値:** 数値 enum を使わない旧 JSON でも、専用 Factory と各レイヤーの復元処理へ到達できる。
- **次に確認:** 文字列 `type` を持つ旧 JSON の各代表型を用いた復元確認（ビルド・テスト未実行）。

# 2026-08-10: SVG 復元メソッドの override 契約を明示

- **関連:** `Artifact/include/Layer/ArtifactSvgLayer.ixx`。
- **確認できた事実:** `ArtifactSvgLayer::fromJsonProperties()` は基底の virtual メソッドを実装していたが、宣言に `override` がなく、シグネチャ不一致をコンパイラに検査させられなかった。
- **変更:** `override` を付与し、Factory の共通復元経路との契約を明示した。
- **価値:** 将来の基底 API 変更や宣言誤りをコンパイル時に検出できる。

# 2026-08-10: Environment Map の JSON 往復を追加

- **関連:** `Artifact/include/Layer/ArtifactEnvironmentMapLayer.ixx`。
- **確認できた事実:** Environment Map は Factory 生成と Inspector 編集に対応していたが、固有の `toJson()` / `fromJsonProperties()` がなく、HDRI パス・強度・回転・背景表示が保存対象になっていなかった。
- **変更:** 基底 JSON に `LayerType::EnvironmentMap` と環境マップ固有値を追加し、名前空間付きキーと旧簡易キーの両方を復元する inline 実装を追加した。
- **価値:** Environment Map の保存・再読込で編集状態を維持できる。
- **次に確認:** HDRI 実体のロードを伴わない JSON 往復でプロパティ値が保持されること（ビルド・テスト未実行）。

# 2026-08-10: SDF シーンの JSON 往復を追加

- **関連:** `Artifact/include/Layer/ArtifactSDFLayer.ixx`, `Artifact/src/Layer/ArtifactSDFLayer.cppm`。
- **確認できた事実:** SDF は Factory 生成、プリミティブ編集、結合設定、出力解像度を持っていたが、固有 JSON がなく、シーンオブジェクトが保存されなかった。
- **変更:** 結合演算、スムージング、出力解像度、各プリミティブの形状・Transform・色・パラメータを JSON 化し、復元時に enum 範囲を制限した。
- **価値:** SDF レイヤーの編集内容を保存・再読込できる。
- **次に確認:** SDF 実描画を伴わない JSON 往復でオブジェクト数と設定値が保持されること（ビルド・テスト未実行）。

# 2026-08-10: Light の JSON 往復を追加

- **関連:** `Artifact/include/Layer/ArtifactLightLayer.ixx`, `Artifact/src/Layer/ArtifactLightLayer.cppm`。
- **確認できた事実:** Light は種別、色、強度、範囲、スポット形状、影、ライトリンクの編集 API を持っていたが、固有 JSON がなく保存時に失われていた。
- **変更:** Light 固有値を JSON 化し、復元時に enum 値を範囲制限して setter 経由で適用するようにした。
- **価値:** 3D Light の編集状態を保存・再読込できる。
- **次に確認:** レンダリングを伴わない Light JSON 往復で全プロパティが保持されること（ビルド・テスト未実行）。

# 2026-08-10: Layer 固有 toJson の override 契約を統一

- **関連:** `Artifact/include/Layer/ArtifactImageLayer.ixx`, `Artifact/include/Layer/ArtifactVideoLayer.ixx`, `Artifact/include/Layer/ArtifactSwitchLayer.ixx`, `Artifact/include/Layer/ArtifactParticleLayer.ixx`, `Artifact/include/Layer/ArtifactPaintLayer.ixx`。
- **確認できた事実:** 各 Layer は基底 `ArtifactAbstractLayer::toJson()` を virtual override していたが、一部宣言に `override` がなく、シグネチャ不一致をコンパイル時に検出できなかった。
- **変更:** 実装済みの固有 `toJson()` 宣言へ `override` を付与した。
- **価値:** Factory／保存経路で利用される virtual JSON 契約の静的検査を揃えた。

# 2026-08-10: SVG toJson の override 契約を補完

- **関連:** `Artifact/include/Layer/ArtifactSvgLayer.ixx`。
- **確認できた事実:** SVG の `fromJsonProperties()` には `override` が付いていたが、同じ virtual 対応の `toJson()` には付いていなかった。
- **変更:** SVG の `toJson()` 宣言にも `override` を付与した。

# 2026-08-10: Null Layer の保存型を補正

- **関連:** `Artifact/src/Layer/ArtifactNullLayer.cppm`。
- **確認できた事実:** Null の `toJson()` は基底 JSON をそのまま返し、基底が設定する `LayerType::Unknown` が残っていたため、Factory 経由の再読込で Null として復元できなかった。
- **変更:** Null の JSON に `LayerType::Null` を明示した。
- **価値:** Null Layer の保存・再読込が Factory の通常経路で成立する。

# 2026-08-10: Camera / SandSim2D / Paint の保存型を補正

- **関連:** `Artifact/src/Layer/ArtifactCameraLayer.cppm`, `Artifact/src/Layer/ArtifactSandSim2DLayer.cppm`, `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- **確認できた事実:** これらの固有 `toJson()` は基底 JSON の `LayerType::Unknown` を残したまま、固有プロパティだけを追加していた。
- **変更:** Camera / SandSim2D / Paint の各 JSON に対応する `LayerType` を明示した。
- **価値:** 保存後も Factory が正しい Layer 実装を選択できる。

# 2026-08-10: Camera enum setter の入力範囲を補正

- **関連:** `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- **確認できた事実:** Projection / Stereo の setter が enum 値を無検証で保持し、JSON や Inspector から範囲外の整数を渡せた。
- **変更:** Projection は 0〜1、Stereo は 0〜2 に clamp してから保持するようにした。
- **価値:** 不正 JSON や外部プロパティ入力で Camera の enum 状態が不正値にならない。

# 2026-08-10: SandSim2D の素材 enum 入力を補正

- **関連:** `Artifact/src/Layer/ArtifactSandSim2DLayer.cppm`。
- **確認できた事実:** `toolMaterial` は JSON・Inspector・公開 setter から enum を無制限 cast していた。`SandMaterial` の定義は Empty〜Acid の 0〜7 である。
- **変更:** すべての入力を `setToolMaterial()` に集約し、0〜7 に clamp するようにした。
- **価値:** 不正入力でシミュレーションの素材判定が未定義値にならない。

# 2026-08-10: Clone mode の JSON 復元範囲を補正

- **関連:** `Artifact/src/Layer/ArtifactCloneLayer.cppm`。
- **確認できた事実:** Clone の `mode` は復元時に直接 enum cast され、`CloneMode::Linear`〜`Spline` の 0〜6 外を保持できた。
- **変更:** JSON 値を 0〜6 に clamp してから `CloneMode` へ変換するようにした。
- **価値:** 不正 JSON で Clone の分岐処理が未定義値にならない。

# 2026-08-10: Clone mode の Inspector 入力範囲を統一

- **関連:** `Artifact/src/Layer/ArtifactCloneLayer.cppm` の `setPropertyValue()`。
- **確認できた事実:** JSON 復元は clamp 済みでも、Inspector の `Mode` 入力は直接 enum cast していた。
- **変更:** Inspector 経路も 0〜6 に clamp して、JSON と同じ不変条件を適用した。

# 2026-08-10: Video ProxyQuality の入力範囲を補正

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`, `Artifact/include/Proxy/ProxyService.ixx`。
- **確認できた事実:** `ProxyServiceQuality` は None〜Eighth の 0〜4 だが、Video の JSON・Inspector・公開 setter 経路が無制限 cast していた。
- **変更:** `setProxyQuality()` で 0〜4 に正規化し、全入力経路を同じ不変条件へ集約した。
- **価値:** 不正な品質値で proxy controller やスケール判定が未定義状態にならない。

# 2026-08-10: 3D 固定形状の Factory 入力範囲を補正

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm`, `Artifact/include/Layer/Artifact3DModelLayer.ixx`。
- **確認できた事実:** `FixedGeometry3D` は Auto〜Cone の 0〜5 だが、Factory の JSON 経路が無制限 cast して初期化パラメータへ渡していた。
- **変更:** Factory で 0〜5 に clamp してから `FixedGeometry3D` へ変換するようにした。`RenderMode` は既存 setter の範囲正規化を利用する。
- **価値:** 不正 JSON でも 3D 固定形状の mesh 生成が未定義値へ落ちない。

# 2026-08-10: Layer BlendMode の共通入力範囲を補正

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `ArtifactCore/include/Layer/LayerBlend.ixx`。
- **確認できた事実:** 基底 `setBlendMode()` が JSON・Particle・Inspector から渡る `LAYER_BLEND_TYPE` を無検証で保持していた。定義範囲は Normal〜SilhouetteLuma の 0〜33。
- **変更:** setter の共通入口で 0〜33 に clamp し、variant override と通常状態の両方へ正規化値を保存するようにした。
- **価値:** すべての Layer の blend mode 入力が有効な enum 範囲に保たれる。

# 2026-08-10: Layer variant BlendMode 復元の範囲を補正

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の variant JSON 復元。
- **確認できた事実:** 通常 Layer の setter は正規化済みでも、variant の `blendModeOverride` は JSON 整数を直接 enum cast していた。
- **変更:** variant override も 0〜33 に clamp して復元するようにした。
- **価値:** 通常状態と variant 状態の BlendMode 不変条件を一致させた。

# 2026-08-10: LayerCachePolicy の共通入力範囲を補正

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/include/Layer/ArtifactAbstractLayer.ixx`。
- **確認できた事実:** LayerCachePolicy は Default / Enabled / Disabled の 0〜2 だが、JSON と Inspector の setter 入力を無検証で保持していた。
- **変更:** `setLayerCachePolicy()` の共通入口で 0〜2 に clamp するようにした。
- **価値:** 不正値でキャッシュ判定が想定外状態にならない。

# 2026-08-10: MaskMode の共通 setter 入力を補正

- **関連:** `Artifact/src/Mask/MaskPath.cppm`, `Artifact/include/Mask/MaskPath.ixx`。
- **確認できた事実:** MaskMode は Add / Subtract / Intersect / Difference の 0〜3 だが、JSON、キーフレーム、Inspector の各経路が setter へ無検証 enum を渡していた。
- **変更:** `MaskPath::setMode()` で 0〜3 に clamp するようにした。
- **価値:** マスク合成の全入力経路で有効なモード値を維持する。

# 2026-08-10: Mask keyframe mode の保存時正規化を追加

- **関連:** `Artifact/src/Mask/MaskPath.cppm` の `setAnimationKeyframe()`。
- **確認できた事実:** 通常の `MaskPath::setMode()` は正規化済みでも、keyframe snapshot は mode を構造体のまま保存していた。
- **変更:** keyframe を格納する時点でも 0〜3 に clamp するようにした。
- **価値:** アニメーション keyframe からのサンプリングでも無効な MaskMode が伝播しない。

# 2026-08-10: Label color index の共通入力範囲を補正

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`。
- **確認できた事実:** UI のラベル色メニューは 0〜7（なし＋7色）を提供する一方、基底 setter は任意の整数を保持できた。
- **変更:** `setLabelColorIndex()` で 0〜7 に clamp するようにした。
- **価値:** JSON・Inspector・UI のどの入力でも存在しないラベル色番号にならない。

# 2026-08-10: Adjustment Layer の保存型を補完

- **関連:** `Artifact/include/Layer/ArtifactAdjustableLayer.ixx`, `Artifact/src/Layer/ArtifactAdjustableLayer.cppm`。
- **確認できた事実:** Adjustment Layer は Factory で生成できるが、固有 JSON がなく、2D 基底の `Unknown` 型で保存されていた。
- **変更:** Adjustment 固有の `toJson()` / `fromJsonProperties()` を追加し、保存型に `LayerType::Adjustment` を明示した。
- **価値:** Adjustment Layer を保存後に Factory の通常経路で正しく再生成できる。

# 2026-08-10: Legacy layerType の Factory 互換範囲を拡張

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm`。
- **確認できた事実:** `type` を持たず `layerType` だけを持つ旧 JSON は、Solid / Image / Text など一部の文字列しか認識せず、Adjustment / Group / Camera / 3D 系などが早期 return していた。
- **変更:** Factory の主要 LayerType と Layer 接尾辞を legacy `layerType` の認識・変換へ追加した。
- **価値:** 数値 `type` を持たない旧 JSON でも、主要 Layer を通常の Factory 復元へ接続できる。

# 2026-08-10: Legacy layerType の接尾辞別名を補完

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm`。
- **確認できた事実:** `MaterialContainerLayer` と `Procedural3D` は認識候補に含めても、型変換条件に同じ別名が揃っていなかった。
- **変更:** これらの別名も実際の `LayerType` 変換へ接続した。

# 2026-08-10: Adjustment effects の復元を補完

- **関連:** `Artifact/src/Layer/ArtifactAdjustableLayer.cppm`。
- **確認できた事実:** Adjustment の専用 `fromJsonProperties()` は共通メタデータだけを復元し、基底の effects 復元処理を呼んでいなかった。
- **変更:** `applyPropertiesFromJson()` を呼び、保存済み effects を Adjustment Layer の復元時にも適用するようにした。
- **価値:** Adjustment Layer の JSON 往復で、型だけでなく実際の調整効果も維持できる。

# 2026-08-10: Mask rasterizer の異常サイズ防御を追加

- **関連:** `Artifact/src/Mask/MaskPath.cppm`, `Artifact/src/Mask/LayerMask.cppm`。
- **確認できた事実:** ラスタライズ入口で出力先が null、または幅・高さが 0 以下の場合でも OpenCV の Mat 生成・参照へ進む経路があった。
- **変更:** 出力先の null と不正サイズを早期 return し、既存の出力 Mat は release するようにした。
- **価値:** 不正な描画要求で null dereference や OpenCV 側のサイズ例外へ進むリスクを抑える。描画内容や通常入力時の処理は変更しない。

# 2026-08-10: LayerMask の画像サイズ不一致を早期拒否

- **関連:** `Artifact/src/Mask/LayerMask.cppm` の `applyToImage()`。
- **確認できた事実:** alpha mask は引数の幅・高さで生成される一方、入力 `cv::Mat` の実サイズは検証されず、サイズ不一致のまま `cv::multiply()` へ進み得た。
- **変更:** null、空・不正サイズ、型不一致に加え、画像の列・行と指定サイズが一致しない場合も早期 return するようにした。
- **価値:** マスク適用時の OpenCV サイズ例外を防ぎ、通常の一致入力の処理は維持する。

# 2026-08-10: Alpha mask 変換パラメータの非有限値を補正

- **関連:** `Artifact/src/Mask/MaskPath.cppm` の `fromAlphaMask()`。
- **確認できた事実:** `simplificationTolerance` と `cornerThreshold` は `std::max()` の前に有限値検証がなく、NaN が OpenCV の輪郭近似や角判定へ渡る可能性があった。
- **変更:** 非有限値は 0 にフォールバックし、有限値だけ既存の下限 clamp を通すようにした。
- **価値:** 外部入力や変換設定の破損で輪郭抽出が不安定になる経路を減らす。

# 2026-08-10: Mask rasterizer の座標変換値を有限化

- **関連:** `Artifact/src/Mask/MaskPath.cppm` の `rasterizeToAlpha()`。
- **確認できた事実:** offset／scale は `cv::Point` 化、膨張、ぼかしカーネル計算の複数箇所で使われるが、NaN／無限大の検証がなかった。
- **変更:** 非有限 offset は 0、非有限 scale は 1 に補正し、以降の全計算で同じ安全値を使うようにした。
- **価値:** 座標変換の破損が OpenCV の不正座標・不正カーネルへ波及する経路を抑える。有限値の既存挙動は維持する。

# 2026-08-10: Shape 復元時の不正 customPath による polygon 消失を防止

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` の `ArtifactShapeLayer::fromJson()`。
- **確認できた事実:** JSON の `customPath` 配列が3点以上でも、頂点検証後の有効数が3未満になる場合がある。その場合も無条件で `customPolygonPoints_` を消していた。
- **変更:** 有効な custom path が3頂点以上成立した場合だけ polygon を相互排他で消し、不正 path の場合は polygon を保持するようにした。
- **価値:** 部分的に破損した shape JSON の復元で、別の有効なカスタム polygon まで失うデータロスを防ぐ。

# 2026-08-10: Solid layer の復元サイズを最小値へ正規化

- **関連:** `Artifact/src/Layer/ArtifactSolidImageLayer.cppm`, `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`。
- **確認できた事実:** 両 `setSize()` は width／height を検証せず `setSourceSize()` へ渡しており、破損 JSON の 0 以下サイズが描画下流へ残り得た。Shape layer には既に最小サイズ防御がある。
- **変更:** Solid の両実装でも width／height を最低1へ clamp してから保持するようにした。
- **価値:** Solid の JSON 復元・プロパティ編集で無効な描画サイズを作らず、Shape と同じ境界契約に揃える。

# 2026-08-10: Image 復元時の空ソースで連番状態を解消

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm` の `fromJsonProperties()`。
- **確認できた事実:** 復元開始時に `image.sequencePaths` を読み込んだ後、`image.sourcePath` が空でも source／画像本体だけを無効化し、連番パスと frame rate は残していた。
- **変更:** 空ソース分岐で sequence paths、frame rate、sequence source、cached index も初期化するようにした。
- **価値:** ソースなしなのに `isImageSequence()` だけが有効になる不整合を防ぎ、空ソース処理を `loadFromPath("")` と揃える。

# 2026-08-10: SourceCrop 変換のサイズ有限性を統一

- **関連:** `Artifact/src/Layer/ArtifactSourceCrop.cppm` の `sourceToOutputTransform()`。
- **確認できた事実:** source／output `QSizeF` は正値検査だけで、NaN の比較が false になる場合に変換計算へ進み得た。setter と JSON 復元側には既存の有限値防御がある。
- **変更:** 両サイズを `hasSourceSize()` で検証し、非有限または非正の入力では恒等変換を返すようにした。
- **価値:** 画像 crop の変換行列へ NaN／無限大が伝播する経路を閉じる。

# 2026-08-10: Image crop の旧フラット JSON 復元を補完

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm` の `fromJsonProperties()`。
- **確認できた事実:** `toJson()` は旧形式の `sourceCrop.*` キーも出力するが、復元側は新形式の `sourceCrop` オブジェクトだけを読んでいた。
- **変更:** オブジェクト形式がない場合、旧フラットキーを `SourceCrop::fromJson()` 用の構造へ組み立てて復元するようにした。
- **価値:** 旧プロジェクト／旧プリセットの crop・pan・zoom・rotation・anchor 状態を保存再読込で失わない。

# 2026-08-10: Construction layer JSON 数値を setter 契約へ統一

- **関連:** `Artifact/src/Layer/ArtifactConstructionLayer.cppm` の `fromJsonProperties()`。
- **確認できた事実:** プロパティ編集では幅・高さ・grid・opacity 等を clamp する一方、JSON 復元は float 値を直接保持していた。
- **変更:** 有限値検証と既存 UI 側に対応する下限・上限を共通の読み取り処理へまとめ、復元時にも同じ範囲へ正規化するようにした。
- **価値:** 破損 JSON の NaN／無限大／極端値が construction 描画サイズやガイド計算へ波及するのを防ぐ。

# 2026-08-10: 共通 transform 復元の非有限値を遮断

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `fromJsonProperties()`。
- **確認できた事実:** `AnimatableTransform3D` の復元入口は position／rotation／scale／anchor を JSON の double から直接 setter へ渡していた。setter は値をそのまま keyframe／current state に保持する。
- **変更:** time-zero の transform 値を finite fallback（位置・回転・anchor は0、scale は1）へ通してから復元するようにした。
- **価値:** 破損した layer JSON の NaN／無限大が共通 transform と下流描画へ伝播するのを防ぐ。子リポジトリは変更していない。

# 2026-08-10: Transform keyframe／variant 復元も有限化

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の transform keyframe と variant override 復元。
- **確認できた事実:** time-zero の transform を有限化しても、rotation／scale／position keyframe と variant transform は JSON の double を直接 `AnimatableTransform3D` へ渡していた。
- **変更:** keyframe の値・空間 tangent、および variant の position／rotation／scale／anchor を同じ有限 fallback へ通すようにした。
- **価値:** アニメーション開始後や variant 切替時にだけ NaN／無限大が現れる復元経路を閉じる。

# 2026-08-10: Variant opacity 復元を有限化

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の variant 復元。
- **確認できた事実:** variant の opacity override だけは JSON double を直接保持し、通常 layer の opacity setter に相当する 0〜1 clamp／有限値検証がなかった。
- **変更:** variant opacity を有限値検証後に 0〜1 へ clamp し、異常値は1へ戻すようにした。
- **価値:** variant 切替時の透明度計算へ NaN／範囲外値が伝播するのを防ぐ。

# 2026-08-10: Empty variants 復元時の base variant 保証

- **関連:** \`Artifact/src/Layer/ArtifactAbstractLayer.cppm\` の variants 復元。
- **確認できた事実:** \`variants\` が存在する空配列の場合、配列を clear した後に fallback の \`"A"\` variant を追加する分岐を通らず、variant が0件になっていた。
- **変更:** 復元分岐の後で常に空集合を検査し、必要なら base variant \`"A"\` を追加するようにした。
- **価値:** 破損／空の variants JSON でも active variant と通常の variant 操作を維持する。

# 2026-08-10: Physics／Motion 復元の有限値・範囲を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の physics／motion JSON 復元。
- **確認できた事実:** UI setter には mode、速度、stiffness、damping、mass、lagTau 等の範囲がある一方、JSON 復元は `std::clamp()` のみで NaN を除外していなかった。
- **変更:** 共通の有限値＋clamp helper を使い、motion mode と physics 初速も同じ契約へ揃えた。
- **価値:** 破損した復元値が Dynamics 更新へ NaN／無効 mode として入り、プレビューを不安定化する経路を減らす。

# 2026-08-10: Fracture／Trail／Fragment 復元の NaN 漏れを補完

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の fracture、motion trail、fragment appearance 復元。
- **確認できた事実:** これらの float は JSON 復元で `std::clamp()` のみを通り、NaN がそのまま内部状態へ残る可能性があった。
- **変更:** 既存の意味付き範囲を変えず、有限値＋clamp helper を適用した。
- **価値:** fracture／trail／fragment の描画・更新計算へ非有限値が伝播する経路を減らす。

# 2026-08-10: Component physics／crowd／fluid 復元を有限化

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の components 復元。
- **確認できた事実:** collision、crowd、particle emitter、fluid の float 値にも `std::clamp()` のみの復元が残っていた。
- **変更:** 既存の各 hard range を維持したまま、finite＋clamp helper を適用した。
- **価値:** コンポーネント評価・シミュレーションへ NaN／無限大が流入する経路を減らす。

# 2026-08-10: Layout／Cloner float 復元を有限化

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の components layout／cloner 復元。
- **確認できた事実:** safe-area padding、gap、cloner offset／jitter／spacing／radius／angle 等は JSON double を直接 float 化していた。
- **変更:** 既存の通常値を維持しつつ、非有限値と極端な浮動小数値だけを有限値＋安全範囲へ補正した。enum の意味は推測して変更していない。
- **価値:** layout 配置や cloner 行列計算へ NaN／無限大が流入する経路を減らす。

# 2026-08-10: Cloner transform 配列の有限値を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `clonerTransforms` と legacy transform 復元。
- **確認できた事実:** Cloner 本体の offset 等を補正しても、配列内の position／rotation／scale は double を直接 float 化していた。
- **変更:** 新形式・legacy 形式の両方で、position／rotation／scale を既存の安全範囲へ通すようにした。
- **価値:** 個別 cloner 操作の行列生成へ非有限値が残る経路を閉じる。

# 2026-08-10: Component descriptor 復元の配列上限を追加

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の generators／fields／cloneModifiers 復元。
- **確認できた事実:** descriptor 配列は JSON サイズをそのまま `reserve()` し、巨大な壊れた配列でも復元処理が全要素を走査する構造だった。近隣の component 配列には既存の上限パターンがある。
- **変更:** 各 descriptor 配列の reserve と復元件数を1024件へ制限した。
- **価値:** 破損 JSON による復元時の過剰メモリ確保・処理時間のリスクを抑える。

# 2026-08-10: Layout enum 復元とプロパティ設定の範囲を一致

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の layout mode／anchor mode／stack direction。
- **確認できた事実:** プロパティ定義に `Layout Mode = 0..2`、`Cross Alignment = 0..2`、`Direction = 0..1` が明記されていた一方、JSON 復元と直接設定は任意の整数を受け入れていた。
- **変更:** JSON 復元とプロパティ設定の双方を、それぞれ既存の hard range と同じ範囲へ clamp した。意味がコード上で確定できない horizontal／vertical pin、scale mode、cloner mode は変更していない。
- **価値:** UI 経由と JSON 経由で layout enum の不正値処理が分岐する問題を減らす。

# 2026-08-10: Cloner clone count の hard range を復元にも適用

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `component.cloner.cloneCount`。
- **確認できた事実:** プロパティ定義の hard range は 1..256 だが、JSON 復元と直接設定は下限のみを適用していた。
- **変更:** JSON 復元と直接設定を 1..256 の同じ範囲へ clamp した。
- **価値:** UI と保存データで cloner の生成数上限が食い違う経路を減らす。

# 2026-08-10: Fracture shard count の hard range を通常経路へ適用

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `fracture.shardCount`。
- **確認できた事実:** プロパティ定義の hard range は 1..256 だが、JSON 復元と通常のプロパティ設定は下限しか制限していなかった。イベント処理内の一時的な requested fragment count は別の内部経路で 1..4096 を使っている。
- **変更:** JSON 復元と通常設定のみを 1..256 へ clamp した。内部イベント経路は変更していない。
- **価値:** 通常の編集・保存データから過大な fracture shard 数が入る経路を減らす。

# 2026-08-10: Physics 直接設定の有限値・hard range を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の physics プロパティ設定。
- **確認できた事実:** JSON 復元には範囲補正がある一方、直接設定では stiffness／damping／follow-through などが float へ直接変換され、他の項目も `std::clamp()` だけで非有限値を防げなかった。
- **変更:** 既存の各 hard range を `finitePhysicsValue` に集約し、通常値は同じ範囲で維持しつつ、非有限値は現在値へフォールバックするようにした。
- **価値:** UI／API から NaN や無限大が physics 設定へ入り、シミュレーションや collider 更新へ伝播する経路を減らす。

# 2026-08-10: Motion／Fracture／Fragment 直接設定の有限値を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `setLayerPropertyValue()`。
- **確認できた事実:** JSON 復元は finite＋clamp 済みでも、motion、trail、fragment、fracture の通常プロパティ設定には `std::clamp()` だけ、または直接 float 化が残っていた。`std::clamp(NaN, ...)` は NaN を除去しない。
- **変更:** 既存 hard range を変えず、共有の有限値＋clamp 処理へ揃えた。通常値は従来範囲を維持し、異常値は現在値（不正なら下限）へ戻す。
- **価値:** UI／API からの異常な数値が motion・fracture・fragment の評価へ流入する経路を減らす。

# 2026-08-10: Component／Layout／Cloner 直接設定の有限値を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の component property dispatch。
- **確認できた事実:** collision、crowd、particle、fluid、layout、cloner の復元側には既存の範囲補正がある一方、直接設定側は `std::clamp()` のみ、または float への直接変換が残っていた。
- **変更:** 復元側で確立済みの安全範囲を `finiteClampedValue` に適用し、非有限値を現在値（不正なら範囲下限）へ戻すようにした。layout／cloner の enum は意味が確定している項目以外を変更していない。
- **価値:** 編集操作から component 評価、配置、cloner 行列へ NaN／無限大が伝播する経路を減らす。

# 2026-08-10: Generator／Cloner compatibility 設定の有限値を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の generator descriptor と compatibility sequence/time-offset 設定。
- **確認できた事実:** sequence の rate／softness は `std::clamp()` のみで、time offset・spacing・radius・angle は double をそのまま descriptor へ格納していた。
- **変更:** 既存の復元・UI で使われている範囲を維持し、finite＋clamp を適用した。descriptor に既存値がある場合はそれを異常値のフォールバックにした。
- **価値:** generator 設定から cloner の評価へ NaN／無限大や過大な座標値が流入する経路を減らす。

# 2026-08-10: Clone modifier の有限値設定を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `component.cloneModifiers.<index>` 編集。
- **確認できた事実:** rate／softness／strength／frequency／phase／indexScale 等は範囲指定されていたが、`std::clamp()` のみで NaN を除去できず、step と一部の座標値も直接 descriptor へ格納していた。
- **変更:** 既存範囲を維持した `setFiniteSetting` を追加し、既存 descriptor 値をフォールバックにして有限化した。
- **価値:** modifier stack の評価へ異常な数値が流入する経路を減らす。

# 2026-08-10: Clone modifier の soft-range 項目を有限値だけ補正

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の plain／random／step modifier 設定。
- **確認できた事実:** position／rotation／scale 系は UI では soft range のみで、hard range は確認できなかった。従って任意の有限値を clamp する根拠はないが、NaN／無限大は descriptor にそのまま入る。
- **変更:** hard range を新設せず、有限値は保持し、非有限値だけ既存値または既定値へ戻す `setFiniteOnlySetting` を適用した。
- **価値:** 仕様を狭めず modifier 評価の異常値流入だけを防ぐ。

# 2026-08-10: Field descriptor の直接設定を有限値だけ補正

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `component.fields.<index>` 編集。
- **確認できた事実:** field strength は 0..1 へ補正されていたが、center／radius／falloff／extent／scale／amplitude 等は double をそのまま descriptor に格納していた。
- **変更:** hard range を推測せず、有限値は保持し、非有限値だけ既存値（既存値も不正なら既定値）へ戻すようにした。
- **価値:** field 評価へ NaN／無限大が流入する経路を減らす。

# 2026-08-10: Cloner transform 直接設定を復元範囲へ統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `component.cloner.transforms.<index>` 編集。
- **確認できた事実:** transform 配列の JSON 復元には position／rotation／scale の安全範囲がある一方、直接設定は double をそのまま float 化していた。
- **変更:** 復元側と同じ position／scale ±100000、rotation ±360000 の finite＋clamp を直接設定にも適用した。
- **価値:** UI 編集から cloner 行列生成へ異常値が流入する経路を閉じる。

# 2026-08-10: Transform 直接設定の非有限値を除去

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の通常 transform property dispatch。
- **確認できた事実:** position／scale／rotation／anchor／initial rotation は UI 定義上 soft range のみで、setter は入力を直接 keyframe／initial 値へ渡していた。
- **変更:** soft range を hard range に昇格させず、有限値は保持し、NaN／無限大だけ現在時刻の値または既定値へフォールバックした。
- **価値:** transform keyframe、行列計算、modifier 評価へ異常値が流入する経路を減らす。

# 2026-08-10: Source size setter の hard range を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `source.width`／`source.height`。
- **確認できた事実:** property 定義の hard range は 1..16384 だが、直接設定は下限のみを適用していた。
- **変更:** width／height の setter を 1..16384 に clamp した。
- **価値:** UI 定義と直接設定で source／thumbnail／変換処理へ渡るサイズ上限が食い違う経路を減らす。

# 2026-08-10: Layer opacity 共通 setter の有限値を保証

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `ArtifactAbstractLayer::setOpacity()`。
- **確認できた事実:** `layer.opacity` は共通 setter に集約されていたが、`std::clamp()` のみで NaN 入力を除去できなかった。mask の数値 setter は別途補正済み。
- **変更:** 有限値は従来どおり 0..1 に clamp し、非有限値は現在値（現在値も不正なら 1）へフォールバックするようにした。
- **価値:** 通常 layer、variant、keyframe へ opacity の NaN が伝播する経路を減らす。

# 2026-08-10: Animation layer 直接編集の有限値を保証

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `animationLayers` property dispatch。
- **確認できた事実:** animation value は double を直接 float 化して keyframe へ渡し、weight は `std::clamp()` のみで NaN を除去できなかった。
- **変更:** 非有限の value は keyframe を作らず拒否し、weight は既存の 0..1 契約を保ったまま有限値へ補正した。2つの legacy／indexed 経路を同じ挙動に揃えた。
- **価値:** animation stack の評価へ NaN／無限大が入る経路を減らす。

# 2026-08-10: Animation interpolation の hard range を統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の animation layer と position keyframe 復元。
- **確認できた事実:** UI property の interpolation hard range は 0..32 だが、直接設定と position keyframe JSON 復元は任意整数を `InterpolationType` へ cast していた。
- **変更:** 直接設定と JSON 復元を 0..32 に clamp した。
- **価値:** 不正な enum 値が補間評価へ流入する経路を減らす。

# 2026-08-10: Solid gradient setter の有限値を統一

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm` と `ArtifactCompositionViewDrawing::makeVersionedSolidGradientImage()`。
- **確認できた事実:** renderer は gradient scale の下限しか補正せず、angle／center／offset の NaN は計算へ流れる一方、両 layer の setter は値を直接保持していた。
- **変更:** 有限の angle／center／offset は保持し、非有限値は既定値へ戻した。scale は renderer と同じ 0.0001 以上へ正規化した。
- **価値:** Solid gradient の画像生成・キャッシュキー・描画へ NaN／無効 scale が流入する経路を減らす。

# 2026-08-10: Solid gradient renderer 境界を有限化

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `makeVersionedSolidGradientImage()`。
- **確認できた事実:** renderer は scale の下限だけを適用し、angle／center／offset／scale の非有限値を直接計算へ渡していた。関数は layer setter を経由しない test／export caller からも呼ばれる。
- **変更:** renderer 境界で通常値を保持したまま、非有限値を既定値へ戻し、scale を 0.0001 以上へ揃えた。
- **価値:** 直接 caller を含む全 gradient 画像生成経路で NaN の画素計算・キャッシュ汚染を防ぐ。

# 2026-08-10: Solid gradient renderer の色入力境界を有限化

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `makeVersionedSolidGradientImage()`。
- **確認できた事実:** start／end color の各チャンネルは layer setter 経由では補正されても、renderer の直接 caller から NaN／無限大／範囲外が渡ると `std::clamp()` 前の alpha 判定や補間へ影響し得た。
- **変更:** renderer 境界で色チャンネルを有限値かつ 0..1 に正規化し、非有限値は RGB 0、alpha 1 へフォールバックして以降の補間だけで使用するようにした。
- **価値:** 直接呼び出しを含む gradient 画像生成で不正な色入力が NaN の画素値へ伝播する経路を減らす。

# 2026-08-10: Solid gradient renderer の角度 overflow を抑制

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `makeVersionedSolidGradientImage()`。
- **確認できた事実:** gradient angle は setter で有限値を保持するが hard range はなく、極端に大きい有限 float は度から radian への乗算で overflow し、`cos`／`sin` が不正値になる可能性があった。
- **変更:** renderer 内で角度を 360 度周期に `fmod` してから三角関数と conical gradient の計算へ渡すようにした。通常値の角度はそのままの見た目を保つ。
- **価値:** 直接 caller を含む極端な角度入力で gradient 全体が NaN 化する経路を減らす。

# 2026-08-10: Solid gradient renderer の補間係数を有限化

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `spreadGradient()`。
- **確認できた事実:** finite な中心／offset でも極端な入力では座標演算が ±∞ になり、repeating 分岐の `floor()` と減算で NaN が生じる可能性があった。
- **変更:** fill type ごとの周期処理より前に、非有限の補間係数を正規化した。NaN は 0.5、±∞ は 0／1 へ寄せ、通常値の計算は変更していない。
- **価値:** 極端な gradient 座標でも色補間と `qRgba()` へ NaN が流れ込む経路を減らす。

# 2026-08-10: Solid layer の gradient color setter を正規化

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm` の gradient color setter。
- **確認できた事実:** JSON／property の QColor 経路は通常有限値だが、公開 setter は FloatColor をそのまま保持し、直接 API 呼び出し後の `toJson()`／cache／QColor 変換へ不正チャンネルが残り得た。
- **変更:** 両 layer の start／end color setter で RGB／alpha を有限値かつ 0..1 に統一し、非有限値は RGB 0、alpha 1 へフォールバックした。
- **価値:** renderer 境界だけでなく layer 状態自体も安全になり、保存・cache・描画の各経路で同じ色契約を共有できる。

# 2026-08-10: Solid layer creation params の入力契約を統一

- **関連:** `Artifact/src/Layer/ArtifactLayerInitParams.cppm` の `ArtifactSolidLayerInitParams` setter 群。
- **確認できた事実:** Solid layer 本体と renderer はサイズ・色・gradient 数値を補正する一方、作成パラメータは width／height、FloatColor、angle／center／scale／offset を直接保持していた。
- **変更:** 作成パラメータでもサイズを 1..16384、色を有限値かつ 0..1、gradient の非有限値と scale 下限を layer 側と同じ契約へ正規化した。
- **価値:** layer 作成時だけ不正値が保存・cache・描画へ流れる経路を塞ぎ、初期化経路と編集経路の挙動を揃える。

# 2026-08-10: Solid layer factory の gradient 設定転送漏れを修正

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm` の Solid layer creation branch。
- **確認できた事実:** factory は init params から色・fill type・angle だけを転送し、reverse／center X/Y／scale／offset を転送していなかったため、作成直後にその設定が既定値へ戻っていた。
- **変更:** 5つの gradient 設定を init params から `ArtifactSolidImageLayer` へ転送するようにした。
- **価値:** 作成時設定と作成後の編集・保存結果が一致し、gradient の意図しない既定値化を防ぐ。

# 2026-08-10: Form Particle の invalid QColor を防御

- **関連:** `Artifact/src/Layer/ArtifactFormParticleLayer.cppm` の `FormParticleSettings::fromJson()`、property dispatch、`colorForPoint()`。
- **確認できた事実:** 不正な色文字列と invalid `QColor` を JSON／property 経路で無条件に保存していた。設定 struct の色フィールドは公開されているため、外部 caller が直接 invalid 値を設定する経路も残っていた。
- **変更:** invalid color は既存値へ戻し、描画直前にも solid／gradient 色を検証して fallback するようにした。
- **価値:** Form Particle の保存復元・編集・直接利用の各経路で、invalid QColor が粒子色計算へ流れることを防ぐ。

# 2026-08-10: Form Particle の数値入力を有限化

- **関連:** `Artifact/src/Layer/ArtifactFormParticleLayer.cppm` の `FormParticleSettings::fromJson()` と property dispatch。
- **確認できた事実:** spacing／particle size／opacity／source threshold／twist／falloff の一部が `std::max()` または直接 float 化のみで、非有限値を保持し得た。
- **変更:** 非有限値を既定値または現在値へ戻し、既存の最小値・0..1 hard range を維持する補助関数を JSON と property の両経路へ適用した。
- **価値:** Form Particle の配置・大きさ・透明度・field 計算へ NaN／∞が入る経路を減らす。

# 2026-08-10: Form Particle の clamp 共通処理を有限化

- **関連:** `Artifact/src/Layer/ArtifactFormParticleLayer.cppm` の匿名 namespace 内 `clampValue()`。
- **確認できた事実:** 設定 struct の公開フィールドは setter を経由せず直接変更でき、描画側には `clampValue()` を通る float 計算が複数残っていた。
- **変更:** floating-point の NaN／±∞を clamp の下限／上限へ寄せてから既存処理を続けるようにした。整数の挙動は維持した。
- **価値:** 直接設定された非有限値が particle vertex の色・透明度・補間係数へ伝播する経路を共通処理で減らす。

# 2026-08-10: Form Particle の色保存出口を正規化

- **関連:** `Artifact/src/Layer/ArtifactFormParticleLayer.cppm` の `FormParticleSettings::toJson()`。
- **確認できた事実:** 描画直前の色 fallback 後も、公開設定を直接変更した invalid `QColor` は JSON 保存時にそのまま `name()` へ渡されていた。
- **変更:** `toJson()` で solid／gradient 色を検証し、有効な既定色を保存するようにした。
- **価値:** invalid 色が保存データへ残り、次回復元時に再発する経路を閉じる。

# 2026-08-10: Form Particle の数値保存出口を正規化

- **関連:** `Artifact/src/Layer/ArtifactFormParticleLayer.cppm` の `FormParticleSettings::toJson()`。
- **確認できた事実:** 色と同様に、公開設定を直接変更した場合は spacing／size／opacity／noise／twist／falloff／threshold が非有限または復元時の想定範囲外のまま JSON 化され得た。
- **変更:** `toJson()` で既存の `fromJson()` と同じ下限・範囲、および既定値 fallback を適用してから保存するようにした。
- **価値:** 不正な数値が保存データへ残り、次回読み込みや別環境へ伝播する経路を減らす。

# 2026-08-10: Form Particle の enum／整数保存出口を正規化

- **関連:** `Artifact/src/Layer/ArtifactFormParticleLayer.cppm` の `FormParticleSettings::toJson()`。
- **確認できた事実:** 公開設定を直接変更すると generator／color／origin mode、grid dimensions、particle limit、render enum が `fromJson()` の hard range を経ずに保存され得た。
- **変更:** `toJson()` でも各 mode と整数値を既存の hard range へ clamp して保存するようにした。
- **価値:** 不正な enum 値や作成負荷を過大化する整数値が保存データへ残る経路を減らす。

# 2026-08-10: Particle emitter の直接 float setter を有限化

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::setLayerPropertyValue()`。
- **確認できた事実:** 同じ関数内に `safeParticleFloat()` がある一方、rotation speed、emitter の radius／width／height／depth／lineLength、opacity 中間値・終端値は直接 float 化または `std::clamp()` していた。
- **変更:** 既存 helper を使い、非有限値を現在値へ fallback し、geometry を 0..1000000、opacity 系を 0..1 に揃えた。
- **価値:** emitter geometry と particle opacity の direct property 経路で NaN／∞が粒子生成へ流れる経路を減らす。

# 2026-08-10: Particle render settings の保存値を有限化

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::toJson()`。
- **確認できた事実:** render settings の復元側は soft particle distance／stretch factor を 0..1000000 へ clamp しているが、保存側は direct setter の値をそのまま JSON 化していた。
- **変更:** 保存時にも同じ finite／0..1000000 契約を適用した。
- **価値:** render settings の NaN／∞が保存データへ残り、次回復元や他環境へ伝播する経路を減らす。

# 2026-08-10: Particle render enum の保存値を正規化

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::toJson()`。
- **確認できた事実:** render enum は property dispatch では clamp されるが、公開 render settings を直接変更した場合は blend／billboard／sort の値をそのまま保存していた。
- **変更:** 保存時にも既存の 0..4／0..3／0..3 range へ clamp した。
- **価値:** 不正な render enum が JSON へ残り、復元時の予期しない描画モードへつながる経路を減らす。

# 2026-08-10: Particle emitter color の保存出口を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::toJson()`。
- **確認できた事実:** emitter の colorStart／colorMid／colorEnd は `QColor::name()` へ直接渡され、公開パラメータを直接変更した invalid color が保存値へ入り得た。
- **変更:** 保存時に invalid color を白へ fallback してから ARGB 文字列化するようにした。
- **価値:** emitter の invalid color が JSON と次回復元へ伝播する経路を減らす。

# 2026-08-10: Particle emitter color の復元入口を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の emitter JSON restore。
- **確認できた事実:** colorStart／colorMid／colorEnd を JSON 文字列から無条件に `QColor` へ代入していたため、不正文字列で既定パラメータを invalid color に置き換え得た。
- **変更:** 復元した `QColor` が valid の場合だけ各パラメータへ代入するようにした。
- **価値:** 不正な保存データが emitter の描画色を壊す経路を防ぐ。

# 2026-08-10: Particle timeScale の overflow 上限を追加

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` の `ParticleSystem::setTimeScale()`。
- **確認できた事実:** setter は非有限値を fallback する一方、有限値には下限しかなく、極端な timeScale は update 内の `deltaTime * timeScale_` を overflow させ得た。
- **変更:** 既存の 0 以上契約を保ったまま、他の particle float property と同じ 1000000 上限へ clamp した。
- **価値:** 極端な時間倍率で粒子シミュレーション時刻が inf／NaN 化する経路を減らす。

# 2026-08-10: ParticleEmitter setParams の runtime 境界を防御

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` の `ParticleEmitter::setParams()`。
- **確認できた事実:** `setParams()` は EmitterParams を直接コピーし、layer 側の property／JSON 正規化を bypass できた。runtime の emitter shape 計算は radius／width／height／depth／lineLength を直接利用し、色もそのまま描画へ渡していた。
- **変更:** setParams 境界で geometry を有限値・0..1000000へ clamp し、invalid color を白へ fallback した。
- **価値:** generator API の直接利用でも emitter geometry の NaN／∞と invalid color が runtime へ流れる経路を減らす。

# 2026-08-10: ParticleEmitter setParams の allocation／flipbook 境界を統一

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` の `ParticleEmitter::setParams()`。
- **確認できた事実:** layer の JSON 復元は maxParticles、burst／aux count、texture dimensions、frame range／rate を clamp していたが、公開 setParams() は直接コピーしていた。
- **変更:** setParams() でも同じ hard range を適用するようにした。
- **価値:** generator API の直接利用で過大 allocation、無効な atlas dimensions、異常な frame rate が runtime へ流れる経路を減らす。

# 2026-08-10: ParticleEmitter setParams の乱数範囲を正規化

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` の `ParticleEmitter::setParams()`、`Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `initializeParticle()`。
- **確認できた事実:** initializeParticle() は life／speed／rotation／scale／opacity の min-max 差を乱数範囲へ渡す。公開 setParams() は max < min や非有限値をそのまま保持できた。
- **変更:** 各範囲を有限値・既存 hard range へ clamp し、max が min 未満にならないよう揃えた。
- **価値:** generator API の直接利用で乱数範囲が負値・NaN 化し、粒子初期化が破綻する経路を減らす。

# 2026-08-10: ParticleEmitter setParams の simulation quality／self-collision 境界を正規化

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` の `ParticleEmitter::setParams()`、`Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `update()`／`applySelfCollisionBroadPhase()`。
- **確認できた事実:** `update()` は fixedTimeStep を除算に、maxSubSteps を反復回数に使い、selfCollisionRadius／selfCollisionResponse は broad-phase のセル半径と補正係数へ直接使う。公開 setParams() ではこれらが非有限値・極端値のまま残り得た。
- **変更:** fixedTimeStep を有限値・0.000001..1.0、maxSubSteps を 1..256、selfCollisionRadius を有限値・0.001..1000000、selfCollisionResponse を有限値・0..1 へ clamp し、非有限値は既定値へ戻すようにした。
- **価値:** 直接 API 利用時にも除算、反復回数、空間ハッシュ、衝突補正へ不正値が流れる経路を減らす。

# 2026-08-10: ParticleEmitter setParams の physics／vector 境界を JSON 経路と統一

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` の `ParticleEmitter::setParams()`、`Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `updateParticle()`／`getEmissionDirection()`。
- **確認できた事実:** JSON 復元では position／rotation／direction／velocityRandom の成分を補正していたが、公開 setParams() は gravity／windDirection を含むベクトルを直接保持していた。updateParticle() はこれらを速度計算へ直接使い、aux／turbulence／drag の scalar も runtime へ渡す。
- **変更:** setParams() で主要ベクトル成分を有限値・±1000000へ clamp し、direction spread、mass、drag、wind／turbulence、補助粒子の scalar を既存の hard range／既定値へ正規化した。
- **価値:** generator API の直接利用でも NaN／∞や過大な physics／aux 値が粒子更新へ流れる経路を減らす。

# 2026-08-10: FlockingEffector の直接設定値を runtime で防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `FlockingEffector::apply(std::vector<Particle>&, float)`。
- **確認できた事実:** Flocking の公開フィールドは setter を経由せず直接設定でき、実装は neighborhoodRadius の二乗と重みをそのまま近傍検索・加速度計算へ使っていた。
- **変更:** deltaTime、近傍半径、3 種の重み、最大加速度を有限値・既存の粒子系 hard range へ局所正規化し、非有限値は無効化した。
- **価値:** 不正な effector 設定が flocking 加速度と近傍探索を NaN／∞化する経路を減らす。

# 2026-08-10: Vortex／Attractor／Repeller の direct effector 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の各 effector `apply()`、`Artifact/src/Layer/ArtifactParticleLayer.cppm` の effector 追加 API。
- **確認できた事実:** これらの effector は公開フィールドを直接使い、radius／falloff／tightness の無効値を `pow()` や半径判定へ渡していた。add API も入力値をそのまま格納する。
- **変更:** deltaTime、距離、radius、falloff／tightness、strength を runtime で有限値・上限へ補正し、ゼロ半径や非有限距離では処理をスキップするようにした。
- **価値:** 不正な direct effector 設定によるゼロ除算、`pow()` の NaN、粒子速度の破綻を抑える。

# 2026-08-10: Force／Turbulence／Wind の direct effector 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の各 effector `apply()`、`Artifact/src/Layer/ArtifactParticleLayer.cppm` の effector JSON 復元。
- **確認できた事実:** Force の vector／strength、Turbulence の frequency／amplitude／evolution、Wind の方向・強度・周波数は公開フィールドから直接 velocity／acceleration 計算へ入る。
- **変更:** deltaTime、ベクトル成分、各 scalar を有限値・既存の JSON hard range へ runtime 正規化してから計算するようにした。
- **価値:** direct API 経由の非有限 effector 設定が粒子の速度・加速度を NaN／∞化する経路を減らす。

# 2026-08-10: KillZoneEffector の無効 zone 設定を no-op 化

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `KillZoneEffector::apply()`、`Artifact/src/Layer/ArtifactParticleLayer.cppm` の JSON 復元。
- **確認できた事実:** JSON 復元では zoneType／size を補正していたが、direct API の不正 enum は `inside=false` のまま判定され、invert=true では全粒子を kill し得た。Plane のゼロ方向も有効な判定として扱われていた。
- **変更:** position／particle position／size／direction の有限性を確認し、size を clamp、不正 enum とゼロ方向 Plane は処理をスキップするようにした。
- **価値:** direct effector 設定による意図しない全粒子削除と不安定な Plane 判定を防ぐ。

# 2026-08-10: Particle effector／system update の null・deltaTime 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::addEffector()`／`applyEffectors()`／`ParticleSystem::update()`。
- **確認できた事実:** `addEffector()` は null unique_ptr を受け入れ、単体粒子更新側だけが null check なしで `enabled` を参照していた。また ParticleSystem は非有限／負 deltaTime を `time_` へ加算し得た。
- **変更:** null effector を登録しないようにし、apply 側にも防御を追加。不正 deltaTime と scaledDelta は更新をスキップするようにした。
- **価値:** direct API の null 登録によるクラッシュと、無効 deltaTime による simulation time の NaN／逆行を防ぐ。

# 2026-08-10: ParticleSystem goToFrame の fps／長時間 accumulator 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::goToFrame()`。
- **確認できた事実:** fps の NaN は従来の `fps <= 0` 判定を通過し、進行 accumulator は float だったため、長時間ターゲットでは 1/120 秒刻みが丸められて currentTime が進まなくなる可能性があった。
- **変更:** fps／targetTime を有限値・正値として検証し、進行 accumulator を double 化。不正な dt と null emitter もその反復でスキップするようにした。
- **価値:** direct API の不正 fps と長時間フレーム移動による無限ループ・時刻破綻を抑える。

# 2026-08-10: Particle preWarm の無効刻み・duration 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::preWarm()`／`ParticleSystem::preWarm()`。
- **確認できた事実:** emitter の preWarm は stepSize を検証せず、0 や NaN の刻みで終了しない可能性があり、無効 duration でも先に既存粒子を clear していた。
- **変更:** duration／stepSize を検証してから clear し、duration／stepSize を既存の runtime hard range へ clamp。double accumulator と正の dt 検証、null emitter 防御を追加した。
- **価値:** direct API の不正 preWarm 入力による無限ループ、過大な反復、意図しない状態消去を防ぐ。

# 2026-08-10: ParticleSystem captureRenderData の非有限粒子を描画境界で防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::captureRenderData()`。
- **確認できた事実:** alive 粒子の position／velocity／scale／opacity／age／lifetime を検証せず render vertex へコピーしていたため、壊れた simulation 値が GPU／描画経路へ流れ得た。
- **変更:** 非有限 position／velocity の粒子は capture 対象から除外し、scalar は有限値・非負・1000000 上限へ正規化。alpha は 0..1、lifetime は最小値を保証した。
- **価値:** render boundary で NaN／∞ vertex が downstream の投影・GPU upload・描画へ伝播する経路を減らす。

# 2026-08-10: ParticleSystem QPainter render の別収集経路を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::render(QPainter&, const QTransform&)`。
- **確認できた事実:** captureRenderData() とは別に QPainter render が alive 粒子を直接収集し、null emitter、非有限 position／prevPosition／velocity／age、NaN stretch／trail 設定を検証していなかった。
- **変更:** 描画対象の収集時に粒子の主要値を有限確認し、null emitter をスキップ。opacity、size、trail width、stretch factor を描画境界で clamp した。
- **価値:** ソフト描画の sort／transform／gradient へ不正値が流れる別経路を抑制する。

# 2026-08-10: ParticleSystem software／GPU render builder の入力境界を統一

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `updateAndRenderSoftwareFrame()`／`renderGPU()`。
- **確認できた事実:** QPainter 経路以外にも stretchFactor を直接使う software frame と GPU vertex builder があり、GPU builder は null emitter・null vertex buffer・非有限 particle を検証していなかった。
- **変更:** software stretch factor を finite clamp。renderGPU() で buffer／capacity、null emitter、主要 particle vector／scalar を検証し、size・stretch・alpha を安全化した。
- **価値:** 全描画 backend で不正値が vertex buffer、投影計算、GPU upload へ流れる経路を減らす。

# 2026-08-10: ParticleSystem camera position の sort／projection 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::setCameraPosition()`、Distance sort、software projection。
- **確認できた事実:** camera position は公開 setter から非有限／極端値をそのまま保持でき、距離比較と投影の両方が直接利用していた。
- **変更:** setter で各成分を有限値・±1000000へ clampし、非有限値は 0 へ fallback した。
- **価値:** NaN camera による sort comparator の不安定化と software projection の無効 depth 計算を抑える。

# 2026-08-10: ParticleRenderSettings の direct setter 境界を正規化

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` の `ParticleSystem::setRenderSettings()`、`Artifact/src/Layer/ArtifactParticleLayer.cppm` の JSON／property 適用。
- **確認できた事実:** setter は設定値を単純コピーしており、公開 API から無効 enum、非有限 soft／stretch／trail scalar、過大 trailLength を保持できた。
- **変更:** blend／billboard／sort enum を既存範囲へ clampし、render scalar と trailLength を有限値・既存 hard range／既定値へ正規化した。
- **価値:** direct API と JSON／property 経路の描画設定契約を統一し、render backend の不正分岐・過大描画負荷を減らす。

# 2026-08-10: Particle flipbook atlas 次元の overflow 境界を統一

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `clampFlipbookFrame()`／`flipbookFrameCount()`／software capture。
- **確認できた事実:** rows／cols は通常 setter で clamp される一方、mutable params() 経路では `rows * cols` を直接計算し、atlas frame 数・frameWidth 計算へ渡していた。
- **変更:** 共通 flipbook helper と render capture の rows／cols を 1..1024 に clamp し、積が安全な範囲で計算されるようにした。
- **価値:** direct API の過大 atlas 次元による整数 overflow、負の frame 数、壊れた flipbook frame 選択を抑える。

# 2026-08-10: ParticleSystem totalParticleCount の整数 overflow 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::totalParticleCount()`、`captureRenderData()` の reserve。
- **確認できた事実:** emitter ごとの particleCount を int へ無検証加算しており、多数 emitter／大きな maxParticles の組み合わせで負値や overflow が reserve へ流れ得た。
- **変更:** null emitter を無視し、int 最大値を超える場合は飽和して返すようにした。
- **価値:** 粒子数集計の overflow による予約容量の破綻と描画 snapshot の不安定化を抑える。

# 2026-08-10: ParticleEmitter emission accumulator の int cast 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::simulateStep()`。
- **確認できた事実:** finite な deltaTime と rate の積でも emitAccumulator_ は int 最大値を超え得て、そのまま `static_cast<int>` していた。
- **変更:** int cast 前に安全上限を確認し、極端な accumulator は maxParticles を1回の上限として消費・リセットするようにした。
- **価値:** direct update API の極端な時間刻みで emission count の未定義 cast と過大ループを防ぐ。

# 2026-08-10: ParticleEmitter updateParticle の状態最終境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::updateParticle()`。
- **確認できた事実:** p.age が NaN の場合は `p.life <= 0` を通過し、flipbook frame の `floor()`／int cast へ進み得た。integration 後の position／velocity／rotation と補間 scalar も同様に runtime へ残り得た。
- **変更:** age が非有限なら粒子を破棄し、integration 後の主要 vector／rotation が無効な粒子も破棄。scale／opacity は finite clamp した。
- **価値:** 壊れた粒子状態が次フレーム、flipbook 計算、描画へ伝播する経路を最終境界で止める。

# 2026-08-10: ParticleLayer transformParticleRenderData の transform 境界を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `transformParticleRenderData()`。
- **確認できた事実:** QTransform の行列／translation と layer opacity を検証せず map／scale／alpha 計算へ使っていた。source size／velocity も direct core 呼び出しでは非有限になり得た。
- **変更:** 無効 transform は identity に fallback。opacity、scale、速度由来 stretch を有限値・既存描画範囲へ正規化した。
- **価値:** layer transform 境界で NaN／∞が core particle data、GPU draw、alpha／size 計算へ再混入する経路を減らす。

# 2026-08-10: ParticleLayer LOD／debug boost の非有限境界を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `applyParticleRenderLOD()`／`boostDebugParticleRenderData()`。
- **確認できた事実:** LOD の非有限 screenScale は keepRatio を経由して `ceil()`／size_t cast へ進み得た。debug boost も size／color を直接算術演算していた。
- **変更:** 非有限／極端な screenScale は安全化し、NaN は元データを保持。debug size／color は finite clamp 後に補正した。
- **価値:** layer LOD とデバッグ描画での整数 cast、size、color の NaN／∞伝播を抑える。

# 2026-08-10: ParticleLayer frame／time 変換の FPS 境界を統一

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の draw／goToFrame／renderToImage／debug draw 経路。
- **確認できた事実:** ParticleSystem 側の goToFrame は FPS を検証する一方、layer の複数経路は composition の framerate を直接使い、NaN／0 が除算・cache frame・fallback render へ入る余地があった。
- **変更:** layer 内の composition FPS 取得を共通 `safeParticleFps()` に通し、0.001..1000 の有限値へ統一した。
- **価値:** GPU／fallback／cache の frame→time 変換で NaN、ゼロ除算、極端な反復時間を抑える。

# 2026-08-10: ParticleLayer renderFrame の size／time 入力境界を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `renderFrame()`／`renderToImage()`。
- **確認できた事実:** public render API は width／height をそのまま QImage allocation に、time を simulation delta と cache frame 計算へ渡していた。
- **変更:** render dimensions を 1..16384、非有限 time を 0 秒へ正規化し、非有限 lastTime も 0 に fallback した。
- **価値:** direct render API の過大 allocation、NaN 時刻、無効な simulation delta の伝播を抑える。

# 2026-08-10: ParticleLayer transform 座標の overflow 境界を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `transformParticleRenderData()`。
- **確認できた事実:** finite な transform でも巨大な行列係数と particle 座標の積で `QPointF` map 結果が float overflow し、GPU particle vertex へ流れる余地があった。
- **変更:** mapped x／y を有限値・±1e7へ clamp し、非有限結果は finite source 座標へ fallback した。
- **価値:** transform 後の座標 overflow による GPU projection／draw の破綻を抑える。

# 2026-08-10: ParticleSystem removeEmitter の通知契約を修正

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::removeEmitter(ParticleEmitter*)`。
- **確認できた事実:** 非登録ポインタや null を渡しても `emitterRemoved` を発行し、管理コンテナの実状態と UI／observer 通知が不一致になっていた。
- **変更:** null／未登録ポインタでは no-op とし、実際に削除した場合だけ erase と通知を行うようにした。
- **価値:** emitter 管理状態と removal notification の整合性を保つ。

# 2026-08-10: ParticleLayer frame number の float overflow 境界を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の frame render／goToFrame／fallback 経路。
- **確認できた事実:** FPS を有限化しても `int64_t frameNumber` を先に float 化する式が複数残り、巨大 frame では time=∞ が simulation state と cache へ流れ得た。
- **変更:** 共通 `safeParticleFrameTime()` で double 計算後に有限値・±1e6 秒へ clamp し、全 layer frame→time 経路へ適用した。
- **価値:** 巨大 frame 番号の float overflow、無効 lastTime、cache／fallback render の時刻破綻を抑える。

# 2026-08-10: ParticleManager removeSystem の通知契約を修正

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleManager::removeSystem()`。
- **確認できた事実:** 未登録 system 名に対しても `systemRemoved` を発行し、管理 map の実状態と observer 通知が不一致になっていた。
- **変更:** 登録済み system が存在する場合だけ erase と通知を行うようにした。
- **価値:** system 管理状態と removal notification の整合性を保つ。

# 2026-08-10: ParticleSystem renderGPU の quad capacity 境界を修正

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::renderGPU()`。
- **確認できた事実:** 1 particle は4 vertexを書き込むが、従来の判定は `vertexCount >= maxVertices` のみで、残り3以下の capacity でも書き込みを許していた。
- **変更:** `vertexCount > maxVertices - 4` で判定し、4 vertex が丸ごと収まる場合だけ quad を生成するようにした。
- **価値:** GPU vertex buffer の末尾 overrun と破損した draw data を防ぐ。

# 2026-08-10: ParticleEmitter flipbook frame の整数 cast 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::updateParticle()`。
- **確認できた事実:** `p.age * frameRate` を `floor()` 後に直接 `int` へ cast してから available frame 数で剰余を取っていた。setter 経由では frameRate を制限しているが、mutable `params()` から極端値が入ると cast 前に int 範囲を超え得る。
- **変更:** frameRate を有限値・0..1000 に正規化し、frame 進行値を double の `fmod()` で available frame 範囲へ先に折り返してから int 化した。
- **価値:** 極端な経過時間や bypass された emitter パラメータでも、flipbook frame 計算の整数 overflow／未定義変換を抑える。

# 2026-08-10: ParticleEmitter self-collision の runtime パラメータ境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::applySelfCollisionBroadPhase()`。
- **確認できた事実:** broad-phase が `selfCollisionRadius` と `selfCollisionResponse` を直接 `max`／`clamp` しており、mutable `params()` 経由の NaN／∞をセルサイズ・衝突補正へ渡す余地があった。
- **変更:** 半径を有限値・0.001..1000000、反応係数を有限値・0..1へ正規化した。
- **価値:** 自己衝突の空間ハッシュと速度補正で非有限値が伝播し、粒子状態を壊す経路を抑える。

# 2026-08-10: ParticleEmitter fixed-step の step count cast 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::update()`。
- **確認できた事実:** `fixedStepAccumulator / fixedTimeStep` を直接 `int` へ cast していた。通常の `setParams()` では安全だが、mutable `params()` の極小 fixedTimeStep や累積値の∞で整数範囲を超え得る。
- **変更:** fixed timestep と max substeps を既存範囲へ再正規化し、step 数を double で計算して 0..maxSubSteps に clamp してから int 化した。累積値が非有限／負の場合もリセットする。
- **価値:** 極端な direct parameter mutation での未定義整数変換と過大な固定刻み反復を防ぐ。

# 2026-08-10: ParticleSystem／ParticleEmitter の時刻と transform 差分を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::update()`／`ParticleSystem::update()`。
- **確認できた事実:** emitter の position／rotation を検証せず inherited velocity と local-space 差分へ使い、両クラスの累積時刻も直接加算していた。mutable パラメータや長時間更新で NaN／∞が次の粒子生成・frame 状態へ残る余地があった。
- **変更:** transform vector を有限値として扱い、無効入力では差分移動を抑制して前回の安全な transform を保持。emitter／system 時刻は非負・float 最大値以内へ飽和させた。
- **価値:** transform 差分由来の非有限 velocity と、累積時刻 overflow の伝播を抑える。

# 2026-08-10: ParticleEmitter 初期粒子の random range 境界を統一

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::initializeParticle()`。
- **確認できた事実:** speed／rotation／scale／opacity／lifetime の各初期値が、mutable `params()` の範囲差をそのまま `QRandomGenerator::bounded()` へ渡していた。範囲が負、非有限、または過大な場合の runtime 契約が不明確だった。
- **変更:** 有限値・±1000000 の範囲へ clamp し、空範囲は下限を返す共通 `randomInRange()` に統一した。
- **価値:** 粒子生成時の不正な乱数幅と、初期 NaN／∞の再侵入を抑える。

# 2026-08-10: ParticleEmitter color variation の整数 cast 境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::initializeParticle()`。
- **確認できた事実:** color variation が mutable `params()` から∞などで入ると、色 variation の計算結果を直接 `int` 化する経路があった。
- **変更:** variation を有限値・0..1へ正規化してから色差分を計算するようにした。
- **価値:** 色生成時の過大な整数変換と不正な QColor 値の伝播を抑える。

# 2026-08-10: ParticleLayer debug fallback の dimension cast 境界を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ParticleDebugLayer::draw()` fallback 経路。
- **確認できた事実:** `localBounds()` の width／height を `ceil()` 後に直接 `int` 化し、renderFrame の 1..16384 clamp より前に変換していた。
- **変更:** 非有限値を 1、有限値を 1..16384 に clamp する `safeParticleDimension()` を追加し、fallback の幅・高さへ適用した。
- **価値:** 異常な layer bounds による整数 overflow と過大な fallback image allocation を抑える。

# 2026-08-10: ParticleLayer transform の RGB 境界を防御

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `transformParticleRenderData()`。
- **確認できた事実:** layer transform では alpha／size を正規化していたが、RGB は source 値をそのまま GPU 用 vertex へコピーしていた。
- **変更:** RGB も有限値・0..1へ正規化してから transformed data へ渡すようにした。
- **価値:** direct particle render data 由来の NaN／範囲外カラーが GPU 描画へ流れる経路を閉じる。

# 2026-08-10: ParticleLayer direct render data の GPU 境界を正規化

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `toCoreParticleRenderData()`。
- **確認できた事実:** direct layer render data を core vertex へ変換する際、z／速度／回転／age／lifetime／atlas 値を無加工でコピーしていた。通常の capture 経路以外では renderer へそのまま到達し得る。
- **変更:** geometry、速度、色、size、stretch、rotation、寿命を有限値・既存描画範囲へ clamp し、atlas rows／cols／frame も 1..1024 と有効 frame 範囲へ正規化した。
- **価値:** direct data bypass から GPU particle renderer へ非有限値や不正 atlas index が流れる経路を閉じる。

# 2026-08-10: ParticleLayer removeEmitter の無効 index 通知を抑制

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::removeEmitter(int)`。
- **確認できた事実:** index の妥当性を確認せず core の remove 呼び出し後に常に `emitterRemoved` を発行していた。無効 index でも observer だけが削除済みと認識する可能性があった。
- **変更:** 0..emitterCount-1 の index だけを処理し、無効 index は no-op にした。
- **価値:** layer の emitter 管理状態と UI／observer 通知の整合性を保つ。

# 2026-08-10: ParticleLayer property の emitter／preview 上限を統一

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `setLayerPropertyValue()`。
- **確認できた事実:** `particle.emitterCount` は非負化だけで上限がなく、大量 emitter の生成ループへ入れた。preview width／height も任意の int を保持し、renderFrame の上限と不一致だった。
- **変更:** emitter 数を既存 JSON 復元上限と同じ 1024、preview dimensions を renderFrame と同じ 1..16384 に clamp した。
- **価値:** property editing による過大な CPU／メモリ負荷と、preview サイズ契約の不一致を抑える。

# 2026-08-10: ParticleLayer property metadata の上限を runtime と同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `getLayerPropertyGroups()`。
- **確認できた事実:** property setter で emitter 数と preview dimensions に上限を追加した後も、editor metadata は emitter 数が無制限、preview dimensions が未設定だった。
- **変更:** emitter count を 0..1024、preview width／height を 1..16384 の hard range として明示した。
- **価値:** UI 入力、property cache、runtime clamp の契約を揃え、過大値が編集面から投入される余地を減らす。

# 2026-08-10: ParticleLayer render property metadata の hard range を同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `getLayerPropertyGroups()`。
- **確認できた事実:** blend／billboard／sort enum と soft particle distance／stretch factor は setter／JSON 側に hard clamp がある一方、editor property には tooltip／soft range しかなかった。
- **変更:** enum を 0..4／0..3／0..3、numeric 値を 0..1000000 の hard range として明示した。既存の soft range は推奨値として維持した。
- **価値:** UI 入力と runtime／persistence の許容範囲を一致させる。

# 2026-08-10: ParticleLayer emitter metadata の enum／flipbook 範囲を同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の emitter property groups。
- **確認できた事実:** runtime／JSON／setter では shape、mode、atlas、frame、frameRate、maxParticles を制限していたが、editor metadata の一部は旧い hard range または未設定だった。
- **変更:** shape 0..7、mode 0..2、texture rows／cols 1..1024、start frame 0..1e9、frame count 1..1e6、frameRate 0.001..1000、maxParticles 1..1e7 に揃えた。
- **価値:** preset／flipbook 編集時の UI 入力契約を runtime の正規化範囲と一致させる。

# 2026-08-10: ParticleLayer emitter metadata の連続値 hard range を同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `getLayerPropertyGroups()`。
- **確認できた事実:** emitter の position／rotation／direction、shape geometry、rate、burst interval は setter／`ParticleEmitter::setParams()` 側で有限値と ±1e6 または 0..1e6 の範囲へ正規化していたが、editor metadata は soft range のみだった。`burstCount` も metadata の 1..100000 が runtime の 0..10000000 と一致していなかった。
- **変更:** 各連続値に runtime 契約と一致する hard range を追加し、burst count を 0..10000000 に揃えた。soft range は通常編集時の推奨範囲として維持した。
- **価値:** UI の入力制限、property setter、JSON／runtime 正規化の境界を揃え、極端値が編集面から投入される余地を減らす。

# 2026-08-10: ParticleLayer particle／physics／aux metadata の hard range を同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の particle、physics、aux property groups。
- **確認できた事実:** life、speed、velocity random、scale、opacity、physics、aux の多くは runtime の有限値・clamp 契約を持つ一方、editor metadata は soft range のみ、または aux count の hard range が 0..256 に留まっていた。
- **変更:** runtime の 0..1e6、±1e6、scale 0..1000、opacity 0..1 などに対応する hard range を追加・更新し、soft range は推奨値として残した。
- **価値:** particle parameter の編集面と runtime 正規化の契約を揃え、極端値や旧い上限による意図しない入力制限を減らす。

# 2026-08-10: ParticleLayer aux trigger metadata の enum 範囲を同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `particle.aux.trigger`。
- **確認できた事実:** tooltip では Trails／Birth／Death の 0..2 を示していたが、property metadata の hard range は未設定だった。
- **変更:** runtime の trigger enum と同じ 0..2 の hard range を追加した。
- **価値:** UI から未定義の aux trigger 値を投入できないようにし、表示説明と入力契約を一致させる。

- **追加確認:** `particle.aux.opacityScale` の soft range が runtime の 0..1 契約を越えて 2.0 まで表示していたため、推奨範囲も 0..1 に修正した。

# 2026-08-10: ParticleLayer velocity random の入力契約を再調整

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `velocityRandomX/Y/Z` metadata と `setLayerPropertyValue()`。
- **確認できた事実:** 初回 metadata 追加では `ParticleEmitter::setParams()` のベクトル正規化に合わせて ±1e6 としたが、layer の property setter はランダム振幅を 0..1e6 に clamp していた。
- **変更:** editor hard range を setter／ランダム振幅の意味に合わせて 0..1e6 に修正した。
- **価値:** metadata と実際の layer property 入力経路の契約を一致させ、負のランダム振幅を UI から投入できないようにする。

# 2026-08-10: ParticleEmitter emission 入力を直前正規化

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `getEmissionPosition()`／`getEmissionDirection()`。
- **確認できた事実:** `setParams()` は position／rotation／direction と shape geometry を正規化するが、公開された mutable params 経路ではその後に非有限値や極端値を設定でき、回転行列・emission offset へ直接入っていた。
- **変更:** emission 直前にベクトルを有限・±1e6、geometry を有限・0..1e6 へ正規化した。
- **価値:** setter を迂回する authoring／preset 状態でも、粒子生成の初期位置・方向が不正値へ汚染される経路を閉じる。

# 2026-08-10: ParticleEmitter update 中の aux append による vector 無効化を防止

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleEmitter::update()` と `updateParticle()`。
- **確認できた事実:** update の range-for 中に、死亡／trail aux の `emitAuxParticlesFromParticle()` が同じ `particles_` へ append していた。再配置時に range-for の参照・iterator が無効化される可能性があった。
- **変更:** Phase 1b 開始時の粒子数を保存し、index ループで既存粒子だけを処理するようにした。追加された aux 粒子は次の simulation step から更新する。
- **価値:** aux emission 有効時の undefined behavior を除去し、更新順序も deterministic に保つ。

# 2026-08-10: aux emission source 参照を vector append から分離

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `emitParticles()` と `updateParticle()`。
- **確認できた事実:** Phase 1b を index ループへ変えても、birth／death／trail aux 生成時に vector 要素への参照を `emitAuxParticlesFromParticle()` へ渡していた。aux append の再配置後、その参照を使い続ける経路が残っていた。
- **変更:** aux 生成前に source particle を値コピーし、death 状態と trail interval の更新を append 前に確定した。
- **価値:** aux emission の全入口で vector reallocation による dangling reference を防ぐ。

# 2026-08-10: ParticleEmitter self-collision cell key の整数境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `applySelfCollisionBroadPhase()`。
- **確認できた事実:** position は有限でも cellSize が小さい場合、`floor(position / cellSize)` の float→int 変換が int 範囲を超え得た。また巨大な差分では `lengthSquared()` が非有限になり得た。
- **変更:** cell coordinate を int の安全な内部範囲へ飽和し、非有限の距離二乗を衝突対象から除外した。
- **価値:** self-collision の broad phase が極端な座標で未定義変換や不正な normal／impulse 計算へ進むのを防ぐ。

# 2026-08-10: ParticleLayer emitter JSON の数値を保存時に正規化

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::toJson()`。
- **確認できた事実:** 通常の `setParams()` 経路では emitter 値が正規化されるが、mutable params から `savedEmitterParams` に raw 値が残る可能性があり、toJson は rate、geometry、vector、opacity、physics、atlas、aux などを無加工で JSON 化していた。
- **変更:** 保存時に有限値・既存 runtime 範囲へ clamp し、enum／integer も対応する hard range に正規化した。推奨値ではなく persistence の安全境界を適用した。
- **価値:** 非有限値や許容外の emitter 設定が保存ファイルへ流出し、次回読込で状態を汚染する経路を閉じる。

- **追加確認:** emitter shape／mode enum も保存時に 0..7／0..2 へ正規化した。

# 2026-08-10: ParticleLayer effector JSON の保存範囲を同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::toJson()` effector serialization。
- **確認できた事実:** effector の読込側は type、strength、vector、radius、frequency、weights、kill zone size 等を clamp していたが、保存側は raw 値を JSON 化していた。
- **変更:** 読込側の許容範囲に合わせて effector 共通値と各型固有値を保存時に有限値・enum／integer 範囲へ正規化した。
- **価値:** mutable effector state から非有限値や許容外値が persistence へ流出する経路を閉じ、save/load の契約を対称化する。

# 2026-08-10: ParticleLayer JSON render settings の setter 経路を復元

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `applyPropertiesFromJson()` と `clearEffectors()`。
- **確認できた事実:** JSON 読込は render settings の各値を個別 clamp していたが、mutable reference へ直接代入して `ParticleSystem::setRenderSettings()` の共通正規化を迂回していた。clearEffectors には const_cast と null dereference 前提も残っていた。
- **変更:** settings を値で編集してから setter へ渡し、effectors は const view から null check 付きで clear するようにした。
- **価値:** JSON 読込と通常 setter の正規化契約を統一し、effectors 管理の不要な const_cast／null dereference を除去する。

# 2026-08-10: ParticleLayer emitter position scaling の const_cast を除去

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::Impl::scaleEmitterPositions()`。
- **確認できた事実:** この処理は emitter vector の追加・削除をせず、各 emitter の `setParams()` を呼ぶだけなのに、const view を mutable vector へ const_cast していた。
- **変更:** const な unique_ptr コンテナをそのまま走査し、所有対象の setter だけを呼ぶ形にした。
- **価値:** vector 所有権を変更せずに const-correct に更新でき、不要な const_cast とその保守リスクを除去する。

# 2026-08-10: ParticleLayer public effector API の入力範囲を統一

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `addForceEffector()`、`addVortexEffector()`、`addAttractorEffector()`、`addWindEffector()`。
- **確認できた事実:** JSON 読込と Turbulence API は effector 値を clamp していたが、他の public add API は position／direction／radius／strength を raw で格納していた。
- **変更:** 共通 helper で vector を有限・±1e6、radius／strength 等を JSON 読込と同じ範囲へ正規化した。
- **価値:** UI 以外の public API 経由でも、極端値が effector simulation へ直接流入しないようにする。

# 2026-08-10: ParticleLayer effector 追加時の frame cache 無効化

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の public effector add API。
- **確認できた事実:** addForce／Vortex／Turbulence／Attractor／Wind は effector を追加しても `clearFrameCache()` を呼んでいなかった。一方 clearEffectors と emitter property 更新は cache を無効化していた。
- **変更:** 各 effector 追加直後に frame cache を無効化した。
- **価値:** software fallback の同一 frame 表示が stale にならず、effector 追加が preview に反映される。

# 2026-08-10: ParticleLayer effector 追加時の saved emitter 同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の public effector add API と `Impl::savedEmitterParams`。
- **確認できた事実:** 空の layer で `firstEmitterOrCreate()` が emitter を生成しても、effector add API は saved params を再構築していなかった。結果として実体の emitter／effector と JSON 保存対象が不一致になり得た。
- **変更:** 各 effector 追加後に `rebuildSavedEmitterParamsFromSystem()` を呼ぶようにした。
- **価値:** effector を最初に追加した layer でも、project save／再読込で emitter と effector が失われないようにする。

# 2026-08-10: ParticleLayer JSON restore quota を有効 object 数で計数

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の emitter／effector JSON restore loops。
- **確認できた事実:** restore quota のカウンタを object 判定より前に increment していたため、非 object 配列要素が 1024 件上限を消費し、有効な後続要素が復元されない可能性があった。
- **変更:** object 判定後に quota を確認・increment するようにした。
- **追加確認:** effector は type 0..10 の検証後に increment し、未知 type も quota を消費しないようにした。
- **価値:** malformed JSON の無効要素が valid emitter／effector の復元を妨げないようにする。

# 2026-08-10: ParticleLayer timeScale metadata の上限を同期

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `particle.timeScale` property metadata と `setTimeScale()`。
- **確認できた事実:** setter／ParticleSystem は timeScale を有限・0..1000000 に clamp していたが、editor metadata に hard range がなかった。
- **変更:** property metadata に 0..1000000 の hard range を追加した。
- **価値:** UI／property cache から runtime 契約外の timeScale が投入される余地を減らす。

# 2026-08-10: ParticleLayer JSON 保存を live emitter snapshot 基準へ変更

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` の `ArtifactParticleLayer::toJson()` と `particleSystem()` 公開 API。
- **確認できた事実:** `particleSystem()` は mutable pointer を返すため外部から emitter params を変更できるが、toJson は `savedEmitterParams` キャッシュを保存していた。キャッシュ更新を伴わない変更が save から欠落し得た。
- **変更:** toJson 内で live emitter の params と effector を同じ serializable snapshot に収集し、それを保存するようにした。system がない場合だけ旧キャッシュへフォールバックする。
- **価値:** 外部 API 経由の編集も project save に反映され、emitter と effector の対応関係を保ったまま保存できる。

- **追加確認:** `particle.physics.turbulenceFrequency` は setter が 0..1000 に clamp していたため、metadata の hard range も 0..1000 に修正した。

# 2026-08-10: ParticleSystem software fallback の数値境界を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::updateAndRenderSoftwareFrame()`。
- **確認できた事実:** software path は emitter／particle の null・非有限値を前提にし、投影半径を直接 int 化し、`dx * dx` を int のまま計算していた。大きな値や mutable params 経由で overflow／不正な座標計算へ進む余地があった。
- **変更:** null／非有限 particle をスキップし、scale／stretch／半径／opacity／soft particle／trail width を有限値・既存上限へ正規化した。距離二乗は float 掛け算へ変更した。
- **価値:** QImage fallback が極端値でクラッシュ、巨大ループ、または不正な投影へ進む経路を局所的に閉じる。

# 2026-08-10: ParticleSystem QPainter fallback の stretch／clear 境界を統一

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::render(QPainter&)` と `clear()`。
- **確認できた事実:** QPainter fallback は finite particle を選別していたが、velocity stretch と render setting の積を上限なしで矩形サイズに使用していた。また clear は null emitter を直接 dereference していた。
- **変更:** stretch を 1..1e6 に clamp し、clear の emitter null check を追加した。
- **価値:** software fallback の巨大描画値と状態破損時の null dereference を抑え、別 fallback 入口でも同じ境界契約を保つ。

# 2026-08-10: ParticleSystem GPU vertex buffer の容量計算を防御

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::renderGPU()`。
- **確認できた事実:** `maxVertices` は粒子 quad の vertex 数として検査していたが、実際の float index は `vertexCount * 8` で計算していたため、極端な int 容量では index が overflow し得た。stretch も QPainter fallback と異なり上限なしだった。
- **変更:** writable vertex capacity を `INT_MAX / 8` 以下へ制限し、stretch を 1..1e6 に clamp した。
- **価値:** GPU vertex buffer への書き込み index の整数 overflow と、巨大 stretch による異常な頂点値を抑える。

# 2026-08-10: ParticleSystem lifecycle の null emitter 防御を統一

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` の `ParticleSystem::update()`、`reset()`、`removeEmitter(int)`。
- **確認できた事実:** 描画・capture 経路は null emitter をスキップしていた一方、update／reset は直接 dereference し、無効な slot の remove は null pointer を通知し得た。
- **変更:** update／reset は null をスキップし、removeEmitter(index) は null slot を no-op にした。
- **価値:** lifecycle と描画経路の状態破損時挙動を揃え、simulation 更新や observer 通知の null dereference を防ぐ。
### 2026-08-10: Particle burst count property input aligned with runtime bounds

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `setLayerPropertyValue()` / `particle.emitter.burstCount`
- **事実:** `ParticleEmitter::setParams()`、JSON 保存・復元、プロパティ metadata は `burstCount` を `0..10000000` として扱っていたが、プロパティ setter だけ `1..10000000` にしていた。
- **対応:** setter も下限を 0 に統一し、UI からバースト数 0（無効化）を設定できるようにした。
- **価値/懸念:** 入力経路間の範囲差を解消した。0 の意味が各 emission mode で同一かは未検証。
- **次に確認:** バーストモードで `burstCount == 0` の実行時挙動を、ビルド・テスト許可後に確認する。
### 2026-08-10: Particle atlas frame property input aligned with metadata bounds

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `setLayerPropertyValue()` / `startFrame`, `frameCount`
- **事実:** `startFrame` と `frameCount` は metadata、JSON 復元、`ParticleEmitter::setParams()` ではそれぞれ `0..1000000000`、`1..1000000` だが、property setter は下限だけを適用していた。
- **対応:** property setter にも同じ上限を適用した。
- **価値/懸念:** UI／JSON／runtime の atlas frame 範囲が統一された。既存保存データの再生互換性は維持される。
- **次に確認:** 極端な atlas frame 値での flipbook frame 選択を、ビルド・テスト許可後に確認する。
### 2026-08-10: Flocking invalid-vector isolation before particle integration

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `FlockingEffector::apply()`
- **事実:** Flocking は通常の `updateParticle()` より前の Phase 1a で実行されるため、無効な position／velocity を持つ粒子が `lengthSquared()`、近傍平均、加速度計算へ入ると NaN が他の粒子へ伝播し得た。
- **対応:** source と neighbor の position／velocity を component 単位で有限値確認し、無効な粒子を Flocking 計算から除外した。
- **価値/懸念:** 1 粒子の壊れた状態が flock 全体の加速度を汚染する経路を遮断した。無効粒子自体の最終処理は既存の粒子更新側に委ねる。
- **次に確認:** 異常値を含む Flocking 粒子群で加速度が有限に保たれることを、ビルド・テスト許可後に確認する。

補足: 有限な入力でも近傍加算の overflow で合成加速度が非有限になる可能性があるため、合成後にも有限性を確認し、無効な加速度をゼロへ正規化する。
### 2026-08-10: Particle QPainter render transform finite guard

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleSystem::render(QPainter&, const QTransform&)`
- **事実:** layer の GPU 変換経路には有限性確認がある一方、generator の public QPainter render 経路は受け取った `QTransform` をそのまま `QPainter::setTransform()` へ渡していた。
- **対応:** 行列要素と translation を確認し、非有限な行列は identity に置き換えてから描画するようにした。
- **価値/懸念:** 直接利用者を含む software/QPainter fallback の invalid transform 伝播を防ぐ。無効行列を identity とする既存 layer 側の扱いに合わせた。
- **次に確認:** 非有限 transform を渡した fallback render が描画を継続できることを、ビルド・テスト許可後に確認する。
### 2026-08-10: Self-collision grid skips non-finite particle positions

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleEmitter::applySelfCollisionBroadPhase()`
- **事実:** broad phase は距離計算時に非有限値を除外していたが、セル構築では非有限 position を `(0,0,0)` 相当のセルへ登録していた。
- **対応:** セル登録前に position の3成分を有限性確認し、無効粒子を grid から除外した。
- **価値/懸念:** 異常粒子が原点近傍の候補探索へ混ざる無駄をなくし、セル構築と衝突計算の境界を一致させた。無効粒子の寿命処理は既存 update path が担当する。
- **次に確認:** 非有限 position を含む self-collision frame で、他粒子の衝突結果が変わらないことをビルド・テスト許可後に確認する。
### 2026-08-10: Emitter inherited velocity finite guard for tiny delta

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleEmitter::update()`
- **事実:** 通常 update の emitter 移動量は `deltaPosition / deltaTime` で計算され、正だが極端に小さい delta では infinity になり得た。固定ステップ側の delta 制限だけでは public update 経路を覆えない。
- **対応:** 成分ごとに有限性を確認し、`±1000000` に clamp、非有限値は 0 に fallback するようにした。
- **価値/懸念:** emitter の inherited velocity が新規粒子へ非有限値として伝播する経路を遮断した。大きな移動は既存の runtime 速度上限に合わせて飽和する。
- **次に確認:** 極小 delta と急な emitter 移動を組み合わせた生成で速度が有限に保たれることを、ビルド・テスト許可後に確認する。
### 2026-08-10: Particle goToFrame simulation time cap aligned with render time

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleSystem::goToFrame()`
- **事実:** layer の `safeParticleFrameTime()` は frame 時刻を 1,000,000 秒へ clamp していたが、`goToFrame()` は巨大 frame／極小 fps の target time をそのまま 120Hz ループへ渡していた。
- **対応:** `goToFrame()` の target time も最大 1,000,000 秒に clamp した。
- **価値/懸念:** render 経路内の seek が極端な入力で長時間ブロックするリスクを除去し、既存の frame-time fallback と上限を統一した。上限超過分の simulation は意図的に省略される。
- **次に確認:** 巨大 frame と通常 frame の seek が有限時間で完了し、通常範囲の結果が変わらないことをビルド・テスト許可後に確認する。
### 2026-08-10: Particle clearEmitters now reports aggregate state change

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `ArtifactParticleLayer::clearEmitters()`
- **事実:** clear は emitter と saved params、frame cache を消去していたが、既存の `particleSystemChanged`／`changed` を通知していなかった。JSON の空 `emitters` 配列や preset 切替では、後続 add がない場合に UI／dirty state が更新されない。
- **対応:** 新しいイベント経路は追加せず、既存シグナルを clear 完了後に発火するようにした。
- **価値/懸念:** 空状態への復元・切替でも property UI と変更状態が同期する。preset が続けて emitter を追加する場合は既存の通知に加えて clear 通知が発生する。
- **次に確認:** 空 emitters JSON 復元と preset 切替で UI／dirty state が更新されることを、ビルド・テスト許可後に確認する。
### 2026-08-10: Particle clearEffectors synchronizes persistence and change state

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `ArtifactParticleLayer::clearEffectors()`
- **事実:** clearEffectors は全 emitter の effectors を消していたが、saved emitter params を再構築せず、既存の変更通知も発火していなかった。live snapshot 保存で通常 JSON は保てるものの、fallback 状態と UI／dirty state が不整合になり得た。
- **対応:** clear 後に saved params を再構築し、既存の `particleSystemChanged`／`changed` を通知するようにした。
- **価値/懸念:** effector 一括削除が通常編集と同じ persistence・通知契約を満たす。空 emitter の場合も aggregate change 通知は発生する。
- **次に確認:** effector 削除後の保存・再読込と UI 更新を、ビルド・テスト許可後に確認する。
### 2026-08-10: Particle system recreation reports layer dirty state

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `ArtifactParticleLayer::createParticleSystem()`
- **事実:** system 再生成と default emitter 作成後に `particleSystemChanged` は通知していたが、layer の変更通知 `changed` がなかった。
- **対応:** 再生成完了後に既存の `changed` を発火し、保存対象の状態変更として扱うようにした。
- **価値/懸念:** system 再作成後の dirty state が UI／保存経路へ伝わる。初期化時に呼ばれる場合は既存 lifecycle の通知契約に従って dirty 扱いになる。
- **次に確認:** system 再生成後の save prompt と property UI 同期を、ビルド・テスト許可後に確認する。
### 2026-08-10: JSON particle restore avoids dirty notification from public clear

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `applyPropertiesFromJson()` の emitter 復元
- **事実:** `clearEmitters()` に aggregate `changed` 通知を追加した後、JSON 復元がその public clear を呼ぶと、ロード中の状態置換が interactive edit として dirty 扱いになる。
- **対応:** JSON 復元では particle system と saved params を内部的に直接クリアし、cache だけを無効化する。公開 `clearEmitters()` の通知契約は維持した。
- **価値/懸念:** ドキュメント復元直後に不要な dirty state を作らず、空／不正 emitter 配列も従来どおり空状態へ置換できる。ロード完了時の全体通知は既存の上位ロード契約に依存する。
- **次に確認:** particle JSON のロード直後に dirty state が立たず、空配列復元でも property UI が空になることをビルド・テスト許可後に確認する。
### 2026-08-10: Unsupported particle effector types no longer consume restore quota

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `applyPropertiesFromJson()` の effector restore quota
- **事実:** `EffectorType` には Drag／Noise／Collision があるが、現行 restore factory では対応 class がなく skip される。quota counter は factory 成功前に増えていたため、未対応要素が後続の有効 effector の復元枠を消費していた。
- **対応:** effector instance の生成に成功した後でのみ `restoredEffectorCount` を increment するようにした。
- **価値/懸念:** 不明／未対応 effector が大量に含まれる JSON でも、有効な後続 effector を上限まで復元できる。未対応 type 自体は引き続き skip される。
- **次に確認:** 未対応 type と有効 type を混在させた JSON の復元で、有効 effector 数が quota どおりになることをビルド・テスト許可後に確認する。
### 2026-08-10: Particle emitter setter normalizes direct enum inputs

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` / `ParticleEmitter::setParams()`
- **事実:** JSON／property setter は shape／mode／auxTrigger を範囲化していたが、public `setParams()` は enum を直接コピーしていた。
- **対応:** setter 入口で `EmitterShape` `0..7`、`EmissionMode` `0..2`、`AuxTriggerMode` `0..2` に正規化した。
- **価値/懸念:** 直接 API 利用でも invalid enum が switch／aux trigger へ伝播しない。既存の有効 enum の挙動は変わらない。
- **次に確認:** invalid underlying enum を含む `EmitterParams` を直接渡した場合も、保存・描画・更新が有効な default mode で継続することをビルド・テスト許可後に確認する。
### 2026-08-10: Particle effector type gets a safe base default

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx` / `ParticleEffector::type`
- **事実:** base effector の `type` は未初期化で、既存の built-in 派生 class は constructor で設定するが、外部／将来の派生 class が設定し忘れると JSON serialize 時の enum 読み出しが未定義値になり得た。
- **対応:** base field の default を `EffectorType::Force` に設定した。
- **価値/懸念:** custom effector の type 設定漏れでも未初期化読み出しを防ぐ。custom effector は引き続き、自身の type を明示的に設定する責務を持つ。
- **次に確認:** type 未設定の custom effector を serialize した場合に安全な値で保存されることを、ビルド・テスト許可後に確認する。
### 2026-08-10: Particle emitter clear resets deterministic particle IDs

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleEmitter::clear()`
- **事実:** clear は粒子、時間、accumulator、乱数 seed、transform state を初期化していたが、`nextParticleId_` は維持していた。そのため同じ seed で reset／seek しても particle notification の ID 列だけが変化した。
- **対応:** clear 時に `nextParticleId_` を 0 へ戻すようにした。
- **価値/懸念:** reset／goToFrame の再現性を visual simulation だけでなく particle ID にも広げた。clear 後の古い ID と新しい ID の一意性を跨いで保証する用途は想定していない。
- **次に確認:** 同一 seed の reset／seek で emitted／died ID 列が一致することを、ビルド・テスト許可後に確認する。
### 2026-08-10: Particle emitter removal keeps signal pointer alive

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleSystem::removeEmitter()` の pointer／index overload
- **事実:** 旧実装は vector から unique_ptr を erase して emitter を破棄した後、破棄済み raw pointer を `emitterRemoved` に渡していた。
- **対応:** 対象 unique_ptr を local に move して vector から除去し、signal 通知が完了するまで local ownership で emitter を生存させるようにした。
- **価値/懸念:** signal receiver が通知 payload を読む時点の dangling pointer を解消した。通知後は local ownership が解放されるため、pointer の長期保持は従来どおり許容されない。
- **次に確認:** pointer／index 両 overload の signal receiver が通知中に安全に emitter を参照でき、通知後に system から消えていることをビルド・テスト許可後に確認する。
### 2026-08-10: Turbulence effector octaves serialization avoids unsafe float-to-int cast

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `toJson()` の Turbulence effector
- **事実:** `TurbulenceEffector::octaves` は float だが、保存時に `safeEmitterInt()` へ暗黙に int 化して渡していた。public mutable field が NaN／巨大値の場合、cast 前の安全確認がなかった。
- **対応:** 有限性を確認し、double の `1..12` clamp 後に int 化し、非有限値は 3 に fallback するようにした。
- **価値/懸念:** 不正な turbulence 設定でも JSON 保存が未定義動作にならない。既存の有効 octaves の保存値は変わらない。
- **次に確認:** NaN／巨大 octaves の保存と再読込で値が `3`／`12` に収まることをビルド・テスト許可後に確認する。
### 2026-08-10: GPU particle vertices clamp extreme finite positions

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleSystem::renderGPU()`
- **事実:** GPU path は position の有限性だけを確認していた。float 最大値近傍の有限 position に quad corner offset を加えると、vertex buffer へ infinity を書き得た。core render conversion には既に `±10000000` の座標 clamp がある。
- **対応:** GPU vertex 生成でも position X/Y を `±10000000` に clamp してから quad 計算へ使うようにした。
- **価値/懸念:** GPU buffer に非有限座標が入る経路を core path と同じ座標契約へ揃えた。極端な座標は画面外の安全な有限座標へ飽和する。
- **次に確認:** float 最大値近傍の particle が GPU buffer に有限 vertex のみを書き込むことをビルド・テスト許可後に確認する。
### 2026-08-10: Software particle projection guards non-finite pixel casts

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleSystem::updateAndRenderSoftwareFrame()`
- **事実:** finite particle position でも投影 `sx/sy` は overflow し得て、旧 code はそのまま `round()` と int cast を行っていた。
- **対応:** 投影値の有限性を確認して異常値を skip し、有限値も int の上下限から半径余白を引いた範囲へ clamp してから pixel cast するようにした。
- **価値/懸念:** 極端な座標での undefined int conversion と `px ± radius` overflow を防いだ。画面外粒子は既存の min/max 判定で描画対象外になる。
- **次に確認:** float 最大値近傍の particle を software fallback へ渡しても frame が生成でき、pixel index が安全範囲に収まることをビルド・テスト許可後に確認する。
### 2026-08-10: Software particle frame dimensions share preview allocation bounds

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleSystem::updateAndRenderSoftwareFrame()`
- **事実:** layer の preview dimensions は `1..16384` に制限されているが、public software frame API は正の int を無制限に `QImage` dimensions として受け取っていた。
- **対応:** frame 生成前に width／height を `1..16384` へ clamp するようにした。
- **価値/懸念:** 巨大 dimensions による過大 allocation／frame 生成失敗のリスクを preview 経路と統一した。要求値が上限を超える場合は有限な最大 preview へ飽和する。
- **次に確認:** 巨大 dimensions の入力で frame が安全に生成され、通常 dimensions の出力が変わらないことをビルド・テスト許可後に確認する。
### 2026-08-10: Particle capture reserve is bounded independently of particle count

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm` / `ParticleSystem::captureRenderData()`
- **事実:** capture は `totalParticleCount()`（int 最大まで飽和）をそのまま vector reserve に使っていた。非有限／dead particle を後段で除外する場合でも、先に巨大 allocation を試みる経路があった。
- **対応:** 初期 reserve を 1,000,000 件までに制限し、実際の有効 particle は従来どおり vector へ追加するようにした。
- **価値/懸念:** 異常な particle 数や多数 emitter での即時 bad allocation リスクを抑えた。通常の小規模 capture の reserve 挙動は変わらない。
- **次に確認:** 大量／無効 particle を含む capture で frame が生成でき、通常粒子の出力件数が変わらないことをビルド・テスト許可後に確認する。
### 2026-08-10: Particle renderToImage resets on backward time seeks

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm` / `ArtifactParticleLayer::renderToImage(QImage&, float)`
- **事実:** `goToFrame()` は時間が戻ると particle system を reset していたが、renderToImage の float-time overload は負の delta を無視して現在の未来 state をそのまま描画していた。
- **対応:** playing 中に要求 time が `lastTime` より前なら既存 `reset()` を呼び、そこから再度 forward update するようにした。
- **価値/懸念:** frame render／fallback の backward seek でも過去時刻の state が再現される。paused 中は従来どおり simulation を進めない。
- **次に確認:** 時刻 `t2` の後に `t1 < t2` を renderToImage した場合、`t1` の画像が reset 後の再シミュレーションと一致することをビルド・テスト許可後に確認する。
### 2026-08-10: Solid layer direct size setters align with init bounds

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm` / `setSize()`
- **事実:** `ArtifactSolidLayerInitParams` は width／height を `1..16384` に制限していたが、solid layer の直接 setter は `max(1, value)` のみで、JSON／public API から巨大 source size を作れた。
- **対応:** Solid2D／SolidImage の両 `setSize()` を `1..16384` clamp に統一した。
- **価値/懸念:** solid source／cache の過大 allocation リスクを init／preview の既存契約と揃えた。通常サイズの挙動は変わらない。
- **次に確認:** 巨大 solid size の JSON 復元と直接 setter が 16384 に収まり、通常 solid の描画が変わらないことをビルド・テスト許可後に確認する。
## 2026-08-10: Solid color setter normalization

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`, `Artifact/src/Layer/ArtifactSolidImageLayer.cppm`
- **事実:** gradient colors already normalized non-finite/channel values, while the regular solid color setters accepted raw `FloatColor` values. The image-layer path also copied that raw value into an animatable keyframe and QColor conversion.
- **修正:** both regular color setters now normalize channels to finite `0..1` values, using RGB fallback `0` and alpha fallback `1`, before storing or updating the property.
- **価値:** direct API, animation, JSON, and preview paths now share the same color safety contract without adding a new event path.
- **未検証:** runtime rendering and animation playback still require the repository's explicit build/test step.
### 2026-08-10 — Solid 描画入口でも source size を防御的に制限

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm`
- **事実:** 固有の `setSize()` と一般プロパティ経路は `1..16384` に制限されているが、基底の protected `setSourceSize()` は raw 値を保持し、Solid の `draw()`／`toQImage()` はそれを直接 `QImage`／gradient image の寸法に使っていた。
- **修正:** 両 Solid の描画・画像化入口で source width/height を `1..16384` に clamp してから downstream の画像生成へ渡す。
- **価値:** 通常の UI／JSON 経路外から不正または過大な source size が入っても、Solid の画像 allocation が無制限に拡大しない。
- **未検証:** 実際の描画結果と大寸法入力時の runtime 挙動は、ビルド・テスト許可後に確認する。
### 2026-08-10 — 共通 gradient utility に最終入力境界を追加

- **関連:** `Artifact/include/Layer/ArtifactSolidGradientUtil.hpp`
- **事実:** Solid 層側の setter は多くの値を正規化していたが、共通 `makeSolidGradientImage()` 自体は QSize、fill type、angle、scale、offset を raw 値で受け、NaN／無限大や過大寸法を内部の QImage／gradient 計算へ渡し得た。
- **修正:** utility 内で寸法を `1..16384`、fill type を `0..5`、scale／offset を有限の bounded range、center と alpha scale を有限値として正規化し、全計算を safe 値へ統一した。
- **価値:** Solid 2D／Solid Image の共有描画経路が呼び出し側の防御に依存せず、直接 utility 呼び出し時も同じ安全契約を持つ。
- **未検証:** gradient の各 fill type における実描画 parity はビルド・runtime 確認が必要。
### 2026-08-10 — thumbnail 共通入口の寸法上限を Solid 経路と統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm`
- **事実:** Solid 2D は基底 `ArtifactAbstractLayer::getThumbnail()` を使い、基底と Solid Image override は要求値を `max(1, ...)` のみで `QImage` allocation／scaled に渡していた。一方、Solid source image と gradient utility の上限は `1..16384` だった。
- **修正:** 基底 thumbnail と Solid Image override の width/height を `1..16384` に clamp した。これにより Solid 2D を含む基底経路も同じ契約になる。
- **価値:** 異常に大きい thumbnail 要求による共通 UI の一時画像 allocation を抑え、Solid の寸法境界を入口から末端まで揃えた。
- **未検証:** 他の専用 thumbnail override（Image／Video 等）の要求上限は今回の範囲外で、runtime の呼び出し契約確認が必要。
### 2026-08-10 — opacity getter の評価出口を有限・範囲内へ統一

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、Solid 2D／Solid Image の描画経路
- **事実:** `setOpacity()` は入力を正規化していたが、`opacity()` は variant override、animated property、animation layer、effect envelope の評価結果をそのまま返していた。Solid 描画はこの値を色 alpha と乗算する。
- **修正:** getter の最終評価値を有限値として `0..1` に clamp し、非有限値は `1.0` に fallback した。
- **価値:** Solid を含む全レイヤーの opacity 消費側が、評価経路の違いに関係なく同じ安全契約を受け取れる。
- **未検証:** variant／animation／effect envelope の組み合わせによる実描画 parity は runtime 確認が必要。
### 2026-08-10 — Solid JSON 出力でも source size を正規化

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm`
- **事実:** factory／固有 `setSize()`／描画入口は `1..16384` を守る一方、raw `setSourceSize()` が使われた場合、Solid の `toJson()` は `solidWidth`／`solidHeight` を未制限のまま保存していた。
- **修正:** JSON 出力値を `1..16384` に clamp して、描画・復元・保存の寸法契約を統一した。
- **価値:** 不正または過大な内部サイズが保存ファイルへ再流出せず、次回ロード時の補正に依存しない canonical な Solid JSON になる。
- **未検証:** 既存プロジェクトの round-trip 実行はビルド・runtime 確認が必要。
### 2026-08-10 — Solid property metadata を runtime の gradient 契約へ同期

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm`
- **事実:** 両 Solid の property editor setter は fill type／center／scale／offset を正規化していたが、property metadata 側に hard range がなく、入力 UI は内部補正へ依存していた。
- **修正:** fill type `0..5`、center X/Y `0..1`、scale `0.0001..1000000`、offset `-1000000..1000000` を両実装の hard range に追加した。角度の自由度は維持した。
- **価値:** UI 入力、setter、共通 gradient utility の契約が揃い、無効値の入力を編集段階から減らせる。
- **未検証:** 実際の property editor のスピン／ドラッグ UI 表示は runtime 確認が必要。
### 2026-08-10 — 静止画 thumbnail override の寸法上限を共通契約へ統一

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`
- **事実:** 静止画 decode は既に width/height、総 pixel 数、channel 数を検証し、crop canvas も decoded image の矩形へ交差していた。一方、専用 `getThumbnail()` は要求値を `max(1, ...)` のみで `QImage::scaled()` に渡していた。
- **修正:** thumbnail target width/height を `1..16384` に clamp した。
- **価値:** base／Solid／静止画 thumbnail が同じ allocation 境界を持ち、異常に大きい UI 要求で一時画像だけが膨らむ経路を塞いだ。
- **未検証:** Image／Video 系の専用 thumbnail override 全体の runtime メモリ挙動は今回の範囲外。
### 2026-08-10 — Shape の独自色変換と破線 stroke helper を最終防衛

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- **事実:** Shape の setter／restore は多くの値を正規化していたが、独自 `toQColor()` は NaN に対して `std::clamp()` のみを使い、破線 native stroke helper も width の有限値を確認せず処理していた。
- **修正:** 色チャンネルを有限値・`0..1`（RGB fallback `0`、alpha fallback `1`）へ正規化し、破線 width は有限かつ正の場合だけ描画するようにした。
- **価値:** Shape の painter／native stroke 両経路で invalid FloatColor と非有限 stroke width が描画計算へ流れない。
- **未検証:** Shape の各 operator／stroke style の実描画 parity は runtime 確認が必要。
### 2026-08-10 — Shape thumbnail override を寸法契約へ同期

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- **事実:** Shape の `setSize()` と software cache は `1..16384` の寸法を使う一方、専用 `getThumbnail()` は `max(1, ...)` のみで要求値を `scaled()` へ渡していた。
- **修正:** thumbnail target width/height を `1..16384` に clamp した。
- **価値:** Shape も base／Solid／Image と同じ thumbnail allocation 境界を持ち、過大な UI 要求による一時画像生成を抑える。
- **未検証:** Shape operator／stroke style を含む実 thumbnail parity は runtime 確認が必要。
### 2026-08-10 — Shape gradient radius setter を property hard range と同期

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- **事実:** `shape.fillGradientRadius` の property metadata は hard range `0..100000` だったが、公開 `setFillGradientRadius()` は有限値なら下限だけを適用し、直接 API／JSON 経由では上限を bypass できた。
- **修正:** setter も `0..100000` に clamp し、非有限値は既存どおり `0.5` に fallback する。
- **価値:** gradient radius の UI・setter・cache rebuild が同じ範囲契約を持ち、過大な gradient geometry を抑える。
- **未検証:** 極端な radius の実描画品質は runtime 確認が必要。
### 2026-08-10 — Shape enum／angle／stroke metadata と setter の範囲を整合

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- **事実:** gradient angle の metadata は `-360..360` だが setter は任意の有限値を受け、stroke width の metadata `0..100000` は setter の実上限 `0..16384` と不一致だった。shape/fill enum の metadata hard range もなく、fill tooltip は `0..3` までしか説明していなかった。
- **修正:** angle setter を `-360..360` に clamp、stroke width metadata を `0..16384` に修正し、shape type `0..6`／fill type `0..5` の hard range と全 fill mode の tooltip を追加した。
- **価値:** property editor と直接 API の値域が一致し、enum の入力漏れと silently-reduced な stroke width を減らす。
- **未検証:** property editor の enum widget 表示と極端な stroke width の runtime 見た目は未確認。
### 2026-08-10 — 静止画 float decode の非有限 pixel を decode 境界で正規化

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`
- **事実:** OIIO／derived cache の float pixel 配列は寸法検証後に `ImageF32x4_RGBA::setFromRGBA32F()` へ渡されるが、Core の setter は NaN／無限大を clone 時に除去しない。finite な HDR 値は保持される契約だった。
- **修正:** OIIO、async reader、derived cache の3つの decode 境界で非有限 channel だけを RGB=`0`、alpha=`1` に置換してから float buffer を構築する。
- **価値:** invalid float が color transform、thumbnail、GPU／software render へ伝播するのを防ぎつつ、finite な HDR intensity は clamp しない。
- **未検証:** 異常 pixel を含む画像の実 decode／render 挙動は runtime 確認が必要。
### 2026-08-10 — 静止画 source metadata の保存境界を decode 契約へ同期

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`
- **事実:** 実デコードは channel 数を `1..64` に制限している一方、metadata の JSON 復元は `0..1024` を許し、`toJson()` は保持値をそのまま出力していた。OIIO の ICC `datasize()` も `size_t` から `int` へ直接変換されていた。
- **修正:** channel 数、alpha index、orientation、pixel aspect、bits per channel、ICC bytes、文字列、channel name 配列を保存・復元・OIIO spec 生成の境界で正規化し、ICC サイズは `256 MiB` 上限とした。
- **価値:** metadata の round-trip 後だけ実デコード契約を超える状態が残る経路と、異常な spec サイズの整数変換リスクを減らす。
- **未検証:** 異常 metadata JSON／巨大 ICC 属性を実際に読み書きする runtime 挙動は未確認。
### 2026-08-10 — 静止画 JSON の保存寸法を decode 上限へ canonical 化

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm` / `ArtifactImageLayer::toJson()`
- **事実:** decode／`setFromQImage()` は source dimensions を `1..16384` に検証しているが、保存処理は内部 `width_`／`height_` をそのまま JSON 化していた。source 欠損時の `0` は有効な空状態として残る。
- **修正:** JSON 出力時の width／height を `0..16384` に clamp し、通常画像の値と空状態を維持しながら異常な内部値の再流出を防いだ。
- **価値:** 静止画の decode、runtime state、保存ファイルの寸法契約が揃い、次回復元時の過大 source size を減らす。
- **未検証:** 壊れた内部 state を作る runtime 経路と既存 project の round-trip は未確認。
### 2026-08-10 — SourceCrop の公開 geometry API で非有限 source size を拒否

- **関連:** `Artifact/src/Layer/ArtifactSourceCrop.cppm` / `hasSourceSize()`
- **事実:** ImageLayer から渡る通常の source/output size は有限な画像寸法だが、`effectiveCropRect()` と `sourceToOutputTransform()` は公開 API として任意の `QSizeF` を受け取る。旧判定は正値だけを確認し、`+∞` を source size として通す余地があった。
- **修正:** source size の width／height が有限かつ正であることを共通判定に追加した。
- **価値:** 無限大の矩形、scale、transform が crop 計算へ流れず、呼び出し側が ImageLayer 以外でも有限 geometry 契約を共有できる。
- **未検証:** 異常な `QSizeF` を直接渡す公開 API の runtime 挙動は未確認。
### 2026-08-10 — Solid gradient scale の setter 上限を utility／property と同期

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm`、`Artifact/src/Layer/ArtifactLayerInitParams.cppm`
- **事実:** 共通 gradient utility と property metadata は scale を `0.0001..1000000` としていたが、両 Solid の setter と init params は有限値なら上限なく保持していた。factory は init params の値を Solid setter へ転送する。
- **修正:** 3つの setter を有限値の `0.0001..1000000` clamp に統一し、非有限値は従来どおり `1.0` に fallback する。
- **価値:** UI、init params、factory、描画 utility が同じ scale 契約になり、過大な gradient span や cache key の不一致を抑える。
- **未検証:** 極端な scale の描画 parity と既存 project の round-trip は未確認。
### 2026-08-10 — Solid gradient center／offset の setter 範囲を同期

- **関連:** `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`Artifact/src/Layer/ArtifactSolidImageLayer.cppm`、`Artifact/src/Layer/ArtifactLayerInitParams.cppm`
- **事実:** center X/Y の metadata は `0..1`、offset の metadata／共通 utility は `-1000000..1000000` だが、3つの setter 群は有限値なら範囲外を保持していた。描画時 utility の clamp と cache／property の保持値が一致しなかった。
- **修正:** center X/Y と offset を各 setter／init params で同じ範囲へ clamp し、非有限値の既存 fallback も維持した。
- **価値:** direct API、JSON、factory、property editor、gradient utility が同一の geometry 契約になり、保存値と描画値の乖離を減らす。
- **未検証:** 範囲端の gradient 見た目と既存 JSON の round-trip は未確認。
### 2026-08-10 — Shape polygon sides の property hard range を setter と同期

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` / `shape.polygonSides`
- **事実:** setter は polygon sides を `3..kMaxShapePathVertices`（現在100000）へ clamp していたが、property metadata に hard range がなく、UI入力は setter の補正に依存していた。
- **修正:** property metadata に `3..100000` の hard range を追加した。
- **価値:** Shape の polygon sides で UI、直接 API、JSON 復元の入力境界が揃い、過大な頂点数の入力を編集段階で抑えられる。
- **未検証:** property editor の integer widget 表示と上限付近の runtime 描画は未確認。
### 2026-08-10 — Shape dash pattern parser を setter と同じ境界へ前倒し

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` / `stringToDashPattern()`
- **事実:** setter は dash pattern を最大1024要素、有限値、`kMaxShapeDimension` 以下へ正規化していたが、文字列 parser は全要素を分割し、巨大な double を float 化してから setter に渡していた。
- **修正:** parser でも1024要素で打ち切り、double の有限性を確認してから寸法上限へ clamp し、float 化するようにした。
- **価値:** プロパティ文字列・JSON 復元の早い段階で不要な巨大配列と非有限値を除外し、setter と同じ dash 契約を共有する。
- **未検証:** 1024要素境界、指数表記、極端な dash 値の native／software 描画 parity は未確認。
### 2026-08-10 — Shape Trim Paths の property 入力を restore 契約へ同期

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` / `TrimPaths` operator property path
- **事実:** restore 正規化は Trim Paths の start／end／offset を有限値・`-100000..100000` に clamp していたが、property setter は `QVariant::toFloat()` の結果を無加工で Core operator へ渡し、metadata に hard range もなかった。
- **修正:** 3 property に hard range を追加し、編集時も有限値・同一範囲・既存 fallback（start/offset=0、end=100）を適用した。
- **価値:** 保存復元とインスペクタ編集で Trim Paths の状態契約が揃い、NaN／無限大や過大値が operator geometry へ流れにくくなる。
- **未検証:** Trim Paths の各 mode における範囲端の描画 parity は未確認。
### 2026-08-10 — Shape Repeater の offset／rotation property を restore 契約へ同期

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` / `Repeater` operator property path
- **事実:** restore 正規化は Repeater offset を `-100000..100000`、rotation を `-360000..360000` の有限値へ clamp していたが、property metadata と編集経路は raw float を Core setter へ渡していた。
- **修正:** 2 property に hard range を追加し、編集時も同じ範囲と非有限 fallback `0` を適用した。
- **価値:** Repeater の保存復元とインスペクタ編集で、transform offset／rotation の値域が一致する。
- **未検証:** 極端な offset／rotation の repeater 描画と property widget 表示は未確認。
### 2026-08-10 — Shape Repeater の copies／opacity metadata を setter と同期

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` / `Repeater` operator property group
- **事実:** property setter／restore は copies を `1..1000`、start/end opacity を `0..100` に clamp していたが、property metadata に hard range がなかった。
- **修正:** copies、start opacity、end opacity にそれぞれ setter と同じ hard range を追加した。
- **価値:** Repeater の主要な数値入力が UI段階から同じ境界を持ち、無効値を後段補正へ流す量を減らす。
- **未検証:** property editor の integer／float widget 表示と上限値の runtime 表現は未確認。
### 2026-08-10 — Shape Offset／Pucker／Rounded operator の property 入力を restore 契約へ同期

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` / `OffsetPaths`、`PuckerBloat`、`RoundedCorners`
- **事実:** restore 正規化は Offset／Pucker amount を `-100000..100000`、Rounded radius を `0..100000` に clamp していたが、property metadata と編集経路は raw float を渡していた。
- **修正:** 3 property に hard range を追加し、property 編集時も有限値・同一範囲・fallback `0` を適用した。
- **価値:** operator の保存復元とインスペクタ編集で geometry 値域が一致し、非有限／過大値が Core path へ流れる経路を減らす。
- **未検証:** 各 operator の範囲端における native／software path の描画 parity は未確認。
### 2026-08-10 — Shape procedural operator metadata を property setter と同期

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` / Wiggle Paths、Zig Zag、Hand Drawn Wobble
- **事実:** property 編集経路は既に amount／frequency／jitter／probability を有限値と既定範囲へ clamp していたが、operator property metadata に hard range がなかった。
- **修正:** Wiggle／Zig Zag の amount `-100000..100000`、frequency `0..10000`、Wobble amount `0..100000`、frequency `0..10000`、jitter／probability `0..1` を metadata に追加した。
- **価値:** procedural operator の UI入力が既存 setter の安全契約と一致し、後段補正への依存を減らす。
- **未検証:** 各 operator の境界値による runtime geometry は未確認。
### 2026-08-10 — Image factory が単一 sequence path を代表画像として復元

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm`、`ArtifactImageInitParams`、`ArtifactImageLayer::setImageSequence()`
- **事実:** init params は sequence paths を1件以上保持でき、ImageLayer の sequence API も1件を受け入れる。一方 factory は paths が2件未満の場合に `imagePath()` だけを参照していたため、sequencePaths だけに1件を設定した生成ではロード対象が空になっていた。
- **修正:** imagePath が空で sequencePaths が1件の場合、その唯一の sequence path を代表画像の load path として使うようにした。
- **価値:** 初期化 API の指定方法にかかわらず、単一フレームの image layer が生成される。2件以上の sequence と明示 imagePath の既存挙動は維持する。
- **未検証:** factory 経由の単一 sequence path の実ロードと source identity は runtime 確認が必要。
### 2026-08-10 — Layer factory の createNewLayer で init params slicing を解消

- **関連:** `Artifact/include/Layer/ArtifactLayerFactory.ixx`、`Artifact/src/Layer/ArtifactLayerFactory.cppm`
- **事実:** `createNewLayer(ArtifactLayerInitParams params)` は base 値渡しだったため、derived init params を渡してもコピー時に slicing され、Solid／Image などの dynamic_cast による固有設定転送が失敗していた。既存の呼び出し側はすべて lvalue params を渡している。
- **修正:** public／Impl の `createNewLayer` を `ArtifactLayerInitParams&` 参照渡しへ変更し、derived params の実型を保持したまま既存 `createLayer()` へ渡すようにした。
- **価値:** Solid の gradient／size、Image の path／sequence／color interpretation など、init params 由来の生成設定が factory 経由でも実際の layer へ届く。
- **未検証:** derived init params を使った各 layer の factory runtime 生成は未確認。module interface 変更の再スキャン影響もビルド未実行のため未確認。
### 2026-08-10 — Layer factory の params 参照を const 化して const_cast 経路を縮小

- **関連:** `Artifact/include/Layer/ArtifactLayerFactory.ixx`、`Artifact/src/Layer/ArtifactLayerFactory.cppm`
- **事実:** factory は init params を読み取るだけなのに `createNewLayer`／`createLayer` が非const参照で、const params を受ける上位サービス側に `const_cast` が必要な経路があった。derived dynamic_cast は前周の slicing 修正後も const 対応が必要だった。
- **修正:** public／Impl の両 factory API を `const ArtifactLayerInitParams&` に変更し、derived dynamic_cast も const pointer へ揃えた。lvalue と temporary の両方で実型を保持できる。
- **価値:** factory API の const 契約が実装と一致し、読み取り専用 params の不必要な可変化を減らす。
- **未検証:** 上位サービスの const_cast 撤去範囲と module 再スキャン、factory の全 derived runtime 生成は未確認。
### 2026-08-10 — ArtifactPr MediaPanel の検索ウィジェット宣言漏れ

- **関連:** `ArtifactPr/include/MediaPanel.ixx`、`ArtifactPr/src/MediaPanel.cppm`
- **事実:** 実装は `searchEdit_` を生成・接続していたが、インターフェース側のメンバー宣言と直接 include が不足していた。
- **修正:** `QLineEdit` の include と `searchEdit_` メンバーを追加した。
- **追加修正:** 既存接続の対象だった `applySearchFilter` を実装し、検索欄をレイアウトへ追加した。シーケンス更新・インポート後も現在の検索語を再適用する。
- **価値:** MediaPanel の検索 UI が宣言・実装間で一致し、未宣言メンバー参照によるビルド失敗を防ぐ。
- **未検証:** ArtifactPr のビルドと検索フィルタの runtime 動作は未確認。
### 2026-08-10 — ArtifactPr thumbnail cache の保存経路を復旧

- **関連:** `ArtifactPr/include/MediaThumbnailer.ixx`、`ArtifactPr/src/MediaThumbnailer.cppm`
- **事実:** `request()` は `cache_` を参照していたが、ワーカーが生成した有効な thumbnail を `cache_` に格納する処理がなく、同一ファイルの再要求が常にデコードへ進んでいた。
- **修正:** ワーカー結果を GUI スレッドへ queued invoke で戻し、`publishThumbnail()` で cache 保存後に既存の `thumbnailReady` を発行するようにした。ワーカー間の新規 signal 接続は追加していない。
- **価値:** MediaPanel の再表示・重複要求で同じメディアを再デコードする負荷を抑え、既存の `cached()` 契約を実際に機能させる。
- **未検証:** ArtifactPr のビルド、複数 thumbnail の到着順、終了直前の queued callback は runtime 確認が必要。
### 2026-08-10 — ArtifactPr MarkerEditDialog の Qt 型依存を自己完結化

- **関連:** `ArtifactPr/include/MarkerEditDialog.ixx`
- **事実:** インターフェースのメンバー宣言が `QLineEdit`、`QPlainTextEdit`、`QComboBox`、`QPushButton`、`QSpinBox` を使用していたが、直接 include は `QColor`／`QDialog`／`QString` だけだった。
- **修正:** メンバー宣言に必要な Qt ヘッダをインターフェースへ追加した。
- **価値:** 他のモジュール経由の偶然の可視性に依存せず、MarkerEditDialog の公開宣言を単独で解析できる。
- **未検証:** ArtifactPr のモジュールスキャンとビルドは未実行。
### 2026-08-10 — ArtifactPr ExportDialog の画像形式変更時拡張子を置換

- **関連:** `ArtifactPr/src/ExportDialog.cppm`、形式コンボボックスの更新処理
- **事実:** PNG／JPEG 選択時、既存拡張子があっても新しい拡張子を単純追加していたため、`output.mp4.png` のようなパスになり得た。
- **修正:** PNG／JPEG の画像シーケンスに限り、パス末尾の既存拡張子を選択形式の拡張子へ置換するようにした。ディレクトリ名中のドットは拡張子判定から除外する。WAV／MP3 分岐は変更していない。
- **価値:** 形式変更時に出力先が予測可能になり、手動で拡張子を直す必要を減らす。
- **未検証:** Windows／UNCパスと形式変更の runtime 表示は未確認。
### 2026-08-10 — ArtifactPr project save の既存ファイル削除先行を解消

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::saveProject()`
- **事実:** 旧保存処理は temp ファイルへ書いた後、既存の最終ファイルを `remove()` してから rename していた。rename に失敗すると元プロジェクトも失われ得る。
- **修正:** `QSaveFile` の write／commit 経路へ置き換え、既存ファイルを先に削除しないようにした。payload の書き込みサイズも確認する。
- **価値:** 保存失敗時に既存プロジェクトを残しやすくなり、コメントと実装の atomic save 契約が一致する。
- **未検証:** Windows 上の commit、権限不足、ディスク満杯、既存ファイルの復元動作は未確認。
### 2026-08-10 — ArtifactPr project load の stale sequence と JSON形状を防止

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::loadProject()`
- **事実:** JSON が object であることを確認せず、active sequence ID が見つからない場合は `currentSequence_` を更新していなかったため、直前プロジェクトのシーケンスが残る余地があった。
- **修正:** JSON parse error と root object 形状を分けて検証し、読込前に current sequence を初期化した。active sequence が不在でも先頭 sequence を選び、sequence が空なら空状態を維持する。out point も非負化した。
- **価値:** プロジェクト切替時の stale UI／再生範囲を減らし、壊れた JSON を読み込んだ後の状態遷移を明確にする。
- **未検証:** 空 sequence、active ID 欠損、旧形式 JSON の runtime 復元は未確認。
### 2026-08-10 — ArtifactPr TransportBar のタイムコード fps 固定を解消

- **関連:** `ArtifactPr/src/TransportBarWidget.cppm`、`TransportBarWidget::updateTimecode()`
- **事実:** タイムコード表示は常に30fpsで frame／秒を計算していたが、`DemoSequence::frameRate` にはシーケンス固有の fps が保持されている。
- **修正:** sequence の frameRate 文字列を安全に数値化し、無効値だけ30fpsへ fallback する共通ヘルパーを使うようにした。
- **価値:** 24／25／60fps のシーケンスでもタイムコード表示が実際の編集基準に一致する。
- **未検証:** 小数 fps（29.97等）の drop-frame 表示と runtime 表示は未確認。
### 2026-08-10 — ArtifactPr MainWindow の fps 固定表示を TransportBar と同期

- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm`、`ProgramMonitorPanel`、`TimelineRulerWidget`
- **事実:** Program Monitor のタイムコードと Timeline Ruler の目盛り・秒ラベルが30fps固定で、sequence の `frameRate` を参照していなかった。
- **修正:** sequence frameRate の安全な数値化を追加し、Program Monitor と Ruler の両方へ適用した。Ruler は sequence 更新時に fps を受け取る。
- **価値:** TransportBar、Program Monitor、Timeline Ruler が同じフレーム基準になり、非30fps sequence の時間表示の食い違いを減らす。
- **未検証:** 29.97fps の drop-frame 規則、sequence 切替直後の runtime 再描画は未確認。
### 2026-08-10 — ArtifactPr TimecodeOverlay の sequence fps 同期漏れを解消

- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm`、`TimecodeOverlayWidget`
- **事実:** オーバーレイは生成時に30fpsを固定設定し、sequenceChanged 後の fps 更新経路がなかった。
- **修正:** 初期値を現在 sequence から設定し、既存の sequenceChanged 接続内で同じ frameRate 契約を再適用するようにした。
- **価値:** TransportBar、Program Monitor、Timeline Ruler、右上 overlay のタイムコード基準が揃う。
- **未検証:** sequence 切替中の overlay 再描画と小数 fps の drop-frame 表示は未確認。
### 2026-08-10 — ArtifactPr VideoSurface の signal snapshot 読み出しを mutex 内へ移動

- **関連:** `ArtifactPr/src/VideoSurface.cppm`、`PrVideoSurface::present()`
- **事実:** 最新画像を mutex 保護下で更新した後、ロック解除後に `latestImage_` を signal payload へコピーしていた。次 frame の更新と同時に読む余地があった。
- **修正:** signal 用の QImage コピーを mutex 保護下で作り、解除後はそのコピーだけを payload へ移動するようにした。
- **価値:** video preview の latest-wins handoff で共有状態の競合窓を狭め、通知 payload と PTS の対応を安定させる。
- **未検証:** 実際の QVideoFrame スレッド境界と high-FPS の runtime 挙動は未確認。
### 2026-08-10 — ArtifactPr VideoPlayer の無効パス表示を placeholder へ戻す

- **関連:** `ArtifactPr/src/VideoPlayerWidget.cppm`、`VideoPlayerWidget::loadFile()`
- **事実:** 空パスや存在しないパスでも `QMediaPlayer::setSource()` と canvas 表示へ進み、読み込み失敗時の入口状態が不明瞭になっていた。
- **修正:** ローカルファイルの存在・通常ファイル判定を先に行い、無効な場合は再生停止と placeholder 表示へ戻すようにした。
- **価値:** Import／Project から不正な media path が渡っても、黒い canvas へ遷移せず利用者に未読み込み状態を示せる。
- **未検証:** ファイル権限、ネットワーク URL、QMediaPlayer の非同期エラー表示は未確認。
### 2026-08-10 — ArtifactPr ProjectPanel の sequence 行 ID を実データへ同期

- **関連:** `ArtifactPr/src/ProjectPanel.cppm`、`ProjectPanel::refreshProjectTree()`
- **事実:** sequence tree item の `Qt::UserRole` は常に整数 `0` で、表示中の `DemoSequence` と識別情報が一致していなかった。
- **修正:** `seq.id` を user data に格納するようにした。既存の tree 構造やイベント配線は変更していない。
- **価値:** 将来の選択・ナビゲーションが表示名ではなく sequence ID で対象を特定でき、同名 sequence にも対応できる。
- **未検証:** ProjectPanel の実クリック選択導線は今回の範囲外で、runtime 接続は未確認。
### 2026-08-10 — ArtifactPr Source／Program Monitor の sequence 切替契約を補正

- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm`、`SourceMonitorPanel`、`ProgramMonitorPanel`
- **事実:** Source Monitor の In／Out frame 変換は33ms固定で、Program Monitor の sequenceChanged handler は空実装だったため、非30fpsで範囲がずれ、sequence切替後に旧 preview が残る余地があった。
- **修正:** In／Out を sequence fps から `positionMs * fps / 1000` で算出し、sequence切替時は preview を停止・placeholderへ戻し、timecodeをゼロへ戻すようにした。
- **価値:** 24／25／60fps sequence の編集範囲が正しくなり、sequence切替時の古い video preview 混入を防ぐ。
- **未検証:** 小数 fps、sequence 切替中の非同期 frame 到着、実機の In／Out 表示は未確認。
### 2026-08-10 — ArtifactPr clip paste／trim の負 frame を防止

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::pasteClip()`／`trimClip()`
- **事実:** paste は負の target を current frame へ補正していたが、補正前の値を new clip へ代入していた。trim は負の newStart をそのまま保持できた。
- **修正:** paste の補正を new clip 作成前へ移動し、trim の newStart も0以上へ clampした。
- **価値:** clip が負の timeline frame に配置されず、paste と trim の編集範囲契約が move／seek と揃う。
- **未検証:** clip duration超過、source range境界、NLE store経路の runtime 編集は未確認。
### 2026-08-10 — ArtifactPr thumbnail request key に seek／サイズを含める

- **関連:** `ArtifactPr/include/MediaThumbnailer.ixx`、`ArtifactPr/src/MediaThumbnailer.cppm`、`ArtifactPr/src/ArtifactPrMainWindow.cppm`
- **事実:** thumbnail queue の重複判定と cache lookup が filePath だけをキーにしていたため、Source Monitor の5つの seek 時刻要求が1件に統合され、同じ画像を複数 slotへ誤配置する余地があった。
- **修正:** path、target size、seek 時刻から request key を作り、queue／cache をそのキーで分離した。生成結果にも seek 時刻を保持し、Source Monitor は対応 slotへ戻す。
- **価値:** 複数時刻の filmstrip がそれぞれ生成・表示され、異なるサイズ要求の cache 衝突も避けられる。
- **未検証:** 同一 path の大量要求、cache clear と到着順、実動画の seek 精度は未確認。
### 2026-08-10 — ArtifactPr timeline second snap を sequence fps へ同期

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::snapToNearestEx()`
- **事実:** 1秒単位の snap が30fps固定で、sequence の frameRate を参照していなかった。
- **修正:** 保存済みの frameRate を安全に数値化し、秒境界の snap 間隔へ適用した。無効値は30fpsへ fallback する。
- **価値:** 24／25／60fps sequence のクリップ・マーカー操作で、秒単位 snap が実時間の境界と一致する。
- **未検証:** 小数 fps の snap 境界と threshold 端の runtime 操作は未確認。
### 2026-08-10 — ArtifactPr setCurrentSequence の保存・通知同期を補正

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`EditorEngine::setCurrentSequence()`、Timeline transition 編集
- **事実:** setter は `currentSequence_` だけを書き換えており、`currentProject_.sequences` の保存対象と `sequenceChanged`／`projectModified` 通知を更新していなかった。
- **修正:** 同一 sequence ID の project snapshot を更新し、既存の sequence／project modified signal を setter から発行するようにした。
- **価値:** Transition の直接編集が画面だけでなく保存状態にも反映され、Program Monitor／Timeline／Project の更新経路が揃う。
- **未検証:** setter 呼び出し元の重複 refresh／signal 回数と project save の runtime round-trip は未確認。
### 2026-08-10 — ArtifactPr playback tick interval を sequence fps へ同期

- **関連:** `ArtifactPr/src/TransportBarWidget.cppm`、`TransportBarWidget::onPlayClicked()`
- **事実:** frame／timecode は sequence fps を参照するようになった一方、再生 timer は33ms固定で30fps相当だった。
- **修正:** sequence fps から `1000 / fps` の tick interval を計算し、最低1msで timer を開始するようにした。
- **価値:** 24／25／60fps sequence の再生 tick と frame進行の基準が一致する。
- **未検証:** 高fps、整数ms丸め、再生速度変更中の runtime frame pacing は未確認。
### 2026-08-10 — ArtifactPr pause tick の1フレーム進行を防止

- **関連:** `ArtifactPr/src/TransportBarWidget.cppm`、`TransportBarWidget::onPlaybackTick()`
- **事実:** `PlaybackSpeed::Pause` は `isPlaying()` では停止状態だが、timer が外部操作後も残った場合、既定分岐が1フレーム進行させる余地があった。
- **修正:** Stop／Pause を先に判定して timer を停止し、frame を変更せず return するようにした。
- **価値:** pause 操作が再生 tick の残留によって意図せず seek されることを防ぐ。
- **未検証:** 外部 pause signal と timer timeout の同時到着順は未確認。
### 2026-08-10 — ArtifactPr setCurrentFrame の範囲契約を seekToFrame と統一

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::setCurrentFrame()`／`seekToFrame()`
- **事実:** `seekToFrame()` は0〜durationへ clamp していたが、公開 setter は負値やduration超過をそのまま保持していた。
- **修正:** setter でも同じ範囲へ clamp し、signal には実際に適用した frame を渡すようにした。
- **価値:** playback、timeline、外部操作の current frame が常に sequence 範囲内になり、負の timecode や範囲外描画を防ぐ。
- **未検証:** durationが負／空sequenceの setter 呼び出しと runtime UI 表示は未確認。
### 2026-08-10 — ArtifactPr Lift／Insert edit command の復元とID生成を修正

- **関連:** `ArtifactPr/src/EditCommand.cppm`、`LiftEditCommand::doLift()`、`InsertEditCommand::doInsert()`
- **事実:** Lift undo の predicate が `c.id == c.id` で常に真になり、復元したclipを含む全clipを削除し得た。Insert は `QObject::sender()` の真偽だけを文字列化しており、一意な挿入IDを生成していなかった。
- **修正:** Lift undo は保存済みの元配列を丸ごと復元し、Insert は専用カウンタでIDを生成するようにした。
- **価値:** video timeline の Lift undo が他clipを壊さず、Insert/Undo対象を識別できる。
- **未検証:** 複数回の Insert／Lift undo-redo、既存 project のID衝突、runtime操作は未確認。
### 2026-08-10 — ArtifactPr NLE edit command の初回適用と挿入IDを修正

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/include/EditCommand.ixx`、`ArtifactPr/src/EditCommand.cppm`
- **事実:** `pushUndo()` は command を登録するだけなのに、Slip／Slide／Ripple／Insert／Overwrite／Lift の初回呼び出し側は `redo()` を実行していなかった。Insert／Overwrite の undo は source clip ID を探しており、挿入された新IDを追跡できなかった。
- **修正:** 各編集操作で初回 `redo()` を実行してから stack へ登録し、Insert／Overwrite に永続する inserted ID を持たせて undo／redo 対象を一致させた。
- **価値:** NLE編集操作が初回から実データへ反映され、undo／redoで元clipや別clipを誤操作しにくくなる。
- **未検証:** 6操作の組合せ、NLE store経路との parity、複数回 undo／redo は runtime 未確認。
### 2026-08-10 — ArtifactPr Ripple／Insert edit の sequence duration 同期を補正

- **関連:** `ArtifactPr/src/EditCommand.cppm`、`RippleDeleteCommand::doRipple()`、`InsertEditCommand::doInsert()`
- **事実:** 両 command は old/new duration を保持していたが、実行時に `DemoSequence::duration` を更新していなかった。
- **修正:** 初回適用・undo・redoの各経路で sequence snapshot の duration を切り替え、`setCurrentSequence()` へ渡すようにした。
- **価値:** timeline の clip構造と sequence終端、再生範囲、保存対象の duration が一致する。
- **未検証:** Insert／Ripple の連続操作と NLE store同期は runtime 未確認。
### 2026-08-10 — ArtifactPr TimecodeOverlay の fps setter を正規化

- **関連:** `ArtifactPr/include/TimecodeOverlayWidget.ixx`、`TimecodeOverlayWidget::setFps()`
- **事実:** setter は0以下の fps を保持し、描画時の `frameToTimecode()` だけが30fpsへ fallback していた。
- **修正:** setter で最低1fpsへ clamp し、保持値と描画値の契約を一致させた。
- **価値:** sequence変更や外部UI設定から無効fpsが渡っても、表示状態が暗黙に別値へ変わらない。
- **未検証:** 0以下fpsを直接設定する runtime 呼び出しと overlay 再描画は未確認。
### 2026-08-10 — ArtifactPr MediaThumbnailer に要求完全一致の cache API を追加

- **関連:** `ArtifactPr/include/MediaThumbnailer.ixx`、`ArtifactPr/src/MediaThumbnailer.cppm`
- **事実:** cache key を seek／サイズ単位へ細分化した後も、公開 `cached(filePath)` は複数 variant のうち任意の1件を返す曖昧さが残った。
- **修正:** 既存の filePath 版を互換維持しつつ、`ThumbnailRequest` の path／サイズ／seek に完全一致する overload を追加した。
- **価値:** 呼び出し側が意図した thumbnail variant を同期取得でき、cache key 契約をAPIから明示的に利用できる。
- **未検証:** 新 overload の外部利用と variant 別 cache hit の runtime 挙動は未確認。
### 2026-08-10 — ArtifactPr TimecodeOverlay の負 frame 入力を拒否

- **関連:** `ArtifactPr/src/TimecodeOverlayWidget.cppm`、`TimecodeOverlayWidget::setCurrentFrame()`
- **事実:** EditorEngine 側は current frame を0以上へ clampするが、overlay の公開 setter は負値をそのまま保持できた。
- **修正:** overlay setter でも0未満を0へ正規化してから更新するようにした。
- **価値:** 外部UIや将来の別再生経路から負 frame が渡っても、負の timecode 表示を防げる。
- **未検証:** 負 frame の直接 runtime 呼び出しと overlay 描画は未確認。
### 2026-08-10 — ArtifactPr sequence 差し替え時の時間範囲を clamp

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`EditorEngine::setCurrentSequence()`
- **事実:** sequence の保存・通知は同期したが、duration が変わる差し替え時に current frame／In／Out が旧 duration を超える可能性が残っていた。
- **修正:** sequence 更新後に current frame、In、Out を0〜新 durationへ clampし、current frame が変わった場合は既存 signal を発行する。
- **価値:** sequence切替・編集後に再生位置と作業範囲が存在しない frame を指さなくなる。
- **未検証:** In > Out の既存状態、空 sequence の runtime 表示は未確認。
### 2026-08-10 — ArtifactPr legacy trim の undo 契約を補正

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`TrimClipCommand`
- **事実:** legacy fallback の trim は clip の start／duration／source 範囲を直接変更していたが、undo stack へ登録しておらず、既存 command も source 範囲を保持していなかった。
- **修正:** TrimClipCommand に旧新の sourceIn／sourceOut を持たせ、fallback trim を初回 redo 後に pushUndo する経路へ統一した。
- **価値:** NLE store を使わない legacy clip でも、trim の表示状態と source 範囲を一体で undo／redo できる。
- **未検証:** runtime の trim → undo → redo と、NLE store 経路との挙動比較は未確認。
### 2026-08-10 — ArtifactPr thumbnail cache の世代境界を追加

- **関連:** `ArtifactPr/include/MediaThumbnailer.ixx`、`ArtifactPr/src/MediaThumbnailer.cppm`、`MediaThumbnailer::clearCache()` / `publishThumbnail()`
- **事実:** 非同期生成中に project unload や cache clear が起きると、古い worker 結果が後から cache と `thumbnailReady` に戻る経路があった。
- **修正:** request に cache 世代を付与し、`clearCache()` で世代を進め、現行世代と一致しない生成結果を publish 前に破棄するようにした。
- **価値:** プロジェクト切替後に旧メディアのサムネイルが新しい表示面へ混入する可能性を抑えられる。
- **未検証:** 実際の切替中に発生する遅延 callback の runtime 挙動は未確認。
### 2026-08-10 — ArtifactPr video preview の旧フレーム残留を抑制

- **関連:** `ArtifactPr/src/VideoPlayerWidget.cppm`、`VideoPlayerWidget::loadFile()`、`VideoCanvas`
- **事実:** 無効パスへ戻る場合や別ファイルを読み込む場合も canvas の直前フレームは保持され、次の表示面切替で旧映像が再利用される可能性があった。
- **修正:** load 成功・失敗の境界で canvas を空 pixmap に戻してから placeholder／新 source を表示するようにした。
- **価値:** メディア切替時に旧ファイルの映像を新しいファイルの初期表示として誤認しにくくなる。
- **未検証:** 実際の decode 遅延中の placeholder／黒画面遷移は runtime 未確認。
### 2026-08-10 — ArtifactPr 保存失敗時の modifiedAt を復元

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::saveProject()`
- **事実:** `QSaveFile` の commit 前に `currentProject_.modifiedAt` を更新していたため、ディスク保存に失敗してもメモリ上の更新時刻だけが進んでいた。
- **修正:** commit 失敗時に保存前の `modifiedAt` を復元するようにした。
- **価値:** 保存失敗後の dirty state とプロジェクトメタデータの意味が一致しやすくなる。
- **未検証:** ディスクフル／権限拒否などの実際の commit 失敗は runtime 未確認。
### 2026-08-10 — ArtifactPr timeline zoom の座標反映を補正

- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm`、`TimelinePanel::onZoomChanged()`、`FRAME_WIDTH`
- **事実:** ズーム値はラベルと未使用のローカル変数にしか反映されず、ruler／clip幅／マーカー操作／移動・トリム計算は常に2 px/frameを使っていた。
- **修正:** ズーム変更時に共有 frame width を更新し、既存の `refreshTimeline()` で関連表示と操作領域を再構築するようにした。
- **価値:** 見た目の倍率とマウス座標から frame へ変換する契約が一致する。
- **未検証:** ズーム変更中のスクロール位置保持と長尺 sequence の runtime 操作は未確認。
### 2026-08-10 — ArtifactPr work area の In／Out 順序を正規化

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`EditorEngine::setInPoint()` / `setOutPoint()`
- **事実:** 公開 setter は負値、duration超過、In > Out／Out < In を受け入れ、再生・書き出し計画へ不正な範囲を渡し得た。
- **修正:** In は0〜Out、Out はIn〜sequence durationへ clampし、端点の順序を常に維持するようにした。
- **価値:** UI操作とショートカット操作のどちらからでも、ワークエリア範囲が有効な frame 区間になる。
- **未検証:** 空 sequence と duration変更直後の端点表示は runtime 未確認。
### 2026-08-10 — ArtifactPr clip property editor の初回適用を補正

- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm`、`ClipPropertiesPanel`、`ClipPropertyCommand`
- **事実:** Volume／Speed／Reverse の UI handler は command を undo stack に積んでいたが、初回 `redo()` を呼んでいなかった。
- **修正:** 各 handler で command を初回適用してから stack に登録するようにした。
- **価値:** property editor の表示値と clip model の値が一致し、続く undo／redo も同じ command 契約で動く。
- **未検証:** slider／combo変更後の runtime undo／redo と NLE store 経路の比較は未確認。
### 2026-08-10 — ArtifactPr 無効 clip 選択を拒否

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::selectClip()`
- **事実:** 選択要求された ID が現 sequence に存在しなくても `selectedClipId_` に保持され、Inspector／command の stale selection になり得た。
- **修正:** video／audio track 全体で存在を確認し、無効 ID は空選択として通知するようにした。
- **価値:** sequence差し替えや遅延UI callback後に、存在しない clip を編集対象として残さない。
- **未検証:** sequence切替と選択通知が同一イベントループ内で競合する runtime 挙動は未確認。
### 2026-08-10 — ArtifactPr property command の dirty 通知を補正

- **関連:** `ArtifactPr/src/EditCommand.cppm`、`ClipPropertyCommand::doApply()`
- **事実:** Volume／Speed／Reverse／Name の command 適用は `clipChanged` のみを通知し、保存状態を示す `projectModified` を通知していなかった。
- **修正:** property command の適用完了時に既存の `projectModified` signal を発行するようにした。
- **価値:** UI property 編集、undo、redo のいずれでもプロジェクトの未保存状態が更新される。
- **未検証:** runtime の dirty indicator と連続 undo／redo 表示は未確認。
### 2026-08-10 — ArtifactPr legacy clip property 操作を undo 経路へ統一

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`setClipSpeed()`、`setClipReversed()`、`setClipVolume()`、`setClipName()`
- **事実:** Inspector は `ClipPropertyCommand` を使う一方、legacy のコンテキストメニュー経路は clip を直接変更して undo stack を迂回していた。
- **修正:** legacy 経路も旧値・新値を持つ `ClipPropertyCommand` を初回適用してから stack に登録するようにした。speed は0.01以上、volumeは0〜2へ正規化した。
- **価値:** 右クリック操作と Inspector 操作で undo／redo と dirty 通知の挙動が一致する。
- **未検証:** 連続 property 操作の stack 粒度と NLE store 側の parity は runtime 未確認。
### 2026-08-10 — ArtifactPr marker 編集を状態 command へ統一

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/src/ArtifactPrEditorEngine.cppm`、marker 編集 API
- **事実:** marker の追加／削除だけが undo 対応で、移動・名前・コメント・全削除は直接変更されていた。位置も sequence 範囲外を受け入れていた。
- **修正:** marker 配列の before／after を保持する `MarkerStateCommand` を追加し、全編集操作を undo stack へ登録。追加／移動位置を0〜durationへ clampした。
- **価値:** marker 操作の復元契約が揃い、範囲外 marker が ruler／書き出し計画へ流れにくくなる。
- **未検証:** 複数 marker 操作の連続 undo／redo と sequence 切替中の表示は runtime 未確認。
### 2026-08-10 — ArtifactPr video ripple delete の undo を補正

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::rippleDeleteSelectedClip()`、`RippleDeleteCommand`
- **事実:** video legacy 経路は後続 clip を詰めながら `DeleteClipCommand` を使っていたため、undo で後続位置と sequence duration が復元されなかった。
- **修正:** video track だけ `RippleDeleteCommand` を初回適用して stack へ登録する経路へ切り替えた。audio track の既存経路は変更していない。
- **価値:** 映像の ripple delete が undo／redo で構造・終端ともに戻る。
- **未検証:** 複数 video track と連続 ripple delete の runtime 挙動は未確認。
### 2026-08-10 — ArtifactPr video split／duplicate の undo を追加

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`splitClipAtPlayhead()`、`duplicateSelectedClip()`
- **事実:** video clip の split／duplicate は配列を直接変更し、undo stack と選択状態の復元を持っていなかった。
- **修正:** video tracks 全体と選択 ID の before／after を保持する `VideoTracksStateCommand` を追加し、映像経路だけ初回変更後に stack へ登録した。audio 経路は変更していない。
- **価値:** Blade／Duplicate 操作を undo／redo しても映像 clip 構造と選択対象が一致する。
- **未検証:** 複数 video track、split後の連続 duplicate、runtime undo／redo は未確認。
### 2026-08-10 — ArtifactPr video duplicate の sequence duration を同期

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`VideoTracksStateCommand`、`duplicateSelectedClip()`
- **事実:** video clip を sequence末尾へ duplicate しても sequence duration は旧値のままで、複製 clip の終端が再生・書き出し範囲外になり得た。
- **修正:** video tracks state command に duration の before／after を保持させ、duplicate後の最大終端を sequence duration へ反映し、undo時に復元するようにした。
- **価値:** 複製した映像 clip、sequence終端、再生範囲が一致する。
- **未検証:** duplicate後の sequence切替と runtime undo／redo は未確認。
### 2026-08-10 — ArtifactPr video split／duplicate の初回保存同期を補正

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::splitClipAtPlayhead()`、`duplicateSelectedClip()`
- **事実:** legacy video trackを直接変更した後、`currentSequence_` の内容が `currentProject_.sequences` へ反映される前に保存できる経路が残っていた。
- **修正:** video split／duplicate の初回変更後に既存 `setCurrentSequence()` を通し、project snapshot と sequence duration を同期してから undo command を登録するようにした。
- **価値:** 画面上で見える映像編集結果と保存対象の sequence が一致する。
- **未検証:** split／duplicate後の save→load round trip は runtime 未確認。

### 2026-08-10 — ArtifactPr video paste を sequence state undo に統一

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::pasteClip()`、`VideoTracksStateCommand`
- **事実:** legacy paste は video track を直接変更して `projectModified()` / `sequenceChanged()` を発行するだけで、映像トラック状態・duration・選択状態を undo command に保存していなかった。
- **修正:** video paste の前後で video tracks、duration、selection を保存し、挿入後に `setCurrentSequence()` と `VideoTracksStateCommand` を使うようにした。audio-only の既存分岐は変更していない。
- **価値:** paste 後の保存用 sequence snapshot と Undo/Redo の復元対象が揃い、末尾以降への貼り付けによる duration 拡張も復元できる。
- **未検証:** 実ビルド・テストは未実行。clipboard の source track 種別を保持しない既存仕様は今回の対象外。
### 2026-08-10 — ArtifactPr video delete を sequence state undo に統一

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::deleteSelectedClip()`、`VideoTracksStateCommand`
- **事実:** legacy video delete は clip 配列を直接削除し、旧 `DeleteClipCommand` を登録していたため、選択状態・保存用 sequence snapshot・sequence通知の復元契約が映像全体では揃っていなかった。
- **修正:** video delete の前後で video tracks、duration、選択状態を保存し、削除後に `setCurrentSequence()` と `VideoTracksStateCommand` を登録するようにした。audio delete の既存経路は変更していない。
- **価値:** 映像削除の undo／redo が clip構造と選択状態を一体で復元し、削除直後の保存対象も同期する。
- **未検証:** 実ビルド・テストは未実行。audio-only deletion と NLE store 経路は今回の対象外。

### 2026-08-10 — ArtifactPr video cut の対象選択を引数へ同期

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::cutClip()`
- **事実:** cut は引数 clip を clipboard へコピーした後、削除対象を `selectedClipId_` に委ねていたため、引数と選択が異なると別の clip を削除する可能性があった。
- **修正:** video track 上の引数 clip だけ、削除前に既存 `selectClip()` で選択状態を同期するようにした。audio clip の既存経路は変更していない。
- **価値:** UI／APIから明示した映像 clip と cut の削除対象が一致する。
- **未検証:** 実ビルド・テストは未実行。複数選択や linked clip の仕様は今回の対象外。

### 2026-08-10 — ArtifactPr video move／trim の初回保存同期を補正

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::moveClip()` / `trimClip()`
- **事実:** legacy video の move／trim は command を undo stack へ登録していたが、初回変更後に `currentProject_.sequences` へ current sequence を同期していなかった。
- **修正:** video clip の初回 move／trim 後だけ既存 `setCurrentSequence()` を通し、保存対象 snapshot を更新するようにした。audio clip の既存経路は変更していない。
- **価値:** タイムライン上の移動・トリム結果が、同じ編集直後の保存にも反映される。
- **未検証:** 実ビルド・テストは未実行。NLE store 経路と audio-only 操作は今回の対象外。

### 2026-08-10 — ArtifactPr video slip／slide／overwrite／lift の初回保存同期を補正

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`slipClip()`、`slideClip()`、`overwriteClipFromSource()`、`liftRange()`
- **事実:** これらの legacy command は初回 `redo()` と undo stack 登録を行っていたが、変更後の current sequence を project snapshot へ同期していなかった。`insertClipFromSource()` は既存 command 内で同期済みだった。
- **修正:** video clip／video track の初回適用後だけ既存 `setCurrentSequence()` を通すようにした。audio 経路は変更していない。
- **価値:** source range、隣接 clip、上書き、lift の結果が編集直後の保存対象にも反映される。
- **未検証:** 実ビルド・テストは未実行。NLE store 経路と audio-only 操作は今回の対象外。

### 2026-08-10 — ArtifactPr video edit command の undo／redo 保存同期を補正

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/src/EditCommand.cppm`、move／trim／slip／slide／overwrite／lift command
- **事実:** 初回編集時に snapshot を同期しても、command の undo／redo 実行時は clip 配列だけが変わり、保存対象の current project sequence が古くなる経路が残っていた。
- **修正:** video clip／video track の command 適用後に既存 `setCurrentSequence()` を通すようにした。audio command の経路は変更していない。
- **価値:** 映像編集の初回適用と undo／redo 後で、画面状態・保存対象・sequence 通知の整合が保たれる。
- **未検証:** 実ビルド・テストは未実行。複合 command の連続 undo／redo と NLE store parity は未確認。

### 2026-08-10 — ArtifactPr video property command の undo／redo 保存同期を補正

- **関連:** `ArtifactPr/src/EditCommand.cppm`、`ClipPropertyCommand::doApply()`
- **事実:** 映像 clip の speed／reverse／volume／name は初回適用と undo／redo で clip 値を更新していたが、project sequence snapshot の同期は行っていなかった。
- **修正:** video clip に限って property 適用後に `setCurrentSequence()` を通し、既存の dirty 通知を維持した。audio clip の経路は変更していない。
- **価値:** Inspector／コンテキスト編集と undo／redo の結果が保存対象にも一致する。
- **未検証:** 実ビルド・テストは未実行。NLE store property 経路との parity は未確認。
### 2026-08-10 — ArtifactPr render plan の映像範囲を sequence duration へ clamp

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`EditorEngine::createRenderPlan()`、`RenderPlan`
- **事実:** In/Out setter は範囲を正規化していたが、render plan API に明示した start／end frame は duration 外や逆順のまま plan に入る可能性があった。
- **修正:** 明示値と既定 In/Out の両方を sequence duration 内へ clampし、end は start 以上へ正規化した。
- **価値:** プレビュー・書き出し側が存在しない映像 frame 範囲を受け取りにくくなる。
- **未検証:** 実ビルド・テスト・実ファイル出力は未実行。
### 2026-08-10 — ArtifactPr video surface の切替時 pending frame を破棄

- **関連:** `ArtifactPr/include/VideoSurface.ixx`、`ArtifactPr/src/VideoSurface.cppm`、`ArtifactPr/src/VideoPlayerWidget.cppm`
- **事実:** media 切替時に canvas を空にしても、surface 内の未送信旧 frame が残り、切替後の表示へ混入する余地があった。
- **修正:** `PrVideoSurface::clear()` を追加し、無効／有効 path の切替前に player 停止と pending frame 破棄を行うようにした。新しい signal は追加していない。
- **価値:** 映像 source 切替時の旧 frame 残留と一瞬の誤表示を抑える。
- **未検証:** 実デコード中の source 切替競合と runtime 表示は未確認。
### 2026-08-10 — ArtifactPr video transition の追加／削除を state command 化

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`addTransition()` / `deleteTransition()`
- **事実:** video transition の追加・削除は配列を直接変更し、保存 snapshot と Undo/Redo の状態 command を持っていなかった。audio track へ流れる既存経路も存在する。
- **修正:** video track の追加・削除だけ before／after の transition 配列を `TransitionStateCommand` に保持し、`setCurrentSequence()` を通すようにした。音声経路は変更していない。
- **価値:** 映像トランジションの編集結果が保存・Undo/Redo と一致し、削除後の復元対象も明確になる。
- **未検証:** 実ビルド・テストは未実行。トランジションの runtime 合成結果と audio track 経路は今回の対象外。
### 2026-08-10 — ArtifactPr video transition resize を state command 化

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/src/ArtifactPrMainWindow.cppm`、`setVideoTransitionDuration()`
- **事実:** timeline の video transition drag resize は UI から sequence copy を直接書き換え、Undo/Redo command を登録していなかった。
- **修正:** video transition 専用 API と `TransitionStateCommand` を追加し、video の場合だけ新経路へ接続した。audio transition は既存 UI 経路へフォールバックする。
- **価値:** 映像トランジションのドラッグリサイズも追加・削除と同じ保存／Undo契約になる。
- **未検証:** 実ビルド・テストは未実行。context menu の固定 duration 操作と runtime 表示は次回確認対象。
### 2026-08-10 — ArtifactPr video transition context duration を state command 化

- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm`、`TimelinePanel::onTransitionRightClicked()`
- **事実:** context menu の固定 duration 操作は menu 作成時の sequence copy を直接書き戻し、映像トランジションの Undo/Redo 経路を迂回していた。
- **修正:** 映像 transition は `setVideoTransitionDuration()` を使い、audio transition は既存の copy 書き戻しへフォールバックする共通処理にした。
- **価値:** drag resize と context duration 操作の映像側で保存・Undo契約が揃い、古い sequence copy による上書きも避けられる。
- **未検証:** 実ビルド・テストは未実行。context menu の runtime 操作と audio fallback は未確認。
### 2026-08-10 — ArtifactPr video import を media pool 保存へ接続

- **関連:** `ArtifactPr/src/MediaPanel.cppm`、`MediaPanel::onImportClicked()`、`EditorEngine::addMediaToPool()`
- **事実:** MediaPanel の import は video file を一覧へ追加していたが、project の `mediaPool` へ登録していなかったため、保存・再読込で素材情報が失われ得た。
- **修正:** mp4／avi／mov／mkv の video import 時だけ既存 `addMediaToPool()` を呼ぶようにした。audio import の既存経路は変更していない。
- **価値:** UI で読み込んだ映像素材が project 保存対象へ入り、再利用可能な media metadata として残る。
- **未検証:** 実ビルド・テスト・save→load runtime は未実行。重複 video path の扱いは次回確認対象。
### 2026-08-10 — ArtifactPr video import の media pool 重複を抑制

- **関連:** `ArtifactPr/src/MediaPanel.cppm`、`MediaPanel::onImportClicked()`、`EditorEngine::mediaPool()`
- **事実:** 同じ video path を複数回 import すると、一覧と project mediaPool の両方に重複 entry が増える経路があった。
- **修正:** video import 前に既存 mediaPool の path／type を照合し、未登録の場合だけ `addMediaToPool()` を呼ぶようにした。
- **価値:** 再 import 操作が保存対象の video metadata を増殖させず、media pool と UI の対応が安定する。
- **未検証:** 大文字小文字の異なる path、symlink／relative path の同一性は未確認。
### 2026-08-10 — ArtifactPr MediaPanel の video path と保存 mediaPool 表示を同期

- **関連:** `ArtifactPr/src/MediaPanel.cppm`、`MediaPanel::refreshMediaList()`、`onItemDoubleClicked()`
- **事実:** video clip の list item に `clip.name` を UserRole path として格納していたため、double-click 時に実ファイルではなく表示名を preview へ渡していた。また保存済み mediaPool の video entry は refresh 対象外だった。
- **修正:** video clip は `sourceFile` を UserRole に格納し、保存済み video mediaPool を path 重複なしで追加表示するようにした。audio clip の既存表示経路は変更していない。
- **価値:** project 再読込後も video source の選択・preview 導線が実ファイルへつながり、import 済み素材が一覧から消えにくくなる。
- **未検証:** 実ビルド・テスト・save→load runtime は未実行。
### 2026-08-10 — ArtifactPr File menu の video import 導線を接続

- **関連:** `ArtifactPr/include/ArtifactPrMainWindow.ixx`、`ArtifactPr/src/ArtifactPrMainWindow.cppm`、`MediaPanel`
- **事実:** File menu の `Import Media...` は action だけ作られ、triggered handler がなく、File menu から import できなかった。
- **修正:** File menu を `Import Video...` として video file dialog、既存 `addMediaToPool()`、MediaPanel refresh へ接続した。audio file filter／処理は追加していない。
- **価値:** File menu から読み込んだ映像も保存対象と MediaPanel 表示へ入り、dock 内 import と同じ project workflow に乗る。
- **未検証:** 実ビルド・テスト・runtime File menu 操作は未実行。
### 2026-08-10 — ArtifactPr File menu video import の重複登録を抑制

- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm`、File menu の video import handler
- **事実:** File menu を接続すると、同じ video path の再 import が mediaPool entry を増やす可能性があった。
- **修正:** 既存 mediaPool の video path／type を照合し、未登録時だけ `addMediaToPool()` を呼ぶようにした。
- **価値:** dock import と File menu import のどちらでも、保存対象の video metadata が重複しにくくなる。
- **未検証:** path 正規化や symlink 同一性は未確認。
### 2026-08-10 — ArtifactPr MediaPanel video import の一覧重複を抑制

- **関連:** `ArtifactPr/src/MediaPanel.cppm`、`MediaPanel::onImportClicked()`
- **事実:** video mediaPool の重複登録を防いだ後も、同一 video path の再 import は QListWidget item を重複追加していた。
- **修正:** video path が既存 UserRole にある場合は一覧追加を省略し、audio の既存追加処理は維持した。
- **価値:** dock import を繰り返しても video 一覧と mediaPool の件数が乖離しにくくなる。
- **未検証:** path 正規化・大文字小文字差・symlink の同一性は未確認。
### 2026-08-10 — ArtifactPr save failure rollback の変数スコープを修正

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`saveProject()`、`loadDemoProject()`
- **事実:** `saveProject()` の commit failure 分岐が `previousModifiedAt` を参照していたが、変数は別関数 `loadDemoProject()` 内にあり、save 側では未宣言だった。
- **修正:** 保存前の modifiedAt を `saveProject()` 内で取得し、demo load 側の不要な宣言を削除した。
- **価値:** 保存失敗時の modifiedAt 復元が正しいスコープで成立し、未宣言変数による compile failure を避ける。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Channel component display shader の cbuffer 契約

- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`、`channelComponentDisplayShaderText`、`BlendParams`
- **事実:** channel display 用 HLSL は `component` だけの短い cbuffer を宣言していた一方、実装は `displayMode` / `displayComponentY` / `displayComponentZ` を参照し、C++ は共通 `BlendParams` を転送していた。
- **修正:** HLSL cbuffer を共通 `BlendParams` の8 scalar構成へ揃え、単一チャンネル参照は C++ が設定する `blendMode` を使うようにした。
- **価値:** MatteTrack assertion 修正後に露出する shader compile failure と、display composite の未宣言値参照を防ぐ。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — MatteTrack の入力契約とSRV binding検証

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`LayerBlendPipeline::applyTrackMatte()`
- **事実:** matte count と optional SRV の関係、幅・高さ0を関数入口で検証しておらず、texture view binding の戻り値も無視していた。
- **修正:** countを1〜3に制限し必要なSRVを要求、0サイズを拒否し、全SRV binding成功を dispatch 前に確認するようにした。
- **価値:** 不完全な track matte 入力をGPU dispatchまで通さず、失敗箇所をCPU側で明確に扱える。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — MatteTrack constant buffer map failure

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`LayerBlendPipeline::applyTrackMatte()`
- **事実:** `MapBuffer()` 後の `pData == nullptr` を警告せず、古い定数値のまま dispatch する経路があった。
- **修正:** map失敗時は即座に false を返し、dispatchを行わないようにした。
- **価値:** GPUへ不確定な matte parameters を送らず、失敗を呼び出し側へ返せる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — blendDirect の GPU binding失敗処理

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`LayerBlendPipeline::blendDirect()`
- **事実:** direct blend 経路は BlendParams の map失敗と Src/Dst/Out texture binding の戻り値を無視して dispatchしていた。
- **修正:** mapまたはbindingが失敗した場合は dispatchせず falseを返すようにした。
- **価値:** 未更新パラメータや未設定リソースでのGPU実行を防ぐ。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — LayerBlend dispatch の texture binding契約

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`convertLayerToFloat()`、`blend()`、`blendDirect()`
- **事実:** convert/通常blendがtexture bindingの失敗を無視し、direct blendは0サイズでもdispatch可能だった。
- **修正:** Src/Dst/Out bindingの戻り値を確認し、direct blendの幅・高さ0を拒否するようにした。
- **価値:** 未設定リソースや無効 dispatchをGPUへ渡さず、呼び出し側が失敗を検知できる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioMixer の旧 JSON 欠損フィールド復元

- **関連:** `ArtifactCore/src/Audio/AudioMixer.cppm`、`AudioMixer::deserialize()`
- **事実:** 旧形式の mixer bus JSON で `volume` / `pan` / `mute` / `solo` が欠落すると、Qt JSON の既定変換により volume/pan が 0、mute/solo が false として上書きされる。
- **修正:** フィールドが存在するときだけ値を復元し、欠損時は `AudioBus` の既定値を保持するようにした。
- **価値:** 古いプロジェクトを読み込んでも、未保存だった mixer 属性が破壊的に無音化されにくい。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Composition Audio Mixer の Master bus 接続

- **関連:** `Artifact/src/Audio/ArtifactAudioMixer.cppm`、`AudioMixer::connectToCoreMixer()`、`AudioMixerMasterBus::connectToCoreBus()`
- **事実:** compact mixer の Master bus は Core mixer の Master bus に接続されておらず、Advanced Routing 側で変更した Master volume/mute が再表示時に失われていた。
- **修正:** Core mixer 接続時に Master bus を接続し、volume と mute を同期する。接続解除時は安全な既定値へ戻す。
- **価値:** Advanced Routing と compact mixer の Master 操作が同じ Core 状態を参照する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Core Master 値を Playback service へ再同期

- **関連:** `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`、`refreshFromCurrentComposition()`
- **事実:** Advanced Routing は Core の Master bus を直接編集するが、refresh 前に呼ばれる service 同期は旧 service 値を再適用するため、表示値と実際の出力 gain/mute が乖離し得た。
- **修正:** Core mixer を再接続した直後に Master bus の volume/mute を `ArtifactAudioService` へ反映する。
- **価値:** Advanced Routing 後も compact UI、Core bus、Playback service の Master 状態が一致する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — MediaPanel audio import の保存・再表示

- **関連:** `ArtifactPr/src/MediaPanel.cppm`、`refreshMediaList()`、`onImportClicked()`
- **事実:** audio import は QListWidget にだけ追加され mediaPool に登録されなかった。また audio clip の refresh は `sourceFile` ではなく `name` をファイルパスとして扱っていた。
- **修正:** audio の mediaPool 登録と重複抑制を追加し、audio track/mediaPool の表示パスを `sourceFile` / `filePath` から復元するようにした。thumbnail 要求は映像拡張子に限定した。
- **価値:** 音声素材が refresh・保存・再読込の境界で消えたり、表示名をファイルとして誤参照したりしにくくなる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — MatteTrack constant buffer の末尾 padding

- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx`、`ArtifactCore/include/Graphics/Shader/Compute/HLSL/MatteTrack.ixx`、`ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`
- **事実:** C++ `MatteTrackParams` は scalar 11個で44 bytesだったが、HLSL constant buffer は16-byte register単位で3 register（48 bytes）。既存の static assertion がこの差を検出していた。
- **修正:** HLSL が参照しない末尾の4-byte paddingを追加し、定数バッファのサイズを48 bytesに一致させた。
- **価値:** Layer blend の C++20 module compile failure を解消し、GPU constant buffer の境界契約を明示できる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Pointwise compute の resource layout と texture 契約

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`LayerBlendPipeline::applyPointwise()`、`PointwiseComputePlan`
- **事実:** Pointwise plan は resource 名を保持しているが、compute pipeline の variable layout は固定名を使っていた。また dispatch 寸法と SRV/UAV の実寸法、入出力 texture の alias が入口で検証されていなかった。
- **修正:** variable layout を plan の resource 名から構築し、executor の ready 状態を再確認する。dispatch 寸法と texture 寸法を一致させ、同一 texture の入出力を拒否する。
- **価値:** カスタム resource 名の plan が bind 失敗しにくくなり、寸法不一致による未処理領域や不正な in-place compute を早期に検出できる。
- **未検証:** 実ビルド・テストは未実行。`git diff --check` のみ実行。

### 2026-08-10 — Layer blend executor の constant buffer bind 初期化

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`createExecutors()`、`createMatteTrackExecutor()`
- **事実:** 通常 Blend と MatteTrack の `ComputeExecutor::setBuffer()` 戻り値が無視され、定数バッファを bind できなくても executor が登録され得た。
- **修正:** `BlendParams` / `MatteTrackParams` の bind 成功を executor 初期化の条件にし、失敗した blend mode は登録せず、MatteTrack は初期化失敗として扱う。
- **価値:** opacity や matte パラメータが未 bind のまま dispatch される遅延不具合を、初期化境界で検出できる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — MatteTrack の入力 texture 寸法・alias 境界

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`applyTrackMatte()`、`ArtifactCore/include/Graphics/Shader/Compute/HLSL/MatteTrack.ixx`
- **事実:** MatteTrack shader は全入力を同じ dispatch 座標で読み、出力寸法だけで bounds check するが、CPU 側は非ゼロ寸法しか検証していなかった。
- **修正:** layer/matte/output texture の実寸法を要求寸法と一致させ、output と入力の alias を拒否する。
- **価値:** 入力不足による範囲外 read と read/write 同一 resource の不定動作を dispatch 前に防げる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Pointwise executor cache key の resource layout 同期

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`applyPointwise()`、`PointwiseComputePlan`
- **事実:** Pointwise executor の再利用 key は shader compile key だけで、plan が持つ parameter/source/output/background/LUT/history の resource 名を含んでいなかった。
- **修正:** resource 名を cache key に含め、同じ shader 本体でも layout 名が変わった plan は executor を再構築する。
- **価値:** custom resource 名を持つ連続 plan の bind が、直前 plan の古い layout に依存しなくなる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — blendDirect の explicit dispatch texture 契約

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`blendDirect()`、`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`
- **事実:** Blend shader は output 寸法だけで bounds check し、direct API は要求 width/height に基づき dispatch するが、入力 texture の最小寸法・format・output alias を検証していなかった。
- **修正:** 全 texture が要求範囲を覆うこと、canonical float format が一致すること、SRV と UAV が alias しないこと、constant buffer が存在することを事前検証する。
- **価値:** direct blend の範囲外 read と read/write hazard を dispatch 前に防ぎ、blend shader の前提を CPU API に反映する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — MatteTrack shader source の単一化

- **関連:** `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`、`ArtifactCore/include/Graphics/Shader/Compute/HLSL/MatteTrack.ixx`
- **事実:** PSO は cppm 内の `kMatteTrackSource` を使う一方、同じ shader の公開 HLSL module が別コピーとして存在していた。両方の現内容は同等だが、将来の layout 修正が片方だけに入る drift リスクがあった。
- **修正:** PSO の source と entry point を既存の `Shaders::MatteTrack` module から参照し、cppm 内の重複 source を削除した。
- **価値:** C++ constant buffer layout の検証対象と実際の PSO source が同じ定義を参照し、再発経路を一つ減らせる。
- **未検証:** 実ビルド・テストは未実行。module 依存の静的確認のみ。

### 2026-08-10 — BlendParams constant buffer の static layout guard

- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx`、`BlendParams`、`LayerBlendComputeShader.ixx`
- **事実:** BlendParams は HLSL の 2 register / 32-byte layout と一致しているが、MatteTrack と異なり C++ 側の static assertion がなかった。
- **修正:** `sizeof(BlendParams) == 32` を検証する static assertion を追加した。
- **価値:** channel display と blend の constant buffer drift を module compile 時に検出できる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioMixer Master bus の composition 永続化

- **関連:** `ArtifactCore/src/Audio/AudioMixer.cppm`、`AudioMixer::serialize()` / `deserialize()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- **事実:** mixer serialization は Master bus を bus 配列から除外し、Master volume/mute を別に保存・復元していなかったため、Advanced Routing で変更した Master 状態が再読込で既定値へ戻り得た。
- **修正:** top-level `master` object に volume(dB) と mute を保存し、存在する場合だけ復元する。旧形式には既定値を維持する。
- **価値:** composition の保存・再読込後も Master の出力設定を保持できる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioRingBuffer clear barrier の新音声保護

- **関連:** `ArtifactCore/src/Audio/AudioRingBuffer.cppm`、`AudioRingBuffer::clear()` / `read()`
- **事実:** consumer は clear 通知を受けると live な `writeCount_` まで read position を進めていたため、clear 後に producer が書いた新しい音声も同じ read で破棄し得た。
- **修正:** clear 時点の write count を atomic に保存し、その時点までだけ read position を進める。
- **価値:** seek/stop 後の新しい音声が clear race で消え、再生開始が一度余計に underflow する経路を狭める。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — clear 後の producer-side refill 可視性

- **関連:** `ArtifactCore/src/Audio/AudioRingBuffer.cppm`、`AudioRenderer::clearBuffer()`、`ArtifactPlaybackEngine::updateAudio()`
- **事実:** clear 通知の consumer 処理前は `readCount_` が古いままなので、producer の `available()` / `write()` が旧音声を占有中と誤認し、seek 後の refill を遅らせ得た。
- **修正:** clear snapshot を logical read position として occupancy/capacity 計算にも使う。SPSC の readCount 所有権は維持する。
- **価値:** seek/stop 後に新しい音声を即座に ring buffer へ再充填でき、consumer の clear callback を待つ余分な無音窓を減らせる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — QtAudioBackend start failure propagation

- **関連:** `ArtifactCore/src/Audio/QtAudioBackend.cppm`、`QtAudioBackend::start()`、`AudioRenderer::start()`
- **事実:** Qt backend は `QAudioSink::start()` 後の error/state を確認せず `active_` を true のままにでき、sink 起動失敗でも上位 renderer が active と判断し得た。
- **修正:** start 直後に `QAudio::NoError` と `StoppedState` を確認し、失敗時は active/callback を戻して sink を停止する。
- **価値:** Qt audio device の起動失敗が「再生中だが音が出ない」状態として隠れにくくなる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — WASAPI GetBuffer の空きフレーム契約

- **関連:** `ArtifactCore/src/Audio/WASAPIBackend.cppm`、`WASAPIBackend::Impl::renderLoop()`
- **事実:** render loop は `padding` 後の空き `framesToWrite` を計算しているのに `GetBuffer(bufferFrameCount)` を要求し、さらに返却領域へ padding offset を加えていた。WASAPI の write cursor 契約では空きフレーム数だけを取得し、返却 pointer の先頭へ書く必要がある。
- **修正:** `GetBuffer(framesToWrite)` に変更し、padding の memset/offset を削除して取得領域へ直接 callback する。
- **価値:** 通常の partial padding 状態で buffer acquire が失敗して音声が更新されない経路と、誤 offset 書き込みを防ぐ。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — WASAPI requestStop の thread lifetime

- **関連:** `ArtifactCore/src/Audio/WASAPIBackend.cppm`、`WASAPIBackend::requestStop()`、`Impl::stopThread()`
- **事実:** requestStop は render thread を detach して返るため、直後の close/destructor が COM audio client を release する前に thread が残る可能性があった。再 start も joinable thread の上書きになり得た。
- **修正:** requestStop は停止通知と AudioClient::Stop だけを行い thread を joinable のまま保持し、start 時に残 thread を reap してから新 thread を生成する。
- **価値:** stop/close/restart の resource lifetime を明示し、detached render thread による use-after-release と thread assignment failure を防ぐ。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — WASAPI exclusive format state alignment

- **関連:** `ArtifactCore/src/Audio/WASAPIBackend.cppm`、`WASAPIBackend::open()`、exclusive `WAVEFORMATEX`
- **事実:** exclusive mode は 2ch Float32 の `exclusiveFmt` を `Initialize()` に渡すが、内部 `mixChannels` / `mixSampleRate` / format flag は shared mix format のままだった。多チャンネル device では render callback の channel 数が実 buffer と不一致になり得た。
- **修正:** exclusive initialize 成功後に、実際の exclusive format を内部 render state と `currentFormat` の元データへ反映する。
- **価値:** exclusive render の callback 書き込み幅と WASAPI buffer の frame layout が一致する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — QtAudioBackend active query synchronization

- **関連:** `ArtifactCore/src/Audio/QtAudioBackend.cppm`、`QtAudioBackend::isActive()`、`active_`
- **事実:** `start()` / `stop()` / `readData()` は mutex で `active_` を扱う一方、`isActive()` だけが生読みしていたため、renderer の状態 query と stop の同時実行で data race になり得た。
- **修正:** `isActive()` も同じ mutex を取得してから `active_` を返すようにした。
- **価値:** audio backend の active 判定が停止処理と同じ同期規約になり、上位 renderer が競合した状態を観測する経路を減らせる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioDownMixer layout-shape guard

- **関連:** `ArtifactCore/src/Audio/AudioDownMixer.cppm`、`AudioDownMixer::process()`
- **事実:** conversion の early return は `AudioChannelLayout` の enum 値だけを比較していたため、`Stereo` と記録された channel count 不一致の入力をそのまま返し得た。AudioRenderer はその場合に出力先 channel 数だけを読み、余剰チャンネルを暗黙に落とす。
- **修正:** 既知 layout の期待 channel 数も一致した場合だけ early return するようにした。形状が不一致なら既存の変換/fallback 経路へ進む。
- **価値:** metadata と実データの channel 数がずれた音声で、downmix の責務を bypass してしまう経路を減らせる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioBus channel-shape normalization gate

- **関連:** `ArtifactCore/src/Audio/AudioBus.cppm`、`AudioBus::addInput()` / `addSideChain()`
- **事実:** AudioBus は input と bus の layout enum が異なる場合だけ downmix していたため、同じ layout 名でも channel 数が異なる segment は、入力 channel 数と buffer channel 数の最小値だけを加算して余剰 channel を暗黙に落としていた。
- **修正:** main buffer / side-chain buffer の期待 channel 数も比較し、不一致時は既存の AudioDownMixer 経路へ送る。
- **価値:** routing の main/side-chain 両方で metadata と実データの不一致を同じ normalization 契約で処理できる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioBus per-channel frame bound

- **関連:** `ArtifactCore/src/Audio/AudioBus.cppm`、`AudioBus::addInput()` / `addSideChain()`
- **事実:** 両 routing 経路は `AudioSegment::frameCount()`（先頭 channel の長さ）だけを上限に使っていたため、短い channel を含む malformed input で `src[i]` が channel の実サイズを越え得た。
- **修正:** 各 channel の source/destination サイズも含めた最小値までで加算するようにした。
- **価値:** 入力 channel 間の frame 数が不一致でも範囲外読みを防ぎ、main と side-chain の安全性をそろえる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Playback segment transform channel bounds

- **関連:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`、`resampleAudioSegment()` / `timeScaleAudioSegment()`
- **事実:** 両 helper は先頭 channel の `frameCount()` を全 channel に使っていたため、channel ごとの frame 数が不一致な segment で補間元を範囲外参照し得た。
- **修正:** channel ごとの実長を補間上限に使い、空 channel は出力を zero-fill する。
- **価値:** playback の resample / speed transform が malformed audio segment でも安全に出力を生成し、下流の入力 guard だけに依存しない。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Playback resample interpolation clamp

- **関連:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`、`resampleAudioSegment()`
- **事実:** channel ごとの短い入力に対して index を clamp しても、補間係数は元の source position から計算されるため、末尾で 1.0 を超える外挿値になり得た。
- **修正:** source position 自体を channel の有効範囲に clamp してから index と係数を計算する。
- **価値:** 不均一 channel 長の resample が末尾で過大な外挿サンプルを生成する経路を防ぐ。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioDownMixer multichannel target mapping

- **関連:** `ArtifactCore/src/Audio/AudioDownMixer.cppm`、`AudioDownMixer::process()`、`AudioBus` multichannel routing
- **事実:** AudioBus は `Surround51/Surround71` を target layout に設定するが、downmixer は Mono/Stereo target しか処理せず、5.1↔7.1 の layout mismatch で空の output segment を返し得た。
- **修正:** surround target 用に標準 channel order の mapping を追加し、7.1→5.1 は back channel を surround に fold、足りない channel は silence にする。
- **価値:** multichannel bus の layout 変換で入力全体が無音化する経路を防ぎ、5.1/7.1 routing を保持する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioDownMixer per-channel sample safety

- **関連:** `ArtifactCore/src/Audio/AudioDownMixer.cppm`、Stereo/Mono downmix branches
- **事実:** 既存の 5.1/7.1→Stereo、Mono→Stereo、multi-channel→Mono 分岐は先頭 channel の frame 数を全 channel に適用し、短い channel で範囲外読みになり得た。
- **修正:** channel ごとの sample access を安全な zero-padding helper に統一し、Stereo output も先に zero 初期化する。
- **価値:** malformed または部分的な audio segment が downmix 中に範囲外読みや未初期化出力を生まない。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioDownMixer mono surround placement

- **関連:** `ArtifactCore/src/Audio/AudioDownMixer.cppm`、Surround51/71 target mapping
- **事実:** surround target の一般 channel copy は Mono 入力を channel 0 にだけ配置していたため、Mono 音声の右出力が無音になっていた。
- **修正:** Mono 入力を Stereo downmix と同じ dual-mono 方針で L/R に複製する。
- **価値:** Mono source を multichannel bus に送った際の左右定位を一貫させる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioDownMixer uniform-shape early return

- **関連:** `ArtifactCore/src/Audio/AudioDownMixer.cppm`、`AudioDownMixer::process()` early return
- **事実:** same layout / channel count だけで変換を省略していたため、channel ごとの frame 長が不揃いな segment が malformed のまま caller に返り得た。
- **修正:** 全 channel の長さが先頭 channel の frame count と一致する場合だけ no-op return する。
- **価値:** 不均一 segment は既存の zero-padding 付き変換経路を通り、downmixer の出力契約が一貫する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Composition audio input guard

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactAbstractComposition::getAudio()`
- **事実:** 各 audio layer は不正な frameCount/sampleRate を拒否していたが、Composition の集約入口には guard がなく、invalid request が mixer buffer 初期化や layer traversal まで進み得た。失敗時の outSegment も stale のまま残り得た。
- **修正:** frameCount/sampleRate が正でない場合は outSegment を clear して false を返す。
- **価値:** composition-level audio API の入力契約を各 layer とそろえ、前回結果の誤再利用を防ぐ。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — ArtifactAudioLayer mono interpolation source

- **関連:** `Artifact/src/Layer/ArtifactAudioLayer.cppm`、`ArtifactAudioLayer::getAudio()`
- **事実:** Mono source の resample loop は `s1`（次の補間点）にも `base0` を使っていたため、フレーム間の線形補間が実質的に無効だった。
- **修正:** 次フレームの `base1` を参照するようにした。末尾では従来どおり zero fallback を使う。
- **価値:** mono 音声の sample-rate 変換が時間軸に沿って滑らかに補間される。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Video audio output sample-rate contract

- **関連:** `ArtifactCore/include/Media/MediaPlaybackController.ixx`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`Artifact/src/Layer/ArtifactVideoLayer.cppm`
- **事実:** FFmpeg audio decoder は既定で 44.1 kHz の S16 stereo PCM を返す一方、`ArtifactVideoLayer::getAudio()` は要求された sample rate を `AudioSegment` に設定するだけで、PCM 自体を変換していなかった。
- **修正:** `MediaPlaybackController::setAudioOutputSampleRate()` を追加し、video layer が要求レートを decoder の resampler に設定する。レート変更時は既存 PCM buffer を破棄して source position から再シークする。
- **価値:** 48 kHz 再生・波形・書き出しで、PCM の実レートと `AudioSegment::sampleRate` が不一致になる経路を防ぐ。
- **未検証:** 実ビルド・テストは未実行。C++20 module の依存スキャンも未実行。

### 2026-08-10 — FFmpeg resampler output capacity

- **関連:** `ArtifactCore/src/Media/MediaAudioDecoder.cppm`、`MediaAudioDecoder::decodeFrameDetailed()` / `flushAndGetRemaining()`
- **事実:** resampler の出力 capacity と flush 残量計算に入力 frame 数・44.1 kHz・固定 stereo byte 数を使っていたため、入力／出力レート変更時に末尾サンプルを取りこぼす、または buffer 見積りが不足する可能性があった。
- **修正:** `swr_get_out_samples()` と現在の出力 bytes-per-sample を使って、通常 decode と flush の capacity を計算するようにした。
- **価値:** 可変 sample-rate の audio decoder で出力 buffer の見積りを resampler の実際の出力契約に合わせる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — FFmpeg packet multi-frame drain

- **関連:** `ArtifactCore/src/Media/MediaAudioDecoder.cppm`、`MediaAudioDecoder::decodeFrameDetailed()`
- **事実:** `avcodec_send_packet()` の後に `avcodec_receive_frame()` を一度しか呼んでいなかったため、1 packet から複数 audio frame を返す codec では後続 frame が破棄され得た。
- **修正:** `EAGAIN` / `EOF` まで receive を繰り返し、各 frame の resampled PCM を一つの `AudioDecodeResult` に連結する。
- **価値:** packet 境界に依存せず、デコーダが返した全 audio frame を上位の audio buffer に渡せる。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Codec-name audio initialization contract

- **関連:** `ArtifactCore/src/Media/MediaAudioDecoder.cppm`、`initializeByCodecName()`
- **事実:** codec-name 初期化は codec context を開いた後に `initialized_` だけを true にし、decode が必須とする `SwrContext` を作っていなかったため、その経路だけ decode が常に未初期化扱いになっていた。
- **修正:** resampler setup を完了できた場合だけ初期化成功とし、失敗時は codec context を解放して false を返す。
- **価値:** 初期化成功状態と decode 可能状態の不一致をなくす。
- **未検証:** 実ビルド・テストは未実行。codec 名だけで入力フォーマットが決まらない場合は、明示的な codec parameters 初期化が必要。

### 2026-08-10 — Audio decoder seek flush

- **関連:** `ArtifactCore/src/Media/MediaAudioDecoder.cppm`、`MediaAudioDecoder::flush()`
- **事実:** codec buffer の flush だけでは `SwrContext` 内の遅延 PCM が残るため、seek／stop 後の最初の audio packet に旧位置のサンプルが混入し得た。
- **修正:** codec flush 後に resampler を再生成し、現在の出力 sample-rate／format 設定を保ったまま遅延状態をリセットする。
- **価値:** seek・stop 境界で古い音声が再生される経路を抑止する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Delayed audio codec packet handling

- **関連:** `ArtifactCore/src/Media/MediaAudioDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`
- **事実:** audio decoder は packet を正常に受理しても codec delay のためその packet では PCM を返さないことがある。controller は空 QByteArray を即時返却していたため、遅延 codec の出力待ち packet を音声終了／欠落として扱い得た。
- **修正:** no-output でも decode 成功を示し、controller が最大 32 packet まで追加取得して PCM を待つ bounded loop を追加した。
- **価値:** codec delay による先頭・途中の音切れを抑えつつ、reader 呼び出しを無制限にしない。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioWriter channel alignment

- **関連:** `ArtifactCore/src/Audio/AudioWriter.cppm`、`AudioWriter::write()`
- **事実:** WAV header は最初の segment の channel count を保持するが、後続 segment の channel 数が少ない場合は実際の interleaved sample 数も減っていたため、frame alignment と data size が header と不一致になり得た。
- **修正:** header の channel 数を毎 frame 必ず書き、不足 channel は zero-fill、余分な channel は従来どおり無視する。
- **価値:** mono/stereo が混在する音声書き出しでも WAV の block alignment と再生時間を維持する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — AudioSyncTools uneven channel safety

- **関連:** `Artifact/src/Audio/ArtifactAudioWaveform.cppm`、`AudioSyncTools::timeStretch()` / `fadeOut()`
- **事実:** 両処理が先頭 channel の frame 数を全 channel に適用し、短い channel の読み書きで範囲外アクセスになり得た。
- **修正:** time-stretch は channel ごとに source index を clamp し、空 channel は zero-fill。fade-out は channel 実長の範囲だけ処理する。
- **価値:** malformed／部分的な AudioSegment を波形編集処理へ渡しても範囲外アクセスを避ける。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Audio analysis/effect segment guards

- **関連:** `ArtifactCore/src/Audio/AudioAnalyzer.cppm`、`ArtifactCore/src/Audio/AudioHighLowPass.cppm`
- **事実:** Analyzer と HighLowPass は先頭 channel の frame 数を全 channel に適用し、短い channel を範囲外参照し得た。HighLowPass は sampleRate 0 の係数計算も防いでいなかった。
- **修正:** channel ごとの available frame 数で処理し、空 channel は skip、sampleRate 不正時は effect を早期 return する。
- **価値:** malformed AudioSegment の解析・filter 処理で範囲外読みと不正係数を防止する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Audio effect uneven channel safety

- **関連:** `ArtifactCore/src/Audio/AudioChorus.cppm`、`AudioReverb.cppm`、`AudioStereoMixer.cppm`、`AudioBassTreble.cppm`
- **事実:** 4 effect が先頭 channel の frame 数を全 channel に適用し、短い channel を範囲外参照し得た。Chorus／BassTreble は sampleRate 0 も係数・LFO 計算に進み得た。
- **修正:** channel ごとの実長で処理範囲を clamp し、空 channel を skip、sampleRate 不正時は rate 依存 effect を早期 return する。
- **価値:** effect rack に不均一または不正な AudioSegment が入っても範囲外読みを防止する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Audio delay/EQ/tone segment guards

- **関連:** `ArtifactCore/src/Audio/AudioDelay.cppm`、`AudioParametricEQ.cppm`、`AudioTone.cppm`
- **事実:** 3 effect が先頭 channel の frame 数を全 channel に適用していた。Delay は不正な delay／sampleRate で zero-size ring-like buffer または modulo 0 に至り得た。
- **修正:** channel ごとの実長で処理し、rate 依存処理の不正 sampleRate を拒否、Delay buffer は最低 1 sample を確保する。
- **価値:** 追加の effect rack 経路でも malformed AudioSegment と不正パラメータによる範囲外／ゼロ除算を防止する。
- **未検証:** 実ビルド・テストは未実行。

### 2026-08-10 — Artifact audio effects channel alignment

- **関連:** `Artifact/src/Audio/Effects/DelayEffect.cppm`、`ReverbEffect.cppm`、`CompressorEffect.cppm`、`LimiterEffect.cppm`、`DistortionEffect.cppm`、`EqualizerEffect.cppm`、`ChorusEffect.cppm`
- **事実:** Artifact 側の effect 群は先頭 channel の sample 数を全 channel に使い、短い channel の読み越しが起こり得た。rate 依存 effect は無効な sample rate でも係数計算へ進み得た。
- **修正:** L/R effect は L/R の共通長、全 channel effect は全 channel の最短長で処理し、invalid rate を早期 return する。
- **価値:** Artifact 側の effect rack でも malformed AudioSegment の範囲外アクセスを防ぐ。
- **未検証:** 実ビルド・テストは未実行。
### 2026-08-10 — AudioSpectrum の空入力・bin 数境界
- **関連:** `ArtifactCore/include/Audio/AudioSpectrum.ixx`、`ArtifactCore/src/Audio/AudioSpectrum.cppm`
- **事実:** `setBins()` は負数を受け入れており、次回の `resize()` で巨大な `size_t` に変換され得た。また空セグメントでは前回の解析結果を保持し、空入力の DFT は 0 除算になり得た。
- **修正:** bin 数を 1 以上に制限し、空入力時に解析状態をクリア、DFT の空入力・不正サイズをゼロ結果で終了するようにした。
- **価値:** malformed / empty audio segment でのメモリ急増・NaN・古い spectrum 表示を防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — Audio panner / formant analyzer の境界修正
- **関連:** `ArtifactCore/src/Audio/AudioPanner.cppm`、`ArtifactCore/src/Audio/FormantExtractor.cppm`
- **事実:** Panner は channel 0 の frame 数を短いチャンネルにも適用していた。Formant の peak 探索は最終 bin の `i + 1` を参照し、track 解析は不正 sample rate で解析幅が 0 になり得た。
- **修正:** チャンネル実長で書き込みを制限し、peak の探索終端を `size - 2` に制限、不正 rate とチャンネル共通長を検証してから frame を切り出すようにした。
- **価値:** malformed audio による範囲外アクセスと解析ループ不全を防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — Audio Tone / Delay parameter sanitization
- **関連:** `ArtifactCore/src/Audio/AudioTone.cppm`、`ArtifactCore/src/Audio/AudioDelay.cppm`
- **事実:** 公開 parameter setter／JSON 復元値が有限値・範囲内である保証なしに処理へ入り、Tone は NaN 周波数・振幅や不正 enum、Delay は NaN／巨大 delay の整数変換と feedback／mix を直接使用していた。
- **修正:** 処理直前に有限値と実用範囲へ正規化し、Tone の位相を常に `[0,1)` へ折り返し、不正 waveform は無音、Delay の delay sample 数を buffer 契約内へ制限した。
- **価値:** 不正な外部パラメータで NaN 拡散や整数オーバーフローを防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — Audio Chorus / Reverb / BassTreble の不正値防護
- **関連:** `ArtifactCore/src/Audio/AudioChorus.cppm`、`AudioReverb.cppm`、`AudioBassTreble.cppm`
- **事実:** Chorus は NaN／巨大 delay を整数変換し、Reverb の decay／mix と BassTreble の dB 値は非有限値のまま DSP 演算へ入っていた。Chorus の ring position は segment ごとの必要長変化にも依存していた。
- **修正:** 各値を有限値・既存 UI 範囲へ正規化し、Chorus の delay を buffer 契約内に制限、segment 縮小時に write position を再正規化した。
- **価値:** malformed parameter による範囲外アクセス、NaN 音声、整数変換不定動作を防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — AudioRingBuffer の容量・チャンネル実長契約
- **関連:** `ArtifactCore/src/Audio/AudioRingBuffer.cppm`
- **事実:** capacity 0 が設定可能で、後続の modulo／free-space 計算が不正になり得た。write は channel 0 の frame 数を全チャンネルへ適用し、短い channel の source を `frames` 分 memcpy していた。
- **修正:** capacity を最低 1 に正規化し、available/free space を上限保護、チャンネルごとの実長だけをコピーして不足部分をゼロ埋めするようにした。
- **価値:** producer 側の範囲外読み取りと容量境界による未定義動作を防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — AudioWriter WAV 固定幅ヘッダ計算
- **関連:** `ArtifactCore/src/Audio/AudioWriter.cppm`
- **事実:** `byteRate` と 24-bit の `blockAlign` を固定幅整数で乗算してから格納しており、極端な sample rate／channel 数で wrap した WAV ヘッダを生成し得た。
- **修正:** 64-bit 中間値で上限検証し、書き出し開始時にも表現可能な channel 数へ制限した。
- **価値:** 音声データ本体を出力できても、ヘッダだけ壊れるケースを防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — AudioPreviewWidget の短い channel 切り出し
- **関連:** `Artifact/src/Widgets/AudioPreviewWidget.cppm` の `onTimerTick()`
- **事実:** preview chunk は channel 0 の frame 数を全 channel に適用し、短い channel で `constData()+offset+chunkSize` を生成して範囲外を参照し得た。
- **修正:** chunk 長は維持し、各 channel の実長だけ `copy_n` し、不足分をゼロ埋めするようにした。
- **価値:** malformed／部分的な AudioSegment でも preview の時間進行を壊さず安全に再生できる。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — Playback audio time-scale の極小速度境界
- **関連:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` の `timeScaleAudioSegment()`／`updateAudio()`
- **事実:** 最小速度 `0.0001` では停止判定を通過し、`sourceFrames / speed` の巨大値を int 化して大きな出力 buffer を確保し得た。
- **修正:** 極小速度を `<=` で停止扱いにし、target frame 数を有限値・`int` 上限確認後に変換するようにした。
- **価値:** 異常な再生速度での整数変換不定動作とメモリ急増を防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — AudioWaveform range slice の加算 overflow
- **関連:** `Artifact/src/Audio/ArtifactAudioWaveform.cppm` の `generateRange()`
- **事実:** `startSample + sampleCount` を先に計算してから mono buffer 長へ clamp しており、巨大な qint64 入力で加算 overflow が起き得た。
- **修正:** 利用可能長から safe start と remaining を求め、残量との `min` で end を算出するようにした。
- **価値:** waveform range 要求の異常値で負の slice 長や不正な切り出しを防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — AudioAnalyzer の FFT／band 境界
- **関連:** `Artifact/src/Audio/ArtifactAudioWaveform.cppm`
- **事実:** FFT size と hop size に下限がなく、size 0/負数の resize や size 1 の window denominator 0 が発生し得た。`computeBands()` は invalid sample rate を bin index に変換していた。spectrogram の start 乗算も int overflow 余地があった。
- **修正:** FFT/hop を最低値へ制限し、window denominator を保護、spectrogram start を qint64 化、band の sample rate／bin width を検証するようにした。
- **価値:** malformed analyzer settings や audio metadata での巨大確保・NaN・範囲外 index を防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — AudioSyncTools の rate／tempo／normalize 境界
- **関連:** `Artifact/src/Audio/ArtifactAudioWaveform.cppm`
- **事実:** `timeStretch()` は非有限・極小 rate を int frame 数へ変換し得た。`detectTempo()` は invalid sample rate で bpm=0 のまま while を回し続け、`normalize()` は NaN／極端 target dB をそのまま gain 計算へ渡していた。
- **修正:** rate と target frame 数を検証し、invalid sample rate は既定 tempo を返し、target dB を有限・実用範囲へ制限した。
- **価値:** malformed audio metadata／sync parameter による無限ループ、巨大確保、NaN 音声を防ぐ。
- **未検証:** ビルド・テストは未実行。
### 2026-08-10 — Core DSP helper の delay／phase 境界
- **関連:** `ArtifactCore/include/Audio/DSP/DelayLine.ixx`、`LFO.ixx`、`AllPassFilter.ixx`
- **事実:** DelayLine は invalid delay を int index 化し、LFO は invalid sample rate／frequency で phase を壊し得た。AllPassFilter は invalid delay／feedback をそのまま使用していた。
- **修正:** delay／rate／feedback を有限値・有効範囲へ正規化し、過大 buffer 要求を拒否、LFO phase を周期内へ折り返すようにした。
- **価値:** 低レベル DSP から NaN、範囲外 index、異常な buffer 確保が上位 effect へ伝播するのを防ぐ。
- **未検証:** ビルド・テストは未実行。
## 2026-08-10 — Waveform display width must be bounded by source frames

- **関連:** `Artifact/src/Audio/ArtifactAudioWaveform.cppm` / `AudioWaveformGenerator::generate`
- **事実:** 表示幅が音声フレーム数を超えても、生成ループが作れる波形ビン数はフレーム数までだった。一方で、要求幅をそのまま `QVector` のサイズと `reserve(width * 2)` に使っていた。
- **判断:** 波形出力幅を `min(displayWidth, numSamples)` に制限し、不要な確保と `int` の積のオーバーフロー余地を抑えた。
- **価値/懸念:** 極端な viewport 幅や外部入力でのメモリ浪費を防ぐ。表示側が要求幅と `WaveformData::width` の一致を前提にしている場合は、必要に応じて別途確認する。
- **次に確認:** 大きな表示幅を指定した既存の waveform consumer が `peaks.size()` と `width` のどちらを描画幅として使うかを確認する。
## 2026-08-10 — Level meter peak state must be symmetric for mono and stereo

- **関連:** `Artifact/src/Audio/ArtifactAudioWaveform.cppm` / `AudioLevelMeter`
- **事実:** mono 処理は左ピークだけを更新し、stereo 処理は RMS レベルだけを更新していたため、ピーク保持値とクリップ通知が入力経路によって不一致だった。`reset()` も保持カウンターを初期化していなかった。
- **判断:** 既存のピーク保持ロジックを左右共通のローカル処理に揃え、stereo でもクリップ通知を行い、reset 時にカウンターをゼロ化した。
- **価値/懸念:** AudioPreview のピーク表示とクリップ状態が mono/stereo で一貫する。attack/release の設定値は既存どおり別途平滑化に使われていないため、今回の範囲では変更していない。
- **次に確認:** `AudioLevelMeter` の consumer が peak 値を直接表示しているか、また attack/release の期待仕様があるかを確認する。
## 2026-08-10 — Audio level bar should sanitize dB values at the paint boundary

- **関連:** `Artifact/src/Widgets/AudioPreviewWidget.cppm` / `AudioLevelBarWidget`
- **事実:** レベル値を下限だけで `std::max` しており、`+∞` や有限の 0 dB 超がそのまま ratio と整数ピクセル幅の計算へ進む可能性があった。
- **判断:** dB 値を有限値へ補正し、-60〜0 dB にクランプしてから ratio を計算するようにした。
- **価値/懸念:** オーディオ入力異常があってもバー描画の整数変換や矩形範囲が壊れにくい。クリップ判定そのものは AudioLevelMeter 側の責務として変更していない。
- **次に確認:** 実 UI で 0 dB 超のピークをクリップ色として表現する仕様が必要か確認する。
## 2026-08-10 — Unbalanced CBuffer packing leaked into later module types

- **関連:** `ArtifactCore/include/Graphics/CBuffer/Constants.ixx`, `Graphics.LayerBlendPipeline`
- **事実:** `LineVertex`、`RectVertex`、`DrawSpriteConstants` の `#pragma pack(push,1)` に対応する `pop` がなく、同じ翻訳単位／モジュールで後続の構造体へ 1-byte packing が継承されていた。`MatteTrackParams` は定義上12個の4-byte要素だが、44 byteとして評価されていた。
- **判断:** 各ローカルな packing 範囲を `#pragma pack(pop)` で閉じた。`MatteTrackParams` のフィールドや HLSL は変更していない。
- **価値/懸念:** C++ constant buffer の 16-byte register 境界が復元され、提示された static assertion の失敗原因を除去する。既存の各 CBuffer 型の意図した packed layout には影響しない。
- **次に確認:** ビルド環境で `LayerBlendPipeline.ixx` の static assertion と関連モジュール依存スキャンを確認する（この環境ではビルド禁止のため未実行）。
## 2026-08-10 — Downmixer must not propagate non-finite controls or samples

- **関連:** `ArtifactCore/src/Audio/AudioDownMixer.cppm`
- **事実:** mix level setters accepted NaN/∞, and direct channel-map/copy paths forwarded non-finite PCM samples. The arithmetic downmix path could therefore emit invalid samples into later mixer stages.
- **判断:** invalid mix levels fall back to their existing defaults; copied and sampled values are converted to zero when non-finite. Finite values and existing gain ranges are otherwise preserved.
- **価値/懸念:** DownMixer becomes a stable boundary for malformed decoded/input audio without changing normal channel mapping behavior.
- **次に確認:** Runtime audio paths should verify that downstream meters and renderers receive finite PCM after conversion; build/runtime verification remains pending.
## 2026-08-10 — Audio cache memory statistics need size_t-safe arithmetic

- **関連:** `ArtifactCore/src/Audio/AudioCache.cppm` / `AudioCache::getMemoryUsage`
- **事実:** frame count と channel count は `int` のまま乗算されてから `size_t` に変換されるため、統計計算だけが 32-bit 整数 overflow を起こす可能性があった。エントリ総和の `size_t` overflow も未処理だった。
- **判断:** 各項目と総和を `size_t` で検査し、表現できない場合は最大値へ飽和させるようにした。
- **価値/懸念:** キャッシュ統計が wraparound して小さく見えることを防ぐ。実際のキャッシュ容量制限や音声データ配置は変更していない。
- **次に確認:** キャッシュ上限がフレーム数基準で妥当かは、runtime のメモリ使用量計測で別途確認する。
## 2026-08-10 — Lip-sync WAV conversion must bound QVector frame sizes

- **関連:** `ArtifactCore/src/Audio/LipSyncTrack.cppm`
- **事実:** WAV の frame 数は `qint64` で取得できるが、音声チャンネルの `QVector` は `int` サイズへキャストして resize していた。また `analyzeFromFile` の NaN frame rate は単純な `<= 0` 判定を通過していた。
- **判断:** frame 数を `QVector` の最大 `int` サイズ以内に限定し、frame rate に有限値チェックを追加した。
- **価値/懸念:** 巨大な WAV や不正な解析パラメータでの不正サイズ化を防ぐ。通常サイズの lip-sync 解析とイベント形式は変更していない。
- **次に確認:** 長時間音声を扱う場合は、必要なら chunked lip-sync analysis を別設計として検討する。
## 2026-08-10 — Formant analysis needs bounded frequency and frame conversions

- **関連:** `ArtifactCore/src/Audio/FormantExtractor.cppm`
- **事実:** frequency/bin の直接 `float`→`int` 変換と、`sampleRate / frameRate` の直接 `double`→`int` 変換があり、極端な値で範囲外変換の余地があった。閾値 setter も非有限・逆順の範囲を受け入れていた。
- **判断:** bin をスペクトル範囲へ clamp し、解析フレーム幅を finite/`int` 範囲内で検査し、無効な閾値更新を無視するようにした。
- **価値/懸念:** 通常のフォルマント解析を維持しながら、極端な解析設定による未定義な整数変換を避ける。
- **次に確認:** 長時間・低 frame-rate の lip-sync 解析は chunk 単位の設計が必要かを別途確認する。
## 2026-08-10 — Rasterizer and analyzer arithmetic must stay outside int overflow

- **関連:** `ArtifactCore/src/Audio/AudioRasterizer.cppm`, `ArtifactCore/src/Audio/AudioAnalyzer.cppm`
- **事実:** waveform bin の `bin * sampleCount` と interleaved bin の同等計算は `int` 掛け算だった。Analyzer の RMS 分母も `frames * channels` の `int` 計算で、無効 sample rate は周波数 bin のゼロ除算を起こし得た。
- **判断:** bin index は 64-bit 中間値で計算し、Analyzer の RMS は double 分母・累積を使う。非有限 PCM は無視し、frequency/sample-rate 入力を検証する。
- **価値/懸念:** 大きな波形データや malformed PCM でも解析の index・RMS・band intensity が wraparound/NaN になりにくい。通常の有限入力の結果は同じ計算意図を維持する。
- **次に確認:** runtime で巨大 waveform の rasterization と無効 sample-rate analyzer の呼び出し元契約を確認する。
## 2026-08-10 — Audio backend callbacks must respect the int frame contract

- **関連:** `ArtifactCore/src/Audio/WASAPIBackend.cppm`, `ArtifactCore/src/Audio/QtAudioBackend.cppm`
- **事実:** `AudioCallback` の frame 引数は `int` だが、WASAPI の `UINT32` frame 数を無検査で cast していた。Qt の Int16 path も `frames * channels` を `QVector` の int サイズへ直接渡していた。
- **判断:** callback frame 数と temporary buffer の sample 数を int 表現可能な範囲へ制限し、WASAPI で処理できない残余フレームは無音で埋めるようにした。
- **価値/懸念:** 異常に大きい backend buffer でも callback 引数や一時バッファのサイズが wraparound しない。通常のデバイス buffer では従来どおり全フレームを処理する。
- **次に確認:** 実デバイスで backend buffer と callback frame 数の runtime 契約を確認する（ビルド・実行は未実施）。
## 2026-08-10 — WASAPI exclusive format selection needs matching integer types

- **関連:** `ArtifactCore/src/Audio/WASAPIBackend.cppm`
- **事実:** exclusive mode の channel 数選択が `std::min(2, UINT32)` になっており、テンプレート引数を推論する MSVC では異なる型のため成立しない。
- **判断:** `std::min<UINT32>(2u, ...)` として device channel count と同じ型で比較するようにした。
- **価値/懸念:** WASAPI exclusive open 経路のコンパイル互換性を改善する。channel 数の上限や実際の exclusive format は変更していない。
- **次に確認:** WASAPI/Qt backend の runtime open は実デバイス依存のため、ビルド・実機確認が必要。
## 2026-08-10 — AudioRenderer callback needs finite PCM and size-safe output clearing

- **関連:** `ArtifactCore/src/Audio/AudioRenderer.cppm`
- **事実:** backend callback の buffer/frame/channel 引数は無検査で `memset` の int 掛け算へ入り、ring buffer 由来の非有限 sample は `std::clamp` へ直接渡されていた。
- **判断:** null/非正 callback 引数を早期終了し、出力 sample 数を `size_t` で計算する。非有限 sample と volume は無音として扱う。
- **価値/懸念:** 再生出力での範囲外書き込み・NaN PCM の伝播を防ぐ。通常の有限 PCM と volume の挙動は維持する。
- **次に確認:** callback 呼び出し元が常に有効 buffer を渡すこと、RT thread での sanitization コストが許容範囲かを runtime で確認する。
## 2026-08-10 — Mixer UI must sanitize volume, pan, and meter inputs

- **関連:** `Artifact/src/Audio/ArtifactAudioMixer.cppm`
- **事実:** channel/master volume と pan は NaN を `std::clamp` へ渡せ、meter 更新も NaN dB を peak/max と signal へ伝播させていた。
- **判断:** NaN/∞ の volume・pan は既定値へ戻し、meter dB は有限値かつ -60〜6.02 dB に正規化してから状態・signal・peak に使う。
- **価値/懸念:** 不正な layer または renderer 値で mixer UI が NaN 表示や stale peak 状態にならない。通常の有限値の範囲と表示上限は維持する。
- **次に確認:** mixer widget が 6.02 dB 上限を意図した表示仕様として扱っているかを runtime で確認する。
## 2026-08-10 — Artifact delay effects need finite parameter and PCM boundaries

- **関連:** `Artifact/src/Audio/Effects/ChorusEffect.cppm`, `Artifact/src/Audio/Effects/DelayEffect.cppm`
- **事実:** effect parameter setters accepted NaN/∞, and process paths passed non-finite input samples through dry/wet arithmetic. Invalid sample rates were also sent to DSP initialization.
- **判断:** UI-declared parameter ranges are enforced with finite fallbacks; invalid sample rates use 44.1 kHz; non-finite input samples become silence before processing.
- **価値/懸念:** Chorus/Delay no longer turn one malformed control or PCM value into a whole output block of NaNs. Normal parameter ranges and signal flow are preserved.
- **次に確認:** Reverb/Compressor/Limiter/Distortion share similar parameter boundaries and should be audited in subsequent walks.
## 2026-08-10 — Compressor and limiter parameters must stay physically valid

- **関連:** `Artifact/src/Audio/Effects/CompressorEffect.cppm`, `Artifact/src/Audio/Effects/LimiterEffect.cppm`
- **事実:** compressor ratio/attack/release and limiter release/gain were accepted without finite/range checks; invalid values could produce zero/NaN coefficients. Non-finite input samples also entered peak detection.
- **判断:** UI ranges are enforced with finite defaults, invalid limiter sample rates fall back to 44.1 kHz, and input samples are normalized to zero before peak/gain processing.
- **価値/懸念:** malformed controls or PCM no longer poison an entire effect block. Existing valid parameter ranges and effect topology remain unchanged.
- **次に確認:** Reverb and Distortion parameter setters should receive the same audit in the next walk.
## 2026-08-10 — Reverb and distortion controls need finite DSP bounds

- **関連:** `Artifact/src/Audio/Effects/ReverbEffect.cppm`, `Artifact/src/Audio/Effects/DistortionEffect.cppm`
- **事実:** Reverb controls were assigned directly into delay/LFO initialization and per-sample feedback math. Distortion accepted NaN for drive/tone/mix/bit depth/downsample, and non-finite input PCM entered nonlinear functions.
- **判断:** All listed parameters now use their UI ranges with finite defaults; invalid reverb sample rates fall back to 44.1 kHz; non-finite input samples become zero.
- **価値/懸念:** Invalid controls cannot create unbounded buffers, zero-rate divisions, or NaN nonlinear output. Valid effect ranges and algorithm selection remain unchanged.
- **次に確認:** The remaining Equalizer effect should be checked for the same setter and input sanitization pattern.
## 2026-08-10 — Equalizer biquad state needs finite gains and PCM

- **関連:** `Artifact/src/Audio/Effects/EqualizerEffect.cppm`
- **事実:** EQ gain setter accepted NaN, which could make biquad coefficients and every subsequent channel state non-finite. Input channel samples were also used without sanitization.
- **判断:** gain is constrained to its declared -12..12 dB range, coefficient inputs have finite fallbacks, and non-finite PCM is treated as zero before filtering.
- **価値/懸念:** A malformed automation value or sample no longer poisons the EQ block. Normal band frequencies, Q values, and finite gain behavior are unchanged.
- **次に確認:** Remaining Artifact audio effect factories and parameter deserialization should be checked for bypassing these setters.
## 2026-08-10 — Common Artifact effect sample-rate setter needs a valid fallback

- **関連:** `Artifact/include/Audio/Effects/ArtifactAudioEffectBase.ixx`
- **事実:** Compressor と Equalizer は base implementation の `setSampleRate` を継承しており、0 以下の sample rate をそのまま保持していた。個別 effect の process guard があっても getter/state は無効値になり得た。
- **判断:** 基底 setter で正の値だけを受け入れ、無効値は既定の 44.1 kHz に戻すようにした。
- **価値/懸念:** 未 override の Artifact audio effect も同じ sample-rate 契約を共有できる。正の sample rate の挙動は変更しない。
- **次に確認:** 外部 caller が sample rate を設定するタイミングと、device rate への再同期を runtime で確認する。
## 2026-08-10 — Scrub controller needs safe frame delta and settings boundaries

- **関連:** `Artifact/src/Audio/ArtifactAudioScrubController.cppm`
- **事実:** scrub speed used `std::abs(int64_t delta)`, which is undefined at the minimum value; volumeScale accepted NaN through `qBound`; latency converted an unbounded qint64 elapsed time directly to int.
- **判断:** frame deltas are measured with unsigned magnitude, speed is finite/capped, volumeScale falls back to 0.5 when invalid, and latency is clamped to the int range.
- **価値/懸念:** malformed timeline positions, persisted settings, or clock gaps no longer poison scrub volume or diagnostics. Normal drag timing and volume behavior are preserved.
- **次に確認:** Scrub worker lifecycle and composition audio extraction should be checked for thread shutdown and stale composition ownership.
