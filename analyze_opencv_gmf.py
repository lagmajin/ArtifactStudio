"""
Analyze .cppm files: find those that use cv:: / CV_ in the module body
but lack #include <opencv2/ in their Global Module Fragment (GMF).
"""
import re
from pathlib import Path

CV_USERS = [
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\Analyze\SmartPaletteAnalyzer.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\Analyze\Histgram.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\AI\ObjectDetector.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\DirectCompute\NegateCS.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\DirectCompute\GlowCS.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\DirectCompute\Blend2D_CS.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\AffineCV.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\Tracking\MotionTracker.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\VHS_CV.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\Glow.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\FaceDetectionEngine.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\Image\FFmpegEncoder.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\SpectralGlowCV.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\Noise.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\GlitchCV.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\Draw.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\SepiaCV.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\Halide\MonochromeHalide.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\FilmGrain.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\OpenCVPuppetEngine.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\Image\FFmpegEncoder.Test.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\Image\ImageF32x4.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\LiftCV.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\Image\ImageF32x4_RGBA.cppm",
    r"X:\Dev\ArtifactStudio\ArtifactCore\src\Image\ImageYUV420.cppm",
]

MODULE_DECL_RE = re.compile(r'^\s*(export\s+)?module\s+\S+\s*;')
CV_USAGE_RE    = re.compile(r'\bcv::|CV_[A-Z_]+')
OPENCV_INC_RE  = re.compile(r'#\s*include\s*[<"]opencv2/')


def analyze(path_str: str):
    p = Path(path_str)
    if not p.exists():
        return None, f"  [MISSING FILE]"

    lines = p.read_text(encoding="utf-8", errors="replace").splitlines()

    # Split into GMF (before module decl) and body (after)
    module_line_idx = None
    for i, ln in enumerate(lines):
        if MODULE_DECL_RE.match(ln):
            module_line_idx = i
            break

    gmf_lines  = lines[:module_line_idx] if module_line_idx is not None else []
    body_lines = lines[module_line_idx + 1:] if module_line_idx is not None else lines

    has_opencv_in_gmf = any(OPENCV_INC_RE.search(l) for l in gmf_lines)

    # Find first cv:: / CV_ usage in body
    first_cv = None
    for rel_i, ln in enumerate(body_lines):
        m = CV_USAGE_RE.search(ln)
        if m:
            abs_line = (module_line_idx + 1 + rel_i + 1) if module_line_idx is not None else rel_i + 1
            first_cv = (abs_line, ln.rstrip())
            break

    return {
        "path": path_str,
        "module_line": module_line_idx + 1 if module_line_idx is not None else None,
        "has_opencv_in_gmf": has_opencv_in_gmf,
        "gmf_preview": lines[:5],
        "first_cv": first_cv,
    }


print("=" * 80)
print("FILES MISSING opencv2 IN GMF  (use cv:: but no #include <opencv2/ before module)")
print("=" * 80)

skipped_has_opencv = []
skipped_no_cv      = []
results            = []

for f in CV_USERS:
    info, err = analyze(f), None
    if isinstance(info, tuple):
        info, err = info
    if info is None:
        print(f"\n[!] {f}\n    {err}")
        continue

    if info["has_opencv_in_gmf"]:
        skipped_has_opencv.append(f)
        continue
    if info["first_cv"] is None:
        skipped_no_cv.append(f)
        continue

    results.append(info)

for r in results:
    print(f"\n{'─'*70}")
    print(f"FILE : {r['path']}")
    print(f"  module declaration at line {r['module_line']}")
    print(f"  GMF has OpenCV include : {r['has_opencv_in_gmf']}")
    print(f"  GMF preview (first 5 lines):")
    for i, ln in enumerate(r["gmf_preview"], 1):
        print(f"    {i:3}: {ln}")
    ln_no, ln_txt = r["first_cv"]
    print(f"  First cv::/CV_ usage  → line {ln_no}: {ln_txt.strip()}")

print(f"\n{'='*80}")
print(f"SUMMARY")
print(f"  Need fix (cv:: but no GMF include) : {len(results)}")
print(f"  Skipped – already has GMF opencv   : {len(skipped_has_opencv)}")
print(f"  Skipped – no cv:: in body          : {len(skipped_no_cv)}")
print()
if skipped_has_opencv:
    print("  Already correct (GMF has opencv):")
    for f in skipped_has_opencv:
        print(f"    {f}")
if skipped_no_cv:
    print("  No cv:: found in body (false positive in input list?):")
    for f in skipped_no_cv:
        print(f"    {f}")
