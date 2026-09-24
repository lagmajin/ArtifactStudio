**最終更新:** 2026-09-24

# コラボレーション実装監査

2026-09-24 追記: 共通 Property Widget 行の preview／commit は、`setValue()` より前に選択対象すべての layer ID で UndoManager layer guard を照会する。ロック喪失後の commit／preview は止め、受理済み preview があれば全対象の編集開始時 value/keyframe/animatable snapshot を戻す。cancel でも preview 基準を戻して破棄する。これはコード経路の確認であり、実機での lock lease 失効中の操作は未検証。他の直接 mutation 入口は未監査。

2026-09-24 追記: `TextContentUndoCommand`、`PuppetPinUndoCommand`、`Deformation2DStateUndoCommand`、`DeformerControlKeyframeUndoCommand` は owner layer ID を UndoManager lock guard に公開する。`TextContentUndoCommand` は専用 collaboration encoder と `layer.text` remote apply で同期する。PuppetPin fallback／Deformer keyframe dedicated operation は未実装。keyframe drag で before/after deformation snapshot が利用できる場合は `layer.deformation2D` CAS に集約して同期する。対応 UI rollback と runtime の lock 失効挙動は未検証。

2026-09-24 追記: `AudioMixerSnapshotUndoCommand` は Composition-wide mixer snapshot で layer identity を持たないため、collaboration scope を unresolved にした。session 中は UndoManager の mutation guard が拒否し、mixer snapshot helper が変更前値へ rollback する。composition-level lock／operation は未実装で、rollback の runtime は未検証。

2026-09-24 追記: Inspector の component descriptor、clone effector stack、cloner transform stack helper は layer mutation guard を snapshot 取得と direct mutation の前に照会する。component descriptor は `layer.components` expected-value operation で同期し、remote apply も current snapshot を compare-and-set する。clone effector stack と cloner transform stack は `layer.stack` expected-value operation で push／undo／redo と remote apply に対応する。lock がない場合は共通 layer mutation guard が編集前に拒否する。実機での rollback／同期確認は未実施。

`collaboration-donor-improvement` スキルの原則に照らした、現行コラボレーション実装の監査結果。全項目は実コード確認済み。

## 対象

| コンポーネント | 場所 | 役割 |
|---|---|---|
| `CollaborationWebSocket` | `ArtifactCore/include/Network/`, `src/Network/` | Qt WebSocketクライアント(再接続/heartbeat/5メッセージ+rule sync) |
| `CollaborationProtocol` | `ArtifactCore/src/Collaborate/` | ReactiveRule同期メッセージ |
| `CollaborationSession` | `ArtifactCore/src/Collaborate/` | 参加者名簿・ロック台帳・操作ログ(トランスポート非依存) |
| `CollaborationSessionAdapter` | `ArtifactCore/src/Collaborate/` | WebSocket と Session の局所アダプター。参加者・presence・lock・operation を既存経路へルーティング |
| `CollabPresenceWidget` / `CollaborationDockController` | `Artifact/include/Widgets/`, `Artifact/src/AppMain.cppm` | 接続設定、参加者一覧、ローカル presence の送受信 |
| `CollabReview` | `ArtifactCore/src/Collaborate/` | コメント+提案(apply/reject)モデル |
| サーバー | `tools/collaboration-server/server.js` | Node.jsプロトタイプ(中継+JSONL operation 履歴+一時 lock/presence) |

## スキル原則との対照

### 準拠している項目

1. **トランスポート境界** — `CollaborationSession` はQObject/signal非依存の状態モデル。`CollaborationSessionAdapter` がWebSocketと接続し、inboundを `process*()` へ、outbound intentを型付きメッセージへ変換する。Qt接続はアダプターの寿命内に閉じる。
2. **presence分類の分離** — `CollabPresenceState` は型付き(カーソル/選択/コンポジション/再生フレーム/ステータス)で、未知キーは `raw` 保持により消失しない。ephemeral分類どおり永続化しない。
3. **コメントは安定IDにアンカー** — `CollabComment` は compositionId/layerId/frame にアンカー。スレッドは1階層のみ、解決/再開/所有者限定編集・削除。
4. **提案は状態を直接変更しない** — `acceptProposal()` は操作配列を**返すだけ**。適用と配信は呼び出し側の責務。ステータス遷移(Pending→Accepted/Rejected/Withdrawn)は強制され、Withdrawn/Acceptedは終端。
5. **ロック権限はサーバー権威** — セッションは要求意図(`requestLocalLock`)と結果(grant/deny)を分離記録。`isLayerLockedByOther()` は自分のロックを阻塞しない。
6. **アセット/ファイル同期は含まれない** — プロトコルにファイル/アセット操作が存在しないことを確認。skill原則5(サイレント上書き禁止)は現状安全。

### 発見事項(対応状況つき)

| # | 重大度 | 発見 | 対応 |
|---|---|---|---|
| 1 | 高 | **エコー照合キーの衝突**: dedupeKeyがタイムスタンプ依存で、同一ミリ秒の複数操作が衝突。さらにサーバー(`server.js` L125-130)が `operation.timestamp` をISO文字列で上書きするため、エコーのキーが元のキーと不一致になり、pending操作が永遠に未確定になる | **修正済み**: `sequence`(opSeq)フィールド追加。サーバーは未知フィールドをspreadで保持するため往復安全。同一ミリ秒衝突の回帰テスト追加 |
| 2 | 高 | **競合解決は部分対応**: property value compare-and-set／property-only macro batch／single keyframe compare-and-set／single property expression compare-and-set を同期し、他 command は未適用。OT/CRDT/merge はない | 競合は拒否して表示し、自動解決しない。server rejection の rollback／sequence 相関は実装済み、全 mutation family は未対応 |
| 3 | 中 | **ロック強制の未統合**: `isLayerLockedByOther()` を参照する編集経路が存在しない(WebSocketクライアント自体がアプリ未接続のため) | 統合フェーズの必須作業として記録。ロック未取得レイヤーへのリモート操作は現状拒否されない |
| 4 | 中 | **Undo統合が未定義**: リモート操作適用時のローカルUndoスタックの扱い(per-user/shared/compensating)が未決 | skill原則4に基づき設計判断待ち。提案モデルなら「accept時に1つのcompensating可能な操作列として適用」が候補 |
| 5 | 中 | **認証・権限なし**: サーバーは任意のclientIdでjoin可能。なりすまし・上書きが可能 | プロトタイプの範囲内。本番利用には認証層が必須。サーバー側課題 |
| 6 | 低 | **サーバーがtimestampを上書き**: クライアント生成時刻は操作レベルで保持されない(監査性)。opSeqで実用上は解決済みだが、サーバー側で `clientTimestamp` を分保存する改善候補 | サーバー改善候補として記録 |
| 7 | 低 | **プレゼンス stale 検出**: セッションモデルは heartbeat timeout による stale participant 判定を持つが、サーバーの最新 presence 自体はクライアント切断時まで残る | **部分対応**: セッション側判定は実装済み。サーバー切断 cleanup で presence を除去。サーバー側独立 timeout は未実装 |
| 8 | 低 | **rule syncとoperation syncの二重プロトコル**: `CollaborationProtocol`(ReactiveRule)と操作同期が別系統。ReactiveEventsは凍結中のため統合は凍結解除後 | ReactiveEvents凍結解除まで保留 |

## 現状更新 (2026-09-24)

MainWindow の Collaboration dock から `ws://` / `wss://`、共有 project ID、表示名を指定して接続できる。設定は `QSettings` に保存する。ローカルの Composition、再生フレーム、選択 layer ID は既存の Composition / frame / selection イベント購読から取得し、200ms の coalescing と同一 payload の抑制を通して presence として送る。参加者一覧は join / leave / presence で同期され、ローカルユーザーと識別色も表示される。

既知 operation schema はサーバーと `CollaborationSessionAdapter` の送受信境界で検証する。空の type、既知操作の空 `layerId`、各操作 payload の不正、送信元 client ID 不一致は拒否される。未知 type は前方互換のため通過する。選択中の単一 layer に対する手動 lock request/release UI を追加し、サーバーの grant/deny/release、他参加者の `lock_updated`、参加時の既存 lock snapshot を UI に反映する。再接続時はローカルの lock 仮定を破棄し、サーバー状態の再受信を待つ。

Composition／選択 layer／再生 frame に紐づく review note を `review.comment.add` operation として共有し、返信、本人コメントの編集、削除 tombstone、thread の resolve/reopen も操作として同期する。選択 note の既存 layer anchor は選択し、frame anchor は Playback Service 経由で seek する。削除は本文を tombstone 表示にし、返信を保持して operation history の順序再生を安定させる。ローカル操作はサーバー echo 後に反映し、他クライアント操作とサーバー version 順を保つ。サーバーはプロジェクト ID の SHA-256 名を使う JSONL に operation 履歴を追加し、再参加と再起動後に履歴を replay する。保存に失敗した operation は broadcast しない。受信時は operation client ID とコメント author ID、既存スレッド状態、所有者、layer anchor、スキーマを検証し、レビュー状態だけを更新する。

追跡対象 Undo command と既知 server operation には部分的な lock enforcement を追加したが、全編集 command を止める強制機構ではない。`SetLayerPropertyValueCommand` と、それを1 childだけ含む MacroUndoCommand wrapper は local push／undo／redo を `property.set` operation として送る。さらに全 child が property value command の MacroUndoCommand は最大128件の `property.batch` operation として送信し、全対象 layer の lock を要求する。単独 `SetLayerPropertyKeyframesCommand` は `property.keyframes` operation として keyframe 列と任意の animatable flag を compare-and-set し、単独 `SetLayerPropertyExpressionCommand` は `property.expression` operation として expression を compare-and-set する。各 operation は期待値を含む。remote 側は現値が一致した場合のみ適用し、送信元は server echo を二重適用しない。不一致または旧 payload は Collaboration dock に競合として表示する。Undo/Redo setter も期待値を検査するため、remote 更新後の古い command が状態を上書きしない。共同セッション中、UndoManager は対応可否を push／undo／redo 前に照会し、未対応 command は適用前に拒否する。ただし UndoManager を通らない直接 mutation 経路はすべて監査・遮断されていない。server の operation 拒否応答は `clientId + opSeq` を返し、保留 push／undo／redo を補償して local stack を復元する。ACK／拒否が確定するまで他の UndoManager mutation と history 保存・offload を保留する。再接続後は `room_ready` までの履歴 replay で保留 operation の echo を照合し、履歴にないものを未コミットとして補償する。server rejection は WebSocket callback を通じ UI に sequence と理由を表示する。他の property command／transform／structure operation も同期しない。

コメント編集では編集前の本文、編集者名／client ID、時刻を replay 中に revision として蓄積する。Review dock の History 操作で未削除コメントの直近 50 版を確認できる。メモリ上のコピー増大を避けるため古い revision は review model から落とすが、元 operation はサーバー履歴上限まで保持される。履歴は operation replay から再構成し、別形式の二重永続化は行わない。

Collaboration server はプロトタイプで、履歴のみファイルへ永続化する。`clientId + opSeq` による重複登録抑止を行い、同じ sequence に異なる operation 内容が届けば拒否する。operation は追記後にファイル `fsync` が成功してから ACK／配信する。書込み失敗後に末尾を元サイズへ戻せなかった room は再起動・調査まで operation 追記を拒否する。起動時は破損した最終レコードだけを回復し、途中の破損はファイルを変更せず room 読み込みを拒否する。これは OS のファイル同期境界であり、ディレクトリエントリやストレージ装置の電源断耐性までは保証しない。受信 frame は 2 MiB、operation は 1 MiB、presence は 64 KiB までとし、join identity と lock layer ID にも長さ制限を設けた。これらは資源上限であり、client ID の真正性や権限を保証しない。30秒間隔の WebSocket ping/pong で半開き接続を検出し、timeout 時は disconnect cleanup により presence と lock を解放する。クライアントの bounded outbound queue は容量超過時に古いメッセージを破棄せず、新しい送信を拒否して既存 protocolError とログに出す。durable operation は非接続時にキューへ入れず、拒否時は Session の pending log/dedupe state を取り消す。lock request/release は送信失敗時に UI の pending 表示を戻す。presence は失敗後も dirty として保持し、500ms〜8秒の exponential backoff で再送する。履歴はプロジェクトごとに既定 16 MiB／100,000 operation の上限を設け、到達後は古いデータを消さず新規 operation を拒否する。上限は環境変数で調整できるが、snapshot／圧縮と管理 UI は未実装。既存の上限超過履歴は読み込み可能だが追記不可。認証・権限管理、保存データの暗号化／保持期限、運用受入れは未実装。接続 UI は誰でも共有 project ID で参加できる制約を明示する。履歴はレビュー本文を含むため、サーバー README に保存先とネットワーク上の制限を記載する。

受信フレーム／各 payload のサイズ制限に加え、同時接続を server 全体 256、project ごとに 64 へ制限した。上限到達 join は履歴読み込み前に拒否する。server は `property.set`、`property.keyframes`、`layer.transform`、`layer.remove` を送信者自身が対象 layer の lock を保持する場合に限り受理し、`property.batch` は全対象 layer を送信者が lock している場合に限り受理する。join 時は `room_ready` barrier を送り、クライアント側は初期 lock snapshot 前の対象コマンドを止める。join payload の project fingerprint（`savedAt` を除外した project JSON の SHA-256）を `.baseline.json` に保存し、room baseline 不一致を拒否する。これは project snapshot 転送ではない。UndoManager の lock guard は property value/keyframe/expression、layer ID を保持する property／structure／state／animation／mask・matte／text・source／component／modulation／audio／variant 系 command と effect property/preset/keyframe/expression/modulation/mask command（owner layer を project item tree 上の全 Composition から解決し lock 判定、解決できない場合は拒否）、複数 layer 整列、Composition 解像度変更、visibility/lock/solo/shy/blend、macro、Transform Gizmo の single/multi transform を対象とし、collaboration session 中は自分の lock 保持を要求する。`AddLayerCommand` は新規 layer 作成時に lock を要求せず、undo 時および既存 layer の参照変更時は影響先を列挙して lock を要求する。layer creation の operation sync は未実装。他の編集 command は layer identity を公開していないため、現時点では local lock guard 対象外。拒否は Collaboration dock の lock 状態欄へ表示する。これらは負荷上限／部分 enforcement であり、認証・rate limit と全 mutation 経路の lock enforcement は未実装。`SetLayerPropertyValueCommand` の operation は MainWindow から送信され remote apply するが、transform／structure operation と他の command family は未接続。

## skill分類による現行機能の整理

| 機能 | 分類 | 状態 |
|---|---|---|
| presence(選択/Composition/再生位置) | `presence` | モデル・WebSocket・MainWindow dock を接続済み。viewport cursor は未接続 |
| コメント(Composition/layer/frame アンカー) | `comment/review` | 追加、返信、本人コメント編集／削除、resolve/reopen の通信・UI を接続 |
| 提案(apply/reject/withdraw) | `comment/review` + `operation sync` の入口 | モデル実装済み |
| 操作同期(transform/property) | `operation sync` | `SetLayerPropertyValueCommand` の expected-value 付き push／undo／redo と remote apply を実装。未対応 UndoManager command はローカル拒否。直接 mutation の遮断、server rejection の確定・rollback、他 command、transform、structure は未実装 |
| ドキュメント同期(状態複製) | `document sync` | 未実装(現スコープ外が正) |
| アセット/blob同期 | `asset sync` | 未実装(現スコープ外が正) |

## 次の推奨順序(donor-matrixのdecision guidance準拠)

1. ~~WebSocket→Sessionアダプタ~~ — 接続済み。MainWindow の join/presence/roster 経路も接続済み
2. **operationスキーマの最小定義** — 実装済み(2026-08-22): `CollabOperations.cppm`(module `Collaborate.Operations`)。`property.set`/`layer.transform`/`layer.add`/`layer.remove` のビルダー+バリデータ(未知typeは前方互換で通過)。Sessionに `createLocalOperation(CollabOperationData)` オーバーロード追加(identity/sequence/pending stampは常時session管理)。
3. **プレゼンスstale検出** — 実装済み(2026-08-22): `staleParticipantClientIds(nowMs, timeoutMs)`。heartbeatタイムアウトで参加者を検出、presence更新で即時解消。
4. ~~コメント変更履歴の UX~~ — 編集前の本文・編集者・時刻を replay から再構成し、Review dock の History 操作で確認。proposal review UI は未実装
5. 提案accept時の適用パイプライン(Undo統合設計を含む)
6. ロック状態を編集導線へ適用し、ユーザーに拒否理由を返す
7. 認証・権限、保持期限／容量管理、永続履歴の運用設計(サーバー側)

2026-09-24 の追記: operation 履歴の容量上限と到達時の明示的な書込拒否を導入した。保持期限／スナップショット／圧縮、運用者向け履歴整理 UI は引き続き未実装。

## Donor 境界の再確認 (2026-09-24)

| Donor | 事実として確認した協調モデル | ArtifactStudio への適用境界 |
|---|---|---|
| [Penpot](https://github.com/penpot/penpot) | file-scoped persistent WebSocket で presence と他ユーザーの変更を受け取り、PostgreSQL と Valkey を使う。Frontend は ClojureScript/React、backend は Clojure 系、MPL-2.0。詳細な merge/conflict と offline 動作は今回確認した資料では未確定。 | project/file の room と権限境界の参考。GPU/render state や media asset の同期モデルとして流用しない。 |
| [tldraw](https://github.com/tldraw/tldraw) | room 単位の server-authoritative sync、WebSocket、store diff、再接続後の変更同期、persistence adapter。公式 starter は Cloudflare Durable Objects + SQLite、presence と cursor を扱う。SDK は独自ライセンスで production 利用に license key が必要。 | Room の operation sequence、永続 replay、presence を分離する参考。現状の JSONL サーバーは履歴全件 replay の小規模 prototype で、tldraw と同等の差分同期や権限機能はない。 |
| [Blender Mixer](https://github.com/ubisoft/mixer) | Python の Blender add-on と server で scene/datablock を同期し、参加者位置と選択を表示。Undo/Redo は不整合を起こしやすいと docs が警告。アクセス制御はなく、同一要素の同時編集で破損し得る。GPL-3.0 (broadcaster sub-package は MIT)、2022-08-01 archive 済み。 | desktop DCC の ownership/scene boundary を学ぶ歴史的資料。既存 layer editing へ直接 apply せず、Undo 方針とロック強制なしに live edit を解禁しない。 |
| [Miru](https://miru.media/) | 公式サイトは collaborative photo/video editing を work-in-progress とし、self-hosted/local-first、online/offline を掲げ、AGPL-3.0 と説明。現行の具体的 transport、CRDT、conflict、presence、permission の実装詳細は未確認。 | media asset と local-first/privacy の制約を調べる先。今の JSONL operation relay だけでは asset identity、cache、source path 同期を解決しない。 |

この比較は採用済み機能を意味しない。現段階の具体的改善は ping/pong による半開き接続の検出で、protocol ping は Qt `QWebSocket` が自動応答する。実ネットワークでの timeout と lock cleanup は未検証。

2026-09-24 追記: Inspector の Clone effector stack と Cloner transform stack を `layer.stack` expected-value operation で同期可能にした。server schema は stack kind と bounded array を検証し、操作には対象 layer lock が必要。

2026-09-24 追記: `property.batch` macro は `SetLayerPropertyValueCommand` と `SetLayerPropertyKeyframesCommand` の混在を受け付け、128 entry／1 MiB 上限、全 entry の CAS preflight、失敗時の逆順 compensation を行う。

2026-09-24 追記: `property.batch` は property expression の CAS entry も受け付け、value／keyframe と混在した macro を共有する。各 expression は expected text を事前照合し、途中失敗時は先行変更を補償する。

2026-09-24 追記: Text content を `layer.text` expected-value operation で送受信する。同期 command には Undo persistence serializer を流用せず、対象 layer lock と各文字列 128 KiB 上限を適用する。

2026-09-24 追記: layer rename を `layer.rename` expected-value operation として同期し、server lock enforcement と remote compare-and-set を追加した。

2026-09-24 追記: active layer variant の切替を `layer.variant` CAS operation で送受信する。remote client は現在 index を比較してから `setActiveVariant` を呼び、存在しない index は拒否する。

2026-09-24 追記: Layer Blend の legacy enum (0〜33) を expected-value `layer.blendMode` operation で同期し、対象 layer lock と remote CAS を実装した。

2026-09-24 追記: layer visibility を `layer.visibility` expected-value boolean operation で同期し、server lock と client CAS を適用した。

2026-09-24 追記: layer solo／shy state を `layer.flag` expected-value boolean operation に接続し、server lock enforcement と remote CAS を追加した。

2026-09-24 追記: project-level layer locked bool を `layer.editLock` CAS operation で同期する。CollaborationSession の layer reservation は別機構で、mutation には引き続き予約を要求する。

2026-09-24 追記: `AddLayerCommand` の単体追加／undo／redo に bounded `layer.add`／`layer.remove` snapshot operation と remote apply を接続。追加は隣接 anchor layer reservation 必須（空 composition は不要）、削除は対象 layer lock 必須。remote apply は ID／composition／anchor／snapshot／親・matte・clone source 参照を検査し、既知の外部パス field を含む snapshot は server／Core validation で拒否し、property／batch／component／stack payload の同 field も拒否する（symbolic な propertyPath 識別子は除外）。参照関係に依存する追加 command と dependent layer がある削除は拒否する。実機での複数 client 順序、asset path portability、Composition node／selection の完全一致は未検証。

2026-09-24 追記: `MoveLayerIndexCommand` の `layer.reorder` を追加。server／Core schema は composition ID と bounded integer index pair を検証し、server は対象 layer lock を必須とする。remote apply は expected index CAS 後に既存 Composition move API を使う。Undo／Redo encoder を追加したが、複数 client 同時変更と描画・階層挙動は runtime 未検証。

2026-09-24 追記: `ChangeLayerOpacityCommand` を `layer.opacity` で同期。Core／server は finite 0〜1 の expected/value を検証し、server は対象 layer lock を要求する。Undo／Redo と remote apply に現在 opacity の CAS を入れた。共通 Property Widget preview は選択各 layer の lock guard を通る。mixed opacity の各開始値を保持するよう修正したが、lock 失効と複数 client runtime は未検証。

2026-09-24 追記: `ChangeLayerParentCommand` の `layer.parent` operation を追加。server は canonical UUID の expected/next parent ID と child／旧親／新親 layer lock を検査する。remote apply は parent CAS 後に既存 setter を使うため、Composition 外の ID、self-parent、循環参照を拒否する。複数 client の階層変更／削除交錯と描画・評価挙動は runtime 未検証。

2026-09-24 追記: `SetTextAnimatorStackCommand` の bounded `textAnimators` stack snapshot を `layer.stack` に追加。Core／server は既存 256 KiB 上限と path field scan を適用し、Undo／Redo と remote apply は snapshot CAS を行う。text animator UI と複数 client の描画結果は runtime 未検証。

2026-09-24 追記: `AnimationLayerStackSnapshotCommand` の lock target 実装と `layer.animationStack` collaboration encoder／remote apply を追加。server／Core は path-free object snapshot を各 256 KiB に制限し、expected snapshot CAS と対象 layer lock を要求する。初回の caller pre-apply を考慮し redo は after state を idempotent に受理する。runtime は未検証。

2026-09-24 追記: 音声 de-click range command に bounded layer.audioDeClickRanges operation を接続。64-bit sample index を canonical decimal string にし、Core／server で qint64 範囲、正規順序、件数、byte size を検証。local Undo／Redo と remote apply は range list CAS。複数 client の編集・音声再生上の一致は未検証。

2026-09-24 追記: Deformation 2D の JSON state operation を接続。Core／server は expected/value object snapshot を各 256 KiB、path-free に制限し、送信には layer lock を要求する。local Undo／Redo は snapshot CAS、remote apply は PuppetTool restore API で cache を再構築し、失敗時に以前の snapshot を復元する。実機での変形結果／複数 client 一致は未検証。

2026-09-24 追記: Puppet pin position keyframe drag は、編集開始時と commit 後の deformation2D snapshot を保持できる場合、専用 keyframe command ではなく既存の `Deformation2DStateUndoCommand` に記録する。これにより keyframe property と persisted pin state を同じ snapshot CAS に含める。snapshot が欠ける fallback command は引き続き session dispatch で拒否される。runtime parity は未検証。

2026-09-24 追記: `SolidGradientUndoCommand` が layer lock scope と `property.batch` encoder を公開する。3つの既存 `solid.gradient*` property value を1 batch にまとめ、local undo／redo は all-expected preflight、remote apply は既存 batch CAS／compensation を利用する。Viewport drag は command push 前に値を書き換えるため初回 redo は all-next の場合だけ idempotent に受理する。複数 client の drag／競合と表示結果は未検証。

2026-09-24 追記: Solid size drag の `SolidSizeUndoCommand` を `layer.solidSize` CAS で同期する。Core／server は expected/value の width／height を有限整数 1〜16384 に制限し、server は layer reservation を必須にする。local／remote apply は current size を比較し、setter readback に失敗した場合は expected dimensions へ戻す。SolidImage physics collider との連携を含む runtime は未検証。

2026-09-24 追記: Solid size／gradient command の idempotent after-state 受理範囲を初回 push に限定。Undo／Redo は current state が expected と一致しなければ拒否し、stale local history の成功扱いを防ぐ。実機で競合後のUndo／Redo parity は未検証。

2026-09-24 追記: Source Crop を専用 `layer.sourceCrop` operation に接続した。Core／server は7-field canonical snapshot、各 32 KiB、数値域を検証し、対象 layer lock を必須にする。`ArtifactImageLayer::restoreSourceCropSnapshot` は空矩形を clamp せず保持し、property cache を同期して readback する。local Undo／Redo と remote apply は full snapshot CAS。animated crop property の競合と異なる source dimensions 間の表示 parity はruntime未検証。

2026-09-24 追記: Viewport の Shape corner radius／star inner radius Undo command が `property.set` CAS と layer lock scope を公開するようにした。初回 redo の pre-applied 値だけを冪等に許容し、stale Undo／Redo は拒否する。複数 client の実行時描画 parity は未検証。

2026-09-24 追記: Shape Polygon 頂点 Undo command は `layer.shapePath` expected/value geometry snapshot で同期する。Core／server は Polygon／Bézier の相互排他 geometry、有限座標 ±1,000,000、各 snapshot 256 KiB を検査し、server は対象 layer lock を要求する。remote apply は現在 geometry CAS と readback を行い、失敗時に expected state へ戻す。複数 client の実行時 parity は未検証。

2026-09-24 追記: `ShapePathVertexEditCommand` を `layer.shapePath` expected/value operation に接続した。snapshot は Polygon／Bézier の相互排他 geometry 全体を含み、Core／server が全座標・256 KiB 上限を検査し、server は対象 layer lock を要求する。path 作成時に消える Polygon override も Undo／Redo と remote apply の値に含めた。CAS mismatch は状態を変更せず拒否し、restore failure は expected snapshot への rollback を行う。複数 client parity は未検証。

2026-09-24 追記: `ShapeOperatorValueUndoCommand` を `layer.shapeOperator` CAS で同期する。Core／server は整数 index 0〜100000、64-byte field、finite expected/value を検証し、server は対象 layer lock を要求する。remote apply は operator getter と setter を直接使用して property cache の値に依存しない。readback と複数 client parity は未検証。

2026-09-24 追記: SVG import Undo command を bounded layer.shapeContents CAS で同期する。contents／ordered stack nodes／active content index を一つの snapshot に含め、Core/server のサイズ上限と layer lock、remote expected/readback を追加した。実機での描画順一致と Undo/Redo は未検証。

2026-09-24 補足: `layer.shapeContents` は Core/server の双方で外部 asset path field を拒否し、他の snapshot operation と同じ path portability 境界を保つ。

2026-09-24 補足: `PuppetPinUndoCommand` fallback は public `pinPosition()` が display canvas 座標を返し、`movePin()` が現在選択 layer を使って authored 座標へ変換するため、座標だけの remote operation では再現性を保証できない。現在 composition に owner layer が見つからない状態でのみ fallback するため、collaboration では operation 未対応として fail-closed を維持する。

2026-09-24 追記: `MoveLayerCommand` を frame/time-scale 付き `layer.moveAtFrame` CAS で同期する。position X/Y の expected 値を比較し、既存 `AnimatableTransform3D::setPosition` と readback を通す。未対応の `layer.transform` generic operation と Transform Gizmo snapshot は引き続き別課題。

2026-09-24 追記: 単体／複数選択 Transform Gizmo の Undo command を既存 `property.batch` CAS に接続した。各 layer の対象 frame key だけを差し替える full keyframe snapshot を構築し、他 frame の key と metadata を保つ。local Undo/Redo は snapshot CAS、初回 push のみ caller pre-apply を許容する。property の `isAnimatable()` capability と「key が存在する」状態を分離して送る。最大 128 property changes／1 MiB を超える group は fail-closed。複数 client の frame/key parity と実機操作は未検証。
