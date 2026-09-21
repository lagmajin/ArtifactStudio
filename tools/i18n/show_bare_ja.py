import re
files = [
    "Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm",
]
jp = re.compile(r'[\u3040-\u30ff\u4e00-\u9fff\uff01-\uff5e]')
prot = re.compile(r'menuText\s*\(|tt\s*\(|TranslationManager::instance\(\).tr\(|tr\s*\(')
for p in files:
    print("==== " + p)
    for i, line in enumerate(open(p, encoding="utf-8").read().splitlines(), 1):
        if jp.search(line) and not prot.search(line):
            print(str(i) + ": " + line.rstrip())
