# Timeline keyframe diagnostics

**最終更新:** 2026-09-21

## 有効化

`Artifact.exe` と同じフォルダに `ArtifactKeyframeDebug.json` を作成し、変更をビルドしたアプリを起動する。

```json
{
  "enabled": true,
  "logFile": "ArtifactKeyframeTrace.jsonl",
  "maxBytes": 8388608,
  "maxKeysPerRecord": 32
}
```

`logFile` が相対パスなら exe と同じフォルダに出力する。絶対パスも指定できる。
Position X に限らず、タイムラインからキーを編集した任意のプロパティを記録する。
既存キーは編集または Undo/Redo した時に記録される。単なる再生では記録しない。
無効化する場合は `enabled` を `false` にするか、設定ファイルを削除する。

## 記録内容と制限

`SetLayerPropertyKeyframesCommand` の適用成功時に JSONL を追記する。
layerId、property、compositionFps、animatable、キー総数、キー時刻の分子／分母、
秒数、値、補間方式、キー時刻と区間中央での評価値を記録する。
既定では1操作あたり先頭32キーまで、ファイル8MiB到達後は追記を停止する。
`maxKeysPerRecord` は1〜256、`maxBytes` は64KiB〜64MiBの範囲で設定できる。
再生中の描画結果やフレーム提示ログではない。評価値が正常でも、キャッシュや
レンダリング経路の不具合を否定するものではない。

## 関連修正と確認待ち

- 再生タイマーからGPUタイムラインへの動的表示更新要求を追加。
- カーブ切替イベントをUIスレッドでは即時反映。
- Project Viewは幅760px未満で詳細欄を隠し、640px未満で検索とフィルターを縦配置。
- 静的確認のみ。ビルド・実機再現・キー評価と表示位置の照合は未実施。
