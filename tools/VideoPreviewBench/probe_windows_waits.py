"""Measure short waits in an independent process without changing timer resolution."""

import argparse
import ctypes
from ctypes import wintypes
import json
import math
import os
from pathlib import Path
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--samples", type=int, default=256)
    parser.add_argument("--interval-ms", type=int, default=1)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("Windows only")
    if not 16 <= args.samples <= 4096 or not 1 <= args.interval_ms <= 16:
        parser.error("Samples must be 16..4096 and interval 1..16 ms")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    sleep = kernel.Sleep
    sleep.argtypes, sleep.restype = [wintypes.DWORD], None
    create = kernel.CreateWaitableTimerExW
    create.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD]
    create.restype = wintypes.HANDLE
    set_timer = kernel.SetWaitableTimer
    set_timer.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_longlong),
                         wintypes.LONG, ctypes.c_void_p, ctypes.c_void_p, wintypes.BOOL]
    set_timer.restype = wintypes.BOOL
    wait = kernel.WaitForSingleObject
    wait.argtypes, wait.restype = [wintypes.HANDLE, wintypes.DWORD], wintypes.DWORD
    cancel = kernel.CancelWaitableTimer
    cancel.argtypes, cancel.restype = [wintypes.HANDLE], wintypes.BOOL
    close = kernel.CloseHandle
    close.argtypes, close.restype = [wintypes.HANDLE], wintypes.BOOL
    results = []
    for name, flags in [("Sleep", None), ("waitable", 0), ("highResolutionWaitable", 2)]:
        # TIMER_MODIFY_STATE | SYNCHRONIZE; unnamed, process-owned timer.
        handle = create(None, None, flags, 0x100002) if flags is not None else None
        if flags is not None and not handle:
            results.append({"method": name, "available": False,
                            "win32Error": ctypes.get_last_error()})
            continue
        durations = []
        due = ctypes.c_longlong(-args.interval_ms * 10000)
        try:
            for _ in range(args.samples):
                started = time.perf_counter_ns()
                if handle:
                    if not set_timer(handle, ctypes.byref(due), 0, None, None, False):
                        raise ctypes.WinError(ctypes.get_last_error())
                    if wait(handle, 5000) != 0:
                        raise RuntimeError("Timer wait failed or exceeded five seconds")
                else:
                    sleep(args.interval_ms)
                durations.append((time.perf_counter_ns() - started) / 1.0e6)
        finally:
            if handle:
                cancel(handle)
                close(handle)
        ordered = sorted(durations)
        results.append({"method": name, "available": True, "samples": len(ordered),
                        "minimumMs": ordered[0], "maximumMs": ordered[-1],
                        "meanMs": sum(ordered) / len(ordered),
                        **{f"p{percentile}Ms": ordered[math.ceil(len(ordered) * percentile / 100) - 1]
                           for percentile in (50, 95, 99)}})
    report = {
        "schema": "artifact-windows-wait-probe-v1", "requestedIntervalMs": args.interval_ms,
        "results": results,
        "policy": "Call-plus-wait wall time; nearest-rank percentiles; no timeBeginPeriod or priority changes",
        "caveat": "Independent non-GUI Python process; not Qt or std::condition_variable timing, GPU execution or preview acceptance",
        "references": ["https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw",
                       "https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer"],
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
