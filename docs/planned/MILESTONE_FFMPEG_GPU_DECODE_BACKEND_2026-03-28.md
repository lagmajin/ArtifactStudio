# FFmpeg GPU Decode Backend Milestone (2026-03-28)

**最終更新:** 2026-08-15
**ステータス:** Vulkan／GPU frame payload と renderer cache 接続の基盤実装済み、policy／preview／runtime parity 待ち

FFmpeg の hardware-accelerated decode backend を CPU software decode とは別系統として作り、
video layer / playback / preview から backend を選べるようにするためのマイルストーン。

ここでいう GPU decode は、単なる「GPU で描画する」ではなく、
`AVHWDeviceContext` / `AVHWFramesContext` を使った hardware frame decode と
GPU surface への bridge を含む経路を指す。

## Goal

- CPU decode backend と GPU decode backend を別実装として持つ
- `auto / cpu / gpu` の選択を明示できるようにする
- GPU decode が使えない環境では CPU backend に自然に fallback する
- `ArtifactVideoLayer` と playback engine から backend を隠蔽し、上位 UI は同じ API で扱えるようにする

## Scope

- `ArtifactCore/src/Media`
- `Artifact/src/Layer`
- `Artifact/src/Playback`
- `Artifact/src/Service`
- `Artifact/src/Widgets/Render`
- `Artifact/src/Preview`
- `ArtifactCore/src/Graphics` 付近の texture / frame bridge

## Non-Goals

- FFmpeg 自体の codec support を増やすこと
- audio decode / audio sync の全面再設計
- すべての platform / driver で同一の hwaccel path を保証すること
- CPU backend の廃止

## Milestones

### M-VD-1 Backend Contract And Capability Detection

- decode backend の public enum を定義する
- `auto / cpu / gpu` の policy を決める
- hardware decode capability を probe する
- backend ごとの失敗理由をログに残す

### M-VD-2 CPU Backend Isolation

- 現行の software decode 経路を `CpuDecodeBackend` として明確化する
- 既存の振る舞いを baseline として固定する
- GPU backend 導入後も CPU 経路が regression reference になるようにする

### M-VD-3 GPU Hwaccel Backend

- FFmpeg の hwaccel API を使って hardware frame を取得する
- `AVFrame` の device-side data を扱える backend を作る
- 可能なら zero-copy または low-copy に寄せる
- hardware unavailable 時は backend の判定で落ちるのではなく policy に従って fallback する

### M-VD-4 GPU Frame Bridge To Renderer

- hardware frame を renderer / texture cache に渡す bridge を作る
- video layer の render path で CPU 変換を減らす
- preview / composition view の両方で同じ frame source を使えるようにする

### M-VD-5 Playback / Seek Semantics

- seek / play / pause / stop 時の buffer reset を backend ごとに定義する
- current frame と decoded frame の整合を保つ
- GPU backend でも timeline playhead と video layer が同じ frame を指すようにする

### M-VD-6 Diagnostics / Fallback Policy

- backend 選択結果と hardware capability をログで見えるようにする
- driver / device / codec / pixel format の失敗要因を切り分けやすくする
- `auto` の fallback 優先順を docs とコードで一致させる

## Recommended Order

1. `M-VD-1 Backend Contract And Capability Detection`
2. `M-VD-2 CPU Backend Isolation`
3. `M-VD-3 GPU Hwaccel Backend`
4. `M-VD-4 GPU Frame Bridge To Renderer`
5. `M-VD-5 Playback / Seek Semantics`
6. `M-VD-6 Diagnostics / Fallback Policy`

## Notes

- `ArtifactVideoLayer` は今でも CPU decode と playback controller に強く依存しているので、
  先に backend contract を固定しないと実装が散らばりやすい。
- 「GPU decode」と「GPU rendering」は別問題なので、両者を混ぜない。
- まずは `auto` が CPU fallback を含むことを前提にして、GPU path は opt-in で始めるのが安全。



---

## Static audit follow-up (2026-07-25)

MediaImageFrameDecoder に FFmpeg の Vulkan hardware device 初期化、hardware frame 検出、GpuVideoFrame 化、direct presentation 判定と安全な download 経路がある。MediaPlaybackController と ArtifactVideoLayer も GPU payload／fallback 状態を扱い、CPU／FFmpeg と MediaFoundation の backend 切替・fallback を持つ。

ただし、文書が要求する public な auto / cpu / gpu policy、GPU capability probe の明示 API、renderer texture bridge の完全接続、GPU frame を Qt preview で直接表示する経路は未完了である。現状は M-VD-1 の一部、M-VD-2 の既存 backend 分離、M-VD-3 の Vulkan 基盤まで進行し、M-VD-4〜6 は未完了または検証待ちとして記録する。

## 現行コード監査 (2026-08-15)

- `MediaImageFrameDecoder` に FFmpeg Vulkan hardware device 初期化、hardware frame 検出、`GpuVideoFrame` 化、direct presentation 判定、安全な download fallback がある。
- `MediaPlaybackController`／`ArtifactVideoLayer` は GPU payload と CPU fallback 状態を扱い、`GPUTextureCacheManager`／composition drawing へ GPU frame を渡す経路を確認した。
- `ArtifactVideoLayer` の通常の `ImageF32x4_RGBA` 消費経路では、Vulkan hardware decode を有効化すると CPU presentation buffer が空になる場合があるため、現状は hardware-only pixel format を避ける安全側の扱いになっている。
- ただし、public な `auto／cpu／gpu` policy と capability probe の統一 API、Qt preview の直接 GPU 表示、seek／playback の backend 間 parity、実機 driver 別受入れは未検証。

判定: **M-VD-1〜3 と renderer 接続の基盤は実装済み。M-VD-4〜6 の完全統合、再生整合、実機受入れは pending。**

## Update 2026-08-15

現行コードを追加照合した。Vulkan hardware device／frame 検出、`GpuVideoFrame` 化、`GPUTextureCacheManager` への受け渡し、CPU download fallback は実装されている。

- 一方、direct Vulkan presentation は同期条件を満たさないため policy 上無効で、GPU frame が常に renderer へ zero-copy で到達する状態ではない。
- また、通常の Video Layer は CPU presentation buffer を前提にしており、GPU payload が取得できても renderer 側で直接消費できない場合は download fallback に戻る。
- `auto／cpu／gpu` の統一 public policy、Qt preview の直接 GPU 表示、seek／playback parity、driver／codec 別の実機受入れは未完了。
- 判定は **Vulkan decode と fallback の基盤は実装済み、direct presentation・policy統合・再生整合・実機受入れは未達** を維持する。
