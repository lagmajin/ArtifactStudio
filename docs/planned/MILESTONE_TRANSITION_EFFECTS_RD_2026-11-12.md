# Transition Effects R&D Suite

**最終更新:** 2026-11-13
**ステータス:** In Progress

**進捗:** 2026-11-13 — 既存静止画captureに全kindの31フレーム連続PNG列を追加し、analyzerは連番・progress mapping・PNG形式／寸法を検証する。ffmpegがある環境ではFFV1/Matroskaを生成し、review HTMLに動画再生とPNGフレーム送りを出す。通常fixtureとalpha laneのFFV1動画はdecode後に元PNGとの31フレーム全画素一致を確認した。追加の細線・人工粒状texture・透明境界fixtureをCrossfadeへ通すcapture出力とanalyzer/HTML接続を追加し、合成bundleで16 kind、3 fixture、1,420 PNGの解析PASS、38動画生成を確認した。Node DOM stubでレビューHTMLの再生・pause・frame-step・終端停止も確認した。baseline比較はfixture A/Bのhash集合も比較するよう強化。前段ではfixtureごとに2名の独立判定欄を追加したが、評価範囲との不一致を解消するため、captureを全16 kind×3 fixtureへ拡張し、各組み合わせでsource-alpha laneとalpha=77 laneの31フレームPNG、progress strip、動画を出力する実装に更新した。HTMLではkind別に折り畳んで表示し、判定者ID・判定・メモをfixture×kind×判定者で保存する。analyzerの構造smokeでは既存の合成PNGを各kindパスへ複製した疑似matrixを使い、48組・4,207 PNGをPASS、HTMLに48パネル／96判定者欄を生成し、JS exportでfixture×kind×2名の独立レコード保存を確認した。FFV1動画も同じ疑似matrixから128本を生成し、source-alpha／alpha=77各31フレームのdecode画像をPNGと全画素比較して一致した。このsmokeはmatrix配線検証であり、全kindを各fixtureで実レンダーした証拠ではない。実ビルドは未達：MSVC Ninja構成は現Toolsetに1041 clui.dllしかなく、VSLANG=1033でもgenerated `msvc_deps_prefix` と実出力が不一致になりinclude一覧が漏れたため停止。既存ClangCL構成はCMake再生成後、`AnimatableTransform2D.cpp.obj.ddi -> AnimatableValue.ixx.obj`の依存循環でNinjaが停止した。C++ test executable未生成のため実renderer capture・実ブラウザ目視・評価者2名の品質判定は未実施。

## 目的

既存のトランジション実装を、再現可能な入力・進行条件・出力キャプチャで比較し、視覚品質、時間的一貫性、alpha／色の扱い、実行コストを評価できる R&D スイートとして整える。製品機能やアルゴリズムを追加する前に、各経路の実際の契約と限界を把握する。

このスイートは主に `ArtifactCore` の `Video.Transitions.*` を対象とする。`Artifact` の `TransitionManager`、NLE transition model、ArtifactPr timeline は別の経路として記録し、同一実装・同一品質だと仮定しない。既存の統合計画は [transition-effects-expansion-2026-07-09.md](../../plans/transition-effects-expansion-2026-07-09.md) を参照する。

## 既存資産と初期所見

- `tests/ArtifactCore/TransitionRenderContractTest.cpp` は、factory 登録、決定性、出力形状、alpha、left frame のみの変更、進行端点、進行ごとの画素変化、複数種のwipe形状などを契約テストしている。
- `tests/ArtifactTestTransitionRenderer.h` と `tests/ArtifactTestTransitionBridge.h` が合成診断画像と共通レンダー入口を提供する。
- `tests/ArtifactCore/TransitionMcpMain.cpp` は同じCPUレンダー経路を headless MCP lab として公開し、単体描画、strip、grid、差分、PNG出力を行える。
- 既存契約テストは実験基盤として有用だが、テストの存在は実行成功、実装経路の製品利用、視覚品質、性能を証明しない。
- 既存計画では NLE→Video 接続、`Artifact` の別系統、旧計画の前提訂正が記録されている。R&Dではこれらの統合未完了をレンダー品質評価と混同しない。
- 現状のMCP lab説明は「CPU・GPU deviceなし」。したがって、この時点でGPU parityや実時間性能を測定済みとして扱わない。
- `tools/analyze_transition_capture.py` は既存contract testのcapture bundleを検査する。画像ファイルの整合性に加え、不透明・alpha=77の両入力でprogress端点を原寸入力A/Bと画素単位比較し、隣接progress間の変更画素数を画像から再計算する。raw timingの要約も行う。見た目の良し悪しや連続再生時の安定性は別途目視が必要。

## R&D対象と評価軸

| 領域 | 対象 | 評価 |
|---|---|---|
| 登録と適用範囲 | `TransitionKind`、factory、具象実装 | 具象種の列挙・生成、未登録・nullの理由、Cut等の非具象値の扱い |
| 視覚挙動 | crossfade、wipe/reveal、spatial、motion/deformation、stylized | 端点、途中経過、境界形状、入力由来画素の分布、時間方向の滑らかさ |
| 画像契約 | RGBA8 CPU入力と出力 | 寸法、alpha保持／合成、透明領域、色順、境界外参照、無効サイズ |
| パラメータ | direction、origin、frame index、fps、size、progress等 | 適用される値、clamp、既定値、設定に対する単調性・決定性 |
| コスト | CPUレンダー経路 | 解像度・transition kind・progress段階ごとのraw timing、必要なら割当計測 |
| 統合経路 | NLE、Artifact effect、ArtifactPr | データ対応と駆動点の静的監査。実際に接続されているとソース／実行で確認できるまで統合済み扱いしない |

## 決定論的fixture案

初期fixtureは人工生成し、外部メディアやdecoder依存を避ける。

1. **色ブロック＋番号付き象限:** A/B由来領域と幾何変形を判別する。
2. **高コントラスト格子と斜線:** seam、aliasing、歪み、反転を観察する。
3. **単色・グラデーション:** blend、色順、progressの基本特性を測る。
4. **alphaパターン:** 不透明、半透明、透明黒、透明hidden-RGBを組み合わせる。
5. **小寸法／非正方形／odd dimensions:** 境界とサイズ依存を確認する。

まず96×64の既存診断対を互換基準として残し、追加fixtureは小さな標準寸法と選定した高解像度で段階導入する。入力画像、条件manifest、renderer/analysis toolはSHA-256を記録する。

## Capture manifest と記録契約

各captureは少なくとも次を記録する。

- suite schema/version、transition stable nameとenum値、source revision
- 入力fixture名とhash、幅・高さ、pixel/channel/alpha契約
- progressの数値、direction/originなどの全設定、frame index、fps
- 実行経路とbackend。未確認・未計測は明示し、CPU結果からGPU動作を推定しない
- warm-up回数、測定反復数、反復ごとのraw milliseconds。統計量はraw sampleから算出
- 出力画像path/hash、入力A/Bとの画像差分指標、生成時エラーや未対応理由
- runner経由の場合はworking tree clean/dirtyとstatus hash、test executable SHA-256、build config、host OS／CPU情報、実行開始・終了時刻、test exit code。verification reportにはanalyzer自身のSHA-256も記録する。

実装上の引数が分からない場合にmanifest上で架空のpropertyを作らず、まずソース監査台帳に「未確認」と記載する。画像変換や色／alpha正規化を追加する場合は元データを保持し、変換条件と実装adapterを記録する。

## 初期実験計画

### Phase 1 — 台帳と既存ハーネス監査

- `TransitionKind` の全値、具象実装、factory登録、既存test coverageを対応付ける。
- `ArtifactTestTransitionBridge` が設定値を各具象実装へどう渡すか、入力形式と出力変異契約を確認する。
- 既存のテスト出力（PNG、contact sheet、behavior report）の形式と出力先、再現性を確認する。
- `Artifact` effect系は別台帳を作成し、登録率・null fallback・alpha経路をソースから再確認する。修正はこのR&D準備の範囲外。

### Phase 2 — Capture基盤とfixture

- 既存の人工A/B診断fixtureとcapture reportを最初のfixtureとして利用し、追加fixtureは後続に回す。
- 既存の受け入れテストtargetは `ArtifactCoreTransitionRenderContractTest`（`tests/ArtifactCore/CMakeLists.txt` 登録）。`tools/run_transition_capture.py` は既存test exeを起動し、`ARTIFACT_TRANSITION_CAPTURE_DIR` で指定した出力先へPNG/reportを保存させ、provenance manifest作成と解析を続けて行う。
- 実行手順（対象buildがすでに存在し、ユーザーがテスト実行を指示した後）:
  ```powershell
  python tools/run_transition_capture.py `
    --test-executable "<build>\bin\<config>\ArtifactCoreTransitionRenderContractTest.exe" `
    --capture-directory "$PWD\transition-capture" `
    --build-config <config>
  ```
- vcpkg runtime DLLがtest executableの隣にない環境では、`--runtime-path "<vcpkg>\installed\<triplet>\bin"` と `--runtime-path "<vcpkg>\installed\<triplet>\debug\bin"` を追加する。
- runnerは新規または空の出力directoryだけ使い、Git revision／dirty status、実行ファイルhash、OS／CPU、build config、test exit code、tool identityを `run_manifest.json` に保存する。analyzerはmanifestが無い既存／直接実行captureを受け入れるが、provenance不足をwarningに出す。
- capture testはcontact sheetに加え `input_A.png` / `input_B.png` を原寸で保存する。解析器は `verification_report.json` を作り、期待16 kind、progress列、端点のLeft→Right、開始／終了画像の完全一致、隣接画像間の実変更画素数とreport値の一致、不透明入力に対するalpha保持、各stepの分類pixel count、全PNGの存在・RGBA形式・寸法・decode可否を検査する。出力画像と元reportのSHA-256を保存し、差し替えや再生成を追跡できる。
- さらに既存 `NonOpaqueAlphaPreserved` 契約に合わせ、全入力pixel alpha=77のA/B pairでも全kindを5 progressでcaptureする。解析器はalpha=77保持、両端の完全pixel一致、alpha laneの隣接画像差分も検査する。runner経由でalpha laneが欠けるcaptureは古いtest executableと見なし、failにする。
- 同時に `review.html` を出力し、入力A/B、progress別全kind grid、各kindのprogress stripと個別フレームをブラウザで見比べられるようにする。kind別の目視判定（未確認／問題なし／要確認）とメモはcapture report hash単位でブラウザへ保存し、JSON exportできる。HTMLはローカル画像を参照し、外部リソースに依存しない。
- 任意の `--baseline <capture-directory-or-verification_report.json>` で既存captureと比較できる。寸法／fps／progress／kind集合／fixture hash／execution pathの条件一致、変更・追加・削除PNGのhash一覧を `capture_delta.json` に出し、HTMLから基準のreview sheetへ移動できる。画像差分は自動failにはせず、意図した更新か人が確認する。
- 自動検査PASSはcapture bundleとreport上の基本的な挙動が整合する意味に限る。画像を開いてcontact sheetと各progressを目視し、方向・境界・破綻を別途確認する。
- 共通progress列は0、0.125、0.25、0.5、0.75、0.875、1を使い、動きの速い／確率的な系統は連続フレーム追加を検討する。
- 全具象kindの小規模coverage captureと、選定代表kindの詳細captureを分ける。
- 新captureのCPU timingは各kind／progressで2回warm-up後に7 raw samplesを保存する。対象はCPU `ArtifactTestTransitionBridge::render` 呼び出し（factory、frame adapter、process、返却画像の破棄を含む）、fixture構築とPNG出力は除外。これは少数sampleの診断値であり合否閾値に使わず、性能比較ではbuild/host条件の記録を追加する。

### Phase 3 — 視覚・時間解析

- 端点差、隣接progress間差分、A/B source classification、alpha誤差、edge/seam位置・連続性を算出する。
- Wipe/reveal系はcoverage領域と境界の位置・形状を評価し、crossfadeとは別の指標で扱う。
- motion/deformation系は特徴点の移動、反転、画面外流出と時間差分を評価する。単一の汎用「品質スコア」へまとめない。
- 指標は既知のsynthetic fixture上で妥当性確認し、スクリーンショットを併記して数値だけで品質判定しない。
- **連続フレーム動画レビュー（実装済み・実capture未検証）:** transition durationとfpsを固定し、各フレームのprogressを `frameIndex / (durationFrames - 1)` から決定して連続PNG列を生成する。各フレームを単体で保存し、ffmpeg利用可能時はFFV1/Matroskaのロスレス動画も作る。表示用動画のcodec／pixel formatを記録し、画素検査は元PNG列を使う。
- 時間評価では通常速度で全体を複数回再生し、スローモーション／フレーム送りで局所を確認する。開始・終了の飛び、境界の停滞／急加速、flicker、shimmer、ghosting、ちらつくalpha fringe、予期しないフレーム重複・欠落をkind別に記録する。
- 時間指標は隣接フレーム差分の平均だけにせず、フレーム差分分布と最大局所変化、境界または特徴領域の位置・速度変化、alpha／色の時間安定性を採用する。意図した大きな動きと欠陥を分離できる根拠がない値は合否にせず参考値とする。
- **評価素材と条件:** 初期は既存の番号付き象限・格子・alpha fixtureを使い、位置対応と境界が追えることを確認する。連続時間評価では3種類のfixture（高コントラスト／細線、自然な質感、透明境界）を選び、全具象transition kindを各fixtureに適用する。各fixture／kindごとにsource alphaを維持したlaneとalpha=77の契約laneを保存し、SHA-256、寸法、alpha契約を固定する。frame rate、duration、progress mapping、入力、出力backendを記録する。
- **比較・合否:** 現行実装captureを条件固定のbaselineとする。視覚判定は全fixture／kind組み合わせごとに2名が通常速度再生とフレーム送りで独立に記録し、重大欠陥（フレーム欠落、端点不一致、意図しない全画面flash、破綻したalpha fringe）が1件でもあれば要調査。それ以外の許容差はbaseline取得後、候補比較前にレビュー観察を基に確定する。baseline未取得・判定者不足は未検証と明記し、合格扱いにしない。
- **経路・性能:** このcapture harnessで確認できるのは現状CPU adapter経路。GPU／実アプリpreview・最終出力の一致は未検証とし、別経路captureなしで一般化しない。render時間は既存のraw sample契約に加え連続シーケンスの総時間とフレーム時間分布を記録するが、予算・合否閾値は対象host/buildのbaseline後に定める。

### Phase 4 — 比較・報告

- kindごとに入力、全条件、progress strip、解析値、raw timing、未対応条件をひとまとめにした結果bundleを出す。
- 連続PNG列と再生用動画をbundleに含め、動画で全体のテンポを見た後、同じフレーム列を1枚ずつ確認できるようにする。動画codecによる見かけの差と元レンダーの差を混同しない。
- 2回目以降のcaptureでは `--baseline <previous-capture-directory>` を使い、条件とfixtureが同じ比較かを確認してから画像hash差分と目視レビューを行う。CPU timing比較は同host/build/measurement contractの時だけ同条件として扱う。
- build、host、実行経路が一致する比較と、異なる環境の参考比較を分離する。
- 次の作業候補（不具合、未実装、統合、最適化）は根拠付きで提示し、別途承認された依頼なしに製品範囲へ拡張しない。

## 完了条件

- 全 `TransitionKind` と具象factory登録の静的台帳がある。
- fixture、capture条件、結果の出所がhashとmanifestで追跡できる。
- 端点・alpha・進行挙動・境界・時間一貫性をカバーする測定／目視資料がある。
- 連続フレームを動画再生とフレーム単位の両方で確認し、判定、再生条件、評価者、重大欠陥、未検証経路を記録している。
- raw timingと実際のbackend情報が保存され、未測定・未対応が明確である。
- NLE、Artifact effect、ArtifactPrの接続状態を混同しない説明がある。

## 制約・検証状態

- これはR&D準備計画であり、アルゴリズム実装・統合作業を自動的に承認するものではない。
- C++ contract testとMCP self-checkの起動、実captureの生成、実データの解析、ブラウザでの目視判定、連続フレーム動画の生成・再生確認は未実施。合成fixture smokeは解析器の配線確認だけで、rendererを通さない。runnerはsource revision、binary hash、build config、host/CPU情報を記録するが、CPU timingは診断値でありbackend parityの証拠にはならない。
- 解析器はPillowを必要とする。これは既存の `tools/ui_visual_compare.py` と同じ画像依存である。
- ビルド、テスト、CMake configureは明示指示を得てから行う。
- 子リポジトリ／サブモジュールは変更しない。
