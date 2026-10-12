# Local Time Warp — マスク領域別の時間操作

**最終更新:** 2026-10-12
**ステータス:** Not Started

## 目的

動画レイヤー内の限定領域を、通常再生の時間を保ったまま個別にスロー／加速／停止／逆再生する。領域は既存のマスク形状とソフト境界で指定し、領域内外を時間サンプル結果と合成する。

添付画像は機能案の参考であり、UIや表現例をそのまま実装仕様とはしない。初期スライスは処理契約と最小設定に限定し、専用の大きな編集画面や画像中の追加表現は対象外とする。

## 現状の再利用資産

- `ArtifactCore::TimeRemapProcessor` と `ArtifactAbstractLayer` の Time Remap に、時間→ソース時間評価、補間、Reverse／Hold／Ramp、保存経路がある。
- `Artifact::IEffectFrameSampler`、`EffectContext`、`EffectHostContract` に別フレーム取得、時間文脈、temporal input 語彙、input bundle の基盤がある。
- `TimeDisplacementEffect` は sampler を使う CPU temporal effect として登録済み。現在は入力画像の指定チャンネル値から各ピクセルの frame offset を作り、複数フレームを取得してブレンドする。
- Composition のマスク評価・エフェクトマスク合成・track matte は既存レンダー経路にある。
- 上記は再利用候補であり、Local Time Warp としての結合・再生・書き出し受け入れが済んだことを意味しない。

## MVP 範囲

1. **入力対象:** 最初は動画レイヤー1枚、既存の単一マスク1つ。マスク不在や未対応入力では元フレームをそのまま返す。
2. **時間指定:** レイヤー／コンポジション時間に対する source-time mapping として定義し、等速を Identity とする。最低限、一定速度、区間カーブ、Hold、Reverse を評価できる。画像案の「映像からドライブ」は MVP 外。
3. **領域合成:** マスクの内外で時間サンプルを切替え、既存マスクのfeather coverageを使ってソフトにブレンドする。境界で元映像との不連続を作らない。
4. **既存 Time Remap との併用:** まず変換順を固定する。候補は composition time → existing layer Time Remap → local offset/mapping → source sample。二重 remap の意味が曖昧な組み合わせは無効化または診断する。
5. **保存:** マッピング、マスク参照、境界量、モードを既存 layer/effect persistence へ後方互換で追加する。初期値は Identity／無効とする。
6. **編集導線:** 既存 Effect Inspector とマスク編集を使う薄い導線から開始。専用 Timeline curve UI は別フェーズ。

## 実装フェーズ

### Phase 0 — 契約と順序の確定

- TimeDisplacement の実際の登録・生成・host 実行経路、sampler/cache key、CPU/GPU fallback、mask coverage のデータ表現をコードで追跡する。
- 既存 effect に直接局所時間モードを追加するか、新しい LocalTimeWarp effect を作るか決定する。別 effect にする場合は既存 TimeDisplacement の挙動と名前を維持する。
- effect stack、layer Time Remap、mask、temporal sample の評価順と cache identity を定義する。

### Phase 1 — CPU MVP

- 1つの既存 mask coverage と source-time mapping から、必要 source frame を bounded に収集する。
- sample の取得は pixel loop 内で行わない。要求フレーム集合を bounded にし、容量不足時は明示的にdefer／fallbackする。
- linear float surface で既存画像とサンプル結果をブレンドする。QImage/QPainter合成は導入しない。
- 各フレーム要求で新しい無制限 map/vector や大容量作業面を生成しない。再利用可能なworkspace/cache契約が定まるまでリアルタイム保証をしない。

### Phase 2 — 統合と編集

- Inspector に時間モード、速度／mapping curve、範囲・feather を公開する。
- 既存 mask 編集結果を参照し、複製／Undo／dirty／選択切替時の状態を維持する。
- preview、playback、RAM preview、render queue が同じ評価と cache key を使う。

### Phase 3 — 品質・範囲拡張

- 複数領域、空間的に異なる速度、滑らかな speed ramp、音声方針、CPU/GPU parity、再生性能を個別に受け入れる。
- マスク追跡や映像内容からの時間駆動は別途設計する。

## リスク／受け入れ条件

- **サンプル爆発:** per-pixel mapping はフレーム要求数が増える。MVP は少数の bounded temporal samples に制限し、無制限の frame cache を作らない。
- **循環依存／再帰合成:** サンプルは対象レイヤーの effect stack をどこまで評価するか明示し、Local Time Warp 自身の再帰呼び出しを防ぐ。
- **キャッシュ誤同一化:** layer ID、時間 mapping revision、mask revision、composition/source frame、resolution、render purpose、既存 Time Remap 状態を識別する。
- **フレーム境界:** source 範囲外、逆再生、fractional time、decoder seek、missing frame 時の deterministic policy を固定する。
- **色とアルファ:** premultiplied/linear surface 契約を保ち、feather境界にハローが出ないこと。
- **受け入れ:** mask外は通常時間と一致し、内側は設定 mapping と一致し、境界はfeatherに従い連続する。保存／再読込、preview／playback／offline renderの一致とUndoを確認する。

## 非対象

- 新しいグローバルイベント配線／signal-slot
- ソフトレンダラーの新機能路線、QImage/QPainter合成
- 独自の追跡・ブラシ・グラデーション生成UI
- コード変更前のビルド・テスト・CMake実行（リポジトリ指示によりユーザー明示指示が必要）

## 関連資料

- `docs/planned/MILESTONE_TEMPORAL_EFFECT_HOST_FOR_TIME_DISPLACEMENT_2026-07-01.md`
- `docs/planned/MILESTONE_TIME_REMAP_CURVE_UI_2026-06-02.md`
- `docs/planned/MILESTONE_MATTE_MASK_TIME_REMAP_SPLIT_ROUTE_2026-04-24.md`
- `docs/technical/HOT_PATH_RULES.md`
