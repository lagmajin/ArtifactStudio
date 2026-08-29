# マイルストーン: Property Modulation 基盤（Bitwig 着想）

**最終更新:** 2026-08-29
**ステータス:** Phase 1 Core 実装済み・Effect と layer opacity の評価接続、effect property の最小編集導線まで実装。詳細管理 UI は未着手
**優先度:** Medium

## 目的

LFO、Random、将来の Audio Follower などの control source を、既存の property path に非破壊で重ねる。Bitwig Studio の Unified Modulation System を、UIやデバイス構造ではなく「source → target mapping と基準値を保持する」契約として参照する。

## Phase 1（実装済み）

- 既存 `Audio.Modulation.Router` を正規 Core とする。
- `ModulationAssignment::forPropertyPath()` が stable property path と target ID を保持する。
- Add と Multiply を同一 target へ合成できる。
- 同じ source / target / mode の再追加は重複せず、depth と enabled を更新する。
- mapping の読み取り用一覧を公開する。
- `AudioModulationRouterTest` に add、multiply、重複更新の静的テストを追加した。
- `AbstractProperty::evaluateValue()` は router と stable target path が明示的に渡された時だけ、数値の評価済み値へ modulation を適用する。Float/Integer の既存 min/max 実値範囲は適用後にも維持する。
- `processAtFrame(frame, frameRate)` は同一フレームを再評価しても source を進めず、逆方向 seek では reset/replay する。Random は seed を保持して再現する。
- `ModulationRouter::sourceDefinitions()` / `restoreSources()` は LFO / ADSR / Random の設定と source ID を値オブジェクトとして往復できる。runtime phase・held value・ADSR gate は保存せず、復元時に決定的な初期状態へ reset する。
- Composition effect は `modulation.sources` / `modulation.assignments` を JSON に保存・復元する。復元順は source definitions → assignments で、assignment は stable target path から target ID を再計算する。
- Layer 所有 router も同じ `modulation.sources` / `modulation.assignments` schema を layer JSON に保存・復元する。初期対象の `layer.opacity` mapping は layer type にかかわらず再読込できる。
- `ModulationRouterSnapshot` は source definitions、assignments、router smoothing を完全置換で復元する。既存 assignment を先に消去するため、save/open と将来の Undo は古い mapping を残さない。
- `addAssignment()` は存在しない source ID を拒否する。JSON / snapshot の復元は source → assignment の順で行うため、欠落 source を参照する mapping は残らない。
- `MacroSource` は 0〜1 の手動／automation 駆動 control value を出力する。時間を進めないため preview と offline render で同一 project state を再現でき、effect/layer JSON と Router snapshot に保存される。
- effect/layer JSON の source ID は `uint32_t` を狭めず double として保存する。target hash は引き続き保存せず target path から再計算する。
- source ID の `0` は無効な予約値とし、`addSource()` は uint32 wrap-around 後も 0 と使用済み ID を発行しない。
- `EffectModulationSnapshotCommand` と `LayerModulationSnapshotCommand` は Router snapshot を before/after として UndoManager に積める。target は weak reference、selection / current frame は変更せず、undo/redo 後に既存の全体更新通知を送る。
- `ArtifactEffectService` は layer effect / composition effect の modulation snapshot 適用 API を公開する。UI は Router を直接変更せず、この API を通じて Undo、project dirty、既存の effect/layer 通知を得る。
- `ArtifactAbstractEffect` は effect ごとの router を所有し、`EffectContext` の composition frame を control clock として editable float/integer property へ適用する。target path は `effect.<instance-id>.<property-name>` とする。
- `ArtifactAbstractLayer` は layer ごとの router を所有し、全レイヤー共通の `layer.opacity` を `layer.<layer-id>.layer.opacity` として評価する。keyframe / animation layer の後、effect envelope の前に適用する。

## 評価順

`base value → keyframe/envelope/expression → modulation Add → modulation Multiply`

`ModulationRouter` は property を直接変更しない。所有側が評価済み base value を `targetValue()` へ渡すことで、keyframe と expression の既存責務を維持する。

## 未実装

- transform など layer opacity 以外の layer property への接続
- Inspector / Property Editor から effect service の modulation snapshot API を呼ぶ詳細編集導線（既存 row の context menu に最小追加済み、Macro 値の直接編集を含む）
- Inspector / Property Editor の追加・削除・量調整 UI
- LFO/Random 以外の source、Audio Follower、macro、tempo sync
- source 自身の modulation、複数声・plugin parameter 対応

## 検証

ビルド・テスト実行は未実施。静的差分確認のみ実施。`AudioModulationRouterTest` に source definition round-trip、snapshot 完全置換、missing source mapping 拒否、Macro value の mapping/restore、source ID wrap-around 回帰ケースを追加し、Composition effect / layer 所有 router の JSON 保存・復元、UndoManager の snapshot command、および effect service の Undo 付き適用 API を追加した。source ID は full uint32 range を保存できる。Property Editor の animatable effect row から source / depth / mix mode を追加する最小導線を追加した。次の確認は、effect / layer JSON の round-trip、service API 経由の command undo/redo、Null Layer opacity mapping の preview / export 一致である。
