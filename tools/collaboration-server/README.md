# ArtifactStudio Collaboration Server

**最終更新:** 2026-09-24

リアルタイム共同編集のためのWebSocketサーバー（プロトタイプ）。

## 起動方法

```bash
cd tools/collaboration-server
npm install
npm start
```

デフォルトで `ws://localhost:8080` でリッスンします。
既定の bind address は `127.0.0.1` です。LAN から接続する場合は `HOST=0.0.0.0` を明示し、
ファイアウォールで接続元を制限してください。インターネット公開には認証と TLS のある
reverse proxy を用意してください。このプロトタイプ自体には TLS とユーザー別 identity 認証はありません。

共有 join token は `COLLAB_ACCESS_TOKEN`（編集可）と `COLLAB_VIEW_TOKEN`（閲覧のみ）で
設定できます。どちらか一方でも設定した場合、join 時にどちらかの token が必要です。
閲覧 token の client は presence と履歴を受信できますが、operation と lock request/release
は server が拒否します。両方が空なら token 認証を無効にし、project ID を知る client を
編集可として許可します。token は全 room 共通です。これはユーザー別の identity／招待／管理
権限ではありません。2つの token に同じ値は設定できません。
クライアント UI は token を保存せず、join 時にだけ送信します。ネットワーク上で token を
保護するには TLS 終端の reverse proxy を使ってください。access token／join policy による
拒否は WebSocket policy close となり、クライアントは自動再接続を止めます。設定を直してから
手動で再接続してください。

operation 履歴はプロジェクトごとの JSONL ファイルに保存されます。既定の保存先は
`tools/collaboration-server/data/` です。保存先は `COLLAB_DATA_DIR` で変更できます。
join 時に compact project JSON（`savedAt` を除外）の SHA-256 fingerprint を送信し、room ごとの
`.baseline.json` に初期値を保存します。別 fingerprint の project copy は join を拒否します。
これは project snapshot の転送・復元ではなく、同じ baseline を開いているかの照合です。
baseline metadata がない既存 room は、review comment operation しか履歴にない場合に限り、初回 join の fingerprint を記録します。
project mutation または未知 operation を含む旧履歴は baseline 不明として拒否されます。
この fingerprint を含む join を送らない旧クライアントは接続できません。
この履歴には共有レビューコメントも含まれます。プロトタイプには認証・暗号化・保持期限が
ないため、信頼できるネットワークと適切なファイルアクセス制限のある環境でのみ使用してください。
履歴の無制限な増加を避けるため、プロジェクトごとに既定で 16 MiB または 100,000 operation
の早い方を上限とします。`COLLAB_MAX_HISTORY_BYTES` と `COLLAB_MAX_HISTORY_OPERATIONS` に
正の整数を設定すると上限を変更できます。上限到達後の新規 operation は保存・配信せず、明示的な
エラーを返します。古い履歴を自動削除しないため、既存履歴が設定上限を超えている場合も読み込みは
維持しますが、新規書き込みは拒否されます。容量を回復するには、停止中にバックアップを取得した上で
管理者が保持方針に沿って履歴を整理する必要があります（自動圧縮・スナップショットは未実装）。
各 operation は client ID と `opSeq` の組み合わせで重複登録を防ぎます。同一内容の再送には
元の server version を返し、同じ sequence に異なる内容が届いた場合は拒否します。
operation の拒否 error には、受信済み operation の `clientId` と安全な整数 `opSeq` を含めます。
クライアントはこの組み合わせで未確定の Undo command を照合し、拒否時にローカル状態を補償します。
既知 operation (`property.set`、`property.batch`、`property.keyframes`、`property.expression`、`layer.components`、`layer.stack`、`layer.animationStack`、`layer.audioDeClickRanges`、`layer.deformation2D`、`layer.solidSize`、`layer.sourceCrop`、`layer.shapePolygon`、`layer.shapePath`、`layer.shapeOperator`、`layer.shapeContents`、`layer.moveAtFrame`、`layer.text`、`layer.rename`、`layer.variant`、`layer.blendMode`、`layer.opacity`、`layer.parent`、`layer.visibility`、`layer.flag`、`layer.editLock`、`layer.transform`、`layer.add`、`layer.remove`、`review.comment.*`) は
サーバー側でも payload の型・必須値・有限数値・review actor identity を検証してから保存します。
未知 type は将来バージョンとの forward compatibility のため通過させますが、クライアントが適用できる保証はありません。
クライアント adapter は `room_ready`（履歴と初期 lock snapshot の処理完了）より前の durable operation と lock request/release を拒否します。接続済み状態だけでは操作・lock を送信できません。
`property.set`、`layer.components`、`layer.stack`、`layer.audioDeClickRanges`、`layer.deformation2D`、`layer.solidSize`、`layer.sourceCrop`、`layer.shapePolygon`、`layer.shapePath`、`layer.shapeOperator`、`layer.shapeContents`、`layer.moveAtFrame`、`layer.text`、`layer.rename`、`layer.variant`、`layer.blendMode`、`layer.visibility`、`layer.flag`、`layer.editLock`、`layer.transform`、`layer.add`（anchor layer）、`layer.remove`、`layer.reorder` は、送信者自身が対象 layer の lock を保持していない場合にサーバー側で拒否します。`property.batch` は payload に含まれる全 layer の lock を送信者が保持している場合だけ受理します。`property.keyframes` と `property.expression` は対象 layer の lock を要求し、期待する keyframe 列または expression が現在値と一致する場合だけ client が適用します。`layer.components` は expected/value の component descriptor snapshot を各 256 KiB まで受け取り、現在 snapshot と期待値が一致する場合のみ client が復元します。`layer.audioDeClickRanges` は正規化済みの decimal-string sample range を expected/value CAS で受け取り、各配列 8192 件・256 KiB までに制限します。`layer.stack` は `clonerTransforms`、`cloneEffectors`、または `textAnimators` の配列 snapshot を同じ上限で受け取り、期待値一致後に復元します。`layer.animationStack` は animation layer object snapshot の expected/value CAS です。`layer.deformation2D` は path field を含まない expected/value snapshot を各 256 KiB まで受け取り、client は PuppetTool の復元 API を通して pin cache を再構築します。 `layer.solidSize` は Solid2D／SolidImage の幅・高さを 1〜16384 の整数 expected/value CAS で同期します。`layer.sourceCrop` は crop 状態全体を canonical expected/value snapshot として同期し、空矩形もそのまま保持します。`layer.shapePolygon` は `points` と `closed` の expected/value snapshot を各 256 KiB までに制限し、有限座標 ±1,000,000 を検証します。client は現在 snapshot 一致後に復元し、readback 不一致時は元状態へ戻します。`layer.shapePath` は相互排他の Polygon／Bézier geometry 全体を expected/value CAS で同期します。Polygon 点、Bézier 位置・接線・smooth flag、両方の closed state を含め、各 snapshot を 256 KiB に制限します。座標の有限性と ±1,000,000 範囲を検証し、client は完全 readback を確認します。`layer.shapeOperator` は operator index・field・有限な expected/value を送り、対象 layer lock と operator 値の CAS を要求します。`layer.shapeContents` は Shape content／ordered stack nodes／active content index を含む全体 snapshot を各 256 KiB に制限し、対象 layer lock と expected snapshot CAS を要求し、snapshot 内の外部 asset path field は拒否します。`layer.moveAtFrame` は frame/time scale と有限な expected/value の position X/Y を送り、bounded frame/time scale と layer lock を要求します。`layer.text` は expected/value の文字列 CAS を各 128 KiB まで受け取り、対象 layer lock を要求します。`layer.rename` は名前の expected/value CAS を各 4 KiB まで受け取り、同じ layer lock を要求します。`layer.variant` は 0〜100000 の整数 index の expected/value CAS です。`layer.blendMode` は `Layer.Blend` enum の 0〜33 を使う expected/value CAS です。`layer.opacity` は有限な 0〜1 の expected/value CAS、`layer.parent` は canonical UUID の expected/next parent CAS で、対象 child と旧／新 parent layer の lock を要求します。`layer.opacity` は有限な 0〜1 の expected/value CAS です。`layer.visibility` は boolean expected/value CAS です。`layer.flag` は `solo`／`shy` のいずれかと boolean expected/value CAS を要求します。`layer.editLock` は project の layer locked 状態を同期します。これは共同編集 server reservation とは別の状態です。各 batch は 1〜128 件・payload 1 MiB 以下で、value、keyframe、expression 変更を含められます。同じ layer/property の重複は拒否します。
`layer.add` と `layer.remove` は compositionId、index、左右 anchor ID、layer snapshot を持ち、snapshot と operation の layer ID が一致する必要があります。asset 配布がないため、非空の source／file／sequence／texture など明示した外部パス field は拒否します。symbolic な `propertyPath` 識別子は対象外です。`property.set`／`property.batch`／keyframe／expression は source path 系 property path と nested path field を、component／stack snapshot は nested path field を拒否し、変更値にローカル source path を保存しません。JSON payload は 768 KiB 以下に制限します。追加は重複 ID、index、composition 内の parent／matte／clone source の存在を client が検査します。削除は対象 layer の expected snapshot が一致し、他 layer からの parent／matte 参照がない場合だけ適用します。削除には対象 layer の server lock が必要です。追加は新規 layer ID ではなく anchorLayerId の server lock を要求します。空 composition からの追加は anchor が無いため lock 不要ですが、同一 anchor の同時追加は layer ID 順に整列します。

コメントなど layer anchor を持つ review operation はこの判定対象に含めません。lock enforcement はこれらの operation type と UndoManager が対象 ID を公開する command に限られます。
layer lock は既定 90 秒の lease です。サーバーの WebSocket ping に応答した owner の lock を更新し、応答が止まると期限切れ後に解放を broadcast します。
失効 sweep は 15 秒間隔です。`COLLAB_LOCK_LEASE_MS` で変更でき、heartbeat と余裕を持たせるため 60,000 ms 未満は拒否します。
lock 数は room ごとに既定 10,000、参加者ごとに 256 までです。`COLLAB_MAX_LOCKS_PER_ROOM` と
`COLLAB_MAX_LOCKS_PER_CLIENT` で正の整数へ変更できます。上限時は lock request を拒否します。
ArtifactStudio の UndoManager は layer identity を公開する代表 command の push／undo／redo を同じ lock 台帳で検査します。全 command が対象ではなく、完全な権限境界ではありません。
operation は JSONL 追記後にファイルを `fsync` してから ACK／配信します。これは OS のファイル同期境界までを保証するもので、
ディレクトリエントリの同期やストレージ装置自体の電源断耐性までは保証しません。書込み失敗時に末尾を元のサイズへ戻せなかった room は、
再起動・履歴確認まで新規 operation を拒否します。
起動時には壊れた最終レコードだけを回復対象にします。履歴の途中に破損を検出した場合はファイルを書き換えず、その room への参加を拒否してログに位置を記録します。

受信フレームは 2 MiB、operation は 1 MiB、presence は 64 KiB までです。join の project/client/user ID は各 256 bytes、
表示名は 128 bytes、lock の layer ID は 256 bytes に制限します。超過した operation／presence／lock request は明示エラーで拒否し、
join の不正な identity は close code 1008 で切断します。これらはリソース上限であり、client ID／user ID の真正性や権限を証明するものではありません。
同時接続はサーバー全体で既定 256、1 project あたり 64 までです。`COLLAB_MAX_TOTAL_CLIENTS` と
`COLLAB_MAX_ROOM_CLIENTS` に正の整数を設定して調整できます。上限を超える join は履歴を読み込む前に拒否され、close code 1013 を返します。

ポートを変更する場合は、1〜65535 の整数で環境変数 `PORT` を設定してください：

```bash
PORT=9000 npm start
```

## ヘルスチェック

```bash
curl http://localhost:8080/health
```

## メッセージプロトコル

### クライアント → サーバー

| タイプ | 内容 |
|--------|------|
| `join` | プロジェクト参加 |
| `operation` | 操作ブロードキャスト |
| `lock_request` | レイヤーロック要求 |
| `unlock_request` | レイヤーロック解放 |
| `presence` | プレゼンス更新 |
| `ping` | アプリケーション heartbeat |

### サーバー → クライアント

| タイプ | 内容 |
|--------|------|
| `history` | 操作履歴（新規参加時） |
| `room_ready` | 履歴、参加者一覧、初期ロック一覧の送信完了 |
| `operation` | リモート操作通知 |
| `lock_granted` | ロック許可 |
| `lock_denied` | ロック拒否 |
| `lock_updated` | ロック状態更新 |
| `lock_released` | ロック解放 |
| `user_joined` | ユーザー参加通知 |
| `user_left` | ユーザー離脱通知 |
| `presence` | プレゼンス更新通知 |
| `pong` | `ping` への応答 |
| `error` | 操作／接続の拒否理由。operation を識別できる場合は `clientId` と `opSeq` を含む |

presence と lock は一時状態として扱い、operation 履歴には保存しません。サーバー再起動後は
operation history は復元されますが、参加者、presence、lock は再参加後に再構築されます。
サーバーは30秒間隔の WebSocket ping/pong で半開き接続を検出し、応答しない接続を終了します。
切断処理により、その参加者の presence と lock は room から除去されます。
