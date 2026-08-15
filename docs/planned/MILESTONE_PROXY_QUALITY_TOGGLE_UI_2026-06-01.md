# Milestone: Proxy Quality Toggle in Preview UI

**最終更新:** 2026-08-15

## 現行コード照合（Update 2026-08-15）

旧記述の「ユーザーが触る UI が欠けている」は現行コードでは部分的に解消済み。`ArtifactViewMenu` に Draft／Preview／Final action があり、`ArtifactProjectService::setPreviewQualityPreset`、`PreviewQualityPresetChangedEvent`、`CompositionRenderController::setPreviewQualityPreset` を通じて downsample と render invalidation に接続され、quality text も settings に保存される。

残る課題は Playback Control／Viewer footer への直結、composition-resident の正式保存、品質 HUD、shortcut、warm-up の runtime 受入である。

作成日: 2026-06-01
親: `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md` (Proxy / Draft 切替)
関連: `MILESTONE_RAM_PREVIEW_RANGE_POLICY_AND_PRIORITY_2026-05-31.md`, `MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md`

---

## 目的

Composition Viewer と Playback Control から proxy / draft / full の
プレビュー品質を直接切替えられるようにする。現在は内部の
`PreviewQualityPreset` が存在するが、ユーザーが触る UI が欠けている。

---

## 既存の土台

- `PreviewQualityPreset` enum (Draft / Preview / Full 相当) — `MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md` に定義済み
- `ArtifactPlaybackControlWidget` — 再生コントロール UI
- `ArtifactCompositionRenderController` — レンダリング品質の `previewDownsample_` を既に保持
- `ArtifactCompositionRenderWidget` — 表示更新の責任
- `MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md` — video proxy が実質終了済み

---

## 未着手要素

- 再生コントロールまたは Composition Viewer フッターから品質 preset を切替える UI
- draft モードで preview が高速化することを示す HUD（例: "DRAFT (1/4)" ）
- preset 切替を composition-resident の `PreviewQualitySettings` として保存
- 切替時に render cache を invalidate する責務が決まっていない
- キーボードショートカット（例: Ctrl+Alt+1/2/3）の追加

---

## フェーズ

### Phase 1: Quality Toggle UI（再生コントロール統合）

- `ArtifactPlaybackControlWidget` の右端に 3 段トグルボタンを追加
  - Draft (低) / Preview (中) / Full (高)
- 押下で `ArtifactCompositionRenderController::setPreviewQuality()` を呼び、
  render cache を即 invalidate & rebuild 開始
- 現在の preset を Composition の設定として `FastSettingsStore` に保存

### Phase 2: Viewer フッター表示

- `ArtifactCompositionViewerFooter` に右下に quality badge を追加
  - Draft 時は色付きアイコン + "DRAFT (1/4)" 
  - Preview 時は "PREVIEW (1/2)"
  - Full 時は非表示
- badge クリックで同じトグルメニューを開く

### Phase 3: Cache Invalidation Policy

- quality 切替を `QualityChangedEvent` として EventBus で発行
- `ArtifactRenderManager` / `ArtifactFrameCache` が quality キーを
  cache key の一部として扱い、切替後に stale な cache 全体を解放
- going Full → Draft 移行時の nasty-frame 防止: 解放直後に seek して
  直近 n フレームを再キャッシュする warm-up を非同期で起動

---

## 検証条件

- Playback Control の Draft トグル押下で viewer の解像度が 1/4 に落ち、FPS が上昇
- 切替後に過去フレームを再生しても古い quality の画像が残らない
- 新規composition 作成時に最後に使った quality preset が初期値として復元される
- 非アクティブコンポジションでも品質設定が保持され、再有効時に反映される

## Static Audit (2026-07-25)

計画時点から実装が進み、`PreviewQualityPreset`（Draft／Preview／Final）、Composition Editor 下部の Full／Half／Quarter resolution combo、Fast Preview メニュー、View Menu の品質操作、`PreviewQualityPresetChangedEvent`、App Settings／session settings の復元経路が確認できる。Render Controller は preset を downsample factor 1／2／4 に変換し、RAM preview cache を invalidate して viewport 更新へ進む。Playback／render contract 側にも previewDownsample と effectiveDownsample の記録がある。

ただし、Project Service の setter 自体には renderer 呼び出しがコメントで残り、実際の反映は event subscription と Render Controller 側に分散している。Viewer footer の専用 DRAFT (1/4)／PREVIEW (1/2) badge、quality を composition-resident に保存する仕組み、品質キーを含む全 frame cache の stale 解放と非同期 warm-up、Ctrl+Alt ショートカット、非アクティブ composition の保持、実解像度／FPS／過去 frame の runtime 検証は未確認である。

判定: **Phase 1 の UI・基本反映と cache invalidate は実装済み。** Phase 2〜3 の専用 badge、永続化粒度、cache policy の完全統一、runtime 検証が残っている。

---

## 関連ファイル（影響）

- `Artifact/src/Widgets/Playback/ArtifactPlaybackControlWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`

## 現行コード監査 (2026-08-15)

`ArtifactAppSettings` に Preview quality／resolution／RAM cache の設定と永続化 API があり、playback／render 側には downsample、RAM preview invalidate、cache state／priority の基盤がある。従って Phase 1 の quality surface と基本反映は進展済みである。一方、専用 viewer badge、composition 単位の品質保存、quality を含む全 cache key の stale 解放、非同期 warm-up、非アクティブ composition の復元、実解像度／FPS／過去 frame の runtime 確認は未証明であり、Phase 2〜3 は pending とする。

---

## 見積

- Phase 1: 6–10h
- Phase 2: 4–6h
- Phase 3: 8–12h

合計: 18–28h
