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

- 状態: 未解決・運用課題
- 関連: `tools/generate_doc_inventory.py`、`docs/INDEX_GENERATED.md`、2026-07-27 以降の planned milestone 文書
- 事実: 生成スクリプトは新規 Markdown を収集できるが、文書ヘッダの短い Status だけを抽出するため、実装監査に基づき INDEX へ手動同期した詳細な Partial／Not started の説明を再生成時に失う。また、最新 milestone 文書が生成後に追加された場合は INDEX から欠落する。
- 価値・懸念: 現在は既存の詳細状態を守るため再生成を避けているが、インベントリの完全性と状態説明の一貫性が分離している。生成時の既存 Status 差分保持、または文書側の構造化 Status／監査サマリー抽出が必要。
- 次の確認: `STATUS` メタデータと監査要約を文書側へ統一し、生成器が既存の詳細状態を安全に再利用できる設計を検討する。
