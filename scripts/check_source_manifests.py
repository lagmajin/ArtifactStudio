#!/usr/bin/env python3
"""Validate explicit source manifests and focused Artifact pack ownership."""

from __future__ import annotations

import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".ixx", ".cppm", ".cpp"}
MANIFEST_PATH_RE = re.compile(
    r'\$\{CMAKE_CURRENT_LIST_DIR\}/\.\./(?P<path>[^"\r\n]+)'
)
ARTIFACT_PACK_SET_RE = re.compile(
    r"set\((?P<name>ARTIFACT_EFFECTS_[A-Z0-9_]+_(?:MODULES|IMPL))\s+(?P<body>.*?)\n\)",
    re.DOTALL,
)
ARTIFACT_PACK_PATH_RE = re.compile(r'"(?P<path>\$\{CMAKE_CURRENT_SOURCE_DIR\}/[^"\r\n]+)"')
MODULE_INTERFACE_RE = re.compile(r"\bexport\s+module\s+(?P<name>[\w.]+)\s*;")
MODULE_IMPLEMENTATION_RE = re.compile(r"(?m)^\s*module\s+(?P<name>[\w.]+)\s*;")
TARGET_LINK_RE = re.compile(
    r"target_link_libraries\((?P<target>[A-Za-z0-9_]+)(?P<body>.*?)\)",
    re.DOTALL,
)
ARTIFACT_EFFECT_SET_RE = re.compile(
    r"set\((?P<name>ARTIFACT_EFFECTS_[A-Z0-9_]+)\s*(?P<body>.*?)\n\)",
    re.DOTALL,
)
ARTIFACT_EFFECT_REMOVE_RE = re.compile(
    r"list\(REMOVE_ITEM\s+(?P<name>ARTIFACT_EFFECTS_(?:MODULES|IMPL))(?P<body>.*?)\)",
    re.DOTALL,
)
ARTIFACTCORE_PACK_SET_RE = re.compile(
    r"set\((?P<name>ARTIFACTCORE_[A-Z0-9_]+_(?:MODULES|IMPL))(?P<body>[^)]*)\)",
    re.DOTALL,
)
ARTIFACT_BASE_PACK_SETS = {
    "ARTIFACT_EFFECTS_MODULES",
    "ARTIFACT_EFFECTS_IMPL",
    "ARTIFACT_EFFECTS_SPATIAL_MODULES",
    "ARTIFACT_EFFECTS_SPATIAL_IMPL",
    "ARTIFACT_EFFECTS_RASTERIZER_MODULES",
    "ARTIFACT_EFFECTS_RASTERIZER_IMPL",
}


@dataclass(frozen=True)
class ManifestSpec:
    name: str
    root: Path
    manifest: Path
    excluded: tuple[re.Pattern[str], ...]


ARTIFACT_EXCLUDED = tuple(
    re.compile(pattern)
    for pattern in (
        r"\.bak$",
        r"src/Audio/Effects/AudioEffectsInit\.cpp$",
        r"src/Test/ArtifactTestRenderQueue\.cppm$",
        r"src/Generator/CloneGenerator\.ixx$",
        r"include/Effects/RadialBlur/RadialBlurEffect\.ixx$",
        r"src/Effects/RadialBlur/RadialBlurEffect\.cppm$",
        r"src/Composition/ArtifactCompositionManager\.cpp$",
        r"include/Effect/ArtifactStabilizer\.ixx$",
        r"src/Effect/ArtifactStabilizer\.cppm$",
        r"include/Render/ArtifactFrameCache\.ixx$",
        r"src/Render/ArtifactFrameCache\.cppm$",
        r"include/Layer/ArtifactLayerGroup\.ixx$",
        r"src/Layer/ArtifactLayerGroup\.cppm$",
        r"include/Color/ArtifactColorWheels\.ixx$",
        r"src/Color/ArtifactColorWheels\.cppm$",
        r"include/Preview/ArtifactTimelineClock\.ixx$",
        r"src/Preview/ArtifactTimelineClock\.cppm$",
        r"include/Image/ImageF32x4_RGBA\.ixx$",
        r"include/Test/ArtifactTestProjectService\.ixx$",
        r"src/Worker/",
        r"Widgets/CommandPalette/",
    )
)

ARTIFACT_CORE_EXCLUDED = tuple(
    re.compile(pattern)
    for pattern in (
        r"stashed_broken_modules",
        r"disabled_modules",
        r"/Halide/",
        r"hostfxr",
        r"\.bak$",
        r"src/ImageProcessing/OpenCV/VignetCV\.ixx$",
        r"src/ImageProcessing/OpenCV/HalfTone\.ixx$",
        r"src/ImageProcessing/OpenCV/FilmGrain\.cppm$",
        r"include/ImageProcessing/OpenCV/G-API/",
        r"include/Image/Blur/BlurGAPI\.cpp$",
        r"include/XR/OpenXR\.ixx$",
        r"src/Diagnostics/ValidationRules\.cppm$",
        r"src/Animation/EasingCurveUtil\.cppm$",
        r"src/Graphics/Compute/(CurveComputer|DuotoneComputer|EchoBlendComputer|EdgeEchoComputer|HalftoneComputer|Histogram|ScopeComputer)\.cppm$",
        r"SpinTransition\.cppm$",
        r"src/ImageProcessing/AnamorphicFlare\.cppm$",
        r"include/Diagnostics/ValidationRules\.ixx$",
        r"include/Asset/AsseetImporter\.ixx$",
        r"src/Image/FFmpegEncoder\.Test\.cppm$",
        r"src/Render/SoftwareRayTracer\.Test\.cppm$",
    )
)

SPECS = (
    ManifestSpec(
        "Artifact",
        ROOT / "Artifact",
        ROOT / "Artifact" / "cmake" / "ArtifactSources.cmake",
        ARTIFACT_EXCLUDED,
    ),
    ManifestSpec(
        "ArtifactCore",
        ROOT / "ArtifactCore",
        ROOT / "ArtifactCore" / "cmake" / "ArtifactCoreSources.cmake",
        ARTIFACT_CORE_EXCLUDED,
    ),
)


def is_excluded(path: str, patterns: tuple[re.Pattern[str], ...]) -> bool:
    return any(pattern.search(path) for pattern in patterns)


def manifest_paths(spec: ManifestSpec) -> list[str]:
    text = spec.manifest.read_text(encoding="utf-8-sig")
    return [match.group("path") for match in MANIFEST_PATH_RE.finditer(text)]


def source_paths(spec: ManifestSpec) -> set[str]:
    paths: set[str] = set()
    for directory in (spec.root / "include", spec.root / "src"):
        if not directory.exists():
            continue
        for path in directory.rglob("*"):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            relative = path.relative_to(spec.root).as_posix()
            if not is_excluded(relative, spec.excluded):
                paths.add(relative)
    if spec.name == "ArtifactCore":
        for filename in ("NetworkRPCClient.ixx", "NetworkRPCServer.ixx"):
            path = spec.root / filename
            if path.is_file():
                paths.add(filename)
    return paths


def artifact_pack_sources() -> list[str]:
    """Return focused Artifact effect-pack module and implementation paths."""

    cmake = (ROOT / "Artifact" / "CMakeLists.txt").read_text(encoding="utf-8-sig")
    packs: dict[str, list[str]] = {}
    for match in ARTIFACT_PACK_SET_RE.finditer(cmake):
        name = match.group("name")
        if name in ARTIFACT_BASE_PACK_SETS:
            continue
        packs[name] = [
            path.replace("${CMAKE_CURRENT_SOURCE_DIR}/", "")
            for path in ARTIFACT_PACK_PATH_RE.findall(match.group("body"))
        ]

    problems: list[str] = []
    module_owners: dict[str, str] = {}
    impl_owners: dict[str, str] = {}
    named_module_owners: dict[str, str] = {}
    for name, paths in packs.items():
        owner = name.removesuffix("_MODULES").removesuffix("_IMPL")
        if name.endswith("_MODULES"):
            for path in paths:
                previous = module_owners.setdefault(path, owner)
                if previous != owner:
                    problems.append(f"Artifact: focused pack module overlap {path} ({previous}, {owner})")
        else:
            for path in paths:
                previous = impl_owners.setdefault(path, owner)
                if previous != owner:
                    problems.append(f"Artifact: focused pack implementation overlap {path} ({previous}, {owner})")

    owners = sorted({name.removesuffix("_MODULES").removesuffix("_IMPL") for name in packs})
    focused_targets: set[str] = set()
    for owner in owners:
        suffix = owner.removeprefix("ARTIFACT_EFFECTS_")
        target_suffix = "SurfaceFX" if suffix == "SURFACEFX" else "".join(part.title() for part in suffix.split("_"))
        focused_targets.add(f"ArtifactEffects{target_suffix}")

    for owner in owners:
        modules = packs.get(owner + "_MODULES", [])
        implementations = packs.get(owner + "_IMPL", [])
        if len(modules) != len(implementations):
            problems.append(
                f"Artifact: focused pack {owner} has {len(modules)} modules and {len(implementations)} implementations"
            )
        for path in modules + implementations:
            if not (ROOT / "Artifact" / path).is_file():
                problems.append(f"Artifact: focused pack source does not exist {path}")

        interface_names: dict[str, str] = {}
        implementation_names: dict[str, str] = {}
        for path in modules:
            source = ROOT / "Artifact" / path
            if not source.is_file():
                continue
            match = MODULE_INTERFACE_RE.search(source.read_text(encoding="utf-8-sig"))
            if not match:
                problems.append(f"Artifact: focused pack interface has no exported module {path}")
                continue
            module_name = match.group("name")
            interface_names[module_name] = path
            previous = named_module_owners.setdefault(module_name, owner)
            if previous != owner:
                problems.append(
                    f"Artifact: focused pack module-name overlap {module_name} ({previous}, {owner})"
                )
        for path in implementations:
            source = ROOT / "Artifact" / path
            if not source.is_file():
                continue
            match = MODULE_IMPLEMENTATION_RE.search(source.read_text(encoding="utf-8-sig"))
            if not match:
                problems.append(f"Artifact: focused pack implementation has no module declaration {path}")
                continue
            implementation_names[match.group("name")] = path

        if set(interface_names) != set(implementation_names):
            problems.append(
                f"Artifact: focused pack {owner} module-name mismatch "
                f"(interfaces={sorted(interface_names)}, implementations={sorted(implementation_names)})"
            )

        suffix = owner.removeprefix("ARTIFACT_EFFECTS_")
        target_suffix = "SurfaceFX" if suffix == "SURFACEFX" else "".join(part.title() for part in suffix.split("_"))
        target = f"ArtifactEffects{target_suffix}"
        if not re.search(rf"add_library\({re.escape(target)}\s+STATIC\)", cmake):
            problems.append(f"Artifact: focused pack target missing {target}")
        target_sources = re.search(
            rf"target_sources\({re.escape(target)}\b(?P<body>.*?)\)",
            cmake,
            re.DOTALL,
        )
        if not target_sources:
            problems.append(f"Artifact: focused pack target_sources missing {target}")
        else:
            body = target_sources.group("body")
            if "PUBLIC FILE_SET CXX_MODULES FILES" not in body:
                problems.append(f"Artifact: {target} target_sources has no public CXX module file set")
            if not re.search(r"\bPRIVATE\b", body):
                problems.append(f"Artifact: {target} target_sources has no private implementation section")
            for kind in ("MODULES", "IMPL"):
                variable = f"${{{owner}_{kind}}}"
                if variable not in body:
                    problems.append(f"Artifact: {target} target_sources missing {variable}")

    effect_sets = {
        match.group("name"): {
            path.replace("${CMAKE_CURRENT_SOURCE_DIR}/", "")
            for path in ARTIFACT_PACK_PATH_RE.findall(match.group("body"))
        }
        for match in ARTIFACT_EFFECT_SET_RE.finditer(cmake)
    }
    for kind, app_variable in (("MODULES", "APP_MODULES"), ("IMPL", "APP_IMPL")):
        base_name = f"ARTIFACT_EFFECTS_{kind}"
        focused_paths = {
            path
            for name, paths in packs.items()
            if name.endswith("_" + kind)
            for path in paths
        }
        missing_from_base = sorted(focused_paths - effect_sets.get(base_name, set()))
        for path in missing_from_base:
            problems.append(
                f"Artifact: focused pack source is not represented in {base_name} {path}"
            )
        if not re.search(
            rf"list\(REMOVE_ITEM\s+{app_variable}\s+\$\{{{base_name}\}}\s*\)",
            cmake,
        ):
            problems.append(f"Artifact: {app_variable} is not cleared by {base_name}")

    removed_from_base = {"ARTIFACT_EFFECTS_MODULES": set(), "ARTIFACT_EFFECTS_IMPL": set()}
    for match in ARTIFACT_EFFECT_REMOVE_RE.finditer(cmake):
        target = match.group("name")
        body = match.group("body")
        removed_from_base[target].update(
            path.replace("${CMAKE_CURRENT_SOURCE_DIR}/", "")
            for path in ARTIFACT_PACK_PATH_RE.findall(body)
        )
        for variable in re.findall(r"\$\{(ARTIFACT_EFFECTS_[A-Z0-9_]+)\}", body):
            removed_from_base[target].update(effect_sets.get(variable, set()))
    for target, base_paths in (
        ("ARTIFACT_EFFECTS_MODULES", effect_sets.get("ARTIFACT_EFFECTS_MODULES", set())),
        ("ARTIFACT_EFFECTS_IMPL", effect_sets.get("ARTIFACT_EFFECTS_IMPL", set())),
    ):
        unowned = sorted(base_paths - removed_from_base[target])
        for path in unowned:
            problems.append(f"Artifact: base effect source is not removed into a focused/residual owner {path}")

    graph: dict[str, set[str]] = {}
    for match in TARGET_LINK_RE.finditer(cmake):
        target = match.group("target")
        linked = {
            token
            for token in re.findall(r"\bArtifactEffects[A-Za-z0-9_]*\b", match.group("body"))
            if token != target
        }
        graph.setdefault(target, set()).update(linked)

    for target in sorted(focused_targets):
        link_bodies = [
            match.group("body")
            for match in TARGET_LINK_RE.finditer(cmake)
            if match.group("target") == target
        ]
        direct_libraries = {
            token
            for token in re.findall(r"\bArtifact[A-Za-z0-9_]+\b", " ".join(link_bodies))
            if token != target
        }
        for required in ("ArtifactCore", "ArtifactRender", "ArtifactEffectContract"):
            if required not in direct_libraries:
                problems.append(f"Artifact: focused pack {target} is missing direct dependency {required}")

    umbrella_expectations = {
        "ArtifactEffectsSpatial": {
            "ArtifactEffectsKeying",
            "ArtifactEffectsBlur",
            "ArtifactEffectsGenerate",
            "ArtifactEffectsDistort",
            "ArtifactEffectsStylize",
            "ArtifactEffectsGlow",
            "ArtifactEffectsOptics",
            "ArtifactEffectsWave",
            "ArtifactEffectsFilters",
            "ArtifactEffectsNoise",
            "ArtifactEffectsAutoMosaic",
        },
        "ArtifactEffectsRasterizer": {
            "ArtifactEffectsMotion",
            "ArtifactEffectsDigital",
            "ArtifactEffectsPatterns",
            "ArtifactEffectsChromatic",
            "ArtifactEffectsShadows",
            "ArtifactEffectsContextual",
            "ArtifactEffectsTemporalContext",
            "ArtifactEffectsFinishing",
        },
        "ArtifactEffectsResidual": focused_targets,
    }
    for umbrella, expected in umbrella_expectations.items():
        missing = sorted(expected - graph.get(umbrella, set()))
        for target in missing:
            problems.append(f"Artifact: umbrella {umbrella} is missing focused pack {target}")

    reachable: set[str] = set()
    pending = ["Artifact"]
    while pending:
        target = pending.pop()
        if target in reachable:
            continue
        reachable.add(target)
        pending.extend(graph.get(target, ()))
    for target in sorted(focused_targets - reachable):
        problems.append(f"Artifact: focused pack target is unreachable from Artifact {target}")
    return problems


def artifact_core_pack_wiring() -> list[str]:
    """Validate ArtifactCore pack source sets and target registration."""

    cmake = (ROOT / "ArtifactCore" / "CMakeLists.txt").read_text(encoding="utf-8-sig")
    packs: dict[str, set[str]] = {}
    for match in ARTIFACTCORE_PACK_SET_RE.finditer(cmake):
        name = match.group("name")
        if name in {"ARTIFACTCORE_MODULES", "ARTIFACTCORE_IMPL"}:
            continue
        owner, kind = name.rsplit("_", 1)
        packs.setdefault(owner, set()).add(kind)

    special_suffixes = {
        "AI": "AI",
        "COLORCOLLECTION": "ColorCollection",
        "FILESYSTEM": "FileSystem",
        "IPC": "IPC",
        "NLE": "NLE",
        "UI": "UI",
        "VST": "VST",
        "VST3": "VST3",
    }
    problems: list[str] = []
    core_targets: set[str] = set()
    module_owners: dict[tuple[str, str], tuple[str, str]] = {}
    for owner, kinds in sorted(packs.items()):
        suffix = owner.removeprefix("ARTIFACTCORE_")
        target = "ArtifactCore" + special_suffixes.get(
            suffix,
            "".join(part.title() for part in suffix.split("_")),
        )
        core_targets.add(target)
        if not re.search(rf"add_library\({re.escape(target)}\s+STATIC\)", cmake):
            problems.append(f"ArtifactCore: pack target missing {target}")
        target_sources = re.search(
            rf"target_sources\({re.escape(target)}\b(?P<body>.*?)\)",
            cmake,
            re.DOTALL,
        )
        if not target_sources:
            problems.append(f"ArtifactCore: pack target_sources missing {target}")
        else:
            body = target_sources.group("body")
            if "PUBLIC FILE_SET CXX_MODULES FILES" not in body:
                problems.append(f"ArtifactCore: {target} target_sources has no public CXX module file set")
            for kind in sorted(kinds):
                variable = f"${{{owner}_{kind}}}"
                if variable not in body:
                    problems.append(f"ArtifactCore: {target} target_sources missing {variable}")

        link_bodies = [
            match.group("body")
            for match in TARGET_LINK_RE.finditer(cmake)
            if match.group("target") == target
        ]
        direct_libraries = set(
            re.findall(r"\bArtifactCore[A-Za-z0-9_]*\b", " ".join(link_bodies))
        )
        if "ArtifactCore" not in direct_libraries:
            problems.append(f"ArtifactCore: pack {target} is missing direct dependency ArtifactCore")

        for match in ARTIFACTCORE_PACK_SET_RE.finditer(cmake):
            if match.group("name").rsplit("_", 1)[0] != owner:
                continue
            for path in ARTIFACT_PACK_PATH_RE.findall(match.group("body")):
                relative = path.replace("${CMAKE_CURRENT_SOURCE_DIR}/", "")
                source = ROOT / "ArtifactCore" / relative
                if not source.is_file():
                    problems.append(f"ArtifactCore: pack source does not exist {relative}")
                    continue
                text = source.read_text(encoding="utf-8-sig")
                module_kind = "interface" if relative.endswith(".ixx") else "implementation"
                module_match = (
                    MODULE_INTERFACE_RE.search(text)
                    if module_kind == "interface"
                    else MODULE_IMPLEMENTATION_RE.search(text)
                )
                if not module_match:
                    continue
                module_name = module_match.group("name")
                module_key = (module_kind, module_name)
                previous = module_owners.setdefault(module_key, (owner, relative))
                if previous != (owner, relative):
                    problems.append(
                        f"ArtifactCore: duplicate {module_kind} module name {module_name} "
                        f"({previous[0]}:{previous[1]}, {owner}:{relative})"
                    )

    graph: dict[str, set[str]] = {}
    for cmake_text in (
        cmake,
        (ROOT / "Artifact" / "CMakeLists.txt").read_text(encoding="utf-8-sig"),
    ):
        for match in TARGET_LINK_RE.finditer(cmake_text):
            target = match.group("target")
            linked = set(re.findall(r"\bArtifactCore[A-Za-z0-9_]*\b", match.group("body")))
            graph.setdefault(target, set()).update(linked - {target})

    visiting: set[str] = set()
    visited: set[str] = set()
    cycle_nodes: set[str] = set()

    def visit(target: str) -> None:
        if target in visiting:
            cycle_nodes.add(target)
            return
        if target in visited:
            return
        visiting.add(target)
        for linked in graph.get(target, ()):
            visit(linked)
        visiting.remove(target)
        visited.add(target)

    for target in graph:
        visit(target)
    for target in sorted(cycle_nodes):
        problems.append(f"ArtifactCore: target link graph cycle detected at {target}")

    reachable: set[str] = set()
    pending = ["Artifact"]
    while pending:
        target = pending.pop()
        if target in reachable:
            continue
        reachable.add(target)
        pending.extend(graph.get(target, ()))
    for target in sorted(core_targets - reachable):
        problems.append(f"ArtifactCore: pack target is unreachable from Artifact {target}")
    return problems


def main() -> int:
    problems: list[str] = []
    for spec in SPECS:
        if not spec.manifest.exists():
            problems.append(f"{spec.name}: missing manifest {spec.manifest}")
            continue
        listed_paths = manifest_paths(spec)
        listed = set(listed_paths)
        on_disk = source_paths(spec)
        duplicate_paths = sorted(
            path for path, count in Counter(listed_paths).items() if count > 1
        )
        missing = sorted(on_disk - listed)
        stale = sorted(listed - on_disk)
        for path in duplicate_paths:
            problems.append(f"{spec.name}: duplicate manifest source {path}")
        for path in missing:
            problems.append(f"{spec.name}: unregistered source {path}")
        for path in stale:
            problems.append(f"{spec.name}: manifest source does not exist {path}")

    problems.extend(artifact_pack_sources())
    problems.extend(artifact_core_pack_wiring())

    if problems:
        print("Source manifest check failed:")
        for problem in problems:
            print(f" - {problem}")
        return 1
    print("Source manifest check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
