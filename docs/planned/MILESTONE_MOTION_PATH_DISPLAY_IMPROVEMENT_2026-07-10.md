# MILESTONE: モーションパス表示改善（Spatial Bezier / 速度可視化 / 適応サンプリング）

> 2026-07-10 作成

## 目的

ビューポートのモーションパスオーバーレイを「読める」だけの表示から、
After Effects 相当の「編集できる・速度が分かる」表示へ引き上げる。
特に spatial bezier タンジェントハンドルの描画・編集、速度が読めるドット表現、
ズーム/曲率に応じた適応サンプリングを導入する。

## 背景

現状の実装（`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`）:

- 有効化: `showMotionPathOverlay_` フラグ（ツールバー / Shading メニュー /
  タイムライングローバルスイッチ）。設定は `compositionShowMotionPathOverlay`
  （`UI/Composition/ShowMotionPathOverlay`）に永続化。
- サンプリング: `motionPathPositionKeyTimes()` で `transform.position.x/y` の
  キー時刻を収集し、min〜max フレームを **2 フレーム固定間隔**（`f += 2`）でサンプル。
- パス生成: 各フレームで `getGlobalTransformAt(f)` を計算しアンカーをワールド座標へ
  マップ。結果を `motionPathCache_` にキャッシュ。
- 描画: past=ピンク / future=青 の直線接続、6 フレームごとのタイムドット、
  現在フレームの黄色マーカー、補間タイプ別のキー点、破線 bounding rect。
- 編集: overlay 上でキー点ドラッグ移動 / Shift 追加 / Alt 削除、
  `hitTestMotionPathSample` でヒット判定。

弱点:

| 弱点 | 内容 |
|---|---|
| 空間ベジェ非対応 | パスは点を直線接続するのみ。spatial tangent handle が無く曲線編集不可 |
| 固定サンプリング間隔 | `f += 2` 固定でズーム/速度非依存。拡大時カクつき・高速移動で粗い |
| 速度表現の欠落 | AE のドットは「1 フレーム 1 ドット」で間隔=速度。6 フレーム間引きで速度情報が消える |
| 現在マーカーのズレ | `f % 2` 近傍サンプルのため奇数フレームで実位置とズレる可能性 |
| コード可読性 | 描画ロジックが 1 関数内に肥大、キャッシュ構造と密結合 |

## ターゲット像

- position.x/y の spatial bezier タンジェントを描画し、ハンドルドラッグで空間補間を編集できる。
- ドット表現が速度を反映する（速いほど間隔が広い）。
- ズーム・曲率に応じてパスのサンプリング密度が変わり、拡大しても滑らか。
- 現在フレームマーカーが常に実補間位置に一致する。
- overlay 描画がヘルパへ分離され、保守しやすい。

## 非ゴール（このマイルストーンの範囲外）

- モーションパスの物理シミュレーション / 自動スムージング AI
- 3D 空間（Z 軸）のフルモーションパス（まず 2D spatial bezier に集中）
- タイムライン側の F カーブ編集との統合再設計
- 複数レイヤーの相対パス表示の高度化

## 現状とギャップ

| 項目 | 現状 | ギャップ |
|---|---|---|
| パス形状 | 直線接続 | spatial bezier 曲線が無い |
| タンジェント | 保持のみ / 未描画 | ハンドル描画・編集が無い |
| サンプリング | 2 フレーム固定 | 適応サンプリングが無い |
| ドット | 6 フレーム間引き | 速度可視化になっていない |
| 現在マーカー | 近傍サンプル | 実補間位置と一致しない場合あり |
| コード構造 | 巨大関数内に密結合 | overlay ヘルパ分離が無い |

## 設計原則

1. spatial bezier の**描画**を先に入れ、**編集**は形が見えてから重ねる。
2. サンプリングと描画を分離し、キャッシュはサンプリング結果を保持する。
3. 既存の toggle / 編集操作（ドラッグ / Shift / Alt）の挙動を壊さない。
4. overlay 描画は `MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY` の overlay 経路と衝突させない。
5. 速度可視化は AE の「フレーム毎ドット」を基準にしつつ、負荷は適応的に抑える。

## Scope（想定する変更ファイル）

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - `buildMotionPathSamples()` / `motionPathPositionKeyTimes()` /
    `motionPathPositionInterpolation()` / `motionPathInterpolationColor()`
  - 描画ループ（`showMotionPathOverlay_` セクション, おおよそ `:25839` 付近）
  - `MotionPathCacheEntry` / `hitTestMotionPathSample()`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- 空間補間の取得元: `ArtifactCore` の transform / property の spatial 情報
  （`transform.position.x/y` の keyframe tangent / spatial bezier）

## Phases

### Phase 1: サンプリング / 描画の分離

巨大な描画ループを保守可能な単位へ分ける。

- モーションパスのサンプリング結果生成を独立関数へ抽出
- 描画（ライン / ドット / キー点 / bounding rect）をヘルパへ分離
- `motionPathCache_` はサンプリング結果を保持する形へ整理
- 既存の見た目・挙動を維持（リファクタのみ）

**Done when:**

- 描画結果が従来と一致する
- サンプリングと描画が分離されている

### Phase 2: 適応サンプリング

固定間隔サンプリングを曲率 / ズーム依存へ変える。

- ズームレベルに応じてサンプル密度を調整
- 曲率が高い区間はサンプルを増やし、直線区間は減らす
- サンプル数の上限を設けて負荷を抑える

**Done when:**

- 拡大してもパスがカクつかない
- 高速移動区間が粗くならない

### Phase 3: 速度可視化ドット

AE 相当の速度が読めるドット表現にする。

- フレーム毎（or 等時間）ドットを基準にし、間隔=速度になる表示
- 過去 / 未来 / 現在の色分けは維持
- ドット密度は適応サンプリングと整合させる

**Done when:**

- ドット間隔で速度の緩急が読める
- 現在フレームマーカーが実補間位置と一致する

### Phase 4: Spatial Bezier 描画

パスを直線接続から spatial bezier 曲線へ。

- `transform.position.x/y` の spatial tangent を取得
- ベジェセグメントとしてパスを描画
- タンジェントハンドル（in/out）を描画（この段階は表示のみ）

**Done when:**

- パスが曲線として描かれる
- タンジェントハンドルが可視化される

### Phase 5: Spatial Bezier 編集

タンジェントハンドルをドラッグ編集可能にする。

- ハンドルのヒットテストとドラッグ
- in/out ハンドルの連動 / 分離（Alt でブレイク等）
- Undo/Redo 統合（既存 `MotionPathUndoCommand` 系を拡張）

**Done when:**

- ハンドルドラッグで空間補間を編集できる
- 編集が Undo/Redo できる

## Recommended Order

1. Phase 1 (分離)
2. Phase 2 (適応サンプリング)
3. Phase 3 (速度可視化)
4. Phase 4 (spatial bezier 描画)
5. Phase 5 (spatial bezier 編集)

### Why This Order

- Phase 1 で構造を整理しないと以降の変更が既存の巨大ループを悪化させる。
- 適応サンプリングは spatial bezier 描画の下地になる（曲線の細分化に使える）。
- 速度可視化はサンプリングが整ってから入れる方が破綻しにくい。
- spatial bezier は描画を先に固め、編集は形が見えてから重ねる。

## 連携先

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`（コンテキストメニュー / toggle）
- `Artifact/src/Widgets/ArtifactTimelineGlobalSwitches.cppm`（表示切替）
- `ArtifactCore` transform / property の spatial 補間情報
- 関連: `docs/planned/MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY_2026-04-10.md`（overlay 経路）
- 参照: `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`（監査記述の更新も検討）

## Validation Checklist

- Phase 1 後、描画結果が従来と一致する
- 拡大してもパスが滑らか
- ドット間隔で速度の緩急が読める
- 現在フレームマーカーが実補間位置に一致
- パスが spatial bezier 曲線で描かれる
- タンジェントハンドルをドラッグ編集でき、Undo/Redo できる
- 既存の toggle / キードラッグ / Shift 追加 / Alt 削除が壊れない

## Notes

現状の描画本体は `:25839` 付近で実装済み（`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02`
の「render 本体はコメントアウト / stub」という記述は古く、実態と乖離。監査の更新も
このマイルストーンで検討する）。最大の価値は spatial bezier ハンドルで、これにより
「読める」から「編集できる」へ引き上がる。

---

## Next Execution Slice

Phase 1 から入る。まずは既存挙動を変えずに描画とサンプリングを分離する。

### Phase 1A の着手点

1. `showMotionPathOverlay_` セクション（`:25839` 付近）の描画をヘルパ関数へ切り出す
2. サンプリング結果生成（path points / key points）を独立関数化する
3. `MotionPathCacheEntry` の役割をサンプリング結果保持に絞る
4. リファクタ前後で描画結果が一致することを確認する

### Phase 1 完了条件

- 描画結果が従来と一致
- サンプリングと描画が分離
- toggle / 編集操作が壊れない

### Phase 1A 実装着手メモ

- まず `ArtifactCompositionRenderController.cppm` の `showMotionPathOverlay_` ブロックで、
  キャッシュ再生成と描画処理をそれぞれ独立したヘルパ単位に分ける
- 既存の dot / keyframe / current-marker の見た目は維持し、振る舞い変更は入れない
- 以降の Phase 2 以降は、この分離された土台の上に積む

### Phase 2 の前提

- サンプリング密度を決めるズーム / 曲率の取得点を確認
- サンプル上限を設けて負荷が跳ねないようにする
- 適応サンプリングが Phase 4 の曲線細分化に流用できる形にする
