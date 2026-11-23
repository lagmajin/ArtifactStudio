# Glow Lab

**最終更新:** 2026-11-10

独立したPython/OpenCV R&D環境。Glow、静止画Light Leak、画像を光が満たしていくFlashを、生成fixture・CC0実写・数値計測で比較する。調査結果と限界は [RESEARCH_NOTES.md](RESEARCH_NOTES.md) に記録する。Still Light LeakはArtifactCore APIとArtifact effectへ接続済みで、Streak source位置と角度を調整できる。MSVC `ArtifactCore` / `ArtifactEffectsGlow`、resident HLSLのDXIL/Vulkan SPIR-V compile、MSVC 14.52によるフル `ArtifactUiTest` buildが成功した。GPU fixtureはcold-start後に `gpuBlendReady=1`、dispatch成功を記録し、Accum RTから出力を切り出してCPU参照と比較できる。初回比較でビューポートUVと画像ソースUVの不一致を見つけ、2D画像レイヤーにソース座標変換を追加した。合成plate比較ではGPU/CPUのencoded RGB MAEがEdge Wash .009416、Streak .011437、Organic .006495。さらにCC0雪景色フレーム（360×640）をアプリfixture入力にできるようにし、3 lookのCPU/GPU画像も取得した。直接出力比較MAEは約.017–.020だが、効果前のLayer RT差を差し引く近似比較は約.0067–.0076。雪景色と低照度ポートレートで独立strength sweep（.20/.35/.55）を撮り、広い色かぶりを抑えた比較候補として.20–.35を記録した。測定と比較画像は `captures/light-leak-realplate-cpu-gpu.png`、`captures/light-leak-winter-strength-rd1.png`、`captures/light-leak-portrait-strength-rd1.png`、`captures/light-leak-cpu-gpu-parity-v2.png`。GPU画像は診断Accum RTから切り出した中間結果で、最終表示・readback経路と3D透視レイヤーは未検証。

## 実行

各スクリプトの詳細は `--help` を参照。生成物は既定で `captures/` に保存される。

```powershell
python experiments/GlowLab/flash_fill_lab.py
python experiments/GlowLab/flash_scatter_front_lab.py --output experiments/GlowLab/captures/scatter-front-rd2 --strength 0.8 --scatter 3.0 --veil-gain 0.00625 --highlight-threshold 0.06 --highlight-range 0.22 --samples 0.18 0.32 0.48 0.64 0.80 0.94
python experiments/GlowLab/flash_fill_decomposition_lab.py --output experiments/GlowLab/captures/fill-decomposition-rd1
python experiments/GlowLab/flash_fill_decomposition_lab.py --output experiments/GlowLab/captures/final-fill-strength48-rd1 --strength 4.8 --scatter 3.0 --scatter-front-coupling 1.0 --base-veil-gain 0.003125 --veil-gains 0 0.0015625 0.003125 0.00625 0.0125 0.025 --highlight-threshold 0.06 --highlight-range 0.22 --highlight-rolloff 0.78 --samples 0.72 --veil-sweep-progress 0.72
python experiments/GlowLab/flash_fill_lab.py --input experiments/GlowLab/assets/flash-photo-plate.png --output experiments/GlowLab/captures/final-fill-strength48-hold-rd1 --frames 9 --duration-seconds 1 --fps 8 --mode radial --source-position 0.08 0.5 --strength 4.8 --scatter 3.0 --scatter-front-coupling 1.0 --veil-gain 0.0015625 --highlight-threshold 0.06 --highlight-range 0.22 --highlight-rolloff 0.78 --global-fill 0.015 --radial-falloff 2.7 --front-duration 0.72 --attack-duration 0.78 --peak-time 1.0 --decay-rate 2.4
python -m pip install -r experiments/GlowLab/requirements-depth.txt
python experiments/GlowLab/flash_depth_estimate.py --input experiments/GlowLab/assets/flash-photo-plate.png --output experiments/GlowLab/captures/depth-anything-portrait-rd1 --download-model
python experiments/GlowLab/flash_video_depth_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/depth-anything-walkway-video-rd1
python experiments/GlowLab/flash_video_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/video
python experiments/GlowLab/flash_video_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/veil-video-gain-0p0125 --mode radial --source-position 0.50 0.14 --strength 0.8 --scatter 0.6 --veil-gain 0.0125 --global-fill 0.03 --front-duration 0.72 --decay-rate-per-second 3.0 --highlight-rolloff 1.0 --radial-falloff 2.7 --highlight-percentiles 70 95 --highlight-smoothing-seconds 0.4 --synthetic-exposure-stops -2 --motion-compensated-metrics --max-dimension 240 --no-png-frames
python experiments/GlowLab/flash_video_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/video-dark-strength24 --mode radial --source-position 0.5 0.14 --strength 2.4 --synthetic-exposure-stops -2 --motion-compensated-metrics --no-png-frames
python experiments/GlowLab/flash_response_knob_sweep_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 24 --output experiments/GlowLab/captures/response-knob-sweep
python experiments/GlowLab/flash_video_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/lowkey-percentile-ema --mode radial --source-position 0.5 0.14 --strength 0.8 --synthetic-exposure-stops -2 --highlight-percentiles 70 95 --highlight-smoothing-seconds 0.4 --motion-compensated-metrics --no-png-frames
python experiments/GlowLab/flash_cut_fixture_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/highlight-cut-rd1/known-exposure-cut.avi --cut-frame 16 --total-frames 32 --post-cut-stops -2
python experiments/GlowLab/flash_video_lab.py --input experiments/GlowLab/captures/highlight-cut-rd1/known-exposure-cut.avi --output experiments/GlowLab/captures/highlight-cut-reset --mode radial --source-position 0.5 0.14 --strength 0.8 --highlight-percentiles 70 95 --highlight-smoothing-seconds 0.4 --highlight-reset-frames 16 --motion-compensated-metrics --no-png-frames
python experiments/GlowLab/flash_video_lab.py --input experiments/GlowLab/captures/highlight-cut-rd1/known-exposure-cut.avi --output experiments/GlowLab/captures/highlight-cut-auto --mode radial --source-position 0.5 0.14 --strength 0.8 --highlight-percentiles 70 95 --highlight-smoothing-seconds 0.4 --detect-highlight-cuts --highlight-cut-threshold 0.35 --motion-compensated-metrics --no-png-frames
python experiments/GlowLab/flash_cut_fixture_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/highlight-cut-rd2/composition-cut.avi --cut-frame 13 --total-frames 32 --post-cut-source-frame 55 --post-cut-stops 0
python experiments/GlowLab/flash_video_lab.py --input experiments/GlowLab/captures/highlight-cut-rd2/composition-cut.avi --output experiments/GlowLab/captures/highlight-cut-rd2/composition-auto --mode radial --source-position 0.5 0.14 --strength 0.8 --highlight-percentiles 70 95 --highlight-smoothing-seconds 0.4 --detect-highlight-cuts --highlight-cut-threshold 0.35 --highlight-cut-spatial-threshold 0.12 --motion-compensated-metrics --no-png-frames
python experiments/GlowLab/flash_exposure_temporal_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/exposure-temporal
python experiments/GlowLab/flash_exposure_cut_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/exposure-cut
python experiments/GlowLab/flash_surface_front_lab.py --video tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 24 --output experiments/GlowLab/captures/surface-front
python experiments/GlowLab/flash_shadow_animation_lab.py --output experiments/GlowLab/captures/shadow-animation
python experiments/GlowLab/flash_spatial_path_sweep_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/spatial-path-sweep
python experiments/GlowLab/flash_spatial_path_sweep_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/spatial-path-sweep-dark --synthetic-exposure-stops -2
python experiments/GlowLab/flash_exposure_video_adaptation_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/exposure-video-adaptation
python experiments/GlowLab/flash_exposure_video_adaptation_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/exposure-video-adaptation-dark --synthetic-exposure-stops -2
python experiments/GlowLab/flash_preview_strength_sweep.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --output experiments/GlowLab/captures/preview-strength-sweep
python experiments/GlowLab/glow_lab.py
python experiments/GlowLab/glow_pyramid_lab.py
python experiments/GlowLab/glow_alpha_lab.py --output experiments/GlowLab/captures/glow-alpha-core-reference
python experiments/GlowLab/glow_look_sweep_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 20 --output experiments/GlowLab/captures/glow-look-sweep-cc0-frame20
python experiments/GlowLab/light_leak_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 20 --strength 0.55 --output experiments/GlowLab/captures/light-leak-cc0-frame20-soft
python experiments/GlowLab/light_leak_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 20 --strength 0.55 --output experiments/GlowLab/captures/light-leak-still-effect-reference
python experiments/GlowLab/light_leak_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 20 --strength 0.30 --output experiments/GlowLab/captures/light-leak-cc0-frame20-sweep-030
python experiments/GlowLab/light_leak_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 20 --strength 0.80 --output experiments/GlowLab/captures/light-leak-cc0-frame20-sweep-080
python experiments/GlowLab/light_leak_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 20 --strength 0.55 --output experiments/GlowLab/captures/light-leak-organic-analytic-rd
python experiments/GlowLab/light_leak_lab.py --input experiments/GlowLab/captures/light-leak-neon-source.png --strength 0.30 --output experiments/GlowLab/captures/light-leak-neon-scene-sweep-030
python experiments/GlowLab/light_leak_lab.py --input experiments/GlowLab/captures/light-leak-neon-source.png --strength 0.55 --output experiments/GlowLab/captures/light-leak-neon-scene-rd
python experiments/GlowLab/light_leak_lab.py --input experiments/GlowLab/captures/light-leak-neon-source.png --strength 0.80 --output experiments/GlowLab/captures/light-leak-neon-scene-sweep-080
python experiments/GlowLab/light_leak_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 20 --strength 0.55 --streak-position 0.50 0.24 --output experiments/GlowLab/captures/light-leak-streak-origin-snow-rd
python experiments/GlowLab/light_leak_lab.py --input experiments/GlowLab/captures/light-leak-neon-source.png --strength 0.55 --streak-position 0.50 0.25 --output experiments/GlowLab/captures/light-leak-streak-origin-center-rd
python experiments/GlowLab/light_leak_lab.py --input experiments/GlowLab/captures/light-leak-neon-source.png --strength 0.55 --streak-position 0.88 0.45 --output experiments/GlowLab/captures/light-leak-streak-origin-right-rd
python experiments/GlowLab/light_leak_lab.py --input tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 --frame-index 20 --strength 0.55 --streak-position 0.50 0.24 --streak-angle 12 --streak-angle-sweep -12 0 2.578 12 --output experiments/GlowLab/captures/light-leak-angle-snow-rd
python experiments/GlowLab/light_leak_lab.py --input experiments/GlowLab/captures/light-leak-neon-source.png --strength 0.55 --streak-position 0.50 0.25 --streak-angle-sweep -12 0 2.578 12 --output experiments/GlowLab/captures/light-leak-angle-neon-rd
python tools/capture_ui_test.py --exe out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe --output experiments/GlowLab/captures/light-leak-app-ja-rd.png --language ja
$env:ARTIFACT_COMPOSITION_GPU_BLEND_NORMAL='1'
$env:ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS='1'
python tools/capture_ui_test.py --exe out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe --output experiments/GlowLab/captures/light-leak-gpu-edge-wash.png --language ja --light-leak --light-leak-look edge-wash --light-leak-compute gpu
python tools/capture_ui_test.py --exe out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe --output experiments/GlowLab/captures/light-leak-gpu-streak.png --language ja --light-leak --light-leak-look streak --light-leak-compute gpu
python tools/capture_ui_test.py --exe out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe --output experiments/GlowLab/captures/light-leak-gpu-organic.png --language ja --light-leak --light-leak-look organic --light-leak-compute gpu
python tools/capture_ui_test.py --exe out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe --output experiments/GlowLab/captures/light-leak-realplate-gpu.png --language ja --light-leak --light-leak-compute gpu --light-leak-input experiments/GlowLab/captures/light-leak-cc0-frame20-soft/light-leak-fixture.png
python tools/capture_ui_test.py --exe out/build/x64-Debug-Agent/bin/Debug/ArtifactUiTest.exe --output experiments/GlowLab/captures/light-leak-portrait-streak-app-gpu.png --language ja --light-leak --light-leak-look streak --light-leak-compute gpu --light-leak-input experiments/GlowLab/assets/flash-photo-plate.png --light-leak-strength 0.30 --light-leak-source 0.74 0.19 --light-leak-angle -8
python experiments/GlowLab/light_leak_alpha_lab.py --output experiments/GlowLab/captures/light-leak-still-effect-alpha-reference
python experiments/GlowLab/light_leak_alpha_lab.py --look organic --output experiments/GlowLab/captures/light-leak-organic-alpha-rd
```

## 主な実験

- `flash_fill_lab.py`: directional/radialの進行front、露光、散乱、highlight responseを比較する基礎sandbox。Radial modeでは白=近距離のグレースケールdepth mapを使い、front arrival delayと距離による露光減衰を独立調整できる。depth mapがシーンの実距離を表す保証はなく、別素材へ転用する前に方向と尺度を確認する。
- `flash_depth_estimate.py`: 任意の静止画像からDepth Anything V2 Small int8 ONNXでrelative depth mapを推定する。`onnxruntime`はoptional。`--download-model`でモデルを明示取得し、near-bright PNG、raw float map、可視化、推定metadataを保存する。モデル出典/ライセンス/限界は`DEPTH_MODEL_NOTICE.md`を参照。
- `flash_video_depth_lab.py`: 連続動画から全フレームdepthを推定し、フレーム別とclip共通のrelative-depth正規化を比較しながら、flat/depth frontの処理動画・contact sheet・flow-compensated temporal metricsを出力する。
- `flash_scatter_front_lab.py`: 従来のglobal scatterと、前線で照らされたハイライト源をPSF拡散前にgatingする形を比較する。`scatter_front_coupling`は0でglobal、1で全ての散乱源を前線連動。PSFの光漏れは前線外へ広がるが、実sceneの遮蔽は扱わない。
- `flash_fill_decomposition_lab.py`: 進行frontの露出、ハイライト散乱、veilを線形光の独立レイヤーとして可視化・計測し、どの成分が画像を満たしているか切り分ける。strength、veil gain、scatter前線連動、highlight threshold/range、rolloffをsweepできる。
- `flash_video_lab.py`: bounded-memory動画処理、フレーム画像、接触シート、露出・入力/出力/新規クリップ率・動き補償を含む時間差分metricsを出力する。direction vector、frame-percentile highlight threshold、shot内EMA smoothing、既知カットreset (`--highlight-reset-frames`, 選択区間内index)、任意のhistogram/thumbnail spatial cut検出 (`--detect-highlight-cuts`)、veil gain (`--veil-gain`)、source-gated scatter (`--scatter-front-coupling`)と合成露出offsetを指定できるが、後者はセンサーnoiseを模擬しない。
- `flash_response_knob_sweep_lab.py`: 1フレームで距離減衰と固定／相対ハイライト閾値を、標準露出と合成低key露出で比較する。
- `flash_cut_fixture_lab.py`: CC0動画の同じフレームを用いて、既知の線形露出カットを含む再現可能な短いAVIを生成する。センサーノイズは含まない。
- `flash_video_preview.py` / `flash_preset.py`: 対話previewとJSON preset handoff。Radial/Directional mode、光源位置、前線方向、距離減衰、固定／frame-percentile／EMAハイライト閾値、散乱強度、front-gated scatter、veil gainを調整でき、Pキーで調整値を保存できる。EMAは連続再生時に更新し、シーク時は現在フレームから初期化する。既知カット時刻は`--highlight-reset-seconds`、任意の自動resetは`--detect-highlight-cuts`で指定する。
- `flash_cut_detection.py`: 32-bin grayscale histogram距離と160px以下thumbnailのmean absolute spatial differenceをcut scoreとして共有する簡易検出器。既定では無効。明るさ・構図の大きな差を拾うheuristicで、ストロボ等をcutと誤認する可能性がある。
- `flash_spatial_path_sweep_lab.py`: CC0動画5時点でradial光源位置とdirectional前線を同条件で比較し、露光・coverage・clipを記録する。`--synthetic-exposure-stops`で線形露出レンジを試せるが、センサーnoiseは模擬しない。
- `flash_exposure_adaptation_lab.py`: 複数strength候補の露出headroomと新規clip量を比較する。
- `flash_exposure_temporal_lab.py`: 合成露出ランプに対するsource-p95 strength制御とEMAの追従性を比較する。
- `flash_exposure_cut_lab.py`: CC0動画から低キー静止画へ切り替える人工カットで、未平滑・EMA・既知境界resetとreset後の短い追従時間を比較する。自動cut検出は対象外。
- `flash_shadow_projection_lab.py`: 既知形状の2.5Dパネルに対するdiffuse照明と投影影のprobe。
- `flash_shadow_animation_lab.py`: 既知形状上で前線時間、影の変位距離、ぼけを独立sweepし、固定地面ROIの影面積・重心・長さ・境界幅とマット可視化を出力する。
- `flash_specular_lab.py`: Blinn–Phong specular、roughness、point/area source sampleと光源移動sequence。
- `flash_surface_front_lab.py`: 既知形状上のdirect surface lightingとflash frontを比較し、CC0実写には画像空間frontのみを適用する。既知depth frontの感度と素材ごとのclip量も計測する。
- `flash_exposure_video_adaptation_lab.py`: 実映像の連続区間で二分探索によるclip制約strength、shot固定、通常/非対称EMA、固定またはframewise strength＋adaptive/平滑highlight rolloffを比較し、時間変動とclip超過を記録する。`--synthetic-exposure-stops` は低キー/高キー露出レンジの制御比較であり、センサーnoiseを模擬しない。
- `flash_preview_strength_sweep.py`: 対話previewの既定lookを連続動画へ適用し、strength候補ごとのクリップ率・平均光量差・代表フレームを比較する。
- `flash_temporal_lab.py` / `flash_motion_lab.py`: 時間減衰と画面空間の移動subject応答。
- `flash_response_lab.py`: shoulder/rolloff比較。
- `glow_lab.py` / `glow_pyramid_lab.py` / `glow_alpha_lab.py` / `glow_look_sweep_lab.py`: soft-knee Glow、縮小blur pyramid、alpha境界、SDR headroom付きlookを比較する。
- `light_leak_lab.py` / `light_leak_alpha_lab.py`: edge wash、位置・角度調整可能なstreak、決定的analytic organic leak、縮尺・alpha契約を比較する。Streak sourceは正規化座標で `--streak-position X Y`、軸角度は `--streak-angle DEGREES` で指定できる。`--streak-angle-sweep` は角度ごとの比較captureを追加する。

fixture動画の出典とCC0条件は `tests/ArtifactCore/fixtures/README.md` を参照。キャプチャはR&D根拠であり、物理カメラ校正や美的な正解を意味しない。

## 追加検証

ArtifactCore / app effectを有効な実装として扱う前に、透明alpha境界、CPU/GPU parity、実写素材での視覚確認、処理時間とGPU resource再利用を個別に確認する。Qt合成への移行やホットパスの無制限な確保をしない。
