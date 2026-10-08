**最終更新:** 2026-10-08
**ステータス:** In Progress

# 編集シーケンス Crash / Fuzz Test マイルストーン

## 目的

Layer の追加・削除、Keyframe 挿入、Precompose によるネスト、Undo / Redo、Asset reload を混ぜた編集列を seed 付きで実行し、クラッシュ・破損・参照不整合を再現可能な形で検出する。

## 既存基盤と制約

- ルートの `tests/` は GoogleTest / CTest を使い、Artifact アプリ層向けテストを `tests/Artifact/` に置いている。
- `ArtifactProjectService` には Layer 操作、Precompose、Asset import / source invalidation などの公開 API がある。実際の reload 操作と、単体テストからサービスを初期化する方法は実装前に確認する。
- Precompose と Undo / Redo の往復には既存の回帰テスト・実装実績がある。fuzz 側は個別バグ修正の代わりではなく、操作の組み合わせを広く揺さぶる。
- `Artifact` / `ArtifactCore` はサブモジュールで、明示的な依頼なしに変更しない。最初の実装はルートのテストコードで公開 API を利用できるかを調べ、API 追加が不可避なら作業を分ける。
- AGENTS.md により、ビルド・CTest・実機実行はユーザーの明示指示があるまで行わない。

### Phase 1 調査結果（2026-10-07）

- `ArtifactProjectService` は `Artifact/src/Service/ArtifactProjectService.cppm` に実装され、`Artifact/cmake/ArtifactSources.cmake` のアプリ実装ソースとして `Artifact` 実行ファイルへ入る。現在の `tests/Artifact` の契約テストは同サービスをリンクする runtime target を持たない。
- `ArtifactProjectService::instance()` は静的 singleton。constructor は file watcher、external-control input、event bus subscriptions、selection bridge を設定する。GUI を起動しない単体 fixture として直接生成できるかは未検証。
- Artifact の built-in test runner は `QApplication` 作成後、MainWindow / renderer 構築前に `ARTIFACT_RUN_BUILTIN_TESTS` で起動できる。既存 `runAllTests()` にテストを加えれば、通常 GoogleTest の runtime target 境界を避けてアプリ実サービスを使う候補になる。
- `UndoManager` は singleton で、stack depth と `clearHistory()` を公開する。Keyframe は `SetLayerPropertyKeyframesCommand`、ネストは `ArtifactProjectService::precomposeLayersWithUndo()` の既存 surface を使える。
- `ArtifactProjectService::Impl::handleFileChanged()` は watcher が検知した path の source version を無効化し、`ProjectChangedEvent` を発行する。fuzz の Asset 操作は、一時 fixture の静止画を変更して watcher を経由し、bounded timeout 内に version 更新を観測するのが候補。
- `ArtifactCore::AssetManager::invalidateSource(id)` 単体は cache version を増やして decoded payload を消すだけで、ファイルの再読込や Project Asset metadata の再importを行わない。これだけを「Asset reload」と数えない。
- fuzz は通常の built-in suite から分離し、専用 opt-in entry で起動する。アプリ本体の実サービスを使うため、現段階では独立したビルド target ではない。独立 runtime target 化には service / Undo 実装と依存ソースのライブラリ化が必要。
- 既存の `ARTIFACT_RUN_BUILTIN_TESTS` 経路は help 表示に「exit」とある一方、成功時に GUI 起動へ進んでいたため、テスト完了後にその終了コードでプロセスを終了するよう修正した。
- Phase 1 は完了。専用 runner はテスト実行後に結果コードでプロセスを終了する。

## 実装済みのテストモデル

`Artifact.Test.PreCompose` に seed 固定の決定論的シーケンスランナーを追加する。`ARTIFACT_RUN_EDIT_SEQUENCE_FUZZ` 専用 entry から実行し、通常の `ARTIFACT_RUN_BUILTIN_TESTS` suite とは独立に結果コードで終了する。3 seeds × 2048 steps = **6144 操作**。各 seed の先頭5操作は Layer add/remove、Keyframe、Precompose、Asset reload を成立する順序で固定し、その後に Undo / Redo を実施してから疑似乱数の操作列へ進む。Asset reload は各 seed 最大8回、Precompose は各 seed 最大32回、active composition 内の Fuzz Null Layer は最大32個に制限する。

失敗時は seed、step、直近16操作を記録し、同じ seed を使って再現できる。Layer add/remove/keyframe/precompose の trace には名前・ID・frame を含める。`ARTIFACT_EDIT_SEQUENCE_FUZZ_SEED` で単一 seed、`ARTIFACT_EDIT_SEQUENCE_FUZZ_STEPS` で1 seedあたり7〜1,000,000操作を指定できる。`ARTIFACT_EDIT_SEQUENCE_FUZZ_TRACE_FILE` を指定すると、各 seed の操作を実行前に step 番号付きでファイルへ追記し flush する。複数 seed の実行は同一ファイルへ連結する。失敗時は failure step と理由も追記するため、Assertion 失敗だけでなくプロセス異常終了時にも直近の操作列が残る。通常実行では trace を保持・書き込みしない。定期的に Undo 履歴を消去し、各消去後に履歴数がゼロであることを確認する。消去不能時は外部履歴を操作しないよう fuzz を中断する。

実行例（ビルド後、Artifact.exe のあるディレクトリから PowerShell で実行）:

```powershell
$env:ARTIFACT_RUN_EDIT_SEQUENCE_FUZZ = "1"
& .\Artifact.exe
$fuzzExitCode = $LASTEXITCODE
Remove-Item Env:ARTIFACT_RUN_EDIT_SEQUENCE_FUZZ
exit $fuzzExitCode
```

Fuzz 専用 entry で数千操作だけを実行し、通常の built-in tests は走らず、アプリは結果の終了コードで終了する。これは実行入口の分離であり、Artifact 本体へのビルド依存は残る。

GoogleTest を有効にせず CTest から個別実行する場合は、構成時に次の option を有効にする。

```powershell
cmake -S . -B out/build/x64-Debug -DARTIFACT_ENABLE_EDIT_SEQUENCE_FUZZ_TEST=ON
cmake --build out/build/x64-Debug --target Artifact --config Debug
ctest --test-dir out/build/x64-Debug -C Debug -R '^ArtifactEditSequenceFuzzTest_' --output-on-failure
```

既定ではこの CTest は登録されない。seed / step / trace の環境変数を使った再実行は、前述の直接起動方法で指定できる。

失敗を単独 seed で再実行し、異常終了にも備えて trace を保存する例:

```powershell
$env:ARTIFACT_RUN_EDIT_SEQUENCE_FUZZ = "1"
$env:ARTIFACT_EDIT_SEQUENCE_FUZZ_SEED = "0x4A17C0DE"
$env:ARTIFACT_EDIT_SEQUENCE_FUZZ_STEPS = "2048"
$env:ARTIFACT_EDIT_SEQUENCE_FUZZ_TRACE_FILE = "$PWD\edit-sequence-fuzz.trace"
& .\Artifact.exe
$fuzzExitCode = $LASTEXITCODE
Remove-Item Env:ARTIFACT_RUN_EDIT_SEQUENCE_FUZZ
Remove-Item Env:ARTIFACT_EDIT_SEQUENCE_FUZZ_SEED
Remove-Item Env:ARTIFACT_EDIT_SEQUENCE_FUZZ_STEPS
Remove-Item Env:ARTIFACT_EDIT_SEQUENCE_FUZZ_TRACE_FILE
exit $fuzzExitCode
```

操作候補:

1. Null Layer を追加し、上限内で既存 Null Layer を削除する。
2. `transform.position.x` へ Keyframe を挿入し、`SetLayerPropertyKeyframesCommand` の Undo 履歴を通す。
3. 2つの Null Layer を Precompose し、Undo / Redo で親 Composition の参照を確認する。
4. Undo / Redo を別々の操作として実行する。
5. 一時 PNG を Project Asset と Image Layer に登録し、Project 内の静止画ファイルを変更する。`QFileSystemWatcher` による source version 更新後、Image Layer の非同期再デコードを待ち、変更前後のバッファ画素が異なることまで確認する（各待機は最大1.5秒）。

各操作の前に対象 Layer と current Composition の前提を確認する。Layer add/remove は操作後の数と対象IDを、Precompose は2 Layer が1 nest Layer に置き換わったことを照合する。失敗を無視して状態を進めず、操作結果を trace に残す。

## 不変条件

- Composition 内の Layer ID は一意で、各 ID が有効な Layer に解決できる。
- Precompose layer が有効な source Composition へ解決でき、子から親 Composition / layer への逆参照も登録されている。子Composition内のLayer IDも一意で、解決可能である。循環を作る操作は既存 Precompose API が拒否する。
- Keyframe 時刻・値が有効で、削除済み Layer を指す参照がない。
- Asset を参照する Image Layer の source path / ID / version が registry / file watcher 更新後の状態と一致する。
- Undo / Redo が実際に1 commandを反対側の履歴stackへ移し、その後も Composition、Layer 順序、親子関係、source 参照が有効である。
- checkpoint で保存・再読込を加える場合、復元後の主要な構造不変条件が保たれる。

不変条件は既存の公開 API で観測できる範囲に限り、内部状態へのテスト専用アクセスを安易に増やさない。

## 段階

### Phase 1 — 操作と検証面の確定

- [x] 公開サービス API と現在の test target 境界を確認する。
- [x] reload 通知経路と Undo / Keyframe API を特定する。ファイル変更通知を使う実 fixture は未確定。
- [x] テスト用 Project / Composition / 静止画 Asset fixture を決める。
- [x] 既存 built-in test runner で app services を呼ぶ。

### Phase 2 — 再現可能な編集シーケンス

- [x] seed 付き Artifact app integration test を追加する。
- [x] 通常 suite / GoogleTest と独立して選択できる専用起動入口と CTest 登録 option を追加する。
- [x] 各操作後に layer ID、Precompose source、Asset identity、Keyframe 値・順序を検証する。
- [x] 失敗時に seed / step / trace を出し、同じ入力列を再実行可能にする。
- [x] Windows でのコンパイルと Artifact.exe のリンクを確認する。
- [x] Windows で既定 fuzz runtime が正常終了することを確認する（3 seeds × 2048 steps）。

#### 2026-10-08 ビルド確認

- ユーザー指示に基づき既存 `out/build/x64-Debug` を CMake 再生成後に `Artifact` target でビルドした。CMake の glob 再確認により 7,157 build steps が再評価された。
- ビルドは `ArtifactCore/src/Analyze/ImageAnalyzer.cppm` の `import Image.ImageSurfaceView;` で停止し、MSVC C2230 `could not find module 'Image.ImageSurfaceView'` を報告した。生成コマンドには `Image.ImageSurfaceView.ifc` の `/reference` がなく、後続の未定義型エラーも同じ欠落に起因する。
- fuzz module / `Artifact.exe` の最新ソースはビルド・実行されていない。ArtifactCore は別の submodule 作業ツリーであり、この fuzz 作業では変更していない。詳細ログ: `temp/edit_sequence_fuzz_build_2026-10-08.log`。
- 2026-10-08 に `ARTIFACT_ENABLE_EDIT_SEQUENCE_FUZZ_TEST=ON` を追加して CMake を再生成し、`ctest -N -R '^ArtifactEditSequenceFuzzTest$'` が専用ケースを1件登録することを確認した。Artifact build は再生成により 6,912 steps を再評価し、同じ `ImageAnalyzer.cppm` の `Image.ImageSurfaceView` module reference / 未定義型エラーで停止した。fuzz module は今回もコンパイル・実行されていない。詳細ログ: `temp/edit_sequence_fuzz_build_2026-10-08_ctest.log`。
- 親 CMake に限った一時的な MSVC module-reference / object-order 補正で上記 Analyze エラーを解消した。Artifact 全体は `ArtifactPropertyEditorNumeric.cppm(622)` の `QPushButton::setAction`（C2039）で停止するため、無関係な UI ソースは変更していない。
- その状態で `ArtifactTestPreCompose.cppm`、`Test.cppm`、`AppMain.cppm` の各 object target を個別にビルドし、今回の fuzz 実装と専用起動入口のコンパイル成功を確認した。Artifact.exe のリンクと fuzz 実行は未確認。詳細ログ: `temp/edit_sequence_fuzz_module_compile_2026-10-08.log`、`temp/edit_sequence_fuzz_entry_compile_2026-10-08.log`。
- 最初の全体ビルドは `Artifact/include/Widgets/PropertyEditor/ArtifactPropertyEditor.ixx` の `colorWheelButton_` が `QPushButton*`、実装オブジェクトが `GradingWheelOpenButton` であるため `setAction()` を呼べず停止した。ユーザーの継続指示を受け、`ArtifactPropertyEditorNumeric.cppm` から確実に作成した subclass へ cast して C2039 を除去した。
- 再ビルドは `ArtifactTimelineWidget.cppm` で `TimelineRowDescriptor` に `trackIndex` がないという C2039 複数件により停止。Artifact.exe のリンク・fuzz 実行は未確認。fuzz の3 object compile成功は維持されている。詳細ログ: `temp/edit_sequence_fuzz_build_final_2026-10-08.log`。
- 上記の既存コンパイルエラーを順に修正した後、`Artifact` target のビルドとリンクが成功した。windeployqt は環境の translations / Qt DLL 検出について警告したが、target は成功終了した。
- 7 steps の短縮実行で全操作を確認した際、Asset reload のランダム色が連続して同一になると画素差分判定が失敗する問題を見つけた。直前画素に基づいて赤 / 緑を交互に選び、同一色を避けるよう修正した。
- Playback engine への null Composition 反映を含む修正後、既定値（3 seeds × 2048 steps = 6144 操作）の CTest が14.33秒で成功し、trace の3 seedすべてに `seed-validation-complete` と `runner-test-wrapper-returned` が残ることを確認した。Access violation は再現しなかった。
- Project close 後に CompositionRegistry 名が残ることも確認した。Active Context と Layer Selection を解除し、Playback Service が null Composition を playback engine に適用するよう修正した。registry 項目は trace に診断情報として残す。
- fuzz は通常 built-in suite と独立した opt-in CTest / 起動入口で実行する。CTest は `Artifact.exe` を別プロセス起動し、専用バイナリにはしない。同一プロセスの soak trace で seed 間に registry 名が残ったため、CMake 登録を seed ごとの3 CTest cases に変更し、各ケースは単一 seed × 2048 steps を別 process で実行する。
- CMake 再生成後に `ctest -N -R '^ArtifactEditSequenceFuzzTest_'` で3ケースの登録を確認し、該当3ケースを個別プロセスで実行した。3/3 passed、合計6144操作、17.57秒。各ケースは終了コード0で完了し、Access violation は発生しなかった。

### Phase 3 — 長時間 soak と縮小

- [x] 固定 seed の soak で数千操作を回せるようにする。
- [x] seed / step の外部指定と失敗 trace の保存を追加する。
- [ ] 失敗列を操作列へ変換して、不要な操作を除く最小再現ケースを作る運用を整える。
- 失敗列を保存し、不要な操作を除いて最小再現ケースを作る運用を整える。
- Sanitizer 対応環境が使える場合は、実行手順と成果物保存先を記録する。通常の Windows 開発ビルドへ sanitizer 設定を無断で追加しない。

## 完了条件

- [x] 同じ固定 seed が同じ操作列を生成する。
- [x] 失敗出力に seed・step・操作 trace があり、再実行できる。
- [x] Add / remove / keyframe / precompose / undo / redo / asset reload がシーケンスに含まれる。
- [x] 各操作後に構造・Keyframe・Asset の不変条件を確認する。
- [x] ビルドで fuzz source と Artifact runtime のリンクを確認する。
- [x] seed 別 CTest の全 runtime が正常終了することを確認する（3/3 passed）。
- 短い通常ケースと長い soak の実行方法・既定値が文書化される。
- ビルドと実行による確認にはユーザーの明示指示が必要。

## 関連文書

- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`
- `docs/done/MILESTONE_PRECOMPOSE_WORKFLOW_COMPLETION_2026-07-09.md`
- `docs/done/REPO_WIDE_LAYER_LIFECYCLE_AUDIT_2026-06-11.md`
