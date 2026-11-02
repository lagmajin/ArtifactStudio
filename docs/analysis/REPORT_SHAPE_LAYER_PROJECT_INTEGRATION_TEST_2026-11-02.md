# Shape Layer Project Integration Test

**最終更新:** 2026-11-02

## 調査結果

- `ShapeLayerAnimationPersistenceContractTest` は Shape Layer JSON 往復後の頂点キーフレーム評価を検証する。
- `ShapeLayerContentManagementContractTest` はコンテンツ、スタック、スタイルのスナップショット復元を検証する。
- `ArtifactProjectRoundTripTest` はプロジェクト保存・再読込を通すが、Shape Layer の複合編集状態は対象にしていない。
- Shape の GPU 画像テストは別経路にあり、プロジェクト全体の保存・再読込と同じシナリオではない。

## 実装

`ShapeLayerProjectIntegrationTest.cpp` は本番の `ArtifactProjectManager` でプロジェクトと Composition を作成し、Shape Layer を追加する。頂点パスのアニメーション、複数 Shape Content、3-stop gradient、dashed/wavy stroke、Content transform、Trim Paths operator、ordered stack を設定してプロジェクトファイルに保存し、閉じてから再読込する。

再読込後にレイヤー型、キーフレームの開始・中間・終了評価、接線、コンテンツ順・選択状態、スタイル、変形、オペレーター、スタック、およびプロジェクト健全性を照合する。描画 pixel / GPU backend の検証は既存の Shape Visual / Render Queue テストに委ね、このテストではプロジェクト保存経路で編集状態が失われないかに集中する。

## 確認状況

テストターゲットを `ArtifactShapeLayerProjectIntegrationTest` として登録した。ユーザーの作業環境ルールに従い、ビルド、CMake、テストの実行は行っていない。実行結果は未確認。
