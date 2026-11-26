# Production Image Effect Controls

**最終更新:** 2026-11-26

**ステータス:** In Progress

## 目的

画像エフェクトを「数がある」状態から、静止画・連番画像の実制作で再現可能に調整、アニメーション、比較できる状態へ進める。GPU を通常経路とする方針を維持し、CPU は比較・安全な fallback として残す。

## 現状と判断

- factory 登録は約141件、`getProperties()` 実装は **98/101**（BlurEffect / NoiseEffect / SurfaceFXEffect の3件が未実装）。
- 2026-08-22 の実測によるパラメータ数分布（getProperties body 内の property 登録呼び出しカウント）:
  - **8+ params (production): 18 effects** — Glow(12), ApertureShapeBlur(10), ChromaKey(9), RadioWaves(9), LiftGammaGain(9), LensDistortion(9) 等
  - **5–7 params (standard): 39 effects** — 大半の標準エフェクト
  - **3–4 params (basic): 32 effects** — Exposure(3), Sharpen(3), Kuwahara(3) 等
  - **1–2 params (minimal): 9 effects** — FreezeFrame(1), LumaKey(1), Invert(2), Grayscale(2) 等。機能が単純なため妥当なものも含む
- Levels、Curves、IBK Keyer、Glow、Pixel Sort Pro は実制作に近い項目を持つ。
- パラメータ「数」自体は中央値5前後で標準的だが、PIEC-2 のメタデータ正規化（tooltip/unit/animation flag/soft range）が全 effect の ~5% のみ完了しているのが主要ギャップ。
- 一部 GPU-only effect（BlurEffect等）は cbuffer を直接持ち getProperties() を経由しないため Inspector から操作不可。
- preview / GPU / CPU / render queue の画素 parity、HDR・premultiplied alpha の runtime 受入は未完了または未検証である。

## 範囲

- `ArtifactAbstractEffect` の共通編集契約と共通合成。
- Inspector / Property Widget における共通プロパティの編集・キーフレーム反映。
- Blur / Bloom、Keying、Lens / Distortion、Temporal、Color の優先 effect 群のパラメータ拡張。
- GPU-first のパラメータ反映と CPU fallback の比較可能性。

## 非目標

- 全 effect を一度に同じ項目数へ揃えること。
- ソフトレンダラーの新機能化。
- 新しい signal / slot、QtCSS、QImage hot path の導入。
- runtime 検証なしで production-ready と宣言すること。

## フェーズ

### PIEC-1: 共通制作コントロール

- 全 effect に Effect Enabled、Effect Mix、Allow Overscan、既存の effect region / mask を一貫して公開する。
- Mix は effect 実装固有ではなく、処理済み結果と入力を共通合成して 0–1 の範囲へ制限する。
- shared property は Inspector と layer Property Widget から編集・アニメーション可能にする。

**完了条件:** 代表 effect を含む全 effect で共通 controls が同じ名前・意味・安全範囲で働く。

**進捗:** 実装済み・runtime未検証。Effect Mix / Enabled / Allow Overscan の契約と共通合成を追加し、既存 Property Widget、project保存、preset、effect複製、Inspector copy/paste、WebUI状態出力、Effect Service（layer / composition）を共通契約へ接続した。実機での preview / export / keyframe 受入は PIEC-5 で確認する。

### PIEC-2: パラメータ品質の正規化

- 各公開パラメータに stable name、表示名、既定値、hard/soft range、step、単位、tooltip、アニメーション可否を付与する。
- 数値 enum を可能な範囲で選択肢として表現し、曖昧な生 float を減らす。
- effect 固有の Mix がある場合は PIEC-1 の Effect Mix と二重の意味を持たないよう整理する。

**完了条件:** 優先 effect 群に安全な編集範囲と保存・キーフレームに耐える stable property identity がある。

**進捗:** Blur、Edge Bloom、Chroma Key、Lens Distortion、Displacement Map、Aperture Shape Blur、Temporal Denoise、Color Wheels、Drop Shadow の既存項目へ表示名、既定値、hard/soft range、step、単位、tooltip を追加した。Blurは従来非公開だった Iterations / Mode / Edge Threshold / Premultiplied Alpha も公開した。2026-08-20 に EdgeBloom、ApertureShapeBlur、ChromaKey、LensDistortion、TemporalDenoise の表示名・min/max・tooltip の不足を正規化した。全 effect 群への展開は継続する。

### PIEC-3: 高頻度 effect の拡張

1. Blur / Bloom: threshold knee、quality/downsample、channel、edge mode、overscan。
2. Keying: black/white clip、matte choke/feather、despill controls、edge/detail preview、入力色空間。
3. Lens / Distortion: radial/tangential coefficients、anamorphic、edge fill、profile/overscan。
4. Temporal: scene-cut、motion-adaptive、ghost suppression、luma/chroma、confidence preview。
5. Color: linear/log 前提、HDR 値、luma preservation、LUT/working-space との責務分離。

**完了条件:** 各群で代表 effect 1件以上が実制作の調整・比較に必要な controls を持つ。

**進捗:** Chroma Key に Matte Black Clip / Matte White Clip、opaque grayscale の Preview Matte、RGB距離／輝度距離を切り替える Luma Only、Spill Desaturation を追加し、Similarity / Edge Softness の出力から matte の下限・上限と despill 強度を明示的に調整できるようにした。2026-09-13 に Core の入力／出力を linear-premultiplied 契約へ揃え、choke／matte blurを除く通常key/despillをcomposition GPU chain向け専用常駐CSへ接続した。2026-10-26 に Core Chroma Key APIへ alpha mode を追加し、premultiplied sourceをstraight colorで判定して同じalpha conventionで出力するようCPU/GPU双方を更新した。続けて Luma／Difference Key にもalpha modeを通し、CPU・単体GPU・composition resident GPU経路のpremultiplied判定／出力を修正、CoreとArtifactラッパー契約テストを追加した。ゼロalpha／NaN入力に対する有限出力の共通契約ケースも追加した。RGBA/BGRA順のChroma比較とLight WrapのCPU近傍読み取り競合も修正した。これらのテストとGPU実行時parityは未実行・未検証。Lens Distortion には CPU/GPU 同一式で Tangential X / Y（decentering）、Radial Quadratic（2次半径係数）、Transparent Edges を追加し、同日に専用常駐CSへ接続した。Edge Bloom には CPU/GPU 共通の Threshold Softness、Edge Boost、Radius サンプル距離、Draft／Standard／High の Quality を追加した。Temporal Denoise は sampled average-luma scene-cut rejection、分散ベースの Motion Adaptive、Ghost Suppression の調整を持つ。keyer の複数 sample、色空間指定、Lens profile の読込 UI、GPU runtime parity は未実装または未検証。

### PIEC-4: 制作診断と比較

- before/after、effect contribution、alpha / matte / channel preview、clipping と範囲外警告を整備する。
- GPU / CPU fallback の理由、effect cost、preview / export 差分を追跡できるようにする。

**完了条件:** 見た目の問題をパラメータ、色管理、backend 差のどれとして調べるべきか判断できる。

### PIEC-5: Production Acceptance

- 静止画・連番画像に対し、HDR値、透明境界、mask、複数 effect stack、GPU/CPU fallback を含む固定 fixture を定義する。
- Preview / Render Queue での output parity、保存・再読込、keyframe を runtime で確認する。

**完了条件:** 優先 effect 群を production-ready と判断できる実測・画素比較の記録がある。

## 依存・関連

- [GPU Effect Parity](MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md)
- [Color Grading](MILESTONE_COLOR_GRADING_2026-03-29.md)
- [Effect-level Mask](FEATURE_EFFECT_LEVEL_MASK_2026-07-02.md)
- [Color / Alpha Contract](MILESTONE_COLOR_ALPHA_CONTRACT_UNIFICATION_2026-07-18.md)

## 次の実装単位

PIEC-2 の代表5 effect（`EdgeBloomEffect`、`ApertureShapeBlurEffect`、`ChromaKeyEffect`、`LensDistortionEffect`、`TemporalDenoiseEffect`）の property metadata 正規化は完了した。次は PIEC-3 として keyer の複数 sample / 色空間、Lens profile、Temporal の motion-adaptive / ghost suppression、Blur/Bloom の quality / edge mode を effect 群ごとに小さく追加する。

## 2026-11-25 — Luma Key Viewport Controls

Composition Viewport の scalar effect control に Luma Key を opt-in し、Low / High Threshold、Softness、Choke、Matte Blur を既存 setter の hard range に合わせて編集できるようにした。しきい値・softness は 0–100%、Choke は -100–100%（最大約3 px の dilation / erosion）、Matte Blur は 0–2x（3×3 matte blend の0–100%）で表示する。View Mode は 0 Final / 1 Matte の named choice として選択できる。各 property に表示名、既定値、範囲、step、単位、tooltip、animation metadata を与えた。描画実行、Undo / Redo、matte 画質・alpha、CPU/GPU parity、fallback 性能の評価は未実施であり、Viewport control の追加だけでは受入完了としない。

## 2026-11-25 — VP Scalar Effect Editing

Composition Viewport の詳細コンテキストメニューから、数値項目の現値と単位を確認し、全 effect の共通 `Effect Enabled` と、有効 effect に対する共通 `Effect Mix`、専用 scalar descriptor で opt-in した effect の有限 hard range 付き `Float` プロパティを編集できる経路を追加した。無効 effect は Enabled toggle のみ表示し、Viewport から再有効化できる。Blur、Glow、Drop / Inner Shadow、Exposure、Edge Bloom、Aperture Shape Blur、Depth Bokeh、Chroma Key、Lens Distortion、Brightness / Contrast、Hue / Saturation、Lift / Gamma / Gain、Vibrance、Temporal Denoise は既存 CPU/GPU implementation または PImpl の値を更新する経路を確認して opt-in した。Blur の `Strength` / `Radius` / `Iterations`、Glow の `Intensity` / `Quality Layers`、Drop / Inner Shadow の `Distance`、Exposure の `Exposure` / `Offset` / `Gamma`、Aperture Shape Blur の `Radius`、Depth Bokeh の `Focus Distance`、Temporal Denoise の `History Frames`、色補正系の代表項目は専用 descriptor を維持し、その他の対象 Float 項目は metadata から追加して同じ項目の重複表示を避ける。Drop Shadow の Shadow Only は明示 toggle descriptor で追加した。Inner Shadow の Distance / Softness / Opacity に finite range、step、unit と説明を付け、既存 setter の範囲と同期させる。整数値は整数 hard range と正の整数 step を検証した専用 descriptor のみ扱う。hard range が 0–1、step が 1 の二値 descriptor は現値チェック付きの1クリック切替にし、既存 display label の選択肢を保つ。opt-in 済み effect の Color 項目は Composition Viewport の詳細メニューから既存 `FloatColorPicker` で編集し、accepted 値のみ effect service 経由で確定する。キャンセル時は値を変えず、既存の Undo command を利用する。Boolean 項目は専用 toggle descriptor を持つ Glow Contribution Only、Drop Shadow Shadow Only、Temporal Denoise Motion Adaptive / Reject Scene Cuts に限り、チェック状態付きメニューから既存 effect service と Undo command で変更する。Exposure、Brightness / Contrast、Hue / Saturation、Lift / Gamma / Gain、Vibrance の Float metadata には既定値、hard / soft range、step、必要な表示単位、tooltip、アニメーション可否を記録した。範囲は hard range を使用し、soft range が定義されていればドラッグ速度の基準に使う。Property metadata の step / unit / display label を利用し、未定義の step は範囲から導出する。数値 readout は最大 10 Hz で更新し、パラメータ preview は pointer 値の変化に追従する。範囲不明、キーフレーム・Expression・Envelope 付きの項目は対象外とする。Temporal Denoise は setter ごとの CPU implementation 再生成をやめ、既存実装の値を更新するよう変更した。

正規化された 0–1 range と `%` unit を持つ Float は Inspector と VP の両方で 0–100% 表示に変換する。既存の名前依存 percent 表示は互換用に維持し、通常の numeric properties は metadata から換算する。

ドラッグ中のプレビューは既存の effect setter 経路を使い、確定時は既存の `ArtifactEffectService` と Undo 経路へ渡す。感度は通常 1 倍、Shift で 0.1 倍、Ctrl で 10 倍とし、両方を押すと通常感度になる。ドラッグ開始時から修飾キーを保持した場合とドラッグ中に切り替えた場合は、各ポインター移動区間の差分を感度別に積算し、値が飛ばないようにする。ドラッグ中の数値ハンドルに感度倍率を併記する。メニュー生成時の property metadata 参照は操作開始時のコールドパスで、ドラッグイベントごとに property 一覧を再構築しない。ソース差分の静的確認のみで、ビルド、Undo / Redo、画像品質、D3D12 / Vulkan、性能の runtime 受入は未実施。

### Vignette Controls

Vignette を Viewport の scalar effect editing に opt-in し、Amount / Radius / Feather / Center X / Center Y を有限範囲・step・表示名・tooltip・アニメーション可否付きで公開した。Amount と中心座標は 0–100% 表示にし、VP から既存 property service 経由で値を反映する。setter ごとに `VignetteCPUImpl` を作り直していた同期処理を既存 CPU implementation の値更新へ変更し、連続ドラッグでの CPU implementation heap allocation を避ける。GPU implementation も既存インスタンスのパラメータを更新する。画像プレビュー、Undo、CPU/GPU・preview/final parity と performance は未実施。

### Sharpen Controls

Sharpen を Viewport の scalar effect editing に opt-in し、Amount / Sigma / Threshold を既存 metadata の hard range、soft range、step、Sigma の px 単位とともに表示する。既存 setter は CPU/GPU implementation の値を更新し、Sharpen は `appendGpuSpatialNodes()` による既存 resident path を持つ。Viewport 操作、画像品質（細線、halo、局所コントラスト）、Undo、backend parity、性能は未実施。

Magic Sharp も Amount descriptor で opt-in し、Master Amount、各 detail band、Noise Threshold、Edge / Shadow Protection、effect-local Mix を既存 hard range から VP に列挙する。property setter は既存 `Impl` 値を直接更新する。共通 Effect Mix は処理済み結果との合成量、Magic Sharp の Mix は multiscale sharpening の内部量として別項目のまま扱う。実操作・Undo、細線／halo／暗部ノイズの画質、CPU preview 性能は未検証。

### White Balance Controls

White Balance（UI表示名はTint）を VP の scalar effect editing に opt-in し、Temperature (K)、Tint、Brightness を finite range と step 付きで操作する。Temperature の単位を K とし、既存 CPU/GPU implementation は setter から同じインスタンスのパラメータを更新する。Temperature は専用 descriptor、Tint / Brightness は既存 Float properties の hard range から列挙する。Preset（0 Custom / 1 Daylight 5500 K / 2 Cloudy 6500 K / 3 Shade 7500 K / 4 Tungsten 3200 K / 5 Fluorescent 4000 K / 6 Flash 6000 K）を名前付き VP choice にし、named presetは温度だけを設定してTint / Brightnessを保つ。Customは現在値を保持する。Temperatureの編集、Preset選択、Undo / Redoはfull-property snapshotで往復し、snapshot読込はPresetを他のproperty適用後に行う。Viewport dragを開始値に戻す時もPreset表示を維持する。既存CPU、direct GPU、resident HLSLの白色点・色温度式、tint / brightness式は変更しない。評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に追記済み。GPU resident path、Undo、neutral patchの色応答、alpha、CPU/GPU・preview/final parity、性能、目視判定は未実施。

### Color Balance Controls

Color Balance を VP の scalar effect editing に opt-in し、Strength を Inspector と VP の両方で 0–100% 表示してドラッグし、Preserve Luma を状態表示付き toggle で切り替える。Strength は既存 CPU/GPU implementation の設定値を更新し、Preserve Luma は既存 boolean property setter を通す。Preset は既存の 0–4 mapping を使う named choice control から選択できる。Preset 表示は設定値から判定し、手動で選んだ Custom は明示状態として保持する。Preset は複数 settings を一括変更するため、VP 選択と Undo / Redo は `ArtifactPresetManager` 全体 snapshot と `EffectPresetSnapshotCommand` を通し、各選択を一つの command にまとめる。snapshot JSON で表せない derived Custom / 明示 Custom の内部 pin 状態も command に含める。GPU resident path と既存の画像処理式は変更しない。Viewport 操作、Preset / Undo / Redo、輝度保持、neutral・飽和色の画素比較、CPU/GPU・preview/final parity、性能は未実施。

### Color Tint Controls

Color Tint を VP の scalar effect editing に opt-in し、Amount to Tint を Inspector と VP の両方で 0–100% 表示して調整し、Map Black To / Map White To を既存 `FloatColorPicker` から編集できるようにした。既存 setter が同じ CPU / GPU implementation へ値を反映する。画像処理式・GPU resident path・色選択のイベント配線は変更しない。Viewport 操作と Undo / Redo、picker cancel、グレースケール ramp と中間調、alpha と背景合成、CPU/GPU・preview/final parity、時間安定性、性能は未実施。

### Wave Controls

Wave を Viewport の数値編集に opt-in し、Amplitude / Frequency / Phase を Inspector と Viewport の双方から操作できる metadata を追加した。Amplitude は px、Frequency は cycles/px、Phase は rad とし、既存 CPU / GPU setter の範囲と位相の周期性を考慮して操作範囲・step を定義した。Wave Type は 0 Sine / 1 Cosine、Orientation は 0 Horizontal / 1 Vertical の名前付き choice として Viewport から選択できる。各 property は整数範囲 0–1、step 1 とし、既存 setter・serialization・CPU / GPU parameter mapping を利用する。波形式と render routing は変更しない。CPU / GPU 実装と画像処理式は変更していない。Viewport 操作、Undo / Redo、細線と境界の変位、alpha、backend / preview parity、時間安定性、性能は未実施。

### Spherize Controls

Spherize を Viewport の数値編集に opt-in し、Amount / Radius / Center X / Center Y を既存 CPU / GPU implementation へ反映する。Amount は -100〜100%、Radius は画像の短辺比、中心座標は画像幅・高さ比として Inspector と VP の両方で 0–100% 表示を揃える。全項目に setter 範囲に対応した hard range、soft range、step、tooltip とアニメーション可否を付け、画像処理式と implementation の生成経路は変更しない。Viewport 操作、Undo / Redo、変位と境界、alpha、CPU/GPU・preview/final・D3D12/Vulkan parity、時間安定性、性能は未実施。

### Ripple Controls

Ripple を Viewport の数値編集に opt-in し、Center X / Y、Amplitude、Frequency、Decay、Phase を表示名・範囲・soft range・step・単位・tooltip 付きで公開した。中心座標は 0–100%、振幅は px、Frequency は cycles/px、Decay は 1/px、Phase は rad とする。Amplitude は専用 descriptor、残りの有限範囲 Float は既存 metadata 列挙を使い、既存 setter が CPU / GPU implementation の値を更新する。CPU 変位 mapper と登録済み GPU resident shader の式は変更しない。Viewport 操作、Undo / Redo、リング変位と減衰、alpha、CPU/GPU・preview/final・D3D12/Vulkan parity、時間安定性、性能は未実施。

### Pinch / Bulge Controls

Pinch / Bulge を Viewport の数値編集に opt-in し、Center X / Y、Radius、Amount を既存 metadata で操作可能にした。中心座標と Radius は正規化値を 0–100% 表示し、Amount は -100〜100% で負値が Pinch、正値が Bulge となる。Amount は専用 descriptor、残る数値は Float metadata から列挙する。既存 CPU mapper と GPU resident shader に値を渡す setter は実装を再生成せず、変形式も変更しない。Viewport 操作、Undo / Redo、輪郭と境界、alpha、CPU/GPU・preview/final・D3D12/Vulkan parity、性能は未実施。

### Magnify Controls

Magnify を Viewport の数値編集に opt-in し、Center X / Y、Radius、Amount を Inspector と同じ値で操作できる metadata を追加した。中心座標と Radius は比率を 0–100%、Amount は -100〜100% 表示とし、既存の範囲・step・説明を設定する。専用 Amount descriptor で opt-in し、他の Float 項目は metadata から列挙する。CPU mapper と GPU resident shader の式および既存実装値の更新経路は変えていない。Viewport 操作、Undo / Redo、拡大領域の品質、alpha、CPU/GPU・preview/final・D3D12/Vulkan parity、性能は未実施。

### Polar Coordinates Controls

Polar Coordinates を Viewport の数値編集に opt-in し、Center X / Y、Radius、Amount を表示名・範囲・soft range・step・単位・tooltip 付きで公開した。中心位置と短辺に対する半径は 0–100%、Amount は元画像と全 polar remap の混合率として 0–100% 表示する。Amount の専用 descriptor で opt-in し、他の bounded Float は metadata 列挙に任せる。既存の CPU mapper、GPU resident shader と setter の implementation 再利用経路は変更しない。Viewport 操作、Undo / Redo、変換境界、alpha、CPU/GPU・preview/final・D3D12/Vulkan parity、時間安定性、性能は未実施。

### Tritone Controls

Tritone を VP の scalar effect editing に opt-in し、Balance / Softness / Master Strength / Color Mix と Shadow / Midtone / Highlight Color、Preserve Luma を操作できる metadata と descriptor を追加する。Preset は 0 Custom / 1 Cinematic / 2 Teal and Orange / 3 Warm Gold / 4 Cold Blue の named choice とし、Custom選択は現在settingsを保持して pin する。Preset適用は全settingsとCustom pinを含む一括snapshotでUndo / Redoし、単一property編集後も値とderived preset表示を一致させる。4つの normalized Float は Inspector と VP の両方で 0–100% 表示する。全数値範囲は既存 setter の制限に合わせ、色 picker と boolean toggle は既存 effect service / Undo 経路を再利用する。既存 CPU/GPU implementation、resident shader、画像式は変更しない。Viewport 操作、Preset / Undo / Redo、R&D fixture のruntime比較、色と alpha、preview/final・CPU/GPU・D3D12/Vulkan parity、時間安定性、性能は未実施。

### Find Edges Controls

Find Edges を VP の数値編集に opt-in し、Amount (0–5×) と Invert toggle を既存 property setter 経由で操作できるようにした。共通 Enabled / Mix と合わせて、既存の CPU / GPU implementation を再生成せずパラメータだけ更新する。描画式は変更していない。現在の CPU Laplacian-normalize / color-mix と GPU Sobel / grayscale の差、および GPU readback 経路を評価計画へ明記し、同等性や性能が確認済みとは扱わない。VP 操作、Undo / Redo、画像・alpha、preview/final、CPU/GPU・D3D12/Vulkan parity、時間安定性、性能は未実施。

### Photo Filter Controls

Photo Filter の Density / Brightness / Contrast / Saturation Boost、Filter Color、Preserve Luma を Viewport の数値・color picker・boolean toggle から操作できるようにした。範囲、step、既定値、soft range、unit、tooltip、アニメーション可否を property metadata に設定し、Density の normalized 値は 0–100% 表示にする。各 setter は既存 CPU / GPU implementation の設定を更新する。Preset 表示は設定値から判定し、named preset の値へ Undo で戻ると名前も戻り、明示選択された Custom は維持する。Preset は0 Custom / 1 Warm / 2 Cool / 3 Sepia / 4 Cyan / 5 Rose のnamed choiceとして選択できる。Presetsは全settingsを置き換えるため、一件の full snapshot Undoに隠しCustom pin状態を含める。既存 CPU HSL / luma 式と GPU 式の差、premultiplied RGB、upload / readback 経路を評価計画に記録した。Viewport 操作、Undo / Redo、画質、色・alpha、CPU/GPU parity、時間安定性、性能は未実施。

### Threshold / Posterize Controls

Threshold を既存 setter から VP 編集できるようにし、正規化された 0–1 値を Inspector / VP の両方で 0–100% 表示する。Posterize の Levels は 2–64 の整数 step として公開する。両 effect の hard range、既定値、step、unit、tooltip を property metadata に追加し、既存の処理式と CPU / GPU 実装は変更しない。Viewport 操作、Undo / Redo、階調境界・banding、alpha、CPU / resident GPU / direct GPU path、preview/final、D3D12/Vulkan parity、性能は未実施。

### Black & White / Grayscale / Invert Controls

Black & White の Reds / Yellows / Greens / Cyans / Blues / Magentas と Tint Amount を VP 数値編集に公開し、Tint Color は既存 `FloatColorPicker` から編集する。Reds の descriptor で effect を opt-in し、6つの hue weight は -2–3、Tint Amount は 0–1 の hard / soft range、step、tooltip を metadata に持たせる。Grayscale は Strength を 0–1（0–100% 表示）で公開し、Mode は0 Perceptual / 1 Linear Luminance / 2 Desaturate の named choice で選択できる。Invert は Strength を0–1（0–100%表示）で公開し、Channel は0 RGB / 1 Red / 2 Green / 3 Blue / 4 Alpha の named choice にする。両方の整数 property に 0–2 / 0–4 の hard range、整数step 1、ja / en / zh / zh-TW のlabelsを設定し、既存setterとCPU / GPU mappingを再利用する。処理式・backend routingは変更しない。各 setter は既存 CPU / GPU implementation の値を更新し、effect の画像式・GPU 経路は変更しない。Viewport 操作、Undo / Redo、画像品質、alpha、path / backend parity、性能は未実施。


### Difference Key Controls

Difference Key を VP の scalar effect editing に opt-in し、Threshold / Softness / Choke / Matte Blur を既存 property setter から調整できるようにした。Reference Color は既存 `FloatColorPicker` から編集し、View Mode は 0 Final / 1 Matte の named choice から選択できる。RGB 距離の Threshold は 0–1.732、Softness と Choke は normalized 値を 0–100%、Matte Blur は 0–2x とし、metadata に表示名・既定値・hard / soft range・step・unit・tooltip・animation 可否を設定した。キー処理式、CPU/GPU implementation、resident path は変更していない。Viewport 操作、Undo / Redo、参照色の色空間解釈、画像・alpha、CPU/GPU・preview/final・D3D12/Vulkan parity、性能は未実施。

### Chroma Key View Mode

Chroma Key の既存 `viewMode` を VP named choice として公開し、0 Final / 1 Screen Matte / 2 Despill Map / 3 Status を選択できる。各選択は既存 property setter と CPU / GPU / resident parameter mapping を通り、HSL/YCbCr key 式や render routing は変更しない。Viewport 操作、Undo / Redo、matte / status output、alpha、CPU / GPU / resident parity、性能の runtime 受入は未実施。


### IBK Keyer Controls

IBK Keyer を VP の scalar effect editing に opt-in し、Screen Correction / Core Matte Clip / Edge Matte Softness / Despill Strength / Matte Gamma / Detail Recovery を有限範囲付きで編集できるようにした。Screen Correction と Matte Gamma は 0–4x、Core Matte Clip / Edge Matte Softness / Despill Strength / Detail Recovery は normalized 0–1 を 0–100% 表示する。Erode / Dilate Radius と Clean Plate path は既存 Inspector に残す。Matte View は 0 Final / 1 Matte / 2 Premultiplied RGB の named choice として VP から選べるようにし、既存 CPU/GPU preview setter と処理順を維持する。setter は既存 CPU/GPU implementation の値を更新し、keying 式や rendering path は変更していない。paired real clean plate がないため画質受入れは未実施であり、Viewport 操作、Undo / Redo、GPU parity、CPU fallback・性能も未確認。

### Color Wheels Controls

Color Wheels を VP scalar effect editing に opt-in し、Lift / Gamma / Gain / Offset の Master と R / G / B 数値を既存 setter から操作できるようにした。Lift / Offset の加算値は -2–2 `linear`、Gamma は 0.1–5x、Gain は 0–4x とし、全 Float に表示名、hard / soft range、step、unit、tooltip、animation 可否を付けた。Lift Master の専用 descriptor で opt-in し、残る有限範囲 Float は既存 metadata 列挙から追加する。Wheel Model は 0 RGB / 1 Lift-Gamma-Gain / 2 Offset-Gamma-Gain / 3 Shadows-Midtones-Highlights の named choice として既存 setter から切り替えられる。Point2D の wheel puck は引き続き Inspector で編集する。CPU / GPU implementation、色処理式、resident path は変更していない。Viewport 操作、Undo / Redo、各 wheel mode の画素応答、alpha、CPU/GPU・preview/final・backend parity、性能は未実施。

### Curves Controls

Curves の S-Curve Strength を 0–100% の Float control、Posterize Levels を 2–256 の整数 levels control として VP に公開した。両方を専用 descriptor で opt-in し、Preset は 0 Identity / 1 S-Curve / 2 Fade In / 3 Fade Out / 4 Invert / 5 Posterize / 6 Custom の named choice で切り替える。serialized Custom Curve の任意点編集は既存 Inspector に残す。Strength / Posterize Levels は対応する preset が選択されている場合に画像応答する。Preset 変更時には既存の bounded 256-entry lookup-table snapshot が更新される。画像式、CPU/GPU、resident path は変更していない。Viewport 操作、Undo / Redo、tonal response、alpha、runtime path parity、性能は未実施。

### Channel Mixer Controls

Channel Mixer の Strength を 0–100%、出力 R / G / B ごとの入力 R / G / B 寄与を -2–2 `linear` の数値として VP に公開し、Monochrome / Preserve Luma は既存 toggle descriptor から編集できるようにした。Preset は 0 Custom / 1 Identity / 2 Warm / 3 Cool / 4 Cross Process / 5 Monochrome の named choice からVPで選べる。Preset 選択は行列係数、Strength、Monochrome、Preserve Luma を置き換えるため、既存の `EffectPresetSnapshotCommand` で全 property と hidden `customPresetPinned` state を一件の Undo に記録・復元する。各 setter で既存 CPU / GPU implementation の設定を同期し、property 更新後は現在の全設定値から Named Preset / Custom を再判定する。行列の係数は UI hard range として -2–2 を採用し、既存 Core の係数処理範囲や画像式は変更しない。画像評価計画は Cinematic Looks Lab の識別済み fixture と alpha chart を使う。Viewport 操作、Undo / Redo、画素応答、色・alpha、CPU / pointwise resident GPU / direct GPU parity、Preview / final・backend 差、性能は未実施。

### Levels Controls

Levels の Master と RGB channel の Input Black / Input White / Input Gamma / Output Black / Output White を既存 finite hard range・step metadata から VP に列挙し、Per Channel toggle を追加する。Preset は0 Custom / 1 Normal / 2 High Contrast / 3 Low Contrast / 4 Brighten / 5 Darken の named choice からVPで切り替える。プリセット選択は全 master/channel settings と Per Channel を置き換えるため、既存 `EffectPresetSnapshotCommand` で全 property と hidden `customPresetPinned` state を一件のUndoに記録・復元する。`levels.master` の複合ハンドル編集はInspectorに残す。変更は既存 CPU / GPU implementation の設定を同期し、処理式や GPU path は変更しない。全設定一致から Named Preset を再判定し、明示選択した Custom は preset pin で保持する。Per Channel=true では RGB channel settings が画像応答し、master settings は Core の既存契約により適用されない。Viewport 操作、Undo / Redo、tone response、alpha、CPU / resident GPU / direct path parity、backend 差、性能は未実施。

### Chromatic Aberration Controls

Red Shift / Blue Shift を 0–50 px、Center X / Y を normalized 0–1（0–100%表示）で VP に公開した。Red Shift descriptor で opt-in し、残る3つの Float は表示 label / unit / hard・soft range / step / tooltip / animation metadata から追加する。各 setter で既存 GPU implementation と CPU implementation の値を更新し、drag ごとに CPU implementation を再生成していた処理を初回生成後の値更新へ変更した。Radial sampling、channel math、resident spatial node、direct GPU upload / readback は変更しない。評価計画では色エッジ位置、clamp・alpha境界、CPU/GPU・preview/final/backend差、allocationと1080p性能を別々に測る。Viewport 操作、Undo / Redo、画像品質、alpha、runtime parity、性能は未実施。

### Add Noise Controls

Amount を 0–100%、Size を 0.1–64 px、Seed を 0–9999 integer とし、Color Noise / Monochrome を含む既存設定を VP から操作できるようにした。Amount / Size / Seed の専用descriptorと既存値に合わせた property metadata を設定し、bool toggle は既存 effect service / Undo 経路を使う。Monochrome が Color Noise より優先される処理順を維持し、seed・amount・size の CPU / GPU 同期、resident spatial node、noise式は変更していない。既存の CPU / HLSL parity pending を受入れ済みとみなさず、seed固定時の再現性、noise分布・空間周期、alpha保持、CPU / direct GPU / resident path と backend差、1080p性能を評価計画に記録した。Viewport 操作、Undo / Redo、画像・alpha、runtime parity、性能は未実施。

### Fill Controls

Fill Preset（0 White / 1 Black / 2 Red / 3 Blue / 4 Green / 5 Custom）を VP named choice にする。Custom は現 settings を保持して pin し、named preset は全 settings を適用する。一選択一件の全状態 snapshot Undo / Redo に Custom pin を含める。Opacity を 0–100% の scalar、Color を既存 `FloatColorPicker`、Preserve Alpha を viewport toggle から編集できるようにした。既存 setter と CPU / GPU implementation、resident generic shader の画像処理は維持する。Color / Opacity / Preserve Alpha の編集後に全設定を named preset と照合し、一項目の Undo で元の preset 表示へ戻るようにした。画像評価計画には synthetic alpha chart と CC0 portrait / forest 素材、RGB / alpha の期待式、ROI、path・backend・性能確認条件を記録した。Viewport 操作、Undo / Redo、画像・alpha、runtime path parity、性能は未実施。

### Shadow / Highlight Controls

Shadow Amount を専用 scalar descriptor で VP に公開し、Amount / Tonal Width / Radius、Color Correction / Midtone Contrast、Black / White Clip の既存 Float property も finite hard range から追加する。全10項目に表示名、hard / soft range、step、unit、tooltip を揃え、Clip は 0–0.5 fraction と明記した。既存 setter と `syncImpls()` で CPU implementation の値を反映し、画像処理式と effect order は変更していない。この effect は CPU 実装のみのため GPU / backend parity は未適用。画像評価計画に tone / alpha ROI、再現条件、CPU 性能評価を記録した。Viewport 操作、Undo / Redo、画素応答、視覚評価、性能は未実施。

### Gradient Ramp Controls

Preset（0 Sunrise / 1 Ocean / 2 Neon / 3 Mono / 4 Custom）を VP named choice に公開し、ja / en / zh / zh-TW labels を追加する。Custom は現 settings を保持して pin し、named preset は全 settings を適用する。一選択一件の全状態 snapshot Undo / Redo に Custom pin を含める。Opacity は scalar、Start / End Color は既存 `FloatColorPicker`、Start / End X/Y は 0–100% の数値、Preserve Alpha は viewport toggle から編集でき、各数値に既定値、hard / soft range、step、unit、tooltip を付ける。setter 後に全設定を Sunrise / Ocean / Neon / Mono と照合して Preset 表示を復元する。既存 CPU / direct GPU implementation と blend 式は維持し、resident spatial path は追加しない。画像評価計画に choice mapping、snapshot Undo、color / direction / opacity / alpha 条件と path 比較を記録した。Viewport 操作、Undo / Redo、画像・alpha、CPU / GPU parity、performance は未実施。

### Colorama Controls

Colorama の Phase を専用 scalar descriptor、Spread / Strength / Saturation Boost / Contrast を finite Float metadata の列挙から Viewport に公開し、Preserve Luma を既存 toggle から操作できるようにした。Phase は 0–1 cycle、Spread の UI 範囲は 0–16 cycles、Strength は 0–100%、Saturation Boost / Contrast は 0–2.5x。Spread setter は既存どおり非負値を受け付けて上限を設けず、有限の UI hard range を metadata に設定した。Source Mode（Luma / Hue）、Palette（Rainbow / Fire / Ocean / Neon / Sunset）、Preset（Custom と5つの named preset）を VP の named choice menu から選択できるようにした。離散値は数値ドラッグ対象にしない。Preset 選択は全設定と明示 Custom pin を含む一回の snapshot Undo / Redo とし、個々の named choice も一選択一 Undo にする。named preset の全設定一致を setter ごとに再判定し、Undo で元 preset 表示を復元する。明示選択した Custom は pin して値変更後も維持する。Colorama の画像式・CPU / direct GPU / resident implementation は変更していない。CPU reference、direct GPU、resident shader に既存の luma / palette / HSL の式差があるため、画像評価計画で個別比較する。Viewport 操作、Preset と Undo / Redo、画素応答、alpha、path / backend parity、性能の runtime 評価は未実施。

### Selective Color Controls

Selective Color を Composition Viewport の scalar controls に加え、Strength、9 色域（Reds / Yellows / Greens / Cyans / Blues / Magentas / Whites / Neutrals / Blacks）× Cyan / Magenta / Yellow / Black の36 adjustment、Preset を編集できるようにした。Strength は0–100%、adjustment は−100..100% 表示・step 0.01 とし、既存 setter の0..1 / −1..1 clamp に一致させた。Relative Mode と Preserve Luma は既存 boolean setter を使う。Preset は0 Custom / 1 Neutral / 2 Warm / 3 Cool / 4 Vivid / 5 Film の名前付き選択肢とし、ja / en / zh / zh-TW の labels を追加した。Selective Color 固有property変更を一回の全状態 Undo snapshot にまとめ、snapshot には明示的 Custom の hidden pin 状態を含める。復元後は全設定値から named preset を再判定し、調整の Undo / Redo で値と表示名を一致させ、明示選択した Custom は維持する。既存 CPU / direct GPU 処理式、backend routing、画像処理は変更していない。Viewport 操作、Undo / Redo、画素応答、alpha、CPU / direct GPU / resident path parity、性能、目視レビューは未実施。
### Linear Wipe Controls

Linear Wipe の Angle を専用 scalar descriptor、Softness / Feather を finite Float metadata から VP に公開した。Angle は 0–359°（1° step）とし、setter が 360°を0°へ wrap する同値端点を UI 範囲から除外した。Softness / Feather は 0–100% 表示。新規 metadata には既定値、hard / soft range、step、unit、tooltip を設定し、既存 setter が CPU / direct GPU / resident 用の設定値を同期する。静的確認で見つけた CPU implementation の Softness 初期値10.0と effect / GPU の0.1の不一致を修正し、新規 CPU instance でも0.1を使うよう揃えた。処理式・GPU shader・alpha処理は変更していない。Composition resident shader は別経路として維持し、direct GPU path にある per-call texture / buffer / staging resource 作成と同期 readback は変更していない。画像の角度応答、default修正前後、alpha / premultiplication、path parity、Undo / Redo、performance は runtime 未検証。

### LUT Effect Controls

`LUTEffect` の Intensity を専用 scalar descriptor と有限 Float metadata から VP に公開した。範囲は0–1（0–100%表示、step 0.01）で、既存 `setIntensity()` / `syncImpls()` がCPU implementation の値を更新する。Preset は0 Custom / 1 Cinematic / 2 Vintage / 3 Cold / 4 Warm / 5 High Contrast / 6 Low Contrast / 7 Desaturated / 8 Kodak 2383 / 9 Fuji 3510 の named choice としてVPから選べる。`LUT File` path の編集は既存 Inspector に残す。プリセットsetterはPresetだけを変え、Intensityとpathを保持する。LUTの補間式、CPU-only execution、alpha wrapper は変更していない。`CinematicLooksLab` にあるLUT9種のsource-mirror / alpha fixtureはruntime proofではない。Viewport操作、Undo / Redo、画像応答、alpha/HDR、1080p performance、目視品質は未検証。


### Kuwahara Controls

Kuwahara の Radius を専用 scalar descriptor で Viewport に opt-in し、Sharpness を有限 Float metadata から列挙して編集できるようにした。Radius は既存 setter の 1–50 px range、1 px step とし、descriptor の感度は400 pxの移動で約15 px変化する値にして、1–16 pxを主要な調整域とする。Sharpness は 0–1 を 0–100% 表示、step 0.01 とする。property 更新は既存 setter / syncImpls() を通し、CPU と direct GPU implementation の設定値を更新する。画像式、CPU filter、GPU shader、resident path は変更していない。Anisotropic property は現行 CPU / HLSL の出力計算が参照しないため Viewport には露出しない。Composition resident Kuwahara path は確認できていない。Viewport 操作、Undo / Redo、画像・alpha品質、CPU / GPU・Preview / final parity、1080p performance は未実施。direct GPU path の入力 texture upload / readback と高 Radius の計算負荷は既存条件として評価計画へ記録した。


### Bevel Controls

Bevel の Strength を専用 scalar descriptor で Viewport に opt-in し、Softness を有限 Float metadata から列挙して編集できるようにした。Strength は 0–5x（step 0.01、soft range 0–2x）、Softness は 0–64（step 0.25、soft range 0–16、unitなし）とする。property 更新は既存 setter / syncImpls() を通し、既存 CPU / direct GPU implementation の設定値を同期する。CPU GaussianBlur と GPU 3x3 local-edge shader で Softness の意味が異なる既知差を評価計画に明記し、path parity は未受入れとする。Edge Mode は現在 CPU / HLSL の出力計算で参照されないため VP に公開しない。highlight / shadow color property は既存 property API にないため対象外。画像式、shader、resident path は変更していない。Viewport 操作、Undo / Redo、画像・alpha品質、CPU / GPU・Preview / final・backend parity、1080p performance は未実施。direct GPU の per-call input upload / readback と同期を評価対象として記録した。


### Kaleidoscope Controls

Kaleidoscope の Segments を integer scalar descriptor で Viewport に opt-in し、Center X / Y、Rotation、Zoom、Feather を finite Float metadata から列挙、Mirror を専用 toggle descriptor から編集できるようにした。Segments は 2–64 integer、Center は 0–100%、Rotation は 0–359.9°、Zoom は 0.01–16x、Feather は 0–100% とし、step / default / soft range / tooltip / animation metadata を設定する。Zoom の16x hard maximumは現在の setter 上限より狭い UI range として記録し、setter自体は変更していない。既存 setter / syncImpls() と CPU / direct GPU / resident node の parameter mapping を利用し、画像式・sampling・shader は変更していない。Viewport 操作、Undo / Redo、画素応答、alpha、CPU / direct GPU / resident・Preview / final・D3D12 / Vulkan parity、1080p performance は未実施。


### Mosaic Controls

Mosaic の Cell Size を専用 scalar descriptor で Viewport に opt-in し、Shape Mode を既存 boolean setter の専用 toggle から操作できるようにした。Cell Size は 1–512 px、step 1、default 8 px、soft range 1–32 px とし、UI hard maximum 512 px を設定した。Shape Mode は四角セル / diamond セルを切り替える。既存 setter / syncImpls() が CPU / direct GPU implementation の値を同期し、画像式・CPU並列セル処理・GPU shader・resident path は変更していない。Viewport操作、Undo / Redo、セル境界・部分セル・alpha品質、CPU / direct GPU・Preview / final・D3D12 / Vulkan parity、1080p performance は未実施。direct GPU path の per-call input upload、staging readback、Flush / WaitForIdle は評価計画に記録した。


### Dithering Controls

Dithering の Algorithm、Color Count、Amount を Composition Viewport に公開した。Color Count は 2–256 colors（整数 step 1）、Amount は 0–100%（step 0.01）で、既存 setter / syncImpls() を通じて CPU / direct GPU implementation に反映する。Algorithm の8種類（Bayer2x2 / Bayer4x4 / Bayer8x8 / Bayer16x16 / Floyd–Steinberg / Atkinson / Sierra / Stucki）は名前付き VP choice menu と Inspector の両方から設定でき、既存 numeric mapping を保つ。choice label は ja / en / zh / zh-TW で翻訳する。Pattern Scale は既存 setter に有限上限がないため Viewport には公開しない。direct GPU HLSL の Bayer2x2 順位表と Bayer2x2 / Bayer4x4 座標式を CPU reference と揃え、Pattern Scale を GPU 側でも反映する。Bayer8x8 / Bayer16x16 と error diffusion は直接 shader の対象外として CPU reference に fallback する。CPU quantization、error diffusion kernel、resident shader は変更していない。画像評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に追加済み。Viewport 操作、Undo / Redo、画素応答、alpha、CPU / direct GPU / resident・Preview / final・D3D12 / Vulkan parity、error diffusion の処理時間は未実施であり、ビルドと runtime 検証も未実施。


### Anisotropic Flow Blur Controls

Anisotropic Flow Blur の Blur Amount、Tensor Noise Scale、Tensor Integration Scale、Edge Adherence を Viewport scalar control に opt-in した。範囲は順に 0–100 px（step 0.1）、0.1–20（step 0.01）、0.1–20（step 0.01）、0–100%（step 0.01）。既存 setter / syncImpl() が既存 CPU implementation の設定値を同期する。`Blur Amount <= 0.5` は現行 filter の no-op 区間であり、表示範囲は変えず評価計画に境界を記録した。Filter 式、structure tensor、9-sample bilinear accumulation、alpha、allocation 経路は変更していない。GPU 実装はなく CPU path のみ。画像評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に記録済み。Viewport 操作、Undo / Redo、画素応答、alpha、CPU preview / final parity、1080p performance は未実施。現行 processor の per-frame image / matrix / tensor scratch allocations と CPU 負荷を測り、Viewport 対応による性能改善を主張しない。

### Bricks Controls

Bricks の Brick Width、Brick Height、Mortar Width、Row Offset を Composition Viewport scalar control に opt-in した。範囲は 8–500 px、4–500 px、0–50 px、0–100% で、既存 property setter と finite bounds を維持する。専用 descriptor に stable ID、表示名、step、感度、unit を設定し、既存 Inspector と Viewport が同じ property を編集する。CPU pattern 式、Composition resident HLSL、resolution-scaled cell / mortar parameters、opaque grayscale / alpha=1 の出力は変更していない。`syncImpls()` は setter ごとに CPU implementation を作り直すため drag allocation と CPU / resident path の parity、interactive preview / final の寸法差を評価計画へ記録した。画像評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に追加済み。Viewport 操作、Undo / Redo、画像応答、alpha、path / backend parity、performance、目視品質は未実施。

### Voronoi Controls

Voronoi の Cell Scale、Jitter、Seed を Composition Viewport scalar control に opt-in した。Scale は1–200 px、Jitter は0–2 cells、Seed は0–9999 integer とし、mode の4種類（Nearest Distance / Edge Gap / Second Distance / Cell Value）は名前付き VP choice menu に追加し、ja / en / zh / zh-TW のラベルを TranslationManager で表示する。Scale / Jitter / Seed は既存 property setter を経由し、CPU implementation と Composition resident node の設定を更新する。hash、3×3 site search、出力式、opaque grayscale / alpha=1 の契約、scale だけを preview 解像度比で縮小する挙動は変更していない。各 setter は CPU implementation を再生成するため、連続 drag 時 allocation を評価計画に記録した。画像評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に追加済み。Viewport 操作、Undo / Redo、source independence、画素応答、alpha、CPU / resident・Preview / final・D3D12 / Vulkan parity、性能、目視品質は未実施。

### Hex Grid Controls

Hex Grid の Cell Size（4–200 px）、Line Width（0.5–20 px）、Angle（0–359.9°）を Composition Viewport scalar control に opt-in した。既存 setter / property path を使用し、360°の重複回転端点を VP range から除外した。CPU / direct GPU / Composition resident shader の式と `cellSize` / `lineWidth` の resolution scaling は変更していない。入力画素を使わないため opaque binary grayscale / alpha=1 となる既存出力契約を維持する。Effect Map で CPU / GPU edge-shape parity は pending。Direct GPU は per-call buffer / executor / PSO / staging texture 生成と Flush / WaitForIdle / readback を持つ既存経路であり、変更せず評価計画に resource churn と待機時間を記録した。CPU setter は drag ごとに implementation を再生成するため allocation frequency も評価対象。画像評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に追加済み。Viewport 操作、Undo / Redo、画素応答、source independence、alpha、CPU / direct GPU / resident・Preview / final・D3D12 / Vulkan parity、性能、目視品質は未実施。

### Rim Light Controls

Rim Light / Edge Light の Angle（0–359.9°）、Width（1–64 px）、Softness（0–100%）、Intensity（0–5x）、effect-local Mix（0–100%）を VP scalar editing に公開し、Color は既存 FloatColorPicker から編集できるようにした。共通 Effect Mix と local Mix は別 property のまま維持する。CPU-only effect の既存 setter / property service / Undo と色 picker 経路を利用し、directional alpha-boundary sample、falloff、RGB additive 式は変更していない。source alpha は保持し、出力 RGB は0–1へ clamp しない既存契約を維持する。setter ごとの CPU implementation 再生成と per-pixel maximum 8 alpha samples、straight / premultiplied alpha、HDR over-range 応答を画像評価計画に記録した。画像評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に追加済み。Viewport 操作、Undo / Redo、color picker cancel、画素応答、alpha / HDR、CPU preview / final parity、performance、目視品質は未実施。

### Edge Detection Controls

Edge Detection の Mode（0 = Sobel / 1 = Motion）、Intensity（0–10x）、Threshold（0–100%）、Invert（0 = off / 1 = on）を既存 scalar Viewport 経路に opt-in する。Mode / Invert はそれぞれ Sobel / Motion、Off / On の名前付き choice action にし、選択中の行だけチェックし、ラベルは ja / en / zh / zh-TW で翻訳する。既存 setter、property path、Effect Mix、effect service / Undo を再利用し、Sobel / frame-difference 式、alpha、serialization、CPU routing は変更しない。CPU-only effect で GPU parity は N/A。各 setter の CPU implementation 再生成と frame ごとの `DeepCopy()`、外周画素および前 frame 不在時の既存 fallback を画像評価計画に記録した。連続 frame fixture が未指定のため Mode 1 の temporal image acceptance は pending。画像評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に追加済み。Viewport 操作、Undo / Redo、画素応答、alpha / border、Preview / final parity、性能、目視品質は未実施。

### Radial Blur Controls

Radial Blur の Amount（0–2）、Quality（2–32 samples）、Center X / Y（0–100%）、Mode（0 = Zoom / 1 = Spin）を Composition Viewport scalar editing に公開した。Mode は Zoom / Spin の名前付き choice action で選び、ja / en / zh / zh-TW のラベルを TranslationManager で表示する。Amount / Center X / Y は既存 range の scalar descriptor、Quality は Integer descriptor で範囲を固定する。既存 setter / property path / Effect Mix / Undo を再利用し、sample 式、nearest-pixel lookup、border clamp、RGBA averaging、CPU-only routing は変更していない。setter ごとの implementation 再生成と frame ごとの `DeepCopy()`、Quality 上限時の処理量を評価計画に記録した。Viewport 操作、Undo / Redo、画素応答、alpha、CPU preview / final 差、性能、目視品質はいずれも未実施。

### Turbulent Displace Controls

Turbulent Displace の Amount（0–1000 px）、Size（1–1024 px）、Octaves（1–12）、Seed（0–9999）、Domain Warp（0–100）を Composition Viewport scalar editing に公開した。Float / Integer property をそれぞれ明示 descriptor 化し、既存 setter / property path / Effect Mix / Undo を再利用する。CPU の row `mt19937` jitter と resident HLSL の deterministic hash jitter、式、RGBA sampling、CPU fallback は変更していない。Amount / Size の resident preview resolution scaling と CPU mapX / mapY の frame allocation を評価計画に記録し、CPU / resident parity と drag 性能は未確認。Viewport 操作、Undo / Redo、画像応答、alpha、preview / final・D3D12 / Vulkan parity、性能、目視品質は未実施。

### Twist Transform Controls

Twist (Geo Transform) の Angle（−720–720°）、Center X / Y（0–100%）を Composition Viewport scalar editing に公開した。3つすべてを descriptor 化し、既存 `angle` / `centerX` / `centerY` setter、property path、Effect Mix / Undo を再利用する。twist field、edge sampling、RGBA bilinear interpolation は変更していない。`TwistTransformGPUImpl::applyGPU()` は CPU implementation を呼ぶため実 GPU path とは扱わない。CPU の source / destination `cv::Mat` と destination image allocation、Preview / final parity を評価計画へ記録した。Viewport 操作、Undo / Redo、画素応答、alpha、性能、目視品質はいずれも未実施。

### Shared Allow Overscan Viewport Control

Enabled effect の共通 `Allow Overscan` toggle を Composition Viewport 詳細メニューに追加し、既存の effect property service と Undo 経路を使う。effect が disabled の間は再有効化 toggle のみ表示する。rasterizer の positive ROI expansion に反映する既存 property を編集し、render pipeline や effect の画像式は変更しない。境界画素、出力 bounds、alpha、CPU / GPU、Preview / final、D3D12 / Vulkan、Undo / Redo、性能の runtime 評価は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に従って行い、未実施の間は受入完了としない。

### Composition Effects in the Viewport

Composition Viewport の詳細コンテキストメニューは、カーソル下にレイヤーがある場合はその layer effect stack を、空白領域では current composition effect stack を対象にする。Composition effect も既存の scalar descriptor、数値 metadata、named choice、toggle、FloatColorPicker を使って編集でき、commit は `ArtifactEffectService::setCompositionEffectProperty()` に渡す。ドラッグ preview は effect instance を直接更新し、既存の render-dirty 経路を使う。新しい画像処理式、signal / slot、event wiring は追加しない。

runtime ではレイヤー上／空白領域の対象選択、同じ effect ID が両 stack にある場合の誤更新防止、数値・choice・toggle・color の Undo / Redo、preset-coupled property の snapshot 復元、stale menu、Composition Viewport と Inspector の値一致を確認する。Composition-level 出力の pixel / alpha、Preview / final、CPU / GPU / resident path、D3D12 / Vulkan、性能は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` の条件で比較する。これらは未実施であり、UI 経路の追加だけで受入完了としない。

### Directional Glow Controls

Directional Glow の Preset（0 Custom / 1 Anamorphic 2.39 / 2 Star 6 / 3 Neon / 4 Dreamy）と Pattern（0 Horizontal / 1 Cross / 2 Star / 3 Custom / 4 Star 6）を名前付き Viewport choice にし、ja / en / zh / zh-TW のラベルを TranslationManager で表示する。Threshold（0–1）、Intensity（0–4x）、Angle Offset（−180–180°）、Length 1 / 2 / 3（1–256 px）、Weight 1 / 2 / 3（0–1）は有限 metadata から数値編集し、Half Sampling は既存 toggle 経路を使う。Threshold / Weight / Length 3 はsetter clampに合わせ、Intensity、Angle Offset、Length 1 / 2 は既存setterの広い有限値受入範囲を維持し、Viewport metadataに有限操作範囲を設定する。Preset / look property は全property snapshotで一括Undo / Redoし、Viewport dragが元値に戻った時も開始時のPreset表示を維持する。Custom Anglesは既存Inspector経路を残す。既存CPU、通常GPU、resident HLSLの画像式・alpha挙動とroutingは変更していない。探索captureではCPUと通常 / resident GPUのalpha契約差の修正後captureがなく、backend parityは未確認。viewport操作、snapshot undo、画素・alpha応答、Preview / final、D3D12 / Vulkan、性能は未実施。画像評価計画は `experiments/ViewportEffectLab/EVALUATION_PLAN.md` に追加済み。
