from __future__ import annotations

from pathlib import Path
import os
import platform

PROC_MEMINFO = Path("/proc/meminfo")

def _meminfo() -> dict[str, int]:
    values: dict[str, int] = {}
    if not PROC_MEMINFO.exists():
        return values
    for line in PROC_MEMINFO.read_text(errors="replace").splitlines():
        if ":" not in line:
            continue
        key, raw = line.split(":", 1)
        parts = raw.strip().split()
        if not parts:
            continue
        try:
            value = int(parts[0])
        except ValueError:
            continue
        values[key] = value * 1024 if len(parts) > 1 and parts[1].lower() == "kb" else value
    return values

def snapshot() -> dict:
    mem = _meminfo()
    logical = os.cpu_count() or 1
    try:
        load = os.getloadavg()
    except (AttributeError, OSError):
        load = (0.0, 0.0, 0.0)
    return {
        "architecture": platform.machine(),
        "platform": platform.system(),
        "logical_cpus": logical,
        "load": list(load),
        "ram_total_bytes": mem.get("MemTotal", 0),
        "ram_available_bytes": mem.get("MemAvailable", mem.get("MemFree", 0)),
        "swap_total_bytes": mem.get("SwapTotal", 0),
        "swap_free_bytes": mem.get("SwapFree", 0),
    }
