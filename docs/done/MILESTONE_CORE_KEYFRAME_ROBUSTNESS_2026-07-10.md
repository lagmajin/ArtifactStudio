# MILESTONE: コアキーフレーム / プロパティ更新の堅牢化

**ステータス:** Complete (static verification)

**完了日:** 2026-07-12

> 2026-07-10 作成

## 目的

ArtifactCore のキーフレーム・プロパティ評価・更新まわりの堅牢性を高める。
現状は複数のキーフレーム実装が並存し、時刻比較・スレッド安全性・値検証・
エラー通知に穴がある。これらを段階的に固め、アニメーションデータの破損や
不定挙動を防ぐ。

## 背景

コアの主なアニメーション実装:

- `ArtifactCore/include/Property/AbstractProperty.ixx` + `.cppm`
  （`KeyFrame` = RationalTime + QVariant、`evaluateValue()` / `addKeyFrame()` 等）
- `ArtifactCore/include/Animation/AnimatableValue.ixx`
  （`AnimatableValueT<T>` = FramePosition + テンプレート、独自の補間 switch）
- `ArtifactCore/include/Core/KeyFrame.ixx`（`KeyFrame` = std::any、大半コメントアウト）
- `ArtifactCore/include/Animation/Animatable(deprecated).ixx`（旧実装）

調査で見えた堅牢性の穴:

| 穴 | 箇所 | リスク |
|---|---|---|
| キーフレーム系の多重実装 | AbstractProperty / AnimatableValueT / Core.KeyFrame / deprecated | 補間・管理ロジックが分散し不整合 |
| `RationalTime == time` 依存 | `addKeyFrame` / `removeKeyFrame` / `setKeyFrame*At`（AbstractProperty.cppm:427,459,525 等） | 異なる scale で同一時刻が false → 重複キー・削除失敗 |
| スレッド非安全な評価キャッシュ | `AnimatableValueT::at()` の `mutable lastCachedIndex_`（AnimatableValue.ixx:118） | render + UI 同時評価で data race |
| ロック未使用 | `<mutex>` を include しつつ addKeyFrame 等でロックなし（AbstractProperty.cppm） | 並行編集 / 評価で競合 |
| 値バリデーション欠如 | addKeyFrame / setValue に NaN/Inf/型不一致チェックなし | 補間で伝播しレンダリング破綻 |
| エラー通知の欠如 | `addKeyFrame` は void、失敗を返さない | 呼び出し側が失敗を検知できない |
| retime の精度劣化 | `RationalTime::fromSeconds(newTime)`（AbstractProperty.cppm:517） | 分数↔秒往復で時刻ずれ |

## ターゲット像

- キーフレームの時刻比較が scale 非依存で一貫する（重複・削除漏れが起きない）。
- キーフレーム評価がスレッド安全（レンダースレッドから安全に読める）。
- 不正値（NaN/Inf/型不一致）が入る前に検出・拒否される。
- キーフレーム操作の成否が呼び出し側へ伝わる。
- 補間ロジックが 1 箇所に集約され、実装間で結果が一致する。

## 非ゴール（このマイルストーンの範囲外）

- キーフレーム機能の新規追加（新しい補間タイプ / spatial bezier 等）
- タイムライン UI の再設計
- `AnimatableValueT<T>` と `AbstractProperty` の完全統合（まず不整合の解消に集中）
- Expression エンジンの再実装（評価の例外安全のみ扱う）

## 設計原則

1. **挙動を変えずに堅牢化する**。まず不変条件を固め、機能は増やさない。
2. 時刻の同一判定は「正規化した比較」に一本化する。
3. 評価パス（読み取り）と編集パス（書き込み）を分離し、キャッシュを安全にする。
4. 不正値は「入口」で弾く（addKeyFrame / setValue）。
5. 二重実装は削除より先に「差異の可視化とテスト固定」を行う。

## Scope（想定する変更ファイル）

- `ArtifactCore/include/Property/AbstractProperty.ixx`
- `ArtifactCore/src/Property/AbstractProperty.cppm`
- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `ArtifactCore/include/Core/KeyFrame.ixx`（整理 / 廃止検討）
- `ArtifactCore/include/Animation/Animatable(deprecated).ixx`（廃止導線）
- `ArtifactCore/include/Property/PropertySerializationBridge.ixx`（往復整合の確認）

## Phases

### Phase 1: 不変条件の明文化とテスト固定

現状挙動を壊さないための土台を作る。

- キーフレーム集合の不変条件を定義（時刻昇順 / 同一時刻の一意性 / 有限値）
- 現状の addKeyFrame / removeKeyFrame / interpolate の結果をテストで固定
- 二重実装（AbstractProperty vs AnimatableValueT）の結果差異を洗い出す

**Done when:**

- コアのキーフレーム操作に回帰テストがある
- 実装間の差異が一覧化される

### Phase 2: 時刻比較の正規化

scale 非依存な時刻同一判定へ統一する。

- `RationalTime` の同一判定ヘルパ（正規化比較）を導入
- addKeyFrame / removeKeyFrame / setKeyFrame*At の `== time` を置換
- 重複キー・削除漏れが起きないことをテストで確認

**Done when:**

- 異なる scale の同一時刻が正しく同一と判定される
- 重複キーが生成されない

### Phase 3: 値バリデーション

不正値を入口で弾く。

- addKeyFrame / setValue で NaN/Inf を検出し拒否 or サニタイズ
- 型不一致の QVariant を検出（PropertyType との整合）
- 失敗時のエラー通知（戻り値 or 診断ログ）を追加

**Done when:**

- NaN/Inf が補間へ伝播しない
- 型不一致が検出される

### Phase 4: スレッド安全な評価

読み取り評価をスレッド安全にする。

- `AnimatableValueT::at()` の `mutable` キャッシュを thread-local か非破壊探索へ変更
- AbstractProperty の編集パスにロック（or copy-on-read）を導入
- レンダースレッドからの同時評価を安全にする

**Done when:**

- render + UI 同時評価で data race が起きない
- 評価結果が並行アクセスで壊れない

### Phase 5: 補間ロジック集約と重複実装の整理

補間の単一実装化と旧実装の廃止導線。

- 補間ロジックを 1 箇所に集約（AnimatableValue の switch と evaluateValue の統一）
- `Animatable(deprecated)` / `Core.KeyFrame` の利用箇所を確認し廃止導線を敷く
- retime の精度劣化（fromSeconds 往復）を有理数演算で緩和

**Done when:**

- 補間結果が実装間で一致する
- 廃止対象の利用箇所が特定・縮小される
- retime の時刻ずれが縮小する

## Recommended Order

1. Phase 1 (テスト固定)
2. Phase 2 (時刻比較正規化)
3. Phase 3 (値バリデーション)
4. Phase 4 (スレッド安全)
5. Phase 5 (補間集約 / 整理)

### Why This Order

- Phase 1 で回帰テストを敷かないと、以降の堅牢化が挙動を壊しても気づけない。
- 時刻比較は最も影響範囲が広く、重複キー等の根本原因なので先に潰す。
- 値バリデーションは独立性が高く、早く入れるほど恩恵が大きい。
- スレッド安全は評価パスの変更を伴うため、時刻・値が固まってから。
- 実装集約・廃止は最後に回し、テストで守られた状態で進める。

## 連携先

- `ArtifactCore/include/Property/AbstractProperty.ixx` / `.cppm`
- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx` / `AnimatableTransform2D.ixx`
- `ArtifactCore/include/Property/PropertySerializationBridge.ixx`
- `ArtifactCore/include/Geometry/Interpolate.ixx`（`KeyframeInterpolator`）
- 関連: `docs/planned/MILESTONE_MOTION_PATH_DISPLAY_IMPROVEMENT_2026-07-10.md`
  （spatial bezier は本堅牢化の上に載せる）

## Validation Checklist

- コアキーフレーム操作に回帰テストがある
- 異なる scale の同一時刻が同一と判定される
- 重複キーが生成されず、削除漏れが起きない
- NaN/Inf/型不一致が入口で検出される
- render + UI 同時評価で data race が起きない
- 補間結果が実装間で一致する
- 既存プロジェクトのシリアライズ往復が壊れない

## Notes

このマイルストーンは「機能追加」ではなく「基盤の堅牢化」。
モーションパス改善（spatial bezier）や AI キーフレーム生成など、キーフレームを
土台にする機能は、まずこのコアが堅牢であるほど安全に載せられる。
特に時刻比較の scale 非依存化と評価のスレッド安全化は、既存の潜在バグを潰す。

---

## Next Execution Slice

Phase 1 から入る。まずは現状挙動をテストで固定し、二重実装の差異を可視化する。

### Current Progress

- `AbstractProperty` の keyframe 時刻比較を共通ヘルパに寄せ、同一時刻の置換・削除・存在判定を揃えた
- `AnimatableValueT` 側も keyframe の正規化ヘルパを追加し、昇順 + 一意の不変条件を明示した
- `Artifact.Test.PropertyKeyframe` を追加し、同一秒の別 scale keyframe が置換・削除と serialization roundtrip で同一視される回帰を固定した
- `RationalTime::operator==` / `operator<` を、乗算や double epsilon に依存しない約分・連分数比較へ変更した
- `12/24` と `24/48` を別時刻としていた誤った削除テストを修正し、大整数・負数・異 scale の比較回帰を追加した
- `AnimatableValueT` の置換・整列・move先衝突の回帰を追加した

> Source/diff checked only. Build / test execution is pending explicit permission.

### Static completion summary

- `RationalTime` equality and ordering are scale independent and avoid floating-point epsilon comparison.
- `AbstractProperty` normalizes keyframe order and uniqueness and uses the same comparison for add, remove, lookup, metadata, and roving operations.
- Invalid property/keyframe values and non-finite Bezier controls are rejected at mutation boundaries.
- `AbstractProperty` and `AnimatableValueT` protect read/write evaluation state with shared/exclusive locking.
- Retime preserves the destination rational scale instead of round-tripping through default-scale seconds.
- `Artifact.Test.PropertyKeyframe` covers equivalent-scale replacement/removal, large integer ordering, AnimatableValue ordering, replacement, and serialization round-trip.
- Completion is source/diff verified only; the test module was not executed by user choice.

### 2026-07-10 Cleanup UI Progress

- Composition Editor Command Palette に selected layer全体の redundant keyframe cleanupを追加
- 前後3キーが許容誤差内で同値の場合のみ中間キーを削除する
- `SetLayerPropertyKeyframesCommand` + `MacroUndoCommand` で一括Undo/Redoに対応

### Phase 1A の着手点

1. AbstractProperty の addKeyFrame / removeKeyFrame / interpolateValue の回帰テストを作る
2. AnimatableValueT<T> の at / addKeyFrame / moveKeyFrame の回帰テストを作る
3. 同一シナリオで両実装の結果差異を一覧化する
4. キーフレーム集合の不変条件（昇順 / 一意 / 有限）を明文化する

### Phase 1 完了条件

- コアキーフレーム操作に回帰テストがある
- 実装間の差異が一覧化される
- この時点では挙動を変えない

### Phase 2 の前提

- `RationalTime` の正規化比較の定義点を決める（value/scale の最小公倍数 or 秒許容誤差）
- 既存シリアライズ（PropertySerializationBridge）と往復整合を崩さない
- 時刻比較変更が motion path の rescaledTo 回避策と衝突しないか確認
