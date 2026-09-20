import re, sys
files = [
    "Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactRenderOutputSettingDialog.cppm",
    "Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm",
]
jp = re.compile(r'[\u3040-\u30ff\u4e00-\u9fff\uff01-\uff5e]')
for p in files:
    try:
        s = open(p, encoding="utf-8").read()
    except FileNotFoundError:
        print(p, "FILE NOT FOUND"); continue
    cnt = 0
    for i, line in enumerate(s.splitlines(), 1):
        if jp.search(line) and not re.search(r'menuText\s*\(|tt\s*\(|TranslationManager::instance\(\).tr\(|tr\s*\(', line):
            cnt += 1
    print(p + ": " + str(cnt) + " bare-ja lines")
