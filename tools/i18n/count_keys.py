import json

def leaves(d, p=""):
    if isinstance(d, dict):
        r = []
        for k, v in d.items():
            r += leaves(v, (p + "." + k) if p else k)
        return r
    return [(p, d)]

e = leaves(json.load(open("Artifact/translations/en.json")))
j = leaves(json.load(open("Artifact/translations/ja.json")))
print("en leaves", len(e))
print("ja leaves", len(j))
print("render keys:", [p for p, _ in e if "render" in p.lower()])
print("menu. keys sample:", [p for p, _ in e if p.startswith("menu.") and "render" in p.lower()][:20])
