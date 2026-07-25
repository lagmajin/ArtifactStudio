# Milestone: Master Properties / Essential Properties（プリコンプ外部プロパティ上書き） (2026-07-08)

**ステータス:** Phase 3 Completed (static verified 2026-07-22; runtime/build verification pending)

---

## 1. 概要

プリコンポジションの特定プロパティ（テキスト・色・位置など）を「親コンプ側から外部上書き」できる仕組み。After Effects の Master Properties / Essential Properties 相当。テンプレート化（.mogrt 的運用）の核。

## 2. なぜ必要か（AE ライクなモーショングラフィックスとして）

- 1 つのプリコンプを複数コンプから異なるパラメータで再利用する広告・バリアブル制作の必須機能。
- 既存 `MILESTONE_MOTION_GRAPHICS_TEMPLATE_2026-06-01.md` はテンプレート枠組みだが、「プリコンプのプロパティを外部から束ねて上書き」の専用設計は未着手。
- Precompose が未完（`unprecompose` 未完）なので、本機能は Precompose 完了を前提とする。

## 3. 参照元ツール

- **After Effects** — Master Properties（レイヤー/プリコンプのプロパティを親から一括制御）。
- **Cinema 4D** — User Data / XPresso 的パラメータ露出。

## 4. 現状（ソース確認・2026-07-08）

- `grep "Master Properties|Essential Properties"` → 0 hit。専用マイルストーンなし。
- 関連基盤:
  - `ArtifactCore/src/Composition/PreCompose.cppm`（Precompose 呼び出し、内部未完成）。
  - Property システム（`PropertyWidget`/`ArtifactPropertyEditor`）は存在するが、外部参照マッピング層なし。

## 5. スコープ（提案 Phase）

- **Phase 1 — プロパティ露出**
  - プリコンプ側で「外部公開プロパティ」を選択・命名する UI。
  - 公開プロパティ → 内部プロパティのマッピングデータモデル。
- **Phase 2 — 親コンプからの上書き**
  - 親コンプのレイヤーインスペクタに公開プロパティを表示し、値を上書き。
  - 評価時にプリコンプ内部へ伝播。
- **Phase 3 — テンプレート連携**
  - `MILESTONE_MOTION_GRAPHICS_TEMPLATE` と接続し、 Essential Properties 的エクスポート。

## 6. リスク / 未確認事項

- Precompose（`unprecompose`）完了をブロック依存として持つ。
- 式/キーフレームとの競合解決ルールを要定義。

## 7. 関連文書

- `docs/planned/MILESTONE_MOTION_GRAPHICS_TEMPLATE_2026-06-01.md`
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`（Precompose 実務完成度）
- `ArtifactCore/src/Composition/PreCompose.cppm`

---

## 8. 実行順

このマイルストーンは、次の順で進めると責務が崩れにくい。

1. `Precompose` 側の内部プロパティ参照モデルを確認する
2. 公開プロパティのデータモデルを先に固定する
3. 親コンプ側の上書き UI は、公開プロパティの読み取りだけに絞る
4. 値伝播は最後に通し、式やキーフレームとの競合をそこで解決する

最初の実作業は Phase 1 の「プロパティ露出」から入る。

### Phase 1 の着手点

1. `PreComposeManager` で保持している nesting / layer source の責務を読み直す
2. 公開プロパティの data model を、親コンプ上書きとは切り離して定義する
3. UI は `PropertyWidget` / `ArtifactPropertyEditor` の既存導線に寄せる
4. `unprecompose` の未完部分には触れず、まずは露出と mapping の定義だけに絞る

### Phase 1 完了条件

- プリコンプ側で「外部公開プロパティ」を選択・命名する入口がある
- 公開プロパティと内部プロパティの mapping が定義されている
- 親コンプの上書き UI は Phase 2 に回しても破綻しない

### Phase 2 の前提

- `unprecompose` の完成度が上がっている
- 式/キーフレームとの競合解決ルールが整理されている
- 先に exposed data model が固まっているため、親側の上書きは後からつなげられる

## 9. 2026-07-14 Current Progress

- [x] Phase 2: parent-side instance override, render-time propagation, nested scope, and unprecompose materialization
- [x] Phase 3: TemplateSlot / TemplateLock の既存テンプレート基盤と Master Properties 公開データの接続点を確認
- [ ] runtime / build による最終確認

- `ExposedPropertyRegistry`、precomp layer単位override、JSON round-tripは実装済み
- Composition Settingsからfocused propertyを参照中の全parent precompへ公開・解除できる
- 公開・解除は一括Undo/Redo対応
- parent側では既存`Master Properties` property groupからinstance overrideを編集できる
- precomp描画時だけoverrideを一時適用し、描画後にchild compositionの元値を復元するため、兄弟precomp instance間で値が漏れない
- precomp samplingは親frameから明示的にchild frameへ変換する
- child frameのanimation/expression評価後にinstance overrideを適用し、Master Propertyを最終優先値として固定する
- nested precompの`Master Properties/*`も同じscope順序で再帰評価できる
- unprecompose時はinstanceの有効override値を復元レイヤーへmaterializeする
- precompose / unprecomposeのUndo snapshotでregistryとoverrideを持つ同じprecomp layer instanceを復元する
- build / runtime verificationは未実施のため`In Progress`を維持する

---

## Static audit follow-up (2026-07-25)

`ArtifactCompositionRenderController` の instance override／nested propagation、Composition Settings の公開導線、JSON／Undo、TemplateSlot／TemplateLock の接続を現行ソースで再確認した。ビルド・実機動作は未実施。

| Phase | 現状 | 判定 |
|---|---|---|
| 1. プロパティ露出 | Exposed property registry、focused property の公開・解除、既存 Master Properties group の表示を確認した。 | 実装済み／表示確認待ち |
| 2. 親コンプ上書き | precomp layer instance override、render-time propagation、child frame変換、nested scope、Undo／JSON round-tripを確認した。 | 実装済み／runtime確認待ち |
| 3. Template連携 | TemplateSlot／TemplateLock の既存基盤と公開データの接続点を確認した。完全なテンプレートexport／再利用フローは未確認。 | 部分実装／統合確認待ち |
| 競合・復元 | expression／keyframe評価後の override 優先順、unprecompose materialize、precompose undo snapshot の記載・実装を確認した。 | 実装済み／runtime確認待ち |

### 現在の判定

Phase 1〜2 と主要な Phase 3 接続は静的に実装済み。テンプレート運用の完全なexport／再利用、および実機での兄弟・nested instance隔離は未検証のため、`Phase 3 Completed — runtime/build verification pending` を維持する。
