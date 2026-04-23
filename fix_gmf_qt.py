import re
import os
import sys

SKIP_FILES = {
    r'X:\dev\artifactstudio\ArtifactCore\src\Application\ArtifactAppSettings.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Asset\AssetImporter.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Asset\AssetDatabase.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Asset\AbstractAssetFile.cppm',
}

# Normalize skip paths to lowercase for comparison
SKIP_FILES_LOWER = {p.lower() for p in SKIP_FILES}

QT_PATTERN = re.compile(r'^#include\s*[<\"](Q[^>\"]*|wobject[^>\"]*)[>\"]')

def fix_cppm_impl_unit(path):
    try:
        with open(path, 'r', encoding='utf-8-sig') as f:
            content = f.read()
    except Exception as e:
        print(f"  ERROR reading {path}: {e}")
        return False
    
    lines = content.split('\n')
    
    # Find "module X;" line (implementation unit declaration)
    # Must match "^module \S" but NOT "^export module" and NOT "^module;" (bare GMF marker)
    impl_decl_idx = None
    for i, line in enumerate(lines):
        stripped = line.strip()
        if re.match(r'^module\s+\S', stripped) and not re.match(r'^export\s+module', stripped):
            impl_decl_idx = i
            break
    
    if impl_decl_idx is None:
        return False  # Not an implementation unit
    
    # Find Qt/wobject includes in GMF (lines 0..impl_decl_idx-1)
    gmf_qt_lines = []
    non_qt_gmf_lines = []
    for line in lines[:impl_decl_idx]:
        if QT_PATTERN.match(line):
            gmf_qt_lines.append(line)
        else:
            non_qt_gmf_lines.append(line)
    
    if not gmf_qt_lines:
        return False  # Nothing to fix
    
    # Rebuild: non-Qt GMF, then impl decl, then Qt headers, then rest
    purview_lines = lines[impl_decl_idx + 1:]
    new_lines = non_qt_gmf_lines + [lines[impl_decl_idx]] + gmf_qt_lines + purview_lines
    new_content = '\n'.join(new_lines)
    
    try:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(new_content)
    except Exception as e:
        print(f"  ERROR writing {path}: {e}")
        return False
    return True


def fix_ixx_interface_unit(path):
    try:
        with open(path, 'r', encoding='utf-8-sig') as f:
            content = f.read()
    except Exception as e:
        print(f"  ERROR reading {path}: {e}")
        return False
    
    lines = content.split('\n')
    
    # Find "export module X;" line
    export_decl_idx = None
    for i, line in enumerate(lines):
        stripped = line.strip()
        if re.match(r'^export\s+module\s+\S', stripped):
            export_decl_idx = i
            break
    
    if export_decl_idx is None:
        return False
    
    gmf_qt_lines = []
    non_qt_gmf_lines = []
    for line in lines[:export_decl_idx]:
        if QT_PATTERN.match(line):
            gmf_qt_lines.append(line)
        else:
            non_qt_gmf_lines.append(line)
    
    if not gmf_qt_lines:
        return False
    
    purview_lines = lines[export_decl_idx + 1:]
    new_lines = non_qt_gmf_lines + [lines[export_decl_idx]] + gmf_qt_lines + purview_lines
    new_content = '\n'.join(new_lines)
    
    try:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(new_content)
    except Exception as e:
        print(f"  ERROR writing {path}: {e}")
        return False
    return True


CPPM_FILES = [
    r'X:\dev\artifactstudio\ArtifactCore\src\Codec\MFFrameExtractor.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Command\LambdaCommand.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Codec\FFMpegVideoDecoder.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Video\Stabilizer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Codec\FFmpegThumbnailExtractor.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Video\PlaybackManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Codec\FFMpegAudioDecoder.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Codec\EncoderSetting.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Network\NetworkRPCServer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\GPU\GPUInfo.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\EnvironmentVariable\EnvironmentVariable.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Asset\AssetManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Property\PropertyLinkManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Mesh\Mesh.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Property\AbstractProperty.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Diagnostics\Logger.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Color\XYZColor.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Diagnostics\CrashHandler.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Color\LabColor.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Geometry\MeshImporter.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Color\FloatColor.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Preview\PreviewQuality.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Utils\UniString.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Geometry\LayerBoundsCalculator.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Utils\Tag.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Color\ColorLUT.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Geometry\LayerAlignment.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Transform\ViewportTransformer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\WASAPIBackend.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Color\ColorHarmonizer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Geometry\Fracture.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Playback\PlaybackClock.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Utils\MulitpleTag.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\SimpleWav.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Geometry\BezierPathSampler.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaSource.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Utils\Id.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\QtAudioBackend.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Geometry\BezierCalculator.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaReader.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Utils\HashValue.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioWriter.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Utils\ExplorerUtils.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Composition\CompositionBuffer2D.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaProbe.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Composition\PreCompose.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Utils\AssetFingerprint.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioVolume.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Transform\StaticTransform2D.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Core\Opacity.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaPlaybackController.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioRingBuffer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Platform\ShellUtils.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Core\AspectRatio.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaInfo.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioRenderer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Core\KeyFrame.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioRasterizer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Common\Point2D.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaImageFrameDecoder.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Transform\Scale2D.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Core\FastSettingsStore.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioPanner.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Analyze\SmartPaletteAnalyzer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\File\FileTypeDetector.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Control\ArtifactExternalControlManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Frame\FrameRange.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioMixer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaFrame.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioDownMixer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\UI\ViewOrientationNavigator.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaEditorController.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\UI\SelectionManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Material\Material.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioDecibels.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\UI\RotoMaskEditor.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\MediaAudioDecoder.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Tracking\MotionTracker.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioCompressor.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\UI\LayoutState.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Physics\PhysicsSystem.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Media\ImageSequenceSource.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioCache.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Frame\FrameRate.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioBus.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\UI\InputOperator.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\AudioAnalyzer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Mask\RotoMask.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Audio\ASIOBackendStub.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\3DLightingDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\AIAnalysisDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\AnimationUIDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Time\TimeRemap.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Scene\SceneNode.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\CoreDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\IO\Image\ImageImporter.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Reactive\ReactiveEvents.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Time\TimeCode.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\DescriptionExamples.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\IO\Image\ImageExporter.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Rig\Rig2D.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Particle\ParticleSystem.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Render\RendererQueueManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\DataAssetDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\Compute.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\IO\asio_async_file_writer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\FinalUtilitiesDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Render\RenderJobModel.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Render\RenderStatics.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Render\RenderWorker.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\ExportImportDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Source\ISource.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\GPUComputeContext.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\EffectsUtilityDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\GPUTextureCacheManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\LayerBlendPipeline.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\LlamaLocalAgent.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\LayerDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\ParticleCompute.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\ImageMathDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\UIWidgetsDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\FluidMaskingDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\TransitionsGeneratorDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\OnnxDmlLocalAgent.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\TieredAIManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Script\Engine\Enviroment\EnvironmentManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\PSDDocument.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\ObjectDetector.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\RenderAudioColorDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Text\TextAnimator.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\Texture\TextureManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\AI\MoreDescriptions.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Script\Engine\Func\BuiltinManager.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Text\GlyphLayout.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\PSOCache.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\CvMatPaintEngine.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\FFmpegAudioEncoder.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\ParticleRenderer.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\FFmpegEncoder.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\ImageF32x4_RGBA.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\ImageProcessing\SharpenDirectionalBlur.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\ImageF32x4_With_Cache.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\FFmpegEncoder.Test.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\Shader\BasicGeometryShader.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\ImageYUV420.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Image\FFmpegEncoder.Helpers.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\Shader\BasicShaders.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\Shader\BasicVertexShader.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\ImageProcessing\ColorTransform\LevelsCurves.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\Graphics\Shader\Compute\MaskCutoutPipeline.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\ImageProcessing\DirectCompute\GlowCS.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\ImageProcessing\DirectCompute\NegateCS.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\ImageProcessing\OpenCV\FaceDetectionEngine.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\ImageProcessing\OpenCV\FaceTracker.cppm',
    r'X:\dev\artifactstudio\ArtifactCore\src\ImageProcessing\OpenCV\SpectralGlowCV.cppm',
    r'X:\dev\artifactstudio\Artifact\src\AI\AIClient.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Audio\ArtifactAudioMixer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\AppMain.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Service\ArtifactToolService.cppm',
    r'X:\dev\artifactstudio\Artifact\src\LOD\ArtifactLODManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\WhiteBalanceEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Localization\Localization.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Composition\ArtifactAbstractComposition.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactExpressionCopilotWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactCurveEditorWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\Blur\BlurEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactCompositionGraphWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\AutoMosaicEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactCompositionAudioMixerWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Color\ColorPaletteManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\WebUI\ArtifactWebUIHost.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Asset\AssetMenuModel.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactVideoLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\ArtifactAbstractEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Application\ActiveContextService.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Audio\ArtifactAudioWaveform.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Composition\MetadataVectorizer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Core\SystemStats.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\LiftGammaGainEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Service\ArtifactPlaybackShortcuts.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\DirectionalGlowEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Asset\AssetDirectoryModel.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\WebUI\ArtifactWebBridge.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactColorPaletteWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\ColorCorrection\HueAndSaturation.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\ColorCorrection\ExposureEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactAlignmentWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\ColorCorrection\BrightnessEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effect\ArtifactCornerPinEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Color\ArtifactColorWheels.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\AIChatWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Timeline\ArtifactTimelineScrubBar.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Timeline\ArtifactTimeCodeWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactTextLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Timeline\ArtifactLayerHierarchyWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Composition\ArtifactInOutPoints.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Service\ArtifactPlaybackService.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactSvgLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effetcs\Keying\ChromaKeyEffect.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactSolidImageLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Service\ApplicationService.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactSolid2DLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactShapeLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Color\ArtifactColorSettings.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Test\ArtifactScrollPoC.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Color\ArtifactColorScienceManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Test\ArtifactTestRenderQueue.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\SpectrumAnalyzerWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effect\ArtifactEffectPreset.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effect\ArtifactFilmEffects.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactTimelineScene.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactCompositionLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactTimelineGlobalSwitches.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactSecondaryPreviewWindow.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactCloneLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effect\ArtifactTransition.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactCameraLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Composition\ArtifactCompositionPlaybackController.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactMessageBox.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactAudioLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\Artifact3DModelLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Script\ArtifactPythonHookManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactMenuBar.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Undo\UndoManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Color\ArtifactColorNodeGraph.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactPythonHookManagerWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactParticleLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactAbstractLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactLightLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effect\ArtifactStabilizer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dock\DockStyleManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Effect\ArtifactMotionBlur.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dock\DockGlowStyle.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Generator\AbstractGeneratorEffector.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactImageLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\TransformGizmo.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactTimelineLayerTestWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Composition\ArtifactCompositionInitParams.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dialog\PrecomposeDialog.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactTextGizmo.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dialog\CreatePlaneLayerDialog.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dialog\CreateCameraLayerDialog.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactHierarchyModel.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Generator\CloneGenerator.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dialog\ColorSwatchDialog.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactMarkdownNoteEditorWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactSoftwareRenderTestWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactGroupLayer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dialog\ArtifactRenderOutputSettingDialog.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactLayerFactory.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dialog\ArtifactCreateCompositionDialog.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactCompositionViewDrawing.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactMainWindow.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Dialog\ApplicationSettingDialog.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactSoftwareRenderInspectors.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Layer\ArtifactLayerSelectionManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Generator\ParticleEmitterDescription.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactLooksPresetBrowser.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactRenderQueuePresetSelector.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactHDRMonitor.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Diagnostics\ArtifactDebugConsoleWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Color\ArtifactColorNode.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\Software\ArtifactSoftwareImageCompositor.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactRenderManagerWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ShaderManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Playback\ArtifactPlaybackEngine.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactPropertyWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\PrimitiveRenderer3D.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactRenderLayerWidgetv2.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\PrimitiveRenderer2D.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\GPUTextureCacheManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Generator\ArtifactParticleGenerator.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\DiligentDeviceManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectExporter.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectCleanupTool.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProject.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactFontPickerWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactObjectReferenceWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\TreeFilterProxyModel.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactObjectPickerDialog.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactFrameCache.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Color\ArtifactColorManagement.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Color\ArtifactColorGradingEngine.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactIRenderer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactPresetManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactInspectorWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactOffscreenRenderer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactRenderScheduler.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactOffscreenRenderer2D.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectItems.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectStatistics.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactProjectHealthDashboard.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactRenderManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactRenderLayerEditor.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactRenderQueuePresets.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectSetting.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactRenderCenterWindow.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Control\ArtifactPlaybackControlWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactPieMenuWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactToolOptionsBar.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectPackager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactLayerCompositeTestWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Asset\ArtifactAssetBrowser.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactToolBar.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Mask\MaskPath.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\CommonStyle.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactCompositionRenderWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactPerformanceProfilerWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectHealthChecker.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\AudioPreviewWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Preview\ArtifactTimelineClock.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Preview\ArtifactPreviewWorker.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\AudioMixerWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Preview\ArtifactPreviewController.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactProjectManagerWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\PropertyEditor\ArtifactPropertyEditor.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Color\ArtifactColorSciencePanel.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Color\ArtifactColorSwatchWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\Artifact3DGizmo.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ReactiveEventEditorWindow.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectInitParams.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactCompositionEditor.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectManager.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectModel.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Project\ArtifactProjectImporter.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\PowerShellWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\ArtifactUndoHistoryWidget.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactAnimationMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactRenderLayerPipeline.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactCompositionMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Preview\ArtifactPreviewCompositionPipeline.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactEditMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactEffectMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactFileMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactHelpMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactLayerMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Render\ArtifactRenderQueueService.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactOptionMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactScriptMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactRenderMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\ArtifactCompositionRenderController.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactTestMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Render\Artifact3DModelViewer.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactTimeMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\ArtifactViewMenu.cppm',
    r'X:\dev\artifactstudio\Artifact\src\Widgets\Menu\Test\ArtifactImageProcessingTestMenu.cppm',
]

IXX_FILES = [
    r'X:\dev\artifactstudio\Artifact\src\Generator\CloneGenerator.ixx',
]


def main():
    fixed = []
    skipped = []
    not_impl = []
    no_qt = []

    for path in CPPM_FILES:
        if path.lower() in SKIP_FILES_LOWER:
            skipped.append(path)
            continue
        if not os.path.exists(path):
            print(f"  NOT FOUND: {path}")
            continue
        result = fix_cppm_impl_unit(path)
        if result:
            fixed.append(path)
            print(f"  FIXED: {path}")
        else:
            try:
                with open(path, 'r', encoding='utf-8-sig') as f:
                    content = f.read()
                lines = content.split('\n')
                impl_decl_idx = None
                for i, line in enumerate(lines):
                    stripped = line.strip()
                    if re.match(r'^module\s+\S', stripped) and not re.match(r'^export\s+module', stripped):
                        impl_decl_idx = i
                        break
                if impl_decl_idx is None:
                    not_impl.append(path)
                else:
                    no_qt.append(path)
            except Exception:
                pass

    for path in IXX_FILES:
        if not os.path.exists(path):
            print(f"  NOT FOUND: {path}")
            continue
        result = fix_ixx_interface_unit(path)
        if result:
            fixed.append(path)
            print(f"  FIXED (ixx): {path}")
        else:
            no_qt.append(path)

    print(f"\n=== SUMMARY ===")
    print(f"Fixed: {len(fixed)}")
    print(f"Skipped (already fixed): {len(skipped)}")
    print(f"Not implementation units (skipped): {len(not_impl)}")
    print(f"No Qt in GMF (no change needed): {len(no_qt)}")

    if fixed:
        print(f"\nFixed files ({len(fixed)}):")
        for f in fixed:
            print(f"  {f}")

    return 0

if __name__ == '__main__':
    sys.exit(main())
