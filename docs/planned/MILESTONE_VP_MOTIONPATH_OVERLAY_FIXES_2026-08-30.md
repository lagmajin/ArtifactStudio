# マイルストーン: VP Motion Path Overlay Fixes (2026-08-30)

**最終更新:** 2026-08-30

> 2026-08-30 作成

## 目的

VP（ビューポート）上のモーションパス描画には、**機能はあるが層状に問題が積み重なっている**。本マイルストーンは観察された 5 つの独立した問題（死蔵コード・実装並走・キャッシュ限定・レンダラ状態直書き・色トークン非整合・keyframe 描画線形増）を 1 つの枠でとらえ、AE / Blender / Nuke 級の「layer の時間編集を即座に追える」モーションパス表示に整える。

参考: 関連 [MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md)（hot path 安定性）、[MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md)（表面仕上げ）

## 背景

観察事実（2026-08-30、`ArtifactCompositionRenderController.cppm` 42443 行基準）:

- L35704-L35706 に「`// Temporarily disable motion path overlay while debugging stray frame-like rectangles in the viewport.`」とコメントで旧実装全体（L35708-L35800+、**170 行超**）が `// ` 付きで残ったまま。**本体パイプラインから呼ばれていない**死蔵コード
- 新実装が **L37009-L37350** と **L41630-L41700** の 2 箇所に分散。L37011-37013 と L41632-41634 で同じ profile scope 名 "MotionPath" が **1 フレーム内に 2 回**走る構造
- L37088-L37095 のコメント：「`300-iteration getGlobalTransformAt() loop is the main >1000ms bottleneck`」
- L37096-L37107 のキャッシュキーは `(layerId, framePos, overlaySerial)` の 3 要素のみで、適応サンプル数を決める zoom も含まれていなかった。**複数選択や layer 切替**のたびに cache miss → >1000ms のホットループが再発火
- L41680-L41698 で `renderer_->setZoom(1.0f)` / `setPan(0.0f, 0.0f)` で**レンダラ内部状態を直接書き換え**、`drawPastFixedPlaneMotionFrames` 後に復元。例外/early return で zoom=1 に固定される race condition リスク
- L37080 の `pathColor{0.9f, 0.4f, 0.8f, 0.9f}` のピンク系は AGENTS.md Visual Language のレイヤー色ガイド（Video=青, Audio=緑, Text=紫, Effect=橙）と非整合。Timeline ruler の playhead orange / theme accent とも別系統
- L37319-L37350：各 keyframe に対し `drawDashedRectOutline` を **影 + 本体の 2 回**呼ぶ。keyframe 数に比例して描画コールが線形増

## Goal

- VP 上でモーションパスが **単一経路**で描画される（profile scope "MotionPath" が 1 フレームに 1 回）
- 単一選択・複数選択・layer 切替のいずれでもキャッシュが効いて **>1000ms のボトルネックが再発しない**
- レンダラ内部状態を draw call 前後に直書きせず、**scope guard 経由**で安全に更新・復元
- 色トークンが Visual Language 整備と整合し、**テーマ切替で一括追従**する
- keyframe 描画が **keyframe 数に比例して増えない**（instanced / batch）

## 2026-08-30 実装更新 — レイヤー別キャッシュの局所対応

- `ArtifactCompositionRenderController::Impl::renderMotionPathOverlayForLayer()` に、既存の hit-test 用単一スナップショットを維持したまま、コンポジション ID と layer ID の組み合わせごとの `MotionPathCacheEntry` 保持を追加した。
- 同一 layer・frame・zoom・`overlayInvalidationSerial_` の組み合わせでは、複数 layer の overlay pass 間で `getGlobalTransformAt()` を再実行せず、既存の描画／hit-test データを再利用する。zoom が変わった場合は適応サンプル密度を再計算する。
- 複数 layer の描画ループ後は、pointer hit-test が参照する単一スナップショットを選択 layer の cache へ戻す。描画順の最後の layer が選択 layer でない場合でも、tangent 編集の対象がずれないようにする。
- `invalidateOverlayComposite()` で単一キャッシュとレイヤー別キャッシュを同時に破棄するため、構成変更・選択変更などの既存 invalidation 契約を stale データが迂回しない。
- これは Phase 3 の「集合キー」や GPU batch 化を先取りするものではなく、既存 API と renderer 経路を変えない検証可能な中間スライスである。ビルド・runtime 受入は未実施。

## 2026-08-30 追加更新 — 旧コメントアウト経路を削除

- 現行の `renderMotionPathOverlayForLayer()` と hit-test 側の `buildMotionPathSamples()` が既に実コードとして存在することを静的確認し、描画関数内に残っていた旧 Motion Path のコメントアウトブロック（実行されない二重実装）を削除した。
- 描画呼び出し、ProfileScope、renderer API、キャッシュ契約は変更していない。Phase 1 はソース上完了相当とし、ビルド・runtime での単一経路確認は未実施とする。

## Non-Goals

- 新規機能追加（モーションパス補間の新方式、ベジェ表示など）
- AGENTS.md に従い QtCSS / `QColorDialog` / 新規 signal/slot / `QImage` / `QPainter::CompositionMode` の新規採用
- `TimelineTrackView` / `TimelineScene` などの旧構造への再参照
- ソフトレンダラーでの追加機能（AGENTS.md 2026-08-15 に従う）


- ソフトレンダラーでの追加機能（AGENTS.md 2026-08-15 に従う）

## Design Principles

1. **Single render path** — モーションパス描画の呼び出し点を 1 箇所に集約。profile scope "MotionPath" は 1 フレームに 1 回
2. **Multi-selection-aware cache** — キャッシュキーは `(layerIds, framePos, overlaySerial)` の集合ベース。layer 集合が変わったときだけ invalidate
3. **Scope guard for renderer state** — `setZoom` / `setPan` / `setCanvasSize` / `setUseExternalMatrices` は RAII / scope guard で更新し、例外／early return でも確実に復元
4. **Color via Visual Language tokens** — `pathColor` / `keyColor` / `currentColor` / `pastColor` / `futureColor` を `ArtifactCore::currentDCCTheme()` 経由の token に切替
5. **Batch keyframe overlay** — keyframe 描画は `drawDashedRectOutlines` のような batch 版を `ArtifactIRenderer` に追加し、N 個の keyframe を 1 コールで描画
6. **No regression in hot-path stability** — [MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md) の Phase 1〜3 と並走可能だが、renderOneFrame 統合前段として位置付ける

## Phases

### Phase 1: 死蔵コード（旧コメントアウトブロック）の削除

- 目的: L35704-L35800+ の 170 行超の `// ` 付き死蔵コードを除去し、ファイル肥大とコード可読性を改善
- 対象:
  - `ArtifactCompositionRenderController.cppm`（L35704-L35800+ 周辺）
- 内容:
  - 旧ブロックの参照シンボル（`buildMotionPathSamples` / `MotionPathSample` / `MotionPathSampleKind`）が他で使われていないか静的検査
  - 旧ブロックを `git log -p` で確認し、安全に削除可能か判断
  - 削除後、L37009- の新実装が単独で動くことを Frame Debug で確認（"MotionPath" profile scope が 1 フレームに 1 回になる）
- DoD:
  - L35704-L35800+ の `// ` ブロックが物理的に消える
  - ビルドが通る
  - 1 フレームに profile "MotionPath" が 1 回しか出ない

### Phase 2: 新実装 2 経路の統合

- 目的: L37009-L37350 と L41630-L41700 に分散する新実装を 1 経路に統合
- 対象:
  - `ArtifactCompositionRenderController.cppm`（L37009-L37350 + L41630-L41700）
  - `renderMotionPathOverlayForLayer` の定義側
- 内容:
  - どちらが正典かを呼び出し点（L41652）と定義（L37009 側か別途定義か）で確定
  - 片方を `// DEPRECATED: see renderMotionPathOverlayForLayer` のコメントで明示し、最終的に削除
  - 統合後、L41630-L41700 の "Historical Plane frames" 描画は **別の profile scope**（例 `"MotionPathFrames"`）に分離
- DoD:
  - 1 フレームに "MotionPath" profile scope が 1 回、"MotionPathFrames" が 0〜1 回
  - VP の見た目に変化がない（ピクセル単位 diff）
  - 旧経路が `// DEPRECATED` コメントのみで残らない（Phase 1 と統合可）

### Phase 3: 複数選択対応キャッシュへの拡張

- 目的: `(layerId, framePos, overlaySerial)` 単一キーから `(layerIds 集合, framePos, overlaySerial)` 集合キーへ拡張し、複数選択や layer 切替でホットループが再発火しないようにする
- 対象:
  - `ArtifactCompositionRenderController.cppm`（L37096-L37107 の `MotionPathCacheEntry` 構造体）
  - `motionPathCache_` メンバの定義
- 内容:
  - `MotionPathCacheEntry::layerIds` を `QVector<LayerID>` に変更
  - キャッシュ比較を `std::equal` で集合一致に切替
  - 集合のハッシュ化（`qHash` の `QVector<LayerID>` 特殊化）を `ArtifactCore` に追加
  - キャッシュ invalidate の頻度が「選択変更」だけに収束

### Phase 4: レンダラ状態の scope guard 化

- 目的: L41680-L41698 の `setZoom(1.0f)` / `setPan(0,0)` / `setCanvasSize` / `setUseExternalMatrices` の直書きを RAII / scope guard に置換し、例外/early return でもレンダラ状態を安全に復元する
- 対象:
  - `ArtifactCompositionRenderController.cppm`（L41680-L41698）
  - 必要なら `ArtifactIRenderer` に `RendererStateScope` を新設
- 内容:
  - `RendererStateScope` を `ArtifactIRenderer` か `ArtifactCompositionRenderController` のヘルパに新設
  - ctor で current zoom/pan/canvasSize/externalMatrices を snapshot
  - dtor で snapshot 値を必ず復元
  - 既存の `drawPastFixedPlaneMotionFrames` の呼び出しを `RendererStateScope` で囲む
  - 例外安全性テスト：例外を投げる `drawPastFixedPlaneMotionFrames` スタブでレンダラ状態が復元されることを確認
- DoD:
  - 例外発生時も zoom / pan / canvasSize / externalMatrices が復元される
  - 直書きパターン（`setZoom` → `setPan` → ... → `setZoom` 戻し）が消える
  - 既存の挙動が再現される（VP の見た目に変化なし）

### Phase 5: 色トークンの Visual Language 統合

- 目的: `pathColor{0.9f, 0.4f, 0.8f, 0.9f}` などの直書き色を Visual Language token 経由に置換し、AGENTS.md のレイヤー色ガイドおよびテーマ切替に追従させる
- 対象:
  - `ArtifactCompositionRenderController.cppm`（L37080, L37274-L37304, L37315-L37329 など）
  - Visual Language 整備 [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md) Phase 4（Color Vocabulary）と並走
- 内容:
  - `theme.motionPath.color()` / `theme.motionPath.pastColor()` / `theme.motionPath.futureColor()` / `theme.motionPath.keyframe()` / `theme.motionPath.current()` を `ArtifactCore::currentDCCTheme()` の派生に追加
  - 既存の `pathColor` 直書きを `theme.motionPath.color()` に置換
  - 過去 / 未来の色分け（L37274-L37277）を `theme.motionPath.pastColor()` / `futureColor()` に切替
  - keyframe の補間色（L37315-L37317 の `motionPathInterpolationColor`）は既存実装を維持しつつ、token 化の例外として記録
  - light / dark 両対応と色覚差バリアントは Phase 5 内で basic 対応のみ（本格対応は Visual Language Phase 4 に委譲）
- DoD:
  - 直書きの `FloatColor` が消え、theme 経由の参照に置換される
  - テーマ切替でモーションパスの色が追従する
  - 補間色 (`motionPathInterpolationColor`) のロジックが壊れない

### Phase 6: keyframe 描画の batch 化

- 目的: 各 keyframe につき `drawDashedRectOutline` を 2 回呼ぶ線形増を、N 個を 1 コールで描画する batch 版に置換
- 対象:
  - `ArtifactCompositionRenderController.cppm`（L37319-L37350）
  - `ArtifactIRenderer`（`drawDashedRectOutlines` のような batch API を新設）
- 内容:
  - `ArtifactIRenderer::drawDashedRectOutlines(span<Rect>, span<FloatColor>, lineThickness, dashLen, gapLen)` を新設
  - 内部で instanced 描画（HLSL の `SV_InstanceID` + structured buffer）に切替
  - AGENTS.md「D3D12 / Diligent backend / render path の低レベル実装を変更する場合は、推測で広く触らず、関連箇所を十分に読んで変更範囲を最小化すること」を遵守
  - 既存の単発 `drawDashedRectOutline` は batch 版に delegate する形に
  - 影 + 本体の 2 コールは、batch 1 コールで「影と本体の 2 グループ」を渡す形に統合
- DoD:
  - N 個の keyframe 描画が定数コール数（≤3 コール）になる
  - 100 keyframe でも Frame Debug の draw call 数が劇的に減る
  - 見た目に変化がない（dash パターン、影の濃さ、current 強調）

## Definition Of Done

- 1 フレームに "MotionPath" profile scope が 1 回しか出ない
- 死蔵コード（170 行+）が物理的に消える
- 複数選択（5 レイヤー）で連続 scrubbing 中の `getGlobalTransformAt` 呼び出し回数が選択変更に比例しない
- 例外発生時もレンダラ状態（zoom / pan / canvasSize / externalMatrices）が必ず復元される
- pathColor / pastColor / futureColor / currentColor が theme 経由の参照に置換され、light / dark 両対応
- 100 keyframe の描画が定数コール数で完了する
- 既存 milestone（VP+TL Hot-Path Stability / Timeline DCC-Feel Gaps / Operation Feel / Visual Language）の進行を妨げない

## 既存 milestone との関係

- [MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md) — 本マイルストーンは Phase 1（renderOneFrame 統合）の前段。Motion path のキャッシュが効くことで「renderOneFrame 統合の効果測定」が正確になる
- [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md) — Phase 4（Color Vocabulary）と本マイルストーン Phase 5 が並走。theme token の追加を協調して行う
- [MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md](MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md) — ショートカット経由のモーション表示切替は Operation Feel の Phase 2 と並走可能

## Recommended Order

1. Phase 1（死蔵コード削除）— 即座に効果、ビルド影響最小
2. Phase 2（新実装統合）— Phase 1 と並走可能、profile 汚染を解消
3. Phase 4（scope guard 化）— Phase 2 と並走可能、安全性向上
4. Phase 3（複数選択キャッシュ）— 性能改善の中核、Phase 1〜2 完了後
5. Phase 5（色トークン統合）— Visual Language Phase 4 と並走
6. Phase 6（keyframe 描画 batch 化）— 最後、AGENTS.md の「D3D12 / Diligent backend 触るときは慎重」ルール適用

## 想定効果

- 1 フレームあたりの "MotionPath" 描画コストが **profile 計測で見える化**される
- 複数選択 scrubbing の **>1000ms ボトルネックが解消**
- レンダラ状態の race condition リスクが消え、**ズーム/パン中の安定性向上**
- テーマ切替でモーションパスの色が追従し、**DCC 感ギャップ解消**と整合
- keyframe 描画の **draw call 数が O(N) → O(1)** になり、大量 keyframe ケースの体感性能向上

## Next Execution Slice

Phase 1 の最小着手点:

1. `buildMotionPathSamples` / `MotionPathSample` / `MotionPathSampleKind` シンボルが他で使われていないか grep
2. 使われていなければ、L35704-L35800+ を `git log -p` で安全確認
3. ブロックを物理削除
4. ビルドと Frame Debug で "MotionPath" scope 回数を確認

完了条件: 170 行+ の死蔵コードが消え、ビルドが通り、Frame Debug の "MotionPath" 出現が 1 フレームに 1 回になる。

## 2026-08-30 現状確認

旧コメントアウト実装の削除は見送り、Historical Plane frames の zoom / pan 復元、固定幅フォントの OS 解決、Motion Path のレイヤー別キャッシュまで局所実装済み。renderer の canvas / external matrices 契約、theme / batch、ビルド・runtime 受入れは AGENTS.md に従いユーザー指示待ち。

  - キャッシュ invalidate のトリガを「layer 集合の追加 / 削除 / 順序変化」に絞る
  - キャッシュヒット時は 300-iteration ループをスキップする条件（L37092-L37094 のコメント）を満たすことを単体で確認
- DoD:
  - 複数選択（5 レイヤー）で連続 scrubbing しても `getGlobalTransformAt` の呼び出し回数がレイヤーの追加削除に比例しない
  - 1 レイヤー時の挙動が従来と同一（キャッシュヒット率ほぼ 100%）
  - キャッシュ invalidate の頻度が「選択変更」だけに収束

## 2026-08-30 Update — state restoration slice

Phase 1 の前提だった `buildMotionPathSamples`／`MotionPathSample`／`MotionPathSampleKind` は viewport の hit test と hover 判定から参照されており、死蔵コードとして削除できないことを静的確認した。代わりに Historical Plane frames の描画で renderer の zoom / pan を退避・復元する局所 `RendererPanZoomScope` を導入した。これにより同描画経路内で例外または将来の早期 return が生じても、zoom / pan は復元される。canvas size と external matrices は renderer の読出し API がないため、この slice では推測による復元を行わない。build / runtime / Frame Debug は未実施。

## 2026-08-30 Update — cross-platform frame label font

選択中／hover中のフレームラベルに残っていた `Consolas` 固定を、Qt の `QFontDatabase::systemFont(QFontDatabase::FixedFont)` へ置換した。ラベルのサイズ、色、描画位置、renderer の状態管理は変更していない。ビルド・runtime確認は未実施。
