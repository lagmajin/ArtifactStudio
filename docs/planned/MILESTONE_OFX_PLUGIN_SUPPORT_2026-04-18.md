# マイルストーン: OFX プラグインサポート実装

作成日: 2026-04-18
**最終更新:** 2026-10-01
優先度: 🔴 最高
対象バージョン: M13

---

## 概要

OpenFX (OFX) 標準プラグイン規格のサポートを実装し、Nuke / DaVinci Resolve / After Effects 互換のサードパーティエフェクトを ArtifactStudio 上でネイティブ動作させるためのマイルストーンです。

これにより業界標準のVFXエフェクトエコシステム全体を取り込み、エフェクトのパリティを一気にAEレベルまで引き上げます。

---

## 目標

✅ OFX 1.4 規格の完全実装
✅ .ofx プラグインの動的ロード
✅ パラメータ自動マッピング
✅ GPU アクセラレーションサポート
✅ 既存エフェクトスタックとの完全な互換性
✅ Boris FX / Sapphire / Neat Video 等のメジャープラグイン動作保証

---

## 進捗メモ

**2026-04-18 時点の実装進捗**

- ✅ OFX ヘッダー統合: `Artifact/include/ofx` を前提にホスト側実装へ接続済み
- ✅ プラグインローダー: `.ofx` / `.dll` / `.so` / `.dylib` の走査と動的ロードを実装
- ✅ ホストインターフェース: `OfxHost` の `fetchSuite` を返せるように実装
- ✅ プロパティスイート: 最小実装を追加
- ✅ イメージエフェクトインターフェース: 最小実装を追加
- ✅ パラメータスイート: 最小実装を追加
- 🟡 `Load / Describe / DescribeInContext`: 自己記述の取り込みまで接続
- 🟡 パラメータ自動マッピング: `ofx.mix` / `ofx.bypass` のブリッジ下地と preview property 変換まで接続
- 🟡 パラメータ read/write: `paramSetValue / paramGetValue` の最小実装を追加
- 🟡 階層/順序: `parent` を `group/path` に反映し、定義順を保持する下地を追加
- ⏳ レンダーバックエンド統合: まだ未着手
- ⏳ 既存エフェクトスタックとの完全互換: まだ未着手

現時点では「OFX プラグインを見つけて host に載せる土台」を優先しており、実レンダリングや完全な param bridge はこれからです。

## Update 2026-08-15

現行コードを再照合した結果、4月時点より実装範囲は広がっている。

- `ArtifactOfxHost` は OFX プラグインの探索・ロード、`fetchSuite`、Property／Param の基本操作、`Load / Describe / DescribeInContext` 相当の自己記述取り込みを持つ。
- `ArtifactOfxEffectImpl` は OFX エフェクトを通常のエフェクト生成経路へ接続し、パラメータ型の基本変換、`ofx.mix`／`ofx.bypass`、画像バッファを渡すレンダー呼び出し、プラグインごとの render instance を実装している。
- エフェクト一覧・メニュー・Inspector には OFX カテゴリ、Plugin Manager、再スキャン、ロード済みプラグイン表示の導線がある。
- ただし OFX 1.4 完全互換とはまだ言えない。時間依存・キーフレーム API の多くは未対応または unsupported 扱いで、カスタム UI、深いパラメータ階層、GPU／Diligent テクスチャ共有、タイル／マルチスレッド、クラッシュ隔離、ブラックリスト、実プラグイン互換検証は未完了。
- したがって現状判定は「ロードして基本パラメータと CPU 系レンダーを接続する実用的な骨格まで実装済み。完全な OFX エコシステム対応は未達」とする。

## Update 2026-10-01（整合性監査と描画経路の修正）

整合性監査で、描画経路が構造的に成立していないことが判明し修正した。

**修正した致命的欠陥**

- **出力 clip の未実装**: `clipGetImage` が clip 名に関わらず常にソース入力バッファを返していた。プラグインが `Output` clip へ書き込むと入力が破壊され、結果として全エフェクトが no-op だった。clip 名で振り分け、出力 clip には `dstPixelData` を返すよう変更。
- **clip → effect の逆引き不能**: レンダーインスタンスの clip はロード済みプラグインの記述 clip と別オブジェクトで生成されるため、逆引き走査が常に失敗し `clipGetImage` が常に `kOfxStatFailed` になっていた。`ClipState` に owner ポインタを持たせ直接解決するよう変更。
- **Rescan 時の use-after-free**: 再スキャンで DLL を解放しても既存のエフェクトは古い `libraryHandle` を保持し、`GetProcAddress` が解放済みメモリを指していた。ホストに世代番号を導入し、世代が古いエフェクトは描画を安全に通過する（バイパス）よう変更。エフェクトのデストラクタで `EndSequenceRender` / `DestroyInstance` を呼ぶようにした。
- **レンダー時刻のハードコード**: `pluginActionRender` に時刻 `0.0` を渡しており全フレームが t=0 評価だった。`EffectContext` の `compositionFrame / frameRate` から時刻を算出して渡すよう変更。

**申告値と実態の不一致を解消**

- キーフレーム API は全滅しているのに、ホストがアニメーション対応パラメータを `1` と申告していた。`SupportsCustom/String/Boolean/ChoiceAnimation` と `kOfxParamPropAnimates` を実態に合わせて `0` に変更。
- `GetClipPreferences` / `IsIdentity` を実装し、クリップの深度・コンポーネント交渉と恒等変換スキップをプラグインに提示。
- `MemorySuite` / `TimeLineSuite` を実装し、OFX C++ Support Library が無条件に取得するスイートを `nullptr` 返却から変更。
- レンダー失敗時の `StatusMessage` を `outArgs` 経由で受け取り、`clipGetImage` 失敗は message suite で通知するようにした（両方に outArgs がないため）。

**パラメータ受け渡しバグ**

- グループ／ページ内パラメータの完全修飾名（`group/name`）と paramSet の裸名キーの不一致で常に引き当て失敗していた。修飾名→裸名のフォールバック解決を追加。
- `"RGBA"` が `"RGB"` に部分一致して alpha が欠落していた（`RGB` 分岐が到達不能）。型を完全一致判定に変更。
- 2D/3D パラメータは第 1 成分しか渡さず、残りが 0 で埋まっていた。全成分を渡すよう変更。`Choice` は整数として渡すよう変更。
- `ofx.mix` がバイパスのしきい値しか機能していなかった。実際のブレンドとして機能するよう変更。

**残存（未実装・未検証）**

- キーフレーム API（`paramGetNumKeys` / `paramSetValueAtTime` の時刻評価）は未実装。ホストはアニメーション非対応として申告するため、プラグイン側でアニメーションを要求しないことが前提。
- GPU／Diligent テクスチャ共有、タイル／マルチスレッドレンダリング、クラッシュ隔離、プラグインブラックリスト、非 Windows（macOS / Linux）でのプラグインロードはいずれも未着手。ロード・`GetProcAddress` は現在 Windows のみ（他プラットフォームでは no-op）。
- 実プラグイン（Boris FX / Sapphire / Neat Video 等）での互換性検証は未実施。描画経路が今回初めて成立したため、実際の動作確認が必要。

---

## 実装タスク

### Phase 1: OFX ホストコア実装

- [x] OFX ヘッダーライブラリの統合
- [x] プラグインローダー実装
- [x] ホストインターフェース実装
- [x] プロパティスイートの実装
- [x] イメージエフェクトインターフェース

### Phase 2: パラメータシステムブリッジ

- [x] OFX パラメータ ↔ Artifact AbstractProperty 双方向マッピング
- [x] アニメーション可能パラメータの自動公開
- [ ] パラメータグループ / 階層構造の伝搬
- [ ] カスタムUIパラメータのフォールバック
- [ ] 時間依存パラメータの対応

### Phase 3: レンダーバックエンド統合

- [ ] CPU レンダーパス実装
- [ ] テクスチャ共有 (Diligent ↔ OFX GL/CL/DX)
- [ ] レンダーコンテキスト管理
- [ ] タイルレンダリング対応
- [ ] マルチスレッドレンダリング対応

### Phase 4: エフェクトスタック統合

- [ ] 既存エフェクトシステムへの透過的追加
- [ ] OFX エフェクト専用インスペクターUI
- [ ] プリセット保存/読み込み対応
- [ ] コピー&ペースト対応
- [ ] エフェクトメニューへの自動追加

### Phase 5: 互換性と安定化

- [ ] 一般的なプラグインでの動作テスト
- [ ] エラーハンドリングとクラッシュ保護
- [ ] プラグインブラックリスト機能
- [ ] パフォーマンス最適化
- [ ] メモリリーク検証

---

## 技術的仕様

### アーキテクチャ図

```mermaid
flowchart LR
    A[Artifact エフェクトスタック] --> B[OFX ホストアダプタ]
    B --> C[OFX プラグインホスト]
    C --> D[サードパーティ OFX プラグイン]

    E[Diligent Render Engine] --> B

    F[AbstractProperty システム] --> B

    G[インスペクターUI] --> B
```

### 対象ファイル

```
ArtifactCore/include/Plugin/OFXHost.ixx
ArtifactCore/src/Plugin/OFXHost.cppm
Artifact/src/Service/PluginManagerService.cppm
Artifact/src/Widgets/OFXEffectInspectorWidget.cppm
cmake/FindOFX.cmake
```

### 依存関係
- OFX API ヘッダー (BSD ライセンス、同梱可能)
- Diligent Engine とのテクスチャ共有機能
- 既存のエフェクトシステムと完全互換

---

## 完了条件

- [ ] `.ofx` ファイルを配置するだけで自動的に認識される
- [ ] エフェクトメニューに全てのプラグインが一覧表示される
- [ ] 標準エフェクトと全く同じ操作感で OFX エフェクトを使用可能
- [ ] 全てのパラメータでキーフレームアニメーションが動作する
- [ ] 少なくとも 3 つ以上のメジャープラグインが正常動作する

---

## リスクと制約

⚠️ 一部プロプライエタリプラグインはホストホワイトリストを持つため動作しない場合がある
⚠️ GPU レンダーパスはプラグイン毎に実装が異なるため個別対応が必要
⚠️ プラグイン側のバグによるクラッシュからホストを保護する仕組みが必要

---

## 推定工数
- Phase 1: 3日
- Phase 2: 2日
- Phase 3: 4日
- Phase 4: 2日
- Phase 5: 3日
- 合計: 14日

---

## 優先順位
🔴 最高優先度: M13 リリース目標
- この機能1つで他の全てのエフェクト実装の優先度が下がる
- VFX パイプラインとの互換性の鍵となる最も重要な機能

---

## 関連ドキュメント

- [`docs/planned/MILESTONE_DCC_FEATURE_GAPS_2026-03-28.md`](docs/planned/MILESTONE_DCC_FEATURE_GAPS_2026-03-28.md)
- [`plans/AFTER_EFFECTS_GAP_ANALYSIS.md`](plans/AFTER_EFFECTS_GAP_ANALYSIS.md)
- [`docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`](docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md)

## 2026-08-15 現行コード監査

OFX header／loader、`ArtifactOfxHost` の property／parameter／image-effect suite、Load／Describe／DescribeInContext、plugin metadata、render instance／begin-sequence、CPU frame-buffer path、`ArtifactOfxEffectImpl` の EffectService 登録と preview property 公開を現行コードで確認した。`paramGetValue`／`paramSetValue` を含む基本 parameter read/write と group/path の保持も存在する。

旧文書の「host に載せる土台のみ」という説明は、CPU effect 経路と parameter bridge の範囲では更新が必要。ただし time-value／key、GPU／GL・CL・DX texture sharing、tiling／multithreading、専用 Inspector、preset／copy、複数実プラグイン互換性、runtime検証は未完了で、OFX 1.4 完全実装・メジャープラグイン動作保証とは判定しない。

## 2026-07-25 実装監査（履歴）

OFX ヘッダー接続、動的ライブラリ走査、`ArtifactOfxHost` の property／parameter／image-effect suite、Load／Describe／DescribeInContext、プラグイン記述の読み込み、`ArtifactOfxEffectImpl` の render instance／begin-sequence／CPU frame buffer 経路、既存 EffectService への `ofx.*` 登録と preview property 公開は実装を確認した。一方、OFX parameter suite の時刻値・キー操作は unsupported のままで、GPU／GL・CL・DX テクスチャ共有、タイル・マルチスレッド契約、専用 Inspector、プリセット／コピー、複数実プラグインでの互換性は確認できない。したがって Phase 1 と Phase 2 の一部、CPU レンダーの基本経路は部分実装、GPU／高度な時間依存・スタック互換は未完了・runtime 未検証とする。
