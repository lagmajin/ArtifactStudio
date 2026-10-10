# マイルストーン: ArtifactScript 協調型コルーチン

**最終更新:** 2026-10-09
**ステータス:** Not Started
**優先度:** Medium
**関連:** `docs/planned/MILESTONE_ARTIFACTSCRIPT_LANGUAGE_EVOLUTION_2026-08-21.md`, `docs/planned/MILESTONE_ARTIFACTSCRIPT_ENGINE_2026-07-21.md`

## 目的

ArtifactScript から複数フレームにまたがる処理を書けるようにする。OS スレッドやファイバーは使わず、再生フレーム境界で停止・再開する協調型コルーチンを導入する。

## 現状と設計制約

- `ArtifactScriptLayerRuntime::evaluateFrame()` はフレーム番号を受け取り、同一フレームの重複実行を抑止する。
- `ArtifactAbstractComposition` はレイヤーの `OnUpdate` を Composition のフレーム評価経路で呼ぶ。この経路は再生だけでなく、任意フレームの評価・スクラブでも使われる。
- 評価器は AST を再帰的に実行し、ローカル変数と呼び出しフレームはメソッド終了時に破棄する。現状のまま `yield` を追加しても、停止位置やローカル変数を保持できない。
- `ArtifactPlaybackService` が Transport の状態と実行を所有し、既存の Playback 状態／フレーム通知は EventBus を通る。新規 Qt signal/slot や UI ローカル timer は追加しない。

したがって、コルーチンの待機タスクを `evaluateFrame()` やレンダー評価から進めてはならない。起動要求は `OnUpdate` から出せるが、再生中の通常フレーム進行であることを実行コンテキストで確認できる場合だけ受理する。停止中、スクラブ、フレームジャンプでは起動・再開しない。

## 提案するスクリプト API

```csharp
void OnUpdate() {
    if (!isCoroutineRunning("FadeIn")) {
        startCoroutine("FadeIn");
    }
}

void FadeIn() {
    this.opacity = 0.0;
    yield return waitFrames(8);
    this.opacity = 1.0;
}
```

- `startCoroutine(methodName)` はコルーチン対象メソッドを開始し、開始できたかを `bool` で返す。同じメソッド名の実行中タスクは重複起動しない。
- `stopCoroutine(methodName)` は対象タスクをキャンセルする。`isCoroutineRunning(methodName)` は実行状態を返す。
- 初版の yield 値は `waitFrames(count)` のみ。count は正の整数に限り、待機後は指定フレーム数が経過した再生更新で再開する。
- `yield return` を含むメソッドのみコルーチンとして呼び出せる。通常の同期メソッドから yield した場合はパース／検証エラーにする。
- `return` またはメソッド末尾で正常終了する。例外・スクリプトエラーは該当タスクを終了し、既存診断経路へ一度だけ報告する。

## 実行と状態の契約

- タスクはレイヤーの `ArtifactScriptLayerRuntime` が所有し、再生の各連続フレームで最大一度だけ進める。
- 各タスクは AST 上の再開位置、呼び出し階層、ローカル値、待機フレームを保持する。継続中に評価 AST を破棄・置換してはならない。
- フレーム番号が連続しない場合はタスクを進めず、後続の連続再生を開始できる状態へ戻す。任意の seek 先で途中状態を推測して再開しない。
- `OnDisable` / `OnDestroy`、スクリプト解除、スクリプト定義のホットリロード、Composition の切替では実行中タスクをキャンセルする。
- コルーチンの実行位置はプロジェクト保存データへ含めない。保存・再読込後はタスクを再開せず、通常の `OnCreate` / `OnStart` ライフサイクルから始める。
- 再生フレーム処理は bounded とし、1タスクが1回の更新で実行できる命令数に上限を設ける。待機前の無限ループで再生を止めない。
- コルーチンは再生タスクであり、レンダー／合成／フレーム評価のホットパスから直接スケジュールしない。再生タスクを進める境界と `OnUpdate` の起動要求を実装段階で明示的に接続する。

## 実装段階

1. AST と評価器に中断可能な実行フレームを導入し、同期実行の既存意味を保つ。
2. Core の `ArtifactScriptLayerRuntime` に bounded なタスク保管、開始・停止・照会、連続フレーム再開を追加する。
3. Artifact の再生経路からのみ Core ランタイムへ再生フレーム進行を伝える。Playback Service の既存状態／フレーム経路を再利用し、Transport の所有権を変えない。
4. Parser、Core runtime、Artifact 統合の契約を静的・実行時に検証し、ビルド／テストはユーザーの明示指示後に行う。

## 初版の対象外

- ファイバー、OS スレッド、並列実行、`async` / `await`、外部 I/O 待ち
- 秒・実時間待ち、フレーム飛び越し時の自動 fast-forward
- コルーチン状態の保存、ホットリロードをまたぐ継続
- 任意のクロススレッド host callback

## 受け入れ条件

- `yield return waitFrames(n)` の前後で副作用が一度ずつ実行され、指定再生フレームで再開する。
- ローカル変数とネストした method call が yield をまたいで保持される。
- 同一フレームの重複評価、停止中評価、スクラブ、逆再生、seek、loop 境界でタスクが誤って進まない。
- 重複起動、明示キャンセル、正常終了、エラー終了、無効化、破棄、hot reload の各状態が定義どおりになる。
- タスク命令数上限を超えた場合に、その更新を有限時間で終了して診断できる。
- 通常の `OnUpdate` 同期実行、Undo 経路、レンダリング／合成ホットパスに非同期動作を持ち込まない。
