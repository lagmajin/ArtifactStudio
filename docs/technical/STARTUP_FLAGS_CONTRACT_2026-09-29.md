# 起動設定（ArtifactStartup.json）仕様

**最終更新:** 2026-10-10

## 目的

IDE、Explorer、パッケージ済みの起動など、**環境変数を設定しにくい経路**から開発用スイッチを渡せるようにする。ArtifactStudio / Artifact / ArtifactCore のアプリ全体に共通する。

既存の `settings.cbor`（ユーザー設定、`QStandardPaths::AppDataLocation` 配下の CBOR バイナリ）とは**別系統**であり、本書の目的是起動時スイッチの入口を一本化することにある。ユーザー設定の保存先は本書で変更しない。

本書は [`HOT_PATH_RULES.md`](HOT_PATH_RULES.md) とともに適用する。矛盾する場合は HOT_PATH_RULES を優先する。

## 1. ファイル

- パス: `QCoreApplication::applicationDirPath()` と同階層の `ArtifactStartup.json`
- 書式: UTF-8 JSON オブジェクト（ネスト可）
- 存在しなければ読み込まない（既定値で運用）
- 同梱用テンプレート: `Artifact/ArtifactStartup.template.json`（実配置は任意。自動コピーしない）

## 2. キー命名

- スキーマと同じ `Group/Name` 形式（例: `Render/SolidRectBatch`）。
- JSON ではネスト可。`{"Render":{"SolidRectBatch":true}}` は `Render/SolidRectBatch` に平坦化される。
- 配列はそのままリスト値として取り込まれる。
- `_` で始まるキーは**ドキュメント用**として無視される（テンプレート内の注釈に使う）。

## 3. 適用レイヤと優先順位

`ArtifactStartup.json` は `ConfigLayer::System` へ流し込む。`System` は最も優先度が低いメモリ専用レイヤなので、既存の設定と衝突しない。

解決順は **JSON > 環境変数 > 既定値**。JSON に該当キーが無いときだけ環境変数にフォールバックする。

- レイヤ: `System`（`LayeredConfigStore`）
- 読み込み: `LayeredConfigStore::importSystemJson(path)`
- 起動点: `Artifact/src/AppMain.cppm`（`QApplication` 構築後、`ArtifactAppSettings::instance()` 初回取得前）

## 4. タイミング（固定）

- **起動時に1回だけ読む。** 実行中に値が変わることはない。
- ファイル監視・再読み込みは**しない**。設定変更を反映するには再起動が必要。
- 設定値の読み出しはコールドパス扱いとし、`qEnvironmentVariable` と同じ扱いとする。

この固定は `docs/technical/HOT_PATH_RULES.md` の「呼び出し頻度が不明な処理はコールドパスと仮定しない」に従う。起動時に確定した値を毎フレーム読むのは禁止ではなく、**再読み込み機構をホットパスに持ち込まない**という制約である。

## 5. 既存環境変数との関係

- 既存の `ARTIFACT_*` 環境変数は**残す**。段階移行する。
- JSON にキーが無い環境変数は、従来どおり環境変数だけで動作する。
- 同じキーが JSON と環境変数の両方にある場合、**JSON が勝つ**。このとき `[StartupFlags]` の警告を 1 回だけ出す（意図しない上書きを可視化するため）。**実装済み**。
- **ビルド時マクロは対象外。** `ARTIFACT_HAS_*` / `ARTIFACT_WITH_*` / `ARTIFACT_RESTORE_*` / `ARTIFACT_BUILD_*` は CMake の `target_compile_definitions` であり、実行時の JSON/env には該当しない。

## 6. 型検証

- JSON の値は `QVariant` として `System` レイヤに入る。
- **現状、型ミスは拒否されない。** バリデータは `ArtifactAppSettings::Impl` 構築時にのみ設定され（`ArtifactAppSettings.cppm` の `setValidator`）、起動読込はそれより前に走るため、`ConfigSchema::validate` の検査を通らない。
- 誤った型（例: 真偽値の代わりに文字列）は静かに既定値へフォールバックする。型不一致の警告ログは**検討中**（未実装）。
- 型ガードを厳密にする場合は、リーダを `AppSettings` 初期化後へ移すか、バリデータを `LayeredConfigStore` 側へ移す。いずれも設計判断を要する。

## 7. 移行対象（段階）

実行時 env 変数として実際に読まれているものは 36 個。移行は用途別に段階で行う。

| 段階 | 対象 | 備考 |
|---|---|---|
| 1 | `ARTIFACT_SOLIDRECT_BATCH` / `_INDIRECT` / `_VERBOSE` | 実装済み。`Render/SolidRect*` として JSON 対応 |
| 2 | GPU 系: `ARTIFACT_RENDER_BACKEND` / `_GPU_ADAPTER` / `_GPU_POLICY` / `_ENABLE_RAY_TRACING` | `Render/*` |
| 3 | 診断系: `ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS` / `_RENDER_TRACE_CRASH` / `_DISABLE_3D_RENDER_TRACE` / `_VIDEO_VERBOSE_LOG` / `_EFFECT_PROFILE` / `_ENABLE_GPU_FRAME_QUERY` | `Diagnostics/*` |
| 4 | XPU 系: `ARTIFACT_XPU*` | `RenderQueue/Xpu*`（Artifact render queue / encoder で部分対応） |
| 5 | テスト起動: `ARTIFACT_RUN_BUILTIN_TESTS` / `_RUN_GPU_BLEND_TESTS` | `Startup/*`。起動分岐なので JSON より env が自然 |

**移行しないもの**: ビルド情報（`ARTIFACT_BUILD_GIT_HASH` 等）、プラグイン ABI（`ARTIFACT_PLUGIN_API_VERSION`）、バージョン文字列。これらはコンパイル時値、または子プロセスへ引き渡す値である。

### XPU 系の JSON key 対応

2026-10-10 時点で Artifact の Render Queue と encoder が以下を `LayeredConfigStore` から起動時に一度解決する。各 JSON key が存在すれば対応する環境変数より優先する。JSON に未設定の場合は従来の環境変数を使用する。

| JSON key | 環境変数 | 用途 |
|---|---|---|
| `RenderQueue/XpuSpec` | `ARTIFACT_XPU`（legacy: `ARTIFACT_HETERO`） | mixed、iGPU role、parity / bench 等の spec |
| `RenderQueue/XpuMaxInFlight` | `ARTIFACT_XPU_MAX_IN_FLIGHT` | frame worker 上限 |
| `RenderQueue/XpuIncludeIntegrated` | `ARTIFACT_XPU_INCLUDE_INTEGRATED`（legacy: `ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED`） | Integrated GPU の参加可否 |
| `RenderQueue/XpuPreviewMinIntervalMs` | `ARTIFACT_XPU_PREVIEW_MIN_INTERVAL_MS` | preview publish 間隔 |
| `RenderQueue/XpuAsyncSequence` | `ARTIFACT_XPU_ASYNC_SEQUENCE` | 連番 writer の bounded async / manager 指定。XPU mixed jobではCPU thread予算内でwriter枠を予約し、logical thread数が6未満なら同期書き込みへ戻す |
| `RenderQueue/XpuParityHash` | `ARTIFACT_XPU_PARITY_HASH` | frame parity hash |
| `RenderQueue/XpuPipeline` | `ARTIFACT_XPU_PIPELINE` | 動画 pipeline 要求（現状は未対応警告） |
| `RenderQueue/XpuBench` | `ARTIFACT_XPU_BENCH` | bench JSON 出力 |
| `RenderQueue/XpuCloneCheck` | `ARTIFACT_XPU_CLONE_CHECK` | clone parity 診断 |
| `RenderQueue/XpuEncoderThreads` | `ARTIFACT_XPU_ENCODER_THREADS` | FFmpeg encoder thread count。非mixed時は0でFFmpeg default。XPU mixed動画では0／未設定を1 threadに制限し、正数指定もhost予算内へclampしてframe lane数から差し引く |
| `RenderQueue/XpuPreset` | `ARTIFACT_XPU_PRESET` | encoder preset override |
| `RenderQueue/XpuMaxHwEncoders` | `ARTIFACT_XPU_MAX_HW_ENCODERS` | プロセス内で同時に開ける hardware encoder session 数。既定値 2、入力値は 1〜16 に clamp。native hardware encoder / FFmpeg hardware pipe / Vulkan pipe で共有し、上限到達時は hardware open を失敗させ既存の backend 選択経路に software fallback させる |

この上限は診断値ではなく実際の session admission 制御である。Render Queue は現在 job を直列実行するため、単一 queue の通常運用では上限に達しにくい。独立した複数 encoder 利用や将来の job 並列化で同時 session を抑制する用途を持つ。

`Artifact/ArtifactStartup.template.json` には key の説明だけを置く。無指定時の自動値・現行動作を変えないため、XPU override 値自体はテンプレートに追加しない。

## 8. 実装上の注意

- **ホットパスでの設定読み出しを避ける。** 起動時に確定した値を呼び出し側が保持し、毎フレーム `LayeredConfigStore::value()` を呼ばない。
- JSON からの診断ログ出力は、フラグが有効なときだけ・submit 1回だけ等の遅延評価にする（`HOT_PATH_RULES.md` の「無条件ログ」禁止に従う）。
- `System` レイヤは `writable=true` にしたが、**永続化しない**。`saveLayer` は path 無し store に対する no-op のままで、JSON ファイル自体をアプリが書き戻すことはない。

## 9. 検証手順

1. `ArtifactStartup.json` を exe と同階層に置き、`{"Render":{"SolidRectVerbose":true}}` を書いて起動。
2. 起動ログに `[SolidRectBatch] flush count=...` が出ることを確認。
3. `{"Render":{"SolidRectBatch":false}}` で非バッチ経路に戻ることを確認。
4. 値を書かない場合は既定値（batch=true / indirect=false / verbose=false）が使われることを確認。

**ビルド・実機検証は未実施**（`AGENTS.md` によりユーザー明示指示が必要）。

## 10. 参照

- `ArtifactCore/include/Configuration/LayeredConfigStore.ixx`（`importSystemJson`）
- `ArtifactCore/src/Configuration/LayeredConfigStore.cppm`（`importSystemJson` 実装、`System` writable 化）
- `ArtifactCore/src/Application/ArtifactAppSettings.cppm`（`Render/SolidRect*` スキーマ登録）
- `Artifact/src/AppMain.cppm`（起動時 JSON 読込）
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm`（`solidRectBatchDebugFlag`、JSON > env > 既定の解決）
- `Artifact/ArtifactStartup.template.json`
- `docs/technical/HOT_PATH_RULES.md`
