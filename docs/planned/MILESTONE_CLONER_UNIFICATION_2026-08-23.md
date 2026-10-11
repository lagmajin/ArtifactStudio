# MILESTONE: Cloner 長期統合計画

**最終更新:** 2026-10-12
**ステータス:** In Progress

2026-08-23 のクローナー領域ウォーク（`CloneCore` → `CloneGenerator` → `ArtifactCloneLayer` → `ArtifactCloneEffectSupport`）で判明した課題を、移行フェーズ付きで整理する。

**設計方針（確定）:** クローナーは **エフェクトではなくレイヤーコンポーネントとして扱うのが正**。Effect時代の `CloneGenerator` は互換アダプタに留め、新規オーサリング・実行・UIはすべて Component 経由に統一する。本計画の全フェーズはこの方針に従う。

## Phase 4 — 細部修正バックログ（2026-08-23 実装済み）

| 項目 | 対応 |
|---|---|
| fps ハードコード解除 | `ArtifactCloneEffectSupport.ixx` の `/30.0f` 5箇所を `layer->compositionFrameRate()` 経由に置換済み |
| PoissonDisk 改良 | `CloneGenerator.cppm` Random分布を O(n²) 全走査から uniform spatial hash（3x3x3近傍判定）へ置換済み。64試行のrejection構造は維持 |
| `blendCloneData` 意味論文書化 | `CloneCore.ixx` に各ブレンドモード（Multiply/Max/Min/Average等）の数学的意味と原点基準の注意を追記済み |
| `mt19937` → `RandomStream` 統合 | `ArtifactCloneLayer.cppm` Random分布を `ArtifactCore::RandomStream` へ置換済み。**legacy `CloneGenerator` 側は意図的に mt19937 を維持**（旧プロジェクト出力の互換保護）。乱数シード同一でもレイヤー側の出列順が変わる点に注意 |

## Phase 4.5 — Effector バグ修正と時間供給（2026-08-23 実装済み）

| 項目 | 対応 |
|---|---|
| `RandomCloneEffector::hash` の符号付き整数オーバーフローUB | `AdvancedEffectors.ixx` — uint32 ラップアラウンド演算に置換 |
| Grid / Radial モードで `opacityDecay` が無視される | `ArtifactCloneLayer.cppm` — 両モードに opacityDecay を適用（Grid は clone.index 基準、Radial は i 基準） |
| Radial モードが endAngle に到達しない（Curve との不一致） | `(end-start)/total` → `(end-start)/(total-1)` に変更。回転も Curve と同一の `angle + rotationStep*i` に統一 |
| legacy 経路で identity 変換クローンが消える | `ArtifactCloneEffectSupport.ixx` — offset=0 の1個目等が `isIdentity()` スキップで消える問題を解消（visible のみ判定） |
| `SoundCloneEffector` の色合成で alpha が失われる | `setRgbF(r,g,b)` → alpha 保存版へ |
| **Delay/Noise/Sound Effector が時間パラメータ未供給で実質停止** | `ArtifactCloneLayer::generateCloneData()` が effector 適用前に `currentTime`（timeline連動）/ `deltaTime`（1/fps）を dynamic_cast で供給するようになった。波状伝播・ノイズアニメーションが実際に動く |

### Phase 4.5 の既知残課題

- `SoundCloneEffector` の `audioAmplitude/audioLowBand/...` は未接続（常時0）。`ArtifactAudioLayer` の解析結果（`AudioAnalyzer`）をフレーム毎に流す配線が必要 → Phase 3 の Effector modifier 化と合わせて対応。
- `NoiseCloneEffector::noise()` は sin合成の擬似ノイズで、sampleY/Z が全クローン共通のため実質1D波。パーリンノイズへの差し替え候補。
- Inspector ComponentTab から Delay/Sound/Noise Effector を追加する UI 導線は未整備（`addEffector` API は存在）。
- Radial の終端契約変更により、既存プロジェクトの Radial 配置見た目が変わる（最後のクローンが初めて endAngle に到達）。

## Phase 5 — エフェクターチェーンの実用化（直列→並列→ネスト）

2026-08-23 の追加調査で判明した現状: 実行構造は「effectors_ を順次 applyToClones する直列チェーン」だが、(a) `addEffector()` 呼び出し元ゼロ（追加手段なし）、(b) 保存/復元は front の TransformCloneEffector のみで2個目以降は消える、(c) `blendMode`/`blendCloneData` は呼び出し元ゼロの死蔵機能、(d) 基底 `strength` メンバーをどの実装も参照しない（Inspector の Strength ノブが無効）、(e) クローナーのネスト（CloneLayer を別 CloneLayer のソースに）は未対応。

| サブフェーズ | 内容 | Scope |
|---|---|---|
| **5.1 直列チェーン実用化（2026-08-23 実装済み）** | ① `clone.effectors` 配列シリアライズ実装（typeId タグ: transform/step/random/delay/sound/noise。legacy `clone.effector.*` は書き込み併記＋読み取りフォールバック）。② 基底 `strength` を全6実装の finalWeight に乗算し Inspector ノブを有効化。③ typeId ファクトリで fromJson から任意種を復元 | 完了 |
| **5.2 並列合成（2026-08-23 ランタイム実装済み）** | `generateCloneData()` の effector 適用ループで、`blendMode == Add`（デフォルト）は従来の in-place 変異パスを維持。それ以外のモード（Subtract/Multiply/Max/Min/Average/Normal）はスナップショットに対して候補変形を計算し `blendCloneData(base, candidate, mode, 1.0)` で合成。**契約:** blendCloneData が合成するのは位置・色・weight のみで、非Addモードでは回転/スケールは base 値フォールバック（文書化済み）。UI の blendMode ドロップダウンは未実装（JSON/API で設定可） | ランタイム完了・UI残 |
| **5.3 ネスト（クローナー内クローナー）** | Phase 2（Cloneレイヤー実体描画）の完了が前提。ソース解決が CloneLayer 自身を返しても再帰深度上限（推奨4）で展開。循環参照は解決失敗として無音スキップ | Large |
| **5.4 UI導線（2026-08-23 実装済み）** | Inspector Components セクションに「+ Effector / - Effector」ボタン行を追加。+は6種タイプのQInputDialog選択で `addEffector()`、-は「index: type」一覧から選んで `removeEffector(index)`。ボタンは CloneLayer 選択時のみ表示、カウント表示付き。AbstractCloneEffector に `effectorTypeName()` 仮想関数を追加 | 完了（エフェクター個別のProperty編集面は後続） |

依存順序: 5.1 → 5.4（UIで作れる）→ 5.2（並列）→ 2（ソース描画）→ 5.3（ネスト）→ 3（modifier統合）。

**Phase 5.1 の既知残課題:** Inspector にエフェクター追加 UI が無いため、現状 API/JSON 経由でのみ複数チェーンを作成可能（5.4 で解消）。`blendMode` は保存されるが 5.2 まで実行に反映されない。

## 現状の構造

クローン生成は3系統が並存する。

| 系統 | 実装 | 状態 |
|---|---|---|
| (a) Effect時代の `CloneGenerator` | `src/Generator/CloneGenerator.cppm:345` | 旧プロジェクト互換アダプタのみ（`ArtifactCloneEffectSupport.ixx:804-851`） |
| (b) `ArtifactCloneLayer` 独自生成 | `src/Layer/ArtifactCloneLayer.cppm:324` | CloneMode 6種。描画は矩形プレースホルダ（`:166`） |
| (c) Component generator stack | `clonerComponentInstances`（Support `:633`） | 正となるべき新経路 |

描画統合点は `drawWithClonerEffect`（Support `:894`）。Image/Shape/Text/Video/Solid/SVG 各レイヤーが複製描画に対応済み。

## Phase 1 — 生成系統の一本化（正 = Component）

**目標:** (a)(b) を読み取り専用互換に降格し、新規オーサリングと実行を (c) に統一。

1. `ArtifactCloneLayer::generateCloneData()` を廃止し、内部で `clonerComponentInstances()` に委譲。`settings_` は起動時に Host descriptor へ移行変換（`syncBuiltinBoolsFromHost` と同型の逆方向マッピング）。
2. `CloneMode`(6種) ↔ `DistributionMode`(8種) の対応表を作り、`clone.mode` JSON 読み込みで descriptor settings へ翻訳。欠落モード（Hexagonal/Noise/Spiral/PoissonDisk）は `extraGeneratorDescriptors_` 側へ。
3. legacy Effect アダプタ（Support `:806-851`）に非推奨ログ（`qWarning` 一回り）を追加し、削除予定バージョンを明記。
- **Scope:** Medium / **リスク:** 既存プロジェクトの見た目変化。変換テスト必須。

## Phase 2 — Cloneレイヤーの実体描画

**目標:** `drawSolidRectTransformed` プレースホルダ（`ArtifactCloneLayer.cppm:166`）を、`sourceLayerId` 参照レイヤーの実描画に置換。

1. `sourceLayerId` → コンポジション解決 → ソースレイヤーのラスタ表面取得（既存キャッシュ経路を再利用）。
2. `drawWithClonerEffect` のインスタンス毎に `drawSpriteTransformed`（GPU テクスチャキャッシュ統合は Image/Video レイヤーと同一経路）。
3. MaterialContainer の `sourceIndex` はフレームバリアント選択に対応。
- **Scope:** Medium-Large / **依存:** Phase 1（インスタンス供給元の統一後が安全）。

## Phase 3 — Effector のスコープ非依存化

**目標:** `AbstractCloneEffector`（`CloneCore.ixx:159`）資産（Basic/Advanced 5種+Sound/Noise/Delay）を全レイヤーのクローン配列に適用可能にする。

1. `AbstractCloneEffector` を `LayerModifierDescriptor` の一種として Host に登録（typeId `artifact.modifier.clone-effector.*`）。
2. `ArtifactCloneEffectSupport.ixx:858-863` の dynamics 適用ブロックに effector チェーンを追加（現状 Crowd/Physics/Fields/Collision のみ）。
3. `ArtifactCloneLayer.cppm:467` の `useEffector` 分岐は modifier 化された effector へのエイリアスに降格。
- **Scope:** Medium / **価値:** Sound/Delay Effector が Shape/Text 等でも動く。

## Phase 4 — 細部修正バックログ

| 項目 | Evidence | Scope |
|---|---|---|
| ✅ fps ハードコード解除（対応済み 2026-09-07） | 旧記録の `ArtifactCloneEffectSupport.ixx:655,686` `currentFrame()/30.0f` は現行コードで `layer->compositionFrameRate()` 経由へ置換済み | Small |
| PoissonDisk 改良（Bridson法 or グリッド加速） | `CloneGenerator.cppm:496` rejection 64回 | Small-Medium |
| `blendCloneData` の Multiply/Max/Min の意味論文書化 | `CloneCore.ixx:111-129`（位置ベクトルへの要素積は直感とズレる） | Small |
| Random 分布の `std::mt19937` → `RandomStream` 統合 | `ArtifactCloneLayer.cppm:377`（乱数統合方針に合わせる） | Small |

## Phase 6 — Cloner GPU 化（配置 → 描画）

**方針:** 既存 CPU 経路を残し、Diligent 経由の HLSL Compute で段階的に GPU 化する。最初に配置データ生成を独立して成立させ、その出力を使う描画統合は次段階とする。GPU 経路は対象条件を満たさない設定、未対応 backend/capability、resource 作成失敗時に既存 CPU 経路へ戻せること。

**配置データの受け渡し候補:** 現在の2D経路は `ArtifactIRenderer::Impl::cmdBuf_` に draw packet を記録し、`present()` が `DiligentImmediateSubmitter` へ順番に submit してから command buffer を reset する。従って将来のGPU配置結果は、GPU resource を submitter 側でpersistentに所有し、配置compute packet と対応するclone draw packetを同じ command buffer 上で連続させるのが既存構造に沿う。Computeを含むsubmitはdeferred contextへ分岐させず、同じ immediate context の順序でdispatch→drawを記録する（Particle packetと同じ安全側の規約）。固定容量超過時は packet を追加せず、そのclone groupをCPU描画へ戻す。

### GPU-1: 配置データ生成

**対象:** 正規経路の Component generator (`clonerComponentInstances`) にある Grid/Matrix の基本配置。最初は単一の有効 Grid/Matrix generator に限定し、複数 generator の連結、Layout、Clone Modifier、Fields、Crowd、Collision、authoritative simulation の合成は対象外として CPU fallback にする。GPU 結果を CPU に readback して通常描画へ戻す構成にはしない。

**実装状況 (2026-10-11):** `ArtifactIRenderer` に明示的な冷パス初期化、renderer所有の固定65,536-record structured output arena、Grid offset dispatchを追加した。dispatch前に要素数・arena容量・有限なspacingとgrid extentを検証し、失敗時はGPU経路を使わない。さらに通常Composition Viewの限定されたraster surface経路に、GPU placement slotを読むtransformed sprite packetとHLSL vertex shaderを接続した。`Render/ClonerGpuPlacement` 起動フラグ（環境変数 `ARTIFACT_ENABLE_GPU_CLONER`、既定off）が有効な場合だけrenderer初期化時にGPU資源を作成し、無効時はCloner用vertex shaderとgraphics PSOも作成しない。GPU経路はGrid/Matrix単独、transform stackなし、CPU最終transformとweightが配置結果に一致、matteなし、非3D、GPU texture SRVが利用できるcloneを対象とする。GPU placement dispatchは有効な2D raster surfaceのSRVを得てから行い、先頭cloneもGPU配置slotを読むかCPU参照transformとの照合後に選択する。cloneごとにCPU側の順番で1 drawを発行し、配置offsetだけをGPU bufferから読む。出力 ABI は `ClonerGridPlacementRecord` (32 bytes: local offset / clone index / valid / padding)、slot 0 はbase、後続slotは安定したGrid順である。rangeはsubmit batch単位で再利用する。capacity不足、未対応構成、GPU packet記録失敗ではCPU描画へ戻る。DXCで配置compute／描画vertex HLSLのコンパイル、glslangでVulkan向けSPIR-V生成、`spirv-val` を確認済み。実際のViewerにおけるGPU描画とCPU parityの状態は GPU-2 の検証記録に記載する。

- CPU descriptor/settings から、dimension・spacing・座標規約・generator transform に必要な値を固定容量の GPU parameter block へ詰める。
- HLSL compute は clone index ごとに独立して配置を計算し、上限付き structured output buffer に書き込む。総数と dispatch 数は CPU が確定し、shader は範囲外 index を書かない。
- 出力 ABI は描画段階と共有できる明示的な GPU DTO とし、`QMatrix4x4` のメモリ表現や Qt 型を shader ABI として流用しない。現行の仮ABIはlocal offset、元の clone index、valid flagを持ち、index順を保って並列出力する。後段の描画側がvalid flagを参照できるまで、GPU-1の出力を既存描画へ置き換えない。
- GPU-1 では CPU 版を正として維持する。parity 確認用 readback は opt-in の低頻度診断・オフライン確認に限り、毎フレームの hot path に含めない。
- 既存コードでは Grid の座標を `startPos + spacing * (x,y,z)` で中央配置し、clone index は `z * (rows * cols) + y * cols + x`。matrix/transform の積順と handedness を CPU 経路に合わせる。
- 現行CPU経路はまず `baseTransform` を1件追加し、その後 Grid の各変換を走査して `cloneTransform.isIdentity()` の場合だけ候補を除外する。GPU側は出力位置を詰める atomic append にせず、`base + 全候補` の固定順スロットを使う。identity候補をdraw対象から外す責務はCPU側に置き、GPUは浮動小数点のidentity判定を重ねない。baseを含む件数契約はCPU parity oracleと合わせて固定する。

**完了条件:** 対象条件の判定と fallback 理由が明示され、GPU buffer の容量・寿命が bounded であり、GPU 出力がCPU参照結果と規定誤差内で一致する。通常経路で readback が発生しないことをコード経路から確認する。実測なしに高速化完了とは判定しない。

### GPU-2: GPU 配置データによる描画

**目標:** GPU-1 の persistent output buffer を、clone ごとの CPU 行列展開・頂点構築を介さず描画 shader から参照する。まず 2D の同一 source clone を対象にし、既存の draw ordering / blend / source texture semantics を保てないケースは CPU 描画へ戻す。

**実装・runtime検証状況 (2026-10-11):** 通常Composition Viewerのraster image layerで、GPU placement recordをHLSL vertex shaderから読み取るtransformed sprite packetを接続済み。CPU側の順序でcloneごとにdrawし、対応できないcloneは既存sprite経路へfallbackする。`tools/capture_ui_test.py --cloner --cloner-backend cpu|gpu` は各モードを明示起動し、GPU fixtureでは `gpuReady=1`、`gridHandled=1`、3×3に対して `gpuDraws=9`、CPU fallback 0を検証する。CPU/GPU renderer readbackは `ja` / `en` / `zh` / `zh-TW` の全4ロケールで946×1085、39200個のfixture色画素を含み、全画素が完全一致した。実行環境はD3D12およびVulkan。今回対象としたGrid/Matrix/Radial subsetは4ロケールでCPU/GPU parityを確認済み。性能計測は未確認。対応範囲外のケース、起動flag既定off、GPU資源／packet失敗時はCPU経路を維持する。

**CPU fallback runtime確認 (2026-10-11):** `capture_ui_test.py --cloner-expect-cpu-fallback` を追加し、Linear `opacityDecay=0.2` のGPU backend設定で、GPU資源準備後にplacement routeが不適格として通常CPU描画へ戻ることを確認した。`ja` / `en` / `zh` / `zh-TW` の全4ロケールで route は `gpuReady=1` / `placementHandled=0` / `gpuDraws=0`、CPU/GPU renderer readbackは同じframe 2・946×1085で全画素一致し、weight trace `1.000,1.000,0.800,0.600` も一致した。これは容量超過やresource failure時のfallback検証とは別である。

**CPU fallback診断の修正 (2026-10-11):** 全体がGPU不適格で通常CPU callbackへ入るケースも `cpuFallbackDraws` に計上し、route fileはCPU描画完了後に一度だけ確定する。fixtureはGPU処理0件に加えCPU fallback drawが1件以上あることを要求する。Linear opacityDecayはGPU対象へ拡張したため、回帰fixtureの不適格設定をRandom legacy modeへ移した。旧exeでLinear fallbackを再実行すると旧診断の `cpuFallbackDraws=0` により新assertionが失敗する一方、renderer readbackはCPU backendと946×1085で全画素一致し、weight traceも一致した。Random fallbackを含む更新済みexeでのcounter確認は未実施。

**Radial opacity boundary fix (2026-10-11):** `opacityDecay=1.0` の旧実行ファイル試験で、CPU trace `1,1,0,0,0` に対し GPU stream が `1,1,0,-1,-2` を出し、GPU 3 draw＋CPU fallback 2件へ分かれる不一致を検出した。GPU eligible stream の共通candidate weightを0..1にclampして、通常CPU `appendCloneInstance` と意味論をそろえた。関連 `ArtifactCloneEffectSupport.ixx` objectはコンパイル済み。修正後のViewer runtimeと4ロケール parityは、現行 `ArtifactUiTest.exe` が旧実装のため未確認である。

**最初の対象経路:** `ArtifactCompositionViewDrawing.cppm` の `applySurfaceAndDraw` が最終的に発行する transformed sprite。ここは同一の raster surface / GPU texture SRV を全 clone が共有する。専用 `ClonerSpriteXformPkt` と vertex shader がGPU placement slotを参照する経路を追加した。対象条件では専用callbackでclone順に流し、全clone分のCPU instance vectorを作らずに1 clone 1 drawを発行して既存順序を維持する。CPU側でもcomposition field channelのopacity／scale処理を反映し、transformまたはweightがGPU placementと一致しないcloneはCPU sprite描画へ戻す。CPUが描画対象として選んだplacement slotをshader側の浮動小数点identity判定で誤clipしないよう、HLSLは範囲内のGrid候補を有効recordとして出力し、identity候補はCPUがdraw packetを発行しないことで除外する。GPU range作成失敗時はCPU cloneの通常 packetへ戻り、packet記録後にsubmitter側のGPU資源またはCB mapが失敗した場合もpacketが保持するCPU fallback matrixから通常transformed spriteをsubmitする。最初の適用条件は単一Grid/Matrix、transform stackなし、有限なclone weightがplacementと一致、cloneごとのtime変更なし、matteなし、非3D、GPU texture SRVが利用可能なraster surfaceとし、gradient・別packet種別・modifierを含むケースはCPU描画に残す。

**現段階の制約:** 対象Gridでは専用callbackでcloneを順番に流し、全clone分のCPU instance vectorをmaterializeしない。固定output arenaは65,536 recordsで、各groupはbase slot分を含むcandidateCount+1件を予約するため、cursor=0で受け入れ可能な候補数は最大65,535件。batch cursorが残容量を超えるdispatchはparameter map／compute dispatch前に拒否し、このstreaming適用条件内では通常のCPU sprite描画へ移る。cursorはsubmitter batchのsubmit後に0へ戻る。overflow境界を実機で超えるfixtureは未実施。clone indexが`int`範囲を超えた候補はsentinelを使ってGPU照合対象から外す。ただしGPU placementとの照合とsubmitter failure時のfallback用に、各cloneのCPU transform matrixは引き続き生成する。CPU vectorの確保を避ける改善であり、CPU配置準備時間全体の削減や高速化はまだ達成したと扱わない。次の性能段階ではCPU側の行列照合・fallback matrix生成も省きつつ、GPU dispatch不成立時やdraw submitter failure時にCPU結果を維持する経路を設計する。

- 現在の専用 transformed-sprite packet は、base transform、source SRV、GPU placement buffer/range、opacity と local-size 補正を保持する。submitter は同じ immediate context でGPU bufferをSRVとして読み取る。placement rangeはsubmit batch単位で再利用する。
- 現状はcloneごとのsingleton drawを維持し、CPU側のdraw順を保つ。透明合成順を崩さないmulti-instance batchingと、CPU fallback matrix生成の削減は後続の性能段階として扱う。
- 初期の singleton draw shader はcloneごとのfixed slotを読む。draw packet自体はCPUが生成したclone順を保ち、CPUがidentityとして除外した候補のslotを発行しない。host側でbuffer rangeとplacement indexを検証し、shaderの`valid`確認も残す。将来のmulti-instance化では、透明合成順を改めて検証する。
- resource は Diligent の backend-neutral API で作成・遷移し、D3D12 と Vulkan を peer backend として扱う。`libs/DiligentEngine` や raw backend API を変更しない。
- frame-slot ごとの resource lifetime、GPU 書き込み後の vertex/shader read state、capacity 上限、描画完了前の再利用防止を明示する。
- 透明描画順、mask/matte、source variant、time offset など既存 semantics を確認し、GPU化できない組合せを CPU fallback へ分類する。

**完了条件:** 代表的な2D sourceでGPU配置出力を直接描画し、CPU経路との画像差分・順序・fallback理由を確認する。D3D12/Vulkanの両方でresource stateと描画結果を確認し、代表 workload でCPU準備時間、GPU時間、入力遅延を測って採用範囲を判断する。

### GPU-3: 対象機能の段階拡張

GPU-2 の証拠を基に Radial/Linear、modifier、Field、3D instance などを個別に追加する。各追加は CPU/GPU 共通の意味論、deterministic seed、描画順、serialization、simulation ownership を確認してから対象にする。GPU-1/2 の完了を待たずに全 generator/effectors を一括移植しない。

**実装・runtime検証状況 (2026-10-11):** Linear generatorのtranslation-only subset（単一generator、transform stackなし、rotationStep=0、opacityDecay=0、Random legacy mode以外）と、Radial generatorのrotationStepとopacityDecay（0..1）を含むGPU placement dispatchとCPU transform照合へ追加した。Radialでは配置角 `startAngle + angleStep * index` と描画回転 `placementAngle + rotationStep * index` を分離し、GPU parameter blockとplacement recordへ回転角を渡す。CPUのComponent生成とGPU向けstream生成の双方で同一の回転増分を使う。旧式Cloner componentの内部設定（count、offset、grid、radial等）を正規generator descriptorへ渡し、UIで設定したRadial count/radius/anglesが評価へ反映されるようにした。Linearの規則は `offsetXYZ * (cloneIndex + 1)`。D3D12 UI fixtureの80×48矩形source／4 clonesでは、Radial `rotationStep=0` と `15`、Grid、Linearを含むCPU/GPU renderer readbackがそれぞれ946×1085で全画素一致した。GPU routeはRadial各5 draw、Grid 9 draw、Linear 4 drawで、CPU fallbackは全ケース0。Radial step=0 は `ja` / `en` / `zh` / `zh-TW` で以前に確認済み。Radial step=15、Grid、Linearも同じ4ロケールすべてで今回再確認した。初回15度stepキャプチャの全黒画像はframe 1の読み戻しで、frame 2で再取得するとCPU/GPU双方に描画され完全一致した。Radial opacityDecay を有限な範囲 0〜1 の設定に限ってGPU placement/drawへ追加し、Grid/Matrixにも同じindex-based weight式を適用してCPU/GPU両経路のrender packetが同じclone weight列 `1.000, 1.000, 0.800, 0.600, 0.400` を使うことをfixtureで確認した。調査で通常transformed-sprite vertex shaderが頂点カラーを固定の白／不透明へ置き換えていたため、Artifact側のvertex shaderを頂点カラー入力に対応させ、CPU fallbackがopacityを保持するよう修正した。D3D12、Radial `rotationStep=15` のdecay=0.2とdecay=0の両方でCPU/GPU renderer readbackが946×1085の全画素一致（各GPU 5 draw／fallback 0）。decay=0.2と0の画像間では11,609画素に変化があり、減衰が出力へ反映されることも確認した。Grid/Matrixはclone index `z*(rows*columns)+y*columns+x` のweight `clamp(1-opacityDecay*index,0,1)` を使う。Matrix用descriptor fixtureはcomponent clonerを無効にしてMatrix generatorを単独追加し、descriptorの `opacityDecay` setterも検証した。D3D12 Matrix decay=0.2は `ja` / `en` / `zh` / `zh-TW` のCPU/GPU renderer readbackが全画素一致（946×1085、GPU 9 draw／fallback 0、weight列 `1.000,1.000,0.800,0.600,0.400,0,0,0,0`）。英語fixtureでdecay=0と比較するとCPU/GPU双方で26,293画素が変化し、decay=0同士のCPU/GPU画像も全画素一致した。Matrix decay=1.0のweight列 `1,1,0,0,0,0,0,0,0` でもCPU/GPU readbackは全画素一致した。Vulkan backendでもMatrix decay=0.2、Grid decay=0.2、Radial rotationStep=15／decay=0.2の `ja` / `en` / `zh` / `zh-TW` readbackをframe 2で比較し、全ケース全画素一致した。GPU drawはMatrix/Grid各9件、Radial 5件でfallbackは全て0。Grid decay=0.2も同4ロケールで全画素一致、GPU 9 draw／fallback 0、weight列同一で、decay=0比26,293画素が変化した。Grid/Matrixは単一generatorで平行移動/Z回転のみのtransform stackまで、Radial/Linearはtransform stackなし、有効modifierなし、非3D、matteなし等のeligibilityを維持する。modifier等は引き続きCPU placement経路へfallbackする。Grid/Matrix/Radialの今回対象subsetはD3D12/Vulkan runtime parityを4ロケールで確認済み。性能計測は未確認。

**Grid/Matrix GPU対象範囲:** 対象は単一の`layerGenerators()` Grid/Matrix descriptorで有限な`opacityDecay`が`0..1`、scale Z=1のtranslation/Z rotation/XY scale transform stackのうちrotation＋diagonal XY scaleで表現できる範囲、sequence無効、GPU対応Plain modifier chain、field/simulation/layout/collisionなし、非3D、matteなしのraster sprite経路。GPUは配置offset/rotation/local scaleを生成し、weightはCPUで安定順に計算してper-draw opacityへ渡す。**Linear/Radial GPU対象範囲:** 対象は単一の各descriptor、finiteなrotation/opacity decay条件を満たし、同じ表現条件のtranslation/Z rotation/XY scale transform stackとGPU対応Plain modifier chainを使う配置。stack translationは各Linear/Radial placement回転後のローカル空間で合成し、累積Z rotationとXY scaleはrecordへ畳み込む。**GPU対応Plain modifier chain:** 有効Plain modifierは複数可。finiteなposition XYZ、Z rotation、scale XY、scale Z=1、rotation X/Y=0を条件とし、現在の回転＋対角XY scaleで表現可能な順序だけ受け入れる。累積scaleが非uniformの後に追加Z rotationを適用するとshearが生じる順序、非Plain modifier、実効time-offset／sequenceはCPU fallback。時間・field／simulationの影響、旧式`ClonerGenerator` effectとの混在もCPUへ戻す。legacy effect由来RadialのlocalBounds中心意味論はGPU対象外。
**最終更新:** 2026-10-12

**Linear rotation extension (2026-10-11):** 従来translation-onlyだったLinear GPU subsetにfiniteな `rotationStep` を追加した。clone indexを0始まりとしたplacement角は `rotationStep * (cloneIndex + 1)` で、CPUの `translate(offset * (cloneIndex + 1))` の後に適用する回転と同順序にする。当初は `opacityDecay != 0` をCPU fallbackに残し、後続のopacity-decay extensionで別途追加した。fixtureにLinear回転step指定を通したが、変更後runtime parityは未検証。

**Linear opacity-decay extension (2026-10-11):** finiteな `opacityDecay` が `0..1` のLinear generatorもGPU placement/draw eligibleとし、既存per-draw opacityへ `clamp(1 - opacityDecay * cloneIndex, 0, 1)` を渡す。rotationStepとの併用も同じCPU transform照合を通す設計。fixtureはLinear clone countを3に固定し、baseを含む4値のweight traceを検証する。既存exeのCPU fixtureで `1.000,1.000,0.800,0.600` を確認したが、GPU placement対応後のruntime parityは未検証。Random legacy modeは引き続きCPU fallback対象。

**無効transform stack項目のGPU適格化 (2026-10-12):** CPU評価が無視する非object項目と `enabled=false` 項目だけのtransform stackは、空stackと同じためGPU placement対象に含める。有効項目が1つでもある場合は引き続きCPU fallback。Linear/Radialのeligibility判定をこの規則へ合わせ、`ArtifactCloneEffectSupport.ixx` の直接MSVC compileは成功。対象アプリexeが最新変更を含まないため、この追加subsetのruntime route検証は未実施。

**Grid/Matrix translation・Z rotation transform stack (2026-10-12):** 当初はscale XYZが1の有効transform stack項目に対応していた。各positionを先行Z回転で変換してtranslation合計を求め、rotation合計はplacement recordへ渡す。Grid placement recordのspacing offsetとsprite packet親行列のstack translation/rotationをCPU順に照合し、誤差が許容範囲を超える場合はCPU fallback。この時点ではscaleを含むstackをfallbackしていた。後続の2D affine stack拡張でXY scaleも追加対象となった。Viewer routeは既存exeが更新前のため未検証。

**Grid/Linear/Radial 2D affine transform stack (2026-10-12):** GPU transform stackをscale Z=1のtranslation/Z rotation/XY scaleへ拡張した。累積transformがrotation＋diagonal XY scaleで表せる順だけを許可し、nonuniform XY scale後のZ rotationはshearを生むためCPU fallbackとする。累積scaleで後続translationを写し、stackとPlain modifierを合成してplacement recordのoffset/rotation/scaleへ渡す。CPU期待行列とGPU合成行列を照合し、CPU fallback matrixにもstackとPlain modifierのXY scaleを反映する。modifier適用後にidentity判定する。`ArtifactCloneEffectSupport.ixx` と `AppMain.cppm` の直接MSVC compile、およびshear-free stack＋Plain transformを含む10,000件のランダム2D affine composition比較は成功。Composition drawing直接compileは既存 `Artifact.Render.IRenderer` IFCが現行 `.ixx` より古く、`dispatchClonerGridPlacement` の宣言不一致で失敗した。現在の `RenderCommandBuffer` IFCはMPM surface型を公開しておらず、MPMに触れずIFCを更新していないため、描画TU compileとViewer runtime parityは未確認。`capture_ui_test.py --cloner-transform-stack` をGrid/Linear/Radialで使えるよう拡張し、2段目へnonuniform XY scaleを設定するmarker確認を加えた。既存exeは古いため追加fixture実行は未実施。

**Grid stack Z rotation verification (2026-10-12):** 現行placement compute HLSLをDXCでDXILへ変換し、D3D12 probeの3x2 Gridに15度のrecord rotationを設定して、6件のrotation出力と配置値がCPU参照式に一致することを確認した。2段のtranslation＋Z rotationを含むQt `QMatrix4x4` probeでは、CPU逐次合成とGPU packet用translation/rotation集約のmatrix max errorが0。これは配置・変換数学の検証であり、アプリ内Viewer routeの検証ではない。

**Grid stack compute-to-draw verification (2026-10-12):** 現行placement compute DXILがD3D12 direct command listで3 slot（base＋2 Grid clone）を出力し、その同一bufferをCloner vertex shaderが読み、512×512 render targetへ描画した。二段translation/Z rotation（30度、-15度）を集約した親行列とGrid offsets/record rotationを使い、CPU参照画像と全画素比較して差分0。clone別pixel/bounds/alpha検証も通過。これはD3D12 shader pathの直接検証で、Diligent Viewer packet／texture bindingを含むアプリend-to-end検証はruntime fixtureで別途確認する。

**Grid/Linear/Radial stack runtime fixture (2026-10-12):** `capture_ui_test.py --cloner-transform-stack` と `ArtifactUiTest` のfixtureを拡張し、Grid/Linear/Radialに2段（rotation 30度と-15度）のstackを設定し、2段目へnonuniform XY scale `(1.5,0.75)` を適用する。アプリが実際のdescriptorを読み戻して generator type・translation・scale・rotation marker を作り、capture helperがそのmarkerとGPU routeを確認するため、古いexeがstack設定を無視して通常配置だけを通す偽陽性を防ぐ。CPU/GPU双方で同じfixtureを使える。Capture helperのPython構文・option validationと `AppMain.cppm` の単独MSVC compileは成功。既存exeではこのruntime fixture未実行。

**2026-10-12 の追加検証:** 現行Cloner support、Composition drawing、IRenderer、AppMain、CompositionRenderControllerのMSVC単独compile 5件が成功。現行placement compute HLSLとCloner sprite VSはDXCでDXIL生成に成功し、computeはNVIDIA RTX 4070 Ti上のD3D12とVulkanでCPU参照式・base slot・nonzero outputOffsetを照合した。さらにD3D12の同一direct command listでLinear placement computeをdispatchし、UAV→VS SRV transition後に同じGPU bufferをCloner VSから直接読み込んでbase+3 cloneを描画した。回転0/15/30/45度、実weight列1/1/0.8/0.6の4 bbox・画素数・alphaは期待値と一致。CPU reference bitmapとの差は1pixelで、30度cloneの下端raster boundary。アプリ側のDiligent Linear全画面 parityは既存 `ArtifactUiTest.exe` が更新前のため未検証。更新リンク graph は未生成のMpm `.cppm.obj` に依存し、MPMソースを変更せずに最新exeをリンクできていない。ProbeはDiligent packet・texture samplingのend-to-end試験を代替しない。

**既存exeによる再確認 (2026-10-12):** MPMを変更・再リンクせず、既存 `ArtifactUiTest.exe` でGrid fixtureのCPU/GPU captureを `en` / `ja` / `zh` / `zh-TW` の4ロケールで実行した。全renderer readbackは946×1085でバイト単位に一致し、各GPU routeは `gpuReady=1` / `placementHandled=1` / `gpuDraws=9` / `cpuFallbackDraws=0`。これは既存exeに含まれる標準Grid経路の確認であり、後から追加したtransform-stack runtime fixtureの実行証拠ではない。

**Random fixture の false-positive 防止 (2026-10-12):** GPU fallback fixtureを既存exeで実行した際、要求したRandom分布ではなく旧exeがGridを描き、routeだけを見るとGPU成功に見える問題を検出した。`ArtifactAbstractLayer::layerGenerators()` で互換Cloner mode 3を明示的な `artifact.generator.cloner.random` descriptorへ正規化し、capture helperはアプリがdescriptorを読み戻して書いたfixture markerのdistribution typeをroute判定前に検証する。marker欠落・type違いはfixture不成立として即時失敗する。ArtifactAbstractLayerとAppMainの単独MSVC compile、capture helperのPython構文検査は成功。既存exeでの再実行はmarker欠落で拒否され、修正版Random CPU fallback routeのruntime結果は新exeの用意後に確認する。

**Plain modifier translation / Z rotation の GPU placement対応 (2026-10-12):** 単一Grid/Matrix・Linear・Radial descriptorに、有効Plain modifierの有限なposition XYZ translationとZ rotationをGPU placement recordへ反映する。CPUの modifier順序を保つため、record offsetには現在のplacement rotationを適用してtranslationを足し、modifier rotationを累積する。rotation X/Y、scale XYZ、非Plain有効modifier、実効time-offset／sequence、field/simulation等は従来どおりCPU fallback。描画fixture `--cloner-plain-translation` と `--cloner-plain-rotation` はGrid descriptor markerで実設定を検証する。関係する5 translation unitの単独MSVC compileが成功し、Compute HLSLはDXILとVulkan 1.1 SPIR-V/spirv-valを通過。NVIDIA RTX 4070 Ti D3D12 direct command list probeではplacement offset/rotation、bbox、画素数、alphaが期待値と一致し、modifier translation `(9,-6,0)` と追加Z rotation 15度を含むbase+2 cloneのCPU raster参照との差は回転境界の2画素。translationのみの従前probeは0 pixel差、既存translation/rotationなしGridも0 pixel差。これはshader/record/raster直接検証であり、Diligent Viewer packet経由の実アプリ parityは未確認。既存exeは更新前なので新fixture markerで拒否される。

**複数Plain modifier chainのshear-free subset (2026-10-12):** Plain modifier列を順番に合成し、既存のrotation＋diagonal scale XY recordで厳密に表せる場合をGPU placementへ追加した。前段の累積scaleを適用してから各modifier translationを回転座標へ足し、modifierごとのstrength適用後scale／Z rotationを累積する。累積scaleがnonuniformで、その後に非zero Z rotationが来るとshearを生成するためCPU fallback。`capture_ui_test.py --cloner-plain-chain` は「uniform scale＋Z rotation」の後に「translation＋nonuniform XY scale」を設定してdescriptor markerで両modifierを検証する。shader ABIは変更せず、既存32-byte recordを使う。3つのshear-free chain合成と1つのshear生成chainを検査するstandalone affine math check、CloneEffectSupport／AppMainのMSVC直接コンパイル、capture helperの構文・fixture option validationは成功。対応する集約値（translation `(12.295496,-4.159421,0)`、rotation 15度、scale `(1.8,0.9)`）をD3D12 direct compute-to-draw probeへ渡し、base+2 clonesのCPU raster比較は全画素差0。アプリruntime parityは既存exeが古くMPM変更を避けているため未確認。

**Plain modifier X/Y scale の GPU draw対応 (2026-10-12):** 有効なPlain modifier chainの回転＋対角scale XY subsetで、scale XYを32-byte placement recordの既存末尾2 floatへ格納し、Cloner sprite vertex shaderでローカル座標へ適用する。CPU順序 `translate → rotate Z → scale XY` を保ち、modifier付き単体cloneもdispatch対象に含める。表現外のshearを生む順序、scale Z、X/Y rotation、非Plain modifierはCPU fallback。`capture_ui_test.py --cloner-plain-scale` とAppMain descriptor markerを追加し、旧exeによる誤判定を拒否する。CloneEffectSupport、ShaderManager、AppMainの直接MSVC compileは成功。Renderer interface compileは、現在の `Artifact.Render.RenderCommandBuffer` IFCが `MpmSurfaceVertex` を公開していないため失敗した。MPMコードは変更していない。RTX 4070 Ti D3D12 direct command-list probeでGrid base+2 clonesへ1.5×0.75 scaleと15度回転を併用し、placement scale/rotation、clone別bbox/画素数/alphaとCPU raster参照が一致、全画素差0。検証中に回転分岐が未scale座標を使うVS不具合を発見して修正した。computeとvertex shaderのVulkan 1.1 SPIR-V validationは成功したが、Vulkan runtime drawとDiligent Viewer packet経由のアプリparityは未確認。

**現行 Cloner shader source validation (2026-10-12):** `ArtifactIRenderer.cppm` と `ShaderManager.cppm` に埋め込まれた現在の compute HLSL と sprite VS を直接抽出し、両方をDXCでDXILとVulkan 1.1 SPIR-Vへ変換、`spirv-val --target-env vulkan1.1`で検証した。4つのshader artifactが生成され、validatorは全て成功。過去の`temp/*-current.hlsl`スナップショットは現行の32-byte placement recordより古いABIだったため、この確認では参照せずソースから再抽出した。これはshader compile/validationであり、アプリruntime parityを証明しない。

**D3D12 standalone Cloner placement-to-draw probe (2026-10-12):** ユーザー承認の一時probeをMSVCで単独ビルドし、NVIDIA RTX 4070 Ti上で実行した。現行compute DXILからGrid / Linear / Radialの13 clone placement recordとbase sentinelを読み戻してCPU式と照合し、追加Grid fixtureのbase＋2 cloneについて同じGPU structured bufferを現行Cloner sprite VSが読み、XY scale `(1.5,0.75)`とZ rotation `30°`を適用して128×128 RGBA targetへ描画した。3 connected componentsのbboxはCPU期待値に各辺1 pixel以内で一致した。これは現行compute-to-VS buffer経路のD3D12直接検証であり、Diligent Viewer packet／実texture samplingを含むアプリend-to-end parityとVulkan runtime drawは未確認。probe source・実行物は`temp/cloner-placement-d3d12-probe.*`にある。

**共通制約:** hot path 中の buffer 再確保、巨大 readback、暗黙の upload、毎フレームの設定再取得を避ける。capacity 超過は bounded fallback とし、GPU capability 不足時も現行結果を維持する。ビルド・テスト・実機測定は明示的な実行指示を得てから行う。

## 検証方針

- Phase 1: 変換前後で `generateTransforms()` の出力行列を比較する GTest（同seed・同設定で許容誤差 1e-4）。
- Phase 2: Image/Shape ソースの複製が GPU/CPU 両パスで同一見た目（render-parity-validation スキル活用）。
- Phase 3: Delay Effector を Text レイヤーに適用し timeOffset が GPU instance data（`:934` `gpuInstance.timeOffset`）まで伝播することを確認。
