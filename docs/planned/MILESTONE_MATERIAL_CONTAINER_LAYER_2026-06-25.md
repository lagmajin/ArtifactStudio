# M-MATCON-1: 素材コンテナーレイヤー設計

作成日: 2026-06-25
最終更新: 2026-08-15
ステータス: Phase 1 実装済み（静的確認 2026-07-29、ビルド／ランタイム確認待ち）
対象: `Artifact/include/Layer/*`, `Artifact/src/Layer/*`, `Artifact/src/Render/*`, `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`, `Artifact/src/Layer/ArtifactCloneLayer.cppm`, `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`
位置づけ: 複数素材を 1 レイヤー内の配列として保持し、通常表示では 0 番目だけを露出しつつ、クローナーやクローナートランスフォームから 1 番目以降も参照できるようにする。

---

## 1. 目的

素材コンテナーレイヤーは、画像・動画・シェイプ・プリコンポジションなどの素材候補を 1 つのレイヤーとして束ねるための layer model である。

基本挙動は配列に近い:

- `items[0]` が通常の露出素材として表示・レンダーされる
- `items[1...]` は通常レンダーには出ないが、明示参照可能な素材スロットとして保持される
- クローナー、クローナートランスフォーム、将来の scatter / instance 系機能は `sourceIndex` を指定して 0 番目以外を使える

これにより、「普段は 1 枚のレイヤーとして扱えるが、生成系では複数素材のバリエーション元になる」状態を作る。

---

## 2. 用語

| 用語 | 意味 |
|---|---|
| Material Container Layer / 素材コンテナーレイヤー | 複数素材スロットを持つ 1 つの `ArtifactAbstractLayer` 派生 |
| Material Slot / 素材スロット | コンテナー内の 1 要素。内部的には layer payload または source reference を持つ |
| Exposed Slot / 露出スロット | 通常表示に使うスロット。初期設計では常に index `0` |
| Source Index | クローナーなどが参照する素材スロット番号。未指定時は `0` |

---

## 3. 基本ルール

### 3.1 通常表示

- composition から見ると、素材コンテナーレイヤーは 1 つの通常 layer として振る舞う
- canvas / timeline / hierarchy / selection ではコンテナー自身を選択対象にする
- render path は `exposedIndex == 0` のスロットだけを描画する
- コンテナーの transform / opacity / blend / effects / masks / lock は、露出スロットの描画結果に対して適用する

### 3.2 0 番目以外の扱い

- `items[1...]` は「隠しレイヤー」ではなく「参照可能な素材候補」
- 通常の composition layer order には参加しない
- solo / visibility / render preflight では、明示参照されない限り通常表示対象に含めない
- diagnostics では、壊れた source path や unsupported payload をスロット単位で報告する

### 3.3 LayerVariant とは分ける

`LayerVariant` は「同じ layer の状態差分」、素材コンテナーは「複数の素材実体を束ねる配列」である。

そのため、素材スロットを `LayerVariant` として実装しない。variant 切替と clone source index が混ざると、保存形式・UI・レンダーキャッシュの意味が曖昧になる。

---

## 4. データモデル案

```cpp
struct MaterialContainerSlot {
    QString id;
    QString name;
    QString layerType;
    QJsonObject layerPayload;
    bool enabled = true;
};

class ArtifactMaterialContainerLayer : public ArtifactAbstractLayer {
public:
    int materialCount() const;
    int exposedIndex() const; // Phase 1 は常に 0

    ArtifactAbstractLayerPtr exposedLayer() const;
    ArtifactAbstractLayerPtr materialLayerAt(int index) const;

    void appendMaterial(ArtifactAbstractLayerPtr layer);
    bool replaceMaterialAt(int index, ArtifactAbstractLayerPtr layer);
    bool removeMaterialAt(int index);
};
```

設計上の注意:

- `ArtifactMaterialContainerLayer` は composition の child layer tree とは別に素材スロットを持つ
- スロット内部 layer は親 composition に直接 append しない
- スロット layer の `composition()` は、必要なら container owner 経由で読み取り専用の context を解決する
- Phase 1 では `exposedIndex` を固定 0 とし、UI からの露出スロット切替は入れない

---

## 5. JSON 形式

```json
{
  "layerType": "MaterialContainer",
  "layerName": "Material Container",
  "materialContainer": {
    "schemaVersion": 1,
    "exposedIndex": 0,
    "slots": [
      {
        "id": "slot-0",
        "name": "Primary",
        "enabled": true,
        "layerType": "Image",
        "payload": {}
      }
    ]
  }
}
```

互換方針:

- 古い project には影響しない新規 `layerType` として導入する
- 単一素材から変換する場合、元 layer payload を `slots[0].payload` に格納する
- `exposedIndex` は将来拡張用に保存するが、Phase 1 の runtime は `0` 以外を通常表示に使わない
- `slot.id` は clone 参照や future diff に備えて安定 ID とする

---

## 6. クローナー連携

クローナー系は source layer に加えて source index を持つ。

```cpp
struct CloneSourceRef {
    LayerID sourceLayerId;
    int sourceIndex = 0;
};
```

参照解決:

1. `sourceLayerId` の layer を composition から取得
2. layer が `ArtifactMaterialContainerLayer` なら `materialLayerAt(sourceIndex)` を使う
3. layer が通常 layer なら `sourceIndex == 0` のみ有効とする
4. 範囲外 index は `0` fallback か skipped clone として扱う。Phase 1 では diagnostics に warning を出す方針

将来の分配モード:

- `Fixed`: 全 clone が同じ `sourceIndex`
- `Cycle`: clone instance index に応じて `sourceIndex = instanceIndex % materialCount`
- `Random`: seed と instance id から安定ランダムに source index を決める
- `Map`: property / effector / texture map から source index を決める

---

## 7. UI 方針

### 7.1 Hierarchy / Timeline

- 既定では 1 行の layer として表示する
- 展開時に Material Slot 一覧を表示できるようにする
- slot 行は composition layer と同じ reorder 対象にしない
- `0` 番目には `Primary` または `Exposed` 表示を付ける

### 7.2 Property

- `Material Slots` セクションを追加する
- `0` 番目を Primary として表示する
- `Add Slot`, `Replace Slot`, `Remove Slot`, `Move Slot Up/Down` を用意する
- Phase 1 では exposed slot 切替 UI は出さない

### 7.3 Asset Browser

- 複数 asset をドロップした場合、素材コンテナーレイヤーとして作成できる導線を将来追加する
- 単一 asset ドロップは既存 image/video layer 作成を維持する

---

## 8. Render / Cache 方針

- 通常 composition render は exposed slot だけを評価する
- clone render は `CloneSourceRef` 解決後の slot layer を source として評価する
- コンテナー自体の transform / effects は、通常表示では exposed slot result に適用する
- clone source として slot を使う場合、コンテナー transform を含めるかどうかは clone mode の責務にする
- cache key には `containerLayerId`, `slotId`, `sourceIndex`, `frame`, `sourcePayloadVersion` を含める

`QImage` を新規 hot path に入れない。slot payload の評価は既存 layer render が返す `ImageF32x4_RGBA` 等の typed image / render target を使う。

---

## 9. Non-goals

- `LayerVariant` の置き換え
- composition の通常 layer tree を多重化すること
- Phase 1 での exposed slot 切替 UI
- Phase 1 でのクローナー random / map 分配
- Diligent / D3D12 backend の低レベル変更
- `ArtifactWidgets` や `libs/DiligentEngine` の変更

---

## 10. 実装フェーズ

### Phase 1: モデルと保存復元

- `ArtifactMaterialContainerLayer` を追加
- `MaterialContainerSlot` と JSON 保存復元を追加
- 通常レンダーは `slots[0]` のみ
- property に読み取り中心の slot 一覧を追加
- diagnostics に empty container / broken slot source を追加

### Phase 2: 編集導線

- 複数 layer / asset から素材コンテナーを作成する command を追加
- slot 追加・削除・並べ替えを undo 対応する
- hierarchy / timeline で slot 展開表示を追加

### Phase 3: クローナー連携

- clone source reference に `sourceIndex` を追加
- fixed / cycle 分配を追加
- 範囲外 index warning と fallback を整備
- clone transform 側から slot index を keyframe / effector で制御できる下地を作る

### Phase 4: 最適化

- slot 単位 cache key を整理
- clone instance から同一 slot を大量参照する場合の shared evaluation を検討
- project health / render preflight のスロット単位表示を整える

---

## 11. 未決事項

- slot payload を完全な layer JSON とするか、asset reference 中心の軽量 payload に寄せるか
- slot 内 layer の transform を保持するか、素材として transform を無視するか
- clone source として使う場合に container effects を含めるか
- `exposedIndex` を将来 UI で切り替えるか、常に 0 番目固定のままにするか
- audio / video duration を複数 slot でどう扱うか

---

## 12. 最初に作るべき薄い slice

1. `MaterialContainer` layer type を追加
2. JSON に `slots` を保存復元
3. `slots[0]` のみを通常表示する
4. Property に slot 数と Primary slot を表示する
5. clone 連携はまだ入れず、設計上の `sourceIndex` だけ残す

## 2026-07-29 実装監査

- `ArtifactMaterialContainerLayer` と `MaterialContainerSlot` が存在し、layer factory から `LayerType::MaterialContainer` として生成できる。
- `materialContainer.slots`、slot id／name／enabled、slot 内 layer payload、`exposedIndex` の JSON 保存復元が実装されている。
- 通常の `draw()` は exposed layer のみを描画し、`localBounds()` も exposed layer を基準にする。Property には slot 数と exposed index が表示される。
- よって「専用 layer の追加」「JSON に slots を保存復元」「露出 slot の通常表示」「Property の slot 数と Primary slot 相当の表示」は Phase 1 の実装済み範囲と判定する。
- clone 側の `sourceIndex` 参照、複数素材からの作成 command、slot の undo／並べ替え、hierarchy 展開、slot 単位 diagnostics は未確認／未実装であり、Phase 2〜4 は未完了とする。
- `Material`／PBR、3D model の base color／metallic-roughness texture は既存の別責務であり、素材コンテナーレイヤーの実装とは区別する。

この slice なら既存 composition tree と clone render に深く踏み込まず、後からクローナー連携を足せる。

## Update 2026-08-15

- `ArtifactMaterialContainerLayer` は現行コードで LayerFactory に登録され、`MaterialContainerSlot` の id／name／enabled／payload、`exposedIndex`、slot 配列の JSON 保存・復元、exposed layer の通常描画、slot 数／exposed index の property 表示を確認できる。Phase 1 の判定は維持する。
- 現行コード検索では、Clone 系の source reference が Material Container の `materialAt(sourceIndex)` を解決する接続、複数 layer／asset から container を作る command、slot の reorder／編集専用 Undo、Hierarchy の slot 展開、slot 単位 diagnostics／cache key は確認できない。
- `setExposedIndex()` は存在するが、Phase 1 の「UIから露出 slot を切り替えない」方針と、runtimeでの切替受入れは分けて扱う。exposed slot 切替を完了機能とは判定しない。
- よって現状は `Phase 1 static implementation confirmed / Phase 2〜4 clone integration, editing UX, undo, diagnostics and runtime validation pending` と整理する。Material／PBR の既存実装は引き続き別責務である。
