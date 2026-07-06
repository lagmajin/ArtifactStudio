#!/usr/bin/env python3
"""
M-DOCMETA Phase 1: Generate inventory of all markdown files.
Usage: python tools/generate_doc_inventory.py
Output: docs/INDEX_GENERATED.md
"""

import os, re, subprocess, sys
from datetime import datetime
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "docs" / "INDEX_GENERATED.md"

SCAN_DIRS = ["docs", "plans", "Artifact/docs", "ArtifactCore/docs"]
EXCLUDE_PATTERNS = [r"node_modules/", r"third_party/", r"libs/", r"vendor/"]


def should_exclude(path: Path) -> bool:
    rel = str(path.as_posix())
    return any(re.search(p, rel) for p in EXCLUDE_PATTERNS)


def get_git_last_modified(filepath: Path) -> Optional[str]:
    try:
        r = subprocess.run(["git", "log", "-1", "--format=%ai", "--", str(filepath)],
                           capture_output=True, text=True, cwd=ROOT, timeout=10)
        if r.returncode == 0 and r.stdout.strip():
            return r.stdout.strip()[:10]
    except (subprocess.TimeoutExpired, FileNotFoundError):
        pass
    return None


def extract_title_and_date(filepath: Path) -> tuple[str, Optional[str], Optional[str], list[str]]:
    title = filepath.stem
    date = None
    status = None
    keywords: list[str] = []
    try:
        content = filepath.read_text("utf-8", errors="replace")
    except (OSError, UnicodeDecodeError):
        return title, date, status, keywords

    for line in content.splitlines():
        m = re.match(r"^#\s+(.+)$", line)
        if m and title == filepath.stem:
            title = m.group(1).strip()
        m = re.search(r"(作成日|Date):\s*(\d{4}-\d{2}-\d{2})", line)
        if m and not date:
            date = m.group(2)
        m = re.search(r"(\d{4}-\d{2}-\d{2})", line)
        if m and not date:
            date = m.group(1)
        m = re.match(r"^Tags?:\s*(.+)$", line, re.IGNORECASE)
        if m:
            keywords.extend(t.strip() for t in m.group(1).split(","))
        m = re.match(r"^\*\*ステータス:\*\*\s*(.+)$", line)
        if m and not status:
            status = m.group(1).strip()
        m = re.match(r"^Status:\s*(.+)$", line, re.IGNORECASE)
        if m and not status:
            status = m.group(1).strip()

    if not date:
        m = re.search(r"(\d{4}-\d{2}-\d{2})", filepath.stem)
        if m:
            date = m.group(1)

    stem = re.sub(r"_\d{4}-\d{2}-\d{2}(_\d{4}-\d{2}-\d{2})?", "", filepath.stem)
    stem = re.sub(r"^(MILESTONE|BUG_REPORT|REPORT|M_)", "", stem).replace("_", " ").replace("-", " ").strip()
    for word in stem.split()[:5]:
        if word and word not in keywords and len(word) > 2:
            keywords.append(word)
    return title, date, status, keywords


def extract_status(filepath: Path) -> Optional[str]:
    try:
        content = filepath.read_text("utf-8", errors="replace")
    except (OSError, UnicodeDecodeError):
        return None

    for line in content.splitlines():
        m = re.match(r"^\*\*ステータス:\*\*\s*(.+)$", line)
        if m:
            return m.group(1).strip()
        m = re.match(r"^Status:\s*(.+)$", line, re.IGNORECASE)
        if m:
            return m.group(1).strip()
    return None


def categorize(path: Path) -> str:
    rel = path.relative_to(ROOT).as_posix()
    if rel.startswith("plans/"):
        return "plans"
    if rel.startswith("Artifact/docs/"):
        sub = rel[14:]
        return f"Artifact/{sub.split('/')[0]}" if "/" in sub else "Artifact"
    if rel.startswith("ArtifactCore/docs/"):
        sub = rel[18:]
        return f"ArtifactCore/{sub.split('/')[0]}" if "/" in sub else "ArtifactCore"
    if rel.startswith("docs/"):
        sub = rel[5:]
        return sub.split("/")[0] if "/" in sub else "root"
    return "other"


def scan_directory(base_dir: Path) -> list[dict]:
    files = []
    if not base_dir.exists():
        print(f"  [SKIP] {base_dir} does not exist")
        return files
    for md_file in sorted(base_dir.rglob("*.md")):
        if should_exclude(md_file):
            continue
        rel_path = md_file.relative_to(ROOT)
        title, date, status, keywords = extract_title_and_date(md_file)
        git_date = get_git_last_modified(md_file)
        size_kb = md_file.stat().st_size / 1024
        files.append({
            "path": rel_path.as_posix(),
            "title": title,
            "date": date or "---",
            "status": status or "---",
            "keywords": ", ".join(keywords[:8]) if keywords else "---",
            "git_date": git_date or "---",
            "size_kb": f"{size_kb:.1f}",
            "category": categorize(md_file),
        })
    return files


def generate_index(all_files: list[dict]) -> str:
    lines = []
    lines.append("# Document Inventory (Auto-Generated)\n")
    lines.append(f"> Generated: {datetime.now().strftime('%Y-%m-%d %H:%M')}")
    lines.append(f"> Total documents: {len(all_files)}\n")
    lines.append("---\n")

    categories: dict[str, list[dict]] = {}
    for f in all_files:
        categories.setdefault(f["category"], []).append(f)

    for cat_name in sorted(categories.keys()):
        cat_files = categories[cat_name]
        lines.append(f"## {cat_name} ({len(cat_files)} files)\n")
        lines.append("| # | File | Title | Date | Status | Modified | Size | Keywords |")
        lines.append("|---|------|-------|------|--------|----------|------|----------|")
        for idx, f in enumerate(cat_files, 1):
            t = f["title"].replace("|", "\\|").replace("\n", " ")[:80]
            k = f["keywords"].replace("|", "\\|")[:60]
            lines.append(f"| {idx} | `{f['path']}` | {t} | {f['date']} | {f['status']} | {f['git_date']} | {f['size_kb']} KB | {k} |")
        lines.append("")

    lines.append("---\n## Statistics\n| Category | Count |\n|---------|-------|")
    for cat_name in sorted(categories.keys()):
        lines.append(f"| {cat_name} | {len(categories[cat_name])} |")
    lines.append(f"| **Total** | **{len(all_files)}** |\n")
    return "\n".join(lines)


def check_broken_links(all_files: list[dict]) -> list[str]:
    warnings = []
    link_pattern = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
    for f_entry in all_files:
        filepath = ROOT / f_entry["path"]
        if not filepath.exists():
            continue
        try:
            content = filepath.read_text("utf-8", errors="replace")
        except (OSError, UnicodeDecodeError):
            continue
        base_dir = filepath.parent
        in_fenced_code = False
        for match in link_pattern.finditer(content):
            link_text, link_target = match.groups()
            line_start = content.rfind("\n", 0, match.start()) + 1
            line = content[line_start:content.find("\n", match.start()) if content.find("\n", match.start()) != -1 else len(content)]

            if line.strip().startswith("```"):
                in_fenced_code = not in_fenced_code
                continue
            if in_fenced_code:
                continue

            if link_target.startswith(("http://", "https://", "ftp://")):
                continue
            if link_target.startswith(("X:\\", "X:/", "C:\\", "D:\\", "file:///")):
                warnings.append(
                    f"  ABSOLUTE: {f_entry['path']} link '{link_text}' "
                    f"-> {link_target} (prefer relative path)"
                )
                continue
            if link_target.startswith("#"):
                continue
            if not (
                "/" in link_target
                or "\\" in link_target
                or link_target.startswith(("./", "../"))
                or link_target.lower().endswith(".md")
                or link_target.lower().endswith(".ixx")
                or link_target.lower().endswith(".cppm")
                or link_target.lower().endswith(".cpp")
                or link_target.lower().endswith(".h")
            ):
                continue
            resolved = (base_dir / link_target).resolve()
            if not resolved.exists():
                warnings.append(
                    f"  BROKEN: {f_entry['path']} link '{link_text}' "
                    f"-> {link_target} (not found)"
                )
    return warnings


def check_lifecycle_issues(all_files: list[dict]) -> list[str]:
    warnings: list[str] = []
    for f_entry in all_files:
        rel_path = f_entry["path"]
        filepath = ROOT / rel_path
        if not filepath.exists():
            continue
        if not rel_path.startswith("docs/planned/"):
            continue

        status = extract_status(filepath)
        if not status:
            continue
        normalized = status.strip().lower()
        explicit_complete = (
            status.lstrip().startswith("✅")
            or normalized.startswith("complete")
            or normalized.startswith("completed")
            or normalized.startswith("done")
            or status.startswith("完了")
        )
        if explicit_complete:
            warnings.append(
                f"  LIFECYCLE: {rel_path} is still under docs/planned/ but marked '{status}'"
            )
    return warnings


def main():
    print("=" * 60)
    print("M-DOCMETA Phase 1: Document Inventory Generator")
    print("=" * 60)
    all_files = []
    for dir_rel in SCAN_DIRS:
        scan_path = ROOT / dir_rel
        print(f"\nScanning: {dir_rel}/")
        files = scan_directory(scan_path)
        print(f"  -> {len(files)} files")
        all_files.extend(files)
    print(f"\nTotal: {len(all_files)} files")

    print("\nGenerating index...")
    index_content = generate_index(all_files)
    with open(OUTPUT, "w", encoding="utf-8") as f:
        f.write(index_content)
    print(f"Output: {OUTPUT}")
    print(f"Size: {len(index_content)} chars")

    print("\n--- Cross-reference validation (Phase 3 preview) ---")
    warnings = check_broken_links(all_files)
    if warnings:
        print(f"Warnings: {len(warnings)}")
        for w in warnings:
            print(w)
    else:
        print("No broken links found (not exhaustive)")

    print("\n--- Lifecycle validation (Phase 2 preview) ---")
    lifecycle_warnings = check_lifecycle_issues(all_files)
    if lifecycle_warnings:
        print(f"Warnings: {len(lifecycle_warnings)}")
        for w in lifecycle_warnings:
            print(w)
    else:
        print("No planned/done lifecycle issues found")
    print("\nDone.")


if __name__ == "__main__":
    main()
