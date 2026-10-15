# ビューポート直接調整とパラメーター影響度の導入可能性

**最終更新:** 2026-10-13

## 結論

両機能とも導入可能だが、性質は異なる。

- **ビューポート直接調整:** 既存の gizmo、overlay、modal drag、Undo の経路を使い、パラメーター別のハンドルを追加する。クローン間隔から始めると小さく実証できる。
- **パラメーター影響度:** パラメーターを微小変更したときの最終画像差分を測る有限差分評価が候補。ただし各パラメーターの前後レンダーを要するため、常時・毎フレーム計算にはせず、明示要求による bounded な解析として設計する。

## 1. ビューポート上での直接調整

### 現在の基盤

- `ArtifactCompositionRenderWidget` / `CompositionRenderController` に描画、入力 routing、2D/3D Transform Gizmo、modal drag、取消・確定の経路がある。
- Shape の Corner Radius / Star Inner Radius は専用 handle controller、overlay、Undo command に分割された直接編集例が `ArtifactLayerEditorWidget` にある。
- Composition 側にも Shape parameter drag と before/after Undo 経路がある。
- Mask、Motion Path、Rig、Text Puppet 等にも対象別の直接編集経路がある。したがって「任意プロパティ値をどこからでも編集する汎用ギズモ」があるわけではないが、対象別の小さな controller を追加できる設計パターンは確立している。

### 対象別評価

| 対象 | 確認できたデータ・経路 | 導入可能性 / 注意 |
|---|---|---|
| Cloner spacing | `ArtifactAbstractLayer` が spacing X/Y/Z を保持し、配置計算に使う。`ArtifactAbstractLayerLayoutRouting` に `component.cloner.spacingX/Y/Z` の更新経路がある。 | **高**。Linear / Grid の配置方向へガイド線とドラッグ端点を置き、既存 setter/property routing を使う案が最小。Radial は半径・開始/終了角で別ハンドルを設計する。 |
| Light range / spread | `ArtifactLightLayer` が Range、Cone Angle、Cone Feather 等を保持し、永続化・property routing を持つ。Render Controller は Light layer を Core Light に変換する。 | **中〜高**。Point/Area は距離円、Spot は円錐境界と feather 境界を操作対象にできる。表示中の光ガイドがすべて直接編集可能とは限らないため、描画・hit test・入力経路の対応を先に点検する。 |
| Effect blur | effect parameter の既存編集経路はある。 | **中**。グローバル blur radius は画面上の半径 handle で操作可能。ユーザーが言う「ぼけの領域」が spatial falloff / region を意味するなら、既存 effect が範囲・方向・feather を持つかを effect ごとに確認し、単一半径と同じUIに見せない。 |

### 推奨実装順と共通契約

1. **Linear / Grid Cloner spacing** を選択レイヤーの overlay handle として実装する。
2. **Spot Light range / cone angle** を追加し、数値ガイドと handle の意味を viewport 上で判別できるようにする。
3. **Gaussian Blur radius** を追加する。領域 blur はそのモデルが存在する effect に限定する。

各 drag は以下を満たす。

- 選択中かつ対応 tool の時だけ handle を表示し、hover / active を明示する。
- Transform、mask、playback scrub、viewport navigation と hit-test 優先順位を決める。
- drag start の値を保持し、pointer move は開始値からの差分で決定する。Esc は開始値へ戻す。
- drag 全体を一つの既存 Undo command にまとめ、失敗時も開始状態へ戻す。
- mouse delta は zoom、DPI、layer / composition 座標系に応じて変換する。
- 新しい signal/slot や QtCSS を追加せず、既存の入力 routing / service / property mutation 経路を再利用する。

## 2. パラメーター調整感度の可視化

### 技術的な実現方法

例示の Glow Radius 90%、Bloom Intensity 70% 等を得るには、各パラメーターの現在値 `p` と許容差分 `Δp` で同一フレームを二度評価し、最終出力 `I` の変化量を比較する有限差分が妥当な出発点である。

`S(p) = D(I(p + Δp), I(p)) / N(Δp)`

- `D`: 出力差分の集約指標（例: linear-light RGB / alpha の ROI 内 RMS または平均絶対差）。
- `N`: パラメーター単位・範囲に依存する差分量の正規化。
- 表示値: 対象パラメーター群のスコアを相対順位へ正規化し、降順表示する。

この指標が表すのは「指定した摂動幅で、現在の出力がどれだけ変わるか」であって、品質向上度や望ましい編集方向ではない。90% は「最も効く方向／正解」という意味ではなく、選択された測定条件内での相対影響順位として説明する必要がある。

### 現状の土台と差分

- `AbstractImageEffect::parameters()` の `EffectParamDef` は name / label / min / max / step を提供する。
- `CreativeEffect::getParameters()` の `EffectParameter` は型、値、min / max を提供する。
- Effect Stack と最終描画経路、Contents Viewer の画像差分表示、Software Render Test の pixel delta / mean / max 指標が存在する。
- ただし、調査時点で、全 effect/backend に共通して「parameter descriptor → 安全な一時 override → 同一条件の最終出力を二枚得る → 差分を集約する」統一 API は確認できなかった。効果系によって parameter contract / CPU・GPU 実装が異なる。

### 推奨 MVP

1. **対象を一つの Effect Stack / 選択 effect に限定**し、数値 parameter のみ対象とする。Boolean、enum、color、resource、構造変更 parameter は対象外。
2. ユーザーが明示的に「影響度を解析」を実行した時にだけ計算し、再生・ドラッグ中・毎フレームには更新しない。
3. 同一 frame、同一 render backend、同一 resolution、同一 input/cache generation で baseline と perturbation を比較する。現在の効果を含む最終画像を測り、effect 単体の内部値だけで代理しない。
4. `Δp` は parameter の範囲に対する小さな比率を基本とする。上下限では有効な片側差分へ切替える。異なる単位間の正規化方式を文書化する。
5. 解析対象数・解像度・同時ジョブ数に上限を持たせる。低解像度 proxy を使う場合は、表示名を「プレビュー影響度」とし、フル解像度と同等だと扱わない。
6. 結果は frame / effect stack revision / input generation / backend / resolution / parameter value / perturbation を鍵に短期間キャッシュし、状態変更時に無効化する。古い非同期結果は generation で破棄する。

### リスクと解釈

- 差分回数は対象パラメーターごとに最低1追加評価を要する。n parameter なら概ね baseline + n perturbation render となり、effect が重いほど待ち時間が大きい。
- 非線形・閾値型効果では `Δp` の選択によって順位が変わる。ゼロ付近・飽和域・現在値の局所感度は別の意味になる。
- パラメーター間の相互作用は単独摂動だけでは分からない。Bloom intensity と radius の組合せなどは個別スコアの足し算にならない。
- Alpha / HDR / 色空間、透明背景、ROI、temporal effect の前フレーム状態を揃えないと差分値が比較不能になる。
- Effect parameter を変更して元へ戻す一時 override が、cache invalidation、GPU state、Undo 履歴、共同編集に副作用を起こさない API が必要。
- ホットパス規則に従い、同期 readback、QImage 化、無制限 job 起動、GPU resource の都度生成は行わない。固定容量の非同期解析枠を使い、枠不足なら defer / reject を返す。

## 推奨判断

- ビューポート直接調整は Cloner spacing から実装を検討できる。個別 controller と既存 Undo の小さな追加で成立し、現行データモデルを変えずに価値を出せる。
- 影響度表示は技術的には成立するが、先に測定指標・正規化・render snapshot / temporary override 契約を固める必要がある。初手では選択中の数値 Effect parameter 数個を、明示実行・低解像度 preview で比較する限定実験が適切。
- 数値の影響度を自動的に「調整すべき順」または推奨値と見せない。方向性や品質評価が必要なら別の評価関数・ユーザー確認を設ける。

## 確認範囲

既存ドキュメントと C++ source の静的確認。ビルド、CMake、テスト、実機測定は行っていない。特に Light / Effect overlay の全 hit-test 経路、Render Queue と preview の完全な同条件 snapshot 再利用、各 effect の parameter clone / temporary override 安全性は未確認。

## 関連資料

- [Widget Map](../WIDGET_MAP.md)
- [Hot Path Rules](../technical/HOT_PATH_RULES.md)
- [Cross-App Feature Opportunities](REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md)
- [Operation State Map](OPERATION_STATE_MAP_2026-08-30.md)
- [Viewport DCC parity analysis](VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md)
