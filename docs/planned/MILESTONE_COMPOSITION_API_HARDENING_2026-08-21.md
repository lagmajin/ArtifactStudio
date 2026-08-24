# マイルストーン: Composition API Hardening

**最終更新:** 2026-08-21
**ステータス:** Not Started
**優先度:** High
**関連:** `docs/CORE_MODULE_QUALITY_AUDIT_2026-08-06.md`, `docs/planned/MILESTONE_EXTERNAL_CONTROL_MIDI_OSC_2026-07-25.md`, `docs/planned/MILESTONE_COMPOSITION_FINAL_EFFECT_2026-04-14.md`, `docs/planned/MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-07-21.md`, `docs/planned/MILESTONE_EXPRESSION_ENGINE_COMPLETION_2026-08-08.md`

## 目的

コンポジションAPI監査(2026-08-21)で判明した構造的問題のうち、軽微な修正は即時対応済みとし、残る契約違反・データ消失・未接続資産を段階的に解消する。

## 即時対応済み(2026-08-21)

- **revision 契約修復**: `appendLayerTop/Bottom`、`removeLayer`、`removeAllLayers`、`moveLayerToIndex`、`bringToFront`、`sendToBack` に `owner_->changed()` を追加(`ArtifactAbstractComposition.cppm`)。レイヤー構造変更でリビジョンが進むようになり、RAMプレビュー等のキャッシュ依存スタンプが信頼できる形に戻った。
- **OSC/MIDI 起動配線**: `ArtifactProjectService::Impl::setupExternalControlInputs()` を新設し、コンストラクタから起動。OscInput(ポート8000)と最初に開けたMIDIデバイスの CC を `ExternalControlManager::observeInput()` へ接続。マッピング済みアドレスは既存の `applyExternalControlValue()` 経路でコンポジションへ反映される。
- **stop() の範囲不一致修正**: `ArtifactCompositionPlaybackController::stop()` の復帰フレームを `frameRange_.start()` から `effectiveStartFrame()`(IN点考慮)へ変更。`goToStartFrame()` と挙動が揃った。
- **再生ホットパスの qDebug 抑制**: `goToFrame` / `onTimerTick` の毎フレームログを削除。

## 残存課題(優先度順)

### P1 — データ消失・契約問題

| # | 項目 | 現状 | 根拠 |
|---|---|---|---|
| 1 | fromJson が基底クラスを直接生成し具象型(2D/3D)が失われる | `makeShared<ArtifactAbstractComposition>` 固定。3Dカメラ4項目も非シリアライズ | `ArtifactAbstractComposition.cppm:4729` 付近、`ArtifactComposition3D.cppm` |
| 2 | CompositionContext(SimulationSettings)が非シリアライズ | toJson に項目自体がない | 同上 |
| 3 | プリコンポ時間オフセット断線 | remap 無効時に startTime/inPoint オフセットが適用されず、プリコンポレイヤーをスライドしても子がずれない | `ArtifactAbstractLayer.cppm:3424-3427`、`ArtifactCompositionViewDrawing.cppm:2085-2098` |
| 4 | unprecompose が同一子コンポを持つ他プリコンポの帳簿を壊す | `childSourceMap.remove(childCompId)` が全値削除 | `PreCompose.cppm:188` |

### P2 — スレッド安全性

| # | 項目 | 現状 |
|---|---|---|
| 5 | コンポジションドメイン全体が無防備 | 5,031行の本体に mutex/atomic ゼロ。レンダーワーカー・GPUプリコンポパスが `child->goToFrame()` 等を直接呼ぶ |
| 6 | レンダーワーカー向け不変スナップショット契約 | 監査文書の方向性(audit doc:114)だが未設計 |

### P3 — 未接続資産の活用・整理

| # | 項目 | 現状 |
|---|---|---|
| 7 | DAG 評価基盤の実描画接続 | EffectGraph/DAGExecutor/CompositionGraphBuilder はほぼ完成だが実描画は即時モードで並行稼働 |
| 8 | 式のクロスレイヤー/コンポジション属性参照 | Copilot サンドボックス限定。実描画パス未注入(Gap 1-4 文書化済み) |
| 9 | CompositionFinalEffectStack の統合判断 | 実働 Rasterizer スタックと型のみの Core/GPU 体系が並立 |
| 10 | プリコンポ子の完全合成描画パス | 複雑な子は最前面レイヤー1枚のサムネイル近似(QImage)。多段フレームキャッシュ不在 |
| 11 | 死蔵コード削除 | `IComposition2D.ixx`、空の CompositionBuffer2D 系、`ArtifactCompositionSetting.cppm`(1行)、`ArtifactCompositionManager.hpp`(存在しない presetHD を呼ぶ) |

### P4 — Manager/API 整備

| # | 項目 | 現状 |
|---|---|---|
| 12 | Manager に単一削除・ソートなし | `removeComposition(id)` 不在(search はフィルタのみで sortField 未実装) |
| 13 | 相対挿入API(特定レイヤーの直上/直下)不在 | 絶対 index 指定のみ |
| 14 | OSC/MIDI の設定 UI・ポート設定永続化 | 今回の配線はデフォルト値固定。ユーザー設定可能にするには UI と保存が必要 |

## 実施フェーズ

### Phase 1 — データ消失解消(P1)

- fromJson をファクトリ化し、type フィールドから 2D/3D を復元する。3Dカメラ項目と CompositionContext を toJson/fromJson に追加する。
- `getSourceFrameAtCompFrame` の remap 無効フォールバックに startTime/inPoint オフセットを実装する(VideoLayer の `timelineFrameToSourceFrame` と同じ式)。
- unprecompose の帳簿削除を key+value 指定に変更する。

### Phase 2 — スレッド安全性(P2)

- まず競合の実測(TSAN またはストレステスト)で被害範囲を確定する。
- レンダーワーカー向けの不変スナップショット(レイヤー列+評価に必要なプロパティ値)を設計する。UI スレッドでのみ可変、ワーカーは読み取り専用ビューを受け取る形。
- `goToFrame` 等のクロススレッド呼び出し箇所を洗い出し、キューイングか分離に寄せる。

### Phase 3 — 資産活用(P3)

- DAG 接続は既存描画経路を壊さないよう、まず「DAG 評価結果と即時モード結果の parity 検証ハーネス」から始める。
- 式の thisComp/他レイヤー注入は `MILESTONE_EXPRESSION_ENGINE_COMPLETION` の Gap 順に従う。
- FinalEffectStack は統合 or 削除を設計レビューで決める。

### Phase 4 — 整備(P4)

- 死蔵ファイル削除(CMake 登録確認の上)。
- Manager へ `removeComposition(id)` とソート実装。
- OSC/MIDI のポート・デバイス選択 UI と設定永続化。

## 対象外

- Group Layer(別計画 `docs/group-layer-plan.md`)
- Mask/Roto 編集のモーダル責務整理(COMPOSITION_EDITOR_CONTRACT の Next Step)
- macOS/Linux の MIDI バックエンド

## リスクと確認方法

- **revision 追加によるイベント増加**: `changed()` は `CompositionChangedEvent` を発行するため、レイヤー追加頻度の高い操作(Undo/Redo、プロジェクト読込)でイベント量が増える。読込時は `suppressLayerChangedEvents_` で抑止済み。確認はプロジェクト読込・Undo/Redo 後のプレビューキャッシュ再生成回数。
- **fromJson ファクトリ化は既存プロジェクト互換が要**: type フィールドがない旧JSONは基底クラスへフォールバックする。確認は旧プロジェクトの再読込。
- **プリコンポ時間オフセット実装は既存プロジェクトの見た目を変え得る**: 現在「スライドしても子がずれない」状態で作られたプロジェクトでは、修正後に位置が変わって見える。確認は既存プリコンポ使用プロジェクトの再生比較。
- **OSC ポート8000は慣習的ポート**: 他アプリとの衝突時は qWarning が出て無効になるだけ(起動は阻害しない)。設定 UI は Phase 4。
- **ビルド・テスト実行**: AGENTS.md 制約によりユーザー指示が必要。
