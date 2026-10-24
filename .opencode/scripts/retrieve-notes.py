#!/usr/bin/env python3
"""Retrieve relevant sections from the trust-LiMe knowledge base.

Usage:
    retrieve-notes.py <query words...> [--top N] [--max-lines M] [--list]
    retrieve-notes.py --list [file]

Searches KERNEL_REUSE_NOTES.md, PLAN_*.md and docs/content/*.md, splits them
into heading-delimited sections, ranks by query-term frequency, and prints the
best sections with their source location.
"""
import argparse
import glob
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
    # drop empty trailing
    return [(lvl, title, body) for (lvl, title, body) in sections]


def score_section(title, body, terms):
    text = (title + "\n" + "\n".join(body)).lower()
    s = 0
    for t in terms:
        s += text.count(t.lower())
    # title boost
    tl = title.lower()
    for t in terms:
        if t.lower() in tl:
            s += 3
    return s


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

    terms = args.query
    # Build candidate sections
    cands = []
    for path in files:
        for lvl, title, body in parse_sections(path):
            sc = score_section(title, body, terms)
            if sc > 0:
                cands.append((sc, path, lvl, title, body))

    cands.sort(key=lambda x: (-x[0], x[1], x[3]))

    if not cands:
        print("No matching sections found.", file=sys.stderr)
        sys.exit(1)

    shown = 0
    for sc, path, lvl, title, body in cands:
        if shown >= args.top:
            break
        rel = os.path.relpath(path, REPO)
        print(f"\n===== {rel} :: {title}  (score {sc}) =====")
        # trim body
        out = body[: args.max_lines]
        print("\n".join(out))
        if len(body) > args.max_lines:
            print(f"... [{len(body) - args.max_lines} more lines truncated]")
        shown += 1

    print(f"\n--- showing {shown} of {len(cands)} matching sections ---", file=sys.stderr)


if __name__ == "__main__":
    main()