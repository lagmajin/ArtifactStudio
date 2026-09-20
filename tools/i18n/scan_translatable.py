import os, re
jp = re.compile(r'[\u3040-\u30ff\u4e00-\u9fff\uff01-\uff5e]')
prot = re.compile(r'menuText\s*\(|tt\s*\(|TranslationManager::instance\(\).tr\(|tr\s*\(')
roots = ["Artifact/src/Widgets/Menu", "Artifact/src/Widgets/Dialog"]
total_code = 0
for base in roots:
    if not os.path.isdir(base):
        continue
    for fn in sorted(os.listdir(base)):
        if not fn.endswith(".cppm"):
            continue
        path = os.path.join(base, fn)
        code = 0
        for raw in open(path, encoding="utf-8").read().splitlines():
            ln = raw
            if not jp.search(ln):
                continue
            if prot.search(ln):
                continue
            # Strip a trailing // comment (naive: files here rarely have // inside strings)
            code_part = ln
            idx = code_part.find("//")
            if idx != -1:
                code_part = code_part[:idx]
            if jp.search(code_part):
                code += 1
        if code:
            print("%-45s %d translatable" % (path.replace("Artifact/src/Widgets/", ""), code))
            total_code += code
print("TOTAL translatable bare-ja (menus+dialogs):", total_code)
