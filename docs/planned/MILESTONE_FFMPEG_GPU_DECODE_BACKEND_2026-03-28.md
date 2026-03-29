# FFmpeg GPU Decode Backend Milestone (2026-03-28)

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

