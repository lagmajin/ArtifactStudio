# M-ANIM-1 Animation Layers Milestone

作成日: 2026-07-07
ステータス: In Progress（コア評価・保存・Undo・Bake 実装済み、UI拡張を継続）
対象: `ArtifactCore/include/Animation/AnimatableValue.ixx`,
      `ArtifactCore/include/Animation/AnimatableTransform2D.ixx`,
      `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`,
      `ArtifactCore/include/Animation/AnimationLayerStack.ixx` (新規),
      `ArtifactCore/src/Animation/AnimationLayerStack.cppm` (新規),
      `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
位置づけ: Maya / MotionBuilder の Animation Layers を 2D モーションデザインに移植。
          `AnimatableValue<float>` / `AnimatableTransform2D/3D` の既存評価パイプラインの上に、
          非破壊の加算合成 / 上書き合成を追加する。
参照:
- `docs/planned/MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` (Animation Layers ❌)
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`
- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`
- `ArtifactCore/include/Rig/Rig2D.ixx` (Bone2D 評価パターン)
- `ArtifactCore/include/Animation/AnimationDynamics.ixx`

### 2026-07-25 実装整理

- `AnimationLayerStackT<float>` の Additive / Override、Weight、Mute、Solo、JSON 保存／復元を実装。
- `ArtifactAbstractLayer` の Opacity／Transform チャンネル評価、Property Editor 表示、コンテキスト操作、Undo を実装。
- 現在フレーム Bake と範囲 Bake を、単一 Override Layer へのキーフレーム縮約として実装。
- レイヤーコンテキストメニューから Work Area 全体を範囲 Bake でき、1回のスナップショット Undo で復元可能。
- タイムライン上の Anim Layer 選択 UI、Merge／Zero Key、ビルド検証は未完了のため本マイルストーンは `done` へは移動しない。

---

## 1. 目的

`MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` のアニメ特化カテゴリ:

> **Animation Layers** — Maya/MotionBuilder 式。複数アニメーションレイヤーを加算/上書き合成。個別に mute/solo/weight 調整。

現在の ArtifactStudio では、レイヤー（`ArtifactAbstractLayer`）の Transform プロパティにキーフレームを
打つと、それが直接最終値になる。これは 1 トラック 1 結果の単純なモデルで、以下のユースケースを満たせない:

- ベースの歩きアニメに、頭の揺れだけ Additive で足したい
- 複数のテイク（Idle, Walk, Run）をオーバーライドで管理したい
- 外注から来た修正アニメを Override Layer としてマージしたい

本 milestone は、`AnimatableValue` の評価結果を **スタック構造** で合成し、
非破壊のレイヤー別ミュート / ウェイト / ソロ を可能にする。

> 重要: これは **レイヤーパネルのレイヤー（`ArtifactAbstractLayer`）とは別概念** である。
> Animation Layer は同一オブジェクト（同一 Bone / 同一 Layer）の
> **プロパティに対して複数のキーフレームセットを重ねる** 仕組み。
> 混乱を避けるため、UI 上は "Anim Layer" と表記する。

> 重要: コアロジックは `ArtifactCore` に自己完結させる。UI 露出とレイヤー管理パネルは
> `Artifact` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。


## 2. 現状整理 (2026-07-07 基準)

### 2.0 2026-07-25 監査メモ

- `AnimationLayerStack` の実装は未確認。
- 次の実装単位は `ArtifactCore` 内の Additive / Override 合成モデルと、weight / mute / solo の評価 API。
- UI と保存形式はコア評価 API の形を確定してから接続する。
- `AnimationLayerStackT<T>`、Additive / Override、weight / mute / solo、フレーム評価を `Animation.Value` に追加済み（source/static verified 2026-07-25）。
- `ArtifactAbstractLayer` が float 用 Animation Layer Stack を保持する公開 API を追加。UI / 保存復元 / 実描画評価への接続は次段階。
- `ArtifactAbstractLayer::opacity()` が現在フレームの Animation Layer Stack を Additive / Override 評価へ通す実評価経路を追加。
- 既存 Property Editor のレイヤーグループに各 Anim Layer の Weight / Mute / Solo / Blend Mode を公開し、編集値を Stack へ反映。
- レイヤーコンテキストメニューから Anim Layer の追加／末尾削除を行える導線を追加。
- Anim Layer の追加／削除を `Change Animation Layers` の一括 Undo コマンドへ統合。
- プロパティパスごとの独立 Stack API と JSON 保存領域を追加し、Transform 等を共有 opacity Stack と混線させない土台を実装。
- `getLocalTransformAt()` の Position X/Y、Rotation、Scale X/Y、Anchor の評価後段へプロパティ別 Stack を接続（空 Stack は既存挙動を維持）。
- Property Editor に Transform 用 Stack の状態グループと、レイヤーコンテキストメニューの一括作成導線を追加。
- 同じコンテキストメニューに Transform 用 Stack の一括削除導線を追加。
- 共有 opacity Stack と Transform 用プロパティ別 Stack の作成／削除を共通スナップショット Undo に統合。
- 各 Anim Layer の `Value (Current Frame)` を Property Editor に公開し、編集値を現在フレームのキーフレームへ書き込む導線を追加。
- 現在フレームの Anim Layer キーフレームに対する補間方式の編集も追加。
- 新規 Additive Layer の初期値を neutral 値（opacity=現在値、scale=1、その他=0）で初期化し、追加直後の不意な変形を防止。
- レイヤーコンテキストメニューに現在フレーム Bake を追加し、共有／プロパティ別 Stack の評価結果を単一 Override Layer へ縮約して出力を維持する。
- キーフレームの補間 enum を Animation Layer JSON に保存／復元するよう修正。

### 2.1 既存資産

| 資産 | ファイル | 内容 |
|---|---|---|
| `AnimatableValue<float>` | `ArtifactCore/include/Animation/AnimatableValue.ixx` | チャンネル単位のキーフレーム補間。`at(RationalTime)` で評価 |
| `AnimatableTransform2D` | `ArtifactCore/include/Animation/AnimatableTransform2D.ixx` | 2D transform のキーフレーム評価 |
| `AnimatableTransform3D` | `ArtifactCore/include/Animation/AnimatableTransform3D.ixx` | 3D transform のキーフレーム評価 |
| `AnimationDynamics` | `ArtifactCore/include/Animation/AnimationDynamics.ixx` | Spring-Damper、Lag Follower |
| `Bone2D::evaluate(time)` | `ArtifactCore/include/Rig/Rig2D.ixx` | ボーン単位の評価入口。Animation Layer 統合の参照パターン |
| Layer Panel | `ArtifactLayerPanelWidget.cppm` | 左ペイン。スタック表示の UI パターンあり |
| `ArtifactTimelineWidget` | `ArtifactTimelineWidget.cppm` | タイムライン中核。キーフレーム表示 / 編集完備 |

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| `AnimationLayerStack` | なし | スタック構造が無い |
| Additive / Override 合成演算 | なし | 複数セットの加算/上書きができない |
| Layer 単位の weight / mute / solo | なし | 個別制御不可 |
| `ArtifactAbstractLayer` への統合 | なし | レイヤーが AnimationLayerStack を保持していない |
| タイムライン上の Anim Layer 選択 UI | なし | どの層のキーフレームを編集しているか不明 |
| 保存 / 復元 | なし | プロジェクト永続化不可 |
| Bake / Merge | なし | 破壊的統合ができない |
| Zero Key | なし | 現在値をキャプチャして Base Layer にできない |

### 2.3 コード検索結果

- `AnimationLayer` → 0 hit（Artifact/, ArtifactCore/ 配下）
- `AnimationLayerStack` → 0 hit
- `additive` + `animation` → 0 hit（実装ロジックとして）
- `AnimatableValue` → 実装済み。`at()` は固定のキーフレームセットから補間

### 2.4 既存 milestone との関係

- `MILESTONE_2D_RIG_SYSTEM_2026-04-15.md` — Phase 1 の Bone アニメ統合で `AnimatableValue` を使用。
  Animation Layer は Rig の上位に自然に乗る
- `MILESTONE_ANIMATION_DYNAMICS_CORE_2026-03-28.md` — Dynamics 評価。Animation Layer より下位で評価
- `MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` — 🔵 アニメ特化 10 件の筆頭
- `MILESTONE_ROADMAP_CURRENT.md` — Priority C（依存重）。本 milestone で依存を具体化する


## 3. Scope / Non-Goals

### Scope

- `AnimationLayerStack` データモデル（`ArtifactCore` 側）
- Additive / Override 合成演算
- Layer 単位の weight (0.0〜1.0)、mute、solo
- `ArtifactAbstractLayer` への `AnimationLayerStack` 保持と評価統合
- タイムライン左ペインの Anim Layer 選択 UI（ドロップダウン or タブ）
- タイムライン右ペインのキーフレーム表示を選択中の Anim Layer に追従
- JSON 保存 / 復元
- Bake Layer / Merge All Layers
- Zero Key（現在値を Base Layer に書き戻し）

### Non-Goals

- `ArtifactAbstractLayer` のレイヤーパネルと混同しない
- Pose Blending（2 ポーズ間の補間）は別 milestone で扱う
- タイムラインクリップ単位の Animation Layer は将来検討
- 3D レイヤーへの Additive 評価は本 milestone ではスコープ外（Transform2D のみ）

---

## 4. Phases

### Phase 1: AnimationLayerStack コアデータモデル (P0, 2 セッション)

- `AnimationLayerStack.ixx` / `AnimationLayerStack.cppm` を `ArtifactCore` に新規追加
- `AnimationLayer` 構造体: `id`, `name`, `weight`, `muted`, `solo`, `blendMode` (Additive/Override)
- `AnimationLayerStack` クラス: `addLayer()`, `removeLayer()`, `setWeight()`, `setMuted()`, `setSolo()`
- `evaluate(RationalTime) -> float` : 各レイヤーの評価結果を Additive 加算または Override で合成
- `evaluateWithBase(RationalTime, float baseValue) -> float` : 外部から Base 値を注入

**Done criteria:**
- `AnimationLayerStack` のインスタンス生成と JSON roundtrip
- 複数 Additive Layer + Override Layer の合成順が正しい / Solo / Mute が期待通り
- 単体テスト完備

### Phase 2: AnimatableValue との統合 (P0, 1 セッション)

- `AnimatableValue<float>` に `AnimationLayerStack` を保持するメンバとアクセサを追加
- `at(RationalTime)` の評価パイプラインの最終段で合成
- Transform の **チャンネル単位** で個別 `AnimationLayerStack` を持つ（Maya 互換）

**Done criteria:**
- `AnimationLayerStack` 未設定時は既存評価と完全一致
- 設定後、Additive 効果が evaluate 結果に反映

### Phase 3: ArtifactAbstractLayer への露出 (P0, 1 セッション)

- `ArtifactAbstractLayer` に `AnimationLayerStack*` アクセサ追加
- `CompositionRenderController::evaluateLayerTransform()` の評価経路に統合

**Done criteria:**
- 2D Layer に Additive Anim Layer を追加し、ビューポートで位置ずれ確認
- Weight 0 → Base のみと一致 / Weight 1 → 完全な Additive 効果

### Phase 4: タイムライン UI (P0, 2 セッション)

- 左ペインに Anim Layer 選択ドロップダウン
- 選択中の Anim Layer に応じて右ペインのキーフレーム表示を切り替え
- Base Layer は背景的にグレーアウト / Anim Layer の追加/削除/リネーム/順序変更

**Done criteria:**
- Anim Layer 切替で右ペインのキーフレームが即座に切り替わる
- Base Layer 編集 → Additive Layer 編集 → 結果確認 の往復が自然

### Phase 5: Weight / Mute / Solo UI + Bake (P1, 1 セッション)

- Weight スライダー、Mute/Solo ボタン
- Bake Layer / Merge All / Zero Key

**Done criteria:**
- Weight で Additive 効果がリアルタイム増減 / Solo ON Layer のみ評価
- Bake → Base に値移動 + 1 undo で復元

### Phase 6: 永続化 + Diagnostics (P1, 1 セッション)

- project JSON に `layer.animationLayerStack` 追加
- 旧プロジェクト後方互換 / Problem View 診断

**Done criteria:**

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_2D_RIG_SYSTEM_2026-04-15.md` | Bone ごとに AnimationLayerStack を持つ拡張先。本 milestone が先に土台を提供 |
| `MILESTONE_ANIMATION_DYNAMICS_CORE_2026-03-28.md` | Dynamics は Animation Layer より下位で評価 |
| `MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` | 🔵 アニメ特化筆頭。本 milestone で実装 |
| `MILESTONE_ROADMAP_CURRENT.md` | Priority C → 本 milestone で依存を解決し Priority A へ昇格候補 |
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | 評価経路に Animation Layer を追加。Controller の low-level call site は増やさない |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **Additive 合成の数値安定性**。`float` の累積加算誤差。`double` 中間バッファで軽減
2. **Override Layer の優先順位**。複数の Override Layer がある場合、最上位のみ有効
3. **チャンネル単位 vs オブジェクト単位**。Transform の各チャンネルに個別 Stack を持たせる設計は柔軟だが UI が複雑化
4. **Dynamics との合成順**。Spring-Damper の減衰が Animation Layer より先か後か。Maya 互換では Dynamics → Animation Layer の順
5. **キーフレームの所属**。Anim Layer 間でのキーフレームコピー/移動は Phase 4 以降

### 6.2 契約上の未解決

- **Weight 0 の扱い**。`weight = 0` は `muted = true` と等価か。Maya では分離（weight=0 でもキーフレーム保持）
- **Additive + Override 混在**。Maya では上から順に Additive 加算 → 最初の Override で打ち切り
- **Bone / Rig との統合**。`Bone2D::evaluate()` に `AnimationLayerStack` をどう渡すか

### 6.3 サブモジュール境界

- `ArtifactCore/include/Animation/AnimationLayerStack.ixx` 新規追加
- `ArtifactCore/src/Animation/AnimationLayerStack.cppm` 新規追加
- `ArtifactCore/include/Animation/AnimatableValue.ixx` に Stack 保持メンバ追加（破壊変更ではない）
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- `AnimationLayerStack` が Additive / Override 合成を正しく評価
- Weight / Mute / Solo が期待通り動作
- `AnimatableValue<float>` の既存評価（Stack 未設定時）が完全一致
- タイムライン左ペインで Anim Layer の選択・切替が可能
- 右ペインのキーフレームが選択中の Anim Layer に追従
- Bake / Merge / Zero Key が undo 対応で動作
- project JSON 保存 → 再読込で全復元 / 旧プロジェクトの後方互換
- Problem View に `anim-layer.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- 既存 `CompositionRenderController` の low-level call site が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-07-07: 初版作成。Maya/MotionBuilder の Animation Layers を ArtifactStudio へ移植する設計。

- 保存→再読込で全復元 / 旧プロジェクトの既存動作維持
