# マイルストーン: VP + Timeline Hot-Path Stability (2026-08-29)

**最終更新:** 2026-08-30
**ステータス:** In Progress — static triage complete for Phase 1

> **このファイルは 2026-08-29 作成版の 2026-08-30 更新版です。** 元ファイル [MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-29.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-29.md) は別物として残してあります。
>
> 2026-08-29 作成

## 目的

Artifact の **「操作感の悪さ」と「安定性のなさ」** は、planned milestone の「機能追加」とは別系統の **ホットパスの重さ** に由来する。本マイルストーンは、ビューポート（VP）とタイムライン（TL）の surface における

- L1: ホットパスの重さ（GPU readback 同期、QImage cache key ミス、renderOneFrame 多重実行）
- L2: イベント配送の二重化（EventBus + Qt signal 同居、購読者ごとの重い処理の多重発火）
- L3: 選択／Property のリビルド過多（selection 復元漏れ、widget 破棄再作成、icon 毎回 I/O）

の 3 層を 1 つの枠でとらえ、**触り心地と安定性を一段底上げ**する。

参考: 関連棚卸し（2026-08-30 更新版） [MILESTONE_TIMELINE_STATUS_INDEX_2026-08-30.md](MILESTONE_TIMELINE_STATUS_INDEX_2026-08-30.md) / 2026-08-29 原本 [MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md](MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md)、演出層 [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md) / 2026-08-29 原本 [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-29.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-29.md)

## 背景

観察事実（2026-08-29）:

- `readbackToImage()` の GPU→CPU 同期ストールがフレームコストの 60-70% を占め、1 フレーム 10-30ms の遅延
- `QImage::cacheKey()` ベースの texture cache が毎フレーム失敗、5-15ms/frame の GPU テクスチャ再作成
- `renderOneFrame()` が 31 箇所から多重呼び出し、マウス移動 1 イベントで最大 4 回の再描画
- `docs/analysis/SHARED_DEVICE_AND_IMAGE_CACHE_AUDIT_2026-08-11.md`（2026-08-11 時点の最新監査）が「GPU→CPU readback は同期的に Flush()/WaitForIdle() するため、hot path に追加しないこと」と注意喚起
- EventBus publish と Qt signal が二重に走る、ブレンドモード変更が `ProjectChangedEvent` を全体に broadcast、レイヤー追加で 5 種類イベントが連続発火
- Property Widget 1 回の選択変更で約 400 widget 破棄/再作成 + 230 回の icon ファイル I/O
- `LayerSelectionChangedEvent` の `compositionId` が `weak_ptr` 期限切れで空文字になり Inspector が `NoLayer` 化
- プロパティ編集時にタイムラインが selection を解除する既知バグ

→ planned milestone 単位では 1 つ 1 つ対処されているが、**ホットパスの重さ**と**イベント配送の二重化**と**選択/Property のリビルド過多**が絡み合って、ユーザーが感じる「操作感の悪さ」と「安定性のなさ」になっている。BUG_TIMELINE_4ISSUES_2026-04-19.md の 4 件の fix 後も、SHARED_DEVICE_AND_IMAGE_CACHE_AUDIT_2026-08-11.md が 4 ヶ月経っても hot path readback 禁止を喚起しており、構造的解決には至っていない。

## Goal

- VP 操作（pan / zoom / ドラッグ / 選択）のフレーム時間を **16ms 以内に収める**ことを体感目標とする
- タイムライン操作（scrub / drag / clip 編集 / keyframe 編集 / 選択切替）で **1 操作 = 1 描画パス**に収める
- イベント連鎖による **N+1 リビルド**を構造的に排除する
- 選択状態が **操作の前後で必ず保存・復元**される

## Non-Goals

- 新規機能追加（planned の機能 milestone は別途進行）
- 巨大単一 widget の即時分割（AGENTS.md に従い別判断）
- ソフトレンダラーの新機能追加・大規模な最適化（AGENTS.md 2026-08-15 に従う）

## Design Principles

1. **No GPU readback on hot path** — エクスポート時または明示的指示時にのみ readback を許可
2. **Stable texture cache key** — `QImage::cacheKey()` ではなくレイヤー ID / リソース UUID をキーに
3. **Single render entry point** — `renderOneFrame()` の直接呼び出しを 1 箇所に集約し、購読者ごとの重複を排除
4. **EventBus or Qt signal, not both** — 同じ意味の通知は片方だけ。購読者は dedup キーで同一 state の再処理を回避
5. **Selection survives refresh** — あらゆるリビルド経路で `selectedLayerIds` を退避・復元
6. **Widget pool & icon cache** — Property Editor の widget / icon を永続化、I/O を初回のみに

## Phases

### Phase 1: GPU Readback 排除

- 目的: hot path から `readbackToImage()` を完全排除
- 対象:
  - `ArtifactIRenderer.cppm`（`readbackToImage()` 354-416）
  - `ArtifactRenderQueueService.cppm`（readback 呼び出し点 1380, 2147）
  - `ArtifactCompositionRenderController.cppm`（hot path 上の `flush() / readbackToImage()` 連鎖）
- 内容:
  - readback を **エクスポート時 / 明示要求時 / デバッグダンプ時** のみ許可し、フラグで gating
  - 通常描画パスから readback を呼ぶ箇所を grep で全列挙し、1 つずつ gating 経由に置換
  - フレーム予算を計測する `VP_FRAME_BUDGET_PROBE` を debug build で有効化
- DoD:
  - 通常の編集操作で `readbackToImage()` が呼ばれない（静的検査で 0 hit）
  - debug build の `VP_FRAME_BUDGET_PROBE` が 16ms 超えを可視化
  - エクスポートの image dump は引き続き動作

### Phase 2: Texture Cache Key の安定化

- 目的: `QImage::cacheKey()` ベースの毎フレーム texture 作成を停止
- 対象:
  - `Artifact/src/Render/PrimitiveRenderer2D.cppm`（`drawSprite` 1081, `drawSpriteTransformed(QTransform)` 1254, `drawSpriteTransformed(QMatrix4x4)` 1486）
  - `ArtifactCompositionRenderController.cppm`（`buildLayerSurfaceCacheKey()` 133-172）
- 内容:
  - texture cache key を **layer ID + 内容ハッシュ** へ移行
  - 内容ハッシュは画像の `bits()` 先頭 N byte + size + format で十分
  - `videoLayer->currentFrameToQImage()` も同様に「同じ frame なら同じ key」になるよう stable hash を付与
  - mask / effect 適用後の image は「ソース layer ID + effect ID + frame」をキーに
- DoD:
  - 静的検索で `image.cacheKey()` の使用箇所が hot path から消える
  - 動画レイヤーで連続 frame を再生中に `CreateTexture()` の呼び出し回数が大幅に減る
  - フレーム budget probe で 1 フレーム 5-15ms の改善が出る

### Phase 3: renderOneFrame 多重実行の統合

- 目的: 31 箇所から直接呼ばれる `renderOneFrame()` を 1 箇所に集約
  - Property 変更時のカスケード render（2-3 回 → 1 回）が体感できる

### Phase 4: EventBus 二重発火の解消

- 目的: Qt signal + EventBus publish の二重配送を排除
- 対象:
  - `Artifact/src/Service/ArtifactProjectService.cpp`（line 215, 526-531）
  - `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`（line 1237-1242, 1813-1817）
- 内容:
  - `notifyProjectMutation` を **EventBus 専用 / Qt signal 専用** に整理
  - ブレンドモード変更は `LayerChangedEvent{BlendMode}` 等の **構造粒度** に合わせて publish、ProjectChangedEvent は構造変化時のみ
  - レイヤー追加時の 5 種イベント連続発火を **LayerCreated 1 種** にまとめ、必要なら `LayerCreated` の payload に付随情報を持たせる
  - 購読者側で「同一 state での重複処理」を dedup する `EventGuard` を `ArtifactCore/include/Event/EventGuard.ixx` に新設
- DoD:
  - `ArtifactProjectService::notifyProjectMutation` から Qt signal emit が消える
  - ブレンドモードコンボ選択時の broadcast 範囲が 1 レイヤーに閉じる
  - レイヤー追加 1 回で発火するイベントが 1 種になる

### Phase 5: Selection 退避復元パターン

- 目的: あらゆるリビルド経路で selection を必ず保存・復元
- 対象:
  - `ArtifactTimelineWidget`（`scheduleRefresh()` 周辺）
  - `ArtifactLayerPanelWidget`（`performUpdateLayout` / `updateLayout`）
  - `ArtifactInspectorWidget`（`LayerSelectionChangedEvent` subscriber）
- 内容:
  - リフレッシュ前に `selectedLayerIds` を snapshot、退避は lightweight な `LayerId` 配列
  - `EventBus` 経由のリフレッシュも `SnapshotGuard` 経由で selection を保持
  - `LayerSelectionChangedEvent::compositionId` が nil の場合は **適用しない** で現行 selection を維持
  - ログに「選択が退避された／復元された」を debug 出力可能に
- DoD:
  - プロパティ編集時にタイムライン selection が解除されない
  - `compositionId.isNil()` のイベントで Inspector が NoLayer 化しない
  - リフレッシュ前後で `selectedLayerIds` が一致する

### Phase 6: Property Widget リビルド抑制

- 目的: 1 選択変更で約 400 widget 破棄/再作成を撲滅
- 対象:
  - `ArtifactPropertyWidget.cppm`（`rebuildUI` 717-723）
  - `ArtifactPropertyEditor.cppm`（`loadPropertyIcon` 923-927）
- 内容:
  - `PropertyRow` widget pool を `WidgetPool<PropertyRow>` として保持、release / acquire で再利用
  - `loadPropertyIcon` を **module 初期化時に全 icon を 1 度だけロード** する `PropertyIconCache` に置換
  - rebuild の signature を `rebuildSignature` 比較で不要リビルドを skip
  - 46 プロパティ × 7-10 子 widget の構造を 1 度だけ構築、selection 切替時は表示値だけ更新
- DoD:
  - 1 選択変更で作成/破棄される widget 数が大幅に減る
  - icon ファイル I/O がアプリ起動時に 1 回だけになる
  - Property 編集の schedule rebuild（80ms 遅延）が不要になる

### Phase 7: viewport 入力帯域の安定化

- 目的: pan / zoom 中の追従性を確保
- 対象:
  - `ArtifactCompositionRenderController`（`notifyViewportInteractionActivity` / `finishViewportInteraction`）
- 内容:
  - viewport 操作中は LOD / preview quality を自動的に下げる
  - `finishViewportInteraction()` 後の最初のフレームで **フル品質** に戻す
  - wheel / middle drag / space+drag すべてを `requestRender()` 経由に統一
- DoD:
  - pan / zoom 中のフレーム時間が安定する
  - 操作終了から 1 フレーム以内にフル品質へ復帰

## Definition Of Done

- 通常の編集操作で GPU readback が走らない（静的検査で 0 hit）
- `QImage::cacheKey()` の hot path 参照が消える
- `renderOneFrame()` の直接呼び出しが 1 箇所に集約
- 1 つのイベントで走る render パスが 1 回
- EventBus publish / Qt signal の二重発火が消える
- selection 状態があらゆるリビルド経路で保存・復元される
- Property Widget の widget 破棄/再作成が劇的に減る
- タイムラインを触っても VP が固まらない
- 既存 milestone（Operation Feel / Visual Language / InOut Slide / Layer Specialization / DCC-Feel Gaps）の進行を妨げない

## 既存 milestone との関係

- [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md) — 表面の演出層。Phase 1〜7 の完了後に着手すると効果的
- [MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md](MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md) — 操作規則の統一。Phase 3 / Phase 7 と並走可能
- [MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md](MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md) — 検索の重さは本マイルストーン Phase 4/Phase 5 の結果に依存
- [MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md](MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md) — P0A 着手点は Phase 5 完了後

## Recommended Order

1. Phase 1（GPU readback 排除）— 効果が大きく、低リスク
2. Phase 2（texture cache key 安定化）— Phase 1 と並走可能、動画レイヤー向け
3. Phase 3（renderOneFrame 統合）— Phase 1 の効果を最大化
4. Phase 4（EventBus 二重発火解消）— L2 対策、別系統で並走可能
5. Phase 5（selection 退避復元）— Phase 4 の結果を受けて着手
6. Phase 6（Property Widget リビルド抑制）— L3 対策、Phase 4/5 と並走可能
7. Phase 7（viewport 入力帯域）— Phase 1〜3 完了後

## 想定効果

- VP 操作（pan / zoom / ドラッグ / 選択）の体感品質が AE / Blender 級に近づく
- タイムライン scrub / drag / 編集中の「引っかかり」「ゴーストピクセル」「選択解除」が消える
- Property Widget のリビルドが体感で速くなり、Inspector の NoLayer 化が解消
- イベント配送が整理されることで、planned milestone で追加する機能の "重さの足し算" が抑えられる

## Next Execution Slice

Phase 1 の最小着手点:

1. `readbackToImage()` の呼び出し点を全列挙（grep）
2. 呼び出し点を `if (allowReadback())` で gating
3. `allowReadback()` のフラグを **export / debug dump / 明示 API のみ true** に絞る
4. 静的検査 CI（または grep ベースの手動チェック）で hot path に readback が残っていないか確認

完了条件: 通常の編集操作で `readbackToImage()` が走らない。debug build の `VP_FRAME_BUDGET_PROBE` が 16ms 超えを可視化。エクスポートの image dump は引き続き動作。

## 2026-08-29 現状確認

本マイルストーンは作成直後のため着手実績なし。Phase 1 から着手可能。ビルド・runtime 受入れは AGENTS.md に従いユーザー指示待ち。

- 対象:
  - `ArtifactCompositionRenderController.cppm`（675, 780, 783 ほか）
- 内容:
  - 直接呼び出しを `requestRender()` ヘルパに統一
  - `requestRender()` は `m_renderScheduled` フラグで重複 coalesce、frame pacing を尊重
  - 1 つの入力イベントで最大 1 回の render 実行を保証
- DoD:
  - 静的検索で `renderOneFrame()` の直接呼び出しが 1 箇所（または `requestRender` 経由のみ）になる
  - マウス移動 1 イベントで render 実行が 1 回だけになる
  - Property 変更時のカスケード render（2-3 回 → 1 回）が体感できる


- QtCSS / `QColorDialog` の新規採用
- 新規 signal/slot の追加
- `QImage` の本流投入
- `QPainter::CompositionMode` による合成実装
- ソフトレンダラーの新機能追加・大規模な最適化（AGENTS.md 2026-08-15 に従う）

## 2026-08-30 Update — Phase 1 static triage

`readbackToImage()` の現行呼び出しを静的に分類した。`ArtifactRenderQueueService`、`ArtifactExportPreRenderPipeline`、`OffscreenCompositionRenderer` は export / offscreen の明示出力であり、通常VPから一律にgateしてはならない。`CompositionRenderController` の readback は明示snapshot API と channel display（Color+Alpha）に限定されている。

唯一、`ArtifactCompositionViewDrawing.cppm` の adjustment-layer fallback は通常描画の互換経路で GPU target を `QImage` にreadbackしている。これは本マイルストーンで「通常編集hot pathからのreadback」として分離して扱う候補だが、ソフト／互換fallbackの意味を変えるため、export用の呼び出しと同じgateへ機械的に移してはならない。`renderOneFrame()` には既に 16ms debounce によるcoalescingの注記があるため、呼び出し数を過去の記録だけで31箇所と断定しない。build / runtime / GPU計測は未実施。
