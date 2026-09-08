# プレビューGPU常駐化：最初の実装範囲

**最終更新:** 2026-09-08

## 状態

静的確認のみ。ビルド・CMake・テスト・アプリ起動・性能計測はユーザー指示により未実施。
1〜5の最初の縦断実装であり、全画像エフェクトのGPU常駐化完了ではない。
実際に重かったエフェクト名は未確認。代表として Exposure を選択した。

## 変更

| 段階 | 今回の実装 | 残る範囲 |
| --- | --- | --- |
| 1 計測 | Exposure の準備／submit／同期readbackのCPU経過時間・転送量。単色GPU経路のhit／dispatch／upload量 | 実機の比較、GPU timestamp、他エフェクトの内訳 |
| 2 静止結果再利用 | 不透明な単色Solid2D／SolidImage＋Exposure列の評価済み色・パラメータを結果キーに使用。時刻だけでは更新しない | 他レイヤー／他エフェクトの時間依存契約の一般化 |
| 3 GPU直結 | 単色入力を1×1のRGBA32FとしてGPUへ渡し、Exposure列と最後のsRGB decodeを融合。出力SRVを既存のsprite描画へ接続 | 一般画像、Blur等の近傍処理、マスク、調整レイヤー、混在スタック |
| 4 再利用・同期 | 最大64件のrenderer所有キャッシュ。最終使用順に置換。色が同じなら入力を再uploadせず出力を再利用。GPU経路はreadback／WaitForIdleなし。従来Exposureのstaging再利用とdevice/context変更時の破棄 | 汎用VRAM予算、複数passのscratch pool、他エフェクトの同期読み戻し撤去 |
| 5 RAMプレビュー | 再生方向に沿うループ越しの先読み順。非ループ末尾の準備判定修正。先行8フレームが揃えば範囲全体の完了前にも既存RAM表示を利用 | GPUフレームキャッシュ、非同期readback／生成キュー全体の拡張 |

## 対応条件・意味の保持

- 主Composition controllerのGPUレイヤー描画に接続。
- 単色・不透明・2D平面、enabled effectがExposureだけ、CPU強制でないこと。
- マスク、マット、modifier、mix、effect region、overscan、シーンライト、2D viewport camera適用は従来経路へ戻す。
- 対応判定はスタック全体で行い、未対応effectがあれば一部だけをGPU適用しない。
- `setContext` 後の値をキーに含め、アニメーション・式等で変わったExposureの結果を固定しない。
- 空間的に一定の演算だけを1画素で評価する。解像度の低下による近似ではない。
- 既存controllerのARGB32/CV float境界に合わせ、入力の格納順と最終upload相当のsRGB→linear変換を明示している。色の実機同等性は未確認。
- 出力は既存の変換・opacity・合成に渡す。UI、ギズモ、入力操作は変更しない。
- Diligent共通APIを使用。D3D12/Vulkanの実機検証はともに未実施。

## リソース寿命

キャッシュはcontrollerのImplが所有し、composition/surface全体クリア時とdevice/context変更時に解放する。
置換や再計算の前に既存rendererの描画をflushし、参照中のspriteを送出する。
computeと後続samplingの順序は同一immediate contextと既存のDiligent resource transitionに従う。
CPU完了待ちの単純削除は行っていない。従来ExposureのCPU読み戻し境界には待機が残る。
従来Exposureのshared device acquireは既存SharedRenderDeviceLeaseで各returnに対応するreleaseを保証する。

## 診断

既存 `Diagnostics/EffectProfiling` または `ARTIFACT_EFFECT_PROFILE=1` を使用。

- `[ExposureTransferProfile]`: CPU wall time。GPU単体時間ではない。初期PSO構築時間は内訳の対象外。
- `[SolidPointwisePreview]`: requests/hits/dispatches/uploadBytes。readbackBytesとidleWaitsはこの新経路のみの値。初回と120回ごとに出力。
- 新経路以外の描画、表示、RAMフレーム格納の転送をゼロと主張するものではない。

## 後日の受入れ条件

同一解像度・backend・再生範囲で、固定Exposure、動くExposure、effect無効化、CPU強制、mask/mix、undo、composition切替を比較する。
固定Exposureのウォームアップ後はdispatch/uploadが増えずhitだけが増えること、変更時だけ再計算することを確認する。
順再生・逆再生・ループ境界・短い範囲・未キャッシュフレームでRAM readyと実画像が一致することを確認する。
これらは未実行であり、速度向上・見た目の同等性・ビルド成功はまだ保証しない。
