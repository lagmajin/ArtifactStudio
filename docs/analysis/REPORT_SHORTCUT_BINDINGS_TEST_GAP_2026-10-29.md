# ShortcutBindings Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`UI.ShortcutBindings` は既定ショートカット、ユーザー override、KeyEvent matching、PortableText JSON 保存・復元、変更 listener を管理する。直接利用するテストが見当たらなかった。

## 追加したテスト

`tests/ArtifactCore/ShortcutBindingsContractTest.cpp` を独立ターゲットとして追加した。default と override の一致、auto-repeat とキー照合、JSON format と portable shortcut の往復、未知 ID だけの JSON による状態保持、listener の解除、空 override による binding 無効化を確認する。Singleton を使うテストのため CTest 実行は直列指定とし、各ケースの最後に既定値へ戻す。ビルド・テスト実行は行っていない。
