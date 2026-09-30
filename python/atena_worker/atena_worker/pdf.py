from __future__ import annotations

from pathlib import Path
import shutil
import subprocess


def _extract_with_pdftotext(path: Path) -> list[str]:
    exe = shutil.which("pdftotext")
    if not exe:
        raise RuntimeError("pdftotext_not_found")
    proc = subprocess.run(
        [exe, "-layout", "-enc", "UTF-8", str(path), "-"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        message = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"pdftotext_failed:{message or proc.returncode}")
    text = proc.stdout.decode("utf-8", errors="replace")
    return [page.strip() for page in text.split("\f") if page.strip()]


def _extract_with_pypdf(path: Path) -> list[str]:
    try:
        from pypdf import PdfReader  # type: ignore
    except Exception as exc:  # optional fallback
        raise RuntimeError("pypdf_not_available") from exc
    reader = PdfReader(str(path))
    pages: list[str] = []
    for page in reader.pages:
        pages.append((page.extract_text() or "").strip())
    return [page for page in pages if page]


def extract_pdf_pages(path: Path) -> tuple[str, list[str]]:
    try:
        return "pdftotext", _extract_with_pdftotext(path)
    except RuntimeError as first:
        try:
            return "pypdf", _extract_with_pypdf(path)
        except RuntimeError as second:
            raise RuntimeError(
                "pdf_backend_unavailable: instale poppler-utils (pdftotext) "
                "ou o pacote Python pypdf"
            ) from second


def group_pdf_pages(path: Path, max_chars: int = 120_000) -> tuple[str, list[dict]]:
    backend, pages = extract_pdf_pages(path)
    if not pages:
        return backend, []

    documents: list[dict] = []
    group: list[str] = []
    group_chars = 0
    start_page = 1

    def flush(end_page: int) -> None:
        nonlocal group, group_chars, start_page
        if not group:
            return
        text_parts: list[str] = []
        page_no = start_page
        for page in group:
            text_parts.append(f"[PAGE {page_no}]\n{page}")
            page_no += 1
        title = path.name if start_page == end_page else f"{path.name} (p. {start_page}-{end_page})"
        documents.append(
            {
                "title": title,
                "locator": f"file://{path.resolve()}#pages={start_page}-{end_page}",
                "text": "\n\n".join(text_parts),
                "page_start": start_page,
                "page_end": end_page,
            }
        )
        group = []
        group_chars = 0
        start_page = end_page + 1

    for index, page in enumerate(pages, start=1):
        if group and group_chars + len(page) > max_chars:
            flush(index - 1)
        group.append(page)
        group_chars += len(page)
    flush(len(pages))
    return backend, documents
