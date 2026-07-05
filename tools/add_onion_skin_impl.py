import os

cppm_path = r'X:\dev\ArtifactStudio\Artifact\src\Widgets\Render\ArtifactCompositionRenderController.cppm'

with open(cppm_path, 'rb') as f:
    raw = f.read()

# Find the end of isShowCameraFrustumOverlay
marker = b'return impl_->showCameraFrustumOverlay_;\r\r\n}'
idx = raw.find(marker)
if idx < 0:
    # Try \r\r\n (double CRLF)
    marker = b'return impl_->showCameraFrustumOverlay_;\r\r\n}'
    idx = raw.find(marker)
if idx < 0:
    marker = b'return impl_->showCameraFrustumOverlay_;\rr\n}'
    idx = raw.find(marker)
if idx < 0:
    # Try different line ending
    marker = b'return impl_->showCameraFrustumOverlay_;\n}'
    idx = raw.find(marker)

if idx >= 0:
    new_impl = (
        b'return impl_->showCameraFrustumOverlay_;\r\n'
        b'}\r\n'
        b'\r\n'
        b'void CompositionRenderController::setShowOnionSkin(bool show) {\r\n'
        b'  if (impl_->showOnionSkin_ == show) return;\r\n'
        b'  impl_->showOnionSkin_ = show;\r\n'
        b'  impl_->invalidateOverlayComposite();\r\n'
        b'  markRenderDirty();\r\n'
        b'}\r\n'
        b'\r\n'
        b'bool CompositionRenderController::isShowOnionSkin() const {\r\n'
        b'  return impl_->showOnionSkin_;\r\n'
        b'}\r\n'
        b'\r\n'
        b'void CompositionRenderController::setOnionSkinFrameCount(int count) {\r\n'
        b'  impl_->onionSkinFrameCount_ = std::clamp(count, 1, 5);\r\n'
        b'  markRenderDirty();\r\n'
        b'}\r\n'
        b'\r\n'
        b'int CompositionRenderController::onionSkinFrameCount() const {\r\n'
        b'  return impl_->onionSkinFrameCount_;\r\n'
        b'}\r\n'
        b'\r\n'
        b'void CompositionRenderController::setOnionSkinOpacity(int percent) {\r\n'
        b'  impl_->onionSkinOpacity_ = std::clamp(percent, 5, 80);\r\n'
        b'  markRenderDirty();\r\n'
        b'}\r\n'
        b'\r\n'
        b'int CompositionRenderController::onionSkinOpacity() const {\r\n'
        b'  return impl_->onionSkinOpacity_;\r\n'
        b'}\r\n'
        b'\r\n'
    )
    raw = raw[:idx] + new_impl + raw[idx + len(marker):]
    print('Onion skin implementations added')
    
    with open(cppm_path, 'wb') as f:
        f.write(raw)
else:
    print('FAIL - marker not found')
    # Debug: find what's around showCameraFrustumOverlay
    idx2 = raw.find(b'showCameraFrustumOverlay_')
    if idx2 >= 0:
        end = min(len(raw), idx2 + 200)
        print(repr(raw[idx2:end]))
