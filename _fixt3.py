import re
p = 'X:/dev/ArtifactStudio/Artifact/include/Widgets/Dialog/ArtifactScreenshotExportDialog.ixx'
with open(p, 'r') as f:
    c = f.read()

# Reset to clean state
c = c.replace("};\n\n}", "};\n\n}")  # already clean

# Add multiChannel field to ScreenshotExportOptions
old1 = '  ScreenshotCaptureSource captureSource = ScreenshotCaptureSource::Renderer;\n};'
new1 = '  ScreenshotCaptureSource captureSource = ScreenshotCaptureSource::Renderer;\n  bool multiChannel = false;          // Multi-channel EXR (AOV)\n};'
c = c.replace(old1, new1)

# Add methods to class
old2 = '  [[nodiscard]] ScreenshotExportOptions options() const;\n};'
new2 = '  [[nodiscard]] ScreenshotExportOptions options() const;\n  void setMultiChannelEnabled(bool enabled);\n  [[nodiscard]] bool multiChannelEnabled() const;\n};'
c = c.replace(old2, new2)

with open(p, 'w') as f:
    f.write(c)
print('done')
