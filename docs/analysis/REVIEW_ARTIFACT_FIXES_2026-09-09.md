# `review-artifact-fixes` 精査台帳

**最終更新:** 2026-09-09

## 調査対象・スナップショット

Artifact のローカルブランチ `codex/review-artifact-fixes` を、現在の `main` と比較する。親子リポジトリのmain統合後に行う読み取り専用レビューであり、ブランチの削除・コミットの再適用はこの台帳では行わない。

## 判定基準

| 判定 | 意味 |
|---|---|
| main相当 | 現在のmainに同じ変更、または同等の実装が存在する |
| 候補 | 現mainにないが、現行責務と境界が明確で再検証する価値がある |
| 旧実装依存 | 古いAPI、古いUI構造、古いモジュール構成に依存している |
| 保留 | 差分の影響が大きく、ビルド・実機確認なしに採否を決められない |
| 却下候補 | 現行仕様・AGENTS制約・後続修正と明確に衝突する |

## 初期観測

- ブランチ先端は `21fedda0`（2026-06-16）。mainより古い別系統で、mainとの差分だけでも462コミットある。
- 直近には Shape Operator、Compositionプリセット、Timelineショートカット、コマンドパレット、Solo操作、設定・Layer Menu修正が含まれる。
- mainへの採否はコミット単位ではなく、現行の責務・入力経路・Undo・モジュール境界・既存仕様との整合で判断する。
- ビルド、CMake再生成、テスト、実機操作は未実施。したがって動作可否は未確認として扱う。

## 第1バッチ（先端8コミット）

| コミット | 内容 | main照合 | 判定 | 次に見る点 |
|---|---|---|---|---|
| `21fedda0` | 設定変更後にCompositionショートカットを再読込、Tracker設定を保持 | `refreshCompositionCycleShortcuts` はmainにない。Tracker修正も枝固有 | 候補 | MainWindowの現行設定反映経路、ApplicationShortcutの競合、既存ShortcutBindings |
| `9c53f8ef` | Shape Operatorのinsert/take/remove/moveとUndo | mainに同名APIなし | 候補・保留 | 現行Shape責務、独自コンテナ、Undo snapshot、Property Editor責務 |
| `99d0978e` | New Compositionの前回プリセット記憶 | mainに同名記録が見当たらない | 候補 | 現行AppSettingsとプリセット名変更時の扱い |
| `1ab834b8` | Shift+Home/EndでレイヤーIn/Outをスナップ | mainに同名ShortcutIdの痕跡なし | 候補・保留 | 現行TimelineKeyBindingと予約キー、Undo単位、Work Area仕様 |
| `e5868fbc` | Ctrl+Kへ編集コマンドを追加 | mainにも`showCommandPalette`はあるが、追加項目の一致は未確認 | 保留 | 現行Paletteの責務とEdit Menuとの重複 |
| `33df1805` | Alt+[ / Alt+]でComposition切替 | mainに`refreshCompositionCycleShortcuts`はない | 候補・保留 | 既存ショートカット予約、フォーカス、Composition順序 |
| `221c4d0b` | Sで選択レイヤーSolo切替 | mainに同名ShortcutId/APIなし | 候補・保留 | `S`予約操作との衝突、複数選択、UndoとDirty更新 |
| `2c9ecb34` | Home/EndでComposition In/Outへ移動 | mainに同名ShortcutId/APIなし | 候補・保留 | Work Area・Composition範囲の正本、Shift派生キーとの整合 |

このバッチでは、`main相当` と断定できるコミットはなかった。候補の採否は、古いコードをそのまま戻すのではなく、現mainのサービス・Undo・ShortcutBindingsへ差分を移し替えられるかで判断する。

## 第2バッチ（Timeline・編集・エフェクト）

| コミット | 内容 | main照合 | 判定 |
|---|---|---|---|
| `5b50273a` | Timeline右ペイン編集の整理 | 同名ファイルはmainにもあるが、実装差分は未比較 | 保留 |
| `c5de053c` | Repeat Last Action、Recent Compositions、UndoManager拡張 | 関連ファイルはmainにもあるが、枝の追加内容が同等か未確認 | 保留 |
| `5d6e8d83` | HLSL・cppm整理、トラッカー／マネージャーのモジュール化 | mainにも関連モジュールが存在 | 旧実装依存の疑い |
| `c0b08cd9` | Color Correction GPU/HLSL parity | mainにもChannelMixer等の実装が存在 | 現main版との比較が必要 |
| `6557e324` | Precompose時の親子時間サンプリング | mainにもComposition/View描画実装が存在 | 現main版との比較が必要 |
| `fc0ef378` | Composition Editorナビゲーション改善 | mainにも関連Service/APIが存在 | 旧API依存の疑い |
| `faf8c541` | AI・レンダー・モジュール整理 | mainにもWorkspaceAutomation等が存在 | 旧実装依存の疑い |
| `e6993039` | Color Correction compute実装 | mainにも同名エフェクト群が存在 | 現main版との比較が必要 |
| `c2cd3f15` | Studioアイコンとプレビュー修正 | mainにも多数のアイコンが存在 | 重複・置換済みの可能性 |
| `e07416b6` | Glow系エフェクト追加 | mainにも同名Glowソースが存在 | main相当か内容比較が必要 |

第2バッチは、ファイルがmainに存在するだけでは採用済みとは判断できない。特にエフェクトとUndoは、後続版で置換されている可能性が高いため、次の段階でシンボル単位の差分とCMake登録を確認する。

## シンボル照合メモ

- `main` には `removeShapeOperatorAt` / `moveShapeOperator` と `LayerEditorContextMenu` 経由の操作がある。一方、`review-artifact-fixes` の `insertShapeOperator` / `takeShapeOperator` と同じAPIは見つからない。Shape Operator機能は部分的に後続実装へ置換済みと判断できる。
- `CompositionNext` / `CompositionPrevious`、`TimelineSoloSelected`、`SnapInToStart`、`JumpToInPoint`、`refreshCompositionCycleShortcuts` はmainで未検出。ショートカット群は未統合候補だが、現行の予約キー規則を先に確認する必要がある。
- `recentCompositionPreset` はmainで未検出。前回プリセット記憶は未統合候補だが、現行AppSettingsの正本を確認してから判断する。
- 現mainでは `S` がTimelineのSlide tool（`ArtifactTimelineWidget.cppm`）として扱われ、Home/EndもPlayback／View Fit経路で使用されている。したがって `221c4d0b` のSolo、`2c9ecb34` のHome/Endは、そのまま採用すると既存操作と衝突する。
- `9c53f8ef` のShape Operatorは、現mainの `removeShapeOperatorAt` / `moveShapeOperator` と責務が重なるため、insert/takeだけを現行Undo経路へ再設計するのが候補になる。

## 精査順

1. 直近のUI・操作コミット（`21fedda0` からTimeline／Shape関連）
2. Undo・入力・ショートカットの境界
3. 古いレンダー・モジュール依存を含む中間コミット
4. それ以前の履歴は、現mainとの差分が残るファイル単位で重複を除いて確認する

## 受入基準

| ID | 基準 | 状態 |
|---|---|---|
| ACC-001 | 現mainにない変更をコミット単位で特定できる | 確認中 |
| ACC-002 | 各候補について現行API・責務・Undo経路との整合を確認できる | 未確認 |
| ACC-003 | 採用候補にビルド・実機確認が必要な点を明記できる | 未確認 |
| ACC-004 | 却下・保留理由をコミットとファイルへ追跡できる | 未確認 |

## 証拠索引

- ブランチ履歴: `git -C Artifact log codex/review-artifact-fixes`
- mainとの差分: `git -C Artifact log main..codex/review-artifact-fixes`
- 主要実装: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- 制約: `AGENTS.md`、`docs/WIDGET_MAP.md`

### 実装開始
- 99d0978e のうち、現行 main と競合しない Composition preset の前回選択記憶を ArtifactFileMenu::handleNewComposition() に移植。既存 QSettings を使用し、名前入力キャンセル時は保存しない。
- Shape Operator / ショートカット群は引き続き個別レビュー待ち。
- コマンドパレットは現行 `StandardActionRegistry::registerAll()` を基盤にしており、旧コミットのダミー登録を移植する必要はない。現行の ActionManager に存在しない操作だけを追加対象とする。
