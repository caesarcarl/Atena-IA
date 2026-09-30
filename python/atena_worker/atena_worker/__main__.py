from __future__ import annotations

import json
import sys
from .protocol import handle, encode_response


def _single_command(argv: list[str]) -> int | None:
    if len(argv) >= 3 and argv[1] == "ingest-file":
        request = {"op": "ingest_file", "path": argv[2]}
        response = handle(request)
        sys.stdout.write(encode_response(response) + "\n")
        return 0 if response.get("ok") else 2
    if len(argv) >= 2 and argv[1] == "health":
        sys.stdout.write(encode_response(handle({"op": "health"})) + "\n")
        return 0
    return None


def main() -> int:
    direct = _single_command(sys.argv)
    if direct is not None:
        return direct
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            request = json.loads(raw)
            if not isinstance(request, dict):
                raise ValueError("request must be object")
            response = handle(request)
        except Exception as exc:  # worker boundary: return error, keep process alive
            response = {"ok": False, "error": "worker_exception", "message": str(exc)}
        sys.stdout.write(encode_response(response) + "\n")
        sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
