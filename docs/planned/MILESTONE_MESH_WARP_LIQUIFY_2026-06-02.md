# M-MOTION-5 Mesh Warp / Liquify Deformation (2026-06-02)

日付：2026-06-02
目標：画像を格子状のメッシュで自由に変形させる「メッシュワープ」ツールを実装する。顔修正、形状調整、 Liquify 風のプッシュ変形に対応。

---

## Goal

- 画像上に配置した格子メッシュをドラッグで変形
- AE の Liquify / Mesh Warp 相当の操作感
- PuppetPin との違い: 規則的なメッシュ全体を変形（局所ではなく面全体）

---

## Definition of Done

- [ ] **メッシュ生成** - 画像全体を NxM の四辺形メッシュに分割
- [ ] **頂点ドラッグ** - メッシュ頂点（格子点）を個別にドラッグ移動
- [ ] **領域ドラッグ** - 円形ブラシで「プッシュ」「引き伸ばし」「回転」「拡大/縮小」変形
- [ ] **リアルタイムプレビュー** - 変形結果を即座に Composition Editor で確認
- [ ] **リセット** - 選択頂点/メッシュ全体を初期状態に戻す
- [ ] **キーフレーム** - メッシュ頂点位置をキーフレーム可能に
- [ ] **メッシュ密度変更** - 4x4 〜 32x32 の範囲で調整可能
- [ ] **ブラシサイズ調整** - Liquify ブラシの半径をマウスホイールで調整

---

## Implementation Phases

### Phase 1: メッシュ変形エンジン

**ファイル**: `ArtifactCore/src/Deform/MeshWarp.cppm` (新規)

**完了条件**:
- [ ] `MeshWarpEngine` クラス: メッシュ定義 + 頂点移動 + 画像変形
- [ ] NxM メッシュ生成 (configurable density)
- [ ] バイリニア補間による変形後画像生成
- [ ] ブラシ変形（プッシュ/引き伸ばし/回転）

```cpp
class MeshWarpEngine {
public:
    struct Config {
        int cols = 16, rows = 12;
    };
    
    void init(int imageWidth, int imageHeight, const Config& cfg);
    
    // 単一頂点移動
    void moveVertex(int col, int row, float2 newPos);
    
    // ブラシ変形（円形範囲内の頂点を移動）
    enum BrushMode { Push, Expand, Rotate, Pinch };
    void brushDeform(float2 center, float radius, float intensity, BrushMode mode);
    
    // 変形実行
    ImageF32x4RGBA warp(const ImageF32x4RGBA& source);
    
    const std::vector<float2>& vertices() const;
    void resetVertices();
};
```

### Phase 2: メッシュ操作UI

**ファイル**: `Artifact/src/Widgets/Deform/ArtifactMeshWarpWidget.cppm` (新規)

**完了条件**:
- [ ] Composition Editor 上にメッシュワイヤーフレーム表示
- [ ] 頂点ピッキングとドラッグ移動
- [ ] 範囲選択（矩形/投げ縄）で複数頂点選択
- [ ] ブラシモード切替（Push / Expand / Rotate / Pinch）
- [ ] ブラシサイズ調整（ホイール）
- [ ] メッシュ密度調整UI
- [ ] リセットボタン

### Phase 3: キーフレーム連携

**完了条件**:
- [ ] メッシュ頂点群を1つのアニメーションプロパティとしてキーフレーム登録
- [ ] フレーム間のメッシュ補間（線形）
- [ ] タイムライン上での表示

---

## Dependencies

- ArtifactCompositionEditor (オーバーレイ描画)
- ImageF32x4RGBA (画像データ)

---

## Total Estimate

| Phase | 時間 |
|---|---|
| Phase 1: メッシュ変形エンジン | 8-12h |
| Phase 2: メッシュ操作UI | 8-12h |
| Phase 3: キーフレーム連携 | 4-6h |
| **合計** | **20-30h** |

---

## Static audit follow-up (2026-07-25)

現行ソースには `LiquifyEffect` があり、CPU 実装と Push 向け GPU 実装、Push / Pinch / Bloat / Twirl / Turbulence / Pucker 等のブラシ種別、半径・強度・中心・角度・mesh density のプロパティ、EffectService からの生成登録まで確認できる。したがって Liquify のエフェクト基盤は部分的に実装済みである。

一方、このマイルストーンが要求する独立した `MeshWarpEngine`、NxM 頂点の永続メッシュ、Composition Editor 上のワイヤーフレーム／頂点ピッキング UI、頂点群のキーフレーム補間、4x4〜32x32 密度 UI、ホイールによるブラシ半径操作は確認できない。既存の `OpenCVPuppetEngine` は Puppet 用の別経路であり、メッシュワープ完了の根拠にはしない。

### Audit status

- Phase 1: 部分実装 — Liquify の画像変形は存在するが、独立した MeshWarpEngine / 頂点メッシュ API は未確認
- Phase 2: 未実装相当 — 専用 Mesh Warp widget、ワイヤーフレーム、頂点／領域操作 UI は未確認
- Phase 3: 未実装相当 — メッシュ頂点群のキーフレーム登録・補間・タイムライン表示は未確認
- Definition of Done: Liquify 基盤の一部のみ達成。メッシュワープ機能全体は未完了
