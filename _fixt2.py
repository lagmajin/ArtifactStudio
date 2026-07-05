import re
p = 'X:/dev/ArtifactStudio/Artifact/include/Widgets/Dialog/ArtifactScreenshotExportDialog.ixx'
with open(p, 'r') as f:
    c = f.read()
# Fix corruption
c = c.replace("};  void setMultiChannelEnabled", "};")
c = c.replace("};" + chr(10) + " [[nodiscard]] bool multiChannelEnabled", "};" + chr(10) + chr(10) + "}")
with open(p, 'w') as f:
    f.write(c)
print('fixed')
