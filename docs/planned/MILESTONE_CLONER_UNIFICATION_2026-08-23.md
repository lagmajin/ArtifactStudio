# MILESTONE: Cloner 長期統合計画

**最終更新:** 2026-08-23

2026-08-23 のクローナー領域ウォーク（`CloneCore` → `CloneGenerator` → `ArtifactCloneLayer` → `ArtifactCloneEffectSupport`）で判明した課題を、移行フェーズ付きで整理する。

**設計方針（確定）:** クローナーは **エフェクトではなくレイヤーコンポーネントとして扱うのが正**。Effect時代の `CloneGenerator` は互換アダプタに留め、新規オーサリング・実行・UIはすべて Component 経由に統一する。本計画の全フェーズはこの方針に従う。

## Phase 4 — 細部修正バックログ（2026-08-23 実装済み）

| 項目 | 対応 |
|---|---|
| fps ハードコード解除 | `ArtifactCloneEffectSupport.ixx` の `/30.0f` 5箇所を `layer->compositionFrameRate()` 経由に置換済み |
| PoissonDisk 改良 | `CloneGenerator.cppm` Random分布を O(n²) 全走査から uniform spatial hash（3x3x3近傍判定）へ置換済み。64試行のrejection構造は維持 |
| `blendCloneData` 意味論文書化 | `CloneCore.ixx` に各ブレンドモード（Multiply/Max/Min/Average等）の数学的意味と原点基準の注意を追記済み |
| `mt19937` → `RandomStream` 統合 | `ArtifactCloneLayer.cppm` Random分布を `ArtifactCore::RandomStream` へ置換済み。**legacy `CloneGenerator` 側は意図的に mt19937 を維持**（旧プロジェクト出力の互換保護）。乱数シード同一でもレイヤー側の出列順が変わる点に注意 |

## Phase 4.5 — Effector バグ修正と時間供給（2026-08-23 実装済み）

| 項目 | 対応 |
|---|---|
| `RandomCloneEffector::hash` の符号付き整数オーバーフローUB | `AdvancedEffectors.ixx` — uint32 ラップアラウンド演算に置換 |
| Grid / Radial モードで `opacityDecay` が無視される | `ArtifactCloneLayer.cppm` — 両モードに opacityDecay を適用（Grid は clone.index 基準、Radial は i 基準） |
| Radial モードが endAngle に到達しない（Curve との不一致） | `(end-start)/total` → `(end-start)/(total-1)` に変更。回転も Curve と同一の `angle + rotationStep*i` に統一 |
| legacy 経路で identity 変換クローンが消える | `ArtifactCloneEffectSupport.ixx` — offset=0 の1個目等が `isIdentity()` スキップで消える問題を解消（visible のみ判定） |
| `SoundCloneEffector` の色合成で alpha が失われる | `setRgbF(r,g,b)` → alpha 保存版へ |
| **Delay/Noise/Sound Effector が時間パラメータ未供給で実質停止** | `ArtifactCloneLayer::generateCloneData()` が effector 適用前に `currentTime`（timeline連動）/ `deltaTime`（1/fps）を dynamic_cast で供給するようになった。波状伝播・ノイズアニメーションが実際に動く |

### Phase 4.5 の既知残課題

- `SoundCloneEffector` の `audioAmplitude/audioLowBand/...` は未接続（常時0）。`ArtifactAudioLayer` の解析結果（`AudioAnalyzer`）をフレーム毎に流す配線が必要 → Phase 3 の Effector modifier 化と合わせて対応。
- `NoiseCloneEffector::noise()` は sin合成の擬似ノイズで、sampleY/Z が全クローン共通のため実質1D波。パーリンノイズへの差し替え候補。
- Inspector ComponentTab から Delay/Sound/Noise Effector を追加する UI 導線は未整備（`addEffector` API は存在）。
- Radial の終端契約変更により、既存プロジェクトの Radial 配置見た目が変わる（最後のクローンが初めて endAngle に到達）。

## Phase 5 — エフェクターチェーンの実用化（直列→並列→ネスト）

2026-08-23 の追加調査で判明した現状: 実行構造は「effectors_ を順次 applyToClones する直列チェーン」だが、(a) `addEffector()` 呼び出し元ゼロ（追加手段なし）、(b) 保存/復元は front の TransformCloneEffector のみで2個目以降は消える、(c) `blendMode`/`blendCloneData` は呼び出し元ゼロの死蔵機能、(d) 基底 `strength` メンバーをどの実装も参照しない（Inspector の Strength ノブが無効）、(e) クローナーのネスト（CloneLayer を別 CloneLayer のソースに）は未対応。

| サブフェーズ | 内容 | Scope |
|---|---|---|
| **5.1 直列チェーン実用化（2026-08-23 実装済み）** | ① `clone.effectors` 配列シリアライズ実装（typeId タグ: transform/step/random/delay/sound/noise。legacy `clone.effector.*` は書き込み併記＋読み取りフォールバック）。② 基底 `strength` を全6実装の finalWeight に乗算し Inspector ノブを有効化。③ typeId ファクトリで fromJson から任意種を復元 | 完了 |
| **5.2 並列合成（2026-08-23 ランタイム実装済み）** | `generateCloneData()` の effector 適用ループで、`blendMode == Add`（デフォルト）は従来の in-place 変異パスを維持。それ以外のモード（Subtract/Multiply/Max/Min/Average/Normal）はスナップショットに対して候補変形を計算し `blendCloneData(base, candidate, mode, 1.0)` で合成。**契約:** blendCloneData が合成するのは位置・色・weight のみで、非Addモードでは回転/スケールは base 値フォールバック（文書化済み）。UI の blendMode ドロップダウンは未実装（JSON/API で設定可） | ランタイム完了・UI残 |
| **5.3 ネスト（クローナー内クローナー）** | Phase 2（Cloneレイヤー実体描画）の完了が前提。ソース解決が CloneLayer 自身を返しても再帰深度上限（推奨4）で展開。循環参照は解決失敗として無音スキップ | Large |
| **5.4 UI導線（2026-08-23 実装済み）** | Inspector Components セクションに「+ Effector / - Effector」ボタン行を追加。+は6種タイプのQInputDialog選択で `addEffector()`、-は「index: type」一覧から選んで `removeEffector(index)`。ボタンは CloneLayer 選択時のみ表示、カウント表示付き。AbstractCloneEffector に `effectorTypeName()` 仮想関数を追加 | 完了（エフェクター個別のProperty編集面は後続） |

依存順序: 5.1 → 5.4（UIで作れる）→ 5.2（並列）→ 2（ソース描画）→ 5.3（ネスト）→ 3（modifier統合）。

**Phase 5.1 の既知残課題:** Inspector にエフェクター追加 UI が無いため、現状 API/JSON 経由でのみ複数チェーンを作成可能（5.4 で解消）。`blendMode` は保存されるが 5.2 まで実行に反映されない。

## 現状の構造

クローン生成は3系統が並存する。

| 系統 | 実装 | 状態 |
|---|---|---|
| (a) Effect時代の `CloneGenerator` | `src/Generator/CloneGenerator.cppm:345` | 旧プロジェクト互換アダプタのみ（`ArtifactCloneEffectSupport.ixx:804-851`） |
| (b) `ArtifactCloneLayer` 独自生成 | `src/Layer/ArtifactCloneLayer.cppm:324` | CloneMode 6種。描画は矩形プレースホルダ（`:166`） |
| (c) Component generator stack | `clonerComponentInstances`（Support `:633`） | 正となるべき新経路 |

描画統合点は `drawWithClonerEffect`（Support `:894`）。Image/Shape/Text/Video/Solid/SVG 各レイヤーが複製描画に対応済み。

## Phase 1 — 生成系統の一本化（正 = Component）

**目標:** (a)(b) を読み取り専用互換に降格し、新規オーサリングと実行を (c) に統一。

1. `ArtifactCloneLayer::generateCloneData()` を廃止し、内部で `clonerComponentInstances()` に委譲。`settings_` は起動時に Host descriptor へ移行変換（`syncBuiltinBoolsFromHost` と同型の逆方向マッピング）。
2. `CloneMode`(6種) ↔ `DistributionMode`(8種) の対応表を作り、`clone.mode` JSON 読み込みで descriptor settings へ翻訳。欠落モード（Hexagonal/Noise/Spiral/PoissonDisk）は `extraGeneratorDescriptors_` 側へ。
3. legacy Effect アダプタ（Support `:806-851`）に非推奨ログ（`qWarning` 一回り）を追加し、削除予定バージョンを明記。
- **Scope:** Medium / **リスク:** 既存プロジェクトの見た目変化。変換テスト必須。

## Phase 2 — Cloneレイヤーの実体描画

**目標:** `drawSolidRectTransformed` プレースホルダ（`ArtifactCloneLayer.cppm:166`）を、`sourceLayerId` 参照レイヤーの実描画に置換。

1. `sourceLayerId` → コンポジション解決 → ソースレイヤーのラスタ表面取得（既存キャッシュ経路を再利用）。
2. `drawWithClonerEffect` のインスタンス毎に `drawSpriteTransformed`（GPU テクスチャキャッシュ統合は Image/Video レイヤーと同一経路）。
3. MaterialContainer の `sourceIndex` はフレームバリアント選択に対応。
- **Scope:** Medium-Large / **依存:** Phase 1（インスタンス供給元の統一後が安全）。

## Phase 3 — Effector のスコープ非依存化

**目標:** `AbstractCloneEffector`（`CloneCore.ixx:159`）資産（Basic/Advanced 5種+Sound/Noise/Delay）を全レイヤーのクローン配列に適用可能にする。

1. `AbstractCloneEffector` を `LayerModifierDescriptor` の一種として Host に登録（typeId `artifact.modifier.clone-effector.*`）。
2. `ArtifactCloneEffectSupport.ixx:858-863` の dynamics 適用ブロックに effector チェーンを追加（現状 Crowd/Physics/Fields/Collision のみ）。
3. `ArtifactCloneLayer.cppm:467` の `useEffector` 分岐は modifier 化された effector へのエイリアスに降格。
- **Scope:** Medium / **価値:** Sound/Delay Effector が Shape/Text 等でも動く。

## Phase 4 — 細部修正バックログ

| 項目 | Evidence | Scope |
|---|---|---|
| fps ハードコード解除 | `ArtifactCloneEffectSupport.ixx:655,686` `currentFrame()/30.0f` | Small |
| PoissonDisk 改良（Bridson法 or グリッド加速） | `CloneGenerator.cppm:496` rejection 64回 | Small-Medium |
| `blendCloneData` の Multiply/Max/Min の意味論文書化 | `CloneCore.ixx:111-129`（位置ベクトルへの要素積は直感とズレる） | Small |
| Random 分布の `std::mt19937` → `RandomStream` 統合 | `ArtifactCloneLayer.cppm:377`（乱数統合方針に合わせる） | Small |

## 検証方針

- Phase 1: 変換前後で `generateTransforms()` の出力行列を比較する GTest（同seed・同設定で許容誤差 1e-4）。
- Phase 2: Image/Shape ソースの複製が GPU/CPU 両パスで同一見た目（render-parity-validation スキル活用）。
- Phase 3: Delay Effector を Text レイヤーに適用し timeOffset が GPU instance data（`:934` `gpuInstance.timeOffset`）まで伝播することを確認。
