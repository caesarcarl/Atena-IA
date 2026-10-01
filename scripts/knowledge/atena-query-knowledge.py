#!/usr/bin/env python3
import argparse
import json
import re
import sqlite3
import unicodedata
from pathlib import Path

STOPWORDS = {
    "a","ao","aos","as","da","das","de","do","dos","e","em","essa","esse","esta","este",
    "isso","isto","na","nas","no","nos","o","os","ou","para","por","que","se","um","uma",
    "como","porque","qual","quais","quando","onde","com",
    "the","an","and","or","of","in","on","to","for","with","what","how","why","is","are"
}

EXPANSIONS = {
    "ponteiro": ["pointer", "pointers", "address", "addresses"],
    "ponteiros": ["pointer", "pointers", "address", "addresses"],
    "malloc": ["malloc", "calloc", "realloc", "free", "dynamic memory"],
    "memoria": ["memory"],
    "dinamica": ["dynamic"],
    "busca": ["search"],
    "largura": ["breadth", "breadth first search", "bfs"],
    "algoritmo": ["algorithm", "algorithms"],
    "algoritmos": ["algorithm", "algorithms"],
    "relacoes": ["relations", "relation"],
    "relacao": ["relation", "relations"],
    "funcoes": ["functions", "function"],
    "funcao": ["function", "functions"],
    "conjuntos": ["sets", "set"],
    "conjunto": ["set", "sets"],
    "matematica": ["mathematics", "math"],
    "discreta": ["discrete"],
    "retornam": ["return", "returns"],
    "retorna": ["return", "returns"],
    "valores": ["values", "value"],
    "valor": ["value", "values"],
    "fila": ["queue"],
    "grafo": ["graph", "graphs"],
    "grafos": ["graph", "graphs"],
}

def fold(s: str) -> str:
    s = unicodedata.normalize("NFKD", s.lower())
    return "".join(c for c in s if not unicodedata.combining(c))

def raw_tokens(q: str):
    return [fold(x) for x in re.findall(r"[\w+#.-]+", q, flags=re.UNICODE)]

def clean_tokens(q: str):
    out = []
    seen = set()
    for t in raw_tokens(q):
        if t in STOPWORDS:
            continue
        if len(t) < 2 and t not in {"c"}:
            continue
        if t not in seen:
            seen.add(t)
            out.append(t)
    return out

def expand_terms(tokens):
    out = []
    seen = set()
    def add(term):
        term = fold(term).strip()
        if not term or term in seen:
            return
        seen.add(term)
        out.append(term)
    for t in tokens:
        add(t)
        for e in EXPANSIONS.get(t, []):
            add(e)
    ts = set(tokens)
    if {"busca","largura"} <= ts:
        for e in ["breadth first search","breadth-first search","bfs","graph","queue"]:
            add(e)
    if {"matematica","discreta"} <= ts:
        for e in ["discrete mathematics","sets","relations","functions"]:
            add(e)
    if "malloc" in ts or "ponteiro" in ts or "ponteiros" in ts:
        for e in ["pointer","pointers","dynamic memory","memory allocation","free"]:
            add(e)
    return out

def route(query: str):
    t = set(clean_tokens(query))
    if any(x in t for x in {
        "python","malloc","ponteiro","ponteiros","algoritmo","algoritmos","c++","cpp",
        "shell","bash","linux","git","kernel","debug","depuracao","programacao"
    }):
        return "atena.dev"
    if any(x in t for x in {
        "matematica","fisica","quimica","biologia","historia","geografia","filosofia",
        "sociologia","calculo","algebra","estatistica","vestibular","enem","teorema",
        "conjuntos","relacoes","funcoes","estudar","estudo"
    }):
        return "atena.study"
    if any(x in t for x in {
        "email","planilha","documento","apresentacao","arquivo","wifi","impressora",
        "trabalho","casa","computador","celular","produtividade","office","libreoffice"
    }):
        return "atena.everyday"
    return "atena.common"

def preferred_sources(query: str):
    t = set(clean_tokens(query))
    pref = []
    avoid = []
    special = {}
    if "python" in t:
        pref += ["dev.python."]
    if "malloc" in t or "ponteiro" in t or "ponteiros" in t:
        pref += ["dev.c.", "dev.cpp."]
        avoid += ["dev.python."]
        special["c_memory"] = True
    if "algoritmo" in t or "algoritmos" in t or ({"busca","largura"} <= t):
        pref += ["dev.algorithms."]
        special["algorithms"] = True
    if "matematica" in t or "discreta" in t or "conjuntos" in t or "relacoes" in t:
        pref += ["study.math."]
        special["study_math"] = True
    return pref, avoid, special

def fts_expr(terms):
    atoms = []
    for term in terms:
        words = [w for w in re.split(r"\s+", term) if w]
        words = [re.sub(r"[^0-9a-zA-Z_À-ÿ+#]+", "", w) for w in words]
        words = [w for w in words if w]
        if not words:
            continue
        if len(words) == 1:
            atoms.append(f'"{words[0]}"')
        else:
            atoms.append('"' + " ".join(words) + '"')
    return " OR ".join(atoms[:32])

def occurrences(haystack: str, terms):
    h = fold(haystack)
    hits = []
    for t in terms:
        ft = fold(t)
        if ft and ft in h:
            hits.append(ft)
    return hits

def score_row(row, original_terms, expanded_terms, pref, avoid, special):
    text = f"{row['title']} {row['content']}"
    orig_hits = occurrences(text, original_terms)
    exp_hits = occurrences(text, expanded_terms)
    title_hits = occurrences(row["title"], expanded_terms)

    bm = float(row["rank"] or 0.0)
    lexical = max(0.0, -bm)

    score = lexical
    score += 3.0 * len(set(orig_hits))
    score += 1.4 * len(set(exp_hits))
    score += 2.5 * len(set(title_hits))

    sid = row["source_id"]
    if any(sid.startswith(p) for p in pref):
        score += 14.0
    if any(sid.startswith(p) for p in avoid):
        score -= 12.0
    if row["quality_flag"] == "short_textbook_review":
        score -= 6.0

    folded = fold(text)
    if special.get("c_memory"):
        if "malloc" in folded:
            score += 16.0
        if "pointer" in folded or "ponteiro" in folded:
            score += 9.0
        if "dynamic memory" in folded or "memoria dinamica" in folded:
            score += 7.0
    if special.get("algorithms"):
        if "breadth first search" in folded or "breadth-first search" in folded or re.search(r"\bbfs\b", folded):
            score += 18.0
        if sid.startswith("dev.algorithms."):
            score += 8.0
    if special.get("study_math"):
        if any(x in folded for x in ["set", "sets", "relation", "relations", "function", "functions"]):
            score += 8.0

    coverage_base = max(1, min(len(set(expanded_terms)), 10))
    coverage = min(1.0, len(set(exp_hits)) / coverage_base)

    if score >= 30 and coverage >= 0.20:
        confidence = "high"
    elif score >= 16 and coverage >= 0.10:
        confidence = "medium"
    else:
        confidence = "low"

    return score, coverage, confidence

def search(conn, query, pack, limit, include_methodology=False, candidate_limit=80):
    original = clean_tokens(query)
    expanded = expand_terms(original)
    expr = fts_expr(expanded)
    if not expr:
        return [], original, expanded, ""

    params = [expr]
    where = ["chunks_fts MATCH ?"]
    if not include_methodology:
        where.append("c.retrieval_policy='answer_evidence'")
    if pack == "atena.common":
        where.append("c.pack_id='atena.common'")
    else:
        where.append("c.pack_id IN (?, 'atena.common')")
        params.append(pack)
    params.append(candidate_limit)

    rows = conn.execute(f"""
        SELECT
          c.chunk_uid,c.source_id,c.pack_id,c.kind,c.title,c.page,c.locator,
          c.retrieval_policy,c.content,d.quality_flag,
          bm25(chunks_fts, 2.0, 1.0) AS rank
        FROM chunks_fts
        JOIN chunks c ON c.id=chunks_fts.rowid
        JOIN documents d ON d.id=c.document_id
        WHERE {' AND '.join(where)}
        ORDER BY rank
        LIMIT ?
    """, params).fetchall()

    pref, avoid, special = preferred_sources(query)
    ranked = []
    for row in rows:
        s, coverage, confidence = score_row(row, original, expanded, pref, avoid, special)
        ranked.append((s, coverage, confidence, row))
    ranked.sort(key=lambda item: (-item[0], -item[1], float(item[3]["rank"] or 0.0)))
    return ranked[:limit], original, expanded, expr

def main():
    ap = argparse.ArgumentParser(description="Atena RAG lexical router/reranker v0.2")
    ap.add_argument("query", nargs="+")
    ap.add_argument("--db", default="~/.local/share/atena/knowledge/index/knowledge.sqlite")
    ap.add_argument("--pack", default=None)
    ap.add_argument("--limit", type=int, default=5)
    ap.add_argument("--include-methodology", action="store_true")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--min-confidence", choices=["low","medium","high"], default="medium")
    args = ap.parse_args()

    query = " ".join(args.query)
    pack = args.pack or route(query)
    db = Path(args.db).expanduser()
    if not db.is_file():
        raise SystemExit(f"Índice não encontrado: {db}")

    conn = sqlite3.connect(db)
    conn.row_factory = sqlite3.Row
    try:
        ranked, original, expanded, expr = search(conn, query, pack, args.limit, args.include_methodology)
    finally:
        conn.close()

    order = {"low":0, "medium":1, "high":2}
    accepted = [x for x in ranked if order[x[2]] >= order[args.min_confidence]]

    if args.json:
        payload = {
            "query": query,
            "route": pack,
            "terms": original,
            "expanded_terms": expanded,
            "fts": expr,
            "hits": []
        }
        for score, coverage, confidence, r in accepted:
            payload["hits"].append({
                "score": round(score, 4),
                "coverage": round(coverage, 4),
                "confidence": confidence,
                "chunk_uid": r["chunk_uid"],
                "source_id": r["source_id"],
                "pack_id": r["pack_id"],
                "kind": r["kind"],
                "title": r["title"],
                "page": r["page"],
                "locator": r["locator"],
                "quality_flag": r["quality_flag"],
                "content": r["content"],
            })
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        return

    print(f"[ROUTE] {pack}")
    print(f"[TERMS] {' | '.join(original)}")
    extra = [x for x in expanded if x not in original]
    if extra:
        print(f"[EXPAND] {' | '.join(extra)}")
    print(f"[CANDIDATES] {len(ranked)}")

    if not accepted:
        print("[CONFIDENCE_GATE] nenhum chunk com confiança suficiente")
        print("[ACTION] não tratar a base local como evidência para esta pergunta")
        return

    for i, (score, coverage, confidence, r) in enumerate(accepted, 1):
        content = re.sub(r"\s+", " ", r["content"]).strip()
        if len(content) > 750:
            content = content[:747] + "..."
        print()
        print(
            f"#{i} score={score:.2f} confidence={confidence} coverage={coverage:.2f} "
            f"pack={r['pack_id']} kind={r['kind']} quality={r['quality_flag']}"
        )
        print(f"source={r['source_id']} | {r['title']} | {r['locator']}")
        print(content)

if __name__ == "__main__":
    main()
