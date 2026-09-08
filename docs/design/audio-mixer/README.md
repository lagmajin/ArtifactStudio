# Audio Mixer concept

**最終更新:** 2026-09-08

**採用状況:** 2026-09-08 ユーザー採用済み。主要外観・操作部品を実装。静的確認のみ、ビルド・テスト・実画面・実音声検証は未実施。内蔵 imagegen を使用。

[モック画像](audio-mixer-dcc-concept-2026-09-08.png)

6 source channels + Master、長いフェーダー、ステレオメーター、チャコール＋アンバー配色。画像内の機能は実装済みを意味しない。実装の列数と名前は現在のコンポジションの実レイヤーに従う。

## 2026-09-08 実装

- `ArtifactCompositionAudioMixerWidget.cppm`: ヘッダーを1行に整理し、現在コンポジション名とRouting導線を表示。チャンネル列の右端へMasterを配置。フッターに既存Playback Serviceを呼ぶ再生・一時停止・停止を追加。
- `ArtifactCompositionAudioMixerPresentation.cppm`: チャコールの176px幅ストリップ、下端の細いチャンネル色、2段FX欄、Output→FX→Pan→Mute/Solo→フェーダーの順に配置。FXの描画と当たり判定を共通の矩形に統一。操作フォーカスのあるチャンネルをアンバー枠で表示（レイヤー選択同期とは別）。
- `ArtifactAudioMixerControls.cppm`: 銀色のフェーダーキャップ、アンバーの指標・パンノブ、固定dB閾値の分割メーター。QProxyStyleの形状をフェーダー描画とネイティブ入力で共有。0 dBを正確なステップに取り、無音から既存のgain上限2.0までの対数表示へ変更。
- ノブは相対ドラッグ、Shift微調整、フォーカス内の矢印キーとホイール、Home／ダブルクリックの中央復帰。フェーダーはダブルクリックで0 dB復帰、フォーカス中のみホイール入力。既存の音量・パン・Mute/Solo・Undo経路を利用。
- `Artifact/App/Icon/Studio/mixer_*.svg`: panel / routing / mute / solo / power / play / pause / stop のオリジナルSVGを追加して使用。既存のアイコンGLOBにより収集される。CMake編集・実行は行っていない。
- Masterメーターへフェーダー位置を信号レベルとして表示していた処理を撤去。MasterのPanは操作不能と明示。

## 部品生成と適用範囲

### ノブ参照の更新（2026-09-08）

ユーザー提供の [FabFilter Pro-Q 4参考画像](pro-q-4-knob-reference-2026-09-08.webp) をノブ外観の参照に採用。フェーダー、アイコン、メーター、全体レイアウトは既存案の採用を維持する。

ノブのみ暗いフラットな円盤、細い外周リング、円盤から離したアンバーの値の弧、短い外周指標へ調整。パンは中央を起点に左右へ弧が伸びる仕様とし、中央目印と控えめなフォーカスリングを残す。参照画像のEQグラフ、ブランド表示、配色全体、動的EQの赤い帯やパネル構成は転用しない。値・ドラッグ・キー操作・Undo経路は変更していない。静的差分確認のみ、ビルド・実画面確認は未実施。

[生成した部品見本](audio-mixer-controls-reference-2026-09-08.png) は内蔵imagegenによるスタイル参照。実際の操作部品はSVGと既存owner-draw部品で実装し、生成画像をUIへ貼り付けていない。見本の50Lノブの指標やフェーダー目盛りの不整合は再現しない。

Qt DCC UIレビュー方針に従い、余白・チャンネル間の区切り・編集数値・フォーカス表示を局所的に整理。QtCSS、新規signal/slot接続、QImage、標準コンテナへの置換は追加していない。ArtifactCore / ArtifactWidgets / Viewport は変更していない。

モックのInput選択、Follow Selection、Master insert、DIM、BPM、サンプルレート／DSP表示、タイムコードは今回追加していない。既存Routing画面とFXメニューを維持する。実機では狭いdock／DPI、フェーダーの端点と0 dB、パン・Undo、FXメニュー、実音声メーター、Composition切替、再生操作の確認が必要。

## Parts generation prompt

Use case: ui-mockup. Create a high fidelity component reference sheet for ArtifactStudio's approved charcoal and amber Audio Mixer desktop DCC UI. Landscape 16:9 image, flat front view, no perspective. Charcoal #242629 background, subtly lighter #303236 control surfaces, off-white text, amber #e4ad53 accent. Three clearly labeled sections: 'ICONS', 'FADER', 'PAN'. ICONS: original bold solid 16px-readable mixer sliders icon, routing branching icon, loudspeaker mute icon, headphones solo icon, power icon, presented at enlarged size with a small actual-size companion. FADER: two tall narrow vertical audio faders, idle and focused, dark recessed rail, satin silver rectangular cap with three horizontal grip lines, amber center index; calibrated +6, 0, -6, -12, -24, -36, -48, -60 scale; subtle amber focus outline on focused version. PAN: three rotary knobs at 50L, C, 50R with matte charcoal circular body, softly beveled rim, clear amber radial index and partial amber arc, small amber numeric labels below. Practical restrained professional Qt/DCC controls, crisp geometry, engineering reference rather than advertisement, no shiny chrome or extreme shadows, no neon, no full application screenshot. These are design references for scalable SVG icons and interactive owner-drawn controls.

## Generation prompt

Use case: ui-mockup. Generate one high-fidelity desktop UI screenshot mockup for ArtifactStudio Audio Mixer, landscape 16:9, sharp legible English UI. Professional dense DCC application panel inspired by After Effects audio workflow and modern DAW mixers. Entire image is a flat front-facing application panel, no physical monitor, no perspective. Charcoal gray #242629 background, slightly lighter #303236 channel strips, off-white labels, restrained amber #e4ad53 editable numbers and active accents, green level meters with yellow peaks and red only for clipping. Compact square controls, bold clear icons, subtle separators, no oversized rounded cards, no glowing futuristic effects. Header 'Audio Mixer' and composition selector 'Composition 01', compact 'Follow Selection' control. Main content six consistent vertical source strips named 'Dialogue', 'Music', 'Ambience', 'Foley', 'Impact', 'Voice FX', then a clearly separated 'Master' strip. Each strip has short input/output routing labels, two compact insert slots, pan knob with value, M and S buttons, long calibrated dB fader with adjacent stereo meter and peak readout, channel name with subtle color underline. Music is selected using a thin amber outline. Master has larger stereo meters, clearly labeled L R, Output Stereo. Bottom compact transport with play pause stop, timecode '00:00:12:08', '48 kHz', no timeline taking space. Functional believable engineering-ready UI, generous fader travel, aligned controls, minimal repetitive text, all seven strips fully visible. Deliver only the mockup image.
