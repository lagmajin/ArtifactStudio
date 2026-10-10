# Particle Geometry Emission Milestone

**最終更新:** 2026-10-10
**ステータス:** In Progress

## Goal

既存の Particle Emitter が、点・基本プリミティブに加えてシェイプ／メッシュ形状を発生元として継続放出できるようにする。各粒子の誕生時に発生元上の位置をサンプリングし、その後の寿命・速度・力・描画は既存 ParticleSystem の経路を使う。

## Scope and invariants

- 既存の Continuous / Burst / Triggered の放出スケジュールを維持し、形状発生元は粒子の初期位置だけを供給する。
- シミュレーション状態を render cache に持たせない。時間更新、決定性、保存形式、既存の GPU/software 描画境界を維持する。
- 形状・メッシュの三角形分布は面積加重でサンプリングし、退化／不正三角形を除外する。
- 入力ジオメトリの取り込みと分布構築はコールドパスで行う。粒子誕生時は既存容量を使い、ジオメトリ配列を確保・再構築しない。
- GPU の `emittedparticle_emitCS_FROMMESH.hlsl` は別レーンの既存資産として扱い、Diligent 経由のGPU emit統合はCPU経路の仕様確定後に判断する。
- 2D/3D Particle レイヤーの分類、Transport、Property Widget の責務をこの機能のために変更しない。

## Current state

- `ParticleEmitter::getEmissionPosition()` は Point / Sphere / Box / Circle / Rectangle / Line をサンプリングする。
- `EmitterShape::Mesh` と `Surface` は列挙値が存在するが、Particle Layer の UI／JSON 復元は両方を無効化していた。
- GPU shader source には FROMMESH バリアントがあるが、エミッターのソース設定からの到達性は確認できていない。
- 本マイルストーンの初期実装で、ParticleEmitter に三角形配列を設定するコールドパス API と、面積加重 Mesh／Surface 表面サンプリングを追加した。既存の連続放出処理は変更しない。

## Phases

### GE-1: Geometry sampling core — Complete (static implementation; runtime verification pending)

- `ParticleEmitter::setEmissionTriangles()` で三角形列を検証し、有限・非退化面だけを保持して面積累積分布を構築する。
- Mesh / Surface emitter の粒子誕生時に三角形を面積加重で選び、重心座標で表面位置を得る。
- 入力を消去する API と source 有無の問い合わせを用意する。
- GE-1 は三角形列を直接受け取る粒子エンジン API までを実装済み。Shape／Mesh layer からの取得・選択・永続化は後続フェーズで接続する。
- 確認項目: 空入力、頂点数不整合、NaN/Inf、退化面、面積比、ランダム点の三角形内包含、source 未設定時の安全な原点 fallback。

### GE-2: Shape path adapter

- Shape layer のベジェ／ポリゴン輪郭を、粒子用の bounded triangle soup に変換する境界を決める。
- 2D path の閉曲線、穴、複数サブパス、開パスの扱いを明示する。初期対応は閉じた塗り領域を優先し、開パスの線放出は別モードとして設計する。
- Path revision が変わった時だけサンプリング分布を再構築し、フレームごとの再三角形化を避ける。

### GE-3: Source ownership and serialization

- レイヤー参照または immutable geometry snapshot のどちらを保存するか決める。参照切れ、レイヤー削除、複製、プロジェクト再読込の振る舞いを規定する。
- 参照 ID と放出設定を既存のプロジェクト保存契約へ追加し、入力形状を複製保存しない。
- Mesh の変形・アニメーション時に、どの時点の geometry を使うか（現在フレーム追従／固定スナップショット）を明示する。

### GE-4: Editor workflow

- エミッター UI から Shape Layer / Mesh Layer を選択し、source kind (surface) と既存の Continuous/Burst モードを組み合わせられるようにする。
- レイヤー固有プロパティを通常 Property Widget に漏らさず、既存の Particle 専用編集面へ配置する。
- 新しい公開 signal/slot は導入せず、既存の明示更新・controller/service 境界を利用する。

### GE-5: GPU and runtime verification

- CPU ParticleSystem の決定的 replay とソフトウェア／GPU描画 snapshot が一致することを確認する。
- GPU FROMMESH shader を再利用する場合は Diligent backend-neutral API、buffer lifetime、source revision invalidation を先に定義し、CPU/GPU別仕様を生まない。
- 形状別、source変更、欠損source、保存／再読込、長時間連続放出を検証する。ビルド・テスト・実機確認は明示指示後に行う。

## Implementation record

- 2026-10-10: GE-1 の三角形入力・検証・面積分布キャッシュと Mesh / Surface birth sampling を Artifact ParticleEmitter に実装。UI、レイヤー参照、保存／再読込接続は未実装。

## Risks and decisions

- 現行 API は呼び出し側から三角形列を渡す段階であり、既存 Mesh Layer や Shape Layer を自動参照しない。
- Mesh surface のみ対応する。体積内放出、法線方向への速度継承、形状輪郭に沿った線放出は別途設計する。
- 生成ソースが動的に変わる場合、分布再構築コストとフレームキャッシュ無効化を制御する必要がある。
