# マイルストーン: PMD / PMX / VMD / Lottie インポート対応

**最終更新:** 2026-08-15

モーショングラフィッカーが最も必要としているインポートフォーマットの実装計画。
最小限、動く状態を最優先に段階的に実装する。

> 2026-04-19 作成

---

## 方針

✅ **完璧より動くことを優先**
- 最初のバージョンは機能不足で良い
- 正しく表示されなくても良い、とにかく開くことが最優先
- 後から徐々に品質を向上させる

✅ **既存パイプラインを最大限再利用**
- 新規にシステムを作らない
- 既存の `Mesh` / `AnimationTrack` / `Layer` 構造にマッピングする

✅ **段階的リリース**
- 1フェーズ毎にリリース可能な状態にする
- ユーザーからのフィードバックを受けながら改善する

---

## Phase 0: 事前準備 ✅ 完了

- [x] `FileTypeDetector` に `.pmd` / `.pmx` / `.vmd` / `.lottie` を登録
- [x] マジックナンバー判定追加
- [x] プロジェクトビュー・アセットブラウザで正しくアイコン表示される

---

## Phase 1: PMD 最小限実装

**目標: PMDファイルを開いて頂点だけでも表示される状態にする**

- [ ] PMD バイナリヘッダーパーサ実装
- [ ] 頂点位置・面インデックスの読み込み
- [ ] 既存の `Mesh` 構造体に変換
- [ ] `MeshImporter` に登録
- [ ] Contents Viewer でワイヤーフレーム表示が出来る

**完了条件:**
> どんなPMDファイルをドラッグしてもクラッシュせず、何かしらの形が表示される状態。
> テクスチャ・ボーン・モーフは一切無視して良い。

**実装予定時間:** 2-3時間

---

## Phase 2: PMD マテリアル・テクスチャ

**目標: テクスチャ付きで表示出来るようにする**

- [ ] マテリアルテーブルの読み込み
- [ ] テクスチャパスの解決
- [ ] アルファ値・両面表示フラグのマッピング
- [ ] 既存の標準マテリアルシェーダーで描画

**完了条件:**
> 標準的なPMDモデルが、テクスチャ込みで凡そ正しい見た目で表示される。

---

## Phase 3: VMD モーションインポート

**目標: VMDファイルを読み込んで既存のアニメーショントラックに変換する**

- [ ] VMD バイナリパーサ実装
- [ ] ボーンアニメーションキーの読み込み
- [ ] 既存の `AnimationTrack` フォーマットに変換
- [ ] タイムラインに自動的にキーフレームが生成される

**完了条件:**
> VMDファイルをレイヤーにドラッグ＆ドロップすると、対応するボーンのキーフレームが全て打たれた状態になる。

---

## Phase 4: Lottie アニメーションインポート

**目標: Lottie JSONを読み込んで形状とアニメーションを再現する**

- [ ] Lottie JSON パーサ実装
- [ ] パス情報を既存の `ShapeLayer` 形式に変換
- [ ] 各プロパティのアニメーションを `AnimationTrack` に変換
- [ ] 最小限のトランスフォーム・不透明度・塗りアニメーションに対応

**完了条件:**
> 単純なLottieアニメーションがタイムライン上で再生出来るようになる。

---

## Phase 5: 品質向上

各フォーマットの品質を段階的に向上させる:

- [ ] PMD ボーン階層・スキニング対応
- [ ] PMD モーフターゲット対応
- [ ] VMD モーフアニメーション対応
- [ ] Lottie エフェクト・マスク対応
- [ ] エラーハンドリング・診断表示強化
- [ ] パフォーマンス最適化

---

## 対象ファイル

| フォーマット | 実装箇所 |
|---|---|
| PMD/PMX | `ArtifactCore/src/Geometry/Importer/PMDImporter.cppm` |
| VMD | `ArtifactCore/src/Animation/Importer/VMDImporter.cppm` |
| Lottie | `ArtifactCore/src/Animation/Importer/LottieImporter.cppm` |

既存の `MeshImporter` と `AnimationTrack` インターフェイスを実装するだけで、
自動的にコンテンツビューワー・レイヤーシステム・タイムライン全てと連携される。

---

## 優先順位

1.  **Phase 1 (PMD 頂点表示)** - 最もインパクトが大きい
2.  **Phase 3 (VMD モーション)** - これがあると一気に実用的になる
3.  **Phase 4 (Lottie)** - 最近の需要が急激に伸びている
4.  **Phase 2 (PMD テクスチャ)**
5.  **Phase 5 (品質向上)**

---

## 注意点

- 最初の版は全く正しく表示されなくても良い。とにかくクラッシュしないで開くこと。

## 2026-07-25 実装監査

`FileTypeDetector` の PMD／PMX と基本的な magic／拡張子判定、`MeshImporter` の PMD 最小読み込みは確認した。一方、VMD のバイナリ parser／bone・morph animation、Lottie parser／composition・shape・mask／effect 変換、PMD／PMX の material・texture・bone・morph、drag-and-drop と timeline 連携、Lottie の実装ファイルは確認できない。したがって Phase 0 と PMD Phase 1 の一部基盤のみで、VMD／Lottie を含む本 milestone は未完了・未検証とする。

## Update 2026-08-15

- PMD／PMX の検出と PMD 最小 Mesh 読み込みは実装済み相当である。詳細な PMD の現状は `MILESTONE_PMD_FILE_IMPORT_SUPPORT_2026-04-19.md` に分離して記録する。
- `ArtifactCore::Export::Lottie::LottieExporter` には JSON の `importFromFile()`、`exportToFile()`、構造検証、shape／precomp／image asset／keyframe の serialization がある。したがって「Lottie JSON parser が存在しない」という旧記載は、汎用 Lottie document parser に限れば更新が必要である。
- ただし、Lottie document を Artifact の ShapeLayer／Composition／AnimationTrack へ変換して配置する importer、Lottie の mask／effect の Artifact 変換、VMD のバイナリ parser と bone／morph animation、PMD／PMX の高機能表示、drag-and-drop／timeline 導線は確認できない。
- 判定は **PMD Phase 1 と Lottie document I/O 基盤は部分実装／VMD import と Artifact への Lottie適用は未実装相当** を維持する。ビルド・テスト・runtime 確認は未実施。
- ユーザーは「動かない」より「ちょっとバグってるけど動く」を100倍好む。
- 不完全な状態でも早くリリースしてフィードバックをもらう方が、最終的な品質は遥かに高くなる。
