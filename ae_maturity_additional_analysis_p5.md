# AE-Like 成熟度追加分析 — パート5（パート1-4 + 既存1-31 以外の問題）

**調査対象**: Animation/Time/Undo, RenderScheduler/RenderQueue, Effect(motionBlur/transition/film/cornerPin/creative)  
**制約**: ソースコードのみ。

---

## P0 — 即時修正必須

### 1. UndoManager が自分自身のモジュールを import
- **発生箇所**: `Artifact/src/Undo/UndoManager.cppm:46`
- **内容**: `import Undo.UndoManager;` を自身のファイル内で実施。モジュール不自然な再帰インポート。
- **影響**: ビルド/ODR に不安定性。

### 2. RemoveLayerCommand の originalIndex_ が redo でキャプチャ
- **発生箇所**: `Artifact/src/Undo/UndoManager.cppm:176-183`
- **内容**: コンストラクタでなく `redo()` 実行時に `originalIndex_` を取得。Undo/Redo サイクルでレイヤー配置が変わると不正インデックス。
- **影響**: Redo 後に Remove が誤ったレイヤーを消す。

### 3. TimeCodeRange::trimEnd が範囲を拡張してしまう
- **発生箇所**: `ArtifactCore/src/Time/TimeCodeRange.cppm:173`
- **内容**: `trimEnd(int)` の実装が `+=`。end frame が増える。
- **影響**: レンジ選択/再生範囲が意図せず拡張。

### 4. RationalTime の rescale で乗算オーバーフロー
- **発生箇所**: `ArtifactCore/src/Time/RationalTime.cppm:90`
- **内容**: `value_ * newScale` を先に計算してから割る。int64 ラップで符号反転/巨大数。
- **影響**: 長時間コンポジションのタイムコードが不正。

### 5. RationalTime の比較演算子で cross-multiplication オーバーフロー
- **発生箇所**: `ArtifactCore/src/Time/RationalTime.cppm:120, 131`
- **内容**: `operator<`, `operator==` 両方で `a.num * b.den` を計算。64bit ラップ。
- **影響**: フレーム比較が不正でシーク/キーフレーム挿入位置がずれる。

### 6. TimeCodeRange / TimeCode の moved-from null deref
- **発生箇所**: `ArtifactCore/src/Time/TimeCodeRange.cppm:46, 67`, `TimeCode.cppm:93-100, 152`
- **内容**: copy ctor/assignment が `impl_` の null チェックなし。Move 後のオブジェクト使用でクラッシュ。
- **影響**: コンポジション操作タイミングでクラッシュ。

### 7. FFmpegEncoder の linesize がハードコード (再掲/拡張)
- **発生箇所**: `Artifact/src/Export/FFmpegEncoder.cppm:445, 676`
- **内容**: `srcLinesize = {w * 4}`。av_frame_get_buffer の実際 stride と不一致。
- **影響**: 書き出しフレームの破損/メモリオーバーラン。

### 8. RenderScheduler の cancel がアウトオブ mutex
- **発生箇所**: `Artifact/src/Render/ArtifactRenderScheduler.cppm:275, 287`
- **内容**: `cancelTask()` / `cancelAllTasks()` が atomic セットの後に mutex 取得中に他の submit が割り込む。
- **影響**: タスク実行状態の不整合。

### 9. RenderQueueService のワーカーが QList を index  traversal 中に delete
- **発生箇所**: `Artifact/src/Render/ArtifactRenderQueueService.cppm:3386`
- **内容**: `queueManager.jobs` (QList) を `[]` 演算子でループ中、別スレッドが `removeJob` / `removeAt` で要素削除。index シフトで要素スキップ or 誤削除。
- **影響**: レンダージョブが無限ループ or ジョブ消失。

### 10. BatchRenderer が RenderTask を解放しない
- **発生箇所**: `Artifact/src/Render/ArtifactRenderScheduler.cppm:569`
- **内容**: `new RenderTask` を pending/active/completed 生ポインタで保持し delete しない。
- **影響**: レンダーキュー実行ごとにメモリリーク。

### 11. RenderContextRegistry のスナップショットが無限蓄積
- **発生箇所**: `Artifact/src/Render/ArtifactRenderQueueService.cppm:3556`
- **内容**: フレームごとに `registerRenderQueueContextSnapshot` が登録するが cleanup がない。
- **影響**: 長時間レンダリングでメモリリーク。

### 12. ensureGpuRendererInitialized の並列Initialize racing
- **発生箇所**: `Artifact/src/Render/ArtifactRenderQueueService.cppm:2243`
- **内容**: 複数ジョブが同時に `initializeHeadless` / `destroy` を呼ぶ可能性。`gpuRenderer_` の二重初期化/二重破棄。
- **影響**: GPU デバイスロスト or クラッシュ。

### 13. encoderMutex を保持したまま busy-wait
- **発生箇所**: `Artifact/src/Render/ArtifactRenderQueueService.cppm:1822`
- **内容**: `setRenderFrameFunc` が `while (frameBuffer.count(nextFrameToEncode))` で mutex 保持。ギャップが生じると他スレッドがブロック。
- **影響**: エンコードスレッド全体のスループット喪失。

### 14. モーションブラの divide-by-zero (samples=1)
- **発生箇所**: `Artifact/src/Effect/ArtifactMotionBlur.cppm:80, 149, 211`
- **内容**: `blurWeightSum / (samples - 1)`。clamp minimum が 1 なので `samples==1` で `0` 除算。
- **影響**: サンプル数 1 の設定で ∞/NaN。

### 15. MotionBlur の clamp-mode で weight が加算されない
- **発生箇所**: `Artifact/src/Effect/ArtifactMotionBlur.cppm:96-108`
- **内容**: 範囲外サンプルを `continue` するが `totalWeight` に加算しない。全サンプルが範囲外の場合ブラーが黒塗り。
- **影響**: エッジで黒縁が発生。

### 16. CornerPin のホモグラフィが特異値判定なし
- **発生箇所**: `Artifact/src/Effect/ArtifactCornerPinEffect.cppm:105`
- **内容**: `computeHomography` の戻り値行列を determinant チェックなしで使用。4点が共線/重複時 NaN/Inf。
- **影響**: コーナーピン適用時に画像崩壊 or クラッシュ。

### 17. PixelateEffect の count==0 で zero-divide
- **発生箇所**: `ArtifactCore/src/Graphics/Effect/PixelateEffect.cppm:47-49`
- **内容**: `width==0 || height==0` でループ不実行、`count==0`、`r_sum / count` でゼロ除算。
- **影響**: モザイク効果がクラッシュ。

### 18. FilmEffects toJSON が grain しか保存しない
- **発生箇所**: `Artifact/src/Effect/ArtifactFilmEffects.cppm:349-369`
- **内容**: `toJSON` で grain のみシリアライズ。それ以外はロード後に消失。
- **影響**: プリセット保存/復元でほとんどのパラメータが lose。

### 19. FilmEffects::process が未使用パラメータを無視
- **発生箇所**: `Artifact/src/Effect/ArtifactFilmEffects.cppm:655-667`
- **内容**: `scratches/dust/lightLeak/jitter` はプロットに反映されない。`applyMasterIntensity` の重複適用と複合。
- **影響**: 設定しても效果なし。

### 20. Halftone/Mirror/Kaleidoscope のモジュール実体が空
- **発生箇所**: `ArtifactCore/src/Graphics/Effect/HalftoneEffect.cppm`, `MirrorEffect.cppm`, `KaleidoscopeEffect.cppm`
- **内容**: 1行モジュール宣言のみ。`process()` の実体がない。
- **影響**: クリエイティブエフェクトの3種が機能しない。

### 21. CreativeEffectFactory が何も登録しない
- **発生箇所**: `ArtifactCore/src/Graphics/Effect/CreativeEffectFactory.cppm:3`
- **内容**: 登録ロジックのコメントのみで実装ゼロ。
- **影響**: ファクトリ経由でエフェクトが作成不能。

### 22. TransitionManager の登録数が10個で後段が未登録
- **発生箇所**: `Artifact/src/Effect/ArtifactTransition.cppm:1006-1015`
- **内容**: 10個のみ登録。Dip/Wipe/Slide/Zoom 系大半が `nullptr` fallback で瞬間切替。
- **影響**: 多くのトランジションが機能しない。

---

## P1 — 高重大度

### 23. RemoveLayerCommand の redo タイミングで originalIndex 取得 (P0 相当、再掲)
- **発生箇所**: `Artifact/src/Undo/UndoManager.cppm`
- **内容**: 上記 P0 #2 と同じ構造の問題。

### 24. UndoManager 二重 import のODRリスク
- **発生箇所**: `Artifact/src/Undo/UndoManager.cppm`
- **内容**: 自己 import でモジュール UE 境界が不自然。
- **影響**: プラットフォームによってはビルド失敗。

### 25. AnimatableTransform3D の不要ヘッダ大量
- **発生箇所**: `ArtifactCore/src/Animation/AnimatableTransform3D.cppm:5-37`
- **内容**: `<iostream>`, `<fstream>`, `<thread>` 等 ~30 の未使用 include。コンパイル時間増加。
- **影響**: ビルドパフォーマンス。

### 26. AnimatableTransform3D の collectUniqueKeyFrameTimes が 24fps 固定
- **発生箇所**: `ArtifactCore/src/Animation/AnimatableTransform3D.cppm:64`
- **内容**: `RationalTime(framePos, 24)` をハードコード。
- **影響**: 24fps 以外のキーフレーム時間が不正。

### 27. EasingCurveUtil が Back 系イージングを candidates から除外
- **発生箇所**: `ArtifactCore/src/Animation/EasingCurveUtil.cppm:70-78`
- **内容**: UI の一覧にあるが候補リストに Back 系がない。UI/バックエンド不一致。
- **影響**: ユーザーが Back を選べない。

### 28. TimeRemap が QImage をホットパスで使用
- **発生箇所**: `ArtifactCore/src/Time/TimeRemap.cppm:9-10`
- **内容**: `QImage` / `QPainter` をフレームブレンディング処理に使用。GPU パスが存在しない。
- **影響**: 高解像度でのパフォーマンス劣化。

### 29. TimeRemap のデフォルト線形マップが 10秒 固定
- **発生箇所**: `ArtifactCore/src/Time/TimeRemap.cppm:61-89`
- **内容**: `timeRemap` 未設定で 10 秒の直線を使用。コンポジションの実際の長さと無関係。
- **影響**: リマップ未設定時に不自然な再生速度。

### 30. TimeRemap::processTimeStretchFFT が FFT を使わない
- **発生箇所**: `ArtifactCore/src/Time/TimeRemap.cppm:333-394`
- **内容**: 関数名に `FFT` とあるが実装は Hann 窓付きサンプルコピー。位相推定/加算合成なし。
- **影響**: ピッチ変移が正しくない。

### 31. TimeCode::fromHMSF の int オーバーフロー
- **発生箇所**: `ArtifactCore/src/Time/TimeCode.cppm:93-100`
- **内容**: `h * 3600` が `int`。長時間メディア (h > 596523) でラップ。
- **影響**: 長時間音声/動画でタイムコード異常。

### 32. TimeCode::fromRationalTime の int トランケーション
- **発生箇所**: `ArtifactCore/src/Time/TimeCode.cppm:152`
- **内容**: `int64_t frameCount` を `int` にキャスト。長尺コンポジションでラップ。
- **影響**: タイムコード表示/シークが不正。

### 33. RealTime に deltaTime 上限なし
- **発生箇所**: `ArtifactCore/src/Time/RealTime.cppm:38`
- **内容**: デバッガ一時停止/フレームスパイクで `deltaTime` が数秒/数十秒に膨張。物理/アニメーションが吹っ飛ぶ。
- **影響**: デバッグ/負荷時の挙動。

### 34. RenderScheduler の completedCount_ 二重カウント
- **発生箇所**: `Artifact/src/Render/ArtifactRenderScheduler.cppm:207, 460`
- **内容**: 成功時に2箇所で `completedCount_++`。プログレスバーが2倍進む/負の残りになる。
- **影響**: レンダー進捗表示が不正。

### 35. RenderScheduler の cancelled も completedCount_ に加算 (二重)
- **発生箇所**: `Artifact/src/Render/ArtifactRenderScheduler.cppm:402, 460`
- **内容**: Skipped/Cancelled を完了としてカウント。同じく二重。
- **影響**: 同上。

### 36. RenderQueueService の catch 後ジョブが Failed にならない
- **発生箇所**: `Artifact/src/Render/ArtifactRenderQueueService.cppm:3686`
- **内容**: ワーカースレッドの catch が `isRendering_=false` するのみ。キュー内のジョブが `Rendering` のまま放置。
- **影響**: レンダリングが完了しない無限待ち。

### 37. RenderQueueService のフレームバッファがアンバウンド
- **発生箇所**: `Artifact/src/Render/ArtifactRenderQueueService.cppm:1822-1830`
- **内容**: `frameBuffer` (std::unordered_map) が欠落フレームでスタールしエントリが無限増殖。
- **影響**: 長時間レンダリングで OOM。

### 38. Halftone/Mirror/Kaleidoscope モジュール宣言のみ
- **発生箇所**: `ArtifactCore/src/Graphics/Effect/HalftoneEffect.cppm`, `MirrorEffect.cppm`, `KaleidoscopeEffect.cppm`
- **内容**: `module ArtifactCore.Graphics.Effect.Halftone;` のみ。process() が存在しない。
- **影響**: 3種のエフェクトがクラッシュ/無視。

### 39. AnimationDynamics のプリセット名判定が float 正確一致
- **発生箇所**: `ArtifactCore/src/Animation/AnimationDynamics.cppm:22-29`
- **内容**: `stiffness == 20.0f && damping == 5.0f` 等でプリセット名を返す。シリアライズ丸めで Custom 固定に。
- **影響**: プリセット復元で常に "Custom" 表示。

---

## P2 — 中程度/保守負債

### 40. Transition alpha が RGB32 固定で破棄
- **発生箇所**: `Artifact/src/Effect/ArtifactTransition.cppm:132, 241, 571`
- **内容**: alpha 計算するも `Format_RGB32` で出力。計算が無駄。
- **影響**: パフォーマンス + alpha チャンネル損失。

### 41. GradingNode の operator= が UB (重複確認)
- **発生箇所**: `Artifact/src/Color/ArtifactColorGradingEngine.cppm:44-73`
- **内容**: placement new 前 destroy が union 型不一致の可能性。
- **影響**: まれにクラッシュ/メモリ破壊。

### 42. TBB ヘッダが未使用でインクルードのみ
- **発生箇所**: `Artifact/src/Render/ArtifactRenderQueueService.cppm:2,69`
- **内容**: `<tbb/parallel_for.h>` 等が include されているが TBB 利用部なし。過去の名残。
- **影響**: コンパイル時間の無駄。

### 43. FFmpegEncoder の 3重コピー
- **発生箇所**: `Artifact/src/Export/FFmpegEncoder.cppm:394-435`
- **内容**: RGBA float → uint8 手動コピー → `sws_scale` でさらに入力バッファにコピー。3重。
- **影響**: エンコードパスのパフォーマンス。

### 44. ショートカット "Ctrl+R" が Redo の一般的な代替と競合
- **発生箇所**: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm:181`
- **内容**: Rulers 表示のトグルに Ctrl+R。AE 等で有的な Redo 代替キーと競合。
- **影響**: ユーザー混乱。

### 45. ColorPaletteManager の FloatColor 丸め (8-bit 量子化)
- **発生箇所**: `Artifact/src/Color/ColorPaletteManager.cppm:57`
- **内容**: `QColor::HexArgb` 保存で float 精度が 8bit/ch に切り捨て。
- **影響**: カラー設定の往復で HDR 色が崩れる。

---

## 追加カウント（パート5 のみ）

| カテゴリ | P0 | P1 | P2 |
|----------|----|----|----|
| Time/Animation 数値演算 | 3 | 3 | 2 |
| Undo ロジック | 2 | 1 | 0 |
| Render スケジューラ race/leak | 6 | 1 | 0 |
| Effect スタブ/論理バグ | 8 | 3 | 1 |
| FFmpeg/エンコード | 2 | 0 | 1 |
| Build (再掲/拡張) | 2 | 2 | 0 |
| 合計 | 23 | 10 | 4 |

---

*分析日: 2026-06-03*
