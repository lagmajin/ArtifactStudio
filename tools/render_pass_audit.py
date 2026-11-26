#!/usr/bin/env python3
"""ArtifactStudio のマルチパス配線を静的に監査する（読み取り専用）。

ビルド・テスト・ソース変更は一切行わない。以下を検出する:

  1. buildFrameRenderPassPlan() が宣言するフレームパス一覧
  2. FunctionalRenderPass として実際に生成されるパス
  3. 宣言のみで実行ラッパーのないパス（Flush など既知のインライン staging は除外可）
  4. RenderPassExecutor::runWithRenderGraph / runAllWithRenderGraph の呼び出し内訳
  5. RenderGraph の使用形態（診断 / 順序検証 / 実行）
  6. C++ ホスト参照のない休眠シェーダ
  7. 呼び出し元のない RenderGraph アダプタ

使い方:
    python tools/render_pass_audit.py
    python tools/render_pass_audit.py --root C:/dev/ArtifactStudio
    python tools/render_pass_audit.py --json
    python tools/render_pass_audit.py --check   # 未実行パスがあれば exit 1
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Iterable

PASS_KIND_RE = re.compile(r"FrameRenderPassKind::([A-Za-z_]\w*)")
FUNCTIONAL_PASS_RE = re.compile(
    r"FunctionalRenderPass\s+[A-Za-z_]\w*\s*\(\s*FrameRenderPassKind::([A-Za-z_]\w*)\s*,\s*"
    r'QStringLiteral\("([^"]+)"\)'
)
RUN_SINGLE_RE = re.compile(r"RenderPassExecutor::runWithRenderGraph\s*\(")
RUN_MULTI_RE = re.compile(r"RenderPassExecutor::runAllWithRenderGraph\s*\(")
RUN_DIRECT_RE = re.compile(r"RenderPassExecutor::run\s*\(")
RENDER_GRAPH_DECL_RE = re.compile(r"\bRenderGraph\s+([A-Za-z_]\w*)\s*[;(]")
DIAGNOSTIC_SNAPSHOT_RE = re.compile(r"compileDiagnosticSnapshot\s*\(")
GRAPH_COMPILE_RE = re.compile(r"\.compile\s*\(")
GRAPH_EXECUTE_RE = re.compile(r"\.execute\s*\(")
GRAPH_EXECUTE_PARALLEL_RE = re.compile(r"\.executeParallel\s*\(")

SOURCE_SUFFIXES = {".cpp", ".cppm", ".ixx", ".h", ".hpp"}
SOURCE_DIRS = (
    Path("Artifact/src"),
    Path("Artifact/include"),
    Path("ArtifactCore/src"),
    Path("ArtifactCore/include"),
)
SHADER_DIRS = (Path("Artifact/shaders"), Path("Artifact/App/shaders"))
CONTROLLER = Path("Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp")
FRAME_EXECUTION = CONTROLLER.with_stem(CONTROLLER.stem + "FrameExecution")
GPU_PASSES = CONTROLLER.with_stem(CONTROLLER.stem + "GpuPasses")
FRAME_DIAGNOSTICS = CONTROLLER.with_stem(CONTROLLER.stem + "Diagnostics")
FOUNDATION = Path("ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx")
EXECUTOR = Path("Artifact/include/Render/CompositionRenderPassExecutor.ixx")
# plan 上は宣言されるが pass オブジェクトを持たない既知のステージ:
#   Flush    : インライン計測のみ
#   Surface/Mask : per-layer pipeline が直接呼び出し
DEFAULT_KNOWN_INLINE = {"Flush", "Surface", "Mask"}
# 既定で詳細表示するマルチパス／エフェクト系シェーダ群（--all-shaders で全件表示）
NOTABLE_SHADER_RE = re.compile(
    r"fsr|anime4k|heightfog|ddgi|vxgi|rtreflection|rtdiffuse|rtao|rtshadow|"
    r"ssr|temporalaa|filmframe|virtualtexture|bloomseparate|gaussianblur|"
    r"depthoffield|motionblur|fft",
    re.IGNORECASE,
)


def find_matching(text: str, open_idx: int, opening: str, closing: str) -> int:
    """文字列/コメントを考慮して対応する閉じ括弧の位置を返す。"""
    depth = 0
    i = open_idx
    n = len(text)
    in_string = in_char = in_line_comment = in_block_comment = False
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if in_line_comment:
            if c == "\n":
                in_line_comment = False
        elif in_block_comment:
            if c == "*" and nxt == "/":
                in_block_comment = False
                i += 1
        elif in_string:
            if c == "\\":
                i += 1
            elif c == '"':
                in_string = False
        elif in_char:
            if c == "\\":
                i += 1
            elif c == "'":
                in_char = False
        else:
            if c == "/" and nxt == "/":
                in_line_comment = True
                i += 1
            elif c == "/" and nxt == "*":
                in_block_comment = True
                i += 1
            elif c == '"':
                in_string = True
            elif c == "'":
                in_char = True
            elif c == opening:
                depth += 1
            elif c == closing:
                depth -= 1
                if depth == 0:
                    return i
        i += 1
    return n - 1


def braced_block(text: str, open_brace: int) -> str:
    end = find_matching(text, open_brace, "{", "}")
    return text[open_brace : end + 1]


def line_of(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def iter_source_files(root: Path) -> Iterable[Path]:
    for rel in SOURCE_DIRS:
        base = root / rel
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                yield path


def unique_ordered(items: Iterable[str]) -> list[str]:
    seen: set[str] = set()
    out: list[str] = []
    for item in items:
        if item not in seen:
            seen.add(item)
            out.append(item)
    return out


def audit_plan(text: str) -> list[str]:
    match = re.search(r"buildFrameRenderPassPlan\s*\(", text)
    if not match:
        return []
    paren = text.index("(", match.start())
    close = find_matching(text, paren, "(", ")")
    brace = text.find("{", close)
    if brace < 0:
        return []
    return unique_ordered(PASS_KIND_RE.findall(braced_block(text, brace)))


def audit_functional_passes(text: str) -> list[dict]:
    out = []
    for match in FUNCTIONAL_PASS_RE.finditer(text):
        out.append(
            {
                "kind": match.group(1),
                "name": match.group(2),
                "line": line_of(text, match.start()),
            }
        )
    return out


def audit_render_graph(controller_sources: dict[Path, str], foundation_text: str, executor_text: str) -> dict:
    declarations = [
        {"variable": match.group(1), "file": str(path), "line": line_of(text, match.start())}
        for path, text in controller_sources.items()
        for match in RENDER_GRAPH_DECL_RE.finditer(text)
    ]
    controller_text = "\n".join(controller_sources.values())
    combined = "\n".join((controller_text, foundation_text, executor_text))
    serial = len(GRAPH_EXECUTE_RE.findall(combined))
    parallel = len(GRAPH_EXECUTE_PARALLEL_RE.findall(combined))
    in_controller = len(GRAPH_EXECUTE_RE.findall(controller_text))
    if parallel == 0 and in_controller == 0 and serial > 0:
        mode = "serial executor wrapper only; no graph-driven frame scheduling"
    elif parallel == 0 and serial == 0:
        mode = "diagnostic / order validation only"
    else:
        mode = "graph execution present (review required)"
    return {
        "declarations": declarations,
        "compile_calls": len(GRAPH_COMPILE_RE.findall(combined)),
        "diagnostic_snapshots": len(DIAGNOSTIC_SNAPSHOT_RE.findall(combined)),
        "serial_execute_calls": serial,
        "parallel_execute_calls": parallel,
        "execute_calls_in_controller": in_controller,
        "mode": mode,
    }


def audit_adapters(foundation_text: str) -> list[dict]:
    out = []
    for name in ("GIRenderGraphAdapter", "PointwiseRenderGraphAdapter"):
        sites = [
            {
                "line": line_of(foundation_text, m.start()),
                "text": foundation_text[m.start() : m.start() + 70].replace("\n", " ").strip(),
            }
            for m in re.finditer(re.escape(name), foundation_text)
        ]
        out.append(
            {
                "adapter": name,
                "occurrences": len(sites),
                "sites": sites[:4],
                "assessment": "definition only (no live caller)" if len(sites) <= 1 else "review call sites",
            }
        )
    return out


def audit_dormant_shaders(root: Path) -> list[dict]:
    sources = list(iter_source_files(root))
    source_texts = [(p, read_text(p)) for p in sources]

    # ビルド一覧（.vcxitems / CMake）は「コンパイル対象」であって「dispatch 配線」ではない。
    # 両者を分けて記録しないと FSR 等を「配線済み」と誤判定する。
    list_texts: list[tuple[Path, str]] = []
    for pattern in ("*.vcxitems", "CMakeLists.txt", "*.cmake"):
        for base in (root / "Artifact", root / "ArtifactCore"):
            for path in base.rglob(pattern):
                if path.is_file():
                    list_texts.append((path, read_text(path)))

    out = []
    seen: set[str] = set()
    for rel in SHADER_DIRS:
        base = root / rel
        if not base.is_dir():
            continue
        for shader in sorted(base.glob("*.hlsl")):
            stem = shader.stem
            if stem in seen:
                continue
            seen.add(stem)
            needle = stem.lower()
            host = None
            for path, text in source_texts:
                if needle in text.lower():
                    host = str(path.relative_to(root))
                    break
            build_list = None
            for path, text in list_texts:
                if needle in text.lower():
                    build_list = str(path.relative_to(root))
                    break
            out.append(
                {
                    "shader": str(shader.relative_to(root)),
                    "host_reference": host,
                    "build_list_reference": build_list,
                    "status": "wired (C++ host reference)"
                    if host
                    else "compile-listed only (no dispatch)"
                    if build_list
                    else "dormant (no reference)",
                }
            )
    return out


def build_report(root: Path, known_inline: set[str]) -> dict:
    split = any((root / path).is_file() for path in (FRAME_EXECUTION, GPU_PASSES, FRAME_DIAGNOSTICS))
    frame_source = FRAME_EXECUTION if split else CONTROLLER
    plan_source = GPU_PASSES if split else CONTROLLER
    graph_paths = (frame_source, FRAME_DIAGNOSTICS) if split else (frame_source,)
    graph_sources = {path: read_text(root / path) for path in graph_paths}
    frame_text = graph_sources[frame_source]
    foundation_path = root / FOUNDATION
    executor_path = root / EXECUTOR
    foundation_text = read_text(foundation_path) if foundation_path.is_file() else ""
    executor_text = read_text(executor_path) if executor_path.is_file() else ""

    declared = audit_plan(read_text(root / plan_source))
    functional = audit_functional_passes(frame_text)
    functional_kinds = {p["kind"] for p in functional}
    unresolved = [k for k in declared if k not in functional_kinds and k not in known_inline]

    return {
        "root": str(root),
        "controller": str(frame_source),
        "plan_source": str(plan_source),
        "plan_declared": declared,
        "functional_passes": functional,
        "coverage": {
            "declared_without_executor": [k for k in declared if k not in functional_kinds],
            "known_inline_excluded": sorted(known_inline),
            "unexpected_gaps": unresolved,
        },
        "executor_calls": {
            "runWithRenderGraph": len(RUN_SINGLE_RE.findall(frame_text)),
            "runAllWithRenderGraph": len(RUN_MULTI_RE.findall(frame_text)),
            "run_direct": len(RUN_DIRECT_RE.findall(frame_text)),
        },
        "render_graph": audit_render_graph(graph_sources, foundation_text, executor_text),
        "adapters": audit_adapters(foundation_text),
        "all_shaders": audit_dormant_shaders(root),
    }


def render_markdown(report: dict, show_all_shaders: bool = False) -> str:
    lines = [
        "# Render Pass Audit",
        "",
        f"- root: `{report['root']}`",
        f"- controller: `{report['controller']}`",
        f"- plan source: `{report['plan_source']}`",
        "",
        "## 1. Frame pass plan (declared)",
        "",
        " -> ".join(report["plan_declared"]) or "(not found)",
        "",
        "## 2. FunctionalRenderPass (executed)",
        "",
        "| line | kind | name |",
        "|---:|---|---|",
    ]
    for p in report["functional_passes"]:
        lines.append(f"| {p['line']} | {p['kind']} | {p['name']} |")

    cov = report["coverage"]
    lines += [
        "",
        "## 3. Coverage",
        "",
        f"- declared without executor: {cov['declared_without_executor'] or 'none'}",
        f"- known inline (excluded): {cov['known_inline_excluded'] or 'none'}",
        f"- unexpected gaps: {cov['unexpected_gaps'] or 'none'}",
        "",
        "## 4. Executor call inventory",
        "",
    ]
    for name, count in report["executor_calls"].items():
        lines.append(f"- `{name}`: {count}")

    rg = report["render_graph"]
    lines += [
        "",
        "## 5. RenderGraph usage",
        "",
        f"- declarations: {rg['declarations']}",
        f"- compile(): {rg['compile_calls']}",
        f"- compileDiagnosticSnapshot(): {rg['diagnostic_snapshots']}",
        f"- serial execute(): {rg['serial_execute_calls']} "
        f"(in controller: {rg['execute_calls_in_controller']})",
        f"- executeParallel(): {rg['parallel_execute_calls']}",
        f"- assessment: {rg['mode']}",
        "",
        "## 6. RenderGraph adapters",
        "",
    ]
    for a in report["adapters"]:
        lines.append(f"- **{a['adapter']}**: {a['assessment']} (occurrences={a['occurrences']})")

    shaders = report["all_shaders"]
    unwired = [s for s in shaders if not s["host_reference"]]
    notable = [s for s in unwired if NOTABLE_SHADER_RE.search(s["shader"])]

    lines += [
        "",
        "## 7. Shaders without C++ dispatch reference",
        "",
        f"- total HLSL files scanned: {len(shaders)}",
        f"- no host reference: {len(unwired)}",
        f"  - compile-listed only (.vcxitems / CMake): "
        f"{sum(1 for s in unwired if s['build_list_reference'])}",
        f"  - no reference at all: {sum(1 for s in unwired if not s['build_list_reference'])}",
        "",
    ]
    display = shaders if show_all_shaders else notable
    label = "all shaders" if show_all_shaders else "notable multi-pass / effect families"
    lines += [f"### {label}", "", "| shader | build list | status |", "|---|---|---|"]
    for s in display:
        listed = "yes" if s["build_list_reference"] else "-"
        lines.append(f"| `{s['shader']}` | {listed} | {s['status']} |")
    if not show_all_shaders:
        lines += ["", "(use --all-shaders to list every HLSL file)"]
    lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    default_root = Path(__file__).resolve().parents[1]
    parser.add_argument("--root", type=Path, default=default_root, help="repository root")
    parser.add_argument("--json", action="store_true", help="emit JSON instead of Markdown")
    parser.add_argument("--all-shaders", action="store_true", help="list every HLSL file, not just notable families")
    parser.add_argument("--output", type=Path, help="also write the report to this file")
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit 1 when a declared pass has no executor and is not a known-inline stage",
    )
    parser.add_argument(
        "--known-inline",
        default=",".join(sorted(DEFAULT_KNOWN_INLINE)),
        help="comma-separated pass kinds allowed to have no executor wrapper",
    )
    args = parser.parse_args(argv)

    root = args.root.resolve()
    if not (root / CONTROLLER).is_file():
        print(f"error: controller not found under {root}", file=sys.stderr)
        return 2

    known_inline = {s.strip() for s in args.known_inline.split(",") if s.strip()}
    try:
        report = build_report(root, known_inline)
    except FileNotFoundError as error:
        print(f"error: required audit source not found: {error.filename}", file=sys.stderr)
        return 2
    text = (
        json.dumps(report, indent=2, ensure_ascii=False)
        if args.json
        else render_markdown(report, show_all_shaders=args.all_shaders)
    )

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + "\n", encoding="utf-8")
        print(f"wrote {args.output}")
    else:
        print(text)

    if args.check and (not report["plan_declared"] or not report["functional_passes"]):
        print("FAIL: frame pass plan or executor definitions not found", file=sys.stderr)
        return 1

    if args.check and report["coverage"]["unexpected_gaps"]:
        print(
            "FAIL: unexpected pass coverage gaps: " + ", ".join(report["coverage"]["unexpected_gaps"]),
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
