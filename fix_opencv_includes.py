#!/usr/bin/env python3
"""
Fix .ixx files where #include <opencv2/...> appears AFTER the export module declaration.
Moves those includes (and any wrapping #ifdef USE_OPENCV blocks) to the global module
fragment — i.e., before the 'export module' line.
"""

import re
from pathlib import Path

FILES_TO_FIX = [
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\TestEncoder.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\IO\Image\SearchImage.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Utils\VectorLike.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Utils\SizeLike.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Core\Point2D.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Image\Transform\ImageTransformCV.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Graphics\TextureFactory.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Image\ImageYUV420.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Image\ImageF32x4_RGBA.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Image\ImageF32x4.ixx",
    r"X:\Dev\ArtifactStudio\ArtifactCore\include\Analyze\Histgram.ixx",
    r"X:\Dev\ArtifactStudio\Artifact\include\Effetcs\ArtifactEffectImplBase.ixx",
    r"X:\Dev\ArtifactStudio\Artifact\include\Effetcs\AutoMosaicEffect.ixx",
    r"X:\Dev\ArtifactStudio\Artifact\include\Effetcs\WhiteBalanceEffect.ixx",
    r"X:\Dev\ArtifactStudio\Artifact\include\Image\ImageF32xN.ixx",
    r"X:\Dev\ArtifactStudio\Artifact\include\Image\ImageF32x4_RGBA_Compat.ixx",
    r"X:\Dev\ArtifactStudio\Artifact\include\Effetcs\LiftGammaGainEffect.ixx",
    r"X:\Dev\ArtifactStudio\Artifact\include\Effetcs\DirectionalGlowEffect.ixx",
    r"X:\Dev\ArtifactStudio\Artifact\include\Layer\ArtifactImageLayer.ixx",
]

# Matches any #include <opencv2/...>
_OPENCV_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*<opencv2/')

# Matches the export module declaration (C++20 module interface unit)
_EXPORT_MODULE_RE = re.compile(r'^\s*export\s+module\b')

# Matches #ifdef USE_OPENCV or #if defined(USE_OPENCV)
_IFDEF_OPENCV_RE = re.compile(
    r'^#\s*(ifdef\s+USE_OPENCV|if\s+defined\s*\(\s*USE_OPENCV\s*\))'
)

# Matches any nested conditional directive opener
_IF_OPEN_RE = re.compile(r'^#\s*(ifdef|ifndef|if)\b')

# Matches #endif
_ENDIF_RE = re.compile(r'^#\s*endif\b')


def _is_opencv_include(line: str) -> bool:
    return bool(_OPENCV_INCLUDE_RE.match(line))


def _extract_opencv_blocks(post_export: list[str]) -> tuple[list[str], list[str]]:
    """
    Walk lines that come after 'export module'. Pull out:
      - bare #include <opencv2/...> lines
      - entire #ifdef USE_OPENCV ... #endif blocks (when they contain opencv includes)

    Returns (remaining, extracted) where extracted lines go before export module.
    """
    extracted: list[str] = []
    remaining: list[str] = []

    i = 0
    while i < len(post_export):
        line = post_export[i]
        stripped = line.strip()

        # --- #ifdef USE_OPENCV block ---
        if _IFDEF_OPENCV_RE.match(stripped):
            block: list[str] = [line]
            i += 1
            depth = 1
            while i < len(post_export) and depth > 0:
                bl = post_export[i]
                bs = bl.strip()
                if _IF_OPEN_RE.match(bs):
                    depth += 1
                elif _ENDIF_RE.match(bs):
                    depth -= 1
                block.append(bl)
                i += 1
            # Only move if the block actually has opencv content
            if any(_is_opencv_include(bl) for bl in block):
                extracted.extend(block)
            else:
                remaining.extend(block)
            continue

        # --- bare opencv include ---
        if _is_opencv_include(stripped):
            extracted.append(line)
            i += 1
            continue

        remaining.append(line)
        i += 1

    return remaining, extracted


def fix_file(filepath: str) -> bool:
    """
    Fix one .ixx file. Returns True if the file was actually modified.
    """
    path = Path(filepath)
    if not path.exists():
        print(f"  SKIP (not found): {filepath}")
        return False

    raw = path.read_bytes()
    has_bom = raw.startswith(b'\xef\xbb\xbf')

    try:
        content = raw.decode('utf-8-sig')   # strips BOM if present
    except UnicodeDecodeError:
        content = raw.decode('latin-1')

    lines = content.splitlines(keepends=True)

    # Locate the 'export module' line
    export_idx: int | None = None
    for i, line in enumerate(lines):
        if _EXPORT_MODULE_RE.match(line):
            export_idx = i
            break

    if export_idx is None:
        print(f"  SKIP (no 'export module' found): {filepath}")
        return False

    post_export = lines[export_idx + 1:]

    if not any(_is_opencv_include(l) for l in post_export):
        # Nothing to move
        return False

    remaining_post, extracted = _extract_opencv_blocks(post_export)

    if not extracted:
        return False

    # Reconstruct: global-fragment lines | extracted opencv | export module | rest
    new_lines = lines[:export_idx] + extracted + [lines[export_idx]] + remaining_post
    new_content = "".join(new_lines)

    out_bytes = (b'\xef\xbb\xbf' if has_bom else b'') + new_content.encode('utf-8')

    if out_bytes == raw:
        return False

    path.write_bytes(out_bytes)
    return True


def main() -> None:
    modified: list[str] = []

    for filepath in FILES_TO_FIX:
        name = Path(filepath).name
        print(f"Checking: {name}")
        try:
            if fix_file(filepath):
                modified.append(filepath)
                print(f"  -> FIXED")
        except Exception as exc:
            print(f"  -> ERROR: {exc}")

    print("\n" + "=" * 60)
    if modified:
        print(f"Modified {len(modified)} file(s):")
        for f in modified:
            print(f"  {f}")
    else:
        print("No files needed modification.")

    not_modified = len(FILES_TO_FIX) - len(modified)
    if not_modified:
        print(f"\n{not_modified} file(s) already correct or not found.")


if __name__ == "__main__":
    main()
