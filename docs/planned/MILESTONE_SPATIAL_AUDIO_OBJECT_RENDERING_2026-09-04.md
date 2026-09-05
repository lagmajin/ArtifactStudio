# Spatial Audio Object Rendering Milestone

**最終更新:** 2026-09-05
**ステータス:** 最小再生、speaker VBAP、解析的headphone、距離音色、明示LFE send、Property編集はコード接続済み・実行未検証。測定HRIR、room、routing／exportは未実装。
**マイルストーン ID:** M-AU-9

## 目的と採用方針

音源を3Dオブジェクトとして配置し、スピーカーでもヘッドホンでも方向・距離・広がりを自然に表現できる独自の空間音響機能を作る。Dolby Atmos / Auro-3D は制作体験の参考とし、公式コーデック、認証、ビットストリーム互換を目標にしない。

**ユーザー方針（2026-09-05）:** ライセンス料を支払わない。独自方式でよいが、品質をできる限り高める。ロイヤリティ、サブスクリプション、有償SDKを必須とする構成は採用しない。有料アダプターを将来必須の工程として残さない。

「無料ダウンロード」と「商用利用・改変・再配布可能」は別扱いとする。外部ライブラリとHRTFデータは個別にライセンス、帰属表示、同梱条件を確認し、採用時にバージョン・出典・ハッシュ・NOTICEを記録する。公開論文だけを根拠に権利条件を断定しない。本設計時点では新規SDK／データセットの採用を確定しない。

品質の優先順は、時刻・座標の正確さ → スピーカー定位 → ヘッドホン定位 → 空間の質感。性能不足時も音声欠落や急な定位変化を起こさず、品質モードを明示して制御する。

## 現状の整理

- 既存 `Audio.*`、Audio Layer、Playback、Mixer、Render Queue を拡張し、別の音声エンジンを併設しない。
- 音源ID、spread、gain、mute、enabled とJSONの基礎は存在する。独立した AudioObject 管理、solo、専用routingまで完成した意味ではない。
- Coreに7.1.4の12ch表現があり、出力設定／Render Queueにも選択経路がある。これは3Dオブジェクトの12ch空間配分の完成を意味しない。
- Headphoneは外部HRIRを使わない解析的なITD／head-shadow処理まで接続済み。測定HRIRによる前後・上下定位の完成版ではない。
- 本文の「接続済み」はコード上の状態。ビルド・実機再生・聴感品質の合格とは区別する。

## 2026-09-05: 最小再生経路の接続

- `ArtifactSpatialAudioLayer` を `ArtifactAudioLayer` 派生へ変更し、既存 WAV デコード、素材管理、clip 時刻、シーク、resampling を再利用。初期対象は mono/stereo WAV。
- Layer の作成メニューへ「3Dオーディオ...」を追加。既存メニューの action dispatch と ProjectService の作成経路を利用し、新規 signal/slot 接続は追加していない。
- Factory に `SpatialAudio` を登録し、数値 type=29 の JSON 復元を接続。既存 audio.* の素材情報と spatial.* を保存する。
- ソース manifest に既存 SpatialAudio の interface/implementation を登録。存在しなかった `Time.Position` import を `Frame.Position` に修正。
- ミュートは Audio Layer の状態へ統一。距離、spread、gain、cone、air absorption の float プロパティを要求された音声フレームで評価する。
- この時点の接続内容には後続のspeaker／headphone実装前の説明が含まれる。現在はStereo、5.1、7.1、7.1.4のspeaker出力と解析的headphone出力をコード接続済みである。専用routing UI、測定HRIR、room、exportは未対応。
- **静的確認のみ:** ビルド、CMake の実行、テスト、実機再生は実行していない。次の実機確認は mono/stereo WAV 作成、移動・距離変更、mute、seek、保存再読込、Undo/Redo、再生停止・再開。失敗パスと素材再リンクも確認する。

## 2026-09-05: 時刻同期とSpeaker VBAPの実装

- `ArtifactAbstractLayer` に明示時刻の3D親子transform評価を追加し、Spatial Audioは要求された音声ブロックの `FramePosition` とcompositionの正確なframerateから `RationalTime` を作る。これによりUIの現在フレームではなく、再生／書き出しが要求した時刻の音源・アクティブカメラ姿勢を使う。
- coneはlistener姿勢ではなく音源のglobal orientationを使う。音源の放射特性をlistenerの回転で変えない。
- `Audio.Spatial.SpeakerLayout` を追加。Stereo、5.1、7.1、7.1.4のPCM channel順とspeaker方向を定義し、LFEをObject定位から除外した3D VBAP triplet／水平pair／最寄speaker fallbackを実装した。gainはエネルギー正規化する。
- `SpatialParams.outputLayout` と保存プロパティを追加。Spatial Audioは指定layoutでPCMを生成し、compositionのMixer／legacy sumは12ch `Surround714` を保持する。spreadはLFEを除くspeakerへ正規化して広げる。
- Stereo素材はspeakerモードで左右を別の仮想方向としてVBAPへ送る。`Stereo Width` は0〜120度で保存・キーフレーム編集でき、0度では一点音源へ戻る。Headphoneの測定HRIR／Stereo展開は未実装である。
- `ArtifactAudioLayer` はPCM生成時にclip volumeを適用済みだったため、Mixer経路が同じvolumeを再適用していた。bus側の二重乗算を除去し、Spatial Audioにも同じ一回適用の契約を使う。
- **境界:** 5.1以上のStereo素材は固定幅の左右仮想音源として扱う。距離に応じたStereo幅、HeadphoneのStereo展開、測定HRIR、room、doppler、完全なbass managementは未実装。
- **静的確認のみ:** module declaration／source manifest／利用箇所／差分を確認した。ビルド、CMake、テスト、実機再生、speaker impulse、長時間負荷、export比較は未実行。

## 2026-09-05: Headphone基礎経路の実装

- `SpatialRenderMode` にSpeaker／Headphoneを追加し、Spatial Audioの保存プロパティとして扱う。Headphoneではspeaker layoutの指定にかかわらずStereo PCMを出す。
- Propertyはspeaker layout／monitorの列挙選択と、距離、spread、cone、LFE、gainのDSP上有効な範囲を提示する。入力値の丸めに依存せず、編集時点で意味のない値を避ける。
- 外部HRIRを同梱せず、固定長・allocation-freeの解析的バイノーラル処理を `SpatialRenderer` に追加した。音源方向から耳間時間差、遠耳のhead shadow low-pass、後方の帯域／レベル補正を計算し、block間でdelayを補間する。seek、mute、source切替、sample-rate変更時はrenderer stateをresetする。
- これはSOFA／測定HRIRの代替品質を主張するものではない。ライセンス確認済みHRIR presetが未決定のため、Headphoneの現段階は独自の基礎定位モードである。測定HRIRのimport・direction補間・filter crossfadeは次段階に残す。
- **静的確認のみ:** historyは128 sample固定で、96kHzまでの物理的ITD上限を収める。delay、filter state、NaN/Infの境界はコード確認済み。実機ヘッドホンの前後・上下・click・CPU測定は未実行。

## 2026-09-05: 距離による音色変化

- `airAbsorption` は従来の全帯域gain減衰に加え、距離と値に応じたone-pole high-frequency roll-offを使う。値が0または距離が0のときはfilterを完全bypassし、既存素材の音色を変えない。
- filter stateはspeaker／headphoneの各経路で再利用し、renderer resetで消去する。実行時のallocationやlockは追加しない。
- **未検証:** cutoff曲線、長距離の聴感、filter切替時のclick、sample-rate別の周波数応答は実機測定が必要。

## 2026-09-05: 明示LFE send

- `lfeSend` と `lfeCutoffHz` をSpatial Audioの保存プロパティに追加した。5.1、7.1、7.1.4のspeakerモードだけで有効になり、LFE channel 3へone-pole low-pass経由で送る。両方とも要求音声ブロック時刻でキーフレーム値を評価する。
- VBAPのObject gainはLFEを常に除外する。LFE sendは主定位のエネルギー正規化と別の明示経路であり、bass managementや全Objectの自動低域転送ではない。
- `AudioDownMixer` の7.1.4→Stereo fallbackは、7.1 bedを既存係数で折り畳み、height 4chを左右へ-9 dBで加える。speaker PCMをStereoデバイスへ渡すための固定matrixであり、HeadphoneのObject定位を置き換えない。
- Compositionの最終look-ahead limiterは任意PCM channel数を処理する。layout変更時は旧channel数のdelay履歴を破棄して、Stereo／5.1／7.1／7.1.4の間で旧PCMを混入させない。
- **未検証:** LFE channel順、cutoff、headroom、複数Objectの低域合算、device downmixはspeaker impulseと実機環境で確認が必要。

## ドナー参照記録

- **ドナー:** REAPER／Ardourのchannel busとroute契約、Cubase/Nuendoのmonitorとdownmixの責務分離。
- **ArtifactStudioの根拠:** `ArtifactAbstractComposition::getAudio()` がlayer PCMをbusへ送り、`AudioBus` と `AudioDownMixer` がlayoutを保持する。今回の `Audio.Spatial.SpeakerLayout` はObject→layoutの一つの境界に限定し、monitor／exportを別ミックスへ分岐させていない。
- **判定:** speaker layoutのPCM表現は部分実装、VBAP Object配分はコード実装・未検証、headphone／HRTF、device downmix、LFE management、monitor separationは不足。
- **永続化・Undo・Export:** outputLayoutはSpatial layer JSONに保存する。既存Property／Undo経路を使うが、実行確認は未完。exportはcompositionの12ch保持までで、FFmpegの実ファイル配列・downmixは未確認。
- **次の最小検証:** mono impulseを各layout・全方位で出し、channel順、LFE無入力、gain energy、seek後の旧音声不在、preview/export一致を確認する。これはユーザー指定によりビルドを省略しているため未実行。

## 設計契約

### 1. 座標・時間・音源

- 音響内部は右手系、+X=右、+Y=上、-Z=前、距離はm。UI／シーン座標からの変換を一箇所に置き、既存シーンのY方向・スケールとの対応を実装前に確認する。
- composition に `metersPerSceneUnit` を保存する。旧プロジェクトは現状の減衰を保つ移行値を使用し、新規プロジェクトの既定値は標準カメラ距離で試聴して決定する。既存プロジェクトの距離感を暗黙に変更しない。
- listener は「アクティブカメラ」「固定リスナー」を選択可能とする。カメラなしは保存された固定姿勢を使う。カメラ切替は短い補間を行い、明示的カットでは履歴をリセットする。
- 音源位置、親子transform、listener、指向性、gain、spreadを**要求音声サンプル時刻**で評価する。UIの現在フレームを参照しない。23.976／29.97等も有理数時刻で扱い、整数fpsへの丸めをしない。
- ブロック先頭／末尾のパラメータからサンプル間補間する。高速移動は制御区間を分割し、カメラ姿勢は正規化した回転補間を使う。評価のためにUIレイヤーの現在時刻を書き換えない。
- AudioObject は安定ID、source asset/layer参照、position、orientation、spread、gain、mute、solo、enabled、距離モデル、直接音／残響sendを持つ。レイヤー複製時は新しいobject ID、Undo／再読込時は元のIDを維持する。
- monoは点音源。stereoは中心＋左右の仮想音源と幅／向きを持つ方式を用意し、既存stereoパン挙動は互換モードとして保存する。勝手にmono化しない。多ch素材はBedとして扱い、Objectとの変換は明示操作とする。

### 2. 処理と所有の分離

```text
編集・素材読み込み
  → サンプル時刻に対応する不変シーンスナップショット
  → 既存デコード／resample／clip処理
  → Objectの距離・指向性・周波数減衰
  → 出力別レンダラー（Speaker または Headphone）
  → 既存Bus（Bed + Object + 残響return）
  → Master／出力デバイス または 書き出し
```

- Artifact側がUI、asset参照、親子transform／keyframe評価を担当。Core側の空間DSPには評価済み数値とPCMを渡し、Qtオブジェクトやカメラ探索を持ち込まない。
- composition単位で空間sceneを管理し、objectごとにフィルター履歴を持つ。IDの再利用で古い音源の履歴を引き継がない。
- 制御側が確保済みのスナップショットを発行し、音声処理側がブロック境界で受け取る。使用中バッファの上書きを防ぐ所有権／世代管理を設計し、単純な二重バッファだけで安全としない。
- 音声callback内でファイル読込、heap確保・解放、mutex待ち、レイヤー走査をしない。デコード・IR準備・layout係数生成はworkerで行う。現行経路がこの条件を満たすとは未確認。
- seek／stop／source交換にはgenerationを付け、古いworker結果を破棄。seekは畳み込み／delay履歴をリセットし、必要なpre-rollを行う。通常の音源終了はtailを排出、stopは明示fade後に停止する。
- 既存のvolumeと新しいobject gainの適用箇所を表にして、LayerとBusで二重乗算しない。soloは既存Mixerとの共通判定にする。
- PreviewとExportは同じDSP・座標・時刻契約を使用する。Exportは実時間予算に拘束されず、指定品質で処理し、IR latencyと末尾tailの出力範囲を明示する。

### 3. スピーカーレンダラー

- stereo、5.1、7.1、7.1.4を第一対象とし、layout descriptorにspeaker ID、方位角、仰角、役割、出力channel indexを保持する。角度とPCM順序を分離し、配列位置から意味を推定しない。
- 基本方式はVBAPを設計候補として採用する。水平面はspeaker pair、3Dは有効なspeaker tripletを事前生成し、非負gainを求めてエネルギー正規化する。退化した組、同一方向、未被覆方向は検出する。
- 上半球のみのlayoutで下方音源を無理なtripletへ送らない。最寄りの有効面／pairへ投影するfallbackを定義し、移動境界を平滑化する。
- spreadは複数の仮想方向へエネルギーを分配して表現する。方向数と分布を固定・再現可能にし、単純な全ch同量加算で音量を増やさない。
- LFEは通常の定位計算から除外。明示sendとlow-passを持ち、低音を無条件にLFEへ抜かない。クロスオーバー／bass managementはデバイス出力設定とObject処理を分ける。
- ch数に応じた係数容量を確保し、現行8要素の固定配列へ12chを渡さない。各チャンネル単独のimpulseで順序を確認する。
- 非対応デバイスは明示したmatrixでstereoへdownmixする。12chを単に切り捨てない。layout変更は停止中または準備済み経路のcrossfadeで行う。

### 4. ヘッドホンレンダラー

- HeadphoneモードはHRIR畳み込みによるbinaural出力とし、スピーカー向けstereoパンと区別する。HRTF未準備時はstereo fallbackを表示する。
- SOFAを外部データの入力候補とする。座標、単位、左右耳、sample rate、delay、対象conventionを検証して内部形式へ変換する。SOFA形式であること自体はデータの再配布許諾を意味しない。
- 内蔵データは無償で商用再配布可能なものを一つ以上選定する。候補を聴感比較し、個人差に備えてHRTF presetを切替可能にする。ユーザー指定データは外部参照とし、無断で交換パッケージへ同梱しない。
- HRIR長を128tap固定にしない。候補データの必要帯域・delayとCPU予算から決定する。短い直接畳み込みと分割FFT畳み込みを比較し、低遅延と長いIRの両立を評価する。
- 移動時は方向補間とフィルターcrossfadeを使う。ITDを壊す単純な波形補間を避け、delay整合後の補間を候補として検証する。高速移動時のclick、音色変化、前後反転を確認する。
- HRTFは直接音をObject単位で処理。多数の残響経路は共有の空間returnへ集約し、計算量を制御する。Ambisonicsは後段の候補とし、初期実装の必須条件にしない。
- head trackingは拡張枠に留める。初期版はlistener/camera回転で完結する。

### 5. 距離と空間の質感

- 距離減衰は近距離で発散せず、最遠距離で不連続に切れない。音量だけでなく高域減衰を周波数依存filterで表現する。
- 指向性coneは**音源のorientation**とlistenerへの方向から計算する。listenerが顔を背けただけで音源の放射特性が変わる式にしない。
- 初期反射はまず簡易roomの一次反射から始め、経路長のdelay、壁の帯域別吸収、方向を持たせる。後期残響は共有FDN等を候補とし、直接音とは別send／returnで調整する。
- 反射数、delay長、feedbackを制限し、mute／seek／room変更後に発散・古いtailの混入を起こさない。人工的な広がりを既定で過剰に加えない。
- 遮蔽とdopplerは定位基盤の完成後。遮蔽は最初に手動量＋filter、将来scene probeを追加する。dopplerは明示有効化し、速度・音速単位を揃え、teleport／seekでは速度履歴を破棄する。

### 6. UI・保存・交換

- Spatial Audioの主要項目はProperty側の専用セクション、object一覧とBed／Object routingはAudio Mixer。Timeline左ペインへ空間プロパティ群を追加しない。
- UIは音源位置・幅・gain、listener選択、Speaker／Headphone、品質、room sendを中心とする。DSP実装名を通常操作の前面に出さない。
- キーフレーム、複製、削除、source変更は既存command／Undo経路を使う。新規signal/slotや中央イベント配線は追加しない。
- 保存にschema version、object ID、単位変換、source参照、routing、layout、HRTF ID/hash、品質、room設定を含める。古いspatial.*は互換値で移行し、未知の任意フィールドは可能な範囲で保持する。
- 独自JSON＋PCM WAVの交換を第一形式とする。source stemとobject automationを保持し、flatten済み多ch WAVも出力できるようにする。12ch順序はmanifestに明記し、再読込で検証する。
- 欠落素材／HRTFは再リンクでき、無音／stereo fallbackの状態を明示する。公式Atmos/Auro形式の出力や互換ロゴは提供しない。

## 実装マイルストーンと完了条件

既存のM-AU-9.1〜9.5を保持して具体化する。9.6以降は今回追加した高品質化段階。番号は実装順ではない。

| ID | 段階・依存 | 完了条件 | 現状 |
|---|---|---|---|
| M-AU-9.1 | Object・時間・座標契約 | 単位、listener、ID複製規則、任意サンプル時刻評価、schema移行を固定。旧JSONの復元とUndoで一致 | 既存ID／保存のみ部分実装 |
| M-AU-9.2 | 再生経路の安定化、9.1後 | mono/stereo、gain一重適用、seek世代管理、sample-rate変更、tail処理、確保／lock境界を検証 | 最小接続済み、実行未検証 |
| M-AU-9.4a | Speaker配置契約、9.2後 | 2/6/8/12chのdescriptor、順序、LFE、downmixをimpulseで確認 | ch定義／出力選択のみ存在 |
| M-AU-9.4b | Speaker定位、9.4a後 | VBAP、spread、境界補間、未被覆方向fallback。全周・頭上移動が連続 | VBAP／fallbackを実装、実機未検証 |
| M-AU-9.6a | HRTF候補評価、9.1後に独立調査可 | 出典・同梱条件と品質比較を記録し、無償配布可能な内蔵候補を決定 | 未着手 |
| M-AU-9.6b | Headphone定位、9.2＋9.6a後 | HRIR import、畳み込み、delay、方向補間、preset切替、stereo fallback | 解析的ITD／head-shadowを実装、HRIRは未実装 |
| M-AU-9.3 | 制作UI、9.1後に設計、9.4b／9.6bで統合 | Property／Mixerから編集・試聴・Undo・保存復元できる。未対応モードを選ばせない | 作成メニュー、Property編集（距離・cone・spread・layout・monitor・LFE）を接続。Mixer routing／実行確認は未完 |
| M-AU-9.7 | 距離・指向性・room、9.4b／9.6b後 | 帯域減衰、音源cone、初期反射、残響tailの安定性と試聴合格 | 簡易減衰のみ存在 |
| M-AU-9.5 | 独自交換・Export、9.3後 | JSON＋WAVとflatten出力を再読込。Previewと同じ時刻／品質で一致 | 未実装 |
| M-AU-9.8 | 最終品質・性能、各経路完成後 | 以下の数値検証・試聴・長時間再生を実施し、結果と限界を記録 | 未着手 |

**実装順:** 9.1 → 9.2 → 9.4a/b → 9.6b → 9.3統合 → 9.7 → 9.5 → 9.8。9.6aの候補調査は前倒し可能。最初の実装単位は9.1の時刻・座標評価と9.2のgain／seek整合に限定する。

## 品質・性能の受入計画

以下の数値は設計目標であり、実測値でも性能保証でもない。実機測定後に対象CPU、デバイス、ビルド構成、object数、IR長、block sizeとともに確定する。

| 分野 | 確認内容・初期目標 |
|---|---|
| 基準条件 | 48kHz、256 sample/block（約5.33ms）、speaker 32 mono objects、headphone 16 mono objectsを別々に測定 |
| 処理予算 | 空間DSP時間p99をblock期間の25%以下、最大を50%以下の目標とする。全audio経路でもdeadline超過なしを確認 |
| 負荷試験 | 上記基準で30分再生、underflow／NaN／Inf／発散なし。64 objectsも測定し対応可能数を公表 |
| レイアウト | 各ch単独impulseが指定speakerへ一致。12chで範囲外アクセスなし。LFEが通常定位計算に混入しない |
| Gain | 減衰・spread・LFE・Masterを無効にした点音源でsum(gain²)=1±1e-4。境界移動で不連続なし。音量設定が一度だけ適用される |
| 時刻 | 23.976/29.97/30/60fps、44.1/48/96kHzで同じサンプル時刻を評価。100回のseekで旧generation音声を出さない |
| 出力一致 | 同じ品質設定・latency補正・pre-roll条件でpreviewオフライン再現とexportの誤差を測定。決定的CPU経路は最大絶対誤差1e-5を初期目標 |
| フィルター | impulse／無音／正弦波／広帯域ノイズでdelay、tail、切替clickを確認。HRTF切替は無音化や急激なレベル変化を起こさない |
| 聴感 | 前後、上下、耳元、遠方、全周移動、stereo幅、複数音源を音量を揃えて比較。前後混同、上下定位、音色変化、疲労感を複数人で記録 |
| 個人差 | 単一HRTFで万人への高精度定位を保証しない。presetごとの結果と選び方を残し、左右パンだけの基準より改善を確認 |
| 保存 | 新旧JSON、素材欠落、HRTF欠落、複製、Undo/Redo、再リンク、layout変更、tailを含むexport再読込を確認 |

- Preview品質はStandard／High、ExportはHighを初期案とする。制御頻度・IR方式・反射数の具体差は測定で決定し、黙ってHRTFを切ったり音源を落としたりしない。
- CPU最適化はバッファ再利用、係数事前計算、SIMD、共有returnを優先する。GPU音声処理は初期範囲外とし、デバイス同期による遅延要因を増やさない。
- テスト／ビルド／CMake実行はユーザーの明示指示後に行う。この文書更新では実行していない。

## 実装上の制約

- 既存の独自コンテナとAudioバッファを優先。PImplの所有規則、LF、global module fragmentのinclude規則を守る。
- 新規モジュールを増やす前に既存Spatial実装への追加を検討する。実装専用依存をinterfaceへ広げず、必要なsource manifest登録を確認する。
- 子リポジトリの変更、依存SDKの追加、既存APIの変更は各実装タスクの範囲を明示して進める。本タスクは設計文書のみ。
- 本文を本機能の上位設計とする。旧高性能設計の「固定128tap」「HRTF同梱を非目的」は今回方針で置き換える。旧文書を実装済み仕様として扱わない。

## 根拠・関連文書

技術的な方式選択は設計判断であり、以下の資料がArtifactStudioの品質や無償利用条件を保証するものではない。

- [Pulkki: Spatial sound generation and perception by amplitude panning techniques](https://aaltodoc.aalto.fi/items/454e38fc-4bbc-4be6-ab8b-3f30b0f7fba0/full) — VBAPを含む振幅パンニングの研究資料。speaker方式の参考。
- [Steam Audio Programmer’s Guide](https://valvesoftware.github.io/steam-audio/doc/capi/guide.html) — HRTFの方向補間や空間処理構成の公式技術資料。SDK採用は未決定。
- [SOFA conventions](https://www.sofaconventions.org/mediawiki/index.php/SOFA_conventions) — 音響データと座標メタデータの入力契約の参考。
- [SOFA Files](https://www.sofaconventions.org/mediawiki/index.php/Files) — HRTF候補の所在。データセットごとの利用条件を別途確認する。
- `docs/planned/MILESTONE_3D_SPATIAL_AUDIO_HIGHPERF_2026-08-23.md` — 旧高性能設計。
- `docs/planned/MILESTONES_BACKLOG.md` — M-AU-1 / M-AU-4 / M-AU-5 / M-AU-9。
- `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`
- `ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`
