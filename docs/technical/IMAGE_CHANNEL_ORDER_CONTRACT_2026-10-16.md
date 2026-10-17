# Image Channel Order Contract

**最終更新:** 2026-10-17

**ステータス:** BGRAをCPU Float32/Float16画像の正規物理順序として適用中

## 決定

ArtifactStudioが所有するCPU画像バッファは、Float32 / Float16の両方で物理チャンネル順を
**B, G, R, A (BGRA)** に統一する。Aは常にindex 3。`ImageF32x4_RGBA`の`RGBA`は論理色/APIの互換名であり、
`rgba32fData()`が返す物理配列順を示さない。

この決定は、内部画像クラスの保存順を固定し、混在を入口・出口の明示変換へ閉じ込める。コード全体への
移行完了や全経路の画素一致を意味しない。

## 適用範囲

| surface / API | 物理または意味上の順序 | 規則 |
|---|---|---|
| `ImageF32x4_RGBA::rgba32fData()` | BGRA | 固定backing順。descriptor setterはBGRA/Float32へ正規化 |
| `ImageF32x4_RGBA::rgba8Data()` | BGRA | 固定backing順。descriptor setterはBGRA/Float32へ正規化 |
| `ImageF16x4::data()` | BGRA | Float32と同じ。constructor / descriptor setterはBGRA/Float16へ正規化 |
| `FloatRGBA`値 / 論理pixel API | RGBA意味順 | `.r()/.g()/.b()/.a()`でアクセスし、物理indexへ直接写さない |
| Artifact所有のOpenCV 4-channel image処理 | BGRA | OpenCVの4-channel型自体は意味を符号化しないため、Artifact境界でBGRAを明示する |
| 外部producerの`cv::Mat` | producer-defined | 型名・channel数だけから順序を推定しない。明示descriptorまたはnamed adapterを使う |
| Artifact image-pipeline GPU texture (`RGBA16F` / `RGBA32F`) | **BGRA値順** | resource format名はRGBAのまま。textureの`float4`値を`[B,G,R,A]`として扱い、CPU BGRAと直接共有する |
| GPUの非imageデータ / 外部RGBA shader API | 各APIの宣言順 | Artifact image-pipeline BGRA surfaceとの境界でのみ明示変換する |
| `QImage` / encoded asset | format / producer-defined | 取り込み境界でBGRA CPU imageへ一度だけ正規化する |

`SurfacePixelStorage::RGBA16Float` / `RGBA32Float`はcomponentの幅と型を表し、物理順序は表さない。
物理順序は必ず`SurfaceChannelOrder`で表す。

## 実装規則

1. 新規CPU Float32 / Float16 imageの生成時にBGRA descriptorを設定する。
2. `setFromRGBA32F`など名前がRGBAを示す入口は、入力を論理RGBAとして受け、内部BGRAへ変換する。
3. 生のbacking bytesをコピーするときは、BGRAを指定するAPIを使い、descriptorを維持する。
   永続化データはchannel orderをkey/metadataで示し、旧RGBAデータには互換読込経路を残す。共有buffer protocolは混在versionが誤色を返さないよう識別子を更新する。
4. effect / filterはtyped viewまたはdescriptor-aware helperで論理R/G/Bを取得する。
5. OpenCVの`COLOR_*2BGRA` / `COLOR_BGRA2*`などの変換は入口・出口で明示する。
6. Artifact image-pipelineのGPU float textureは値順をBGRAとし、BGRA CPU backingから並べ替えずupload/readbackする。shader内の論理RGBA演算では境界helperでR/Bを明示的に取り出す。
   共通single-pass shader生成器のpixel functionにはlogical RGBA値を渡し、戻り値をBGRA textureへ保存する。
7. 不明または未対応のorderをBGRA/RGBAと推測して処理しない。失敗を返すか、producerにdescriptorを要求する。
8. 変換はalpha index 3を保ち、R/Bだけをswapする。
9. BGRA-native GPU image chain内でBGRA↔RGBAの画像全体変換を繰り返さない。未移行shaderとの一時adapterは明示し、移行完了後に除去する。

## 互換名と明示変換

- `ImageF32x4_RGBA`、`rgba32fData()`、`toCanonicalRGBA32FC4()`など既存名は互換性のため残ることがある。前二者の内部orderは常にBGRA。
- 内部`ImageF32x4_RGBA` / `ImageF16x4`の生ポインタは常にBGRA。`setColorDescriptor()`はprimaries / transfer / alpha metadataを更新するが、storageとchannel orderを固定する。外部のRGBA生bufferはImage surfaceへ入れる前に専用setter / adapterで正規化する。
- `toCanonicalBGRA32FC4()` / `toCanonicalBGRA16FC4()`はCPU正規順へのcopy/変換。
- `toCanonicalRGBA32FC4()` / `toCanonicalRGBA16FC4()`はRGBAを要求する外部API向けの明示変換であり、戻り値をArtifact内部の常用imageとして保持しない。BGRA-native Artifact GPU image chainの標準ではない。
- `RGBA16F` / `RGBA32F`というresource format名は成分の数・幅・型を示す。Artifact image-pipeline内の色値の順序は別契約のBGRAとする。OpenGL等のformat swizzle機能に依存せず、D3D12/Vulkanを含む全backendで同じshader規約を使う。
- API名と互換性を変更する場合は、全callerを移行し、legacy overloadの暗黙解釈を残さない。

## 受け入れ条件

- Float32 / Float16 CPU imageの全生成・copy・conversion経路がBGRA descriptorと実メモリを一致させる。
- 非対称な色（例: R=0.8, G=0.3, B=0.1, A=0.6）をRGBA入口とBGRA backingで往復しても論理色とalphaが一致する。
- CPU effect、直接BGRA GPU upload/readback、preview、composition、save/exportで同じ論理色になる。
- `rgba32fData()` / `ImageF16x4::data()`をraw indexでRGBAとして読む箇所が残らない。残すraw BGRA accessは明示コメントとorder検査を持つ。
- BGRA-native GPU image chainに不要なBGRA↔RGBA全画像変換がなく、外部/画面境界の変換だけが明示される。

## 関連文書

- [Render Format Contract](RENDER_FORMAT_CONTRACT_2026-05-16.md)
- [Image Format & Channel Order Conventions](IMAGE_FORMAT_CONVENTIONS.md)
- [Color channel order consistency audit](../analysis/COLOR_CHANNEL_ORDER_CONSISTENCY_AUDIT_2026-10-16.md)
