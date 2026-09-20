import os, re
jp = re.compile(r'[\u3040-\u30ff\u4e00-\u9fff\uff01-\uff5e]')
prot = re.compile(r'menuText\s*\(|tt\s*\(|\.tr\s*\(|tr\s*\(|TranslationManager::instance\(\).tr\(|QObject::tr\(')
prot_any = re.compile(r'menuText\s*\(|tt\s*\(|TranslationManager::instance\(\).tr\(|tr\s*\(')
roots = ["Artifact/src/Widgets/Menu", "Artifact/src/Widgets/Dialog"]
total_protectable = 0
for base in roots:
    if not os.path.isdir(base):
        continue
    for fn in sorted(os.listdir(base)):
        if not fn.endswith(".cppm"):
            continue
        path = os.path.join(base, fn)
        lines = open(path, encoding="utf-8").read().splitlines()
        bare = 0
        for ln in lines:
            if jp.search(ln) and not prot.search(ln):
                bare += 1
        if bare:
            print("%-55s %d bare-ja" % (path.replace("Artifact/src/Widgets/",""), bare))
            total_protectable += bare
print("TOTAL protectable bare-ja in Menu+Dialog:", total_protectable)
