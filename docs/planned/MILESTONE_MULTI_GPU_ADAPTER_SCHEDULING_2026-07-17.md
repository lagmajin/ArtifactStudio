# Milestone: Multi-GPU Adapter Selection and Scheduling

**ステータス:** In Progress

**日付:** 2026-07-17

## 目的

iGPU / dGPU / 複数dGPUを認識し、用途に適したGPUを明示選択できる基盤を作る。
リアルタイム描画は単一の高性能GPUへ集約し、動画処理と独立フレームレンダーを
別GPUへ分配することで、GPU間転送を抑えながら総スループットを高める。

## 現状

- D3D12 / Vulkan device生成時にadapterを明示指定していない。
- プロセス全体で単一の共有 `IRenderDevice` / immediate contextを使用する。
- 選択GPUの名前・IDは一部ログに存在するが、共通のadapter registryはない。
- iGPU / dGPU分類、VRAM予算、RT capabilityによる選択がない。
- Render QueueとRAM PreviewはGPU単位のjob schedulingを持たない。

## 方針

- ビューポート、3D、GIは選択した単一dGPUで実行する。
- iGPUは対応する場合にdecode / encode / proxy生成へ優先利用する。
- 複数dGPUは1フレーム内のpass分割ではなく、独立フレーム・独立jobへ割り当てる。
- GPU間で中間render targetを毎フレーム転送しない。
- DiligentEngineをforkせず、Artifact側でdevice registryを所有する。

## Phase 1 — 選択中adapterの診断

- [x] adapter名、Vendor/Device ID、backend、RT対応のread-only API
- [x] 選択中adapterの共通debug state
- [ ] Diagnostics UIとbug reportへの表示

## Phase 2 — Adapter Registry

- [x] D3D12 / Vulkan adapter列挙の共通表現
- [x] Integrated / Discrete / Software / Unknown分類
- [x] local/unified memory、RT capabilityとAutoスコア
- [ ] compute、encode/decode capability
- [ ] stable adapter IDと起動間の設定保存

## Phase 3 — 明示選択

- [x] `Auto / High Performance / Power Saving / Specific GPU` policy
- [x] device生成前にadapter indexを適用
- [x] 無効な指定adapterからAuto相当へ安全にfallback
- [x] device lost時の再生成と診断

初期実装では `ARTIFACT_GPU_POLICY` と `ARTIFACT_GPU_ADAPTER` を使用する。
`Specific` はadapter indexまたはadapter名の部分一致を受け付ける。UIと永続設定は
stable adapter ID導入後に接続する。
既存のone-shot device recoveryを維持し、復旧回数、成功/失敗、復旧前後の
adapter debug stateを共通診断へ記録する。

## Phase 4 — Media GPU分離

- [ ] QSV / NVDEC / AMF等のdecode capability検出
- [ ] iGPU decodeとdGPU renderの境界をCPU frameまたは承認済み共有面に限定
- [ ] encode / proxy / thumbnail jobのGPU選択

## Phase 5 — Multi-dGPU job scheduling

- [ ] GPUごとの独立device/context/cache worker
- [ ] Render Queueをフレーム範囲単位で分配
- [ ] viewport用GPUをバックグラウンドjobから保護
- [ ] VRAM圧力・平均frame time・失敗率による割当調整
- [ ] deterministic outputと失敗frameの再キュー

## 完了条件

- 複数adapter環境で選択GPUを明示・保存・診断できる。
- Auto選択がsoftware adapterより実GPU、iGPUより対応dGPUを優先する。
- RT GIはRT対応メインGPUへ配置される。
- 複数dGPUのRender Queueで単一GPUより高いフレームスループットを得る。
- GPUが1台の環境では現在と同じ単一device経路を維持する。

## 関連文書

- `docs/planned/MILESTONE_CACHED_HYBRID_GLOBAL_ILLUMINATION_2026-07-17.md`
- `docs/planned/MILESTONE_RENDER_QUEUE_GPU_BACKEND_2026-04-03.md`
- `docs/planned/IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-27.md`
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md`
