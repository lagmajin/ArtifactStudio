**最終更新:** 2026-08-30
**ステータス:** Implemented — runtime verification pending

**構成メモ:** 2D本体は `ArtifactParticleLayer.cppm` に残し、3D派生層を
`ArtifactParticleLayer3D.cppm` へ分離。2D専用のProperty／Viewport導線と
3DレイヤーのJSON変換責務を同一実装ファイルへ混在させない。

# Particle 2D Particular-style Workflow (2026-08-30)

## 目的

`ArtifactParticleLayer` を、単に多数の数値を持つレイヤーではなく、2D VFXを短時間で作成・調整・再利用できる独立した制作面へ整理する。Particularからは操作モデルと制作体験のみを参考にし、コードや固有実装は持ち込まない。

対象は2D専用とし、`Particle3D`、流体、ReactiveEvents、新規グローバルシグナルはこのマイルストーンの範囲外とする。

## 既存資産

- `ArtifactParticleLayer` は2Dレイヤーとして既存のEmitter、Simulation、Particle、Aux、Render設定を保持している。
- `ArtifactParticleGenerator` はEmitter Shape、Continuous／Burst／Triggered、Life、Size／Color／Opacity、Pre-Warm、Deterministic Simulationを提供している。
- Force、Vortex、Turbulence、Attractor、Repeller、Wind、Flocking、Kill Zone等のEffector型とJSON復元経路が存在する。
- `ParticleRenderSettings` はBlendとBillboardの設定を持ち、GPU描画経路も存在する。

## Phase 1: 2D専用プロパティ面の整理

### UI構成

通常のレイヤー固有プロパティと混在させず、Particle専用面で次の順に表示する。

1. **Quick Setup** — preset、reset、preview／pre-warm、seed
2. **Emitter** — shape、size、rate、burst、direction、spread、world/local space
3. **Particle Life** — birth／life／deathの要約とカーブ入口
4. **Physics** — gravity、drag、wind、turbulence、collision
5. **Effectors** — effector stack
6. **Appearance** — texture、sprite sheet、blend、billboard
7. **Trails / Aux** — aux particleとtrail設定
8. **Simulation** — fixed step、substeps、determinism、max particles

### 完了条件

- 2D Particleの主要設定が上記の責務別グループで見つけられる。
- advanced／diagnostic項目を初期表示から分離できる。
- 既存property pathとJSON互換性を維持する。
- 通常の`ArtifactPropertyWidget`へコンポーネント由来の設定を追加しない。

## Phase 2: Lifeカーブ

### 対象カーブ

- Size over Life
- Opacity over Life
- Color over Life
- Rotation over Life
- Velocity／drag modulation over Life（後段）

### 契約

- birth、life、deathを0〜1のnormalized lifeで評価する。
- 既存のmin／max値は初期値および互換保存値として扱う。
- カーブ未設定時は現在の線形補間と同じ結果にする。
- カーブ編集は1回の編集セッションを1つのUndoトランザクションにする。
- 固定シード、frame seek、pre-warmで同じ入力から同じ結果を得る。

### 完了条件

- カーブの追加、削除、リセット、補間方式変更が可能。
- 0、0.5、1.0の代表点を直接編集できる。
- 既存プロジェクトの読み込み結果が変わらない。
- GPU／software両経路で評価値の意味が一致する。

## Phase 3: Effector Stack UI

### 操作

- Force、Wind、Turbulence、Vortex、Attractor、Repeller、Collision、Killを追加できる。
- 有効／無効、名前変更、複製、削除、並べ替えができる。
- 選択中Effectorの強度、位置、方向、範囲、falloffを専用面で編集する。
- stack順がシミュレーション評価順と一致する。
- 未対応型は壊さず読み込み、unknown／legacyとして診断表示する。

### 完了条件

- JSONのeffectors配列とUIのstackが双方向に一致する。
- no-op、空削除、キャンセルがUndo履歴を汚さない。
- Effectorの追加・削除・並べ替えが保存／再読込後も維持される。
- 既存の公開APIを再利用し、新規のグローバルイベント配線を追加しない。

## Phase 4: Viewport直接編集

### 2Dハンドル

- Emitter: 原点、半径、幅／高さ、Line端点、方向矢印
- Attractor／Repeller: 中心、影響半径、falloff表示
- Vortex: 中心、半径、回転方向、tightness
- Wind／Force: 方向矢印、強度表示
- Kill Zone／Collision: 境界矩形またはポリゴン

### 操作契約

- 選択中Particle 2Dだけにハンドルを表示する。
- `G`、`R`、`S`、軸拘束、`Esc`、`Enter`の既存汎用操作と衝突させない。
- ドラッグ開始時にsnapshotを取り、ドラッグ完了時だけUndoへ登録する。
- キャンセルは完全にsnapshotへ戻す。
- 画面座標とシミュレーション座標の変換を明示し、DPIやviewport zoomに依存したずれを避ける。
- overlay描画は既存のComposition Render Widget／Overlay責務に置く。

### 完了条件

- Emitterと主要Effectorをプロパティ面へ往復せず調整できる。
- ドラッグ中もプレビューが破綻せず、終了時に1回だけUndoされる。
- 別レイヤー選択、pane移動、キャンセルでハンドルや対象が残留しない。
- 実ランタイムでEmitter、Attractor、Vortex、Forceの4ケースを確認する。

## 実装順序

1. Phase 1のPropertyGroup再配置と専用面の責務固定
2. normalized life curveのCore契約とJSON互換層
3. Phase 3のEffector StackとUndo接続
4. Phase 4のEmitter／Effector overlayとdrag transaction
5. preset、trail、GPU／software parityの受入

## 所有権

| 領域 | 所有者 |
|---|---|
| Emitter、Life、Effector、固定ステップ | `ArtifactCore`／`ArtifactParticleGenerator` |
| 2D Particle layer、property path、JSON bridge | `ArtifactParticleLayer` |
| Effector stack編集面 | ArtifactのParticle専用editor surface |
| ハンドルとoverlay | `ArtifactCompositionRenderWidget`／既存Overlay責務 |
| GPU描画 | `ArtifactCore::ParticleRenderer` と既存Diligent経路 |

## 受入シナリオ

1. Point Emitterを作成し、方向とSpreadをViewportで調整する。
2. Size／Opacity／Color over Lifeをカーブ編集し、0／50／100%で確認する。
3. TurbulenceとAttractorをstackへ追加し、並べ替えと無効化を確認する。
4. Emitter、Attractor、Vortexをドラッグして、Undo／Redoとキャンセルを確認する。
5. Pre-Warm、frame seek、保存／再読込を行い、固定シードで結果が再現することを確認する。

## リスクと未検証事項

- 現在のEffectorはCore型とLayer APIが存在するが、全型を専用UIで編集できるかは未確認。
- Lifeカーブの既存Animation APIとの最小依存境界は実装時に確認する。
- GPU／softwareでのカーブ評価一致は実行検証が必要。
- Viewportの直接操作は既存Transform／Mask／Penモードとの入力優先順位を設計確認する。
- ビルド、テスト、実ランタイム確認は明示許可後に実施する。
