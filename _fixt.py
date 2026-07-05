#!/usr/bin/env python3
import re
p="X:/dev/ArtifactStudio/Artifact/include/Widgets/Dialog/ArtifactScreenshotExportDialog.ixx"
with open(p,'r') as f: c=f.read()
c=c.replace("}; void setMultiChannelEnabled(bool enabled);","};")
c=re.sub(r'  ScreenshotCaptureSource captureSource = ScreenshotCaptureSource::Renderer;\n};','  ScreenshotCaptureSource captureSource = ScreenshotCaptureSource::Renderer;\n  bool multiChannel = false;\n};',c)
c=re.sub(r'  \[\[nodiscard\]\] ScreenshotExportOptions options\(\) const;\n};','  [[nodiscard]] ScreenshotExportOptions options() const;\n  void setMultiChannelEnabled(bool enabled);\n  [[nodiscard]] bool multiChannelEnabled() const;\n};',c)
with open(p,'w') as f: f.write(c)
print("OK")