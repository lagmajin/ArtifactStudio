# 画像R&D labs 整列参照（alignment reference）監査

**最終更新:** 2026-11-19
**ステータス:** 完了（静的チェック + 人による読み合わせ。該当ラボの再実行・人手 gate・製品経路は未実施）

## 目的

PencilPrintLab の RD10 で、時間測定の参照の作り方が「追従の残差」ではなく「効果量そのもの」を測っている場合が判明した（RD9 の結論は参照の取り違えによる欠陥）。同じ構造の誤りが他のラボに無いかを，抽出し，例外なく確認するための監査を行う。

対象は `experiments/` と `tools/` 配下の Python ソース。判定の軸は次の 3 点である。

1. **参照クラス**: 整列 MAE の参照は「同じ処理を通した frame 0 の出力」か、それとも生入力か。前者なら追従を測れ、後者なら効果量を測る。
2. **参照のフィルタ特性**: 参照が補間 warp で作られている場合，被測定テクスチャ自体が low-pass され，残差は blur 限界になる。補間なしの厳密シフトなら追従を直接測れる。
3. **シフトの厳密性**: `cv2.warpAffine` の `INTER_NEAREST` はオフセットが小さい範囲では整数移動と一致するが，大きいオフセットでは固定小数点座標変換のため一致しない（RD10 で dx=1,2 は一致、dx=3 以上は最大 0.125 の差を確認）。厳密な整数移動の参照に使うべきではない。

## 方法

- 静的チェック: `tools/audit_alignment_references.py`（`ast` で関数を単位に解析し，整列メトリクスの参照がどう作られているかを規則で判定）
- 出力 JSON: [`alignment_reference_audit.json`](alignment_reference_audit.json)
- 出力内のファイル名はリポジトリ相対パスとし、`out/` / `build/` / `__pycache__/` の複製・生成ツリーは走査しない。
- 各ヒットは人手で読み，判断と対応を §3 に記録する。ツールのヒットは「レビュー候補」であり、判定そのものではない。

## 判定結果

| # | 対象 | 参照の作り方 | 判定 | 対応 |
|---|---|---|---|---|
| 1 | `PencilPrintLab/rd9_integer_lock.py::metrics_sequence` | **効果適用前の生入力**の整数シフトを参照 | **実欠陥（高）** | RD10 で参照を「frame 0 の出力の厳密整数シフト」に変更。RD9 の「content-lock と screen-lock は同等（0.149）」という結論は撤回。RD10 で再測定し、dither 単独の content-lock は内側平均 0.00000000、screen-lock は 0.125〜0.148 に分離 |
| 2 | `PencilPrintLab/rd6_temporal_deep.py::deep_temporal`（RD7 / RD8 が再利用） | 処理済み frame 0 を **bilinear 0.5k 平行移動**した参照 | **blur 限界（中）** | 参照クラスは正しい（処理済み出力）が、参照の resample が dither を low-pass するため残差は blur 限界。判定に使えない旨を RD10 の評価計画 §11.2 と README に明記。RD10 は整数系列（補間なしの参照）を追加して測り直した |
| 3 | `WatercolorLab/rd0_baseline.py::temporal_metrics`（RD1 / RD3 / RD5 / RD6 / RD7 が再利用） | 処理済み frame 0 を **bilinear 0.5k 平行移動**した参照 | **blur 限界（中）**＋**名称の誤解** | 参照クラスは正しい（RD5/6/7 は候補出力を受け渡している）。ただしキーの名前 `integer_alignment_probe` は 0.5k（整数ではない）を指しており誤解を招く。README・評価計画に blur 限界と名称の注意を追記（§4） |
| 4 | `RetroImageLab/retro_lab.py::temporal_dither_study` | 処理済み `rendered[0]` を **numpy の整数スライス**で参照 | **問題なし** | 参照は処理済み出力、参照側の再サンプルは無い。RD10 の「整数系列で content-lock の残差が 0」と整合する |
| 5 | `RetroImageLab/retro_lab.py::texture_frequency_suite_study` | `INTER_NEAREST` の warpAffine | **誤検知** | 回転行列でマスクを strand 軸へ揃える処理であり、移動の参照ではない。測定参照に影響しない |
| 6 | `RetroImageLab` の video grain 系 2 関数 / `GlowLab/glow_threshold_temporal_lab.py` | 光学フローの motion map による `remap` | **問題なし（情報）** | 参照は処理済み出力で、motion map に基づく比較。補間による限界は残るが、参照クラスの誤りではない |
| 7 | `DenoiseLab` / `UpscaleLab` / `MatteLab` / `DetailSharpeningLab` / `GlowLab` の `INTER_NEAREST` | コンタクトシートの拡大表示 | **問題なし** | 出力表示のみ。測定参照ではない |

監査結果が示すとおり、構造的に同じ誤りを共有していたのは PencilPrintLab と WatercolorLab の 2 ラボのみだった。RD10 の方式（参照を処理済み出力に、補間なしの厳密シフトを追加）を適用すれば、どちらのラボも判定可能になる。

## 対応内容

- RD9 の結論撤回と RD10 での再測定: `experiments/PencilPrintLab/README.md`、`EVALUATION_PLAN.md` §11
- PencilPrintLab rd6 / rd8 の blur 限界: `EVALUATION_PLAN.md` §11.2
- WatercolorLab の blur 限界と名称の注意: `experiments/WatercolorLab/README.md`、`EVALUATION_PLAN.md`
- 再発防止: `tools/audit_alignment_references.py`（新規）。`python tools/audit_alignment_references.py --root . --format markdown` で再実行できる

## 未実施・未検証

- WatercolorLab / RetroImageLab / GlowLab の測定値そのものの再実行はしていない（既存 RD の出力はそのまま残している）。したがって上記 #3 の判断は「参照の作り方が blur 限界である」という構造的な確認であり、既存数値の妥当性判定ではない。
- 人手 gate（閾値記入）と blind review の採点は未実施。既存数值は記述的 baseline のままだ。
- 動的照明・実写素材・製品 runtime 経路・GPU 実装はいずれも対象外。

## 関連資料

- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `experiments/PencilPrintLab/EVALUATION_PLAN.md`（§11 実行履歴）
- `experiments/PencilPrintLab/captures/rd10/review/RD10_TABLES.md`
- `tools/audit_alignment_references.py`
