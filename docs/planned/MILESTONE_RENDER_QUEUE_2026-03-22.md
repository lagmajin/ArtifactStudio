# MILESTONE_RENDER_QUEUE_2026-03-22

レンダーキューを、単発出力の補助機能ではなく、長時間運用と障害復旧を前提にした制作基盤へ寄せるための整理メモ。

## 目標

- バックグラウンドレンダーを実運用できる状態にする
- レンダー失敗時の原因と失敗フレームを追跡しやすくする
- in/out と work area をキュー投入時に正しく反映する
- 履歴とログを再起動後も参照できるようにする
- 将来的な分散レンダリングと checkpoint / resume を見据えた構造にする

## 取り組み対象

- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- `Artifact/docs/MILESTONE_RENDER_MANAGER_2026-03-17.md`

## 現在の優先項目

1. 進捗表示と残り時間推定の改善
2. キュー並び替えの操作性改善
3. エラー文言の原因可視化
4. in/out と work area の反映
5. 履歴とログの永続化
6. checkpoint / resume
7. 分散レンダリングの下地

## 補足

- 分散レンダリングは、単純な UI 追加ではなく、ジョブ分配・結果回収・失敗再送の設計が必要になる。
- 自動アクションは、シャットダウン以外に「フォルダを開く」「完了通知」「次のジョブを起動」などが候補になる。
