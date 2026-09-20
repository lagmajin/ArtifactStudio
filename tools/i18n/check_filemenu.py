import os, re
jp = re.compile(r'[\u3040-\u30ff\u4e00-\u9fff\uff01-\uff5e]')
prot = re.compile(r'menuText\s*\(|tt\s*\(|TranslationManager::instance\(\).tr\(|tr\s*\(')
for p in ["Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm",
          "Artifact/src/Widgets/Menu/ArtifactRenderOutputSettingDialog.cppm"]:
    print("====", p)
    for i, ln in enumerate(open(p, encoding="utf-8").read().splitlines(), 1):
        if jp.search(ln) and not prot.search(ln):
            print(i, ":", ln.rstrip())
