# Workflow Gap Deep Dive — 2026-06-16

作成日: 2026-06-16
目的: 痛みメモ / 機能 audit / 既存 milestone で繰り返し挙がる **未提案の 7 つのワークフロー不足** を 1 枚にまとめ、それぞれを掘り下げる。
対象: 制作中ワークフローの **入力 → 操作 → 編集** までの細い不足
参照:
- `docs/analysis/MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md`
- `docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md`
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`
- `docs/planned/MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md`
- `docs/done/MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07.md` (✅ 完了)
- `docs/planned/MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md`
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`

---

## 結論サマリ

| # | テーマ | 痛み度 | 提案有無 | 既存着手 | 推奨 |
|---|---|---|---|---|---|
| 1 | アセットインスタンス共有 | 🟠 中 | なし | なし | **新規 milestone 推奨** |
| 2 | 数値入力の相対値 | 🟡 中 | **完了済** | `MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07` | 再着手しない |
| 3 | キーフレーム コピー&ペースト | 🔝🔴 高 | 言及あり | なし | **`ADVANCED_COPY_PASTE` から分離して着手** |
| 4 | In/Out スライド | 🟡 中 | なし | 部分的 | **新規 milestone 推奨** |
| 5 | Source Text Keyframe | 🔝🔴 高 (P0) | なし | なし | **新規 milestone 推奨** |
| 6 | Audio Scrubbing | 🔝🔴 高 (P0) | なし | なし | **新規 milestone 推奨** |
| 7 | Auto-Orient | 🔝🔴 高 (P0) | なし | なし | **新規 milestone 推奨** |

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。

---

## 1. アセットインスタンス共有 (Asset Instance Sharing)

### 1.1 痛み

`MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md`:

> 🟠 同じアセットを5回タイムラインに置くと、5回分全部メモリにロードされる
> 1回だけ置いて何度も複製する。編集すると全部に反映されてしまうので最後に一個ずつ切り離す

### 1.2 現状

- `ArtifactCore/src/Asset/AssetManager.cppm` (24 行) — ほぼ空の PImpl スタブ
- `AssetImporter.cppm` / `AssetMetaFile.cppm` / `AssetDatabase.cppm` — import / meta / db のみ
- `ArtifactAbstractLayer` 側に `sourcePath_` を保持する `ArtifactVideoLayer` / `ArtifactImageLayer` などは **それぞれが単独でロード**
- **instance 概念なし**: 5 個の同じ image layer は 5 個の decode / 5 個の GPU texture upload
- **reference count なし**: 1 個を削除しても 4 個は独立

### 1.3 設計の柱

- `AssetInstance` を `ArtifactCore/include/Asset/AssetInstance.ixx` に追加
  - `instanceId: QString` (UUID v7)
  - `sourceRef: AssetSourceRef` (path / URL / content hash)
  - `payload: AssetPayload` (decoded bytes を weak ptr で保持)
  - `referenceCount: int` (atomic)
- `AssetInstanceRegistry` シングルトン
  - `acquireInstance(sourceRef) -> AssetInstance*` (refcount++)
  - `releaseInstance(instanceId)` (refcount--, 0 で `payload` を destroy)
  - `instancesByComposition(compId) -> QHash<instanceId, refCount>`
- `ArtifactAbstractLayer` に `instanceId_` を持たせる
  - 既存の `sourcePath_` は `sourceRef` の path 部分と一対一対応
  - 復元時に path から instance を解決し、共有

### 1.4 不変条件

- `QImage` の hot path 流入禁止。decoded bytes は `ImageF32x4RGBAWithCache` 等の既存型に寄せる
- 既存 `AssetDatabase` / `AssetImporter` の API は温存。`AssetInstanceRegistry` は **上に乗る薄い layer**
- 共有された instance の **mutation は全 layer に伝播**。これは AE と同じ挙動
- 切り離し（unlink）は `LayerService::unlinkAsset(layerId, newPath)` で明示的に行う

### 1.5 フェーズ

- **Phase 1**: `AssetInstance` データモデル + `AssetInstanceRegistry` シングルトン (Core)
- **Phase 2**: `ArtifactAbstractLayer::setSourcePath(path)` で instance を解決し、`sourcePath_` → `instanceId_` 移行
- **Phase 3**: Inspector に `Unlink Asset` / `Relink` を追加。`Unlink` で instance 分離、`Relink` で再共有
- **Phase 4**: 既存プロジェクトの読み込みで `sourcePath_` から instance を再構築
- **Phase 5**: GPU texture cache 側で instance 単位の共有を確認。`GPUTextureCacheManager` の `cacheKey` に `instanceId_` を入れる

### 1.6 影響

- 5 個複製で 5 倍メモリ → 1 個分の decoded payload 共有
- 5 個複製で 5 回 decode → 1 回 decode (cache hit)
- 編集反映: AE と同じ挙動（共有中は全 layer に反映、`Unlink` で分離）

---

## 2. 数値入力の相対値

### 2.1 痛み

`MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md`:

> 🟡 プロパティの数値を相対値で変更出来ない
> 現在値をメモして、足し算した値を手入力する。電卓を常に開いて作業している

### 2.2 結論: **再着手しない**

`MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07.md` (2026-06-08 完了) で **`+10` / `-5` / `*2` / `/3` 入力が全数値フィールドで対応** している。

痛みメモの 7 項目は **解消済み**。

---

## 3. キーフレーム コピー & ペースト

### 3.1 痛み

`CORE_MODULE_MISSING_FEATURES_2026-04-19.md`:

> 🟡 キーフレームコピーペースト
> レイヤー間でキーを移動出来ない

`MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md` の優先度 ⭐🌟🌟🌟 (中) だが、AE parity の観点では必須。

### 3.2 現状

- `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` で **status: Not Started**
- `KeyframeClipData` 構造体案と `copyKeyframes(layerId, propPath) -> KeyframeClipData` API 案あり
- `ArtifactTimelineKeyframeModel.cppm` に Timeline 表示側は実装済み
- `ArtifactAbstractLayer` 側に **keyframe 抽出 API はあるが clipboard service は不在**

### 3.3 設計の柱

- 既存 `ADVANCED_COPY_PASTE_2026-03-28` から **Keyframe だけを分離** して本 milestone とする
  - Layer / Effect / Property clipboard は別 milestone に残す
- `ArtifactClipboardManager` を `Artifact/src/Service/ArtifactClipboardManager.cppm` に新規追加
  - `copyKeyframes(layerId, propPath) -> KeyframeClipData`
  - `pasteKeyframes(layerId, targetPropPath, sourceFrameRange, targetFrame)`
  - 内部 payload は `QJsonDocument` 経由
- Undo: `PasteKeyframeCommand` (`Artifact/Undo/`)
- Timeline 入力:
  - `Ctrl+C` / `Cmd+C` で選択 keyframe をコピー
  - `Ctrl+V` / `Cmd+V` で playhead 位置にペースト
  - 右クリック → `Copy Keyframes` / `Paste Keyframes Here` / `Paste at Original Frame`

### 3.4 3 つのペースト挙動

- **Paste at Original Frame**: コピー元と同じ frame にペースト
- **Paste at Playhead**: playhead 位置にオフセットしてペースト
- **Paste Relative**: 元の frame 範囲の `Δframe` を保ったままペースト

### 3.5 不変条件

- 既存 `KeyframeClipData` 案は **JSON schema 経由** に寄せる
- 単一 property path と複数 property 両対応
- レイヤー間で **property path が同名なら** 自動マッチ。異名なら手動選択
- 既存 keyframe がある位置に paste した場合の挙動:
  - Replace: 既存を削除して paste
  - Merge: 既存を維持して paste
  - Skip: 衝突位置に paste しない
  - Default は **Replace**

### 3.6 フェーズ

- **Phase 1**: `ArtifactClipboardManager` シングルトン + `copyKeyframes / pasteKeyframes` 基礎
- **Phase 2**: Timeline 入力（`Ctrl+C/V` + 右クリック）
- **Phase 3**: 3 つの paste 挙動 (Original / Playhead / Relative) + Undo
- **Phase 4**: 複数 property 対応 + 異名 property 手動マッピング
- **Phase 5**: 既存 `ADVANCED_COPY_PASTE` 全体進捗の更新

---

## 4. In/Out スライド (Layer In/Out Point Slide)

### 4.1 痛み

`CORE_MODULE_MISSING_FEATURES_2026-04-19.md`:

> ⚠️ 中優先 イン点 / アウト点 スライド
> 現在は切り取りしかない

`MOTION_GRAPHICS_AD_PRODUCTION_THINKING_MEMO_2026-05-28.md` の尺合わせとも関連する。

### 4.2 現状

- `ArtifactTimelineTrackPainterView.cpp` には **clip の in/out point をスライドする経路は限定的**（grep 0 hit）
- `ArtifactWorkAreaControlWidget` は **work area 専用**。layer 単位の in/out point は別
- `ArtifactInOutPoints` (`Artifact.Composition.InOutPoints`) は **comp 単位の in/out のみ**
- `MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md` は ripple 専用で slide は未対応

### 4.3 設計の柱

- `ArtifactTimelineTrackPainterView` に **clip の in/out point ハンドル** を追加
  - 既存の `leftEdge / rightEdge` を再設計し、handle として再描画
  - left handle: `inPoint` を frame range `[0, outPoint - 1]` 内でスライド
  - right handle: `outPoint` を frame range `[inPoint + 1, duration]` 内でスライド
  - 中央ドラッグ: `inPoint / outPoint` を平行移動
- **slide vs ripple vs trim** の 3 つを明示分離
  - **slide**: in/out を平行移動、ソース側は同じ range を表示
  - **ripple**: slide + 後続 clip を詰める
  - **trim**: in/out を伸ばす/縮める
- `SlideClipCommand` (`Artifact/Undo/`)
- ショートカット: `Alt+←/→` で 1 frame slide、`Shift+Alt+←/→` で 10 frame

### 4.4 不変条件

- slide 範囲は **clip の source 範囲を超えない**
- ソース素材を **`outPoint` で cut しない**。slide は時間窓だけを動かす
- ripple との **混同禁止**。UI 上で mode 切替を明示
- 1 undo 単位で 1 slide 操作
- work area slide とは別 surface

### 4.5 フェーズ

- **Phase 1**: `ArtifactTimelineTrackPainterView` の clip edge を handle 化。hover / cursor 変更
- **Phase 2**: slide 操作と `SlideClipCommand`
- **Phase 3**: ripple との mode 切替 UI
- **Phase 4**: キーボードショートカット + 複数選択時の slide
- **Phase 5**: ソース範囲を超える slide のクランプ

---

## 5. Source Text Keyframe

### 5.1 痛み

`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`:

> #18 Source Text keyframe
> `setSourceText` 等無し。テキスト編集はインライン編集のみ

`AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` の P0 に分類。

### 5.2 現状

- `ArtifactTextLayer` は **text 内容（`text` プロパティ）はあるが**、時間変化の機構は無し
- `ArtifactCore/src/Text/TextAnimator.cppm` は **glyph-level** アニメータ（位置 / 透明度 / scale）。文字列そのものの時間変化は別物
- `setSourceText` / `getSourceText` API 不在
- Timeline 上に text content track なし

### 5.3 設計の柱

- `ArtifactTextLayer::setSourceText(frame, QString)` / `getSourceText(frame) -> QString`
  - 内部に `std::map<FramePosition, QString>` を保持
  - frame の **直前の entry** を返す
  - エントリ不在時は `defaultText_`
- Timeline 上に **`Text Content` トラック** を追加
  - 表示は text content の value 略称（最初の 12 文字）
  - キーフレームマークは通常 keyframe と同じ形状
  - 編集: 右クリック → `Edit Text at Frame` で inline editor
- Inspector に **`Source Text` パネル** を追加
  - `Add Keyframe at Playhead`
  - 既存 text animator との precedence ルール: **Source Text > Animator > Default**

### 5.4 HSL Component blend との相互作用

- 既存 `Text Animator` の `Hue / Saturation / Color / Luminosity` は glyph-level
- Source Text Keyframe は **layer-level**（layer 全体の text を切替）
- 両方が active な場合:
  - まず Source Text で layer の text を切替
  - その後 Text Animator を適用
  - これが AE と同じ挙動

### 5.5 不変条件

- `QImage` / `setStyleSheet` / 新規 signal-slot 追加禁止ルールは維持
- text 内容 keyframe は **Timeline 上で 1 track** に閉じる
- 既存 `Text Animator` のプロパティ path と衝突しない
- CJK / 縦書き / フォント fallback は既存 `Text` 基盤にそのまま乗る

### 5.6 フェーズ

- **Phase 1**: `ArtifactTextLayer` に `sourceTextKeyframes_` 追加。get/set API
- **Phase 2**: Timeline 上の `Text Content` トラック描画
- **Phase 3**: Inspector の `Source Text` パネルと Undo
- **Phase 4**: 既存 `Text Animator` との precedence ルール統合
- **Phase 5**: 永続化と project JSON 接続

---

## 6. Audio Scrubbing

### 6.1 痛み

`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`:

> #20 Audio Scrubbing
> — 一致コードなし。scrub 時のリアルタイム音声 preview がない。playbackEngine への導線のみ存在

`AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` の P0 に分類。

### 6.2 現状

- `ArtifactPlaybackService.cppm:488` で reason ログあり
- `ArtifactAudioMixerWidget.cppm:122` で spectrum 表示
- **`AudioScrubController` クラス不在**。scrub 時の音声再生は **silent**

### 6.3 設計の柱

- `Artifact/src/Audio/ArtifactAudioScrubController.cppm` を新規追加
  - `startScrub(sourceAudioFile, playheadFrame)`
  - `updateScrub(playheadFrame)` — 速度感で音量を調整
  - `stopScrub()`
  - `playheadFrame` から逆算して該当 sample をデコード
- `ArtifactCompositionRenderController` の scrub 経路に組み込み
  - `ArtifactTimelineScrubBar` の `mouseMove` 中に呼び出し
  - 5 ms 以内のレスポンスを目標
- 速度感: **scrub 速度が速いほど volume を下げる**（AE 互換）

### 6.4 不変条件

- 通常 playback と **独立した buffer** を使う
- scrub 中は **mute / solo / volume** を尊重
- video scrub と同じ frame 位置から audio を逆算
- 10 ms 以下の遅延を目標
- 既存 `ArtifactAudioEngine` の audio thread と衝突しない

### 6.5 フェーズ

- **Phase 1**: `ArtifactAudioScrubController` 骨格。start / stop のみ
- **Phase 2**: `ArtifactTimelineScrubBar` の mouseMove 連動
- **Phase 3**: 速度感による volume 調整
- **Phase 4**: 複数 audio layer の mix 対応（mute / solo 反映）
- **Phase 5**: 設定 UI（on/off、遅延調整）

---

## 7. Auto-Orient

### 7.1 痛み

`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`:

> #12 Auto-Orient
> — 一致するコードなし。path に沿う向き自動補正がない

`AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` の P0 に分類。

### 7.2 現状

- `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` / `MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md` で **motion path 表示は実装**
- **`Transform.Rotate` の keyframe はあるが auto-orient 機能なし**
- 0 hit (grep / findstr)

### 7.3 設計の柱

- `ArtifactAbstractLayer::setAutoOrient(AutoOrientMode mode)`
  - `AutoOrientMode { Off, AlongPath, AlongPathAtFrameStart }`
  - 内部に `autoOrient_` を保持
- `CompositionRenderController` の layer transform 評価時に:
  - `mode == AlongPath` の場合、frame の **前後の keyframe** から tangent を計算
  - tangent の角度を layer の rotation に **加算**
  - 既存 rotation keyframe と **合成**
- 既存 `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` の motion path 計算と共有

### 7.4 合成ルール

- **No rotation keyframe + AutoOrient = On**: tangent のみ
- **Rotation keyframe + AutoOrient = On**: keyframe rotation + tangent offset
- **AutoOrient = Off**: 従来通り
- **両方のときは user rotation が優先**、tangent は auxiliary

### 7.5 不変条件

- AutoOrient は **毎 frame 再評価**。事前計算不要
- motion path がない layer は AutoOrient が無効
- 既存 `Transform.Rotate` keyframe の **意味は変えない**
- Composition setting で全体 ON/OFF 可能
- per-layer override 可能

### 7.6 フェーズ

- **Phase 1**: `AutoOrientMode` enum + `setAutoOrient` / `getAutoOrient` API
- **Phase 2**: `CompositionRenderController` の transform 評価に auto-orient 計算
- **Phase 3**: motion path 計算との統合（`MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` 参照）
- **Phase 4**: Inspector の layer transform パネルに ON/OFF
- **Phase 5**: 全体 ON/OFF と user rotation との precedence ルール

---

## 8. 推奨着手順

痛みの重さと依存関係から、推奨着手順は次の通り。

1. **キーフレーム コピー&ペースト** — `ADVANCED_COPY_PASTE` から分離して着手。1 セッションで最大の歓び
2. **In/Out スライド** — Timeline の本質的編集機能。ripple と並ぶ基礎
3. **アセットインスタンス共有** — Core 側の foundation。Phase 1 を 1 セッションで済ませる価値
4. **Source Text Keyframe** — Text 系の重要な AE 互換機能
5. **Audio Scrubbing** — Playback 系の AE 互換機能
6. **Auto-Orient** — motion path との統合で価値が出る

> 6 と 7 は **P0 の AE 互換機能** だが、依存（motion path 完成 / audio engine 拡張）が重いため、3〜5 個目の着手が安全。

---

## 9. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` | Layer / Effect / Property clipboard。本 memo §3 は Keyframe だけを分離して着手。 |
| `MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07.md` | **完了**。本 memo §2 は再着手しない。 |
| `MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md` | ripple。本 memo §4 は slide を補完。 |
| `MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` | Timeline 上 keyframe 編集。本 memo §3 と接続。 |
| `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` | motion path。本 memo §7 Auto-Orient はこれに依存。 |
| `MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md` | text 編集。本 memo §5 Source Text Keyframe はこの上に乗る。 |
| `MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md` | playback cache。本 memo §6 Audio Scrubbing は playback 経路。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |
| `MILESTONE_MARKER_FOUNDATION_2026-06-16.md` | 別 topic。 |
| `MILESTONE_LUT_BROWSER_2026-06-16.md` | 別 topic。 |
| `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` | 別 topic。 |

---

## 10. リスクと未解決論点

### 10.1 横断リスク

- **依存の重なり**: Auto-Orient は motion path、Audio Scrubbing は audio engine、Source Text Keyframe は text layer。**それぞれ既存の milestone を前提にする** ため、依存先 milestone の進捗を要確認
- **Undo 粒度**: 6 つのうち 4 つは `QUndoCommand` 派生を追加する。`Artifact/Undo/` の整理が必要
- **永続化**: 6 つのうち 4 つは project JSON に新フィールドを追加する。後方互換を保った追加が必要
- **サブモジュール境界**: 6 つのうち Asset Instance と Auto-Orient は `ArtifactCore` 側に手を入れる。明示依頼時のみ

### 10.2 個別の未解決

- **Asset Instance**: `QImage` を内部に持たない decoded payload 型（`ImageF32x4RGBAWithCache` 等）の canonical path を Phase 1 開始時に再確認
- **Keyframe Copy/Paste**: 既存 keyframe がある位置の default 挙動を `Replace / Merge / Skip` のどれにするか。Phase 1 で固定
- **In/Out Slide**: slide と ripple の UI 上の mode 切替を 1 段にするか 2 段にするか。Phase 3 で決定
- **Source Text Keyframe**: text content track のレンダリング時に layout cache をどう扱うか。Phase 2 で測定
- **Audio Scrubbing**: 5 ms / 10 ms 目標のどちらを既定にするか。Phase 1 で実測
- **Auto-Orient**: tangent 計算の数値安定性。frame 0 / duration 端での挙動。Phase 2 で検証

### 10.3 サブモジュール境界

- それぞれ **明示依頼時のみ** submodule bump
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 11. Done Criteria (全体)

- 6 テーマすべてに `Artifact/Undo/` 配下の `QUndoCommand` 派生が 1 個以上追加される
- 6 テーマすべてに project JSON への永続化（後方互換）が追加される
- 6 テーマすべてに `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` への健全性 contribution がある
- 6 テーマすべてで `QImage` / `setStyleSheet` / 新規 signal-slot / 新規 global signal が増えていない
- `ArtifactWidgets` を触っていない
- 既存完了の `MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07` を再着手しない

---

## 12. 更新履歴

- 2026-06-16: 初版作成。痛みメモと FEATURE_AUDIT の 7 項目を 1 枚に整理。`MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07` の完了を確認。
