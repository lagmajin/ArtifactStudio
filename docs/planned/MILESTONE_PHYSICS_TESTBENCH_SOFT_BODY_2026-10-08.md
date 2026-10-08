# Physics Testbench: Soft Body First

**最終更新:** 2026-10-08

**ステータス:** Not Started

## 目的

制作レイヤーへの物理設定追加とは独立した、ソルバーの挙動を試し、条件を再現できる Physics Testbench を用意する。第一対象は既存 `Physics.SoftBody` の布・ゲル表現とする。

この面は制作コンポジションを変更せず、物理設定・初期条件・コライダーを組み合わせた実験、再生、停止、リセット、状態確認を提供する。既存のレイヤー統合や Soft Body/Cloth System のマイルストーンとは分け、そこで成熟した契約を後から共有できる設計にする。

## 現状確認

- `ArtifactCore/src/Physics/SoftBodySolver.cppm` に Verlet solver、固定 timestep、substep、拘束、風、衝突、snapshot/restore がある。
- `ArtifactCore/src/Physics/PhysicsSystem.cppm` は `LayerID` ごとに solver と snapshot を管理するため、単体テストベンチからは直接の所有者にしない。テストベンチは solver を独立所有し、Composition の再生状態やグローバルな PhysicsSystem に依存しない境界を優先する。
- `tests/ArtifactCore/PhysicsDeterminismTest.cpp` に決定性、重力、減衰などのコードレベル契約テストがある。GUI testbench はこれらの置き換えではなく、視覚的な調整・観察面として扱う。
- `docs/planned/MILESTONE_PROFESSIONAL_SOFT_BODY_2026-07-11.md` が制作向けの Preview/Bake/Render parity、GPU deformation、authoring UX を扱う。testbench の完成をそれらの完了とみなさない。

## 初期スコープ

1. アプリ内の独立した Physics Testbench surface を開く。既存 Composition / Layer / Timeline を変更しない。
2. Soft Body の preset/primitive: 布格子、自由ゲル格子。初期段階では他 solver を追加しない。
3. パラメータ: 格子分割、重力、固定 timestep、constraint iteration、減衰、風、pin、重力床および既存 collider 形状。
4. 操作: Play/Pause、Step、Reset、初期状態へ戻す。UI の playback service や Composition frame を使わず、テストベンチ専用の明示的な制御とする。
5. 観察: 格子/点/拘束と collider の可視化、経過時間・step 数・点数など既存 solver から得られる診断値。
6. 再現: preset、設定値、初期条件を小さな明示的データとして保存・読込できる形を設計する。プロジェクト設定や `settings.cbor` へ混ぜない。

## 設計境界

- solver の進行は固定 timestep とし、描画更新間隔と切り離す。UI 側の timer は表示更新だけを担当し、simulation の状態所有者にはしない。
- reset は同じ初期条件から同じ状態を再生成する。Step は固定 timestep を1回進める。
- solver の snapshot を使う場合、UI の一時状態と物理状態を区別し、保存・復元契約を明示する。
- 専用 surface は既存 UI の配置・登録方法を調査してから決める。新規 signal/slot、Qt stylesheet、QImage/QPainter 合成は導入しない。
- Hot path に毎フレーム設定文字列構築や可変長の診断ログを持ち込まない。診断は明示操作または低頻度の表示更新に限定する。
- このマイルストーンでは solver アルゴリズムの改変、PhysicsSystem の再設計、制作レイヤーへの新しい物理機能追加を行わない。

## フェーズ案

| Phase | 内容 | 完了条件 |
|---|---|---|
| P0 | UI/サービス/テスト構成を調査し、独立 surface と solver 所有境界を確定 | 既存ドック・ウィンドウ登録経路とデータ境界を文書化 |
| P1 | 最小テストベンチと布格子、Play/Pause/Step/Reset | Composition と独立して開始・停止・初期化できる |
| P2 | 物理値・pin・collider の編集と可視化 | 設定変更後の reset で再現可能に実験できる |
| P3 | 実験条件の保存/読込と solver contract coverage | 同一条件の再実行・snapshot復元の結果を比較できる |
| P4 | ゲル等の preset、診断表示、UX整理 | 制作向け Soft Body/Cloth System へ知見を還元できる |

## 受入条件

- Testbench を開閉しても active Composition、Timeline、Playback Service の状態が変わらない。
- 一定の入力・seed・step 列から同じ solver state を再現できる。
- Pause 中は solver state が進まず、Step は固定 timestep 1 回だけ進む。
- Reset は初期条件と solver 設定を保って初期状態へ戻る。
- collider と pin の状態が画面で判別できる。
- 既存の Soft Body 制作統合マイルストーンの責務・進捗を混同しない。

## 未決事項

- surface を独立ドック、トップレベル tool window、または既存 Physics/Simulation 面の独立モードのどれに置くか。
- 条件ファイルの形式と保存場所。
- 最初の preset を布とゲルのどちらから見せるか。提案は布格子を既定にし、ゲルを追加 preset とする。

## 関連

- `docs/planned/MILESTONE_PROFESSIONAL_SOFT_BODY_2026-07-11.md`
- `docs/planned/MILESTONE_LAYER_PHYSICS_COMPONENT_2026-06-13.md`
- `ArtifactCore/src/Physics/SoftBodySolver.cppm`
- `tests/ArtifactCore/PhysicsDeterminismTest.cpp`
