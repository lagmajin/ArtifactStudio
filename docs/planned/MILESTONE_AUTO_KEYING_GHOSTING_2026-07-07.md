# M-AK-1 Auto-Keying + Keying Sets + Ghosting Milestone

作成日: 2026-07-07
ステータス: Draft
対象: `ArtifactCore/include/Animation/AnimatableValue.ixx`,
      `ArtifactCore/include/Animation/AnimatableTransform2D.ixx`,
      `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`,
      `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Widgets/ArtifactPlaybackShortcuts.cppm`
位置づけ: Maya の Auto-Keying / Keying Set / Ghosting を 2D モーションデザインに移植。
          アニメーション作業のループ効率を大幅に向上させる基盤機能群。
参照:
- `docs/planned/MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` (Auto-Keying ❌, Keying Set ❌, Ghosting ❌)
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (Roving Keyframes / Motion Sketch)
- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `Artifact/src/Widgets/ArtifactPlaybackShortcuts.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (Onion Skin 既存)

---

## 1. 目的

Maya のアニメーション作業ループで標準的な以下の 3 機能を ArtifactStudio に移植する:

### Auto-Keying (Maya `S` キー状態)
ON 時、パラメータを変更するたびに自動でキーフレームを生成。
現在は `F9` 等のショートカットで手動キーイングのみ。

### Keying Set (Maya Channel Box 連携)
どのプロパティにキーを打つかをプリセット保存。
"Transform Only", "All Keyable", "Custom" など。
Key Selected（選択チャンネルのみ）と組み合わせて使う。

### Ghosting / Motion Trail (MotionBuilder / Maya)
タイムライン上に前後 N フレームの半透明オーバーレイ（Ghosting）、
またはビューポート上のモーショントレイル残像（Motion Trail）。

> 重要: これら 3 機能は個別の milestone に分割せず、
> 「アニメーション作業のループ効率向上」という共通目的で 1 milestone に束ねる。
> いずれも既存の `AnimatableValue` / `OnionSkin` / `PlaybackShortcuts` に上積みする。

---

## 2. 現状整理 (2026-07-07 基準)

### 2.1 既存資産

| 資産 | ファイル | 内容 |
|---|---|---|
| `AnimatableValue<float>` | `ArtifactCore/include/Animation/AnimatableValue.ixx` | `setKeyframeAt(time, value)`, `hasKeyframeAt(time)` |
| `AnimatableTransform2D/3D` | `ArtifactCore/include/Animation/` | チャンネル単位のキー管理 |
| Onion Skin | `ArtifactCompositionRenderController.cppm` | `onionSkinFrames_`, `onionSkinOpacity_`（ビューポート） |
| `ArtifactPlaybackShortcuts` | `ArtifactPlaybackShortcuts.cppm` | ショートカットルーティング |
| `ArtifactTimelineTrackPainterView` | `ArtifactTimelineTrackPainterView.cppm` | owner-draw キーフレーム描画 |

### 2.2 不足

| 軸 | 状況 |
|---|---|
| Auto-Keying モード状態管理 | なし |
| 値変更検出と自動キー挿入 | なし |
| Keying Set プリセット管理 | なし |
| タイムライン Ghosting 描画 | なし（ビューポート Onion Skin のみ） |
| Motion Trail 描画 | なし |
| J/K キーフレームジャンプ | なし |

Implementation note:

- `Artifact/src/Service/ArtifactPlaybackShortcuts.cppm` では `J/K/L` が shuttle 系に割り当て済み
- `ArtifactCore/include/UI/ShortcutBindings.ixx` には `AnimationGoToNextKeyframe` / `AnimationGoToPreviousKeyframe` が既にある
- そのため Phase 4 の keyframe jump は、既存 shuttle を壊さない経路で追加するのが前提になる
- Ghosting は `ArtifactCompositionRenderController.cppm` の onion-skin 経路が土台なので、timeline ghosting はそこから派生した表示面として実装する

---

## 3. Scope / Non-Goals

### Scope

- Auto-Keying ON/OFF トグル（グローバル + per-layer）
- `AnimatableValue::setValue()` のフックに auto-key 挿入
- Keying Set プリセット: "All Keyable", "Transform Only", "Custom"
- Keying Set 編集 UI（プロパティのチェックリスト）
- タイムライン Ghosting: 前後 N フレームを半透明キーフレームとして表示
- ビューポート Motion Trail: 指定フレーム数分の過去位置をラインで接続
- J (prev keyframe) / K (next keyframe) キーフレームジャンプ

### Non-Goals

- Motion Sketch（マウスパス→キーフレーム一括生成）→ 別 milestone
- Roving Keyframes → 別 milestone
- 3D ビューポートの Motion Trail → 将来拡張
- Ghosting のオニオンスキン風ビューポート表示 → 既存 Onion Skin でカバー

---

## 4. Phases

### Phase 1: Auto-Keying コア (P0, 1 セッション)

- `AutoKeyingMode` enum: `Off`, `On`, `OnPerLayer`
- `AnimatableValue<float>::setValue()` に auto-key フック追加
  - `setValue(time, value)` → Auto-Keying ON なら `setKeyframeAt(time, value)` を自動実行
- グローバルトグル（ステータスバー or タイムラインツールバー）
- Per-layer override（選択レイヤーのみ Auto-Keying）

**Done criteria:**
- Auto-Keying ON でパラメータ変更→即キーフレーム生成
- Per-layer ON で選択レイヤーのみ自動キー / 他は影響なし
- toggle OFF で既存の手動キーイング動作を維持

Suggested first slice:

- `AbstractProperty::setValue()` ではなく、まず `Artifact` 側の property editing path から auto-key の入口を絞る
- 既存の keyframe 追加経路を再利用して、フレーム内重複の dedup ルールを先に決める
- keying set は `Channel Box` の selection state と切り離して、最小の model から始める

Current implementation note:

- `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm` で auto-key と keying set のガードを property commit path に接続済み
- `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` で playhead key insertion / frame key editing も同じ keying set ルールに接続済み
- `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm` で Auto-Key toggle, Ghosting toggle, Ghosting frame count / opacity, `All Keyable / Transform Only / Custom` の設定面を追加済み
- Custom keying set は `UI/Timeline/CustomKeyingSetPropertyPaths` に保存し、プロパティ名の allowlist として使う
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` にプレイヘッド周辺の薄い ghost marker 表示を settings-driven で追加済み
- ghosting の frame count / opacity は settings で調整可能になり、描画側はその値を直接参照する
- 次の確認点は、`Transform Only` / `Custom` の実運用で抜け道がないかと、ghosting の描画面をどこまで既存 onion-skin から流用するかに絞る

### Phase 2: Keying Set (P1, 1 セッション)

- `KeyingSet` データモデル: 名前 + プロパティパスのリスト
- プリセット: "All Keyable", "Transform Only", "Custom"
- Keying Set 選択 UI（ドロップダウン）
- Channel Box（M-CBOX-1）と連携し、選択チャンネルのみキーイング
- 選択プロパティ以外は `setValue()` 時に auto-key スキップ

**Done criteria:**
- Keying Set "Transform Only" 選択時、Opacity を変更してもキーが打たれない
- Custom Set のプロパティ追加/削除が UI から可能

### Phase 3: タイムライン Ghosting (P1, 1 セッション)

- `ArtifactTimelineTrackPainterView` に Ghosting 描画追加
  - 前後 N フレームのキーフレーム位置を半透明で表示
  - N の設定（デフォルト 3）、opacity 設定
  - 現在フレームのキーフレームは通常表示、Ghost は別色
- 設定 UI: Ghost Frames Before / After スピナー

**Done criteria:**
- タイムライン上に前後フレームのキーが半透明表示
- 現在フレームのキーと Ghost の区別が明確

### Phase 4: Motion Trail + J/K ジャンプ (P1, 1 セッション)

- ビューポート上に Motion Trail 描画（選択レイヤーの過去 N フレームの位置をライン接続）
- Trail の色 / 長さ（フレーム数）設定
- J / K キーフレームジャンプ: 前のキー / 次のキーにプレイヘッドを移動
  - `ArtifactPlaybackShortcuts` にショートカット登録

**Done criteria:**
- 選択レイヤーの Motion Trail がビューポート上に表示
- J で前のキーフレーム、K で次のキーフレームにジャンプ

### Phase 5: 永続化 + Diagnostics (P2, 1 セッション)

- project JSON に auto-keying / keying set / ghosting 設定保存
- Problem View 診断（auto-key 重複、keying set 不一致 etc.）

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_CHANNEL_BOX_2026-07-07.md` | Keying Set の UI として Channel Box の選択チャンネルを利用 |
| `MILESTONE_ANIMATION_LAYERS_2026-07-07.md` | Anim Layer 選択時に auto-key がどの Layer に打たれるかの制御 |
| `MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` | J/K ジャンプ、Ghosting、Auto-Keying の監査結果 |

---

## 6. リスク

1. **Auto-Keying のパフォーマンス**。高頻度の値変更（スライダードラッグ等）でキーが flood するのを防ぐ。フレーム単位で重複キーを dedup
2. **Keying Set と Animation Layer の競合**。選択 Anim Layer と Keying Set の両方が未設定のときのフォールバック
3. **Ghosting の再描画コスト**。前後 10 フレーム × 50 キーの描画負荷。キー数上限を設ける

---

## 7. Done Criteria (全体)

- Auto-Keying ON でパラメータ変更→即キーフレーム / 重複キーの dedup
- Keying Set によるキー対象のフィルタリング
- タイムライン Ghosting が前後 N フレーム表示
- ビューポート Motion Trail が選択レイヤーの軌跡を表示
- J/K キーフレームジャンプ動作
- 全設定の保存復元 / 旧プロジェクト互換
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot なし

---

## 8. 更新履歴

- 2026-07-07: 初版作成。Maya Auto-Keying / Keying Set / Ghosting の ArtifactStudio 移植設計。