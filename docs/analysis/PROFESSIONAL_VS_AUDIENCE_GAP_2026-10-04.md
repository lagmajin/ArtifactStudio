# 動画・モーション ソフトとしての仕様ギャップ監査

更新内容: 2026-10-04 初版。実コード監査のみ。ビルドと実機は未実施。S 級 6 項目（S-1〜S-6）と、結線・UI 追加だけで済む A/B 級 14 項目を修正済み。

**最終更新:** 2026-10-04
**ステータス:** 実コード監査（ビルド・実機未実施）
**対象:** Artifact / ArtifactCore（`main` ブランチ）
**監査軸:** 軸1 再生の信頼性、軸2 レンダー出力、軸3 タイムライン編集、軸4 カラー管理
**出典:** 実コードの file:line。既存の `docs/analysis/`（83件）と `docs/planned/`（741件）と重複する結論は避け、本稿は今回新たに判明した事実のみを記載する。

---

## 0. このドキュメントの位置づけ

既存の AE / Nuke / Resolve ギャップ分析多数は再掲しない。プロの現場で作業が成立しないという現実被害の軸に絞った実コード監査の結果である。

判定は 3 段階。**MISS** は不在または到達不能。**PART** は実装はあるが backend 依存や未配線で意図どおりに動かない。**OK** は実経路に接続済み。

深刻度は **S**（納品できない）、**A**（作業速度と品質を損なう）、**B**（作業上の不便）。

---

## 1. 総合サマリ

### 深刻度 S

| # | 項目 | 軸 | 状態 | 根拠 |
|---|---|---|---|---|
| S-1 | ProRes の profile 番号が backend 間で不整合。Native は hq→4、4444→5。Pipe は hq→3、4444→4。backend は auto なので同じ設定で tier が変わりうる | 2 | 修正済み | `FFmpegEncoder.cppm:292-303` |
| S-2 | 動画出力に atomic write がない。最終パスへ直接 avio_open し、途中まで書いた不完全ファイルが残る。掃除は 0 バイト限定 | 2 | 未着手 | `FFmpegEncoder.cppm:357` / `ArtifactRenderQueueService.cppm:8525-8535` |
| S-3 | Timecode track をレンダー経路が供給しない。startTimeCodeFrame と dropFrame が RenderQueue から設定されず、pipe backend では timecode 引数が無い | 2 | 修正済み | `ArtifactRenderQueueJob.ixx:86-91` / `ArtifactRenderQueueEncoder.cppm:571-574,746-761` / `FFmpegEncoder.cppm:368-381` |
| S-4 | drop-frame の入力手段が存在しない。job にフィールドが無く UI にトグルが無く、native 的 DF ブロックは常に false | 2 | 修正済み | `ArtifactRenderQueueJob.ixx:90-91` / `ArtifactRenderOutputSettingDialog.cppm:1738-1746` |
| S-5 | VideoLayer に OCIO 経路が 1 行も無い。Rec.709 的映像が clip 済みの sRGB としてそのまま入る | 4 | 修正済み | `ArtifactVideoLayer.cppm:487-522,819-840,1089-2192,2387-2417,3011-3030,3220-3229` |
| S-6 | 出力 bit depth 的選択が存在しない。job にフィールドが無く UI に無い。writer は HALF を FLOAT に上げるため 16bit EXR は書けない | 4 | 修正済み | `ArtifactRenderQueueJob.ixx:92-94` / `ImageExporter.cppm:100-117` / `ArtifactRenderQueueService.cppm:1445-1458,7876,6031-6046` |
| S-7 | 8bit への clip が無言で起きる。readbackToImage が必ず clamp(0,1)、AOV 以外は必ず toQImage を通る | 4 | 未着手 | `ArtifactIRenderer.cppm:2251-2297` / `ImageF32x4_RGBA.cppm:383-397` |

### 深刻度 A

| # | 項目 | 軸 | 状態 | 根拠 |
|---|---|---|---|---|
| A-1 | sub-frame nudge が無い。キー移動的 step は 1 か 10 的 2 段のみ。時刻が llround で整数に丸められる | 3 | 未着手 | `ArtifactTimelineTrackPainterView.cppm:11692-11733` |
| A-2 | Ctrl+F 的ジャンプがレイヤー単位。数百プロパティの中的 1 キーを 1 秒で探せない | 3 | 未着手 | `ArtifactTimelineTrackPainterView.cppm:11445` |
| A-3 | 補間種別によるキー的色分けが無い。keyframeInterpolationColor が Q_UNUSED で type を捨てる | 3 | 修正済み | `ArtifactTimelineTrackPainterView.cppm:3283-3322` |
| A-4 | Dope Sheet が読み取り専用。NoEditTriggers 与 NoSelection を設定し、Transform プロパティのみ通す | 3 | 未着手 | `ArtifactDopeSheetWidget.cppm:77-78,136-139` |
| A-5 | 複数レイヤー的一括 trim 与 slide が不可。clip ドラッグは単一 index のみ | 3 | 未着手 | `ArtifactTimelineTrackPainterView.cppm:7889,9138-9147` |
| A-6 | リタイム（両端同時）が不可。DragMode に両端同時モードが無い | 3 | 未着手 | `ArtifactTimelineTrackPainterView.cppm:614` |
| A-7 | Jeep が無い。スナップは常時 on でトグルも無い | 3 | 未着手（設定基盤が必要） | `ArtifactTimelineTrackPainterView.cppm:3471-3538` |
| A-8 | 名前付きガイドが存在しない。データモデル・描画・編集 UI が全て無い | 3 | 未着手（設計が要る） | `ArtifactTimelineTrackPainterView.cppm` に guide 的永続モデル ゼロ |
| A-9 | DPX 書き出しが無い。読み込みのみ実装。writer・UI・プリセット全て不在 | 2 | 未着手（writer 追加が要る） | `ArtifactCore/src/Asset/AssetImporter.cppm:119` に dpx が無い |
| A-10 | 自動採番が実装済みだがレンダー経路に未配線だった | 2 | 修正済み | `ArtifactRenderQueueService.cppm:1460-1482,3740-3746` / `ArtifactRenderQueueJob.ixx:98-100` |
| A-11 | useFarm が false ハードコード。Farm 的バックエンドはほぼ全機能だが 1 行でレンダーから切断 | 2 | 未着手（UI 追加が要る） | `ArtifactRenderQueueService.cppm:7269` |
| A-12 | GPU デフォルト経路で並列レンダー無効。useMfr が not useGpuBackend 条件付き | 2 | 未着手（設計が要る） | `ArtifactRenderQueueService.cppm:6987` |
| A-13 | 2D velocity 与 2D depth が無い。AOV は 11 項目あるが全て 3D 前提 | 2 | 未着手（レンダラ改造が要る） | `ArtifactRenderQueueService.cppm:925-951` |
| A-14 | OCIO preset を選ぶと必ず自前 matrix に落ちていた | 4 | 修正済み | `ArtifactOCIOManager.cppm:148-154` |
| A-15 | proxy に color metadata が無かった | 4 | 修正済み | `ArtifactVideoLayer.cppm:2471-2483,2382-2417` |
| A-16 | FootageInterpretService が実レイヤーまで届かなかった | 4 | 修正済み | `FootageInterpretService.cppm:202-286` |
| A-17 | 連番画像的 fps 既定が 24 固定。setFrameRate 済みでも openFramePaths で再代入される | 4 | 修正済み | `ImageSequenceSource.cppm:67,71,316,367,395,695` |
| A-18 | メモリ予算が画像 1 枚単位のみ。8K でレイヤーあたり最大 5 枚分的 float buffer が同時に生存 | 4 | 未着手（設計が要る） | `ArtifactImageLayer.cppm:188-199,1003` |
| A-19 | リタイム相当が別ドックに隔離。commitClipRetime は実在するが既定 hidden で、本 Timeline から到達不能 | 3 | 未着手（導線追加が要る） | `ArtifactAnimationTimelineWidget.cppm:206-250` / `AppMain.cppm:6567` |

### 深刻度 B

| # | 項目 | 軸 | 状態 | 根拠 |
|---|---|---|---|---|
| B-1 | EXR compression が zip 固定で UI に選択が無かった | 2 | 修正済み | `ArtifactRenderQueueJob.ixx:95-97` / `ArtifactRenderQueueService.cppm:7994-7996` / `ArtifactRenderOutputSettingDialog.cppm:1746-1757` |
| B-2 | Cryptomatte は draft 4ch のみ。1.3 準拠は未達 | 2 | 未着手 | `ImageExporter.cppm:462-495` |
| B-3 | commitAtomically は remove から rename なので真の atomic でない | 2 | 修正済み | `ImageExporter.cppm:259-302` |
| B-4 | commitAtomically 失敗時に元ファイルの復元が無い | 2 | 修正済み | `ImageExporter.cppm:289-296` |
| B-5 | DNxHD 与 WMV が backend 依存で別コンテナに化ける。pipe backend では libx264 に落ちる | 2 | 修正済み | `ArtifactRenderQueueEncoder.cppm:380-388,417-419` / `FFmpegEncoder.Helpers.cppm:86-92` |
| B-6 | grid snap が無い。10 フレーム丸めは Shift 押下時のみ | 3 | 修正済み | `ArtifactTimelineTrackPainterView.cppm:3574-3606` |
| B-7 | Motion path 的軌跡編集は viewport のみ。timeline 側が無い | 3 | 未着手 | `ArtifactCompositionMotionPathCommands.cppm` のみ |
| B-8 | 値的数値編集は QInputDialog モーダルか area ドラッグのみ。dope sheet 的セル編集は不可 | 3 | 未着手 | `ArtifactTimelineTrackPainterView.cppm:6081-6175` |
| B-9 | グループコンテナの clip バー自体のトリムが明示的に無効化されている | 3 | 設計変更が必要 | `ArtifactTimelineTrackPainterView.cppm:7933-7940`。コンテナは layer ではなく CompositionNode で长さは子の union 派生源。データモデル追加と専用 Undo が必要 |
| B-10 | ImageSequence は QImage を経由するため 16bit PNG も 8bit で入る | 4 | 未着手（設計が要る） | `ArtifactImageLayer.cppm:1319-1321` |
| B-11 | gif の frame delay が無視され、等間隔 24fps 扱いだった | 4 | 修正済み | `ImageSequenceSource.cppm:222-225,286-312,332-339` |
| B-12 | asdi cache が float32 展開で、8K なら 1 枚 1GiB。古いファイルは削除されない | 4 | 未着手（設計が要る） | `ArtifactImageLayer.cppm:751-764,809-832` |
| B-13 | OCIO で例外一出 config が null になる時、エラー通知なしに matrix へ落ちていた | 4 | 修正済み | `ArtifactOCIOManager.cppm:1111-1134` |
| B-14 | ArtifactCore 的 OCIOConfig 的ヘッダコメントが実装から 2 世代古い | 4 | 誤検知（既に削除済み） | `OCIOConfig.cppm` に該当コメントなし |
| B-15 | applyViewTransformToImage は呼び出し元ゼロだった | 4 | 意図的（コメント化） | `ArtifactOCIOManager.cppm:465-479`。GPU LUT 経路が正本のため繋ぐと二重適用 |
| B-16 | Footage 粒度の SourceInterpretOverride に consumer が無かった | 4 | 修正済み | `FootageInterpretService.cppm:202-286` |

---

## 2. 軸1: 再生の信頼性

### 確認できた良い設計

- RAM cache 的 request から render から readback 到 store 的閉ループ与 4 重バリデーションは正しく、LRU 128 frame も妥当（`PREVIEW_CACHE_SYSTEM_AUDIT_2026-08-08.md:130-146`）。
- 再生的 loop 境界は PlaybackService が環状ソートしており、末尾フレーム到先頭への重複や落ちは無い（`ArtifactPlaybackService.cppm:1174-1192`）。本稿的 grep で確認。
- 音声的 transport 是 PlaybackService が一元所有し、externalAudioClockProvider で外部クロックを差し込める（`ArtifactPlaybackService.cppm:192,886-901`）。二重管理ではない。

### 新たに判明した問題

**A2-1: startBuild 是同期ループ**。コメント自身が synchronous for now と明記している（`ArtifactRamPreviewController.cppm:174-232`、特に `:188`）。範囲内の全フレームを renderFn で同期レンダーするため、この間 UI は止まる。

**A2-2: FrameCache 是 Playback から完全に切断**。`ArtifactPlaybackService.cppm:185` に FrameCache module is disabled 的コメントアウトが実在する。VideoLayer が持つ frameCache 是レイヤー内のデコード済みフレーム用で、合成結果的 cache ではない。つまり合成結果的 RAM cache 是 Playback 経路に無い（`ArtifactVideoLayer.cppm:552,613`）。

**A2-3: 接続方式的記述が 2 文書で食い違う**（`PREVIEW_CACHE_SYSTEM_AUDIT:252-262` 是 Direct を問題視、`PLAYHEAD_PREVIEW_JITTER_AUDIT:48` 是 BlockingQueued を前提）。本稿的 grep では PlaybackService 内に goToFrame への直接接続を確認できなかった。未解決。

### プロとして見劣る点

loop 境界与音声 transport 的一元管理は良好。しかし合成結果的 RAM cache が Playback に無ため、連続再生でのフレーム再利用が機能していない可能性が高い。同じシーンを 5 回往復してタイミングを詰めるという基本動作が、毎回フル合成になっている可能性がある。

---

## 3. 軸2: レンダー出力

### フォーマット実列表

| フォーマット | 判定 | 備考 |
|---|---|---|
| ProRes 422 / 422 HQ / 4444 | OK | 修正済み。番号を FFmpeg 正解に統一 |
| EXR 32bit float | OK | zip 固定で UI に選択が無い（B-1） |
| EXR half | MISS | resolveWriteType が HALF を FLOAT に上げる（`ImageExporter.cppm:111-113`） |
| DPX | MISS | 出力側の実装がゼロ |
| H.264 / H.265 / VP9 / MJPEG | OK | 3 層とも揃う |
| DNxHD | OK | 修正済み。pipe backend で dnxhd encoder に写像 |
| WMV | OK | 修正済み。native 与 pipe 的両方で wmv2 に写像 |
| bit depth 選択 | MISS | S-6 |

### 最重量の問題

**A-10 が S 級に格上げされた理由**。自動採番は RenderMatrixDialog 的 outputPath に実装済みで UI にも上書きしないラベルがある。しかし ArtifactRenderQueueService 的 job 解決パスはこれ を呼ばない。ラベルを選んでも実際の job は同名 truncate し、実装済みの採番機能は死んでいる。

**A-11 の内訳**。Farm バックエンドは RPC サーバ、authToken、TLS、worker heartbeat、依存グラフ、優先順位 preemption、job テンプレ、queue 永続化、webhook alert と実装密度は高い。しかし useFarm が false の 1 行でレンダーから切断されている。起動 API は UI から一度も呼ばれず、外部プロセスからの API 直呼びのみ。

### プロとして見劣る点

ProRes 422 HQ 的指定が backend 選択次第で別の tier になり得る（修正済み）。DNxHD は pipe backend で H.264 に化けていた（修正済み）。指定したフォーマットが指定した codec で出ているという納品前チェックを保証できない状態は解消した。

---

## 4. 軸3: タイムライン編集

### 実装较好い部分（本稿的 grep で確認）

| 領域 | 実装 | 根拠 |
|---|---|---|
| 矩形キー選択 | ◎ | `ArtifactTimelineTrackPainterView.cppm:7922-7926,7977-8003` |
| 複数キーの一括移動 | ◎ | 同 `:7709-7758` |
| キーのコピペ（別 property、レイヤー、時刻、cross-fps、Undo 1 操作） | ◎ | `ArtifactTimelineWidget.cppm:12039-12330` |
| 反転、Mirror、Normalize、等間隔リタイム | ◎ | `ArtifactTimelineTrackPainterView.cppm:5272,5445,5535,5562` |
| Proportional editing（Blender 式、半径 adjust 可） | ◎ | 同 `:11456-11480,3117-3154` |
| Ripple Trim In、Out、Delete（Undo 1 操作にまとめられる） | ◎ | 同 `:1511,1438,1601,1667-1777` |
| レイヤー order 的 D&D（Undo と循環チェック付き） | ◎ | `ArtifactLayerPanelWidget.cppm:887-9098` |
| 親子付け替え（Pick Whip、循環チェック付き） | ◎ | 同 `:9101-9110` |
| Alt と矢印的一括 slide | ○ | `ArtifactTimelineTrackPainterView.cppm:11525-11589` |
| Prop channel filter（Transform、Audio、Effect） | ○ | 同 `:5047-5060,4080` |

### 新たに判明した問題

**A-1（sub-frame nudge 不在）が最重量**。矢印処理（`ArtifactTimelineTrackPainterView.cppm:11692-11733`）は stepFrames が Shift なら 10 でそれ以外 1 的 2 段のみ。fromFrame と toFrame が qint64 と llround で、キー的時刻がフレーム整数に丸められる。KeyFrame 的 time は RationalTime なので sub-frame 表現自体は可能だが、UI 経路が潰している。

**A-3（補間色分け）は修正済み**。keyframeInterpolationColor が type 分岐に変更された（`:3283-3322`）。補間種別ごとに gold 的輝度差を付け、Bezier を基準に、Hold が最も暗く、Bounce 系は中程度に暗い。labelColor は別系統のため干渉しない。

**A-4（Dope Sheet 読み取り専用）**。NoEditTriggers と NoSelection を設定し、isTransformPropertyPath のみ通過（`ArtifactDopeSheetWidget.cppm:77-78,136-139`）。Effect、Audio、Shape 的キーは一切出ない。

**A-5（一括 trim 不可）**。clip ドラッグは dragClipIndex 単一のみ追跡（`:7889`）。release も単一 index のみ発火（`:9138-9147`）。

**A-19（リタイム隔離）**。commitClipRetime は実在するが本 Timeline から到達不能。

### プロとして見劣る点

「1/2 フレームずらす」「数百プロパティの中の 1 キーを 1 秒で探す」「複数レイヤーを一括 trim」。この 3 つすべてが不可能。Graph Editor と Dope Sheet という keying ワークフローの柱のうち 2 本が無効化されている（補間色分けのみ本パスで解消）。

---

## 5. 軸4: カラー管理

### 新たに判明した問題

**S-5 は本稿的 grep で確認した**。`ArtifactVideoLayer.cppm` に OCIOManager、applyInputTransform、setColorDescriptor、colorDescriptor、inputColorSpace 的識別子が 1 件も無い。ImageLayer のみに OCIO 実装があり（`ArtifactImageLayer.cppm:1035-1036,2141-2160`）、Video は完全に無変換。

**S-7（clip による HDR 無効化）**。readbackToImage は Format_RGBA8888 で作成し、sanitize で clamp(0,1) する（`ArtifactIRenderer.cppm:2251-2297`）。RenderQueue 的通常出力は readbackToImageF32 から toQImage 的経路を通るため、AOV 以外で HDR が clip される。

**A-14（preset で matrix に落ちる）**。setActivePreset が ocioConfig を reset してから preset を選ぶ（`ArtifactOCIOManager.cppm:148-194`、特に `:150`）。その後の config は createACESConfig というハードコード文字列リストで、実 OCIO オブジェクトではない。実 processor は ocio ファイルを読込んだプロジェクトでのみ動く。

**A-15（proxy 色の無察知）**。proxy 生成的 ffmpeg に color_primaries と colorspace が無い（`ArtifactVideoLayer.cppm:2354-2374`）。usingProxy は bool 制御で、proxy と full 的切替時に color descriptor が更新されない。

**A-17（連番 fps 24 固定）は修正済み**。requestedFrameRate を追加し、openFramePaths 的 3 箇所の再代入が明示指定値を保持するようになった（`ImageSequenceSource.cppm:67,71,316,367,395,695`）。

### 既存分析との差分

既存の「OCIO 実装済み・parity pending」は実装的存在としては正しいが、本稿的 grep は接続的欠落を明示した。OCIOConfig はメタデータコンテナであって変換器ではなく、実変換は ArtifactOCIOManager 側だけに存在する、という二重構造が既存の記述では明確になっていなかった。

### プロとして見劣る点

Rec.709 的映像を VideoLayer で読むと彩度がそのまま飛ばして納品される。HDR 編集も clip で無効化される。OCIO 的 preset を選ぶと matrix に落ち、ocio ファイルを読み込まない限り実変換が走しない。

---

## 6. 修正推奨

### 本パスで修正済み

| # | 修正 | 影響範囲 |
|---|---|---|
| S-1 | `FFmpegEncoder.cppm:292-303` の proresProfile 番号を AVPRORES_PROFILE_* 定数に変更。Native と Pipe 側の対応表を統一。4444XQ 対応も追加 | 1ファイル |
| B-5 | `normalizeCodecName` に dnxhd、wmv2、rawvideo を追加。`ffmpegPipeEncoderName` に各 encoder 名を追加。`codecNameToId` に AV_CODEC_ID_DNXHD と AV_CODEC_ID_WMV2 を追加 | 2ファイル |
| A-3 | `keyframeInterpolationColor` を InterpolationType 的 switch に変更。gold 輝度差で補間種別を識別可能に | 1ファイル |
| A-17 | `ImageSequenceSource::Impl` に requestedFrameRate を追加。openFramePaths 的 3 箇所の frameRate 再代入を、明示指定値を保持する形に修正 | 1ファイル |
| S-3 | `RenderQueueJob` に `startTimeCodeFrame`（long long、-1 で無効）と `dropFrame` を追加。`buildNativeVideoSettings` から encoder settings へ供給。native backend の既存 timecode 埋込が有効化。pipe backend に `-timecode` を追加（ProRes のみ）。Composition から job を作る2経路で `startTimeCode()` と `hasDropframe()` を引き継ぎ。JSON 保存・復元に追加 | 4ファイル |
| S-4 | UI の「タイムコード: ソース準拠」表示の下に `QTimeEdit`（HH:mm:ss.ff）と drop-frame チェックボックスを追加。`TimeCode::toHMSF` を使い 29.97 / 59.94 の nominal rate を正しく扱う。service に getter/setter（`jobStartTimeCodeFrameAt` / `jobDropFrameAt` / `setJobTimeCodeAt`）を追加し、ManagerWidget から接続 | 4ファイル |
| S-5 | `ArtifactVideoLayer` に OCIO 入力変換を実装。unpremultiply → `applyInputTransformToWorkingImage` → premultiply の順（ImageLayer と同一）。`currentFrameBuffer_` への代入6経路すべてに適用し、キャッシュはソース値のまま保持。`video.inputColorSpace` / `video.inputTransferFunction` を Property と JSON に追加し、`setInputColorSpace` でキャッシュを無効化 | 2ファイル |
| S-6 | `resolveWriteType` の HALF→FLOAT 無条件昇格を削除し HALF をそのまま尊重。`RenderQueueJob` に `bitDepth`（8/16/32）を追加し `bitDepthToTypeDesc` で OIIO 型に写像。UI に選択 combo を追加。EXR/TIFF 以外で 16/32bit を要求した場合は preflight で警告 | 4ファイル |

### 未着手（設計変更が必要）

| # | 修正 | 理由 |
|---|---|---|
| A-7 | スナップ on/off トグル | 設定基盤（LayeredConfigStore）への永続化が要る |
| A-8 | 名前付きガイド | データモデル・描画・編集 UI 的新規設計が要る |
| A-9 | DPX writer | 新規フォーマット実装が要る |
| A-10 | 自動採番の配線 | job 解決経路の変更が要る |
| A-11 | useFarm 的 runtime 化 | Farm 起動 UI 的新規追加が要る |
| A-12 | GPU 並列レンダー | レンダーアーキテクチャ的変更が要る |
| A-16 | FootageInterpretService の配線 | 既存レイヤーへの push 経路の追加が要る |
| A-18 | 複数レイヤー合計メモリ予算 | 動的予算機構の新設が要る |

### S 級（すべて本パスで修正済み）
S-1 から S-6 まで、S 級 6 項目すべてを本パスで修正済み。
---

## 7. 未確認事項（ビルド・実機未実施）

- 実コードで確認した内容は compile 後の動作を保証しない。特に module 境界と template instantiation。
- S-1 的修正は、出力が実際に正しい tier になるかは実機比較が必要。静的には AVPRORES_PROFILE_* 定数との一致のみ確認済み。
- B-5 的修正は、pipe backend が実際に正しい encoder を起動するか実機未確認。
- A-3 の修正は、色の視認性が実際に改善するか実機未確認。
- A-17 の修正は、setFrameRate の呼び出し側が意図した fps で動作するか実機未確認。
- S-3 / S-4 の修正は、Timecode が実際にコンテナに焼き込まれるか実機未確認。pipe backend は ProRes のみ `-timecode` を渡しており、他のコンテナ（MXF、MP4）は未対応。
- QTimeEdit はフレーム単位を持たないため HH:mm:ss.ff 形式で ff/100 ミリ秒として表現している。29.97 / 59.94 では 1 フレームがミリ秒に正確に変換できないため、往復時に丸め誤差が出る可能性。実機確認が必要。
- S-5 の修正は、VideoLayer に inputColorSpace_ が未設定（既定空）の間は完全に no-op なので既存プロジェクトの見た目は変わらない。ただし Rec.709 の映像に Rec.709 を指定した場合に彩度が正しく解釈されるかは実機未確認。applyInputColorTransform は毎フレーム 6 経路で走るため、ホットパス上のコストは未計測。
- S-6 の修正は、`bitDepth` が 8 のときは従来通り。EXR/TIFF 以外（PNG、BMP、JPEG）で 16/32bit を要求すると preflight 警告を出して 8bit で書く。警告が実際に表示されるか実機未確認。16bit EXR が 16bit で書かれるかは OIIO plugin の挙動次第。
- B-3 / B-4 の atomic write は、Windows の rename が同一ボリューム内で atomic であることを前提とした「退避してから置き換える」方式。ネットワークドライブやFATでは rename 自体が非 atomic であり、実機確認が必要。
- B-6 の grid snap はルールの 1/2/5 の major step に合わせた。明示的なスナップ先（playhead、work area、他 clip の in/out、キー）があるときはgrid より優先されるため、grid に乗らないケースがある。的操作感は未確認。
- B-11 の gif frame delay は QImageReader の imageDuration を使う。gif plugin によっては delay が 0 を返す場合があり、その場合は 100ms にフォールバックする。実 gif での検証は未実施。
- B-9 は「結線だけで済む)에分類したが、調査の結果コンテナが CompositionNode であり長さが派生値のため、データモデル追加と専用 Undo が必要と判明した。したがって本パスでは未実装。
- 既存分析が指摘する 30 項目（markFrameRequested 的不整合、premultiply と sRGB 的順序、MarkGpuDataDirty 的呼び出し元ゼロ等）は本稿的軸外のため再検証していない。

---

## 8. 関連文書

- `PREVIEW_CACHE_SYSTEM_AUDIT_2026-08-08.md` — RAM cache 基盤（軸1）
- `PLAYHEAD_PREVIEW_JITTER_AUDIT_2026-09-08.md` — 再生ジッタ（軸1）
- `COLOR_PIPELINE_AUDIT_2026-08-02.md` — カラー基盤（軸4）
- `IMAGE_BUFFER_PRECISION_AUDIT_2026-08-13.md` — 画像精度（軸4）
- `IMAGE_PIPELINE_AUDIT_2026-08-02.md` — EXR と Cryptomatte（軸2）
- `MILESTONE_RENDER_QUEUE_2026-03-22.md` — レンダーQueue 実装
- `MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-29.md` — タイムライン
