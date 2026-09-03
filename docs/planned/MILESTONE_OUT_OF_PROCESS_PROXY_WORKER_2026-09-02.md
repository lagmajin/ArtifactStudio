**最終更新:** 2026-09-02

# MILESTONE: Out-of-Process プロキシ生成ワーカー（ArtifactProxyWorker）

**ステータス:** 部分実装（worker runtime 検証済み、host UI 統合検証待ち）  
**親マイルストーン:** `M-WKR-1 Background Utility Worker Process`  
**関連箇所:** `Proxy.Service`、`ArtifactProxyManager`、`ArtifactProjectManagerWidget`

## 目的

動画プロキシ生成だけを最初の縦切りとして `ArtifactProxyWorker.exe` に分離し、現在 UI スレッドで同期実行される `ArtifactProxyManager::generateProxy()` を置き換える。これは汎用 Utility Worker の先行実装であり、render farm、thumbnail、waveform、画像プロキシまでは含めない。

## 現状確認（2026-09-02）

- `Artifact/src/Layer/ArtifactVideoLayer.cppm` の `ArtifactProxyManager::generateProxy()` は `ffmpeg` を起動し、`waitForFinished(-1)` で完了を待つ。
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` の `processNextProxyJob()` はタイマーからこの同期 API を呼ぶため、動画ジョブ中は UI イベントループを塞ぐ。
- 既存の `ArtifactWorker` target は render-farm 用であり、プロキシ生成の実行先として再利用しない。
- worker 専用 target でも FFmpeg の C API 依存を解決でき、native 経路の runtime 検証まで完了している。配布時の DLL 同梱条件は staged package で確認済み。

## 2026-09-02 実装記録

- `ArtifactProxyWorker.exe` の QtCore console target と、`--request <JSON>` による単発 job 実行を追加した。
- worker は `ffmpeg.exe` を起動し、partial output を final output へ rename してから JSON Lines の terminal result を出力する。
- Project View の既存 proxy queue は新しい signal/slot を追加せず、既存 timer tick から worker の終了状態を poll する。成功は exit code と non-empty final output を確認した場合だけ project に反映する。
- Project View は同じ timer tick で worker stdout の JSON Lines を読み、`progress.fraction` を proxy queue の進捗表示へ反映する。未完了の行は次の tick まで buffer に保持する。
- `failed.reason` がない異常終了では、host が worker stderr を補助診断として進捗表示の tooltip に残す。
- `ArtifactProxyWorker` target の Debug build、`--help`、不正 request の失敗 JSON、1 秒の H.264/AAC 動画を入力にした ffmpeg half-scale proxy 生成を確認済み。さらに、2 秒・320×180 の H.264/AAC 実動画を `backend: mediaFoundation` と `backend: native` でそれぞれ 160×90 H.264 MP4 へ変換し、exit code 0・completed JSON・`ffprobe` を確認済み。native 出力には 48kHz AAC 音声も含まれる。worker EXE の packaging は staged package で runtime 確認済み、host UI 統合はコード確認まで完了している。
- host は worker の exit code だけで成功扱いにせず、active job と一致する `completed.jobId` と `completed.outputPath` の両方を確認してから project に反映する。無関係な JSON Lines は進捗・失敗・完了状態を変更しない。
- host は `completed.outputBytes` と実際の final output サイズも照合し、完了通知と成果物の不一致を拒否する。
- 追加の実機 smoke test で `comp1.mov` を `scale: 0.125` に変換し、FFmpeg backend は 21,275 bytes、native backend は `h264_mf` で 18,961 bytes を生成。native 出力は `ffprobe` で H.264 50×50 video stream を確認した。
- `tools/proxy_worker_smoke_test.py` を追加し、worker path・fixture・backend・scale・`--audio-reencode`・`--hardware-accel`・`--expect-backend`・`--cancel-after-ms` を指定した JSON Lines、exit code、jobId/outputPath、outputBytes、completed の要求値反映、cancel 時の非完了終了と output cleanup、任意の ffprobe 検証を一括実行できるようにした。一時 request/output は自動 cleanup する。
- smoke test に `--expect-failure` を追加し、明示 backend の capability 拒否を「非ゼロ終了・completed なし・output なし」として検証できるようにした。`mediaFoundation + audioReencode` の拒否で `passed: true` を確認した。
- smoke test は成功・失敗・キャンセルの全経路で最終出力だけでなく `temporaryOutputPath` の partial が残らないことも検証し、失敗時は worker の `failed.reason` を結果JSONへ出力する。
- smoke test に `--timeout-seconds` を追加し、ワーカーがハングした場合も terminate／kill 後に明確な失敗として終了する。結果JSONには `timedOut` も記録する。
- smoke test の `auto` backend も成功し、実行環境では `backend: native`、`h264_mf`、H.264 50×50、18,961 bytes を確認した。
- 一時生成した H.264/AAC 素材で `--audio-reencode --require-audio` を実行し、FFmpeg backend、H.264 40×22 と AAC audio stream、12,149 bytes、`passed: true` を確認した。
- 同じ素材で `backend=native` と `audioReencode=true` を指定した場合も成功し、結果の実 backend が `ffmpeg` へ切り替わること、H.264/AAC 出力が維持されることを確認した。
- `backend=auto` と `audioReencode=true` の組み合わせも成功し、Media Foundation を選ばず FFmpeg backend へ切り替わり、H.264 40×22 と AAC audio stream を維持した。
- native backend の処理中に `--cancel-after-ms 50` を実行し、`returncode: 1`、`completed: null`、`outputBytes: 0`、`passed: true` を確認した。
- smoke test に `--hardware-accel` を追加し、native backend で要求値 `hardwareAccelRequested=true`、`h264_mf`、`hardwareEncoderUsed=true`、H.264 50×50、18,961 bytes を確認した。
- `mediaFoundation` は ProRes の `comp1.mov` では completed を返さず失敗した。Media Foundation の映像入力互換性は H.264 fixture で別途確認する。smoke test は失敗時も return code・completed message・outputBytes・`passed:false` を出力する。
- H.264 fixture では `mediaFoundation` の Eighth も成功し、H.264 116×64 を確認した。MFT が極小サイズを受け付けないため、Media Foundation 経路はアスペクト比を維持した最小 64px/axis へクランプする。ProRes などの入力非互換は `auto` fallback の対象とする。
- Native の hardware H.264 encoder でも極小 Eighth 出力が `MF_E_INVALIDMEDIATYPE` になるため、同じ最小 64px/axis の寸法制約を適用した。音声付き fixture の native Eighth で完了、`qualityPreset: eighth`、`passed: true` を確認した。
- ProRes `comp1.mov` の `auto` も成功し、native (`h264_mf`) が Media Foundation 単体より先に処理できる場合は、FFmpeg fallback まで進まず H.264 50×50 を生成することを確認した。
- Project View の `Cancel Proxy Queue` は待機中 job を破棄し、実行中 worker には cancel token を渡す。worker 終了後は partial output を破棄し、生成開始前に退避した旧 proxy を復元する。
- `ProxyServiceQuality::Eighth` は Project View、Timeline、Layer Menu、Inspector の表示・選択・queue scale (`0.125`) まで接続した。
- Project View の video proxy 出力パス選択で Eighth を Quarter と共有していた分岐を修正し、`0.125` は `ProxyServiceQuality::Eighth` 固有の cache path を使うようにした。
- Project View の共通 `proxyFilePathForFootage()` も動画では `ArtifactProxyManager` の MP4 cache path を返すよう統一し、表示・反映側が旧 JPEG path を参照する不整合を解消した。
- Project View の破棄時は cancel token を先に発行し、active job の final／partial output を破棄して退避済みの旧 proxy を復元するようにした。
- 次回 queue 構築時に active worker がない場合、job 固有の backup／cancel token に加えて残存 `*.partial` も掃除し、クラッシュ後の partial を再利用・誤認しないようにした。
- Project View の動画判定を `flv / m2ts / ts / mpg / mpeg / wmv / 3gp / ogv` まで拡張し、FFmpeg が扱える入力を画像 proxy と誤認しないようにした。
- さらに `mts / mxf / vob / ogm / asf / 3g2` を動画判定へ追加し、Worker 経路の形式カバレッジを揃えた。
- `ArtifactProxyManager::proxyFilePath()` は `None` 品質に対して空パスを返すようにし、無効品質が Eighth cache path に解決される曖昧さを除去した。
- host request にも `qualityPreset` (`full / half / quarter / eighth`) を付与し、数値 scale と品質名を同時に記録するようにした。
- Worker の `completed` response にも適用済み `qualityPreset`（scale のみの旧 request は `custom`）を返し、request／実行結果の照合を可能にした。
- host は `completed.qualityPreset` も active job の scale と照合し、不一致の完了通知を project 反映対象から除外する。
- job／output が一致していても preset が不一致の場合は、完了扱いにせず進捗 tooltip に原因を表示する。
- Worker は未知の `qualityPreset` を無視せず request error として拒否し、品質指定の typo による意図しない scale での成功を防ぐ。
- Media Foundation 経路も frame 読み出し前に cancel token を監視し、キャンセル時は partial output を削除して `failed.reason` を返すようにした。
- Worker path は `Proxy/WorkerPath` で override 可能にし、未設定時は従来どおりアプリケーション隣接の `ArtifactProxyWorker.exe` を使う。
- Debug runtime の `ArtifactProxyWorker.exe` を `dumpbin /DEPENDENTS` で確認し、Qt6Core、avcodec、avformat、avutil、swscale の依存 DLL が同じ runtime directory に存在することを確認した。Media Foundation は Windows system DLL (`MFPlat.dll` / `MFReadWrite.dll`) 依存で、同梱対象外。新設した `ArtifactProxyWorkerPackage` を vcpkg Ninja で実行し、`proxy-worker-package/Debug` に Worker、`ffmpeg.exe`、Qt6Core DLL が staging されることも確認した。
- staged package の初回実行で Qt／FFmpeg の二次依存 DLL 不足が判明したため、vcpkg runtime DLL 全体を package target からコピーするよう修正。再 staging 後、隣接 `ffmpeg.exe` を使った FFmpeg quarter proxy が `returncode: 0`、`passed: true` で完了した。
- package target の Windows 専用 vcpkg runtime copy を条件分岐し、非 Windows configure で Windows script を参照しないよう整理。再 staging 後の FFmpeg quarter smoke test も成功した。
- FFmpeg DLL copy command も Windows／DLL 検出時だけ生成するよう条件化し、空の copy command を非 Windows package に持ち込まないようにした。Windows staged package の再 staging は成功した。
- 同じ staged package から Native quarter proxy (`h264_mf`) も `returncode: 0`、`passed: true` で完了。さらに Auto＋音声再エンコードを staged `ffmpeg.exe` で実行し、H.264 video と AAC audio、`passed: true` を確認した。
- staged package から Native Eighth も `qualityPreset: eighth`、`returncode: 0`、`passed: true` で完了し、最小寸法補正を含む配布物の動作を確認した。
- staged package の Native 経路で Full／Half／Quarter／Eighth を連続実行し、全て `returncode: 0`、`passed: true`、対応する `qualityPreset` を確認した。
- H.264 fixture の Media Foundation job を 50ms 後にキャンセルし、`returncode: 1`、`completed: null`、`outputBytes: 0`、`passed: true` を確認した。
- smoke test も `completed.qualityPreset` を検証し、音声付き fixture の native quarter-scale で `qualityPreset: quarter`、`passed: true` を確認した。
- Windows では request の `backend` に `mediaFoundation`、`native`、または `auto` を指定できる。`native` は FFmpeg C API で映像を decode → `swscale` 縮小 → H.264 encode し、入力音声を対応するコンテナへ stream copy する。Media Foundation は SourceReader → CPU 縮小 → SinkWriter による映像専用の MP4/H.264 経路で、音声を含む proxy は `native` または `ffmpeg` を選ぶ。`auto` は native → Media Foundation → ffmpeg の順で fallback する。native の完了結果には採用エンコーダー名と検出候補 (`encoder`, `encoderCandidates`) を含める。通常の Project View は `QSettings` の `Proxy/WorkerBackend` を request に渡し、既定値 `ffmpeg` を維持する。
- `backend: mediaFoundation` と `audioReencode: true` の組み合わせは、別 backend へ暗黙に切り替えず、worker が capability error として拒否する。自動選択 (`auto`) は音声再エンコード要求時に FFmpeg 経路へ限定する。
- Windows 以外で `backend: mediaFoundation` を明示指定した場合も capability error として拒否し、明示指定と `auto` fallback の意味を分離した。

## 境界と非目標

- `ArtifactProxyWorker.exe` はローカルの一回限りの動画プロキシ job だけを実行する。
- 親プロセスは job の開始、状態取得、完了結果の project 反映だけを担う。
- `ArtifactVideoLayer` は引き続き proxy path / quality の保持と再生切替だけを担い、生成プロセスを所有しない。
- 新しいグローバル signal/slot や EventBus は追加しない。既存の proxy queue の tick と明示的な状態照会で UI を更新する。
- render farm、常駐 worker、複数 job の並列実行、画像の `QImage` ベース proxy、汎用 `UtilityJob` 化は対象外。

## 実行契約

最初は stdio の 1 行 JSON（JSON Lines）を採用する。local socket は常駐化が必要になった時点で再評価する。

### 起動入力

親は request JSON を一時ファイルに原子的に書き、worker を `--request <file>` で起動する。CLI に素材パスを列挙しないため、引用符・長いパス・将来の option 追加を安定して扱える。

```json
{
  "protocolVersion": 1,
  "jobId": "uuid",
  "sourcePath": "...",
  "outputPath": ".../.proxy/clip_proxy_half.mp4",
  "scale": 0.5,
  "videoCodec": "libx264",
  "audioCodec": "aac",
  "backend": "ffmpeg",
  "hardwareAccel": false,
  "audioReencode": false,
  "temporaryOutputPath": ".../.proxy/clip_proxy_half.partial.mp4"
}
```

`backend` は `ffmpeg`（既定）、`native`（FFmpeg C API）、`mediaFoundation`（Windows 限定）、`auto`（native → Media Foundation → ffmpeg fallback）のいずれか。
`hardwareAccel: true` を native と併用すると、利用可能な NVENC / QSV / AMF / Media Foundation H.264 encoder を優先し、利用できない場合は検出済みの代替 encoder に戻る。これは hardware frame decode を保証するものではない。
`audioReencode: true` は AAC 再エンコード対応の FFmpeg 経路を選び、native の音声 stream-copy 制約を回避する。`auto` でも音声を落とす Media Foundation 経路はスキップする。Project View では `Proxy/AudioReencode` から request に渡される。backend / hardware / audio の3項目は App Settings schema にも登録する。

### worker 出力

stdout は JSON Lines のみとし、stderr は診断ログ用に残す。親は polling 時に読めた完全行だけを処理する。

```json
{"type":"progress","jobId":"uuid","fraction":0.42}
{"type":"completed","jobId":"uuid","outputPath":"...","outputBytes":1234,"backendRequested":"auto","backend":"native","encoder":"h264_mf","hardwareAccelRequested":false,"hardwareEncoderUsed":true,"audioReencodeRequested":false}
{"type":"failed","jobId":"uuid","reason":"ffmpeg exited with code 1"}
```

worker は成功時だけ temporary output を final output へ rename し、失敗・cancel 時は temporary output を削除する。親は `completed` 通知だけでなく exit code と final output の存在を確認して成功扱いにする。
FFmpeg 子プロセス経路も実行中に `cancelPath` を監視し、cancel ファイル検出時は terminate（必要なら kill）して partial output を削除する。
host は worker 起動前に既存 final proxy を job 固有の退避ファイルへ移し、成功時だけ破棄する。失敗・cancel・stale・host 破棄時は旧 proxy を復元する。
次回の proxy queue 構築時、active worker が存在しない場合に限り、proxy 出力ディレクトリ内の job 固有 orphan backup / cancel token (`.*.proxy-job.json.previous`, `.*.proxy-job.json.cancel`) を掃除する。

## フェーズ

### Phase 0 — 既存プロトタイプの受入確認

- 他 AI が用意した worker source、entry point、依存 DLL、ライセンス情報を特定する。
- `ArtifactProxyWorker` という target 名と出力先が既存 target と衝突しないことを確認する。
- worker が `ffmpeg.exe` を探索する規則を、既存 render queue の探索規則と揃える。

**完了条件:** 取り込む source と配布物が明確で、ビルド target の責務が render-farm worker と分離されている。

### Phase 1 — 単発 worker EXE

- `Artifact` 配下の最小 QtCore console target として `ArtifactProxyWorker.exe` を追加する。
- request file を検証し、動画 source に対して既存と同じ scale / H.264 / AAC 設定で `ffmpeg.exe` を子プロセス起動する。
- JSON Lines の started / progress / completed / failed を出力する。
- output の temporary-to-final rename、異常終了時の partial cleanup、0 以外の exit code を実装する。

**完了条件:** 単独実行で half / quarter / eighth の動画プロキシを生成し、final output が完全成功時にだけ見える。

### Phase 2 — Proxy Manager adapter

- `ArtifactProxyManager` に job handle を返す非同期入口を追加し、同期 `generateProxy()` の呼び出し元を段階的に adapter へ置換する。
- host 側は `QProcess` の状態・stdout・stderrを既存の queue tick から poll し、進捗と terminal result を保持する。
- cancel は worker を終了させ、終了確認後に temporary output を掃除する。二重開始、同一 source/quality の重複、古い job 完了の上書きを jobId と generation で防ぐ。

**完了条件:** `ArtifactProjectManagerWidget` の動画 proxy queue が UI を塞がず、一度に一つの worker job を追跡できる。

### Phase 3 — Project 反映と失敗復旧

- worker の正常終了・final output 検証後だけ `syncProxyPathToProject()` を呼ぶ。
- 失敗／cancel／worker crash では既存の失敗表示経路に reason を渡し、proxy metadata を成功扱いにしない。
- source timestamp、quality、output path を job snapshot に記録し、実行中に source が変わった job は結果を反映しない。
- アプリ終了時には worker を明示終了し、次回起動時に残った `.partial` を安全に掃除する。

**完了条件:** cancel、worker crash、source 更新、アプリ終了のいずれでも壊れた proxy path を project に保存しない。

### Phase 4 — 実機受入と次判断

- 長尺・4K・空き容量不足・ffmpeg 不在・出力パス書込不可を確認する。
- proxy / full の切替、保存・再読込、連続ジョブ、cancel 後の再実行を確認する。
- worker の依存 DLL 配置と `ffmpeg.exe` の配布／探索を確認する。
- C API 化、常駐化、thumbnail/waveform への展開は、この結果を踏まえて `M-WKR-1` 側で判断する。

**完了条件:** UI 応答性、成果物の原子性、失敗時の project 整合、配布時の実行可能性を実機で確認できる。

## 受入基準

- [x] 動画 proxy job が実行中でも編集・スクラブ・再生をブロックしない。
- [ ] worker crash、ffmpeg failure、cancel が main process の終了や壊れた metadata を招かない。
- [x] Project View から proxy queue をキャンセルでき、実行中 worker の partial output を成功扱いにしない。
- [x] Artifact runtime / `ARTIFACT_DEPLOY_DIR` へ worker 本体と検出済み FFmpeg fallback executable を deploy できる。
- [x] full / half / quarter / eighth の既存命名規則と品質設定を維持する。
- [x] 成功時だけ final proxy が現れ、VideoLayer がその path に切り替わる。
- [x] 同一 source/output/scale の queue 重複・実行中重複を抑止し、古い job の結果による上書きを防ぐ。
- [x] queue 作成時の source 更新時刻・サイズを完了時に照合し、古い job の output を反映・保持しない。
- [ ] 既存の `ArtifactWorker`（render farm）と target・プロトコル・責務を共有しない。

## 依存・リスク

| 項目 | 方針 |
|---|---|
| 配布 | `ArtifactProxyWorker.exe` と必要 DLL、`ffmpeg.exe` の同梱位置を packaging で明示する。 |
| IPC | protocol version と jobId を必須にし、未知 message は失敗にせず診断へ残す。 |
| キャンセル | 強制終了後も output を成功扱いにせず、親が final path を検証する。 |
| C++ modules | worker target は既存の小さな console entry を基準にし、app UI module を import しない。 |
| 将来統合 | 本 milestone は `M-WKR-1` の proxy-only slice。共通 scheduler / job contract への早期一般化はしない。 |

## 実装順

1. Phase 0 の受入確認
2. request/result protocol と単発 EXE
3. atomic output / cleanup
4. Proxy Manager adapter と queue polling
5. project 反映・cancel・stale 防止
6. 実機受入後に、汎用 worker へ広げるか判断
