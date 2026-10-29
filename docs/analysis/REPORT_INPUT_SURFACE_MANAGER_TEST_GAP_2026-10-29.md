# InputSurfaceManager Test Coverage Gap

**最終更新:** 2026-10-29

## 調査

`Input.Surface.Manager` は Off / RealTimeCapture / StepEntry / Hybrid の状態、armed と各入力フラグ、capture pending、target/context、frame 値を管理し、状態差分を EventBus に通知する。直接的な `InputSurfaceManager` テストは既存テスト群から見つからなかった。

## 追加したテスト

`tests/ArtifactCore/InputSurfaceManagerContractTest.cpp` を独立契約テストとして登録した。リアルタイム capture の開始・commit、StepEntry と Hybrid のフラグ正規化、frame の負値 clamp、cancel/disarm、reset 時の既定値と状態通知を確認する。Singleton 状態を共有するため専用 executable とし、CTest 実行を直列化する。ビルド・テスト実行は行っていない。
