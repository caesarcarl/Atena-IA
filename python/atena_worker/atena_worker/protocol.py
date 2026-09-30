from __future__ import annotations

import json
from pathlib import Path
from .chunking import chunk_text
from .resources import snapshot
from .pdf import group_pdf_pages

TEXT_EXTENSIONS = {
    ".txt", ".md", ".markdown", ".rst", ".c", ".h", ".cpp", ".hpp",
    ".py", ".java", ".kt", ".js", ".ts", ".json", ".toml", ".yaml", ".yml",
    ".sh", ".bash", ".cmake", ".html", ".css", ".sql"
}

def handle(request: dict) -> dict:
    op = request.get("op")
    if op == "health":
        return {"ok": True, "worker": "atena-worker", "protocol": "atena.worker/1"}
    if op == "resources":
        return {"ok": True, "resources": snapshot()}
    if op == "chunk":
        text = str(request.get("text", ""))
        chunks = chunk_text(text, int(request.get("max_chars", 1800)), int(request.get("overlap_chars", 220)))
        return {"ok": True, "chunks": [c.to_dict() for c in chunks]}
    if op == "ingest_file":
        path = Path(str(request.get("path", ""))).expanduser().resolve()
        if not path.is_file():
            return {"ok": False, "error": "file_not_found", "path": str(path)}
        suffix = path.suffix.lower()
        if suffix == ".pdf":
            try:
                backend, documents = group_pdf_pages(path, int(request.get("max_document_chars", 120000)))
            except Exception as exc:
                return {"ok": False, "error": "pdf_extract_failed", "message": str(exc), "path": str(path)}
            return {
                "ok": True,
                "kind": "pdf",
                "backend": backend,
                "source": {"path": str(path), "title": path.name, "size_bytes": path.stat().st_size},
                "documents": documents,
            }
        if suffix not in TEXT_EXTENSIONS:
            return {"ok": False, "error": "unsupported_file_type", "extension": suffix}
        text = path.read_text(encoding="utf-8", errors="replace")
        return {
            "ok": True,
            "kind": "text",
            "backend": "python-stdlib",
            "source": {"path": str(path), "title": path.name, "size_bytes": path.stat().st_size},
            "documents": [{
                "title": path.name,
                "locator": f"file://{path}",
                "text": text,
            }],
        }
    return {"ok": False, "error": "unknown_operation", "op": op}

def encode_response(response: dict) -> str:
    return json.dumps(response, ensure_ascii=False, separators=(",", ":"))
