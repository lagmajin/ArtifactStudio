**最終更新:** 2026-08-06

# 動画レイヤー プレビュー再生遅延 原因調査メモ

**結論: デコードがワーカースレッドで間に合わないとき、レンダースレッドがデコードを同期で待ってしまうのが遅延の根幹。**

調査対象: `Artifact/src/Layer/ArtifactVideoLayer.cppm` (3,082行)、`ArtifactCore/src/Media/MediaPlaybackController.cppm` (1,621行)、`ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`。

## 再生パイプラインの構造

`draw()` (ArtifactVideoLayer.cppm:2558) は「リアルタイムは wall-clock 駆動、デコード待ちで描画スレッドをブロックしない（最後の良いフレームを表示）」という設計だが、その設計が破綻する抜け穴がある。

1. `draw()` は `cachedFrameImageBuffer()` → `currentFrameImageBuffer()` の順でフレーム取得 (ArtifactVideoLayer.cppm:2583-2586)。
2. どちらも空なら `currentFrameImageBuffer()` が `decodeFrameToImageBuffer()` を呼ぶ (cppm:1910, 2025)。
3. この関数は `getVideoFrameAtFrameDirectRaw()` を同スレッド（＝描画/UI スレッド）で同期的に呼ぶ (cppm:2026)。**FFmpeg の seek＋デコードが描画スレッドでブロック**。
4. ワーカースレッド (QtConcurrent, cppm:1614) は1フレームずつデコード＋float変換＋prefetch をこなす。4K/HEVC 等で1フレームのデコードがフレーム間隔（30fpsなら33ms）を超えると、描画側が「今のフレームがキャッシュに無い」→同期デコード→描画スレッドが数 ms〜数十 ms 固まる＝コマ落ち・カクつき。

## 遅延を大きくする要因（優先順）

1. **描画スレッドでの同期フォールバックデコード（最重要）**: `draw()` の設計コメント (cppm:2602) と実装が矛盾。「decode-worker が1フレームでも遅れたら描画側が同期デコード」になる。重いコーデックでは常に引っかかる。
2. **GPU/ハードウェアデコードが無効化**: `draw()` 先頭コメント (cppm:2561-2563) 「This layer consumes ImageF32x4_RGBA only. Enabling Vulkan hardware decode … can leave the CPU presentation buffer empty」と明記。Vulkan HW デコード実装は `MediaImageFrameDecoder.cppm:303` に存在するが、プレビューは常に CPU ダウンロード→float 変換を通る。ハードウェアデコードの恩恵ゼロ。
3. **全フレームが float RGBA (16byte/px) に変換**: `decodedVideoFrameToImageF32x4_RGBA()` (cppm:1661, 2028) で BGRA→RGBA スワズル＋float 化。4K なら1フレーム 64MB の帯域をデコードごとに消費。8bit の2倍。
4. **FFmpeg ランダムシークはキーフレーム基点＋前方再デコード**: `decodeVideoFrameDirectAtFrameRaw()` (MediaPlaybackController.cppm:260) は `kMaxSequentialFrameGap=8` の範囲外やスクラブ時は毎回シーク＋前方デコード。キャッシュミス時のペナルティ大。
5. **フレームキャッシュは 120 枚固定** (cppm:573 `frameCache_(120)`)。高解像度（4K float 64MB×120 ≈ 7.7GB）だと実質数秒分しか保持できず、メモリ不足で早期追い出し→再デコード増。
6. **複数動画レイヤーはレイヤーごとにデコードワーカー1本**: 複数同時再生でデコード直列化。
7. **prefetch は +1 フレームのみ、かつ pending リクエストが無い時だけ** (cppm:1816): 逆再生・スキップ・スクラブには効かず、毎回同期デコードに落ちる。

## 改善案（優先順）

1. **`draw()` の同期フォールバックを排除**: キャッシュミス時は最後の良いフレームを表示して即 return（コメントの意図通り）。遅延は「1フレーム遅れ」で止まり、描画スレッドは固まらない。→ 最優先、数行〜数十行の修正でコマ落ちの大部分が解消。
2. **デコードワーカーの先読みを N+2〜N+4 に拡張**、キューを latest-only ではなく数件保持。
3. **GPU デコード→テクスチャ直接提示**をプレビューに通す (`ImageF32x4RGBAWithCache` の GPU テクスチャ側を使用)。CPU ダウンロード＋float 変換を回避。
4. **キャッシュを解像度に応じて容量(枚数/MB)制限**。低解像度プロキシ優先 (`ProxyServiceQuality::Full` 以外はスケール済みでデコード、cppm:2262 に仕組みあり) の運用を既定化。
5. スクラブ時は同期デコードではなく「最寄りキーフレームの非同期デコード要求」に留める。

**備考**: 動画対応は AGENTS.md 開発優先方針 (2026-07-27) で「当面後回し」扱い。本調査は既存経路の静的解析のみ（ビルド・再生実測は未実施）。
