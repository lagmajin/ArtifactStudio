# R&D レビュー運用 Runbook（オフライン）

**最終更新:** 2026-11-17

画像エフェクト R&D の証跡検証からレビュー、ゲート記入、再検証までの手順。製品コード・ビルド・実アプリ操作は含めない。Windows の GUI 操作は行わず、レビュー者が任意でブラウザーで HTML を開く。

## 境界と原則

- **ツールはゲートを設定しない。** 閾値の記入と品質判定は実装者とは別のレビュー者の作業
- `findings_state` は `none` / `advisory` / `scored`、`gate_form.status` は `thresholds_unset` / `thresholds_partial` / `thresholds_entered`
- レビュー者が `gates_fixed=true` と報告しても、GATE_FORM の閾値欄が空なら `gate_claim_mismatch` が立ち、scoreable とは扱わない
- ここでの所見は R&D の証跡であり、製品採用や画質合格を意味しない

## フェーズ 0: 証跡検証

```powershell
python experiments/review_harness/rehearse.py
python experiments/review_harness/generate_review_templates.py
python experiments/review_harness/verify_runbook_phases.py
python experiments/review_harness/intake_template.py --template experiments/review_harness/out/templates/<lab>/<rd>/submission.json
```

`intake_template.py` は空テンプレートを拒否し、検証後に `out/findings/` へ取り込んで `post_findings_verify.py --strict` を実行する。完了済みフェーズ 2/3 のリハーサルは `completed_phases_rehearsal.py` を参照。

`validate_runs.py` が全 run の manifest hash・出力・blind ID を検証する。出力は `out/validation.json`。失敗した run はレビュー対象外として扱う。

## フェーズ 1: blind レビュー準備

```powershell
python experiments/review_harness/build_review_page.py
```

`out/review.html` に証跡画像と空票をまとめる。blind key は折りたたみ状態で、レビュー完了前に開かないこと。各 run の `review/*FORM.md` に所見を記入する。

## フェーズ 2: 閾値記入（レビュー者の手作業・自動化しない）

レビュー者が各 run の `review/GATE_FORM.md` の `tolerance` 欄へ、baseline を見て決めた許容差を記入する。未記入の項目は未評価のまま残す。AI ツールはこの欄を埋めない。

完了後に確認:

```powershell
python experiments/review_harness/manage_gate_forms.py
```

`out/gate_inventory.json` で全 run のフォーム有無を確認する。既存フォームは上書きしない。

## フェーズ 3: 所見 JSON 提出

レビュー者が `out/findings/*.json` へ 1 run につき 1 ファイル置く。

```json
{
  "lab": "PencilPrintLab",
  "rd": "rd5",
  "reviewer": "name",
  "date": "2026-11-18",
  "gates_fixed": true,
  "findings": [
    {"blind_id": "C01", "verdict": "accept", "note": "...", "defect": null}
  ]
}
```

`verdict` は `accept` / `reject` / `undecided`。`gates_fixed` はフェーズ 2 を実際に完了した場合のみ `true`。

## フェーズ 4: 所見提出後の再検証

```powershell
python experiments/review_harness/post_findings_verify.py --strict
```

提出前の状態をスナップショットし、全パイプラインを再実行して以下を検証する。

- 証跡 hash が再実行でも通ること
- `findings_state` の遷移が `gates_fixed` と整合すること
- GATE_FORM の実際の閾値欄と報告状態が一致すること
- API Consumer の `scoreable_runs` が独立計算と一致すること

出力は `out/post_findings_verify.json`。`--strict` は整合性エラーで非ゼロ終了する。

## フェーズ 5: 状態の消費

```powershell
python experiments/review_harness/example_api_consumer.py
python experiments/review_harness/downstream_gate_report.py
```

`out/gate_status.json`（schema 1）を機械可読 API として使う。`example_api_consumer.py` は scoreable な run のみを抽出し、`downstream_gate_report.py` は CSV と Markdown を生成する。

## リハーサル（実ゲートを触らずに手順を確認）

```powershell
python experiments/review_harness/state_transition_rehearsal.py
python experiments/review_harness/threshold_entry_rehearsal.py
```

- `state_transition_rehearsal.py`: 一時所見で `none → advisory → scored`（報告上）を検証。閾値は空のままなので Consumer は scoreable に含めない
- `threshold_entry_rehearsal.py`: `out/sandbox/` にハーネスと1 run を複製し、複製の GATE_FORM へ閾値を記入して真の scoreable 遷移を検証。実リポジトリの GATE_FORM は変更しない

## フェーズ進捗の自動確認

```powershell
python experiments/review_harness/verify_runbook_phases.py
python experiments/review_harness/verify_runbook_phases.py --require-complete
```

`verify_runbook_phases.py` はこの Runbook の `## フェーズ N:` 見出しを読み、各フェーズの成果物を検証する。フェーズ 2 と 3 はレビュー者の手作業なので、未実施なら `awaiting_reviewer` と報告され、自動で成功扱いにはならない。`--require-complete` は未完了がある場合に非ゼロ終了する。出力は `out/runbook_verification.json`。

## クリーンルーム受け入れ

```powershell
python experiments/review_harness/cleanroom_acceptance.py
```

ハーネス出力 `out/` を一時退避して空から再生成し、全ツール・全フェーズ・リハーサル・テンプレートを検証する。実 GATE_FORM の閾値が未記入であることも直接確認する。失敗時は退避した出力を復元する。

## 未検証・禁止事項

- ブラウザーでの HTML 表示、レビュー者の実際の判定は未検証
- ビルド、CMake、テスト、実アプリ起動、Windows の GUI 操作は明示指示まで禁止
- ツールが GATE_FORM の閾値を自動記入したり、品質判定を下すことを禁止
