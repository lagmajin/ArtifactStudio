# InputBinding Test Coverage Gap

**最終更新:** 2026-10-31

## 調査

`Input.Operator` の `InputBinding` は KeyDown と key code、必須 modifier、禁止 modifier を照合する。既存テスト群には `InputBinding::matches()` の直接テストが見つからなかった。Chord の追加キー列には設定 API が見当たらないため、今回のテストは現行公開 API で構成できる単一キー照合に限定した。

## 追加したテスト

`tests/ArtifactCore/InputBindingContractTest.cpp` を専用ターゲットとして登録した。KeyDown 以外や異なるキーの拒否、必須／禁止 modifier、左右 Ctrl の区別、他の許可 modifier の共存を確認する。Chord キー列の設定経路については別途 API 調査が必要であり、ここでは推測で公開 API を追加していない。ビルド・テスト実行は行っていない。

## KeyMap modifier side coverage（2026-10-31）

KeyMapでは同じキーの `LCtrl` binding と `RCtrl` binding が別の完全一致キーとして同時登録され、それぞれの検索結果も分離される契約を追加した。既存テストは異なるmodifier maskの独立indexを確認していたが、左右同種modifierの組み合わせは未確認だった。ビルド・テストは未実行。
