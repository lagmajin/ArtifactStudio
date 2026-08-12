# Shared Device Lease / Image Cache Audit

**最終更新:** 2026-08-11

## 結論

- Shared render device は `RefCntAutoPtr` とは別の手動 refCount を持つ。GPU effect の acquire/release 対称性が崩れると、共有デバイスが解放されない。
- `ImageF32x4RGBAWithCache` の CPU→GPU upload、GPU→CPU readback、SRV/UAV 取得は実装済みであり、旧 Insight の「未実装」という記録は現状に一致しない。

## 確認できた事実

### Shared render device

- `Artifact/src/Render/DiligentDeviceManager.cppm` の acquire は成功ごとに shared refCount を増やす。
- `Artifact/src/Effects/` では acquire 使用ファイルが 51、release 使用ファイルが 15 だった（2026-08-11 の静的検索）。
- 永続 GPU resource を持つ effect と、一時的なローカル device/context を使う effect が混在している。両者を同じ機械置換で修正してはならない。
- `SharedRenderDeviceLease` を導入し、まず一時利用の `InvertEffect` を移行した。残りは effect の所有期間ごとに段階移行する。

### ImageF32x4RGBAWithCache

- `SetGpuResources()` は GPU texture を作成し、CPU dirty 時の SRV/UAV 取得は `UpdateTexture()` で CPU→GPU upload を行う。
- `MarkGpuDataDirty()` 後の `image()` は staging texture への copy/readback を行い、古い CPU data をそのまま再uploadしない。
- `MarkGpuDataDirty()` の呼び出し元は静的検索で 0 件だった。外部 GPU pass が cache の UAV を書く経路を追加する場合、この呼び出しを必須とする。

## 未確認・次の確認

- ビルド・実機確認は未実施。
- shared device の refCount が GPU effect の作成・破棄、および device loss/backend 切替後に 0 へ戻ること。
- GPU effect が `ImageF32x4RGBAWithCache` の UAV を直接使う場合に、書込み後の `MarkGpuDataDirty()` が必ず呼ばれること。
- GPU→CPU readback は同期的に `Flush()` / `WaitForIdle()` するため、hot path に追加しないこと。
