# MILESTONE: Aspect Ratio / Resolution Remap Wizard - 2026-06-07

作成日: 2026-06-07  
対象: aspect ratio 変更時のマスク / keyframe / anchor 再計算  
優先度: 🟠 高

---

## 目的

コンポの解像度や aspect ratio を変更したときに、マスク・キーフレーム・アンカーポイントが崩れないようにする。

現在は `HD -> Square` のような変更で座標系がずれ、手動で個別調整するしかない。
このマイルストーンでは、解像度変更時に **自動再計算ウィザード** を提供する。

---

## 問題

### 1. aspect ratio 変更で座標が崩れる

不満:
- 1920x1080 で作ったコンポを 1080x1080 に変えると、マスクやシェイプが崩れる
- ピクセルアスペクト比の扱いが絡むが、手動で直すしかない
- キーフレームやアンカーポイントも一緒にずれる

改善:
- resolution change 時に再計算ウィザードを出す
- どの基準で維持するかを選べるようにする
- 既存の mask / keyframe / anchor をまとめて remap する

完了条件:
- 変更前と変更後の差分が明示される
- 位置・スケール・アンカーの再計算方針を選べる
- 手動調整の手数を大幅に減らせる

---

## 変換ポリシー

ウィザードでは最低でも次の保持基準を選べるようにする。

- `Center Locked`
  - 中央を基準に再配置する
- `Top Left Locked`
  - 左上を固定する
- `Stretch To Fit`
  - 変更先 aspect ratio に合わせて引き伸ばす
- `Fit With Padding`
  - 比率を維持して余白を持たせる
- `Fit With Crop`
  - 余白を削って見た目を維持する

---

## 実装の読み替え

### Resolution Change

- 単なる size 変更ではなく、content bounds と anchor の remap として扱う
- `visibleBounds` / `layoutBounds` / `maskBounds` を参照して再計算する
- pixel aspect ratio も含めて、表示上の座標変換を統一する

### Wizard

- 変更前に何が動くかを説明する
- 変更後の preview を見せる
- 影響が大きい場合は warning を出す

---

## 詳細実装スライス

### A. Remap Wizard

#### 入口
- Composition settings dialog
- Project View の composition action
- output preset / responsive layout panel

#### 触るもの
- `ArtifactCompositionInitParams`
- `ArtifactAbstractComposition`
- `ArtifactCompositionMenu`
- `ArtifactProjectManagerWidget`
- layout / bounds query

#### データ契約
- old resolution
- new resolution
- old aspect ratio
- new aspect ratio
- pixel aspect ratio
- remap policy

#### 実装順
1. 変更前の差分を計算する
2. policy を選ばせる
3. 変更後 preview を出す
4. 確定時に remap を適用する

#### 失敗時の扱い
- 変換不能な項目は個別に警告する
- 互換維持できない場合は適用を止める
- preview と実適用が違う場合は silent fix しない

#### Phase 1: preflight
- [ ] 変更前の resolution / aspect ratio 差分を表示する
- [ ] mask / keyframe / anchor への影響を列挙する
- [ ] 変更不能項目を warning 化する

#### Phase 2: policy selection
- [ ] `Center Locked` / `Top Left Locked` / `Stretch To Fit` を選べるようにする
- [ ] `Fit With Padding` / `Fit With Crop` を選べるようにする
- [ ] 変更後の見た目方針を明示する

#### Phase 3: preview and apply
- [x] 変更後 preview を表示する (AspectPreviewWidget — 青=old / 橙=new のアスペクト比可視化)
- [x] 確定時に remap を適用する (applyResolutionRemap — Phase 2で実装済み)
- [ ] undo / redo できるようにする (要 QUndoCommand 基盤)

---

### B. Mask / Keyframe / Anchor Remap

#### 入口
- composition size change
- mask editor
- property editor / timeline keyframes

#### 触るもの
- mask path
- keyframe tracks
- anchor point
- bounds query
- transform / layout mapping

#### データ契約
- original bounds
- target bounds
- anchor lock mode
- scale policy
- offset policy

#### 実装順
1. mask 座標を target bounds に remap する
2. anchor point を保持方針に従って再計算する
3. keyframe 位置を統一ルールで調整する
4. layout variant と整合させる

#### 失敗時の扱い
- path が壊れるなら warning を出す
- keyframe が変換不能ならその範囲を示す
- anchor だけ別の基準で動かないようにする

#### Phase 1: bounds-based remap ✅ (Phase 2で実装)
- [x] mask path を old bounds -> new bounds で remap する
- [x] anchor point を bounds basis で再計算する (detection only — 実際のremapは未実装)
- [x] keyframe positions を変換する (mask animation keyframesのみ)

#### Phase 2: policy-based offsets ✅
- [x] center locked の offset を導入する
- [x] top-left locked の offset を導入する
- [x] stretch / fit / crop の挙動を分ける

#### Phase 3: diagnostics
- [x] どの要素がどれだけ動いたかを示す (Impact group — mask vertex count / anchor / keyframe 表示)
- [x] 変更不能な要素を明示する (hasImpact() で空の場合は表示)
- [x] preview と結果の差分を読めるようにする (AspectPreviewWidget + Impact summary)
- [ ] プロパティキーフレームのremap (要 AnimatableValue 走査 + 座標変換)

---

## 推奨実行順

1. Remap Wizard
2. Mask / Keyframe / Anchor Remap

理由:
- まず wizard でユーザーに意図を選ばせる方が安全
- その後に具体要素の remap を policy で実装すると整理しやすい

---

## 関連

- [`docs/planned/MILESTONE_RESPONSIVE_LAYOUT_COMPOSITION_2026-06-05.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RESPONSIVE_LAYOUT_COMPOSITION_2026-06-05.md)
- [`docs/planned/MILESTONE_CONTENT_BOUNDS_SYSTEM_2026-06-07.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_CONTENT_BOUNDS_SYSTEM_2026-06-07.md)
- [`docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)
- [`docs/planned/MILESTONE_MATTE_MASK_TIME_REMAP_SPLIT_ROUTE_2026-04-24.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_MATTE_MASK_TIME_REMAP_SPLIT_ROUTE_2026-04-24.md)
- [`docs/planned/MILESTONE_AE_STYLE_MASK_EDITING_GRAMMAR_2026-06-02.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE_STYLE_MASK_EDITING_GRAMMAR_2026-06-02.md)

---

## 備考

- これは、aspect ratio / resolution 変更時の座標ズレを減らすための実装計画。
- ビルドやテストは実施していない。
