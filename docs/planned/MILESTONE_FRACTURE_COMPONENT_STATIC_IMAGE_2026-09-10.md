# Fracture Component — Static Image MVP

**最終更新:** 2026-09-10

**ステータス:** In progress — 破砕設定の統一、保存／再読込、seek再現、Undo/Redo、静止画 GPU テクスチャ破片描画まで実装済み。runtime受入確認と実 GPU 表示確認が未完了。

## 目的

静止画レイヤーを指定フレームで一度だけ破砕し、破片を飛散させる映像編集向けMVPを完成させる。破片の実行状態は保存せず、設定とSeedから再生成できることを正とする。

## 現状の根拠

- `ArtifactCore/src/Geometry/Fracture.cppm` に破片生成と衝撃適用がある。
- `ArtifactCore/src/Physics/FractureEngine.cppm` にポリゴン分割とVoronoi破砕がある。
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm` に `fracture.enabled`、`fracture.triggerFrame`、破片数、減衰、重力、衝撃感度、事前生成の設定と描画同期がある。
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm` に衝撃イベントから破砕を発火する経路がある。
- 現状は `builtin.fracture` descriptor と `fracture.*` の実プロパティパスが併存しており、正式なcomponent設定面としての統一が必要。

## 第1段階の受け入れ条件

1. 静止画レイヤーで `triggerFrame` 到達時に破砕が一度だけ発火する。
2. フレームを戻して再生した場合、同じ設定から同じ破片配置と初速を再現する。
3. 非連続seekで破片の実行状態を誤って持ち越さない。
4. 保存／再読込後も設定、破砕プロファイル、Seed相当の再現条件が維持される。
5. Undo/Redoで破砕設定の変更を戻せる。
6. 破片生成は設定変更・初回評価などのコールドパスで行い、通常のフレーム更新で再生成しない。
7. 通常のLayer Propertiesへコンポーネント由来の詳細項目を追加せず、既存のComponents／専用面の責務を維持する。

## 実装順

1. `fracture.*` と `component.fracture.*` の読み書き責務を整理し、旧保存データは読み取れる形で互換維持する。
2. 静止画のtrigger、reset、seek経路を固定し、実行状態と設定状態を分離する。
3. 保存／再読込とUndo/Redoを確認する。
4. GPU描画で破片のUV、opacity、transform、frame境界を確認する。
5. ビルド・テスト・runtime確認を実行し、結果を本書へ追記する。

## 境界と保留

- 初版は静止画のみ。シェイプ、マスク、動画、複雑な衝突は後続段階とする。
- 物理状態はCore／Physics側、破片描画はArtifact／Diligent側、設定UIはComponents専用面が所有する。
- 新しいグローバルsignal/event、QtCSS、Qt合成、QImageホットパス追加は行わない。
- 外部物理エンジンのコード移植は行わず、必要になった場合も先にライセンスと境界を確認する。

## GPU破片描画契約（次の実装単位）

破砕geometryが持つ `localPolygon` と `localUV` を、Coreの物理状態やrender cacheへ混在させず、Artifactの描画契約へ渡す。描画packetはtexture viewを借用し、所有権を持たない。

```text
ArtifactIRenderer::drawTexturedTriangleLocal(
    position[3], uv[3], texture, transform, opacity)
```

- `localPolygon` は既存のtriangulate経路で三角形へ分解する。
- 各三角形はposition 3点とUV 3点を持つ固定容量packetとし、フレームごとの可変長packet確保を避ける。
- textureは既存の静止画GPU upload/cacheが返す `ITextureView*` を借用する。破砕描画側でupload、readback、QImage変換は行わない。
- UVが不正またはtextureが利用できない場合は、既存のsolid polygon fallbackへ落とす。fallbackは診断可能にするが、通常フレームで文字列を構築しない。
- Immediate submitterとbindless submitterは同じpacket意味論を使い、D3D12/Vulkan固有コードをArtifactの上位層へ漏らさない。
- 描画用textureは静止画のsource revisionに紐づけ、source変更時だけgeometry/material参照を無効化する。

### 未解決の受け入れ確認

- `ArtifactImageLayer::draw()` が実際に利用中のGPU texture viewを取得できるか。
- software／fallback経路で同じ破片UV結果を維持できるか。
- 破片数上限と固定容量packetの上限値。
- Diligentの既存sprite PSOを拡張できるか、三角形専用PSOが必要か。
# 実装進捗

- 2026-09-10: 静止画 `ImageF32x4_RGBA` の既存 GPU テクスチャキャッシュを再利用し、破片 polygon/UV を三角形ファンとして描画する経路を Artifact 側へ接続した。
- 2026-09-10: 画像以外のレイヤーは従来のソリッド破片フォールバックを維持する。bindless submitter が未対応 packet を検出した場合は既存の immediate fallback に戻る。
- 未検証: 実 GPU 表示、外部行列経路、ソフトウェア描画との視覚 parity。ビルド・テストは未実行。
