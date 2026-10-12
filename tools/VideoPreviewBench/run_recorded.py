"""Run one standalone preview trial and preserve its launch identity and outcome."""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess


def fingerprint(path):
    if not path.is_file():
        return None
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--output-prefix", required=True, type=Path)
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    arguments = options.arguments
    if arguments[:1] == ["--"]:
        arguments = arguments[1:]
    if not options.executable.is_file():
        parser.error("Executable does not exist")
    if not arguments or any(a == "--report" or a.startswith("--report=") for a in arguments):
        parser.error("Supply benchmark arguments; this runner owns --report")
    mode = "polling"
    if arguments.count("--measurement-loop") > 1:
        parser.error("Specify measurement-loop only once")
    if "--measurement-loop" in arguments:
        index = arguments.index("--measurement-loop") + 1
        if index >= len(arguments):
            parser.error("Missing measurement-loop value")
        mode = arguments[index]
    if mode not in ("polling", "qt") or any(a.startswith("--measurement-loop=") for a in arguments):
        parser.error("Use --measurement-loop polling or --measurement-loop qt")
    prefix = options.output_prefix.resolve()
    report = Path(str(prefix) + ".json")
    launch = Path(str(prefix) + "-launch.json")
    log = Path(str(prefix) + ".log")
    partial = Path(str(report) + ".partial.json")
    if any(p.exists() for p in (report, launch, log, partial)):
        parser.error("Output already exists; choose a new prefix")
    exe = options.executable.resolve()
    if exe in (report, launch, log, partial):
        parser.error("Output collides with executable")
    prefix.parent.mkdir(parents=True, exist_ok=True)
    arguments = [*arguments, "--report", str(report)]
    tracked = ("ARTIFACT_RENDER_BACKEND", "QT_PLUGIN_PATH", "QT_LOGGING_RULES",
               "QT_FORCE_STDERR_LOGGING", "ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP",
               "ARTIFACT_VIDEO_PREVIEW_PROFILE_UPLOAD")
    record = {
        "executable": str(exe), "sha256BeforeLaunch": fingerprint(exe)["sha256"],
        "startedAtUtc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "arguments": arguments, "report": str(report), "mode": mode, "exitCode": None,
        "environment": {key: os.environ.get(key) for key in tracked},
        "startupFileBeforeLaunch": fingerprint(exe.parent / "ArtifactStartup.json"),
        "identityCaveat": "File hashes and selected environment values; not loaded-module or effective-setting verification",
    }

    def save():
        launch.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")

    save()
    try:
        with log.open("x", encoding="utf-8") as output:
            result = subprocess.run([str(exe), *arguments], cwd=exe.parent,
                                    stdout=output, stderr=subprocess.STDOUT)
        record["exitCode"] = result.returncode
    except BaseException as error:
        record["runnerError"] = type(error).__name__
        raise
    finally:
        record["finishedAtUtc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        record["executableAfterRun"] = fingerprint(exe)
        record["startupFileAfterRun"] = fingerprint(exe.parent / "ArtifactStartup.json")
        save()
    print(json.dumps({"exitCode": result.returncode, "report": str(report)}))
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
