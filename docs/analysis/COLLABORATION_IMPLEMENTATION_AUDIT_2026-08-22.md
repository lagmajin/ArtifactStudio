**最終更新:** 2026-08-22

# コラボレーション実装監査

`collaboration-donor-improvement` スキルの原則に照らした、現行コラボレーション実装の監査結果。全項目は実コード確認済み。

## 対象

| コンポーネント | 場所 | 役割 |
|---|---|---|
| `CollaborationWebSocket` | `ArtifactCore/include/Network/`, `src/Network/` | Qt WebSocketクライアント(再接続/heartbeat/5メッセージ+rule sync) |
| `CollaborationProtocol` | `ArtifactCore/src/Collaborate/` | ReactiveRule同期メッセージ |
| `CollaborationSession` | `ArtifactCore/src/Collaborate/` | 参加者名簿・ロック台帳・操作ログ(トランスポート非依存) |
| `CollabReview` | `ArtifactCore/src/Collaborate/` | コメント+提案(apply/reject)モデル |
| サーバー | `tools/collaboration-server/server.js` | Node.jsプロトタイプ(中継+履歴+ロック権限) |

## スキル原則との対照

### 準拠している項目

1. **トランスポート境界** — `CollaborationSession` はQObject/signal非依存の純粋状態モデル。inboundは `process*()` メソッド、outboundは `createLocalOperation()` 戻り値。ネットワーク層との結合は未接続(将来アダプタ1枚で済む)。
2. **presence分類の分離** — `CollabPresenceState` は型付き(カーソル/選択/コンポジション/再生フレーム/ステータス)で、未知キーは `raw` 保持により消失しない。ephemeral分類どおり永続化しない。
3. **コメントは安定IDにアンカー** — `CollabComment` は compositionId/layerId/frame にアンカー。スレッドは1階層のみ、解決/再開/所有者限定編集・削除。
4. **提案は状態を直接変更しない** — `acceptProposal()` は操作配列を**返すだけ**。適用と配信は呼び出し側の責務。ステータス遷移(Pending→Accepted/Rejected/Withdrawn)は強制され、Withdrawn/Acceptedは終端。
5. **ロック権限はサーバー権威** — セッションは要求意図(`requestLocalLock`)と結果(grant/deny)を分離記録。`isLayerLockedByOther()` は自分のロックを阻塞しない。
6. **アセット/ファイル同期は含まれない** — プロトコルにファイル/アセット操作が存在しないことを確認。skill原則5(サイレント上書き禁止)は現状安全。

### 発見事項(対応状況つき)

| # | 重大度 | 発見 | 対応 |
|---|---|---|---|
| 1 | 高 | **エコー照合キーの衝突**: dedupeKeyがタイムスタンプ依存で、同一ミリ秒の複数操作が衝突。さらにサーバー(`server.js` L125-130)が `operation.timestamp` をISO文字列で上書きするため、エコーのキーが元のキーと不一致になり、pending操作が永遠に未確定になる | **修正済み**: `sequence`(opSeq)フィールド追加。サーバーは未知フィールドをspreadで保持するため往復安全。同一ミリ秒衝突の回帰テスト追加 |
| 2 | 高 | **競合解決なし**: サーバーは中継のみ(OT/CRDTなし)。同時編集は到着順のlast-write-wins。skill分類の「operation sync」に必要な順序付け・競合ポリシーが未達 | 設計上の範囲: skill推奨シーケンスに従い presence/comment/proposal までを現行スコープとする。プロパティ単位の同時編集は未サポートと明示 |
| 3 | 中 | **ロック強制の未統合**: `isLayerLockedByOther()` を参照する編集経路が存在しない(WebSocketクライアント自体がアプリ未接続のため) | 統合フェーズの必須作業として記録。ロック未取得レイヤーへのリモート操作は現状拒否されない |
| 4 | 中 | **Undo統合が未定義**: リモート操作適用時のローカルUndoスタックの扱い(per-user/shared/compensating)が未決 | skill原則4に基づき設計判断待ち。提案モデルなら「accept時に1つのcompensating可能な操作列として適用」が候補 |
| 5 | 中 | **認証・権限なし**: サーバーは任意のclientIdでjoin可能。なりすまし・上書きが可能 | プロトタイプの範囲内。本番利用には認証層が必須。サーバー側課題 |
| 6 | 低 | **サーバーがtimestampを上書き**: クライアント生成時刻は操作レベルで保持されない(監査性)。opSeqで実用上は解決済みだが、サーバー側で `clientTimestamp` を分保存する改善候補 | サーバー改善候補として記録 |
| 7 | 低 | **lookbehind/条件分岐なし**(regexではなくcollab文脈では該当なし)、**プレゼンスのstale検出なし**: `lastPresenceMs` を記録するが切断検出はheartbeat/タイムアウト未実装 | タイムアウトベースのstale判定を次段階候補に |
| 8 | 低 | **rule syncとoperation syncの二重プロトコル**: `CollaborationProtocol`(ReactiveRule)と操作同期が別系統。ReactiveEventsは凍結中のため統合は凍結解除後 | ReactiveEvents凍結解除まで保留 |

## skill分類による現行機能の整理

| 機能 | 分類 | 状態 |
|---|---|---|
| presence(カーソル/選択/ビューポート/再生位置) | `presence` | モデル実装済み、UI描画未接続 |
| コメント(レイヤー/フレームアンカー、スレッド、解決) | `comment/review` | モデル実装済み、UI未接続 |
| 提案(apply/reject/withdraw) | `comment/review` + `operation sync` の入口 | モデル実装済み |
| 操作同期(transform/property) | `operation sync` | 輸送枠組みのみ。スキーマ未定義・競合解決なし |
| ドキュメント同期(状態複製) | `document sync` | 未実装(現スコープ外が正) |
| アセット/blob同期 | `asset sync` | 未実装(現スコープ外が正) |

## 次の推奨順序(donor-matrixのdecision guidance準拠)

1. ~~WebSocket→Sessionアダプタ(signal接続のため設計レビュー必須)~~ — 設計レビュー待ち
2. **operationスキーマの最小定義** — 実装済み(2026-08-22): `CollabOperations.cppm`(module `Collaborate.Operations`)。`property.set`/`layer.transform`/`layer.add`/`layer.remove` のビルダー+バリデータ(未知typeは前方互換で通過)。Sessionに `createLocalOperation(CollabOperationData)` オーバーロード追加(identity/sequence/pending stampは常時session管理)。
3. **プレゼンスstale検出** — 実装済み(2026-08-22): `staleParticipantClientIds(nowMs, timeoutMs)`。heartbeatタイムアウトで参加者を検出、presence更新で即時解消。
4. 提案accept時の適用パイプライン(Undo統合設計を含む)
5. 認証層(サーバー側)
