# 設計レビュー: 外部スクリプト API 強化（P2 大型項目）

**最終更新:** 2026-11-17
**ステータス:** 設計レビュー用提案（未承認・未実装）
**対象:** C#（Roslyn/CSX） / Python（`artifact` モジュール） / ArtifactScript（独自言語）

## 背景

2026-11-17 の静的監査で、3 つのスクリプト基盤について以下の共通ギャップが判明した。

- **世界への窓口が狭い**: C# は `Time` / `DeltaTime` / `Frame` のみ。ArtifactScript は composition / property / keyframe はあるが再生 Transport とエフェクトが未接続。Python は登録関数は多いが `WorkspaceAutomation` の 100 超の操作の一部しか露出していない。
- **戻り値契約が不安定**: Python は `bool` / `"OK"` / `"ERROR: ..."` / `{"success":false}` が混在。C# は `exception.ToString()` を文字列で返すのみ（2026-11-17 で構造化 JSON を追加済み）。
- **UI 接続が未完**: C++ 側の `CSharpScriptEngine` には反復セッション API が揃っているが、UI は `executeScriptFile()` しか呼んでいない。
- **実機未検証**: hostfxr 有効環境、Roslyn の実際の参照解決、session のリロード／停止挙動は未検証。

P0 / P1 の小さな修正（参照解決、構造化エラー、AE ラッパー再帰、数値プロパティ、文字列／配列ユーティリティ）は 2026-11-17 に実装済み。本文書は残る P2 の設計レビュー事項を扱う。

## 横断制約（AGENTS.md 準拠）

- **新規 signal/slot 接続は絶対禁止**。既存の service / command / event bus 経路を再利用する。
- **再生 Transport の所有権は `ArtifactPlaybackService` に統一**。Composition / Engine / 個別 Controller への直接操作は禁止。スクリプトは read のみ、操作は service API 経由。
- **ホットパスでの重いアロケーション禁止**。フレームごとの文字列構築・コンテナ拡張・ログ整形は遅延評価／category フラグにする。
- **新規 `.ixx` / `.cppm` は再スキャン要因**。既存ファイルへの追記を優先し、公開宣言に他モジュール型を出さずコールバック／前方宣言で隔離する。
- **QtCSS / `QColorDialog` / `QImage` / `QPainter::CompositionMode` の新規採用禁止**。
- **ビルド・テストはユーザー明示指示まで禁止**。本文書の受入条件は実機検証を含むが、実行は別途承認が必要。
- 独自コンテナ（`NamedVector` / `ArtifactArray` / `HashMap` 等）を優先。既存の標準コンテナを機械的に置換しない。

---

## 項目 1: C# `ArtifactApi` facade

### 目的
C# スクリプトから composition / layer / selection / ログへアクセスできる窓口を、1 つの facade オブジェクトとして提供する。

### 現状
`SessionGlobals` は `Time` / `DeltaTime` / `Frame` のみ。`ScriptOptions` に `AddReferences` は 2026-11-17 に追加済みだが、独自型への参照はまだ無い。

### 提案 API（C# 側・概念形）

```csharp
public sealed class SessionGlobals
{
    public double Time { get; set; }
    public double DeltaTime { get; set; }
    public ulong Frame { get; set; }
    public ArtifactApi Artifact { get; } = new();
}

public sealed class ArtifactApi
{
    public void Log(string message);
    public string[] GetLayerNames();
    public string? GetSelectedLayerName();
    public string? GetProperty(string layer, string path);
    public bool SetProperty(string layer, string path, string value);
    public string GetCompositionId();
}
```

`Artifact.SetProperty` は既存 command / service 経路へ橋渡しし、直接レイヤーを書き換えない。

### 実装箇所
- `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactApi.cs`（新規・C# のみ。C++ module 圏外なので再スキャン要因にならない）
- `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`（`SessionGlobals` に `Artifact` を追加）
- `ArtifactCore/include/Script/CSharpScriptEngine.ixx` / `ArtifactCore/src/Script/CSharpScriptEngine.cpp`（C# → C++ callback の登録 API。既存 `invokeUtf8` と同じ UTF-8 3 引数契約を逆方向に使う）

### C# → C++ callback の方式
`UnmanagedCallersOnly` な delegate を C# 側で公開し、C++ 側 `get_function_pointer` で解決して保持する。新規 signal/slot は作らず、呼び出し先は既存の `WorkspaceAutomation` / `ArtifactProjectService` の command 経路に限定する。

### リスク / 未決事項
- **delegate の寿命**: host 終了時に C++ が保持する関数ポインタを無効化する順序を定義する必要がある。
- **複数セッション**: 現状 `_session` / `_globals` は static 1 つ。レイヤー単位の同時実行を Want するなら session ID 付き辞書が必要（項目 4 で言及）。
- **例外と停止ポリシー**: `OnEnable` / `OnDisable` 相当のライフサイクルフックを追加する前に、例外時の挙動（セッション継続／破棄）を定義する。
- **スレッド**: Roslyn 評価が UI スレッド外で走る場合の service 呼出しをどう揃えるか未決定。

### 受入条件（未実行）
1. `Artifact.Log("x")` が C++ の `OutputCallback` に到達する。
2. `Artifact.GetLayerNames()` が composition のレイヤー名と一致する。
3. callback が command 経路を通り、Undo / dirty 状態が既存経路と同じになる。
4. host 終了後に delegate を呼ばないことを確認する。

---

## 項目 2: Python 型付き object model と統一結果型

### 目的
`app` / `project` / `selected_layers` を ID 保持プロキシとして公開し、戻り値を `ApiResult(ok, value, errorCode, diagnostics)` に統一する。

### 現状
- `Artifact/src/Export/Python/ArtifactPythonAPI.cpp` は登録関数方式。型付き object export は未実装。
- 戻り値が混在（上記背景）。
- `WorkspaceAutomation.ixx` にキーフレーム／レンダーキュー／選択／マーカー等の操作が既にあるが、Python からは一部のみ露出。

### 提案 API（概念形）

```python
artifact.workspace_snapshot() -> dict
artifact.get_active_composition() -> CompositionProxy | None
artifact.selected_layers -> list[LayerProxy]
artifact.set_keyframe(layer, path, frame, value, interp=None) -> ApiResult
artifact.get_keyframes(layer, path) -> list[dict]
artifact.render_job_status(index) -> dict
artifact.cancel_render_queue(index) -> ApiResult
```

`ApiResult` は既存マクロ互換のため **opt-in フラグ**で導入し、既定は現行の文字列／bool 戻り値を維持する。

### 実装箇所
- `Artifact/src/Export/Python/ArtifactPythonAPI.cpp`（既存 `register*API` に追記。新規 `.ixx` は作らない）
- `ArtifactCore/src/Script/Python/CorePythonAPI.cpp`（`workspace_snapshot` 等、`_workspace_invoke` の薄いラッパー）
- `ArtifactCore/src/Script/Python/PythonEngine.cpp`（`registerFunction` のラッパーで `ApiResult` 変換を opt-in 化）

### リスク / 未決事項
- **Proxy の寿命**: ID 保持プロキシがレイヤー削除後も残る場合の挙動（ stale 検出）を定義する。
- **QObject 所有権**: Python 側から PythonEngine 実行環境の外へ QObject を出さない。`QAction` 等は C++ 側で寿命管理する（menu.py ローダー項目と共通の懸念）。
- **JSON 境界**: 既存 `_workspace_invoke` の引数アンラップが不揃いなので、型付き JSON スキーマを先に定義する。
- **ドキュメント**: `MILESTONE_EXTENDSCRIPT_STYLE_SCRIPT_RUNTIME_2026-04-06.md` の Phase 2〜5 と重複するため、実装前にマイルストーンの線引きを更新する。

### 受入条件（未実行）
1. `workspace_snapshot()` が capability 発見（CLI マイルストーン P0）に使える構造を返す。
2. `set_keyframe` が既存 `WorkspaceAutomation::setKeyframe` と同じ補間名・時刻ドメインを使う。
3. opt-in 無効時は既存マクロ（`ae_utility_pack`）が壊れない。
4. stale proxy が明示的なエラーを返し、クラッシュしない。

---

## 項目 3: ArtifactScript 再生 Transport 読み取り API

### 目的
レイヤースクリプトから再生状態を **read-only** で参照できるようにする。操作（play/pause/stop/seek）はスクリプトからは直接行わない。

### 現状
`ArtifactScriptCompositionApi` に Transport 系 callback は無い。Python 側には `_native_play` / `_native_pause` / `_native_stop` / `_native_current_frame` があるが、ArtifactScript には無い。

### 提案 API

```
isPlaying() -> bool
isPaused() -> bool
getPlaybackState() -> string   // "stopped" | "playing" | "paused"
getCurrentFrame() -> int64
getPlaybackRange() -> [int64, int64]
```

### 実装箇所
- `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx`（`ArtifactScriptCompositionApi` に任意コールバック追加。未設定なら登録しない既存パターン）
- `Artifact/src/Composition/ArtifactAbstractComposition.cpp`（`installCompositionScriptApi()` で `ArtifactPlaybackService` の状態を読む callback を注入）

### 設計上の決定事項
- **読み取りのみ**。Transport 操作 API をスクリプトへ露出する場合は、別途 `ArtifactPlaybackService` の公開 API を呼ぶ command を定義し、AGENTS.md の「UI から直接送らない」ルールをスクリプトにも適用する。
- **EventBus の状態を読むのみ**で、新規 signal/slot は作らない。
- ホットパス（毎フレームの `isPlaying()` 等）は文字列を構築しない。`getPlaybackState()` は 3 つの固定文字列リテラルのみを返す。

### 受入条件（未実行）
1. Play / Pause / Stop 後に `getPlaybackState()` が service state と一致する。
2. seek 後に `getCurrentFrame()` が Composition / Timeline / Viewport と一致する。
3. スクリプトから Transport 操作ができないことを静的レビューで確認する。

---

## 項目 4: ArtifactScript 協調型コルーチン

### 目的
`yield return waitFrames(n)` で複数フレームにまたがるスクリプト処理を書けるようにする。

### 現状
`docs/planned/MILESTONE_ARTIFACTSCRIPT_COROUTINES_2026-10-09.md` は **Not Started**。実装なし。

### 提案（第 1 スライス）
- `waitFrames(n)` と `startCoroutine` / `stopCoroutine` / `isCoroutineRunning` の最小形。
- 再開位置・呼出階層・ローカル値・待機フレームを保持する実行コンテキストを `ArtifactScriptLayerRuntime` 内に **bounded 固定容量**で保管。
- 連続フレームのみ再開。seek / スクラブ / 逆再生では進めない。
- `OnDisable` / `OnDestroy` / ホットリロードでキャンセル。命令数上限を設ける。
- 保存データにはコルーチン状態を含めない。
- 進行は `ArtifactPlaybackService` の既存連続フレーム経路のみ。新規 signal/slot・UI timer は禁止。

### 実装箇所
- `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cpp`（AST / 評価器の中断・再開）
- `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx`（`ArtifactScriptLayerRuntime` にタスク保管。既存の任意コールバック／値型パターンを踏襲）
- `Artifact/src/Composition/ArtifactAbstractComposition.cpp`（Artifact 側の再生フレーム経路との接続）

### リスク / 未決事項
- AST 再開位置の表現方法（PC 相当の整数か、ノード-pointer か）で所有権とホットリロード安全性が変わる。
- ローカル値の保持を `ArtifactScriptValue` のコピーにするとアロケーションが増える。参照／スロット方式を検討する。
- 命令数上限とタスク上限の具体的な値は未決定。
- `yield` を新規キーワードにする場合、既存スクリプトとの予約語衝突を確認する（ショートカットキーではないが命名規約として）。

### 受入条件（未実行）
1. `waitFrames(3)` が 3 フレーム後にのみ再開する。
2. seek では再開しない。
3. ホットリロードでタスクがキャンセルされ、旧定義の状態が残らない。
4. タスク上限・命令数上限を超えると診断が出てクラッシュしない。

---

## 推奨実施順

1. **項目 3**（Transport read-only）: 他基盤の設計に影響せず、既存パターンだけで実装できる。
2. **項目 1**（C# facade）: delegate 寿命と停止ポリシーの定義が先決。設計確定後に実装。
3. **項目 2**（Python object model）: CLI マイルストーンとの線引き整理が必要。
4. **項目 4**（コルーチン）: 評価器の根幹に触れるため最後。命令数・アロケーション設計を先にレビューする。

いずれも実装前に本レビューの承認と、ビルド／テスト実行の明示指示が必要である。
