# InputBinding Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`Input.Operator` の `InputBinding` は KeyDown と key code、必須 modifier、禁止 modifier を照合する。既存テスト群には `InputBinding::matches()` の直接テストが見つからなかった。Chord の追加キー列には設定 API が見当たらないため、今回のテストは現行公開 API で構成できる単一キー照合に限定した。

## 追加したテスト

`tests/ArtifactCore/InputBindingContractTest.cpp` を専用ターゲットとして登録した。KeyDown 以外や異なるキーの拒否、必須／禁止 modifier、左右 Ctrl の区別、他の許可 modifier の共存を確認する。Chord キー列の設定経路については別途 API 調査が必要であり、ここでは推測で公開 API を追加していない。ビルド・テスト実行は行っていない。
