from __future__ import annotations

from dataclasses import dataclass, asdict
import re

@dataclass(slots=True)
class Chunk:
    index: int
    text: str
    start: int
    end: int

    def to_dict(self) -> dict:
        return asdict(self)

def normalize_text(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()

def chunk_text(text: str, max_chars: int = 1800, overlap_chars: int = 220) -> list[Chunk]:
    if max_chars < 256:
        raise ValueError("max_chars deve ser >= 256")
    if overlap_chars < 0 or overlap_chars >= max_chars:
        raise ValueError("overlap_chars inválido")
    text = normalize_text(text)
    if not text:
        return []

    chunks: list[Chunk] = []
    pos = 0
    index = 0
    n = len(text)
    while pos < n:
        hard_end = min(n, pos + max_chars)
        end = hard_end
        if hard_end < n:
            # Prefer paragraph/sentence boundaries near the end of the window.
            window = text[pos:hard_end]
            candidates = [window.rfind("\n\n"), window.rfind(". "), window.rfind("; ")]
            cut = max(candidates)
            if cut >= int(max_chars * 0.55):
                end = pos + cut + (2 if window[cut:cut+2] in {"\n\n", ". ", "; "} else 1)
        piece = text[pos:end].strip()
        if piece:
            chunks.append(Chunk(index=index, text=piece, start=pos, end=end))
            index += 1
        if end >= n:
            break
        pos = max(pos + 1, end - overlap_chars)
    return chunks
