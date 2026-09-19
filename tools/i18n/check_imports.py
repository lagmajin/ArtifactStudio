from pathlib import Path

FILES = [
    "Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm",
    "Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm",
    "Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm",
]

for f in FILES:
    c = Path(f).read_text(encoding="utf-8")
    has_import = "import Translation.Manager;" in c
    uses = "TranslationManager::instance()" in c or "menuText(" in c
    print(f"{f}: import={has_import} uses_translation={uses}")
