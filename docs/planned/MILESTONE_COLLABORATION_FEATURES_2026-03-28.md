# コラボレーション機能 Milestone

**作成日:** 2026-03-28  
**最終更新:** 2026-09-24
**ステータス:** In Progress — WebSocket／Session 接続、presence dock、選択 layer の手動ロック予約、layer property／structure/state／transform と macro の UndoManager lock guard、既知 layer operation のサーバー側 lock enforcement、heartbeat 更新付き lock lease timeout、room／参加者ごとの lock 数上限、Composition/layer/frame アンカー付き review note の追加／返信／編集／直近 50 版の編集履歴表示／削除／resolve／reopen、アンカーへの移動、サーバー側 operation 履歴の JSONL 保存・再起動後 replay、履歴容量上限、任意設定の全 room 共通 edit/view token と viewer の server-side mutation 拒否を実装。policy close による join 拒否後は自動再接続を止める。adapter は履歴・lock snapshot 完了を示す `room_ready` 前の durable operation と lock request/release を拒否する。`SetLayerPropertyValueCommand` の property value、単独 keyframe 列、単独 property expression を expected-value 条件付きで送受信し、Undo/Redo を補償 operation として送る最小経路を追加し、1 child の MacroUndoCommand は子 command を同期対象として dispatch する。全子が property value command の macro は `property.batch` で最大128件まで同期し、すべての対象 layer の lock を必要とする。server operation rejection は `clientId + opSeq` で特定し、保留中の push／undo／redo を期待値検証付きで補償する。ACK／拒否が届くまで追加 Undo command と history 保存・offload を止める。再接続では `room_ready` までに履歴 echo が届けば ACK とし、履歴にない保留 operation は未コミットと判定して補償する。共同セッション中、UndoManager は対応可否を push／undo／redo 前に照会し、未対応 command は適用前に拒否する。ただし UndoManager を通らない直接 mutation 経路は未監査・未遮断。layer.add／layer.remove の限定同期を実装。transform／その他 structure operation、remote operation の共有 Undo 履歴は未完了。
**関連コンポーネント:** ArtifactProject, ArtifactComposition, Network, VersionControl

2026-09-24 追記: 共通 Property Widget 行のプレビュー／確定値編集は、値変更前に実際の全対象 layer ID を UndoManager の layer mutation guard に渡す。対象行では未ロック時の直接 preview mutation も抑止し、preview 中の lock 失効または edit cancel では全対象の値／keyframe／animatable 状態を開始時の snapshot へ戻す。Inspector、Timeline、各専用 editor、ツールなど残りの直接 setter／keyframe 操作は未監査であり、この共通行 guard の適用範囲外。
2026-09-24 追記: Text/Puppet の Undo command は owner layer ID を guard に公開する。`TextContentUndoCommand` は `layer.text` expected-value operation で同期し、Puppet pin／deformation state／deformer keyframe command は未対応 operation のため fail-closed で拒否し、呼び出し元の rollback 経路へ戻す。直接 tool mutation の全面的な preflight と remote state 同期は引き続き未完了。
2026-09-24 追記: Composition Audio Mixer の routing snapshot command は layer lock scope を表せないため、共同セッションでは scope unresolved として fail-closed にする。Composition-wide mixer operation と権限モデルは未実装。
2026-09-24 追記: Inspector の component descriptor、clone effector stack、cloner transform stack の各 helper は Undo command 作成前に対象 layer lock を確認する。component descriptor snapshot は `layer.components` expected-value operation で push／undo／redo と remote apply に対応。clone effector stack と cloner transform stack は `layer.stack` expected-value operation で push／undo／redo と remote apply に対応する。

2026-09-24 追記: Viewport の Shape corner radius／star inner radius Undo command を `property.set` expected-value operation に接続し、対象 layer lock と remote property CAS を利用する。初回 redo のみ viewport が command push 前に適用した値を受理し、それ以降の Undo／Redo は現在値が expected と一致する場合だけ進む。描画結果と複数 client の runtime parity は未検証。

2026-09-24 追記: Viewport の Shape Polygon 頂点 Undo command は `layer.shapePath` geometry snapshot CAS を利用する。Polygon と Bézier を含む相互排他 geometry 全体、layer lock、remote exact readback を適用し、座標・件数・payload size を Core／server で検証する。複数 client の頂点編集、undo/redo、描画 parity は未検証。

2026-09-24 追記: Bézier path の共通 `ShapePathVertexEditCommand` を `layer.shapePath` snapshot operation に接続した。snapshot は Polygon／Bézier の相互排他 geometry 全体（各点列、tangents、smooth、両 closed state）を含み、Core／server で座標域と payload size を検証する。path 作成 Undo は以前の Polygon override も復元する。複数 client runtime parity は未検証。

2026-09-24 追記: Viewport の `ShapeOperatorValueUndoCommand` を `layer.shapeOperator` finite expected/value CAS に接続した。server／Core は operator index と field 長を検証し、layer lock を要求する。remote apply は operator の現在値を比較してから既存 setter を使い、readback が合わなければ元値へ戻す。複数 client の描画・Undo/Redo は未検証。

## 現行コード監査 (2026-09-24)

`CollaborationWebSocket` と `CollaborationSessionAdapter` は MainWindow の Collaboration dock から接続され、参加者一覧と選択／Composition／再生位置の presence が同期する。参加者一覧には選択 layer 名を最大 3 件表示し、composition／選択 ID が変わらない presence 更新では layer 名の再解決を避ける。Timeline の layer 行には共同編集 lock を示す表示がある。join 時は `savedAt` を除外した project JSON の SHA-256 fingerprint を server と照合し、異なる baseline を拒否する（snapshot 転送ではない）。既知 operation schema はサーバーとアダプター送受信境界で検証する。選択中の単一 layer に対する手動 lock request/release UI、join 時の lock 状態復元、server heartbeat で更新する期限付き lock lease と server 側 lock 数上限を実装した。初期 lock snapshot 完了前は追跡対象 Undo command を止め、property value/keyframe/expression、layer ID を保持する property／structure／state／animation／mask・matte／text・source／component／modulation／audio／variant 系 command と effect property/preset/keyframe/expression/modulation/mask command（owner layer を project item tree 上の全 Composition から解決し lock 判定、解決できない場合は拒否）、複数 layer 整列、Composition 解像度変更、visibility/lock/solo/shy/blend、macro、Transform Gizmo の single/multi transform は push/undo/redo 時に自分の lock 保持を要求する。サーバーも property.set／property.keyframes／layer.transform／layer.remove を送信者自身が lock を保持する場合に限って受理し、property.batch は全対象 layer の lock を要求する。まだ全編集 command へ対象 layer ID が伝播しておらず、他の mutation 経路は enforcement 対象外。拒否理由は Collaboration dock に表示する。Composition/layer/frame アンカー付き review note は operation で共有し、履歴から復元して一覧表示する。返信、本人コメントの編集・編集履歴表示、削除 tombstone、thread の resolve/reopen も同期する。選択 note から既存 layer と frame anchor へ移動できる。提案 UI は未実装。`CollaborationProtocol`、`LayerLockManager`、`EditSession` の基盤や `LayerLockIndicator` も存在する。

一方、通常の layer/property operation は Session 履歴に記録され server から配信される。`SetLayerPropertyValueCommand` と単独 `SetLayerPropertyKeyframesCommand`（各1 child wrapper を含む）は expected-value operation を送信し、property value／keyframe／expression command のみを含む macro は最大128件・payload 1 MiB までの `property.batch` として local push／undo／redo を送信し、受信側は property 値または keyframe 列の期待値が現在値と一致するときだけ適用する。サーバー echo は送信元で再適用しない。不一致・旧形式 operation は適用せず dock に競合を表示する。Undo/Redo の値設定も期待値を確認し、外部変更後の古い command による上書きを拒否する。server 拒否は `clientId + opSeq` で保留 command に結び付け、push／undo／redo を補償して history state を戻す。ACK／拒否が届くまでは追加 Undo command と history の保存・offload を止める。再接続後は `room_ready` が示す履歴 replay 完了を使い、履歴 echo がない保留 operation を未コミットとして補償する。共同セッション中、UndoManager は対応可否を push／undo／redo 前に照会し、未対応 command は適用前に拒否する。ただし直接 mutation 経路は未監査であり、この制限を迂回し得る。競合検出は行うが、自動 merge や競合解決 UI はない。この経路は他の property command、transform／その他 structure operation、remote operation の共有 Undo 履歴、project snapshot の転送／復元を扱わない。layer.add／layer.remove は bounded snapshot と参照検証付きの限定対応を追加した。提案 UI と承認後の適用 pipeline、全編集経路への lock 強制、ユーザー別 identity・権限・招待、管理者権限、TLS も未完了。共有 edit/view token はサーバー全体で有効な簡易 role gate であり、アカウント認証や room ごとの管理権限ではない。サーバー履歴には既定 16 MiB／100,000 operation の追記上限を実装済みだが、snapshot／圧縮や運用者向け整理機能はない。ACK 前のファイル `fsync`、受信 frame／presence／identity のサイズ、同時接続数にも上限を設けた。現時点の判定は、**接続・presence・review note 基盤と一部 property 同期を実装、共同編集機能全体は未完了** とする。

---

## 概要

複数人での同時編集を可能にするコラボレーション機能を実装する。

---

## 機能要件

### ★★★ 必須機能

#### 1. プロジェクト共有

- 複数ユーザーでのプロジェクト共有
- 権限管理（閲覧/編集/管理）
- 招待リンクの生成

**工数:** 8-12 時間

#### 2. 競合検出

- 同時編集の検出
- ロック機構
- マージ機能

**工数:** 12-16 時間

#### 3. 変更履歴

- バージョン管理
- 差分表示
- 巻き戻し機能

**工数:** 10-14 時間

### ★★ 重要機能

#### 4. リアルタイム同期

- 変更の即時反映
- 操作のブロードキャスト
- 競合の自動解決

**工数:** 16-20 時間

#### 5. コメント/レビュー

- レイヤーへのコメント
- タイムライン上のマーカー
- レビューモード

**工数:** 8-10 時間

### ★ 推奨機能

#### 6. プレゼンス表示

- 編集中ユーザーの表示
- カーソル位置の共有
- 編集中レイヤーのハイライト

**工数:** 6-8 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **プロジェクト共有** | 8-12h | 🔴 高 |
| **競合検出** | 12-16h | 🔴 高 |
| **変更履歴** | 10-14h | 🔴 高 |

### P1（重要）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **リアルタイム同期** | 16-20h | 🟡 中 |
| **コメント/レビュー** | 8-10h | 🟡 中 |

### P2（推奨）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **プレゼンス表示** | 6-8h | 🟢 低 |

**合計工数:** 60-80 時間

---

## Phase 構成

### Phase 1: プロジェクト共有基盤

- 目的:
  - 複数ユーザーでの共有を可能に

- 作業項目:
  - プロジェクトオーナー権限
  - 招待リンク生成
  - 権限管理（Read/Write/Admin）

- 完了条件:
  - 複数ユーザーがプロジェクトにアクセス可能
  - 権限に基づく操作制限

### Phase 2: 競合検出とロック

- 目的:
  - 同時編集の競合を防止

- 作業項目:
  - レイヤー単位のロック
  - 編集中ユーザーの表示
  - ロック解除のタイムアウト

- 完了条件:
  - 編集中レイヤーは他ユーザーが編集不可
  - ロック状態が可視化

- 実装案:
  ```cpp
  class LayerLockManager {
      QMap<LayerID, LockInfo> locks_;
      
      struct LockInfo {
          UserID userId;
          qint64 timestamp;
          QString userName;
      };
      
      bool acquireLock(const LayerID& layerId, const UserID& userId) {
          if (locks_.contains(layerId)) {
              // タイムアウトチェック
              if (QDateTime::currentMSecsSinceEpoch() - locks_[layerId].timestamp > 30000) {
                  locks_.remove(layerId);  // タイムアウトで解除
              } else {
                  return false;  // 既にロック中
              }
          }
          
          locks_[layerId] = {userId, QDateTime::currentMSecsSinceEpoch(), getUserName(userId)};
          return true;
      }
  };
  ```

### Phase 3: 変更履歴管理

- 目的:
  - 全ての編集を追跡

- 作業項目:
  - コミットベースの履歴
  - 差分の保存
  - 巻き戻し機能

- 完了条件:
  - 過去の状態へ復元可能
  - 差分を視覚化

### Phase 4: リアルタイム同期

- 目的:
  - 変更を即時反映

- 作業項目:
  - WebSocket 接続
  - 操作のブロードキャスト
  - 競合の自動解決（OT/CRDT）

- 完了条件:
  - 他ユーザーの操作が数秒で反映
  - 競合が自動解決

- 実装案:
  ```cpp
  class CollaborationService {
      QWebSocket* socket_;
      
      void sendOperation(const Operation& op) {
          QJsonObject json;
          json["type"] = "operation";
          json["userId"] = currentUserId_;
          json["operation"] = op.toJson();
          
          socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(json).toJson()));
      }
      
      void onOperationReceived(const Operation& op) {
          // 競合チェック
          if (canApply(op)) {
              apply(op);
          } else {
              // 競合解決ロジック
              resolveConflict(op);
          }
      }
  };
  ```

### Phase 5: コメント/レビュー

- 目的:
  - フィードバック機能

- 作業項目:
  - レイヤーへのコメント
  - タイムラインマーカー
  - レビューモード

- 完了条件:
  - コメントの追加/編集/削除
  - マーカー位置へのジャンプ

### Phase 6: プレゼンス表示

- 目的:
  - 編集中ユーザーの可視化

- 作業項目:
  - ユーザーカーソルの共有
  - 編集中レイヤーのハイライト
  - アクティビティログ

- 完了条件:
  - 他ユーザーの位置がわかる
  - 編集中が一目でわかる

---

## 技術的課題

### 1. 競合解決アルゴリズム

**課題:**
- 同時編集の競合をどう解決するか

**解決案:**
- **Operational Transformation (OT)** - Google Docs 方式
- **CRDT (Conflict-free Replicated Data Type)** - 分散システム向け
- **Last-Writer-Wins** - 単純だがデータ損失のリスク

### 2. ネットワーク遅延

**課題:**
- 遅延のある環境での同期

**解決案:**
- 楽観的更新（先に表示）
- リトライ機構
- オフライン編集のキューイング

### 3. データ整合性

**課題:**
- 部分適用による不整合

**解決案:**
- トランザクション管理
- 原子操作的な適用
- 整合性チェックの定期実行

---

## 期待される効果

### コラボレーション

| 指標 | 現在 | 改善後 |
|------|------|--------|
| **同時編集者数** | 1 人 | 無制限 |
| **フィードバック時間** | 数時間 | 数分 |
| **マージ作業** | 手動 | 自動 |

### ユーザー体験

- リアルタイムでの共同作業
- 編集中の競合を心配しない
- 簡単にフィードバック

---

## 関連ドキュメント

- `docs/planned/MILESTONE_DATA_PERSISTENCE_2026-03-28.md` - データ永続化
- `docs/planned/MILESTONES_BACKLOG.md` - 全体バックログ

---

## 実装順序の推奨

1. **Phase 1: プロジェクト共有** - 基盤機能
2. **Phase 2: 競合検出** - 必須機能
3. **Phase 3: 変更履歴** - 信頼性向上
4. **Phase 4: リアルタイム同期** - コア機能
5. **Phase 5: コメント/レビュー** - UX 向上
6. **Phase 6: プレゼンス** - 可視化

---

**文書終了**

2026-09-24 追記: Clone effector stack と Cloner transform stack の snapshot を `layer.stack` expected-value operation として同期し、push／undo／redo と remote apply を扱う。両 stack は layer lock と CAS が必要で、snapshot は各 256 KiB に制限する。未対応としていた Inspector stack 編集の同期穴を閉じた。

2026-09-24 追記: `property.batch` は value と keyframe 変更を混在可能にし、最大128件を一つの expected-value preflight と適用単位として扱う。keyframe entry は既存 `property.keyframes` と同じ bounded encoding／animatable flag 契約を、expression entry は expected／next string CAS を使用し、server と ArtifactCore validator は payload 全体を 1 MiB 以下に制限する。remote apply は全 entry の期待値を先に照合し、適用失敗時は先行変更を逆順に戻す。

2026-09-24 追記: `TextContentUndoCommand` に Undo persistence と独立した collaboration encoder を追加し、`layer.text` expected/value CAS を push／undo／redo と remote apply に接続した。文字列は各 128 KiB、対象 layer lock 必須。Puppet tool が保持する pin／deformation snapshot の remote apply は引き続き未対応。

2026-09-24 追記: `RenameLayerCommand` は専用 collaboration encoder で `layer.rename` expected/value CAS を push／undo／redo から送信する。remote apply は現在の layer name を期待値と照合し、名前は各 UTF-8 4 KiB、対象 layer lock 必須。

2026-09-24 追記: `ChangeActiveVariantCommand` を `layer.variant` integer expected/value CAS で同期する。operation は対象 layer lock を必要とし、index は 0〜100000 の有限整数。remote apply は現在 index と expected を照合し、存在しない variant への変更を反映失敗として拒否する。

2026-09-24 追記: `ChangeLayerBlendModeCommand` を `layer.blendMode` integer CAS で同期する。remote apply は現在 mode と expected を比較してから `setBlendMode` し、`Layer.Blend` の 0〜33 範囲に限定する。対象 layer lock を必須とする。

2026-09-24 追記: `SetLayerVisibilityCommand` を `layer.visibility` boolean CAS で同期し、対象 layer lock と現在値照合後の remote apply を追加した。Solo／Shy 状態はこの operation に含めない。

2026-09-24 追記: layer solo／shy の Undo command を `layer.flag` CAS operation で同期する。payload は flag 名と expected/value bool、server は対象 layer lock を必須にし、remote client は現在値を照合する。

2026-09-24 追記: project layer の `isLocked` を `SetLayerLockCommand`／`layer.editLock` bool CAS で同期する。これは協調編集 server reservation と別状態で、remote apply は期待値照合後に project layer lock を更新する。対象 layer の協調 lock reservation は operation 送信時に引き続き必要。

2026-09-24 追記: `AddLayerCommand` の単体追加／undo／redo を `layer.add`／`layer.remove` operation に接続した。追加 payload は composition ID、layer ID と一致する layer JSON、基準 index と左右 anchor ID を含み 768 KiB 以下に制限する。source／file／sequence／texture など既知の外部パス field を含む source layer は asset 配布未対応のため拒否する。symbolic な propertyPath 識別子は拒否対象に含めない。property／batch／keyframe／expression／component／stack validator も source path 系 property と既知の外部パス field を拒否する。symbolic な propertyPath 識別子は対象外とする。同じ anchor 間の concurrent add は layer ID 順に挿入してクライアント間の順序を揃える。remote apply は重複 ID、composition、index、parent／matte／clone source 参照を検証し、Composition の正式な append／reorder 経路へ適用する。削除は expected layer snapshot の一致、dependent parent／matte 参照が無いこと、対象 layer reservation を条件にする。現在 index は他の同期追加による並び替えを許容するため CAS 条件にしない。依存 layer を変更する追加 command と参照先になっている削除は fail-closed。実機同期、asset path portability、remote operation の共有 Undo 履歴は未検証／未対応。

2026-09-24 追記: `MoveLayerIndexCommand` を `layer.reorder` に接続した。operation は composition ID、対象 layer の expected index と移動先 index を持ち、push／undo／redo を CAS で適用する。対象 layer reservation を server で要求し、remote apply は composition と現在 index を照合してから reorder する。移動先が remote の現在 layer 数に対して不正、または expected index が一致しない場合は拒否する。実機での複数 client 同時 reorder と composition の downstream order-dependent behavior は未検証。

2026-09-24 追記: `ChangeLayerOpacityCommand` を `layer.opacity` expected/value CAS で同期する。値は finite float の 0〜1 に制限し、対象 layer reservation を要求する。local undo／redo と remote apply は expected opacity を照合し、setter 後の値も確認する。共通 Property Widget row の preview／commit は全対象 layer ID の guard を通り、preview 中断時は各 layer の開始 opacity に戻す。実機の lock 失効動作は未検証。

2026-09-24 追記: `ChangeLayerParentCommand` を `layer.parent` expected/next parent ID CAS で同期する。対象 child と非nil の旧／新 parent layer reservation を必須とし、remote apply は canonical UUID、現在の親 ID、同一 Composition 内の parent 存在、self-parent と循環を既存 `setParentById` の検証で確認する。undo／redo も期待親 ID が現在値と一致する場合だけ動作する。複数 client で parent 変更と layer remove が交錯する場合は runtime 未検証。

2026-09-24 追記: `SetTextAnimatorStackCommand` を `layer.stack` の `textAnimators` kind で同期する。snapshot は各 256 KiB 以下、対象 layer lock と expected snapshot 一致を要求し、push／undo／redo と remote apply に対応する。復元失敗時は開始 snapshot に戻す。実機での text animator 編集・評価同期は未検証。

2026-09-24 追記: `AnimationLayerStackSnapshotCommand` を `layer.animationStack` expected/value object CAS で同期する。snapshot は各 256 KiB 以下で外部 path field を拒否し、対象 layer lock を要求する。caller が先に適用する既存編集経路のため初回 redo は after snapshot の再適用を成功扱いし、以降の undo／redo と remote apply は現在 snapshot を照合する。runtime の animation layer playback／複数 client 一致は未検証。

2026-09-24 追記: SetAudioDeClickRangesCommand を layer.audioDeClickRanges expected/value CAS で同期する。sample index は JSON number の精度喪失を避ける decimal string とし、配列ごとに 8192 ranges／256 KiB、昇順・非接触の正規化範囲を要求する。undo／redo と remote apply は現在値を照合する。runtime は未検証。

2026-09-24 追記: `Deformation2DStateUndoCommand` を path-free bounded `layer.deformation2D` snapshot CAS で同期する。local redo は既存 caller が push 前に適用する状態を idempotent に受理し、undo／redo は期待 snapshot と比較する。remote apply は PuppetTool の restore API を通じて pin cache を再構築する。複数 client の viewport／deformation runtime は未検証。

2026-09-24 追記: Puppet pin position keyframe drag は full deformation snapshot を取得できたとき `layer.deformation2D` operation にまとめる。keyframe property と保存済み pin state を同一 Undo／CAS 境界に置き、snapshot がない fallback 経路は未対応として送信前に拒否する。

2026-09-24 追記: Solid Gradient の viewport drag を既存 `property.batch` に接続する。center X/Y と angle の3 property を単一 operation にまとめ、各値の expected CAS を local Undo／Redo と server／remote apply に適用する。初回 push は caller がすでに after 値を適用している状態を認識する。runtime 未検証。

2026-09-24 追記: `SolidSizeUndoCommand` を `layer.solidSize` expected/value CAS に接続した。Solid2D／SolidImage の幅・高さを 1〜16384 の整数に制限し、server は対象 layer lock を要求する。local Undo／Redo と remote apply は寸法を事前照合し、setter 後に検証する。viewport 初回 push は after dimensions がすでに適用済みの状態を扱う。runtime 未検証。

2026-09-24 追記: Solid resize／gradient の初回 push に限り caller がすでに書いた after state を受理し、それ以降の Undo／Redo は expected state が一致するときだけ適用する。stale history を既に目標値だからという理由で成功扱いしない。

2026-09-24 追記: Source Crop の exact snapshot restore API を追加し、`layer.sourceCrop` expected/value CAS で viewport crop 編集を同期する。snapshot は cropRect／pan／zoom／rotation／anchor／enabled／preserveAspect 全体を保持するため、空矩形も失わない。local Undo／Redo は初回 pre-applied redo のみを許し、remote apply は対象 layer lock と expected snapshot を照合する。runtime 未検証。

2026-09-24 追記: SVG import の Undo command を layer.shapeContents expected/value CAS に接続した。snapshot は shape contents、ordered stack nodes、active content index をまとめ、各 256 KiB／256 contents／1024 stack nodes に制限し、外部 asset path field を拒否する。server は対象 layer lock を要求し、remote apply は snapshot 全体を照合してから restore/readback する。複数 client の SVG import・Undo/Redo parity は未検証。

2026-09-24 追記: `MoveLayerCommand` を `layer.moveAtFrame` expected/value operation に接続した。frame・time scale・position X/Y を同期し、finite/bounded 値と対象 layer lock を server/Core が検証する。local Undo/Redo と remote apply は frame 上の現在値 CAS を行い、readback 後に通知する。複数 client の keyed transform parity は未検証。

2026-09-24 追記: Transform Gizmo の単体／group Undo を `property.batch` に接続した。対象 frame の keyframe だけを更新する expected/next full keyframe snapshots で、他の frame の key と key metadata を保持する。property animatable capability は key の有無と別に CAS する。上限を超える group は送信前に拒否する。実機・複数 client parity は未検証。
