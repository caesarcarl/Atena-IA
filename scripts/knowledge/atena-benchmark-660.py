#!/usr/bin/env python3
import argparse
import importlib.util
import re
import sqlite3
from pathlib import Path


def load_query_module(path: Path):
    spec = importlib.util.spec_from_file_location("atena_query_knowledge", path)
    if spec is None or spec.loader is None:
        raise SystemExit(f"Não consegui carregar {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def parse_bank(path: Path):
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"^\[(DEV|STUDY|EVERYDAY)-(\d{3})\]\n"
        r"TOPICO: (.*?)\n"
        r"MODO_SUGERIDO: (.*?)\n"
        r"PACK: (.*?)\n"
        r"PERGUNTA: (.*?)\n"
        r"BUSCA: (.*?)(?=\n\[|\n\n-+|\Z)",
        flags=re.M | re.S,
    )
    out = []
    for m in pattern.finditer(text):
        out.append({
            "id": f"{m.group(1)}-{m.group(2)}",
            "topic": m.group(3).strip(),
            "mode": m.group(4).strip(),
            "pack": m.group(5).strip(),
            "question": m.group(6).strip(),
        })
    return out


def main():
    ap = argparse.ArgumentParser(description="Benchmark de roteamento/recuperação do banco de 660 perguntas Atena")
    ap.add_argument("--bank", default="knowledge-base/evaluation/ATENA_Questionario_RAG_660_Perguntas_v0.1.txt")
    ap.add_argument("--query-script", default="scripts/knowledge/atena-query-knowledge.py")
    ap.add_argument("--db", default="~/.local/share/atena/knowledge/index/knowledge.sqlite")
    ap.add_argument("--limit", type=int, default=90, help="quantidade de perguntas; use 0 para todas")
    args = ap.parse_args()

    bank = Path(args.bank).expanduser()
    script = Path(args.query_script).expanduser()
    db = Path(args.db).expanduser()
    if not bank.is_file(): raise SystemExit(f"Banco de perguntas ausente: {bank}")
    if not script.is_file(): raise SystemExit(f"Query script ausente: {script}")
    if not db.is_file(): raise SystemExit(f"Knowledge DB ausente: {db}")

    questions = parse_bank(bank)
    if not questions: raise SystemExit("Nenhuma pergunta reconhecida")
    if args.limit > 0:
        # Amostra estratificada por categoria, mantendo ordem e variedade.
        cats = {"DEV": [], "STUDY": [], "EVERYDAY": []}
        for q in questions: cats[q["id"].split("-")[0]].append(q)
        per = max(1, args.limit // 3)
        picked = []
        for cat in ("DEV","STUDY","EVERYDAY"):
            seq = cats[cat]
            if len(seq) <= per: picked.extend(seq); continue
            step = max(1, len(seq)//per)
            picked.extend(seq[::step][:per])
        questions = picked[:args.limit]

    mod = load_query_module(script)
    conn = sqlite3.connect(db)
    conn.row_factory = sqlite3.Row
    route_ok = 0
    evidence = 0
    high = 0
    medium = 0
    failures = []
    try:
        for q in questions:
            route = mod.route(q["question"])
            if route == q["pack"]:
                route_ok += 1
            else:
                failures.append((q["id"], "route", q["pack"], route, q["question"]))
            ranked, _, _, _ = mod.search(conn, q["question"], route, 3, False)
            accepted = [x for x in ranked if x[2] in ("medium","high")]
            if accepted:
                evidence += 1
                if accepted[0][2] == "high": high += 1
                else: medium += 1
    finally:
        conn.close()

    n = len(questions)
    print(f"perguntas={n}")
    print(f"roteamento_correto={route_ok}/{n} ({route_ok*100.0/n:.1f}%)")
    print(f"com_evidencia_local={evidence}/{n} ({evidence*100.0/n:.1f}%)")
    print(f"top_confidence_high={high} medium={medium}")
    if failures:
        print("\nPrimeiros erros de rota:")
        for item in failures[:20]:
            qid, _, expected, got, question = item
            print(f"- {qid}: esperado={expected} obtido={got} | {question}")

if __name__ == "__main__":
    main()
