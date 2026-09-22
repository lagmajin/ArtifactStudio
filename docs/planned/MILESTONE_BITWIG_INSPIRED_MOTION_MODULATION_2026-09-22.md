# Bitwig-inspired Motion Modulation and Reusable Animation Blocks

**最終更新:** 2026-09-22
**ステータス:** In Progress (Phase 0 監査・契約固定済み → Phase 1 待ち)
**対象:** ArtifactStudio / ArtifactCore

## 目的

Bitwig Studio の「変調を任意の値へ接続する」「オートメーションを再利用可能なクリップとして扱う」「条件付きイベントと手続き信号を組み合わせる」という設計パターンを、モーショングラフィックス向けに独立実装する。

Bitwig Studio の UI、実装、アセット、名称を移植するものではない。既存の ArtifactStudio の Property、Animation、Component、Particle、Fluid、GPU レンダリング境界へ適合する、ライセンス上独立した設計として実装する。

## 参照した設計パターン

- Unified Modulation: LFO、Envelope、Noise、Steps、Macro、Audio Follower、Math などを任意のパラメータへ割り当てる。[公式ガイド](https://www.bitwig.com/userguide/latest/the_unified_modulation_system/)
- Automation Clips: カーブのループ、伸縮、オフセット、独立時間、保存、再利用。[Bitwig Studio 6 Release Notes](https://downloads.bitwig.com/6.0/Release-Notes-6.0.html)
- The Grid: 入力、時間信号、数学処理、出力を接続するモジュール方式。[公式ページ](https://www.bitwig.com/the-grid/)
- Operators: 確率、周期、繰り返し、イベント間関係による制御。[公式ガイド](https://www.bitwig.com/userguide/bws44-504/operators/)
- Alias Clips: 同じパターンを複数箇所から参照し、Unique 化も可能にする。[Bitwig Studio 6 Release Notes](https://downloads.bitwig.com/6.0/Release-Notes-6.0.html)

## 価値

- 同じアニメーションカーブを複数レイヤー、複数プロパティで再利用できる。
- LFO やノイズを固定のエフェクト機能ではなく、位置、色、Opacity、粒子量などの共通変調源として扱える。
- Seed 付きの確率・周期制御により、手続き的でも再現可能な表現を作れる。
- Property Editor、Timeline、Components、Particle、Fluid の編集経路を同じ評価契約へ寄せられる。

## 実装フェーズ

### Phase 0 — 現行基盤監査と契約固定

- `Property.Abstract`、`Animation.Value`、既存の envelope / dynamics、Audio Modulation Router、Particle / Fluid の入力点を棚卸しする。
- 評価単位を `frame` / `subframe`、値の型、範囲、補間、Seed、品質段階として定義する。
- UI 用の表示値と、レンダー用の評価値を分離する。
- 既存の `ArtifactPlaybackService` を時間の唯一の所有者として維持する。

完了条件:

- 変調評価がどのパラメータからも同じ snapshot 契約で呼び出せる。
- GPU / ソフトウェア経路が同じ評価済み値を受け取る境界が文書化される。

### Phase 1 — 共通変調源

最初に以下だけを実装する。

- `Constant`
- `LFO`
- `Envelope`
- `Noise`（Seed 付き）
- `Steps`
- `Macro`
- `Math / Remap / Clamp`

候補契約:

```text
ModulationSource::evaluate(ModulationContext) -> Scalar / Vector / Color
ModulationBinding(source, destination, amount, offset, range, blendMode)
```

初期の destination は Transform、Opacity、Color、Particle emission、Fluid strength に限定する。任意の C++ callback や無制限の動的実行は導入しない。

完了条件:

- 1つのソースを複数 destination へ割り当てられる。
- Modulation amount を 0 にすると元のアニメーション値へ完全に戻る。
- 同じ Seed、同じ frame、同じ入力で CPU / GPU 用 snapshot が一致する。

### Phase 2 — 再利用可能な Automation Clip

`AutomationClip` を独立データとして追加する。

- 時間範囲とポイント列
- 補間曲線 / curvature
- loop、stretch、offset
- free-time / parent-follow 相当の時間ポリシー
- Seed と編集履歴用の安定 ID
- Property 型への remap

保存形式は既存のプロジェクト JSON / バイナリ境界に合わせ、カーブ評価のホットパスで再確保しない。曲線の編集はコールドパスで行い、再生時は事前評価済みまたは容量確保済みの snapshot を使う。

UI は最初から専用の巨大エディタを作らず、Timeline の選択範囲を `Automation Clip` 化し、Property Editor から割り当て、再利用、Unique 化できる最小導線とする。

完了条件:

- Transform / Opacity のカーブをクリップ化して保存・再読込できる。
- クリップを別レイヤーへ適用しても元のレイヤーのキーを破壊しない。
- loop / stretch / free-time の結果が Timeline、Viewport、Render Queue で一致する。

### Phase 3 — Alias / Unique とプリセット

- 同一 `AutomationClipPattern` を複数クリップから参照する。
- Alias 編集は参照先全体へ反映する。
- `Make Unique` で対象だけを独立コピーする。
- カーブブラウザ、カテゴリ、プレビュー、最近使ったカーブを追加する。

完了条件:

- Alias と Unique の違いが UI 上で明確である。
- 削除・保存・Undo / Redo・プロジェクト再読込で参照切れを起こさない。

### Phase 4 — Audio Follower と外部入力

- 音量、帯域、Transient、Envelope Follower を変調源にする。
- 音声解析結果はフレーム評価用 snapshot として扱い、レンダー中の不定な再解析を避ける。
- Audio Reactive はまず Scale、Opacity、Particle emission のみに限定する。

動画デコードや音声機能全体を拡張するフェーズではない。既存の Audio / Preview 経路を利用し、モーショングラフィックスの入力として必要な最小分析だけを接続する。

### Phase 5 — Operators 相当の条件付きイベント

粒子、リピーター、クリップ再生、トランジションに対して以下を提供する。

- Chance
- Repeat / Burst
- Cycle / Recurrence
- First / Later / Previous に相当する条件
- Seed と deterministic replay

ランダム値は毎フレーム直接生成せず、イベント ID、サイクル番号、Seed から決定的に求める。Undo、再生、書き出し、再読込で同じ結果になることを必須とする。

### Phase 6 — 限定用途の Modulation Graph

The Grid 全体を再現せず、まず以下のノードだけを提供する。

- Time / Frame / Phase
- LFO / Envelope / Noise
- Math / Mix / Clamp / Remap
- Audio Follower
- Property Output
- Particle / Fluid Output

ノードグラフはコンポジションまたはコンポーネントに所有させ、評価グラフは immutable snapshot としてレンダーへ渡す。サイクル検出、ノード数上限、評価コスト上限、GPU 非対応ノードの明示的フォールバックを必須とする。

## ArtifactStudio への接続先

| 接続先 | 初期対象 | 後続対象 |
|---|---|---|
| Property Editor | Transform / Opacity / Color | Effects / Components |
| Timeline | Automation Clip の作成・編集 | Alias / Curve Browser |
| Composition Viewer | 変調結果の表示 | Audio Reactive preview |
| Particle | emission / size / velocity | Operators / graph output |
| Fluid | strength / buoyancy / source | graph output |
| Render Queue | snapshot 評価 | deterministic bake |

## 非対象

- Bitwig Studio のコード、UI、バイナリ、プリセットのコピー
- Bitwig の音源、MIDI、CV、ハードウェア制御の移植
- 初期フェーズでの汎用スクリプト実行ノード
- `QPainter` / Qt CompositionMode を使った新規合成経路
- ホットパスでの毎フレーム動的確保、文字列再構築、巨大カーブ再生成
- 動画デコード優先度の引き上げ

## 受入基準

1. 同一プロジェクト、同一 Seed、同一フレームで Preview と Render Queue が同じ変調値になる。
2. Transform / Opacity / Color の3種類以上で同じ `ModulationBinding` 契約を使える。
3. Automation Clip の保存、再読込、Alias、Unique、Undo / Redo が成立する。
4. Audio Reactive を無効化しても通常の静止画・シェイプ再生へ影響しない。
5. 変調グラフのサイクル、ノード数超過、未対応 GPU ノードが明示的な診断になる。
6. 既存の Playback Service、Property Editor、Timeline、Viewport の責務分離を壊さない。

## 実装優先度

最初の実装単位は **Phase 0 + Phase 1 + Phase 2 の Transform / Opacity 最小版** とする。これで「変調源 → プロパティ → 再利用カーブ → 保存／再生」の核を成立させてから、Audio Follower、Operators、Graph へ進む。

## 未解決事項

- Color / Vector / Quaternion を同じ変調 API で扱うための型表現
- Automation Clip をレイヤー所有にするか、コンポジション共有アセットにするか
- GPU 評価可能な変調源の一覧と CPU fallback の境界
- Audio Follower の解析窓、遅延、キャッシュ寿命
- Alias 更新時の Undo 粒度と参照循環の扱い
