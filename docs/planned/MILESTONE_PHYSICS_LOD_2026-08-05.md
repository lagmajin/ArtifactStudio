# 物理LOD導入マイルストーン

**最終更新:** 2026-08-05

## 現状監査

統一された物理LODは未実装。既存コードには個別の品質調整口があるが、画面上の距離・占有率から自動適用する共通ポリシーはない。

| 項目 | 現状 | 判定 |
|---|---|---|
| 更新Hz低下 | `Physics2D::step` 等の呼び出し側制御は可能 | 共通LOD未対応 |
| サブステップ削減 | `Physics2D::step`、SoftBody、MPMに個別設定あり | 部分対応 |
| Collision Mesh簡略化 | Box／Circle／Polygon生成とShape差し替えAPIはある | Shape所有・復元管理が未対応 |
| 遠景Rigid Body停止 | Bodyの個別停止APIはある | 距離判定未対応 |
| Sleeping強化 | Body単位のSleep有効化・閾値APIあり | Reduced／MinimalのLOD制御を実装済み |
| 小物の衝突省略 | 共通の画面サイズ判定なし | 未対応 |
| CCD停止 | Body単位のBullet設定APIあり | Minimalで停止するLOD制御を実装済み |
| Constraint反復削減 | SoftBodyに反復設定あり | Rigid／全体LOD未対応 |
| 破砕段階削減 | Fracture設定と段階状態はある | LODによる段階削減未対応 |
| Cloth頂点数削減 | SoftBody生成・制約処理はある | LOD再サンプリング未対応 |
| Fluid解像度低下 | FluidSolver2Dの解像度は生成時固定 | ランタイムLOD未対応 |

## 設計原則

- 物理LODはレイヤー本体の矩形ではなく、画面上の投影サイズ、速度、重要度、影響範囲で判定する。
- 物理シミュレーションのLODと描画LODを同じ値に直結させない。
- LOD変更でエネルギーや位置が不連続に変化しないよう、切替時に速度・sleep・サブステップ状態を扱う。
- 重要物、近景、選択中、相互作用中、Constraint接続中のBodyは最低品質を保護する。
- 最終レンダー、書き出し、決定論モードでは自動物理LODを既定で無効化する。
- Fluid解像度変更やCloth頂点削減は状態再マッピングが必要なため、最初の導入対象にしない。

## 導入段階

### M1：共通評価コンテキスト

- `PhysicsLODContext` を定義する。
- 画面上の投影サイズ、距離、速度、重要度、相互作用状態、品質上限を保持する。
- `Full / Reduced / Minimal / Frozen` 相当の段階を定義する。
- LODを固定する手動設定と、自動判定を無効化する設定を用意する。

### M2：安全な低リスク制御

- 更新Hz低下をシミュレーション蓄積時間で実装する。
- サブステップ上限を段階的に下げる。
- 遠景かつ非相互作用のRigid Bodyをsleepまたは更新対象外にする。
- 速度・衝突・Constraint接続が発生したBodyを即座に通常品質へ復帰させる。

### M3：剛体・衝突LOD

- 小物の衝突参加を省略できる明示的なポリシーを追加する。
- Collision Meshを高精度Polygonから簡略形状へ切り替える。
- CCDは速度とサイズを満たす場合だけ有効にし、停止条件を明示する。
- Sleeping強化はしきい値変更だけでなく、復帰条件とセットで設計する。

### M4：Constraint・破砕LOD

- Constraint反復回数を段階制御する。
- 破砕は遠景で生成 shard 数、破砕段階、デブリ生成量を削減する。
- 破砕後に近景へ戻った場合の再生成可否を決める。不可逆な削減は最終品質モードでは禁止する。

### M5：Cloth・Fluid

- Clothは頂点削減ではなく、まず制約反復数と更新頻度を下げる。
- 頂点削減を行う場合は、元メッシュと物理メッシュを分離し、状態の再マッピングを用意する。
- Fluidはsolver iteration削減を先行する。
- 解像度変更は密度・速度・障害物を再サンプリングできる状態管理を実装してから行う。

## 実装順序

1. 共通評価コンテキスト
2. Rigid Bodyの更新頻度・sleep・サブステップ
3. Collision Mesh／CCDの条件制御
4. Constraint反復と破砕段階
5. Clothの更新頻度・反復
6. Fluid iteration
7. Cloth頂点数とFluid解像度の動的変更

## 検証項目

- LOD切替時にBodyが瞬間移動しない。
- 遠景停止後、近景化・衝突・外力で正しく復帰する。
- CCD停止時に高速小物が不自然にすり抜けない条件が定義されている。
- Constraint削減で破綻した伸びや爆発的なエネルギーが発生しない。
- 破砕段階削減で shard の重心・総質量・主要な視覚結果が許容範囲に収まる。
- Cloth／Fluidの解像度変更で状態が消失・発散しない。
- 自動LOD無効時に既存の物理挙動を維持する。

## 実装済みの初期基盤

`ArtifactCore/src/Physics/PhysicsSystem.cppm` に、既存挙動を変えない既定値で次を追加した。

- `PhysicsLODLevel`（Full／Reduced／Minimal／Frozen）
- `PhysicsLODSettings`
- 物理システム全体の更新Hz制限
- Rigid Bodyのサブステップ上限
- SoftBodyの最大サブステップとConstraint反復数
- Fluid solver反復数
- Frozen時の物理更新停止
- Reduced／Minimalの既定予算（更新Hz、サブステップ、反復数）
- Reduced／MinimalのSleep強化
- MinimalのRigid Body CCD停止
- SoftBodyのCollision反復削減とMinimal時の自己衝突省略
- 破砕／MPMのサブステップ上限削減
- Fluid密度・速度場のランタイム再解像度化
- Minimal時のPolygon Collision MeshのAABB簡略化
- Minimal時の破砕計算・破砕イベント省略
- Minimal時の破砕shard／debris生成数削減
- Reduced／Minimal時のClothグリッド頂点削減

これは共通制御の入口であり、距離判定や自動LOD選択はまだ行わない。`Full` または未設定時は従来の更新頻度・サブステップを維持し、`Reduced`／`Minimal` を選択した場合だけ既定予算を適用する。個別値はプリセットを上書きできる。Box2D Bodyについては、Reduced／MinimalでSleepポリシーを適用し、MinimalではCCDを停止する。SoftBodyはCollision反復を削減し、Minimalでは自己衝突を省略する。MPM／破砕系はサブステップ上限を削減する。

## 現時点の対応判断

共通設定の入口と更新頻度・サブステップ・反復数・Sleep・CCD・SoftBody自己衝突・Fluid再解像度化・Minimal時のPolygon Collision Mesh簡略化・破砕計算抑制・破砕shard／debris生成数削減・Clothグリッド低解像度化を導入した。破砕生成時のdebrisCount適用も確認済み。破砕後に既存結果を再構成する段階変更は、別途状態所有と復元責務を確認する。

## Update 2026-08-15

- `PhysicsSystem.cppm` の現行実装で `PhysicsLODLevel`、`PhysicsLODSettings`、全体の更新Hz、Rigid／SoftBody／Fluid／MPM のサブステップ・反復予算、Frozen停止、Reduced／Minimal の Sleep・CCD・自己衝突・Polygon AABB・破砕・Clothグリッド制御を確認できる。初期基盤の記述は現状と整合する。
- `ArtifactAbstractLayer`／`ArtifactAbstractComposition` から physics LOD settings、物理ステップ、snapshot restore の経路は参照されている。一方、画面投影サイズ・距離・速度・重要度・相互作用状態から自動的に `Full / Reduced / Minimal / Frozen` を選ぶ `PhysicsLODContext`／共通判定器は現行コードで確認できない。
- Fluid再解像度やCloth／破砕の削減は設定値に応じた制御であり、切替時の状態再マッピング、近景復帰時の完全再構成、最終レンダー／決定論モードの自動LOD無効化を受入れた証拠はない。
- よって現状は `manual LOD policy and per-solver controls implemented / automatic screen-space selection, transition safety, persistence and runtime validation pending` と判定する。自動LOD未実装という冒頭の監査結論は維持する。
