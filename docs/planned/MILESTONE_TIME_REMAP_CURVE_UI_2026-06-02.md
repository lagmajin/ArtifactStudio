# M-MOTION-7 Time Remap Curve UI (2026-06-02)

日付：2026-06-02
**最終更新:** 2026-08-15
目標：タイムリマップ（可変速再生）のカーブ編集 UI を実装。特定部分をスローにしたり早送りしたりする速度ランプを直感的に編集可能にする。

---

## Goal

- レイヤーの「タイムリマップ」プロパティを有効にしたとき、Curve Editor で速度カーブを視覚編集可能に
- `value` ＝ ソース時間（フレーム）、`velocity` ＝ 再生速度として表示
- キーフレーム間の速度変化をベジェカーブで制御

---

## Definition of Done

- [ ] **タイムリマップ有効化** - レイヤー右クリック → 「タイムリマップを有効化」
- [ ] **Curve Editor 統合** - タイムリマップ有効時、Curve Editor に TimeRemap トラックが追加される
- [ ] **Value グラフ** - x=タイムライン時間, y=ソース時間 のマッピングカーブ
- [ ] **Speed グラフ** - フレームごとの再生速度（100% = 等速）を棒グラフ/折れ線で表示
- [ ] **速度ランプ** - キーフレーム間のイージングを変更すると速度グラフが自動更新
- [ ] **ループ・ピンポン** - TimeRemap と loopOut Expression の併用動作確認

---

## Implementation Phases

### Phase 1: TimeRemap モデル

**完了条件**:
- [ ] `TimeRemap` プロパティのキーフレームモデル確認（`AbstractProperty` で既にキーフレーム対応済みのため、ほぼ既存）
- [ ] タイムリマップ有効時にレイヤーのソース時間解決が切り替わることを確認
- [ ] キーフレーム追加時の自動速度計算（隣接キーフレーム間の傾き）

### Phase 2: Curve Editor 統合

**完了条件**:
- [ ] TimeRemap トラックを Curve Editor に追加
- [ ] Value グラフ + Speed グラフ の表示切替
- [ ] 速度一定/イージング適用時の視覚フィードバック

### Phase 3: UI トリガー

**完了条件**:
- [ ] レイヤーコンテキストメニューに「タイムリマップを有効化」
- [ ] Inspector に TimeRemap プロパティ表示
- [ ] Curve Editor タブが自動フォーカス

---

## Dependencies

- ArtifactCurveEditorWidget (既存)
- TimeRemap (Core.TimeRemap または AbstractProperty)
- ArtifactAbstractLayer

---

## Total Estimate

| Phase | 時間 |
|---|---|
| Phase 1: TimeRemap モデル確認 | 2-3h |
| Phase 2: Curve Editor 統合 | 4-6h |
| Phase 3: UI トリガー | 2-3h |
| **合計** | **8-12h** |

---

## 2026-07-25 現状確認

Phase 1 の Core モデル相当は存在する。`ArtifactCore/src/Time/TimeRemap.cppm` に `TimeRemapProcessor` があり、時間→ソース時間の補間、逆マッピング、Bezier/Ease/Constant 系の補間、`getSpeedAtTime()`、constant speed／ramp／hold／reverse の生成補助を提供している。

一方、現行 `Artifact/src` の Timeline／Curve Editor 統合では TimeRemap 専用トラック、Value／Speed グラフへの TimeRemap 追加、レイヤーコンテキストメニューの有効化、Inspector 表示を確認できない。したがって本 UI マイルストーンは「Core の TimeRemap 基盤のみ部分実装、UI 統合 Phase 2〜3 は未着手」と整理する。

未確認事項:

- AbstractProperty の実際の TimeRemap source-of-truth との接続
- 隣接キーからの速度自動計算と Curve Editor の表示同期
- loopOut / ping-pong との併用
- 逆再生、freeze、速度ランプの runtime 検証

## Update 2026-08-15

現行コードを再確認したところ、2026-07-25 時点から実装が進んでいる。

- `ArtifactCore::TimeRemapProcessor` / `TimeRemapEffect` は、キーフレーム補間、Bezier/Ease/Constant/Hold、逆マッピング、`getSpeedAtTime()`、フレームブレンドを実装済み。
- `ArtifactAbstractLayer` に有効化、キーフレーム追加、合成フレームからソースフレームへの解決があり、`ArtifactVideoLayer` と合成レンダー側から実際に参照されている。Animation メニューからの有効化、フリーズ、時間反転の導線も存在する。
- `ArtifactCurveEditorWidget` に `sampleSpeedGraph()` と `setSpeedGraph()` が追加され、隣接キーフレームの傾きから `Speed (%)` 用のサンプルとタンジェントを生成できる。ただし、現行コード上で選択レイヤーの TimeRemap キーを Curve Editor に渡す呼び出しは確認できず、実際のトラック統合は未完了。
- Value（合成時間→ソース時間）グラフ、TimeRemap 専用キートラック、速度グラフの編集結果をレイヤーへ書き戻す経路、Inspector 表示、Undo／保存・再読込の専用経路は未確認。
- `loopOut` Expression との併用、逆再生・フリーズ・速度ランプの再生／書き出し品質は、コード静的確認だけでは完了扱いにできない。

現状は「Core とレイヤー runtime は実装済み、Animation メニューと Speed グラフ生成の部品も実装済み、Curve Editor 接続と編集・永続化・検証が未完了」。Phase 1 は部分完了から実装済み寄り、Phase 2〜3 は部分実装のまま。
