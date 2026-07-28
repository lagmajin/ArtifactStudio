# AE・他DCC 機能差ギャップ最新レポート — 2026-07-28

**作成日:** 2026-07-28
**目的:** `REPORT_AE_GAP_UPDATE_2026-07-03.md`（07-08 訂正含む）と `REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` を基準に、2026-07-03〜07-28 の実装進捗を反映した最新スナップショットを提供する。
**調査方法:**
- git ログ走査: Artifact 399 コミット / ArtifactCore 155 コミット（2026-07-03 以降）
- ソース直接検証: 30 項目について `Artifact/src`, `Artifact/include`, `ArtifactCore/src`, `ArtifactCore/include` を実コード確認（docs のみの言及は未実装扱い）

**凡例:** ✅ 実装あり / 🟡 部分実装 / ❌ 未実装 / ⚠️ 未検証（ログ言及のみ・実体未確認）

---

## 1. エグゼクティブサマリ

この 3.5 週間で **旧レポートで「未着手」とされた項目の多くが実装済みまたは部分実装に転じた**。特に大きいのは:

1. **OCIO / ACES / スコープ / LUT** — 06-16 時点「全て 0 hit」→ OCIOManager + Color Science Panel + Vector/Waveform/Parade スコープ + .cube 書き出しまで実装
2. **Audio Scrubbing / Adjustment Layer / Track Matte（データ+GPU）/ Auto-Orient / Time Remap** — AE P0/P1 ギャップの大半が着手済みに転換
3. **Shape レイヤーの大規模強化** — 10 種のシェイプオペレーター + ネイティブベジエ描画パイプライン（07-27 に Qt 依存を外した native tessellation へ移行）
4. **連番画像ワークフロー** — Asset Browser のシーケンス対応（07-27 集中実装）+ ImageSequenceSource（Core）+ レイヤー統合（07-28、本日 main 実装済・ビルド未検証）
5. **独自の強み領域が拡大** — Animation Layers（Maya 風）、Pointwise Effect Fusion（IR→HLSL codegen）、GI(SSGI/DDGI) contracts、OpenVDB→Pyro ブリッジ、OTIO adapter、プロキシワークフロー

一方、**字幕/キャプション、planar tracker、Loudness meter、Plugin Manager UI、AEP/PSD/Lottie import、チーム制作機能** は引き続き未実装。

---

## 2. AE ギャップ更新表（対 2026-07-03 レポート）

| # | 領域 | 07-03 状態 | 07-28 状態 | 根拠 |
|---|------|-----------|-----------|------|
| P0-1 | Preview/Cache 安定性 | 🔄 継続中 | 🔄 大幅前進 | preview disk cache global budget、f16 composition format、static GPU cache bounded eviction、source version drift invalidation 群（07-12/21/27） |
| P0-2 | Track Matte | 🟡 データのみ | 🟡→✅ 描画実装 | `ArtifactLayerMatte.ixx`（MatteType/BlendMode/FitMode）+ `LayerBlendPipeline.ixx`/`MaskCutoutPipeline` GPU 適用。drag-link UX は未完 |
| P1-2 | Text Animator UX | 🔄 一部 | ✅ 大幅進展 | Range/Wiggly セレクタのプロパティ接続、プリセット 7 種、timeline group badge（07-25） |
| P1-3 | Motion Blur | 🟡 未変化 | 🟡 前進 | `TimeRemap.cppm` に shutter angle/samples、VectorBlurEffect。レイヤー横断の velocity pass は未 |
| P1-4 | Adjustment Layer | 🟡 スタブ | ✅ 実装 | `ArtifactAdjustableLayer.cppm` + render controller の effect carrier 適用 + テスト（`Artifact.Test.AdjustmentLayer`） |
| P1-5 | Parent/Pick-Whip | 🆕 設計のみ | 🟡 実装着手 | `feat_expression_property_pickwhip`（07-25）、`fix_pickwhip_use_evaluator_value`。ワイヤ描画等は未完 |
| P2-1 | Marker System | ❌ | 🟡 部分実装 | marker band 描画・MarkerHitResult（TrackPainterView）。Inspector/永続化/Undo 未 |
| P2-2 | Shape Operators | 🔄 進行中 | ✅ 大幅進展 | TrimPaths/Repeater/MergePaths/OffsetPaths/PuckerBloat/RoundedCorners/WigglePaths/ZigZag/Twist/HandDrawnWobble + 追加/削除/並べ替え UI + undo（07-24）+ native bezier 描画（07-27） |
| P2-3 | Precompose 完成 | 🟡 未変化 | ✅ 完了級 | PrecomposeUndoCommand、cycle detection、unprecompose atomic restore、nested time mapping 安定化（07-14/16） |
| P2-5 | Time Remap | 🟡 未変化 | 🟡→✅ 前進 | Animation メニュー（有効化/フリーズ/時間反転 + ショートカット）+ Core TimeRemap frame blend profile |
| P3-3 | Tracker | 🟡 着手 | 🟡 前進 | MotionTracker/TrackerManager + CameraTrackerTool + overlay 表示。専用 UI パネルなし |
| P3-4 | OFX | 🆕 進行中 | 🟡 前進 | `ArtifactOfxHost.cppm`（suite/bundle 探索）+ `ArtifactOfxEffectImpl` + effect service 登録。Manager UI なし |
| - | Audio Scrubbing | ❌ | ✅ 実装 | `ArtifactAudioScrubController` + timeline scrub drag + 診断連携 |
| - | Auto-Orient | ❌ | 🟡 部分実装 | `AnimatableTransform3D` AutoOrientMode（Off/AlongPath/AlongPathAtFrameStart）評価あり。UI 導線弱い |
| - | Roto/Paint | ❌ | 🟡 初期実装 | `ArtifactPaintLayer.cppm`（フレーム毎バッファ + undo stack）+ Layer メニュー。ブラシツール UI は最小限 |
| - | Interpret Footage | ✅ | ✅ | 変化なし |
| - | Keyframe 系 | ✅ | ✅ 拡張 | velocity-based Easy Ease（07-25）、curve numeric edit shortcuts、tangent mode 一括適用（07-24） |
| - | Roving Keyframes / Motion Sketch / Source Text Keyframe | ❌ | ❌ | 変化なし |
| - | Layer Styles 残り（Color/Gradient/Pattern Overlay） | ❌ | ❌ | 変化なし（確認範囲で追加なし） |

### 新規実装（AE 対比で追加された機能）

| 機能 | 内容 | 対応 DCC |
|------|------|---------|
| **Animation Layers** | プロパティ別ブレンドスタック + bake（current frame / work area / range）+ JSON 永続化 + undo（07-25） | Maya / MotionBuilder 由来。AE には無い |
| **Proxy ワークフロー** | Proxy 生成サービス + 品質切替（None/Quarter/Half/Full）+ video proxy decode switch（07-25） | AE のプロキシ相当 |
| **Quick Layer 作成** | QuickLayer creation MVP + 配置オプション（07-24） | — |
| **Accessibility 一式** | accessible name/focus/high contrast/hit target scaling（07-25、20+ コミット） | 他 DCC でも稀 |
| **Channel Box** | key all / key selected / lock channels（07-25） | Maya 由来 |
| **連番画像ワークフロー** | Asset Browser sequence 展開/プレビュー/relink/診断（07-27）+ ImageSequenceSource（LRU cache/prefetch/seek）+ ドロップ→1 レイヤー化と永続化（07-28 本日実装、ビルド未検証） | AE の連番フッテージ相当 |

---

## 3. 他 DCC ギャップ更新（対 2026-06-16 レポート）

### 3.1 カラー（Resolve / Baselight）— 最大の転換点

06-16 時点「完全に未着手（全 0 hit）」→ 以下が実装済み:

- ✅ **OCIO config**: `ArtifactCore/include/Color/OCIOConfig.ixx`（roles/colorspaces/display-view、ACES ビルトイン fallback）
- ✅ **OCIO Manager + UI**: `ArtifactOCIOManager.cppm` + `ArtifactColorSciencePanel.cppm`（preset/display/view combo、config 読込、状態表示）
- ✅ **スコープ**: VectorScope / WaveformScope / ParadeScope / Histgram（ArtifactWidgets）+ Composition Editor のプレビュー追従タブ（07-21 `feat_Add_preview_scope_tabs`）
- ✅ **LUT 書き出し**: `Color.LUTWriter` + 「Export LUT as .cube」ボタン
- 🟡 **LUT Browser**: ColorSciencePanel 内に LutEntry リストあり（専用ブラウザ UI は簡易）
- ❌ Qualifier / Power Window、HDR/Dolby Vision mastering、ACES IDT/ODT の本格パイプライン

### 3.2 レンダー/出力（Premiere / AME）

- ✅ **EXR マルチチャンネル float 出力**（07-14 `feat_multichannel_exr_float_render_queue`）
- ✅ **5.1 / 7.1 マルチチャンネル音声出力**（07-14）
- 🟡 **フォーマット拡張**: EncoderKind に VP9/AV1/ProRes/HAP/PNG/TIFF/EXR sequence 等の enum + FFmpegEncoder 経路あり。全コーデックの dispatch/実機検証は未完
- ❌ Audio-only export、放送セーフ、Loudness meter (BS.1770)

### 3.3 NLE 連携（Premiere / Resolve Edit）

- ✅ **OpenTimelineIO JSON adapter**（07-20、markers/transitions/track gaps round-trip）— 06-16 時点 0 hit から新規
- 🟡 **OpenAssetIO** foundation（07-20）
- ❌ Magnetic timeline、Multi-cam、字幕（SRT/WebVTT/CEA-708）、Timecode burn-in

### 3.4 3D / シミュレーション（Houdini / Nuke / C4D）

- ✅ **OpenVDB**: metadata 検査 + density snapshot 読込 + Pyro fields ブリッジ（07-20/21）— 0 hit から新規
- ✅ **Softbody GPU UV deform + grid triangle**（07-20）、**prefracture fragment pipeline**（07-16）、**boids flocking effector**（07-20）
- ✅ **GI 基盤**: SSGI/DDGI render contracts、quality presets、temporal bilateral resolve、height fog、toon lighting、PCF shadow、CACAO SSAO、dual kawase、SPD downsample（07-17〜20）
- ✅ **Cloner / MoGraph**: clone modifier stack（plain/random/step/formula/spline/3D rotation）+ 決定論的 physics（bounce/damping）+ matrix generator（07-17）
- 🟡 **PBR / lookdev**: viewport PBR（metallic/roughness/環境フォールバック）+ `feat_lookdev_pbr_preview_foundation`（07-17）。マテリアルエディタ UI・押し出しは未
- ⚠️ **Rig2d**: `feat_core_collaboration_rig2d_animation`（07-25）コミットあり、実体の厚み未確認
- ❌ IK/FK 本格リグ、Cloth/Hair、planar tracker、deep data / pass merge（Nuke 的）

### 3.5 エフェクト合成基盤（Nuke Blink / Fusion 的な独自路線）

- ✅ **Pointwise Effect Fusion**: effect fusion IR → HLSL codegen、shader cache、preflight validation、alpha-safe codegen（07-17）。複数ポイントワイズエフェクトの 1-pass 融合という Nuke/Blink 級の基盤
- ✅ **RGB/HSV keyer pass**（07-20）+ ChromaKeyEffect（effect service 登録済）
- ❌ Keyer のパラメータ UI 充実、Advanced Spill Suppressor、Refine Edge

### 3.6 オーディオ（Audition / Fairlight）

- ✅ **VST3 ホスト**: `VST3Loader.cppm`（factory 取得まで実装）— 06-16「VST2 のみ」から前進
- 🟡 **CLAP ホスト**: `CLAPHost.cppm`（entry 読込 + process。param 系はスタブ）
- ✅ Audio spectrum viewport overlay / waveform overlay + キャッシュ（07-25）
- ❌ Loudness meter、sidechain、bus routing matrix UI

### 3.7 アセット管理 / コラボレーション

- ✅ **Asset Source Registry**: canonical identity、versioned decoded payload cache、project JSON round-trip、localize/relink undo、problem view 連携（07-12 集中実装）— 06-16「Asset Instance Sharing ❌」を解消
- ✅ **Asset Browser 強化**: 参照スキャン/未使用フィルタ/disk thumbnail cache/削除・移動 undo（07-25）
- ⚠️ collaboration 系コミット（07-22/25 `feat_core_script_collaboration_updates` 等）あり、実体未確認
- ❌ Team project 本体、cloud asset library、render farm web UI（report dashboard web host は 07-21 に追加）

---

## 4. 引き続き未実装の主要ギャップ（優先順位付き）

### 開発優先方針（2026-07-27: 静止画/連番/シェイプ/合成/3D 優先）に沿う項目

| # | 機能 | 状態 | 備考 |
|---|------|------|------|
| 1 | **連番シーケンスの時間駆動再生** | 🟡 | ImageSequenceSource と ArtifactImageLayer::draw の接続（Insight.md 記録済、milestone 次項目） |
| 2 | **Track Matte drag-link UX** | 🟡 | データ+GPU は完成。タイムライン上のドラッグ設定 UI のみ残 |
| 3 | **Marker の永続化/Inspector/Undo** | 🟡 | 描画はあり。編集導線が未完 |
| 4 | **Shape Boolean（MergePaths）の正確性検証** | 🟡 | operator は存在。native bezier 化後の合成品質確認が必要 |
| 5 | **Keyer UI + Spill Suppressor / Refine Edge** | 🟡/❌ | keyer pass はあり。仕上げ系が無い |
| 6 | **3D マテリアルエディタ UI / 押し出し** | 🟡/❌ | viewport PBR のみ |
| 7 | **Motion Blur のレイヤー velocity pass + UI** | 🟡 | shutter 設定はあるが合成本流に velocity 蓄積なし |

### 方針外（動画・長期）だが差分として大きい項目

| # | 機能 | 状態 |
|---|------|------|
| 8 | 字幕/キャプション（SRT/WebVTT import/export） | ❌ |
| 9 | Mocha 風 planar tracker / トラッカー専用 UI パネル | ❌/🟡 |
| 10 | Loudness meter (BS.1770/LUFS) | ❌ |
| 11 | OFX Plugin Manager UI / サードパーティ実証 | ❌ |
| 12 | AEP / PSD レイヤー / Lottie import | ❌（今回未検証だが実装痕跡なし） |
| 13 | Roving Keyframes / Motion Sketch / Source Text Keyframe | ❌ |
| 14 | Layer Styles 残り（Color/Gradient/Pattern Overlay） | ❌ |
| 15 | Multi-cam / Magnetic timeline / チーム制作 | ❌ |
| 16 | AI 系（roto/denoise/auto caption）※Anime4K 風 upscale pass のみあり | ❌ |
| 17 | .mogrt 風テンプレート | ❌ |
| 18 | Qualifier / Power Window（Resolve カラーページ） | ❌ |

---

## 5. 旧レポートの訂正（0 hit 判定の偽陰性）

06-16 レポートで「完全未実装（0 hit）」とされていたが、**現在ソースで実装が確認できた**項目:

| 旧判定 | 実際 | 根拠 |
|--------|------|------|
| OCIO / ACES 0 hit | ✅ | `OCIOConfig.ixx` / `ArtifactOCIOManager.cppm` / ColorSciencePanel |
| Vector/Waveform/Parade scope 0 hit | ✅ | ArtifactWidgets 各スコープ + preview scope tabs |
| LUT export 0 hit | ✅ | `Color.LUTWriter` + .cube export UI |
| Audio Scrubbing 0 hit | ✅ | AudioScrubController + timeline 統合 |
| Adjustment Layer スタブ | ✅ | ArtifactAdjustableLayer + render controller 適用 |
| Paint layer 0 hit | 🟡 | ArtifactPaintLayer（初期実装） |
| OFX host 0 hit | 🟡 | ArtifactOfxHost（bundle 探索 + suite） |
| VST3 host 0 hit | ✅ | VST3Loader |
| Image sequence export 0 hit | 🟡 | EncoderKind + FFmpegEncoder 経路 |
| EXR multilayer 0 hit | ✅ | multichannel EXR render queue（07-14） |
| Asset Instance Sharing ❌ | ✅ | Asset Source Registry 一式（07-12） |
| Auto-Orient ❌ | 🟡 | AnimatableTransform3D::AutoOrientMode |

**教訓（07-08 訂正と同様）**: キーワード grep ベースのギャップ走査は命名差で偽陰性を出しやすい。本レポートはソース直接検証 + git ログ照合で作成した。次回更新時も grep 単独判定を避けること。

---

## 6. 推奨着手順（2026-07-28 版）

開発優先方針（静止画 → 連番 → シェイプ → 合成 → 3D）と整合する順:

### 短期（0-2 週間）
1. **連番シーケンス再生接続**（ImageSequenceSource → ImageLayer::draw）— milestone 継続項目
2. **Track Matte drag-link UX** — 残りは UI のみ、費用対効果最大
3. **Marker 完成**（永続化 + Inspector + Undo）— 部分実装の完了
4. **MergePaths / native bezier の合成品質検証** — 07-27 移行後の回帰確認

### 中期（2-4 週間）
5. **Keyer UI + Spill Suppressor** — keyer pass 基盤の活用
6. **Motion Blur velocity pass** — 合成品質（優先方針 4）に直結
7. **3D マテリアル UI**（lookdev foundation の UI 化）— 優先方針 5
8. **Render Format dispatch 完成 + 実機検証**（image sequence / ProRes）

### 長期（方針転換後）
9. 字幕/キャプション、planar tracker、Loudness meter、Plugin Manager UI、AEP/PSD/Lottie import

---

## 7. 更新履歴

- 2026-07-28: 初版作成。07-03/07-08 版 AE ギャップと 06-16 版 DCC ギャップに対し、git ログ（Artifact 399 / Core 155 コミット）とソース直接検証 30 項目を反映。
