import os, re
jp = re.compile(r'[\u3040-\u30ff\u4e00-\u9fff\uff01-\uff5e]')
prot = re.compile(r'menuText\s*\(|tt\s*\(|TranslationManager::instance\(\).tr\(|tr\s*\(')
p = "Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm"
if not os.path.exists(p):
    print("MISSING"); raise SystemExit
translatable = 0
for i, ln in enumerate(open(p, encoding="utf-8").read().splitlines(), 1):
    if jp.search(ln) and not prot.search(ln):
        tag = "COMMENT" if ln.strip().startswith("//") else "BARE-JA"
        if tag == "BARE-JA": translable = True
        print(i, tag, ":", ln.strip()[:80])
        if tag == "BARE-JA": translatable += 1
print("translatable bare-ja:", translatable)
