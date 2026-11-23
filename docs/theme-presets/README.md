# Theme Preset JSON

**最終更新:** 2026-11-20

`Artifact` のテーマは JSON で上書きできます。

## Keys

- `base`: `Studio`, `Maya`, `Modo`, `Blender`, `DaVinci`, `3ds Max`, `Nuke`, `After Effects`, `Light`
- `accentColor`
- `textColor`
- `backgroundColor`
- `secondaryBackgroundColor`
- `selectionColor`
- `borderColor`
- `buttonColor`
- `buttonHoverColor`
- `buttonPressedColor`
- `focusRingColor` は入力などのキーボードフォーカス枠
- `dockFocusFrameColor` はフォーカス中のドックの外枠。省略時や無効な色の指定時は `#A5CCFF`

## Example

外部プリセットは `UI/ThemePresetPath` に JSON ファイルのパスを指定すると、起動時に読み込まれます。ドックの外枠を含む例は [Studio Neutral Amber](studio_neutral_amber_skin.json) を参照してください。
