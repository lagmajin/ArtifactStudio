# Milestone: Track Matte Drag-Link UX

**最終更新:** 2026-09-01

### RenderQueue 直接描画分岐の補正（2026-09-01）

`ArtifactCompositionViewDrawing.cppm` の共通描画で、有効なトラックマット参照があるレイヤーをGPU/直接描画分岐へ流さず、既存の `applySurfaceAndDraw` に通す条件補正を行った。これにより単純 Solid、Image、SVG、Video、Text、Particle の一部でマット適用が抜ける問題を抑制する。QImageソースマップを使うRenderQueue経路自体は残っているため、GPU化完了ではなく、GPU中間面へ移行できない場合のCPU正確性補正として扱う。

Shapeについても、マット付きの場合のみ既存の `toQImage()` 境界を介して同じ適用経路に送る。FormParticleは汎用GPUオフスクリーン経路では対象にできるが、CPUフォールバック側の専用サーフェス化が残件である。

RenderQueueの新しい単一フレームGPU経路にも、Stretch配置・最大3ソース・3D以外という条件付きでGPU中間面処理を導入した。ソースと対象をオフスクリーン面に描画し、`ArtifactIRenderer::applyTrackMatte()` を通して本体面へ戻す。FormParticleを含むGPU描画レイヤーを汎用的に対象化できるが、条件外は既存QImage経路へフォールバックするため、RenderQueue全体のGPU化完了とは扱わない。

MatteTrackパイプラインはフレームごとに再生成せず、GPUレンダラーの寿命に合わせて保持する。解像度変更またはGPUデバイス再初期化時には破棄して再構築する。

GPUマット処理で対象面・出力面は合成後にフラッシュして解放し、後続レイヤーが参照するソース面だけをフレーム終了まで保持する。

RenderQueue単一GPUフレーム経路では、明示的な3Dカメラ行列を渡せないため、3Dソース／対象はGPU MatteTrackの条件から除外し、既存経路へフォールバックする。

GPU中間面の寸法はコンポジション論理サイズではなく、解像度プリセット／Crop後の実GPUレンダーターゲット寸法に合わせる。MatteTrackの全入力寸法契約を満たすためである。

呼び出し関係を確認した結果、実運用の連番GPUフレーム処理は `Impl::renderSingleFrame()` であり、`renderSingleFrameGPU()` は現在未参照の旧実装である。今回のGPUマット導入は実運用経路を対象としている。

作成日: 2026-06-01
親: `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md` (Track Matte / Mask / Blend)
関連: `MILESTONE_PROPERTY_REFERENCE_LINKING_2026-05-11.md` (Property Row Pick Whip Surface)

---

## 目的

レイヤーパネル上で「レイヤーをドラッグしてトラックマットに設定する」という
After Effects 標準操作を実現する。MatteType 列挙と LayerMatteReference の
データモデルは既に存在するため、これを Inspector / Layer Panel の UI 操作へ
接続する段階に入る。

## 仕様の骨子

1. `drag source`
   - レイヤー行をドラッグ元にする
   - reorder 用 DnD と matte link 用 DnD を分ける
   - modifier で matte link を明示できるようにする
2. `drop target`
   - matte を受ける行または matte 専用スロットをドロップ先にする
   - ハイライトは「並び替え候補」ではなく「参照先候補」として出す
   - self-reference と循環参照は拒否する
3. `result`
   - drop の結果は layer reorder ではなく `TrackMatteReference` の作成・更新にする
   - `Alpha / Luma / Inverted` の mode を維持する
   - undo / redo で元に戻せるようにする

---

## 既存の土台

- `Artifact/include/Mask/LayerMask.ixx` — mask path 管理
- `Artifact/include/Layer/ArtifactLayerMatte.ixx` — `MatteType` (Alpha/Luma/Inverse), `MatteBlendMode`, `TrackMatteReference`
- `Artifact/src/Mask/LayerMask.cppm` — `compositeAlphaMask` でルミナンス・アルファ合成済み
- `Artifact/src/Undo/UndoManager.cppm` — `MaskEditCommand`

---

## 実装状況メモ (2026-07-07)

- `ArtifactLayerPanelWidget` で matte link 用の drag mode が実装済み
- `Alt + Drag` で Track Matte を設定でき、self-reference / cycle は拒否される
- `ChangeLayerMatteReferencesCommand` を通して Undo/Redo が積まれる
- Inspector 側にも matte 参照の切り替え導線と badge 表示があり、文書の Phase 2/3 はかなり進んでいる
- いま残っているのは、移動時の dangling reference の扱いと、見た目の微調整・説明整理
- 削除時の dangling matte reference は project 層で掃除するようになった

---

## 未着手要素

- レイヤーパネルからのドラッグ＆ドロップでトラックマット受け側レイヤーを指定する UI
- ドラッグ中のターゲットハイライトとドロップ許可条件の制御
- ターゲットレイヤーが削除・移動されたときの dangling reference 処理

## 進捗メモ

- レイヤーパネルの `Alt + Drag` で matte link を設定できる
- matte の self-reference / cycle は UI と表示の両方で拒否・警告する
- Inspector の Matte 行はクリックで最初の source へ focus できる
- Inspector の Matte 行は右クリックで `MatteType` を切り替えられる
- Layer Panel の matte badge は source 名と type を表示する

## 現行コード照合（Update 2026-08-15）

`ArtifactLayerPanelWidget` には Alt-drag の matte link mode、matte slot／layer drop、target tooltip／highlight、self-reference／cycle 拒否、source 置換、`ChangeLayerMatteReferencesCommand` による Undo／Redo が存在する。Inspector の参照表示と MatteType 切替も確認できるため、上記の「未着手要素」のうち基本 UI は実装済みとして扱う。

残る確認課題は、削除・移動後の dangling reference の全経路、複数 matte slot の runtime 表示、実機での drop affordance と Undo／Redo 受入である。

## Update 2026-08-15

- `ArtifactLayerPanelWidget`／`ArtifactLayerPanelPresentation` と `ChangeLayerMatteReferencesCommand` の現行経路を確認し、Alt+Drag による matte link、self-reference／cycle 拒否、Undo/Redo、matte badge 表示は実装済みとして扱う。
- `ArtifactMatteReferenceRule` と Render Queue 側の診断には missing source、self-reference、hidden source、cycle の検出があり、削除時の参照掃除も `ArtifactAbstractComposition`／Undo 経路に存在する。
- したがって Phase 1〜3 の主要基盤は実装済み。ただし実機でのドラッグ操作、drop target の視覚的ハイライト、dangling reference の移動／再配置ケース、合成結果の runtime 受入は未検証とする。

## GPU / Composition Path 照合 2026-09-01

- `ArtifactCore::MatteTrack` の compute shader と `LayerBlendPipeline::applyTrackMatte()` により、Alpha／Luma／反転、opacity、最大3ソースのGPUマット適用は実装されている。
- ただし `ArtifactCompositionRenderController` のGPU経路は、マット元を `toQImage()`／`currentFrameToQImage()` で解決し、`Fit / Fill / Stretch` 後にGPUへアップロードしている。GPUで行われているのは対象レイヤーへのマット係数適用であり、マット元レイヤー自体のGPUコンポジション結果ではない。
- レンダーキューの `renderLayerSurface()` も元サーフェス取得が中心で、マット元の global transform、mask、rasterizer effect、precomp、3D camera/depth を同一コンポジション空間へ焼き込む契約にはなっていない。
- そのため、同サイズ・単純2D素材の静止画マットは実用候補だが、変換付きレイヤー、ネスト、3D、動的エフェクト、高解像度連番でGPU／CPU結果の一致は未検証であり、機能十分とは判定しない。
- 次の実装境界は `MatteTrack` shader の拡張ではなく、マット元を対象と同じフレーム・座標・色／アルファ契約のGPU surfaceとして解決する共有レンダー経路の追加である。欠落時に未マスクで継続するfallbackも、受入前に明示エラーまたは安全な透明出力へ方針確定する。

### GPU surface 実装分割案

1. `MatteSourceSurfaceCache` をコンポジションフレーム単位で導入し、source layer ID、frame、composition size、surface generation、color／alpha契約をキーにする。
2. 単純2Dソースは既存のGPU texture cacheを再利用し、Transform込みのコンポジション空間へ描画したRTV／SRVを保持する。元画像の `QImage` を直接マット入力にしない。
3. Mask／rasterizer effect／precomp／3Dを含むソースは、既存の per-layer GPU draw と同一の surface stage を通し、対象レイヤーのマット評価からは再帰を除外する。ソース自身の matte を許可する場合は DAG 検査済みの順序で解決する。
4. `MatteTrack` は source SRV の配列と対象 layer float SRVだけを受け取り、FitをCPU画像リサイズではなく正規化UV／サンプラー契約で扱う。複数マット数の上限は固定配列から段階的に拡張する。
5. source missing／GPU surface生成失敗は「未マスクで継続」と「透明化」を暗黙に切り替えず、diagnostic stateと出力ポリシーを呼び出し側で明示する。

#### 実装進捗 2026-09-01

- GPU経路で、`fitMode == Stretch` かつ adjustment／group child／final-render除外ではない参照sourceを既存の `drawGpuLayerToIntermediate()` で親コンポジション空間のオフスクリーンRTへ描き、そのSRVを直接マット入力へ渡す接続を追加した。sourceのTransform、通常のMask／Effect処理、Precomp描画を同一のGPU layer draw境界へ寄せる。
- sourceが非表示／非アクティブ、RT確保失敗、GPU描画失敗時は従来のQImage fallbackを維持する。Transform付きsourceを含めた一般経路は実装段階に入ったが、座標・色・アルファのruntime parityは未検証である。
- なお、`ArtifactRenderQueueService` のGPUレンダーは現時点でも `QHash<Id, QImage>` のmatte source収集を使っている。ビューア経路と同じGPU source surface契約を共有するまで、レンダーキューを含む全経路のGPU化完了とは扱わない。

#### 既存APIを使った候補呼び出し順

- フレーム開始時に、参照されるマット元IDだけをDAG順で収集する。
- 各source IDについて、サイズ一致のcolor RTV／SRVとdepth DSVを一時管理し、rendererのcanvas、zoom、pan、override target、depth状態を保存する。
- 保存した状態から composition-space viewport を設定し、`drawGpuLayerToIntermediate()` を呼び出して source layer の Transform込み結果をcolor RTVへ描く。source自身のmatte参照は解決済み依存だけを許可し、未解決の再帰は診断して停止する。
- flush後にSRVを `MatteSourceSurfaceCache` へ登録し、target layerの `prepareGpuLayerForBlend()` にはQImageではなくこのSRVを優先して渡す。
- 呼び出し後はRTV／DSV、viewport、canvas、zoom、pan、active camera、exposed-property scopeを必ず保存値へ戻す。sourceごとのRT aliasは禁止する。
- フレーム終了時に一時surfaceを破棄し、persistent cacheはsource revision、frame、composition size、surface generationが一致する場合だけ再利用する。

この手順は既存の `renderPrecomp2DGpuOutput()` を参考にできるが、一般レイヤー版では `drawGpuLayerToIntermediate()` の再入とsource matteの依存順を追加で制御する必要がある。

#### 先に受入する最小ケース

- 変換付き画像マット：sourceの位置・回転・スケールがコンポジション空間で一致する。
- マスク付き／エフェクト付きマット：sourceの最終アルファとLumaが一致する。
- 動的フレーム：sourceとtargetが同じcomposition frameを参照する。
- precomp／3Dマット：深度やネストの結果を含め、CPU経路と同じシルエットになる。
- 欠落・循環・GPU失敗：未マスク継続にならず、表示とdiagnosticが一致する。

---

## フェーズ

### Phase 1: Inspector Matte セクション強化

- Inspector に Matte グループを表示（現在は存在するが、ターゲットレイヤー選択導線がない）
- レイヤー参照をクリックしたときに Project / レイヤーパネルへ focus jump
- MatteType トグル（Alpha / Luma / Inverted Luma）の即時切替
- Undo via `AddLayerCommand` / `RemoveLayerCommand` を流用

### Phase 2: レイヤーパネル DnD

- レイヤー行を Alt + ドラッグで `matte link` モードへ移行できるようにする
- ドラッグ中、受容先レイヤー行の左端に参照先ハイライトを出す
- ドロップで `TrackMatteReference` を設定し、Undo を積む
- 条件: マット化できるのは「単一レイヤーまたはグルー 1 つ」のみ
  - precomp / footage / null / 3D model をターゲット可能
  - 自身をマットに設定する循環参照を拒否
  - 既存の reorder DnD と見た目を混同しない

### Phase 3: Matte 視覚的確認

- レイヤーパネル行の左端アイコンでマット関係を表現（インジケータ + ホバーでタイプ表示）
- Inspector に「Matte Source: <Layer Name> (Alpha)」のサマリ表示
- マットが魔のモードの場合、合成結果のプレビューから破綻検知フラグを更新

---

## 検証条件

- レイヤー A をレイヤー B のマットとして DnD 設定 → Composition Viewer で
  アルファ合成が正しく行われる
- MatteType 切替で Luma / Inverted Luma のビットが切り替わる
- Undo / Redo でマット設定が巻き戻る
- マット対象レイヤーを削除 → MatteReference がクリアされ fallback 描画になる
- 循環参照ドロップ → 拒否通知が表示される

---

## 関連ファイル（対象）

- `Artifact/include/Layer/ArtifactLayerMatte.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyWidget.cppm`
- `Artifact/src/Undo/UndoManager.cppm`

---

## 見積

- Phase 1: 8–12h
- Phase 2: 12–18h
- Phase 3: 6–10h

合計: 26–40h
