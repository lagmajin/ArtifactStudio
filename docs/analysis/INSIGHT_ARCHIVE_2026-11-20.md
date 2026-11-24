# Insight Archive (through 2026-11-20)

**最終更新:** 2026-11-20

> 2026-11-20 にルート `Insight.md` から分離した、修正・訂正・レビュー基盤整備完了・旧 R&D 区切りの記録。各項目は全文を保持する。未完・判断待ち・未検証の項目はルート `Insight.md` を参照。

## 2026-11-18: RD9 の「content-lock は screen-lock と同等」は参照の取り違えによる測定欠陥だった
- **関連:** `experiments/PencilPrintLab/rd9_integer_lock.py`、`experiments/PencilPrintLab/rd10_continuous_threshold.py`。
- **確認できた事実:** RD9 の整数整列 MAE は「効果適用前の生入力を整数シフトしたもの」を参照に比較していたため、dither の追従ではなく効果量の差を測っていた（screen / content_nearest / content_interp がいずれも 0.149 前後）。RD10 で参照を「frame 0 の出力の厳密整数シフト」に変更したところ、dither 単独の content-lock は内側平均 0.00000000（厳密一致）、screen-lock は 0.125〜0.148 となり、明瞭に分離した。
- **価値または懸念:** 「dither 追従を測る計測」を設計する場合、参照は「同じ処理を通した frame 0 の出力」でなければ効果量と追従を分離できない。RD6〜RD9 の半ピクセル系列の整列 MAE は参照の resample が dither を low-pass するため、同じ理由で blur 限界が不可避であり、判定に使えない。
- **次に確認すべきこと:** RD6〜RD9 の記録を「測定方法的限界付き」として再解釈し、README と評価計画の該当記述を修正済み。半ピクセル系列で追従を blur なしに測るには、被写体を整数シフトで再做した系列か、閾値場の式評価そのものを比較対象にする必要がある。
- **未検証:** RD6〜RD9 の各主張が他測定で支持されるか。

## 2026-11-18: precomp 逆引きは実装済み、残る空白は footage/effect（訂正済み）
- **関連:** `docs/analysis/DCC_THIN_AREA_SURVEY_2026-11-18.md`、`Artifact/src/Widgets/ArtifactCompositionGraphWidget.cpp`、`Artifact/src/Service/ArtifactProjectService.cpp`。
- **確認できた事実:** `ArtifactCompositionGraphWidget.cpp:722-760` が `sourceCompositionId()` を辿って composition 間の依存エッジを Flowchart 描画済み。`ArtifactProjectService.cpp:3119-3168` が BFS で直接・推移的自己参照を検出し、`PreComposeManager::registerPrecompLayer` で参照登録もある。当初「precomp 逆引き 0 件」と書いたのは不正確で訂正した。一方 `ArtifactCore` に `effectReferences` / `dependsOn` / `usedBy` / `referenceCount` は 0 件で、effect 参照の逆引き API は無い。
- **残る空白:** 参照可視化と循環検出は precomp / matte に限定。footage は `AssetDatabase` が id↔path マップのみで参照元を持たず、effect も逆引き API が無い。DCC の Dependencies ビュー相当を横断で実装するなら、この 2 種類の参照レコードが必要。
- **次に確認すべきこと:** 既存の `AssetDatabase` / `PreComposeManager` を拡張するか、新規の参照レジストリを置くかの設計判断。
- **未検証:** runtime / UI 受入れ、Undo/CAS 境界。

## 2026-11-18: シーン依存の逆引き可視化も DCC の薄い分野（訂正済み）
- **関連:** `docs/analysis/DCC_THIN_AREA_SURVEY_2026-11-18.md`、`Artifact/src/Diagnostics/AppValidationRules.cpp`、`ArtifactCore/include/Asset/AssetDatabase.ixx`。
- **確認できた事実:** `Artifact/include/**/*.ixx` を grep し `DependencyGraph` / `ReferenceGraph` / `referencedBy` / `collectDependencies` は 0 件。ただし最初に「依存逆引きは完全に 0 件」と書いたのは不正確で、`Artifact/src/Diagnostics/AppValidationRules.cpp` の `ArtifactMatteReferenceRule` が **マット参照に限定した循環依存検出**（DFS + 重複抑制 + `createCircularDependency`）を実装していた。サーベイと本項目を訂正した。
- **残る空白:** マット以外のアセット（footage）・effect・precomp を横断する一般の参照グラフは存在しない。`AssetDatabase` は id↔path マップのみで参照元を持たず、`relinkAssetPath` / `relinkSource` は断片的な修復 API に留まる。
- **価値または懸念:** AE の missing footage や Nuke の Dependencies ビューのように「どこが何を参照し、どこに影響するか」を横断で見せる機能は DCC の基本だが、Artifact はマット循環検出と 1 件ずつの relink に留まっている。
- **次に確認すべきこと:** `AssetDatabase` / `relinkAssetPath` の契約、Problem View との責務分離、Undo/CAS との境界。
- **未検証:** UI / runtime 受入れ、既存 asset identity 設計との整合。

## 2026-11-17: 取り込み連鎖のクリーンルームリハーサルを受け入れに統合
- **関連:** `experiments/review_harness/intake_cleanroom_rehearsal.py`、`experiments/review_harness/out/intake_cleanroom_rehearsal.json`。
- **確認できた事実:** `intake_template.py --skip-verify → post_findings_verify.py --strict → verify_runbook_phases.py` の連鎖を、空から作ったサンドボックスで順に実行するツールを追加した。8 チェックすべて成功し、サンドボックスでは6フェーズすべて ok、findings_state=scored、thresholds_entered_runs=1。実リポジトリは閾値記入 0・findings 空を維持。このツールを cleanroom_acceptance.py に組み込み、再実行は16ツール/9チェックすべて OK だった。
- **価値または懸念:** ユーザー指定の連鎖そのものを自動検証でき、クリーンルーム受け入れに含めることで再現性が担保される。実ゲートへの副作用は直接パースで検出する。
- **次に確認すべきこと:** レビュー者による実テンプレート提出後の本番実行。
- **未検証:** ブラウザー表示、実際のレビュー判定。

## 2026-11-17: ハーネス出力を一から再生成するクリーンルーム受け入れを追加
- **関連:** `experiments/review_harness/cleanroom_acceptance.py`、`experiments/review_harness/out/cleanroom_acceptance.json`。
- **確認できた事実:** `out/` を一時退避して空から作り直し、15 ツール（検証・ページ生成・集約・API・Consumer・テンプレート・3リハーサル・フェーズ検証など）を順に実行した。全 15 ステップ OK、8 チェック OK（19/19 証跡、統合リハーサル、状態遷移、サンドボックス閾値記入、完了フェーズ、フェーズ ok_with_pending、18 テンプレート对、実ゲート未記入）。captures は約 7.9 GB でコピーしないため、R&D データは実物を読み取り、ハーネス出力のみを再生成する設計。失敗時は退避出力を復元する。
- **価値または懸念:** ハーネス出力が既存成果物に依存せず再現できることを証明できる。実 GATE_FORM 閾値 0 も直接パースで確認するため、リハーサルが実ゲートを汚した場合も検出できる。
- **次に確認すべきこと:** レビュー者による実ゲート記入と所見提出後の再受け入れ。
- **未検証:** ブラウザー表示、実際のレビュー判定。

## 2026-11-17: テンプレート取り込みとフェーズ 2/3 完了リハーサルを追加
- **関連:** `experiments/review_harness/intake_template.py`、`experiments/review_harness/completed_phases_rehearsal.py`、`experiments/review_harness/out/completed_phases_rehearsal.json`。
- **確認できた事実:** 記入済みテンプレートを検証して `out/findings/` へ取り込み、`post_findings_verify.py --strict` まで実行するフローを追加した。空テンプレートは `rejected: template is not filled: reviewer, date, findings (empty)` で拒否。記入済みテンプレートの実取り込みは `intake OK`、状態は `none 17 / advisory 1` へ遷移し、削除後は `none 18` に復元。サンドボックスでは閾値記入＋所見提出で6フェーズすべて ok、8チェックすべて成功し、実リポジトリは thresholds 0・findings 空を維持。
- **価値または懸念:** テンプレートと提出物の経路を分離し、空下書きの誤提出を防げる。サンドボックスで完了状態を再現できるため、実ゲートを汚さずに運用確認ができる。
- **次に確認すべきこと:** レビュー者が実際にテンプレートを埋めて提出すること。
- **未検証:** ブラウザー表示、実際のレビュー判定。

## 2026-11-17: Runbook 由来のフェーズ検証と提出テンプレート生成を追加
- **関連:** `experiments/review_harness/verify_runbook_phases.py`、`experiments/review_harness/generate_review_templates.py`、`experiments/review_harness/out/runbook_verification.json`。
- **確認できた事実:** REVIEW_RUNBOOK.md の `## フェーズ N:` 見出しから6フェーズを抽出し、各成果物を検証するツールを追加した。実行結果はフェーズ0/1/4/5が ok、フェーズ2/3が awaiting_reviewer、整体は ok_with_pending。`--require-complete` は手作業フェーズ未完了で非ゼロ終了（exit 1）。提出テンプレートは18 run × 2形式＝36ファイルを生成し、rd5 の JSON には実 blind ID 24件・`gates_fixed=false`・`findings=[]` が入る。提出ディレクトリは README のみで所見なし。实证リポジトリの thresholds entered は0、findings_state はすべて none を維持。
- **価値または懸念:** 手作業フェーズを自動成功させずに進捗を追跡できるため、ゲート未設定という未達状態が見失われない。テンプレートが提出ディレクトリに入らないため、下書きを実所見と誤認するリスクも低い。
- **次に確認すべきこと:** レビュー者がテンプレートを埋めて提出し、`--require-complete` が通るようになること。
- **未検証:** ブラウザー表示、実際のレビュー判定。

## 2026-11-17: 所見提出後の再検証自動化と運用 Runbook を追加
- **関連:** `experiments/review_harness/post_findings_verify.py`、`experiments/review_harness/REVIEW_RUNBOOK.md`、`experiments/review_harness/out/post_findings_verify.json`。
- **確認できた事実:** 所見提出後の再検証ツールを追加した。提出前の状態をスナップショットし、7 ステップのパイプライン再実行後に、GATE_FORM の実際の閾値欄と報告状態の一致・findings_state と gates_fixed の整合・Consumer の scoreable 集合の一致を検証する。所見なしでは steps OK / integrity OK / changed 0。一時所見（gates_fixed=false）では `none 18 → none 17 / advisory 1` と `advisory_runs` の追加を検出し、削除後は元に戻った。実リポジトリの閾値記入は 0 のまま。
- **価値または懸念:** レビュー者の手作業（閾値記入・所見提出）とツールの自動検証を分離した運用が文書化され、状態遷移が自動で追跡できる。ゲート未設定という未達状態は維持される。
- **次に確認すべきこと:** レビュー者による実 GATE_FORM への閾値記入と所見提出後の `--strict` 再実行。
- **未検証:** ブラウザー表示、実際のレビュー判定。

## 2026-11-17: ゲート状態 API と統合リハーサルを追加
- **関連:** `experiments/review_harness/gate_api.py`、`experiments/review_harness/rehearse.py`、`experiments/review_harness/out/gate_status.json`。
- **確認できた事実:** ゲート状態を schema_version 1 の JSON として公開する API を追加した。`policy.gates_set_by_tools=false` / `quality_decision_by_tools=false` を明示し、18 run の証跡・ゲート記入・所見状態を構造化。全ツールを順に実行する `rehearse.py` も追加し、5 ステップすべて OK（19/19 証跡、18/18 ゲートフォーム、所見 0、ゲート記入 0）を確認。リハーサルは所見を捏造せずゲートを触らない。
- **価値または懸念:** ダッシュボード HTML に加えて機械可読 API ができたため、外部ツールがゲート状態を消費できる。状態は「 unmet（ゲート未設定・人手レビュー未実施）」が正直に報告されており、完了判定を誤らせない。
- **次に確認すべきこと:** レビュー者が GATE_FORM.md に閾値を記入し、所見 JSON を提出した後の再リハーサル。
- **未検証:** 外部ツールからの API 消費、レビュー者の判定結果。

## 2026-11-17: 所見集約とゲート記入ダッシュボードを追加
- **関連:** `experiments/review_harness/aggregate_review.py`、`experiments/review_harness/out/review_summary.json`、`experiments/review_harness/out/gate_dashboard.html`。
- **確認できた事実:** 検証済み所見 JSON を run 別・横断で集約し、verdict 内訳・欠陥数・ゲート記入状況を出すツールを追加した。空状態では 18 run / 所見 0 件で正常動作。有効・無効のサンプル所見を置いた検証では、有効 1 / 無効 1 を正しく分離し、undecided 2 件と defect 1 件を集計。ダッシュボード HTML は 18 行・2,860 bytes で全 run のゲート状態を表示。サンプル削除後は空状態に戻る。
- **価値または懸念:** 集約対象を `out/findings/*.json` に限定しないと、管理用の `findings_check.json` 自身を所見として拾うバグがあった。専用ディレクトリで解消済み。gate 未記入の所見は advisory として扱い、品質判定はしない。
- **次に確認すべきこと:** レビュー者による実際の所見提出とゲート記入。
- **未検証:** ブラウザー表示、レビュー者の判定結果。

## 2026-11-17: 所見 JSON 照合と GATE_FORM 統一管理を追加
- **関連:** `experiments/review_harness/validate_findings.py`、`experiments/review_harness/manage_gate_forms.py`。
- **確認できた事実:** レビュー所見 JSON（lab/rd/reviewer/date/gates_fixed/findings）を検証し、blind ID が対象 run の key に存在すること・重複がないこと・verdict が規定値であること・参照 run が validation.json で OK であることを照合するツールを追加した。正常系サンプルは OK、存在しない ID と重複 ID を持つ異常系サンプルは正しく失敗。GATE_FORM は全18 run のうち既存2件を保持したまま16件の空欄フォームを生成し、再実行では created 0 で上書きしないことを確認。証跡検証は変更後も 19/19 OK。
- **価値または懸念:** gate 未設定の状態では所見を助言扱いとし、警告を出すがデータは有効とする設計にした。レビュー者が閾値を記入して gates_fixed=true にした後はじめて採点済み所見として扱える。
- **次に確認すべきこと:** レビュー者による GATE_FORM 記入と所見 JSON の実際の作成。
- **未検証:** 実際のブラウザー表示、レビュー者の判定結果。

## 2026-11-17: 全 R&D run の証跡検証と静的レビュー基盤を用意
- **関連:** `experiments/review_harness/validate_runs.py`、`experiments/review_harness/build_review_page.py`、`experiments/WatercolorLab/captures/rd0/manifest_rd0.json`。
- **確認できた事実:** WatercolorLab RD0〜RD7 と PencilPrintLab RD0〜RD9 の計19 run を横断して、manifest の証跡 hash・出力ファイル・blind key と空票の ID 一致を検証するツールを実装した。実行結果は 19/19 OK。検証時に WatercolorLab RD0 の manifest だけが旧形式の画像パス（review/ プレフィックス欠落）で1件失敗し、manifest と生成器を修正して解消した。静的 HTML も生成でき、19 セクション・16 画像・19 の折りたたみ key を含む。
- **価値または懸念:** gate 未設定と人手レビュー未実施はレビュー者が行う必要がある作業であり、AI 側で勝手に確定できない。この基盤はその作業を可能にするだけで、品質合格や採用判断は行わない。
- **次に確認すべきこと:** レビュー者が GATE_FORM.md に閾値を記入し、blind レビューを実施すること。
- **未検証:** 実際のブラウザー表示、レビュー者の判定結果。

## 2026-11-17: 水彩 R&D は RD7 でひと区切り、次の候補は鉛筆画・版画風変換
- **関連:** `experiments/WatercolorLab/README.md`、`experiments/PencilPrintLab/EVALUATION_PLAN.md`、`docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md`。
- **確認できた事実:** 水彩画風変換は RD0〜RD7 で fixture/baseline、v1〜v5 の比較、時間安定性、性能、各段の blind セットを用意した。 gate 未設定・人手判定未実施のため完了ではなく保留とし、候補 16「鉛筆画・版画風変換」の評価計画を新規作成した。
- **価値または懸念:** 水彩 R&D で得た「近傍演算は premultiplied のまま同じカーネルで処理し、descriptor 差と絶対値を分けて見る」という契約は、次のエフェクトでも再利用できる。
- **次に確認すべきこと:** 鉛筆画・版画風の RD0 で baseline と gate 用の ROI を固定する。
- **未検証:** 鉛筆画・版画風の既存実装は検索範囲で未確認（着手時に再監査）。品質 gate は未設定。

## 2026-11-17: 新規画像R&D候補の着手状況と重複の見取り図
- **関連:** `docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md`、`docs/planned/IMAGE_RND_PRIORITY_PORTFOLIO_2026-11-16.md`、`tools/LensFlareLab/`、`ArtifactCore/include/ImageProcessing/AnamorphicFlare.ixx`、`docs/planned/MILESTONE_ANISOTROPIC_FLOW_BLUR_2026-06-13.md`。
- **確認できた事実:** 新規画像R&D候補20案のうち1〜14は初期資産（ObjectRemovalLab / ImageCorrectionSuite / RetroImageLab / ColorMatchLab 等）があり、未着手は15〜20。アナモルフィック／レンズフレアは LensFlareLab、GlowLab の light leak、Core `AnamorphicFlare`、Artifact `ArtifactCinematicLensFlareEffect` と既存研究が十分あり「新規」候補からは外れる。`AnisotropicFlowBlur` は実装済みだが `watercolor` preset は未実装（2026-07-25 監査）。
- **価値または懸念:** テーマ選定時にこの見取り図がないと、既存研究と重複したラボを量産しやすい。水彩画風変換（候補15）は方向場・紙目・減色の既存資産を再利用できるが、正解画像と素材ライセンスが評価の初期壁になる。
- **次に確認すべきこと:** `experiments/WatercolorLab/EVALUATION_PLAN.md` の gate を RD0 baseline 取得後に固定し、ライセンス確認済みの実水彩参照を追加できるか判断する。
- **未検証:** 学習モデルを使わないアルゴリズムで「水彩らしさ」の blind review gate が成立するか、実写素材のライセンス、製品経路との parity。

## 2026-11-15: Shader graph state guard must accept successful compilation warnings
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::isStateEnvelope`、`dynamic_graph_prototype.py::compile_document_from_data`。
- **確認できた事実:** candidate compile成功resultは`warnings: string[]`を返し、`diagnostics`配列はcompile failure wrapperだけにある。直前のstate guardはcompilation diagnosticsを常時必須にして正常なcompile stateを拒否する形だった。成功時のwarning配列または失敗時のstructured diagnosticsのどちらかを受け入れるよう修正した。
- **価値または懸念:** compile成功/失敗の双方でUI stateを採用できるsource contractになった。browser runtimeでの採用確認は未実施。
- **次に確認すべきこと:** success result with empty/nonempty warningsとunavailable result with diagnosticsを使って初期ロード・action responseの両方を確認する。
## 2026-11-15: Shader graph editor actions should apply responses in request order
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::post`。
- **確認できた事実:** 各UI actionは独立したfetchを開始し、HTTP応答が返った順に`state`を置換する構造だった。連続した操作の応答順が逆転すると古いdocument snapshotが後から画面stateへ入る可能性があるため、actionごとにfocus identityをcaptureし、promise queueで要求順に送信・応答適用するよう変更した。
- **価値または懸念:** editor内の操作順とサーバー処理順・画面snapshot適用順を一致させる。通信遅延時は後続操作が先行応答まで待つため、複数requestの総完了時間は逐次化される。ブラウザ動作確認は未実施。
- **次に確認すべきこと:** 素早いparameter editとstructure actionの連続操作で、focus復元、Undo順、dirty状態が正しく進むことをbrowserで確認する。
## 2026-11-15: Shader graph node rendering should index incoming links once
- **関連:** `experiments/ShaderGraphLab/local_graph_lab.html::renderNodes`。
- **確認できた事実:** input pinごとにrequired-state判定で全linksを走査し、さらにDisconnect button用にlinksを再scan/map/filterしていた。nodes描画ごとにtarget node/socket→`{link,index}` listを一度構築して両方で再利用するようにした。duplicate nodeの描画ordinalも各nodeで`indexOf`とprefix `slice/filter`を行わず、map callback indexと累積countで求める。multi-selection dragのsource node lookupはrender時のID indexを使う。wire描画はlinkごとのdocument node検索とsocketごとのDOM selector queryを、描画ごとのnode-operation/socket-element indexで置き換えた。
- **価値または懸念:** node数N、input数I、link数E、重複node ID走査コストが概ねO(N²+I×E)だった部分をO(N+E+I)へ抑える。wire描画のlinkごとO(N) node検索と多数のCSS selector queryもindex構築へ集約する。UI描画時間やDOM構築全体のボトルネック解消は未計測。
- **次に確認すべきこと:** large graphのrender時間、missing-required表示、multi-writer disconnect index、duplicate ID card offsetをbrowserで確認する。
## 2026-11-15: Shader graph structural edits should preserve repairable parameter records
- **関連:** `experiments/ShaderGraphLab/dynamic_graph_prototype.py` の `delete_node`、`delete_nodes_batch`、`duplicate_nodes_batch`。
- **確認できた事実:** ノード削除は不正な parameter record を残す一方、binding が空になった record を一律削除していた。複製・Expose・Detach・Copy は無関係な graph parameter record や binding が不正だと操作全体を失敗させる場合があった。さらに、list/dict型の不正IDに対する set/dict membership が型例外を起こし得た。削除では有効な binding の参照だけを除去し、構造不正 record は保持するよう変更した。複製では妥当な record/type の binding のみ複製し、不正データは保持・スキップする。Expose/Detach は無関係な不正 record を読み飛ばし、対象 ID の曖昧さ・既存 binding・対象 binding の不正形状を引き続き拒否する。Copy は選択ノードに関係する露出 record のみを検証・exportし、無関係な不正 record を無視する。Paste はclipboard内graph parameter ID/bindingの重複、登録parameterの型不一致、共有valueとnode値の不一致をrejectしてからIDを付け替える。Detach で最後の binding が外れた場合も、parameter record 自体が不正なら修復用に残す。該当する編集経路では、集合照合の前にIDが文字列かを確認するよう修正した。
- **価値または懸念:** R&D editor の repairable document 方針と構造編集の挙動を揃え、局所編集で不正データを不可逆に消したり、独立した編集まで阻害したりする危険を減らせる。現状は prototype 内の実装であり、コアライブラリの採用根拠ではない。
- **次に確認すべきこと:** 明示的な実行許可後、単体削除・一括削除・複製・undo/redo を valid / malformed parameter fixture で操作し、履歴復元と保存再読込まで比較する。現時点ではビルド・テストを実行していない。
## 2026-11-14 — Shader graph promotion has existing consumers

- **関連:** `ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx`, `ArtifactCore/src/Diagnostic/DiagnosticRegistry.cppm`, `ArtifactCore/src/Graphics/MeshRenderer.cpp`, `Artifact/src/Widgets/ArtifactShaderGraphWidget.cpp`, Shader Graph R&D Core境界案
- **確認できた事実:** 現public `Artifact.ShaderNode.Core`は編集可能なunique_ptr/Pin graph、debug state、HLSL splice結果を公開する。`DiagnosticRegistry`も`NodeDebugInfo`をsnapshot保存する。`MeshRenderer`はJSONを読みGraph loweringを呼び、WidgetはNodeGraphとlayoutを編集する。
- **価値 / 懸念:** Core昇格は新しい純粋Document/Validator/IRだけでは完了せず、既存Widget、project JSON/clipboard、診断、renderer adapterのconsumer移行順を守る必要がある。consumerを忘れたmodule分割は破壊的link/build変更になる。
- **次に確認すること:** Core API実装を提案する段階で全consumerのmodule dependency、ownership、seriali
## 2026-11-14 — Current shader graph hash covers emitted source only

- **関連:** `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp`, `tests/ArtifactCore/ShaderGraphContractTest.cpp`
- **確認できた事実:** `compileMaterialGraph()`は`helperHlsl`をhashし、その後`blockHlsl`を64-bit FNV-1aへ流す。Layout JSON bytes、resource identity/revision、backend/deviceはこのhash入力ではない。layout不変性に加え、source-affecting parameterとhelper-sourceの検査ケースを追加した。
- **価値 / 懸念:** source hashをdocument identityやresource invalidation keyとして誤用するのを防ぐ。FNV-1aはcache collision耐性やcross-version semantic identityを自動保証しない。
- **次に確認すること:** 実行許可後にcontract casesを走らせ、backend cacheが必要とするschema/compiler/backend/device generationを別key fieldにする契約を検討する。
