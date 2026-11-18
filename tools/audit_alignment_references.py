"""Static audit of alignment references used by the offline image R&D labs.

RD10 (PencilPrintLab) showed that an alignment measurement can silently
measure the wrong thing when the reference frame is not derived from the
processed output, or when the reference is resampled with an interpolation
kernel that low-passes the very texture the measurement is about.

This tool parses the Python sources of `experiments/` and `tools/` with
`ast` and reports functions that compute an "aligned" (or "drift") metric,
classified by how their reference frame is built:

  RAW_REFERENCE      high    the aligned metric compares the processed output
                              against a raw source input, so it measures effect
                              magnitude instead of follow behaviour.
  TRANSLATION_WARP   medium  the reference is produced by an interpolation
                              warp (bilinear), which low-passes the texture
                              under test; the residual is blur-limited.
  NEAREST_WARP       high    the reference uses INTER_NEAREST, which is not
                              an exact integer translation for larger offsets.
  MOTION_REMAP       info    the reference uses a motion map (remap); the
                              reference is processed, but the residual is
                              still interpolation-limited.

The output is a review candidate list, not a verdict: every hit must be
read before a conclusion is drawn.

Usage:
    python tools/audit_alignment_references.py --root . --format markdown
    python tools/audit_alignment_references.py --root . --output docs/analysis/alignment_reference_audit.json
"""

from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path

RAW_NAMES = ("source", "input", "base", "fixture", "raw_")
PROCESSED_NAMES = ("out", "output", "rendered", "processed", "result", "candidate")
ALIGNED_NAMES = ("aligned", "alignment", "drift")
DIFFERENCE_NAMES = ("mae", "delta", "residual", "difference", "error", "deviation")


def _matches(name: str, needles: tuple[str, ...]) -> bool:
    low = name.lower()
    return any(n in low for n in needles)


def _is_path_like(name: str) -> bool:
    low = name.lower()
    return low.endswith("path") or low.endswith("dir") or low.endswith("file")


def _call_name(node: ast.AST) -> str:
    if isinstance(node, ast.Attribute):
        return node.attr
    if isinstance(node, ast.Name):
        return node.id
    return ""


def _names_in(node: ast.AST) -> set[str]:
    return {n.id for n in ast.walk(node) if isinstance(n, ast.Name)}


def audit_function(func: ast.FunctionDef) -> list[dict]:
    findings: list[dict] = []
    func_raw = {a.arg for a in func.args.args
                if _matches(a.arg, RAW_NAMES) and not _is_path_like(a.arg)}
    func_processed = {a.arg for a in func.args.args if _matches(a.arg, PROCESSED_NAMES)}
    aligned_lines: list[int] = []
    warp_linear: list[int] = []
    warp_nearest: list[int] = []
    motion_remap: list[int] = []
    computes_difference = False

    for node in ast.walk(func):
        if isinstance(node, ast.Call):
            name = _call_name(node.func)
            if name == "warpAffine":
                flags = [kw.value for kw in node.keywords if kw.arg == "flags"]
                for value in flags:
                    if isinstance(value, ast.Attribute) and value.attr == "INTER_NEAREST":
                        warp_nearest.append(node.lineno)
                    elif isinstance(value, ast.Attribute) and value.attr == "INTER_LINEAR":
                        warp_linear.append(node.lineno)
            elif name == "remap":
                motion_remap.append(node.lineno)
        if isinstance(node, ast.Name) and _matches(node.id, ALIGNED_NAMES):
            aligned_lines.append(node.lineno)
        if isinstance(node, ast.Attribute) and _matches(node.attr, ALIGNED_NAMES):
            aligned_lines.append(node.lineno)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Sub):
            computes_difference = True

    if not aligned_lines:
        return findings

    def add(rule: str, severity: str, line: int, detail: str) -> None:
        findings.append({"rule": rule, "severity": severity, "function": func.name,
                         "line": line, "detail": detail})

    first = min(aligned_lines)
    if func_raw and func_processed:
        add("RAW_REFERENCE", "high", first,
            f"function takes raw parameter(s) {sorted(func_raw)} and processed parameter(s) "
            f"{sorted(func_processed)}; confirm the aligned metric compares processed vs processed")
    if warp_nearest and computes_difference:
        add("NEAREST_WARP", "high", warp_nearest[0],
            "INTER_NEAREST warpAffine inside an aligned difference metric; "
            "not an exact integer translation")
    if warp_linear:
        add("TRANSLATION_WARP", "medium", warp_linear[0],
            "bilinear translation warp builds the aligned reference; the residual is blur-limited")
    if motion_remap:
        add("MOTION_REMAP", "info", motion_remap[0],
            "motion-map remap builds the aligned reference; reference is processed but "
            "interpolation-limited")
    return findings


def audit_source(path: Path, root: Path) -> list[dict]:
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"))
    except (SyntaxError, UnicodeDecodeError, OSError):
        return []
    findings: list[dict] = []
    for func in ast.walk(tree):
        if isinstance(func, (ast.FunctionDef, ast.AsyncFunctionDef)):
            for f in audit_function(func):
                f["file"] = path.relative_to(root).as_posix()
                findings.append(f)
    return findings


def collect(root: Path, subdirs: tuple[str, ...] = ("experiments", "tools")) -> list[dict]:
    findings: list[dict] = []
    for sub in subdirs:
        base = root / sub
        if not base.exists():
            continue
        for path in sorted(base.rglob("*.py")):
            relative_parts = path.relative_to(root).parts
            if (any(part in {"__pycache__", "out", "build", ".git"}
                    for part in relative_parts)
                    or path.name == Path(__file__).name):
                continue
            findings.extend(audit_source(path, root))
    return findings


def to_markdown(findings: list[dict]) -> str:
    lines = ["# Alignment reference audit", ""]
    if not findings:
        lines.append("No candidate found.")
        return "\n".join(lines) + "\n"
    lines.append("| rule | severity | file | function | line | detail |")
    lines.append("|---|---|---|---|---:|---|")
    for f in findings:
        lines.append(f"| {f['rule']} | {f['severity']} | `{f['file']}` | `{f['function']}` | {f['line']} | {f['detail']} |")
    lines.append("")
    lines.append("Review candidates only. Each hit must be read before a conclusion is drawn.")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".")
    parser.add_argument("--output", default="")
    parser.add_argument("--format", choices=("markdown", "json"), default="markdown")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    findings = collect(root)
    payload = {
        "root": ".",
        "scanned": ["experiments", "tools"],
        "finding_count": len(findings),
        "findings": findings,
        "note": "review candidates; static analysis does not decide validity",
    }
    if args.output:
        out = Path(args.output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if args.format == "json":
        print(json.dumps(payload, indent=2, ensure_ascii=False))
    else:
        print(to_markdown(findings))


if __name__ == "__main__":
    main()
