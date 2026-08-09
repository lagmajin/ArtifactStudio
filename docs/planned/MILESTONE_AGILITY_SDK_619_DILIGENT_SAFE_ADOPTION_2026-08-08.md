# Agility SDK 1.619 / Diligent-safe Adoption Milestone

**最終更新:** 2026-08-08

**ステータス:** In Progress

## 目的

ArtifactStudio が既に同梱している DirectX 12 Agility SDK 1.619 を前提に、DiligentEngine の resource state、descriptor、command list、pipeline 管理と競合しない範囲から新機能を段階導入する。

当面の対象は静止画レイヤー、連番画像レイヤー、シェイプレイヤー、画像処理・レイヤー合成の compute path と診断機能とする。動画対応は対象外とする。

## 現状

- `Artifact/src/AppMain.cppm` が `D3D12SDKVersion = 619` と `D3D12SDKPath = ".\\"` を公開している。
- `Artifact/CMakeLists.txt` が retail 1.619 系の `D3D12Core.dll` を実行ファイルの隣へ配置する。
- 通常の GPU resource、descriptor、resource state、command list、PSO は DiligentEngine が所有・追跡する。
- DiligentEngine の DXC 経路は利用可能な最大 Shader Model を検出し、指定値が上限を超える場合は downgrade する。
- Agility SDK の選択と、SM 6.9 対応 DXC・GPU driver・feature capability の成立は別条件である。

## 基本原則

1. DiligentEngine は原則変更しない。D3D12 backend の所有権・state trackingを変えない小さな opt-in は、fork に閉じて個別に導入できる。
2. 描画・compute command は既存の Diligent API を正規経路とする。
3. native D3D12 API は capability 取得、診断、OS/runtime 通知など、Diligent の内部状態を変更しない用途に限定する。
4. 新機能はすべて実行時 capability check と既存経路への fallback を持つ。
5. capability 未対応、DXC 不足、driver 不足をエラー終了の理由にしない。
6. D3D12 専用最適化を共通 renderer interface の必須契約にしない。
7. preview Agility SDK は採用せず、retail 1.619 系に固定する。
8. 性能差が測定できない機能は既定経路へ昇格させない。

## 対象機能と優先度

| 優先度 | 機能 | 方針 |
|---|---|---|
| P0 | Capability inventory | runtime、DXC、Shader Model、`D3D12_OPTIONS22` などを一度だけ取得し診断へ公開 |
| P1 | Increased 1D Dispatch Limit | 大規模画像 compute を対応上限まで単一 dispatch 化し、非対応時は既存の分割 dispatch を維持 |
| P1 | Shader Model 6.9 compute kernels | 画像処理に限定した opt-in variant として導入し、既存 shader を必須 fallback とする |
| P2 | Periodic Trim Notifications | 再生成可能な ArtifactStudio 所有 cache の解放要求に変換 |
| P2 | Tight Alignment | D3D12 Tier 1 対応時だけ、小さな default-heap buffer に明示opt-in |
| P2 | CPU Timeline Query Resolves | DX12 専用の計測・診断 path として隔離 |
| P3 | Revised Resource View Creation | Diligent 管理外の専用 buffer が必要になった場合のみ再評価 |

## 明示的な非対象

- Enhanced Barriers の native command list への直接挿入
- Work Graphs / `DispatchGraph()`
- Diligent 管理 resource に対する独自 residency、descriptor、state transition 操作
- Diligent command list の途中で行う native draw / dispatch
- DXR 1.2、Shader Execution Reordering、Opacity Micromaps
- preview SDK 1.7xx 系および preview driver 必須機能
- Shader Model 6.10 preview、Cooperative Vector 系機能

Enhanced Barriers は Diligent の resource state tracking と二重管理になるため、Diligent 側に正式な抽象化と状態同期契約が追加されるまで保留する。Work Graphs も pipeline、resource binding、barrier、command list の独自管理が必要になるため同様に保留する。

## Phase 0: Runtime / Capability Baseline

### 作業

- 実際にロードされた D3D12 runtime と期待する SDK version の診断情報を追加する。
- native device interface の取得可否を、所有権を奪わない照会としてまとめる。
- 次を capability snapshot として起動時に一度だけ取得する。
  - Shader Model 上限
  - DXC version と SM 6.9 compile 可否
  - `D3D12_FEATURE_D3D12_OPTIONS22`
  - `Max1DDispatchSize`
  - CPU Timeline Query Resolve 対応
  - Periodic Trim Notification 利用可否
- capability の取得失敗は `unsupported` として扱い、既存描画を継続する。
- 同じ feature query を frame ごとに実行しない。

### 完了条件

- Agility SDK が利用できない環境でも既存 Diligent path が起動可能である。
- capability snapshot が App Debugger または既存診断出力から確認できる。
- feature の有無だけで renderer backend や resource ownership が切り替わらない。

## Phase 1: Increased 1D Dispatch Limit

### 作業

- 1D dispatch を使用する画像処理 path を棚卸しし、最初の対象を一つに限定する。
- dispatch group 数を `Max1DDispatchSize` と shader thread-group size から安全に算出する。
- 従来上限または未対応環境では、既存の tiled / split dispatch を使用する。
- 単一 dispatch と分割 dispatch で同一 shader、同一 resource binding、同一出力契約を維持する。
- 画像幅・高さ・pixel count の乗算 overflow とゼロサイズを明示的に処理する。

### 最初の候補

- pointwise color operation
- histogram / scope aggregation の前処理
- layer blend の独立した大規模 pixel pass

### 完了条件

- 既存 Diligent `DispatchCompute()` 経路だけで実行される。
- capability ON/OFF で出力が許容誤差内に一致する。
- 4K、8K、極端な横長画像で dispatch 範囲外アクセスがない。
- GPU timestamp により、対象 workload で有意な改善または同等性を確認できる。

## Phase 2: Shader Model 6.9 Image Compute Variant

### 作業

- SM 6.9 対応 DXC をアプリ配布物として固定できるか確認する。
- 既存の画像処理 shader 一つに SM 6.9 variant を追加する。
- 最初は次のうち、実測価値があるものだけを採用する。
  - native 16-bit arithmetic
  - 16/64-bit wave operations
  - long vector を利用できる局所的な演算
- shader variant selection は capability snapshot に基づき、PSO 作成前に決定する。
- SM 6.9 compile、PSO 作成、実行のいずれかが失敗した場合、同一セッションで失敗 variant を再試行せず既存 shader へ fallback する。

### 制約

- SM 6.9 を全 shader の既定値にしない。
- DXIL reflection と Diligent resource remapping の成立を個別に確認する。
- half precision 化で色域、alpha、HDR、NaN/Inf 処理の契約を変えない。
- cross-backend の共通 HLSL を無条件に SM 6.9 専用構文へ変更しない。

### 完了条件

- SM 6.9 非対応 GPU / driver / DXC で既存 shader が選択される。
- 静止画・連番画像の同一フレーム比較が許容誤差内に収まる。
- HDR、透明境界、負値、NaN/Inf を含む入力で劣化がない。
- GPU時間、VRAM転送量、PSO作成時間の少なくとも一つに測定可能な利点がある。

## Phase 3: Memory Pressure Notification

### 作業

- Periodic Trim Notification を DX12 専用の低レベル通知として受け取る。
- 通知を Diligent resource への直接操作に変換しない。
- 次のうち再生成可能で、ArtifactStudio が所有する cache の縮小要求に限定する。
  - preview frame cache
  - intermediate image cache
  - unused shader / pipeline cache の既存解放入口
  - layer GPU cache の既存 eviction 入口
- UI thread や render thread を通知 callback から直接ブロックしない。
- trim storm を避ける cooldown と解放量上限を定義する。

### 完了条件

- 通知 callback 内で Diligent object を直接破棄しない。
- cache 解放後も必要 resource が通常経路で再生成される。
- 通知非対応環境の挙動が現在と同一である。

## Phase 4: CPU Timeline Query Resolve Diagnostics

### 作業

- CPU Timeline Query Resolve を App Debugger / performance diagnostics 専用機能として設計する。
- Diligent の既存 query / fence / command submission を置き換えない。
- native query heap と結果 buffer を使う場合は、Diligent 管理 resource と混在させず専用 ownership に閉じる。
- feature 無効時は既存 GPU timing または `unavailable` 表示へ fallback する。

### 完了条件

- 計測無効時の command stream と frame output に変化がない。
- 計測有効時も同期 wait による恒常的な frame stall を追加しない。
- query resource の寿命が device、queue、in-flight frame より短くならない。

## 検証マトリクス

| 条件 | 期待結果 |
|---|---|
| Agility 1.619 + SM 6.9対応DXC/GPU | 対応 variant を選択 |
| Agility 1.619 + 古いDXC | SM 6.9を無効化し既存 shader を選択 |
| Agility 1.619 + SM 6.9非対応GPU | 既存 shader を選択 |
| `OPTIONS22` 非対応 | 分割 dispatch を選択 |
| feature query失敗 | unsupported として継続 |
| D3D11 / Vulkan backend | D3D12固有 capability を参照せず既存経路を維持 |
| WARP | capability に従い安全に fallback |
| device recreation | capability と native interface を再取得し、古い参照を破棄 |

## 横断的な受け入れ条件

- DiligentEngine を変更する場合は、resource ownership・state tracking・command list contractを変えない局所的な opt-in に限る。
- native D3D12 command によって Diligent の resource state tracking が不整合にならない。
- capability 無効時の出力と操作性が現状から退行しない。
- 静止画、連番画像、シェイプ、layer blend の代表ケースで fallback parity を確認する。
- device removal、shader compile failure、PSO creation failure がユーザーデータ損失や起動不能につながらない。
- 新しい最適化の有効状態と fallback 理由を診断情報から確認できる。
- runtime性能検証が終わるまで、最適化を既定ONにしない。

## 実装順序

1. Phase 0: capability snapshot と診断
2. Phase 1: Increased 1D Dispatch の単一 workload 実証
3. Phase 2: SM 6.9 の単一 image compute variant 実証
4. Phase 3: memory pressure notification
5. Phase 4: CPU timeline diagnostics
6. 実測結果を基に既定ON候補を個別承認

## Current Status

- `2026-08-08` Phase 0 着手
  - Agility SDK runtimeだけでなく、同じpackageの1.619 headerをWindows SDKより優先して使用するCMake設定を追加。
  - D3D12 device生成時に一度だけ取得する`D3D12AgilityCapabilitySnapshot`を追加。
  - Agility runtimeのロード有無とパス、header/requested SDK version、device/DXCのShader Model、`D3D12_OPTIONS22`、1D dispatch上限、`ID3D12Device15`取得可否を収集。
  - snapshotを既存GPU adapter診断文字列と起動ログへ接続。
  - runtime/build検証、およびApp Debugger上の専用表示は未確認。
- `2026-08-08` Phase 1 着手
  - Invert effectへ`numthreads(256, 1, 1)`のpointwise 1D variantを追加。
  - `OPTIONS22`、device/DXC SM 6.9、65,535超かつdevice上限内という全条件を満たす場合だけ1D variantを選択。
  - 条件未成立時は従来の8x8 2D dispatchを維持し、1D shader/PSO作成失敗時も2D PSOを再構築してfallbackする。
  - 初回のextended dispatch選択をgroup数、pixel数、device上限とともに診断ログへ出力。
  - runtime出力parityとGPU時間は未検証。
- `2026-08-08` Phase 2 foundation
  - 共通`ComputePipelineDesc`へshader compilerとHLSL Shader Modelの明示指定を追加し、未指定時の既存挙動を維持。
  - Invertのextended 1D variantだけをDXC / SM 6.9へ固定し、2D経路は従来のdefault compiler/versionを維持。
  - SM 6.9 shader作成失敗時はcompiler/version指定もdefaultへ戻して2D PSOを再構築。
  - 16-bit演算やlong vectorなど、画質・性能の実測が必要なSM 6.9固有演算はまだ導入していない。
- `2026-08-08` Phase 3 foundation
  - shared D3D12 device生成時に`ID3D12Device15::RegisterTrimNotificationCallback()`を登録し、device解放・invalidate時に解除。
  - callback内ではresourceを破棄せず、要求byte数とgenerationだけをatomic stateへ保存。
  - GPU texture cacheは通常のupload処理境界で新しいgenerationを検出し、要求量を上限にLRU entryを解放。
  - callback threadからDiligent object、Qt UI、cache lockへアクセスしない。
  - 実際のmemory pressure通知、複数cache instanceでの解放量、cooldownはruntime未検証。
- `2026-08-08` App Debugger diagnostics
  - Diagnosticsタブへruntime/header SDK、device/DXC Shader Model、OPTIONS22とdispatch上限、trim登録・要求状態を追加。
  - CPU Timeline QueryとRevised Viewsはcapabilityだけを表示し、Diligent所有command/queryへ未接続であることを明示。
- `2026-08-08` Phase 4 integration decision
  - CPU Timeline Query Resolveの実測にはCPU-resolve query heapだけでなく、Diligentが所有するcommand list上でqueryを記録する必要がある。
  - native commandをDiligent contextの途中へ挿入するとcommand/state tracking契約を破るため、現段階ではcapability診断までに限定。
  - Diligent側にCPU timeline query abstractionまたは安全なnative command interop境界が追加されるまで、計測本体は保留。
- `2026-08-08` Tight Alignment foundation
  - DiligentEngineの`BufferDesc::MiscFlags`へ`MISC_BUFFER_FLAG_TIGHT_ALIGNMENT`を追加。D3D12 backendだけがこのhintを解釈し、D3D12 Agility SDK header 619以上かつ`D3D12_FEATURE_D3D12_TIGHT_ALIGNMENT` Tier 1のdefault-heap committed bufferにだけ`D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT`を付与する。
  - dynamic、upload、readback、sparse buffer、textureは既存のallocation pathを維持する。未対応GPUまたは他backendではhintを無視して従来どおり作成する。
  - `DiligentImmediateSubmitter`の小さなimmutable index / unit-quad vertex bufferを最初のopt-in対象とした。
  - capability snapshotとApp DebuggerへTier・利用可否を追加。runtime上の実アラインメント値・VRAM削減量・描画parityは未検証。

## 中止・保留条件

- Diligent resource state を native API 側から変更する必要が生じた。
- Diligent の private implementation への依存が必要になった。
- cross-backend interface に D3D12 固有型を露出する必要が生じた。
- fallback parity が成立しない。
- 対象 workload で性能または安定性の利点を測定できない。

該当した機能は無理に実装せず、Diligent 側の正式対応待ちまたは別マイルストーンへ分離する。

## 関連文書

- [`docs/analysis/GRAPHICS_GPU_AUDIT_2026-08-02.md`](../analysis/GRAPHICS_GPU_AUDIT_2026-08-02.md)
- [`docs/planned/MILESTONE_GPU_LAYER_BLEND_COMPUTE_2026-03-21.md`](MILESTONE_GPU_LAYER_BLEND_COMPUTE_2026-03-21.md)
- [`docs/planned/MILESTONE_APP_DEBUGGER_RENDER_COST_BREAKDOWN_2026-04-24.md`](MILESTONE_APP_DEBUGGER_RENDER_COST_BREAKDOWN_2026-04-24.md)
- [`Artifact/docs/MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md`](../../Artifact/docs/MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md)
- [Microsoft: Agility SDK 1.619 / Shader Model 6.9](https://devblogs.microsoft.com/directx/shader-model-6-9-retail-and-more/)
- [Microsoft: Enhanced Barriers specification](https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html)

## 未確認事項

- 配布中の `dxcompiler.dll` / `dxil.dll` の実バージョンと SM 6.9 対応状況
- 対象GPU各社の必要driver versionと実機範囲
- 現在の画像compute pathで1D dispatch上限が実際のボトルネックになっている箇所
- Periodic Trim Notificationを既存cache evictionへ接続する最小責務境界
- CPU Timeline Query Resolveを既存App Debuggerへ追加した場合の計測overhead
