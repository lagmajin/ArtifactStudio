# AI Debug Location / Execution Context Milestone

**ステータス:** Not Started

**最終更新:** 2026-09-04

## Goal

AI Debugger、Trace、Crash Diagnostics、App Debugger が、単なるログ行ではなく「どの呼び出し箇所で、どの実行文脈・対象・因果関係の下で起きたか」を構造化して追跡できる共通基盤を作る。

## Scope

### In

- `std::source_location` を保持する不変の `SourceLocation`。
- 呼び出し箇所を安定して集約する `callsiteId`。
- スレッド、frame、task、object、親 event を実行時に付与する `RuntimeContext`。
- `DebugLocation::current()` と、既存 API のデフォルト引数で呼び出し側を変更せず採取できる形。
- スレッドローカルの `DebugContext` と RAII scope による、レンダリング／非同期タスク境界での context 継承。
- `DiagnosticRecorder`、`TraceRecorder`、`DebugIdentity`、MCP の read-only debug tools が同じ event identity を参照できる JSON 契約。

### Out

- `ReactiveEvents` の変更。
- 新規 signal/slot やグローバル event wiring。
- 通常時の完全 stacktrace 採取。
- すべての値への常時 `lastWriter` 追跡。
- GPU backend／Diligent の低レベル render path の広域変更。

## Data Model

コンパイル時の呼び出し箇所と実行時状態は混在させない。

```cpp
struct SourceLocation {
    const char* file{};
    const char* function{};
    uint32_t line{};
    uint32_t column{};
    uint64_t callsiteId{};
    const char* module{};
};

struct RuntimeContext {
    uint32_t threadId{};
    uint64_t taskId{};
    uint64_t frame{};
    uint64_t objectId{};
    uint64_t parentEventId{};
    uintptr_t instruction{};
};

struct DebugLocation {
    SourceLocation source{};
    RuntimeContext runtime{};

    static DebugLocation current(
        std::source_location source = std::source_location::current(),
        const char* module = nullptr) noexcept;
};
```

`SourceLocation` の生成は constexpr/consteval に寄せてもよいが、`threadId`、`taskId`、`frame`、`instruction` は必ず実行時に `DebugContext::current()` から合成する。`module` はファイル名から推測せず、明示タグとする。

## Phases

### AIDLC-1: Source identity

- `SourceLocation` と `callsiteId` の canonical contract を定義する。
- file path はログ比較可能なプロジェクト相対表現を優先し、hash だけでなく file/line/column も保存する。
- `DebugLocation::current()` を default argument で利用できることを確認する。

### AIDLC-2: Runtime context and causality

- TLS ベースの `DebugContext`、`DebugContextScope` を追加する。
- frame、task、object、parent event を、既存の frame／task 境界から明示的に引き継ぐ。
- TBB の内部 ID に依存せず、必要ならアプリ側の stable task/trace ID を割り当てる。

### AIDLC-3: Recorder integration

- `DiagnosticRecorder` と `TraceRecorder` の event に `DebugLocation` と event ID を付与する。
- parent event ID により非同期処理を含む causal chain を復元できるようにする。
- instruction address と stacktrace は assertion、crash、明示 capture に限定する。

### AIDLC-4: AI debugger contract

- MCP／App Debugger が event ID、callsite、object、frame、parent chain を同じ JSON で返す。
- `debug.trace`、`debug.flow`、`debug.rootCause` が文字列解析ではなく構造化 identity を使う。
- `lastWriter` は高価な常時計測ではなく、選択した property／resource に対する opt-in provenance として設計する。

## Acceptance Criteria

- `SetPosition(value)` のような既存形式で、呼び出し元の source location を取得できる。
- 同一 callsite のログを `callsiteId` で横断検索でき、file/line/column も表示できる。
- render frame、worker task、対象 Layer/Composition を含む event を親子関係として復元できる。
- context の欠落はゼロ値ではなく `unknown` として JSON に明示される。
- 通常の debug event は stacktrace を採取せず、ホットパスの allocation とロックを増やさない。
- `ReactiveEvents`、新規 signal/slot、Diligent backend の変更なしに初期 vertical slice を閉じる。

## Related

- [MCP AI デバッグシステム](MILESTONE_MCP_AI_DEBUG_SYSTEM_2026-08-02.md)
- [Lightweight Tracer / Frame Timeline](MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md)
- [External Semantic Debugger / ArtifactDebugger.exe](MILESTONE_EXTERNAL_SEMANTIC_DEBUGGER_2026-09-02.md)
- [App Debugger / Frame Debug Bundle Persistence worklog](../worklog/APP_DEBUGGER_FRAME_DEBUG_BUNDLE_PERSISTENCE_2026-06-17.md)
