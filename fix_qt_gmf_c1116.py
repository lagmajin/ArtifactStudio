"""
Fix Qt GMF C1116 issues: move Qt #includes from Global Module Fragment (before
`export module X;`) to module purview (after `export module X;`).
"""
import re
import os

FILES = [
    r"X:\dev\artifactstudio\ArtifactCore\include\Color\ColorLUT.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Icon\SvgToIcon.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Color\ColorHarmonizer.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Shape\ShapeTypes.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\VectorLike.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\StringLike.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\StringConvertor.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\ImageProcessing\OpenCV\CvUtils.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\String.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\SizeLike.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\ImageProcessing\OpenCV\FaceTracker.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\ImageProcessing\OpenCV\FaceDetectionEngine.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\ImageProcessing\ColorTransform\LevelsCurves.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Shape\ShapePath.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\ImageProcessing\DirectCompute\GlowCS.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Color\Grading\ColorScopes.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Shape\ShapeLayer.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\ImageProcessing\OpenCV\G-API\GPUInfoOld.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\MultipleTag.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\Localization.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\ImageProcessing\DirectCompute\NegateCS.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Preview\PreviewQuality.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\Id.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Shape\ShapeGroup.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Scene\SceneNode.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Color\Grading\ColorCurves.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Utils\AssetFingerprint.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Physics\FractureEngine.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Rig\Rig2D.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Mesh\Mesh.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Color\ColorSpace.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Playback\PlaybackClock.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\LayerBlendCS_PSO_Helper.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Reactive\ReactiveEvents.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\GPUInfo.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\GPUComputeContext.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Physics\2D\Physics2D.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Property\PropertyTypes.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\GLMHelper.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Property\PropertyLinkManager.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Tracking\MotionTracker.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Property\PropertyGroup.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Property\Property.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Transform\Rotate.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\Texture\TextureManager.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Render\RendererQueueManager.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\UI\StandardActions.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\ShaderCreationHelper.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Property\AbstractProperty.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\Shader\Vertex\BasicVertexShader.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Transform\StaticTransform2D.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Time\RationalTime.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Render\RenderJobModel.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Time\TimeCode.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Transform\Scale2D.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\UI\SelectionManager.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\MediaAudioDecoder.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\UI\InteractiveActions.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\ISource.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\MediaSource.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\ImageSequenceSource.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\MediaReader.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\UI\InputOperator.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\MotionBlur.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\Shader\HLSL\BasicShaders.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Math\InterpolatorFactory.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\Shader\Compute\MaskCutoutPipeline.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\MediaImageFrameDecoder.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\MediaPlaybackController.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Material\Material.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\Shader\HLSL\BlurShaders.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\Shader\HLSL\GraphicsHelper.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\MediaFrame.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\MediaInfo.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\Shader\HLSL\ColorCorrectionShaders.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Graphics\Shader\Compute\LayerBlendComputeShader.ixx",
    r"X:\dev\artifactstudio\ArtifactCore\include\Media\MediaMetaData.ixx",
]


def is_qt_include(line: str) -> bool:
    """Return True if the line is a Qt #include that must be in module purview."""
    s = line.strip()
    # Qt headers: #include <Q...>, #include "Q...", #include <q...> (e.g. qminmax.h)
    if re.match(r'#include\s*[<"][Qq]', s):
        return True
    # wobject (verdigris/verbump Qt-based)
    if re.match(r'#include\s*[<"]wobject', s):
        return True
    return False


def process_file(filepath: str) -> str:
    """Process one file; return status string."""
    if not os.path.exists(filepath):
        return f"  MISSING: {filepath}"

    with open(filepath, 'rb') as f:
        raw = f.read()

    has_bom = raw.startswith(b'\xef\xbb\xbf')
    has_crlf = b'\r\n' in raw

    text = raw[3:].decode('utf-8') if has_bom else raw.decode('utf-8')
    # Normalise to LF for processing
    text = text.replace('\r\n', '\n')

    # Find `export module X;` line
    m = re.search(r'^export module [^;\n]+;', text, re.MULTILINE)
    if not m:
        return f"  NO export module: {os.path.basename(filepath)}"

    export_start = m.start()
    export_end   = m.end()
    export_line  = m.group(0)

    gmf   = text[:export_start]
    after = text[export_end:]   # everything after `export module X;`

    lines = gmf.split('\n')
    qt_lines  = []
    keep_lines = []
    for line in lines:
        if is_qt_include(line):
            qt_lines.append(line.rstrip('\r'))
        else:
            keep_lines.append(line)

    if not qt_lines:
        return f"  skip (no Qt in GMF): {os.path.basename(filepath)}"

    # Reconstruct: GMF without Qt includes, then export module, then Qt includes
    new_gmf = '\n'.join(keep_lines)
    inserted = '\n'.join(qt_lines)
    new_text = new_gmf + export_line + '\n' + inserted + after

    # Restore original line endings
    if has_crlf:
        new_text = new_text.replace('\n', '\r\n')

    output_bytes = (b'\xef\xbb\xbf' if has_bom else b'') + new_text.encode('utf-8')
    with open(filepath, 'wb') as f:
        f.write(output_bytes)

    return f"  OK ({len(qt_lines)} Qt includes moved): {os.path.basename(filepath)}"


if __name__ == '__main__':
    results = []
    for fp in FILES:
        result = process_file(fp)
        results.append(result)
        print(result)

    ok = sum(1 for r in results if r.strip().startswith('OK'))
    skip = sum(1 for r in results if 'skip' in r)
    err = len(results) - ok - skip
    print(f"\nDone: {ok} fixed, {skip} skipped (no Qt in GMF), {err} errors")
