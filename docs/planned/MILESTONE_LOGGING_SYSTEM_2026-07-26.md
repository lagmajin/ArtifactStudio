# Logging System Expansion Milestone (2026-07-26)

**ステータス:** In Progress
**最終更新:** 2026-08-15

## 目的

既存の Qt ログ取り込みを維持しながら、詳細なカテゴリ制御、自動ファイル出力、VP／レンダリング経路向けの低アロケーション高速ログ経路を整備する。

## 実装範囲

- `ArtifactCore::Logger` に構造化カテゴリを追加する。
- 高頻度ログ用に固定長・事前確保のリングバッファを追加する。
- 高速経路では `QString`、`QDateTime`、ヒープ確保、ファイル I/O を発生させない。
- 高速ログは後段の drain 処理で通常 Logger に取り込めるようにする。
- 既存の `qDebug`／`qWarning` は互換入力として残し、段階的移行を可能にする。
- ファイル出力は既存の自動出力を拡張し、将来 text／JSONL、世代管理、カテゴリ別閾値を追加できる設計にする。

## 非対象

- VP／D3D12 レンダリング本体の広範囲なログ置換。
- Qt のグローバルシグナルや新しい中央イベント配線。
- ビルド設定・CMake 再生成・サブモジュール変更。

## 段階

1. 基盤: `LogCategory`、固定長 `FastLogRecord`、bounded ring buffer、drop counter。**実装済み**
2. 変換: drain 時に通常 Logger／ファイル／Debug Console へ統合。**実装済み**
3. 設定: カテゴリ階層、レベル閾値、ファイルローテーション、JSONL 出力。**ファイル設定・ローテーション・JSONL は実装済み**
4. 移行: レンダリングの計測点から限定的に専用 API へ移行。**`ViewportTransformer` の Fit/Fill を移行済み**

Debug Console には Context とは別に Category フィルタを追加し、`Render.VP` などの高速ログカテゴリを保存済みプリセットから選択できる。

## 完了条件

- 高速 API がログ 1 件ごとのヒープ確保なしで呼び出せる。
- バッファ満杯時にレンダリングを停止せず、drop 件数を取得できる。
- 既存 Logger の UI／ファイル出力と互換性を保つ。
- VP／GPU 系カテゴリを個別に有効化・無効化できる。

## 現在の API

```cpp
Logger::instance()->tryFastLogFormat(
    LogLevel::Debug, LogCategory::RenderVP, frame,
    "layer=%u pass=%u", layerId, passId);
```

```cpp
Logger::instance()->setLogFileFormat(LogFileFormat::JsonLines);
Logger::instance()->setMaxLogFileBytes(20ull * 1024ull * 1024ull);
Logger::instance()->setFileLoggingEnabled(true);
```

`tryFastLogFormat()` は固定長のスタックバッファで整形し、リングバッファへ格納する。ログが無効または満杯の場合は `false` を返す。`drainFastLogs()` は通常の Logger 経路へ戻すため、既存のファイル出力と Debug Console を再利用できる。

## 未確認事項

- 現在の Qt／MSVC モジュール構成で `std::array` と atomic メンバーの公開が問題ないかは未ビルド。
- drain の呼び出し周期はアプリのメインループまたは既存診断更新経路で決定する。

## Update 2026-08-15

- `ArtifactCore::Logger` に `LogCategory`、固定長 fast record、bounded ring、drop counter、`tryFastLogFormat()`、`drainFastLogs()`、カテゴリ有効／無効化が実装されている。`ViewportTransformer` が `RenderVP` の fast API を利用していることも確認できる。
- ファイル出力は有効化・パス・最大バイト数・JSON Lines形式・flush を持ち、ログファイル準備／ローテーション経路がある。Debug Console は Logger の drain を呼び、既存ログ表示へ統合されている。
- ただし現行コード検索では、カテゴリ階層の閾値設定、カテゴリ別の永続プリセット選択、fast path の全高頻度レンダー移行、drop／memory の専用UI表示は完了根拠が弱い。`qDebug`／`qWarning` の互換経路も多数残っている。
- よって現状は `Phase 1〜2 implemented / file format and rotation substantially implemented / Phase 4 selective migration and category UX partial / no-build runtime validation pending` と整理する。本文のモジュール構成未ビルド注意は維持する。
