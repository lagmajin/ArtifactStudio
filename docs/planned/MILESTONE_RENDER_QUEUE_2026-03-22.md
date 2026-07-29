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

## Static Audit (2026-07-25)

現行ソースでは、`ArtifactRenderQueueService` と `RenderJobModel` / `RenderQueue` に queue job、status、progress、error、並び替え・削除・複製・名前変更・範囲／出力設定変更、start/pause/cancel、rerun の入口があり、`WorkspaceAutomation` からも同じサービス経路を利用できる。preset selector、job panel、queue manager の UI も存在する。Render Farm の checkpoint/retry/progress 基盤は後続層として追加されている。

ただし、目標全体の完了とは判定しない。再起動後の履歴・ログ永続化、in/out/work area の全投入経路での正確な反映、queue 状態と実 renderer の厳密な同期、失敗原因／失敗 frame の UI 表示、checkpoint/resume の実成果物整合、分散失敗時の再送は static evidence だけでは保証できない。`WorkspaceAutomation` の API 入口は、実 UI と長時間 queue の runtime 受け入れ証拠ではない。

判定: queue の基本操作と UI/API surface は実装済み、長時間運用・復旧・永続化・分散の acceptance は未検証または後続 milestone に分割済み。

2026-07-30 implementation loop: summary ETA の進捗開始時刻を queue 同期で失わないよう保持し、queue reorder 時のみ index 対応をリセットする処理を追加。runtime の時間精度・長時間運用は未検証。
途中 job 削除時も index の詰まりによる誤流用を避けるため timing map をリセットし、次の progress event から再計測する。
progress rollback（再試行／再開）時も該当 job の ETA 開始時刻をリセットする。
