# Milestone: Composition Editor Performance Improvement (M-CP-IMP-1)

**Status:** 部分完了（dirty tracking と制約付き GPU 部分再合成は実装済み。独立 CompositionChangeDetector のレンダー判断への統合、GPU binding／compute最適化、性能測定、runtime検証は未完了）
**最終更新:** 2026-10-02

## 2026-10-02 現行コード再監査

- `Artifact/src/Render/CompositionChangeDetector.cppm` に変更 layer ID の保持、full redraw 判定、reset API が存在することを確認。
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` は `CompositionChangeDetector::markLayerChanged` を呼び出すが、同 detector の `needsFullRedraw`／変更 ID を実レンダー判断に使う経路は確認できない。
- 実際の差分再合成は別の `RenderDamageTracker` 経路で確認できる。レイヤー無効化領域を追跡し、対応可能なレイヤー／合成条件に絞って部分 GPU 再合成を実行し、描画・表示成功後に領域を消費する。非対応条件では全体再描画へ戻す。
- bounded tile plan と readback/presentation 成功後の damage 消費も実装されている。コード上の経路確認であり、性能効果・画質・runtime受入は未検証。
- SRV/UAV batch binding、compute dispatch 最適化、応答性の計測、および GPU runtime 検証も今回確認していない。未実装／未検証として保持する。

判定: **差分更新基盤は実装済み、マイルストーン全体は部分実装。独立 detector の判断統合、GPU binding／compute 最適化、性能計測／runtime受入が残るため完了マークは付けない。**

## 2026-08-15 現行コード監査

Composition render widget の dirty flag／coalesced update、layer dirty flags、layer surface／GPU texture cache、render key、interactive downsample、TransformGizmo／overlay cache、render queue の単一行更新は現行コードで確認できる。したがって cache／invalidation／UI 更新抑制の基盤は実装済みである。

一方、独立した `CompositionChangeDetector` による変更レイヤー単位の差分レンダー契約、LayerBlendPipeline の要求単位での SRV/UAV batch binding、texture size に応じた compute dispatch 最適化、50% 応答性目標の計測、GPU runtime 検証は確認できない。キャッシュの存在だけでは部分再描画の完了とは判定しない。

判定: **差分更新・cache・debounce 基盤は部分実装、Phase 1 の独立 change detector、Phase 2〜3 の GPU 最適化、性能計測／runtime 検証は pending。**
## 🎯 目的
Composition Editor のUI応答性を向上させ、レイヤー操作時のパフォーマンスを最適化する。

## 🏗️ 実装内容

### Phase 1: 差分レンダリング実装 (仮説1)
- [ ] CompositionChangeDetector クラスの実装
- [ ] レイヤー変更検出システムの追加
- [ ] 変更レイヤーのみ再描画機能
- [ ] 全Composition再レンダリングの回避

### Phase 2: SRV/UAV バインディング最適化 (仮説5)
- [ ] LayerBlendPipeline のバッチ処理実装
- [ ] 複数レイヤーの同時バインド最適化
- [ ] GPUパイプライン効率化
- [ ] リソースバインディングオーバーヘッド削減

### Phase 3: Compute Shader ディスパッチ最適化 (仮説2)
- [ ] テクスチャサイズに応じた動的スレッドグループ調整
- [ ] GPU占有時間の最適化
- [ ] 小規模テクスチャの処理効率化

## 🚀 期待される成果
- **UI応答性向上**: レイヤー操作時の遅延を50%削減
- **GPU効率化**: 不要な再描画とバインディングオーバーヘッドを削減
- **全体パフォーマンス**: Composition Editor の快適な操作性を実現

## 🔗 関連ドキュメント
- [Composition Editor Performance Hypotheses](docs/bugs/COMPOSITION_EDITOR_PERF_AND_COMPUTE_HYPOTHESES_2026-03-24.md)
- [LayerBlendPipeline](ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx)
- [Composition Editor Playback Feel Refinement](docs/planned/MILESTONE_COMPOSITION_EDITOR_PLAYBACK_FEEL_REFINEMENT_2026-04-23.md)

## 📅 実装スケジュール
- **Phase 1**: 2026-03-31 - 2026-04-02 (差分レンダリング)
- **Phase 2**: 2026-04-03 - 2026-04-05 (バインディング最適化)
- **Phase 3**: 2026-04-06 - 2026-04-08 (Compute最適化)</content>
<parameter name="filePath">docs/planned/MILESTONE_COMPOSITION_EDITOR_PERFORMANCE_IMPROVEMENT_2026-03-31.md
