# SelectionManager Test Coverage Gap

**最終更新:** 2026-10-31

## 調査

`UI.SelectionManager` は現在の Composition、選択レイヤー、選択アセットを保持し、状態変更を EventBus に公開する。テスト検索では、このクラス固有のイベントと状態に対する直接テストが見当たらなかった。

## 追加したテスト

`tests/ArtifactCore/SelectionManagerContractTest.cpp` を独立ターゲットに登録した。Composition ID の重複設定抑制、レイヤー選択の順序保持と clear 通知、アセット選択の置換と通知内容を EventBus subscriber から検証する。グローバル singleton を使うため、それぞれ固有 ID を利用し、実行は単一テストプロセス内で直列に行う設定とする。ビルド・テスト実行は行っていない。

## 追加拡充（2026-10-31）

レイヤー選択では、選択後に呼び出し元の vector を変更しても Manager の保持状態と既発行イベントの選択内容が変わらないことを追加検査する。これは `selectLayers(const std::vector<Id>&)` が入力を内部状態およびイベントへ値コピーする境界を固定する。ビルド・テストは未実行。

## 追加拡充（2026-10-31）

空のレイヤー選択を複数回 clear した場合も各呼び出しで空状態イベントが発行される契約、`selectLayers({})` で選択を空へ置き換える契約、アセット選択へ空 vector を渡すと既存選択が消えて空状態イベントが発行される契約を追加した。SelectionManager の現行実装はいずれも無条件にイベントを発行するため、これらは現行動作のcharacterizationである。ビルド・テストは未実行。
