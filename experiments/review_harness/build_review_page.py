"""Build a static, offline review page from validated R&D runs.

The page shows evidence images and blank review forms per run. It does not
display blind keys until the reviewer clicks a disclosure button, and it never
sets or evaluates gates.
"""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LABS = ("WatercolorLab", "PencilPrintLab", "MaterialSeparationLab", "SeamlessTextureLab")


def collect() -> list[dict]:
    runs = []
    for lab in LABS:
        cap = ROOT / "experiments" / lab / "captures"
        if not cap.exists():
            continue
        for manifest in sorted(cap.glob("rd*/manifest*.json")):
            m = json.loads(manifest.read_text(encoding="utf-8"))
            base = manifest.parent
            images = []
            evidence = m.get("evidence", {})
            items = evidence.items() if isinstance(evidence, dict) else enumerate(evidence)
            for kind, ev in items:
                if not isinstance(ev, dict):
                    continue
                rel = ev.get("file", "")
                if rel.lower().endswith(".png"):
                    p = base / rel
                    if p.exists():
                        images.append({
                            "label": f"{m.get('rd', '')} {kind}",
                            "src": str(p.relative_to(ROOT)).replace("\\", "/"),
                        })
            forms = []
            for form in sorted(base.glob("review/*FORM.md")):
                forms.append({
                    "name": form.name,
                    "src": str(form.relative_to(ROOT)).replace("\\", "/"),
                    "text": form.read_text(encoding="utf-8"),
                })
            keys = []
            for key in sorted(base.glob("*blind_key.json")) + sorted(base.glob("blind_key.json")):
                keys.append({
                    "name": key.name,
                    "src": str(key.relative_to(ROOT)).replace("\\", "/"),
                    "text": key.read_text(encoding="utf-8"),
                })
            gate_forms = [
                {"name": p.name, "src": str(p.relative_to(ROOT)).replace("\\", "/")}
                for p in base.glob("review/GATE_FORM.md")
            ]
            runs.append({
                "lab": lab,
                "rd": m.get("rd", ""),
                "stage": m.get("stage", ""),
                "gates": m.get("gates", "unknown"),
                "images": images,
                "forms": forms,
                "keys": keys,
                "gate_forms": gate_forms,
            })
    runs.sort(key=lambda r: (r["lab"], r["rd"]))
    return runs


def render(runs: list[dict]) -> str:
    sections = []
    for run in runs:
        imgs = "".join(
            f'<figure><img src="{html.escape(i["src"])}" alt="{html.escape(i["label"])}">'
            f'<figcaption>{html.escape(i["label"])}</figcaption></figure>'
            for i in run["images"]
        ) or "<p>No evidence images in manifest.</p>"
        forms = "".join(
            f'<details><summary>{html.escape(f["name"])}</summary>'
            f'<pre>{html.escape(f["text"])}</pre></details>'
            for f in run["forms"]
        ) or "<p>No blank form.</p>"
        keys = "".join(
            f'<details class="key"><summary>{html.escape(k["name"])} (-blind key, open after review-)</summary>'
            f'<pre>{html.escape(k["text"])}</pre></details>'
            for k in run["keys"]
        ) or "<p>No blind key.</p>"
        gates = ", ".join(html.escape(g["name"]) for g in run["gate_forms"]) or "none found"
        sections.append(f"""
<section class="run">
  <h2>{html.escape(run['lab'])} / {html.escape(run['rd'])}</h2>
  <p class="meta">stage: {html.escape(run['stage'])} | gates: {html.escape(str(run['gates']))}</p>
  <p class="meta">gate form: {gates}</p>
  <div class="images">{imgs}</div>
  <h3>Blank review forms</h3>{forms}
  <h3>Blind keys (do not open before review)</h3>{keys}
</section>
""")
    return f"""<!doctype html>
<html lang="ja">
<head>
<meta charset="utf-8">
<title>ArtifactStudio R&D offline review</title>
<style>
body {{ font-family: sans-serif; margin: 1.5rem; background: #1e1e1e; color: #ddd; }}
h1 {{ font-size: 1.4rem; }}
.run {{ border: 1px solid #555; padding: 1rem; margin: 1rem 0; }}
.meta {{ color: #aaa; font-size: .9rem; }}
.images {{ display: flex; flex-wrap: wrap; gap: 1rem; }}
figure {{ margin: 0; }}
img {{ max-width: 480px; border: 1px solid #666; }}
figcaption {{ font-size: .85rem; color: #aaa; }}
details {{ margin: .5rem 0; }}
pre {{ background: #2a2a2a; padding: .5rem; max-height: 18rem; overflow: auto; }}
.key summary {{ color: #f8b878; }}
</style>
</head>
<body>
<h1>ArtifactStudio R&amp;D offline review</h1>
<p>静的レビュー用ページ。証跡画像と空票を表示する。blind key はレビュー後に開くこと。
gate 閾値の決定と品質判定はレビュー者の責務であり、このページは行わない。</p>
{''.join(sections)}
</body>
</html>
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="experiments/review_harness/out")
    args = parser.parse_args()
    out = ROOT / args.out if not Path(args.out).is_absolute() else Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    runs = collect()
    path = out / "review.html"
    path.write_text(render(runs), encoding="utf-8")
    print(f"generated review page for {len(runs)} runs -> {path}")


if __name__ == "__main__":
    main()
