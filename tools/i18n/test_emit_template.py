import subprocess
import sys
from pathlib import Path

tmp = Path("temp_i18n_probe.cppm")
tmp.write_text(
    'void f(){ auto a = tt("probe.new_key", "New"); '
    'auto b = menuText(QStringLiteral("probe.other_key"), QStringLiteral("Other")); }\n',
    encoding="utf-8",
)
out = Path("temp_i18n_template.json")
result = subprocess.run(
    [sys.executable, "tools/i18n/audit_translations.py", str(tmp),
     "--locale", "Artifact/translations/ja.json",
     "--baseline", "Artifact/translations/en.json",
     "--emit-template", str(out)],
    capture_output=True, text=True,
)
print("STDOUT:", result.stdout)
print("STDERR:", result.stderr)
print("TEMPLATE:", out.read_text(encoding="utf-8") if out.exists() else "NO FILE")
tmp.unlink(missing_ok=True)
out.unlink(missing_ok=True)
