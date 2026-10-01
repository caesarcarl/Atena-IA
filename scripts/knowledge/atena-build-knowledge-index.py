#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import re
import sqlite3
import sys
import unicodedata
from pathlib import Path

SCHEMA_VERSION = 1

def eprint(*args):
    print(*args, file=sys.stderr)

def normalize_text(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = unicodedata.normalize("NFC", text)
    # Preserve page boundaries (\f), but remove noisy trailing spaces.
    pages = []
    for page in text.split("\f"):
        lines = [re.sub(r"[ \t]+$", "", line) for line in page.splitlines()]
        # Collapse excessive blank lines while preserving paragraphs.
        out = []
        blank = 0
        for line in lines:
            if not line.strip():
                blank += 1
                if blank <= 1:
                    out.append("")
            else:
                blank = 0
                out.append(line)
        pages.append("\n".join(out).strip())
    return "\f".join(pages)

def word_count(text: str) -> int:
    return len(re.findall(r"\w+", text, flags=re.UNICODE))

def choose_cut(text: str, target: int, minimum: int) -> int:
    if len(text) <= target:
        return len(text)
    window = text[:target]
    candidates = [
        window.rfind("\n\n"),
        window.rfind(". "),
        window.rfind("? "),
        window.rfind("! "),
        window.rfind("; "),
        window.rfind("\n"),
        window.rfind(" "),
    ]
    cut = max(candidates)
    if cut < minimum:
        cut = target
    elif window[cut:cut+2] in (". ", "? ", "! ", "; "):
        cut += 1
    return cut

def chunk_page(page_text: str, target_chars: int, overlap_chars: int):
    text = page_text.strip()
    if not text:
        return
    pos = 0
    ordinal = 0
    n = len(text)
    while pos < n:
        remaining = text[pos:]
        cut = choose_cut(remaining, target_chars, max(320, target_chars // 2))
        chunk = remaining[:cut].strip()
        if chunk:
            yield ordinal, chunk
            ordinal += 1
        if pos + cut >= n:
            break
        next_pos = pos + cut - overlap_chars
        if next_pos <= pos:
            next_pos = pos + cut
        pos = next_pos

def load_manifest(path: Path):
    docs = []
    with path.open("r", encoding="utf-8") as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                raise SystemExit(f"{path}:{lineno}: JSON inválido: {exc}") from exc
            required = ("source_id", "pack_id", "kind", "title", "text_path", "sha256")
            missing = [k for k in required if not row.get(k)]
            if missing:
                raise SystemExit(f"{path}:{lineno}: campos ausentes: {', '.join(missing)}")
            docs.append(row)
    return docs

def init_db(conn: sqlite3.Connection):
    conn.executescript("""
    PRAGMA journal_mode=WAL;
    PRAGMA synchronous=NORMAL;
    PRAGMA foreign_keys=ON;

    CREATE TABLE metadata (
        key TEXT PRIMARY KEY,
        value TEXT NOT NULL
    );

    CREATE TABLE documents (
        id INTEGER PRIMARY KEY,
        source_id TEXT NOT NULL UNIQUE,
        pack_id TEXT NOT NULL,
        kind TEXT NOT NULL,
        title TEXT NOT NULL,
        pdf_path TEXT,
        text_path TEXT NOT NULL,
        sha256 TEXT NOT NULL,
        bytes INTEGER NOT NULL DEFAULT 0,
        pages INTEGER,
        authority TEXT NOT NULL DEFAULT 'reference',
        retrieval_policy TEXT NOT NULL DEFAULT 'answer_evidence',
        text_sha256 TEXT,
        word_count INTEGER NOT NULL DEFAULT 0,
        chunk_count INTEGER NOT NULL DEFAULT 0,
        quality_flag TEXT NOT NULL DEFAULT 'ok'
    );

    CREATE TABLE chunks (
        id INTEGER PRIMARY KEY,
        document_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE,
        chunk_uid TEXT NOT NULL UNIQUE,
        source_id TEXT NOT NULL,
        pack_id TEXT NOT NULL,
        kind TEXT NOT NULL,
        title TEXT NOT NULL,
        page INTEGER,
        ordinal INTEGER NOT NULL,
        locator TEXT NOT NULL,
        authority TEXT NOT NULL,
        retrieval_policy TEXT NOT NULL,
        content_sha256 TEXT NOT NULL,
        content TEXT NOT NULL,
        word_count INTEGER NOT NULL
    );

    CREATE VIRTUAL TABLE chunks_fts USING fts5(
        title,
        content,
        source_id UNINDEXED,
        pack_id UNINDEXED,
        kind UNINDEXED,
        retrieval_policy UNINDEXED,
        content='chunks',
        content_rowid='id',
        tokenize='unicode61 remove_diacritics 2'
    );

    CREATE TRIGGER chunks_ai AFTER INSERT ON chunks BEGIN
      INSERT INTO chunks_fts(rowid,title,content,source_id,pack_id,kind,retrieval_policy)
      VALUES (new.id,new.title,new.content,new.source_id,new.pack_id,new.kind,new.retrieval_policy);
    END;

    CREATE TRIGGER chunks_ad AFTER DELETE ON chunks BEGIN
      INSERT INTO chunks_fts(chunks_fts,rowid,title,content,source_id,pack_id,kind,retrieval_policy)
      VALUES('delete',old.id,old.title,old.content,old.source_id,old.pack_id,old.kind,old.retrieval_policy);
    END;

    CREATE TRIGGER chunks_au AFTER UPDATE ON chunks BEGIN
      INSERT INTO chunks_fts(chunks_fts,rowid,title,content,source_id,pack_id,kind,retrieval_policy)
      VALUES('delete',old.id,old.title,old.content,old.source_id,old.pack_id,old.kind,old.retrieval_policy);
      INSERT INTO chunks_fts(rowid,title,content,source_id,pack_id,kind,retrieval_policy)
      VALUES (new.id,new.title,new.content,new.source_id,new.pack_id,new.kind,new.retrieval_policy);
    END;

    CREATE INDEX idx_chunks_pack ON chunks(pack_id);
    CREATE INDEX idx_chunks_source ON chunks(source_id);
    CREATE INDEX idx_chunks_policy ON chunks(retrieval_policy);
    CREATE INDEX idx_chunks_kind ON chunks(kind);
    """)

def build(args):
    manifest = Path(args.manifest).expanduser().resolve()
    output = Path(args.output).expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    docs = load_manifest(manifest)
    if not docs:
        raise SystemExit("Manifesto vazio.")

    tmp = output.with_suffix(output.suffix + ".tmp")
    if tmp.exists():
        tmp.unlink()

    conn = sqlite3.connect(tmp)
    try:
        init_db(conn)
        conn.execute("INSERT INTO metadata(key,value) VALUES(?,?)", ("schema_version", str(SCHEMA_VERSION)))
        conn.execute("INSERT INTO metadata(key,value) VALUES(?,?)", ("chunk_target_chars", str(args.chunk_chars)))
        conn.execute("INSERT INTO metadata(key,value) VALUES(?,?)", ("chunk_overlap_chars", str(args.overlap_chars)))

        total_chunks = 0
        indexed_docs = 0
        skipped = 0

        for row in docs:
            text_path = Path(row["text_path"]).expanduser()
            if not text_path.is_file():
                eprint(f"[WARN] texto ausente: {text_path}")
                skipped += 1
                continue

            raw = text_path.read_text(encoding="utf-8", errors="replace")
            text = normalize_text(raw)
            if not text.strip():
                eprint(f"[WARN] texto vazio: {text_path}")
                skipped += 1
                continue

            pages = text.split("\f")
            actual_pages = len(pages)
            words = word_count(text)
            text_sha = hashlib.sha256(text.encode("utf-8")).hexdigest()

            quality = "ok"
            declared_pages = row.get("pages")
            if actual_pages <= 1 and (declared_pages or 0) > 1:
                quality = "page_boundaries_missing"
            elif (declared_pages or actual_pages) <= 4 and row.get("kind") == "textbook":
                quality = "short_textbook_review"

            cur = conn.execute("""
                INSERT INTO documents(
                    source_id,pack_id,kind,title,pdf_path,text_path,sha256,bytes,pages,
                    authority,retrieval_policy,text_sha256,word_count,quality_flag
                ) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)
            """, (
                row["source_id"], row["pack_id"], row["kind"], row["title"],
                row.get("pdf_path"), str(text_path), row["sha256"], int(row.get("bytes") or 0),
                int(row.get("pages") or actual_pages), row.get("authority") or "reference",
                row.get("retrieval_policy") or "answer_evidence",
                text_sha, words, quality
            ))
            document_id = cur.lastrowid

            doc_chunks = 0
            global_ordinal = 0
            for page_no, page_text in enumerate(pages, 1):
                for _, chunk in chunk_page(page_text, args.chunk_chars, args.overlap_chars):
                    content_sha = hashlib.sha256(chunk.encode("utf-8")).hexdigest()
                    uid_material = f"{row['source_id']}|{page_no}|{global_ordinal}|{content_sha}"
                    chunk_uid = hashlib.sha256(uid_material.encode("utf-8")).hexdigest()[:32]
                    locator = f"page:{page_no}"
                    conn.execute("""
                        INSERT INTO chunks(
                            document_id,chunk_uid,source_id,pack_id,kind,title,page,ordinal,
                            locator,authority,retrieval_policy,content_sha256,content,word_count
                        ) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)
                    """, (
                        document_id, chunk_uid, row["source_id"], row["pack_id"], row["kind"],
                        row["title"], page_no, global_ordinal, locator,
                        row.get("authority") or "reference",
                        row.get("retrieval_policy") or "answer_evidence",
                        content_sha, chunk, word_count(chunk)
                    ))
                    global_ordinal += 1
                    doc_chunks += 1

            conn.execute("UPDATE documents SET chunk_count=? WHERE id=?", (doc_chunks, document_id))
            total_chunks += doc_chunks
            indexed_docs += 1
            print(f"[INDEX] {row['source_id']}: {doc_chunks} chunks | {quality}")

        conn.execute("INSERT INTO metadata(key,value) VALUES(?,?)", ("documents", str(indexed_docs)))
        conn.execute("INSERT INTO metadata(key,value) VALUES(?,?)", ("chunks", str(total_chunks)))
        conn.commit()

        # Integrity checks before atomically replacing the live DB.
        integrity = conn.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise SystemExit(f"SQLite integrity_check falhou: {integrity}")
        fts_rows = conn.execute("SELECT count(*) FROM chunks_fts").fetchone()[0]
        chunk_rows = conn.execute("SELECT count(*) FROM chunks").fetchone()[0]
        if fts_rows != chunk_rows:
            raise SystemExit(f"FTS inconsistente: chunks={chunk_rows}, fts={fts_rows}")
    finally:
        conn.close()

    os.replace(tmp, output)
    print(f"[OK] DB: {output}")
    print(f"[OK] documentos={indexed_docs} chunks={total_chunks} ignorados={skipped}")

def main():
    ap = argparse.ArgumentParser(description="Constrói o índice persistente de conhecimento da Atena.")
    ap.add_argument(
        "--manifest",
        default="~/.local/share/atena/knowledge-source/_meta/documents.jsonl",
        help="documents.jsonl produzido pelo fast-track"
    )
    ap.add_argument(
        "--output",
        default="~/.local/share/atena/knowledge/index/knowledge.sqlite",
        help="SQLite FTS5 de saída"
    )
    ap.add_argument("--chunk-chars", type=int, default=1600)
    ap.add_argument("--overlap-chars", type=int, default=220)
    args = ap.parse_args()
    if args.chunk_chars < 700:
        ap.error("--chunk-chars deve ser >= 700")
    if args.overlap_chars < 0 or args.overlap_chars >= args.chunk_chars // 2:
        ap.error("--overlap-chars deve ser >= 0 e menor que metade do chunk")
    build(args)

if __name__ == "__main__":
    main()
