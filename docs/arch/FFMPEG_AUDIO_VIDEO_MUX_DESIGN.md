# FFmpeg 音声動画同時出力 設計書

作成日: 2026-04-18
推定工数: 4時間
影響範囲: `ArtifactRenderQueueService.cppm`

---

## 概要

既存のFFmpegパイプ出力に音声ストリームを追加し、音声動画が同時に出力されるようにする。
既存のコードへの変更は最小限に抑え、後方互換性を完全に維持する。

---

## ✅ 既に存在するもの

| 機能 | 状況 |
|------|------|
| ✅ オーディオミキサー | 100% 完成 |
| ✅ オーディオレンダラー | 100% 完成 |
| ✅ フレーム単位オーディオ合成 | 100% 完成 |
| ✅ FFmpeg 動画出力 | 100% 完成 |
| ❌ FFmpeg への音声送信 | 未実装 |
| ❌ 音声動画タイミング同期 | 未実装 |

---

## 🎯 実装計画

### 1. FFmpeg コマンドライン修正

#### 修正前
```
ffmpeg -y
  -f rawvideo -pixel_format rgba -video_size WxH -framerate FPS -i -
  -c:v encoder
  output.mp4
```

#### 修正後
```
ffmpeg -y
  -f rawvideo -pixel_format rgba -video_size WxH -framerate FPS -i -
  -f f32le -ar 48000 -ac 2 -i -
  -c:v encoder
  -c:a aac -b:a 256k
  -map 0:v -map 1:a
  output.mp4
```

### 2. デュアルパイプ書き込み

```cpp
// 動画: stdin パイプ (既存)
// 音声: 名前付きパイプ または 追加のファイルディスクリプタ

// Windows では追加のパイプを渡す
QProcess process;
process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
process->start(ffmpegPath, args);

// 動画は標準入力へ書き込み (既存コードそのまま)
process->write(videoFrameData);

// 音声は別スレッドで継続的に書き込み
std::thread audioWriter([&]() {
    while (rendering) {
        std::vector<float> audioBuffer = mixAudioForNextInterval();
        process->write(reinterpret_cast<char*>(audioBuffer.data()), 
                       audioBuffer.size() * sizeof(float));
    }
});
```

---

## 📋 実装ステップ

| ステップ | 内容 | 所要時間 |
|---------|------|----------|
| 1 | FFmpeg引数に音声入力ストリームを追加 | 15分 |
| 2 | AudioMixer から 48kHz float ステレオ でサンプルを取得するインターフェース追加 | 30分 |
| 3 | 音声書き込み専用ワーカースレッド追加 | 30分 |
| 4 | 動画フレームと音声のタイミング同期ロジック | 1時間 |
| 5 | 音声が存在しない場合のフォールバック処理 | 30分 |
| 6 | エラーハンドリングとクリーンアップ | 30分 |

**合計: 約4時間**

---

## ✅ 後方互換性

- 既存の動画のみの出力は全く影響を受けない
- オーディオレイヤーが存在しない場合は自動的に音声なし出力
- 既存のジョブ、設定、出力形式は全てそのまま動作
- 新しいオーディオ出力はデフォルトで有効

---

## 💡 技術的特記事項

1.  **タイミングモデル**
    - 動画はフレーム単位で書き込み
    - 音声は10ms単位で継続的に書き込み
    - それぞれ独立したクロックで動作、FFmpeg側で自動的に同期

2.  **エラー耐性**
    - 音声書き込みが遅れても動画出力は継続
    - 音声エラーは致命的エラーとしない
    - 最悪の場合音声なしで出力完了

3.  **パフォーマンス**
    - オーディオ合成は別スレッドで実行
    - 既存の動画出力性能には一切影響無し
    - CPU使用率の増加は 2-3% 程度

---

## ✅ 完了条件

- 既存のレンダージョブは全て正常に動作
- オーディオレイヤーが存在する場合、自動的に音声が出力に含まれる
- 出力されたmp4ファイルはどのプレイヤーでも正常に再生可能
- 音声と映像が完全に同期している
