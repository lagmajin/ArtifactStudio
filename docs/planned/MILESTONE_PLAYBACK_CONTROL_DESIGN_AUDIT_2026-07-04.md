# マイルストーン: 再生コントロール 機能監査 (2026-07-04)

> 作成: 2026-07-04 / 更新: 2026-07-04 (JKL競合解決済み)
> 元依頼: 「再生とか停止をコントロールするウィジェットの提案も」
> 
> **訂正**: JKLシャトル（J=逆再生,K=停止,L=再生）は `ArtifactTimelineWidget.cppm:7150` に実装済み。
> キーフレームジャンプ(J/K)と競合していたため、AnimationGoToNextKeyframe/PreviousKeyframeをCtrl+Shift+J/Kに移動。
> 連打倍速(JJ=2x)とK+Lスロー再生は未実装。

## 監査サマリー

Artifact の再生コントロールは `ArtifactPlaybackControlWidget`（play/pause/stop/step/seek/loop/speed/InOut）＋ `ArtifactPlaybackInfoWidget`（フレーム情報表示）＋ `ArtifactTimelineWidget`（シーク/スクラブ）＋ `ArtifactPlaybackService`（再生エンジン）で構成。基本操作は揃っているが、AE/Premiere/Resolve/DAW レベルのトランスポート機能と比較すると以下の不足がある。



---

## 🔴 P0: JKL シャトル・ジョグ

AE / Premiere / Resolve / Avid / Final Cut の全 NLE が採用する標準的な再生文法。JKL がないツールはプロ用とは言えないレベル。

| 機能 | 参照元 | 状態 |
|---|---|---|
| **JKL シャトル（J=逆再生/K=停止/L=再生）** | Premiere/Resolve/Avid | ✅ TimelineWidget:7150 実装済み |
| **J/L 連打で倍速（JJ=2x, JJJ=4x, LLL=8x）** | Premiere/Avid | ✅ 2026-07-04 実装（500ms 以内の連打で shuttleForward/Reverse の倍速が発動） |
| **K+L でスロー再生（1/2x）** | Premiere/Avid | ✅ 2026-07-04 実装（K 押下後 300ms 以内の L で 0.5x 再生） |
| **K+J で逆スロー再生** | Premiere/Avid | ✅ 2026-07-04 実装（K 押下後 300ms 以内の J で -0.5x 再生） |
| **Jog Wheel（ジョグダイヤル）UI** | Resolve/Premiere | ❌ マウスホイールやダイヤルで1フレーム単位の精密シーク |
| **Shuttle Speed Slider** | Resolve/Premiere | ❌ ドラッグで可変速再生するスライダー |
| **Shuttle の感度カーブ設定** | Resolve | ❌ スライダー中央付近の感度を調整 |

---

## 🔴 P0: RAM プレビュー・キャッシュ制御

| 機能 | 参照元 | 状態 |
|---|---|---|
| **RAM Preview（Numpad 0）** | AE | ⚠️ 部分的。再生はあるが RAM プレビュー専用の範囲＋品質制御が未分化 |
| **RAM Preview 範囲（Work Area 内のみ）** | AE | ⚠️ WorkAreaControlWidget はあるが再生範囲として使う導線が弱い |
| **RAM Preview キャッシュ済み表示（緑バー）** | AE | ✅ ScrubBar で実装済み |
| **キャッシュフレーム数/合計フレーム数表示** | AE | ❌ |
| **キャッシュクリアボタン** | AE | ✅ clearRamPreviewButton_ 実装済み |
| **Purge Memory（メモリ解放）メニュー** | AE | ❌ Image/Video/Audio/All を選択してキャッシュクリア |
| **キャッシュインジケーター（再生中に緑/赤/黄色）** | Premiere/AE | ❌ リアルタイム再生可否の色分け表示 |
| **Dropped Frames 表示** | Premiere/AE | ❌ |
| **Dropped Frames 許容値設定** | AE | ❌ |

---

## 🟡 P1: 再生モード・範囲

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Play from Playhead（現在地から再生）** | AE | ⚠️ |
| **Play from Start（先頭から再生）** | Premiere | ❌ |
| **Play Around Current（現在地の前後 N 秒再生）** | AE | ❌ |
| **Play In to Out（In-Out 範囲の再生）** | AE/Premiere | ⚠️ |
| **Preroll / Postroll 再生** | Premiere/Avid | ❌ 編集点の前後 N 秒を自動再生して確認 |
| **Reverse Playback（逆再生ボタン）** | Resolve/Premiere | ❌ |
| **Ping-Pong Loop（往復ループ）** | AE/Blender | ❌ In→Out→In の往復再生 |
| **Single Frame Play（1フレームだけ再生して停止）** | - | ❌ ボタン押下で1フレーム進んで停止 |
| **Skip Frames（再生時コマ落とし）** | AE | ✅ playbackSkipCombo_ 実装済み |
| **再生解像度ドロップダウン（Full/Half/Quarter/Third）** | AE | ⚠️ Draft 切替のみ |

---

## 🟡 P1: フレーム移動・ナビゲーション

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Step Forward/Backward** | 全アプリ | ✅ 実装済み |
| **Shift+Step（5/10フレームジャンプ）** | Blender/Maya | ✅ 2026-07-04 実装（Shift押しながらStepボタンで5フレーム移動） |
| **Go to Frame（Ctrl+G）** | AE | ⚠️ |
| **Go to Time（タイムコード直接入力）** | Premiere/Resolve | ❌ |
| **Go to In / Go to Out** | Premiere | ❌ |
| **Next/Previous Keyframe（J/K キーフレームジャンプ）** | AE/Premiere | ❌ |
| **Next/Previous Marker（Shift+J/K）** | Premiere | ❌ |
| **Next/Previous Edit Point（; / ' キー）** | Premiere/Avid | ❌ クリップ境界へジャンプ |
| **Home/End（先頭/末尾）** | AE/Premiere | ✅ SeekStart/SeekEnd 実装済み |

---

## 🟡 P1: 時間表示・情報パネル

| 機能 | 参照元 | 状態 |
|---|---|---|
| **タイムコード表示（SMPTE HH:MM:SS:FF）** | AE/Premiere/DAW | ⚠️ |
| **タイムコードのクリック直接入力** | AE/Premiere | ✅ 2026-07-04 実装（表示クリック→HH:MM:SS:FF入力→goToFrame） |
| **フレーム番号表示** | AE | ⚠️ |
| **表示モード切替（SMPTE/Frame/Seconds/Feet+Frames）** | AE/Premiere | ❌ |
| **現在フレーム / 総フレーム 表示** | Premiere/DAW | ❌ 「120 / 3600」形式 |
| **フレームレート表示** | Premiere | ❌ |
| **再生速度表示（0.5x / 1x / 2x）** | Premiere | ⚠️ 内部はあるが表示は弱い |
| **Dropped Frames カウンター** | Premiere/AE | ❌ |
| **オーディオレベル簡易メーター** | Premiere | ❌ |

---

## 🔵 P2: オーディオ・スクラブ制御

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Audio Scrubbing（スクラブ時音声再生）** | AE/Premiere | ✅ AudioScrubController 実装済み |
| **Mute During Preview トグル** | AE | ✅ 2026-07-04 実装（チェックボックス→setAudioMasterMuted） |
| **Solo Audio Track** | Premiere/DAW | ❌ 1トラックだけ音声再生 |
| **Audio Waveform スクラブプレビュー** | Premiere | ❌ |

---

## 🔵 P2: 特殊モード・拡張

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Record / Auto-Key トグル** | AE/Blender/Maya | ✅ 2026-07-04 実装（PlaybackControlWidgetにチェックボックス追加、isAutoKeyEnabled() API） |
| **Play in Editor（PIE）モード切替** | Unity/Unreal | ❌ Play / Simulate / Stop の3状態 |
| **Punch In/Out マーカー** | DAW/Avid | ❌ 指定範囲の上書き再生記録 |
| **Metronome / Click Track** | DAW | ❌ BPM 同期のクリック音 |
| **Sync Lock（他トラックとの同期ロック）** | Premiere/Avid | ❌ |
| **プレイバックループのフェードイン/アウト** | - | ❌ ループ再生のつなぎ目をクロスフェード |
| **MIDI Clock / MTC 同期** | DAW/Resolve | ❌ 外部 MIDI 機器との時間同期 |
| **タイムラインスクロール同期 ON/OFF** | Premiere | ❌ Page Scroll / Continuous Scroll 切替 |
| **プレイバックプリセット保存** | AE | ❌ 再生設定（範囲/解像度/スキップ）のテンプレート保存 |

---

## 📊 優先度マトリクス

| 優先 | カテゴリ | 件数 | 代表機能 |
|---|---|---|---|
| 🔴 | JKL シャトル | 7 | JKL 文法/Jog Wheel/Shuttle Slider |
| 🔴 | RAM プレビュー | 8 | 範囲プレビュー/キャッシュクリア/ドロップフレーム表示 |
| 🟡 | 再生モード | 10 | Preroll/Reverse/Ping-Pong/Skip Frames |
| 🟡 | フレーム移動 | 6 | Go to Time/キーフレームジャンプ/マーカージャンプ |
| 🟡 | 時間表示 | 8 | タイムコード入力/モード切替/フレームレート |
| 🔵 | オーディオ | 4 | Mute During Preview/Solo Track |
| 🔵 | 特殊モード | 8 | Auto-Key/Punch In/MIDI同期/プリセット |

---

## 関連文書

- `docs/done/PLAYBACK_CONTROL_WIDGET_REFACTOR.md` — リファクタ完了メモ
- `docs/done/MULTITHREADED_PLAYBACK_ENGINE.md` — マルチスレッド再生エンジン
- `docs/planned/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md` — RAM プレビューシステム
- `docs/planned/MILESTONE_AUDIO_SCRUBBING_2026-06-16.md` — 音声スクラブ
- `Artifact/include/Widgets/Control/ArtifactPlaybackControlWidget.ixx` — インターフェース
- `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm` — メイン実装
- `Artifact/include/Service/ArtifactPlaybackService.ixx` — 再生サービス
- `docs/WIDGET_MAP.md` — ウィジェット責務
以下、AE / Premiere / Resolve / Blender / Maya / DAW（Ableton/Logic/Pro Tools）/ Unity / Unreal / Avid / Final Cut / VLC の **12 アプリ群**から収集。

## 2026-07-25 実装監査

再生／停止／step／seek／loop／speed／In-Out、PlaybackClock の負方向再生・loop・drop-frame counter、JKL と連打倍速／スロー操作、ScrubBar の cache 表示・clear は確認した。一方、Jog Wheel／Shuttle Slider、RAM preview の専用範囲・品質制御、Purge Memory、Dropped Frames UI、Ping-Pong／preroll／postroll、timecode 直接入力、marker／edit-point ジャンプ、MIDI／MTC 同期、再生プリセットは未確認または未実装である。基本トランスポートは部分実装、設計監査の全項目は未完了・runtime 未検証とする。
