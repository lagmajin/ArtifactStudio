# ArtifactCore 層 安全実装リスト

✅ UI / アプリ層に一切触れずに実装可能
✅ 単体テスト可能
✅ 後方互換性 100% 維持
✅ 既存コードを壊さない
✅ 後で全ての機能の下地になる

---

## ■ 今すぐ実装可能なコア層機能
### 🟢 優先度 最高 （半日 〜 1日）

---

#### ✅ 1. キーフレームベジェ補間ロジック
**実装場所:** `ArtifactCore/Animation/Interpolator.ixx`
**影響範囲:** ゼロ
**所要時間:** 4時間

✅ 状況:
- データ構造は既に完全に定義済み
- `interpolateValue()` メソッドがただ線形補間しているだけ
- どこからも呼ばれていないヘルパー関数として実装するだけ
- UI は全く必要ない。後から付ければ良い

✅ 実装内容:
```cpp
// 追加するだけ。既存コードは一切変更しない
double interpolateBezier(double t, double inX, double inY, double outX, double outY);
double interpolateBezierVelocity(double t, double inX, double inY, double outX, double outY);
```

✅ メリット:
- 後でグラフエディタを作るときにこのロジックをそのまま使える
- 単体テストで完全に検証可能
- 今の線形補間とフラグで切り替え可能

---

#### ✅ 2. トランスフォーム 行列計算
**実装場所:** `ArtifactCore/Animation/Transform2D.ixx`
**影響範囲:** ゼロ
**所要時間:** 3時間

✅ 状況:
- 現在は QTransform が直接使われている
- 親子階層の乗算ロジックがアプリ層に書かれている
- コア層に移管するだけ

✅ 実装内容:
```cpp
Transform2D Transform2D::operator*(const Transform2D& parent) const noexcept;
QTransform Transform2D::toQTransform() const noexcept;
```

✅ メリット:
- 親子階層実装時にこのロジックをそのまま使う
- 全てのレイヤーがこの計算を共有するようになる
- バグの発生箇所が一箇所に集中する

---

#### ✅ 3. イージング定数プリセット
**実装場所:** `ArtifactCore/Animation/Easing.ixx`
**影響範囲:** ゼロ
**所要時間:** 2時間

✅ 実装内容:
AE 互換のイージング定数を定義するだけ
- EasyEase
- EasyEaseIn
- EasyEaseOut
- Hold
- Linear

✅ メリット:
- 後から全ての箇所でこの定数を参照するだけで良くなる
- ファイルフォーマットの互換性が最初から確保出来る

---

### 🟢 優先度 高 （1日 〜 2日）

---

#### ✅ 4. 浮動小数点時間 内部表現
**実装場所:** `ArtifactCore/Time/Rational.ixx`
**影響範囲:** 極小
**所要時間:** 1日

✅ 状況:
- 現在フレーム番号 int64_t ベース
- サブフレーム、モーションブラー実装のためには Rational 時間が必須
- 既にデータ型は存在するが使われていない

✅ 実装内容:
フレーム番号から Rational 時間への変換メソッドを追加するだけ。
既存の int64_t ベースのAPIはそのまま残す。

---

#### ✅ 5. カラーブレンド関数
**実装場所:** `ArtifactCore/Graphics/Blend.ixx`
**影響範囲:** ゼロ
**所要時間:** 4時間

✅ 状況:
- 現在ブレンドモードは GPU シェーダーにしか実装されていない
- CPU 側で同じ計算が出来る必要がある
- 単体テスト可能

✅ 実装内容:
全てのブレンドモードの CPU リファレンス実装を書く。
```cpp
FloatColor blend(FloatColor dst, FloatColor src, BLEND_MODE mode, float opacity);
```

---

#### ✅ 6. レイヤー描画順序ソーター
**実装場所:** `ArtifactCore/Composition/LayerOrder.ixx`
**影響範囲:** ゼロ
**所要時間:** 3時間

✅ 実装内容:
レイヤーのリストを受け取って、正しい描画順序でソートする純粋関数を実装する。
- 調整レイヤー
- トラックマット
- 3D レイヤー
- ガイドレイヤー

✅ メリット:
後で実際にコンポジションエンジンに組み込む時にロジックを何も考えなくて良い。

---

## ■ ❌ 絶対に今はやらなくて良い事
- どんな種類の UI 変更
- Widget の改造
- 既存のメソッドのシグネチャ変更
- レンダラーの書き換え
- 既存の動作しているコードのリファクタリング

---

## ■ 作戦
**次の2週間はコア層にだけ専念する。**

1.  上記の機能を一つづつ実装する
2.  全て単体テストを書く
3.  アプリ層には一切 PR を出さない
4.  全て完了してから一気にアプリ層と接続する

この方法であれば、何も壊さずに着実に前進する事が出来ます。
途中でいつでも止める事が出来て、既存リリースに一切影響が出ません。
