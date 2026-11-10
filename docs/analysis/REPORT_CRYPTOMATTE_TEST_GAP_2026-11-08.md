# Cryptomatte Test Coverage Gap

**最終更新:** 2026-11-08

## 調査

`Image.Cryptomatte` は `CryptoPixel`（ピクセル単位のID+coverage蓄積）、`CryptoImage`（画像全体のIDバッファ変換とrank層生成）、`CryptoManifest`（ID→名前マッピング）、`CryptoExrWriter`（EXR書き出し）から成る。公開型は `CryptoSample` / `CryptoPixel` / `CryptoManifest` / `CryptoManifestEntry` / `CryptoImage` / `CryptoLayer` / `CryptoExrWriteResult` / `CryptoExrWriter` だが、これらを直接扱うテストは存在しなかった。

実装（`ArtifactCore/src/Image/Cryptomatte/CryptoPixel.cpp`、`CryptoImage.cpp`）を精読し、次の挙動を契約として確認した。

- `CryptoSample::nameToId` は MurmurHash3 x86 32 の結果を 24 bit（`0x00ffffff`）に切り詰めた固定値で、名前に対して決定的。
- `CryptoSample::nameToFloat` はその ID を float 化し、`floatToId` は非有限値・0 以下を 0 として扱い、有限の正値は切り捨てで ID へ戻す。
- `CryptoPixel::addSample` は非有限 ID・coverage・coverage 0 以下を拒否し、coverage を 0〜1 にクランプする。同一 ID の再登録は coverage を合算（1.0 でクランプ）し、既存サンプル数が `kMaxSamples`（16）に達している場合は最弱サンプルを coverage が上回る場合のみ置換する。
- `CryptoPixel::getSamplesForRank(rank)` は rank ごとに 2 本のサンプルを返す。rank が負なら空、末尾 rank は 1 本だけになり得る。
- `CryptoManifest::addEntry` は空名を無視し、同一名および同一ハッシュの重複登録を拒否する。`findName` は未登録ハッシュに空文字を返し、`findHash` は未登録名でも名前ハッシュ（決定的値）を返す。空名のみ 0 を返す。
- `CryptoImage::resize` は 0 以下の寸法で失敗し、画像を 0×0 にクリアする。`fromIdBuffer` / `fromIdCoverageBuffers` は必要数に満たないバッファを読み飛ばす。ID が 0 以下・非有限のピクセルのサンプルは作られない。
- `CryptoImage::generateRankLayers` は負 rank でゼロ平面を返し、rank 内の第 2 識別子は `outId1` / `outCoverage1` へ出る。
- `CryptoImage::maxRank` はサンプル数が最大のピクセルの値から `(n + 1) / 2` で求める。公開バッファ API は 1 ピクセルにつき ID 1 本しか供給しないため、実利用で rank が 2 を超えることは稀である。

## 追加したテスト

`tests/ArtifactCore/CryptomatteContractTest.cpp` を独立ターゲットとして登録した（`ArtifactCoreCryptomatteContractTest`、ラベル `image;cryptomatte;id;coverage;cpu;contract`）。名前ハッシュの決定性と 24 bit 収まり、float エンコード往復、`floatToId` の非有限・非正値拒否、サンプルの coverage 降順、同一 ID の coverage 合算と 1.0 クランプ、非有限・非正値寄与の拒否、サンプル上限到達時の最弱置換、`clear`、負 rank、manifest の登録・重複排除・未登録ハッシュの不在報告・未登録名の決定的ハッシュ、metadata 生成、画像の resize 拒否、バッファ不足・負寸法の無視、Object／Material バッファの同一識別子空間、coverage バッファの値保持、`maxRank`、rank 層の第 2 識別子、負 rank のゼロ平面を確認する。

`ArtifactCore` のみにリンクする純粋CPU ターゲットで、GPU・外部ファイル・ネットワークには依存しない。ビルド・テスト実行は行っていない。

修正した `CryptoPixel::addSample` / `CryptoImage::fromIdBuffer` の期待値は、修正後実装を忠実に再現したシミュレーションで全件一致することを確認済みである。コンパイルと実行による検証は未実施である。

## 実装側の指摘と対応（2026-11-08 修正済み）

精読により 2 件の実バグを確認した。ユーザー指示に基づき `ArtifactCore`（子リポジトリ）を修正済みである。

1. **`CryptoPixel::addSample` の重複 ID 合算時に再ソートしない（修正済み）。** `samples_[i].coverage` を合算して `return` するため、サンプル配列が coverage 降順のまま保たず、rank 取得（`getSamplesForRank` は配列順を前提）の並びが崩れていた。合算後に `sort()` を呼ぶよう修正し、降順が維持されるようにした。テスト `AccumulatedCoverageKeepsTheSamplePairInDescendingOrder` は当初の characterization（実装順を期待値）から、正仕様（降順を維持）を期待値とする形へ更新した。

2. **`CryptoImage::fromIdBuffer` のバッファ長判定で符号付き整数の乗算が起こる（修正済み）。** `static_cast<std::size_t>(width) * height` は `height` が負のとき符号付きオーバーフローを起こし、判定が未定義動作となっていた。`resize` を先に呼んで寸法検証を済ませ、その後で両オペランドを `size_t` にしてピクセル数を計算する形へ変更した。`fromIdCoverageBuffers` も同一の構造に揃えた。**有効な寸法での観測可能な挙動は変更前と同一**（バッファ不足時も `resize` が先に走るため画像はリサイズ済みのまま）で、変わるのは負寸法時の未定義動作が除去された点だけである。テスト `NegativeDimensionsAreRejectedBeforeAnyBufferIsRead` は安全側の挙動（画像が 0×0 にクリアされ、サンプルが作られない）を固定する。

## 残る制約

3. **`CryptoExrWriter` は OIIO 依存のため本テスト対象から除外した。** `Image.Cryptomatte.ExrWriter` は `OpenImageIO` を include するため、`ArtifactCore` へリンクする別ターゲットからの import 経路を追加の検証が要る。EXR 書き込みの契約（channel 名、metadata prefix、`ranks` クランプ）は未テストのままである。

4. **`CryptoImage` は 1 ピクセルにつき ID 1 本の API しか公開していない。** `CryptoPixel` は最大 16 サンプルを保持でき、`maxRank` と rank 層生成もその前提で実装されているが、`CryptoImage` 側に複数 ID を 1 ピクセルへ入れる経路がない。Cryptomatte の本来の目的であるアンチエイリアス対応（1 ピクセル複数 ID）は現状 `CryptoImage` 経由では表現できない。マイルストーンの「複数ID／coverage を生成する native Cryptomatte pass」が未完である理由と整合する。API 追加を伴うため今回の範囲外とした。
