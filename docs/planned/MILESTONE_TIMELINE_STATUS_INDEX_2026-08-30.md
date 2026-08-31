# Timeline Planned Milestone — Status Index (2026-08-29)

**最終更新:** 2026-08-30

> **このファイルは 2026-08-29 作成版の 2026-08-30 更新版です。** 元ファイル [MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md](MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md) は別物として残してあります。

タイムライン系 planned マイルストーンを、2026-08-15 時点の Update / Progress 節を基準に「現状ステータス」で棚卸ししたインデックス。

姉妹文書 [MILESTONE_TIMELINE_INDEX_2026-04-22.md](MILESTONE_TIMELINE_INDEX_2026-04-22.md) は「役割ごとの入口」を目的とするので併用する。
姉妹文書 [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md) と [MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md) は本インデックスからリンクされる 2026-08-30 更新版。

## 判定基準

| 区分 | 判定基準（本ファイル内で一貫適用） |
|---|---|
| ✅ 実装済み相当 | Update に「実装済み」「主要経路まで完了」「主要目標達成」「コード移行は完了相当」と明記 |
| 🟡 部分実装 | Update に「部分実装」「主要基盤実装済み、〜は未完了」「基盤は進んだが未完了」「実装済み、〜は未検証」 |
| ⬜ 未着手 | Update なし / 進捗セクションが短い / 「着手候補」止まり / Phase 1 から着手予定のみ |
| 🔍 検証のみ | Update に「runtime 受入れ未」「実機確認待ち」「性能受入れ未」と中心、コード変更は完了 |
| ↪ SUPERSEDED | 冒頭注記で他文書に吸収済み。吸収先リンクを併記 |

## 一次表（ステータス別）

### ✅ 実装済み相当

| マイルストーン | 主な根拠（2026-08-15 Update / Progress より） |
|---|---|
| [MILESTONE_TIMELINE_QGRAPHICSSCENE_ELIMINATION_2026-03-31.md](MILESTONE_TIMELINE_QGRAPHICSSCENE_ELIMINATION_2026-03-31.md) | 「`ArtifactTimelineTrackPainterView` が正規経路になり、`TimelineScene` / `ClipItem` の互換層は退役済み」「右ペインについては主要目標を達成」 |
| [MILESTONE_TIMELINE_RIGHT_PANE_FULL_OWNER_DRAW_2026-04-08.md](MILESTONE_TIMELINE_RIGHT_PANE_FULL_OWNER_DRAW_2026-04-08.md) | 「右ペインの生成・描画・seek・clip/keyframe操作が `ArtifactTimelineTrackPainterView` を正規経路」「旧 `TimelineScene`／`ClipItem` のソースが残っていない」 |
| [MILESTONE_TIMELINE_TRACKVIEW_OWNER_DRAW_MIGRATION_2026-03-27.md](MILESTONE_TIMELINE_TRACKVIEW_OWNER_DRAW_MIGRATION_2026-03-27.md) | 「`ArtifactTimelineTrackPainterView` がタイムライン右ペインの正規 surface」「コード移行は完了相当」 |
| [MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md](MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md) | `Status: Archived reference`。本体は `docs/done/` 側。TrackPainterView に peak/RMS の owner-draw 実装済み |

### 🟡 部分実装

| マイルストーン | 完了範囲 | 未完了 / 未検証 |
|---|---|---|
| [MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md](MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md) | Phase 1〜5 の主要基盤は実装済み（current/selected/playhead 同期、keyframe 編集、search、visual language、owner-draw への寄せ） | 「全操作の受入れ、owner-draw 完全移行、性能・runtime 検証は未完了。ビルド・テストは未実施」 |
| [MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md](MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md) | インクリメンタル検索、表示モード、hit count、結果ナビゲーション、主要な簡易 query、child relation、source asset 検索は実装済み | 「選択・keyframe・scroll との回帰確認は未完了」 |
| [MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03.md](MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03.md) | Phase 1〜3 着手（header 切替、default `Keyframes Only`、selected-layer 自動展開） | 「Phase 4 still remains for broader property coverage and more AE-like lane granularity」 |
| [MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md](MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md) | 種別色、選択／hover の派生色、keyframe の interpolation／easing／color label、レーン分割、playhead overlay、左パネルの active border が主要経路まで実装済み | 「共通 token/helper への整理、色覚差・light/dark theme を含む実機回帰、状態形状の横断統一」 |
| [MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md](MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md) | Phase 1（keyframe marker の click modifier／矩形選択／batch move、clip 選択の modifier 伝播）は実装済み | 「rubber-band の最終 UX、plain scroll と Ctrl+Scroll の厳密な役割固定、復帰 shortcut、inline property editor、複数レイヤーの snap／collision 統一、timeline→Inspector 往復強調、ripple edit の一操作 Undo」 |
| [MILESTONE_TIMELINE_INOUT_SLIDE_2026-06-16.md](MILESTONE_TIMELINE_INOUT_SLIDE_2026-06-16.md) | Slide mode/tool、mouse drag、apply、Undo、Keyboard/batch は実装済み | 「source 境界、複数選択の 1 undo、保存／再読込、キーフレーム追従の runtime parity 確認、clamp 通知と UI 表記」 |
| [MILESTONE_TIMELINE_SCRUBBAR_FRAME_CACHE_OVERLAY_2026-04-10.md](MILESTONE_TIMELINE_SCRUBBAR_FRAME_CACHE_OVERLAY_2026-04-10.md) | Cache Range Contract、overlay rendering、playback/cache bridge、empty-state は主要経路まで完了 | 「再生・停止・seek 中の実機表示と長時間更新時の性能受入れ」 |
| [MILESTONE_TIMELINE_TOOLTIPS_2026-04-10.md](MILESTONE_TIMELINE_TOOLTIPS_2026-04-10.md) | キーフレーム／編集面の tooltip、keyframe area／clip／drag 中 tooltip が実装済み | Layer bar ツールチップ、コンポジション領域ツールチップ、設定 ON/OFF 項目、テーマ対応、性能回帰 |
| [MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md](MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md) | Phase 1 完了（種別識別、共通 descriptor、表示モード）。Phase 2 は Audio 波形まで実装、Video/Text/Shape/Particle/Image/3D/Camera は補助ラベル実装 | 「Phase 2 の Audio フェード／オートメーション、Phase 3 Video サムネイルストリップ、Phase 4 Text/Shape/Image/Particle 専用トラック」 |
| [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md) | Phase 1 の主要 painter で AA / TextAA / SmoothPixmapTransform を明示的に有効化。固定幅フォントは OS の FixedFont 解決へ移行。右ペイン frame grid は zoom 適応密度。timecode を playhead 同期。既存 status setter は常設ラベルへ保持し、選択数も既存 selection event から常設表示 | theme固有フォント token、playhead の status 常時供給、ruler unit、runtime目視確認は未完了 |
| [MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md](MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md) | TrackPainterView に Ctrl+wheel の現在位置中心ズーム、Shift の水平 pan、Alt 併用の垂直ズーム、設定可能な zoom shortcut が実装済み | status bar の常時 zoom 表示、単位切替、保存／再読込、runtime操作確認は未完了 |
| [MILESTONE_TIMELINE_DILIGENT_GPU_SURFACE_2026-08-29.md](MILESTONE_TIMELINE_DILIGENT_GPU_SURFACE_2026-08-29.md) | opt-in GPU preview、immutable snapshot、visible row / clip / keyframe のDiligent表示、初期化失敗時の既存timeline復帰 | glyph atlas、waveform / thumbnail texture、CPU/GPU分離計測、D3D12/Vulkan実機・device loss受入 |
| [MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md](MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md) | Historical Plane frames の zoom / pan 復元を局所 RAII 化。旧コメントアウト経路を削除。Motion Path は layer 別・frame・zoom・overlay serial cache へ拡張 | canvas / external matrices の復元契約、描画経路統合、theme、batch、runtime検証 |
| [MILESTONE_LAYER_EFFECT_WIPE_SLIDE_DISSOLVE_ZOOM_2026-08-30.md](MILESTONE_LAYER_EFFECT_WIPE_SLIDE_DISSOLVE_ZOOM_2026-08-30.md) | AE の Wipe 5 個 + Slide/Dissolve/Zoom 8 個、計 13 effect を LinearWipe 雛形で追加し AE Transitions カテゴリ 4 系統 18 個中 14 個を揃える | Inspector / Undo / JSON 連携、color token 化、CMake 登録、runtime 受入れ |
| [MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md) | readback呼び出しを export / offscreen / explicit viewport / adjustment fallback に静的分類 | adjustment fallbackのGPU化、実計測、event・selection・property各Phase |

### ⬜ 未着手

| マイルストーン | 着手の手がかり |
|---|---|
| [MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md](MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md) | P0 着手点として「J/K キーフレームジャンプ」「選択キーフレーム情報ボックス」「Easy Ease / In / Out 適用前提の固定」「Selected / Hovered / Current-frame-hit と keyframe 色の役割分離」を列挙 |

### 🔍 検証のみ

| マイルストーン | 残検証 |
|---|---|
| [MILESTONE_TIMELINE_INOUT_SLIDE_2026-06-16.md](MILESTONE_TIMELINE_INOUT_SLIDE_2026-06-16.md) | 実装完了相当だが source 境界／複数選択保存復元／runtime parity は未検証（部分実装と重複記載） |

## SUPERSEDED マップ（吸収済み、参照用）

| SUPERSEDED 元文書 | 吸収先 |
|---|---|
| [MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md](MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md) | [MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md](MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md) |
| [MILESTONE_TIMELINE_COLOR_KEYFRAMES_2026-06-05.md](MILESTONE_TIMELINE_COLOR_KEYFRAMES_2026-06-05.md) | [MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md](MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md) |
| [MILESTONE_TIMELINE_KEYFRAME_AREA_EDITING_2026-06-15.md](MILESTONE_TIMELINE_KEYFRAME_AREA_EDITING_2026-06-15.md) | [MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md](MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md) |
| [MILESTONE_TIMELINE_KEYFRAME_CONNECTIONS_2026-06-05.md](MILESTONE_TIMELINE_KEYFRAME_CONNECTIONS_2026-06-05.md) | [MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md](MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md) |
| [MILESTONE_TIMELINE_PROPORTIONAL_KEYFRAME_EDITING_2026-07-06.md](MILESTONE_TIMELINE_PROPORTIONAL_KEYFRAME_EDITING_2026-07-06.md) | [MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md](MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md) |
| [MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md](MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md) | [MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md](MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md) |
| [MILESTONE_TIMELINE_CURVE_EDITOR_MODE_PHASE1_EXECUTION_2026-04-10.md](MILESTONE_TIMELINE_CURVE_EDITOR_MODE_PHASE1_EXECUTION_2026-04-10.md) | [MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md](MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md) |
| [MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03_EXECUTION.md](MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03_EXECUTION.md) | [MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03.md](MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03.md) |
| [MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md](MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md) | [MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md](MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md) |
| [MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03_EXECUTION.md](MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03_EXECUTION.md) | [MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md](MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md) |
| [MILESTONE_TIMELINE_WAVEFORM_2026-04-10.md](MILESTONE_TIMELINE_WAVEFORM_2026-04-10.md) | [MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md](MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md) |

## 着手の目安（今この瞬間に 1 つ選ぶなら）

- **コード規模 小〜中、操作規則を 1 組確定**：
  [MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md](MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md) Phase 2A（plain scroll = pan、`Ctrl+Scroll` = zoom、復帰 shortcut、zoom 中の現在位置視認性）
- **未着手のまま残っている具体着手点**：
  [MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md](MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md) Phase 1A（修飾キー別の wheel ズーム、ステータスバー `%` 表示）
- **監査ベースの P0 を 1 個ずつ**：
  [MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md](MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md) P0A（J/K ジャンプ → 選択 keyframe 情報ボックス → Easy Ease 適用前提 → 色役割分離）
- **DCC 感ギャップの演出層を 1 Phase ずつ**：
  [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md) Phase 1（AA baseline）— 主要 painter の `Antialiasing` / `TextAntialiasing` / `SmoothPixmapTransform` を helper 経由 ON、固定幅フォントを OS の `FixedFont` 解決へ置換
- **検証のみで手離れ**：
  [MILESTONE_TIMELINE_INOUT_SLIDE_2026-06-16.md](MILESTONE_TIMELINE_INOUT_SLIDE_2026-06-16.md) の source 境界・複数選択 1 undo・保存／再読込・runtime parity 確認

## 使い方

- 着手テーマを選ぶときは、まず ✅／🟡／⬜／🔍 の区分で範囲感を掴む
- SUPERSEDED マップは「以前どこへ向かっていたか」の参照用。新規着手先には含めない
- 既存の役割別インデックス [MILESTONE_TIMELINE_INDEX_2026-04-22.md](MILESTONE_TIMELINE_INDEX_2026-04-22.md) と併用する（本書はステータス棚卸しが目的）
- 判定は 2026-08-15 までの Update / Progress 節のみを根拠とする。コード検索は含めない（AGENTS.md に従いビルド・テストは AI 側で実行しないため）

- **操作感・安定性を 1 Phase ずつ**：
  [MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md) Phase 1（GPU readback 排除）— eadbackToImage() の呼び出し点を grep で全列挙し、export / debug dump / 明示 API のみ true のフラグで gating
- **VP モーションパスを 1 Phase ずつ**：
  [MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md](MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md) Phase 1（死蔵コード削除）— L35704-L35800+ の //  付き 170 行+ を静的検査後物理削除、Frame Debug で 'MotionPath' scope 1 フレーム 1 回を確認
- **レイヤー effect トランジション追加を 1 Phase ずつ**：
  [MILESTONE_LAYER_EFFECT_WIPE_SLIDE_DISSOLVE_ZOOM_2026-08-30.md](MILESTONE_LAYER_EFFECT_WIPE_SLIDE_DISSOLVE_ZOOM_2026-08-30.md) Phase 1（RadialWipeEffect）— LinearWipe ディレクトリを雛形に copy して中心座標 + 円弧判定に差分編集、Artifact/CMakeLists.txt の GLOB または force list に登録
