# 独自ドッキングウィジェットマネージャ移行マイルストーン

**最終更新:** 2026-08-15
**ステータス:** Phase 1–3 partial / two-panel MVP implementation complete, runtime verification pending。floating lifecycle and backend switch pending

## 現行コード監査 (2026-08-15)

`ArtifactDockManager` facade、QADS adapter、`ArtifactNativeDockSurface`、backend-neutral な `DockLayoutDocument`／registry、portable JSON の保存・復元は現行コードに存在する。`ArtifactMainWindow` の公開契約は `Artifact.DockManager` を参照し、QADS の area 変換や manager 所有は実装側へ閉じられている。一方、`ArtifactMainWindow.cppm` 自体は QADS の生成・登録・floating callback を backend facade 内で扱う構造が残っており、独自 backend と QADS backend の実運用切替、floating lifecycle の完全な抽象化、workspace preset の backend 非依存化は未完である。

## 目的

Qt Advanced Docking System（QADS）を参考実装として活用しつつ、ArtifactStudio のドッキング責務を独自のドッキングウィジェットマネージャへ段階的に移行する。

QADS を一度に削除せず、既存のレイアウト・タブ・floating・workspace 保存を維持しながら、アプリケーション側 API と backend を分離する。

## 次の実装方針: Two-panel MVP

全パネルを一度に独自 backend へ移すのではなく、まず以下の2面だけで独自 dock surface の実運用経路を成立させる。

- `Composition Viewer`
- `Inspector`

この2面で、登録、表示、tab 化、activate、close、area 移動、portable layout 保存・復元までを確認する。Timeline、Project、Asset Browser、Components、Effects、Render Queue、Debug 系は QADS backend に残してよい。

MVP の目的は機能数を増やすことではなく、独自 backend が実際の編集セッションで壊れずに使えることを確認することである。

### 現在の実装状態

`ARTIFACT_NATIVE_DOCK_MVP=1` の opt-in 経路を追加し、Composition Viewer と Inspector を `NativeDockSurface` に登録できるようにした。表示、activate、visibility、pinned、tab routing、portable layout のコード経路も接続済み。通常起動時は従来どおり QADS backend を使用する。native surface の表示、サイズ、保存・復元、QADS との混在はまだ runtime 未検証であり、現時点では既定 backend を変更しない。

## 背景と現状の課題

- `ArtifactMainWindow` は `QWidget` 上に `CDockManager` を直接保持している。
- 公開 API が `ads::DockWidgetArea` を直接受け取り、QADS の型が `AppMain` まで漏れている。
- QADS の overlay、floating window、resize 再描画に対するアプリ側 workaround が増えている。
- `QMainWindow::saveState()` は使用せず、ADS の状態保存を別経路で管理している。

## フェーズ

### Phase 1 — アプリ側 docking API の切り離し

状態: In Progress

- [x] `Artifact::DockArea` を導入する
- [x] `ArtifactMainWindow` の登録 API から `ads::DockWidgetArea` を排除する
- [x] QADS の area 変換を `ArtifactMainWindow` 実装内部へ閉じ込める
- [x] `ArtifactDockManager` facade の最小 seam を導入し、dock 登録を移す
- [x] ADS state の保存・復元を backend seam 経由にする
- [x] 可視性・activate・close・pinned 操作を backend seam 経由にする
- [x] floating の生成・geometry 設定を backend seam 経由にする
- [x] central、tab 移動、splitter 操作を backend seam 経由にする
- [x] deferred floating の materialize を backend seam 経由にする
- [x] floating container の列挙を backend seam 経由にする
- [x] QADS manager の生成と overlay 準備を backend seam 経由にする
- [x] floating widget creation の callback 接続を backend seam 経由にする
- [x] `ArtifactMainWindow::Impl` から QADS manager の所有ポインタを除去する
- [x] dock registry の所有を backend へ移し、既存処理は互換参照で維持する
- [x] QADS 非依存の `Artifact.DockManager` 契約 module を追加する
- [x] backend registry に area、tab group、floating、visible、pinned、geometry を記録する
- [x] `DockLayoutEntry` の backend-neutral JSON 変換 API を追加する
- [x] QADS state とは独立した portable dock layout JSON の取得 API を追加する
- [x] portable JSON 出力前に visible／pinned／floating geometry を registry へ同期する
- [x] portable layout JSON をアプリ終了時の FastSettingsStore へ並行保存する
- [x] portable layout JSON から area／tab group／visible／pinned／floating 状態を適用する移行用 API を追加する
- [x] `ARTIFACT_USE_PORTABLE_DOCK_LAYOUT=1` による opt-in 比較復元経路を追加する
- [x] portable layout JSON を versioned envelope 化し、旧配列形式も後方互換で読む
- [x] versioned portable layout を `DockLayoutDocument` 契約型へ集約する
- [x] QADS visibility／top-level change signal から registry を逐次同期する
- [x] tab group を同一 dock area の決定的な dock ID 集合から再計算する
- [x] portable layout entry の dock ID／floating geometry を入力検証する
- [x] dock 登録時の空ID／重複ID／再登録時のstale entryを防止する
- [x] portable document version を共有定数で管理する
- [x] `AppMain` から QADS include／型参照を除去する
- [ ] floating lifecycle と native window policy を facade へ移す
- [x] QADS 型が `ArtifactMainWindow` の公開インターフェースへ再流出しないことを静的確認する
- [x] `DockStyleManager` の公開APIからQADS manager型を除去する

### Phase 2 — レイアウトモデルと永続化の独立

- [x] dock 識別子、area、tab group、visible、pinned、floating geometry の内部モデルを定義する
- [x] backend-neutral な `DockLayoutRegistry` 契約型を追加する
- [ ] QADS state blob と独自レイアウトモデルを分離する
- [x] 旧 QADS レイアウト適用後に独自モデルを生成する移行経路を追加する
- [ ] workspace preset / session 保存の backend 依存をなくす

### Phase 3 — 独自 dock surface

- [x] dock surface、tab surface、splitter surface の最小実装を追加する
- [x] dock の登録・削除・表示切り替え・tab 化を独自 backend で実装する
- [x] native surfaceにactivate相当のtab前面化操作を追加する
- [x] native surfaceでpinned状態を保持・保存・復元する
- [x] native surfaceで実行中のdock area移動を実装する
- [x] native surfaceでTop／Bottom領域を独立tab surfaceとして扱う
- [x] QADS adapter／native surfaceのbackend kind／capability契約を追加する
- [x] backend kindの文字列変換を共通契約へ集約する
- [x] native surfaceが未対応floating entryを黙ってdocked化しないようにする
- [x] native surfaceのdock一覧／area問い合わせAPIを追加する
- [x] native surfaceのvisible／pinned問い合わせAPIを追加する
- [x] native surfaceへportable layoutのarea／visibleを適用する最小復元APIを追加する
- [x] native surface自身のportable layout保存／復元APIを追加する
- [x] native surfaceのportable layout旧配列形式を後方互換で読む
- [ ] `ArtifactMainWindow` は facade のみを参照する構成にする
- [ ] QADS backend と native backend を切り替え可能にする

#### Two-panel MVP acceptance slice

- [ ] Composition Viewer と Inspector を native backend で生成できる
- [ ] 2面の docked / tabbed / area 移動 / activate / close を実操作で確認する
- [ ] 2面の portable layout 保存・復元を確認する
- [ ] 残りのパネルは既存 QADS backend のままでも起動・操作できる
- [ ] native backend の不成立時に、全 workspace を壊さず QADS backend へ戻せる

### Phase 4 — floating と drag/drop

- [ ] 独自 floating window の lifecycle を定義する
- [ ] drag preview と insertion target のモデルを定義する
- [ ] Windows の native window / backing store / resize ordering を検証する
- [ ] QADS overlay workaround を削除する

### Phase 5 — QADS backend の撤去

- [ ] QADS adapter を既定 backend から外す
- [ ] QADS 依存を CMake と include/module graph から削除する
- [ ] 旧 layout の移行期間と fallback 方針を決定する
- [ ] レイアウト復元、floating、workspace mode、focus mode の回帰確認を完了する

## 設計方針

- 既存の `ArtifactMainWindow` 公開 API を急激に変更せず、段階的に facade へ移す。
- QADS の内部型は adapter 実装内に閉じ込める。
- 新しいグローバル signal / slot 配線は追加しない。
- QADS の floating 問題を独自 manager 側へコピーせず、状態モデルと表示責務を分離する。
- まず docked layout と persistence を安定させ、floating / drag-drop は後段で扱う。

## 完了条件

- QADS を無効化した backend で、標準 dock の登録・表示・tab 化・保存・復元が動作する。
- QADS の型がアプリケーションの公開 API に現れない。
- floating と drag/drop の失敗時に、workspace 全体のレイアウトが破損しない。
- QADS backend と独自 backend の比較が可能である。
- ビルド、静的 module hygiene、レイアウト復元、floating resize の検証結果を記録する。

## 次の実装単位

次は Composition Viewer と Inspector に限定して native backend を実運用経路へ接続し、QADS backend との混在・切替・portable layout 復元を確認する。その後に floating lifecycle と native window policy を facade の責務へ移す。全パネルを独自 backend へ移すことは MVP の完了条件ではない。

## 検証メモ

- 静的確認済み: `Artifact/include/Widgets/**/*.ixx` にQADS型・QADSヘッダ参照なし。
- 静的確認済み: `ArtifactSources.cmake` に `ArtifactDockManager.ixx` と `ArtifactNativeDockSurface.ixx` を登録済み。
- 静的確認済み: `git -C Artifact diff --check` 通過。
- 未確認: ビルド、module hygiene、native surfaceの実機表示、floating／drag-drop回帰。
