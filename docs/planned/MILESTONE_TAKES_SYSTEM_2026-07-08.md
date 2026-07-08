# Milestone: Takes System（レンダーバリエーション / パラメータオーバーライド一括） (2026-07-08)

> 状態: DRAFT（新規・専用マイルストーン未作成を確認済み。CROSS_APP 文書で FOUNDATION 提案のみ）

---

## 1. 概要

1 つのコンポジションに対して「複数のレンダーバリエーション（異なるテキスト・色・カメラ・表示切替など）」を管理し、一括レンダリングする仕組み。Cinema 4D の Takes 相当。

## 2. なぜ必要か（AE ライクなモーショングラフィックスとして）

- 広告・SNS 運用で「同じ構成の色違い・コピー違いを大量出力」が日常的。
- AE には標準 Takes がなくプラグイン（True Comp Duplicator 等）で代替されているが、Artifact はモーショングラフィックス特化なので標準機能として持つ価値が高い。
- Master Properties（別マイルストン）と組み合わせるとテンプレート × バリエーション運用が完結。

## 3. 参照元ツール

- **Cinema 4D** — Takes（タグ/オーバーライド/バッチレンダ）。
- **Unreal** — Level Variant / Scene State。

## 4. 現状（ソース確認・2026-07-08）

- `docs/analysis/REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md` で「Layer / Composition Takes — FOUNDATION, P1」として提案のみ。専用マイルストーンなし。
- `MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md` はレイヤーバリアントだが、コンプ全体の Takes 管理ではない。
- grep `Takes` → 専用 milestone なし。

## 5. スコープ（提案 Phase）

- **Phase 1 — Take データモデル**
  - コンポジションに複数 Take を持たせ、各 Take がプロパティオーバーライド集合を保持。
- **Phase 2 — オーバーライド UI**
  - 特定レイヤー/プロパティを「この Take だけ上書き」する操作。
- **Phase 3 — バッチレンダ**
  - Render Queue と連携し、Take ごとに出力（命名規則・フォルダ分け）。

## 6. リスク / 未確認事項

- 既存 `MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS` との責務境界を要定義（重複回避）。
- Master Properties とオーバーライド解決順序の統一が必要。

## 7. 関連文書

- `docs/analysis/REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md`
- `docs/planned/MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md`
- `docs/planned/MILESTONE_MASTER_PROPERTIES_2026-07-08.md`（依存関係）

---

## 8. 実行順

このマイルストーンは、次の順で進めると重複を減らしやすい。

1. `Master Properties` との責務境界を先に固定する
2. Take のデータモデルを、単なるプリセットではなく上書き集合として定義する
3. Render Queue 連携は最後に回し、まずは UI と保存形式を固める
4. 既存のレイヤーバリアントと重なる部分は、コンポ単位の管理に限定する

最初の実作業は Phase 1 の Take データモデルから入る。

### Phase 1 の着手点

1. `Composition State Variants` との重なりを確認し、Take を「単なる状態名」ではなく override 集合として定義する
2. `Master Properties` は exposed control の公開側、`Takes` はその束ね替え側として責務を分ける
3. UI はまず保存形式と data model を先に定義し、render / batch は後ろに回す
4. 既存のレイヤーバリアントはコンポ全体の管理に広げず、必要最小限の上書きだけに限定する

### Phase 1 完了条件

- コンポジションに複数 Take を持てる
- 各 Take が property override の集合を保持できる
- Master Properties 側の exposed control と組み合わせても解決順が破綻しない

### Phase 2 の前提

- override UI を載せるだけの data model が固まっている
- 保存形式と責務境界が先に決まっている
- render queue 連携を後段にしても意味が通る
