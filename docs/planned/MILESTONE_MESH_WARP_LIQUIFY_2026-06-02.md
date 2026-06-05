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