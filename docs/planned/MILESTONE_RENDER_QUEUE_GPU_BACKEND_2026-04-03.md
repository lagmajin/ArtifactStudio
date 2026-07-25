# マイルストーン: Render Queue GPU Backend Selection / Fallback

> 2026-04-03 作成

## 目的

Render Queue から GPU backend を選択できるようにし、CPU backend と fallback を並行運用できる状態にする。

このマイルストーンは、既存の Render Queue / FFmpeg GPU encode milestone をつなぐ実行計画として扱う。

---

## 背景

現在の Render Queue は、主に CPU ベースの出力と履歴 / ログ / 進捗表示を中心にしている。

一方で、動画出力や最終レンダリングでは GPU の活用余地がある。

このマイルストーンでは、render queue の UI / service 側から GPU backend を選べるようにし、失敗時は CPU に自然に戻る構造を目指す。

---

## 取り込み方針

### 原則

1. CPU backend は残す
2. GPU backend は opt-in から始める
3. backend 選択は job 設定の一部として扱う
4. backend の失敗理由を diagnostics で読めるようにする
5. FFmpeg hwaccel と renderer frame bridge は別 milestone の成果を利用する

---

## Phase 1: Backend Contract

### 目的

Render Queue が backend を表現できるようにする。

### 作業項目

- render job に backend enum を追加する
- auto / cpu / gpu を区別する
- backend 選択を job 設定に持たせる
- fallback 優先順を定義する

### 完了条件

- job から backend が読める
- auto 選択時の挙動が説明できる

---

## Phase 2: GPU Encode Path

### 目的

GPU backend を実際の出力経路へつなぐ。

### 作業項目

- FFmpeg GPU encode backend を Render Queue から呼べるようにする
- frame bridge を render output へ接続する
- capability probe を最初に通す

### 完了条件

- GPU backend が実際に選べる
- 失敗時の理由が見える

---

## Phase 3: UI / Diagnostics

### 目的

backend 選択と fallback を操作しやすくする。

### 作業項目

- Render Queue UI に backend selector を追加する
- capability / fallback / selected backend を表示する
- hardware encoder の失敗理由をログへ出す

### 完了条件

- UI だけで backend を確認できる
- GPU 失敗時に CPU へ切り替えられる

---

## 既存 milestone との関係

- `docs/planned/MILESTONE_FFMPEG_GPU_ENCODE_BACKEND_2026-04-03.md`
- `docs/planned/MILESTONE_RENDER_QUEUE_2026-03-22.md`
- `docs/planned/MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md`

この文書は、FFmpeg 側の encode backend milestone と Render Queue 側の運用をつなぐ。

---

## 推奨順

1. Phase 1
2. Phase 2
3. Phase 3

## Static Audit (2026-07-25)

現行ソースでは、job の `renderBackend` に auto/cpu/gpu を保持し、Render Queue UI から選択・表示できる。GPU renderer の初期化、GPU readback、multi-channel 制約、FFmpeg video backend の native GPU / pipe-hw / pipe-vulkan / native / pipe fallback、選択 backend と失敗理由のログ、JSON 保存・復元が確認できる。したがって Phase 1〜3 の主要な queue/service/UI 経路は static 実装済みと判定する。

未確認なのは、実機ごとの capability probe と runtime の GPU→CPU fallback、encoder/renderer の実測品質・速度、GPU 使用率表示、zero-copy frame bridge、Diligent と FFmpeg 間の GPU resident 受け渡しである。QImage fallback/readback も残るため、完全な GPU-only 経路とは判定しない。ビルド・実行確認は未実施。
