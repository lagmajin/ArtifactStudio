# Keyframe State Spec

作成日: 2026-06-17  
対象: `Artifact/src/Timeline/ArtifactTimelineIconModel.cppm`, `Artifact/include/Timeline/ArtifactTimelineIconModel.ixx`,
`Artifact/src/Widgets/Timeline/ArtifactTimelineLabel.cppm`,
`Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`,
`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`,
`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`,
`Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`

位置づけ: キーフレームの見た目、編集可否、意味状態を分離して扱うための仕様書。

---

## 1. 目的

タイムライン上のキーフレームは、今後アイコン生成ヘルパーや描画補助が増えても破綻しないように、状態定義を先に固定する。

この仕様の狙いは次の3つ。

1. 見た目の一貫性を保つ
2. 編集状態と意味状態を混ぜない
3. アイコン生成をプログラム化しやすくする

---

## 2. 基本方針

キーフレームは「単一の状態」ではなく、3層で表す。

1. **基礎状態**: 形と塗り
2. **編集状態**: 選択、フォーカス、ロック、無効
3. **意味状態**: 通常、ホールド、補間、イージング、ダミー

表示は `基礎状態 + 編集状態 + 意味状態` の合成で決める。

---

## 3. 基礎状態

### 3.1 形

標準形は **ひし形** を基本とする。

補助形は必要最小限に留める。

- `diamond`
- `circle`
- `square`

ただし、タイムライン上の通常キーフレームは原則として `diamond` を使う。

### 3.2 充填

充填は次の4段階で扱う。

- `outline`
- `filled`
- `semiFilled`
- `disabled`

標準の通常キーは `outline`、現在フレーム上のキーは `filled` を基本とする。

---

## 4. 編集状態

編集状態は、表示優先度の高い順に扱う。

1. `selected`
2. `focused`
3. `hovered`
4. `locked`
5. `disabled`

### 4.1 選択

選択中のキーフレームは、枠線を少し太くして識別する。

### 4.2 ロック

ロック状態は、鍵バッジなどの小さな補助記号で表す。

### 4.3 無効

無効状態は、彩度を落として透過気味にする。

---

## 5. 意味状態

意味状態は、キーフレームが何を表すかを示す。

- `normal`
- `hold`
- `ease`
- `auto`
- `manual`
- `dummy`

### 5.1 通常

何も付かない通常キー。

### 5.2 ホールド

値が保持されるキー。角を少し硬く見せる、または小バッジで示す。

### 5.3 補間

補間ありのキー。小さな曲線や矢印のバッジで表す。

### 5.4 ダミー

編集の都合で置かれるが、ユーザーに主役として見せないキー。

---

## 6. 表示ルール

### 6.1 優先順位

見た目の優先順位は次の順とする。

1. `disabled`
2. `locked`
3. `selected`
4. `current frame`
5. `meaning badge`
6. `base shape`

### 6.2 現在フレーム

現在フレーム上のキーフレームは、最も強く目立たせる。

ただし、ロックや無効の状態を上書きしない。

### 6.3 色

色は状態そのものではなく、できるだけトラックやレイヤー種別に寄せる。

状態差分は主に次で表す。

- 塗り
- 線幅
- 透過
- バッジ

---

## 7. 推奨ステート定義

実装では次の3群に分けると扱いやすい。

### 7.1 基礎ステート

- `empty`
- `filled`
- `semiFilled`

### 7.2 編集ステート

- `normal`
- `selected`
- `hovered`
- `focused`
- `locked`
- `disabled`

### 7.3 意味ステート

- `normal`
- `hold`
- `ease`
- `auto`
- `manual`
- `dummy`

---

## 8. アイコン生成方針

キーフレームアイコンは、AI 生成よりも **プログラム生成** を基本とする。

理由:

1. 16px 前後でも読みやすさを保ちやすい
2. 状態差分を機械的に合成しやすい
3. 既存 UI のトーンに揃えやすい

AI は初案の意匠確認には使えるが、最終実装は SVG か描画コードで固定する。

---

## 9. 生成ヘルパーの想定

将来的には次のようなヘルパーを置く。

- `KeyframeIconHelper`
- `KeyframeStateStyle`
- `KeyframeBadge`

役割は次のとおり。

- 基本形を描く
- 状態ごとの色と線を合成する
- バッジを重ねる

---

## 10. 実装メモ

- 状態を増やしすぎない
- 基本形はひし形に固定する
- ロック、無効、選択中は同時に表現できるようにする
- 現在フレームだけは強調度を別扱いにする
- まずは `base + modifiers` の合成で設計する

---

## 11. 初期案

最初の具体案は次の組み合わせを推奨する。

- 通常キー: 空のひし形
- 現在フレーム: 塗りつぶしひし形
- 選択中: 枠線強調
- ロック: 鍵バッジ
- ホールド: 角が硬いひし形
- 補間あり: 小さな曲線バッジ
- 無効: 低彩度 + 透過

---

## 12. 次の作業

1. この仕様を基準にアイコン状態 enum を決める
2. `KeyframeIconHelper` の責務を切る
3. タイムライン描画側の参照点を1か所に寄せる
4. 必要ならアイコン案を SVG 化する

