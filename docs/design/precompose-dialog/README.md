# Pre-compose dialog design reference

**最終更新:** 2026-09-09

Version 2 は実装参照として採用し、Version 1 は比較用に残す。

- `precompose-dialog-concept-2026-09-09.png`: Version 1。現行項目のみの基準案。
- `precompose-dialog-v2-preview-2026-09-09.png`: Version 2・採用。プリコンポーズ予定の静止サムネイルと概要を加えた案。

参照対象は、現行 `PrecomposeDialog` の名前、選択レイヤー一覧、2つの配置方法、3つのチェック項目、Windows順の確定／キャンセル操作の配置・密度・配色に限定する。画像を根拠に新しい処理、選択方式、signal／slotを追加しない。

Version 2 のサムネイルは実画像を生成せず、選択レイヤー名から構成を示す軽量 owner-draw とする。毎フレーム生成せず、選択内容変更時だけ更新する。
