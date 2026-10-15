# Standalone Test Image Generator

**最終更新:** 2026-10-15

カラーバー、カラーホイール、グレースケールグラデーション、チェッカーボードを PNG 画像として生成する独立ツールです。ArtifactStudio の exe、CMake、テスト実行経路には接続しません。

## 実行

Python 3 と Pillow が必要です。依存を利用できる Python 環境から、リポジトリルートで実行します。

```powershell
python tests/test_image_generator.py
```

既定では `test-images/` に 1920×1080 の PNG を4枚出力します。出力先を変える場合:

```powershell
python tests/test_image_generator.py --pattern wheel --width 1024 --height 1024 --output-dir C:/temp/test-images
```

`--pattern` は `bars`、`wheel`、`gradient`、`checkerboard`、`all` を指定できます。チェッカーのセル幅は `--cell-size` で変更できます。

このツールはソースと画像ファイルを生成するだけで、アプリケーションの起動、ビルド、テスト登録、既存ファイルの変更は行いません。
