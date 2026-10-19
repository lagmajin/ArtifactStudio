# レイヤーマスク独立テスト

**最終更新:** 2026-10-16
**ステータス:** Not Started

## 目的

FBXアニメーション独立テストと同じく、製品アプリ本体とは別に選択・起動できるテストを用意し、レイヤーマスクの演算、レイヤー状態への適用、保存／復元、Composition描画までを切り分けて確認する。GUI確認が必要なケースは独立テスト窓とMCP操作・非アクティブ窓captureを使い、再現手順・診断JSON・画像をテスト出力へ残す。

## 既存資産と責務

- `tests/mask_visual/MaskVisualTest.cpp` は `LayerMask::compositeAlphaMask()` のCPUアルファ合成契約をPNG出力付きで確認する既存テスト。引き続き低レベルの演算回帰に使い、重複した演算ケースをGUIテストへ移さない。
- `tests/ArtifactTestMcpServer.h` と `tests/ArtifactTestWindowCapture.h` はFBX Viewer等の独立GUIテストで共有されている。新規テストもこの共通helperを使う。
- `docs/planned/MILESTONE_COMPOSITION_LAYER_SURFACE_CACHE_TEST_2026-10-16.md` はレイヤーsurface cacheの検査が対象であり、マスク機能全体の独立テストを代替しない。共有可能なfixture手法のみ参照する。
- 実装前に `LayerMask`、`MaskPath`、`ArtifactAbstractLayer` のmask API、layer serialization、既存Composition描画でのmask適用・fallback経路を読み、fixtureが実際に通る経路を特定する。

## 対象範囲

1. **演算契約（CPU）**: add/subtract/intersect/replace、opacity、inversion、feather、expansion、enabled状態、空maskと複数pathの境界値を決定的な数値assertと画像出力で確認する。既存 `ArtifactMaskVisualTest` のケースを棚卸しし、不足分だけを足す。
2. **レイヤー状態・永続化**: maskの追加／更新／削除、index境界、copy/edit isolation、JSONまたはproject save/load後のpath属性・頂点・状態保持を確認する。保存形式を迂回したmockではなく既存serialization経路を使う。
3. **実描画（GUI）**: 小さな静止画またはshape fixtureにmaskを適用し、mask有効／無効、反転、編集前後の出力を同一backend・同一条件でcapture比較する。fixtureが意図したComposition render pathへ進んだことを状態／診断に記録する。
4. **独立操作**: opt-in CMake optionと単独executableを設ける。GUI側はFBX Viewerと同じMCP server/capture helperでfixture準備、case実行、state・診断取得、PNG captureを行う。CPU契約suiteはGUIを起動せず単独選択できるようにする。

## 受け入れシナリオ

- 初期maskの内側・外側・境界付近の期待alphaと、画像の寸法／形式を確認する。
- 合成モードを複数pathで適用し、重なり領域と非重なり領域の期待値を検査する。
- enabled切替／inversion／opacity／feather／expansionの変更が結果へ反映され、無効化やpath削除後に古い結果が残らない。
- レイヤーへ設定したmaskを保存して再読込し、path geometryと全mask設定を比較する。
- GUI fixtureでmask有効／無効および編集前後のcaptureが期待どおり異なり、同条件再captureは一致する。
- 失敗時にcase名、期待値、観測値、layer識別情報、render path、capture pathをJSONとログに残す。
- それぞれのCPU suiteとGUI executableを個別に選択でき、通常アプリ起動・データを変更しない。

## 制約・確認方法

- 変更は親リポジトリの `tests/`、root CMake設定、`docs/` に限定する。`Artifact` 等の子リポジトリは編集しない。
- fixtureと診断はテスト専用とし、新しい製品UI、公開signal/slot、mask編集仕様、render fallbackは導入しない。
- 描画確認は実際の製品Composition経路を使う。CPU合成結果だけでGPU／Composition表示まで合格とみなさない。
- GUIが見られない環境ではCPU契約・永続化suiteの結果とGUI受け入れ未確認を分けて報告する。
- ビルド、CMake configure、テスト起動はユーザーが明示するまで実行しない。

## 既知の未決事項

- 既存 `ArtifactMaskVisualTest` のケース一覧と精度許容値を棚卸しし、追加すべき演算境界を確定する。
- GUI fixtureで簡潔に生成でき、Composition mask pathを通せる最小のlayer種別を選ぶ。
- project round-tripをGUI fixtureへ含めるか、既存serialization contract suiteへ分離するかを既存テスト構成に合わせる。
