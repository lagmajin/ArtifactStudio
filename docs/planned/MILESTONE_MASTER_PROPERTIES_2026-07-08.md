# Milestone: Master Properties / Essential Properties（プリコンプ外部プロパティ上書き） (2026-07-08)

> 状態: DRAFT（新規・未実装・専用マイルストーン未作成を確認済み）

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
