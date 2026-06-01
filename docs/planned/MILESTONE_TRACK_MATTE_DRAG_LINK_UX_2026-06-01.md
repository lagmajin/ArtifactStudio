# Milestone: Track Matte Drag-Link UX

作成日: 2026-06-01
親: `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md` (Track Matte / Mask / Blend)
関連: `MILESTONE_PROPERTY_REFERENCE_LINKING_2026-05-11.md` (Property Row Pick Whip Surface)

---

## 目的

レイヤーパネル上で「レイヤーをドラッグしてトラックマットに設定する」という
After Effects 標準操作を実現する。MatteType 列挙と LayerMatteReference の
データモデルは既に存在するため、これを Inspector / Layer Panel の UI 操作へ
接続する段階に入る。

## 仕様の骨子

1. `drag source`
   - レイヤー行をドラッグ元にする
   - reorder 用 DnD と matte link 用 DnD を分ける
   - modifier で matte link を明示できるようにする
2. `drop target`
   - matte を受ける行または matte 専用スロットをドロップ先にする
   - ハイライトは「並び替え候補」ではなく「参照先候補」として出す
   - self-reference と循環参照は拒否する
3. `result`
   - drop の結果は layer reorder ではなく `TrackMatteReference` の作成・更新にする
   - `Alpha / Luma / Inverted` の mode を維持する
   - undo / redo で元に戻せるようにする

---

## 既存の土台

- `Artifact/include/Mask/LayerMask.ixx` — mask path 管理
- `Artifact/include/Layer/ArtifactLayerMatte.ixx` — `MatteType` (Alpha/Luma/Inverse), `MatteBlendMode`, `TrackMatteReference`
- `Artifact/src/Mask/LayerMask.cppm` — `compositeAlphaMask` でルミナンス・アルファ合成済み
- `Artifact/src/Undo/UndoManager.cppm` — `MaskEditCommand`

---

## 未着手要素

- レイヤーパネルからのドラッグ＆ドロップでトラックマット受け側レイヤーを指定する UI
- Inspector の Matte セクションでの受け側レイヤー参照の可視化・変更
- ドラッグ中のターゲットハイライトとドロップ許可条件の制御
- MatteType 切替（Alpha / Luma / Inverted Luma）を Inspector から即切替
- ターゲットレイヤーが削除・移動されたときの dangling reference 処理

---

## フェーズ

### Phase 1: Inspector Matte セクション強化

- Inspector に Matte グループを表示（現在は存在するが、ターゲットレイヤー選択導線がない）
- レイヤー参照をクリックしたときに Project / レイヤーパネルへ focus jump
- MatteType トグル（Alpha / Luma / Inverted Luma）の即時切替
- Undo via `AddLayerCommand` / `RemoveLayerCommand` を流用

### Phase 2: レイヤーパネル DnD

- レイヤー行を Alt + ドラッグで `matte link` モードへ移行できるようにする
- ドラッグ中、受容先レイヤー行の左端に参照先ハイライトを出す
- ドロップで `TrackMatteReference` を設定し、Undo を積む
- 条件: マット化できるのは「単一レイヤーまたはグルー 1 つ」のみ
  - precomp / footage / null / 3D model をターゲット可能
  - 自身をマットに設定する循環参照を拒否
  - 既存の reorder DnD と見た目を混同しない

### Phase 3: Matte 視覚的確認

- レイヤーパネル行の左端アイコンでマット関係を表現（インジケータ + ホバーでタイプ表示）
- Inspector に「Matte Source: <Layer Name> (Alpha)」のサマリ表示
- マットが魔のモードの場合、合成結果のプレビューから破綻検知フラグを更新

---

## 検証条件

- レイヤー A をレイヤー B のマットとして DnD 設定 → Composition Viewer で
  アルファ合成が正しく行われる
- MatteType 切替で Luma / Inverted Luma のビットが切り替わる
- Undo / Redo でマット設定が巻き戻る
- マット対象レイヤーを削除 → MatteReference がクリアされ fallback 描画になる
- 循環参照ドロップ → 拒否通知が表示される

---

## 関連ファイル（対象）

- `Artifact/include/Layer/ArtifactLayerMatte.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyWidget.cppm`
- `Artifact/src/Undo/UndoManager.cppm`

---

## 見積

- Phase 1: 8–12h
- Phase 2: 12–18h
- Phase 3: 6–10h

合計: 26–40h
