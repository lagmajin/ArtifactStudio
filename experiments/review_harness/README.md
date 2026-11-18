# Offline R&D Review Harness

**最終更新:** 2026-11-18

画像エフェクト R&D の全 run について、以下をオフラインで行うツール。

1. 証跡（manifest に記録した SHA-256）と blind ID の一貫性を検証する
2. blind シート・キー・空票を 1 つの静的 HTML にまとめ、レビュー者が所見を JSON として書き出せるようにする

対象ラボは `WatercolorLab` / `PencilPrintLab` / `MaterialSeparationLab` / `SeamlessTextureLab` の 4 ラボ。ラボを追加するときは `build_review_page.py` / `manage_gate_forms.py` / `validate_runs.py` / `cleanroom_acceptance.py` / `intake_cleanroom_rehearsal.py` の `LABS` 定義を 5 箇所すべて更新する（更新漏れは run 数の不整合として `cleanroom_acceptance.py` が検出する）。

制品コード（Artifact / ArtifactCore / ArtifactWidgets）は変更しない。ビルド、CMake、テスト、実アプリ起動、Windows の GUI 操作は行わない。ブラウザーで HTML を開いて操作するのはレビュー者自身の判断に委ねる。

## 使い方

```powershell
python experiments/review_harness/validate_runs.py
python experiments/review_harness/build_review_page.py
python experiments/review_harness/manage_gate_forms.py --fill-missing
python experiments/review_harness/validate_findings.py --findings 'experiments/review_harness/out/findings/*.json'
python experiments/review_harness/aggregate_review.py
python experiments/review_harness/gate_api.py
python experiments/review_harness/example_api_consumer.py
python experiments/review_harness/rehearse.py
python experiments/review_harness/state_transition_rehearsal.py
python experiments/review_harness/threshold_entry_rehearsal.py
python experiments/review_harness/downstream_gate_report.py
python experiments/review_harness/post_findings_verify.py --strict
python experiments/review_harness/generate_review_templates.py
python experiments/review_harness/verify_runbook_phases.py
python experiments/review_harness/intake_template.py --template experiments/review_harness/out/templates/<lab>/<rd>/submission.json
python experiments/review_harness/completed_phases_rehearsal.py
python experiments/review_harness/cleanroom_acceptance.py
python experiments/review_harness/intake_cleanroom_rehearsal.py
```

出力は `experiments/review_harness/out/`。

- `validation.json`: run ごとの証跡検証結果
- `review.html`: blind シートと入力欄を持つ静的ページ（画像は相対パス参照）
- `gate_inventory.json`: 全 run の GATE_FORM 有無。`--fill-missing` は空欄フォームを生成するが既存ファイルは上書きしない
- `findings_check.json`: レビュー所見 JSON のスキーマ・blind ID・証跡状態の照合結果
- `review_summary.json`: run 別・横断の所見集約（verdict 内訳、欠陥数、ゲート状態）
- `gate_dashboard.html`: 全 run のゲート記入状況ダッシュボード

所見ファイルは `out/findings/*.json` に置く。集約ツールはそこだけを読み、`findings_check.json` などの管理ファイルは対象にしない。

`gate_status.json` は機械可読なゲート状態 API（schema_version 1）。`policy` に「ツールはゲートも品質判定も行わない」と明示し、各 run の証跡・ゲート記入・所見状態を構造化して返す。`findings_state` は `none` / `advisory` / `scored` の3値で、`advisory_only` より曖昧さがない。`gate_claim_mismatch` は「レビュー者が gates_fixed=true と報告したが GATE_FORM の閾値欄が空」な状態を検出する。

`example_api_consumer.py` は API を消費する側の例で、証跡 OK・閾値記入済み・有効な所見・不整合なしの run だけを `scoreable_runs` として挙げる。`rehearse.py` は全ツールを順に実行する統合リハーサル。`state_transition_rehearsal.py` は一時サンプル所見で `none → advisory → scored`（報告上）の遷移を検証し、実際の GATE_FORM 閾値は未記入のままであることと、Consumer がその run を scoreable と誤判定しないことを確認してからサンプルを削除する。

`threshold_entry_rehearsal.py` は実ファイルを触らず、ハーネスと1 run を `out/sandbox/` へ複製し、その複製の GATE_FORM 閾値欄を実際に記入して `findings_state=scored` ・ `thresholds_entered_runs>0` ・ `gate_claim_mismatch=false` ・ Consumer の `scoreable_runs` に含まれることを検証する。実リポジトリの GATE_FORM は変更されない。

`downstream_gate_report.py` は `gate_status.json` をさらに下流で消費する例で、CSV と Markdown を生成する。閾値設定や品質判定は行わない。

`post_findings_verify.py` は所見提出後の再検証自動化。提出前の状態をスナップショットし、全パイプラインを再実行して状態遷移と整合性（GATE_FORM の実際の閾値欄、findings_state、Consumer の scoreable 集合）を検証する。完全な手順は [`REVIEW_RUNBOOK.md`](REVIEW_RUNBOOK.md) を参照。

`generate_review_templates.py` は全 run の提出用 JSON / Markdown テンプレートを `out/templates/<lab>/<rd>/` へ生成する。実際の blind ID を埋め、`gates_fixed=false`・`findings=[]` の空状態にする。提出ディレクトリ `out/findings/` には書き込まない。既存テンプレートは既定では保持し、`--force` で上書きする。

`verify_runbook_phases.py` は `REVIEW_RUNBOOK.md` の `## フェーズ N:` 見出しを読み、各フェーズの成果物を検証する。手作業フェーズ（閾値記入・所見提出）は `awaiting_reviewer` として報告し、自動で成功扱いにしない。`--require-complete` は未完了フェーズがある場合に非ゼロ終了する。出力は `out/runbook_verification.json`。

`intake_template.py` は記入済みテンプレートを検証して `out/findings/<lab>_<rd>.json` へ取り込み、`post_findings_verify.py --strict` まで実行する。空テンプレート（reviewer/date 空・findings 空）は拒否し、既存提出物は `--force` なしでは上書きしない。`completed_phases_rehearsal.py` はサンドボックスでフェーズ 2/3 完了状態（閾値記入＋所見提出）を再現し、6 フェーズすべて `ok` になることを検証する。実リポジトリの GATE_FORM と findings は変更しない。

`cleanroom_acceptance.py` は `out/` を一時退避して空から作り直し、16 ツールを順に実行して全フェーズ・全リハーサル・テンプレート生成を検証する。実 GATE_FORM の閾値欄と実 findings が空であることも直接パースして確認する。失敗時は退避した `out/` を復元する。captures は約 7.9 GB と大きいため実物を読み取るが、ハーネス出力は毎回クリーンに再生成される。

`intake_cleanroom_rehearsal.py` は指定された連鎖 `intake_template.py → post_findings_verify.py --strict → verify_runbook_phases.py` をクリーンなサンドボックスで順に実行する。記入済みテンプレートとサンドボックス内の GATE_FORM 閾値を使い、6 フェーズすべて `ok` になることと、実リポジトリの閾値が 0・findings が空であることを検証する。クリーンルーム受け入れにも組み込まれている。

所見 JSON の例:

```json
{
  "lab": "PencilPrintLab",
  "rd": "rd9",
  "reviewer": "name",
  "date": "2026-11-18",
  "gates_fixed": false,
  "findings": [
    {"blind_id": "T01", "verdict": "undecided", "note": "...", "defect": null}
  ]
}
```

`verdict` は `accept` / `reject` / `undecided`。`gates_fixed=false` の所見は助言扱いとして警告を出すが、データ自体は有効。`validation.json` で失敗している run や存在しない blind ID はエラーとして報告する。

## 使う側の契約

- gate 閾値は各ラボの `review/GATE_FORM.md` にレビュー者が記入する。このツールは閾値を自動決定しない
- blind key はレビュー票と別に管理する。HTML は既定では key を表示せず、レビュー後に展開する
- ここで得た所見は R&D の証跡であり、製品採用や画質合格を意味しない
