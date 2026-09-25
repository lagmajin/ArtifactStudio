# マイルストーン: シリアライズ診断境界と軽量ワークスペーススナップショット

**最終更新:** 2026-09-25  
**ステータス:** In Progress

## 実装更新 2026-09-25

- DebugBridge の周期取得を専用の軽量 workspace snapshot へ切り替えた。プロジェクト詳細、Composition 全体、選択 Layer の保存用 `toJson()` はこの周期経路から外れた。
- 軽量 snapshot は project/composition/selection/render queue の識別子・件数・現在状態・warning を返す。選択 Layer は ID・クラス名・名前だけを含める。
- `WorkspaceAutomation::workspaceSnapshot()` と明示的な AI project/composition snapshot は互換性のため変更していない。
- ソース差分の静的確認のみ実施。ビルド／テスト／実機確認は未実施のため、型・module 統合と DebugBridge consumer 互換は未検証。

## 目的

プロジェクト保存に使うシリアライズと、デバッガー／AI が状態を読むための定期スナップショットを分離する。JSON 関連の失敗を、発生箇所・対象オブジェクト・失敗理由まで追えるようにし、診断スナップショットが全プロジェクトの `toJson()` を高頻度に呼ぶ構造を解消する。

## 根拠と確認済みの現状

- `DebugBridgeFileWriter` は250msタイマーで `buildDebugBridgeSnapshotJson()` を呼ぶ。
- `buildDebugBridgeSnapshotJson()` は `WorkspaceAutomation::workspaceSnapshot()` を経由し、`projectSnapshot()` は `ArtifactProject::toJson()` を呼ぶ。Composition とその Layer の `toJson()` が続けて実行される。
- `ArtifactParticleLayer::toJson()` は粒子設定を JSON 化するが、失敗を型付き結果として呼び出し元へ伝える契約は見当たらない。
- 統一シリアライズ基盤と `ProjectSerializer` facade は存在する一方、通常プロジェクトの各 Layer／Composition は用途別 `toJson()` が主経路として残る。全面移行は未完了。
- 提示されたスタックだけでは、JSON 構文エラー、例外、不正メモリアクセス、競合のどれが起きたか断定できない。Particle の `toJson()` はスタック上の現在位置であり、根本原因の確定ではない。
- 2026-09-15 のローカル stderr ログには `[AbstractProperty] rejected incompatible or non-finite value for 'solid.color'` の反復がある。今回のスタックとの因果関係は未確認。

## 設計方針

1. **診断スナップショット専用 DTO**
   - デバッグブリッジ／WorkspaceAutomation の read-only snapshot は、必要な名前・ID・件数・選択・状態・警告だけを専用の軽量 DTO から構成する。
   - 診断用途から project/composition/layer の保存用 `toJson()` を呼ばない。
   - project 全体の詳細取得が必要な AI API は明示的なオンデマンド操作として分離し、定期ブリッジの周期処理に含めない。

2. **シリアライズ診断を返す共通境界**
   - 段階移行できる結果型／コンテキストを設け、成功データに加えて error code、object type、stable ID、JSON path、簡潔な理由を返す。
   - 子オブジェクトの失敗箇所を親まで path 付きで伝播し、どの layer/property が原因か追えるようにする。
   - 非有限値・範囲外値・不正 enum の扱い（拒否、既定値置換、警告）を項目ごとに明示し、暗黙の丸めで問題を隠さない。
   - 既存 `toJson()` API とファイル形式を一括変更しない。まず adapter/facade 境界で診断を付加し、互換性を確認した型から段階移行する。

3. **頻度と所有権を明示**
   - 250ms のデバッグ周期では全ツリー走査、JSON ツリー構築、全プロジェクトコピーを行わない。
   - DTO は UI/プロジェクト所有スレッドで一貫した状態から取得する。別スレッドへ渡す場合は immutable snapshot とし、live layer pointer を保持しない。
   - revision/dirty state を使って変更時のみ再構築し、同一 payload の比較のために全量 JSON を毎回生成する構造を避ける。
   - ホットパス規則に従い、周期診断に無条件の文字列整形や非ゼロの大容量確保を持ち込まない。

## フェーズ

### Phase 1: 呼び出し経路と失敗の分類

- debug bridge の周期経路、WorkspaceAutomation snapshot、project save/export の呼び出し元を分類する。
- 「JSONエラー」を構文、値検証、シリアライズ中クラッシュ、I/O失敗、復元失敗に分け、既存ログと error reporting のどこで失われるか記録する。
- Particle のスタックについては、再現ログ／例外コード／クラッシュダンプがない限り原因を断定しない。

### Phase 2: 軽量な診断スナップショット

- DebugBridge 用の専用 snapshot DTO/collector を実装する。
- `workspaceSnapshot()` の定期利用に保存形式の project JSON を混入させない。
- 差分または revision ベースで更新し、payload が不変なら周期ごとの全量 JSON 構築を省く。
- snapshot の schema version と警告コードを維持し、既存 consumer との互換を明示する。

### Phase 3: 共通シリアライズ診断コンテキスト

- 既存 Serialization Framework に path/object context と構造化 diagnostic の伝播を追加する設計を確定する。
- `ArtifactProject` の復元契約が未確定である点を尊重し、project 全体の `ISerializable` 化を前提にしない。
- 子レコード単位で処理を継続できる範囲と、全体を失敗させるべき整合性境界を定義する。
- 代表的な Layer 1種（Particle を候補）で adapter を試し、従来 JSON との schema 互換を確認する。

### Phase 4: 段階移行と運用診断

- 対象を監査結果で選び、Layer/Composition の serializer を診断コンテキスト対応へ順次移す。
- save/load、AI の明示的詳細取得、debug bridge の診断取得がそれぞれ別の用途・頻度契約を守ることを確認する。
- エラー報告に object ID/type/path と error code を出し、巨大 payload や機密値をログへ出さない。

## 受け入れ条件

- DebugBridge の周期 snapshot が `ArtifactProject::toJson()`、`ArtifactAbstractComposition::toJson()`、各 Layer の保存用 `toJson()` を呼ばない。
- **部分実装:** 上記の DebugBridge 周期呼び出しから project/composition/selected layer の保存用 `toJson()` 呼び出しを外した。ソース静的確認済み、runtime 未検証。
- 定期 snapshot はプロジェクトサイズに比例する JSON 全量構築をせず、必要な状態・summary と structured warnings を返す。
- snapshot は同じ UI/所有スレッド上で整合した状態を読むか、immutable DTO を読む。live object pointer を周期境界越しに保持しない。
- 代表 Layer の失敗診断に object type、stable ID、JSON path、分類可能な error code が含まれる。
- 従来形式の JSON key/schema と正常データの値が維持され、旧プロジェクトの復元互換を損なわない。
- 不正値が発生した場合、どの値を拒否・既定化したかを機械判読可能に報告し、無言の補正をしない。
- runtime/crash の原因が確認できていないケースでは、修正済みと報告せず未確認として残す。

## 制約・対象外

- このマイルストーンは `ReactiveEvents` に触れない。
- サブモジュールは変更しない。実装が必要でも Artifact 親リポジトリ側で閉じる。
- JSON schema を一括変更せず、project format の切り替えや全 Layer の一斉 `ISerializable` 移行を含めない。
- ビルド、CMake、テスト、実機確認は明示指示があるまで行わない。
- `docs/INDEX_GENERATED.md` は生成ファイルのため、マイルストーン追加後に手作業編集しない。

## 最初の作業単位

Phase 1 と Phase 2 の設計／実装を先行する。最初の差分では周期 DebugBridge snapshot を軽量化し、保存用 serializer の包括移行とは分けてレビュー可能にする。その後 Phase 3 で診断契約を固め、Particle を代表 adapter として段階移行する。
