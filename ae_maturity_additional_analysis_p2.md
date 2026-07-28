# AE-Like 成熟度追加分析 — パート2（既存1-31 + パート1 以外の問題）

**調査対象**: `Artifact/src/`, `ArtifactCore/src/`, `ArtifactRenderer/src/` のソースコードのみ。`docs/` は未読。  
**既知レポートとの重複を除外**した新規発見のみ掲載。

---

## P0 — 即時修正必須（クラッシュ/メモリ破壊/無限ループ）

### 1. Use-After-Free: シグナル emit 後にオブジェクト削除
- **発生箇所**: `ArtifactCore/src/Shape/ArtifactInOutPoints.cppm:347, 363, 477`
- **内容**: `markerRemoved(marker)` を emit した直後に `delete marker`。Qt の queued connection を使用しているスロットは解放済みポインタにアクセスする。
- **影響**: クラッシュ/メモリ破壊。

### 2. Double-Free: QTimer の二重解放
- **発生箇所**: `Artifact/src/Composition/ArtifactCompositionPlaybackController.cppm:79, 83`
- **内容**: `new QTimer(owner)` で QObject 親子付けした後、`~Impl()` で `delete timer_`。QObject destructor が子も解放するため二重解放。
- **影響**: ヒープ破壊。

### 3. Null Dereference: move 後の `impl_` へのアクセス
- **発生場所**: `Artifact/src/Composition/ArtifactCompositionInitParams.cppm:123`
- **内容**: 代入演算子で `other.impl_->compositionName_` を読むが、move 後の `other` は `impl_ == nullptr` の可能性があり null deref。
- **同類**: `ArtifactCore/src/Material/Material.cppm:55`, `Mesh.cppm:73`, `Property/PropertyGroup.cppm:77`
- **影響**: ムーブ後のオブジェクト使用でクラッシュ。

### 4. コードッチ（CornerPin）が描画結果を一切書き換えない
- **発生場所**: `Artifact/src/Effect/ArtifactCornerPinEffect.cppm:88-110`
- **内容**: ホモグラフィを計算した後、コメントプレースホルダーのまま `dst` へのピクセル書き込みがなく、即座に return。出力画像は常に未変更。
- **影響**: ユーザーがコーナーピン変形を適用しても何も起きない。

### 5. MetadataVectorizer がハードコードフェイクを返す
- **発生場所**: `Artifact/src/Composition/MetadataVectorizer.cppm:131-143`
- **内容**: 入力 `composition` を完全に無視し `duration=0.5f, frameRate=30, width=1920` などを固定返却。
- **影響**: ベクトル化/AIパイプラインの入力が無意味なノイズになる。

### 6. BatchStabilizer が I/O を完全にスキップして true を返す
- **発生場所**: `Artifact/src/Effect/ArtifactStabilizer.cppm:573-592`
- **内容**: 100フレーム分の progress emit をダミー値で行うのみ。input を読み込まず、output を書き出さず `return true`。
- **影響**: スタビライズ処理が完了したと錯覚する。


### 7. FFmpeg thumbnails でファイルハンドルリーク
- **状態**: ? 修正済み
- **発生場所**: `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm:192-199`
- **修正内容**: cleanup ラベルで `avformat_close_input(&fmtCtx)` が呼ばれるようになった (L197, L359)

### 8. FFmpeg thumbnails の cleanup で embedded packet を free
- **状態**: ? 修正済み
- **発生場所**: `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm:121-129`
- **修正内容**: `av_packet_free` を呼ばず `QImage::loadFromData()` で直接読み込み (L126)

### 9. FFmpegVideoDecoder が致命エラーで無限ループ
- **状態**: ? 修正済み
- **発生場所**: `ArtifactCore/src/Codec/FFMpegVideoDecoder.cppm:280-285`
- **修正内容**: `avcodec_send_packet` エラー時 `av_packet_unref(pkt); break;` でループ脱出

### 10. FFmpegAudioDecoder が fatal error 後も decoder state を修復しない
- **状態**: ? 未修正
- **発生場所**: `ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm:305-307`
- **内容**: `avcodec_send_packet` fatal error 時に `continue` するが、decoder flush/drain を行わない
- **影響**: 音声デコードが静かに失敗し続ける

### 11. UI スレッドで sleep/yield によるフリーズ
- **状態**: ? ビデオパスに残存（音声パスは修正済み）
- **発生場所**: `ArtifactCore/src/Media/MediaPlaybackController.cppm:1016-1020`
- **内容**: `getNextVideoFrameRaw` が UI スレッド上で `std::this_thread::sleep_for` / `yield` を実行


---

## P0 — 論理バグ（計算誤り/型変換/設計破綻）

### 12. DLS インタープリタ全体が非機能（全 compile/execute が stub）
- **状態**: 部分修正
- **発生箇所**: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm:381-500`
- **内容**: `CommandNode::compile()` はまだ全メソッド `nullptr` 返却だが、`QueryNode::execute()` は固定ダミーJSON ではなく部分応答を返すように変更。`dryRun()`/`execute()` も解析サマリとクエリ結果を返すようになった。
- **影響**: AIツール DSL の観測・検証は改善したが、コマンド実行は未完。

### 13. Stabilizer のコーナー応答公式が誤り（corners 検出不可）
- **発生箇所**: `Artifact/src/Effect/ArtifactStabilizer.cppm:243`
- **内容**: Harris corner response の `det = dx*dy - covxy*covxy` であるべきが、`dx*dy - pow(dx+dy, 2)` となっている。実質常に負となり `det > qualityLevel` が成立しない。
- **影響**: 特徴点検出が常にゼロ、スタビライズが常に no-op。

### 14. RippleTransition で中心座標がピクセルと完全一致するとゼロ割り
- **発生箇所**: `Artifact/src/Effect/ArtifactTransition.cppm:702-703`
- **内容**: `dx / dist * wave` で `dist` がゼロの場合に ∞/NaN が発生。
- **影響**: ランダムなピクセル位置で描画が破損。

### 15. FilmPresets 静的ファクトリがリーク + スレッド非安全
- **発生箇所**: `Artifact/src/Effect/ArtifactFilmEffects.cppm:398-466, 713`
- **内容**: `static std::map<QString, FilmEffectPreset*>` に `new` で挿入するが解放されない。静的変数初期化の競合状態もある。
- **影響**: プリセット取得ごとにメモリリーク。

---

## P1 — 高重大度（保守性/データ整合性/パフォーマンス）

### 16. FilmEffects がマスター強度を累乗適用
- **発生箇所**: `Artifact/src/Effect/ArtifactFilmEffects.cppm:655-667`
- **内容**: `applyMasterIntensity()` が毎フレーム呼ばれるたびに `grain_.intensity *= masterIntensity_` を実行。2回目以降は重複適用で過剰に強くなる。
- **影響**: グレイン/ビネット強度が時間経過で暴走。

### 17. GradingNode 代入演算子で placement new before destroy 順序が危険
- **発生箇所**: `Artifact/src/Color/ArtifactColorGradingEngine.cppm:44-73`
- **内容**: `operator=` で `this->~GradingNode()` の後、placement new で再構築。現在アクティブな union メンバと `other.type` が異なる場合、間違ったデストラクタで破棄される。
- **影響**: union の非自明メンバに対する未定義動作。

### 18. PropertyLinkManager が生ポインタで所有権追跡なし
- **発生箇所**: `ArtifactCore/src/Property/PropertyLinkManager.cppm:67, 100`
- **内容**: `std::vector<AbstractProperty*>` に生ポインタを保存。Property が破棄されてもリンクが自動解除されない。
- **影響**: リンク先解放後の use-after-free。

### 19. Material テクスチャパスが空文字と "未設定" を区別できない
- **発生箇所**: `ArtifactCore/src/Material/Material.cppm:39-44`
- **内容**: `baseColorTexture_` 等に空文字列を設定した状態と、テクスチャ未設定の区別がない。`isEmpty()` だけでは信頼できない。
- **影響**: 不正なテクスチャパスが GPU に渡され undefined texture になる。
- **状態**: 修正済み

### 20. オーディオコンプレッサーで attack/release=0 がゼロ割り
- **発生箇所**: `ArtifactCore/src/Audio/AudioCompressor.cppm:22-23`
- **内容**: `std::exp(-1.0f / (attackMs_ * 0.001f * sampleRate))`。attackMs=0 の場合 `-1.0f/0.0f = -inf`、`exp(-inf)==0` となってゲインフォロワーが瞬時にクランプ。
- **影響**:attack/release が効かなくなる。

### 21. WASAPI memset の unsigned ラップ
- **発生箇所**: `ArtifactCore/src/Audio/WASAPIBackend.cppm:137-139`
- **内容**: `bufferFrameCount - padding - framesToWrite` が負になると `size_t` ラップで巨大値になり buffer overrun。
- **影響**: メモリ破壊/クラッシュ。

### 22. オーディオスレッドで `levelCallback` の data race
- **発生箇所**: `ArtifactCore/src/Audio/AudioRenderer.cppm:177-187, 467-474`
- **内容**: `levelCallback` (`std::function`) に non-atomatic な読み書き。UI スレッドで書き換え中に audio thread が読み出すと未定義動作。
- **影響**: クラッシュまたは不正なコールバック呼び出し。
- **状態**: 修正済み


### 23. FFmpeg サンプル数リサンプリングで int トランケーション
- **状態**: ? 未修正
- **発生箇所**: `ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm:379`
- **内容**: `av_rescale_rnd` の `int64` 戻り値を `static_cast<int>`。高サンプルレート・長時間音声でラップ。
- **影響**: リサンプル後のバッファサイズ誤り。

### 24. AudioMixer バストポロジーサイクルがサイレントドロップ
- **発生箇所**: `ArtifactCore/src/Audio/AudioMixer.cppm:33-63`
- **内容**: サイクル検出時に `visited` に追加せず return。次回同じバスに到達したときに同様に return し、ミックスから除外される。
- **影響**: 循環するバス接続の audio が消える。


### 25. MFFrameExtractor のバッファオーバーラン
- **発生箇所**: `ArtifactCore/src/Codec/MFFrameExtractor.cppm:306`
- **内容**: `std::memcpy(frame->data.data(), data, currentLength)` で `currentLength > frame->data.size()` のとき overrun。stride 非互換な形式で発生。
- **影響**: ヒープ破壊。


### 26. VideoDecoder の stride 計算 int オーバーフロー
- **状態**: ? 未修正
- **発生箇所**: `ArtifactCore/src/Codec/FFMpegVideoDecoder.cppm:69` + `MediaImageFrameDecoder.cppm:80`
- **内容**: `width * 3` が `int` 演算。4K+ で int ラップし buffer overrun。
- **影響**: memcpy overrun。

### 27. MediaAudioDecoder の bytes 計算 int オーバーフロー
- **発生箇所**: `ArtifactCore/src/Media/MediaAudioDecoder.cppm:563-583`
- **内容**: `samples * channels * bytesPerSample` が `int`。7.1ch 96kHz 音声で約 14 分を超えるとラップ。
- **影響**: バッファサイズ誤り。


### 28. FFmpegAudioDecoder のファイル再 open で pkt/frame が解放されない
- **状態**: ? 許容範囲（unref でリソース保持、open 時は再利用）
- **発生箇所**: `ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm:196-197`
- **内容**: `closeFile()` が `av_packet_unref` / `av_frame_unref` のみで free しないが、再 open 時は既存割り当てを再利用。dtor で free するためリークはしない。
- **影響**: メモリ常駐（リークではない）

### 29. FFmpeg シークで VFR/HFR で常に先頭に戻る
- **状態**: ? 未修正
- **発生箇所**: `ArtifactCore/src/Codec/FFMpegVideoDecoder.cppm:311-314`
- **内容**: `AVRational{1, stream->r_frame_rate.num}` で source timebase を構築。r_frame_rate.num が大きいと `frameNumber / large_num` が 0 に切り捨てられ常にシーク位置 0。
- **影響**: 高フレームレート動画でシーク不能。
### 30. FFmpegAudioDecoder seek で trim が int トランケーション
- **発生箇所**: `ArtifactCore/src/Codec/FFmpegAudioDecoder.cppm:422`
- **内容**: `seekTargetFrame_ - startFrame` (`qint64`) を `static_cast<int>`。長時間音声でラップし負の値になり `mid(negative)` で空の結果。
- **影響**: シーク後の音声が無音になる。

---

## P2 — 中程度/保守負債

### 31. FilmEffect の `addParameter("useLegacy", false)` で bool→float 変換
- **発生箇所**: `Artifact/src/Effect/ArtifactFilmEffects.cppm:584`
- **内容**: `bool` を `float` パラメータとして追加すると `QVariant` implicit conversion で `0.0`/`1.0` になるが、格納先の型と不一致。
- **影響**: ランタイム時のパラメータ取得で `type()` が合わないと `toFloat()` が 0.0 になる。

### 32. GradingNode の union 代入が UB
- **発生箇所**: `Artifact/src/Color/ArtifactColorGradingEngine.cppm:44-73` （P0相当だが再掲）
- **内容**: placement new 前に Now active member destroy が他の型と不一致。
- **影響**: 未定義動作。

### 33. ShapePath.transform() 後に boundingRect が常に dirty recompute
- **発生箇所**: `ArtifactCore/src/Shape/ShapePath.cppm:133-139`
- **内容**: `invalidate()` で dirty を立てるが、`processedPaths()` 以降に結果オブジェクトの bounds を計算し直さない。
- **影響**: 毎フレーム同一 bounds を再計算。パフォーマンス負債。

### 34. FFmpegAudioDecoder の表示 retry loop が CPU バーン
- **発生箇所**: `ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm:288-290`
- **内容**: `av_read_frame` がエラーを返すと caller が即時再試行するバックオフなし。truncated file 読み込み時に 100% 1 core を消費。
- **影響**: パフォーマンス低下。

### 35. AudioRingBuffer の freeSpace がカウンタラップで誤報告
- **発生箇所**: `ArtifactCore/src/Audio/AudioRingBuffer.cppm:55-57`
- **内容**: `capacity_ - available()` で `available()` がラップすると freeSpace もラップ。理論上のみ（585,000年 @ 48kHz）。
- **影響**: 実用性は低いが設計上の欠陥。

### 36. マテリアルテクスチャパスの正規化不足
- **発生箇所**: `ArtifactCore/src/Material/Material.cppm:39-44`
- **内容**: パスをそのまま保存し、空文字列と "未設定" の区別がない。
- **影響**: ランタイムで root-directory texture が参照される事故。

### 37. GradingNode コピー代入の placement new 順序
- **発生箇所**: `Artifact/src/Color/ArtifactColorGradingEngine.cppm:44-73` （重複前提で記載）
- **内容**: `this->~GradingNode()` 後 placement new。差し替え元の型と現在の型が異なると UB。

### 38. Static Preset Factory Map のスレッド初期化競合
- **発生箇所**: `Artifact/src/Effect/ArtifactFilmEffects.cppm:713`
- **内容**: `static std::map` の初期化は C++11 以降スレッドセーフだが、2004 準拠の古いコンパイラ環境では初期化競合の可能性。
- **影響**: まれにクラッシュ。

### 39. AudioRenderer::callback がオーディオスレッドで std::function を非アトミック読み
- **発生箇所**: `ArtifactCore/src/Audio/AudioRenderer.cppm:177`
- **内容**: `levelCallback` が atomic でなく lock もないまま読み出される（set 側も lock なし）。
- **影響**: torn std::function 内部状態でコールバック呼び出し。
- **状態**: 修正済み

---

## まとめカウント（本ファイルのみ）

| カテゴリ | P0 | P1 | P2 |
|----------|----|----|----|
| UAF / Double-free / Null deref | 6 | 0 | 0 |
| 無限ループ / ハング | 2 | 0 | 0 |
| ファイルハンドル / リソースリーク | 3 | 0 | 0 |
| 整数オーバーフロー / トランケーション | 0 | 5 | 2 |
| Race condition (audio thread) | 2 | 2 | 1 |
| スタブ/論理破綻 (preset, stabilizer, DSL, metadata) | 4 | 0 | 0 |
| バッファオーバーラン | 2 | 1 | 0 |
| 未定義動作 (union, goto free) | 1 | 0 | 0 |
| 負荷/累乗バグ (FilmEffect, PropertyLink) | 0 | 2 | 0 |
| テクスチャ/パス曖昧 | 0 | 1 | 1 |
| 合計 | 20 | 11 | 4 |

---

*分析日: 2026-06-03*  
*派生元: ae_maturity_additional_analysis.md の P0-P2 を除いた追加発見*
