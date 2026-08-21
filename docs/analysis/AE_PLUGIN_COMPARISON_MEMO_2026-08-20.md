**最終更新:** 2026-08-20

# AE／プラグイン比較メモ

## 判定方法

機能名やクラスの存在だけで「対応済み」とせず、次の経路で確認する。

`挙動・UI → Artifactモデル → Render/DAG → Preview/Queue/Export → 保存・再読込`

状態は `実装済み`、`部分実装`、`未接続`、`未確認`、`対象外` に分ける。

## これまでの主な確認結果

| 領域 | 現状 | 主要な不足 |
|---|---|---|
| エフェクト基盤 | カタログ・factory・CPU/GPU実装は広い | GPU-native chain、共通Host Contract、依存・時間・ROIの実行接続、CPU/GPU parity |
| OFX | loader、describe、基本parameter、CPU renderあり | 時刻／keyframe／ROI／Interact／DrawSuite、GPU共有、第三者plugin受入 |
| Temporal | Echo、Feedback、Time Remap、Optical Flow等あり | history/reset、seek/reverse/cache無効化、occlusion/confidence、Queue parity |
| Matte／Mask | Roto、Layer Mask、複数MatteのCore evaluatorあり | 全renderのcanonical化、GPU複数matte、Luminance parity、実素材受入 |
| Adjustment Layer | モデル・UI・効果適用あり | 現在はreadback後のQImage適用。GPU DAG／Preview-Queue共通契約なし |
| Precomp | source composition、時間remap、thumbnail描画あり | 子コンポをQImage化して合成。Collapse Transformations相当なし |
| Blend Mode | enum、CPU分岐、UI、Undo、保存あり | GPU全経路の恒常接続、Dissolve時間挙動、parity |
| Keyer | Chroma/Luma/Difference/IBK、Clean Plate入力あり | Clean Plate生成更新、診断workspace、edge/spill、実素材受入 |
| Particles | emitter、physics、GPU billboard、JSONあり | independent child/secondary、deterministic cache、bake、Queue parity |
| Text／Expression | animator、range selector、expression parserあり。Expression SelectorのCore／評価／Inspector項目は部分実装 | Pick Whip、property path解決、Expression Selectorの完全なinline error／AE互換、Text Layer双方向連携 |
| Tracker | point／planar／cameraの基盤あり | panel、retrack/outlier、failure recovery、real-footage acceptance |
| Work Area | Composition model、Timeline操作、Playback範囲、Render Queue範囲、Automation API、JSON保存・復元あり | 全widgetの状態同期、Preview／Queueのruntime parity、再起動後の実機受入 |
| Layer Style | shadow、bevel、satin、stroke、Color／Gradient／Pattern Overlayの個別effectあり | AE式統一style stack、order、presets、contour UI |
| Color | OCIO/ACES、LUT、scopes、grading基盤あり | production render全面適用、Qualifier、Power Window、node graph、HDR parity |
| AEP/MOGRT | 内部template／保存基盤あり | AEP parser、`.mogrt`互換、media replacement、依存manifest |
| Captions/Markers | SRT/WebVTT、marker model、操作Undo、JSON永続化あり | 専用編集UI、expression API、Text Layer双方向連携 |
| Render/MFR | cache、queue、MFR基盤あり | 実MFRは無効、共有状態安全性、順序・sequential parity |

### Precomp境界の追加確認

`ArtifactCompositionLayer` は時刻remapを子コンポへ渡す基盤を持つが、通常のComposition Viewでは子コンポを `getThumbnailAtFrame()` で `QImage` 化して親へ渡している。`draw()` のno-opはComposition View側が専用描画を担当する設計による意図的なものだが、GPUの子コンポジションsurfaceを親Render Targetへ直接接続してはいない。`Collapse Transformations`／`Continuously Rasterize`相当のレイヤー設定も確認できない。これは単純なフラグ追加ではなく、nested compositionのGPU surface contractが必要な中規模課題と判定する。

### Layer Styleの追加確認

Drop Shadow／Inner Shadow／Bevel／Satin／Strokeに加え、Color／Gradient／Pattern Overlayも個別effectのfactory／catalog／実装を確認した。一方、AEのLayer Styleのように一つのstackとして順序・全体enable・Contour・copy/paste・style presetを扱う共通モデル／Inspector導線は確認できない。したがって「個別effectのカタログ対応」と「Layer Style parity」は分けて扱う。

### AEP preflightの次段階設計

parser本体へ進む場合は、最初からAEP全機能を変換せず、読み取り専用のpreflight結果を先に作る。最低限の変換対象は `composition`（名前・幅・高さ・frame rate・duration）、`layer`（id・name・type・in/out・transform）、親子関係とする。各項目は `supported`／`unsupported`／`invalid` を持つ診断エントリにし、未対応のeffect、source、expression、media replacementは入力全体を黙って捨てず、ファイル名・composition ID・layer ID付きで報告する。

実装境界は、AEP/AEPXのバイナリ／XML読込、Artifact JSONへの変換、既存 `ArtifactProjectImporter` への登録を分離する。`.mogrt` はAEP parserの別名として扱わず、manifest・template metadata・media replacementの契約が確認できるまで独立したunsupported診断を維持する。runtimeでの変換受入れとround-trip検証ができるまでは、preflight結果をimport成功と報告しない。

最小結果モデルは `format`、`sourcePath`、`compositions[]`、`issues[]`、`canImport` とし、Composition項目には `externalId`・`name`・`width`・`height`・`frameRate`・`duration`、Layer項目には `externalId`・`name`・`type`・`parentExternalId`・`inFrame`・`outFrame`・`transform` を持たせる。`issues[]` は severity、code、message、compositionExternalId、layerExternalId を持ち、未対応項目が一つでもある場合は `canImport=false` とする。受入れ条件は、同一入力の再実行で診断順序が安定すること、外部IDが重複しないこと、unsupportedをimport成功として扱わないこと、Artifact JSONへの変換後にcomp／layer数とtransformの基本値が一致することとする。

### Expression Selectorの最小拡張案

現行の`TextAnimatorState`は`RangeSelector`と`WigglySelector`を値保持し、`SelectorEvaluationContext`はsource textとglyph列を既に受け取れる。一方、Expression Selector専用の状態・評価入口は未確認だった。追加する場合は既存RangeSelectorを置換せず、`ExpressionSelector`（enabled、expression、seed、diagnostic）を別状態として持ち、各glyph評価時に`textIndex`（1始まり）、`textTotal`、`text`、`selectorValue`だけを読み取り専用コンテキストへ渡す構成が最小である。式の失敗は全体描画を止めず、selector単位のinline diagnosticとゼロウェイトへフォールバックする。UI・保存・評価を同時に広げる必要があるため、小規模修正ではなく次段階の中規模課題と判定する。

## 小さく着手しやすい候補

1. Preset JSONのthumbnail／schema情報を保存対象に含める。単一effectのround-trip修正で、影響範囲が限定的。
2. Composition MarkerのExpression／Text Layer連携を統合する（Timelineの基本表示は実装済み）。
3. Matte／Precomp／AdjustmentのCPU readback箇所に診断情報を追加し、未接続経路を可視化する（Matte／Precomp／Adjustmentの初期診断まで実装済み）。
4. Blend ModeのGPU未接続モードを明示的にfallback表示する（診断ログ実装済み）。
5. AEPは基本comp／layer／transformのimport preflightを拡張する（基本JSON preflightは部分実装済み、AEP parserは未実装）。

## 大きい項目（先送り）

- GPU-native effect DAGと共通Host Contract
- GPU上のcanonical Matte evaluator
- Adjustment LayerのGPU合成
- Collapse Transformations／nested 3D境界
- OFX完全互換・Interact UI

これらは実装量だけでなく、Preview／Queue／Exportの共通レンダー契約を変更するため、単発のeffect追加より後に扱う。

## 次の実装順

### 実装済み（2026-08-20）

- `ArtifactEffectPreset` のthumbnailをBase64でJSON保存・復元
- Preset schema versionを3へ更新し、version 2からのmigrationを追加
- 空のPreset IDを読み込んだ場合に生成UUIDを保持
- `ArtifactPresetManager` でPresetの`effect_id`と適用先effectを検証
- `ArtifactPresetManager` で異なるeffectへのPreset誤適用を拒否
- GPU Blend Pipelineで未登録モードがNormalへfallbackする際の診断ログを追加
- SoloレイヤーのPreview dirty-cache判定と非Solo除外経路を確認
- Matte評価で欠落source IDを1回の評価単位に集約して診断ログへ出力
- Matte欠落診断の同一source組み合わせを限定キャッシュで重複抑制
- Composition Viewの既存debug文字列へMatte欠落source情報を毎回付加（ログ本体は限定キャッシュで重複抑制）
- Marker操作で実際に状態が変わった場合だけUndo snapshotを追加し、空削除・対象なし削除のUndoノイズを抑制
- Effect出力欠落／サイズ不一致からCPUへ再実行する既存fallbackをFallbackTrackerへ記録
- Marker JSON読込で不正なpositionを0フレームへ誤配置せず、該当項目をスキップ
- Marker XML読込は既存コードでframeの数値検証済みであることを確認（追加変更なし）
- Layer Styleは個別EffectのID／factory／catalogは確認できたが、共通stackの単純な既存拡張点は確認できず、中規模課題として保留
- Preset parameter読込で非object・空名・型情報なしの不正項目をスキップ
- PresetのColor parameter読込で無効な色値を登録しないよう検証
- Preset collectionのファイルサイズ（16 MiB）・件数（100,000）制限は既存実装で確認（追加変更なし）
- Preset collection読込時の重複ID上書きを既存挙動のまま警告ログ化
- Adjustment LayerのComposition View readback境界をレイヤーID単位で限定診断ログへ記録
- Precompのchild composition thumbnail（QImage）サンプリング境界をレイヤーID単位で限定診断ログへ記録
- 未ロードOFX／未登録Effect factoryのgeneric fallbackをFallbackTrackerへ記録
- Effect manager未初期化で生成をスキップする経路もFallbackTrackerへ記録
- Project importerの`validateFile()`でAEP／AEPX／MOGRTを未対応形式として明示診断（parserは未実装のまま）
- 実際の`importProject()`入口でもAEP／AEPX／MOGRTをJSON読込前に拒否し、明示的なエラーを返す
- `getFileFormatVersion()`でも未対応形式を`unsupported.<extension>`として返し、`unknown`へ埋没させない
- AEP／AEPX／MOGRT拒否時に`ProjectHealthReport`へ`UnsupportedInterchangeFormat` issueを追加（公開API変更なし）
- Playback engine／controller未接続時もComposition Marker／Chapterの前後移動を`ArtifactInOutPoints`へフォールバック
- Playback Service未接続時のShortcut直接Marker操作にも既存snapshot Undoを適用
- Composition Markerをkeyframe markerと分離した読み取り専用visualとしてTimeline painterへ表示
- Composition切替／変更時にComposition Marker visualを再供給
- 無効なCompositionへの切替時に前CompositionのMarker visualをクリアし、stale表示を防止
- Marker追加／削除時に既存の`PlaybackInOutPointsChangedEvent`を再利用し、Timeline visualをqueued refresh（Marker専用event／signalは追加していない）
- Composition切替開始時に旧CompositionのMarker／Composition変更購読を解除し、無効切替後のstale refreshを防止
- Markerのcomment／type／color／link／tag属性変更も既存event busへ通知し、visual live refresh対象に統合
- Composition Markerはズーム時のみcommentラベルを表示し、通常ズームのタイムライン密度を維持
- Composition MarkerのChapterをコメントMarkerと別形状（ダイヤ）で描画し、既存のchapter属性を表示へ反映
- Composition MarkerのTimeline変換前に非有限frameを除外し、異常座標の描画を防止
- Project importerの基本preflightで入力ファイルの存在・通常ファイル・読取可否、JSON object root、空でないstring `name` を検証
- 同preflightで存在する `compositions` が配列で、各エントリがobjectであることを検証
- Compositionエントリの`id`を検証し、importer内での暗黙スキップを事前診断
- Composition `id` の重複もpreflightで拒否し、参照先の曖昧化を防止
- Composition内に存在する `layers` が配列で、各エントリがobjectであることをpreflightで検証
- 既存Layer `id` の型・空文字・重複を検証（IDなしの旧形式は許容）
- Layer `parentId` が存在する場合の型（string）を検証
- 非空のLayer `parentId` が同一Composition内の既知Layerを指し、自身を指さないことを検証
- Layerの親参照が循環していないことをpreflightで検証
- preflightのComposition件数上限を実import側の10,000件上限と一致
- CompositionごとのLayer件数にも100,000件のpreflight上限を設定
- Project `version`／`minVersion` が有限な非負数文字列であることをpreflightで検証
- `minVersion` が `version` を超える矛盾もpreflightで拒否
- 標準JSONの`importProject()`入口から上記preflightを実際に呼び出し、失敗時は`InvalidProjectPreflight` health issueを返す
- Text／Expressionについて現行ソースを再走査し、Expression SelectorのCore／評価／Inspector部分を追加。Pick Whip、property path解決、完全なinline error／AE互換、Text Layer双方向連携は未実装
- Expression Selectorの初期土台として`SelectorEvaluationContext`に任意の`textIndex`／`textTotal`を追加（既存Range評価はデフォルト値で挙動不変）
- `ExpressionSelector` Core状態（enabled／expression／seed／diagnostic）を追加し、Text Animator stateから保持可能にした（JSON/UI/evaluation接続は未実装）
- Text Animator JSONでExpression Selectorのenabled／expression／seedを保存・復元し、round-trip比較対象へ追加
- Expression Selectorの復元式文字列をtrim・最大16,384文字に制限
- Expression Selectorの復元seedを-1,000,000〜1,000,000に制限
- Expression Selectorの復元`expression`型を検証し、非string値は無効化してdiagnosticを保持
- enabled状態で空のExpression Selector式を復元した場合も無効化してdiagnosticを保持
- `TextAnimatorEngine::evaluateExpressionSelector()` を追加し、各glyphへ`textIndex`／`textTotal`／`text`／`seed`／Range結果由来の`selectorValue`を渡して有限数を0〜1へclampするCore評価境界を実装
- Text Layerのper-glyph animator stackでExpression Selector weightsをRange／field weightsと乗算する経路を接続（既存expression無効時は従来挙動）
- Expression評価時のdiagnosticは`SelectorResult`に保持し、描画中に永続Animator stateへ書き戻さない。Overviewのruntime error反映は専用の副作用なし状態通知が必要なため未実装
- 既存Text Animator Inspector property groupへExpression Selectorのenabled／expression／seed編集項目を追加（専用UIは新設せず、既存保存経路を再利用）
- 既存Selector OverviewにExpression Selector状態（off／on／error）を追加表示
- InspectorでExpression Selectorを有効化する場合も空式を拒否し、JSON復元と同じdiagnostic規則を適用
- Layer Styleについて個別EffectのCPU／GPU実装とPreset登録は確認済み。共通stackの保存・順序を小さく補う既存経路は見つからず、個別Effect側への追加変更は見送る

Preset round-trip、Marker Undoノイズ抑制、Matte／Effect／Blend fallback診断、Composition MarkerのTimeline基本表示、Expression SelectorのCore／評価／Inspector部分を実装した。Pick Whip／Text Layer双方向連携、AEP parser、GPU-nativeなPrecomp／Adjustmentは、既存の責務境界をまたぐ次段階の中規模以上の課題として残す。ビルド・テストはAGENTS.mdの指示どおり、明示許可を得るまで実行しない。
