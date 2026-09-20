# ホットパス実装ルール

**最終更新:** 2026-09-20

## 目的

再生、描画、合成、入力、GPU command 構築など、短い時間予算で繰り返される経路に、フレーム落ち・操作遅延・メモリ急増を起こす処理を持ち込まないための共通ルールである。

本書は ArtifactStudio、Artifact、ArtifactCore のホットパス変更に適用する。個別設計文書により厳しい制約がある場合は、そちらを優先する。

## ホットパスの範囲

次のいずれかに該当する処理をホットパスとして扱う。

- フレームごとの render、compose、present、playback tick
- scrub、drag、pan、zoom、gizmo、paint など連続入力中の更新
- GPU command、resource transition、upload、readback の投入
- audio callback、decode供給、simulation step
- レイヤー、effect、property、timeline のフレーム評価
- 高頻度イベント、診断、キャッシュ参照

呼び出し頻度が不明な処理は、直接 caller を確認するまでコールドパスと仮定しない。

## 禁止事項

### 1. 非境界な確保

steady-state のホットパスで、次を新規に行わない。

- `new` / `delete`、大容量コンテナの生成・拡張・深いコピー
- texture、buffer、staging resource、fence、PSO、shader、descriptor pool の都度生成
- リングやプールの枯渇時に one-shot resource を追加生成するフォールバック
- `QImage`、CPU画像、preview画像、thumbnail の都度生成・形式変換
- 文字列連結、`QString::arg()`、JSON化、ストリーム整形を伴う無条件ログ

必要な容量は初期化、resize、composition変更、cache rebuildなどのコールドパスで事前確保する。可変負荷には固定上限の ring、pool、arena、呼び出し側workspaceを使う。

### 2. 同期待機と暗黙の転送

- `WaitForIdle`、fence wait、blocking invoke、同期I/Oを通常フレームへ追加しない。
- GPU→CPU readback、CPU→GPU upload、`QImage`変換をAPI内部で暗黙に行わない。
- 非同期APIの内部でリングが満杯になった際、無制限の並列仕事や一時resourceへ逃がさない。
- immediate contextを複数threadから同時操作しない。Diligent submissionは所有laneへ直列化する。

### 3. 無制限なバックプレッシャー回避

キュー、リング、worker、cacheには必ず上限を持たせる。容量不足時は次のいずれかを契約として選ぶ。

- coalesce: 最新要求へ集約する
- defer: pendingのまま公平に再試行する
- drop: staleまたは表示不要な要求だけを破棄する
- back off: producer頻度を下げる
- bounded fallback: 事前確保済みの低コスト経路へ移す

容量不足を恒久的な失敗として記録しない。deferした要求がqueue先頭を塞がず、再試行されることを確認する。

### 4. 毎フレームの全量処理

- composition全体、全layer、全frame、全cache entryの走査を安易に追加しない。
- 不変データのhash、signature、property path、tooltip、diagnostic textを毎回再構築しない。
- revision、generation、dirty bit、stable keyで変更時だけ更新する。
- UI repaintやcache統計通知はsingle-shot coalescer等で集約する。

### 5. 見かけだけの高速化

- quality、resolution、effect、mask、lightingを無断で落として性能修正としない。
- CPU readbackを別threadへ移しただけで、転送量やresource churnが残る場合は解決済みとしない。
- present/vsync待ちとrenderer本体のGPU時間を混同しない。

## 許容される例外

コールドパスの確保、または小さくboundedな確保は、次をすべて満たす場合に限り許容する。

1. 発生条件と最大頻度が明確である。
2. 最大容量と所有期間が明確である。
3. ring、pool、再利用、呼び出し側workspaceで代替できない理由がある。
4. failure/backpressure時に無制限な確保へ移行しない。
5. コードコメントと最終報告に理由を記載する。

## GPU readback固有ルール

- staging textureとfenceは固定数のringまたはpoolで再利用する。
- 全slot busyは正常なbackpressureであり、追加resource生成の根拠にしない。
- slot獲得失敗は呼び出し側へ明示し、対象frameを失敗にせずdefer可能にする。
- callback不発、空画像、generation不一致、composition切替を区別する。
- current visible frameを背景cache buildより優先する。
- D3D12/Vulkanでresource state、fence、Map/Unmap、context ownershipを確認する。

## 診断ルール

性能問題の修正前後で、対象に応じて次を取得する。

- accepted/deferred/dropped request数
- queue depth、ring occupancy、pool miss
- CPU frame time、GPU time、present time
- allocation/resource creation数
- upload/readback bytes
- cache hit/miss、stale rejection

診断が無効なときは文字列を構築しない。詳細ログはcategoryまたは明示flagで遅延評価し、ファイルflushは停止・失敗・checkpoint境界にまとめる。

## レビューチェックリスト

- この関数は1秒または1フレームに何回呼ばれるか。
- steady-stateでheap、GPU resource、thread taskを生成していないか。
- pool枯渇時にone-shot allocationへ逃げていないか。
- 同期待機、readback、upload、Map/Unmapがどのthreadで起きるか。
- queueとcacheの最大容量、退避、再試行、公平性が明示されているか。
- stale resultがgeneration/revisionで拒否されるか。
- disabled diagnosticsのコストが実質ゼロか。
- D3D12/Vulkanの両laneで契約が成立するか。
- 元の画質・意味・入力応答を維持しているか。
- 実機検証が未実施なら、性能改善を断定していないか。

## 今回の再発防止例

非同期GPU readbackの固定ringが満杯になったとき、one-shot staging textureとfenceを毎回生成してはならない。slot獲得を失敗として返し、RAM preview frameをpendingのままqueue末尾へdeferする。これにより、資源数を固定しながら他frameの進行と後続再試行を維持する。
