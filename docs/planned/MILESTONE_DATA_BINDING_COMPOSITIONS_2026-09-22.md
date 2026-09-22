# M-DATABIND-1 Data-Bound Compositions

**最終更新:** 2026-09-22

**ステータス:** Not Started

## 目的

CSV / JSON project assets を、再現可能な読み取り専用データとして text、画像差替え、
数値 property に bind する。表データを制作で安全に使えるようにし、ネットワーク接続や
任意コード実行はこの段階に含めない。

## 現状の根拠

- 旧 Data-Driven Engine 計画は `MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md` に統合済み。
- Property registry と asset / serialization の基盤は存在するが、データアセットから制作 property
  へのユーザー向け bind 導線は未確認である。

## 範囲

1. UTF-8 CSV と JSON array-of-objects の import、schema、diagnostics。
2. project asset としての参照、content hash / modified time、保存／再読込。
3. 明示的な binding: row selector + column + target property。
4. text、scalar / bool / color、画像 source の片方向評価。
5. missing column、型不一致、行不足、更新待ちを render/export 前に診断する。

## 非対象

- Google Sheets 等への直接接続、任意 URL fetch、Excel の完全互換。
- expression からの任意ファイルアクセス。
- bind の結果を source property へ暗黙に書き戻す二方向同期。

## フェーズ

### Phase 1 — Data asset contract

- CSV / JSON の最小 parser、schema、typed value、parse diagnostic を Core に置く。
- asset identity と project JSON の永続化を定義する。

### Phase 2 — Read-only binding evaluation

- binding descriptor を property path と source column の間に作る。
- evaluation は explicit context / time / row selector を受け、元 property を破壊しない。
- hot path でのファイル read、無制限 allocation、文字列再構築を避ける。

### Phase 3 — Authoring and preflight

- Inspector / dedicated binding surface で source、column、row、fallback を編集する。
- render queue と editor で同じ preflight diagnostics を用いる。

### Phase 4 — Deterministic batch output

- row range を指定した batch render request を導入する。
- 各 output が使用した data revision を記録する。

## 完了条件

- CSV / JSON が project asset として保存／再読込できる。
- bind の失敗を可視化し、export が意図しない空値で成功扱いにならない。
- 同じ project、data revision、row 指定で同じ出力を再現できる。

## 関連

- `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`
- `docs/planned/MILESTONE_DATA_DRIVEN_ENGINE_2026-04-21.md` (superseded)

