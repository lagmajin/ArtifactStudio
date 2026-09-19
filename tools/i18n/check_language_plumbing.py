from pathlib import Path

FILES = [
    "Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm",
    "ArtifactCore/src/Localization/Localization.cppm",
    "ArtifactCore/include/Utils/Localization.ixx",
]
for f in FILES:
    c = Path(f).read_text(encoding="utf-8")
    print(f"{f}: paren={c.count('(') - c.count(')')} brace={c.count('{') - c.count('}')}")

checks = {
    "ArtifactCore/include/Utils/Localization.ixx": [
        "enum class PluralCategory", "pluralCategoryFor", "translatePlural", "pluralCategory("],
    "ArtifactCore/src/Localization/Localization.cppm": [
        "fallbackChainFor", "PluralCategory::Few", "translatePlural", "pluralSuffix"],
    "Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm": [
        "import Core.Localization;", "availableLocales()", "kLanguageOptions"],
}
for f, needles in checks.items():
    c = Path(f).read_text(encoding="utf-8")
    for n in needles:
        print(f"  {f}: {'OK' if n in c else 'MISSING'} -> {n}")
