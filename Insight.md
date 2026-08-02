# Insight Log

実装・調査中に得た、将来の改善案、設計上の仮説、再利用できる知見を記録する。

このファイルの内容は仕様や実装指示ではない。採用・優先順位付け・実装の可否は、別途ユーザーまたは設計レビューで判断する。

## 記録ルール

- 事実と推測を分ける。
- 未検証の内容には `未検証` と付ける。
- 依頼外の変更を避けるため、記録だけで実装を始めない。
- 関連ファイルや次の検証方法を残し、後から再開できるようにする。

## Insights

### 2026-07-28 — 連番シーケンスの再生は ArtifactImageLayer::draw と ImageSequenceSource の接続が次段階

- 状態: 未検証（設計案）
- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`ArtifactCore/src/Media/ImageSequenceSource.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- 事実: 今回の実装で sequence は 1 レイヤーに集約され、`sequencePaths` / `sequenceFrameRate` がレイヤー JSON に永続化されるようになった。ただし表示は代表フレーム（先頭）固定で、`ImageSequenceSource`（bounded LRU キャッシュ・先読み・差し替え検出実装済み）は Artifact 側から未使用のまま。
- 閃き・仮説: `ArtifactImageLayer` の Impl に `ImageSequenceSource` を保持し、コンポジションフレーム→（layer inPoint/startTime と sequenceFrameRate 換算）→ `seekSourceFrame` でフレーム切替するのが最小接続。AssetManager の sourceVersion 更新との二重キャッシュ（フレーム単位 LRU vs 代表パス単位 decodedPayload）の役割分担整理が必要。
- 価値・懸念: 動画デコードに依存せずタイムライン再生の基盤ができる。懸念は draw ホットパスでの同期読込（現行 prefetch は単一画像前提）と、フレーム切替時の GPU テクスチャ共有（`canShareSourceGpuTexture`）の整合。
- 次の確認: ビルド確認後、実機で sequence ドロップ→保存→再読込での関係維持を検証し、そのうえで draw 接続の実装単位を切る。

### 2026-07-28 — 非同期インポートと同期インポートの責務差は統合余地がある

- 状態: 未検証
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`（`importAssetsFromPaths` / `importAssetsFromPathsAsync` / `registerImportedAssets`）
- 事実: 同期版は「検出→フレームレート入力→コピー→登録」、非同期版は今回の対応で「コピー→検出→入力→登録」となり、キャンセル時の振る舞いが異なる（同期は中止、非同期は単体登録へフォールバック）。重複登録は `ArtifactProject::addAssetFromPath` の canonical パス重複排除で防がれる。
- 閃き・仮説: 将来的に同期版を非同期版＋完了待ちに寄せるか、登録部分を `registerImportedAssets` へ共通化すると二重実装を解消できる。
- 次の確認: 同期版の呼び出し元（Asset Browser の明示 import など）でキャンセル時中止の振る舞いが必要かユーザー確認。

### 2026-07-27 — SharedPtr への段階移行は std::shared_ptr との相互運用で進めやすい

- 状態: 調査中
- 関連: `ArtifactCore/src/Memory/SharedPtr.cppm`、`Artifact/src/Service/ArtifactEffectService.cppm`、`Artifact/src/Layer/ArtifactVideoLayer.cppm`
- 事実: `SharedPtr` は `std::shared_ptr` からの暗黙/明示構築と代入を受けられ、`makeShared(T*, deleter)` も備えているため、既存の `std::shared_ptr` 返却点を一気に総置換せずに wrapper 側へ寄せられる。
- 閃き・仮説: effect / video / undo のような高頻度パスは、所有型を `SharedPtr` にしても内部実装の一部に `std::shared_ptr` を残す段階移行が現実的。完全移行より先に、API 境界の direct `std::shared_ptr` を減らす方が安全に波及しやすい。
- 価値・懸念: 置換範囲を小さく保ちながら移行を進めやすい。一方、`makeShared(T*, deleter)` の使い方を雑に広げると、所有責任が見えにくくなるので、release 系の一時橋渡しに限るのがよさそう。
- 次の確認: `ArtifactProjectService` / `ArtifactPropertyWidget` など残る濃いクラスターで、API だけ先に `SharedPtr` 化して内部実装は段階的に追従する方針が通るか確認する。

### 2026-07-25 — テクスチャ画像形式の入口統合

- 状態: 実装済み・要検証
- 関連: `ArtifactCore/src/Asset/AssetImporter.cppm`、`Artifact/src/Asset/AssetDirectoryModel.cppm`、OIIO画像読込
- 事実: `FileTypeDetector` は GIF/HDR/WebP/ICO/DDS/KTX を画像として認識していたが、AssetImporter の対応拡張子一覧と Asset Browser の画像判定が一部一致していなかった。
- 対応: AssetImporter、AssetDirectoryModel、ArtifactAssetBrowser を `FileTypeDetector` の拡張子判定へ統一し、Browserだけが個別に扱っていた JPE/JFIF もDetectorへ移した。
- 価値・懸念: テクスチャ形式のインポート導線が統一される。一方、現行の `loadImageViaOIIO` は UINT8 RGBA へ正規化するため、HDR/float の完全な精度保持は別課題。
- 次の確認: OIIO ビルドで各形式の decode 可否を実ファイルで検証し、必要なら float バッファ経路を追加する。

<!--
テンプレート:

### YYYY-MM-DD — 短い題名

- 状態: 未検証 / 調査中 / 有望 / 採用見送り / 完了
- 関連: `path/to/file`、機能名
- 事実:
- 閃き・仮説:
- 価値・懸念:
- 次の確認:
-->
## 2026-07-26 - Resident Debug Agent boundary

- 状態: 実装済み・要実機検証
- Related files/features: `Artifact/src/AppMain.cppm`, `tools/debug-mcp-server`, playback diagnostics.
- Confirmed fact: a resident app-side agent can publish a lightweight playback snapshot without opening `AppDebuggerWidget`, and can cooperatively pause playback from MCP session state.
- Hypothesis / unverified: the same checkpoint path can be extended to property, render-resource, and buffer health probes without materially perturbing playback if sampling remains bounded.
- Value / concern: this gives the AI a live semantic observation point; arbitrary GPU memory inspection remains outside the current boundary.
- 対応: MCP側にwatchの登録・列挙・削除ツールを追加し、アプリ側のresident bridgeがwatch値をスナップショットへ反映するようにした。break hitには直前8件とresume後8件の有界スナップショットを保存する。重複していた軽量bridge writerは撤去し、完全なDebugBridgeFileWriterだけを常駐経路とした。
- Next check: ライブ再生でwatch登録、break hit、pause、resume後のafterSnapshotsを実機確認する。

### 2026-07-27 — 連番検出の欠番契約が実装と不一致

- 状態: 実装済み・要実機検証
- 関連: `ArtifactCore/include/Asset/AssetSequence.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`、画像連番インポート
- 事実: `Asset.Sequence` のコメントは連続した整数フレームだけを連番化すると説明しているが、`detectSequences()` は prefix / suffix / padding と最低枚数だけでグループ化し、隣接フレーム番号の連続性を検査していなかった。したがって `0001-0016, 0018-0048` も1本のシーケンスとして検出されていた。
- 対応: `MissingFramePolicy` を追加し、既定の `Split` では欠番位置で連番を分割するようにした。`Preserve` を明示指定した呼び出し側には `missingFrames` を返すため、将来の hold/error UI を無理なく追加できる。
- 価値・懸念: 欠番を黙って圧縮して再生フレーム番号がずれる事故を防ぐ。一方、VFX素材で意図的な欠番を1アセットとして扱う場合は、インポートUIから `Preserve` と明示ポリシーを選ぶ導線が今後必要になる。
- 次の確認: `J:\dev\ArtifactStudio_TestSequences\png_missing_frame` を使い、Project Viewでの検出結果、タイムライン配置後のフレーム17、保存・再読込時の挙動を実機確認する。

### 2026-07-27 — 2D/3Dギズモのツールモード同期が未統一

- 状態: 実装済み・要実機検証
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Widgets/Render/Artifact3DGizmo.ixx`
- 事実: ツール変更時の `setGizmoMode()` は `TransformGizmo` のモードだけを更新し、`Artifact3DGizmo` の `GizmoMode` は直接同期していなかった。3Dギズモの既定値は `Move` だった。
- 対応: Controllerに2D/3Dモード変換を集約し、3DメニューもControllerを通すようにした。実装を持たない3D `Full` メニューは撤去し、2DのAll/Anchor/Noneは安全に操作できる3D Moveへ正規化する。
- 価値・懸念: Move / Rotate / Scale の表示と操作を一致させ、無反応なFullモードを排除した。真の統合3Dハンドルは、hit-test優先順位を定義してから別途追加する。
- 次の確認: Move / Rotate / Scale を切り替えた際の3D軸描画、hover、drag operationを実機で確認する。

### 2026-07-27 — LODManagerの変更通知で旧値が失われる

- 状態: 実装済み・要実機検証
- 関連: `Artifact/src/LOD/ArtifactLODManager.cppm`、LOD policy
- 事実: `getDetailLevel()` は`currentLevel_`を`newLevel`へ更新した後、`detailLevelChanged(currentLevel_, newLevel)`を発火していたため、通知されるold/newが同じ値になっていた。現時点の検索では接続先は見つからない。
- 対応: 更新前のDetailLevelをローカル保存してから状態を書き換え、通知には正しいold/newを渡すようにした。
- 価値・懸念: LOD変更時だけresourceを切り替える設計で不要な再生成や切替漏れを防げる。`getDetailLevel()` が状態変更を伴う設計自体は、既存API互換のため維持している。
- 次の確認: 実機でズーム境界を往復し、通知値とrendererの品質遷移を確認する。

### 2026-07-27 — 廃止済みサブモジュールのローカル登録情報

- 状態: ユーザー判断待ち
- 関連: 親リポジトリの `.gitmodules`、ローカル Git 設定、`Artifact_dev_review`
- 事実: `git submodule update --init --recursive` は `.gitmodules` に URL がない `Artifact_dev_review` を検出して失敗する。親indexには古いgitlinkが残るが、同ディレクトリは `Artifact` リポジトリの `codex/review-artifact-fixes` worktree として実在し、独自のGit管理情報を持つ。
- 対応: worktreeとその作業を失う危険があるため、自動削除は行わなかった。
- 価値・懸念: 本来の3サブモジュール更新には影響しないが、汎用的な再帰サブモジュール操作は失敗する。親の不正gitlink除去は有益だが、レビューworktreeの保持要否を先に決める必要がある。
- 次の確認: `Artifact_dev_review` を保持するか破棄するかを決めた後、親indexからgitlinkを外すか、正式なsubmoduleとして復元するかを選ぶ。

### 2026-07-27 — RT callback の SharedPtr 化は 2 スロット切替で逃がせる

- 状態: 実装済み・要検証
- 関連: `ArtifactCore/src/Audio/AudioRenderer.cppm`
- 事実: `std::atomic<std::shared_ptr<...>>` を直接 `SharedPtr` に置き換えるのはできないが、2 スロットの `SharedPtr` を用意して、設定側が非アクティブ側へ書き込み後に atomic index を切り替える方式なら、audio callback 側をロックなしのまま `std::shared_ptr` 露出を外せる。
- 価値・懸念: RT 経路のロックを増やさずに基盤 wrapper を導入できる。一方で、複数 writer の同時設定や slot 初期化順序は再確認が必要。
- 次の確認: 実機で callback 付け替え中の音切れ、停止後の null callback、連続再設定時の race を確認する。

### 2026-07-27 — routing を raw pointer key にすると SharedPtr 依存を減らせる

- 状態: 実装済み・要検証
- 関連: `ArtifactCore/src/Audio/AudioMixer.cppm`
- 事実: `AudioBus` の実体寿命を `SharedPtr` の所有で維持しつつ、routing map のキー/値だけを raw pointer にすると、比較・探索の都合で残っていた `std::shared_ptr` 依存をかなり減らせた。
- 価値・懸念: 内部グラフの保持を wrapper へ寄せやすい。反面、raw pointer から `SharedPtr` を復元するための resolve 経路が必要で、バス削除時の参照掃除を忘れると dangling を招く。
- 次の確認: bus 削除、再接続、serialize/deserialize 後に routing が正しい `SharedPtr` を返すかを確認する。

### 2026-07-27 — `ArtifactArray` の実装が二重化している

- 状態: 完了
- 関連: `ArtifactCore/include/Core/ArtifactArray.ixx`、`ArtifactCore/src/Memory/ArtifactArray.cppm`
- 事実: 同一の `ArtifactCore::ArtifactArray` 名に対して、`Core.ArtifactArray` は `::operator new/delete` を直接使うコンテナを、`Memory.ArtifactArray` は `std::allocator` をテンプレート引数に持つ別実装を公開していた。全ソース検索では前者だけが `Core.ArtifactFoundation` 経由で利用されていた。
- 対応: 未使用の `Memory.ArtifactArray` 実装とCMakeの明示登録を削除し、`Core.ArtifactArray` を唯一の `ArtifactArray` 定義にした。
- 価値・懸念: 型名衝突とアロケータ方針の曖昧さを解消した。今後は用途別確保が必要な箇所で `Memory.ArtifactAllocators` と `std::pmr` コンテナを明示利用する。
- 次の確認: CMake再生成後にモジュール依存スキャンと通常ビルドで未参照モジュール削除を確認する。

### 2026-07-29 — ドキュメント INDEX の詳細 Status 同期と自動生成が衝突する

- 状態: 緩和策実装済み・再生成確認待ち
- 関連: `tools/generate_doc_inventory.py`、`docs/INDEX_GENERATED.md`、2026-07-27 以降の planned milestone 文書
- 事実: 生成スクリプトは新規 Markdown を収集できるが、文書ヘッダの短い Status だけを抽出するため、実装監査に基づき INDEX へ手動同期した詳細な Partial／Not started の説明を再生成時に失う。また、最新 milestone 文書が生成後に追加された場合は INDEX から欠落する。
- 価値・懸念: `tools/generate_doc_inventory.py` に既存 INDEX の詳細 Status 引き継ぎと、`状態:`／`進捗状態:` の抽出を追加した。これにより新規文書の収集と既存監査状態の保持を両立できる見込みだが、実 INDEX 再生成時の差分確認はまだ行っていない。
- 次の確認: INDEX をバックアップ可能な手順で一度再生成し、新規 milestone の収録、既存 Status の保持、件数・分類差分を確認する。

### 2026-07-29 — ドキュメント INDEX dry-run はファイルごとの git log がボトルネックになる

- 状態: 未解決・性能課題
- 関連: `tools/generate_doc_inventory.py::get_git_last_modified`
- 事実: 生成器は Markdown ごとに個別の `git log -1` を実行する。1100 件超の文書を対象に Status 引き継ぎを含む dry-run を行ったところ、長時間経過しても完了せず、処理を停止した。INDEX の書き込みは発生していない。
- 価値・懸念: 文書数が増えるほど生成・検証の反復が遅くなり、Status 同期の安全確認を阻害する。git log の一括取得またはファイル単位の不要な履歴照会削減が必要。
- 次の確認: `git log --name-only` 等の一括履歴マップを作り、現行出力と同じ Modified 日付を保ったまま生成時間を測定する。

### 2026-07-30 — Audio Scrub の再入場時バッファ境界

- 状態: 静的対策実装済み・実機確認待ち
- 関連: `Artifact/src/Audio/ArtifactAudioScrubController.cppm`
- 事実: `stopScrub()` は出力バッファを消去するが、停止通知を経由せず再入場する呼び出しでは前回のキューが残り得る。
- 対応: `startScrub()` でも開始前にバッファを消去し、前回のデバイスオープン失敗状態をリセットする。
- 価値・懸念: 新しいドラッグで古いスクラブ音が再生される可能性を抑えた。実機で stop/start、再入場、デバイス失敗復帰を確認する必要がある。

### 2026-08-01 — フレームギズモの2D／3D操作経路統一

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact3DGizmo`
- 事実: ビューキューブ後は2Dレイヤーにも3Dフレームギズモを描画していたが、入力条件が3Dレイヤー限定だったため、ハンドル操作とドラッグ移動が開始できなかった。
- 対応: フレームギズモの描画・ヒットテスト・ドラッグ更新を3Dギズモへ統一し、2Dレイヤーでは位置・回転・スケールだけ既存の2D Transformへ書き戻す。Text Gizmoは専用責務として維持する。
- 価値・懸念: ビューキューブの有無でギズモ操作経路が分岐しなくなる。2D通常表示、ビューキューブ表示、3Dレイヤーでのハンドル位置と回転／スケールの実機確認が必要。

### 2026-08-01 — BrushTool の共有状態と VP 入力

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Application/ArtifactApplicationManager.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Tool/ArtifactBrushTool.cppm`
- 事実: Brush / Eraser の入力は Viewport Controller から `ArtifactBrushTool` へ渡される。Diameter、Opacity、Hardness、Spacing、Angle、Roundness は Application Manager が所有する共有ツールへ設定し、PaintLayer がストローク単位で参照する構成にした。
- 価値・懸念: Tool Options と Viewport が別インスタンスを参照して設定が反映されない問題を避けられる。一方、途中適用されたストロークの Undo 粒度と筆圧入力は未検証である。
- 次の確認: 実ブラシ描画で設定変更が即時反映されること、連続ストロークの Undo が一筆単位になること、Eraser の透明化が期待どおりであることを確認する。

### 2026-08-01 — 図形の中心作成はプレビューだけでなく確定矩形も中心基準にする

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、Rectangle / Ellipse tool
- 事実: Alt ドラッグは開始点を中心として扱う必要がある。終点だけを開始点から反対側へ延長しても、確定時の `QRectF(start, end)` が中心を反映しないため、プレビューと生成結果の位置がずれる。
- 対応: 中心作成フラグをセッション状態として保持し、プレビューと確定処理の双方で `center - delta` から `center + delta` の矩形を生成する共通計算を使用した。
- 価値・懸念: Alt／Shift+Alt の作成結果と表示が一致する。実機でマウス移動中の修飾キー切替、負方向ドラッグ、マスク／シェイプ双方を確認する必要がある。

### 2026-08-01 — Text ツールの作成候補は Esc で確定前に破棄する

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `ArtifactCompositionRenderController`、`ArtifactCompositionEditor` の Text tool input
- 事実: Text ツールのマウス押下後からリリース前まではレイヤー未作成の候補状態だが、既存の Esc 処理はマスクとブラシに限定されていた。
- 対応: 候補フラグ、ドラッグ状態、始点／終点を消去する `cancelTextToolInteraction()` を追加し、エディタの Esc 処理から呼び出すようにした。
- 価値・懸念: クリック位置を誤った場合に不要な Text レイヤーを作成せずに操作を取り消せる。確定済み Text の編集ダイアログに対する Esc は既存の Qt ダイアログ責務を維持する。

### 2026-08-01 — Point Text の Enter は編集確定、Box Text は改行を維持する

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm::ArtifactTextEditorDialog`
- 事実: 既存の Text 編集ダイアログは通常の Enter を常に `QTextEdit` に渡すため、Point Text でも改行が挿入されていた。Box Text は段落編集のため通常 Enter を改行として扱う仕様である。
- 対応: Point Text の通常 Enter を `accept()` に接続し、Ctrl+Enter の既存確定操作と Esc の破棄操作は維持した。Box Text と修飾キー付き Enter は従来どおり編集欄へ渡す。
- 価値・懸念: Point／Box の編集モデル差をダイアログ側で明示できる。実機で IME 確定キー、Ctrl+Enter、Box Text の改行挿入を確認する必要がある。

### 2026-08-01 — Puppet のピン操作は既存選択状態をキー操作へ接続する

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `ArtifactPuppetTool`、`CompositionRenderController`、`CompositionViewport`
- 事実: Puppet はクリックでピン追加／選択と変形を行っていたが、CtrlクリックのStarch種別指定とDelete/Backspaceによる選択ピン削除が入力経路に接続されていなかった。
- 対応: Ctrlクリックで追加直後のピンをStarch(type=1)に設定し、Puppetツール中のDelete/Backspaceを選択ピン削除へ振り分けた。
- 価値・懸念: ピンの基本ライフサイクルがVP操作だけで完結する。回転ハンドル、Overlap深度、キーフレーム化は未検証・未実装である。

### 2026-08-01 — MotionSketch の速度表示に区間加速度を追加する

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の MotionSketch overlay
- 事実: スケッチ中のオーバーレイはサンプル数と直近区間の速度を表示していた。
- 対応: 3点以上のサンプルがある場合、直近速度と1区間前の速度の差を加速度として計算し、HUDに `v` と `a` を表示する。表示幅も拡張した。
- 価値・懸念: ドラッグの加速／減速を記録中に確認できる。サンプル間隔が一定でない場合は厳密な物理単位ではなく、隣接サンプル差分の表示である。

### 2026-08-01 — Brush は QTabletEvent を既存 Mouse 経路へ合流させる

- 状態: 実装済み・ビルド／実機確認待ち
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`ArtifactCompositionRenderController`、`ArtifactBrushTool`
- 事実: CompositionViewport は QMouseEvent のみを Controller に渡しており、タブレットの pressure が BrushStroke に届かなかった。
- 対応: `WA_TabletTracking` を有効化し、TabletPress/Move/Release を Controller の既存 Brush 経路へ渡す圧力ブリッジを追加した。BrushTool は pressure を 0.05〜1.0 にクランプし、半径と不透明度へ反映する。通常マウスは圧力 1.0 のまま動作する。
- 価値・懸念: 新しい公開シグナルや描画経路を増やさず、タブレットだけ筆圧を利用できる。実機で押下・筆圧変化・離上、マウス復帰、消しゴムモードを確認する必要がある。

### 2026-08-01 — Clone Stamp は同一 Paint フレームのF32サンプルから開始する

- 状態: 基礎実装済み・ビルド／実機確認待ち
- 関連: `ArtifactPaintLayer::applyCloneStampAtFrame`、`ArtifactCompositionRenderController`、`ToolType::Clone`
- 事実: 既存 PaintLayer は `ImageF32x4_RGBA` のCPUバッファを公開しており、QImageを介さずピクセル複製できる。Clone Stampの既存UI欄はあったが、ToolTypeと入力経路が未接続だった。
- 対応: Clone toolを追加し、Alt+クリックでPaintレイヤーのソースを設定、通常ドラッグでソース／描画位置のオフセットを保った円形スタンプを適用する。選択中レイヤーと異なるPaintレイヤーをVP上でAltクリックした場合は、そのレイヤーをサンプル元にする。Cloneオプションのフレームオフセットをサンプル元の現在フレームへ加算し、時間ずれの複製を可能にした。最初の適用だけUndoスナップショットを記録し、ソース領域は事前コピーして重なりによる自己汚染を防いだ。既存の「位置固定」オプションも共有BrushTool経由で接続し、オフ時はソースを固定する。Esc はドラッグ中の操作と設定済みソースをクリアし、設定中はVPにSourceマーカーを表示する。
- 価値・懸念: 静止画Paintレイヤーの基本的なクローン操作が成立する。別レイヤー／時間オフセット／非整列サンプリング、筆圧ごとのスタンプ形状は次段階であり、実機で境界・重なり・Undoを確認する必要がある。
### 2026-08-01 — Render Queue 選択的レンダリングのジョブモデルを拡張

- 状態: 実装済み（ジョブモデルと JSON 保存/復元、実レンダリング分岐・UI・ビルド/実行検証は未完了）
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`, `docs/spec/SPEC_RENDER_QUEUE_SELECTIVE_2026-07-31.md`
- 事実: 既存 `ArtifactRenderJob` は開始/終了フレーム、解像度などの基本値のみを持ち、選択レイヤー、ROI、パス分割、解像度プリセットを保持できなかった。
- 実装: `FrameRangeMode`、`RegionMode`、`LayerFilterMode`、`ResolutionPreset`、`RenderPassConfig` と関連フィールドを追加し、LayerID リストを含む JSON 保存/復元を追加した。未知の enum 値は後方互換の既定値へ戻す。
- 実装追加: ソフトウェア単一フレーム経路にブラックリスト、Selected/Custom/Solo/Visible フィルタ、ROI クロップ、解像度プリセットを接続し、キュー実行時に Composition/WorkArea/SingleFrame の有効範囲を解決する。
- 実装追加: GPU 単一フレーム経路にも同じレイヤーフィルタ、ROI、解像度プリセットを適用した。GPU は readback 後に ROI/出力サイズを適用する。
- 実装追加: キューの通常フレーム処理（`renderSingleFrame`）にも GPU レイヤーフィルタを伝播し、GPU 初期化サイズを解像度プリセットから解決するようにした。
- 実装追加: `ArtifactRenderQueueService` に `jobSelectiveSettingsAt` / `setJobSelectiveSettingsAt` を追加し、範囲・ROI・レイヤー ID リスト・除外設定・パス設定・解像度プリセットを QVariantMap 経由で UI/自動化から編集可能にした。
- 実装追加: `WorkspaceAutomation` に `getRenderQueueJobSelectiveSettingsAt` / `setRenderQueueJobSelectiveSettingsAt` を登録し、同じ設定を自動化呼び出しから読み書きできるようにした。
- 実装追加: `splitPasses` をキュー開始時に有効な `RenderPassConfig` ごとの独立ジョブへ展開する処理を追加した。各ジョブはパス名をジョブ名・出力名へ反映し、パスのフィルタ/ID リストを通常のレンダー設定へ変換する。
- 実装追加: ROI 使用時の Composition/Half/Third/Quarter 解像度計算をコンポジション全体ではなくクロップ矩形サイズ基準へ変更した。
- 実装追加: `SelectedFrames` 用に `selectedFrameRanges` をジョブへ追加し、JSON/VariantMap で保持できるようにした。キュー開始時は非連続区間ごとに Custom 範囲の独立ジョブへ展開し、出力名へ `_f開始-終了` を付ける。
- 懸念: UI 側での非連続フレーム選択編集は未実装。範囲の終了値は既存キュー契約に合わせて排他的に扱う。
- 改善: SelectedFrames 展開前に不正区間を除外し、重複/隣接区間を統合する。全区間が不正な場合は元ジョブを保持して消失を防ぐ。
- 実装追加: Render Queue preflight で ROI の空/範囲外、SelectedFrames の有効区間ゼロ、Render Pass 分割の有効パスゼロを診断する。範囲外 ROI は実行時クリップのため Warning、空設定は Error とした。
- 修正: `Selected` レイヤーフィルタの空ホワイトリストを「全レイヤー」ではなく「描画対象なし」として扱い、preflight で Error にする。Selected Render Pass の空 Layer ID も同様に検出する。
- 修正: `Solo` フィルタを、Solo レイヤーが存在する時だけ Solo レイヤーに限定し、Solo が一つもない場合は通常の可視レイヤーへフォールバックする AE 互換挙動に統一した。
- 確認・実装: `ArtifactAbstractLayer` には既存の `isGuide()` があるため新しい属性を増やさず、`excludeGuideLayers` を CPU/GPU の全レイヤーフィルタへ接続した。
- 実装追加: 外部レンダラー呼び出しにもキュー実行時に解決した start/end frame を渡し、WorkArea/SingleFrame/SelectedFrames 展開と外部経路の範囲を一致させた。
- 実装追加: Composition View の既存グリッド描画に、ズームに応じた 1-2-5 系列の自動主間隔を追加した。固定間隔へ戻す `setGridAutoStepEnabled(false)` も提供し、保存済み `GridSettings` 自体は変更しない。
- 懸念: 自動ステップの目標表示間隔は 100 viewport px 固定で、複数グリッド/極座標/アイソメトリック/ラベル描画は未実装。
- 実装追加: 既存 `GridSettings::showNumbers` を Composition View overlay で利用し、主グリッド線の X/Y 座標ラベルを最大 48 個ずつ描画するようにした。ズームアウト時のラベル密集を避けるため画面間隔 24px 未満では抑制する。
- 実装追加: Composition View に極座標グリッド表示モードを追加し、同心円リング・24方向の放射線・中心マーカーを既存 primitives で描画する。`setGridPolarMode(false)` で従来の矩形グリッドへ戻せる。
- 実装追加: Composition View にアイソメトリックグリッド表示モードを追加し、30°/150°/90° の3方向の平行線ファミリを描画する。極座標/矩形グリッドとは排他的に表示する。
- 改善: 極座標とアイソメトリックの setter は有効化時に相互排他を適用し、両モードの同時描画を防ぐ。
- 実装追加: 極座標モードで `subdivisions` に基づく細分リングも描画し、主リングとの重複を除外する。細分間隔が 4 viewport px 未満の場合は描画を抑制する。
- 実装追加: アイソメトリックモードにも `subdivisions` に基づく3方向の細分線を追加し、細分間隔が 4 viewport px 未満の場合は抑制する。
- 実装追加: `CompositionRenderController::snapCanvasToGrid()` を追加し、矩形は XY 丸め、極座標は半径/15°角度丸め、アイソメトリックは現在の細分間隔で座標丸めを提供する。極座標中心は controller が保持する最新キャンバスサイズから求める。
- 実装追加: `ArtifactCompositionRenderWidget` の Ctrl レイヤードラッグから `snapCanvasToGrid()` を呼び出し、表示グリッドモードと実際の移動スナップを接続した。既存のコンポジション中心スナップは先に適用し、グリッドスナップが最終位置を決める。
- 実装追加: 自動グリッドステップの目標 viewport 間隔を `setGridAutoStepTargetViewportInterval()` で変更可能にし、表示とスナップの両方が同じ設定を使用する。値は 24〜512 px に制限する。
- 懸念: パス展開はキュー開始時に行うため、開始後のジョブ数表示が増える。既存のジョブ ID はインデックスベースなので、UI は開始前に設定する必要がある。
- 懸念: ガイドレイヤーの判定 API、SelectedFrames の非連続区間、GPU レンダーパス、パス別出力、Advanced UI は未接続。現状 `SelectedFrames` は保存済みの custom 範囲へフォールバックする。
- 次に確認: `renderSingleFrameGPU` と外部レンダラーへ同じ範囲・レイヤーフィルタ契約を伝播し、Render Queue UI からこれらの設定を編集可能にする。
- 実装追加: アイソメトリックグリッドのスナップを画面 X/Y の独立丸めから斜交基底の格子座標丸めへ変更した。描画中の斜線ファミリの交点にスナップしやすくなる。描画とスナップの基底値が将来別々に変更されないかは未検証。
- 修正: Composition View の Shift+ホイール横移動は、横チルト入力が存在する場合に `angleDelta().x()` を優先するよう変更した。横デルタを持たないマウスでは従来どおり縦デルタを横移動へフォールバックする。
- 実装追加: Render Queue のジョブ詳細プレビューに、サービスが保持する選択的レンダー設定（範囲、領域、レイヤーフィルタ、解像度、Crop）を表示するようにした。編集 UI そのものではないが、設定が保存されているかをジョブ単位で確認できる導線になる。
- 実装追加: Composition Render Widget に `setRotationSnapDegrees()` / `rotationSnapDegrees()` を追加し、Shift+キャンバス回転の刻みを既定45度から外部設定できるようにした。値は1〜360度へ制限し、15/30/45/90度などのプリセットを利用できる。
- 改善: Shift+回転ドラッグ中は Alt で15度、Ctrl で90度の一時プリセットを選べるようにした。通常は widget に設定された既定刻みを使うため、既存操作との互換性を保つ。
- 改善: Composition View の回転ドラッグ角度を横移動量だけでなく、ビューポート中心からの開始ベクトルと現在ベクトルの角度差から算出するようにした。中心付近だけは角度が不定になるため、従来の横移動フォールバックを残している。
- 改善: 回転ギズモモードでも既存の `drawAnchorCenterOverlay()` を表示するようにし、回転中心を視覚的に確認できるようにした。描画実体は既存のアンカー表示を再利用している。
- 実装追加: タブレット入力の `xTilt/yTilt` を `ArtifactBrushTool` へ転送し、傾きの大きさをブラシ真円度、方向をブラシ先端角へ反映した。筆圧と同様にストロークへ記録し、TabletRelease で傾きを初期化する。
- 実装追加: `ArtifactBrushTool` に `pressureAffectsSize` / `pressureAffectsOpacity` を追加した。既定は両方有効で、筆圧をサイズだけ／不透明度だけへ割り当てる編集面を将来追加できる。
- 修正: タブレット筆圧の下限を0.05から0.0へ変更し、筆圧ゼロを正しく無描画として扱えるようにした。通常のマウス／TabletRelease の復帰値は1.0のまま。
- 実装追加: 3Dギズモのドラッグ中に、Move/Rotate/Scale のモード名と Position・Rotation・Scale の現在値を右上HUDへ表示するようにした。既存の renderer オーバーレイ primitive のみを使用し、操作終了時は自動的に消える。
- 修正: 3Dギズモから2D/3Dレイヤーへスケール値を反映する際、各軸の絶対値を最低0.001にクランプするようにした。反転スケールの符号は維持し、ゼロ化による退化や後続の逆変換不安定化を避ける。
- 改善: 3DギズモのドラッグHUDに、`localBounds` と現在スケールから算出した表示サイズ（px）を追加した。位置・回転・スケールと同じドラッグ時の値として提示する。
- 実装追加: 3Dギズモの Rotate モードで Shift を押している間は回転を15度刻みにスナップし、Ctrl 同時押しではスナップを無効化する。既存のギズモ内部ドラッグ計算後に適用するため、位置・スケールの挙動には影響しない。
- 実装追加: 3Dギズモのドラッグ開始時に Position／Rotation／Scale のスナップショットを取得し、終了時に `GizmoTransformUndoCommand` を1件だけUndoManagerへ登録する経路を追加した。3D投影フレームのコーナードラッグと通常の3D軸ドラッグを対象とし、2D専用TransformUndoCommandとは分離している。
- 改善: 3DギズモUndo/Redoの復元後に既存の `LayerChangedEvent::Modified` を発行するようにし、プレビュー・タイムライン等の購読側へ変更を伝播するようにした。
- 安定化: 3Dギズモのドラッグが成立しなかったマウスリリースでも保留中のUndoスナップショットを破棄するようにし、次回操作へ古い履歴状態が漏れないようにした。
- 改善: 3DギズモのUndo登録前に Position／Rotation／Scale の差分を閾値比較し、実質的な変更がないクリック／ドラッグでは空のUndo項目を作らないようにした。
- 実装追加: Composition View のホイールズームを既存の16msタイマーで約150ms補間するようにした。smoothstep補間とズーム位置のキャンバスアンカー再計算により、マウス位置を保ったまま滑らかに拡大縮小する。連続ホイール入力では目標倍率を更新して追従する。
- 改善: Composition View の `zoomIn()` / `zoomOut()` も同じ smooth zoom 経路へ統一した。ツールバーや既存アクションからの倍率変更でも、ビューポート中央をアンカーに補間される。
- 安定化: Reset View／Fit／100% など即時ビューポート操作では進行中のズーム補間をキャンセルし、タイマーの残りフレームが新しいビュー状態を上書きしないようにした。
- 実装追加: キャンバス回転ドラッグ中の角度を既存Info Overlayへ表示し、マウスリリース時に消去するようにした。スナップ後の実際の角度を1桁小数で表示する。
- 改善: Zoom Tool の通常クリック／Altクリックもホイール・ツールバーと同じsmooth zoom経路へ統一した。クリック位置をアンカーにした拡大／縮小が約150msで補間される。
- 実装追加: Zoom Tool のマーキー領域ズームも、選択領域のキャンバス中心をビューポート中心へ移す補間経路へ変更した。倍率計算は従来どおり領域全体が収まる値を使い、表示遷移だけを滑らかにしている。
- 修正: 回転角度HUDの消去を回転ドラッグ時だけに限定した。Clone／Eraser等、別操作が保持しているInfo Overlayを通常のマウスリリースで消さないようにした。
- 実装追加: `CompositionRenderController::zoomAtFactor()` も150msのsmoothstep補間へ変更した。Editorが直接呼ぶホイール／ズーム操作でも、物理viewportアンカーを維持したまま拡大縮小する。Fit／100%は進行中補間をキャンセルする。
- 安定化: Controller側のズーム補間に世代番号を追加し、連続入力やFit／100%切替後に古い `singleShot` コールバックが新しいズーム状態へ干渉しないようにした。
- 実装追加: Composition Render Widget のパンを、ドラッグ終了時の速度から0.86倍ずつ減衰させる慣性スクロールへ拡張した。既存16msタイマーを共有し、ホイール入力開始時には慣性を停止する。
- 安定化: 慣性パンへ渡す速度を各軸±3px/msに制限し、低頻度イベントや一時的な入力遅延による過大な飛びを防いだ。
- 実装追加: 実際の入力を処理する `ArtifactCompositionEditor` 側にもパン慣性を追加した。RenderWidget側と同じ速度上限・0.86減衰・weak_ptr再スケジュールを使い、Space／中ボタンのパン終了後も自然に継続する。
- 安定化: CompositionEditorで新しいマウスジェスチャーが始まると、前回のパン慣性を即時停止するようにした。選択やギズモ操作中に背後のパンが継続しない。
- 修正: 実際のCompositionEditorのShift+ホイール横移動も横チルト入力を優先するよう変更し、横軸のあるマウスと縦軸のみのマウスの両方で意図どおり動作するようにした。
- 安定化: CompositionEditorのホイール入力開始時にもパン慣性を停止し、ホイールズーム／横移動と残留パンアニメーションが同時に走らないようにした。
- 改善: Controllerの連続ズーム中はView Historyを入力イベントごとに追加せず、アニメーション開始時の1件だけを記録するようにした。`zoomInAt/zoomOutAt` の重複pushも削除した。
- 改善: `CompositionRenderController::panBy()` でも履歴追加を250ms単位にまとめ、パン慣性の各フレームが個別のView History項目にならないようにした。パン入力開始時には進行中のズーム補間もキャンセルする。
- 改善: CompositionEditorでEscapeを押した場合もパン慣性を即時停止するようにし、既存の各種キャンセル操作と同じくビューの継続移動を止められるようにした。
- 安定化: `CompositionRenderController::panBy()` にX/Yの有限値検証を追加し、NaN／無限値を履歴・renderer・ズームキャンセル処理へ流さないようにした。
- 改善: Controller側の各ズーム補間フレームで renderer の `zoomAroundViewportPoint()` を使うようにし、キャンバス回転中でもviewportアンカーがずれないようにした。手計算の `setZoom + setPan` は削除した。
- 安定化: Controller側の再スケジュール式ズームコールバックをweak_ptr＋一時的shared_ptrで保持する方式へ変更し、補間終了後にstd::functionが自己循環して残留しないようにした。
- 安定化: ControllerのReset View／Fit Selection／Fit Visible でもズーム補間の世代を無効化するようにし、即時ビュー変更後に古い補間が再適用されないようにした。
- 安定化: `zoomAtFactor()` に有限値・正値チェックを追加し、外部入力のNaN／無限値／0以下でズーム状態やアニメーションを壊さないようにした。
- 改善: 3D投影フレームの上下左右エッジハンドルを、ドラッグ時にそれぞれY／X軸のスケール経路へ振り分けるようにした。従来はコーナーと同じScreen軸へ流れていたため、単一辺の操作でも両軸が変化する可能性があった。コーナーは既存のScreen軸経路を維持する。
- 実装追加: 3D投影フレームの上側に回転ハンドルを描画し、投影空間でヒットテストするようにした。選択時は既存の3DギズモのZ軸Rotate経路へ振り分け、既存の回転HUD・Shift 15度スナップ・Undoスナップショットを再利用する。ハンドル位置はフレーム上辺からのローカル空間オフセットで決める。
- 改善: 3D投影フレームの回転ハンドル用leader lineを、上辺との接続線ではなくフレーム中心からハンドルまでの線として描画するようにした。投影後のフレーム姿勢に追従するため、回転中の現在方向を視覚的に読み取りやすくなる。
- 実装追加: 3D投影フレームのコーナー／エッジ／回転ハンドルをダブルクリックした際のリセット経路を追加した。回転ハンドルはZ回転を0度、その他のフレームハンドルはXYスケールを1.0へ戻し、GizmoTransformUndoCommandとLayerChangedEventを通してUndo・表示更新へ接続する。
- 改善: 3D投影フレームの上下左右エッジをShiftドラッグした場合、ポインタで駆動した軸の倍率をもう一方にも適用し、ドラッグ開始時のアスペクト比を維持するようにした。Ctrl併用時は自由リサイズを優先し、コーナーと回転ハンドルには影響しない。
- 改善: 3D投影フレームのエッジリサイズで、Ctrlなしの場合は反対側の辺が固定されるよう、ローカル軸方向へサイズ変化の半分だけギズモ位置を補正するようにした。Ctrl時は中心固定として位置補正を行わない。カメラ投影後の見た目ではなく、レイヤーのワールド変換から軸方向を求める。
- 改善: ブラシカーソルプレビューにも現在の筆圧を反映するようにした。Pressure Affects Size時は直径を筆圧で縮小し、Pressure Affects Opacity時は透明度を低下させる。筆圧0でもカーソル自体は視認できる最小径・最小透明度を保つ。
- 改善: ブラシドラッグ中のストロークプレビューにもPressure Affects Opacityを反映し、カーソル輪郭だけでなく描画中の軌跡も現在の筆圧に応じて薄くなるようにした。筆圧0付近でも軌跡が完全には消えない下限を設けている。
- 実装追加: Penでマスク頂点を追加する際、Shift押下中は直前の頂点からの角度を45度刻みにスナップするようにした。既存のpending mask作成と後続のUndo・LayerChangedEventはそのまま利用し、初回頂点や既存頂点編集には影響させない。
- 改善: Penの新規マスク頂点追加でCtrlを押している場合、コンポジション左・右・上・下境界へ10px（ズーム補正済み）の吸着判定を行うようにした。境界スナップは新規頂点作成だけに限定し、既存頂点編集やハンドル操作の座標は変更しない。
- 拡張: Ctrlスナップの候補に、可視レイヤーの既存マスク頂点を追加した。候補はキャンバス座標へ変換して最近傍を選ぶため、レイヤーごとの移動・回転を考慮できる。ロック状態や既存パスの編集経路には介入せず、新規頂点配置だけに適用する。
- 改善: Ctrlスナップが実際に成立した新規マスク頂点位置へ、作成中だけシアン色のクロスマーカーを表示するようにした。pending maskの解除時に表示状態と座標をクリアし、他のマスク編集やツールへ残留しない。
- 修正: マスク新規頂点のCtrlスナップはAlt併用時に無効化するようにした。Ctrlで吸着、Altで吸着解除という仕様上の修飾キー優先順位を明示し、Shiftの45度制約は別途継続する。
- 改善: 既存マスク頂点のShiftドラッグも、新規頂点追加と同じ8方向（45度刻み）へスナップするよう統一した。複数選択頂点の移動では主頂点の補正デルタが従来どおり他頂点へ適用される。
- 修正: 既存マスク頂点のドラッグ中スナップでもAlt併用時は吸着を無効化するようにし、新規頂点追加時のCtrl／Alt修飾キー優先順位と一致させた。
- 改善: 既存頂点ドラッグ中のPenプレビュー位置を、Ctrlスナップ前のポインタ座標ではなく吸着後のキャンバス座標へ更新するようにした。実際の頂点位置と表示マーカーのずれを防ぐ。
- 安定化: 頂点ドラッグのプレビュー座標更新を、実際に適用したShift／Ctrl補正後のローカル座標から再計算するようにした。生ポインタやスナップ前座標が残る経路を避け、複数選択移動時も主頂点の表示位置と実データを一致させる。
- 拡張: 既存マスク頂点のCtrlドラッグでも、同一レイヤー内の別マスク／別頂点を最近傍スナップ候補に追加した。ドラッグ中の頂点自身と同時選択頂点は候補から除外し、選択群の移動が内部頂点へ引き寄せられないようにした。
- 安定化: 最近傍マスク頂点スナップで直接使用するQt型に`QLineF`のincludeをグローバルモジュールフラグメントへ明示追加した。間接includeへの依存を避け、C++20 modulesの依存境界を保つ。
- 改善: EraserのLast Stroke Only／全フレーム消去後に既存の`publishLayerModified`を呼ぶようにした。描画バッファ更新だけでなく、タイムライン・保存・他の購読側にも消去結果を伝播させる。
- 安定化: Last Stroke OnlyのEraser操作では`canUndo()`を確認してから復元・変更通知・再描画を行うようにした。Undo履歴が空のクリックで不要なModified通知を発行しない。
- 実装追加: Anchor Pointツールの既存中心マーカーに、現在のアンカーX/Yを表示する数値HUDを追加した。3Dは現在フレームのアニメーション値、2DはStaticTransform2Dのアンカー値を使い、Anchor表示が有効なときだけ描画する。
- 修正: 2DアンカーHUDの実装時に、2D変換には`anchorXAt()`が存在しないことを確認し、`anchorPointX()`／`anchorPointY()`へ切り替えた。3Dの時系列アンカー評価とは責務を分ける。
- 改善: Anchor HUDを3DレイヤーではX/Y/Z表示へ拡張した。3D時のみパネル幅を広げ、Zは現在フレームの`anchorZAt()`から取得する。2D表示の密度と既存レイアウトは維持する。
- 実装追加: 3DレイヤーでCtrl+ダブルクリックしたとき、AnchorをlocalBounds中央へ戻し、回転・スケールを考慮したPosition補正を同時に行うようにした。AnchorとPositionの前後値を`AnchorPointUndoCommand`へ保存し、Undo/RedoとLayerChangedEventを接続する。2Dレイヤーには適用しない。
- 安全化: 3D Anchor中心リセットのCtrl+ダブルクリック呼び出しをAnchor Pointツールがアクティブな場合に限定した。通常の選択／編集ツール上のCtrl+ダブルクリックで意図せずAnchorを変更しない。
- 実装追加: Anchor PointツールのCtrl+ダブルクリックによるlocalBounds中央リセットを2Dレイヤーにも拡張した。2D回転・スケール後も見た目の位置を維持するため、Anchor移動量を変換してPositionを補正し、専用Undoコマンドで復元できるようにした。2D変換APIの仕様に基づく実装であり、ビルド未検証。
- 実装追加: 3D投影フレームの内部領域をドラッグして、レイヤー面のScreen移動へ接続した。コーナー／エッジ／回転ハンドルを先に判定し、残った投影四辺形内部だけを移動対象にすることで、ハンドル操作との競合を避けている。投影頂点がnear/far範囲外の場合は誤った内部判定を避ける。ビルド未検証。
- 改善: 3D投影フレーム内部の移動中にShiftを押すと、開始時のレイヤー面ローカルX/Y軸のうち移動量が大きい軸へ拘束するようにした。投影後の画面方向ではなくワールド変換後のレイヤー面軸へ射影するため、カメラを斜めにした状態でも軸ロックの意味を維持する。ビルド未検証。
- 改善: 3D投影フレームのコーナーリサイズで、Shift時は初期アスペクト比を維持し、Shiftなしでも反対側コーナーを固定する位置補正を追加した。補正はワールド変換後のレイヤー面X/Y軸とlocalBounds寸法から算出し、Ctrl中心固定時は適用しない。ビルド未検証。
- 実装追加: 3D投影フレームの移動／リサイズ中に、現在の実効幅・高さ、位置X/Y、Z回転、操作種別を表示するHUDを追加した。HUDは既存のCanvas Overlay描画経路に限定し、3D描画行列やQt CSSを変更しない。ビルド未検証。
- 改善: 3Dフレーム操作HUDに位置ZとScale X/Yを追加し、平面移動と軸ギズモ操作の結果を同じ表示で確認できるようにした。表示幅・高さは3D値を含むため拡張した。ビルド未検証。
- 安定化: 3D投影フレーム描画前に、投影四隅をnear/farクリップ範囲へ照合する判定を追加した。全頂点がカメラ背面またはクリップ外の場合は枠・ハンドルを描画せず、部分的に範囲内の場合は既存のGPUクリッピングへ委ねる。`QVector4D`を直接includeし、ビルド未検証。
- 改善: 3D投影フレームの四隅の一部だけがnear/far範囲外にある場合、枠線・ハンドル・回転leaderを半透明化するようにした。全点範囲外時の非表示判定は維持し、部分クリップ時の存在確認を可能にする。ビルド未検証。
- 実装追加: Puppetツールのオーバーレイに、OpenCVPuppetEngineが保持する変形後メッシュの三角形ワイヤーフレームを追加した。インデックス範囲を検証してから辺を描画し、既存のピン色分け・選択表示を変更しない。ビルド未検証。
- 実装追加: Puppetで選択中のピンに回転leaderと回転ハンドルを表示し、Alt+ドラッグでピン回転を編集できる経路を追加した。回転値はPinRecordへ正規化して保存し、既存のdeformLayer／Undo外部経路を壊さない範囲で操作する。ビルド未検証。
- 改善: Puppetツールで選択中のピンをダブルクリックすると回転値を0度へ戻す操作を追加した。Puppetツールがアクティブな場合だけ実行し、通常のレイヤー編集上のダブルクリックには介入しない。ビルド未検証。
- 改善: PuppetのAlt回転ドラッグ中にShiftを押すと15度刻みへスナップするようにした。Shiftなしの自由回転と既存の-180〜180度正規化は維持する。ビルド未検証。
- 拡張: Puppet新規ピン追加時の修飾キーをPosition（通常）、Starch（Ctrl）、Bend（Shift）、Overlap（Alt）へ割り当てた。複数修飾キー時はCtrlのStarchを優先し、既存ピンのAlt回転操作とは追加操作の分岐で分離する。ビルド未検証。
- 拡張: PuppetのStarchピン上で修飾キーなしのホイールを回すと、剛性weightを5%刻みで0〜100%調整できるようにした。表示ラベルにも現在のStarch値を併記し、Shift/Alt/Ctrlの既存ナビゲーション操作とは競合させない。ビルド未検証。
- 修正: PuppetのdeformLayerでOpenCVPuppetEngineへ渡す`PuppetPin.weight`が固定1.0だったため、PinRecordの調整済みweightを0〜1へクランプして渡すようにした。これでStarchホイール編集が実際の変形重みに反映される。ビルド未検証。
- 拡張: PuppetのOverlapピン上でも通常ホイールでdepthを±0.05調整できるようにし、範囲を-1〜1へ制限した。表示ラベルに現在の深度を併記し、既存のStarch weight編集と同じポインタ判定・ナビゲーション除外ルールを再利用する。ビルド未検証。
- 修正: PuppetのBend回転単位をArtifact側のEngine境界で度からラジアンへ変換した。UI／PinRecord／オーバーレイは度数表示を維持し、OpenCVPuppetEngineの`std::sin/cos`呼び出しへだけラジアンを渡す。ArtifactCore側は変更していない。ビルド未検証。
- 改善: MotionSketch中に`[`/`]`でSmoothingを0.1刻み調整できるようにし、既存のSketch HUDへ現在のSmoothingとSample Rateを追加表示した。既存のPenマスクOpacityショートカットはツール分岐で維持する。ビルド未検証。
- 拡張: MotionSketch中のShift+`[`/`]`をSample Rateの5fps刻み調整へ割り当てた。通常の`[`/`]`によるSmoothing調整との役割を分離し、HUD表示値を即時確認できるようにした。ビルド未検証。
- 拡張: MotionSketch中に`W`でShow Wireframeを切り替えられるようにし、Sketch HUDへWF:ON/OFFを追加表示した。設定は既存MotionSketchToolのshowWireframe状態を利用する。ビルド未検証。
- 実装追加: MotionSketchのShow Backgroundを追加し、スケッチ中の`B`で背景表示を切り替えられるようにした。OFF時は既存のCanvas Overlay上へ暗い半透明面を描画して軌跡／ワイヤーフレームを強調し、HUDへBG:ON/OFFを表示する。ビルド未検証。
- 改善: Puppetの通常ピンドラッグ中にShiftを押すと、開始位置から移動量の大きいXまたはY軸へ移動を拘束するようにした。Alt回転ドラッグは別経路を維持し、回転中に位置拘束を適用しない。ビルド未検証。
- 改善: PuppetのBendピンラベルに現在の回転角を度数表示するようにした。UI操作・正規化後のPinRecord値をそのまま表示し、Engine境界でのラジアン変換前後を混同しない表示責務にした。ビルド未検証。
- 修正: Puppetピンのダブルクリックリセットを種別別に整理した。Bendは回転0度、Starchはweight 1.0、Overlapはdepth 0.0へ戻し、Positionピンではイベントを消費せず通常のダブルクリック処理へ通す。ビルド未検証。
- 実装追加: Rectangle／Shape作成中の通常ホイールで角丸半径を2px刻み調整し、作成確定時にArtifactShapeLayerのcornerRadiusへ反映するようにした。半径は短辺の半分でクランプし、作成中HUDへ`R`値を表示する。既存のShift正方形・Alt中心作成・修飾キー付きナビゲーションは維持する。ビルド未検証。
- 改善: Rectangle／Shape作成中の角丸半径をVPプレビューにも反映した。Shape/EllipseShapeでは既存のGPUネイティブ`drawRoundedPanel`を使い、Maskモードや半径0では従来の矩形描画へフォールバックする。ビルド未検証。
- 拡張: Rectangle／Shapeツールで作成済みの選択Shape上でも通常ホイールからcornerRadiusを調整できるようにした。作成中のセッション調整を優先し、作成後は選択ShapeのProperty dirty・変更通知・再描画を通す。ビルド未検証。
- 改善: Rectangle／Shapeツールで選択ShapeをダブルクリックするとcornerRadiusを0へ戻すリセットを追加した。作成中や半径0のShapeでは処理せず、他ツールのダブルクリックへ影響させない。ビルド未検証。
- 改善: 作成済みShapeのcornerRadiusホイール調整とダブルクリックリセットを`ShapeCornerRadiusUndoCommand`へ接続した。前後値を保存し、Undo/Redo時もProperty dirty・LayerChangedEvent・UndoManager通知を再実行する。ビルド未検証。
- 改善: Brush／Eraserツール中の`[`/`]`でブラシ径を2px刻み調整できるようにした。PenのマスクOpacity、MotionSketchのSmoothing／Sample Rateとはツール分岐で競合しない。ビルド未検証。
- 拡張: Brush／Eraserのブラケット操作にShift=Flow±5%、Ctrl=Opacity±5%を追加した。修飾キーなしの径±2pxは維持し、各値はBrushTool既存setterの範囲クランプを利用する。ビルド未検証。
- 改善: Brush／Eraserカーソル横へDiameter、Flow、Opacityの小型HUDを追加した。筆圧によるカーソル径・透明度プレビューとは独立した設定値表示で、描画中のストロークプレビュー処理を変更しない。ビルド未検証。
- 改善: Brush／EraserカーソルHUDへHardness、Roundness、Angleも追加表示した。既存BrushToolの値を読み取るだけに留め、描画パラメータや新規イベント配線は変更していない。ビルド未検証。
- 拡張: Brush／Eraserのブラケット操作にAlt=Hardness±5%、Ctrl+Alt=Roundness±5%、Shift+Alt=Angle±15°を追加した。既存の径・Flow・Opacity操作を維持し、BrushToolのsetterによる範囲正規化を利用する。ビルド未検証。
- 拡張: Brush／EraserのCtrl+Shift+Alt+ブラケットでSpacingを5%刻み調整できるようにし、カーソルHUDにもSpacingを表示した。ブラシ点の間隔だけを変更し、既存ストロークの再計算は行わない。ビルド未検証。
- 改善: Eraser時のカーソルHUDへPaint／Layer／Lastの現在モードを追加表示した。モード切替の既存設定を読み取るだけで、クリック時の消去処理やUndo経路は変更していない。ビルド未検証。
- 拡張: Brushの筆圧連動をFlowにも適用した。開始・逐次適用・終了の各BrushStrokeで`flow × pressure`を使い、既存のSize／Opacityと同じクランプ前提の経路へ揃えた。連動は`pressureAffectsFlow`で無効化可能で、既定は有効。ビルド未検証。
- UI追加: Brush Optionsへ`Pressure Flow`チェックボックスを追加し、筆圧によるFlow変調をユーザーが切り替えられるようにした。既存のoptionChanged経路からBrushToolへ反映し、新規シグナルは追加していない。ビルド未検証。
- UI追加: Brush Optionsへ`Pressure Size`／`Pressure Opacity`切替を追加した。既存BrushToolの筆圧フラグへ接続し、Flowを含む3つの筆圧連動を個別に無効化できるようにした。ビルド未検証。
- UI追加: Brush Optionsへ`Tilt Angle`／`Tilt Roundness`切替を追加した。BrushStroke生成時のペン傾きによる先端変形を個別に無効化でき、Angle／Roundnessの手動設定は維持される。ビルド未検証。
- 表示改善: Brush／Eraserカーソル輪郭にも現在のTilt Angle／Roundnessを反映した。描画時のBrushStroke生成と同じ傾き補正をプレビューへ適用し、設定値だけでなく実効先端形状を確認できるようにした。ビルド未検証。
- 表示改善: Brush／Eraserのカーソル輪郭と描画中ストロークプレビューの透明度へ実効Flowを反映した。Pressure Flow有効時はFlow×Pressure、無効時は基準Flowを使い、プレビューだけが実描画より濃く見える不一致を抑えた。ビルド未検証。
- 表示改善: Brush／Eraser HUDを2行化し、基準設定に加えてPressure、実効Flow、Tilt X/Yを表示した。筆圧・傾きによる実効値を描画前に確認できるが、HUDは読み取り専用のまま維持する。ビルド未検証。
- 拡張: BrushStrokeへSize Jitter／Opacity Jitterを追加し、PaintLayerの各ダブへ座標と点番号から決定的な変動を適用した。Undo／逐次適用で見た目が変わらないよう乱数エンジンは保持せず、Brush Optionsから0〜100%を設定できる。ビルド未検証。
- 拡張: BrushStrokeへScatterを追加し、各ダブを半径内の決定的な方向・距離へ散らせるようにした。Brush Optionsから0〜100%を設定でき、Size／Opacity Jitterと同じく再適用時の再現性を保つ。ビルド未検証。
- 表示改善: Brush／Eraser HUDの動的値行へSize Jitter／Opacity Jitter／Scatterを追加表示した。Pressure・実効Flow・Tiltの表示は維持し、値の編集経路はBrush Optionsに限定する。ビルド未検証。
- 拡張: BrushStrokeへAngle Jitter／Roundness Jitterを追加し、各ダブへ決定的な角度±180度・真円度変動を適用した。Brush Optionsから個別設定でき、既存のTilt補正後の値を基準にする。ビルド未検証。
- 表示改善: Brush／Eraser HUDのダイナミクス行へAngle／Roundness Jitterも追加し、Size／Opacity／Angle／Roundness JitterとScatterを一目で確認できるようにした。ビルド未検証。
- 拡張: BrushStrokeへFlow Jitterを追加し、流量係数へ独立した決定的変動を適用した。Brush OptionsとHUDから設定値を確認でき、Opacity Jitterとは別に扱う。ビルド未検証。
- 表示改善: Brush／EraserカーソルへScatter最大範囲の薄いリングを追加した。先端輪郭と散布範囲を分離して表示し、描画中のストロークプレビューやPaintLayerの計算は変更していない。ビルド未検証。
- 拡張: Brush Optionsで変更した共有BrushTool設定をQSettingsへ保存し、MainWindow起動時に復元するようにした。CoreのAppSettings契約は変更せず、既存のoptionChanged経路を保存フックとして再利用する。ビルド未検証。
- 改善: MainWindow起動時に復元したBrushToolの全設定をBrush OptionsへSignalBlocker付きで同期する`syncBrushOptionsFromTool()`を追加した。永続化値・描画値・UI表示値の不一致を防ぎ、同期時のoptionChanged再発火も抑制する。ビルド未検証。
- 改善: Brush ColorのQSettings復元とカラーボタンのパレット同期を追加した。保存されたRGBA文字列を範囲クランプしてBrushToolへ戻し、明度に応じたボタン文字色も再計算する。ビルド未検証。
- 改善: QSettings復元へEraserのmode（Paint／Layer／Last）とLast Stroke Onlyも追加した。optionChangedの共有保存キーを優先し、旧形式の`eraser/*`キーもフォールバックとして読めるようにした。ビルド未検証。
- 改善: `syncBrushOptionsFromTool()`でEraser Optionsのサイズ、強さ、Hardness、Angle、Roundness、Last Stroke Only、モード選択も同期するようにした。起動直後のBrush／Eraser UIが同じ共有BrushTool状態を表示する。ビルド未検証。
- 改善: Clone Stampのradius／Aligned／Time OffsetもQSettingsから復元するようにした。radiusはBrush Size保存値を優先して共有状態の上書きを避け、既存のBrushTool契約だけを利用する。ビルド未検証。
- 改善: BrushTool復元同期へClone OptionsのRadius／Aligned／Time Offset表示も追加した。`QSignalBlocker`で同期時のoptionChanged再発火を抑え、Clone UIと共有ツール状態を一致させる。ビルド未検証。
- UI追加: Eraser OptionsへHardness設定を追加し、BrushToolの共通hardnessへ反映するようにした。Eraser Strength（opacity）とは別のパラメータとして扱い、既存の消去モード・Undo処理は変更していない。ビルド未検証。
- UI追加: Eraser Optionsへ共通ブラシ先端のAngle／Roundnessを追加した。消しゴム専用の形状状態を増やさず、BrushToolの既存setterへ接続してBrushと同じ先端形状を共有する。ビルド未検証。
- 拡張: TrackPointツールのブラケット操作でFeature Size（通常）とSearch Size（Shift）を2px刻み調整できるようにした。Inner領域がOuter領域を越えないよう上下限を保ち、既存Tracker Gizmoの状態・描画を更新する。ビルド未検証。
- 表示改善: TrackPoint GizmoにFeature／Searchの現在幅・高さを表示するHUDを追加した。サイズ変更の結果をVP上で即時確認でき、NCC／MotionTrackerの計算経路は変更していない。ビルド未検証。
- 表示改善: 3D投影フレームへ薄い対角線リファレンスマークを追加した。投影後のコーナー位置とアスペクト変化を読み取りやすくする補助表示で、フレームのヒットテストやリサイズ計算は変更していない。ビルド未検証。
- 改善: MotionSketchのSmoothing／Sample Rate／WireframeをQSettingsへ保存・復元し、起動時にOptions UIへ同期する処理を追加した。設定値は既存のMotionSketchTool APIだけを通し、ビルド未検証。
- 改善: MotionSketchのBキーによる背景表示切替も`motionSketch/showBackground`へ保存し、起動時に復元するようにした。UI項目がない表示補助状態も既存のツール状態と同じ永続化境界に揃えた。ビルド未検証。
- 改善: MotionSketchのキーボード操作（W、[／]）で変更したWireframe／Smoothing／Sample Rateも即時保存するようにした。Options UI経由と直接操作経由で設定の永続化が分岐しない。ビルド未検証。
- 改善: TrackPointのFeature/Search領域サイズをQSettingsへ保存し、起動時にTracker Gizmoへ復元するようにした。ブラケット操作だけでなく領域ハンドルのドラッグも同じ設定へ反映する。ビルド未検証。
- UI改善: Viewメニューの「定規を表示」をCompositionRenderControllerへ接続し、表示状態をQSettingsへ保存・復元するようにした。既存のViewport Ruler／Scale Overlay描画経路は変更していない。ビルド未検証。
- UI追加: Render Queue Job DetailsへSelective Render設定（Adjustment Layer除外／Split Passes）を追加し、既存の`jobSelectiveSettingsAt`／`setJobSelectiveSettingsAt` APIへ接続した。選択ジョブ変更時のUI同期も実装した。ビルド未検証。
- 表示改善: Render Queueのプレビュー要約へSelective RenderのAdjustment除外／Split Passes状態を追加した。ジョブ詳細を開かなくても出力範囲の追加条件を判別できる。ビルド未検証。
- UI追加: Selective Renderへ`Exclude Guide Layers`も追加し、RenderQueueServiceが既に保持するガイドレイヤー除外設定を編集・復元できるようにした。ビルド未検証。
- UI追加: Selective Renderへ解像度プリセット（Custom／Composition／Half／Third／Quarter）を追加し、ジョブ単位の`resolutionPreset`を既存サービスAPIへ保存・復元するようにした。ビルド未検証。
- UI追加: Selective RenderへFrame Range（Composition／Work Area／Custom／Selected Frames／Single Frame）とRegion（Full／ROI／Custom Crop）のモード選択を追加し、ジョブ単位で保存・復元するようにした。ビルド未検証。
- UI追加: Render QueueのROI／Custom Crop用にCrop X/Y/Width/Height編集欄を追加し、`cropX/cropY/cropW/cropH`をジョブ単位で保存・復元するようにした。ビルド未検証。
- UI追加: Selective Renderへレイヤーフィルタ（All／Selected／Solo／Visible／Custom Layers）の選択を追加し、`layerFilterMode`をジョブ単位で保存・復元するようにした。ビルド未検証。
- 改善: Render QueueでSplit Passesを初めて有効化した際、renderPassesが空ならBeautyパスを自動生成するようにした。既存の明示パスは保持し、空定義によるpreflightエラーを避ける。ビルド未検証。
- 表示改善: Split Passes有効時、プレビュー要約へ有効なRenderPassConfig名を`Passes:`として表示するようにした。Beauty自動生成後の実際の出力分割内容をジョブ詳細外から確認できる。ビルド未検証。
- UI改善: RegionがFullのときCrop X/Y/Width/Heightを無効化し、ROIまたはCustom Crop選択時だけ編集可能にした。入力値自体は保持し、モード切替で破棄しない。ビルド未検証。
- UI追加: Split Passesの`Configure Passes…`を追加し、カンマ区切りの名前入力から有効なRenderPassConfig一覧を保存できるようにした。重複名は除外し、空入力では既存設定を保持する。ビルド未検証。
- UI追加: Selective RenderのCustom Layersへ現在のレイヤー選択を取り込む`Use Current Selection`ボタンを追加した。選択レイヤーIDを`layerWhitelist`へ保存し、フィルタモードをCustomへ切り替える。空選択では既存設定を保持する。ビルド未検証。
- UI追加: 現在のレイヤー選択をSelective Renderの`layerBlacklist`へ追加する`Exclude Current Selection`を追加した。既存のWhitelistやレイヤーフィルタモードと併用できる。ビルド未検証。
- 表示改善: Render Queueの要約へlayerWhitelist／layerBlacklistの件数を表示し、選択・除外レイヤーの適用状態を確認できるようにした。IDそのものは表示せず、要約の横幅を抑えている。ビルド未検証。
- UI追加: Selective Renderへ`Clear Included`／`Clear Excluded`を追加し、layerWhitelist／layerBlacklistを個別に解除できるようにした。フィルタモードは維持し、対象リストだけを空にする。ビルド未検証。
- UI改善: Whitelist／Blacklistが空のジョブでは対応するクリアボタンを無効化し、ジョブ切替時の設定件数と操作可能状態を一致させた。ビルド未検証。
- 改善: Current SelectionのWhitelist／Blacklist取り込み時に反対側のリストから同じLayer IDを除去するようにした。相互排他的な指定を保ち、同一レイヤーが同時にinclude／excludeされる矛盾を防ぐ。ビルド未検証。
- UI追加: ViewメニューのGrid Settingsへズーム連動ステップ／極座標グリッド／アイソメトリックグリッドの切替を追加した。既存CompositionRenderController APIへ接続し、状態をQSettingsへ保存・復元する。ビルド未検証。
- UI改善: 極座標／アイソメトリックの相互排他をメニュー表示にも反映し、一方を有効化した際にもう一方のチェックを解除するようにした。Controller側の排他契約とUI状態を一致させる。ビルド未検証。
- UI追加: Grid Settingsへ「グリッド数値を表示」を追加し、既存GridSettings::showNumbersとメジャー線ラベル描画をViewメニューから切り替えられるようにした。ビルド未検証。
- UI追加: Grid Settingsへズーム連動ステップの目標ビューポート間隔編集を追加した。24〜512pxに制限し、Controllerへ適用した値をQSettingsへ保存・復元する。ビルド未検証。
- 修正: Viewメニューの「グリッドを表示」「グリッドにスナップ」はActionだけ存在してController接続がなかったため、実際の表示／snapToGrid状態へ接続した。両方をQSettingsへ保存・復元し、メニューのチェック状態も同期する。ビルド未検証。
- 修正: Viewメニューの「ガイドを表示」もControllerへ接続し、showGuides状態をQSettingsへ保存・復元するようにした。既存のガイド生成・描画処理は変更していない。ビルド未検証。
- 実装: 「ガイドにスナップ」をControllerへ追加し、Pen頂点ドラッグ時の既存ガイドスナップ経路へ接続した。Ctrl押下に加えてメニュー状態が有効条件となり、状態はQSettingsへ保存・復元する。ビルド未検証。
- 表示改善: 極座標グリッドで`showNumbers`が有効な場合、同心円の半径ラベル（r値）を表示するようにした。ズームが低すぎる場合はラベルを抑制し、既存の線描画・スナップ計算は変更していない。ビルド未検証。
- 実装: Pen頂点ドラッグ時に未使用だった`snapCanvasToGrid()`を接続し、Grid SettingsのsnapToGridを実際の頂点位置へ適用するようにした。ガイドスナップとは別に適用され、両方の設定を既存のスナップ経路で統合する。ビルド未検証。
- 表示改善: アイソメトリックグリッドでも`showNumbers`有効時に現在の格子間隔を表示する補助ラベルを追加した。極座標の半径ラベルと同じズーム抑制条件を使い、線生成は変更していない。ビルド未検証。
## 2026-08-02 — Brush/Eraser の対象レイヤー自動補完

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Brush/Eraser マウス開始処理。
- 確認できた事実: Paint レイヤー未選択時の自動作成は既に存在したが、画像・シェイプ等の非 Paint レイヤーを選択した状態ではブラシ処理へ進めなかった。
- 実装: 選択レイヤーが存在しても `ArtifactPaintLayer` でない場合は、新しい Paint レイヤーを作成して選択対象に切り替える。作成時には短い情報オーバーレイを表示する。
- 価値: ツール選択後の最初のクリックが無反応になる状態をなくし、Brush/Eraser の「対象 Paint レイヤーがない場合の自動作成」という仕様を実際の編集導線に揃えられる。
- 次に確認すべきこと: 実ランタイムで既存レイヤー選択中の Brush/Eraser 開始、Undo、レイヤー順序を確認する（未検証）。
## 2026-08-02 — Text ツールのダブルクリック編集入口

- 関連: `ArtifactCompositionRenderWidget` / `CompositionRenderController` / `ArtifactTextLayer`。
- 実装: ビューポートのダブルクリック時、選択中レイヤーが Text なら既存の Qt 入力ダイアログで本文を編集し、変更後にレイヤー更新・GPU/オーバーレイ無効化・再描画を行う。
- 方針: 新規シグナル／スロット配線や別のテキスト編集モデルは導入せず、既存の選択状態と `setText()` を利用した最小の編集導線とした。
- 未検証: 実ランタイムのダブルクリック判定、Undo 履歴、マルチライン入力の表示品質。
## 2026-08-02 — Text 内容変更の Undo 対応

- 関連: `ArtifactCompositionRenderController.cppm` の `TextContentUndoCommand`。
- 実装: ダブルクリック編集で本文を変更した場合、変更前後の文字列を既存 `UndoManager` に登録するようにした。Undo/Redo は `ArtifactTextLayer::setText()`、`changed()`、LayerChangedEvent 通知を通る。
- 価値: Text ツールの直接編集が、単なる即時反映ではなく既存編集履歴へ統合された。
- 未検証: 実ランタイムでの Undo/Redo と再描画、空文字列入力、レイヤー削除後の履歴実行。
## 2026-08-02 — Text ダブルクリック対象のレイヤーヒット解決

- 関連: `ArtifactCompositionRenderController::editTextAtViewport()`。
- 実装: 選択中レイヤーが Text でない場合でも、ダブルクリック位置を viewport→canvas→layer local へ変換し、可視・アクティブな Text レイヤーを上位から検索して編集対象にする。
- 価値: 先にレイヤーを選択していなくても、キャンバス上のテキストを直接編集できる導線になった。
- 未検証: 変形・回転した Text、重なった Text、ロックレイヤーのヒット順。
## 2026-08-02 — Text 編集ヒットテストの安全化

- 関連: `CompositionRenderController::editTextAtViewport()`。
- 実装: 選択中 Text でもダブルクリック位置がレイヤー境界外なら編集を開始しないようにし、ロック／選択ロックされた Text は位置検索から除外した。
- 価値: 意図しない編集ダイアログ表示とロックレイヤーの編集を防ぐ。
- 未検証: 複雑な非矩形テキスト形状、親子レイヤー変換、実ランタイムのロック状態。
## 2026-08-02 — 3D Frame Gizmo のダブルクリックリセット

- 関連: `CompositionRenderController::resetSelected3DTransform()`。
- 実装: 選択中の 3D レイヤーをビューポートでダブルクリックすると、位置 X/Y・回転・XY スケールを初期値へ戻す。Z 位置は保持する。
- Undo: 既存 `GizmoTransformUndoCommand` を利用し、変更前後の TransformSnapshot を履歴へ登録する。
- 安全性: ロック／選択ロック中のレイヤーは対象外。
- 未検証: 実ランタイムでの 3D カメラ投影、キーフレーム時の値、Undo/Redo 表示。
## 2026-08-02 — 3D 投影フレームの最小サイズクランプ

- 関連: `CompositionRenderController::handleMouseMove()` の projected frame resize。
- 実装: コーナー／エッジリサイズ後の XY スケールを、ローカル境界の 1 px 未満にならないようにクランプする。Shift の均等スケールや Ctrl の中心固定計算後に適用する。
- 価値: 極端なドラッグでフレームがゼロ化し、ハンドルや選択枠が消える状態を防ぐ。
- 未検証: 負スケール反転を意図的に使うケース、非均一な 3D 親変換、実ランタイムの境界表示。
## 2026-08-02 — Text ダブルクリック時の選択同期

- 関連: `CompositionRenderController::editTextAtViewport()`。
- 実装: 選択状態とは別の Text レイヤーを位置ヒットで見つけた場合、編集ダイアログを開く前に `setSelectedLayerId()` で選択・プレビュー・ギズモ状態を同期する。
- 価値: 編集後もプロパティ／ギズモ／レイヤー選択がクリック対象と一致する。
- 未検証: 複数選択状態と親子レイヤーの選択表示。
## 2026-08-02 — Puppet ピンドラッグの Undo 統合

- 関連: `CompositionRenderController` / `ArtifactPuppetTool`。
- 実装: ピンドラッグ開始時に位置・回転を保存し、マウスリリース時に変更があれば `PuppetPinUndoCommand` を UndoManager へ登録する。Undo/Redo ではピン状態を復元し、メッシュ変形も再適用する。
- 価値: Position/Bend/Overlap 等のピン操作を、既存の編集履歴と一貫して取り消せる。
- 未検証: PuppetTool のライフタイム、ピン削除後の履歴、実ランタイムでの画像再変形。
## 2026-08-02 — Puppet Starch/Overlap パラメータの Undo 対応

- 関連: `adjustSelectedPuppetPinWeightAt()` / `adjustSelectedPuppetPinDepthAt()`。
- 実装: ホイール操作で変更する Starch Weight と Overlap Depth の変更前後を `PuppetPinScalarUndoCommand` に登録した。
- 価値: ピン移動・回転に加え、ピン属性の微調整も編集履歴から個別に戻せる。
- 未検証: 連続ホイール操作の履歴粒度、ピン削除後の履歴実行、実ランタイムの再描画。
## 2026-08-02 — TrackPoint 解析範囲と結果 HUD の改善

- 関連: `trackerTrackForward()` / `trackerTrackBackward()` / `trackerTrackAll()`。
- 実装: Forward/Backward の固定 30 フレーム処理を廃止し、現在フレームからコンポジションの `FrameRange` 終端／始端まで解析するようにした。解析後は対象範囲と平均 Confidence を情報 HUD に表示する。
- 価値: 長いコンポジションでも追跡範囲が実際のタイムラインに一致し、結果品質をその場で確認できる。
- 未検証: 非ゼロ開始フレーム、巨大なフレーム範囲、解析中の UI 応答性。
## 2026-08-02 — TrackPoint 解析開始 HUD

- 関連: `trackerTrackForward()` / `trackerTrackBackward()` / `trackerTrackAll()`。
- 実装: 同期的なフレーム走査を開始する直前に解析範囲を情報オーバーレイへ表示し、完了時の Confidence HUD へ自然につなげた。
- 価値: 長い解析範囲でも、操作が受け付けられたことと対象範囲をユーザーが確認できる。
- 未検証: UI スレッド上の同期解析中にオーバーレイが描画されるタイミング。
## 2026-08-02 — Ellipse ツール名称の明確化

- 関連: `Artifact/src/Widgets/ArtifactToolBar.cppm`。
- 実装: `ToolType::Ellipse` のツールバー表示名を曖昧な「シェイプ」から「楕円」へ変更した。
- 価値: Shape と Ellipse の責務がツールバー上でも区別でき、仕様上のツール名と UI 表示を一致させる。
- 未検証: ローカライズ資産やアクセシビリティ読み上げ文言の追加箇所。
## 2026-08-02 — Track All の非ゼロ開始フレーム修正

- 関連: `trackerTrackAll()`。
- 確認・修正: 完了 HUD が参照する `range` と走査範囲が一致しておらず、`duration()` を 0 始点として使っていた。`FrameRange::start()/end()/frameCount()` を使い、実際の開始フレームから終了フレームまで走査するよう修正した。
- 価値: 非ゼロ開始のコンポジションでも Track All の画像フレームと Confidence 表示が一致する。
- 未検証: 非ゼロ開始フレームの実ランタイム解析。
## 2026-08-02 — MotionSketch 終了時の再生状態復元

- 関連: `CompositionRenderController::handleMouseRelease()`。
- 修正: 自動再生を開始した MotionSketch がサンプル不足で終了しても、元々停止中だった場合は Playback を確実に pause し、保存していた `motionSketchWasPlaying_` をクリアする。
- 価値: クリックだけで終了したケースや短いスケッチ後に、意図せず再生が継続する状態を防ぐ。
- 未検証: 実ランタイムの再生中開始／停止中開始の両ケース。
## 2026-08-02 — MotionSketch の早期終了バグ修正

- 関連: `CompositionRenderController::handleMouseMove()` / `handleMouseRelease()`。
- 修正: `handleMouseMove()` に重複していた `finishSketch()` 処理を削除し、MotionSketch の終了を `handleMouseRelease()` のみに統一した。
- 影響: 最初のマウス移動でスケッチが確定してしまう状態を解消し、ドラッグ中の全サンプルを収集できるようにした。自動再生の停止処理もリリース時に維持される。
- 未検証: 実ランタイムでの長いドラッグ、キャンセル、再生中開始。
## 2026-08-02 — MotionSketch 終端サンプル補完

- 関連: MotionSketch の press/move/release 経路。
- 実装: 最後に通過したキャンバス座標を保持し、リリース時に Sample Rate のスロットルを尊重しながら最終サンプルを追加試行する。終了・キャンセル時は座標をクリアする。
- 価値: ドラッグ終端の位置がキーフレームへ反映されず、動きが途中で止まって見えるケースを減らす。
- 未検証: リリース直後の短いドラッグ、低い Sample Rate、キャンセル操作。
## 2026-08-02 — TrackPoint Apply 結果 HUD

- 関連: `trackerApplyToPosition()` / `trackerApplyToAnchor()`。
- 実装: Position Apply 後に選択レイヤー適用か新規 Null 作成かを表示し、Anchor Apply 後には選択レイヤーへの書き込み状態を表示する。
- 価値: 解析結果の適用操作が無反応に見える状態を減らし、次に必要な選択操作を明確にする。
- 未検証: 実ランタイムの Null 作成、既存レイヤーへのキーフレーム反映。
## 2026-08-02 — TrackPoint FrameRange 入力検証

- 関連: `trackerTrackForward()` / `trackerTrackBackward()` / `trackerTrackAll()`。
- 実装: 解析開始前に `FrameRange::isValid()` を確認し、不正な範囲で clamp／フレーム走査へ進まないようにした。start=end の単一フレーム範囲は有効な入力として保持する。
- 価値: 空のプロジェクトや未確定範囲で不正な解析範囲・無限ループを避ける。
- 未検証: 実ランタイムでの空範囲生成条件。
## 2026-08-02 — TrackPoint Null 適用後の選択同期

- 関連: `trackerApplyToPosition()` / `ArtifactPointTrackerTool::applyTrackingResult()`。
- 実装: 新規 Null レイヤーへ追跡結果を書き出した場合、作成された最上位レイヤーを選択状態へ同期する。Apply の戻り値も確認し、キーフレームが無い場合は失敗 HUD を表示する。
- 価値: Apply 後に結果レイヤーを探し直す必要がなく、空結果を成功と誤認しない。
- 未検証: 複数同時適用、レイヤー追加失敗、実ランタイムの選択同期。
## 2026-08-02 — TrackPoint 複数ポイント一括 Apply

- 関連: `ArtifactPointTrackerTool::applyAllTrackingPoints()` / TrackPoint コンテキストメニュー。
- 実装: Core 側に存在していた全ポイント適用 API を Controller へ公開し、各トラッキングポイントを個別 Null レイヤーへ一括書き出しするメニュー項目を追加した。
- 価値: 複数点トラッキング結果を 1 点ずつ適用する手作業をなくし、複数点ワークフローを UI から完結できる。
- 未検証: 複数点結果の実ランタイム適用、レイヤー順序、Undo 粒度。
## 2026-08-02 — TrackPoint 複数 ID の重複除去

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm` の `applyAllTrackingPoints()`。
- 修正: フレームごとの並び順に依存した直前 ID 比較を廃止し、`std::set` で全結果中の有効ポイント ID を一意化した。
- 価値: 複数フレームに同じポイントが現れる通常のトラッキング結果でも、ポイントごとに Null レイヤーを 1 つだけ作成できる。
- 未検証: 実ランタイムでの複数点結果、ポイント ID の順序が変わるトラッカー。
## 2026-08-02 — Ellipse ツールの直接選択導線

- 関連: `Artifact/src/Widgets/ArtifactToolBar.cppm`。
- 実装: メインツールバーに Shape とは独立した「楕円」アクションを追加し、`Shift+Q` で `ToolType::Ellipse` を直接選択できるようにした。既存 Shape アイコンをフォールバックとして利用する。
- 価値: Ellipse の実装済みキャンバス作成経路へ、通常の UI から到達できる。
- 未検証: Qt のショートカット競合、ツールバーのコンパクト表示、実ランタイムの Ellipse 作成。
## 2026-08-02 — MotionSketch 開始フレーム固定

- 関連: `ArtifactMotionSketchTool::beginSketch()` / `finishSketch()`。
- 修正: 自動再生中に終了時の現在フレームを基準にしていた処理を改め、スケッチ開始時のフレームを保存してキーフレーム生成へ使用するようにした。大きなフレーム番号には `int64_t` を使う。
- 価値: 再生しながら描いても、スケッチの最初の位置から正しい時間範囲へ記録される。
- 未検証: 再生中の長時間スケッチ、非ゼロ開始フレーム、Undo/Redo。
## 2026-08-02 — Ellipse 選択時の Tool Options 同期

- 関連: `ArtifactToolOptionsBar::setCurrentTool()`。
- 修正: ツールバーの表示名を「楕円」に分離したことに合わせ、楕円選択時も Shape オプションフレームを表示するよう判定を拡張した。
- 価値: Ellipse アクション追加後も、Shape のサイズ・塗り・ストローク等のオプション編集を失わない。
- 未検証: 実ランタイムでのツール切り替えとオプションバー表示。
## 2026-08-02 — Ellipse 作成 HUD の明確化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の矩形／楕円プレビュー HUD。
- 修正: Ellipse モードではサイズ表示を `Ellipse 幅 x 高さ` に切り替え、矩形／角丸矩形だけに意味のある角丸半径 `R` 表示を出さないようにした。
- 価値: 楕円作成中に、実際に編集できる形状と HUD の情報が一致する。
- 未検証: 実ランタイムでの EllipseMask / EllipseShape 両モードの表示、ローカライズ済みフォント幅。
## 2026-08-02 — Viewport 定規のパン／ズーム追従

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`。
- 修正: 定規の目盛り計算へ renderer の pan と zoom から求めた可視キャンバス原点・範囲を渡し、定規本体とスケールバーは変形後のキャンバス端ではなくビューポート端へ固定した。
- 価値: キャンバスをパンして原点が画面外へ移動しても、表示中の範囲に対応する目盛りとスケールバーを確認できる。
- 未検証: 回転ビュー、DPI スケーリング、実ランタイムでの大きなパン値。
## 2026-08-02 — Viewport 定規の SubMinor 目盛り

- 関連: `Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`、定規 HUD 描画。
- 修正: 5 分割以上のステップでは `Major` / `Minor` / `SubMinor` を生成し、描画長を 10 / 6 / 3 px に分けた。従来は補助目盛りがすべて同じ長さだった。
- 価値: ズームに応じた 1-2-5 系列の細かい間隔を視覚的に読み取りやすくする。
- 未検証: 低ズームでの目盛り密度、回転ビュー、実ランタイムのアンチエイリアス。
## 2026-08-02 — Viewport 定規ラベルを実座標化

- 関連: `Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: メジャー目盛りのラベルを固定ステップ名から各目盛りのキャンバス座標へ変更した（例: `0 px`, `80 px`, `160 px`）。小数ステップでは精度に応じて小数桁も調整する。
- 価値: パン・ズーム中でも、目盛り位置の実際の座標を読み取れる。
- 未検証: 単位名が空文字列の場合、長いラベルの重なり、実ランタイム表示。
## 2026-08-02 — Viewport 定規の目盛りキャッシュ

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 修正: pan、zoom、ビューポートサイズ、キャンバスサイズをキャッシュキーとして保持し、いずれかが変化した場合だけ水平／垂直目盛りを再生成するようにした。
- 価値: 内容が静止した再描画で目盛り計算とベクター生成を繰り返さず、オーバーレイ描画の負荷を抑えられる。
- 未検証: 高頻度 pan／zoom 操作時のキャッシュ更新、キャンバスの動的リサイズ、実ランタイム性能。
## 2026-08-02 — Text 作成直後の編集開始

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Text ツール release 処理。
- 修正: Text レイヤーの作成成功を確認した後、既存の `editTextAtViewport()` を呼び出して即時入力へ遷移するようにした。編集内容は既存の `TextContentUndoCommand` 経路を再利用する。
- 価値: クリック作成・ドラッグ作成のどちらでも、作成後に別操作を挟まずテキストを入力できる。
- 未検証: モーダル編集ダイアログのキャンセル時の UX、IME 入力、段落テキストの長文入力。
## 2026-08-02 — Paint ストロークの Undo ショートカット接続

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`。
- 修正: Brush／Eraser がアクティブなとき、`Ctrl+Z` 相当の Undo が選択中 `ArtifactPaintLayer` の `undoLastStroke()` を優先して実行する公開経路を追加した。描画キャッシュ・変更通知・情報 HUD も更新する。
- 価値: Paint レイヤーの内部ストローク履歴を通常の編集操作から戻せる。
- 未検証: グローバル Undo 履歴との混在、逐次適用された長いストローク、Redo との組み合わせ。
## 2026-08-02 — Paint Undo のキー処理順序を固定

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` の `keyPressEvent()`。
- 修正: `InputOperator` がキーを先に消費する前に、Brush／Eraser の Undo を判定するようにした。Paint ストロークを戻せた場合はイベントをそこで完了させる。
- 価値: 入力コンテキスト設定に依存せず、Viewport 上の Paint Undo が確実に専用履歴へ到達する。
- 未検証: ショートカット設定を変更した環境、IME 中の Ctrl+Z、Redo との組み合わせ。
## 2026-08-02 — Pen のマスクキーボード操作を接続

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、マスク操作 Controller API。
- 修正: Pen アクティブ時に `Esc` で作成中マスクをキャンセル、`Delete / Backspace` で選択頂点またはホバー頂点を削除、`Ctrl+A` で編集可能な頂点を全選択できるようにした。入力コンテキストより前に処理する。
- 価値: 仕様にあるマスク編集の基本ショートカットが、既存の Undo 対応 Controller 操作へ到達する。
- 未検証: IME／アクセシビリティキー設定、複数パス削除後の選択状態、実ランタイムのキーフォーカス。
## 2026-08-02 — Pen のマスク複製ショートカット

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`CompositionRenderController::duplicateHoveredMask()`。
- 修正: Pen アクティブ時の `Ctrl+D` を、ホバー中マスクの既存複製・Undo 経路へ接続した。
- 価値: マスク仕様の複製操作を、コンテキスト UI に依存せず Viewport から実行できる。
- 未検証: ホバーなしでの選択マスク複製、複数マスクの連続複製、実ランタイムのショートカット競合。
## 2026-08-02 — マスクのコピー／並び順操作を接続

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、マスク Controller の clipboard／reorder API。
- 修正: Pen アクティブ時の `Ctrl+C / Ctrl+V` をマスクのコピー／ペーストへ、上下矢印をホバー中マスクの合成順移動へ接続した。
- 価値: マスクを同一レイヤーまたは別レイヤーへ再利用し、合成順をキーボードから調整できる。
- 未検証: OS クリップボードとの共存、ホバーなしの選択マスク操作、上下矢印とレイヤーナッジの競合。
## 2026-08-02 — Composition Editor 側にもマスクキー操作を接続

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- 修正: Viewport Widget だけでなく Composition Editor のキーイベント経路にも、マスクの `Ctrl+C / Ctrl+V` と上下矢印の並び順変更を追加した。フォーカスが親 Editor 側にある場合も同じ Controller API を使う。
- 価値: フォーカス位置によるショートカット取りこぼしを減らし、マスク編集操作を一貫させる。
- 未検証: 親子 Widget のイベント伝播時に同一操作が二重適用されないこと、実ランタイムのフォーカス遷移。
## 2026-08-02 — 3D フレームハンドルのカメラクリッピング

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` の `drawSelectionFrameOverlay()`。
- 修正: 3D カメラ行列が有効な場合、コーナー・エッジ・回転ハンドルごとに clip-space の Z と W を検査し、背面／near-far clip 外のハンドルを描画しないようにした。
- 価値: レイヤーがカメラ背面へ回り込んだとき、操作対象ではないハンドルが画面に残る誤認を防ぐ。
- 未検証: クリップ面をまたぐフレーム、極端な透視投影、実ランタイムのハンドル可視性。
## 2026-08-02 — 3D Text フレーム HUD のボックス寸法表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の projected frame HUD。
- 修正: 3D Text レイヤーをリサイズ中、既存の幅・高さ・位置・スケール情報に `maxWidth` と `boxHeight` に基づく `TextBox W x H` を追加表示する。
- 価値: 見た目の投影サイズと、段落テキストが実際に折り返すボックス寸法を区別して確認できる。
- 未検証: Point Text、アニメーション中のボックス値、HUD の高さが増えた場合の表示領域。
## 2026-08-02 — 3D フレーム HUD の行数追従

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の projected frame HUD。
- 修正: 表示文字列の改行数から HUD 高さを算出し、TextBox 情報を追加した場合も固定高さで切れないようにした。幅も長い数値列に合わせて拡張した。
- 価値: Text レイヤーのリサイズ中に、追加されたボックス寸法情報まで常に読める。
- 未検証: 極端に長い数値、DPI スケーリング、画面下端付近での HUD 配置。
## 2026-08-02 — 3D フレーム HUD の投影中心追従

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の projected frame HUD。
- 修正: カメラ行列が有効な場合、レイヤーのローカル中心を投影し、その近傍へ HUD を配置するようにした。画面端では viewport 内へクランプする。
- 価値: リサイズ対象と数値情報の距離が近くなり、複数の 3D レイヤー操作でも視線移動を減らせる。
- 未検証: 透視投影で中心が画面外にある場合、DPI スケーリング、実ランタイムの HUD 重なり。
## 2026-08-02 — HUD 行数計算の MSVC 型整合

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 修正: `QString::count()` の `qsizetype` を HUD 行数計算前に `int` へ明示変換し、`std::max` の型推論に依存しないようにした。
- 価値: MSVC／C++20 modules 環境でのテンプレート推論エラーを避け、TextBox 追加後の動的 HUD 高さ計算を成立させる。
- 未検証: 極端な改行数による int 範囲超過（通常の HUD 文字列では発生しない）。
## 2026-08-02 — Render Queue SingleFrame の終端フレーム保護

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` のフレーム範囲解決。
- 修正: `SingleFrame` がコンポジション範囲の最終フレームを指す場合に、`std::clamp` の下限と上限が逆転しないよう、最小終端を `startF + 1` として正規化した。
- 価値: 終端フレームのスナップショットが無効範囲や不正なクランプ計算にならず、1 フレームのジョブとして維持される。
- 未検証: 空の FrameRange、負のフレーム番号、外部レンダラーの終端フレーム契約。
## 2026-08-02 — Render Queue SelectedFrames の初期範囲

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の選択的レンダー設定。
- 修正: `Selected Frames` モードへ切り替えた際、保存済みの選択区間が空なら現在ジョブの有効な start/end を 1 区間の初期値として保存する。
- 価値: UI でモードを選んだだけのジョブが、空の範囲による preflight エラーで停止しない。
- 未検証: 非連続なタイムライン選択を提供する将来 UI との統合、ワークエリア由来ジョブの初期範囲、実ランタイム。
## 2026-08-02 — SelectedFrames の負フレーム対応

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` の `expandSelectedFrameRanges()`。
- 修正: 選択区間の検証から `start < 0` の排除を外し、`end > start` の半開区間だけを検証するようにした。実際のコンポジション範囲へのクランプは後段へ委ねる。
- 価値: 開始フレームが負値のタイムラインでも、SelectedFrames の区間を正しくキュー展開できる。
- 未検証: 負フレームを含むコンポジションの外部エンコーダー出力名、複数区間の重複統合。
## 2026-08-02 — SelectedFrames の区間数表示

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` のジョブ概要。
- 修正: FrameRangeMode が SelectedFrames の場合、保存されている非連続区間の数を `Selected ranges: N` として概要へ追加した。
- 価値: 複数区間が別ジョブへ展開される設定かどうかを、詳細欄を開かず確認できる。
- 未検証: 区間数が多い場合の概要幅、展開後ジョブ一覧との表示同期。
## 2026-08-02 — SelectedFrames の範囲編集 UI

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 実装: Selected Frames 用に開始／終了フレームのスピンボックスを追加し、確定時に半開区間 `[start, end)` を `selectedFrameRanges` へ保存する。既存ジョブに範囲がない場合はジョブの start/end を初期表示する。
- 価値: SelectedFrames が単なるモード表示ではなく、Render Queue Inspector から実際に区間を編集できる。
- 未検証: 非連続区間を複数入力する UI、負フレーム、実ランタイムの選択モード切替。
## 2026-08-02 — SelectedFrames の非連続区間編集

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 実装: `Add Range` で現在の `[start, end)` 入力を既存の `selectedFrameRanges` へ追加し、`Clear Ranges` で全区間を消去できるようにした。Inspector 同期時にボタンの有効状態も更新する。
- 価値: 非連続区間を複数登録し、Render Queue のジョブ分割仕様へ UI から直接渡せる。
- 未検証: 重複区間の自動統合（サービスの展開段階で処理）、区間数が多い場合の UI 幅、空リストでの preflight 表示。
## 2026-08-02 — SelectedFrames 区間一覧の可視化

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 修正: Inspector に登録済みの `[start, end)` 区間一覧を表示するラベルを追加し、ジョブ同期時に `Ranges: [a, b), ...` を再生成するようにした。
- 価値: 非連続区間の登録結果を、キュー投入前にその場で確認できる。
- 未検証: 区間数が非常に多い場合のラベル折り返し、重複統合後の表示差、実ランタイムの再選択。
## 2026-08-02 — Flexible Grid のズームフェード

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のグリッド描画。
- 修正: ズーム値に応じて major／minor／axis グリッド色のアルファを `0.25〜1.0` の範囲で縮退させ、矩形・極座標・アイソメトリック表示へ適用した。
- 価値: 大きくズームアウトしたときの線密度による視覚ノイズを抑え、キャンバス内容を読みやすくする。
- 未検証: 極端なズーム値、透過色設定との組み合わせ、実ランタイムの表示バランス。
## 2026-08-02 — Grid ラベルのズームフェード整合

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の極座標／アイソメトリック／矩形グリッドラベル。
- 修正: グリッド線と同じ `gridFade` を数値ラベルのアルファにも適用した。
- 価値: ズームアウト時に線だけ薄くなってラベルだけが残る不整合を防ぐ。
- 未検証: 低ズーム時のラベル可読性、DPI スケーリング、実ランタイム表示。
## 2026-08-02 — SelectedFrames 未選択状態のリセット

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の `syncDetailEditorsFromJob()`。
- 修正: ジョブが未選択または無効な場合、区間一覧を `Ranges: none` に戻し、開始／終了入力と Add/Clear ボタンを無効化する。
- 価値: 前のジョブの SelectedFrames 情報が Inspector に残って見える状態を防ぐ。
- 未検証: ジョブ削除・一括削除直後の再描画順序、実ランタイムのフォーカス状態。
## 2026-08-02 — Text 作成キャンセル時の仮レイヤー破棄

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Text ツール mouseRelease 処理。
- 事実: 新規 Text レイヤー作成後に編集ダイアログを開くため、入力をキャンセルすると初期値だけのレイヤーが残る経路があった。
- 修正: ダイアログが未承認の場合、作成直後のレイヤー ID を使って仮レイヤーを削除し、選択状態と合成キャッシュを更新するようにした。
- 価値: 作成操作のキャンセルが「何も作成しなかった」状態になり、空の Text レイヤーがタイムラインへ残らない。
- 未検証: 実ランタイムでのダイアログキャンセル、Undo 履歴との組み合わせ、サービス削除後の選択同期。
## 2026-08-02 — SelectedFrames 区間の重複登録防止と順序正規化

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の SelectedFrames UI。
- 修正: 同一の `[start, end)` 区間を二重登録せず、追加後の区間一覧を開始フレーム・終了フレーム順に並べるようにした。
- 価値: 非連続区間の表示順とキュー展開順の予測可能性を高め、同一区間の重複ジョブ生成を抑える。
- 未検証: 重なり合うが完全一致しない区間の統合方針、サービス側での追加正規化。
## 2026-08-02 — Render Queue ROI 出力サイズの実クロップ整合

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` の software/GPU 出力サイズ計算。
- 事実: コンポジション外へはみ出す crop は描画時にコンポジション矩形へ交差処理される一方、解像度プリセットの基準サイズが要求された `cropW/cropH` のままだった。
- 修正: 実際に生成されたクロップ画像、またはコンポジション矩形との交差矩形を基準にプリセット解像度を計算するようにした。
- 価値: ROI が境界外へはみ出すケースでも、出力ピクセルサイズと実画像の内容が一致しやすくなる。
- 未検証: ROI が完全にコンポジション外の場合の事前診断・バックエンド別エンコーダー挙動。
## 2026-08-02 — 完全境界外 ROI の事前エラー化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` の render preflight。
- 修正: ROI/custom crop とコンポジションの交差矩形が空の場合、従来の境界外警告ではなくキュー投入を止める設定エラーとして報告するようにした。部分的なはみ出しは従来どおり警告とクリップを維持する。
- 価値: 完全外側の ROI が意図せずフルフレームへフォールバックする曖昧な挙動を防ぐ。
- 未検証: UI の preflight 表示更新タイミング、保存済み旧ジョブの再読込時の診断表示。
## 2026-08-02 — SelectedFrames UI とサービス展開ルールの統一

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 修正: UI で区間を追加した後、開始順に並べ、重複・接続する区間を表示段階でマージするようにした。
- 価値: Inspector の区間一覧と、キュー実行時に生成される個別ジョブの区間境界を一致させる。
- 未検証: 既存保存データに含まれる不正な区間、極端に多数の区間を登録した場合の UI 負荷。
## 2026-08-02 — Selective Render レイヤーリスト要約

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の Selective Render Inspector。
- 修正: 保存済み whitelist／blacklist の先頭 ID を短縮表示し、残数と Included／Excluded の区別を常時表示するラベルを追加した。ジョブ未選択時は `none` に戻す。
- 価値: レイヤーフィルタの実体を件数だけでなく確認でき、Custom／Selected 設定の誤投入を減らす。
- 未検証: UUID 以外の ID 表現、非常に狭い Inspector 幅での折り返し。
## 2026-08-02 — レイヤーフィルタモードの明示

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 修正: Included／Excluded の ID 要約に `All / Selected / Solo / Visible / Custom` の現在モードを追加し、ジョブ未選択時の初期表示も `Mode: All` に統一した。
- 価値: ID リストが空でも、フィルタモード自体が Selected／Solo などへ切り替わっている状態を見落としにくくする。
- 未検証: 古い保存値が範囲外のモード番号を持つ場合の表示。
## 2026-08-02 — Render Pass 有効数の Inspector 表示

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` の Configure Passes ボタン。
- 修正: 有効かつ名称付きの render pass 数を数え、`Configure Passes (N)…` と表示するようにした。Split Passes が無効、またはジョブ未選択時はボタンを無効化する。
- 価値: パス分割設定の存在と設定数を、ダイアログを開かずに確認できる。
- 未検証: pass 名変更直後の Inspector 更新順序、非常に長いパス名との併用。
## 2026-08-02 — Layer Eraser 全フレーム dirty 通知

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm` の `clearAllFrames()`。
- 調査: `markDirty()` が現状 no-op であることを確認し、全フレームへ通知するだけの変更は実効性がないため撤回した。
- 現状: `clearAllFrames()` 後の `changed()` と呼び出し側の `publishLayerModified()` がレイヤー全体の再描画経路を担う。フレーム単位 dirty API の実装は別課題として残る。
- 価値: 効果のないループを追加せず、キャッシュ無効化の責務を実際の revision／publish 経路に残せた。
- 未検証: フレーム単位 GPU キャッシュを本当に導入する場合の世代キー設計。
## 2026-08-02 — 楕円シェイプの自動レイヤー名

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Rectangle/Ellipse 作成処理。
- 修正: EllipseShape モードでは `Ellipse` を基底名にし、RectangleShape モードでは `Rectangle` を維持するようにした。
- 価値: タイムライン上で作成したシェイプ種別を名前から識別できる。
- 未検証: 既存プロジェクトの命名規則、ローカライズ表示名との整合。
## 2026-08-02 — Render Pass 出力ファイル名の衝突回避

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` の `expandEnabledRenderPasses()`。
- 事実: 異なる pass 名でもサニタイズ後に同じ文字列になると、同一の出力パスへ複数ジョブが書き込む可能性があった。
- 修正: キュー内の通常ジョブ出力を先に予約し、展開済み pass 出力も大文字小文字を畳み込んで追跡する。衝突時は `_2`, `_3` の連番を付与する。
- 価値: `A/B` と `A:B` のような pass 名だけでなく、別ジョブの既存出力との上書きも避けやすくする。
- 未検証: 外部プロセスが同時に同じパスへ書き込むケース、既存キュー内の意図的な同一出力設定。
## 2026-08-02 — Renderer 非依存 ViewportOverlayManager

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: 複数の水平／垂直ルーラーとスケールバーを ID で登録・削除・表示切替できるデータ管理クラスを追加した。個別設定から既存の tick／scale-bar factory を呼び出し、描画用中間データだけを返す。
- 価値: レンダラーや描画 widget に依存せず、将来の複数単位・複数位置オーバーレイへ拡張できる。
- 未検証: 実 widget からの複数 overlay 接続、キャッシュ最適化、3D compass／距離計測 overlay。
## 2026-08-02 — Viewport Overlay runtime 設定更新

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: 登録済み ID に対してルーラー／スケールバー設定を差し替える `configureRuler()` と `configureScaleBar()` を追加した。存在しない ID は `false` を返す。
- 価値: ズーム単位、目標ピクセル間隔、水平／垂直方向、ラベル表示を overlay 再登録なしで変更できる。
- 未検証: runtime 設定変更と外部キャッシュの同期、同一 ID の異種 overlay 競合。
## 2026-08-02 — Viewport Overlay tick cache

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: ルーラーごとに zoom／viewport origin／viewport size／canvas size と生成 tick を保持し、入力が同じ場合は再計算を省略する。設定変更と `invalidateCache()` で明示的に破棄する。
- 価値: overlay 描画が毎フレーム同じ tick 計算を繰り返す負荷を抑え、pan／zoom 更新時だけ再生成できる。
- 未検証: 多数 overlay のメモリ量、浮動小数点の微小変動によるキャッシュヒット率、実 renderer 統合後の invalidation 契約。
## 2026-08-02 — Scale Bar 配置アンカー

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: Scale Bar に BottomLeft／BottomRight／TopLeft／TopRight の anchor と X/Y margin を追加し、生成データの viewport 座標へ反映した。デフォルトは従来の左下配置を維持する。
- 価値: 同一 viewport に複数単位のスケールバーを異なる隅へ配置する基盤になる。
- 未検証: ラベル矩形との重なり回避、極小 viewport、DPI スケーリング。
## 2026-08-02 — Ruler 基準アンカー

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: Ruler に Start／Center／End の anchor を追加し、Center はキャンバス中央、End はキャンバス終端を基準に tick 範囲を生成するようにした。Start は従来の viewport origin 基準を維持する。
- 価値: 画面追従ルーラーと、キャンバス中央・終端基準の補助ルーラーを同じ manager で扱える。
- 未検証: viewport が canvas より大きい場合の表示、anchor 切替時の外部 cache invalidate、垂直軸の実描画位置。
## 2026-08-02 — Renderer 非依存 Grid Label データ

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: Grid Label の単位・目標ピクセル間隔・表示位置を登録できる設定と、ズームから現在の間隔／ラベルを生成する API を追加した。表示切替と設定更新は他の overlay と同じ ID 管理で扱う。
- 価値: グリッド描画本体から、間隔ラベルの計算責務を分離できる。
- 未検証: 実グリッド描画への接続、複数単位ラベルの重なり回避。
## 2026-08-02 — Renderer 非依存 Viewport Compass データ

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: Compass の登録・設定・表示切替と、yaw から X/Y 軸終点を計算するデータ生成 API を追加した。座標とサイズだけを返し、実描画は renderer 側へ分離している。
- 価値: 3D viewport の方向表示を既存の overlay 管理へ追加できる。
- 修正補足: 画面座標系では yaw=0 の Y 軸を上向き（負の画面 Y）として生成するようにした。X 軸は右向きを維持する。
- 未検証: カメラの pitch／roll、実 3D gizmo との方位同期。
## 2026-08-02 — Viewport tick cache の許容誤差比較

- 関連: `Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: cache hit 判定の zoom、origin、viewport／canvas size を厳密一致から相対許容誤差比較へ変更した。微小な浮動小数点揺れでは tick を再生成しない。
- 価値: フレームごとのカメラ行列・DPI 計算による僅かな数値差でキャッシュが無効化されるケースを抑える。
- 未検証: 極端に大きい canvas 座標、意図的に非常に小さい pan 差を即時反映したいケース。
## 2026-08-02 — Viewport Overlay 1フレーム一括生成

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 実装: Ruler／Scale Bar／Grid Label／Compass の可視データを `ViewportOverlayFrameData` にまとめる `generateAll()` を追加した。各 ID の個別生成 API と tick cache はそのまま再利用する。
- 価値: renderer adapter が overlay 種別ごとに manager を反復呼び出しせず、1 フレーム単位の中間表現を受け取れる。
- 未検証: overlay 数が非常に多い場合のコピー量、描画 backend での ID 順序依存、frame data の再利用。
## 2026-08-02 — Viewport Overlay lifecycle clear

- 関連: `Artifact/include/Widgets/Render/ViewportScaleOverlay.ixx`、`Artifact/src/Widgets/Render/ViewportScaleOverlay.cppm`。
- 修正: 全 overlay エントリと ruler tick cache を一括破棄する `clear()` を追加した。manager 寿命中の ID 採番はリセットせず、旧 frame data と新 overlay の ID 衝突を避ける。
- 価値: composition／workspace 切替時に旧 overlay が残る状態を、manager の単一操作でリセットできる。
- 未検証: 外部 renderer が保持する旧 frame data の寿命、長時間運用時の ID 上限。
## 2026-08-02 — AnchorPoint Ctrl+ダブルクリック中央リセット

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 修正: AnchorPoint ツールがアクティブな場合だけ viewport 上の Ctrl+左ダブルクリックを、既存の `resetSelected2DAnchorToCenter()` へ接続した。アンカー移動と位置補正は既存の `AnchorPoint2DUndoCommand` を通る。
- 価値: AnchorPoint ツールでレイヤー中心へ戻す操作を、Inspector や別 UI を開かずに実行できる。
- 未検証: Text／3D レイヤー選択中の優先順位、複数選択時の対象、実ランタイムの modifier 判定。
## 2026-08-02 — 2D回転のShiftスナップ

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、TransformGizmo の Rotate 操作。
- 実装: Shift 押下中の2D回転角を45度単位へスナップし、確定角度から累積ドラッグ値を再同期するようにした。これにより、回転リングと HUD の表示、後続のマウス差分、位置補正が同じ角度を参照する。
- 価値: ビューポート上で水平・垂直・斜め方向へ正確に配置しやすくなる。
- 未検証: Shift の押下／解除をドラッグ途中で切り替えた場合の操作感、複数選択時の各レイヤー補正、実ランタイムの角度表示。
## 2026-08-02 — AnchorPoint 数値HUD

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、AnchorPoint ツール。
- 実装: アンカーポイントをドラッグしている間、現在のローカル座標を `OVR:ANCHOR X/Y` としてアンカー付近に表示する。表示は既存の overlay panel／renderer 経路を使用する。
- 価値: 視覚的な中心合わせだけでなく、レイヤー座標を確認しながら精密にアンカーを配置できる。
- 未検証: キャンバス端でのラベルの画面外クリップ、極端なズーム時のラベル密度、複数選択時の代表値の妥当性。
## 2026-08-02 — 回転スナップ刻みの設定化

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、Viewport の Rotate 操作。
- 実装: `ArtifactStudio/Viewport/RotationSnapDegrees` を読み取り、15／30／45／90度の最近傍値へ正規化してShiftスナップに使う。未設定時は45度を既定値とする。
- 価値: 固定45度だけでなく、精密配置や粗い方向合わせに適した刻みへ切り替えられる。既存の設定保存方式と互換性がある。
- 未検証: 設定UIからの書き込み導線、設定変更中のドラッグ、複数アプリ設定スコープ。
## 2026-08-02 — Scale Center の倍率HUD

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、Scale Center ハンドル。
- 実装: 中心基準の均等スケール中に、既存の resize badge へ開始倍率に対する現在倍率をパーセント表示する。
- 価値: サイズ変化を目視だけでなく「150.0%」のような数値で確認できる。既存のoverlay描画とズーム追従を再利用する。
- 未検証: 初期倍率が負値／極端値のレイヤー、他のScaleハンドルとの表示形式統一、複数選択時の代表倍率。
## 2026-08-02 — Scale コーナーの均等倍率既定

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`、Scale の四隅ハンドル。
- 実装: 四隅ドラッグはShiftなしで開始矩形のアスペクト比を維持し、Shift押下時だけ自由な縦横比変更を許可する。固定点は反対側のコーナーとして、位置補正の既存経路へ渡す。
- 価値: Scaleツールの既定操作を均等スケールへ揃え、意図しない画像・シェイプの変形を抑える。
- 未検証: ドラッグ途中のShift切替、負方向への反転、テキスト／シェイプ固有サイズ編集との組み合わせ。
## 2026-08-02 — 3D軸ギズモの変形HUD統合

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、3Dフレーム／軸ギズモのオーバーレイ。
- 実装: 投影フレームの移動・リサイズだけでなく、`Artifact3DGizmo` の軸ドラッグ中も同じ位置・サイズ・回転HUDを表示する。操作種別は `GIZMO` として区別する。
- 価値: 3D操作で現在値が見えなくなる経路をなくし、フレームギズモと軸ギズモのフィードバックを統一する。
- 未検証: 軸ギズモの各モードでのパネル位置、カメラ行列が無効な場合のフォールバック、HUDと軸ラベルの重なり。
## 2026-08-02 — 3D HUD 操作モードの具体化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、3D変形HUD。
- 修正: 軸ギズモ操作時の汎用的な `GIZMO` 表示を、実際の `MOVE`／`ROTATE`／`SCALE` モード名へ置き換えた。投影フレームの移動・リサイズ表示は従来どおり優先する。
- 価値: 数値がどの操作によって変化しているかをHUDだけで判別できる。
- 未検証: `Full` モード時の表示名、軸ごとの操作名表示、モード切替中のドラッグ状態。
## 2026-08-02 — 3D HUD の軸／平面表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact3DGizmo` のHUD。
- 実装: 軸ギズモのアクティブハンドルを `X`／`Y`／`Z`／`XY`／`YZ`／`XZ`／`SCREEN` としてモード名へ付加する。投影フレーム操作時は従来の `MOVE`／`RESIZE` 表示を優先する。
- 価値: 同じRotate／Scaleモードでも、どの軸・平面を操作しているかを即時に把握できる。
- 未検証: `Full` モードのNone軸、軸ラベルの長さによるHUD幅、画面平面ハンドルの実操作名。
## 2026-08-02 — Mask tangent ダブルクリックリセット

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 実装: Penツール中のダブルクリックで、現在hover中のIn／Out tangentを既存の`resetHoveredMaskTangent()`経由でゼロへ戻す。編集トランザクションとUndo経路は既存実装を再利用する。
- 価値: ベジェハンドルを正確に原点へ戻す操作を、Inspectorや手動ドラッグなしで実行できる。
- 未検証: 頂点本体上のCtrlクリックとの競合、hover更新が間に合わない高速ダブルクリック、ロック済みマスク。
## 2026-08-02 — 3D HUD の実操作優先表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact3DGizmo::activeOperation()`。
- 修正: HUDの操作名を表示モードだけでなくアクティブ操作から決定するようにした。FullモードでTranslate／Rotate／Scaleのどれを掴んだかが正しく表示され、操作未確定時は従来のモードをフォールバックに使う。
- 価値: Fullモードや複合ギズモでのフィードバックが実際の入力状態と一致する。
- 未検証: activeOperation がドラッグ開始直後に更新されるタイミング、Noneからモードへ戻る瞬間、既存の軸ラベルとの組み合わせ。
## 2026-08-02 — 3D HUD 幅の内容適応

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、3D変形HUD。
- 実装: HUD各行のフォント幅からパネル幅を計算し、238〜360pxの範囲で調整する。軸／平面ラベルやTextBox情報が長い場合も、固定幅による切れを抑える。
- 価値: 操作状態の追加情報を表示しても、既存の数値行を読みやすく保てる。
- 未検証: 非常に長いフォント名・テキスト情報、狭いビューポートでの左右余白、DPI倍率ごとの見た目。
## 2026-08-02 — 常時3D HUDの操作／軸統一

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、3Dギズモ常時HUD。
- 修正: 右上の常時HUDもactiveOperationを優先し、`3D Rotate Axis Z`のように操作種別と軸を表示する。固定幅を276pxへ拡張して、位置・回転・サイズ・スケール行の可読性を確保した。
- 価値: 画面内に複数の3Dフィードバックが存在しても、操作状態の表記ルールが揃う。
- 未検証: 極端に狭いビューポート、Screen／複合平面の表記、2つのHUDが同時表示される場合の重なり。
## 2026-08-02 — 常時3D HUDの狭幅対応

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、常時3DギズモHUD。
- 修正: パネル幅をビューポート幅から算出し、176〜276pxへ制限した。右端位置も同じ幅を使って再計算し、通常サイズでは既存の余白を保つ。
- 価値: 小さいプレビュー領域やドッキング状態でも、HUDが右側へ大きくはみ出すケースを抑える。
- 未検証: 176px未満の極小領域、DPIスケーリング、他オーバーレイとの重なり。
## 2026-08-02 — 3D 常時HUDの平面軸表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、常時3DギズモHUD。
- 修正: XY／YZ／XZ平面ハンドルを一律の`Plane`ではなく、それぞれの平面名として表示する。X／Y／Z／Screenの既存表示も維持する。
- 価値: 平面移動・平面回転・平面スケールで、拘束方向をHUDだけから判別できる。
- 未検証: `GizmoAxis::None`からの一瞬の表示、Fullモードの平面ハンドル、狭幅時の文字収まり。
## 2026-08-02 — Mask 頂点Ctrlクリックのタンジェントリセット

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 実装: Penツールで頂点をCtrlクリックした際、In／Out両タンジェントをゼロへ戻す専用APIを追加し、既存のマスク編集トランザクションへ接続した。既にゼロの場合は従来のハンドル操作へフォールバックする。
- 価値: 仕様のCtrl+クリックリセットを実装し、ベジェ頂点を直線化する操作を明示的に行える。
- 未検証: Ctrl押下のままドラッグした場合の従来操作との互換性、既にゼロの頂点、アニメーション中のマスクパス。
## 2026-08-02 — Mask 頂点ダブルクリックのリセット導線

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、Mask Pen操作。
- 修正: Penのダブルクリック処理で、まずhover中のIn／Outハンドルをリセットし、ハンドルがない場合はhover中の頂点の両タンジェントをリセットするフォールバックを追加した。
- 価値: ハンドルの正確なヒットが難しいズーム状態でも、頂点本体からベジェを直線化できる。
- 未検証: 近接する複数頂点のhover優先順位、閉じたパスの共有接線、ダブルクリック時のパス編集開始との競合。
## 2026-08-02 — Brush主要設定のアクセシビリティ補完

- 関連: `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`、Brush Tool Options。
- 修正: Diameter／Hardness／Opacity／Flow／Spacing／Angle／Roundness の各spin boxへ英語のaccessible nameと操作説明tooltipを追加した。既存の値範囲・信号接続・レイアウトは変更していない。
- 価値: 数値コントロールが視覚的な単位だけでも、スクリーンリーダーやキーボード操作で意味を識別できる。
- 未検証: 日本語ロケールでの読み上げ、Eraser側共有コントロールとの命名整合、狭いTool Options幅。
## 2026-08-02 — Eraser主要設定のアクセシビリティ補完

- 関連: `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`、Eraser Tool Options。
- 修正: Eraser diameter／opacityにもaccessible nameとtooltipを追加し、既存のhardness／angle／roundness／strength／modeと命名粒度を揃えた。
- 価値: Eraserの主要数値設定を支援技術から識別しやすくする。
- 未検証: BrushとEraserの共有設定を切り替えた際の読み上げ順、狭幅レイアウト。
## 2026-08-02 — Last Stroke Only のアクセシビリティ補完

- 関連: `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`、Eraser Tool Options。
- 修正: Last Stroke Onlyチェックボックスにaccessible name／descriptionを追加し、単なる表示ラベルではなく「直前のストロークだけを消去する」意味を支援技術へ伝える。
- 価値: Layer EraserやPaint Eraserとの違いを、視覚に依存せず判別できる。
- 未検証: mode comboとの二重表示時の読み上げ順、ローカライズ文字列。
## 2026-08-02 — Image Sequence 端点フレーム保持

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`refreshSequenceFrame()`。
- 修正: レイヤー相対フレームが連番枚数の範囲外になった場合、最初／最後の有効フレームへクランプして表示を保持する。空のフレームを返すのはシーケンス自体が空、読み込み失敗、画像不正の場合に限定する。
- 価値: レイヤーのout pointや長い表示範囲で、末尾到達時に画像が突然消える不安定さを抑える。
- 未検証: 意図的なblank tailを必要とする素材、ホールド以外のloop／ping-pong設定、シーケンスFPSとコンポFPSの差。
## 2026-08-02 — Image Sequence FPS時間変換

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`ArtifactImageLayer::draw()`。
- 実装: コンポジションFPSと連番素材FPSが異なる場合、レイヤー相対フレームを秒へ変換してからシーケンスフレームへ丸める。FPS情報が無い場合は従来の1:1フレーム対応へフォールバックする。
- 価値: 24fps素材を30fpsコンポジションで再生する場合などに、フレームの進み方が時間基準で安定する。
- 未検証: 非整数FPS、負の開始時刻、ドロップフレーム表現、loop／ping-pong設定との組み合わせ。
## 2026-08-02 — Renderメニューのアクセシブル説明

- 関連: `Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm`、Render menu actions。
- 修正: 現在のコンポジション追加、全コンポジション追加、キュー表示、管理、出力設定、開始、全削除の各 QAction に、操作内容を説明するaccessible descriptionを追加した。既存のショートカット・有効状態・実行経路は変更していない。
- 価値: 日本語の短い表示名だけでは意味が曖昧なアクションでも、支援技術から目的を識別できる。
- 未検証: QtプラットフォームごとのQAction説明読み上げ、メニュー再構築時の保持、ローカライズ。
## 2026-08-02 — Renderメニューのステータス説明

- 関連: `Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm`、Render menu actions。
- 修正: accessible descriptionと同じ目的文をQActionのstatus tipにも設定した。メニュー選択時のステータスバー表示と支援技術向け説明を同じ語彙に揃えた。
- 価値: アクションを実行する前に、現在の操作対象をステータスバーで確認できる。
- 未検証: メインウィンドウ側のstatus bar受信、メニュー再構築後の表示、翻訳方針。
## 2026-08-02 — Project Settings 検証のエクスポート接続

- 関連: `Artifact/src/Project/ArtifactProjectExporter.cppm`、`ArtifactProjectSettings::validate()`。
- 事実: エクスポート前はプロジェクトツリー検証のみで、プロジェクト名の禁止文字など設定モデルの検証結果を評価していなかった。
- 修正: エクスポート前に設定を検証し、Error は書き出しを中止、Warning／Info はログへ通知するようにした。
- 価値: まだ専用のプロジェクト設定ダイアログがない状態でも、不正なメタデータを成果物へ流し込む経路を減らせる。
- 未検証: GUIからの保存経路がExporterを必ず通るか、既存プロジェクトの不正名を開いて再保存する場合のユーザー通知。
## 2026-08-02 — ビューポートホイールの高精細入力対応

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`wheelEvent()`。
- 事実: 従来は `angleDelta()` のみを読み、トラックパッド等の `pixelDelta()` だけを返す入力では垂直デルタがゼロでも固定の縮小分岐へ進んでいた。
- 修正: pixel deltaを正規化して連続ズーム量へ変換し、Shift水平パンでもpixel deltaを利用するようにした。従来のマウスホイールの120単位経路は維持した。
- 価値: 高精細ホイール／トラックパッドでのズーム方向誤判定と、入力粒度を無視した飛び飛びの操作を抑える。
- 未検証: 各OS・QtプラットフォームのpixelDeltaスケール、自然スクロール設定、長時間の高頻度イベント時の描画負荷。
## 2026-08-02 — パッケージ化経路のプロジェクト検証

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`、`collectAndPackage()`。
- 事実: パッケージ化は `project->toJson()` を直接呼び、通常Exporterの設定／ツリー検証を経由していなかった。
- 修正: アセットコピー開始前にプロジェクト検証を実行し、設定Errorまたはツリー不整合があれば処理を中止する。Warning／Infoはログへ通知する。
- 価値: 不正なメタデータや壊れた参照を含むスタンドアロンパッケージの生成を防ぎ、通常保存との検証基準を揃える。
- 未検証: 既存の不完全プロジェクトを救済目的でパッケージ化する運用、コピー途中の失敗時に作成済みAssetsをロールバックする仕様。
## 2026-08-02 — パッケージJSONの原子的書き込み

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`、`collectAndPackage()`。
- 事実: パッケージの `project.json` は通常の `QFile` へ直接書き込み、書き込みサイズと完了状態を確認していなかった。
- 修正: `QSaveFile` を使い、完全な書き込みとcommitに成功した場合だけ最終ファイルへ置換する。Assetsディレクトリ作成の失敗も明示的に扱う。
- 価値: パッケージ生成中のディスク容量不足やI/O障害で、既存の `project.json` を壊したり不完全なJSONを残したりするリスクを下げる。
- 未検証: アセットコピー途中の失敗時に残るファイルのクリーンアップ方針、ネットワークドライブ上のQSaveFile置換動作。
## 2026-08-02 — Rig編集ツール型の基盤追加

- 関連: `Artifact/include/Tool/ArtifactToolManager.ixx`、`Artifact/src/Tool/ArtifactToolManager.cppm`、`Artifact/src/Widgets/ArtifactToolBar.cppm`。
- 事実: リグ描画ヘルパーは存在する一方、仕様が参照するRigSelect／RigWeightのToolTypeが未定義だった。
- 修正: `RigSelect` と `RigWeight` をToolTypeへ追加し、toolNameとツールラベルを登録した。既存の選択・描画・イベント経路はまだ変更していない。
- 価値: 後続のリグピック／ウェイトペイント実装が、既存ツール管理契約上の明確な型を利用できる。
- 未検証: ツールバーへの専用ボタン、ショートカット、実際のリグ選択・ウェイト操作。
## 2026-08-02 — Rigツールバー登録

- 関連: `Artifact/src/Widgets/ArtifactToolBar.cppm`。
- 修正: 既存のActionGroup／既存lambdaを利用し、RigSelectとRigWeightのツールボタンを追加した。専用の新規signal/slot接続は追加していない。
- 価値: リグ編集ツールを内部APIだけでなく、ユーザーが選択できるUI状態へ進めた。
- 未検証: 実際のボーンピック、ウェイトペイント、アイコン候補の配布環境での表示。
## 2026-08-02 — Rigオーバーレイ表示の接続

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 修正: `showRigOverlay` の公開設定を追加し、RigSelect／RigWeight選択時または明示フラグ有効時に、選択中の `ArtifactAbstract2DLayer` のボーン、コントロール、スキンメッシュ線を既存描画ヘルパーで表示するようにした。
- 価値: リグ編集UIの最初のフィードバックとして、現在対象の骨格とコントロールをVP上で確認できる。
- 未検証: 変形後メッシュ表示、カメラ変換を含む座標一致、選択ボーンの強調、ボーン／コントロールのピック操作。
## 2026-08-02 — RigSelectのボーン／コントロールヒットテスト

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`handleMousePress()`／`handleMouseRelease()`。
- 修正: RigSelectの左クリックで、選択中の2Dレイヤーを対象にコントロールを優先してヒットテストし、次にボーン線分を再帰探索するようにした。レイヤーのglobal transformを逆変換してローカル座標で距離判定し、ズームに応じてヒット閾値を補正する。
- 状態: 選択したボーン／コントロールIDをController内に保持し、マウスリリースまでの一時ドラッグ状態を管理する。実際の回転・値変更は次段階に残している。
- 価値: リグオーバーレイが単なる表示から、編集対象を識別できるインタラクション基盤へ進んだ。
- 未検証: ボーンの親子座標がresolvedTransformで表現される場合の線分位置、選択ハイライト描画、ドラッグ中のポーズ更新とUndo。
- 追加項目: RigSelectボーン回転ドラッグ。
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、RigSelectのmouse press／move経路。
- 修正: 選択したボーンをドラッグすると、ボーン原点を基準にローカル座標の角度差を計算し、`setRigBoneLocalTransform()` でrotationを更新する。Shift押下時は15度刻みにスナップし、階層を再計算して描画・レイヤー変更通知を行う。
- 価値: リグオーバーレイとヒットテストが、実際のポーズ編集操作へ接続された。
- 未検証: Undoコマンド化、キーフレーム自動追加、制約／SmartBone評価との同時利用、親ボーン回転時の表示座標。
- RigControl操作を追加。Pointはローカル位置、SliderはXドラッグ量をmin/maxへ正規化、Angleは原点周りの角度として更新する。リリース時に値変更を専用UndoCommandへまとめる。
- 未検証: コントロール値に紐づくRigPropertyBinding／制約の即時評価、Pointのドラッグ基準、スライダー感度のUI設定。
- Rigコントロール変更後にレイヤーの通常フレーム評価経路を呼び、既存のRigPropertyBinding／制約／値クランプを再利用する。選択中のボーン原点とコントロールに黄色のリングを描画する。
- 未検証: 高頻度ドラッグ中の評価コスト、Angle／Sliderコントロールの表示原点、キーフレーム値との競合。
- RigSelect／RigWeightのVP下部に、現在モードと選択中のボーン／コントロール名を表示するHUDを追加した。表示ハイライトと同じ状態を使うため、選択対象の確認経路が二重化される。
- 未検証: 小さいビューポートでのHUD重なり、ローカライズ、Weightモードの実編集との表示整合。
- EscapeキーでRigSelectの選択とドラッグ状態を解除するController APIを追加し、既存のCompositionEditorキー処理へ接続した。マスク／テキストのキャンセルを先に評価する順序は維持している。
- 未検証: RigWeightのブラシ状態解除、フォーカスが子ウィジェットにある場合のキー伝播。
- ボーン描画とヒットテストの座標を、各ボーンのlocal/resolved positionから`globalMatrix()`基準へ統一した。親子階層の子ボーンでも原点と先端が同じ階層変換を通り、描画と選択判定の不一致を減らせる。
- 未検証: 非一様スケールを持つ親子ボーン、スキンメッシュのdeform座標との一致、カメラ投影下のruntime表示。
- RigWeightモードで、既存SkinMeshの読み取り専用頂点ウェイトを選択ボーンへ投影し、deform後頂点を青→緑→赤で可視化する表示を追加した。Coreの頂点書き込みAPIは変更していない。
- 未検証: 4ウェイトスロット以外の表現、ボーン選択前の対象インデックス、密なメッシュでの描画負荷。
- RigWeightでも同じボーンヒットテストを使って対象ボーンを選択できるようにした。RigWeightでのクリックは対象選択だけを行い、RigSelectのような回転ドラッグは開始しない。
- 未検証: Weight Paintブラシによる書き込み、複数ボーン選択、選択状態の別パネルとの同期。
- RigSelect回転のUndoを追加。ドラッグ開始時のBoneTransformを保持し、リリース時に変更があれば専用UndoCommandを1件だけ積む。undo/redoでは同じリグAPIと階層更新を利用する。
- 未検証: 複数ボーン選択、キーフレーム化、制約評価との組み合わせ。
- RigSelect／RigWeight QActionにaccessible name／descriptionを追加し、選択操作とウェイトヒート表示の違いを支援技術へ明示した。
- 未検証: QtプラットフォームごとのQAction読み上げ、ローカライズ、ツールチップとaccessible descriptionの表示順。
- ControllerのtoolTypeToOverlayLabelへRigSelect／RigWeightを追加し、既存HUDや診断表示で一般名`Tool`へフォールバックしないようにした。
- 未検証: 全オーバーレイの表示箇所、翻訳、狭幅ビューでの文字切り詰め。
- RigSelect／RigWeightに既存のselectカーソル資産を割り当て、通常Selectionと同じArrowへ落ちず、リグ編集モード中であることをカーソルでも示すようにした。新規アイコンは追加していない。
- 未検証: ウェイトブラシ実装後のブラシ形状カーソル、DPI別のカーソル可読性。
- Rig PointコントロールのShiftドラッグを水平／垂直軸ロック、AngleコントロールのShift操作を15度スナップとして実装した。ボーン回転と同じ修飾キー規則をコントロールにも適用する。
- 未検証: RTL座標系、細かい角度範囲、Pointの親階層変換。
- Rig HUDの選択表示へ、ボーン回転角、PointのX/Y、Slider／Angleの現在値を追加した。ドラッグ中の操作結果を別パネルへ移動せず確認できる。
- 未検証: 長い日本語名と数値の横幅、狭いビューポートでの切り詰め、単位表記のローカライズ。
- Rigオーバーレイの明示表示設定を`ArtifactStudio/RigOverlayVisible`へ保存し、Controller生成時に復元するようにした。ツール選択による一時表示は従来どおり自動で行う。
- 未検証: 複数Controller生成時の設定共有、Workspace別設定の分離、設定移行時のキー互換。
# 2026-08-02: Rig表示をラインデバッグ項目へ分離

- 関連: `ArtifactCompositionRenderController` / Rig overlay
- 事実: Rig の骨、コントロール、スキン表示は従来、Rig overlay が有効なら一括描画されていた。
- 変更: `LineDebugKind::RigBone`、`RigControl`、`RigSkin` を追加し、既存のライン表示配列から個別に描画を抑制できるようにした。既定値は骨・コントロールを表示、スキンを非表示とした。
- 価値: Rig Select と Weight の作業時に、表示ノイズを既存のデバッグ表示経路で段階的に減らせる。
- 未検証: UI 側で新しい3項目を公開するメニュー接続はまだ追加していない。現時点ではコントローラ内部の表示状態を利用する実装段階。
- 次に確認: ライン表示設定の既存UIへ Rig 3項目を追加する際、既存メニューの責務と保存キーを確認する。

# 2026-08-02: Rigライン表示の設定を保存

- 関連: `ArtifactCompositionRenderController::setLineDebugKindVisible`
- 変更: RigBone / RigControl / RigSkin の表示状態を `QSettings` に保存し、再起動時に復元するようにした。
- 判断: 既存の全ラインデバッグ項目へ一括で保存仕様を広げず、今回追加したRig表示項目に限定した。
- 未検証: ビルド・実行による設定復元確認は未実施。

# 2026-08-02: Rig WeightのVPペイント入力を追加

- 関連: `CompositionRenderController::handleMousePress/Move/Release`
- 変更: RigWeight選択時にメッシュ上をドラッグすると、選択ボーンのウェイトを距離フォールオフ付きで加算する処理を追加した。Ctrlドラッグでは減算し、4スロット内に対象ボーンがない頂点は最小ウェイトスロットを再利用する。更新後は全スロットを正規化する。
- 制約: CoreのSkinMesh APIを変更せず、公開済みの `vertices()` と `setVertices()` のみを利用している。
- 未検証: Undo復元、ミラー、ブラシ設定UI、ビルド・実行確認は未実施。

# 2026-08-02: Rig Weightのドラッグ単位Undo

- 関連: `RigSkinWeightsUndoCommand`
- 変更: Weightペイント開始時の全頂点スナップショットと、リリース時の結果を保存し、1回のドラッグを1つのUndo操作として登録するようにした。
- 制約: Coreの型やUndo基盤を変更せず、既存の `UndoCommand` と `SkinMesh::setVertices()` を利用した。
- 未検証: 実行時のUndo/Redo、巨大メッシュでのスナップショット負荷、ビルド確認は未実施。

# 2026-08-02: Rig Weightブラシの可視フィードバック

- 関連: `drawViewportCanvasOverlay` / RigWeight
- 変更: ウェイトブラシの現在位置に円形カーソルを描画し、Ctrl押下時は減算色へ切り替えた。画面下部に ADD/SUB、半径、Opacityを表示するHUDも追加した。
- 判断: 既存の `ArtifactIRenderer` 描画経路を使用し、Qtの新規合成やQImage化は行っていない。
- 未検証: 高DPI・極端なズーム時の見かけの半径、実行時表示確認は未実施。

# 2026-08-02: Rig Weightブラシ設定のキーボード調整

- 関連: `ArtifactCompositionEditor::keyPressEvent` / `adjustRigWeightBrush`
- 変更: RigWeight中の `[` / `]` でブラシ半径を調整し、Ctrl併用でOpacityを調整できるようにした。値は `QSettings` に保存して次回起動へ引き継ぐ。
- 初期値: 半径36、Opacity0.35。半径は2〜500、Opacityは0.01〜1.0にクランプする。
- 未検証: 実行時のキー入力と設定復元、ビルド確認は未実施。

# 2026-08-02: Rig Weight正規化コマンド

- 関連: `normalizeRigWeights`
- 変更: RigWeight中に `N` を押すと、各頂点の4スロットのウェイトを合計1.0へ正規化する。変更全体を `RigSkinWeightsUndoCommand` へ登録する。
- 判断: 無ウェイト頂点は勝手にボーンへ割り当てず、そのまま維持する。
- 未検証: キー入力、Undo/Redo、非正規化データの実行確認とビルドは未実施。

# 2026-08-02: Rig Weightスムージング

- 関連: `smoothRigWeights`
- 変更: RigWeight中に `S` を押すと、選択ボーンのウェイトをブラシ半径内の近傍頂点平均へ寄せ、全スロットを再正規化する。処理全体をUndo可能にした。
- 制約: 現在は頂点数に対して単純な近傍走査を行うため、大規模メッシュでは計算量が増える。必要になった段階で空間インデックス化を検討する。
- 未検証: 実行時の平滑化結果、Undo/Redo、性能、ビルドは未確認。

# 2026-08-02: Rig Weightミラー

- 関連: `mirrorRigWeights`
- 変更: RigWeight中に `M` を押すと、選択ボーンのウェイトを各頂点のX軸対称位置に最も近い頂点からコピーする。コピー後は全ウェイトを再正規化し、Undo可能にした。
- 制約: 左右ボーン名の対応情報がCoreにないため、現段階では同一ボーンの幾何学的ミラーとして実装している。将来の左右ボーン対応時に置換する。
- 未検証: 実行時結果、非対称メッシュ、大規模メッシュ性能、ビルドは未確認。

# 2026-08-02: ミラー先の未割当スロット対応

- 関連: `mirrorRigWeights`
- 変更: ミラー先の頂点に選択ボーンのスロットがない場合も、最小ウェイトスロットへ選択ボーンを割り当ててコピーするよう修正した。これにより片側だけに存在するウェイトも反対側へ伝播できる。
- 未検証: 非対称メッシュでの最適な対応頂点判定、実行時結果、ビルドは未確認。

# 2026-08-02: Rig Weight Flow

- 関連: RigWeightブラシ入力・HUD
- 変更: Flowパラメータを追加し、塗布量を `Opacity × Flow × 距離フォールオフ` で計算するようにした。Shift＋`[` / `]` でFlowを調整し、値はQSettingsへ保存する。
- 未検証: キー入力、設定復元、描画HUD、ビルドは未確認。

# 2026-08-02: Rigポーズの一時キャプチャ／適用

- 関連: `captureRigPose` / `applyCapturedRigPose` / `RigPoseUndoCommand`
- 変更: RigSelectまたはRigWeight中に Ctrl+Shift+C で現在のボーン・コントロール状態をキャプチャし、Ctrl+Shift+Vで選択リグへ適用できるようにした。適用はUndo/Redo可能。
- 制約: 現時点ではセッション内クリップボードであり、ポーズライブラリへの永続保存やUIパネルは未実装。
- 未検証: 実行時のポーズ適用、ブレンド、Undo/Redo、ビルドは未確認。

# 2026-08-02: Rigポーズのブレンド貼り付け

- 関連: `ArtifactCompositionEditor::keyPressEvent`
- 変更: Ctrl+Shift+B でキャプチャ済みポーズを50%ブレンド適用できるようにした。通常のCtrl+Shift+Vは100%適用のまま維持する。
- 未検証: 実行時ブレンド、Undo/Redo、ショートカット競合、ビルドは未確認。

# 2026-08-02: Rigポーズキャプチャ状態のHUD表示

- 関連: `drawViewportCanvasOverlay`
- 変更: ポーズをキャプチャ済みの場合、Rig HUDへ `POSE:READY` を表示するようにした。
- 価値: Ctrl+Shift+V / B が有効な状態を視覚的に確認でき、未キャプチャ状態での誤操作を減らせる。
- 未検証: 長いレイヤー名や高DPI時のHUD幅、実行時表示、ビルドは未確認。

# 2026-08-02: Rigポーズスロット永続化

- 関連: `saveRigPoseSlot` / `applyRigPoseSlot`
- 変更: Ctrl+Shift+1/2で現在のポーズを各スロットへ保存し、Ctrl+Shift+Alt+1/2で対応スロットを読み込み適用する。現在は1〜9スロットに対応する保存形式を用意し、ボーン変換とコントロール値をQSettingsへ格納する。
- 制約: ショートカットUIはまず1/2のみ公開。名前付きライブラリやサムネイルは次段階とする。
- 未検証: QVector2D等のコントロール値のQSettings往復、再起動後の適用、Undo/Redo、ビルドは未確認。

# 2026-08-02: ポーズスロットのPoint値を明示シリアライズ

- 関連: `rigPoseToVariantMap` / `rigPoseFromVariantMap`
- 変更: Pointコントロールの`QVector2D`をtype/x/y形式で保存し、読み込み時に明示的に再構成するようにした。Scalar値もtype/value形式へ統一した。
- 価値: QSettingsのQt型登録状態に依存せず、ポーズスロットのコントロール値を往復できる。
- 未検証: 実際の再起動後復元、Slider/Angleの型差、ビルドは未確認。

# 2026-08-02: Rigポーズスロット1〜9の入力公開

- 関連: `ArtifactCompositionEditor::keyPressEvent`
- 変更: Ctrl+Shift+1〜9で対応スロットへ保存し、Ctrl+Shift+Alt+1〜9で読み込み適用できるようにした。保存形式側の1〜9対応と入力範囲を一致させた。
- 未検証: 数字キー配列、Alt/Shift組み合わせ、再起動後の各スロット復元、ビルドは未確認。

# 2026-08-02: Rigポーズスロット全消去

- 関連: `clearRigPoseSlots`
- 変更: Ctrl+Shift+Alt+Backspaceで永続化済みのRigポーズスロット1〜9を一括削除できるようにした。
- 制約: 個別スロット削除UIは未実装。誤操作を避けるため、RigSelect / RigWeight中だけ有効にしている。
- 未検証: 実行時ショートカット、設定削除後の再起動確認、ビルドは未確認。

# 2026-08-02: ポーズ全消去時の一時キャプチャ無効化

- 関連: `clearRigPoseSlots`
- 変更: 永続スロットを全消去した際、セッション内のPoseSnapshotと`POSE:READY`状態も同時にクリアするようにした。
- 価値: 削除後にCtrl+Shift+Vで古い一時ポーズが適用される不整合を防ぐ。
- 未検証: 実行時のHUD更新、ショートカット後の適用拒否、ビルドは未確認。

# 2026-08-02: Viewport内Rig階層リスト

- 関連: `drawViewportCanvasOverlay`
- 変更: Rig overlay表示中に、右上へ読み取り専用のRig HIERARCHYリストを描画するようにした。ボーンの子階層、選択状態、コントロール一覧を表示する。
- 判断: まずDockや新規シグナルを増やさず、既存のArtifactIRenderer描画経路で階層の可視化を提供した。専用Dockパネルは別段階とする。
- 未検証: 深い階層・大量コントロール時の省略表示、高DPI、実行時描画、ビルドは未確認。

# 2026-08-02: Rig階層リストの省略表示

- 関連: Viewport内Rig HIERARCHY
- 変更: 表示行数を超えたボーン／コントロールがある場合、末尾に`… more`を表示して省略状態を明示するようにした。
- 価値: 深い階層や大量コントロールで、表示されていない項目を「存在しない」と誤認しにくくする。
- 未検証: 高DPI、長い名前との重なり、実行時描画、ビルドは未確認。

# 2026-08-02: Rig Weightミラー対応距離制限

- 関連: `mirrorRigWeights`
- 変更: X軸対称点の最近傍距離がブラシ半径の2倍を超える場合、その頂点はミラー対象から除外するようにした。
- 価値: 穴や大きな非対称を持つメッシュで、遠い無関係な頂点へウェイトが誤コピーされるのを防ぐ。
- 未検証: 非対称メッシュの実行結果、閾値のUX、ビルドは未確認。

# 2026-08-02: Ctrl+Tabリグ編集モード切替

- 関連: `ArtifactCompositionEditor::keyPressEvent`
- 変更: Ctrl+Tabで通常のSelectionとRigSelectを切り替えるようにした。RigSelect / RigWeight中はSelectionへ戻り、それ以外からはRigSelectへ入る。
- 判断: 既存のToolManager `setActiveTool` を利用し、ツールバー側の既存同期経路を再利用した。
- 未検証: OS/QtのCtrl+Tab予約キーとの競合、ツールバー同期、実行時表示、ビルドは未確認。

# 2026-08-02: RigSelectのキーボード回転

- 関連: `nudgeSelectedRigBoneRotation`
- 変更: RigSelect中に選択ボーンがある場合、`E`で+15度、`Shift+E`で-15度回転する。マウス操作と同じRigBoneTransformUndoCommandへ登録する。
- 価値: キーボードだけでポーズの微調整ができ、既存のUndo履歴と一貫する。
- 未検証: 既存Eショートカットとの競合、実行時回転、ビルドは未確認。

# 2026-08-02: RigSelectのPointコントロールキーボード移動

- 関連: `nudgeSelectedRigControl`
- 変更: RigSelect中にShift＋矢印キーで選択中のPointコントロールを1単位ずつ移動できるようにした。ControlValueUndoCommandへ登録する。
- 制約: Slider / Angleコントロールには適用せず、既存のマスク頂点移動より先にRigSelect条件で処理する。
- 未検証: 矢印キー競合、実行時移動、Undo/Redo、ビルドは未確認。

# 2026-08-02: Pointコントロール粗調整

- 関連: RigSelect Point keyboard nudge
- 変更: Shift＋矢印の1単位移動に加え、Ctrl+Shift＋矢印で10単位移動できるようにした。
- 未検証: ショートカット競合、実行時移動、Undo/Redo、ビルドは未確認。

# 2026-08-02: Rigモード終了時の選択状態クリア

- 関連: Ctrl+Tab Rigモード切替
- 変更: RigモードからSelectionへ戻る際、選択ボーン／コントロールとドラッグ状態を`clearRigSelection()`でクリアするようにした。
- 価値: 再入場時に古いRig選択が意図せず復活する状態を防ぐ。
- 未検証: 実行時切替、オーバーレイ状態、ビルドは未確認。

# 2026-08-02: Rigポーズブレンドのコントロール値対応

- 関連: `applyCapturedRigPose`
- 変更: 50%ブレンド適用時、ボーンだけでなくPointコントロールの`QVector2D`も線形補間するようにした。非数値コントロールはブレンド率50%以上で目標値へ切り替える。
- 価値: 「Pose blended 50%」の表示と実際のコントロール挙動を一致させる。
- 未検証: Slider / Angle / Pointの実行時ブレンド、Undo/Redo、ビルドは未確認。

# 2026-08-02: Rig Weight設定の起動時クランプ

- 関連: `CompositionRenderController` コンストラクタ
- 変更: QSettingsから復元した半径・Opacity・Flowにも編集時と同じ範囲制限を適用した。
- 価値: 手動編集された設定値や旧バージョンの異常値で、ブラシ描画が極端な値にならない。
- 未検証: 異常設定からの起動、ビルドは未確認。

# 2026-08-02: Rig Weightストローク補間

- 関連: RigWeight `handleMouseMove`
- 変更: 前回のマウス位置から現在位置までを最大64サンプルで補間し、ストローク上の最小距離を使ってウェイトを塗るようにした。
- 価値: 高速ドラッグ時の塗り抜けを減らし、ブラシ半径に応じた連続ストロークを実現する。
- 未検証: 大規模メッシュ・高速入力時の性能、実行時の塗布結果、ビルドは未確認。

# 2026-08-02: RigWeight時のスキン表示保証

- 関連: `drawViewportCanvasOverlay`
- 変更: RigWeightアクティブ中はLineDebugKind::RigSkinの既定設定に関係なくスキン／ウェイト表示を有効化するようにした。
- 価値: Weight Paintへ切り替えた直後にヒートマップが見えない状態を防ぐ。
- 未検証: 実行時の表示切替、手動非表示とのUX整合、ビルドは未確認。

# 2026-08-02: Rig Weightクリックダブ

- 関連: RigWeight `handleMousePress`
- 変更: メッシュ上のクリック時に初回ブラシダブを即時適用するようにした。移動イベントが発生しない単一点のウェイト編集も可能になった。
- 未検証: 高DPI座標変換、クリックとドラッグのUndo単位、実行時結果、ビルドは未確認。

# 2026-08-02: Rig Weightの選択クリックと塗布クリックを分離

- 関連: RigWeight `handleMousePress`
- 変更: ボーンまたはコントロール上のクリックはペイントを開始せず、メッシュ上のクリック／ドラッグだけがウェイト編集を開始するようにした。
- 価値: ペイント対象ボーンを選択する操作で意図せずウェイトが変わる事故を防ぐ。
- 未検証: コントロール重なり時のヒット順、実行時入力、ビルドは未確認。

# 2026-08-02: Rigコンテキストメニューの操作導線

- 関連: `ArtifactCompositionEditor::showViewportContextMenu`
- 変更: リグを持つ2Dレイヤー上の右クリックメニューに、Rig Overlay表示切替、ウェイトのNormalize/Smooth/Mirror、Pose Capture、50% Blend適用を追加した。
- 価値: 専用Dockや新規シグナルを増やさず、既存Controller APIを再利用してRig操作へ到達できる。
- 未検証: 右クリック対象レイヤーの判定、メニュー実行時のUndo・ポーズ適用、ビルドは未確認。
- 追加: Pose Slot 1への保存・50%適用・全Slot消去も同じメニューから呼び出せるようにした。

# 2026-08-02: ViewメニューからRig Overlayを切り替え

- 関連: `ArtifactViewMenu`, `CompositionRenderController`
- 変更: Viewメニューにチェック式の「リグオーバーレイを表示」を追加し、現在のComposition EditorのRig Overlay状態と同期させた。
- 価値: Rigツールを選択していない状態でも、ボーン・コントロール・スキン表示を明示的に確認できる。
- 未検証: メニュー表示時の状態同期、対象レイヤー未選択時の表示、ビルドは未確認。

# 2026-08-02: ウェイトマップ表示をViewメニューから分離制御

- 関連: `ArtifactViewMenu`, `drawViewportCanvasOverlay`
- 変更: `LineDebugKind::RigSkin`をViewメニューのチェック項目へ公開し、RigWeightツール選択時の強制表示を解除した。既定値は既存設定どおり有効。
- 価値: ウェイトマップを表示だけ切り替えたい場合や、Weight Paint中に骨格・コントロールだけ確認したい場合に対応できる。
- 未検証: 既存設定との互換性、実行時の表示切替、ビルドは未確認。

# 2026-08-02: Viewメニューの制作補助オーバーレイ拡張

- 関連: `ArtifactViewMenu`
- 変更: オニオンスキンとセーフマージンをViewメニューのチェック項目として追加し、既存Controller APIと状態同期させた。
- 価値: Composition Editorを操作中に、ツール切替なしで主要な確認用オーバーレイを管理できる。
- 未検証: オニオンスキンのキャプチャ更新、セーフマージン描画、ビルドは未確認。

# 2026-08-02: ViewメニューからRig Poseを再利用

- 関連: `ArtifactViewMenu`
- 変更: 現在のRigポーズCaptureとPose Slot 1の50%適用をViewメニューへ追加し、既存Controller APIへ接続した。
- 価値: コンテキストメニューを開かずに、ポーズの保存・ブレンド適用を実行できる。
- 未検証: Rig未選択時の無効化粒度、実行時のCapture／Apply、ビルドは未確認。

# 2026-08-02: ViewメニューのRig Pose Slot管理

- 関連: `ArtifactViewMenu`
- 変更: Pose Slot 1への保存と全Pose Slot消去を追加し、Capture／50%適用と合わせてメニューから一連の管理を可能にした。
- 価値: ポーズの一時保存・再利用・リセットを同一のViewメニューで完結できる。
- 未検証: Slotデータの永続化確認、Rig未選択時のUX、ビルドは未確認。

# 2026-08-02: Rig Viewメニューのアクセシビリティ情報

- 関連: `ArtifactViewMenu`
- 変更: Rig Overlay、ウェイトマップ、オニオンスキン、セーフマージン、Pose操作へAccessible Name/Descriptionを追加した。
- 価値: 日本語表示だけに依存せず、支援技術や自動UI操作から各操作の目的を識別できる。
- 未検証: Qtアクセシビリティツリー上の読み上げ結果、ビルドは未確認。

# 2026-08-02: EraserのLast Stroke Only操作

- 関連: `ArtifactCompositionEditor::keyPressEvent`, `ArtifactPaintLayer::undoLastStroke`
- 変更: Eraserツール中の`Ctrl+Alt+Z`で、選択中Paintレイヤーの直前ストロークだけを戻す操作を追加した。変更イベントと再描画も既存経路へ通知する。
- 価値: 消し過ぎずに直前の消去／描画だけを取り消す、仕様のLast Stroke Only操作をキーボードから利用できる。
- 未検証: UndoManagerとの履歴順序、空履歴時の挙動、実行時入力、ビルドは未確認。

# 2026-08-02: EraserのLayer Eraser導線

- 関連: `CompositionViewport::showViewportContextMenu`
- 変更: Eraserツール中にPaintレイヤーを右クリックすると、確認付きで全フレームを消去できる項目を追加した。既存`clearAllFrames()`、LayerChanged通知、再描画を利用する。
- 価値: ストローク単位の消去とレイヤー全体消去を明確に分け、破壊的操作には確認を挟める。
- 未検証: 確認ダイアログの実行時表示、保存状態、ビルドは未確認。

# 2026-08-02: Layerメニューから初期Rigレイヤーを生成

- 関連: `ArtifactLayerMenu::handleCreateRig`
- 変更: コンポジションサイズのSolid 2Dレイヤーを作成し、`root`ボーンと`root_ctrl` Pointコントロールを初期化する「リグレイヤー」項目を追加した。
- 価値: Rig編集を開始するための空レイヤー準備を短縮し、既存の`ArtifactAbstract2DLayer::rig2D()`所有モデルをそのまま利用できる。
- 未検証: 作成直後の選択同期、root位置／描画、保存・Undo、ビルドは未確認。
- 追加: rootをコンポジション中央へ配置し、階層更新後に`LayerChangedEvent::Modified`を発行するよう補強した。
- 追加: 作成完了後は既存ToolManagerをRigSelectへ切り替え、生成直後からボーン／コントロールを編集できる状態にした。
- 追加: 編集用SolidのOpacityを0.18に設定し、Rig下地がオーバーレイを覆わないようにした。

# 2026-08-02: RigSelectからBキーでWeight Paintへ移行

- 関連: `ArtifactCompositionEditor::keyPressEvent`
- 変更: `RigSelect`中の単独`B`キーで既存`ToolType::RigWeight`へ切り替えるショートカットを追加した。MotionSketch中の既存Bキー挙動は維持。
- 価値: Rig作業中の選択・ウェイト編集の切り替えをキーボードだけで行える。
- 未検証: Rigレイヤー未選択時の挙動、実行時ツール切替、ビルドは未確認。

# 2026-08-02: Compositionメニューからレンダーキューへ追加

- 関連: `ArtifactCompositionMenu::addCurrentToRenderQueue`
- 変更: 現在のコンポジションを既存`ArtifactRenderQueueService::addRenderQueueForComposition`へ送るメニュー項目を追加した。
- 価値: Compositionメニューからレンダーキュー登録までの導線を補完し、仕様監査の「レンダーキューに追加」を実UIへ接続した。
- 未検証: Queue登録後のUI更新、重複ジョブ方針、ビルドは未確認。

# 2026-08-02: 選択的レンダー範囲のCompositionメニュー導線

- 関連: `ArtifactCompositionMenu`
- 変更: 「現在フレーム」「ワークエリア」をそれぞれRender Queueへ追加し、追加直後のジョブへ`frameRangeMode`とフレーム範囲を設定するようにした。
- 価値: Render Queue Managerを開いてから範囲を手入力する手順を短縮し、既存のSelective Settings形式と整合させた。
- 未検証: ワークエリア終端の包含規約、Queue UIの再読込、ビルドは未確認。

# 2026-08-02: Renderメニューから一時停止

- 関連: `ArtifactRenderMenu::pauseRender`
- 変更: 既存`ArtifactRenderQueueService::pauseAllJobs`を呼ぶ「レンダリングを一時停止」項目を追加し、キューにジョブがある場合だけ有効化した。
- 価値: Render Queue Managerを開かずに全ジョブを停止できる。
- 未検証: 再開操作とのUI関係、実行中ジョブの状態遷移、ビルドは未確認。
- 追加: 開始アクションの表示を「開始／再開」に統一し、一時停止後も同じ既存開始経路で復帰できることをUI上で明示した。

# 2026-08-02: Renderメニューの全ジョブキャンセル

- 関連: `ArtifactRenderMenu::cancelRender`
- 変更: 既存`ArtifactRenderQueueService::cancelAllJobs`を呼ぶ「全ジョブをキャンセル」を追加し、ジョブが存在するときだけ有効化した。
- 価値: Queue Managerを開かずに実行中・保留中ジョブをまとめて停止できる。
- 未検証: キャンセル後の履歴表示、確認ダイアログの要否、ビルドは未確認。

# 2026-08-02: CompositionメニューのVariant依存を明示化

- 関連: `ArtifactCompositionMenu.cppm`
- 変更: 選択的レンダー設定で直接利用する`QVariantMap`をグローバルモジュールフラグメントから明示includeした。
- 価値: Qt型の間接include依存を減らし、C++20 modulesの実装単位を自己完結させる。
- 未検証: モジュール全体のビルド、依存スキャンは未確認。

# 2026-08-02: Work Area APIの型照合

- 関連: `ArtifactCompositionMenu::addWorkAreaToRenderQueue`
- 変更: `ArtifactAbstractComposition::workAreaRange()`の戻り値に合わせ、範囲取得を`start()`／`end()`へ統一した。
- 価値: Fileメニューの既存利用箇所と同じAPI契約に揃え、フィールドアクセスによるコンパイル不整合を回避する。
- 未検証: ビルドは未確認。
# 2026-08-02: 選択レイヤーのみのレンダーキュー登録

- 関連: `ArtifactCompositionMenu::addSelectedLayersToRenderQueue`
- 変更: 選択中レイヤーのIDを`layerWhitelist`へ格納し、既存Selective Settingsの`layerFilterMode=4`でキュー登録する項目を追加した。
- 価値: レンダーキュー管理画面を開いてから手動でレイヤーを指定せず、現在の選択をそのまま限定レンダーへ送れる。
- 未検証: 選択順・レイヤー削除後のWhitelist、実行時出力、ビルドは未確認。

# 2026-08-02: 選択レイヤーレンダーの範囲バリエーション

- 関連: `ArtifactCompositionMenu::addSelectedLayersToRenderQueue`
- 変更: 選択レイヤー限定キュー登録を、全範囲・ワークエリア・現在フレームの3モードへ拡張した。
- 価値: レイヤー選択とフレーム範囲を同時に指定でき、仕様の「選択範囲をレンダーキューに追加」に近い操作導線になった。
- 未検証: 終端フレームの包含規約、選択状態変化後のWhitelist、ビルドは未確認。

# 2026-08-02: Viewport右クリックから単一レイヤーをキュー登録

- 関連: `CompositionViewport::showViewportContextMenu`
- 変更: 右クリック対象レイヤーを単一WhitelistとしてRender Queueへ追加する項目を実装した。
- 価値: レイヤーパネルで選択状態を変更せず、Viewport上で対象を確認したまま限定レンダーを作成できる。
- 未検証: 右クリック対象と現在選択の差異、キュー登録後のSelective Settings表示、ビルドは未確認。
- 追加: 同じ単一レイヤーWhitelistを全範囲・現在フレーム・ワークエリアの3範囲で登録できるよう共通化した。

# 2026-08-02: Compositionレンダー登録項目のアクセシビリティ補完

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`
- 変更: 全コンポジション・現在フレーム・ワークエリアのキュー登録アクションにAccessible Name/Descriptionを追加した。
- 価値: 選択レイヤー用項目と同じ読み上げ情報を持たせ、メニュー操作の意味と対象範囲を支援技術から判別できる。
- 未検証: 実機の読み上げ結果、ビルドは未確認。

# 2026-08-02: Textフォント選択のリアルタイムプレビュー

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: フォント選択コンボボックスのイベントを既存イベントフィルタで監視し、プレビューを再描画するようにした。描画時には選択中のフォントファミリーを一時QFontへ反映する。
- 価値: ダイアログを確定する前にフォント変更の見た目を確認できる。
- 未検証: フォントフォールバック時の表示、各OSのフォント一覧更新、ビルドは未確認。
- 追加: サイズ、Bold、Italicの変更も同じイベントフィルタでプレビューへ反映するようにした。
- 追加: All Caps、Underline、Alignmentもプレビューへ反映し、入力コントロール変更時に再描画する対象へ追加した。
- 追加: TrackingとStretchもプレビュー用QFontへ反映し、文字間隔・横幅変更を確定前に確認できるようにした。
- 追加: Stroke／Stroke WidthとShadow／Blurもプレビュー描画へ反映し、関連コントロール変更時に即時更新するようにした。
- 追加: TextLayerのText／Stroke／Shadow Colorをプレビューへ反映し、固定色との差異をなくした。
- 追加: 固定サンプル文ではなく現在のTextLayer内容をプレビューし、長文は180文字で省略するようにした。

# 2026-08-02: CompositionからAdvanced Render Managerへ直接到達

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`
- 変更: Compositionメニューに「高度なレンダー設定を開く…」を追加し、既存の`ArtifactRenderCenterWindow`を再利用してRender Managerを表示するようにした。
- 価値: 仕様書のAdvanced設定（フレーム範囲、レイヤーフィルター、ROI、出力設定）へ、レンダーキュー登録後だけでなくComposition画面から直接到達できる。
- 未検証: 既存ウィンドウ再利用時の親子階層、実機表示、ビルドは未確認。
- 追加: Advanced項目を開く前に現在コンポジションを既定設定でキューへ追加し、登録済みジョブをRender Managerで調整できる導線にした。

# 2026-08-02: 3Dレイヤーのコンテキストメニューリセット

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: 選択中3Dレイヤーの右クリックメニューに`Reset 3D Transform`を追加し、既存のUndo対応コントローラ処理を呼び出すようにした。
- 価値: ダブルクリック操作を知らない利用者でも、位置・回転・スケールの初期化へ明示的に到達できる。
- 未検証: 右クリック対象と選択レイヤーが異なる場合の表示条件、実機操作、ビルドは未確認。

# 2026-08-02: 3D Transformの数値入力

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: 選択中3Dレイヤーの右クリックメニューから位置XYZ・回転XYZ・スケールXYを順番に入力し、既存の`GizmoTransformUndoCommand`で一括適用できるようにした。スケールは最小値0.001でクランプする。
- 価値: 3Dフレーム仕様の「数値入力」へ到達する実装導線を追加し、ドラッグ操作だけに依存せず正確な変形値を設定できる。
- 未検証: モジュール再スキャン後のビルド、入力途中キャンセル時のUX、キーフレーム時の値適用は未確認。

# 2026-08-02: 3D Transformのコピー／ペースト

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: 右クリックメニューから3Dレイヤーの位置・回転・スケールを`QSettings`の一時クリップボードへ保存し、別レイヤーへ既存の一括適用APIでペーストできるようにした。値がない場合はペースト項目を無効化する。
- 価値: 数値入力を繰り返さず、複数3Dレイヤーへ同じ姿勢・配置を再利用できる。
- 未検証: アプリ再起動後の設定残存、異なるコンポジション間の値適用、実機操作、ビルドは未確認。

# 2026-08-02: 3D Transformクリップボードの明示消去

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: 右クリックメニューに保存済み3D変形値の消去項目を追加し、値が存在する場合のみ有効化した。
- 価値: `QSettings`に残った古い変形値を意図せず再利用する事故を防げる。
- 未検証: 実機メニュー表示、ビルドは未確認。

# 2026-08-02: 3D Transform貼り付け値の有限値検証

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: `QSettings`から読み込んだ3D変形値を適用する前に、位置・回転・スケール全成分が有限値か検証するようにした。
- 価値: 設定破損や手動編集によるNaN／無限大がレンダー・Undo経路へ流入するのを防ぐ。
- 未検証: 異常値設定の実機挙動、ビルドは未確認。

# 2026-08-02: Text Animatorプリセット選択UI

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: テキスト編集ダイアログにText Animatorのプリセット選択を追加し、Typewriter／Slide Up／Scale In／Rotation In／Tracking Fade／Wiggly Position／Blur Reveal／Noneを既存の`text.animatorPreset`プロパティ経由で適用できるようにした。Keep currentでは既存値を維持する。
- 価値: Core側に存在するRange／Wiggly Selector実装へ、テキスト編集画面から直接到達できる。
- 未検証: プリセット適用後のプレビュー、Animator数との同時指定、ビルドは未確認。

# 2026-08-02: Text Animator操作のアクセシビリティ情報

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 変更: Animator数とプリセット選択にAccessible Name/Descriptionを追加した。
- 価値: 読み上げ環境でも、数値がAnimatorスタック数であることと、プリセット適用タイミングを判別できる。
- 未検証: 実機の読み上げ結果、ビルドは未確認。

# 2026-08-02: OCIO表示変換はcache hit経路もpost-process対象

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Color/ArtifactOCIOManager.cppm`, `docs/memo/OCIO_MISSING_FEATURES_2026-08-01.md`
- 事実: composition cacheを再利用する分岐は、cache生成分岐と別に表示処理を持つ。表示変換を生成時だけに置くと、cache hit時に未変換SRVを直接表示する経路が残るため、両分岐を同じpost-process順序に揃えた。
- 価値: キャッシュ有無による色味の不一致と、config解除後に前回LUTが残る表示状態を防げる。
- 未検証: cache hit/miss切替、config解除、Exposure/Gamma変更を含む実機表示とビルドは未確認。

# 2026-08-02: OCIO GPU経路はnative shaderとLUT bakeを分離

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`, `Artifact/src/Render/ArtifactFinalPostProcess.cppm`
- 事実: OCIO GPU shader descriptor/resource metadataの取得APIを追加する一方、実表示経路はOCIO CPU Processorからベイクした3D LUTを既存のDiligent LUT compute passへ渡している。
- 仮説: 既存GPU passを再利用するLUT bake経路は接続範囲を限定できるが、HDRの広い入力範囲や動的uniformを完全に扱うにはnative GPU shader bindingが必要。
- 次に確認: LUT domain設計、HDR値の保持、OCIO GPU shaderのHLSL wrapperと1D/3D resource bindingをruntimeで比較する。

# 2026-08-02: キーイング拡張は既存EffectServiceへ閉じ込められる

- 関連: `Artifact/include/Effects/Keying/LumaKeyEffect.ixx`, `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`, `Artifact/include/Effects/Keying/DifferenceKeyEffect.ixx`, `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`
- 事実: Chroma Keyと同じ`ArtifactAbstractEffect`／CPU実装経路を使うことで、Luma KeyとDifference KeyをInspectorカタログ・EffectServiceへ追加できた。新規シグナルやQt合成は不要だった。
- 価値: キーイング機能の不足を、Diligent低レベル経路やReactiveEventsへ波及させず補完できる。
- 未検証: モジュールビルド、GPU経路との併用、Property Editorでの色／閾値編集、実画像のマット品質。
- 次に確認: 代表的な緑幕・明暗分離・参照色差分画像でCPU結果を比較し、必要なら既存GPUキーイング経路へ段階的に移す。

# 2026-08-02: Composition Graphは既存Widgetの登録だけで導入可能

- 関連: `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`, `Artifact/src/AppMain.cppm`
- 事実: Composition Graph Widgetは既にレイヤー検索、親子リンク、ダブルクリック選択、EventBus更新を持っていた。AppMainのProjectタブ群へ登録するだけで、既存のUI責務を保ったまま利用可能になった。
- 価値: ノードグラフ比較の最初の段階を、新しい中央イベント配線や描画基盤なしで実現できる。
- 仮説: 現状はレイヤー関係グラフであり、Nuke/Houdini相当のエフェクト接続グラフとは責務が異なる。エフェクトグラフ化は別設計として切り分けるべき。
- 未検証: 起動時レイアウト、タブ復元、プロジェクト切替時の表示更新、ビルド。

# 2026-08-02: Composition Graphのレイヤー操作はProjectServiceへ委譲する

- 関連: `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`
- 事実: 選択、削除、複製、名前変更、Visible/Locked/Solo/Shy、親設定、親解除、スタック順変更を、既存`ArtifactProjectService` APIへ委譲してグラフのコンテキストメニューへ追加した。
- 価値: Graph WidgetがレイヤーデータやUndo・選択同期を直接所有せず、既存のサービス責務とEventBus更新を維持できる。
- 仮説: Nuke/Houdini相当のエフェクト接続グラフを追加する場合も、同じWidgetへ直接ロジックを詰め込まず、EffectService／Graphモデルの専用境界を先に設けるべき。
- 未検証: 実機での各操作、Undo履歴、循環親設定の拒否表示、ビルド。

# 2026-08-02: Composition Graphのノード配置はUI設定として保存する

- 関連: `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`
- 事実: レイヤーノードの移動を`QSettings`へ保存し、Graph再構築時にLayer ID単位で位置を復元するようにした。エフェクトノードはレイヤーノード位置から再配置される。
- 価値: EventBus更新やProject保存形式を変更せず、Composition Graphの作業レイアウトだけを維持できる。
- 仮説: 将来のGraph専用保存を導入する場合は、UI配置とEffect接続／評価データを別キー・別バージョンで管理する必要がある。
- 未検証: 実機ドラッグ後の再起動復元、Layer ID再生成時の古い設定、設定削除／レイアウトリセット、ビルド。

# 2026-08-02: DAGのステージ数とGraphのイベント経路は単一の契約にする

- 関連: `Artifact/include/Effects/ArtifactAbstractEffect.ixx`, `Artifact/include/Engine/DAG/Node.ixx`, `Artifact/include/Engine/DAG/LayerGraphBuilder.ixx`, `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm`
- 事実: `EffectPipelineStage` は PreProcess を含む6段階であり、DAGの配列・ループ・UI色分けを同じ段階数へ揃えた。Graphの子アイテムはViewのカスタムコンテキストメニューで直接検出できないため、親アイテム解決と共通メニュー入口が必要だった。
- 価値: LayerTransformの接続漏れと、ラベル上の右クリックが編集メニューを失う不整合を防げる。
- 未検証: モジュールビルド、全ステージのポート互換性、実機での子アイテム右クリックとDAG評価。
- 次に確認: Generator／Geometry／Materialのポート契約を実評価へ広げる前に、各ステージの入力・出力型と実装済みエフェクトを対応表にする。

# 2026-08-02: 既存の歪みエフェクトは実装済みでもInspectorカタログが別管理になる

- 関連: `Artifact/src/Service/ArtifactEffectService.cppm`, `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- 事実: `Optics Compensation`、`Turbulent Displace`、`Liquify` はEffectServiceの生成・一覧に存在する一方、Inspectorの検索カタログには一部が登録されていなかった。
- 価値: 既存機能を再実装せず、検索・カテゴリ経由の発見性だけを補完できる。
- 仮説: エフェクト追加時はServiceの生成一覧とInspectorカタログを同じ変更単位で確認するチェックが有効。
- 未検証: 起動後の検索結果、追加操作、プロパティ編集、ビルド。

# 2026-08-02: TrackPointは実装済みでもメインツールバーの導線が欠ける

- 関連: `Artifact/include/Tool/ArtifactToolManager.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: `ToolType::TrackPoint`、Tracker Gizmo、前後／全フレーム追跡、位置・アンカー適用は存在していたが、メインツールバーのアクションとツール名表示が登録されていなかった。
- 価値: 既存のトラッキング実装へ、ツールバーから到達できる導線を追加できる。
- 仮説: ToolType追加時はツールバー、オーバーレイ表示、ショートカットの3箇所を同時に確認する必要がある。
- 未検証: 実機でのアクション表示、Gizmo選択、追跡開始、ビルド。

# 2026-08-02: TrackPointの導線は表示名レジストリも揃える必要がある

- 関連: `Artifact/src/Tool/ArtifactToolManager.cppm`
- 事実: ツールバーから`ToolType::TrackPoint`を選択できても、`ArtifactToolManager::toolName()`に対応ケースがなく、サービス経由の名称が`Unknown`になっていた。
- 価値: UI表示とサービス／自動化側のツール名称を一致させられる。
- 未検証: ツールチップ、サービス名称取得、ビルド。

# 2026-08-02: Viewerツールオーバーレイの表示名はToolType追加時に拡張が必要

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: Brush、Clone、Eraser、ScrubPreviewは入力処理とツールバー導線が存在したが、オーバーレイ名のswitchにケースがなかった。
- 価値: 選択中ツールの状態をViewer上で一貫して確認できる。
- 未検証: 実機での各ツール切替表示、ビルド。

# 2026-08-02: Motion Tracker作成後はTrackPointを自動選択すると導線が連続する

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- 事実: レイヤーメニューからトラッカーを作成しても、作成完了後のアクティブツールは変更されていなかった。
- 価値: 動画レイヤー選択→トラッカー作成→ポイント配置の操作を途切れずに進められる。
- 未検証: 実機での選択状態、既存トラッカー時の挙動、ビルド。

# 2026-08-02: Composition Editorのツール表示もToolType追加時の同期対象

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: Viewerオーバーレイとツールバーで表示可能なTrackPoint等が、Composition Editorのモードボタンでは汎用の`Tool`表示になっていた。
- 価値: エディタ上部のモード表示と実際のアクティブツールを一致させられる。
- 未検証: 実機での各ツール切替、ビルド。

# 2026-08-02: リグ系ツールもComposition Editorのモード表示へ反映する

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: RigSelect、RigWeight、Puppet、MotionSketchはToolTypeと入力処理が存在する一方、Composition Editorのモードボタンでは汎用表示にフォールバックしていた。
- 価値: リグ編集ツールの選択状態をエディタ上部で識別できる。
- 未検証: 実機での表示、ビルド。

# 2026-08-02: Render QueueのFarm設定はサービスにあるため状態表示を再利用できる

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`, `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- 事実: RenderQueueServiceはFarm有効状態とワーカー数を公開していたが、Queue Managerのサマリーには表示されていなかった。
- 価値: ネットワーク／Farmレンダーの設定状態を、キュー画面を離れず確認できる。
- 未検証: Farm有効時の表示、ワーカー数変更後の更新、ビルド。

# 2026-08-02: Render FarmのRPC稼働状態も既存Service APIから表示できる

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`, `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- 事実: Queue Serviceは`isFarmRpcServerRunning()`を公開していたが、Queue Managerの状態表示ではワーカー数のみだった。
- 価値: Farmが有効でもRPC待受が停止している状態を、キュー画面で区別できる。
- 未検証: RPC起動／停止時の表示更新、ビルド。

# 2026-08-02: EffectServiceとInspectorカタログの差分監査で12件の登録漏れを発見

- 関連: `Artifact/src/Service/ArtifactEffectService.cppm`, `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- 事実: Serviceの利用可能エフェクト一覧に存在するStroke、Inner Shadow、Bevel、Satin、Rim Light、Radial Blur、Linear Wipe、Kaleidoscope、Dithering、Kuwahara、Radio Waves、GrayscaleがInspectorカタログから漏れていた。
- 価値: 既存エフェクトを再実装せず、検索・カテゴリ・追加UIから発見できるようにした。
- 未検証: 各項目の検索結果、追加操作、プロパティ表示、ビルド。

# 2026-08-02: 比較表の段落・ステンシル評価を現行コードへ同期

- 関連: `docs/analysis/CROSS_APP_COMPARISON_2026-08-01.md`, `Artifact/src/Layer/ArtifactTextLayer.cppm`, `ArtifactCore/include/Layer/BlendModeInfo.ixx`
- 事実: TextLayerは水平／垂直揃え、Wrap、Leading、Paragraph Spacingを保存・Property Group・描画へ反映し、BlendModeはStencil Alpha/LumaとSilhouette Alpha/LumaをCPU/GPU経路へ持っていた。
- 価値: 未着手扱いの比較表が実装状況を過小評価しないようにした。
- 未検証: 実機での各UI操作とビルド。

# 2026-08-02: 編集・変形ToolTypeもComposition Editorの表示マッピングが必要

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: Move、Scale、Rotation、Zoom、Ripple、Rolling、Slip、Slideはツールとして定義されていたが、モードボタンの表示switchに未登録だった。
- 価値: タイムライン編集系とビューポート変形系の現在状態を同じUIで確認できる。
- 未検証: 実機での表示、ビルド。

# 2026-08-02: 比較表の2Dポイントトラッカー評価を現行実装へ同期

- 関連: `docs/analysis/CROSS_APP_COMPARISON_2026-08-01.md`, `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: 2DポイントトラッカーはNCCベースの前後追跡、TrackPointツール表示、追跡結果の適用導線まで実装済みであり、比較表の評価を★★★★☆へ更新した。総合トラッキング評価も★★☆☆☆から★★★☆☆へ更新した。
- 価値: 実装済みのUI・追跡操作を比較表上で未実装扱いしない。
- 未検証: 実機での追跡精度、結果適用、ビルド。

# 2026-08-02: Stabilizerの特徴点応答式を修正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: 旧式は水平・垂直勾配の二乗和だけを使い、`dx * dy - (dx + dy)^2` を判定していたため、正のコーナー応答を得られなかった。局所輝度勾配の構造テンソルからHarris応答を計算する形へ置き換えた。
- 価値: 既存Stabilizerの特徴点検出が実際に候補点を返せる状態になる。
- 未検証: 実映像での検出数、追跡精度、ビルド。

# 2026-08-02: Stabilizerの追跡途中トラック無効化を修正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: フレーム総数を保持する`processedFrames_`と、追跡途中の位置数を比較していたため、全フレーム処理前にトラックが無効化されていた。追跡完了後に全フレーム分の位置が揃ったトラックだけを有効化するよう変更した。
- 価値: 修正した特徴点検出結果がモーション推定まで到達できる。
- 未検証: 実映像での連続追跡、モーション推定、ビルド。

# 2026-08-02: Stabilizerのモーション推定を平行移動だけからSimilarity変換へ拡張

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`, `ArtifactCore/include/Video/Stabilizer.ixx`
- 事実: 旧実装は特徴点の平均移動量だけを使い、`rotation` と `scale` を常に初期値のまま返していた。中心化した点対から回転・スケール・平行移動を推定し、既存パラメータの有効／無効設定を反映するようにした。
- 価値: 回転や拡大縮小を含むカメラ揺れを、既存のフレーム処理経路へ渡せる。
- 未検証: 実映像での推定方向、境界補間、ビルド。

# 2026-08-02: Stabilizer再実行時の平滑化結果蓄積を防止

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: `stabilize()`／`smoothMotions()`の再実行時に`smoothedMotions_`を初期化していなかったため、前回結果が後ろに蓄積していた。
- 価値: 同じ入力を再処理した際に、フレーム番号と平滑化結果の対応が崩れない。
- 未検証: 再実行操作、ビルド。

# 2026-08-02: Stabilizer特徴点検出でmaxFeatures/minDistanceを適用

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`, `ArtifactCore/include/Video/Stabilizer.ixx`
- 事実: 検出候補を応答値でソートし、`FeatureDetectionParams.maxFeatures` と `minDistance` による非最大抑制を行うようにした。従来は候補点数と密集度が設定値に制限されていなかった。
- 価値: 特徴点追跡の計算量と点分布を既存パラメータから制御できる。
- 未検証: 実画像での検出数、追跡速度、ビルド。

# 2026-08-02: Stabilizer追跡結果の座標参照先を修正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: マッチング処理は最良座標を検出候補配列の末尾へ追加していたが、トラック更新側は候補配列の先頭を参照していた。追加されたマッチ結果領域を使うように修正した。
- 価値: モーション推定へ渡る各トラックが、実際にマッチしたフレーム位置を保持する。
- 未検証: 実映像での追跡精度、ビルド。

# 2026-08-02: Stabilizerの成功判定を実推定結果に同期

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: 特徴点やフレームモーションが空でも`stabilize()`が完了扱いになり得た。特徴点・モーションが存在しない場合は失敗を返し、`totalFeatures_`も有効トラック数へ更新するようにした。
- 価値: 呼び出し側が空の安定化結果を成功と誤認しない。
- 未検証: 特徴点不足時のUI表示、実映像、ビルド。

# 2026-08-02: AutoMosaicの画像端領域クリップを修正

- 関連: `Artifact/src/Effects/AutoMosaicEffect.cppm`
- 事実: 画像外へはみ出す手動／顔検出領域を、x/yを0へ丸めるだけで幅・高さを計算していたため、負座標領域の右端・下端が過剰に処理され得た。QRectの画像矩形との交差領域をOpenCV ROIへ渡すようにした。
- 価値: 画像端のモザイク領域が指定範囲どおりに適用され、ROI範囲外アクセスのリスクを下げる。
- 未検証: 画像端・負座標の手動領域、ビルド。

# 2026-08-02: Linear WipeのSoftnessをCPU/GPU経路へ反映

- 関連: `Artifact/src/Effects/LinearWipe/LinearWipeEffect.cppm`
- 事実: `Softness`プロパティは保持・公開されていたが、CPU実装は固定の境界式、GPU実装はangleとfeatherのみを使用していた。両経路でSoftnessを境界遷移幅として使うようにした。
- 価値: CPUフォールバックとGPU処理でワイプ境界の調整結果が一致し、既存プロパティが実際に機能する。
- 未検証: CPU/GPU境界一致、実機表示、ビルド。

# 2026-08-02: MosaicのShape Mode平均領域をCPU/GPUで同期

- 関連: `Artifact/src/Effects/Mosaic/MosaicEffect.cppm`
- 事実: CPUはセル内のダイヤモンド領域だけを平均していたが、GPUはセル全体の平均色をダイヤモンドへ適用していた。GPUも同じダイヤモンド領域を平均対象にした。
- 価値: Compute経路とCPUフォールバックでShape Modeの見た目が一致する。
- 未検証: CPU/GPU比較、実機表示、ビルド。

# 2026-08-02: Stroke GPU経路の色チャンネル順を修正

- 関連: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`
- 事実: GPU経路はRGBAテクスチャへ直接出力する一方、パラメータ色をBGR順で渡していた。GPU用のストローク色をR/G/B/A順に修正した。
- 価値: GPU処理で赤・青が入れ替わる問題を防ぎ、表示色を指定色に一致させる。
- 未検証: CPU/GPU色比較、実機表示、ビルド。

# 2026-08-02: Bevel/Satin GPU経路の色チャンネル順を修正

- 関連: `Artifact/src/Effects/Bevel/BevelEffect.cppm`, `Artifact/src/Effects/Satin/SatinEffect.cppm`
- 事実: GPUのRGBAテクスチャへ渡すハイライト／シャドウ／サテン色がBGR順だった。GPUパラメータをR/G/B/A順へ揃えた。DropShadow等の既にRGBA順だった経路は変更していない。
- 価値: BevelとSatinでも指定色とGPU出力色が一致する。
- 未検証: CPU/GPU色比較、実機表示、ビルド。

# 2026-08-02: Stabilizerの退化スケールを下限保護

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: 対応点が同一点へ潰れるケースではSimilarity推定のスケールが0になり、既存の逆変換でゼロ除算し得た。推定スケールを正の下限でクランプした。
- 価値: 退化した追跡結果でも処理経路をNaN／無限大へ進めない。
- 未検証: 退化入力、実映像、ビルド。

# 2026-08-02: Drop Shadow GPUぼかし半径をSoftnessへ同期

- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`
- 事実: CPU経路は`ceil(softness * 2.5)`相当のカーネルを使う一方、GPU経路は常に±4ピクセルの9×9サンプルだった。GPUもSoftnessから最大サンプル半径を計算するようにした。
- 価値: 大きなSoftness設定がGPU経路でも実際のぼかし範囲へ反映される。
- 未検証: GPU性能、CPU/GPU見た目比較、ビルド。

# 2026-08-02: AutoMosaicの重複領域を統合

- 関連: `Artifact/src/Effects/AutoMosaicEffect.cppm`
- 事実: 顔検出領域と手動領域、または複数の検出領域が重なる場合に同じ画素へモザイク処理を複数回適用していた。画像内へクリップした領域を重なり／隣接単位で統合してから一度だけ処理するようにした。
- 価値: 重複領域の過剰処理を避け、処理量と結果の不安定さを抑える。
- 未検証: 複数顔・手動領域の重なり、ビルド。

# 2026-08-02: AutoMosaic領域統合を推移的に完了

- 関連: `Artifact/src/Effects/AutoMosaicEffect.cppm`
- 事実: 領域追加時の一段階統合だけでは、第三領域が複数の既存領域を橋渡しするケースで重なりが残り得た。統合後も重なり／隣接がなくなるまで再走査するようにした。
- 価値: 顔検出と手動領域が複雑に連結する場合も一つの処理領域へまとめられる。
- 未検証: 複雑な重なりパターン、ビルド。

# 2026-08-02: PuppetToolのPinRecordへweightフィールドを追加

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: `pinWeight()`、`setPinWeight()`、エンジンへのピン変換、表示処理が`PinRecord::weight`を参照していたが、レコード定義にフィールドが存在しなかった。
- 価値: Puppet Toolの既存weight APIが型定義と一致し、コンパイル可能なデータモデルになる。
- 未検証: ビルド、weight変更の実機反映。

# 2026-08-02: PuppetToolのピン削除をエンジンへ同期

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: `removePin()`はUI側の`LayerPins::pins`だけを削除し、OpenCV Puppet Engine内のピンを残していた。削除時にエンジンをリセットし、次回変形で残存ピンを再登録しないようにした。
- 価値: 削除済みピンが変形結果へ影響し続ける問題を防ぐ。
- 未検証: ピン削除後の再変形、ビルド。

# 2026-08-02: PuppetToolの再バインド失敗を再試行可能に修正

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: `toQImage()`またはRGBA変換が失敗しても`needsRebind`を解除していたため、以後の変形で再バインドを試行しなかった。画像バインド成功時だけフラグを解除するようにした。
- 価値: 一時的な画像取得失敗から、次回変形で自動復旧できる。
- 未検証: 画像取得失敗／復旧、ビルド。

# 2026-08-02: MotionSketchのUndoフレームレートをコンポジションへ同期

- 関連: `Artifact/src/Tool/ArtifactMotionSketchTool.cppm`
- 事実: Undoスナップショットのフレーム番号と復元用`RationalTime`が常に24fpsで処理されていた。コンポジションの実fpsで保存・復元するようにした。
- 価値: 24fps以外のコンポジションでMotion SketchをUndo/Redoしても、キーフレーム位置がずれない。
- 未検証: 30/60fpsでのUndo/Redo、ビルド。

# 2026-08-02: Command PaletteのAdd Maskスタブを実装

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: `palette.dummy.addMask`は実行ログを出すだけだった。選択中レイヤーのsourceSizeに合わせた矩形MaskPathを作成し、LayerMaskを追加して既存Undo変更通知を呼ぶ処理へ置き換えた。
- 価値: コマンドパレットから選択レイヤーへフルソース矩形マスクを追加できる。
- 未検証: UI実行、Undo復元、ビルド。

# 2026-08-02: Command PaletteのAdd MaskへUndo復元を追加

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: 追加処理を専用`UndoCommand`へ移し、既存マスク一覧をbefore/afterとして保存してUndo/Redoできるようにした。UndoManagerがない場合は直接適用する。
- 価値: コマンドパレット操作が既存のマスク編集履歴と同じく可逆になる。
- 未検証: UI実行、Undo/Redo、ビルド。

# 2026-08-02: Command PaletteのAdd MaskコマンドIDを正式化

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: 実処理化後も`palette.dummy.addMask`という仮IDを使っていたため、`palette.layer.addMask`へ整理した。
- 価値: コマンド一覧・ログ・将来のショートカット連携で、実機能として識別できる。
- 未検証: 既存設定からのID移行、ビルド。

# 2026-08-02: Command PaletteのAboutダミーを情報ダイアログ化

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`
- 事実: `About Command Palette`は実行ログだけを出すダミーだった。パレットの用途を説明する情報ダイアログを表示する正式コマンドへ変更した。
- 価値: パレット自身の機能説明へユーザーが到達できる。
- 未検証: UI表示、ビルド。

# 2026-08-02: Point Tracker結果適用後のTransform通知を追加

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: 追跡キーフレームを書き込んだ後に`setDirty(LayerDirtyFlag::Transform)`と`changed()`を呼んでいなかった。Nullレイヤー生成時も選択レイヤー書き出し時も、適用後にTransform変更を通知するようにした。
- 価値: UI再描画・保存判定など既存の変更監視経路へ追跡結果が届く。
- 未検証: 適用後の表示・保存、ビルド。

# 2026-08-02: Point Tracker適用時の非有限値を遮断

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`, `ArtifactCore/src/Tracking/MotionTracker.cppm`
- 事実: Coreのexport結果を変更せず、Artifact側で時間・X/Yが有限値のキーフレームだけを採用するようにした。
- 価値: 追跡失敗や壊れた入力がNaN／無限大のTransformキーフレームとして保存されるのを防ぐ。
- 未検証: 不正結果の適用、ビルド。

# 2026-08-02: ImageLayerのtoQImage連番フレーム更新を追加

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 連番画像のキャッシュ更新は`draw()`経路に限られており、`toQImage()`を直接呼ぶ処理では前フレームの画像を返し得た。現在フレームと連番fpsから対象フレームを解決し、`toQImage()`でも更新するようにした。
- 価値: 連番画像を使うサムネイル・編集ツール・変形処理が現在フレームを参照する。
- 未検証: 連番サムネイル、Puppet／変形経路、ビルド。

# 2026-08-02: ImageLayerのcurrentFrameBuffer連番同期を追加

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: GPU／エフェクト経路が`currentFrameBuffer()`を直接参照すると、`draw()`や`toQImage()`を経由せず前フレームの連番バッファを使う可能性があった。連番時だけ既存の`toQImage()`によるフレーム解決を通すようにした。
- 価値: CPU画像経路とバッファ経路で連番の現在フレームがずれるリスクを減らす。
- 未検証: GPUエフェクト適用中の連番切替、ビルド。

# 2026-08-02: Corner PinのCPUホモグラフィワープを実装

- 関連: `Artifact/src/Effect/ArtifactCornerPinEffect.cppm`
- 事実: 4点からホモグラフィを計算していたが、出力画像へ適用していなかった。既存のRGBA32FバッファをOpenCVの`warpPerspective`で逆向きサンプリングし、透明境界で結果を生成するようにした。
- 価値: Corner Pinの8点プロパティが実際の画像変形へ反映される。
- 未検証: 極端な四辺形・退化した4点、GPU経路、ビルド。

# 2026-08-02: Noise Generatorの多層ノイズ生成を実装

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: 生成時に`scale`、`octaves`、`frameNumber`を使わず、毎回の単純乱数をRGBA各チャンネルへ生成していた。フレーム依存の決定的な乱数グリッドを複数解像度で補間・合成し、グレースケールノイズとして出力するようにした。
- 価値: 同じフレームの再生成結果が安定し、スケールとオクターブ設定がノイズの粒度・複雑さへ反映される。
- 未検証: 実UIからのパラメータ接続、Perlin／Simplex固有の品質、ビルド。

# 2026-08-02: Noise Generatorの内部グリッド上限を追加

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: 多層化により`scale`とオクターブ数の組み合わせ次第で、出力解像度を超える巨大な乱数グリッドを確保し得た。各オクターブの内部グリッド寸法を最大512に制限した。
- 価値: 高解像度出力や高オクターブ設定でも、不要なメモリ急増を防ぐ。
- 未検証: 高負荷設定での画質と実行時間、ビルド。

# 2026-08-02: Gradient／Shape Generatorの入力境界を補強

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: 無効な出力サイズではOpenCV行列生成前に処理を止め、Gradientの半径除算は最小値を持たせ、Shapeの負のサイズ設定は0へクランプするようにした。
- 価値: 空画像や異常なUI入力での除算・不正サイズ・負領域描画を防ぐ。
- 未検証: 無効値を直接渡す呼び出し経路、ビルド。

# 2026-08-02: Radio Wavesの存続波だけを走査

- 関連: `Artifact/src/Effects/Generate/RadioWavesEffect.cppm`
- 事実: これまでは現在時刻までに発生した全ウェーブを走査し、寿命切れを内側で破棄していた。現在時刻と寿命から存続可能な発生番号を先に絞り、周波数・寿命の安全値も使うようにした。
- 価値: 長時間プレビューでも、寿命切れウェーブの累積走査による計算量増加を防ぐ。
- 未検証: 高周波・長時間プレビュー、ビルド。

# 2026-08-02: Liquifyのバイリニア座標順を修正

- 関連: `Artifact/src/Effects/Liquify/LiquifyEffect.cppm`
- 事実: CPUサンプラーでx方向隣接点とy方向隣接点が逆に割り当てられ、補間係数と異なる軸の画素を混合していた。`c10`を(x1,y0)、`c01`を(x0,y1)へ修正した。
- 価値: Push／Pinch／BloatなどCPU Liquifyの境界補間が意図した二次元座標に一致する。
- 未検証: 各ブラシ種別の画質、GPU Pushとの一致、ビルド。

# 2026-08-02: AIDSLのselect filterをASTへ保持

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `parseFilter()`が存在していたが、`select layers ... where`の解析結果では常に`nullptr`を代入していた。where以降のトークンがある場合に既存パーサーを呼び出し、`SelectLayersCommand::filter`へ保存するようにした。
- 価値: レイヤー選択DSLの条件式が解析段階で失われず、後続のcompile／execute実装が利用できる。
- 未検証: 複数条件の実行評価、compile／executeの未実装部分、ビルド。

# 2026-08-02: AIDSLの比較式評価を実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `PropertyRef`の存在確認と`Literal`の常時trueだけで、`BinaryExpr`は常にtrueを返していた。プロパティ／リテラル値を解決し、数値の型差を吸収した比較、文字列比較、正規表現一致、無効な式のfalse判定を追加した。
- 価値: 解析されたselect filterが条件式として評価可能になる。既存のネスト表現による複数条件も比較結果のAND相当として評価できる。
- 未検証: DSL実行経路、正規表現の性能、ビルド。

# 2026-08-02: Liquify GPU Pushの変位方向をCPUへ同期

- 関連: `Artifact/src/Effects/Liquify/LiquifyEffect.cppm`
- 事実: CPUは`q + direction * strength`をサンプリングしていたが、GPU HLSLは`q - direction * strength`を使っていた。GPUのPush方向をCPUと同じ加算方向へ修正した。
- 価値: GPU／CPUフォールバックでPushブラシの見た目が反転しない。
- 未検証: GPU実機での方向一致、他ブラシ種別、ビルド。

# 2026-08-02: Lens DistortionのCPUズーム境界を補強

- 関連: `Artifact/src/Effects/LensDistortion/LensDistortionEffect.cppm`
- 事実: ズームsetterは0以下を受け入れ、CPU経路ではその値で除算していた。GPU経路と同じ`0.001`を下限としてCPU計算にも適用した。
- 価値: CPUフォールバック時の無限値・NaN生成を防ぎ、GPU／CPUで最低ズーム条件を一致させる。
- 未検証: ズーム0以下のUI入力、ビルド。

# 2026-08-02: KaleidoscopeのCPUサンプリングをGPUへ同期

- 関連: `Artifact/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm`
- 事実: CPU経路は変形後座標を最近傍丸めしていたが、GPU経路はバイリニア補間を使っていた。既存の`sampleBilinear`でCPUも補間するようにした。
- 価値: CPUフォールバックとGPU実行で、Kaleidoscopeのエッジ品質・縮小時の見た目が一致する。
- 未検証: GPU／CPU画質の実機比較、ビルド。

# 2026-08-02: KaleidoscopeのFeather半径をGPUへ同期

- 関連: `Artifact/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm`
- 事実: CPUは中心からの距離の1.5倍をFeather基準にしていたが、GPUは中心から画像四隅までの最大距離を基準にしていた。CPUも画像範囲から最大距離を算出するようにした。
- 価値: Feather境界の位置がCPU／GPU経路で一致する。
- 未検証: 非中央中心点・高Feather値での実機比較、ビルド。

# 2026-08-02: Find Edges GPUのアルファ保持を追加

- 関連: `Artifact/src/Effects/FindEdges/FindEdgesEffect.cppm`
- 事実: GPU HLSLはエッジ値をRGBへ出力する際にアルファを常に1へ設定していた。入力中心画素のアルファを保持し、CPU経路と同じ透明度扱いにした。
- 価値: 透明レイヤーへFind Edgesを適用しても、GPU経路で不透明化しない。
- 未検証: GPU／CPUのエッジ強度差、実機描画、ビルド。

# 2026-08-02: Find EdgesのCPU／GPU強度差を把握

- 関連: `Artifact/src/Effects/FindEdges/FindEdgesEffect.cppm`
- 事実: CPUはLaplacian＋画像全体の正規化＋元画像とのブレンド、GPUはSobel強度の飽和出力であり、アルファ修正後もエッジ強度のアルゴリズム差は残る。
- 価値: 今後のGPU parity対応で、単なる係数調整では解消できない差分を明確化できる。
- 未検証: 実画像での差分量、GPU側の二段階正規化設計。

# 2026-08-02: HexGrid GPUの入力クランプをCPUへ同期

- 関連: `Artifact/src/Effects/Rasterizer/HexGridEffect.cppm`
- 事実: CPUはセルサイズ4、線幅0.5を下限としていたが、GPU HLSLは受け取った値をそのまま除算・判定に使っていた。GPU側でも同じ下限を適用した。
- 価値: 異常値や未同期パラメータでも、CPUフォールバックと同じグリッド形状になり、ゼロ除算リスクを抑える。
- 未検証: GPU実機での異常値入力、ビルド。

# 2026-08-02: Vignetteの偏心中心向け半径を補正

- 関連: `Artifact/src/Effects/Rasterizer/VignetteEffect.cppm`
- 事実: 半径基準を常に中心から原点までの距離としていたため、中心点を右下へ移動すると反対側の画面領域を十分に覆えなかった。CPU／GPU双方で中心から四隅までの最大距離を使うようにした。
- 価値: 偏心したVignetteでも画面全体に一貫したフェザー境界を生成する。
- 未検証: 中心点端部・半径0・高Feather値、ビルド。

# 2026-08-02: Radial ShadowのCPU合成をGPUへ同期

- 関連: `Artifact/src/Effects/RadialShadow/RadialShadowEffect.cppm`
- 事実: CPUはRGBAへ`cv::addWeighted`を適用し、色チャンネル順もGPUと異なっていた。CPUも入力RGBへ影色×アルファを加算し、入力アルファへ影アルファを加算して0〜1へクランプするようにした。
- 価値: CPUフォールバック時の色反転・アルファの過剰加算を防ぎ、GPU経路と結果の定義を揃える。
- 未検証: 透明画像・高不透明度での実機比較、ビルド。

# 2026-08-02: Inner Shadowのアルファ合成を修正

- 関連: `Artifact/src/Effects/InnerShadow/InnerShadowEffect.cppm`
- 事実: 影係数に`(1 - source alpha)`を掛けていたため、不透明なソース内部では影が消え、実質的に外側へ寄った合成になっていた。ソースアルファ内で影色を補間し、出力アルファは元のアルファを保持するようにした。
- 価値: Inner Shadowが不透明レイヤーの内側へ正しく表示される。
- 未検証: 透明境界・ぼかし量・GPUフォールバックでの実機表示、ビルド。

# 2026-08-02: Drop ShadowのCPU影色チャンネル順を修正

- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`
- 事実: RGBA32F画像の`cv::Mat`へ影色を`B,G,R,A`順で書き込んでいたため、CPU経路だけ赤青が入れ替わっていた。RGBA順で影マットを生成するようにした。
- 価値: CPUフォールバックとGPU経路で影色が一致する。
- 未検証: GPU／CPU実機比較、半透明影、ビルド。

# 2026-08-02: Drop Shadow GPUの画像外サンプルを透明化

- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`
- 事実: CPUのオフセット画像は範囲外を0で初期化していたが、GPUの`alphaAt`は端画素をクランプしていた。GPUも範囲外をアルファ0として扱うようにした。
- 価値: 影が画像端で不自然に反復する問題を抑え、CPU経路と境界挙動を一致させる。
- 未検証: 大きなオフセット・ぼかし半径での実機比較、ビルド。

# 2026-08-02: Inner Shadowの影色チャンネル順を修正

- 関連: `Artifact/src/Effects/InnerShadow/InnerShadowEffect.cppm`
- 事実: RGBA32Fの`cv::Mat`ビューへ影色をBGR順で格納していたため、CPU経路だけ赤青が入れ替わっていた。影マットをRGBA順で生成するようにした。
- 価値: Inner ShadowのCPUフォールバックで指定色が正しく表示される。
- 未検証: 半透明境界・各色での実機表示、ビルド。

# 2026-08-02: SatinのCPU色順と合成係数をGPUへ同期

- 関連: `Artifact/src/Effects/Satin/SatinEffect.cppm`
- 事実: CPUサテンマットがBGR順で、さらにサテンアルファへソースアルファを事前乗算していたため、色が反転しGPUより弱くなっていた。RGBA順へ修正し、合成時のアルファ係数をGPUと同じにした。
- 価値: CPUフォールバック時の色とサテン強度がGPU経路と一致する。
- 未検証: 半透明ソース・反転・ぼかし量での実機比較、ビルド。

# 2026-08-02: StrokeのCPU影色チャンネル順を修正

- 関連: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`
- 事実: RGBA32FのStrokeマットへ影色をBGR順で格納していたため、CPU経路だけ赤青が入れ替わっていた。RGBA順でマットを生成するようにした。
- 価値: StrokeのCPUフォールバックで指定色がGPU経路と一致する。
- 未検証: 各色・半透明ソースでの実機表示、ビルド。

# 2026-08-02: Stroke／Satin GPUの境界サンプルを透明化

- 関連: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`, `Artifact/src/Effects/Satin/SatinEffect.cppm`
- 事実: CPU側の膨張・オフセット用アルファ画像は範囲外を0としていたが、GPUの`alphaAt`は端画素をクランプしていた。両シェーダーで範囲外をアルファ0にした。
- 価値: 画像端でストローク／サテンが不自然に反復する差異を抑える。
- 未検証: 大きな幅・距離・ソフトネスでの実機比較、ビルド。

# 2026-08-02: Glow CPUのRGBA輝度係数を修正

- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`
- 事実: RGBA32FのRGBチャンネルを`cv::COLOR_BGR2GRAY`へ渡していたため、赤と青の輝度寄与が逆になっていた。RGBA順の0.299／0.587／0.114係数で直接輝度を計算するようにした。
- 価値: CPUフォールバック時のGlow抽出マスクがGPU・標準的なRGBA輝度定義と一致する。
- 未検証: 赤青単色入力でのGPU／CPU比較、ビルド。

# 2026-08-02: Glow CPUのハイライトマスクをGPUへ同期

- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`
- 事実: CPUは閾値を引いた後に`1/(1-threshold)`で正規化していたが、GPUは閾値を超えた輝度へGainを直接適用していた。CPUも閾値超過値へGainを適用し、0〜1へ飽和させるようにした。
- 価値: CPUフォールバックとGPUのGlow発光量が同じ定義になる。
- 未検証: Gain・低輝度入力での実機比較、ビルド。

# 2026-08-02: GlowマスクのOpenCVクランプ型を明示

- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`
- 事実: ハイライトマスクの上限処理をOpenCVの`Scalar`形式へ揃え、既存の行列演算と同じ型経路で明示した。
- 価値: OpenCVオーバーロードの暗黙変換に依存せず、RGBA32Fマスクの上限処理を安定させる。
- 未検証: ビルド。

# 2026-08-02: Luma Keyの適用時閾値をクランプ

- 関連: `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`
- 事実: UI経由のproperty setter以外から閾値が設定されると、適用時のluma判定範囲が0〜1を外れる可能性があった。low／highを並べ替えた後、適用直前にも0〜1へクランプするようにした。
- 価値: 直接API利用や復元データに異常値があっても、キーアルファ計算が安定する。
- 未検証: 不正閾値の復元・実画像、ビルド。

# 2026-08-02: Chroma Keyの適用時パラメータを正規化

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`
- 事実: 直接APIや復元データからsimilarity／smoothness／spillReductionへ範囲外値が入ると、距離判定やスピル係数が不安定になり得た。適用時にRGB最大距離`√3`、最小softness、spill 0〜1へクランプするようにした。
- 価値: UI外からの不正入力でもキーアルファとスピル除去が安定する。
- 未検証: 範囲外値の復元・実画像、ビルド。

# 2026-08-02: Hue/SaturationのCPU色空間変換をRGBAへ修正

- 関連: `Artifact/src/Effects/ColorCorrection/HueAndSaturation.cppm`
- 事実: RGBA32FのRGBチャンネルをBGR用のHSV変換へ渡していたため、CPU経路で赤青が入れ替わって色相・彩度が変化していた。RGB2HSV／HSV2RGBへ変更した。
- 価値: CPUフォールバック時の色相・彩度結果が入力のRGBA順とGPU経路に一致する。
- 未検証: 赤青単色・色相ラップ・Colorizeでの実機比較、ビルド。

# 2026-08-02: Displacement Mapのクランプ補間を修正

- 関連: `Artifact/src/Effects/DisplacementMap/DisplacementMapEffect.cppm`
- 事実: 画像外座標で整数インデックスだけをクランプし、補間係数は負値・1超過のままだったため、端画素から外側へ外挿していた。サンプリング座標全体を画像範囲へクランプしてから補間するようにした。
- 価値: 非Wrapモードの強い変位でも、境界が安定して端画素へクランプされる。
- 未検証: 大きな負方向・正方向変位、Wrapモード、ビルド。

# 2026-08-02: Render QueueプリセットのAudio分類を実装

- 関連: `Artifact/src/Render/ArtifactRenderQueuePresets.cppm`
- 事実: `Audio`カテゴリが常に空を返し、カスタムのWAV／MP3／AAC／PCMプリセットも分類できなかった。コンテナとコーデックを正規化して音声形式を判定し、Videoも既知の動画形式だけを返すようにした。
- 価値: プリセット選択UIで音声プリセットを正しく絞り込め、CSS／HTMLなどを動画として誤分類しない。
- 未検証: 音声プリセットUI、FFmpeg書き出しとの接続、ビルド。

# 2026-08-02: Generator基底の出力サイズ・フレーム範囲を正規化

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: `setOutputSize()` が0以下のサイズを保持し、`setFrameRange()` が逆順範囲をそのまま保持していた。サイズを最小1へ、範囲を昇順へ正規化した。
- 価値: 生成処理へ渡る基本パラメータが不正にならず、後続のバッファ確保・フレーム判定が安定する。
- 未検証: UI外からの不正値入力、ビルド。

# 2026-08-02: Generator固有パラメータの入力正規化

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: Noiseのscale／octaves、Shapeのsizeがsetter経由では無制限で、NaNや極端な値が内部に保持され得た。生成処理の想定範囲に合わせて有限値・範囲を正規化した。
- 価値: UI外の設定復元やスクリプト入力でも、生成処理のグリッド計算・形状サイズが安定する。
- 未検証: NaN／無限値の入力、実画像生成、ビルド。

# 2026-08-02: Noise GeneratorのWhiteNoiseモードを接続

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: `noiseType_`はログ出力以外で参照されず、WhiteNoiseを選んでも補間済みの低周波ノイズになっていた。WhiteNoiseでは出力解像度の乱数場を直接使うようにした。
- 価値: UI上のWhiteNoise選択が実際の生成結果へ反映される。乱数seedは従来同様フレームとoctaveから決定論的に生成する。
- 未検証: 各NoiseTypeの実画像比較、GPU連携、ビルド。

# 2026-08-02: Gradient GeneratorのConicモードを接続

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: `GradientType::Conic`がRadialと同じ分岐へ入り、角度方向のグラデーションを生成していなかった。中心周りの角度を0〜1へ正規化する処理を追加した。
- 価値: UIで選択できるConicグラデーションが実際の出力へ反映される。
- 未検証: 角度の開始位置・境界、GPU連携、ビルド。

# 2026-08-02: Generatorの列挙型パラメータを正規化

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: Gradient／Noise／Shapeの各setterが外部から不正な列挙値をそのまま保持していた。列挙範囲へクランプしてから内部状態へ格納するようにした。
- 価値: 復元データやスクリプト由来の不正値で、未定義分岐やログ用配列の範囲外アクセスが起きる可能性を下げる。
- 未検証: 不正enum入力、ビルド。

# 2026-08-02: Clone Generatorの配置パラメータを正規化

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: spacing／radius／grid spacing／spiral rotations／rotation stepがNaNや無限値を保持でき、radiusは負値も許容していた。有限値検証とradiusの非負化を追加した。
- 価値: クローン変換行列の生成へ不正な浮動小数値が伝播する可能性を下げる。
- 未検証: 不正値からの復元、各分布モードの実配置、ビルド。

# 2026-08-02: Clone Generatorの分岐・bounds入力を正規化

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: distribution／transform modeが不正enum値を保持でき、boundsも負値・NaN・無限値を配置計算へ渡せた。列挙範囲と非負有限値へ正規化した。
- 価値: 外部入力による不正分岐やクローン配置の数値破綻を防ぐ。
- 未検証: 不正enum・bounds復元、実配置、ビルド。

# 2026-08-02: Clone GeneratorのSpiralオフセットを反映

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Spiral分布だけoffsetのX/Yを適用せず、Zのみ反映していた。螺旋配置後にX/Yオフセットを適用するよう修正した。
- 価値: 他の分布モードと同様に、Spiralでも全軸の配置オフセットが機能する。
- 未検証: 3D変換順序、各回転設定、ビルド。

# 2026-08-02: Clone GeneratorのPoisson Disk配置を接続

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Random分布の`usePoissonDisk`設定が無視され、通常の独立乱数配置になっていた。指定時は最大64回の候補再試行で、spacingを最小距離として既存位置との重なりを抑えるようにした。
- 価値: Random配置でクローンが局所的に密集する問題を軽減できる。候補が見つからない場合は通常の乱数候補へフォールバックする。
- 未検証: 高密度配置、3D距離、実表示、ビルド。

# 2026-08-02: Clone GeneratorのGrid回転ステップを接続

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Grid2D／Grid3Dでは`rotationStep`を参照せず、設定しても全クローンが同じ向きだった。生成順インデックスに応じたZ回転を追加した。
- 価値: Grid分布でも既存の回転ステップ設定を利用でき、分布モード間の設定挙動が揃う。
- 未検証: 3D変換順序、Grid表示、ビルド。

# 2026-08-02: Clone GeneratorのHexagonal回転ステップを接続

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Hexagonal分布でも`rotationStep`が無視されていた。生成順インデックスに応じた回転を追加した。
- 価値: Hexagonal／Grid／Linearなどで回転ステップ設定の挙動が揃う。
- 未検証: 六角形配置の変換順序、実表示、ビルド。

# 2026-08-02: Clone GeneratorのSplineフォールバック回転を接続

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: Spline未設定時のLinearフォールバックでは`rotationStep`が適用されず、Spline設定時と結果が一致しなかった。フォールバックにも生成順回転を追加した。
- 価値: Splineの有無で回転設定が突然失われず、フォールバック時の挙動が通常のLinear分布と揃う。
- 未検証: Spline未設定時の表示、変換順序、ビルド。

# 2026-08-02: Particle EmitterのdeltaTime入力を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: `ParticleEmitter::update()`が負値・NaN・無限値のdeltaTimeを受けると、内部時刻や粒子寿命・速度計算へ不正値が伝播し得た。非有限値または0以下の更新を早期無視するようにした。
- 価値: 再生停止・外部制御・フレーム時間異常時に粒子シミュレーションが壊れにくくなる。
- 未検証: 可変フレームレート、決定論モード、ビルド。

# 2026-08-02: Particle Emitterの発生数境界を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: `emitParticles()`が負のcountや0以下のmaxParticlesを受けても発生可能数を計算していた。早期終了とavailable値の非負化を追加した。
- 価値: 不正なEmission設定で負数の発生処理や容量計算が発生せず、Burst／Continuous双方の挙動が安定する。
- 未検証: 不正なmaxParticles・burstCount、補助粒子、ビルド。

# 2026-08-02: Particle Emitterのゼロ方向フォールバックを追加

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: directionがゼロベクトルの場合、正規化後もゼロのままで発生粒子の速度方向が失われていた。発生方向を既定のY軸へフォールバックするようにした。
- 価値: 無効な方向設定でも、速度やdirection spreadがゼロへ退化しない。
- 未検証: ゼロ方向、spread、ビルド。

# 2026-08-02: Particle Emitterのspread後方向を正規化

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: direction spread適用後のベクトルをそのまま返していたため、spread角度によって方向ベクトル長が変化し、速度スケールまで変動し得た。返却前に正規化した。
- 価値: spreadは方向だけを変え、初速の大きさは既存設定に委ねられる。
- 未検証: 大きなspread、ゼロ方向、ビルド。

# 2026-08-02: Particle Emitterの発生形状寸法を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: Sphere／CircleのradiusとBox／Rectangle／Lineの寸法が負値のまま発生位置計算へ使われ得た。形状ごとの発生位置計算時に非負化した。
- 価値: 不正な寸法でも発生領域が反転せず、設定値異常による予期しない分布を抑える。
- 未検証: 負寸法、各EmitterShape、ビルド。

# 2026-08-02: Particle EmitterのdirectionSpreadを正規化

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: directionSpreadがNaN・無限値・180度超でも角度計算へ渡せた。有限値を0〜180度へクランプしてからspreadを適用するようにした。
- 価値: 異常な入力による不安定な三角関数計算や意図しない反転を防ぐ。
- 未検証: 範囲外spread、ゼロ方向、ビルド。

# 2026-08-02: Particle寿命計算のゼロ除算を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: `updateParticle()`がmaxLifeを直接除数に使っており、0以下・非有限値の粒子ではlifeが不正化し得た。更新時に最小0.001秒の有限寿命へ正規化した。
- 価値: 不正な寿命設定でもNaNがシミュレーションへ伝播せず、粒子の死亡判定が安定する。
- 未検証: maxLife異常値、補助粒子、ビルド。

# 2026-08-02: Particle Continuous発生率を正規化

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: Continuous emissionのrateが負値・非有限値でも発生累積へ加算され、NaN時は粒子数変換へ不正値が伝播し得た。rateを有限非負値へ正規化し、累積値も有限性を確認するようにした。
- 価値: 外部設定異常でContinuous発生が停止・暴走する可能性を抑える。
- 未検証: 極端なrate、maxParticles到達時、ビルド。

# 2026-08-02: Sphere Emitterの一様サンプリングを修正

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: 球体発生の極角を一様サンプリングしていたため、球面密度が極付近へ偏っていた。`cos(phi)`を一様にし、半径も立方根サンプリングする既存処理と組み合わせた。
- 価値: Sphere emitterの発生位置が体積内で一様になる。
- 未検証: 球体分布の統計比較、他EmitterShape、ビルド。

# 2026-08-02: Particle Burst間隔の異常値を保護

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: BurstのintervalがNaNだと比較が成立せず、Burstが発生しなくなっていた。intervalを有限非負値へ正規化し、既存の0秒間隔は維持した。
- 価値: 不正なintervalでもBurstが永久停止せず、設定意図に近いフォールバック動作になる。
- 未検証: NaN／負interval、0秒Burst、ビルド。

# 2026-08-02: Box Fieldの寸法入力を正規化

- 関連: `Artifact/include/Effects/Field/BoxField.ixx`
- 事実: halfExtentとfalloffWidthが負値・非有限値を保持でき、SDF距離・フォールオフ評価へ不正値が入る可能性があった。setterで非負有限値へ正規化した。
- 価値: Box FieldのCPU評価が異常な外部入力でも安定する。
- 未検証: 不正寸法、falloff境界、ビルド。

# 2026-08-02: Linear Fieldの座標入力を正規化

- 関連: `Artifact/include/Effects/Field/LinearField.ixx`
- 事実: startPos／endPosが非有限値を保持でき、direction・length・influence計算へNaNが伝播し得た。setterで各軸を有限値へ正規化した。
- 価値: Linear Fieldの勾配評価が異常な外部入力でも安定する。
- 未検証: NaN／無限座標、ゼロ長区間、ビルド。

# 2026-08-02: Radial Fieldの入力を正規化

- 関連: `Artifact/include/Effects/Field/RadialField.ixx`
- 事実: center／axis／innerRadius／outerRadiusが非有限値や負半径を保持でき、軸投影・半径フォールオフへ不正値が伝播し得た。座標・軸を有限値、半径を非負有限値へ正規化した。
- 価値: Radial FieldのCPU評価が異常な外部入力でも安定する。
- 未検証: ゼロ軸、inner>outer、範囲外値、ビルド。

# 2026-08-02: Spherical Fieldの入力を正規化

- 関連: `Artifact/include/Effects/Field/SphericalField.ixx`
- 事実: center／radius／falloffWidthが非有限値や負値を保持でき、距離・減衰評価へ不正値が伝播し得た。座標を有限値、半径と減衰幅を非負有限値へ正規化した。
- 価値: Spherical FieldのCPU評価が異常な外部入力でも安定する。
- 未検証: 範囲外値、falloff境界、ビルド。

# 2026-08-02: AIDSLのAdd Key構文を実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `add key at 12f opacity = 0`の解析分岐がproperty／valueを格納せず、実質的に空のAddKeyCommandを生成していた。正しいトークン位置を検証し、複数トークンの値も再構成して格納するようにした。
- 価値: AIDSLスクリプトからキーフレーム追加コマンドの入力情報が失われない。
- 未検証: compile実装、複合値、実行経路、ビルド。

# 2026-08-02: AIDSLのRename/Delete/Group構文を接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`, `docs/planned/AI_TOOL_DSL_IMPLEMENTATION_GUIDE_2026-04-05.md`
- 事実: 仕様に記載されたrename selected／delete selected／group layers into構文がスタブで、ASTへ何も追加していなかった。Selectedターゲットとテンプレート／グループ名を各Commandへ格納するようにした。
- 価値: AIDSLスクリプトのトランザクション内外で、これらの編集意図を後段compileへ渡せる。
- 未検証: compile／実行／undo接続、ターゲット範囲、ビルド。

# 2026-08-02: AIDSLの基本Query構文を接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`, `docs/planned/AI_TOOL_DSL_IMPLEMENTATION_GUIDE_2026-04-05.md`
- 事実: `query selected_layers`／`query comp_size`／`query list properties of selected`がスタブで、クエリ配列へ追加されていなかった。対応するQueryNodeを生成し、comp_sizeの任意IDも保持するようにした。
- 価値: DSL解析結果を既存のQuery実行経路へ渡せる。
- 未検証: 実データ取得、選択状態・プロパティ一覧の完全実装、ビルド。

# 2026-08-02: AIDSLのFind/Describe Query構文を接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`, `docs/planned/AI_TOOL_DSL_IMPLEMENTATION_GUIDE_2026-04-05.md`
- 事実: `query find layers where ...`と`query describe layer ...`が未処理で、未知コマンド扱いになっていた。既存のfilter parserとQueryNodeへ接続した。
- 価値: レイヤー検索・記述クエリの解析結果を既存Query実行経路へ渡せる。
- 未検証: filter評価、レイヤー名からIDへの解決、実データ取得、ビルド。

# 2026-08-02: AIDSLのActive Composition Queryを接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: QueryActiveCompの実行ノードは存在したが、`query active_comp`がparserで生成されず未知構文になっていた。既存ノードへ接続した。
- 価値: DSLから現在のComposition情報を問い合わせる導線が成立する。
- 未検証: active compositionの実ホスト同期、実行結果、ビルド。

# 2026-08-02: AIDSLのComp Size省略IDを補完

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `query comp_size`は空IDを照合して常にunknownになっていた。ID省略時は既存comp lookupの先頭IDを対象にするようにした。
- 価値: 仕様どおり引数なしのcomp_size queryでも既知Compositionを対象にできる。
- 未検証: active compの厳密な選択、実寸法取得、ビルド。

# 2026-08-02: AIDSL Transaction compileを実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: TransactionCommand::compileが常にnullptrを返し、トランザクション内の子CommandをActionへ束ねていなかった。TransactionActionを生成し、子compile結果を順序どおり収集するようにした。
- 価値: 個別Commandのcompile実装が追加された際に、transaction単位のActionへ自然に集約できる。
- 未検証: 子Commandのcompile実装、実行・undo、ビルド。

# 2026-08-02: AIDSL filterのAND結合を修正

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: 複数filter条件の`and`をEqで代用していたため、両条件がfalseでも結合結果がtrueになり得た。BinOp::Andを追加し、左から短絡評価する明示的なANDへ変更した。
- 価値: `where a == x and b == y`が期待どおり両条件成立時だけtrueになる。
- 未検証: 複数条件、ネスト、ビルド。

# 2026-08-02: AIDSL値パーサーの境界を修正

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: 空値で`front()`を参照する可能性があり、負の整数も文字列として解析されていた。空値を安全に扱い、符号付き整数を数値として認識するようにした。
- 価値: 不完全な入力でのクラッシュリスクを下げ、`-12`などのproperty valueを正しく扱える。
- 未検証: 不完全構文、符号付き値、ビルド。

# 2026-08-02: AIDSL filterのOR結合を実装

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: filter parserは`and`しか扱えず、条件結合時の演算子も固定だった。BinOp::Orと結合演算子列を追加し、and／orを記述順に評価できるようにした。
- 価値: `where type == "text" or type == "shape"`のような検索条件をASTへ保持できる。
- 未検証: 演算子優先順位、括弧、複合filter、ビルド。

# 2026-08-02: AIDSL FrameExpr::resolveを実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: FrameExpr::resolveの宣言に対する実装がなく、フレーム式を解決できなかった。数値、コンテキスト名、数値文字列を処理し、未解決値は0へフォールバックするようにした。
- 価値: AddKeyなどのフレーム指定を後段compileで解決できる基盤ができる。
- 未検証: コンテキスト式、負フレーム、ビルド。

# 2026-08-02: Generator基底のapply/outputを実装

- 関連: `Artifact/include/Generator/AbstractGeneratorEffector.ixx`, `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: AbstractGeneratorEffector::apply()がログ出力だけで生成処理を呼ばず、生成結果を保持するAPIもなかった。現在フレーム・出力サイズでgenerateContentを呼び、内部バッファをoutput()から参照できるようにした。
- 価値: Solid／Gradient／Noise／Shape Generatorを基底API経由で実際に生成・取得できる。
- 未検証: applyToLayer接続、バッファ再利用、ビルド。

# 2026-08-02: Generator applyへフレーム範囲を反映

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: startFrame／endFrameを保持していたが、範囲外のcurrentFrameでもgenerateContentが実行されていた。apply時に範囲外を早期終了するようにした。
- 価値: Generatorのフレーム範囲設定が実際の生成タイミングへ反映される。
- 未検証: 範囲境界、無効化後のoutput保持、ビルド。

# 2026-08-02: AIDSL InterpreterのTransaction compileを接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: AIDSLInterpreter::compileTransactionも子Commandのcompile呼び出しがコメントアウトされ、常に空のTransactionActionを返していた。子Actionを生成・収集するようにした。
- 価値: Interpreter経由のtransaction compileでも、個別CommandのActionを失わずに保持できる。
- 未検証: 個別Command compile、実行・undo、ビルド。

# 2026-08-02: AIDSLのUseComp compileを実装

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: UseCompCommand::compileが常にnullptrで、名前／IDのComposition解決結果をActionへ渡せなかった。lookupまたは`#` IDを解決し、CommandActionとして返すようにした。
- 価値: Transaction compileからComposition切替意図を保持できる。
- 未検証: ホスト側実行、未解決名、ビルド。

# 2026-08-02: AIDSL SetProperty compileを実装

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: SetPropertyCommand::compileが常にnullptrで、property pathと値を後段へ渡せなかった。空pathを拒否し、CommandActionへpropertyとValueの文字列表現を格納するようにした。
- 価値: Transaction compileからset操作の意図と値を保持できる。
- 未検証: 実ホストのproperty適用、型変換精度、ビルド。

# 2026-08-02: AIDSL AddKey compileを実装

- 関連: `Artifact/src/Tool/AIDSL/include/AIToolDSL/DSLTypes.ixx`, `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: AddKeyCommand::compileが常にnullptrで、frame／property／valueが後段へ渡らなかった。CommandActionへ3要素を保持するようにした。
- 価値: Transaction compileからキーフレーム追加意図を失わずに引き渡せる。
- 未検証: 実ホストのkeyframe適用、frame解決、型変換、ビルド。

# 2026-08-02: AIDSL Rename/Delete/Group compileを実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: Rename／Delete／Groupのcompileが常にnullptrで、テンプレート・グループ名・ターゲット情報を後段へ渡せなかった。CommandActionへ各情報を格納するようにした。
- 価値: AIDSL transactionから編集操作の意図を失わずに引き渡せる。
- 未検証: 実レイヤー操作、Specificターゲット、undo、ビルド。

# 2026-08-02: AIDSL SelectLayers compileを実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: SelectLayersCommand::compileが常にnullptrで、filter有無やlookup上の対象数を後段へ渡せなかった。CommandActionへ選択モードと対象数を格納するようにした。
- 価値: Transaction compileでselect操作を表現できる。
- 未検証: filter実評価、実選択状態の同期、ビルド。

# 2026-08-02: AIDSL executeのCommand compile段階を接続

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: AIDSLInterpreter::execute()はQueryだけを処理し、Commandのcompile結果を確認していなかった。全Commandをlookup付きでcompileし、生成できたAction数を結果へ含めるようにした。
- 価値: 実ホスト適用前でも、スクリプト内Commandの解決可否を実行結果から確認できる。
- 未検証: ホストAction dispatch、実編集、undo、ビルド。

# 2026-08-02: AIDSL dryRunのcompile可否を可視化

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: dryRunは構文サマリーのみで、Commandがcompile可能か確認できなかった。lookup付きcompileを実行せずに行い、compiledActionCountを返すようにした。
- 価値: スクリプト適用前に、未解決Compositionや不正pathによるcompile失敗を把握できる。
- 未検証: 実ホスト適用、型検証、ビルド。

# 2026-08-02: Generator outputキャッシュのサイズ変更を反映

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- 事実: setOutputSize()後もoutput()が旧サイズの生成バッファを保持し、再生成前に古い結果を返す可能性があった。サイズ変更時に出力キャッシュを無効化するようにした。
- 価値: output()の内容と現在設定された出力サイズの不一致を防ぐ。
- 未検証: サイズ変更後の再生成、バッファ再利用、ビルド。

# 2026-08-02: Generatorのレイヤー適用経路は既存API未接続

- 関連: `Artifact/src/Generator/AbstractGeneratorEffector.cppm`, `Artifact/include/Layer/ArtifactAbstractLayer.ixx`
- 事実: `ArtifactAbstractLayer`にはgeneratorを受け取って生成バッファをレイヤーソースへ適用する既存APIがなく、`layerGenerators()`は設定・シリアライズ用の保持領域だった。
- 価値または懸念: concrete layerへの依存や新規イベント経路を追加して接続すると、Generatorの責務を越えて画像バッファ変換・レイヤー所有権まで広げることになる。
- 次に確認すべきこと: Generator出力をレイヤーソースへ渡す正式なRender/Asset APIが定義された段階で、`applyToLayer()`をそのAPI経由で実装する。
- 未検証: 将来の正式API、ビルド。

# 2026-08-02: OCIO入力変換からviewer補正を分離

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: `applyInputTransformToWorkingImage()`のレガシーフォールバックが、source transfer decode／色空間変換後にviewer exposure・gammaを適用していた。viewer補正は表示変換専用の設定であり、入力素材のworking space変換へ混入させるべきではない。
- 価値: OCIOライブラリが使えない環境でも、入力変換が表示状態に依存せず決定的になる。
- 未検証: 実OCIO config、HDR素材、ビルド。

# 2026-08-02: OCIO viewer設定の非有限値を正規化

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: viewer exposure/gamma の setter と JSON復元が `std::clamp` のみで、NaN/Inf入力を有効値として保持する可能性があった。
- 価値: 表示変換時の `pow()` に不正値を渡さず、設定復元後も有限なviewer状態を保証する。
- 未検証: 壊れたJSON、UI入力、ビルド。

# 2026-08-02: TextLayerのスタイル数値を有限値へ正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: font size、stroke width、shadow、tracking、stretch、leading の setter がNaN/Infをそのまま保持し得た。
- 価値: Text Toolの入力・JSON復元経路から不正なレイアウト値がシェーピングや描画へ流れることを防ぐ。
- 未検証: 異常値を含む既存プロジェクト、フォント依存レイアウト、ビルド。

# 2026-08-02: Keyingパラメータの非有限値を防止

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`, `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`
- 事実: Chroma/Luma Keyのプロパティ入力は `std::clamp` 前にNaN/Infを検査しておらず、不正値がキー判定へ流れる可能性があった。
- 価値: キーイングのCPU経路が入力異常時にも有限な閾値で評価される。
- 未検証: 実画像品質、GPU/runtime経路、ビルド。

# 2026-08-02: BatchRendererの範囲境界を正規化

- 関連: `Artifact/src/Render/ArtifactRenderScheduler.cppm`
- 事実: `renderRange()` は逆順FrameRangeでも開始通知・タスク投入へ進み、`renderAroundFrame()` は負のradiusで範囲が反転し得た。
- 価値: バッチ投入前に不正範囲を拒否し、周辺フレーム要求を常に非負radiusとして扱う。
- 未検証: scheduler runtime、巨大なframe番号、ビルド。

# 2026-08-02: BatchRendererの終端フレームオーバーフローを防止

- 関連: `Artifact/src/Render/ArtifactRenderScheduler.cppm`
- 事実: 終端が整数最大値のFrameRangeでは、`f <= lastFrame` の後の `++f` がオーバーフローし、バッチ投入ループが終了しない可能性があった。
- 価値: 終端フレーム投入後に明示的にbreakし、最大値を含む範囲でも有限回で終了する。
- 未検証: 最大値付近の実行、scheduler runtime、ビルド。

# 2026-08-02: RAM Preview失敗理由を状態へ反映

- 関連: `Artifact/src/Render/ArtifactRamPreviewController.cppm`
- 事実: render callbackがfalseを返した際、`frameFailed`シグナルだけが通知され、`RamPreviewState::lastErrorMessage`へ失敗情報が保存されていなかった。
- 価値: UIや診断側がシグナルを取り逃しても、最後の失敗フレームと理由を状態スナップショットから取得できる。
- 未検証: callback失敗、再ビルド、ビルド。

# 2026-08-02: RAM Previewの空範囲設定で旧状態を破棄

- 関連: `Artifact/src/Render/ArtifactRamPreviewController.cppm`
- 事実: `setPreviewRange(start >= end)` は新しい範囲のstateを作らず、以前のframeStatesとqueueを保持していたため、空範囲なのに旧フレームが問い合わせ可能になる可能性があった。
- 価値: 空／逆順範囲を設定した時点でstate、queue、カウンタ、エラーを一貫してリセットする。
- 未検証: 範囲変更中のbuild、再設定、ビルド。

# 2026-08-02: Layer dirty時にthumbnail cacheを無効化

- 関連: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 事実: `setDirty()` はrevisionとdirty flagだけを更新し、`getThumbnail()`のキャッシュを無効化していなかった。
- 価値: レイヤー変更後に旧サムネイルを返さず、将来の実コンテンツthumbnail生成へ正しい無効化境界を提供する。
- 未検証: 各layer mutation、thumbnail renderer、ビルド。

# 2026-08-02: FrameCache容量ゼロとprefetch終端を修正

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: maxFrameCount=0でも`put()`がentryを追加でき、prefetchRange/cancelPrefetchは終端が整数最大値の場合にincrement overflowの可能性があった。
- 価値: 設定した容量上限を厳密に守り、最大フレームを含むprefetch範囲でも有限回で処理を終了する。
- 未検証: cache eviction、prefetch worker、ビルド。

# 2026-08-02: FrameCache eviction候補枯渇時のフォールバック

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: LRU/LFU/FIFO/Sizeの候補キューが無効化後のstale recordだけになった場合、`evictOne()`はentryを削除せず、`evictToFit()`が終了しない可能性があった。
- 価値: live candidateが見つからない場合もentries mapから1件を削除し、容量調整を有限時間で完了させる。
- 未検証: 各eviction policy、長時間運用、ビルド。

# 2026-08-02: FrameCacheのnull entry取得をmiss扱い

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: entries mapにnull SharedPtrが残った場合、`get()`がhitCountを増やしてnullを返していた。
- 価値: null entryを除去し、返却値とhit/miss統計の意味を一致させる。
- 未検証: 破損状態の復旧、統計表示、ビルド。

# 2026-08-02: RenderPerformanceMonitorの計測値境界を修正

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: FPS=0/NaNでframe budgetを計算でき、frame timeのNaN/負値もメトリクスへ蓄積できた。
- 価値: performance monitorがゼロ除算や不正な平均値を生成せず、有限な計測値だけを集計する。
- 未検証: runtime FPS表示、異常入力、ビルド。

# 2026-08-02: PerformanceMonitor reset時にFPS窓を再初期化

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: `reset()` は履歴とFPSカウンタを消去していたが、`QElapsedTimer`を再起動していなかった。
- 価値: reset後の最初のFPS集計が、reset前の経過時間を分母に含めず新しい計測窓から始まる。
- 未検証: runtime FPS表示、長時間停止後のreset、ビルド。

# 2026-08-02: ProgressiveRendererのdownsample境界を正規化

- 関連: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 事実: draft/preview qualityのdownsampling値をそのまま保持しており、0以下の値が後段の解像度計算へ渡る可能性があった。
- 価値: downsampleを常に1以上に保ち、品質変更時のゼロ除算・不正サイズを防ぐ。
- 未検証: 実レンダー品質切替、UI入力、ビルド。

# 2026-08-02: FrameCache module interfaceのinclude位置を修正

- 関連: `Artifact/include/Render/ArtifactFrameCache.ixx`
- 事実: `export module Artifact.Render.FrameCache;` の後にQtヘッダをincludeしており、module purview内のinclude禁止ルールに違反していた。
- 価値: Qtヘッダをglobal module fragmentへ移し、MSVC/Ninja dyndepのモジュール解析条件を満たす。
- 未検証: module scan、ビルド。

# 2026-08-02: 公開module interfaceのinclude位置を追加修正

- 関連: `Artifact/include/Effect/ArtifactStabilizer.ixx`, `Artifact/include/Preview/ArtifactTimelineClock.ixx`, `Artifact/include/Plugin/LayerPluginAdapter.ixx`
- 事実: 3つの公開interfaceでも`export module`後にQt/wobject/ABIヘッダをincludeしていた。
- 価値: module purview内のincludeをなくし、C++20 moduleスキャン時の依存解釈を一貫させる。
- 未検証: module scan、Stabilizer/Timeline/Pluginのビルド。

# 2026-08-02: AIDSL undoの未適用成功を防止

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: undo stackにActionがあれば、apply/revert APIが存在しないにもかかわらず`undo()`がtrueを返していた。
- 価値: ホスト状態を変更していないのにundo成功と報告する誤認を防ぐ。
- 次に確認すべきこと: host actionへapply/revert契約を追加する設計時に、undo/redoを実装する。
- 未検証: host integration、ビルド。

# 2026-08-02: AIDSL find layersのlookup条件を実装

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: `QueryFindLayers` はfilter ASTを保持していたが、実行時は条件を評価せず全layer IDを返していた。lookupから得られるlayer IDとグループ名を`id`/`name`として条件評価するようにした。
- 価値: `find layers where id ...` / `name ...` の基本的な検索が、ホストの全プロパティAPIなしで機能する。
- 制限: transformやeffect等の実レイヤープロパティはlookupに存在しないため、未対応のまま。
- 未検証: 複合条件、ホスト連携、ビルド。

# 2026-08-02: SimpleSpline終端制御点の参照を修正

- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: Catmull-Rom補間の最終セグメントで、次の制御点が`i + 1`ではなくセグメント数に依存する誤ったindex式になっていた。
- 価値: 終端付近のSpline位置・接線が最後の制御点を基準に計算される。
- 未検証: Spline分布の実座標、単一点／二点入力、ビルド。

# 2026-08-02: SimpleSpline接線をHermite微分へ修正

- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: 接線がHermite位置basis (`h2`,`h3`) の線形和になっており、曲線位置の微分ではなかった。
- 価値: Spline上の向き・回転に使う接線が、Catmull-Rom/Hermite曲線の実際の微分方向になる。
- 未検証: 接線方向のruntime表示、Spline分布、ビルド。

# 2026-08-02: SimpleSplineの非有限パラメータを防止

- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: `getPoint()` はNaN/Infの`t`を`std::clamp`へ渡し、NaNのままsegment indexへ変換する可能性があった。
- 価値: 補間入力が常に有限範囲となり、未定義なindex変換を防ぐ。
- 未検証: 異常入力、Spline分布、ビルド。

# 2026-08-02: CloneGenerator分布パラメータを正規化

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: variationのNaNとgrid spacingの負値がsetterから保持され、Random/Grid系の生成式へ流れる可能性があった。
- 価値: variationを0〜1、grid spacingを0以上に統一し、分布生成の入力前提を守る。
- 未検証: 各分布モード、異常入力、ビルド。

# 2026-08-02: Particle Generator補間入力を有限値へ正規化

- 関連: `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 事実: 三段階色／値補間のmid position・timeと速度由来のstretch計算がNaN時に`std::clamp`後も不正値を保持し得た。
- 価値: パーティクルの色補間・flipbook/stretch表示で不正な計算値が伝播するのを防ぐ。
- 未検証: 異常なパラメータ、ソフトウェア描画、ビルド。

# 2026-08-02: Particle SystemのtimeScaleを正規化

- 関連: `Artifact/include/Generator/ArtifactParticleGenerator.ixx`
- 事実: `setTimeScale()` がNaN/Infや負値をそのまま保持し、更新deltaTimeへ乗算していた。
- 価値: シミュレーション時間を有限・非負に保ち、逆向き／非有限時間による状態破壊を防ぐ。
- 未検証: 時間倍率変更、停止・再開、ビルド。

# 2026-08-02: Stabilizerの変換値境界を補正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: processFrame() が不正なoutput sizeや、scale=0/NaN、translation/rotationの非有限値をそのまま画像変換へ渡す可能性があった。
- 価値: 変換時のゼロ除算・不正座標・不正画像サイズを防ぎ、異常なmotionでも元画像に近い安全なフォールバックを使う。
- 未検証: 実映像スタビライズ、境界塗り、ビルド。

# 2026-08-02: LiveStabilizerの履歴サイズを正規化

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: `setMaxHistorySize()` が0以下の値を保持し、履歴削除時のerase範囲計算を破壊する可能性があった。
- 価値: 履歴数を最低1件に保ち、負値由来の範囲外削除を防ぐ。
- 未検証: 履歴サイズ変更中のruntime、ビルド。

# 2026-08-02: Stabilizerの空フレーム入力を早期処理

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: null/0サイズ画像でも変換ループへ進み、空画像に対するborder座標clampが不正範囲になる可能性があった。
- 価値: 空フレームを安全にそのまま返し、無効な画素座標計算を防ぐ。
- 未検証: 空画像、境界塗り、ビルド。

# 2026-08-02: Stabilizer特徴検出・平滑化の境界を修正

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: feature block size、quality/min distance、smoothing windowを未検証のまま使用し、負値で画素アクセスや空の平滑化結果になる可能性があった。
- 価値: 特徴検出の近傍が画像内に収まり、平滑化結果が少なくとも各motion frameに対応する。
- 未検証: 小画像、異常パラメータ、実映像、ビルド。

# 2026-08-02: Stabilizer block size検証の整数overflowを防止

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: block sizeと画像サイズの比較で、巨大な設定値に対する`blockSize * 2`の整数overflow余地があった。
- 価値: 乗算前の安全な除算比較により、異常に大きいblock sizeでも未定義な範囲判定を防ぐ。
- 未検証: 極端な設定値、小画像、ビルド。

# 2026-08-02: Stabilizer feature track座標を検証

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: trackFeatures() が外部由来のQPointFを直接intへ変換しており、NaN座標や空画像で探索へ進む可能性があった。
- 価値: 空画像を早期終了し、非有限のfeature座標を除外して画素アクセスを安全にする。
- 未検証: 外部track入力、空画像、ビルド。

# 2026-08-02: TextGizmo hit testのzoom境界を修正

- 関連: `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
- 事実: hitTest() がzoom=0/NaNのとき直接除算し、無限のhit thresholdで広範囲をText handleとして扱う可能性があった。
- 価値: zoomが不正でも有限の閾値でText Toolの選択判定を継続する。
- 未検証: zoom変更、viewport hit test、ビルド。

# 2026-08-02: TextLayer boxサイズの非有限値を防止

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: setMaxWidth/setBoxHeight は0以下だけを判定し、NaN/Infをlayoutへ保持し得た。
- 価値: Text Tool resize、property、JSON復元の全経路でbox layout値を有限値に保つ。
- 未検証: 異常JSON、resize操作、フォントレイアウト、ビルド。

# 2026-08-02: TextLayer ruby scaleの非有限値を防止

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: setRubyScale() がNaN/Infを`std::clamp`後も保持し得た。
- 価値: ルビ文字レイアウトの倍率を常に0.1〜1.0の有限値へ保つ。
- 未検証: ルビ表示、異常JSON、ビルド。

# 2026-08-02: TextLayer paragraph spacingの非有限値を防止

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: setParagraphSpacing() は負値だけを補正し、NaN/Infを保持し得た。
- 価値: 段落レイアウトへ不正なspacingを渡さず、Text ToolとJSON/property経路を安定させる。
- 未検証: 段落レイアウト、異常入力、ビルド。

# 2026-08-02: TextAnimator property更新の数値入力を正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: animator property path更新がQVariantのNaN/Infを各range・wiggly・transform値へ直接渡していた。
- 価値: Text Tool／AIDSL／property編集からのアニメータ数値を有限値に統一し、selector・layout計算の不正値伝播を防ぐ。
- 未検証: animator編集、JSON/QVariant異常入力、ビルド。

# 2026-08-02: TextGizmoレイヤー切替時のdrag状態をリセット

- 関連: `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
- 事実: setLayer() がlayerポインタだけを更新し、旧レイヤーのdrag/handle状態を保持していた。
- 価値: レイヤー切替後に旧操作が新レイヤーへ誤適用される可能性を防ぐ。
- 未検証: 選択切替中のdrag、Text Tool runtime、ビルド。

# 2026-08-02: TextGizmo selector overlayの非有限値を補正

- 関連: `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
- 事実: selector weight/cluster boundary/line boundaryを`std::clamp`のみで描画座標へ渡していた。
- 価値: NaN/Inf selector値がオーバーレイ矩形や境界線のgeometryへ伝播するのを防ぐ。
- 未検証: selector preview、異常アニメータ値、ビルド。

# 2026-08-02: Stabilizer未マッチtrackの伝播を停止

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: updateFeatureTracks() が新フレームで未マッチのtrackをvalidのまま残し、次の探索で古い位置を再利用する可能性があった。
- 価値: 各段階で実際にマッチしたtrackだけを次フレームへ伝播し、motion推定に stale feature を混入させない。
- 未検証: 追跡精度、欠落特徴点、実映像、ビルド。

# 2026-08-02: Stabilizerへの空フレーム追加を拒否

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: `addFrame()` がnull/0サイズ画像をframesへ追加でき、後続の特徴検出やoutput size設定へ無効画像が混入し得た。
- 価値: Stabilizer内部のフレーム列を有効画像だけに保ち、空入力由来の失敗を早期に防ぐ。
- 未検証: 空フレーム追加、フレーム位置整合、ビルド。

# 2026-08-02: Stabilizer clearFramesの統計状態をリセット

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: clearFrames() はフレームとmotionを消去していたが、totalFeaturesとprocessingTimeを保持していた。
- 価値: 新しい入力シーケンス開始時に前回の特徴数・処理時間が混入しない。
- 未検証: UI統計表示、再利用、ビルド。

# 2026-08-02: Stabilizer外部track差し替え時の特徴数を同期

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: setFeatureTracks() はtrack配列を差し替えてもtotalFeaturesを更新していなかった。
- 価値: 外部検出結果を設定した直後も、valid track数と統計表示が一致する。
- 未検証: 外部track入力、統計UI、ビルド。

# 2026-08-02: AIDSL layer filterのproperty aliasを追加

- 関連: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm`
- 事実: find layersのlookup filterは短縮名id/nameだけを提供していた。
- 価値: `id`/`name`に加えて`layer.id`/`layer.name`も同じlookup値で評価し、既存のproperty path記法と整合させる。
- 未検証: DSL実行、複合条件、ビルド。

# 2026-08-02: LiveStabilizerの空フレーム履歴混入を防止

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: LiveStabilizer::processFrame() がnull/0サイズ画像をhistoryへ追加していた。
- 価値: Live履歴を有効画像だけに保ち、空フレームで履歴長や初期化状態が壊れるのを防ぐ。
- 未検証: 空入力、停止・再開、ビルド。

# 2026-08-02: Stabilizer可視化の不正geometryを除外

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: visualizeFeatures/visualizeMotionVectors() が非有限QPointFをQPainterへ渡す可能性があった。
- 価値: デバッグ描画でNaN/Inf座標を描画せず、可視化経路の不正geometry伝播を防ぐ。
- 未検証: 異常track表示、QPainter runtime、ビルド。

# 2026-08-02: Stabilizer単一フレームを恒等変換で処理

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: framesが1枚の場合、フレーム間追跡が実行されずfeatureTracksが空になり、安定化処理が失敗していた。
- 価値: 単一フレームを有効な恒等motionとして扱い、プレビューや静止画入力でもStabilizerを利用できる。
- 未検証: FrameMotionの既定値、単一画像、ビルド。

# 2026-08-02: Stabilizer入力変更時に旧motionを破棄

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: setFeatureTracks/addFrame後も旧frameMotions・smoothedMotions・stabilizedフラグが残り、入力と結果が不一致になる可能性があった。
- 価値: tracksまたはフレーム追加後は再安定化を要求し、旧結果の再利用を防ぐ。
- 未検証: tracks差し替え、フレーム追加後の再実行、ビルド。

# 2026-08-02: Stabilizer設定変更時に旧結果を無効化

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: `setParams()` がパラメータだけを差し替え、旧motionとstabilizedフラグを保持していた。
- 価値: smoothing／feature／transform設定変更後に、変更前の安定化結果を返さず再計算を要求する。
- 未検証: 設定変更後の再実行、runtime、ビルド。

# 2026-08-02: Live/Batch Stabilizer設定変更時の状態整合

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: LiveStabilizerは設定変更後も過去の履歴を保持し、BatchStabilizerは未処理時の進捗カウンタを保持していた。
- 価値: 設定変更後のLive履歴を再構築し、未実行Batchの進捗表示を新設定に合わせて初期化する。
- 未検証: 実時間入力、batch UI、ビルド。

# 2026-08-02: Stabilizer出力でalphaを保持

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: processFrame() が`Format_RGB32`で結果を作成しており、入力alphaを補間しても出力形式で保持できなかった。
- 価値: ARGB32 premultipliedへ変更し、透明背景を含む素材のスタビライズ結果でalphaを維持する。
- 未検証: premultiplied alphaの補間品質、実映像、ビルド。

# 2026-08-02: CloneGenerator spacingの負値を防止

- 関連: `Artifact/src/Generator/CloneGenerator.cppm`
- 事実: 基本spacingは有限値のみを確認し、負値を保持していた。
- 価値: Linear/Grid等の配置間隔を常に非負に保ち、逆向き間隔による意図しない配置を防ぐ。
- 未検証: 各分布モード、異常入力、ビルド。

# 2026-08-02: DistributionModesの標準型includeを自己完結化

- 関連: `Artifact/include/Generator/DistributionModes.ixx`
- 事実: `SimpleSpline` が使用する `std::clamp`、`std::isfinite`、`std::vector` の直接includeが不足していた。
- 価値: 他moduleの推移的includeに依存せず、公開interface単体で宣言・実装を解析できる。
- 未検証: module scan、Spline分布、ビルド。

# 2026-08-02: Difference Keyの閾値入力を有限値へ正規化

- 関連: `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`
- 事実: threshold/softness のプロパティ変更がNaN/Infを `std::clamp` に渡し、そのまま判定へ保持する可能性があった。
- 価値: Chroma/Lumaと同じくDifference Keyでも異常入力時のアルファ計算を安定させる。
- 未検証: 実画像品質、GPU/runtime経路、ビルド。
# 2026-08-02: 連番フレーム番号の安全な変換

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm` の連番画像フレーム選択。
- 事実: composition FPS と sequence FPS を掛けた結果を `qint64` に直接キャストする処理が `draw()` と `toQImage()` に重複していた。
- 対応: 共通の `resolveSequenceFrame()` に集約し、非有限値・非正の FPS・`qint64` 上限超過を扱ってからキャストするようにした。
- 価値: 極端な時間値や設定値での未定義動作を避け、描画経路と Qt 画像取得経路のフレーム解決を一致させる。
- 次に確認: 実フレーム番号が `qint64` 上限付近になるケースを含む連番再生の runtime 受入れ。ビルド・テストは未実施。
# 2026-08-02: Liquid Glow の非有限パラメータ防止

- 関連: `Artifact/src/Effects/Glow/LiquidGlowEffect.cppm`。
- 事実: setter が `std::clamp` のみを使っており、NaN 入力が内部実装へ伝播し得た。
- 対応: threshold/radius/intensity/flowScale/distortion/phase を有限値確認後にクランプし、異常値は各デフォルトへ戻すようにした。
- 価値: Gaussian blur と flow map 生成で NaN が広がる経路を遮断する。
- 次に確認: 実画像での glow 品質と GPU/runtime 経路。ビルド・テストは未実施。
# 2026-08-02: 基本エフェクト setter の有限値検証

- 関連: `DitheringEffect`、`AddNoiseEffect`、`BevelEffect`。
- 事実: 一部の setter が clamp/max のみで、NaN や infinity を CPU/GPU 実装へ渡し得た。
- 対応: amount/scale/size/strength/softness を有限値確認後に制約し、異常値は既定値へ復帰させた。
- 価値: ノイズ量・パターン尺度・ベベル計算に非有限値が混入する経路を減らす。
- 次に確認: GPU と CPU の実画像一致、ビルド・runtime受入れ。
# 2026-08-02: Luma Key の直接実装値ガード

- 関連: `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`。
- 事実: property 経路では有限値化済みだが、CPU 実装は内部値が直接非有限になった場合を想定していなかった。
- 対応: low/high/softness を apply 境界でも有限値化し、既定値と制約値を適用した。
- 価値: alpha matte 計算の NaN 伝播を property 経路外からも防ぐ。
- 次に確認: 実画像での閾値境界、CPU/runtime 受入れ。ビルド・テストは未実施。
# 2026-08-02: White Balance の色補正入力ガード

- 関連: `Artifact/include/Effects/WhiteBalanceEffect.ixx`。
- 事実: temperature/tint/brightness の setter が clamp のみで、NaN がそのまま内部値に残り得た。温度変換・指数計算・GPU定数へ影響する。
- 対応: 有限値確認後に既存範囲へクランプし、異常値は 6500K / 0 / 0 に復帰させた。
- 価値: CPU/GPU の色補正係数へ非有限値が伝播する経路を塞ぐ。
- 次に確認: CPU/GPUの色補正一致とHDR入力。ビルド・テストは未実施。
# 2026-08-02: Colorama のパラメータ入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/ColoramaEffect.cppm`。
- 事実: phase/spread/strength/saturationBoost/contrast が clamp/max のみで、非有限値を内部設定へ保持し得た。
- 対応: 有限値を確認してから既存範囲へ制約し、異常値は既定設定へ戻した。
- 価値: CPU/GPU のパレット補間・色変換に非有限値が流入する経路を減らす。
- 次に確認: パレット境界と CPU/GPU の実画像一致。ビルド・テストは未実施。
# 2026-08-02: Color Balance の入力値ガード

- 関連: `Artifact/src/Effects/ColorCorrection/ColorBalanceEffect.cppm`。
- 事実: shadow/midtone/highlight の各バランス値と range/strength が clamp のみだった。
- 対応: 各 setter で有限値を確認し、異常値は中立値または既定 range/strength に復帰させた。
- 価値: CPU/GPU のトーンウェイトと色差分に非有限値が混入する経路を防ぐ。
- 次に確認: range 境界・preserve luma・CPU/GPU の実画像一致。ビルド・テストは未実施。
# 2026-08-02: Fill の opacity 入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/FillEffect.cppm`。
- 事実: opacity setter が clamp のみで、非有限値を CPU/GPU の fill 設定へ渡し得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常値は既定 opacity 1.0 に戻した。
- 価値: 塗りつぶしの alpha 合成係数への異常値伝播を防ぐ。
- 次に確認: preserve alpha と GPU fallback の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Gradient Ramp の座標・opacity入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/GradientRampEffect.cppm`。
- 事実: start/end point と opacity の setter が clamp のみで、非有限値が設定に残り得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常な座標は start=(0,0)、end=(1,1)、opacity は 1.0 に復帰。
- 価値: CPU/GPU の勾配方向・alpha計算への非有限値伝播を防ぐ。
- 次に確認: 各 preset の勾配方向と preserve alpha。ビルド・テストは未実施。
# 2026-08-02: Selective Color の調整値ガード

- 関連: `Artifact/src/Effects/ColorCorrection/SelectiveColorEffect.cppm`。
- 事実: strength と各色域の CMYK 調整値が clamp のみで、非有限値が設定に残り得た。
- 対応: 有限値確認後に strength は 0..1、調整値は -1..1 に制約し、異常値は既定値へ復帰させた。
- 価値: 色域別補正の CPU/GPU 計算へ非有限値が伝播する経路を防ぐ。
- 次に確認: relative mode と preserve luma の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Tritone のトーンパラメータガード

- 関連: `Artifact/src/Effects/ColorCorrection/TritoneEffect.cppm`。
- 事実: balance/softness/masterStrength/colorMix が clamp のみで、非有限値を保持し得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常値は TritoneSettings の既定値へ復帰させた。
- 価値: CPU/GPU の三色トーン補間に非有限値が流入する経路を防ぐ。
- 次に確認: preserve luma とプリセット切替時の実画像結果。ビルド・テストは未実施。
# 2026-08-02: Color Wheels の中立値フォールバック

- 関連: `Artifact/src/Effects/ColorCorrection/ColorWheelsEffect.cppm`。
- 事実: lift/gamma/gain/offset の setter が clamp のみで、非有限値が設定に残り得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は lift/offset=0、gamma/gain=1 の中立値へ復帰させた。
- 価値: wheel 計算と GPU パラメータへの非有限値伝播を防ぐ。
- 次に確認: master と RGB 個別調整の CPU/GPU 実画像一致。ビルド・テストは未実施。
# 2026-08-02: Photo Filter の入力値ガード

- 関連: `Artifact/src/Effects/ColorCorrection/PhotoFilterEffect.cppm`。
- 事実: density/brightness/contrast/saturationBoost が clamp のみで、非有限値が設定に残り得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は PhotoFilterSettings の既定値へ復帰させた。
- 価値: CPU/GPU のフィルタ色・明度・彩度計算への非有限値伝播を防ぐ。
- 次に確認: プリセット切替と preserve luma の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Channel Mixer の行列入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/ChannelMixerEffect.cppm`。
- 事実: strength は clamp のみ、9要素の mixer 行列は有限値確認なしで設定されていた。
- 対応: strength を既定値へ復帰し、行列要素の非有限値を単位行列相当の要素へ置換した。
- 価値: CPU/GPU のチャンネル混合と shader 定数へ NaN/infinity が伝播する経路を防ぐ。
- 次に確認: モノクロ・preserve luma とプリセット切替の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Hue and Saturation の入力ガード

- 関連: `Artifact/include/Effects/ColorCorrection/HueAndSaturation.ixx`、`Artifact/src/Effects/ColorCorrection/HueAndSaturation.cppm`。
- 事実: 公開 setter と property 経路の両方が clamp のみで、非有限値を内部へ保持し得た。
- 対応: hue/saturation/lightness を有限値確認後に既存範囲へ制約し、異常値は 0/1/0 の中立値へ復帰させた。
- 価値: CPU/GPU の HSV/HSL 計算への非有限値伝播を防ぐ。
- 次に確認: colorize と hue wrap の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Levels のレベル値・ガンマ入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/LevelsEffect.cppm`。
- 事実: input/output level は値を直接保持し、input gamma も clamp のみだったため、非有限値が正規化・指数計算へ到達し得た。
- 対応: 各 setter で有限値を確認し、異常値は inputBlack/outputBlack=0、inputWhite/outputWhite=255、gamma=1 に復帰させた。
- 価値: レベル計算の分母・ガンマ指数と CPU/GPU 設定への非有限値伝播を防ぐ。
- 次に確認: per-channel と input black/white の境界受入れ。ビルド・テストは未実施。
# 2026-08-02: Grayscale の strength 入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/GrayscaleEffect.cppm`。
- 事実: strength setter が clamp のみで、非有限値が CPU/GPU のグレースケール混合へ流入し得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常値は既定 strength 1.0 に復帰させた。
- 価値: モノクロ化と元画像の混合係数への NaN/infinity 伝播を防ぐ。
- 次に確認: 3種の mode と GPU fallback の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Brightness/Contrast の入力ガード

- 関連: `Artifact/include/Effects/ColorCorrection/BrightnessEffect.ixx`。
- 事実: brightness/contrast/highlights/shadows の公開 setter が clamp のみだった。
- 対応: 有限値確認後に -1..1 へ制約し、異常値は中立値 0 に復帰させた。
- 価値: CPU/GPU の明度・コントラスト係数へ非有限値が伝播する経路を防ぐ。
- 次に確認: highlights/shadows の実画像結果と GPU fallback。ビルド・テストは未実施。
# 2026-08-02: Invert の strength 入力ガード

- 関連: `Artifact/src/Effects/ColorCorrection/InvertEffect.cppm`。
- 事実: strength setter が clamp のみで、非有限値が CPU/GPU の反転混合係数へ流入し得た。
- 対応: 有限値確認後に 0..1 へ制約し、異常値は既定 strength 1.0 に復帰させた。
- 価値: RGB/チャンネル反転の混合計算を非有限値から保護する。
- 次に確認: RGB/単一チャンネル/Alpha の実画像結果。ビルド・テストは未実施。
# 2026-08-02: Curves のカーブ強度ガード

- 関連: `Artifact/src/Effects/ColorCorrection/CurvesEffect.cppm`。
- 事実: strength が clamp のみで、S-curve の制御点生成と LUT 計算へ非有限値が到達し得た。
- 対応: setter と S-curve 点生成の両方で有限値を確認し、異常値を strength 0 に復帰させた。
- 価値: CPU/GPU のカーブ/LUT生成を非有限入力から保護する。
- 次に確認: 各 preset と posterize levels の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Exposure の指数計算入力ガード

- 関連: `Artifact/include/Effects/ColorCorrection/ExposureEffect.ixx`。
- 事実: exposure/offset/gamma の公開 setter が clamp のみで、gamma は逆数・pow 計算へ直結していた。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は exposure/offset=0、gamma=1 に復帰させた。
- 価値: CPU/GPU の露光・ガンマ計算への非有限値伝播を防ぐ。
- 次に確認: EV境界、offset、gamma の CPU/GPU 実画像一致。ビルド・テストは未実施。
# 2026-08-02: Kuwahara のカーネル入力ガード

- 関連: `Artifact/src/Effects/Kuwahara/KuwaharaEffect.cppm`。
- 事実: radius/sharpness が clamp のみで、radius はカーネルサイズ計算へ直結していた。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は radius=5、sharpness=0.5 に復帰させた。
- 価値: CPU/GPU の局所統計計算とカーネル生成を非有限入力から保護する。
- 次に確認: anisotropic モードを含む実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Aperture Shape Blur の入力ガード

- 関連: `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm`。
- 事実: radius/rotation/edge brightness/highlight boost の property 入力が非有限値を保持し得た。radius はカーネルサイズ計算へ直結する。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は radius=18、rotation=0、edge=0.2、boost=0.35 に復帰させた。
- 価値: aperture blur のカーネル生成・PSF計算への異常値伝播を防ぐ。
- 次に確認: PSF画像経路と shape/rotation の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Anisotropic Flow Blur の入力ガード

- 関連: `Artifact/src/Effects/Blur/AnisotropicFlowBlurEffect.cppm`。
- 事実: blur amount、tensor scale、edge adherence の property 入力が clamp のみだった。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は既定値へ復帰させた。
- 価値: ベクトル場・積分スケール・ぼかし量の計算を非有限入力から保護する。
- 次に確認: tensor integration と edge adherence の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Blur の radius/strength 入力ガード

- 関連: `Artifact/include/Effects/Blur/BlurEffect.ixx`、`Artifact/src/Effects/Blur/BlurEffect.cppm`。
- 事実: radius は max のみ、strength/edge threshold は clamp のみで、公開 setter と CPU 実装の双方に非有限値の余地があった。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は radius=10、strength=1、edge threshold=0.1 に復帰させた。
- 価値: Gaussian/edge blur のカーネル・混合計算を非有限入力から保護する。
- 次に確認: blur mode、premultiplied alpha、GPU fallback の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Reaction Diffusion Blur の入力ガード

- 関連: `Artifact/src/Effects/Blur/ReactionDiffusionBlurEffect.cppm`。
- 事実: blur radius/feed/kill/pattern strength/evolution の property 入力に非有限値の余地があり、iterations は有限でない整数変換以外の問題はなかった。
- 対応: 浮動小数値を有限値確認後に既存範囲へ制約し、異常値は各既定値へ復帰させた。
- 価値: 反応拡散の反復・模様生成・ぼかし係数への非有限値伝播を防ぐ。
- 次に確認: iterations と evolution の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Gaussian Blur の sigma 入力ガード

- 関連: `Artifact/include/Effects/GauusianBlur.ixx`。
- 事実: CPU/GPU 実装の sigma setter が値を直接保持し、カーネルサイズ・blur計算へ非有限値が到達し得た。
- 対応: 有限値確認後に 0..64 へ制約し、異常値は既定 sigma 5.0 に復帰させた。
- 価値: CPU/GPU の Gaussian kernel と ROI計算を非有限入力から保護する。
- 次に確認: sigma=0 の無効化動作と GPU fallback。ビルド・テストは未実施。
# 2026-08-02: Sharpen の強調パラメータガード

- 関連: `Artifact/src/Effects/Sharpen/SharpenEffect.cppm`。
- 事実: amount/sigma/threshold が clamp のみで、sigma は Gaussian kernel と blur sigma に直結していた。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は amount=1、sigma=1、threshold=0 に復帰させた。
- 価値: シャープ化のカーネル生成と閾値判定を非有限入力から保護する。
- 次に確認: sigma=0 と threshold 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Glow の多層パラメータガード

- 関連: `Artifact/src/Effects/Glow/GlowEffect.cppm`。
- 事実: CPU/GPU の glow gain、layer count、sigma、growth、alpha 系 setter が値を直接保持していた。
- 対応: 両実装の setter を有限値確認・範囲制約付きにし、多層数を 1..16 に制限した。
- 価値: 多段 Gaussian のカーネル・alpha 正規化・GPU定数への異常値伝播と過大反復を防ぐ。
- 次に確認: 多層 glow の ROI と CPU/GPU 実画像一致。ビルド・テストは未実施。
# 2026-08-02: Drop Shadow の影パラメータガード

- 関連: `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`。
- 事実: distance/angle/softness/opacity の setter が max/clamp または直接代入で、非有限値が影の三角関数・blur・alpha計算へ到達し得た。
- 対応: 有限値確認後に既存制約を適用し、異常値は distance=5、angle=135、softness=8、opacity=75 に復帰させた。
- 価値: CPU/GPU の影位置・ぼかし・不透明度計算を非有限入力から保護する。
- 次に確認: GPU fallback と shadow color / premultiplied alpha の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Inner Shadow の影パラメータガード

- 関連: `Artifact/src/Effects/InnerShadow/InnerShadowEffect.cppm`。
- 事実: distance/angle/softness/opacity の setter が max/clamp または直接代入で、非有限値が影位置・blur・alpha計算へ到達し得た。
- 対応: 有限値確認後に既存制約を適用し、異常値は distance=5、angle=135、softness=8、opacity=75 に復帰させた。
- 価値: 内側影のオフセット・カーネル・不透明度計算を非有限入力から保護する。
- 次に確認: CPU/GPU fallback と境界 alpha の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Find Edges の amount 入力ガード

- 関連: `Artifact/src/Effects/FindEdges/FindEdgesEffect.cppm`。
- 事実: amount setter が clamp のみで、非有限値が CPU の edge/color 混合と GPU 定数へ流入し得た。
- 対応: 有限値確認後に 0..5 へ制約し、異常値は既定 amount 1.0 に復帰させた。
- 価値: エッジ強調と元画像の混合計算を非有限入力から保護する。
- 次に確認: invert モードと amount 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Directional Glow のストリーク入力ガード

- 関連: `Artifact/include/Effects/DirectionalGlowEffect.ixx`。
- 事実: threshold/intensity/length/weight/angle の setter が max/clamp または直接代入で、非有限値がストリーク計算へ到達し得た。
- 対応: 有限値確認後に既存制約を適用し、異常値は既定の threshold=0.8、length=64/128、weight=0.6/0.4 などへ復帰させた。
- 価値: streak 半径・重み・GPU定数・角度計算を非有限入力から保護する。
- 次に確認: pattern 切替と custom angle の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Chromatic Glow の色収差入力ガード

- 関連: `Artifact/include/Effects/Glow/ChromaticGlowEffect.ixx`。
- 事実: threshold/radius/intensity/dispersion/tintMix は clamp のみ、angle は直接代入で、非有限値がサンプリング座標・GPU定数へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は既定値へ復帰させた。
- 価値: 色収差の半径・強度・角度・サンプリング計算を非有限入力から保護する。
- 次に確認: 色収差角度と tint mix の CPU/GPU 実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Satin のハイライト入力ガード

- 関連: `Artifact/src/Effects/Satin/SatinEffect.cppm`。
- 事実: distance/angle/softness/opacity の setter が max/clamp または直接代入で、非有限値が satin のオフセット・三角関数・blur・alpha計算へ到達し得た。
- 対応: 有限値確認後に既存制約を適用し、異常値は distance=0、angle=0、softness=5、opacity=50 に復帰させた。
- 価値: CPU/GPU の satin ハイライト計算を非有限入力から保護する。
- 次に確認: invert と色・alpha境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Radial Shadow の中心・距離入力ガード

- 関連: `Artifact/src/Effects/RadialShadow/RadialShadowEffect.cppm`。
- 事実: distance/softness/opacity/centerX/centerY が max/clamp のみで、非有限値が距離・alpha・GPU定数へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は distance=10、softness=8、opacity=0.75、center=(0.5,0.5) に復帰させた。
- 価値: 放射状影の中心距離・blur・不透明度計算を非有限入力から保護する。
- 次に確認: 色と中心位置境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Vignette の画面係数入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/VignetteEffect.cppm`。
- 事実: amount/radius/feather/center が clamp のみで、非有限値が距離・マスク計算へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は amount=.7、radius=.8、feather=.4、center=(.5,.5) に復帰させた。
- 価値: CPU/GPU の画面周辺減光マスクを非有限入力から保護する。
- 次に確認: center と feather 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Film Damage のアナログ効果入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/FilmDamageEffect.cppm`。
- 事実: grain/dust/scratches/gate weave/flicker/film burn は clamp のみ、evolution は直接代入だった。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は各既定値へ復帰させた。
- 価値: ノイズ・傷・揺れ・焼けとフレーム進行の計算を非有限入力から保護する。
- 次に確認: seed と evolution を含むフレーム間再現性。ビルド・テストは未実施。
# 2026-08-02: Mosaic のセルサイズ入力ガード

- 関連: `Artifact/src/Effects/Mosaic/MosaicEffect.cppm`。
- 事実: cell size setter が max のみで、非有限値がブロックサイズから整数化され得た。
- 対応: 有限値確認後に 1 以上へ制約し、異常値は既定 cell size 8 に復帰させた。
- 価値: CPU/GPU のモザイクブロック計算とサンプリングを非有限入力から保護する。
- 次に確認: shape mode と大きな画像サイズでの実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Linear Wipe の境界入力ガード

- 関連: `Artifact/src/Effects/LinearWipe/LinearWipeEffect.cppm`。
- 事実: angle は fmod へ直接渡され、softness/feather は clamp のみだったため、非有限値が wipe の投影・境界計算へ到達し得た。
- 対応: 有限値確認後に angle を 0..360 相当に正規化し、softness/feather を既存範囲へ制約した。
- 価値: CPU/GPU の wipe マスク境界と除算を非有限入力から保護する。
- 次に確認: 角度 wrap と feather 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Bricks のタイル寸法入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/BricksEffect.cppm`。
- 事実: brick width/height、mortar、offset が max/clamp のみで、非有限値が除算・タイル配置へ到達し得た。
- 対応: 有限値確認後に既存下限・範囲を適用し、異常値は width=64、height=32、mortar=3、offset=.5 に復帰させた。
- 価値: タイル行列・モルタル計算を非有限入力から保護する。
- 次に確認: offset の周期境界と大画像での実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Chromatic Aberration の座標入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/ChromaticAberrationEffect.cppm`。
- 事実: red/blue shift と center が clamp のみで、非有限値が画素オフセット・中心距離計算へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は shift=2、center=(.5,.5) に復帰させた。
- 価値: CPU/GPU の色チャンネルサンプリング座標を非有限入力から保護する。
- 次に確認: 画像端・中心ピクセル・GPU fallback の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Kaleidoscope の幾何入力ガード

- 関連: `Artifact/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm`。
- 事実: center/rotation/zoom/feather が clamp/max/fmod または直接計算へ渡され、非有限値が分割角度・半径・フェード計算へ到達し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は center=.5、rotation=0、zoom=1、feather=0 に復帰させた。
- 価値: CPU/GPU の万華鏡投影と境界フェードを非有限入力から保護する。
- 次に確認: segments、mirror、rotation wrap の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Lens Distortion の投影入力ガード

- 関連: `Artifact/include/Effects/LensDistortion/LensDistortionEffect.ixx`。
- 事実: CPU/GPU の distortion/center/zoom setter が値を直接保持していた。
- 対応: distortion を -100..100、center を 0..1、zoom を 0.01 以上に有限値確認付きで制約した。
- 価値: CPU/GPU の投影座標・半径・除算計算を非有限入力から保護する。
- 次に確認: invert distortion と中心境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Radio Waves の時間・発生パラメータガード

- 関連: `Artifact/src/Effects/Generate/RadioWavesEffect.cppm`。
- 事実: origin/frequency/expansion/lifespan/stroke/opacity/currentTime が max/clamp のみだった。
- 対応: 有限値確認後に既存下限・範囲を適用し、異常値は各既定値へ復帰させた。
- 価値: 波の発生間隔・半径・寿命・アルファ・時間進行計算を非有限入力から保護する。
- 次に確認: currentTime と frequency 境界のフレーム再現性。ビルド・テストは未実施。
# 2026-08-02: Auto Mosaic の境界フェザー入力ガード

- 関連: `Artifact/include/Effects/AutoMosaicEffect.ixx`。
- 事実: feather setter が max のみで、非有限値が自動モザイク領域の境界処理へ到達し得た。
- 対応: 有限値確認後に 0 以上へ制約し、異常値は既定 feather 0 に復帰させた。
- 価値: 顔検出・カスタム領域どちらの境界フェードも非有限入力から保護する。
- 次に確認: 顔検出とカスタム領域の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Edge/Rim Light の入力ガード

- 関連: `Artifact/src/Effects/Rasterizer/EdgeEffect.cppm`。
- 事実: Edge と Rim Light の mode/intensity/threshold、角度・幅・softness・mix が clamp/fmod または直接計算へ非有限値を渡し得た。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は各エフェクトの既定値へ復帰させた。
- 価値: エッジ判定・リム角度・線幅・alpha混合計算を非有限入力から保護する。
- 次に確認: Edge/Rim の CPU/GPU 実画像一致と角度 wrap。ビルド・テストは未実施。
# 2026-08-02: Simple Rain の雨滴入力ガード

- 関連: `Artifact/src/Effects/Generate/SimpleRainEffect.cppm`。
- 事実: density/streak/speed/wind/opacity/depth/splash は clamp のみ、evolution は直接代入だった。
- 対応: 有限値確認後に既存範囲へ制約し、異常値は各既定値へ復帰させた。
- 価値: 雨滴数・位置・速度・飛沫・alpha・フレーム進行計算を非有限入力から保護する。
- 次に確認: evolution と seed を含むフレーム間再現性。ビルド・テストは未実施。
# 2026-08-02: Liquify の変形入力ガード

- 関連: `Artifact/include/Effects/Liquify/LiquifyEffect.ixx`。
- 事実: CPU/GPU の amount/radius/center/angle/mesh density setter が値を直接保持していた。
- 対応: amount/radius/center/angle を有限値確認・範囲制約付きにし、mesh density を 4..128 に制限した。
- 価値: ブラシ変形・座標投影・メッシュ計算への非有限値と過大反復を防ぐ。
- 次に確認: 各ブラシ種別と mesh density 境界の実画像受入れ。ビルド・テストは未実施。
# 2026-08-02: Displacement Map の変位入力ガード

- 関連ファイル: `Artifact/src/Effects/DisplacementMap/DisplacementMapEffect.cppm`
- 気づき: 水平・垂直変位量は自己サンプリング座標へ直接加算されるため、非有限値が入ると画素サンプリング全体が壊れる。チャンネル enum も property 経由では任意の整数に変換できる。
- 対応: 変位量を有限値かつ ±4096 に制限し、非有限値は既定値 20 に戻した。チャンネル値は Luminance〜Alpha の範囲へ正規化した。
- 価値または懸念: 不正なプロパティ入力で NaN/Inf 座標や未定義チャンネルが伝播するのを防ぐ。±4096 の上限は実装上の安全上限であり、仕様上の最大値としては未検証。
- 次に確認すべきこと: UI 側で変位量の許容範囲を定義する際に、この上限と負値による反転方向を整合させる。
# 2026-08-02: Spherize の球面変形入力ガード

- 関連ファイル: `Artifact/include/Effects/Spherize/SpherizeEffect.ixx`
- 気づき: Spherize は amount、radius、中心座標を CPU/GPU の両実装へ共有し、radius はゼロ除算、座標は画面外の極端なサンプリングへ直結する。
- 対応: amount を有限値かつ -100〜100、radius と中心座標を有限値かつ 0〜1 に制限し、非有限値は既定値へ戻した。CPU/GPU 実装の setter を同じ規則に統一した。
- 価値または懸念: property 経由の NaN/Inf や範囲外値が計算・GPU 定数へ流れるのを防ぐ。
- 次に確認すべきこと: 半径 0 を UI で許容する仕様が必要か、既定の最小半径を設けるべきかを確認する。
# 2026-08-02: Feedback の履歴変形入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/FeedbackEffect.cppm`
- 気づき: Feedback は前フレームのサンプリング座標を zoom・回転・中心オフセットから算出するため、非有限値が入ると履歴参照が破綻する。
- 対応: amount、decay、中心オフセット、zoom、rotation を有限値として検証し、既存 property 範囲に合わせてクランプした。非有限値は各既定値へ戻した。
- 価値または懸念: property 経由の NaN/Inf が座標計算や重み計算へ伝播するのを防ぐ。rotation の許容範囲は既存 UI 定義（±180）に合わせた。
- 次に確認すべきこと: 履歴サンプラーが異なる解像度を返す場合の座標スケーリングは別課題として確認する。
# 2026-08-02: Chromatic Relief の入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/ChromaticReliefEffect.cppm`
- 気づき: Chromatic Relief は方向からオフセットを計算し、edge softness は GaussianBlur の sigma に使うため、非有限値がそのまま画像処理へ流れると不正な処理条件になる。
- 対応: Relief Amount、Chromatic Offset、Direction、Edge Softness、Mix を有限値として検証し、既存の範囲制限と既定値フォールバックを適用した。
- 価値または懸念: property 入力由来の NaN/Inf による OpenCV パラメータや色ブレンド値の破綻を防ぐ。
- 次に確認すべきこと: Direction の UI 表示範囲を決める場合は、内部の任意角度対応と整合させる。
# 2026-08-02: Echo の残像重み入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/EchoEffect.cppm`
- 気づき: Echo の decay と blend operator は複数フレームの重み計算へ直接使われ、非有限値が入ると全エコーの合成結果が壊れる。
- 対応: 両値を有限値として検証してから 0〜1 にクランプし、非有限値は既定値へ戻した。
- 価値または懸念: property 入力由来の NaN/Inf の伝播を防ぐ。echoCount の整数値は既存の 1〜16 制限を維持した。
- 次に確認すべきこと: blend operator の値と実際の合成モードの対応表は別途仕様確認する。
# 2026-08-02: Difference Matte のしきい値入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/DifferenceMatteEffect.cppm`
- 気づき: Difference Matte の threshold はフレーム間差分を二値化する境界値であり、非有限値が入ると比較結果が不定になる。
- 対応: threshold を有限値として検証してから 0〜1 にクランプし、非有限値は既定値 0 に戻した。
- 価値または懸念: property 経由の NaN/Inf がマット生成へ伝播するのを防ぐ。参照フレーム offset の既存 1〜60 制限は維持した。
- 次に確認すべきこと: 異なる解像度の参照フレームを座標スケールするかは別仕様として確認する。
# 2026-08-02: Rasterizer 生成系の入力ガード拡張

- 関連ファイル: `Artifact/src/Effects/Rasterizer/RadialBlurEffect.cppm`, `HalftoneEffect.cppm`, `StripesEffect.cppm`, `HexGridEffect.cppm`
- 気づき: これらの静止画エフェクトは半径・セルサイズ・周波数・角度などを直接ピクセル座標や除算へ使うため、既存の clamp だけでは NaN/Inf が残る。
- 対応: 各 setter で有限値を検証し、既存の下限・UI 想定範囲に加えて安全上限を適用した。非有限値は各エフェクトの既定値へ戻した。
- 価値または懸念: 不正入力による座標計算・除算・GPU/CPU 実装同期の破綻を防ぐ。安全上限は実装上の上限であり、仕様上の最大値としては未検証。
- 次に確認すべきこと: 各 UI の property min/max を setter の安全上限と明示的に揃える。
# 2026-08-02: Voronoi のセル生成入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/VoronoiEffect.cppm`
- 気づき: Voronoi の scale はセル座標の除数、jitter はセル位置の乱数オフセットとして使われ、seed は擬似乱数計算へ渡る。
- 対応: scale を有限値かつ 1〜200、jitter を有限値かつ 0〜2、seed を 0〜9999 に制限し、非有限値の float は既定値へ戻した。
- 価値または懸念: 不正入力でセル分割の除算や乱数座標が破綻するのを防ぐ。上限は既存 property 定義に合わせた。
- 次に確認すべきこと: mode の各値が表示する距離指標と UI 表示名で一致しているか確認する。
# 2026-08-02: Vector Flow Glitch の入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/VectorFlowGlitchEffect.cppm`
- 気づき: Glitch Amount、Frequency、Chromatic Aberration、Edge Flow Influence、Evolution は画像処理設定へ直接渡され、既存の clamp だけでは非有限値を除外できない。
- 対応: 各 property 値を有限値として検証し、既存範囲へクランプした。非有限値はヘッダ既定値に合わせてフォールバックした。
- 価値または懸念: 不正な property 入力が VectorFlowGlitch の内部計算へ伝播するのを防ぐ。
- 次に確認すべきこと: `VectorFlowGlitchSettings` の各範囲定義と UI の min/max を別途揃える。
# 2026-08-02: Vector Blur のモーション入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/VectorBlurEffect.cppm`
- 気づき: shutter angle と exposure compensation はベクトル移動量・合成重みへ使われ、CPU/GPU の両実装へ同期される。
- 対応: 両 float 値を有限値として検証し、既存 property 範囲（角度 0〜720、露出 0〜4）へクランプした。非有限値は既定値へ戻した。
- 価値または懸念: NaN/Inf がモーションブラーのサンプリングや GPU 定数へ伝播するのを防ぐ。samples の既存 2〜32 制限は維持した。
- 次に確認すべきこと: 速度探索の固定範囲（±16 px）が高解像度素材で十分かは別途確認する。
# 2026-08-02: Glitch の乱数・強度入力ガード

- 関連ファイル: `Artifact/src/Effects/Rasterizer/GlitchEffect.cppm`
- 気づき: Glitch の intensity、color shift、scanlines は画素変形と合成量に使われ、seed はフレーム番号と結合して乱数初期化へ使われる。
- 対応: float 値を有限値として検証して 0〜1 にクランプし、seed を既存 property 範囲の 0〜9999 に制限した。非有限値は既定値へ戻した。
- 価値または懸念: 不正入力による画素処理・乱数系列の不安定化を防ぐ。
- 次に確認すべきこと: フレーム番号と seed の加算が整数オーバーフローしないよう、長時間タイムラインの型変換を確認する。
# 2026-08-02: Lift Gamma Gain の色調入力ガード

- 関連ファイル: `Artifact/include/Effects/LiftGammaGainEffect.ixx`
- 気づき: Lift/Gamma/Gain は GPU の pow と乗算へ直接渡され、既存の clamp だけでは NaN/Inf を除外できない。
- 対応: RGB 各 setter で有限値を検証し、Lift は -1〜1、Gamma は 0.1〜5、Gain は 0〜4 にクランプした。非有限値は中立値へ戻した。
- 価値または懸念: CPU/GPU の色調計算へ不正値が伝播するのを防ぐ。Gamma と Gain の非有限値はそれぞれ 1 を中立値とした。
- 次に確認すべきこと: Lift の符号と GPU 側の RGB/BGR 変換が UI のチャンネル表記と一致するか確認する。
# 2026-08-02: Dithering のアルゴリズム enum 入力ガード

- 関連ファイル: `Artifact/src/Effects/Dithering/DitheringEffect.cppm`
- 気づき: Dithering の Algorithm は property から整数を enum へ直接変換しており、範囲外の値が CPU/GPU のアルゴリズム分岐へ渡る可能性があった。
- 対応: Bayer〜Stucki の 8 種類に対応する 0〜7 へ正規化してから enum を保持する。
- 価値または懸念: 未定義 enum 値による処理分岐の不整合を防ぐ。color count / amount / pattern scale は既存の入力ガードを維持した。
- 次に確認すべきこと: Algorithm の property UI が enum 名を表示できる場合は、整数表示から名称表示へ改善する。
# 2026-08-02: Procedural Texture Generator の property 導線

- 関連ファイル: `Artifact/include/Effects/Generator/ProceduralTextureGenerator.ixx`
- 気づき: Generator は preset を生成できる一方、getProperties() が空で、幅・高さ・seed・preset を通常の property 導線から編集できなかった。
- 対応: 4 項目の property を公開し、setPropertyValue() を追加した。preset を 0〜5、サイズを 1〜8192 に正規化した。
- 価値または懸念: UI/保存経路から生成設定を編集でき、極端なサイズによる過大なメモリ要求を一定範囲で防げる。プリセット enum の 0〜5 は ArtifactCore の定義に依存する。
- 次に確認すべきこと: Generator の property 名を既存の生成 UI と統一し、seamless/outputFormat などの設定を公開するか判断する。
# 2026-08-02: Procedural Texture の一括設定経路を正規化

- 関連ファイル: `Artifact/include/Effects/Generator/ProceduralTextureGenerator.ixx`
- 気づき: 個別 setter にはサイズ制限を追加したが、setSettings() は設定構造体をそのまま受け入れるため、別経路から範囲外サイズが残る可能性があった。
- 対応: 一括設定後にも width/height を 1〜8192 へ正規化するようにした。
- 価値または懸念: UI 以外の設定復元・プリセット経路でも、生成サイズの安全条件を一貫して適用できる。
- 次に確認すべきこと: settings 内の各 float パラメータにも同様の正規化が必要か、ArtifactCore 側の生成関数の責務と分けて確認する。
# 2026-08-02: Add Noise のサイズ・seed 入力ガード

- 関連ファイル: `Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm`
- 気づき: Add Noise の size は CPU ノイズ座標のスケールとして使われ、seed は CPU/GPU の乱数初期化へ渡る。size の下限だけでは極端な値を許していた。
- 対応: size を有限値かつ 0.1〜64、seed を 0〜9999 に制限した。非有限値は既定値へ戻した。
- 価値または懸念: 過大なノイズスケールや未制限 seed による処理の不安定化を防ぐ。64 の上限は実装上の安全上限であり、仕様上の最大値としては未検証。
- 次に確認すべきこと: GPU 経路で size を未使用のままにするか、CPU と同じノイズスケールを GPU にも実装するか確認する。
# 2026-08-02: Turbulent Displace の変位入力ガード

- 関連ファイル: `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm`
- 気づき: amount、size、octaves、domain warp はノイズ座標・反復・変位量へ直接使われ、size は除算、octaves はループ回数に使われる。
- 対応: amount 0〜1000、size 1〜1024、octaves 1〜12、seed 0〜9999、domain warp 0〜100 に正規化した。float の非有限値は既定値へ戻した。
- 価値または懸念: NaN/Inf、過大な反復回数、極端な変位量による処理時間・座標計算の破綻を防ぐ。上限の一部は実装上の安全上限で未検証。
- 次に確認すべきこと: UI property に min/max を設定し、setter と表示範囲を一致させる。
# 2026-08-02: Wave の波形入力ガード

- 関連ファイル: `Artifact/include/Effects/Wave/WaveEffect.ixx`
- 気づき: Wave の amplitude と frequency はピクセル座標へ加える変位の計算に、phase は三角関数へ、waveType/orientation は分岐へ直接使われる。
- 対応: CPU/GPU 実装の setter を統一し、amplitude を ±4096、frequency を ±10 に制限、非有限値を既定値へ戻した。phase も有限値を確認し、enum 相当の整数は 0/1 に正規化した。
- 価値または懸念: NaN/Inf や範囲外の整数が座標計算・GPU 定数・分岐へ伝播するのを防ぐ。上限は実装上の安全上限として未検証。
- 次に確認すべきこと: Wave の UI property に min/max を設定し、負の frequency/amplitude を仕様として許容するか確認する。
# 2026-08-02: Liquify のブラシ enum・seed 入力ガード

- 関連ファイル: `Artifact/include/Effects/Liquify/LiquifyEffect.ixx`
- 気づき: Liquify の brush type は property 整数から enum へ変換され、turbulence seed はノイズ関数へ直接渡る。既存の float setter にはガードがあるが、この 2 値は未制限だった。
- 対応: CPU/GPU 実装の brush type を Push〜Pucker（0〜5）へ正規化し、seed を 0〜9999 に制限した。
- 価値または懸念: 未定義 brush 分岐や極端な seed の伝播を防ぐ。既存の amount/radius/center/angle/mesh density のガードは維持した。
- 次に確認すべきこと: UI の brush type 表示が enum 名と整数値のどちらを想定しているか確認する。
# 2026-08-02: Bevel の softness 上限ガード

- 関連ファイル: `Artifact/src/Effects/Bevel/BevelEffect.cppm`
- 気づき: Bevel の softness は CPU 側の Gaussian kernel サイズへ変換されるため、下限だけの制限では極端な入力が巨大なカーネルを生成し得る。
- 対応: softness を有限値かつ 0〜64 にクランプし、非有限値は既定値 2 に戻した。
- 価値または懸念: 異常な property 入力による CPU 処理時間・メモリ負荷の急増を防ぐ。64 は実装上の安全上限として未検証。
- 次に確認すべきこと: UI の Softness 最大値を setter の上限と一致させる。
# 2026-08-02: Optics Compensation の投影入力ガード

- 関連ファイル: `Artifact/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm`
- 気づき: Center X/Y は歪み中心、FOV は投影係数へ使われるため、既存の範囲 clamp だけでは NaN/Inf を除外できない。
- 対応: center X/Y と FOV を有限値として検証し、既存範囲へクランプした。非有限値は中心 0.5、FOV 45 の既定値へ戻した。
- 価値または懸念: property 入力由来の不正値が歪み座標・投影計算へ伝播するのを防ぐ。Direction の符号正規化は既存動作を維持した。
- 次に確認すべきこと: FOV 1 度付近の投影計算が意図した補正強度になるか確認する。
# 2026-08-02: Add Noise の Size を CPU/GPU で統一

- 関連ファイル: `Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm`
- 気づき: Size property は CPU setter と property には存在したが、CPU の noiseAt 座標にも GPU shader の乱数座標にも反映されておらず、実質未実装だった。
- 対応: Size でピクセル座標を量子化してノイズ粒度を変える処理を CPU に追加し、同じ値を GPU constant buffer と HLSL へ渡して同じ座標規則を適用した。
- 価値または懸念: Size 設定が実際のノイズ粒度へ反映され、CPU fallback と GPU 経路の挙動差を縮小できる。GPU/CPU の乱数実装自体は既存どおり別実装である。
- 次に確認すべきこと: 実画像で Size 1 未満・大きい Size の見た目と CPU/GPU の粒度差を確認する。
# 2026-08-02: Physical Halation の光学入力ガード

- 関連ファイル: `Artifact/src/Effects/Glow/PhysicalHalationEffect.cppm`
- 気づき: Threshold、Spread、Intensity、Red Diffusion、Softness は Halation の光量・拡散処理へ直接渡され、既存 clamp だけでは非有限値を除外できない。
- 対応: 各 property を有限値として検証し、既存範囲へクランプした。非有限値はヘッダ既定値へ戻した。
- 価値または懸念: NaN/Inf が光学処理設定へ伝播するのを防ぐ。既存の CPU-only 経路は変更していない。
- 次に確認すべきこと: ArtifactCore 側の Halation::Settings の想定範囲と UI property の min/max を揃える。
# 2026-08-02: Residual Glow の履歴入力ガード

- 関連ファイル: `Artifact/src/Effects/Glow/ResidualGlowEffect.cppm`
- 気づき: Residual Glow の radius は kernel サイズへ、decay/history mix は履歴合成の重みへ直接使われる。既存 clamp だけでは非有限値が残る。
- 対応: threshold、radius、intensity、decay、history mix を有限値として検証し、既存範囲へクランプした。非有限値は各既定値へ戻した。
- 価値または懸念: 不正入力による kernel 計算・履歴合成の破綻を防ぐ。履歴サンプリング経路自体は変更していない。
- 次に確認すべきこと: 長時間履歴の decay 上限 0.995 がメモリ保持・残像量の想定と合うか確認する。
# 2026-08-02: Luminescence Caustics の入力ガード

- 関連ファイル: `Artifact/src/Effects/Glow/LuminescenceCausticsEffect.cppm`
- 気づき: Scale は逆数、Evolution は位相、Edge Weight/Intensity/Color Shift はハイライト合成へ直接使われる。既存 clamp だけでは非有限値が残る。
- 対応: 6 つの property を有限値として検証し、既存範囲へクランプした。非有限値は各既定値へ戻した。
- 価値または懸念: 不正入力による逆数・三角関数・光量合成の破綻を防ぐ。既存の CPU 実装と property 名は変更していない。
- 次に確認すべきこと: Evolution の単位（度）を UI 表示やアニメーション側と統一する。
# 2026-08-02: White Balance property 範囲の明示

- 関連ファイル: `Artifact/src/Effects/WhiteBalanceEffect.cppm`
- 気づき: setter 側では温度・Tint・Brightness の範囲を正規化していたが、getProperties() に min/max がなく、編集 UI が入力範囲を認識できなかった。
- 対応: Temperature 1000〜20000 K、Tint/Brightness -1〜1、Preset 0〜6 を property metadata に設定した。
- 価値または懸念: UI と保存・編集経路が setter の既存仕様を共有しやすくなる。Preset の整数表示は既存仕様を維持した。
- 次に確認すべきこと: Preset を整数ではなく選択肢名として扱える property API があるか確認する。
# 2026-08-02: Turbulent Displace の property 範囲同期

- 関連ファイル: `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm`
- 気づき: setter には安全上限を追加済みだったが、getProperties() に min/max がなく、UI からは上限が見えなかった。
- 対応: Amount、Size、Octaves、Seed、Domain Warp の property metadata を setter の範囲と同期した。
- 価値または懸念: UI と復元経路が同じ入力範囲を共有し、過大な変位・反復数を事前に抑えられる。
- 次に確認すべきこと: 同様に metadata が未設定の静止画エフェクトを順次揃える。
# 2026-08-02: Optics Compensation property 範囲同期

- 関連ファイル: `Artifact/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm`
- 気づき: center/FOV の setter には範囲制限がある一方、getProperties() に min/max がなく、UI が制約を事前に表示できなかった。
- 対応: Center X/Y 0〜1、FOV 1〜180、Direction -1〜1 の property metadata を追加した。
- 価値または懸念: UI と setter の範囲が一致し、不正な投影入力を編集段階で抑えやすくなる。
- 次に確認すべきこと: Direction を二択 enum として表示できる property 表現があるか確認する。
# 2026-08-02: Stroke の輪郭入力ガード

- 関連ファイル: `Artifact/src/Effects/Stroke/StrokeEffect.cppm`
- 気づき: Width は CPU の Gaussian kernel と GPU の探索半径へ使われ、Opacity はアルファ合成へ直接使われる。既存の下限・clamp だけでは非有限値や GPU 上限超過を防げなかった。
- 対応: 色は有効な QColor のみ受け入れ、Width を有限値かつ 0〜64、Opacity を有限値かつ 0〜100 に制限した。property metadata も同じ範囲へ同期した。
- 価値または懸念: 不正入力による巨大 kernel・探索範囲や合成値の破綻を防ぐ。64 は GPU 探索実装の上限に合わせた安全上限。
- 次に確認すべきこと: Stroke Color の property picker が不正 QColor を返す場合の UI 側挙動を確認する。
# 2026-08-02: Edge Bloom の光量入力ガード

- 関連ファイル: `Artifact/include/Effects/Glow/EdgeBloomEffect.ixx`
- 気づき: Edge Bloom の radius は CPU の kernel サイズ、threshold/edge boost/amount/tint mix はハイライト抽出と合成へ使われる。既存 clamp だけでは非有限値が残る。
- 対応: 5 つの setter で有限値を検証し、既存範囲へクランプした。非有限値は各既定値へ戻した。
- 価値または懸念: 不正入力による kernel・閾値・光量計算の破綻を防ぐ。CPU/GPU 共通の setter で同じ条件を適用する。
- 次に確認すべきこと: 実画像で radius 上限 32 と CPU/GPU の bloom 広がりが十分か確認する。
# 2026-08-02: Twist Transform の角度入力ガード

- 関連ファイル: `Artifact/include/Effects/Transform/TwistTransform.ixx`
- 気づき: Twist Transform の Angle は property から直接 float へ代入され、非有限値や極端な角度をそのまま保持していた。
- 対応: Angle を有限値かつ ±720 度に制限し、非有限値は既定値 45 度へ戻した。property metadata にも同じ範囲を追加した。
- 価値または懸念: 変形係数へ不正値が伝播するのを防ぐ。実際の field 適用・再描画通知は既存コメントどおり未実装であり、今回の範囲では変更していない。
- 次に確認すべきこと: GeometryTransform の field 適用契約と再描画経路を確認してから、変形本体を実装する。
# 2026-08-02: Bend Transform の property 導線

- 関連ファイル: `Artifact/include/Effects/Transform/BendTransform.ixx`
- 気づき: Bend Transform は angle/direction/size の setter だけがあり、property 公開と setPropertyValue() が未実装だった。
- 対応: 3 パラメータの有限値・範囲ガード、getProperties()、setPropertyValue()、min/max metadata を追加した。
- 価値または懸念: GeometryTransform の設定を既存 property 編集・保存経路へ載せられる。field 適用と実際の変形本体は既存の未実装範囲であり、今回も変更していない。
- 次に確認すべきこと: Bend の field 適用契約と再描画経路を確認してから変形本体を実装する。
# 2026-08-02: SurfaceFX の有限値正規化

- 関連ファイル: `Artifact/include/Effects/SurfaceFX/SurfaceFXEffect.ixx`
- 気づき: SurfaceFX は矩形・要素の property を既に正規化していたが、std::clamp は NaN/Inf を排除しないため、anchor/element 座標や時間値が不正状態になり得た。
- 対応: property setter 内に有限値付き clamp を追加し、座標・サイズ・feather・強度・opacity・roughness・rotation・in/out time を補正した。既存の矩形境界・時間順序の正規化は維持した。
- 価値または懸念: SurfaceFX の設定復元・編集経路で非有限値がレンダリング設定へ残るのを防ぐ。out time の非有限値は「未指定」を表す -1 に戻した。
- 次に確認すべきこと: SurfaceFXData のデフォルト値と property fallback の中立値を完全に揃える。
# 2026-08-02: Corner Pin の座標入力ガード

- 関連ファイル: `Artifact/src/Effect/ArtifactCornerPinEffect.cppm`
- 気づき: Corner Pin の 8 点座標は property から double を直接保持し、非有限値や property metadata の上限超過をそのまま homography 計算へ渡していた。
- 対応: 入力を有限値として検証し、既存 property の ±100000 範囲へクランプした。非有限値は 0 に戻した。
- 価値または懸念: OpenCV の homography/warpPerspective へ NaN/Inf が伝播するのを防ぐ。座標系（ピクセル値か正規化値か）は既存仕様を維持し、今回変更していない。
- 次に確認すべきこと: 退化四辺形で computeHomography が有限だが不安定な行列を返す場合の判定基準を確認する。
# 2026-08-02: PBR Material の材質入力ガード

- 関連ファイル: `Artifact/include/Effects/Render/PBRMaterialEffect.ixx`
- 気づき: PBR 材質の QColor は無効値を受け入れ、float 値は std::clamp/max だけで NaN/Inf を除外していなかった。Emissive Intensity は property metadata の上限 100 と setter が不一致だった。
- 対応: 色は有効な QColor のみ受け入れ、Metallic/Roughness/AO/Emissive Intensity を有限値として検証した。Emissive Intensity を 0〜100 に揃えた。
- 価値または懸念: レンダリング材質へ不正な色・係数が流れるのを防ぎ、property metadata と setter の範囲を一致させる。
- 次に確認すべきこと: toMaterial() が emissiveColor/AO/emissiveIntensity を Material へ反映できる API を確認する。
# 2026-08-02: IES Light の property setter 統一

- 関連ファイル: `Artifact/include/Effects/Light/IESLightEffect.ixx`
- 気づき: IES Light は setter に範囲処理がある一方、setPropertyValue() がメンバーへ直接代入しており、範囲・有限値検証を迂回していた。IES path も空白だけの値をそのまま保持していた。
- 対応: Intensity/Temperature を有限値付きの setter 経由へ統一し、property min/max を追加した。UseTemperature も setter 経由にし、IES path は trim してから load するようにした。
- 価値または懸念: UI・保存復元経路でも光源係数と温度の入力条件が一貫する。Intensity の 1000 上限は実装上の安全上限として未検証。
- 次に確認すべきこと: loadIES() の実ファイル存在・LM-63 parse 結果を返す責務を render pipeline と整理する。
# 2026-08-02: HDR Display の出力設定入力ガード

- 関連ファイル: `Artifact/include/Effects/ColorCorrection/HDRDisplayEffect.ixx`
- 気づき: HDR Display は property setter を迂回して係数を直接代入し、DisplayMode も任意整数を enum として保持できた。
- 対応: Peak Nits、Paper White、Saturation Boost を有限値付き setter 経由へ統一し、DisplayMode を 0〜3 に正規化した。property metadata にも範囲を追加した。
- 価値または懸念: HDR 出力係数・モードの不正値が後段のトーンマッピングへ伝播するのを防ぐ。
- 次に確認すべきこと: HDR10/HLG/scRGB の実際の出力変換実装がどの層にあるか確認する。
# 2026-08-02: Linear Field の評価入力ガード

- 関連ファイル: `Artifact/include/Effects/Field/LinearField.ixx`
- 気づき: start/end は有限値へ正規化されていたが、evaluateAt() の worldPos は未検証で、非有限座標が influence へ伝播する可能性があった。
- 対応: worldPos の各成分と投影 t を有限値検証し、不正値は influence 0 にフォールバックした。
- 価値または懸念: Field 評価結果へ NaN/Inf が残るのを防ぐ。GPU バッファ生成 TODO は API 契約未確認のため変更していない。
- 次に確認すべきこと: ArtifactAbstractField の GPU データ契約を確認し、CPU と同じ LinearField 情報を安全に公開できるか判断する。
# 2026-08-02: Field 評価座標の有限値ガード拡張

- 関連ファイル: `Artifact/include/Effects/Field/RadialField.ixx`, `SphericalField.ixx`, `BoxField.ixx`
- 気づき: 各 Field の設定 setter は一部の値を正規化していたが、evaluateAt() の worldPos と距離計算結果は未検証だった。
- 対応: 3 種類の Field で worldPos 各成分と算出距離を有限値検証し、不正値は influence 0 にフォールバックした。
- 価値または懸念: Field 評価結果の NaN/Inf 伝播を防ぐ。GPU バッファ生成 TODO は引き続き API 契約未確認のため未変更。
- 次に確認すべきこと: ArtifactAbstractField の評価呼び出し側で不正座標を早期除外できるか確認する。
# 2026-08-02: PBR Material の材質反映漏れを補完

- 関連ファイル: `Artifact/include/Effects/Render/PBRMaterialEffect.ixx`, `ArtifactCore/include/Material/Material.ixx`
- 気づき: PBRMaterialEffect は albedo/metallic/roughness だけを toMaterial() へ反映し、既に保持していた emissive color/intensity と ambient occlusion が Material へ渡っていなかった。
- 対応: 既存 Material API の setEmissionColor、setEmissionStrength、setOcclusionStrength を使い、3 値を toMaterial() へ反映した。
- 価値または懸念: property 編集した発光・AO 設定が実際の材質変換結果にも反映される。
- 次に確認すべきこと: MaterialRender 側が emission/occlusion を利用する経路と、テクスチャ未設定時の fallback を確認する。
# 2026-08-02: FrameCache の eviction policy 正規化

- 関連ファイル: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 気づき: CachePolicy は 5 種類の enum だが、setter は任意の enum 値をそのまま保持でき、evictOne() の default 分岐へ意図せず落ちる可能性があった。
- 対応: setPolicy() で LRU〜Size の 0〜4 に正規化してから候補キューを再構築するようにした。
- 価値または懸念: キャッシュ eviction の挙動を定義済み policy に限定できる。policy の UI/property 導線自体は別途確認が必要。
- 次に確認すべきこと: キャッシュ設定の保存・復元経路で CachePolicy をどの形式で扱っているか確認する。
# 2026-08-02: Progressive Renderer の品質設定正規化

- 関連ファイル: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 気づき: RenderQuality は 4 種類だが setQuality() が任意 enum を保持でき、downsampling も下限だけで過大値を許していた。
- 対応: Quality を Draft〜Custom の 0〜3 に正規化し、draft/preview downsampling を 1〜64 に制限した。qualityChanged は正規化後の値を通知する。
- 価値または懸念: 未定義品質分岐や極端なプレビュー縮小率を防ぐ。64 の上限は実装上の安全上限として未検証。
- 次に確認すべきこと: downsampling の UI 最大値と、Custom 品質の実際の扱いを確認する。
# 2026-08-02: RenderPerformanceMonitor の FPS・空統計ガード

- 関連ファイル: `Artifact/src/Render/ArtifactFrameCache.cppm`
- 気づき: 平均フレーム時間が 0 の初期状態やゼロ時間フレーム時に、fpsChanged の平均 FPS 計算が 0 除算になる可能性があった。また未計測状態を performance acceptable と判定していた。
- 対応: 平均フレーム時間が正の場合のみ FPS を計算し、それ以外は 0 を通知する。未計測状態は acceptable を false とした。
- 価値または懸念: 初期状態・ゼロ時間計測で Inf/NaN が UI へ流れるのを防ぐ。ゼロ時間フレームを許容する既存 recordFrameRender の仕様は維持した。
- 次に確認すべきこと: performance monitor の初期表示で未計測状態を別ステータスとして表示するか検討する。
# 2026-08-02: Luma Key setter の入力検証統一

- 関連: `Artifact/include/Effects/Keying/LumaKeyEffect.ixx`, `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`
- 事実: プロパティ経由では閾値と softness を補正していたが、CPU 実装の setter 直接呼び出しは未検証だった。
- 対応: low/high threshold と softness に有限値・範囲補正を追加し、プロパティの hard range も 0..1 に統一した。
- 価値: UI 経由と内部 API 経由で異常値の扱いが分岐せず、Luma Key のアルファ計算を安定させられる。
- 次に確認: GPU/runtime 側の keying 実装が追加される場合は同じ制約を共有する。
# 2026-08-02: Difference Key setter の入力検証統一

- 関連: `Artifact/include/Effects/Keying/DifferenceKeyEffect.ixx`, `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`
- 事実: プロパティ経由では threshold と softness を補正していたが、CPU 実装の直接 setter は未検証だった。
- 対応: 両 setter に有限値・範囲補正を追加し、プロパティの hard range を soft range と一致させた。
- 価値: Difference Key の距離計算で異常な閾値や softness が内部 API から混入する経路を減らした。
- 次に確認: GPU/runtime 側の keying 実装が追加される場合は同じ制約を共有する。
# 2026-08-02: Chroma Key property range の明示化

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`
- 事実: setter 側では similarity、smoothness、spill reduction の範囲補正が存在したが、プロパティ metadata に範囲が設定されていなかった。
- 対応: 3 プロパティの soft/hard range を setter の制約と一致させ、無効な QColor を無視するようにした。
- 価値: UI 側の入力制御と実行時の制約が一致し、無効色による意図しないキー色変更を防ぐ。
- 次に確認: GPU/runtime keying の実装時には CPU と同じ色空間・距離定義を受け入れ条件として整理する。
# 2026-08-02: Keying CPU の非有限画素伝播防止

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`, `Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`
- 事実: 画素値が NaN/Inf の場合、色距離計算からアルファへ非有限値が伝播する可能性があった。
- 対応: 色距離または入力アルファが非有限の場合は、その画素の出力アルファを 0 にして判定を継続する。
- 価値: 不正な入力画素がキーイング結果全体へ NaN を広げることを防ぐ。
- 次に確認: GPU/runtime 実装では shader 側でも同等の非有限値ポリシーを定義する。
# 2026-08-02: Text Tool 作成入口の堅牢化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: Text Tool は既に point/box text の作成動線を持っていたが、レイヤー名がレイヤー数依存で削除後の重複を防げず、座標・サイズの非有限値も入口で拒否していなかった。
- 対応: 既存の `uniqueLayerNameForCurrentComposition` を利用し、キャンバス座標と box size の有限値を検証してから Text Layer を作成するようにした。
- 価値: Text Tool の作成操作が既存レイヤー名と衝突せず、異常な入力で壊れたテキストレイヤーを生成しにくくなる。
- 次に確認: 実機 UI で point text と box text の編集開始・undo を確認する（ビルド未実行）。
# 2026-08-02: ディスクプレビュー容量変更の即時 eviction

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: ディスクキャッシュ容量の変更は上限値を更新するだけで、既存キャッシュの超過分は次回フレーム書き込みまで残っていた。
- 対応: 容量設定変更時に既存の global budget enforcement を再利用し、削除された現在キャッシュのフレーム状態も更新するようにした。
- 価値: 設定変更直後からディスク使用量を新しい上限へ近づけられる。
- 次に確認: ビルド後、容量を下げた際の eviction と manifest 再生成を実機で確認する。
# 2026-08-02: OCIO working/display/view setter の入力検証

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: working space、display、view の setter は任意の文字列を状態と `OCIOConfig` に渡していた。
- 対応: trim 後に有効な config の候補一覧で検証し、存在しない値は無視するようにした。
- 価値: 不正な色空間や表示 view が transform へ流れ、不要な fallback を発生させる経路を減らした。
- 次に確認: カスタム OCIO config の display/view 切り替えと GPU descriptor 再生成を実機で確認する。
# 2026-08-02: OCIO 設定ファイルパスの正規化

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: `loadConfigFile` は入力パスをそのまま Core/OCIO に渡しており、空白や存在しないパスを明示的に拒否していなかった。
- 対応: trim 後に絶対パス化し、存在確認を通過したパスだけを Core と OCIO に渡すようにした。
- 価値: 設定読み込み失敗を早期に判定し、Core と OCIO で異なるパス解決になる可能性を減らした。
- 次に確認: カスタム config の相対パス運用が必要な場合は、呼び出し側の基準ディレクトリ仕様を確認する。
# 2026-08-02: Layer Effect Envelope の非有限出力防止

- 関連: `Artifact/src/Animation/ArtifactLayerEffectEnvelope.cppm`
- 事実: envelope の時間補間は clamp 済みだったが、effectStart/effectEnd が NaN/Inf の場合は補間結果へ伝播した。
- 対応: サンプリング時に start/end を有限値へ fallback してから補間するようにした。
- 価値: procedural animation の共通 envelope が異常パラメータから NaN を生成しない。
- 次に確認: エフェクト側の envelope 接続時に start/end の意図した範囲を UI metadata と揃える。
# 2026-08-02: Brush Tool パラメータの有限値検証

- 関連: `Artifact/include/Tool/ArtifactBrushTool.ixx`
- 事実: ブラシの各 setter は clamp を行っていたが、NaN/Inf に対する `std::clamp` は値を保持し得た。
- 対応: radius、opacity、flow、硬さ、spacing、角度、jitter、scatter、pressure、tilt に有限値 fallback を追加した。
- 価値: ブラシ描画へ非有限パラメータが入り、サイズ・間隔・色付け計算が壊れる経路を減らした。
- 次に確認: 実機の筆圧・傾き入力と clone brush のパラメータ同期を確認する。
# 2026-08-02: Brush Tool 色成分の有限値検証

- 関連: `Artifact/include/Tool/ArtifactBrushTool.ixx`
- 事実: ブラシ色の各成分も clamp のみで、NaN/Inf が色状態に残る可能性があった。
- 対応: RGB は 0、alpha は 1 を fallback とする有限値検証を追加した。
- 価値: 異常なブラシ色がペイント処理や合成へ伝播する経路を塞いだ。
- 次に確認: カラーピッカーからの alpha と HDR 色入力の仕様を確認する。
# 2026-08-02: Brush Tool ストローク座標の有限値検証

- 関連: `Artifact/src/Tool/ArtifactBrushTool.cppm`
- 事実: mouse press/move/release の座標を `QLineF` とストローク点へ直接渡していた。
- 対応: press は非有限座標を拒否し、move は無視、release は最後の有効点だけで確定するようにした。
- 価値: 壊れた座標入力がペイントストロークや距離計算へ混入する経路を防ぐ。
- 次に確認: 高 DPI やタブレット入力での座標変換境界を実機確認する。
# 2026-08-02: Motion Sketch 入力の有限値検証

- 関連: `Artifact/src/Tool/ArtifactMotionSketchTool.cppm`
- 事実: スケッチ座標と smoothing/sample rate の setter は clamp 前に非有限値を拒否していなかった。
- 対応: begin/add の座標を検証し、異常座標を拒否。smoothing は 0.5、sample rate は 60fps を非有限値の fallback とした。
- 価値: 不正な座標や時間間隔が keyframe 生成へ伝播する経路を減らした。
- 次に確認: タブレット入力・高 DPI 座標変換後のスケッチ再生を実機確認する。
# 2026-08-02: Puppet Tool 入力の有限値検証

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: pin 座標、rotation、weight、depth、hit-test threshold は clamp/fmod 前に非有限値を拒否していなかった。
- 対応: 異常座標を拒否し、各数値に有限値 fallback と範囲制約を追加した。
- 価値: Puppet pin の位置・変形・ヒットテストが異常入力で壊れる経路を減らした。
- 次に確認: 2D rig の pin 編集と undo/redo を実機確認する。
# 2026-08-02: Puppet engine 同期時の pin 検証

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: engine へ同期する経路では PinRecord の座標・rotation・weight・depth を直接変換していた。
- 対応: 非有限座標の pin を除外し、rotation/weight/depth を有限値・範囲補正してから `PuppetPin` を生成するようにした。
- 価値: setter を経由しない復元・内部状態からの異常値も engine へ伝播しない。
- 次に確認: pin の削除・再 bind 後の engine 状態と描画結果を実機確認する。
# 2026-08-02: Audio Service の音量・パン入力検証

- 関連: `Artifact/src/Service/ArtifactAudioService.cppm`
- 事実: master volume、layer volume、pan は clamp のみで、NaN/Inf が音量・パン状態へ残る可能性があった。
- 対応: 音量は 1.0、pan は 0.0 を有限値 fallback としてから既存範囲へ clamp するようにした。
- 価値: オーディオバスやレイヤーへ非有限値が伝播し、dB 変換やミキサー状態が壊れる経路を減らした。
- 次に確認: ミュート・デバイス切り替えと volume/pan の保存・再読込を実機確認する。
# 2026-08-02: Audio dB 変換の有限値防御

- 関連: `Artifact/src/Service/ArtifactAudioService.cppm`
- 事実: レイヤー値から直接呼ばれる `linearToDecibels` は NaN/Inf を受ける可能性があった。
- 対応: dB 変換前に有限値を検証し、異常値は linear 1.0 に fallback するようにした。
- 価値: UI setter を経由しない音量値でも、ミキサーへ非有限 dB が伝播しない。
- 次に確認: 読み込み済みレイヤーの異常音量を含むプロジェクトで再生状態を確認する。
# 2026-08-02: Motion Sketch 確定前のサンプル列検証

- 関連: `Artifact/src/Tool/ArtifactMotionSketchTool.cppm`
- 事実: begin/add では入力検証しているが、内部 sampledPoints と sampledTimes の不一致・破損を finish 時に検証していなかった。
- 対応: keyframe 生成前に配列長、座標、非負の有限時刻を検証し、異常時は確定を中止するようにした。
- 価値: 壊れたサンプル列から不正な frame keyframe を作成する経路を防いだ。
- 次に確認: 長時間スケッチとキャンセル直後の再開でサンプル列が正しく初期化されることを確認する。
# 2026-08-02: Effect Preset の atomic save

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: effect preset は通常の QFile へ直接書き込み、書き込み途中の失敗や空パスを明示的に扱っていなかった。
- 対応: QSaveFile に変更し、trim 済みパス、完全 write、commit 成功を確認するようにした。
- 価値: プリセット保存中の中断で既存ファイルが壊れる可能性を下げた。
- 次に確認: 保存先フォルダ権限エラーと上書き保存後の再読込を確認する。
# 2026-08-02: Effect Preset 読み込みパスの検証

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: effect preset 読み込みは空パス・空白・不存在ファイルを QFile の open に任せていた。
- 対応: effect の有無、trim 済みパス、ファイル存在を先に検証するようにした。
- 価値: 無効なプリセット入力を早期に拒否し、読み込み失敗の責務を明確にした。
- 次に確認: 互換性のない旧 preset JSON を読み込んだ場合の effect 状態保持を確認する。
# 2026-08-02: Mask Preset の atomic save/load 検証

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: mask preset も通常 QFile へ直接保存し、読み込み時の空パス・不存在確認もなかった。
- 対応: 保存を QSaveFile + 完全 write + commit に変更し、保存/読み込み双方で trim 済みパスを検証するようにした。
- 価値: mask preset の途中書き込みによる破損と無効パスの曖昧な失敗を減らした。
- 次に確認: mask preset の上書き保存後に paths/enabled が保持されることを確認する。
# 2026-08-02: Project Manager 入出力パスの正規化

- 関連: `Artifact/src/Project/ArtifactProjectManager.cppm`
- 事実: 本体プロジェクトの load/save 入口は空白や空パスをそのまま importer/exporter へ渡していた。
- 対応: load は trim・存在・通常ファイルを検証し、save は trim 済みの非空パスを backup/export/hook/状態更新で一貫して使うようにした。
- 価値: 保存先・読み込み元の解決が一貫し、無効パスで既存プロジェクト状態を不必要に変更する経路を減らした。
- 次に確認: 相対パス保存と auto-save/recovery のパス連携を実機確認する。
# 2026-08-02: 非同期 Project load のパス正規化

- 関連: `Artifact/src/Project/ArtifactProjectManager.cppm`
- 事実: `loadFromFileAsync` は入力パスを worker と UI 更新 lambda にそのまま渡していた。
- 対応: 非同期処理開始前に trim・存在・通常ファイルを検証し、以後は normalizedPath を一貫して使用するようにした。
- 価値: 同期/非同期でプロジェクトパスの扱いが分岐せず、無効入力で worker を起動しない。
- 次に確認: 非同期ロード失敗時の callback と UI 状態保持を確認する。
# 2026-08-02: 非同期 Project save のパス正規化

- 関連: `Artifact/src/Project/ArtifactProjectManager.cppm`
- 事実: `saveToFileAsync` は worker、backup、exporter、hook、状態更新へ元の fullpath を個別に渡していた。
- 対応: 開始時に trim 済み非空パスを検証し、以後の全経路で normalizedPath を使うようにした。
- 価値: 同期 save と非同期 save のパス解決差異を減らし、無効パスで worker を起動しない。
- 次に確認: 非同期 save の成功/失敗 callback と backup 生成を確認する。
# 2026-08-02: Project Importer 入力パス検証

- 関連: `Artifact/src/Project/ArtifactProjectImporter.cppm`
- 事実: Manager を経由しない Importer 直接利用では入力パスをそのまま保持し、load 時に QFile の open 結果だけへ依存していた。
- 対応: setter で trim し、JSON 読み込み前に空パス・不存在・ディレクトリを拒否するようにした。
- 価値: Project Importer の直接利用でも同期/非同期 Manager と同じ入力境界になる。
- 次に確認: 相対パスと Unicode パスの import を確認する。
# 2026-08-02: Color Palette Mapping パス検証

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: palette mapping の保存/読み込みは Core manager へ入力パスを直接渡していた。
- 対応: 保存では trim 済み非空パス、読み込みではさらに存在確認を行ってから Core API を呼ぶようにした。
- 価値: effect/mask preset と同じ入力境界を palette mapping にも適用した。
- 次に確認: palette mapping の Unicode パスと上書き保存を確認する。
# 2026-08-02: Shape Layer 数値 setter の有限値検証

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: gradient angle/center/radius、stroke width/taper、corner radius、star inner radius は setter で clamp せず、または NaN/Inf を保持し得た。
- 対応: 各 setter に有限値 fallback と既存の意味に沿った範囲制約を追加した。
- 価値: 静止画・シェイプレイヤーの描画パスへ異常な geometry/style 値が伝播する経路を減らした。
- 次に確認: gradient、rounded shape、star shape の保存/再読込後の描画を確認する。
# 2026-08-02: Shape Layer 色状態の正規化

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Shape Layer の色は描画時に clamp されていたが、fill/stroke/gradient の setter は内部状態へ直接保存していた。
- 対応: RGB/alpha を有限値検証して 0..1 に正規化する共通 helper を追加し、4 系統の色 setter に適用した。
- 価値: 保存・再読込や別 renderer 経由でも異常な色状態が残らない。
- 次に確認: HDR 色入力を Shape Layer に渡す場合の色域仕様を確認する。
# 2026-08-02: Shape Layer カスタム頂点の検証

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: custom polygon/path setter は頂点配列をそのまま内部 geometry へ保存していた。
- 対応: polygon の座標、Bezier vertex の位置と in/out tangent を有限値検証し、異常頂点を除外するようにした。
- 価値: 静止画・シェイプ geometry の path 計算へ NaN/Inf が混入する経路を防いだ。
- 次に確認: 無効頂点を含む custom path の保存/再読込と undo を確認する。
# 2026-08-02: Shape Layer property metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: gradient angle/center/radius と stroke width は setter 側に制約がある一方、Inspector property metadata の範囲が未設定だった。
- 対応: angle、center、radius、stroke width に soft/hard range を追加し、setter の許容範囲と一致させた。
- 価値: Inspector の入力 UI と Shape Layer 実行時の制約が一致する。
- 次に確認: property panel で範囲表示とアニメーション値の編集を確認する。
# 2026-08-02: Shape geometry property metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: width/height、corner radius、star points/inner radius の setter 制約に対し、Inspector metadata の hard/soft range が未設定だった。
- 対応: geometry パラメータへ setter と一致する範囲を追加した。
- 価値: Shape Layer の Inspector から無効なサイズや shape パラメータを入力しにくくなる。
- 次に確認: 大きな canvas size と star/polygon の上限表示を property panel で確認する。
# 2026-08-02: Shape stroke metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: stroke taper と cap/join/align enum は setter 側で範囲制約がある一方、Inspector metadata の hard range が未設定だった。
- 対応: taper を 0..1、cap/join/align を 0..2 として明示した。
- 価値: Shape stroke の UI 入力が実行時の有効範囲から外れない。
- 次に確認: stroke style の property editing と preset 再読込を確認する。
# 2026-08-02: Text Layer 基本 typography metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: font size、tracking、leading の setter は入力を検証していたが、Inspector metadata に範囲がなかった。
- 対応: font size は 1..1000、tracking/leading は -1000..1000 の hard range と実用域の soft range を追加した。
- 価値: Text Layer の typography 編集 UI と実行時の入力制約が一致する。
- 次に確認: point/box text で font size、tracking、leading のアニメーション編集を確認する。
# 2026-08-02: Text Layer stroke/shadow metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: stroke width、shadow offset、shadow blur の setter は有限値・非負制約を持つが、Inspector metadata の範囲が未設定だった。
- 対応: stroke width、shadow offsets、shadow blur に hard/soft range を追加した。
- 価値: Text Layer の描画負荷と表示上の異常値を Inspector から抑制できる。
- 次に確認: shadow blur/offset のアニメーションと preset 再読込を確認する。
# 2026-08-02: Text Layer 色状態の正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: text fill/stroke/shadow の setter は FloatRGBA へ色成分を直接保存していた。
- 対応: RGB/alpha を有限値検証し、0..1 に clamp してから内部状態へ保存するようにした。
- 価値: Text Layer の保存・再読込や別 renderer 経由でも異常な色成分が残らない。
- 次に確認: HDR 色入力を Text Layer に渡す場合の色域仕様を確認する。
# 2026-08-02: Text Layer path text 入力の検証

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: path text の Bezier segments と start/end offset は setter で直接保存されていた。
- 対応: segment の 4 点を有限値検証し、異常 segment を除外。offset は非有限値を 0 に fallback するようにした。
- 価値: path text の文字配置計算へ異常 geometry や非有限 offset が伝播しない。
- 次に確認: open path/closed path の text layout と reverse/offset 編集を確認する。
# 2026-08-02: Text path offset metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: path start/end offset は有限値へ補正されるが、Path Options の Inspector metadata に範囲がなかった。
- 対応: start/end offset に hard -100000..100000、soft -1000..1000 を設定した。
- 価値: path text の offset 編集で過大値を入力しにくくなり、UI と runtime の入力契約が明確になる。
- 次に確認: 長い path と reverse を含む text layout で offset 操作を確認する。
# 2026-08-02: Font Usage manifest の atomic 出力

- 関連: `Artifact/src/Project/ArtifactProjectStatistics.cppm`
- 事実: font usage の JSON/CSV manifest は QFile へ truncate 書き込みしており、途中失敗で既存レポートを壊し得た。
- 対応: QSaveFile に変更し、trim 済みパス、完全 write、commit 成功を確認するようにした。
- 価値: プロジェクト統計出力の破損リスクを下げた。
- 次に確認: JSON のみ、JSON+CSV、書き込み権限エラーの各ケースを確認する。
# 2026-08-02: Project Packager target path validation

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: packager は targetDir の空文字を確認していたが、空白や既存ファイルをディレクトリとして扱うケースを明示していなかった。
- 対応: targetDir を trim し、既存パスが通常ファイルなら packaging を開始しないようにした。
- 価値: Assets 作成や既存ファイル削除へ進む前に、出力先の不整合を検出できる。
- 次に確認: 既存ディレクトリ、未作成ディレクトリ、既存ファイルを target に指定した場合を確認する。
# 2026-08-02: Text Animator preset metadata の範囲同期

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text Animator preset は実装上 0〜7 の ID を扱うが、Inspector metadata に範囲がなかった。
- 対応: animator preset に hard/soft range 0〜7 を追加した。
- 価値: 未定義 preset ID が UI から入力される経路を減らした。
- 次に確認: preset 切り替えと custom animator への復帰を確認する。
# 2026-08-02: Project Packager 欠落アセットの失敗化

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: external file が存在しない場合、copy を skip して元の filePath を残したまま package が成功する可能性があった。
- 対応: 欠落または通常ファイルでない外部ファイルを検出した時点で packaging を失敗させるようにした。
- 価値: 再読込時に参照切れになる不完全 package を生成しない。
- 次に確認: missing asset、permission error、正常 package の各ケースを確認する。
# 2026-08-02: Project Packager 上書き失敗の明示化

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: 既存 Assets ファイルの削除結果を確認せず、copy へ進んでいた。
- 対応: destination の削除に失敗した場合は warning を出して packaging を中断するようにした。
- 価値: ロック・権限エラーで古い asset が残る不完全 package を防ぐ。
- 次に確認: Windows 上の開いている asset と read-only destination の挙動を確認する。
# 2026-08-02: Project Packager の asset preflight

- 関連: `Artifact/src/Project/ArtifactProjectPackager.cppm`
- 事実: 欠落 external file の検出がコピー開始後だったため、後半の欠落で前半だけコピー済みの partial package が残り得た。
- 対応: 全 external file を先に存在・通常ファイル検証してから Assets のコピーを開始するようにした。
- 価値: 欠落アセットで失敗する場合に、packaging の副作用を発生させにくくした。
- 次に確認: 複数アセットのうち一つが欠落するケースで出力先が未変更であることを確認する。
# 2026-08-02: 3Dプリミティブ寸法の非有限値防御

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`、3DプリミティブのJSON復元とProperty Editor入力。
- 事実: 3Dレイヤーの寸法はJSONおよびプロパティ経由で `float` 化され、そのままメッシュ生成に渡り得た。
- 対応: 寸法を有限値・0.01〜100000の範囲へ正規化し、NaN/Infinityは現在値へフォールバックする共通ヘルパーを追加した。
- 価値/懸念: 不正な保存データやUI入力によるメッシュ計算の破綻を局所的に防ぐ。低レベルのDiligent backendやCompositionGraphは変更していない。
- 次に確認: 3D材質値の上限・有限値保証がMaterial側の契約と一致するか、ビルド許可後に確認する。
# 2026-08-02: 3D材質のJSON復元と入力範囲

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- 事実: 3DレイヤーのJSON出力にはベースカラーと材質係数が含まれていたが、復元側ではテクスチャ以外の材質値を反映していなかった。
- 対応: ベースカラー、metallic、roughness、opacityを復元し、材質プロパティの範囲と有限値を入力経路で正規化した。normal/occlusion strengthも範囲を設定した。
- 価値/懸念: 保存→再読込で材質状態が欠落する問題を縮小する。Material側の既定値・上限契約は未検証。
- 次に確認: ビルド許可後に既存プリセットとの互換性と、3Dレンダラーへの材質値伝播を確認する。
# 2026-08-02: キーイング色入力の正規化

- 関連: `Artifact/include/Effects/Keying/ChromaKeyEffect.ixx`、`Artifact/include/Effects/Keying/DifferenceKeyEffect.ixx`、`Artifact/src/Effects/Keying/DifferenceKeyEffect.cppm`。
- 事実: QColor経由の不正色をDifference Keyが受け入れる経路があり、直接のFloatRGBA setterも有限値・範囲を保証していなかった。
- 対応: 無効なQColorを無視し、Chroma/Differenceの色チャンネルを有限値かつ0〜1へ正規化した。
- 価値/懸念: キー色の不正値がCPUキー処理へ伝播する可能性を減らす。GPU runtime受入れそのものは未検証。
- 次に確認: ビルド許可後、3種類のKey効果を実画像で比較し、アルファ境界とspillの品質を確認する。
# 2026-08-02: 連番画像のフレームレート境界

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`。
- 事実: 連番画像のフレームレートは有限値チェックのみで、非常に大きい正数をそのまま `ImageSequenceSource` に渡し得た。
- 対応: API入力とJSON復元の両方でフレームレートを `0.001〜1000 FPS` に正規化した。
- 価値/懸念: 異常な保存値によるフレーム番号計算・先読み負荷の暴走を抑える。実際の高FPS素材の運用上限は未検証。
- 次に確認: 実機で連番の保存→再読込→タイムラインフレーム切替を確認する。
# 2026-08-02: 静止画レイヤーのCrop入力境界

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`sourceCrop` のProperty Editor経路。
- 事実: Crop位置・サイズ、pan、zoom、rotation、anchorは `QVariant` から直接double化され、非有限値や極端な値を受け得た。
- 対応: 各入力を有限値チェックと妥当な範囲へ正規化してから `SourceCrop` に渡すようにした。
- 価値/懸念: 静止画・連番画像の編集時に変換行列やCrop計算へ不正値が伝播する可能性を減らす。範囲はUI操作上の安全上限として設定しており、仕様上の最大値は未検証。
- 次に確認: ビルド許可後、極端なCrop/Zoom値を保存・再読込して表示が安定するか確認する。
# 2026-08-02: Text Wiggly Animatorの復元値正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- 事実: Text AnimatorのWiggly設定と変形値はJSONからfloatへ直接変換され、非有限値や範囲外の値が復元され得た。
- 対応: Wigglyの周波数・相関・位相と、Animatorのscale/opacity等を有限値・妥当範囲へ正規化し、Wiggles/SecのPropertyにもhard rangeを追加した。
- 価値/懸念: プロシージャル文字アニメーションの保存→再読込で異常値が評価へ流れる可能性を減らす。実時間品質は未検証。
- 次に確認: ビルド許可後、Wiggly有効状態の保存・再読込と長時間再生を確認する。
# 2026-08-02: 画像素材の色空間メタデータ正規化

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`。
- 事実: `setInputInterpretation()` では色空間名をtrimしていたが、JSON復元では直接代入していた。
- 対応: 画像レイヤーの保存復元でも input color space / transfer function をtrimして、素材解釈の同一性を保つようにした。
- 価値/懸念: UI経由とプロジェクト再読込で色空間名の扱いがずれる可能性を減らす。実OCIO config上の名称存在確認はManager側の責務として残している。
- 次に確認: 複数のOCIO configで素材別解釈を保存・再読込し、変換結果を比較する。
# 2026-08-02: 3Dモデル入力パスの検証

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- 事実: 3Dモデル読み込みは空文字以外をそのままImporterへ渡し、空白付き入力や存在しないファイルでもImporter処理へ進んでいた。
- 対応: 入力をtrimし、通常ファイルの存在を確認してからImporterを呼ぶようにした。既存の読み込み失敗時のfallback挙動は維持した。
- 価値/懸念: 無効パスによるImporter呼び出しと曖昧な失敗を減らす。URI/仮想ファイルパスは未対応のまま。
- 次に確認: ビルド許可後、相対パス・空白付きパス・欠損ファイルの復元挙動を確認する。
# 2026-08-02: Footage解釈サービスのFPS入力境界

- 関連: `Artifact/src/Service/FootageInterpretService.cppm`。
- 事実: Footageのフレームレート変更は `<= 0` のみを検査しており、NaNや極端に大きい値が preflight / 適用処理へ進み得た。
- 対応: 有限値チェックと `0.001〜1000 FPS` の範囲正規化を preflight と適用経路へ追加した。
- 価値/懸念: 素材解釈変更に伴う時間比率計算の異常を抑制する。高FPS素材の上限は未検証。
- 次に確認: ビルド許可後、KeepTime / KeepKeyframes 各モードで境界値を確認する。
# 2026-08-02: 3D材質の保存項目補完

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- 事実: 3D材質のJSONにはベースカラー・metallic・roughness・opacityとテクスチャはあったが、emission color/strength、normal strength、occlusion strengthが出力されていなかった。
- 対応: 上記の材質値をJSONへ追加し、復元時にも有限値・範囲正規化して反映するようにした。
- 価値/懸念: 3Dレイヤーの保存→再読込で材質表現が欠落する範囲を縮小する。既存ファイルには既定値で後方互換する。
- 次に確認: ビルド許可後、材質各値の変更を保存・再読込してレンダー結果を比較する。
# 2026-08-02: 音声レイヤーの音量・パン有限値防御

- 関連: `Artifact/src/Layer/ArtifactAudioLayer.cppm`。
- 事実: 音声レイヤーの `setVolume` / `setPan` は clamp のみで、NaN入力ではNaNが残る可能性があった。JSON復元とProperty入力も同じsetterを通る。
- 対応: 音量・パンを有限値確認後に既存範囲へclampし、無効値はそれぞれ既定値へ戻すようにした。
- 価値/懸念: 音声ミックスとキャッシュ比較に不正値が伝播する可能性を減らす。音声DSP自体は変更していない。
- 次に確認: ビルド許可後、境界値の保存・再読込と再生キャッシュ無効化を確認する。
# 2026-08-02: 音声サンプルレート不正値の防御

- 関連: `Artifact/src/Layer/ArtifactAudioLayer.cppm`。
- 事実: WAVロード後のduration計算はサンプルレートが0でも除算し得た。
- 対応: サンプルレートが正の場合のみdurationを計算し、0以下なら未ロード扱いにした。
- 価値/懸念: 壊れた音声メタデータによる無限値の伝播を防ぐ。デコーダーの検証自体は変更していない。
- 次に確認: ビルド許可後、無効サンプルレートの音声ファイルでロード失敗とUI表示を確認する。
# 2026-08-02: Cameraレイヤーの光学・手ぶれ入力境界

- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- 事実: FOV、正投影サイズ、clip、IPD、手ぶれ減衰・周波数の一部setterは有限値や上限を保証していなかった。
- 対応: Property/JSON/APIの共通setterで有限値確認と既存UI範囲相当のclampを行うようにした。
- 価値/懸念: 投影行列や手ぶれ計算へNaN・極端な値が伝播する可能性を減らす。カメラの実レンダリング挙動は未検証。
- 次に確認: ビルド許可後、Perspective / Orthographic / Stereo各モードで境界値を保存・再読込する。
# 2026-08-02: Camera手ぶれ振幅ベクトルの正規化

- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- 事実: 手ぶれ位置・回転の振幅setterは `QVector3D` をそのまま保持し、JSON復元時の非有限成分を防いでいなかった。
- 対応: 位置振幅を0〜10000、回転振幅を0〜360へ各軸ごとに正規化し、非有限値は0へ戻すようにした。
- 価値/懸念: 手ぶれ計算へNaNが流れる可能性を減らす。shake offset本体は別の外部制御値として維持した。
- 次に確認: ビルド許可後、軸ごとの振幅保存・再読込と手ぶれプレビューを確認する。
# 2026-08-02: Camera手ぶれオフセットの入力防御

- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- 事実: 手ぶれオフセット・回転本体のsetterはベクトル値を直接保持していた。
- 対応: 各軸を有限値確認し、位置は±100000、回転は±360000度に制限した。
- 価値/懸念: view matrix生成へ非有限値が伝播する可能性を減らす。外部制御側が意図的に極端な値を使う契約は未確認。
- 次に確認: ビルド許可後、shake APIとJSON復元の両方で軸別入力を確認する。
# 2026-08-02: レンダーキュー出力FPSの有限値防御

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: 出力設定のFPSは `std::clamp` のみで正規化され、NaN入力では不正値が残る可能性があった。
- 対応: 有限値を確認して1〜240 FPSへclampし、無効値は30 FPSへフォールバックするようにした。
- 価値/懸念: レンダー設定からフレーム時間計算・エンコーダー引数へNaNが伝播する可能性を減らす。実エンコーダー受入れは未検証。
- 次に確認: ビルド許可後、レンダーキュー設定の保存・再読込と境界FPSでの出力を確認する。
# 2026-08-02: レンダーキューOverlay変換値の正規化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: Overlayのoffset・scale・rotation setterはscale以外の有限値確認がなく、scaleもNaNを防げなかった。
- 対応: offsetを±100000、scaleを0.05〜8、rotationを±360000度に正規化し、非有限値は既定値へ戻すようにした。
- 価値/懸念: 最終出力合成へ不正な変換値が伝播する可能性を減らす。出力品質の実機確認は未実施。
- 次に確認: ビルド許可後、Overlay付きジョブの保存・再実行と境界値を確認する。
# 2026-08-02: レンダージョブ一括更新時の設定正規化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: 個別setterには出力設定の範囲制限があったが、`updateJob` はジョブ構造体を直接置換していた。
- 対応: 一括更新経路でも解像度・FPS・ビットレート・音声ビットレート・音声サンプルレートを正規化するようにした。
- 価値/懸念: API経路によるsetter迂回で不正なレンダー設定が残る可能性を減らす。codec等の文字列正規化は既存経路の責務として残した。
- 次に確認: ビルド許可後、個別更新と一括更新で同じ設定結果になるか確認する。
# 2026-08-02: レンダージョブ一括更新時のフレーム範囲整合

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: 個別のフレーム範囲setterは開始0以上・終了が開始以上を保証していたが、`updateJob` の構造体置換はこの整合を迂回していた。
- 対応: 一括更新後にも開始・終了フレームの順序と下限を正規化するようにした。
- 価値/懸念: 不正な範囲による空レンダーや負フレーム指定を減らす。上限値は既存仕様に合わせて未追加。
- 次に確認: ビルド許可後、逆転範囲を含むジョブの保存・再実行を確認する。
# 2026-08-02: Camera JSON復元のsetter迂回補正

- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`。
- 事実: Camera FOV と shake trauma はJSON復元時にメンバーへ直接代入され、setter側で追加した有限値・範囲保証を迂回していた。
- 対応: JSON復元経路にも同じFOV / traumaの正規化を明示的に適用した。
- 価値/懸念: API入力と保存復元でカメラ値の安全性が揃う。manual FOVの状態遷移は既存挙動を維持した。
- 次に確認: ビルド許可後、異常値JSONでPerspectiveカメラを再読込して状態を確認する。
# 2026-08-02: 既存Footage FPSの異常状態からの復旧

- 関連: `Artifact/src/Service/FootageInterpretService.cppm`。
- 事実: 新しいFPSは検証していたが、Footageに既にNaNが入っている場合、旧FPSとの比率計算へ進む可能性が残っていた。
- 対応: currentOverrideで異常FPSを無効値として扱い、変更適用時も旧FPSが有限値でない場合は比率計算を行わず新しいFPSへ置き換えるようにした。
- 価値/懸念: 壊れた既存プロジェクトからの再解釈でNaN比率が広がる可能性を減らす。
- 次に確認: ビルド許可後、異常FPSを含むプロジェクトのFootage再解釈を確認する。
# 2026-08-02: OCIO設定JSONの文字列正規化

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`。
- 事実: 通常のOCIO設定setterはtrimと有効値確認を行う一方、`fromJson` は設定名を直接代入していた。
- 対応: preset / working space / display / view / looks のJSON復元値をtrimして、保存形式の余白差を除去した。
- 価値/懸念: UI設定とプロジェクト復元時の名前不一致を減らす。有効値のconfig照合は既存のconfigロード後状態に依存する。
- 次に確認: ビルド許可後、カスタムOCIO configで余白付き設定の復元を確認する。
# 2026-08-02: OCIO設定復元時の候補整合

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`。
- 事実: JSON復元後のworking space / display / viewは、active configの候補一覧と照合されず無効名が残り得た。
- 対応: config復元後に候補一覧を確認し、無効な値はworking space・display・viewそれぞれの既定候補へ戻してconfigへ反映するようにした。
- 価値/懸念: OCIO実運用で保存された設定とconfig差し替えがあっても無効な表示変換状態を減らす。looksの候補照合は既存APIの制約上未追加。
- 次に確認: ビルド許可後、config差し替え後のJSON復元で表示・view変換が有効になるか確認する。
# 2026-08-02: OCIO Looks入力の正規化

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`。
- 事実: working/display/viewはsetterでtrimしていたが、Looksだけは未加工文字列をconfigへ渡していた。
- 対応: Looksもtrim済みの値を保持・config反映するよう統一した。
- 価値/懸念: Looks指定の余白差による表示変換不一致を減らす。Look名候補の列挙APIは未提供のため照合はしていない。
- 次に確認: ビルド許可後、Looks指定付きOCIO表示変換を保存・再読込する。
# 2026-08-02: プロジェクト設定JSONの文字列正規化

- 関連: `Artifact/src/Project/ArtifactProjectSetting.cppm`。
- 事実: 設定JSONの name / author は前後空白を保持したまま復元され、バリデーション上の表示名と保存値がずれる可能性があった。
- 対応: JSON復元時に両フィールドをtrimした。
- 価値/懸念: プロジェクト設定の再読込で不要な空白が残る問題を減らす。API setterの既存挙動は変更していない。
- 次に確認: ビルド許可後、空白付き設定の保存・再読込とバリデーション表示を確認する。
# 2026-08-02: 一括更新時のOverlay値整合

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: `setJobOverlayTransform` は値を正規化していたが、`updateJob` の構造体置換ではOverlay offset/scale/rotationが未検証だった。
- 対応: 一括更新経路にも個別setterと同じ有限値・範囲正規化を追加した。
- 価値/懸念: API経路による不正なOverlay変換の再侵入を防ぐ。レンダー品質の実機確認は未実施。
- 次に確認: ビルド許可後、一括更新と個別更新の出力変換が一致するか確認する。
# 2026-08-02: レンダージョブ一括更新時の文字列設定正規化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: `updateJob` は output format / codec / audio codec / channel mode を構造体から直接置換していた。
- 対応: 空値を既定値へ戻し、channel modeを許可された値へ正規化した。
- 価値/懸念: setterを迂回する一括更新でもエンコーダー設定が空・未知値になりにくい。codec profileの詳細検証は既存仕様に委ねている。
- 次に確認: ビルド許可後、個別更新と一括更新で出力設定が一致するか確認する。
# 2026-08-02: レンダージョブ一括更新時のパス・名前正規化

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: `updateJob` の構造体置換では job name / output path / audio source path の前後空白が残り得た。
- 対応: 一括更新時にも各文字列をtrimし、空のoutput pathは既存の既定出力パス生成へ渡すようにした。
- 価値/懸念: 同じジョブを個別setterで更新した場合との保存結果の差を減らす。
- 次に確認: ビルド許可後、空白付きパスを含むジョブの保存・再実行を確認する。
# 2026-08-02: プロジェクト設定JSON出力のcanonicalize

- 関連: `Artifact/src/Project/ArtifactProjectSetting.cppm`。
- 事実: JSON復元はtrimしていたが、API setterで設定された前後空白はtoJsonでそのまま出力されていた。
- 対応: メモリ上の値を変更せず、JSON出力時のname / authorだけtrimして保存するようにした。
- 価値/懸念: 保存→再読込で設定値が揺れる問題を減らす。UI上の入力中表示は従来どおり。
- 次に確認: ビルド許可後、空白付き入力のメモリ値と保存値の意図した差を確認する。
# 2026-08-02: PaintレイヤーのClone Stamp入力境界

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: Clone Stampの座標・半径・opacity・hardnessを直接計算し、半径NaNではピクセル領域の整数化へ進み得た。default canvasサイズもJSON値をそのまま採用していた。
- 対応: 座標と数値の有限値を確認し、半径を0.5〜10000へ制限、default width/heightを1〜100000へ正規化した。
- 価値/懸念: 静止画ペイント操作と保存復元で不正値がバッファ計算へ流れる可能性を減らす。通常のブラシ描画経路は既存処理を維持した。
- 次に確認: ビルド許可後、Clone Stampの境界値と異常JSON復元を確認する。
# 2026-08-02: Paintブラシストロークの有限値防御

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: ブラシ半径・揺らぎ係数・角度・点座標を描画ループで直接使用していた。
- 対応: ストローク数値の有限値を入口で確認し、各点の非有限座標はスキップするようにした。
- 価値/懸念: NaNによる整数化・ブラシ範囲計算の破綻を防ぐ。無効ストロークは描画せず、既存のundo状態も作成しない。
- 次に確認: ビルド許可後、通常ブラシの異常入力とundo/redo状態を確認する。
# 2026-08-02: PaintフレームJSONのメモリ上限

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: PaintフレームJSONのwidth/heightを検査せず、pixel payloadの検証前に画像をresizeしていた。
- 対応: 寸法を最大16384に制限し、RGBA32F payloadが512 MiBを超える場合は復元を拒否するようにした。
- 価値/懸念: 不正または破損したプロジェクトによる巨大メモリ確保を抑制する。大判ペイント素材の上限は未検証。
- 次に確認: ビルド許可後、大判フレームと破損Base64の復元失敗が安全に扱われるか確認する。
# 2026-08-02: Text Animator selector復元値の正規化

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- 事実: Animatorのrange start/end/offset/easeとpositionはJSONから直接float化され、前回のWiggly正規化対象外だった。
- 対応: selector range・ease・positionを有限値と既存UI想定範囲へ正規化して復元するようにした。
- 価値/懸念: Text Animatorの保存データから異常値が選択範囲・文字変形へ流れる可能性を減らす。selector units/shapeのenum検証は既存仕様に依存する。
- 次に確認: ビルド許可後、範囲Animatorの保存・再読込と選択表示を確認する。
# 2026-08-02: 3Dレイヤーenum復元の検証

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- 事実: fixedGeometry と renderMode はJSON整数を直接enumへcastしていた。
- 対応: 定義済み範囲を確認してから復元し、未知値は現在の安全な状態を維持するようにした。
- 価値/懸念: 破損・将来バージョン由来のenum値がメッシュ生成や表示モードへ流れる可能性を減らす。
- 次に確認: ビルド許可後、未知enumを含む3D JSONの復元結果を確認する。
# 2026-08-02: SourceCrop共通pan値の正規化

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`、画像・動画レイヤーの共有Cropモデル。
- 事実: panはsetterとJSON復元の両方で有限値・上限を保証していなかった。
- 対応: pan各軸を有限値確認後に±1000000へclampし、JSON復元もsetterを通すようにした。
- 価値/懸念: 画像・動画共通のCrop変換で不正なpanが行列計算へ流れる可能性を減らす。既存の動画再生経路は変更していない。
- 次に確認: ビルド許可後、画像・動画双方のCrop保存復元と極端なpanを確認する。
# 2026-08-02: SourceCrop JSONのrect / anchor防御

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`。
- 事実: cropRect・anchorのJSONヘルパーは数値型確認のみで、非有限値をそのままQRectF/QPointFへ渡す可能性があった。
- 対応: rect/pointの有限値確認と、anchorの非有限値フォールバックを追加した。
- 価値/懸念: 画像・動画共通のCrop変換で不正な矩形・anchorが残る可能性を減らす。
- 次に確認: ビルド許可後、異常値を含むCrop JSONの復元と保存結果を確認する。
# 2026-08-02: SourceCrop共通zoom上限

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`。
- 事実: SourceCropのzoomは正値化のみで、画像側Property入力の上限をJSON復元や共有モデルAPIが迂回できた。
- 対応: 共通setterでzoomを0.001〜1000にclampし、画像・動画の復元経路を統一した。
- 価値/懸念: 極端なCrop変換による小数計算・出力範囲の不安定化を抑える。既存の大倍率利用は上限に合わせて制限される。
- 次に確認: ビルド許可後、画像・動画のzoom境界と保存復元を確認する。
# 2026-08-02: SourceCrop共通rotation上限

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`。
- 事実: rotationは非有限値だけを防ぎ、極端な角度は共有API・JSON復元からそのまま保持できた。
- 対応: rotationを±360000度へclampし、画像・動画共通のCrop変換へ適用した。
- 価値/懸念: 極端な角度による三角関数計算や表示変換の不安定化を抑える。通常の回転操作の範囲には影響しない。
- 次に確認: ビルド許可後、Crop rotationの境界値を保存・再読込する。
# 2026-08-02: SourceCrop矩形setterの有限値防御

- 関連: `Artifact/src/Layer/ArtifactSourceCrop.cppm`。
- 事実: setCropRectはrect.normalized()のみを行い、APIから非有限矩形を直接保持できた。
- 対応: QRectFの各成分を検証し、非有限値なら空矩形へ戻すようにした。
- 価値/懸念: JSON以外の編集・サービス経路でもCrop変換へ不正矩形が入る可能性を減らす。
- 次に確認: ビルド許可後、無効QRectF入力とclampToSourceの既定矩形復帰を確認する。
# 2026-08-02: Shapeレイヤー寸法上限

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: setSizeは1以上のみを保証し、保存データやAPIから極端なキャンバス寸法を受け入れ得た。
- 対応: width/heightを1〜100000へclampし、shape cache・software描画の過大確保を抑えるようにした。
- 価値/懸念: 破損JSONや誤入力による巨大な描画負荷を減らす。大判シェイプの上限は未検証。
- 次に確認: ビルド許可後、寸法境界の保存復元とcache更新を確認する。
# 2026-08-02: Shape寸法上限をProperty定義へ統一

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: setSizeの上限とShape Propertyのhard rangeが100000 / 16384で不一致だった。
- 対応: setSize側をProperty定義と同じ16384へ揃えた。
- 価値/懸念: UI経由とAPI・JSON経由で許容寸法がずれる問題を解消する。
- 次に確認: ビルド許可後、Property入力とJSON復元の最大寸法が一致することを確認する。
# 2026-08-02: Shapeカスタム頂点のJSON復元フィルタ

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: custom polygon / Bézier pathの頂点はJSON復元時にsetterを通らず直接vectorへ格納されていた。
- 対応: position・inTangent・outTangentの各成分が有限値の頂点だけを復元するようにした。
- 価値/懸念: 破損したShape JSONがポリゴン・パス描画へNaNを流す可能性を減らす。頂点数の上限は既存仕様に委ねている。
- 次に確認: ビルド許可後、異常頂点を含むカスタムShapeの復元と保存を確認する。
# 2026-08-02: Shapeオペレーター復元件数上限

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: shapeOperators JSON配列を件数制限なしでreserveし、破損・悪意あるデータで過大な復元処理へ進み得た。
- 対応: fromJson / restoreOperatorsFromJson の復元件数を最大128へ制限した。未知のoperator typeは従来どおり無視する。
- 価値/懸念: シェイプ復元時のメモリ・処理量の暴走を抑える。128件の仕様上限は未検証。
- 次に確認: ビルド許可後、128件超のオペレーターJSON復元が安全に切り詰められるか確認する。
# 2026-08-02: Shapeカスタム頂点配列の上限

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- 事実: custom polygon / custom Bézier pathのJSON配列を件数制限なしでreserveしていた。
- 対応: 各配列の復元件数を最大100000に制限し、巨大JSONによる先行メモリ確保を抑えた。
- 価値/懸念: 既存の有限値フィルタと合わせて、破損Shapeデータの復元負荷を抑制する。上限値は実素材で未検証。
- 次に確認: ビルド許可後、上限超過配列と通常のBezier編集を確認する。
# 2026-08-02: Paintフレーム配列の復元上限

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: Paint JSONのframes配列を件数制限なしで復元していた。
- 対応: 1レイヤーあたりの復元フレーム数を最大10000に制限した。
- 価値/懸念: 破損JSONによる大量フレームのメモリ・復元処理負荷を抑える。正式な長尺ペイント上限は未検証。
- 次に確認: ビルド許可後、10000件超のフレームJSONを安全に切り詰められるか確認する。
# 2026-08-02: Paintピクセルpayloadの有限値正規化

- 関連: `Artifact/src/Layer/ArtifactPaintLayer.cppm`。
- 事実: Paint JSONのBase64 payloadはサイズ検証があっても、float値そのもののNaN / Infinityを含み得た。
- 対応: RGBA32F復元直後に全チャンネルを走査し、非有限値を0へ置換するようにした。
- 価値/懸念: 破損したペイントデータが描画・合成へ不正浮動小数を流す可能性を減らす。復元時の全画素走査コストは受け入れる。
- 次に確認: ビルド許可後、異常float payloadの復元と通常ペイント画像の再読込を確認する。
# 2026-08-02: Puppetピン数上限

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`。
- 事実: Puppet Toolは各レイヤーのpin配列を無制限に追加できた。
- 対応: 1レイヤーあたり最大1024 pinに制限し、超過追加を拒否するようにした。
- 価値/懸念: PuppetEngineへの過大なpin配列と、誤操作による処理負荷を抑える。1024件のUI上限は未検証。
- 次に確認: ビルド許可後、上限到達時のUI応答と既存pin操作を確認する。
# 2026-08-02: Text Animator配列の復元上限

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- 事実: Property EditorのanimatorCountは最大16だったが、JSONのtext.animators配列は件数無制限でreserve・復元していた。
- 対応: JSON復元も最大16 Animatorへ制限し、UI/APIの上限と一致させた。
- 価値/懸念: 巨大JSONによるText Animatorの過大復元を抑える。既存の16件超データは先頭16件を採用する。
- 次に確認: ビルド許可後、16件超のText Animator JSON復元と通常のselector編集を確認する。
# 2026-08-02: Keying CPU alpha normalization

- 関連: `Artifact/src/Effects/Keying/LumaKeyEffect.cppm`, `ChromaKeyEffect.cppm`, `DifferenceKeyEffect.cppm`
- 事実: CPU キーイング3種で入力の非有限値を無効 alpha にし、最終 alpha を `0..1` に正規化した。
- 価値: 不正な float や過大な元 alpha が後段の合成へ伝播するリスクを抑える。
- 次に確認すべきこと: 実データでキー境界と spill reduction の画質を確認する。
# 2026-08-02: Text style setter range alignment

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: font family の空白入力を既定値へ戻し、tracking/leading の setter を Inspector の hard range（`-1000..1000`）と一致させた。
- 価値: UI、JSON、アニメーション経由で値が入っても、レイアウト計算へ極端な値が流れない。
- 次に確認すべきこと: 多言語・縦書き・長文でのレイアウト実データ確認。
# 2026-08-02: Disk preview cache image integrity check

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: ディスクキャッシュの manifest 照合前に、対象が正のサイズを持つ通常ファイルで、Qt の画像リーダーが読める PNG か検証するようにした。
- 価値: 書き込み途中・破損・非画像ファイルをキャッシュヒットとして扱わず、再レンダリングへ戻せる。
- 次に確認すべきこと: キャッシュ容量削減中の同時読み書きで manifest と frame の整合性を確認する。
# 2026-08-02: Image sequence path restore bound

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 連番画像パスの setter と JSON 復元に最大 `100000` フレームの上限を追加し、JSON 配列内の文字列以外を無視するようにした。
- 価値: 破損・悪意あるプロジェクトで巨大なパス配列を生成してメモリやロード処理を圧迫することを防ぐ。
- 次に確認すべきこと: 非常に長い連番と欠落フレームを含むプロジェクトの再読込挙動。
# 2026-08-02: Source text keyframe restore bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Source Text キーフレームの JSON 復元を最大 `10000` 件に制限し、`timeScale <= 0` の不正な時間値を無視するようにした。
- 価値: 破損プロジェクトによる大量キーフレーム生成や不正な RationalTime の混入を防ぐ。
- 次に確認すべきこと: 大量キーと異常な時間スケールを含むプロジェクトの再読込。
# 2026-08-02: Form particle noise input normalization

- 関連: `Artifact/src/Layer/ArtifactFormParticleLayer.cppm`
- 事実: noise amount/scale/speed/phase の JSON 復元とプロパティ更新で非有限値を拒否し、実用上の上限を適用した。
- 価値: プロシージャル粒子の時間位相や空間周波数が NaN/Inf・極端な値になり、シミュレーションや描画を壊す経路を塞ぐ。
- 次に確認すべきこと: 長時間再生時の noiseSpeed と大きな phase の見た目・性能。
# 2026-08-02: Particle turbulence effector normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: turbulence の frequency/amplitude/evolution/octaves と共通 strength を JSON 復元時に有限値・範囲へ正規化し、追加 API でも同じ制約を適用した。
- 価値: 粒子 effector の NaN/Inf や過大な周波数・反復数による不安定化を防ぐ。
- 次に確認すべきこと: force/vortex/attractor 等の他 effector についても保存復元値と setter の制約を照合する。
# 2026-08-02: Procedural 3D noise restore normalization

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: terrain/path の noise scale・amplitude・evolution を有限値と範囲へ正規化し、terrain octaves を `1..12` に制限した。
- 価値: procedural 3D の JSON 破損値が無限・極小周波数や過大反復として生成処理へ流れるのを防ぐ。
- 次に確認すべきこと: 地形・パスの各ノイズ設定でプリセットと既存シーンの見た目を確認する。
# 2026-08-02: Particle radial effector restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: vortex/attractor/repeller の radius、falloff、angular velocity 等を JSON 復元時に有限値・範囲へ正規化した。
- 価値: 粒子場の半径・速度が NaN/Inf や極端な値となり、近傍探索やシミュレーションを不安定化する経路を減らす。
- 次に確認すべきこと: wind/flocking/kill effector の保存復元値も同じ基準で照合する。
# 2026-08-02: Particle wind and flocking restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: wind の強度・turbulence・周波数・evolution、flocking の近傍半径・重み・最大加速度を JSON 復元時に有限値・範囲へ正規化した。
- 価値: 群集・風場の計算に NaN/Inf や過大な近傍探索値が流入する経路を抑える。
- 次に確認すべきこと: Kill effector の zone type/size と wind direction の異常値復元。
# 2026-08-02: Particle kill-zone restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: Kill zone の type を `0..2` に制限し、各サイズを有限値かつ `0..100000` に正規化した。
- 価値: 破損プロジェクトから未知の zone type や負・無限サイズが衝突判定へ入るのを防ぐ。
- 次に確認すべきこと: 各 zone type の境界上（size=0 を含む）で kill 判定を確認する。
# 2026-08-02: Particle effector vector restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: 全 effector 共通の position/direction と Force の各成分を有限値・`±1000000` の範囲へ正規化してから復元するようにした。
- 価値: 破損したベクトル値が粒子の近傍計算・移流・力計算へ NaN/Inf として伝播するのを防ぐ。
- 次に確認すべきこと: 既存プロジェクトの巨大座標を意図的に保持する必要があるか確認する。
# 2026-08-02: Image input color-space validation

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の入力色空間設定で、アクティブな OCIO config が候補を返す場合は未知の色空間名を空に戻すようにした。config 未ロード時は既存の保存値を保持する。
- 価値: 素材別色空間の指定ミスが無言で変換処理へ流れ、意図しない色変換になるリスクを下げる。
- 次に確認すべきこと: config 切替後に既存レイヤーの入力色空間を再検証する運用を確認する。
# 2026-08-02: Image JSON color-space validation parity

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の JSON 復元でも直接代入をやめ、UI/API と同じ `setInputInterpretation()` を通すようにした。
- 価値: プロジェクト再読込だけが未知の OCIO 色空間名を受け入れる経路をなくした。
- 次に確認すべきこと: config 切替後の再読込と、OCIO config 未ロード時の保存値保持。
# 2026-08-02: Particle wind direction restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: Wind の direction 各成分も共通の有限値・座標範囲検証を通して復元するようにした。
- 価値: 壊れた方向ベクトルが風場計算へ NaN/Inf として伝播する最後の共通ベクトル経路を塞ぐ。
- 次に確認すべきこと: zero direction を許容するか、UI 側で正規化するかを仕様確認する。
# 2026-08-02: Shape procedural operator input normalization

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Shape の Wiggle Paths / Zig Zag operator の amount・frequency 更新で非有限値を拒否し、frequency を `0..10000` に制限した。
- 価値: シェイプのプロシージャル頂点生成へ NaN/Inf や過大周波数が入る経路を抑える。
- 次に確認すべきこと: Wobble と他 operator の setter 範囲を Core 側仕様と照合する。
# 2026-08-02: Shape wobble operator input normalization

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Hand Drawn Wobble の amount・frequency・pressure jitter・gap probability 更新を有限値と範囲内へ正規化した。
- 価値: ランダムな輪郭生成で NaN/Inf、過大周波数、不正な確率値が使われるのを防ぐ。
- 次に確認すべきこと: Wobble の seed と stroke spacing を含む Core 側設定の範囲確認。
# 2026-08-02: Shape repeater input normalization

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Repeater の copies を `1..1000`、start/end opacity を `0..100` に制限し、非有限 opacity は既定値へ戻すようにした。
- 価値: シェイプ複製数による過大な描画負荷と、不正な透明度値の伝播を抑える。
- 次に確認すべきこと: Repeater の位置・scale・rotation のベクトル入力も Core setter の仕様と照合する。
# 2026-08-02: Shape operator JSON normalization parity

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Shape operator の JSON 復元後にも Repeater/Wiggle/ZigZag の値を setter 経由で正規化し、UI 更新時と同じ制約を適用した。
- 価値: Core operator の `fromJson()` が直接受け入れた異常値を、レイヤー境界で吸収できる。
- 次に確認すべきこと: Wobble を含む全 operator の getter/setter API が揃った時点で同じ復元後検証へ拡張する。
# 2026-08-02: Shape Wobble JSON normalization parity

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Hand Drawn Wobble の JSON 復元後にも amount・frequency・pressure jitter・gap probability の範囲検証を適用した。
- 価値: Core operator の復元処理が受け入れた異常値を、レイヤー境界で UI 更新時と同じ基準に補正する。
- 次に確認すべきこと: seed と stroke spacing の復元 API が利用可能になった時点で追加検証する。
# 2026-08-02: Shape operator restore coverage expansion

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: JSON 復元後の operator 正規化を Trim Paths、Offset Paths、Pucker/Bloat、Rounded Corners まで拡張した。
- 価値: 主要なパス加工 operator の有限値・範囲検証をレイヤー境界で一貫させた。
- 次に確認すべきこと: Repeater の point/vector と rotation の有限値検証。
# 2026-08-02: Shape repeater transform restore normalization

- 関連: `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Repeater の anchor/position/scale、offset、rotation を JSON 復元後に有限値・範囲へ正規化した。scale の不正値は `(1,1)` に戻す。
- 価値: Repeater の累積変換で NaN/Inf や極端な指数計算が発生する経路を抑える。
- 次に確認すべきこと: scale のゼロ値を仕様として許容するか確認する。
# 2026-08-02: Procedural 3D mesh density restore bounds

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: Terrain columns/rows を `2..4096`、Path samples を `2..10000`、sides を `3..256` に制限して JSON 復元するようにした。
- 価値: 破損プロジェクトがメッシュ生成時に無制限の頂点数や過大な描画負荷を発生させるのを防ぐ。
- 次に確認すべきこと: UI の品質プリセットが想定する最大密度と上限値の整合。
# 2026-08-02: Render queue restore bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: レンダーキュー JSON 復元を最大 `10000` ジョブ、各ジョブの multi-channel 出力チャンネルを最大 `128` 件に制限した。
- 価値: 破損したキュー定義による大量ジョブ生成や、チャンネル配列の過大なメモリ使用を防ぐ。
- 次に確認すべきこと: selected frame ranges / render passes の復元件数上限も運用値と照合する。
# 2026-08-02: Render queue nested restore bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: 各レンダージョブの selected frame ranges を最大 `10000` 件、render passes を最大 `256` 件に制限して復元するようにした。
- 価値: 巨大なネスト配列による復元時間・メモリ使用の膨張を抑える。
- 次に確認すべきこと: layer whitelist/blacklist の配列上限と、長大な文字列フィールドの扱い。
# 2026-08-02: Render queue layer-filter restore bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: layer whitelist/blacklist の JSON 復元を各最大 `10000` 件に制限し、文字列以外の要素を無視するようにした。
- 価値: 壊れたフィルター配列が大量の LayerID 生成や不要な検索負荷を発生させるのを防ぐ。
- 次に確認すべきこと: 長大な ID 文字列や不正 ID の `LayerID` 構築時の挙動。
# 2026-08-02: Disk preview manifest validation bounds

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: preview manifest の frame 配列を最大 `100000` 件に制限し、非オブジェクト・負の frame 番号を有効エントリとして扱わないようにした。
- 価値: 壊れた manifest による過大な走査や不正な frame ヒットを防ぐ。
- 次に確認すべきこと: manifest の frameCount と実ファイル一覧の整合性を、容量削減中も確認する。
# 2026-08-02: Render queue numeric restore normalization

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: JSON 復元時の解像度を `1..16384`、FPS を有限値かつ `0.001..240`、bitrate を `0..1000000` に制限し、フレーム範囲の負値も補正した。
- 価値: 通常の更新 API を経由しない復元経路でも、出力設定の NaN/Inf・過大値・逆順範囲を防ぐ。
- 次に確認すべきこと: overlay の数値設定と audio bitrate/sample rate の復元値も同じ経路で照合する。
# 2026-08-02: Render queue overlay and audio restore normalization

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: overlay offset/scale/rotation と audio bitrate を JSON 復元時に有限値・範囲へ正規化した。
- 価値: 復元経路から極端なオーバーレイ変換や過大な音声エンコード設定が実行へ流れるのを防ぐ。
- 次に確認すべきこと: 実際の出力フォーマットごとの bitrate/sample rate の許容範囲を受入れ確認する。
# 2026-08-02: Render queue LayerID string bound

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: queue JSON から復元する LayerID 文字列を最大 `1024` 文字に制限した。
- 価値: 巨大な文字列を ID として構築し、フィルター照合へ流す経路を防ぐ。
- 次に確認すべきこと: LayerID の正規フォーマット検証を既存の ID API で実施できるか確認する。
# 2026-08-02: Puppet hit-test threshold bound

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: Puppet pin の hit-test threshold を有限値かつ `0..100000` に制限した。
- 価値: 巨大な threshold の二乗で float overflow が起き、全 pin が誤選択される経路を防ぐ。
- 次に確認すべきこと: zoom 値と pin overlay のスケール計算も同じ overflow 条件で確認する。
# 2026-08-02: Puppet overlay input normalization

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: Puppet overlay 描画で pin 座標の非有限値をスキップし、renderer zoom を有限値かつ `0.001..10000` に制限した。
- 価値: 破損した pin 状態や異常 zoom が overlay の座標・サイズ計算を壊すのを防ぐ。
- 次に確認すべきこと: 変形 mesh vertex の非有限値が返る場合の edge 描画スキップ。
# 2026-08-02: Puppet mesh overlay vertex validation

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: Puppet mesh overlay の edge 描画前に、参照 vertex の x/y が有限値か検証するようにした。
- 価値: 変形エンジンから壊れた vertex が返っても、overlay 全体ではなく該当 edge だけを安全にスキップできる。
- 次に確認すべきこと: deformation 結果そのものの有限値検証と renderer 境界の扱い。
# 2026-08-02: Image sequence frame dimension guard

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 連番フレームを CPU/GPU バッファへ変換する前に、画像寸法を正値かつ最大 `16384x16384` と検証するようにした。
- 価値: レイヤーサイズ未確定時でも、異常に巨大な素材がメモリ確保と変換処理へ流れるのを防ぐ。
- 次に確認すべきこと: 単一画像の `loadFromPath()` 経路にも同じ寸法上限が適用されているか確認する。
# 2026-08-02: Single image dimension guard

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: OIIO の単一画像ヘッダー検査でも寸法を正値かつ最大 `16384x16384` に制限した。
- 価値: 連番以外の単一画像でも、過大な素材を AssetManager とバッファ変換へ渡さない。
- 次に確認すべきこと: 上限超過時に既存画像を保持するか、placeholder を表示するかの UX 方針。
# 2026-08-02: Rig2D restore array bounds

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: Rig2D JSON 復元前に bones `4096`、controls `1024`、constraints/propertyBindings `4096`、smartBones `1024` の上限を適用した。
- 価値: Core 側を変更せず、破損プロジェクトによるリグ要素の大量生成を親レイヤー境界で防ぐ。
- 次に確認すべきこと: 既存プロジェクトの最大リグ規模と上限値の運用整合。
# 2026-08-02: OIIO decode dimension guard

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: OIIO の実デコード関数でも最大 `16384x16384` と正の channel 数を検証してから RGBA 変換・QImage 確保を行うようにした。
- 価値: ヘッダー検査後にファイルが差し替わる競合や prefetch 経路でも、巨大画像の確保を防ぐ。
- 次に確認すべきこと: OIIO のタイル/多解像度画像で spec 寸法と実デコード寸法が一致するか確認する。
# 2026-08-02: Rig2D skin mesh restore validation

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: Rig2D の skin mesh 復元前に、頂点の必須 weight/bone index 配列長・有限値を検証し、頂点を最大 `1000000` 件、triangle index を最大 `3000000` 件へ制限した。
- 価値: Core の malformed JSON 読み込みで配列外アクセスや過大な mesh 生成が起きる経路を親側で防ぐ。
- 次に確認すべきこと: skin mesh の bone index が実際の bone 数以内か、Rig2D 構築後に照合する。
# 2026-08-02: Rig2D skin bone-index validation

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin mesh の bone index を復元対象 bones 数と照合し、範囲外の index を `-1` に補正するようにした。
- 価値: 存在しない bone 参照が skin 評価や pose 更新へ伝播するのを防ぐ。
- 次に確認すべきこと: weight 合計の正規化を Core 復元後に再確認する。
# 2026-08-02: Rig2D skin weight normalization

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin mesh の各頂点 weight を `0..1` に clamp した後、合計 1 へ正規化し、合計 0 の場合は先頭 weight を 1 にするようにした。
- 価値: 不正な weight 合計による未定義・過剰変形を抑え、skin 評価へ安定した入力を渡す。
- 次に確認すべきこと: bone index が `-1` の weight を正規化対象から除外する必要があるか仕様確認する。
# 2026-08-02: Rig2D invalid-bone weight exclusion

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: 範囲外 bone index に対応する weight を `0` として扱い、正規化合計から除外するようにした。
- 価値: 存在しない bone への影響が、残存 weight として変形結果へ混入するのを防ぐ。
- 次に確認すべきこと: 全 bone index が無効な頂点の fallback を、rest position 維持として受け入れ確認する。
# 2026-08-02: Rig2D all-invalid skin fallback

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin vertex の全 bone index が無効な場合、fallback weight を無効 bone に割り当てず全て `0` にするようにした。
- 価値: 存在しない bone への weight 1 が残り、Core の skin 評価へ不正影響が入るのを防ぐ。
- 次に確認すべきこと: 全 weight 0 の頂点を rest position で保持する Core 側挙動を確認する。
# 2026-08-02: Rig2D skin triangle integrity check

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin mesh の triangle index を復元後の頂点数未満に制限し、3要素単位でない末尾 index を除去するようにした。
- 価値: skin 評価時の頂点配列外参照と不完全な三角形生成を防ぐ。
- 次に確認すべきこと: 重複 index や極小面積三角形を許容する既存仕様との整合。
# 2026-08-02: Rig2D skin vertex range normalization

- 関連: `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`
- 事実: skin mesh の頂点位置を `±1000000`、UV を `±100000` に clamp してから Core 復元するようにした。
- 価値: 異常に大きい座標・UV が skin 変形や描画境界計算を overflow させるリスクを下げる。
- 次に確認すべきこと: 通常の高解像度素材でこの上限が狭すぎないか確認する。
# 2026-08-02: Stabilizer parameter normalization

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: Stabilizer の output size を最大 `16384x16384`、border fill を `0..1`、smoothing window を `1..10000` に正規化した。
- 価値: 既存のスタビライザー処理に極端な画像サイズ・境界値・平滑化窓が流入するのを防ぐ。
- 次に確認すべきこと: Live/BatchStabilizer の setParams も共通正規化へ統一できるか確認する。
# 2026-08-02: Live and batch stabilizer parameter parity

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: Live/BatchStabilizer の setParams にも output size、border fill、smoothing window の正規化を適用し、Live の history size を `1..10000` に制限した。
- 価値: Stabilizer の全実行経路で同じ入力制約を使い、履歴バッファの過大化も防ぐ。
- 次に確認すべきこと: バッチ入力フレーム数と各 frame 寸法の上限を確認する。
# 2026-08-02: Stabilizer frame input bounds

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: StabilizerEffect の蓄積フレームを最大 `10000` 件、Live/Batch 系の入力画像を最大 `16384x16384` に制限した。
- 価値: 長大なバッチや巨大画像が追跡・平滑化処理のメモリ使用量を無制限に増やすのを防ぐ。
- 次に確認すべきこと: 上限到達時に UI へ拒否理由を返す既存通知経路があるか確認する。
# 2026-08-02: Batch stabilizer path normalization

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: BatchStabilizer の入力・出力ファイルパスを setter で trim するようにした。
- 価値: 空白付き・空のパスがバッチ処理へそのまま伝播するのを抑える。
- 次に確認すべきこと: start/stop 処理で入力ファイル存在と出力ディレクトリ書込可否を明示的に返せるか確認する。
# 2026-08-02: Batch stabilizer process preflight

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: BatchStabilizer::process() で入力が実ファイルか、出力がディレクトリでないか、入力と出力が同一パスでないかを事前検証するようにした。
- 価値: 実処理前にダミー成功を返す・入力を上書きする明らかな経路を防ぐ。
- 次に確認すべきこと: ダミー進捗処理を実際のフレーム読み書きへ置き換える段階を別途設計する。
# 2026-08-02: Point tracker apply bounds

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: トラッキング適用時に pointId を非負へ制限し、FPS を有限値かつ `0..240`、時刻・座標を `±1e9` 以内に検証してからキーフレーム化するようにした。
- 価値: 壊れたトラッキング結果が RationalTime や Transform3D への整数 overflow を起こす経路を防ぐ。
- 次に確認すべきこと: MotionTracker の frame range と comp の作業範囲を適用前に照合する。
# 2026-08-02: Point tracker composition-range validation

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: トラッキング結果の frame を composition frame range と照合し、範囲外のキーを除外し、適用キーがゼロなら失敗を返すようにした。
- 価値: composition 外の keyframe 生成と、何も適用していないのに成功扱いになる経路を防ぐ。
- 次に確認すべきこと: work area を適用範囲にする選択肢が必要か確認する。
# 2026-08-02: Point tracker multi-point apply bound

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`
- 事実: 全トラッキングポイントの一括適用で生成対象 point ID を最大 `1024` 件に制限した。
- 価値: 破損した tracker 結果から大量の Null レイヤーが生成されるのを防ぐ。
- 次に確認すべきこと: 上限到達時に UI へ一部適用の状態を通知する経路を確認する。
# 2026-08-02: Camera tracker analysis range bounds

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: Camera Tracker の解析範囲を composition range と交差させ、最大 `100000` フレームに制限し、入力画像を最大 `16384x16384` に制限した。
- 価値: 逆転・過大な in/out や巨大画像が tracker 解析へ入るのを防ぐ。
- 次に確認すべきこと: decode できたフレームが 0 件の場合に tracker.solve() へ進まないことを実データで確認する。
# 2026-08-02: Camera tracker decoded-frame preflight

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: Camera Tracker は decode に成功したフレーム数を数え、0 件なら solve を実行せず失敗を返すようにした。
- 価値: 空の入力で tracker が見かけ上成功し、空のカメラ/Null レイヤーを生成する経路を防ぐ。
- 次に確認すべきこと: solver が要求する最小フレーム数を確認し、必要なら solve 前に明示する。
# 2026-08-02: Camera tracker minimum-frame preflight

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: Camera Tracker の solve 前に decode 成功フレームが最低 2 件あることを要求するようにした。
- 価値: 単一フレーム入力をカメラトラッキング成功として扱う誤判定を防ぐ。
- 次に確認すべきこと: CameraTracker solver の特徴点・フレーム数の最小要件を API 仕様と照合する。
# 2026-08-02: Camera tracker feature-layer bound

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: Camera Tracker の feature point から生成する Null レイヤーを最大 `1024` 件に制限した。
- 価値: 異常な solver 結果による大量レイヤー追加と composition 構造の過負荷を防ぐ。
- 次に確認すべきこと: 上限到達時の feature point 可視化・通知方法を確認する。
# 2026-08-02: Stabilizer direct-frame dimension guard

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: StabilizerEffect::processFrame() の直接入力にも最大 `16384x16384` の寸法検証を追加した。
- 価値: addFrame() を経由しない呼び出しでも、巨大画像を変換処理へ渡さない。
- 次に確認すべきこと: processFrame の outputSize と入力画像のメモリ上限を実運用値と照合する。
# 2026-08-02: Stabilizer interpolation bounds

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: Stabilizer の bilinear 補間係数を有限値・`0..1` に clamp し、補間後 RGBA を `0..255` に丸めた。
- 価値: 境界計算の異常値が QImage の pixel 値へ伝播するのを防ぐ。
- 次に確認すべきこと: borderFill のサンプル座標が常に source bounds 内になることを確認する。
# 2026-08-02: Stabilizer border coordinate validation

- 関連: `Artifact/src/Effect/ArtifactStabilizer.cppm`
- 事実: border fill のサンプル座標を整数化する前に x/y の有限値を確認するようにした。
- 価値: 異常な逆変換座標が NaN のまま境界 clamp・整数化へ進むのを防ぐ。
- 次に確認すべきこと: source が 1 pixel 幅/高さの場合の bilinear 境界分岐を確認する。
# 2026-08-02: Text paragraph setter range alignment

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: maxWidth/boxHeight を `0..100000`、paragraphSpacing を `0..1000` に制限し、Inspector の hard range と setter を一致させた。
- 価値: JSON・UI・アニメーション経路で極端なテキストレイアウト値が流入するのを防ぐ。
- 次に確認すべきこと: 長文・Box text で上限到達時のレイアウト応答を確認する。
# 2026-08-02: Text stroke, shadow, and path range alignment

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: stroke width、shadow offset/blur、path start/end offset の setter を Inspector hard range と一致させた。
- 価値: 描画余白やパスレイアウトへ極端な値が入り、過大なバッファや不安定な配置になるのを防ぐ。
- 次に確認すべきこと: Source Text / ruby の長文入力に対する長さ上限を仕様と照合する。
# 2026-08-02: Text input length bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: 通常テキスト、Source Text キーフレーム API、Source Text JSON 復元の文字列を最大 `1000000` 文字に揃えた。
- 価値: 長大な入力が shaping・layout・glyph cache を無制限に膨張させるのを防ぐ。
- 次に確認すべきこと: 上限超過を UI に通知する既存エラー表示経路があるか確認する。
# 2026-08-02: Ruby text length bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: rubyText の setter も本文・Source Text と同じ最大 `1000000` 文字へ制限した。
- 価値: ruby 注釈だけが shaping/layout 処理を過大化させる経路を防ぐ。
- 次に確認すべきこと: フォント名や rich-text payload の文字列上限を同じポリシーに含めるか確認する。
# 2026-08-02: Text font-family string bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: font family 名を trim 後最大 `1024` 文字に制限し、空文字は既存どおり Arial に戻すようにした。
- 価値: 異常に長い font resolver 入力による探索・キャッシュ処理の膨張を防ぐ。
- 次に確認すべきこと: 複数フォント fallback 名を扱う場合の上限仕様を確認する。
# 2026-08-02: Text animator count range alignment

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text Animator の add/set API を最大 `16` 件に制限し、Inspector の hard range と一致させた。
- 価値: UI 外の API・JSON 操作から animator が無制限に増え、レイアウト評価が膨張するのを防ぐ。
- 次に確認すべきこと: animator JSON 配列復元と preset 適用時も同じ上限を維持することを確認する。
# 2026-08-02: Source text keyframe frame bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Source Text の keyframe setter が負の frame 番号を拒否するようにした。
- 価値: UI 外の API 呼び出しからタイムライン範囲外の不正 keyframe が追加されるのを防ぐ。
- 次に確認すべきこと: comp frame range の上限も setter 側で照合する必要があるか確認する。
# 2026-08-02: Source text JSON frame bound

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Source Text JSON keyframe の timeValue も `0` 未満を無効として、負の timeScale と合わせて復元しないようにした。
- 価値: API と JSON 復元でタイムライン外キーの扱いを統一する。
- 次に確認すべきこと: composition frame range 上限を超えるキーの扱いを確認する。
# 2026-08-02: Camera tracker solve-result validation

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm`
- 事実: CameraTracker の success 結果でも cameraPath が空、または有限 pose が一つもない場合はレイヤー生成を行わないようにした。
- 価値: solver の不完全結果から空・壊れたカメラ構造を composition に追加するのを防ぐ。
- 次に確認すべきこと: 有効 pose の frame 範囲と実際の cameraPath 件数を照合する。
# 2026-08-02: Render queue string restore normalization

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: JSON 復元時の composition/job name、output path、format/codec、encoder backend、audio path/codec/channel mode を trim してから登録するようにした。
- 価値: 空白付き設定が format 判定・出力先検証・backend 選択を不安定化する経路を減らす。
- 次に確認すべきこと: trim 後に空になる必須項目の扱いを既存 queue validation と照合する。
# 2026-08-02: Render queue channel key normalization

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: マルチチャンネル出力の復元・検証時に、チャンネルキーを trim してから既存の canonical key 判定へ渡すようにした。
- 価値: JSON の空白混入で有効な出力チャンネルが無効扱いになる経路を減らす。
- 次に確認すべきこと: renderer channel key の別名変換が既存仕様どおりか確認する。
# 2026-08-02: Text animator restore normalization

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text Animator の JSON 復元で名前を trim・256文字に制限し、空名を既定名へ戻す。Selector の enum 値を定義範囲へ clamp し、正規表現パターンを4096文字に制限する。
- 価値: 不正な enum 値や極端に大きい selector 設定が復元後の評価・UI表示を不安定化する経路を減らす。
- 次に確認すべきこと: Text Animator の評価側で selector pattern の不正な正規表現をどう扱うかを既存仕様と照合する。
# 2026-08-02: Text animator invalid regex fallback

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text Animator の JSON 復元およびプロパティ更新時に、selector pattern を4096文字へ制限し、無効な正規表現なら regex selector を無効化する。
- 価値: 不正な保存値や編集値が TextAnimatorEngine の regex 評価へそのまま流れる経路を減らす。
- 次に確認すべきこと: regex selector の UI で無効化理由を表示する必要があるか、既存のエラー表示方針と照合する。
# 2026-08-02: Text animator property edit bounds

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: UI からの Text Animator プロパティ更新にも、JSON 復元と同じ名前長・enum・範囲・wiggly・変形・stroke/blur の境界値を適用した。
- 価値: 保存時だけでなく、プロパティ編集から極端な値が評価経路へ入る経路を揃えて制限する。
- 次に確認すべきこと: Text Animator の各表示プロパティの UI range とコード側上限が一致しているか確認する。
# 2026-08-02: Image color interpretation restore normalization

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 素材の source path を JSON 復元時に trim し、入力 transfer function 名を trim 後1024文字に制限してから既存の OCIO 入力解釈へ渡すようにした。
- 価値: 保存値の空白や異常に長い識別子が素材再読込・OCIO 適用経路へそのまま入る可能性を減らす。
- 次に確認すべきこと: transfer function の実ライブラリ名検証を OCIO 側 API で行えるか確認する。
# 2026-08-02: Preview disk manifest integrity tightening

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: ディスクプレビューの manifest 読み込みサイズを16 MiB以内に制限し、全 frame entry を object・非負 frame・正の byte 数・単一ファイル名として検証するようにした。
- 価値: 壊れた manifest やパス要素を含む entry を有効なキャッシュとして扱わず、復元時のメモリ消費とファイル参照の曖昧さを抑える。
- 次に確認すべきこと: 大規模キャッシュの manifest 上限が運用上十分か確認する。
# 2026-08-02: Particle emitter restore collection bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: Particle の JSON 復元で emitter と effector を各1024件までに制限し、object 以外の配列要素を無視するようにした。
- 価値: 壊れた／過大な保存配列が復元時に大量の emitter・effector 生成を引き起こす経路を抑える。
- 次に確認すべきこと: emitter/effector enum の定義範囲を確認し、不正な型値を既定値へ戻す。
# 2026-08-02: Particle restore enum validation

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter shape を0..7、emission mode を0..2へ clampし、effector type は0..10以外を復元対象から除外するようにした。
- 価値: 不正な enum 値が未定義の挙動や意図しない effector 分岐へ流れる経路を減らす。
- 次に確認すべきこと: 現在未対応の effector 種別（Drag/Noise/Collision）の実体化方針を別タスクとして確認する。
# 2026-08-02: Particle emitter numeric restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter 復元時の rate、life/speed、burst、frame rate、mass、auxiliary 設定を有限値・範囲・min/max 整合性で正規化し、texture path も trim・長さ制限した。
- 価値: NaN/無限値や極端な設定が粒子生成数・寿命・補助粒子数へ伝播する経路を抑える。
- 次に確認すべきこと: emitter の位置・速度・scale ベクトルにも同じ有限値検証を適用する。
# 2026-08-02: Particle emitter vector restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter 復元時の velocity random、position、rotation、direction を有限値かつ±1,000,000以内に制限し、scale 系の min/max と中間位置も正規化した。
- 価値: 破損したベクトルやスケール値が粒子配置・速度・補間計算へ伝播する経路を抑える。
- 次に確認すべきこと: textureRows/Cols、startFrame/frameCount、auxTrigger の復元範囲を同じ方針で確認する。
# 2026-08-02: Particle texture frame restore normalization

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter 復元時の texture rows/columns、start frame、frame count、aux trigger を定義範囲へ制限した。
- 価値: 不正なテクスチャ分割や巨大なフレーム範囲がアトラス参照・アニメーション評価へ伝播する経路を抑える。
- 次に確認すべきこと: particle layer のプロパティ編集経路にも同じ frame/texture 上限が揃っているか確認する。
# 2026-08-02: Chroma key property input normalization

- 関連: `Artifact/src/Effects/Keying/ChromaKeyEffect.cppm`
- 事実: Chroma Key の similarity、smoothness、spill reduction のプロパティ編集値を有限値として検証し、既存の評価範囲へ clamp してから CPU 実装へ渡すようにした。
- 価値: UI／外部プロパティ経由の NaN・無限値・過大値がキーイング評価へ入る経路を減らす。
- 次に確認すべきこと: Chroma Key の setter 直接呼び出し経路にも同じ範囲が適用されているか確認する。
# 2026-08-02: Particle emitter property edit bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter のプロパティ編集経路にも、shape/mode、位置・回転・方向、rate/burst、texture path・分割数の有限値・範囲制限を適用した。
- 価値: JSON 復元時だけでなく、UI／外部プロパティ編集から極端な値が粒子生成へ入る経路を揃えて制限する。
- 次に確認すべきこと: 残りの emitter 寿命・スケール・aux プロパティ編集も同じ上限へ統一する。
# 2026-08-02: Particle lifetime and speed property bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: emitter の frame rate、mass、life、speed、velocity random、direction spread の編集値を有限値・範囲で正規化し、life/speed の min/max 逆転も補正した。
- 価値: 編集経路から NaN や極端な寿命・速度が粒子シミュレーションへ入る経路を復元処理と同じ基準に揃える。
- 次に確認すべきこと: opacity と scale の編集値にも上限を明示して復元基準と照合する。
# 2026-08-02: Particle scale and opacity property bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: particle の scale/opacity の編集値を有限値・範囲で正規化し、各 min/max の逆転を補正した。
- 価値: 補間途中の NaN や不正な範囲が粒子描画へ伝播する経路を抑える。
- 次に確認すべきこと: gravity/wind/drag と aux 系の編集値も同じ有限値基準へ揃える。
# 2026-08-02: Particle physics and auxiliary property bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: drag、gravity、wind、turbulence、max particles、auxiliary 設定、color position の編集値を有限値・範囲で正規化し、不正な QColor は既存値を維持するようにした。
- 価値: 物理計算・補助粒子生成・色補間へ NaN や極端な値が入る経路を抑える。
- 次に確認すべきこと: particle layer 全体の render 設定にも同じ有限値検証が揃っているか確認する。
# 2026-08-02: Particle render settings bounds

- 関連: `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- 事実: particle render の blend/billboard/sort enum を定義範囲へ制限し、soft particle distance と stretch factor を有限値・範囲で復元・編集するようにした。
- 価値: 不正な描画モードや極端な距離・伸長値がレンダー設定へ入る経路を抑える。
- 次に確認すべきこと: render 設定の enum 定義が tooltip の値域と一致しているか確認する。
# 2026-08-02: Procedural3D property edit bounds

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: terrain/path の columns、samples、sides、サイズ、noise、radius、audio gain、source path などの編集値を復元時と同じ有限値・範囲へ揃えた。
- 価値: UI／外部プロパティ編集から過大なメッシュ分割や NaN が Procedural3D 生成へ入る経路を減らす。
- 次に確認すべきこと: path の taper/twist/repeat/scale と material の数値編集も同じ基準で確認する。
# 2026-08-02: Procedural path and material property bounds

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: Procedural3D の material emission strength と path の taper、twist、offset、repeat、scale を有限値・範囲で正規化した。
- 価値: パス生成・マテリアル計算へ NaN や極端な係数が入る経路を抑える。
- 次に確認すべきこと: path の sourceLayerId や base color の入力検証が既存責務と一致しているか確認する。
# 2026-08-02: Procedural3D string and color input normalization

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: path sourceLayerId を trim・1024文字に制限し、Procedural3D の base/emission color は不正な QColor を無視して既存値を維持するようにした。
- 価値: 外部プロパティ入力による曖昧なレイヤー参照や不正色の伝播を抑える。
- 次に確認すべきこと: JSON 復元側の sourceLayerId と色値にも同じ正規化が適用されているか確認する。
# 2026-08-02: Procedural3D JSON restore parity

- 関連: `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`
- 事実: Procedural3D JSON 復元でも terrain/path の source path、sourceLayerId、サイズ、audio gain、radius、taper、twist、offset、repeat、scale を編集経路と同じ基準で正規化した。
- 価値: 保存データ経由だけ異なる値域や非有限値が生成器へ入る不整合を減らす。
- 次に確認すべきこと: JSON 復元とプロパティ編集の共通正規化関数化は、既存モジュール依存を見て別途判断する。
# 2026-08-02: Preview disk manifest duplicate frame rejection

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 事実: manifest の frame entry に同一 frame 番号が重複している場合、キャッシュ全体を無効として扱うようにした。
- 価値: 破損・競合した manifest の一部だけを有効扱いする曖昧さを減らし、frame とファイルの一対一対応を保つ。
- 次に確認すべきこと: manifest 書き込み側が常に重複を生成しないことを確認する。
# 2026-08-02: OCIO input transform argument normalization

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: 素材入力変換の直接呼び出しでも source color space と transfer function を trim してから OCIO processor・legacy fallback へ渡すようにした。
- 価値: レイヤー setter を経由しない呼び出しでも空白混入した識別子が OCIO lookup を不安定化する経路を減らす。
- 次に確認すべきこと: OCIO runtime config に存在しない source color space の fallback 方針を実素材で確認する。
# 2026-08-02: Image source path restore bound

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の単一 source path も JSON 復元時に trim し、最大32768文字へ制限するようにした。
- 価値: sequence path と単一素材 path で復元時の入力境界が異なる不整合を減らす。
- 次に確認すべきこと: loadFromPath 側の既存パス正規化と重複しないか確認する。
# 2026-08-02: Image sequence property edit normalization

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の source path 編集値を trim・32768文字に制限し、sequence frame rate の編集プロパティを追加して0または0.001〜1000fpsへ正規化するようにした。
- 価値: sequence の保存復元・API 設定・プロパティ編集で frame rate の扱いを揃え、既存 sequence source にも変更を即時反映する。
- 次に確認すべきこと: sequence path 配列を UI から編集する導線が必要か、既存の素材管理責務と照合する。
# 2026-08-02: Image sequence frame rate property exposure

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: Image sequence のみ `image.sequenceFrameRate` を Image プロパティグループへ公開し、0.001〜1000fps の hard range と fps 表示を設定した。
- 価値: 追加した sequence frame rate 編集経路を UI の正規プロパティ導線へ接続する。
- 次に確認すべきこと: sequence path 自体の編集責務は Asset 管理 UI と重複しないか確認する。
# 2026-08-02: Image sequence API path bound

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: `setImageSequence()` から登録する各 frame path も trim 後32768文字に制限した。
- 価値: JSON 復元・プロパティ編集・API 登録で sequence path の入力上限を統一する。
- 次に確認すべきこと: ImageSequenceSource 側の path 正規化と責務の重複を確認する。
# 2026-08-02: OCIO settings identifier restore bounds

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: OCIO 設定 JSON の preset name、working space、display、view、looks を trim し、識別子ごとの最大長を設けてから既存の config lookup へ渡すようにした。
- 価値: 設定復元時の異常に長い識別子や空白混入が config 選択を不安定化する経路を減らす。
- 次に確認すべきこと: `ArtifactOCIOConfig::loadFromJson` 側の config path 境界と整合させる。
# 2026-08-02: OCIO config API input bounds

- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`
- 事実: preset 名を trim・256文字、外部 config path を trim・32768文字に制限してから既存の preset/path 解決へ渡すようにした。
- 価値: UI や API からの設定変更でも JSON 復元と同じ識別子・パス境界を適用する。
- 次に確認すべきこと: `loadConfig(const OCIOConfig&)` は所有元の config path を直接利用するため、Core 側の境界と責務分担を確認する。
# 2026-08-02: Image input color interpretation property exposure

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: ImageLayer の Image プロパティグループへ `image.inputColorSpace` と `image.inputTransferFunction` を追加し、既存の `setInputInterpretation()` を編集経路として利用するようにした。
- 価値: P0 の素材別色空間指定を保存/API だけでなく、通常のプロパティ編集導線から扱えるようにする。
- 次に確認すべきこと: 利用可能な OCIO color space の候補表示を既存 property editor が支援するか確認する。
# 2026-08-02: Image color interpretation property guidance

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: Input Color Space の property tooltip に現在の OCIO working space 候補を表示し、Input Transfer に対応する transfer function 名を案内するようにした。
- 価値: 専用 picker を新設せず、素材別色空間指定の入力ミスを減らす。
- 次に確認すべきこと: property editor が候補選択 UI を提供できる場合は、tooltip より選択式へ移行する。
# 2026-08-02: Image OCIO color space canonicalization

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: 入力 color space 名を利用可能な OCIO working space と case-insensitive に照合し、該当時は config の canonical 名へ解決するようにした。不一致は従来どおり空値へ戻す。
- 価値: `sRGB` / `srgb` のような表記差で有効な素材解釈が拒否される問題を減らす。
- 次に確認すべきこと: OCIO config が alias を公開する場合の canonical 名選択を実素材で確認する。
# 2026-08-02: Render queue property string bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: Render Queue の output path を trim・32768文字、job name を trim・256文字に制限する編集経路を追加した。
- 価値: JSON 復元と UI/API からのジョブ更新で文字列境界を統一する。
- 次に確認すべきこと: output format / codec / encoder backend の編集 setter にも同じ正規化が適用されているか確認する。
# 2026-08-02: Render queue codec property bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: output format、codec、codec profile の編集値を trim・256文字、audio source path を trim・32768文字に制限した。
- 価値: Render Queue の JSON・UI/API 更新で encoder 設定文字列の入力境界を統一する。
- 次に確認すべきこと: audio codec と channel mode の setter 長さ・canonical 化を確認する。
# 2026-08-02: Render queue audio property bounds

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: audio codec を trim・256文字に制限し、`updateJob()` の全主要 job/audio 文字列も path 32768文字・識別子256文字へ統一した。
- 価値: 個別 setter だけでなく一括 job 更新経路でも異常に長い文字列が残らないようにする。
- 次に確認すべきこと: audio codec の許可値 canonical 化が既存 encoder 実装の責務か確認する。
# 2026-08-02: Text alignment and wrap enum bounds

- 関連: `Artifact/src/Layer/ArtifactTextLayer.cppm`
- 事実: Text の horizontal/vertical alignment と wrap mode を JSON 復元・プロパティ編集時に定義範囲へ clamp するようにした。
- 価値: 不正な enum 値が paragraph layout の switch 分岐へ流れる経路を減らす。
- 次に確認すべきこと: Text style の他 enum（font weight/style 等）の復元経路も確認する。
# 2026-08-02: Layer expression restore bound

- 関連: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 事実: effect property の JSON 復元時に expression を trim し、最大16384文字に制限してから既存の serialization bridge へ渡すようにした。
- 価値: 保存データから過大な expression が評価・編集 UI へ流れる経路を抑える。
- 次に確認すべきこと: expression の編集 UI/API 経路にも同じ上限を適用する。
# 2026-08-02: Expression editor input bound

- 関連: `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`
- 事実: Expression Copilot から property へ適用する式を trim・最大16384文字に制限してから既存 apply handler へ渡すようにした。
- 価値: 保存復元と編集 UI の expression 長さ境界を揃える。
- 次に確認すべきこと: Python/API から expression を設定する経路にも同じ上限を適用する。
# 2026-08-02: Image load API path normalization

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: `loadFromPath()` 本体でも path を trim・32768文字に正規化してから sequence 判定、OIIO 読み込み、Asset 登録、保存値へ利用するようにした。
- 価値: API 直接呼び出しでも property/JSON と同じ素材 path 境界を保証する。
- 次に確認すべきこと: OIIO が扱える実パス長の OS 制約と、32768文字上限の妥当性を確認する。
# 2026-08-02: Property widget expression restore bound

- 関連: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- 事実: property widget の serialized expression 復元も trim・最大16384文字へ制限した。
- 価値: layer JSON と widget 側の個別 property 復元で expression 上限が異なる経路を減らす。
- 次に確認すべきこと: project import が widget serialization を経由するか、復元責務を整理する。
# 2026-08-02: Puppet selected pin validation

- 関連: `Artifact/src/Tool/ArtifactPuppetTool.cppm`
- 事実: selected pin ID の設定時に trim・256文字制限を適用し、現在の pin 集合に存在しない ID は選択状態へ保持しないようにした。
- 価値: 削除済み・破損した pin 参照が overlay や編集 UI に残る状態を減らす。
- 次に確認すべきこと: pin ID の生成・保存側でも同じ文字列上限を確認する。
# 2026-08-02: Project importer collection bounds

- 関連: `Artifact/src/Project/ArtifactProjectImporter.cppm`
- 事実: project import の AI tags を最大10000件・各256文字、compositions を最大10000件、composition layers を最大100000件に制限した。
- 価値: 破損・過大な project JSON による importer の大量オブジェクト生成を抑える。
- 次に確認すべきこと: projectItems と source registry snapshot の復元側にも同等の上限があるか確認する。
# 2026-08-03: Project item importer collection bound

- 関連: `Artifact/src/Project/ArtifactProjectImporter.cppm`
- 事実: `projectItems` 復元前に配列を最大100000件へ制限するようにした。
- 価値: project JSON の過大な top-level item 配列をそのまま manager へ渡さず、import 時の大量生成を抑える。
- 次に確認すべきこと: manager 側の folder nesting 復元にも深さ・子 item 上限があるか確認する。
# 2026-08-03: Project item tree restore bounds

- 関連: `Artifact/src/Project/ArtifactProject.cppm`
- 事実: project item tree の復元に総数100000件、folder nesting depth 64 の上限を追加した。
- 価値: project JSON の深すぎる／大量の nested folder が再帰処理と owned item allocation を過剰に消費する経路を抑える。
  - 次に確認すべきこと: 上限到達時に import 結果へ警告を返す必要があるか確認する。

# 2026-08-03: Project import file size bound

- 関連: `Artifact/src/Project/ArtifactProjectImporter.cppm`
- 事実: JSON プロジェクトの読み込み前にファイルサイズを 256 MiB 以下へ制限した。
- 価値: `readAll()` と JSON パースが巨大な入力で過剰なメモリを消費する経路を抑える。
- 次に確認すべきこと: 大規模な正当プロジェクトの実ファイルサイズが上限に収まるか確認する。

# 2026-08-03: Project item API restore bounds

- 関連: `Artifact/src/Project/ArtifactProject.cppm`
- 事実: `addProjectItemsFromJson()` に項目総数100000件、folder depth 64、sequence path 100000件の上限を追加した。
- 価値: importer以外のJSON復元入口でも、深いツリーや巨大な連番配列による過剰な所有・文字列確保を抑える。
- 次に確認すべきこと: 上限到達時に呼び出し側へ部分復元を通知するAPIが必要か確認する。

# 2026-08-03: Preset JSON file size bound

- 関連: `Artifact/src/Project/ArtifactPresetManager.cppm`
- 事実: mask/effect preset の読み込み前に16 MiBのファイルサイズ上限を追加した。
- 価値: preset JSON の `readAll()` とパースが巨大入力で過剰なメモリを消費する経路を抑える。
- 次に確認すべきこと: 既存presetの実サイズ分布を確認し、必要なら上限値を仕様化する。

# 2026-08-03: Locale JSON file size bound

- 関連: `Artifact/src/Translation/TranslationManager.cppm`
- 事実: locale JSON の読み込み前に8 MiBのファイルサイズ上限を追加した。
- 価値: 翻訳ファイル入力の `readAll()` と再帰的な flatten 処理が巨大入力で過剰なメモリを消費する経路を抑える。
- 次に確認すべきこと: 翻訳運用で8 MiBを超えるlocaleが発生しないか確認する。

# 2026-08-03: Batch template JSON file size bound

- 関連: `Artifact/src/Render/ArtifactBatchRenderer.cppm`
- 事実: batch template JSON の列挙・読み込み前に8 MiBのファイルサイズ上限を追加した。
- 価値: template directory 内の巨大JSONが一覧取得時に無制限に読み込まれる経路を抑える。
- 次に確認すべきこと: batch templateの運用ファイルサイズが上限に収まるか確認する。

# 2026-08-03: Effect preset collection input bounds

- 関連: `Artifact/src/Effect/ArtifactEffectPreset.cppm`
- 事実: effect preset collection の読み込みにファイル16 MiB、エントリ100000件の上限を追加し、非object要素を無視するようにした。
- 価値: 外部preset JSONの巨大配列や異常要素が大量のpreset所有を引き起こす経路を抑える。
- 次に確認すべきこと: 上限到達時に読み込み結果へ警告を返す必要があるか確認する。

# 2026-08-03: Color grading preset input bounds

- 関連: `Artifact/src/Color/ArtifactColorGradingEngine.cppm`
- 事実: color grading preset の読み込みにファイル16 MiB、grading node 100000件の上限を追加した。
- 価値: 外部grading presetの巨大JSONやnode配列による過剰なメモリ・処理量を抑える。
- 次に確認すべきこと: 実運用のgrading presetサイズが上限に収まるか確認する。

# 2026-08-03: Workspace JSON file size bound

- 関連: `Artifact/src/Core/ArtifactWorkspaceManager.cppm`
- 事実: workspace session/layout JSON の読み込み前に8 MiBのファイルサイズ上限を追加した。
- 価値: 壊れた、または巨大化したworkspace設定で `readAll()` が過剰なメモリを消費する経路を抑える。
- 次に確認すべきこと: 既存workspace設定の最大サイズを確認する。

# 2026-08-03: Revision storage input bounds

- 関連: `Artifact/src/Project/ArtifactRevisionService.cppm`
- 事実: revision ledgerを16 MiB、snapshot JSONを256 MiB、ledger内revision件数を100000件に制限した。
- 価値: 履歴復元時の巨大JSON読み込みと大量record生成によるメモリ・処理量を抑える。
- 次に確認すべきこと: 大規模プロジェクトのsnapshot実サイズと履歴件数が上限内か確認する。

# 2026-08-03: Color palette input bounds

- 関連: `Artifact/src/Color/ColorPaletteManager.cppm`
- 事実: palette JSONの読み込みにファイル16 MiB、palette 100000件の上限を追加した。
- 価値: 外部paletteの巨大配列や異常なパス指定による過剰な読み込みを抑える。
- 次に確認すべきこと: 実運用paletteのサイズと件数が上限内か確認する。

# 2026-08-03: Animation preset input bound

- 関連: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- 事実: Property Widgetのanimation preset JSON読み込みに16 MiBのファイルサイズ上限を追加した。
- 価値: ユーザー選択ファイルの巨大JSONがUI操作中に無制限に読み込まれる経路を抑える。
- 次に確認すべきこと: 既存animation presetの最大サイズを確認する。

# 2026-08-03: Bundle IPC response bound

- 関連: `Artifact/src/Application/ArtifactProjectBundleIpc.cppm`
- 事実: Bundle IPCのraw responseとJSON payloadを32 MiB以下に制限した。
- 価値: IPC相手からの巨大応答で受信バッファとJSONパースが過剰に膨らむ経路を抑える。
- 次に確認すべきこと: bundle export/importの正当な応答サイズが上限内か確認する。

# 2026-08-03: Locale flatten bounds

- 関連: `Artifact/src/Translation/TranslationManager.cppm`
- 事実: locale JSONのflatten処理に項目100000件、object depth 64の上限を追加した。
- 価値: サイズ上限内でも極端に深い、または多数の翻訳項目を持つJSONによる再帰・map更新の過剰化を抑える。
- 次に確認すべきこと: 翻訳カタログの実項目数とネスト深度を確認する。

# 2026-08-03: External renderer summary bound

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: external renderer summary JSONの読み込みを16 MiB以下に制限した。
- 価値: 外部rendererからの巨大summaryでqueue処理中のJSONバッファが過剰化する経路を抑える。
- 次に確認すべきこと: external rendererが生成するsummaryの実サイズを確認する。

# 2026-08-03: Shortcut preset input bound

- 関連: `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`
- 事実: shortcut preset JSONの読み込みに4 MiBのファイルサイズ上限を追加した。
- 価値: ユーザー選択のshortcut presetが巨大な場合にUI上で無制限に読み込まれる経路を抑える。
- 次に確認すべきこと: 既存shortcut presetの最大サイズを確認する。

# 2026-08-03: Debugger JSON input bounds

- 関連: `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- 事実: debugger bundle/state JSONの読み込みに8 MiBのファイルサイズ上限を追加した。
- 価値: 診断UIのJSON読み込みで巨大なdebug bundleやstateが無制限に展開される経路を抑える。
- 次に確認すべきこと: 通常のdebug bundle/stateサイズが上限内か確認する。

# 2026-08-03: Color grading preset name validation

- 関連: `Artifact/src/Color/ArtifactColorGradingEngine.cppm`
- 事実: grading preset名を空、`.`/`..`、256文字超、ディレクトリ区切りを含む値として保存・読み込みできないようにした。
- 価値: preset名から保存先パスを組み立てる際の意図しないディレクトリ逸脱を防ぐ。
- 次に確認すべきこと: 既存preset名の命名規則がこの制約に適合するか確認する。

# 2026-08-03: Color grading preset atomic save

- 関連: `Artifact/src/Color/ArtifactColorGradingEngine.cppm`
- 事実: grading preset保存をQSaveFile経由にし、全payload書き込み成功時のみcommitするようにした。
- 価値: 保存途中のI/O失敗で既存preset JSONが中途半端な内容に置き換わるリスクを抑える。
- 次に確認すべきこと: 実環境でpreset保存後の再読み込みを確認する。
