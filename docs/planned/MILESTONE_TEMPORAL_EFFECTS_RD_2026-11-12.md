# Temporal Effects R&D — 前後フレーム参照エフェクト検証

**最終更新:** 2026-11-12
**ステータス:** In Progress

Phase 1 のテストソースと登録を追加済み。ビルド・実行は未実施。

## 目的

前後フレームを読むエフェクトについて、既存のフレームサンプラーとエフェクト群を使い、評価順・シーク・再生・キャッシュ制限で結果が壊れないかを再現可能なテストプロジェクトで確認する。製品機能を増やす前に、機能契約と実行時の成立条件を把握する。

## 現状の確認

- `ArtifactEffectFrameSampler` は composition frame をキーに、レイヤーごとのCPU float画像履歴を保持する。既定上限は64フレーム／256 MiB。
- `sampleCurrentLayerFrameRelative()` は現在評価中フレームに offset を足して取得する。明示フレーム参照もある。
- GPU空間エフェクトの履歴経路は直前フレーム（`currentFrame - 1`）だけを有効と判定し、出力を現在フレームのeffect処理後に記録する。GPU経路は任意の前後offset履歴ではない。
- GPU履歴poolの満杯時は `recordLayerFrame()` が `false` を返すが、Composition View callsiteは返値を使わない。近くのコメントにはCPU sampler fallbackとあるため、実際のfallback経路は要確認事項として扱う。
- `EffectInputRequest`にはtemporal offset／lookback metadataがあるが、現時点のrepository searchでは`LegacyEffectAdapter`や`describeDependencies()`の利用側は見つからない。sampler経由の各effectがこのmetadataへ接続しているとは仮定しない。
- Composition Viewの`setComposition()`はcomposition replacement時にsurface／GPU cachesをclearするが、`ArtifactEffectFrameSampler::invalidateAll()`は呼ばない。temporal discontinuity判定もcomposition IDではなくframe番号だけで比較する。同じLayer IDを再利用するcomposition切替で古いCPU履歴が残らないかは未確認。
- CPU sampler APIは正負offsetを表現できるが、履歴もeffect stack完了後に格納される。preview discontinuity時には `N-1` を1フレームだけpre-rollする実装があり、`N+1` が事前格納されるhost経路は未確認。
- CPU samplerの各取得は `copyFrame()` → `SetCpuImage()` → `DeepCopy()` を通り、フレーム全体を複製する。Echo／Time Blurなどは複数frame取得ごとにこのコピーが生じるため、取得数・画像サイズ別にCPU時間と確保量を計測する。
- CPU／GPUの履歴登録コメントと配置は、effect stack完了後のレイヤー出力を次frame用に保存する契約を示している。effect段ごとの履歴ではなく、前frameの最終rasterizer出力を読む意味と、複数effect stackでの合成順をhost試験で固定する。
- sampler は seek／不連続な時間移動／composition変更で全履歴を無効化する想定。revision変更時の古い履歴破棄もある。
- Echo、Frame Blend、Temporal Denoise、Time Blur、Temporal Median、Feedback などの個別契約テストは存在する。
- 個別テストの多くは固定応答 sampler を使うため、実 `ArtifactEffectFrameSampler` の格納・相対検索・eviction・revision invalidationを通した統合保証とは別である。
- 履歴はCPU画像であり、GPU資源は保持しない。GPU readback／コピーの時間と再生時の負荷は実測課題として扱う。

### エフェクトの時間方向（ソース監査）

| エフェクト／経路 | 要求方法 | 方向と評価上の注意 |
|---|---|---|
| Frame Blend、Echo、Feedback、Temporal Median、Temporal Denoise | 相対frame sampler | 過去frameを参照。lookback数と不足時の挙動を個別確認する。 |
| Time Blur | 相対frame sampler | centered modeは過去・未来の両側、trailing modeは過去のみ。未来frameが既に履歴にあるケースとhost再生中の取得可能性は別に扱う。 |
| Time Warp | 画素駆動の符号付き相対offset | source channelから画素ごとのoffsetを作り、同一offset群を取得する。必要frame数、欠落frame、workspace確保の計測対象。 |
| Time Displacement | 絶対frameを先に要求し、失敗時に相対offsetへfallback | 画素ごとの符号付き時間ずらし。decoder／hostが絶対frameを提供できる範囲を確認する。 |
| DeBlink | 相対 -1 と +1 | 両隣接frameのlumaを使うCPU repair経路がある。未来側のhost供給が品質条件。 |
| GPU spatial temporal pass | GPU固定履歴slot | `currentFrame - 1` だけを有効化する。現在フレーム結果の記録はpass適用後。 |

## テストプロジェクト

名称: `TemporalEffectsRD`（受け入れテストtarget `ArtifactTemporalEffectsRDContractTest`）

### 決定論的な入力

小さな線形 float RGBA画像に、透明背景上をframeごとに移動する白いパッチを置いた12フレームの合成列を用意する。各フレームの基準画素値をテスト側で生成し、外部メディアやデコーダの揺れを排除する。現行focused testは12画素幅・1画素高であり、alpha境界、静止領域、移動領域を含む。

### 検証ケース

1. **隣接フレーム参照:** frame N から N-1 と N+1 を絶対指定で取り、各々の識別色・移動位置を確認する。
2. **相対参照:** current frame binding を変えて -1／+1 を取得し、要求先 frame が正しいことを確認する。
3. **順不同評価:** 0, 1, 2… と 6, 2, 9, 3… の異なる順序で評価し、要求した frame の出力が評価順に依存しないことを確認する。
4. **シークと履歴破棄:** 履歴構築後に discontinuity を無効化し、古い隣接フレームが返らず、不足時の effect fallback が決定的であることを確認する。1 frameだけpre-rollした直後のEcho等で部分lookbackがどう正規化されるかを検査する。同じLayer IDを持つ別compositionへ切り替えたときのCPU履歴も検査する。GPU pool満杯時のfalse返却もComposition Viewのfallbackと結び付くか確認する。
5. **revision更新:** 同一frameの内容revision変更後に stale frame が参照されないことを確認する。
6. **境界と欠落:** 最初／最後のフレーム、履歴未格納フレーム、空の layer ID、異なる寸法のサンプルを扱う挙動を記録する。
7. **上限:** frame count と byte budget の超過、単一frameがbudgetを超える場合を確認し、保持量が上限を越えないことを検査する。
8. **Effect smoke:** 同一入力列に Frame Blend、Echo、centered Time Blurを適用し、単一frame、複数lookback、過去・未来の実サンプラー取得を確認する。Time Warpでは一つの入力画像から符号の異なる相対offsetが同時に選ばれるケースを検査する。続いて Temporal Denoise／Time Displacement／DeBlinkを追加し、lookback要求数・欠落frame動作・画素駆動offsetを特定する。
9. **連続性:** 連続前進、逆方向、ランダムseek後の再開で、出力系列と sampler 状態を比較する。差がある場合は履歴無効化や再ウォームアップ仕様として切り分ける。

## 記録する指標

- 要求frame、取得成否、現在frameとのoffset、履歴保持frame数／bytes。
- CPUでの格納／取得／効果評価時間と割当回数。常時フレームログは使わず、明示的なR&D計測時だけ収集する。
- 各ケースの期待RGBAと実測RGBA、順序違いの差分、欠落時fallback。
- GPU経路を計測する場合はCPU history取得・アップロード・GPU dispatch・readbackを別々に記録し、単一の合計時間だけで評価しない。

## 合格条件

- 同じ frame と同じ入力内容を要求した場合、評価順にかかわらず取得画素が一致する。
- invalidation 後に無効化前のframeが返らない。
- count／byte上限を越える画像履歴が保持されない。
- 欠落時の挙動がエフェクトごとに明示され、未初期化データや古いframeへフォールバックしない。
- CPU historyのコピー／GPU転送コストが計測可能で、再生・スクラブ用途への制約が説明できる。

## フェーズ

### Phase 0 — 既存契約の静的確認

既存の sampler、render controllerのstore／invalidate箇所、Temporal effect各クラスと個別テストを照合する。実際に+1（未来フレーム）がホストから取得可能か、seek後に何フレームのwarm-upが必要かをcaller単位で記録する。

### Phase 1 — sampler契約テスト

実 `ArtifactEffectFrameSampler` を使うテストを追加し、保存・相対／絶対取得・revision・invalidation・byte/frame evictionと、Frame Blend／Echo／Time Blur／Time Warp effectからの実サンプラー参照を検証する。テストソースと既存 Artifact test target への登録は追加済み。ビルドと実行は未実施。

### Phase 2 — temporal effect R&D fixture

12画素幅の透明背景上を移動する決定論的パッチをframe 0〜11に置き、Frame Blend、複数lookback Echo、centered Time Blurを実sampler経由で評価するfocused smokeケースを追加済み。Frame Blendはframe Nを処理した完成画像をstoreし、N+1の評価で読む逐次評価も通す。別の5画素 fixture で、Time Warpが1画像内のdark pixelから過去、bright pixelから未来を同時に取得するケースも追加した。未来frame未格納時のTime Blur欠落処理と、lookback2に対して1 frameだけ履歴があるEchoの再正規化も検査する。複数effect stack、Composition切替時のhistory invalidation、順不同のhost評価、seek後warm-up、preview／RAM preview／offline render間の同一性は未実装・未確認。レンダーhostの制御が必要なケースはeffect単体契約とhost統合契約を分ける。

### Phase 3 — 実行時・性能評価

ユーザーがビルド／テストを明示した後に、focused test、必要なら実アプリ再生・scrubを計測する。現段階ではテスト・ビルド・CMakeは未実行。

## 制約・非対象

- 子リポジトリは変更しない。
- GPU/Diligent render pathやsoft rendererの機能実装、UI、effect catalog追加は行わない。
- `QImage`／`QPainter`による合成、フレーム中の無制限キャッシュ・確保、無条件ログを導入しない。
- 動画デコードを前提とせず、最初はメモリ上の決定論的画像列で sampler 契約を分離する。

## 関連資料

- `docs/technical/HOT_PATH_RULES.md`
- `docs/analysis/EFFECT_MAP_2026-07-16.md`
- `docs/planned/MILESTONE_LOCAL_TIME_WARP_MASK_2026-10-12.md`
- `Artifact/include/Effects/ArtifactEffectFrameSampler.ixx`
- `Artifact/src/Effects/ArtifactEffectFrameSampler.cpp`
- `tests/Artifact/FrameBlendEffectContractTest.cpp`
- `tests/Artifact/EchoEffectContractTest.cpp`
