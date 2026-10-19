#!/usr/bin/env python3
"""nb-compare.py — validate a notebook operation against its baseline snapshot.

Implements the trust-LiMe RULE: an operation on a notebook is validated if the
computations of the notebook generate the same result as before the operation.

Two comparisons are made:

1. Cell outputs — every code cell of the baseline notebook whose source is
   unchanged by the operation must produce identical outputs (stream text and
   embedded images) in the new executed notebook. Cells containing
   ``tablePerf`` are skipped (timings vary run to run). Code cells whose source
   is *absent* from the new notebook are reported: the only acceptable case is
   the run cell (it gains the mesh-scan case registrations); its computation
   is covered by comparison 2. Any other modified/deleted cell is a failure.

2. Probe files — every baseline ``PAR_*.son`` snapshot (values and effective
   probe coordinates, i.e. the ``# Temps x= ...`` header lines) must be
   reproduced exactly by the new run.

Exit status: 0 = operation validated, 1 = regression / mismatch.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path


def cell_source(cell):
    src = cell.get("source", [])
    return "".join(src) if isinstance(src, list) else src


def cell_key(cell):
    return hashlib.sha256(cell_source(cell).encode("utf-8")).hexdigest()


def outputs_of(cell):
    """Return (stream_text, list_of_png_b64, list_of_error_tracebacks)."""
    streams, images, errors = [], [], []

    def _flat(x):
        return "".join(x) if isinstance(x, list) else x

    for o in cell.get("outputs", []):
        t = o.get("output_type")
        if t == "stream":
            streams.append(_flat(o.get("text", "")))
        elif t in ("display_data", "execute_result"):
            png = (o.get("data") or {}).get("image/png")
            if png:
                images.append(_flat(png))
        elif t == "error":
            errors.append("\n".join(o.get("traceback", [])))
    text = "\n".join(streams)
    # Volatile output, like the tablePerf timings: the trustutils run
    # summary line carries the wall-clock time ("... run in 1.4s"), which
    # legitimately varies run to run and is not a computational result.
    text = "\n".join(ln for ln in text.split("\n")
                    if not ln.strip().startswith("=> A total of"))
    return text, images, errors


def load_nb(path):
    with open(path, "r", encoding="utf-8") as f:
        nb = json.load(f)
    return [c for c in nb.get("cells", []) if c.get("cell_type") == "code"]


def compare_notebooks(old_path, new_path):
    """Compare executed notebooks. Returns (ok, n_compared, problems)."""
    old_cells = load_nb(old_path)
    new_cells = load_nb(new_path)

    # Index the new notebook by source hash (several cells may share a source).
    new_by_key = {}
    for c in new_cells:
        new_by_key.setdefault(cell_key(c), []).append(c)

    problems = []
    n_compared = 0
    for i, old in enumerate(old_cells):
        src = cell_source(old)
        if "tablePerf" in src:
            continue  # performance table: timings legitimately vary
        bucket = new_by_key.get(cell_key(old))
        if not bucket:
            if "run.addCase(" in src:
                # The run cell: gains the mesh-scan registrations. Its actual
                # computation (the baseline case) is verified via the .son
                # comparison. Informational only.
                print(f"  note: run cell modified (source changed) — "
                      f"validated via .son comparison [old cell #{i}]")
                continue
            preview = src.strip().replace("\n", " / ")[:90]
            problems.append(f"original cell #{i} changed or removed: {preview}")
            continue
        new = bucket.pop(0)
        n_compared += 1
        old_stream, old_png, old_err = outputs_of(old)
        new_stream, new_png, new_err = outputs_of(new)
        if old_err or new_err:
            if old_err != new_err:
                problems.append(f"cell #{i}: error output changed")
        if old_stream != new_stream:
            problems.append(f"cell #{i}: stream output differs\n"
                           f"    --- baseline ---\n{_indent(old_stream)}\n"
                           f"    --- new ---\n{_indent(new_stream)}")
        if old_png != new_png:
            problems.append(f"cell #{i}: image output differs")
    return (not problems), n_compared, problems


def _indent(text, n=6):
    pad = " " * n
    return "\n".join(pad + line for line in text.splitlines()) if text else pad + "<empty>"


def parse_son(path):
    """Return (list_of_exact_header_lines, list_of_float_rows)."""
    headers, rows = [], []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("#"):
                headers.append(line)
            elif line.strip():
                try:
                    rows.append([float(x) for x in line.split()])
                except ValueError:
                    headers.append(line)  # unexpected non-numeric line: compare exactly
    return headers, rows


def compare_sons(old_dir, new_dir):
    """Compare baseline PAR_*.son snapshots against the fresh run's ones."""
    old_dir, new_dir = Path(old_dir), Path(new_dir)
    problems = []
    old_sons = sorted(p for p in old_dir.glob("*.son"))
    n = 0
    for old_file in old_sons:
        new_file = new_dir / old_file.name
        if not new_file.is_file():
            problems.append(f"probe file missing in new run: {old_file.name}")
            continue
        n += 1
        old_h, old_r = parse_son(old_file)
        new_h, new_r = parse_son(new_file)
        if old_h != new_h:
            problems.append(f"{old_file.name}: header/effective-probe lines differ")
        if old_r != new_r:
            problems.append(f"{old_file.name}: probe values differ")
    return (not problems), n, problems


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("old", help="baseline executed notebook (.ipynb)")
    ap.add_argument("new", help="newly executed notebook (.ipynb)")
    ap.add_argument("--son-old", required=True,
                    help="directory with the baseline PAR_*.son snapshots")
    ap.add_argument("--son-new", required=True,
                    help="directory with the fresh run's PAR_*.son files")
    args = ap.parse_args()

    ok_nb, n_cells, nb_problems = compare_notebooks(args.old, args.new)
    print(f"notebook cells compared identical: {n_cells}")
    ok_son, n_sons, son_problems = compare_sons(args.son_old, args.son_new)
    print(f"probe files compared identical:    {n_sons}")

    problems = nb_problems + son_problems
    if problems:
        print(f"\nNOT VALIDATED — {len(problems)} difference(s):")
        for p in problems:
            print(f"  - {p}")
        return 1
    print("VALIDATED — baseline computations reproduce exactly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
