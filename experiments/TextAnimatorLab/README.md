# TextAnimatorLab

**最終更新:** 2026-08-14

テキストアニメーターの設計を、ArtifactCore本体へ変更を入れる前に検証する実験プロジェクト。

この段階では純粋なPythonリファレンスモデルを使う。現在のArtifactCore実装を期待値として扱わず、設計上の不変条件を先に検証する。

## 実行

```powershell
python experiments/TextAnimatorLab/audit_design.py
python experiments/TextAnimatorLab/audit_design.py --tier smoke
python experiments/TextAnimatorLab/audit_design.py --tier contract --json-out reports/contract.json
python experiments/TextAnimatorLab/audit_design.py --intent intent_examples.json --fixture text_sample1
python -m unittest discover -s experiments/TextAnimatorLab -p 'test_*.py'
```

ArtifactCoreの実行スモーク（Debug）は、QtのDebug QPAプラグインを明示して実行する。
Release版 `qoffscreen.dll` とDebug版 `qoffscreend.dll` を混ぜると、`QGuiApplication`
初期化時に落ちるため、構成を混在させない。

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\vcpkg\installed\x64-windows\debug\Qt6\plugins\platforms'
& '.\build_text_runtime_v2\ArtifactCore\Debug\ArtifactCoreTextSmoke.exe' 'Text Sample1'
& '.\build_text_runtime_v2\ArtifactCore\Debug\ArtifactCoreTextSmoke.exe' 'Text Sample1 🧪'
```

これはまずGlyph状態のJSONを出す実行確認であり、フォントshapingとアニメータの状態を
ArtifactCore上で通す。ピクセル描画の確認はこのスモークが通った後の次段階とする。

実描画確認では第2引数にPNGを指定する。2048pxのAtlas全体は縮小するとglyphが細い
矩形に見えるため、確認時はAtlasのglyph領域を拡大して目視する。

```powershell
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\vcpkg\installed\x64-windows\debug\Qt6\plugins\platforms'
& '.\build_text_runtime_v2\ArtifactCore\Debug\ArtifactCoreTextSmoke.exe' `
  'Text Sample1 🧪' '.\build_text_runtime_v2\artifactcore_text_atlas.png'
```

GPU glyph smoke の成功条件は、PNG を保存できることだけではない。readback 画像に
非透明ピクセルが1つ以上必要で、カラー glyph を含む入力では RGB の非グレースケール
画素も1つ以上必要になる。ログの `nonzeroAlpha`、`colorPixels`、`colorPreserved` を
確認する。これにより、空の透明画像やカラー情報を失った画像を成功扱いしない。

変形用 PSO の確認では通常実行を使う。通常 glyph の回転・scale・opacity が有効になり、
Submitter が変形用 pipeline を選択する。比較用に `ARTIFACT_TEXT_SMOKE_NO_TRANSFORM=1`
を設定すると、同じ入力を非変形 pipeline で実行できる。

```powershell
$env:ARTIFACT_TEXT_SMOKE_NO_TRANSFORM = '0'
& '.\build_gpu_text_clean\Artifact\Debug\ArtifactTextGlyphSmoke.exe' `
  'Text Sample1 🧪' '.\gpu_text_transformed.png'
$env:ARTIFACT_TEXT_SMOKE_NO_TRANSFORM = '1'
& '.\build_gpu_text_clean\Artifact\Debug\ArtifactTextGlyphSmoke.exe' `
  'Text Sample1 🧪' '.\gpu_text_plain.png'
Remove-Item Env:ARTIFACT_TEXT_SMOKE_NO_TRANSFORM
```

## 段階

- `smoke`: 基本文字列、空文字、Emoji、CJK、RTL、combining mark
- `contract`: Selector単位、範囲境界、Animator合成、有限値
- `stress`: 長文と大量Glyph（初期版ではfixture生成の準備）

## 設計モデルと製品実装の比較

```text
fixture
  → DesignModel（期待する設計）
  → ArtifactCore Adapter（後続フェーズ）
  → 差分レポート
```

このLabの合格は、ArtifactCoreが正しいことを意味しない。差分が出たときは、設計モデルと製品実装のどちらを修正するかを判断する。

## 現在の監査範囲

- Pythonの保守的なUnicode grapheme-like clusterモデル（実shapingの代替ではない）
- Percentage / Index / Cluster / Line selector
- Grapheme selection keeps a cluster together while reporting both selected
  rendered Glyphs and selected logical units
- Tag selection requires explicit fixture metadata; missing metadata is a
  warning, not a silent successful selection
- Start / Endの正規化
- Square / RampUp / RampDown / Smooth
- 複数Animatorの決定的な順序適用
- Position / Scale / Rotation / Opacityの有限値検証
- fixtureごとのJSONレポート

JSONレポートには、後続のArtifactCore Adapterと比較するため、`virtualGlyphCount`、`virtualClusterCount`、`virtualLineCount`を含める。

Regexは設計モデルで監査するが、実shaping、font fallback、GPU instance dataの実比較は後続Adapterで追加する。

## AI Intent契約

低価格モデルが低レベルの`text.animators.N.*`を直接操作しないよう、Intent形式を定義する。

- `intent_schema.json`: 構文、enum、上限、型の契約
- `intent_examples.json`: 小さなIntentの例

操作は必ず次の段階に分ける。

```text
inspect → plan → validate → preview → apply
```

`previewOnly`のIntentは製品データを変更せず、対象Glyph数、選択単位、Operator合成、警告を返す。結果は`pass` / `warning` / `error`を区別する。`apply`は複数の低レベルProperty変更を1つのUndo単位として適用する。

単一Intent JSONとIntent JSON配列の両方を受け取る。配列はデフォルトで先頭要素をPreviewし、`--intent-index 1`で別の例を選べる。

### AIに返すべき診断

- `targetNotFound`
- `unsupportedSelectionUnit`
- `selectionEmpty`
- `invalidRegex`
- `operatorConflict`
- `unsupportedOperatorMode`
- `missingOperatorRange`
- `invalidOperatorRange`
- `invalidOperatorColor`
- `operatorValueOutOfRange`
- `timelineOutOfRange`
- `textStructureChanged`
- `gpuInputNonFinite`

Intent契約が固まるまで、WorkspaceAutomationへ新しい公開APIは追加しない。

## 独自機能の優先順

AE互換のパラメータを増やすより、作業時間を減らす機能を先に検証する。

1. Content-aware continuity：文章変更後もToken / Word identityを追跡
2. Layout-preserving motion：重なり・Box外逸脱・安全領域を監査
3. Hierarchical selection：Grapheme / Word / Line / Paragraph / Tag
4. Relational fields：伝播、隣接、中心から外側、同一語同期
5. Procedural operators：Spring、Noise、外部Value Field

独自機能は`preview`の出力に対象、追従、制約補正、警告を含める。設計が曖昧な場合は自動適用しない。

文章変更追従の設計モデルは次で確認できる。

```powershell
python experiments/TextAnimatorLab/audit_design.py --word-diff "Text Sample1" "Text Sample2"
```

一致するWordは追跡し、追加・削除を分ける。重複Wordは曖昧として警告し、無確認の自動適用を避ける。

Layout制約の設計モデルは次で確認できる。

```powershell
python experiments/TextAnimatorLab/audit_design.py --layout-check "[0,8,30]" "[10,10,10]" --box-width 50
```

補正された位置と補正量を返し、負の幅や不正なBoxはエラーにする。

Timingの展開は次で確認できる。

```powershell
python experiments/TextAnimatorLab/audit_design.py --timeline 3 1.0 0.25 0.5 --easing spring
```

回転アニメーションの設計プレビューは、CoreやGPUを使わず次で確認できる。

```powershell
python experiments/TextAnimatorLab/audit_design.py --rotation-demo --fixture text_sample1
python experiments/TextAnimatorLab/audit_design.py --rotation-demo --fixture emoji_sentence
```

同じ入力に対して、文字順、単語単位、中心から外側の3方式を比較する。これは
「回転量」と「選択ウェイト」の責務を分離した設計確認であり、実際の描画品質や
フォント shaping の検証ではない。

Staggerは後続Glyphの開始時刻を遅らせ、開始前・終了後は0〜1へClampする。`spring`は明示的なOvershootを許し、Opacityなどの最終プロパティ側で必要に応じてClampする。不正なDuration、Stagger、Easingはエラーにする。Opacityは`0〜1`、Scaleは`0以上`に制約し、Position / Rotation / Skew / TrackingではOvershootを保持する。

将来のArtifactCore Adapterは、次の形式のsnapshotを比較器へ渡す。

```json
{"GlyphCount": 12, "ClusterCount": 11, "LineCount": 1, "states": []}
```

```powershell
python experiments/TextAnimatorLab/audit_design.py --core-snapshot core_snapshot.json --fixture text_sample1
```

比較器は構造数の差、必須フィールド欠落、GPU/CPU入力へ渡す状態の非有限値をエラーにする。

サンプルsnapshotは`snapshots/`に置いてある。

```powershell
python experiments/TextAnimatorLab/audit_design.py `
  --core-snapshot experiments/TextAnimatorLab/snapshots/text_sample1_core_like.json `
  --fixture text_sample1
```

`bad_structure`と`bad_state`は、比較器が失敗を検出できることを確認するための異常系fixtureである。
