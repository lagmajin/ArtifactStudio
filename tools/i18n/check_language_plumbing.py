from pathlib import Path

FILES = [
    "ArtifactCore/include/Utils/Localization.ixx",
    "ArtifactCore/src/Localization/Localization.cppm",
    "Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm",
    "Artifact/src/AppMain.cppm",
]
for f in FILES:
    c = Path(f).read_text(encoding="utf-8")
    print(f"{f}: paren={c.count('(') - c.count(')')} brace={c.count('{') - c.count('}')}")

checks = {
    "ArtifactCore/include/Utils/Localization.ixx": ["struct LocaleChangedEvent"],
    "ArtifactCore/src/Localization/Localization.cppm": ["publish(LocaleChangedEvent"],
    "Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm": ["setLanguageCode"],
}
for f, needles in checks.items():
    c = Path(f).read_text(encoding="utf-8")
    for n in needles:
        print(f"  {'OK' if n in c else 'MISSING'}: {f} -> {n}")
