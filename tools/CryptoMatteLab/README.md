# CryptoMatteLab

**最終更新:** 2026-11-10

Cryptomatte multi-ID / coverage native pass のための独立 R&D ラボ。

このラボは製品コードを一切変更せず、出荷済みの Cryptomatte API を実測し、
それを **Cryptomatte 仕様の独立リファレンス実装** と突き合わせる。
両者が食い違う場合、その差異は「実装が仕様と一致していない」という
証拠になる（同一コードの言い換えではない）。

---

## 何を測るか

| # | プローブ | 測るもの |
|---|---------|---------|
| 1 | `spec-vectors` | 参照モデルが **公式 Cryptomatte テストベクタ** を再現するか（以降の比較の前提） |
| 2 | `id-encoding` | `CryptoSample::nameToFloat` が仕様の `uint32_to_float32` と一致するか |
| 3 | `manifest-identity` | manifest の ID 表現が仕様の hex（float ビット）と一致するか |
| 4 | `multi-id-image` | 1 ID/px の取り込みで `maxRank()` が 1 に留まるか、rank 1 が空のままか |
| 5 | `multi-id-pixel` | `CryptoPixel` の 16 サンプル容量と rank 取得が multi-ID に対応しているか |
| 6 | `coverage-sum` | coverage 合計が保存されるか（rank ベースリーダーの期待値） |
| 7 | `coverage-clamp` | 同一 ID の重複サンプルがマージ・clamp されるか |
| 8 | `coverage-saturation` | 16 サンプル超過時の打ち捨て挙動 |
| 9 | `manifest-format` | manifest のキー/値向きと ID 表現が仕様か |
| 10 | `metadata-key` | EXR メタデータキーの導出が仕様か |
| 11 | `exr-required-attributes` | 書き出した EXR に `name`/`hash`/`conversion` が実在するか（**実読取**） |
| 12 | `exr-metadata-key-length` | 書き出した EXR のメタデータキー長（**実読取**） |
| 13 | `exr-channel-naming` | 書き出した EXR のチャネル名が reference reader のパターンに一致するか（**実読取**） |
| 14 | `exr-channel-list` | 書き出した EXR の全チャネル名（**実読取**） |

### 実読取する部分

プローブ 11〜14 は、`CryptoExrWriter` で実際に EXR を書き出し、
**OIIO の `ImageInput` でヘッダを読み戻して**判定する。属性名・チャネル名を
コード読解ではなく実データから判定するので、推測が入らない。

---

## 使う

```bash
# プロジェクトルートで有効化
cmake -B build -DARTIFACT_BUILD_CRYPTOMATTE_LAB=ON
cmake --build build --target ArtifactCryptoMatteLab

# 実行
./build/bin/Debug/ArtifactCryptoMatteLab --out /tmp/cryptomatte-lab
```

オプション:

- `--out <dir>` : 出力先（既定 `cryptomatte-lab-out`）
- `--width N` / `--height N` : テスト画像サイズ（既定 64x64）

出力物:

- `cryptomatte-lab-report.json` : 全 findings（`id` / `severity` / `title` / `detail`）
- `cryptomatte-lab-sample.exr` : ヘッダ検査用のサンプル EXR
- stdout : 要約（severity ごとの finding 一覧）

`severity` は `info` / `warn` / `fail` の 3 種。
`fail` は仕様違反が確定したもの、`warn` は仕様との差異があるもの。

---

## オラクルの出どころ

参照モデル（`crypto_spec_reference.*`）は本リポジトリのコードから独立している。

- ハッシュ: MurmurHash3 x86 32-bit、seed 0
- 指数部補正: exponent が 0 または 255 のとき bit 23 を反転（denormal / Inf / NaN の回避）
- ID 変換: `uint32_to_float32`（32bit の **bit 再解釈**）
- manifest: `{"<name>": "<8桁hex>"}`
- メタデータキー: 型名 ID の 8 桁 hex から末尾 1 桁を除いた 7 桁

公式テストベクタ（`hello` / `cube` / `sphere` / `plane`）は
Psyop/Cryptomatte の `nuke/cryptomatte_utilities_tests.py` 由来。
検証に使う前に、まず `spec-vectors` で参照モデル自身が再現していることを確認する。

仕様: https://github.com/Psyop/Cryptomatte/blob/master/specification/cryptomatte_specification.pdf

---

## 設計上の注意

- **製品コード非変更**: ラボは `ArtifactCore` を link して出荷 API を呼ぶだけ。
  `ArtifactCore` / `Artifact` のソースは一切変更しない。
- ** submodule 不可侵**: `ArtifactCore` は子リポジトリなので、
  ラボから読み取るだけで編集しない。修正は別途、ユーザーの明示依頼がある場合のみ。
- **独立した TARGET**: `EXCLUDE_FROM_ALL` で既定ビルドに混ざらない。CMake 登録は
  ルート `CMakeLists.txt` の `ARTIFACT_BUILD_CRYPTOMATTE_LAB`（既定 OFF）経由。
- **ホットパス非該当**: オフライン計測ツールであり、レンダリング経路ではない。

---

## 未検証事項（2026-11-10 時点）

以下は現時点で **未実行**。ラボのコードと CMake 登録は作成済みだが、
ビルド・実行はしていない（AGENTS.md により明示指示が必要）。

- `ArtifactCryptoMatteLab` のビルド成功可否
- 各 finding の実際の severity 値
- `spec-vectors` の照合結果（参照モデルの自己検証）
- EXR ヘッダ実読取（プローブ 11〜14）の判定結果
  — 実装は済んでいるが、実行していないので結果は未確定
- MSVC で `ArtifactCore` のモジュールを外部ターゲットから import した際の
  IFC 解決。`tests/ArtifactCore/CryptomatteContractTest.cpp` が
  `ArtifactCore` を直接リンクして `import Image.Cryptomatte.Image;` を
  使っているのと同じ構成にしてあるが、`tests/` 配下と `tools/` 配下では
  コンパイル単位の解決順序が異なる可能性がある

なお `exr-required-attributes` / `exr-channel-naming` は、
2026-11-10 時点ではコード読解に基づく静的所見として記載していたが、
実装を見直す過程で OIIO `ImageInput` による実読取へ置き換え済み。
静的所見ではなく実データに基づく判定になった。

---

## 既知の所見（コード読解に基づく、未実行）

いずれも 2026-11-10 のコード読解で確認したもので、ラボ実行による確認は未実施。

1. **ID エンコーディングが仕様と異なる**
   `CryptoSample::nameToFloat` は MurmurHash3 の結果を 24bit に切り詰め、
   その整数を `static_cast<float>` している。仕様は指数部補正つきの 32bit hash を
   float の **bit 再解釈** する。同一名前で異なる値になり、仕様準拠リーダで名前解決できない。

2. **CryptoImage は 1 ID/px しか取り込めない**
   公開 API（`fromIdBuffer` / `fromIdCoverageBuffers`）は 1 ピクセル 1 ID 固定。
   `maxRank()` は常に 1、rank 1 以降は恒久的に空。
   一方 `CryptoPixel` は 16 サンプル保持でき rank 取得も 2 サンプルずつ返すため、
   **ストレージモデルは multi-ID 対応済み**で、制約は取り込みAPI側にある。

3. **manifest のキー/値向きが逆**
   出荷実装は `{hash: name}`、仕様は `{name: id_hex}`。

4. **`conversion` 属性を出力していない**
   仕様は `name` / `hash` / `conversion` を必須属性とするが、
   `CryptoExrWriter` は `name` / `hash` / `manifest` を出力するのみ。
   ラボは EXR ヘッダを読み戻して属性の実在を確認する（プローブ 11）。

5. **チャネル命名が reference reader に不合致**
   出荷は `CryptoObject00.R` 等の大文字1文字サフィックス、
   reference reader の正規表現は `.red` / `.r` を要求する。
   ラボは EXR ヘッダのチャネル名を実読取し、reference reader の
   パターンで実際に何本一致するかを数える（プローブ 13）。

---

## 関連

- `docs/planned/MILESTONE_CRYPTOMATTE_2026-08-01.md`
- `docs/analysis/REPORT_CRYPTOMATTE_TEST_GAP_2026-11-08.md`
- `tests/ArtifactCore/CryptomatteContractTest.cpp`（出荷側の契約テスト）
