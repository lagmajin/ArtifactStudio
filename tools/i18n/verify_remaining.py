import json

def get(d, p):
    cur = d
    for x in p.split("."):
        cur = cur.get(x) if isinstance(cur, dict) else None
    return cur

for n in ["en", "ja"]:
    d = json.load(open("Artifact/translations/" + n + ".json", encoding="utf-8"))
    print(n, "valid json")

for k in ["menu.render.start", "dialog.render.added_to_queue",
          "menu.layer.debug_blend_added", "dialog.layer.outer_offset"]:
    print(k, "en=", get(json.load(open("Artifact/translations/en.json")), k)[:40],
          "| ja=", get(json.load(open("Artifact/translations/ja.json")), k)[:40])

# verify the %1 arg chain is intact in source
src = open("Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm", encoding="utf-8").read()
i = src.find("dialog.render.added_to_queue")
print("added_to_queue src snippet:", repr(src[i-10:i+120]))
