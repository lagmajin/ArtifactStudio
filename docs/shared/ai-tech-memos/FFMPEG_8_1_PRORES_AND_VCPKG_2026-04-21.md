# FFmpeg 8.1 / ProRes / vcpkg

> 2026-04-21

## Summary

- この repo のローカル build tree は最初 `ffmpeg 8.0.1` を掴んでいた
- vcpkg manifest baseline を上げることで `ffmpeg 8.1` 系へ寄せられる
- vcpkg HEAD の `ffmpeg` port は `8.1#2`
- 現在の `FFmpegEncoder` は `AV_CODEC_ID_PRORES` を `avcodec_find_encoder()` で引いており、`prores` / `prores_aw` / `prores_ks` を backend 名で明示選択していない

## Evidence

- ローカル `build/vcpkg_installed/vcpkg/info/ffmpeg_8.0.1_x64-windows.list`
- `C:\vcpkg` の `ports/ffmpeg/vcpkg.json` は `version: 8.1`, `port-version: 2`
- `avcodec_send_frame()` / `avcodec_receive_packet()` 系が現行の encode API
- `ArtifactCore/include/Video/FFMpegEncoder.ixx` の設定は `videoCodec / container / preset / crf / profile / zerolatency` まで
- `ArtifactCore/src/Image/FFmpegEncoder.cppm` は ProRes を `profile` 文字列から pix_fmt と profile 番号へ畳み込んでいる

## Risks

- `vcpkg.json` の baseline 更新だけでは、ローカル vcpkg checkout が古いと install が失敗する
- `qtbase / qtmultimedia / qtwebsockets` など他 dependency も version database mismatch を起こしうる
- `prores_ks` は FFmpeg 内蔵 encoder であり、Apple 純正実装そのものとは別
- `codec_id` ベースの選択だけだと、build によってどの ProRes encoder が選ばれるかを app 側で固定しにくい

## Recommended App Contract

- `EncoderBackend = NativeFFmpegAPI / PipeFFmpegExe`
- `VideoCodec = ProRes / H264 / HEVC / VP9 / MJPEG / PNG / GIF / APNG / WebP`
- `ProResFlavor = prores / prores_aw / prores_ks`
- `ProResProfile = proxy / lt / standard / hq / 4444 / xq`
- `PixelFormat = yuv422p10le / yuv444p10le / yuva444p10le`
- `Vendor = "appl" など 4 文字`

## Option Mapping Sketch

- `prores_ks`
  - encoder name で明示的に選ぶ
  - profile と pix_fmt を app 側で固定する
  - 4444 系なら alpha 対応 pix_fmt を選ぶ
- `prores_aw` / `prores`
  - encoder name で明示的に選ぶ
  - `vendor` や `profile` の AVOption を渡す
  - Apple ProRes 系として UI 上で扱う
- H.264 / HEVC / VP9
  - 既存の `preset / crf / zerolatency / profile` をそのまま流す

## Next Steps

- `FFmpegEncoder` の settings に `encoderFlavor` を足すか確認する
- `avcodec_find_encoder_by_name()` を使う経路を追加するか確認する
- `vcpkg` / build tree の baseline と lock を揃える
- native ProRes の可否は encoder options の実測で判断する
