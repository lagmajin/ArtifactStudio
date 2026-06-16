# String Helper Proposal (ArtifactCore) — 2026-06-16

**Author**: CommandCode (dev/commandcode-2026-06-16 worktree)
**Status**: Proposal (未着手)
**Target**: `ArtifactCore/include/Utils/**` 配下

---

## 背景

`ArtifactCore` の `Utils.String` には現状 `calculateLevenshteinDistance` 1 個しかなく、`split` / `join` / `trim` / `toLower` / `toUpper` / `format` / `path normalize` 等の典型 API が皆無。
各所 (TimeCode / CsvParser / MultipleTag / LUTLoader / PatternNameGenerator 等) で重複して自前実装されている。

## 方針

- AGENTS.md 準拠: 新規 signal/slot 追加なし / `QImage` 追加なし / QtCSS なし
- 既存 `QString / std::string / std::string_view` を `StringLike` concept で受ける薄いラッパ
- `ArtifactCore/CMakeLists.txt` の GLOB_RECURSE 自動収集を信じて、ヘッダを置くだけで module 登録される
- テストは `tests/ArtifactCore/UtilsTest.cpp` に追記

---

## Module A: `Utils.String` 拡張 (汎用 string 関数)

新規ヘッダ: `ArtifactCore/include/Utils/String.ixx` を分割または拡張

### A-1: split / splitView
```cpp
QStringList split(const QString& s, QChar sep,
                  Qt::SplitBehavior behavior = Qt::KeepEmptyParts,
                  Qt::CaseSensitivity cs = Qt::CaseSensitive);
std::vector<std::string_view> splitView(std::string_view s, char sep);  // 零コピー
QStringList splitAny(const QString& s, const QString& seps,
                     Qt::SplitBehavior = Qt::KeepEmptyParts);
```
- 既存重複: TimeCode / CsvParser / MultipleTag / LUTLoader / ColorScope
- 既存: `QString::split` を直接呼ぶ箇所が 5+ 個所

### A-2: join
```cpp
QString join(const QStringList& parts, QChar sep);
QString join(const QStringList& parts, const QString& sep);
QString join(std::span<const QStringView> parts, QChar sep);
```
- 既存: `MultipleTag::toString(sep)` が tag 特化で join 相当

### A-3: trim / trimStart / trimEnd
```cpp
QString trimmed(const QString& s);                  // 全角空白も対応
QString trimmed(std::string_view s);
QChar unicodeWhitespaceChar();                      // \u00A0 等も含める判定
bool isWhitespace(QChar c);                         // QString::isSpace は ASCII のみ
```
- 既存: `CsvParser` / `MultipleTag::addTag` で自前 trim
- 注意: `QString::trimmed` は ASCII whitespace のみ。全角/ZWSP を考慮するなら明示実装

### A-4: case conversion
```cpp
QString toLower(const QString& s, Qt::LocaleCode loc = Qt::DefaultLocaleCode);
QString toUpper(const QString& s, Qt::LocaleCode loc = Qt::DefaultLocaleCode);
bool iequals(QStringView a, QStringView b);
bool iless(QStringView a, QStringView b);
bool icontains(QStringView haystack, QStringView needle);
bool istartsWith(QStringView s, QStringView prefix);
bool iendsWith(QStringView s, QStringView suffix);
```
- 既存: `MultipleTag::filterByPrefix` 内の `Qt::CaseInsensitive` ベース手書き
- ロケール依存 (tr-TR の I / de-DE の ß) が必要な場合はオプションで `QString::toLower(loc)` 経由

### A-5: contains / startsWith / endsWith の span 対応
```cpp
bool contains(QStringView haystack, QStringView needle, Qt::CaseSensitivity cs = Qt::CaseSensitive);
bool contains(QStringView haystack, QChar needle);
bool startsWith(QStringView s, QStringView prefix, Qt::CaseSensitivity cs = Qt::CaseSensitive);
bool endsWith(QStringView s, QStringView suffix, Qt::CaseSensitivity cs = Qt::CaseSensitive);
bool containsAny(QStringView haystack, std::initializer_list<QStringView> needles);
```
- 既存は `QString` 直渡しのみ。`QStringView` 受け取りでコピー削減

### A-6: replace
```cpp
QString replaceAll(QStringView s, QStringView from, QStringView to,
                   Qt::CaseSensitivity cs = Qt::CaseSensitive);
QString replaceFirst(QStringView s, QStringView from, QStringView to,
                     Qt::CaseSensitivity cs = Qt::CaseSensitive);
int replaceInPlace(QString& s, QStringView from, QStringView to, int maxCount = -1);
```
- 既存: TimeCode `setFromQString` 内の `.replace('.', ':')`、MultipleTag 等

### A-7: padding
```cpp
QString padLeft(QStringView s, int width, QChar fill = QLatin1Char('0'));
QString padRight(QStringView s, int width, QChar fill = QLatin1Char(' '));
QString repeat(QStringView s, int count);
```
- 既存: TimeCode `toString` の `QString("%1:%2:%3:%4").arg(h, 2, 10, QChar('0'))`

### A-8: char class 判定
```cpp
bool isAscii(QChar c);
bool isAsciiDigit(QChar c);
bool isAsciiAlpha(QChar c);
bool isAsciiAlphaNumeric(QChar c);
bool isAsciiHexDigit(QChar c);
bool isIdentifierStart(QChar c);                    // Unicode 識別子 (Lu/Ll/Lt/Lm/Lo/Nl/_\u0024 等)
bool isIdentifierContinue(QChar c);
```

### A-9: safe integer / number parse
```cpp
std::optional<int> tryParseInt(QStringView s, int base = 10, bool* ok = nullptr);
std::optional<long long> tryParseLongLong(QStringView s, int base = 10, bool* ok = nullptr);
std::optional<double> tryParseDouble(QStringView s, bool* ok = nullptr);
std::optional<float> tryParseFloat(QStringView s, bool* ok = nullptr);
bool tryParseInt(QStringView s, int& out, int base = 10);
bool tryParseDouble(QStringView s, double& out);
```
- 既存: `QString::toInt` (失敗時 0 を返す) → 0 と失敗の区別がつかない
- 既存: `CsvParser.ixx:91-97` でも `toInt` を呼んでおり、不正値検出していない

### A-10: numeric formatting
```cpp
QString formatInt(int64_t v, int base = 10, int width = 0, QChar fill = QLatin1Char('0'),
                  QChar sign = QLatin1Char('\0'));
QString formatDouble(double v, int precision = -1, char format = 'g',
                      int width = 0, QChar fill = QLatin1Char('0'));
QString hex(uint64_t v, int width = 0, bool upper = true);
QString oct(int64_t v, int width = 0);
QString bin(uint64_t v, int width = 0);
```
- 既存: `QString::number / QString::asprintf` が氾濫。0-padding 付きは `QString("%1").arg(v, width, 10, QChar('0'))` の繰り返し

### A-11: regex helpers
```cpp
QRegularExpression compileRegex(const QString& pattern,
                                QRegularExpression::PatternOptions options = {});
bool matches(const QRegularExpression& re, QStringView s);
QString regexReplace(const QRegularExpression& re, QStringView s, QStringView replacement);
QStringList regexMatchAll(const QRegularExpression& re, QStringView s);
bool isValidRegex(const QString& pattern, QString* errorMessage = nullptr);
```
- 既存: `<regex>` を多数 include しているがヘルパ無し。`MultipleTag::filterByPattern` が `QRegularExpression` を生で組み立て

---

## Module B: `Utils.Text.Encoding` (新規)

`QString ↔ std::*` の 4 箇所重複を解消 + Latin-1 / BOM 検出 / 安全 UTF-8 検証

新規ヘッダ: `ArtifactCore/include/Utils/Text/Encoding.ixx` (module `Utils.Text.Encoding`)

### B-1: UTF-8 検証
```cpp
bool isValidUtf8(std::string_view bytes);
std::optional<int> utf8CharLength(uint8_t leadByte);
std::optional<int> utf8CodepointCount(std::string_view bytes);
QString fromUtf8Checked(std::string_view bytes);    // 不正なら null QString
```

### B-2: 変換
```cpp
std::string toLatin1(const QString& s);             // 既存 QString::toLatin1 の薄い型
std::string toUtf8(const QString& s);
std::u16string toUtf16(const QString& s);
std::u32string toUtf32(const QString& s);
std::wstring toWide(const QString& s);              // Win: UTF-16, others: UTF-32

QString fromLatin1(std::string_view s);
QString fromUtf8(std::string_view s);
QString fromUtf16(std::u16string_view s);
QString fromUtf32(std::u32string_view s);
QString fromWide(std::wstring_view s);
```
- 既存: `StringConvertor.ixx` の 3 関数 + `UniString::operator std::string()` (`src/Utils/UniString.cppm:181`) + `StringLike.ixx` の 4 関数で重複
- **バグ温床**: `UniString::operator std::string()` は UTF-16 単位の 2byte 並びをそのままコピー → ASCII 以外で文字化け

### B-3: BOM 検出
```cpp
enum class ByteOrderMark { None, UTF8, UTF16_LE, UTF16_BE, UTF32_LE, UTF32_BE };
ByteOrderMark detectBOM(const void* data, size_t size);
QString stripBOM(QStringView s);
```

### B-4: endian swap
```cpp
std::u16string swapEndian(std::u16string_view s);
std::u32string swapEndian(std::u32string_view s);
```

### B-5: codepoint 単位操作
```cpp
std::u32string toCodepoints(QStringView s);
QString fromCodepoints(std::u32string_view cps);
int codepointCount(QStringView s);                  // QString::length() は UTF-16 単位
bool isValidUnicodeScalarValue(uint32_t cp);
```

---

## Module C: `Utils.Path` 拡張 (path ユーティリティ)

既存: `Path.ixx` は `getAppPath / getIconPath` のみ。`class Path` (Pimpl) は中身空。

### C-1: 純粋関数版
```cpp
QString joinPath(const QString& base, const QString& leaf);
QString joinPath(const QString& base, const QStringView& leaf);
QString joinPath(std::initializer_list<QStringView> parts);
QString normalizePath(const QString& path);         // "a//b/./c" → "a/b/c"
QString canonicalPath(const QString& path);         // weakly_canonical 相当
QString relativePath(const QString& from, const QString& to);
QString directoryOf(const QString& path);
QString fileNameOf(const QString& path);
QString stemOf(const QString& path);                // "/a/b.c.txt" → "b.c"
QString extensionOf(const QString& path);           // "/a/b.txt" → "txt"
QString baseNameOf(const QString& path);            // "/a/b.txt" → "b"
bool isAbsolutePath(const QString& path);
QString toNativeSeparators(const QString& path);
QString fromNativeSeparators(const QString& path);
```
- 既存: `QDir::cleanPath / QFileInfo` を直接呼ぶ箇所 3+ 個所 (Path / ShellUtils / ExplorerUtils)

### C-2: クラス版 (Pimpl 実装)
```cpp
class Path {
public:
    explicit Path(const QString& s);
    Path& append(const QString& s);
    Path& operator/=(const QString& s);
    QString str() const;
    bool isAbsolute() const;
    bool exists() const;
    bool isFile() const;
    bool isDir() const;
    qint64 fileSize() const;
    QDateTime lastModified() const;
    bool createDirectory() const;
    bool remove() const;
    Path parent() const;
    Path filename() const;
    Path stem() const;
    Path extension() const;
    bool operator==(const Path& other) const;
    bool operator<(const Path& other) const;
private:
    class Impl; std::unique_ptr<Impl> impl_;
};
```

---

## Module D: `Utils.String.SemVer` (新規, version compare)

`SemVer::parse / compare`、OS バージョン文字列、driver バージョン比較用

```cpp
struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    QString preRelease;       // "alpha.1"
    QString buildMeta;        // "001"
    static std::optional<SemVer> parse(QStringView s);
    static SemVer fromString(QStringView s);         // 失敗時 SemVer{0,0,0,"invalid"}
    int compare(const SemVer& other) const;         // < 0 / 0 / > 0
    bool isValid() const;
    QString toString() const;                        // "1.2.3-alpha.1+build.5"
};
bool operator==(const SemVer& a, const SemVer& b);
bool operator<(const SemVer& a, const SemVer& b);
bool operator>(const SemVer& a, const SemVer& b);
int compareVersionStrings(QStringView a, QStringView b);   // "1.2.3" vs "1.10.0"
```

---

## Module E: `Utils.Text.ColorHex` (新規, color hex formatting)

AE / NLE ドメイン特化

```cpp
struct ColorHex {
    enum class Format { RGB, RGBA, ARGB, RRGGBB, RRGGBBAA, AARRGGBB };
    static QString format(const ArtifactCore::FloatRGBA& c, Format fmt = Format::RGBA8);
    static std::optional<ArtifactCore::FloatRGBA> parse(QStringView s);
    static QString rgbToCss(const ArtifactCore::FloatRGBA& c);
    static QString rgbToHsl(const ArtifactCore::FloatRGBA& c);
    static QString rgbToHsv(const ArtifactCore::FloatRGBA& c);
    static QString rgbToHex(const ArtifactCore::FloatRGBA& c, bool alpha = true, bool upper = true);
    static std::optional<ArtifactCore::FloatRGBA> hexToRgb(QStringView s);
};
```
- 既存: `FloatColor / FloatRGBA` に hex API 無し

---

## Module F: `Utils.Text.Identifier` (新規, identifier sanitization)

`Rule/NamingRule.hpp` の中身が空なので実装

```cpp
namespace NamingRule {
    bool isValidIdentifier(QStringView s);           // C++ identifier 風
    bool isValidFileName(QStringView s);             // Windows / Mac / Linux で valid
    bool isValidLayerName(QStringView s);            // AE 互換
    bool isValidCompositionName(QStringView s);
    QString toValidIdentifier(QStringView s);
    QString toValidFileName(QStringView s, QChar replacement = QLatin1Char('_'));
    QString toValidLayerName(QStringView s);
    QString quoteIfNeeded(QStringView s, QChar quote = QLatin1Char('"'));
}
```

---

## Module G: `Utils.Text.Encoding.URL` (新規, URL / base64 / HTML escape)

W3C / HTTP 系ヘルパ

```cpp
QString urlEncode(QStringView s, bool formEncoded = false);
QString urlDecode(QStringView s);
QString base64Encode(QByteArrayView bytes, bool urlSafe = false);
QByteArray base64Decode(QStringView s, bool urlSafe = false, bool* ok = nullptr);
QString htmlEscape(QStringView s, bool quoteOnly = false);
QString htmlUnescape(QStringView s);
QString jsonEscape(QStringView s, bool ensureAscii = true);
QString xmlEscape(QStringView s);
```

---

## Module H: `Utils.String.ScriptDetection` (新規, 文字種判定)

```cpp
enum class Script {
    Unknown, Common, Latin, Greek, Cyrillic, Armenian, Hebrew, Arabic,
    Devanagari, Thai, Han, Hiragana, Katakana, Hangul, Ethiopic,
    CJK, Emoji,
};
Script detectScript(QChar c);
Script detectDominantScript(QStringView s);
bool isCJK(QChar c);
bool isEmoji(QChar c);
bool isRightToLeft(QChar c);
```

---

## Module I: `Utils.Text.TimeCodeFormatting` (TimeCode 拡張)

`TimeCode::toString` の `HH:MM:SS:FF` 0-pad 固定 → SMPTE drop-frame 等

```cpp
namespace TimeCodeFormat {
    enum class Separator { Colon, Semicolon, Dot, Comma };
    enum class DropFrame { Auto, Always, Never };
    QString format(const ArtifactCore::TimeCode& tc,
                   const ArtifactCore::FrameRate& fps,
                   Separator sep = Separator::Semicolon,
                   DropFrame df = DropFrame::Auto,
                   bool forceDropFrame = false);
    QString formatFrameNumber(int64_t frame, const ArtifactCore::FrameRate& fps,
                              int width = 0, QChar fill = QLatin1Char('0'),
                              QChar sign = QLatin1Char('\0'));
    QString formatTimecodeRange(const ArtifactCore::TimeCodeRange& range,
                                const ArtifactCore::FrameRate& fps);
    QString formatRational(const ArtifactCore::RationalTime& rt,
                            const ArtifactCore::FrameRate& fps);
}
```

---

## Module J: `Platform.ShellQuoting` (新規, プロセス起動時の quoting)

```cpp
namespace ShellQuoting {
    enum class ShellType { Posix, CmdExe, PowerShell };
    QString quote(QStringView arg, ShellType shell = ShellType::Posix);
    QStringList quoteAll(QStringView command, std::initializer_list<QStringView> args,
                         ShellType shell = ShellType::Posix);
    QString escapeForCmdExe(QStringView s);
    QString escapeForPowerShell(QStringView s);
    QString escapeForPosix(QStringView s);
}
```

---

## 実装優先度

| 優先度 | Module | 根拠 |
|---|---|---|
| ★★★ | A-1〜A-7 (汎用 string) | 5+ 箇所で重複、即時削減効果大 |
| ★★★ | B-2 (UTF-8 検証 + 変換) | UniString バグ温床を根本解消 |
| ★★★ | C-1 (path 純粋関数) | 3+ 箇所で重複、QDir 直接呼出撲滅 |
| ★★ | A-9 (safe integer parse) | CsvParser の silent failure 解消 |
| ★★ | A-11 (regex helper) | MultipleTag の生組み立て簡素化 |
| ★★ | B-1 (UTF-8 検証) | 外部ファイル読込時の安全性 |
| ★★ | F (identifier sanitization) | `Rule/NamingRule.hpp` 実装 |
| ★ | D (SemVer) | Asset メタや plugin version 比較 |
| ★ | E (color hex) | AE 互換性 + ユーザー要望多 |
| ★ | G (URL / base64 / HTML) | HTTP / clipboard 機能 |
| ★ | I (TimeCode formatting) | SMPTE drop-frame 対応 |
| ○ | H (script detection) | ニッチ |
| ○ | J (shell quoting) | セキュリティ、ただし呼び出し箇所が限られる |

## テスト方針

`tests/ArtifactCore/UtilsTest.cpp` に追記 (GoogleTest):

- A-1〜A-11: 各関数に 3-5 アサーション (境界値、empty、Unicode 含む)
- B-1: 不正 UTF-8 配列の rejection
- B-2: ASCII / 日本語 / サロゲートペア round-trip
- C-1: `"a//b/./c"` → `"a/b/c"`、`"C:\\a\\..\\b"` → `"C:/b"`
- D: `"1.2.3" < "1.10.0"`、`"1.0.0-alpha" < "1.0.0"`
- F: `"layer name / 1"` → `"layer_name_1"`
- G: `"a b/c"` → `"a%20b%2Fc"`、base64 round-trip
- I: `29.97 df` の `;` 区切り

## 関連ファイル

- 既存: `ArtifactCore/include/Utils/String.ixx`, `StringLike.ixx`, `StringConvertor.ixx`, `UniString.ixx`, `Path.ixx`, `Id.ixx`, `HashValue.ixx`, `AssetFingerprint.ixx`, `NameGenerator.ixx`, `MultipleTag.ixx`, `Tag.ixx`, `Localization.ixx`, `ExplorerUtils.ixx`, `JsonLike.ixx`
- 既存: `ArtifactCore/include/Rule/NamingRule.hpp` (空)
- 既存: `ArtifactCore/include/Time/TimeCode.ixx` / `Frame/FrameRate.ixx`
- 既存: `ArtifactCore/include/Color/ColorSwatch.ixx` / `Color/Grading/LUTLoader.ixx`
- 既存: `ArtifactCore/include/Data/CsvParser.ixx`
- 既存: `ArtifactCore/include/Platform/ShellUtils.ixx`
- 既存: `ArtifactCore/include/EnvironmentVariable/EnvironmentVariable.ixx`
- 既存テスト: `tests/ArtifactCore/UtilsTest.cpp`
- 既存 CMake: `ArtifactCore/CMakeLists.txt` (GLOB_RECURSE 自動収集)
