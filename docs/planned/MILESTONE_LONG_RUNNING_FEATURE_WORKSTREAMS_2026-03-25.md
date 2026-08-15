# マイルストーン: 長期機能拡張ワークストリーム

> 2026-03-25 作成

**最終更新:** 2026-08-15

## Update 2026-08-15

現行コードを確認すると、Property／Keyframe は `AbstractProperty` の値・アニメーション・補間・keyframe編集を基盤に持ち、Timeline／Inspector／Property Widget、Undo、copy/paste、3D transform など複数の編集経路へ接続されている。ただし全 derived layer／mask／camera／effect を完全に同一契約へ収束させたことは未確認である。

Project／Asset workflow では Asset Browser と Project Manager の選択同期、Recent／Favorites、relink、missing／unused 検出、import・cleanup・thumbnail の基盤が存在する。残りは Asset Browser と Project View の情報面統合、drag-and-drop／relink の全導線、metadata／duration／fps の横断表示と runtime 受入である。

Software Composition では software render compositor、software render inspectors／test widgets、current layer／composition の診断経路が存在する。一方、GPUとの比較を目的にした一貫した compare／split view、overlay／gizmo／crop／bounds の全機能 parity、回帰検出ワークフローは未確認である。

判定は「3つの長期テーマはいずれも基盤と部分統合が進行済み。Property／Keyframe の完全統合、Asset／Project の横断workflow、Software Composition の比較・検証面は継続課題」とする。

## 目的

空き時間で少しずつ進めると効く、時間はかかるが完成度への寄与が大きい作業をまとめる。

この文書は操作感改善ではなく、**機能そのものを増やす / 強くする** ための長期ワークストリームを扱う。

---

## 1. Property / Keyframe 統合の完成

### 目的

キーフレームを `ArtifactCore::AbstractProperty` に一本化し、Inspector / Timeline / Playback / Graph 的な機能を同じ正に乗せる。

### 目標

- UI ローカルの keyframe 管理をなくす
- transform / opacity / effect / mask / camera の編集対象を統一する
- keyframe lane や graph editor を後から自然に載せられるようにする

### 主な作業

- `AbstractProperty` ベースの保存先を残りの derived layer にも広げる
- `AnimatableTransform3D` の bridge を整理する
- Timeline keyframe lane を `AbstractProperty` 直結にする
- 前後キー移動、追加、削除、ジャンプを共通化する
- property path ベースで `layer.opacity` / `transform.position` / `effect.param` を扱う

### 連携先

- `ArtifactPropertyWidget`
- `ArtifactAbstractLayer`
- `AnimatableTransform3D`
- `ArtifactTimelineWidget`
- `ArtifactInspectorWidget`

---

## 2. Project / Asset ワークフロー強化

### 目的

素材管理とプロジェクト構成を、実務で回しやすい形に育てる。

### 目標

- Asset Browser と Project View を同じ情報面として扱う
- import / relink / unused / missing を明確にする
- browse -> drag -> timeline / composition の導線を短くする
- metadata を見ながら作業できるようにする

### 主な作業

- Asset Browser / Project View 選択同期の強化
- recent / favorites / unused / missing の導線
- thumbnail / type / size / duration / fps の表示改善
- internal drag-and-drop の整理
- folder / bin / organization の見直し
- import 後の反映と undo の整合

### 連携先

- `ArtifactAssetBrowser`
- `ArtifactProjectManagerWidget`
- `ArtifactProjectModel`
- `AssetDatabase`
- `ArtifactProjectService`

### 長期価値

素材が増えたときに破綻しにくい。
プロジェクトの状態把握が速くなる。

---

## 3. Software Composition 強化

### 目的

ソフトウェア版コンポジションを、デバッグ・検証・比較のための強力な実験場として育てる。

### 目標

- Diligent なしでも描画経路を確認できる
- GPU path の問題を切り分ける基準面として使える
- layer / composition / gizmo / crop / blend の差分確認をしやすくする

### 主な作業

- software composition view の機能強化
- software layer view の見た目と挙動の整理
- overlay / gizmo / crop / bounds の可視化
- current composition / current layer 追従の安定化
- fallback path の debug ログ整備
- compare view / split view 的な比較導線

### 連携先

- `ArtifactSoftwareCompositionTestWidget`
- `ArtifactSoftwareLayerTestWidget`
- `ArtifactSoftwareRenderTestWidget`
- `ArtifactCompositionRenderController`
- `ArtifactIRenderer` software backend

### 長期価値

- GPU / compute / viewport の不具合を切り分けやすくなる
- バックエンド差分の検証に使える
- レンダリング系の回帰検出がしやすくなる

---

## 優先順位

### 最優先

1. Property / Keyframe 統合
2. Project / Asset ワークフロー
3. Software Composition 強化

### 理由

- Property / Keyframe は編集機能全体の土台になる
- Project / Asset は制作フローの入口と管理を担う
- Software Composition はデバッグと比較の基盤になる

---

## 実装順の提案

### ステップ 1

`AbstractProperty` の残差分を潰し、Timeline / Inspector の keyframe を単一系統へ寄せる。

### ステップ 2

Asset Browser と Project View の同期、import/relink、unused 表示を整理する。

### ステップ 3

Software Composition に overlay、gizmo、bounds、crop の可視化を追加し、診断能力を上げる。

---

## 関連文書

- `docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md`
- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `docs/planned/MILESTONE_OPERATION_FEEL_REFINEMENT_2026-03-25.md`
- `docs/planned/MILESTONE_M12_POLISH_AND_STABILITY_2026-03-17.md`
- `docs/planned/MILESTONES_BACKLOG.md`
- `Artifact/docs/MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md`
- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`

---

## Next Step

空き時間で進めるなら、まず次の 3 点を細分化する。

1. Property / Keyframe の未統合箇所一覧
2. Project / Asset の同期フロー一覧
3. Software Composition の debug overlay 要件一覧

---

## Static audit follow-up (2026-07-25)

Property／keyframe、Project／Asset、Software render の現行ソースを横断確認した。ビルド・runtime parity は未実施。

| ワークストリーム | 現状 | 判定 |
|---|---|---|
| Property / Keyframe 統合 | `AbstractProperty` と keyframe API は広く使われ、Text／Transform等の保存・Timeline／AI操作経路もある。一方、全 derived layer／effect／mask／camera の単一経路統合は未確認。 | 部分実装 |
| Project / Asset workflow | Asset Browser／Project Manager、selection、unused／missing表示、relink、cleanup、metadata系の実装を確認した。全導線の同期・undo整合は未確認。 | 実装済み部分／統合確認待ち |
| Software Composition 強化 | Software render/compositor、layer／composition test surface、bounds／gizmo／fallback診断の要素は存在する。比較／split viewと全overlayの統一は未確認。 | 部分実装 |

### 現在の判定

3つの長期ワークストリームはいずれも基盤または関連機能が存在するが、文書の完了条件を満たす全体統合は未確認。最優先の Property／Keyframe は、残りの非統合経路を棚卸ししてから実装を進める状態とする。

