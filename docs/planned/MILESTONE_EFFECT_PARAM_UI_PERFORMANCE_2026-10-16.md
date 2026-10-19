**最終更新:** 2026-10-16

# Effect Parameter UI Performance — Phase 0 / Phase 1 Implementation Plan

対象: Inspector のエフェクトパラメータ（スライダ／数値）ドラッグ時に
ArtifactStudio 全体が重くなる問題に対する、計測（Phase 0）と低リスク改善（Phase 1）
の具体実装計画。本ドキュメントは仕様であり、実装は未着手。

---

## 1. 症状

- エフェクトパラメータを調整している間、UI（Inspector 操作、ビューポート、場合により全体）
  がもたつく / フレームが落ちる。
- レイヤー transform など他プロパティより重いという体感がある。

## 2. 目的（このフェーズの範囲）

- **Phase 0**: 挙動を一切変えずに、「ドラッグ1ステップあたりのコスト内訳」を実測できる
  計測を追加する。計測結果に基づいて Phase 1 の採否・優先度を決める。
- **Phase 1**: レンダリングアーキテクチャ・undo の意味論を変えずに適用できる、
  独立した小差分の改善計画を定義する。

Phase 2〜4（dependency-aware ROI / effect-node 中間キャッシュ、プレビュー mode 確定と
per-node 世代、レンダラスレッド分離）は本ドキュメントのスコープ外。別計画とする。

---

## 3. 現状の確定事項（コード根拠）

### 3.1 UI 経路
- Inspector のエフェクトパラメータ UI は専用ウィジェットではなく
  `ArtifactInspectorWidget::Impl::effectPropertyWidget`（= `ArtifactPropertyWidget`）が担う。
  - 生成: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm:1699`
  - 合成エフェクト設定: 同 `:1871` / レイヤー設定: 同 `:1950`
- 実体の編集経路は `ArtifactPropertyWidget.cppm` 内の 2 つの near-duplicate ループ。
  - Layer エフェクト: `:3555` → `addRowsFromProperties`（`:3664`）、commit/preview ラムダは `:3666-3695`。
    `hasFocusedEffect && effectID != focusedEffectId` でフィルタ（`:3558`）。
  - Composition エフェクト: `:1854` → `:1959-` 付近。
- commit/preview はいずれも `effect->setPropertyValue` を **同期で直接** 呼び、
  `commitEffectPreview` / `scheduleEffectPreview`（`:565-597`）で dirty + notify する。
- Layer transform 行は `beginValueEdit` / `cancelValueEdit` を渡す（`:3106-3107`, `:3237-3238`）が、
  エフェクト行（`:3664`）は `beginValueEdit` / `cancelValueEdit` を **既定 `{}` のまま**。

### 3.2 render への波及
- `notifyLayerPropertyPreviewChanged` / `notifyLayerPropertyAnimationChanged` は
  `layer->setDirty(LayerDirtyFlag::Effect); layer->changed();`
  （`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm:1003-1020`）。
- Effect dirty は lightweight 合成から除外され、
  `invalidateLayerSurfaceCache`（`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:18408`）
  で full / draft サーフェスを破棄、世代を進める（`:19146-19162`, `:18408-18445`）。
- 加えて full-frame effect / 3D / mask / 非 Normal blend は
  `recordLayerDamage` で `requiresFullRedraw=true`（`:18497-18509`）。
  full-frame 判定は `ArtifactAbstractLayer::hasEnabledFullFrameEffect`
  （`Artifact/src/Layer/ArtifactAbstractLayer.cppm:2718`）。
- 描画は GUI スレッドで実行（`QMetaObject::invokeMethod(this, ...)` `:19518`）。

### 3.3 間引きと緩和
- スライダ preview スロットル: 16ms（`Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorNumeric.cppm:69`
  `kSliderPreviewIntervalMs`）。
- effect preview coalesce: 33ms singleShot（`ArtifactPropertyWidget.cppm:580`）。
  → preview と slider で二重の間引きが存在。
- 選択レイヤー限定の緩和: `viewportInteracting_` で LOD 低下＋effect 解像度 0.25
  （`:13470`, `:13505-13512`）。

### 3.4 undo 経路
- 正規の undo 付き API は別に存在する:
  `ArtifactEffectService::setEffectProperty`（`Artifact/src/Service/ArtifactEffectService.cppm:1612`）と
  `setCompositionEffectProperty`（`:1691`）。いずれも `SetPropertyCommand`
  （`Artifact/src/Undo/UndoManager.cppm:996`）を push し、`LayerChangedEvent` /
  `EffectServiceChangedEvent` を post する。
- 一方 3.1 の直接経路は undo コマンドを積まない（enable/remove ボタンのみ service 経由）。
  → **エフェクト param の値編集は現状 undo 履歴に入らない**（既存ギャップ。3.1/3.4 の経路差に起因）。

### 3.5 既存計測基盤
- `interactionPerfEnabled_`（marker file `ArtifactViewportPerf.enable`, `:16806`）、
  `interactionPerf_` / CSV 出力（`:20422`）、`viewportInteractionIdleMs_ = 120`（`:16831`）。
- `layerSurfaceCacheInvalidationCounts_` / `layerSurfaceCacheDiagnosticsEnabled_`
  （`:18418-18426`, `:16700`）。
- 起動フラグ契約: `docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md`
  （JSON > 環境変数 > 既定、起動時1回読み、`Group/Name` 形式、ホットパスで再読しない）。

---

## 4. Phase 0 — 計測（挙動不変 / 最優先）

### 4.1 目的
以下を分離して実測する。
1. 1 ドラッグステップの CPU コスト（`setPropertyValue` 実時間）。
2. notify（`setDirty` / `changed()`）の実時間。
3. `LayerDirtyFlag::Effect` 起点の surface cache invalidation 回数。
4. full redraw へ昇格した比率。
5. GUI スレッドのブロック時間（1 preview / 1 commit あたり）。
6. coalesce で間引かれた preview 数 vs 実 notify 数。

### 4.2 フラグ
- marker 方式（既存 `ArtifactViewportPerf.enable` と同型）を採用:
  exe 隣の `ArtifactEffectParamPerf.enable`。
- または `ArtifactStartup.json` の `Diagnostics/EffectParamPerf`（契約に準拠）。
- 起動時 1 回読み、`bool` メンバに保持する。**ホットパスでは読まない。**
- 無効時は `QString::arg` / 文字列連結 / ストリーム整形を一切実行しない（
  `docs/technical/HOT_PATH_RULES.md` 準拠、遅延評価）。

### 4.3 計測点（提案）
| # | 箇所 | 計測内容 |
|---|------|----------|
| 1 | `ArtifactPropertyWidget.cppm:3666-3695`（layer）/ `:1959`（comp） | `setPropertyValue` 所要時間と呼び出し回数 |
| 2 | `scheduleEffectPreview` / `commitEffectPreview` `:565-597` | 間引き数 / 実 notify 数 |
| 3 | `ArtifactPropertyWidgetShared.cppm:1003-1020` | `setDirty` と `changed()` の時間分離 |
| 4 | `ArtifactCompositionRenderController.cppm:18408` / `:18497` | invalidation 回数 / `requiresFullRedraw` 比率（既存 `counts_` 流用） |
| 5 | `:19518` → フレーム終了 | GUI ブロック時間（max/avg） |
| 6 | `:20422` と同方式 | 出力シンク（CSV / カウンタ） |

計測は既存の `interactionPerf_` 基盤への相乗り、または並列シンクで実装する。
バッファは固定容量・bounded とし、溢れはカウントのみ（dropped）とする。

### 4.4 完了条件 / リスク
- フラグ OFF 時に計測可能なオーバーヘッドがゼロ（`bool` 読みのみ）。
- フラグ ON で CSV に上記 6 項目が出る。
- 経路は不変。リスクはほぼゼロ。

---

## 5. Phase 1 — 低リスク改善（Phase 0 の結果で採否判断）

各項目は独立適用可能な小差分として扱い、Phase 0 の数値を根拠に取捨する。

### 1.1 preview/commit の分離と間引き統一
- 変更: `ArtifactPropertyWidget.cppm:565-597` 付近。effect preview を単一 coalescer に寄せ、
  未確定 invalidation を 1 本化。commit は即時維持。
- 根拠: 現状 16ms（slider）+ 33ms（effect）の二重間引き（3.3）。
- リスク: 中。体感挙動が変わるため Phase 0 の後。

### 1.2 エフェクト param の undo / 編集セッション整備
- 変更: `ArtifactPropertyWidget.cppm:3664` に `beginValueEdit` / `cancelValueEdit` を配線。
  値は `SetPropertyCommand`（`UndoManager.cppm:996`）で。
- 位置づけ: 現状 undo が積まれない既存ギャップの修正を兼ねる（3.4）。
- 代替（より安全）: commit ラムダを `ArtifactEffectService::setEffectProperty` /
  `setCompositionEffectProperty` に委譲。既に undo＋イベント込みだが、
  post されるイベントが増え 1 ステップ負荷が増える可能性 → 要計測。
- リスク: 中。**3.4 の経路差が意図的か未確認のため、実装前に確定させる**（§6）。

### 1.3 ローカル編集で full 破棄を避ける（render 最小差分）
- 変更: `ArtifactCompositionRenderController.cppm:18408` / `:18497` 付近。
  transient な effect preview 中（`viewportInteracting_` 時）は draft cache を保持し、
  full cache clear / `requiresFullRedraw` を commit 時のみに限定。
- 根拠: Effect dirty が full/draft 全破棄＋full redraw に直結（3.2）。
- 位置づけ: 最高価値だが render controller 変更のため Phase 0 の証拠が前提。
- リスク: 中〜高。Diligent/DX12 は最小差分で。

### 1.4 冗長 notify の削減
- 変更: `ArtifactPropertyWidgetShared.cppm:1003-1020` 付近。
  preview 経路が `ProjectChangedEvent` 等の重い伝播を誘発していないか確認し、
  ドラッグ中は抑制。
- リスク: 低〜中。

---

## 6. 未確認事項（Phase 1 着手前に確定）

1. **undo 経路差の意図**: 3.1 の直接経路が undo 無しなのが意図的なのか、別ハンドラが
   補完しているのか。1.2 の前に確定。→ 現時点の所見では補完は見当たらない（未検証）。
2. **Inspector のフィルタ**: layer エフェクトループ（`:3555`）の `focusedEffectId` フィルタは
   確認済み（`:3558`）。comp 側（`:1858`）も同様。追加の未確認はなし。

---

## 7. 実施順序

1. Phase 0 計測実装（フラグ OFF でゼロオーバーヘッドを確認）。
2. 計測 → §5 の閾値判断（どこが支配的か）。
3. Phase 1 を 1.1 → 1.4 の順で、独立に適用・計測比較。
4. 1.3 は render 影響が確認できた場合のみ。
（ビルド・実機確認はユーザーの明示指示後に実施。）

---

## 8. 参照ファイル

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`（effect 行 `:3555-3726`, preview `:565-597`,
  layer 編集セッション `:2705`, `:2739`, `:2775`, `:2989`）
- `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`（`createPropertyRow:1270`,
  `addRowsFromProperties:429`, `notify*:1003-1020`）
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorNumeric.cppm:69`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  （`:16700`, `:16806`, `:18408`, `:18497`, `:19146-19162`, `:19518`, `:20422`）
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm:2718`
- `Artifact/src/Effects/ArtifactAbstractEffect.cppm:876,881`
- `Artifact/src/Service/ArtifactEffectService.cppm:1612,1691`
- `Artifact/src/Undo/UndoManager.cppm:996`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm:1699,1871,1950,7339`
- `docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md`
- `docs/technical/HOT_PATH_RULES.md`
