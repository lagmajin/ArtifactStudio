# コラボレーション機能 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** ArtifactProject, ArtifactComposition, Network, VersionControl

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
