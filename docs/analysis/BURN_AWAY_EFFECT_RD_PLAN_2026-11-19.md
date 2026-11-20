# Burn Away エフェクト R&D 評価計画

**最終更新:** 2026-11-19

## 1. 目的と対象

- **目的:** 画像／平面レイヤーが「燃えた紙のように徐々に焦げて消える」表現を、レイヤーエフェクト1つで自己完結して実現する。 Burning paper の前線（front）が層を侵食し、生存側の縁が炭化し（char）、前線に細い火の粉の縁光（ember）が乗る。
- **対象:** `Artifact.Effect.BurnAway`（`Artifact/include/Effects/BurnAway/BurnAwayEffect.ixx`、`Artifact/src/Effects/BurnAway/BurnAwayEffect.cpp`）の CPU 参照実装と generic resident GPU シェーダ。
- **対象素材:** 画像レイヤー（不透明写真、背景透過 PNG）、平面／シェイプレイヤー（単色・グラデーション）。
- **対象外:** 灰・火の粉の@flying 粒子表現（Particle / Smoke レイヤー側の別案件）、3D 深度を贯彻执行する体積燃焼、書き出し時の一時オブジェクトの破壊。
- **現状:** 実装は本計画と同時に追加した新規コード。CPU 参照経路は §7 のとおり評価まで実施済み。GPU generic resident シェーダは同一の式・同一のノイズハッシュで実装しているが、実行差分の測定は未実施。

## 2. 比較対象

| 役割 | 内容 | 正解として扱える根拠 |
| --- | --- | --- |
| baseline | 同じ入力・同じ設定で本エフェクトを無効（Progress = 0）とした出力 | エフェクトが非中立時に限って差分が発生することを保証する基準 |
| 合成 ground truth |  Progress = 1 の出力（層が完全に消滅した面） | 「進捗 1 で全面消失」が契約であるため、合成結果として検証可能 |
| 物理的参照 | 実写の燃える紙の映像（外部） | 物理的正解ではなく、質感の参照。炭化色・縁光の配色の下限を支える補助資料とする |

実写や他製品の画像を無条件に物理的正解として扱わない。

## 3. 評価素材

1. 高周波な紙目の白 photograph（1920×1080）— 粒状ノイズが炭化帯で undue に立=center ないか。
2. 背景透過 PNG（合成nus あり／縁に半透明ピクセルのあるもの）— alpha 境界での発光のにじみ確認。
3. 平坦な単色シェイプ（矩形）— 一定の色面での炭化グラデーションとバンディング確認。
4. HDR 値の存在する高輝度素材（窓・天空）— 线性 HDR 域でのクリップと発光の上限確認。
5. 極端設定: Progress = 0 / 0.001 / 0.5 / 1、Edge Width = 0、Spread = 0、Octaves = 1、Seed 固定の連続フレーム。

素材の入出力と取得手順は §6 の証跡として保存する。リポジトリに置けない場合はアクセス方法と識別情報を記録する。

## 4. 画像条件

- 入力: `ImageF32x4_RGBA`（線形）。色空間は `EffectContext` の `inputColorSpace` / `workingColorSpace` に従う。
- 出力: 同一色空間・同一 alpha 契約（alpha は premultiplied 相当の扱いを踏襲し、RGB は alpha を超える加算を行わない）。
- 解像度: 評価は 1920×1080 と 960×540 の2種。空間パラメータはすべて層高で正規化しているため、分辨率によって見た目が変わらないことを §5「経路と解像度」で確認する。
- 乱数: Seed を固定（既定 0）。同一入力・同一 Seed で同一結果になることを前提とする。
- フレーム: 単一フレーム評価と、Flicker を 1.0 にした連続フレーム評価（時間安定性）。
- 保存形式: 評価用は HDR を保持できる形式（16bit float PNG / EXR）。画面キャプチャ用の 8bit PNG は表示用であり、測定には使わない。

## 5. 評価項目と合否基準

各項目に評価領域・手順・判定者・合格条件・許容差・重大な欠陥を定める。閾値は baseline 取得後、候補の最終比較前に確定する。

| 項目 | 評価領域 | 手順 | 合否 | 重大な欠陥 |
| --- | --- | --- | --- | --- |
| 効果の正しさ | 全面 | Progress 0 / 0.5 / 1 を出力し、0 で source と非負の誤差で一致、1 で alpha が全面 0 | Progress 0: 各チャンネル絶対誤差 ≤ 1e-6（float 演算の順序差を含む）。Progress 1: alpha 最大値 = 0 | Progress 0 で層が欠ける／Progress 1 で残像が残る |
| 効果の正しさ（連続性） | 全面 | Progress を 0→1 に 0.05 刻みで変更 | alpha 合計が単調減少し、逆行なし | 前線が後退して alpha が増える区間 |
| 画質と細部 | 境界帯（幅 = Edge Width）、平坦部 | 等倍／4倍拡大で目視 | 輪郭に階段状のバンディング、無効な halo なし。炭化帯に不自然な縞がない | 境界の階段状アーティファクト、無効な白縁 |
| 色と alpha | 境界帯、透明背景 | 複数背景色（黒・白・中間灰）に合成して目視 | 透明ピクセルの RGB が背景色を持ち込まない。縁の発光に黒縁（premultiplied 違反）が出ない | 合成後に黒い輪郭が残る |
| 時間安定性 | 連続フレーム | Progress 固定・Flicker 1.0 で 30 フレーム取得し、フレーム間差分を測定 | 発光強度がフレーム間で滑らかに変化し、flicker が意図した帯域に収まる | 1 フレームだけ強く発光する点発光 |
| 経路と解像度 | 全面 | CPU 参照出力と GPU 出力を同一入力で比較 | 同一 seed で alpha 境界が同位置（1 画素以内）、絶対誤差 ≤ 2/255（8bit 相当） | GPU で境界が反転・消失 |
| 経路と解像度（解像度） | 全面 | 1920×1080 と 960×540 で同一 Progress を出力 | 境界位置の相対誤差 ≤ 1%（正規化のため） | 解像度で炭化帯幅が変わらない |
| 性能 | 全面 | 1920×1080 で CPU 参照 / GPU それぞれを 30 回計測（中間値） | GPU が CPU 参照以下。CPU 参照は 1 フレーム 16ms 未満を既定の目標とし、未達なら原因と代替案を記録 | ホットパスで毎フレームのensure な大確保が発生 |
| ホットパス割当 | — | コードレビュー | 毎フレームの新規ヒープ確保・再確保・巨大 readback がない（ノイズはシェーダ内・CPU 参照は作業バッファを1回確保） | 毎フレームの確保 |
| 空間パラメータの正規化 | 全面 | 解像度差の比較 | Edge Width / Ember Width / Edge Softness / Noise Scale が層高基準で解釈される | ピクセル単位の混入 |

### 測定統計

- 画素値分布: RGB・輝度・alpha の最小／最大、平均、中央値、分位点（1/50/99%）、クリップ率（0–1 未満の丸めではなく実値域で算出）、非有限値数。
- 参照との差分: CPU 対 GPU でチャンネル別 MAE / RMSE / 最大値 / 閾値超過画素率と差分画像。Progress 0 では source との絶対誤差。
- 透明度・境界: alpha 分布、境界帯（±Edge Width）の alpha 統計、背景 3 色との合成差。
- 効果固有: 炭化帯の平均輝度低下率、縁光帯の最大輝度（HDR 値域のまま）、Progress と残存 alpha 面積の対応（残存面積が Progress に対して単調）。
- 適用しない項目: 参照差分（PSNR/SSIM）— 本エフェクトは意図的に像を変化させるため、SSIM の低下は欠陥ではない。差分は alpha 境界位置の一致と分布の比較に置き換える。

測定結果には入力／参照の識別情報、領域、測定空間・値域、alpha の扱い、指標の定義・単位、集計方法、ツールとバージョン、許容値、残す。

## 6. 実行条件・証跡

- 対象 GPU・バックエンド: Diligent の D3D12 経路（preview と最終出力の両方）。Vulkan 経路が利用できる場合は同条件を追加し、未実行なら「未検証」と記す。
- 実行環境: 本機のアプリ（`Artifact`）による実アプリ経路で取得する。CPU 参照出力は effects contract test 相当の最小ハーネスで取得し、経路の同一性を明記する。
- 性能計測: GPU は `ARTIFACT_EFFECT_PROFILE` 相当のプロファイリング、CPU 参照は計測用ハーネスの経過時間（測定回数・中央値・環境）。
- 証跡の保存先: `experiments/BurnAwayLab/`（評価計画・入力素材・取得スクリプト・manifest）。生成画像は Git の既定除外に従い、最終証跡のみ hash と条件付きで `git add -f` する。
- 完了条件: 必須項目の結果・合否・根拠、未解決事項、採用判断、後続実装の受入条件を本書に追記する。未実施・未検証の項目は未実施と明記し、画質達成や製品統合完了と表現しない。

## 7. 実施結果（2026-11-19 初回実行）

### 実行環境・手段

- ビルドツリー: `out/build/x64-Debug-Agent`（Ninja / MSVC 14.52.36807 / CMake 4.4.4）。
- 評価ハーネス: `tools/BurnAwayLab`（`ArtifactBurnAwayLab`）。合成紙面（繊維ノイズ＋印刷バー）を生成し、Burn Away の CPU 参照実装を各 Progress で適用し、PNG と統計を出力する。
- 生成物: `captures/burn-away-20261119/`（`source.png`, `burn_p00/10/25/40/60/80/100.png`）。`/captures/` は Git 追跡外のため、再現手順は本書とラボ README に記載する。

### 統計（CPU 参照、960×540、0.5 グレ背景合成）

| Progress | mean alpha | opaque fraction | mean luminance | max luminance |
| --- | --- | --- | --- | --- |
| 0.00 | 1.000000 | 1.000000 | 0.784483 | 0.928605 |
| 0.10 | 0.886330 | 0.882398 | 0.682966 | 1.407562 |
| 0.25 | 0.711266 | 0.707797 | 0.550058 | 1.399446 |
| 0.40 | 0.531864 | 0.527311 | 0.406863 | 1.404552 |
| 0.60 | 0.304050 | 0.300226 | 0.223856 | 1.403053 |
| 0.80 | 0.065945 | 0.062407 | 0.048748 | 1.402104 |
| 1.00 | 0.000000 | 0.000000 | 0.000000 | 0.928605 |

- 残存 alpha は Progress に対して単調減少し、Progress 1.00 で alpha 最大値 0（完全消失）。**効果の正しさ（合成 ground truth）: 合格**。
- Progress 0.00 で mean alpha 1.0・mean luminance が入力と一致。**中立設定が厳密 identity: 合格**（契約テストでも 1e-6 以内で一致）。
- max luminance は 1.40 前後で、縁のEMS 発光が線形 HDR 域で 1.0 を超える（.clip される）。発光は意図した HDR 値であり、入力値の clip 率ではない。

### 目視評価

| 項目 | 結果 | 判定 |
| --- | --- | --- |
| 炭化帯（char） | 生存側に、境界に沿った不規則な暗色帯が形成される。濃度は黒〜暗褐で、暖色寄り | 合格 |
| 縁の発光（ember） | 炭化帯の外縁に沿って細い橙の帯。`burn_p40.png` / `burn_p80.png` で形状が破綻しない | 合格 |
| 境界の形状 | fBm ノイズにより水平線にならない。端に段差や階段状アーティファクトなし | 合格 |
| 消失領域 | 消失部分は完全に透明（背景色のみ） | 合格 |
| 発光の白飛び | 初期値（Ember Intensity 2.0 / Ember Width 0.02）では発光部が白く飽和し「ネオン帯」に見えた | **不合格 → 修正済み**（既定を 0.8 / 0.012 へ、炭化色を 0.16/0.10/0.06 へ変更） |

### 実施中に発見し修正した欠陥

1. **内部バッファのチャンネル順**: `ImageF32x4_RGBA::rgba32fData()` は B,G,R,A 順。実装が R,G,B と解釈していたため、炭化・発光の色が channel を跨いで適用されていた。CPU 実装（BGRA 書き込み）と generic resident シェーダ（`.bgra` スワizzle）の両方を修正し、**着火順を検証する契約テストを追加**（`CharBandDarkensWarmChannelsInTheRightOrder`）。
2. **アルファ契約**: レイヤーエフェクト面の合成は **straight alpha**。当初「premultiplied」と仮定して発光に alpha を掛け、さらに発光で alpha を底上げしていたため、消失領域に紙の色が残る状態になっていた。straight alpha 前提（プレビュー経路 `toQImage()` → `SurfacePixelTarget::Rgba8SrgbStraight` と整合）に修正し、alpha の底上げと発光への alpha 乗算を削除。
3. **発光のゲーティング**: 発光を侵食係数でgateすると、境界上（距離 0）で alpha が 0.5 となり発光の強さが半減する。straight alpha では最終 alpha が発光を自然に減衰させるため、ゲーティングそのものを廃止。

### 未実施（未検証）

- **GPU 経路の実測**: generic resident シェーダの実行・CPU との差分測定は未実施。シェーダは CPU と同じ式を同じノイズハッシュで実装しているが、**parity は未検証**。
- **Vulkan 経路**: 未検証。
- **性能計測**: 処理時間・GPU メモリは未計測。
- **実アプリ経路**: 合成レイヤー上での確認は未実施（ラボは Effector API 直叩き）。
- **高輝度素材・透過 PNG・極端設定**: 合成紙面のみ。異方的な alpha 境界、極小/極大パラメータの画像評価は未実施。
- **解像度の比較**: 空間パラメータは層高正規化のため設計上は不変だが、実測比較は未実施。

以上より、本 R&D は「**CPU 参照経路は受入可**、**GPU 経路・性能・実アプリ経路は未検証**」の状態であり、効果の製品統合完了を意味しない。GPU parity と実アプリ経路の評価完了が次の受入条件。

## 8. 灰・火の粉の追加（2026-11-19）

### 実装

`ParticlePresets` に burning paper 用の2プリセットを追加し、Particle Layer のプリセット選択に���録した。

| プリセット | 内容 | 主要パラメータ |
| --- | --- | --- |
| `ash`（灰） | 紙片がゆっくり上昇しながら翻り、減衰して消える | Rectangle 960×16 / Continuous 26/s / life 3.0–6.5s / speed 8–26 / 上方向 35° / scale 2–5→1–3 / 灰褐 198,191,178 → 92,86,78 / drag 0.9 / gravity +26 / wind 18 / turbulence 0.45/22 / 回転 -70..70°/s |
| `ember`（火の粉） |  burning front から飛び出し、冷却しながら縮む | Rectangle 960×10 / Continuous 45/s / life 0.7–1.9s / speed 55–170 / 上方向 28° / scale 1.5–3.5→0 / 橙 255,186,92 → 170,40,0 / drag 1.3 / gravity +70 / turbulence 0.9/26 |

登録箇所（Particle Layer 側の3面）:
- `Artifact/include/Generator/ArtifactParticleGenerator.ixx` — `ParticlePresets::ash()` / `::ember()` 宣言
- `Artifact/src/Generator/ArtifactParticleGenerator.cpp` — 実装
- `Artifact/src/Layer/ArtifactParticleLayer.cpp` — `loadPreset()` の分岐、`availablePresets()` の一覧、`particle.quick.preset` の index→name 対応表（soft/hard range を 0–19 → 0–21 へ）

### 使い方の前提（既知の制約）

- **プリセット選択は index のみ**で、`particle.quick.preset` は Integer 型（0–21）。`ash` = 20、`ember` = 21 に対応するが、UI にはプリセット名が出せないため、**現状は index を指定する運用**になる。PropertyType に列挙型が無いことによる制約であり、今回の範囲では見送った。
- **エミッターは burning front に自動追従しない**。燃える位置は `Progress` のキーフレームで前端が動くため、粒子レイヤーのエミッター位置も同じキーフレームで合わせる運用になる（`tools/BurnAwayLab` はこの対応.front を同じ正規化式で計算している）。
- **加算的な発光はレイヤーブレンドモード側**の責務。粒子データは通常合成で、火の粉を明るく見せたい場合は Particle Layer のブレンドモードを Add にする。

### 検証状況

- コンパイル: `ArtifactParticleGenerator.cpp` / `ArtifactParticleLayer.cpp` の個別コンパイルは成功。
- 画像評価（合成フレーム）: **実施済み**（下記 §9）。`captures/burn-away-20261119/` に各進捗の `burn_pNN.png`（効果のみ）と `burn_pNN_particles.png`（灰・火の粉合成）と `particles_only_pNN.png`（粒子のみ）を生成。
- 粒子単独の見た目の確認（GPU 経路 / ソフトフォールバック差分、密度・視認性）: ソフト経路のみ確認。GPU 経路（Diligent billboard）との差分は未計測。

## 9. クオリティ向上（2026-11-19 2nd pass）

### 効果側（Burn Away）

| 追加 | 内容 | 既定値 |
| --- | --- | --- |
| `Spark Density` | 前線に沿って散在する白熱した火花点。滑らかな橙の帯を「火の粉の粒」に分割する | 0.5 |
| `Residue` | 焼けた領域に残す薄い灰の膜。straight alpha のため膜そのものは独立した coverage を持つ | 0.0（透明の挙動を維持） |
| 煤（soot）帯 | 炭化帯の 3 倍幅の弱い減光帯。上面に「冷えた煙」として softly に残る | 炭化量の 45% 相当（追加パラメータなし） |
| 炭化の色 | 乗算係数を (0.16, 0.10, 0.06) から「暗転量」ベースの (1-0.84d, 1-0.90d, 1-0.94d) へ変更し、炭化と煤を合成 | — |

- **GPU パラメータの組み替え**: generic resident のパラメータ枠は 16 枠しかないため、火の粉の色を 1 float に pack（8bit/ch、2^24 未満なので float 往復で厳密一致）して `Residue` と `Spark Density` の枠を捻出した。CPU と GPU で同じ値を使うため parity は保たれる（contract test で pack の往復を検証）。

### 粒子側（ash / ember）

- 灰: rate 26→55、size 2–5→5–10、色 (156,150,140)→(132,126,116)（明るい紙の上でも視認できる暗さ）、speed 8–26→14–42、乱流 0.45/22→0.35/34。
- 火の粉: rate 45→80、size 1.5–3.5→2.5–6.5、speed 55–170→90–280、life 0.7–1.9→1.0–2.6、重力 70→95。

### 実施中に見つかった実装側の落とし穴（いずれも評価側で検出した）

1. **粒子シミュレーションは 1 回の update で上限までしか進まない**。`ParticleSystem::update()` は固定タイムステップ＋最大サブステップ数でクランプされるため、`renderFrame(w, h, 1.5)` のような大きな delta を 1 回渡すと約 0.07 秒分しか進まず、粒子がほぼ出ない。**フレーム単位で `1/60` 刻みに進める**のが正解（备忘: `Insight.md` に記録）。
2. **ラボの合成コードのチャンネル順ミス**。`QRgb` は 0xAARRGGBB なので下位バイトは Blue。`(pixel >> (8 * channel)) & 0xFF` で R/G/B を取り出す実装は R と B が入れ替わり、灰の粒子が青く見えた（正解は `qRed/qGreen/qBlue`）。これは評価側（ラボ）のバグで製品コードではない。製品側の問題にも見えるため、粒子のみの PNG を別途出力して切り分けたうえで修正した。

### 再評価後の目視結果

- 炭化帯: 不規則な暗色帯 ✓
- 煤の帯: 上面に柔らかい煙状の減光 ✓
- 火花: 前線に白熱した点状ハイライト ✓
- 火の粉（粒子）: 前線から橙色で上昇 ✓
- 灰（粒子）: 前線上をゆっくり漂う暗い粒 ✓
- 消失領域: 完全に透明（Residue = 0 の既定）✓

### 残る未検証

- GPU 経路の parity（シェーダと CPU の同一式だが実測差分なし）
- Vulkan 経路、性能計測、実アプリ経路、高輝度／透過 PNG 素材、極端設定
- `Residue > 0` の見た目は合成フレームで確認していない（契約テストで alpha が 0 より大きく 1 未満であることを確認したのみ）

## 10. GPU parity の実測（2026-11-20）

### 手段

- アプリ GUI テストのフィクスチャ（`ARTIFACT_UI_TEST_EFFECT_SUITE_*`）は offscreen 環境で swap chain 作成に失敗し、GPU 出力（Accum RT）を得る前にハングした（CPU 実行は取得成功）。したがって**アプリ経路は測定に使えない**と記録し、代わりに評価ラボから**製品が登録している generic resident シェーダそのもの**を GPU にディスパッチして比較する方式を採った。
- `tools/BurnAwayLab --gpu-parity`：効果のコンストラクタが登録した `burn_away` シェーダを `findGpuGenericShader` で取得し、コンポジション層パイプラインと**同じ prelude・同じ cbuffer レイアウト（ResidentGenericParams: p[16] + width/height/time/frame/historyValid）・同じ 16 パラメータ**で D3D12 compute にディスパッチ。入力は `makeGpuImageUploadBuffer` で canonical RGBA へ正規化してアップロード、readback は RGBA、CPU 出力は B,G,R,A のため比較は論理 RGBA へ揃えた。
- 入力: ラボの合成紙面 960×540、Progress 0.45 / Ember Intensity 1.2 / Spark Density 0.6 / Char Amount 0.9。

### 結果

| 指標 | 値 | 合格基準 | 判定 |
| --- | --- | --- | --- |
| mean absolute error | 0.000000 | ≤ 2/255 (0.00784) | **合格** |
| RMSE | 0.000001 | ≤ 2/255 | **合格** |
| max difference | 0.000084 (≈0.02/255) | ≤ 2/255 | **合格** |
| 差分画素（>0.5/255） | 0 / 518400 (0.0000%) | 境界が同位置（±1 画素） | **合格** |

- 差分画像: `captures/burn-away-20261119/parity_diff.png`（全面ほぼ黒、最大差分位置 (540,253) の 1 画素のみ）。
- **GPU 経路と CPU 参照は float 丸め以外の差が存在しない**。この効果は「残り」の重箱（GPU parity）ではなく、未検証の項目を再検証済みへ移す結果が得られた。

### 同時に見つけて修正した実装上の欠陥

- **CPU カーネルのチャンネル順固定**（重要）: 実装当初、内部バッファが B,G,R,A 順であると仮定して色演算を書いていた。しかし backing storage の順序は入力源によって異なり（QImage 由来 = BGRA、OIIO デコード = RGBA）、実アプリでは OIIO 由来の画像レイヤーで **R/B が入れ替わる**故障になり得た。GPU 側は canonical RGBA でアップロードされるため不一致。`colorDescriptor().channelOrder` を見て読み書きを分岐するよう修正し、**GPU の canonical RGBA 契約と一致**させた（`Insight.md` の「CPU effect kernels must not hardcode channel order」規則に沿う）。
- **プリセット名の UI 化**: パーティクルレイヤーのプリセット選択は index のみ（`ash`=20 / `ember`=21）で UI に名前が出なかった。プロパティエディタには Integer プロパティの enum 選択肢（名前テーブル）机制が既にあり、`particlePresetNames()` / `particlePresetDisplayNames()` を単一ソースとして追加し、`Ash` / `Ember` が名前で選択できるようになった（プロパティシステムの変更は不要）。

## 関連資料

- [画像エフェクト R&D 画像評価ルール](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)
- [Professional Depth of Field Effect R&D](../planned/MILESTONE_DOF_PROFESSIONAL_EFFECT_RND_2026-11-10.md)
