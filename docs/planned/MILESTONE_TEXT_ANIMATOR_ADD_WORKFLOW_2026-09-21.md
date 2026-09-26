# M-TXT-ANIM-1: Text Animator の追加ワークフロー仕上げ（AE 風個別追加・Timeline 露出）

**最終更新:** 2026-09-26

**ステータス:** In Progress（P1 の個別プロパティ追加と P2 の Timeline 左ペイン露出は 2026-09-26 に実装・静的確認済み。ビルド・実機は未確認）

## 目的

`ArtifactTextLayer` へテキストアニメーション（Text Animator）を追加する導線を、既存の追加機構を保ったまま、AE ライクな操作（個別プロパティ追加＋Timeline でのキーフレーム編集）まで拡張する。

## 前提（2026-09-21 実コード照合の確定事実）

追加機構と評価経路は実装済み。

- Core: `TextAnimatorEngine`（Range/Wiggly/Expression セレクター、`AnimatorSelectorSet`、`applyAnimatorSets`）。
- レイヤー: `addAnimator()` / `removeAnimator()` / `setAnimatorCount()`（最大16）、プリセット7種（`buildTextAnimatorPreset`）、Undo 用 `textAnimatorStackSnapshot()` / `restoreTextAnimatorStack()`。
- 評価: `perGlyphMode_` で `resolvedTextAnimatorStackAtTime()` → `applyAnimatorSets()` をタイムライン時刻で評価し、`animatedGlyphBounds()` でバウンズを更新（`Artifact/src/Layer/ArtifactTextLayer.cppm`）。
- Inspector: `text.animatorCount` は `ArtifactAnimatorCountPropertyEditor`（Add ボタン＋プリセットメニュー）、`text.animators.N.*` は `getLayerPropertyGroups()` で公開（`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm:1117` → `createPropertyEditorWidget`）。
- Timeline 左ペイン右クリック: `Text Animator` サブメニュー（プリセット7種＋Clear Animators、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` 4974 行付近）。
- VP 右クリック: `Add Text Animator`（`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` 1148 行付近）。
- VP ギズモ: `ArtifactTextGizmo` が `text.animators.N.*` をドラッグで編集する。
- Timeline キーフレームモデル: `collectAnimatablePropertyRefs()` が `text.animators.N.*` を収集し、`parseAnimatorPropertyPath()` ＋ `setLayerPropertyValue()` で書き込み、`displayLabelForPropertyPath()` で表示ラベルを解決する。

実装前差分（2026-09-21 時点）:

- Timeline 左ペインの標準プロパティグループは `Transform` のみに限定されている（`computeTimelineHiddenLayerPropertyGroup`）。そのため Animator グループは Timeline の行表示では非表示で、キーフレーム編集は Inspector 経由になる。2026-08 の分析で「Text Animator timeline 未配線」とされたのはこのポリシーに起因する。
- 追加は「プリセット全体」または「空の Animator」のみで、AE の「Animate ▼」に相当する個別プロパティ（Position / Opacity / Scale / Rotation / Fill Color / Stroke Color / Tracking / Skew / Blur）を1個の Animator として追加する導線がない。
- `docs/spec/SPEC_TEXT_TOOL_REQUIREMENTS_2026-07-31.md` の 5.2 は Text Animator を「未実装」としたままだった（2026-09-21 に照合済みとして更新）。

## Phase 計画

| Phase | 内容 | 状態 | 主な対象 |
| --- | --- | --- | --- |
| P0 | runtime 受入（追加 → プリセット適用 → キーフレーム → 再生） | 未着手（ビルドはユーザー許可待ち） | 受入観点 |
| P1 | 個別プロパティ追加導線（AE 風「Animate ▼」） | 実装済み（静的確認のみ、2026-09-26） | Timeline 右クリック / Inspector / `ArtifactTextLayer::addAnimatorProperty` |
| P2 | Timeline 左ペインへの Animator グループ露出 | 実装・静的レビュー済み（ビルド・実機未確認、2026-09-26） | `ArtifactTimelineKeyframeModel` / `shouldHideTimelinePropertyGroup` の `PropertyGroup` 版 |
| P3 | プリセット拡充・ユーザー保存プリセット | 未着手 | `buildTextAnimatorPreset` |

### P0 — runtime 受入

ビルド・テスト・CMake 実行は AGENTS.md により**ユーザーの明示指示が必要**。指示を受けたら次を確認する。

- `check_module_hygiene` と D3D12／Vulkan の両経路で起動確認。
- 受入観点: テキストレイヤーで Animator を追加 → Range Selector の Start を動かして文字ごとの変化が出る → キーフレームを打つ → 再生で文字が動く → 1操作が1回の Undo／Redo で往復する。
- 文字種: ASCII／CJK／絵文字／合字／縦書き。リッチテキスト境界（`perGlyphMode_ = !isRichText` のため HTML テキストは animator 評価の対象外）の挙動を実機で確認する。
- 1000 文字でのフレーム時間（GPU テキスト描画とラスタの両経路、プロファイル確認）。

### P1 — 個別プロパティ追加導線

- Timeline 左ペイン右クリックの `Text Animator` サブメニュー、または Inspector の Animator count エディタに、個別プロパティ（Position / Scale / Rotation / Opacity / Fill Color / Stroke Color / Tracking / Skew / Blur）を1プロパティ＝1 Animator として追加する項目を追加する。
- 個別追加は `ArtifactTextLayer::addAnimatorProperty()` に集約し、構造変更の Undo は完全な Animator stack snapshot を扱う `SetTextAnimatorStackCommand` へ畳む。
- 追加直後は Range Selector（Start/End/Offset/Shape）を既定値のまま残し、対象プロパティのみ「変化量あり」の初期値にする。
- **実装メモ（2026-09-26）:** `ArtifactTextLayer::addAnimatorProperty()` を追加し、Position / Scale / Rotation / Opacity / Fill Color / Stroke Color / Tracking / Skew / Blur を1項目＝1 Animatorとして末尾へ追加する。Inspector の `+` メニューと Timeline 左ペイン右クリックの `Text Animator > Animate` の両方から同じIDへ接続した。構造変更は汎用プロパティ値Undoではなく、既存の `SetTextAnimatorStackCommand` に before/after snapshot を渡して1 Undoへ畳む。Inspectorは複数対象を1 Macro Undoへまとめ、Default／Preset追加、末尾Animator削除、既存の`Preset`列によるstack置換／Clearも同じ経路を使う。これにより削除・置換前のAnimator名、Selector、プロパティ値、キーフレームをUndoで復元する。TimelineのPreset置換／Clearに加え、Composition Viewport右クリックのDefault Animator追加もmutation guard・完全スタックUndo・`LayerChangedEvent`を通る経路へ統一した。`SetTextAnimatorStackCommand` の Undo/Redo 本体も `text.animators` の既存プロパティ変更通知を発行し、レイヤーdirty・再描画・Timeline更新を復元後に再評価させる。単一レイヤー操作は既存の `layer.stack` 共同編集プロトコルへ載る。複数選択Macroは全対象のmutation guardを通すが、現在の共同編集batchがstack子コマンドを未対応のため共同編集セッション中はpreflightで安全に拒否される。新規 signal/slot は追加していない。
- 初期変化量は Position Y=72、Scale=0、Rotation=35°、Opacity=0、Tracking=24、Skew=20°、Blur=10。Fill Color は赤の color override、Stroke Color は赤の stroke override と幅2を有効化する。Range Selector は既存既定値を維持する。
- Timelineのプロパティ表示名はcamelCaseを分割し、`positionX` / `fillColor` / `strokeColor` を `Position X` / `Fill Color` / `Stroke Color` と表示する。

### P2 — Timeline 左ペインへの Animator グループ露出

- AGENTS.md は左ペインの標準グループを Transform のみに限定する。Animator グループの露出はこの例外として**ユーザーの明示要求または設計レビューが必要**（2026-09-22 にユーザーが承認）。
- 候補: (a) テキストレイヤー選択時のみ「Animators」のサブツイストを出す、(b) 既存の「Text Animator」右クリックに「Timeline に表示」を追加し、表示状態をレイヤー単位で保持する。
- 露出する場合は表示名ではなくプロパティパス `text.animators.*` で識別し、AGENTS.md の Text Animator 動的グループ規約に従う。
- **実装メモ（2026-09-22 中断）:** `ArtifactAbstractLayer.ixx` に `isTimelineTextAnimatorLayerPropertyGroup(const ArtifactCore::PropertyGroup&)` の宣言だけを追加した（実装・呼出しは未配置のためリンク影響なし）。実装（述語本体＋9箇所の呼出し例外: `ArtifactLayerPanelWidget.cppm` 3箇所、`ArtifactTimelineKeyframeModel.cppm` 2箇所、`ArtifactTimelineTrackPainterView.cppm` 4箇所。いずれも `group.name()` 判定の前にパス判定を挟む形）を入れようとした時点で、別セッションが `ArtifactAbstractLayer.cppm` を `ArtifactAbstractLayer*Support.cppm` 群へ分割中であることを検出した。`isTimelineHiddenLayerPropertyGroup` など既存述語の定義が一時的にどのファイルにも存在しないため、分割完了後に新しい定義場所へ追従してから実装を再開する。
- **実装メモ（2026-09-26 完了）:** 上記の分割は完了し、述語定義は `Artifact/src/Layer/ArtifactAbstractLayerPropertyGroups.cppm` に戻っている。AGENTS.md の例外承認も同日得られた（本文にも追記済み）。`isTimelineHiddenLayerPropertyGroup` はグループ名文字列でしか判定できないため、`ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup` に `PropertyGroup` 版オーバーロードを追加し、Text Animator グループなら非表示にせず、それ以外は既存判定へ委譲する形で実装した。呼出し 9 箇所すべてを `group` 全体を渡す形へ置換済み。名引版は温存している。ビルド・実機は未確認。
- **静的レビュー（2026-09-26）:** Timeline 内の `getLayerPropertyGroups()` 消費箇所を再検索し、左ペイン3箇所、右ペイン4箇所、キーフレームモデル2箇所の計9箇所が `PropertyGroup` 版へ統一されていることを確認した。例外述語はグループ内の全プロパティが厳密に `text.animators.<数値index>.*` 配下の場合だけ true とするため、他レイヤーの非 Transform グループを誤って露出しない。

### P3 — プリセット拡充・ユーザー保存プリセット

- `buildTextAnimatorPreset` の既存7種を棚卸しし、欠落プリセット（例: Fade In Words、Path 追従）の追加、またはユーザー定義プリセットの保存／読込を `ArtifactCore::LayeredConfigStore` など既存の設定経路で扱うかを設計レビューで決める。

## 受入条件

- テキストレイヤーから、Inspector・Timeline 右クリック・VP 右クリックのいずれでも Animator を追加できる。
- 追加した Animator のプロパティが Inspector と（P2 採用時）Timeline で編集・キーフレーム化できる。
- 追加から再生・Undo までが1操作1 Undo で閉じる。
- 新規 signal／slot、QtCSS、QImage 化、QPainter 合成を追加しない。Timeline 左ペインの表示ポリシー変更は例外手続きを踏む。

## 検証状況

- 2026-09-21: 現状照合（上記「前提」）と SPEC 5.2 の更新を実施。ビルド・実機確認は未実施（ユーザー指示待ち）。
- 2026-09-26: P1/P2 の静的確認、`git diff --check`、英日翻訳JSONの構文確認を実施。保存は `toJson()` の完全stack snapshot、再読込は `fromJsonProperties()` の `restoreTextAnimatorStack()` を使い、Animator値・式・キーフレーム・envelopeを往復する経路であることを確認した。ビルド、module hygiene、起動、Undo/Redo、D3D12/Vulkan実機確認は未実施（ユーザー指示待ち）。

## 関連文書

- `docs/spec/SPEC_TEXT_TOOL_REQUIREMENTS_2026-07-31.md`
- `docs/done/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`
- `ArtifactCore/docs/MILESTONE_TEXT_SYSTEM_2026-03-12.md`
- `docs/analysis/ae_maturity_additional_analysis_p3.md`（M-3: Text Animator timeline 未配線の指摘）
- `Insight.md`（2026-09-21 の項目）
