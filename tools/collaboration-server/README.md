# ArtifactStudio Collaboration Server

リアルタイム共同編集のためのWebSocketサーバー（プロトタイプ）。

## 起動方法

```bash
cd tools/collaboration-server
npm install
npm start
```

デフォルトで `ws://localhost:8080` でリッスンします。

ポートを変更する場合は環境変数 `PORT` を設定してください：

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

### サーバー → クライアント

| タイプ | 内容 |
|--------|------|
| `history` | 操作履歴（新規参加時） |
| `operation` | リモート操作通知 |
| `lock_granted` | ロック許可 |
| `lock_denied` | ロック拒否 |
| `lock_updated` | ロック状態更新 |
| `lock_released` | ロック解放 |
| `user_joined` | ユーザー参加通知 |
| `user_left` | ユーザー離脱通知 |
| `presence` | プレゼンス更新通知 |
