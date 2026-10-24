#!/usr/bin/env python3
"""Retrieve relevant sections from the trust-LiMe knowledge base.

Usage:
    retrieve-notes.py <query words...> [--top N] [--max-lines M] [--list]
    retrieve-notes.py --list [file]

Searches KERNEL_REUSE_NOTES.md, PLAN_*.md and docs/content/*.md, splits them
into heading-delimited sections, ranks them with BM25, and prints the best
sections with their source location.
"""
import argparse
import glob
import math
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def default_files():
    out = []
    krn = os.path.join(REPO, "KERNEL_REUSE_NOTES.md")
    if os.path.exists(krn):
        out.append(krn)
    for p in sorted(glob.glob(os.path.join(REPO, "PLAN_*.md"))):
        out.append(p)
    for p in sorted(glob.glob(os.path.join(REPO, "docs", "content", "*.md"))):
        out.append(p)
    return out


def parse_sections(path):
    """Return list of (level, title, body_lines)."""
    sections = []
    cur = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"^(#{1,4})\s+(.*)$", line.rstrip("\n"))
            if m:
                if cur is not None:
                    sections.append(cur)
                cur = (len(m.group(1)), m.group(2).strip(), [])
            else:
                if cur is None:
                    cur = (0, os.path.basename(path), [])
                cur[2].append(line.rstrip("\n"))
        if cur is not None:
            sections.append(cur)
    return [(lvl, title, body) for (lvl, title, body) in sections]


def tokenize(text):
    return re.findall(r"[a-z0-9_]+", text.lower())


def build_corpus(section_list):
    toks = [tokenize(s[2] + " " + " ".join(s[3])) for s in section_list]
    n = len(toks)
    avgdl = (sum(len(t) for t in toks) / n) if n else 1.0
    df = {}
    for t in toks:
        for term in set(t):
            df[term] = df.get(term, 0) + 1
    return toks, n, avgdl, df


def bm25_score(q_tokens, doc_tokens, n, avgdl, df, k1=1.5, b=0.75):
    score = 0.0
    dl = len(doc_tokens)
    tf_map = {}
    for term in doc_tokens:
        tf_map[term] = tf_map.get(term, 0) + 1
    for q in set(q_tokens):
        if q not in tf_map:
            continue
        tf = tf_map[q]
        nq = df.get(q, 0)
        idf = math.log((n - nq + 0.5) / (nq + 0.5) + 1.0)
        denom = tf + k1 * (1.0 - b + b * dl / avgdl)
        score += idf * (tf * (k1 + 1.0)) / denom
    return score


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("query", nargs="*", help="query terms")
    ap.add_argument("--top", type=int, default=6, help="number of sections to show")
    ap.add_argument("--max-lines", type=int, default=40, help="max body lines per section")
    ap.add_argument("--list", action="store_true", help="list section headings only")
    ap.add_argument("--files", nargs="*", help="override file list")
    args = ap.parse_args()

    files = args.files if args.files else default_files()

    if args.list:
        for path in files:
            print(f"=== {os.path.relpath(path, REPO)} ===")
            for lvl, title, body in parse_sections(path):
                print(f"{'#' * max(lvl, 1)} {title}")
        return

    if not args.query:
        ap.error("provide a query (or --list)")

    # Collect all sections first (needed for corpus stats).
    all_sections = []
    for path in files:
        for lvl, title, body in parse_sections(path):
            all_sections.append((path, lvl, title, body))

    toks, n, avgdl, df = build_corpus(all_sections)
    q_tokens = tokenize(" ".join(args.query))

    scored = []
    for (path, lvl, title, body), dt in zip(all_sections, toks):
        sc = bm25_score(q_tokens, dt, n, avgdl, df)
        # title boost
        tl = tokenize(title)
        for q in set(q_tokens):
            if q in tl:
                sc += 2.0
        if sc > 0:
            scored.append((sc, path, lvl, title, body))

    scored.sort(key=lambda x: (-x[0], x[1], x[3]))

    if not scored:
        print("No matching sections found.", file=sys.stderr)
        sys.exit(1)

    shown = 0
    for sc, path, lvl, title, body in scored:
        if shown >= args.top:
            break
        rel = os.path.relpath(path, REPO)
        print(f"\n===== {rel} :: {title}  (score {sc:.2f}) =====")
        out = body[: args.max_lines]
        print("\n".join(out))
        if len(body) > args.max_lines:
            print(f"... [{len(body) - args.max_lines} more lines truncated]")
        shown += 1

    print(f"\n--- showing {shown} of {len(scored)} matching sections ---", file=sys.stderr)


if __name__ == "__main__":
    main()
EOF