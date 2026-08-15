# FFmpeg GPU Encode Backend Milestone (2026-04-03)

**最終更新:** 2026-08-15
**ステータス:** capability probe／backend 選択／fallback 基盤は実装済み、zero-copy と実機受入れ待ち

FFmpeg の hardware-accelerated encode backend を CPU software encode とは別系統として追加し、
Render Queue からエンコード backend を選択できるようにするためのマイルストーン。

ここでいう GPU encode は、単なる「GPU で描画する」ではなく、
`AVHWDeviceContext` / `AVHWFramesContext` を使った hardware frame encode と
GPU surface からの直接エンコードを含む経路を指す。

## Goal

- CPU encode backend と GPU encode backend を別実装として持つ
- `auto / cpu / gpu` の選択を明示できるようにする
- GPU encode が使えない環境では CPU backend に自然に fallback する
- Render Queue の export job から backend を隠蔽し、既存 API は同じで扱えるようにする
- ハードウェアエンコーダー (NVENC, QSV, AMF, VAAPI) を自動検出し、手動選択も可能にする

## Scope

- `ArtifactCore/src/Image/FFmpegEncoder.cppm` 拡張
- `Artifact/src/Render/ArtifactRenderQueueService.cppm` 設定追加
- `Artifact/src/Widgets/Render/RenderQueueManagerWidget.cpp` UI 拡張
- `ArtifactCore/src/Graphics` の texture / frame bridge
- `Artifact/src/Service` の encode profile 管理

## Non-Goals

- FFmpeg 自体の codec support を増やすこと (既存コーデックのみ)
- audio encode の実装
- すべての platform / driver で同一の hwaccel path を保証すること
- CPU backend の廃止
- リアルタイム streaming encode

## Milestones

### M-RE-1 Backend Contract And Capability Detection

- encode backend の public enum を定義する (`EncodeBackend::Auto/CPU/GPU`)
- hardware encode capability を probe する (Vendor 検出 + codec サポート確認)
- backend ごとの初期化失敗理由を logging する
- システムコンテキスト (NVIDIA/Intel/AMD) の検出ロジックを分離する

### M-RE-2 CPU Backend Isolation

- 現行の software encode 経路を `CpuEncodeBackend` として明確化する
- 既存の `FFmpegEncoder` の振る舞いを baseline として固定する
- GPU backend 導入後も CPU 経路が regression reference になるようにする
- pixel format conversion の责任範囲を backend 侧に闭じ込める

### M-RE-3 GPU Hwaccel Backend Core

- FFmpeg の hwaccel API を使って hardware frame を取得する
- `AVBufferRef* hwdevice_ctx` と `AVHWFramesContext` を backend で管理する
- 各ベンダー用の初期化 (NVENC/QSV/AMF/VAAPI) を選択的にロードする
- avcodec_send_frame/receive_packet の GPU path を通す
- frame queue と drain semantics を backend ごとに実装する

### M-RE-4 GPU Frame Bridge From Renderer

- `ArtifactIRenderer` / `CompositionRenderer` から GPU texture を引いてくる bridge を作る
- video layer ではなく render output が source であることを明確にする
- texture readback を最小化し、可能なら zero-copy でエンコードする
- preview / render queue の両方で同じ frame source を扱えるようにする

### M-RE-5 Encode Settings And Preset Management

- backend ごとの preset システム (quality/performance/balanced)
- profile / level / pixel format の backend 依存制約を UI で表現する
- CRF vs bitrate のbackend别opt を整理する
- `RenderJob` に `backend` と `hwDevice` フィールドを追加する

### M-RE-6 Diagnostics And Fallback Policy

- backend 選択結果と hardware capability を console で見えるようにする
- driver / device / codec / pixel format の失敗要因を切り分けやすくする
- `auto` の fallback 優先順を docs とコードで一致させる
- GPU encode が使えない場合、自動的に CPU backend に切り替える
- エンコード失敗時のリカバリ手順 (cleanup + retry) を確立する

## Recommended Order

1. `M-RE-1 Backend Contract And Capability Detection`
2. `M-RE-2 CPU Backend Isolation`
3. `M-RE-3 GPU Hwaccel Backend Core`
4. `M-RE-4 GPU Frame Bridge From Renderer`
5. `M-RE-5 Encode Settings And Preset Management`
6. `M-RE-6 Diagnostics And Fallback Policy`

## Dependencies

- `docs/planned/MILESTONE_FFMPEG_GPU_DECODE_BACKEND_2026-03-28.md` (アーキテクチャ参照)
- `Artifact/docs/MILESTONE_RENDER_QUEUE_2026-03-22.md`
- `ArtifactCore/docs/MILESTONE_FFMPEG_ENCODER_2026-03-17.md`

## Notes

- GPU encode はハードウェア世代・ドライバーバージョンに敏感なため、capability probe を最初に固める。
- 初期段階では `auto` は「GPU が使えれば GPU、なければ CPU」とし、明示的な `gpu` 選択は opt-in にする。
- FFmpeg の hwaccel は decode と encode で API が異なるため、decode backend と共有するのは capability detection 部分のみとする。
- `Render Queue` UI は既存の job 編集ダイアロogoを拡張し、backend プルダウンを追加する。

---

## Static audit follow-up (2026-07-25)

FFmpegEncoder と Render Queue に GPU backend kind、NVENC／QSV／AMF／VAAPI の候補列挙、encoder availability probe、GPU settings 生成、CPU／GPU backend 選択の基盤がある。Render Queue から GPU encoder を選ぶ経路と hardware encoder 不在時のエラー／fallback 判断も確認できる。

一方、public な EncodeBackend::Auto/CPU/GPU 契約の完全な統一、renderer texture からの zero-copy bridge、backend ごとの設定 UI／job schema、auto の CPU retry と cleanup／recovery の実運用は未確認である。M-RE-1〜3 は基盤実装、M-RE-4〜6 は未完了または検証待ちとして記録する。

## 現行コード監査 (2026-08-15)

- FFmpeg encoder probe、NVENC／QSV／AMF／VAAPI 相当の codec 名解決、Render Queue の encoder backend／preset 設定、GPU 不可時の CPU／pipe fallback 経路を確認した。
- native GPU backend と pipe-hw／pipe-vulkan の入口はあるが、renderer texture から encoder までの zero-copy、backend ごとの完全な job schema／UI 契約、失敗後 cleanup→CPU retry の実運用は未検証。
- GPU encoder の可用性、品質／速度、GPU 使用率は環境依存のため、静的コード確認だけでは完了扱いにしない。

判定: **M-RE-1〜3 の基盤と queue integration は実装済み。M-RE-4〜6 の完全統合、recovery、性能受入れは pending。**

## Update 2026-08-15

現行コードを追加照合した。FFmpeg encoder probe、hardware codec 候補、Render Queue の backend／preset 選択、native GPU／pipe-hw／pipe-vulkan／CPU fallback の入口は存在する。

- `ArtifactIRenderer` の GPU texture を encoder が直接受け取る zero-copy bridge は確認できない。
- backend ごとの job schema／UI 契約の完全な統一、失敗後の cleanup→CPU retry、実機 capability／品質／速度／GPU使用率の受入も未完了。
- 判定は **capability／選択／fallback 基盤は実装済み、zero-copy・recovery・性能受入れは未達** を維持する。
