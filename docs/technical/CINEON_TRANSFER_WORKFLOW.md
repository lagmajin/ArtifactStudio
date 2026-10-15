# Cineon / DPX 転送関数ワークフロー

**最終更新:** 2026-10-15

## コア契約

`ArtifactCore` が Cineon の転送関数を提供し、画像入出力や表示処理は同じ関数を利用する。内部合成値は `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md` に従って scene-linear とする。Cineon は RGB の転送関数であり、色域・原色変換は別途入力色空間に基づいて行う。

- `ColorTransferFunction::decode(code, TransferFunction::Cineon)` は正規化コード値を scene-linear に変換する。
- `ColorTransferFunction::encode(linear, TransferFunction::Cineon)` は scene-linear を正規化 Cineon コード値に変換する。
- DPX 読み込みの `KodakLog` 解釈は素材レイヤーから ArtifactCore の Cineon 変換へ接続する。自動判定は明示されたメタデータに限り、素材ごとの解釈 override を優先する。
- `ImageExportOptions` は `inputTransferFunction` と `outputTransferFunction` を両方指定した場合に、RGB を入力 transfer から linear 経由で出力 transfer へ変換する。アルファは変更しない。未指定なら既存の画素値を保持する。
- 出力時は `TransferFunction` メタデータに選択名を記録する。DPX Cineon 出力では標準の `dpx:Transfer=Logarithmic` と `oiio:ColorSpace=KodakLog` も記録し、読み込み側で Cineon と判定できるようにする。
- `ImageExportOptions::bitsPerSample` はコンテナ型より低い整数ビット深度を指定する。DPX は `dataType=UINT16` と組み合わせ、10 / 12 / 16 bit を指定できる。packing 自体は OIIO DPX writer の責務とする。

```cpp
ImageExportOptions options;
options.inputTransferFunction = QStringLiteral("linear");
options.outputTransferFunction = QStringLiteral("cineon");
options.dataType = OIIO::TypeDesc::UINT16;
options.bitsPerSample = 10; // 12 / 16 も指定可能
```

呼び出し側は、入力画素が実際に linear であることと、用途に合う出力 bit depth を選ぶことに責任を持つ。`ImageExportOptions::colorSpace` は色空間メタデータであり、transfer 関数選択や gamut 変換を代行しない。

## 範囲と未確認事項

- 本 API は転送関数のみを変換する。入力と出力の primaries / gamut 変換、display/view transform は行わない。
- 専用 `ArtifactCoreImageIoContractTest` で OIIO DPX writer の実ファイルを 10 / 12 / 16 bit で往復し、transfer header、Cineon code values、alpha、`ImageImporter` の bit depth / Cineon 判定を確認する。
- エクスポート変換を有効にする場合、入力・出力 transfer 名の両方を明示する。不明な名前はエラーとして返す。
- 画像の読み込み往復と Cineon エンコードは、`ARTIFACT_ENABLE_IMAGE_IO_TEST=ON` で独立実行ファイル `ArtifactCoreImageIoContractTest` を登録できる。ビルド時はこの target のみを指定し、CTest では同名テストだけを選べる。これは `Artifact` アプリ exe をテストプロセスとして起動しない。
