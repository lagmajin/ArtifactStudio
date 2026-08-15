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

## 実装状況メモ (2026-07-07)

- `ArtifactLayerPanelWidget` で matte link 用の drag mode が実装済み
- `Alt + Drag` で Track Matte を設定でき、self-reference / cycle は拒否される
- `ChangeLayerMatteReferencesCommand` を通して Undo/Redo が積まれる
- Inspector 側にも matte 参照の切り替え導線と badge 表示があり、文書の Phase 2/3 はかなり進んでいる
- いま残っているのは、移動時の dangling reference の扱いと、見た目の微調整・説明整理
- 削除時の dangling matte reference は project 層で掃除するようになった

---

## 未着手要素

- レイヤーパネルからのドラッグ＆ドロップでトラックマット受け側レイヤーを指定する UI
- ドラッグ中のターゲットハイライトとドロップ許可条件の制御
- ターゲットレイヤーが削除・移動されたときの dangling reference 処理

## 進捗メモ

- レイヤーパネルの `Alt + Drag` で matte link を設定できる
- matte の self-reference / cycle は UI と表示の両方で拒否・警告する
- Inspector の Matte 行はクリックで最初の source へ focus できる
- Inspector の Matte 行は右クリックで `MatteType` を切り替えられる
- Layer Panel の matte badge は source 名と type を表示する

## 現行コード照合（Update 2026-08-15）

`ArtifactLayerPanelWidget` には Alt-drag の matte link mode、matte slot／layer drop、target tooltip／highlight、self-reference／cycle 拒否、source 置換、`ChangeLayerMatteReferencesCommand` による Undo／Redo が存在する。Inspector の参照表示と MatteType 切替も確認できるため、上記の「未着手要素」のうち基本 UI は実装済みとして扱う。

残る確認課題は、削除・移動後の dangling reference の全経路、複数 matte slot の runtime 表示、実機での drop affordance と Undo／Redo 受入である。

## Update 2026-08-15

- `ArtifactLayerPanelWidget`／`ArtifactLayerPanelPresentation` と `ChangeLayerMatteReferencesCommand` の現行経路を確認し、Alt+Drag による matte link、self-reference／cycle 拒否、Undo/Redo、matte badge 表示は実装済みとして扱う。
- `ArtifactMatteReferenceRule` と Render Queue 側の診断には missing source、self-reference、hidden source、cycle の検出があり、削除時の参照掃除も `ArtifactAbstractComposition`／Undo 経路に存在する。
- したがって Phase 1〜3 の主要基盤は実装済み。ただし実機でのドラッグ操作、drop target の視覚的ハイライト、dangling reference の移動／再配置ケース、合成結果の runtime 受入は未検証とする。

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
