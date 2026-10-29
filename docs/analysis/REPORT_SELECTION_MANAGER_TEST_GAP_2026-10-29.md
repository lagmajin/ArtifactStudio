# SelectionManager Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`UI.SelectionManager` は現在の Composition、選択レイヤー、選択アセットを保持し、状態変更を EventBus に公開する。テスト検索では、このクラス固有のイベントと状態に対する直接テストが見当たらなかった。

## 追加したテスト

`tests/ArtifactCore/SelectionManagerContractTest.cpp` を独立ターゲットに登録した。Composition ID の重複設定抑制、レイヤー選択の順序保持と clear 通知、アセット選択の置換と通知内容を EventBus subscriber から検証する。グローバル singleton を使うため、それぞれ固有 ID を利用し、実行は単一テストプロセス内で直列に行う設定とする。ビルド・テスト実行は行っていない。
