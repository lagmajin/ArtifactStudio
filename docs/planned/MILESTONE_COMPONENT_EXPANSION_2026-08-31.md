# MILESTONE: レイヤーコンポーネント到達性・Runtime Parity

**最終更新:** 2026-08-31  
**ステータス:** Planned — `MILESTONE_LAYER_COMPONENT_SYSTEM_2026-04-18.md` の follow-up

## 目的

既存の built-in layer component を、Components 専用面から依存関係を壊さず有効化でき、各コンポーネントの有効状態が既存の正規 runtime 経路で意味のある結果になる状態へ揃える。

これは新しいコンポーネント基盤や simulation world を作る計画ではない。`ArtifactAbstractLayer` の component host と `ArtifactAbstractComposition` の authoritative component simulation を正として、未接続の UI と runtime parity を補完する。

## 現状（2026-08-31 静的確認）

### Built-in descriptor

`ArtifactLayerComponentSystem.ixx:725-866` には次の **10 種**の descriptor factory がある。

| Phase | Component | 現状 |
|---|---|---|
| Generate | Cloner | Components UI・描画経路あり。別途 Cloner 統合計画の対象。 |
| Arrange | Layout | UI・簡易 grid 配置あり。mode parity は未完了。 |
| Intent | Crowd | 設定と簡易評価、authoritative simulation あり。Inspector 追加導線がない。 |
| Drive | Motion Dynamics | 既存の layer frame evaluation で動作。descriptor との完全な責務統一は未完了。 |
| Drive | Sequence Player | 常に disabled の placeholder。runtime は未実装。 |
| Dynamics | Collision | 設定・authoritative AABB collision・contact event あり。Inspector 追加導線がない。 |
| Dynamics | Joint | composition の既存 joint constraint 経路あり。Inspector 追加導線がない。 |
| Topology | Fracture | 既存 fracture state/event 消費経路あり。Inspector 追加導線がない。 |
| Emit | Particle Emitter | collision event からの spawn と既存 particle state 経路あり。Inspector 追加導線がない。 |
| Dynamics | Fluid | Components UI と既存実装あり。本計画の対象外。 |

Physics と Script はこの descriptor 群とは別の既存 Components 導線で扱う。

### 既存の正規 runtime

- `ArtifactAbstractComposition::evaluateLayerComponentSimulation()` は既に crowd、composition-floor / instance AABB collision、contact、fracture event、particle spawn、runtime snapshot、seek restore を処理する。
- `evaluateJointConstraints()` は既存の composition 側 joint constraint 経路である。
- Motion Dynamics は `ArtifactAbstractLayer` の frame evaluation 内に実装済みである。
- `LayerComponentHost` は required component の validation を持つ。Crowd / Collision / Particle Emitter は Cloner、Joint は Collision を prerequisite とする。

したがって「evaluator 本体を新設する」「Joint を新規に composition へ統合する」「Motion Dynamics をゼロから分離する」は本計画の対象ではない。

## ゴール

1. Collision / Crowd / Particle Emitter / Joint / Fracture / Motion Dynamics を Components 専用面から選択・有効化できる。
2. UI が prerequisite を可視化し、無効な component graph を作らない。
3. 既存 authoritative simulation と preview extraction が同じ component enabled state を参照する。
4. Layout の既存設定が各対応 mode で一貫して反映される。
5. Placeholder の Sequence Player を隠すか、別計画へ移管する判断を完了する。

## 非ゴール

- 新しい simulation world / evaluator の新設
- Boids / RVO solver の新設、CrowdSimulator2D の新規 module 化
- 新規 Audio React / Time Remap / State Driven component の追加
- 動画読み込み・動画デコード、ソフトレンダラーの新機能追加
- `ArtifactWidgets`、`libs/...`、`third_party/*` の変更

Audio Reactive は `MILESTONE_AUDIO_REACTOR_SYSTEM_2026-03-30.md`、Time Remap は `MILESTONE_TIME_REMAP_CURVE_UI_2026-06-02.md` で扱う。新規 component の提案は既存の binding / TimeRemap / composition state の責務を確認した別マイルストーンにする。

## 実施フェーズ

### Phase 0: 真実表と移行境界の確定

各対象について、descriptor typeId、legacy property path、専用 UI、prerequisite、正規 runtime consumer、保存復元、preview / render extraction を対応表にする。

完了条件:

- descriptor 10 種と Physics / Script の別系統を混同しない。
- 対象 6 種の正規 property path と runtime consumer を特定する。
- `enabledLayerComponents(phase)` を使える箇所と legacy property を維持すべき箇所を明記する。
- 本計画と Cloner / Physics / Particle / Audio / Time Remap の既存計画の重複を除く。

### Phase 1: Components 専用 UI の到達性と依存解決

`ArtifactInspectorWidget` の Add Component メニューと active component 切替に、Collision / Crowd / Particle Emitter / Joint / Fracture / Motion Dynamics を追加する。

- コンポーネントの設定は通常 Property Widget へ露出せず、Components 専用面を正規導線とする。
- prerequisite を満たさない選択は、必要 component を明示して自動有効化するか、理由を表示して追加を抑止する。動作しない enabled state を作らない。
- Crowd は既存 `ArtifactCrowdSettingsWidget` を接続する前に、同 widget と component property の source-of-truth を確認する。二重保存・二重更新を作らない。
- Fracture / Particle Emitter は単に有効化できるだけでなく、発火条件（contact）が必要であることを UI で説明する。

完了条件:

- 対象 6 種を Components 専用面から追加・選択・無効化できる。
- Crowd / Collision / Particle Emitter / Joint の prerequisite が UI 操作だけで常に有効か、明確に診断される。
- Components 専用面以外にコンポーネント由来の group を通常 Property Widget へ新規露出しない。

### Phase 2: 既存 authoritative evaluator の parity hardening

新設ではなく、`evaluateLayerComponentSimulation()`、`evaluateJointConstraints()`、`ArtifactCloneEffectSupport` の既存責務を整理する。

- descriptor / legacy property の二重状態を Phase 0 の対応表に従って一本の enabled state として扱う。
- preview extraction が authoritative state のあるフレームでそれを優先し、同じ Crowd / Collision 計算を重複適用しないことを確認する。
- snapshot、discontinuous seek、component 有効切替後の state invalidation を既存挙動に合わせて確認する。
- Joint と Motion Dynamics は挙動の置換を目的にせず、既存の authority と順序を維持したまま descriptor との対応を明文化する。

完了条件:

- Crowd / Collision / Fracture / Particle Emitter の enabled state が preview と authoritative simulation で矛盾しない。
- Joint / Motion Dynamics を含め、既存の実行順序と物理 state ownership を壊さない。
- 全フレームで再計算する新しい evaluator や並列の simulation state を導入しない。

### Phase 3: 個別機能の完成度

優先順位は静止画・連番画像・シェイプレイヤーで効果が確認しやすいものを優先する。

1. **Layout** — 現在の実装と UI が示す mode の対応を確定し、未対応 mode は明示的に無効化するか実装する。`layoutGap` / `layoutMaxPerRow` を各対応 mode で一貫して反映する。
2. **Crowd** — 既存の deterministic steering を基準に、cohesion / separation / alignment / maxSpeed / jitter が結果と保存再読込に反映されることを固める。本格 Boids / RVO は別計画にする。
3. **Collision → Fracture / Particle Emitter** — contact から topology / emit event が一度だけ消費され、seek / restore で二重発火しないことを確認する。
4. **Sequence Player** — runtime を伴う独立計画へ移すまで、Components UI には公開しない。placeholder を通常の「未対応コンポーネント」と誤認させない。

## 確認方法

ビルド・テスト・runtime 実行はユーザー許可後に行う。実行前の静的確認と、許可後の runtime 受入れを分ける。

| 段階 | 確認 |
|---|---|
| 静的 | Inspector の action / property path、descriptor prerequisite、runtime consumer、JSON 保存復元の経路が一致する。 |
| UI runtime | 対象 6 種を Components 専用面から操作し、依存不足が自動解決または明確に診断される。 |
| Simulation runtime | Crowd / Collision / Fracture / Particle Emitter で preview と authoritative state が同一フレームで一致し、seek 後に event が二重発火しない。 |
| 回帰 | Motion Dynamics、Joint、Cloner、Fluid の既存操作・保存再読込に回帰がない。 |

## 影響範囲

- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/ArtifactCrowdSettingsWidget.cppm`（source-of-truth が一致すると確認できた場合のみ）
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`
- `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`

新規 `.ixx` / `.cppm` はこのマイルストーンの前提にしない。必要性が確認された場合のみ、module dependency と CMake 登録を別途設計確認する。

## 関連マイルストーン

- `docs/planned/MILESTONE_LAYER_COMPONENT_SYSTEM_2026-04-18.md` — 基盤と統合先
- `docs/planned/MILESTONE_CLONER_UNIFICATION_2026-08-23.md` — Cloner の正規経路
- `docs/planned/MILESTONE_PHYSICS_PRODUCTION_HARDENING.md` — solver 品質・GPU 化
- `docs/planned/MILESTONE_AUDIO_REACTOR_SYSTEM_2026-03-30.md` — Audio Reactive binding
- `docs/planned/MILESTONE_TIME_REMAP_CURVE_UI_2026-06-02.md` — Time Remap
