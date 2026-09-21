from pathlib import Path

c = Path("Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm").read_text(encoding="utf-8")
lines = c.split("\n")
hits = [l.strip() for l in lines if "tr(" in l or "TranslationManager" in l or "menuText" in l or "TranslationHelper" in l]
print("hits:", len(hits))
for l in hits[:30]:
    print(l[:160])
