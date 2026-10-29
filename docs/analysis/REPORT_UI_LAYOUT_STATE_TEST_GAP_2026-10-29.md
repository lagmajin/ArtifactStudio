# UiLayoutState Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`UI.Layout.State` は geometry、QMainWindow state、ADS dock state を opaque bytes として JSON / QSettings / FastSettingsStore に保存し、ISerializable と schema migration に接続する。テスト検索では `UiLayoutState` の直接テストが見当たらなかった。

## 追加したテスト

`tests/ArtifactCore/UiLayoutStateContractTest.cpp` を独立ターゲットとして登録した。任意バイナリの JSON 往復、欠落フィールドの既定値、QSettings prefix 間の分離、ISerializable deserialize による完全置換を確認する。テストは外部 UI や実アプリ設定を変更せず、QTemporaryDir 内の一時 INI に限定する。ビルド・テスト実行は行っていない。
