**最終更新:** 2026-11-19

# 2D Particle Shape Target Evaluation

この実験は既存の2D Particle Layerに追加したShape/Text吸引オプションを、決定論的な合成fixtureで評価する。実行計画と受入条件の詳細は[planned milestone](../../docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md)、fixtureとcaptureコマンドは[Particle2DPlayground README](../../tools/Particle2DPlayground/README.md)を参照。

## Fixture

- 320×180、30 fps、seed `20261118`、360 particles、NVIDIA GeForce RTX 4070 Ti。
- Shapeは外側・内側ellipse path、Merge Paths/Subtract、Fill/Strokeのordered stack。TextはArial Regular 400、54px、`A8`、stroke 1.5px。
- Shape/Text/empty Shape/particle LayerIDを固定し、別プロセス・backend間でtarget sampling seedを揃える。
- 外部の画像素材なし。Arialは実行環境のフォントを使い、Qt・resolved font情報はcapture reportに記録する。
- VulkanとD3D12でframe 30/60/90/180/240、frame 60 direct seek、disabled default parityをcaptureする。

## 再実行

ArtifactStudio rootからPowerShellで実行する。Buildは既存Ninja/Debug構成を指定する。

```powershell
cmake --build out/build/x64-Debug-Agent --target ArtifactParticle2DPlayground --parallel 4
./experiments/Particle2DShapeTarget/run_evaluation.ps1 -Executable out/build/x64-Debug-Agent/bin/Debug/ArtifactParticle2DPlayground.exe
python ./experiments/Particle2DShapeTarget/compare_backends.py
```

PNGとレポートは再生成可能なscratchとして `output/particle-shape-target/` に保存され、ルート `.gitignore` で除外される。`compare_backends.py` は2 backendのRGBA/alpha差分、direct-seek画像、粒子位置、性能値とVulkan画像統計をJSONへ書き出す。

## 評価上の注意

最新の固定ID fixtureではShape/Textの連続画像とframe 60 direct seekはbackend間で一致し、frame 90/240のparticle centerも一致した。baseline frame 60/direct seekには最大1 channelの少数画素差がある。性能はDebugの単一process内5 batchesで測定した探索値であり、run間confidence・threshold・Release比較・GPU memory・cache容量の根拠にはしない。製品Composition View / Render Queueの確認が残るため、このR&Dは`In Progress`。
