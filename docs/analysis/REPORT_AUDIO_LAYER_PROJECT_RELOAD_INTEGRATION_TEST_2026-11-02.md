# Audio Layer Project Reload Integration Test

**最終更新:** 2026-11-02

## 背景

`AudioLayerRenderQueueIntegrationTest.cpp` には実WAV fixtureを使う waveform／volume、複数レイヤーmix、Render Queue WAV出力、video/audio muxの統合テストがある。一方、Audio Layerのプロジェクト保存・再読込後に、source、mute、volume、panを復元して同じComposition PCMを再生成するケースはなかった。Audio Layer受入計画でも、実ファイルの保存再読込とmute／soloのruntime parityを未検証としている。

## 追加した受入ケース

`MultipleAudioLayersKeepMuteMixAndRenderedSamplesAfterProjectReload` は独立した正弦波WAVを2つ生成し、Production `ArtifactProjectManager` で2つのAudio Layerを含むCompositionを作る。片方mute時と両方有効時のComposition PCMを比較してmute/mix経路を確認した後、mute状態でプロジェクトを保存・閉じて再読込する。復元後はComposition PCMを保存前とサンプル単位で比較し、同じCompositionをRender QueueからWAVへ出力してsample rate・frame count・RMSも確認する。

復元後は両layerのsource path、volume、pan、mute、load状態を照合し、muteされたCompositionの全サンプルとRMSが保存前と一致することを確認する。CTestでは `ArtifactAudioLayerProjectReloadIntegrationTest` として単独選択できる。

追加で `CompositionAudioBlocksStayAlignedAcrossForwardAndBackwardSeeks` を設け、3秒のWAVに含めた2つのtone領域と無音領域を、frame 15 → 45 → 69 → 45 → 15 → 69 の非単調な順序でCompositionから取得する。各frameを最初の基準PCMとサンプル単位で比較し、前後シークで別位置の音声が混ざらない契約を確認する。CTest名は `ArtifactAudioLayerSeekAlignmentIntegrationTest`。

さらに `MissingSourceStopsAudioAndRelinkRestoresCompositionOutput` では、有効な440 Hz sourceのプロジェクトを保存した後に実ファイルを削除し、プロジェクトを再読込する。source pathがmissing状態でも保持され、Layer出力が失敗しComposition出力が無音になることを確認してから、880 Hzの別sourceへ再接続し、Compositionが再び音を出して失敗前PCMと異なることを確認する。CTest名は `ArtifactAudioLayerSourceRecoveryIntegrationTest`。

このケースが必要な欠陥として、非空pathの読み込み失敗時に `ArtifactAudioLayer` が `isLoaded=false` だけを設定し、初回project load時に `sourcePath_` が空のまま残って再保存・relink用のpathを失うことを確認した。失敗経路で要求pathを保持し、source asset identity、共有PCM、sample metadata、de-click範囲、resampled cacheをクリアして `changed()` を発行するようにした。Audio Layer実装単位と統合test翻訳単位のcompileは成功した。

## 範囲と未確認事項

これらのCPU統合ケースはデバイスからの実時間出力、scrub後のhardware audio clock、UI indicator、solo挙動を検査しない。これらはPlayback Engine/deviceを使う独立runtime受入として残る。

MSVC build treeではCMakeの `/showIncludes` prefix検出が文字化けした値を生成し、`VSLANG=1033` をconfigure/build presetへ設定してもcl.exeの出力は日本語のままだった。全target buildは既存の `ArtifactAbstractLayerBounds.cpp` / `ArtifactAbstractLayerEffectPersistence.cpp` で、MSVC 19.51 (`14.51.36231`) の `Animation.Transform3D` IFC import中に `std::_Stop_callback_base::_Do_attach` の特殊化で `C1116` が出て停止した。`ArtifactAbstractLayerBounds.cpp.obj` 単独のNinjaビルドでも同じ診断を再現した。clang-cl build treeはOpenMPとglslcを明示指定して構成できたが、Audio test targetの依存グラフが `AnimatableTransform2D.cpp.obj.ddi` → `AnimatableValue.ixx.obj` → `ArtifactCore/CXX.dd` のcycleで停止した。CTestには3ケースが登録済みだが、共有する `ArtifactAudioLayerRenderIntegrationTest.exe` は未生成。

Microsoft C++ Teamは、third-party headerをincludeした後に `import std;` を使って構築したpartitioned C++23 moduleをimportすると発生するC1001/C1116 regressionを、14.52 Preview向け更新で修正したと案内している（[MSVC Build Tools Preview updates — July 2026](https://devblogs.microsoft.com/cppblog/msvc-build-tools-preview-updates-july-2026/)）。ただし、この環境の `ArtifactAppRuntime` は `CXX_MODULE_STD=OFF` で、失敗した翻訳単位のgenerated modmapにも `std` IFC参照がないため、同一原因とは確認できない。現在PATHにあるMSVCは14.51.36231のみで、VS 2022 installationにはC++ compilerがなく、14.52での比較は未実施。

追加テストを含む `AudioLayerRenderQueueIntegrationTest.cpp.obj` と変更した `ArtifactAudioLayer.cpp.obj` はMSVCで個別コンパイル成功した。実行ファイルのリンクまでは到達できず、追加3ケースを含むAudio test suiteの実行結果は未確認である。
