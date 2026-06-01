# Milestone: Proxy Quality Toggle in Preview UI

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

---

## 関連ファイル（影響）

- `Artifact/src/Widgets/Playback/ArtifactPlaybackControlWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`

---

## 見積

- Phase 1: 6–10h
- Phase 2: 4–6h
- Phase 3: 8–12h

合計: 18–28h
