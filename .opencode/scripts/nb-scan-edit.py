#!/usr/bin/env python3
"""nb-scan-edit.py — mechanical notebook surgery for the mesh-convergence scan.

Importable helpers (used by the per-form driver scripts) plus a small CLI.
All edits are structural only: no existing cell is rewritten except the run
cell, which gains the scan-case registrations, and markdown sentences that
must stay in phase with the new layout. Notebook JSON style is preserved
(nbformat: indent=1, raw UTF-8, trailing newline).
"""
import json
import sys
from pathlib import Path


def load(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save(nb, path):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(nb, f, indent=1, ensure_ascii=False)
        f.write("\n")


def get_src(cell):
    s = cell["source"]
    return "".join(s) if isinstance(s, list) else s


def set_src(cell, text):
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    cell["source"] = [ln + "\n" for ln in lines[:-1]] + ([lines[-1]] if lines else [])


def next_ids(nb, count):
    """Generate `count` unique cell ids, continuing the existing scheme."""
    used = set()
    mx = 0
    for c in nb["cells"]:
        cid = c.get("id", "")
        used.add(cid)
        if cid.startswith("lime") and cid[4:].isdigit():
            mx = max(mx, int(cid[4:]))
    out = []
    for i in range(count):
        n = mx + 1 + i
        cid = f"lime{n:04d}"
        while cid in used:
            n += 1
            cid = f"lime{n:04d}"
        used.add(cid)
        out.append(cid)
    return out


def code_cell(cid):
    return {"cell_type": "code", "execution_count": None, "id": cid,
            "metadata": {}, "outputs": [], "source": None}


def md_cell(cid):
    return {"cell_type": "markdown", "id": cid, "metadata": {}, "source": None}


def insert_registration(nb_path, block):
    """Insert `block` (text) into the run cell, just above the line issuing
    run.printCases() — or, failing that, run.runCases(). The scan cases must
    be registered (and partitioned) before the single runCases() call."""
    nb = load(nb_path)
    target = None
    for cell in nb["cells"]:
        if cell["cell_type"] != "code":
            continue
        src = get_src(cell)
        if "run.printCases()" in src or "run.runCases(" in src:
            target = cell
            break
    if target is None:
        raise SystemExit("nb-scan-edit: no run cell (printCases/runCases) found")
    src = get_src(target)
    lines = src.split("\n")
    for i, ln in enumerate(lines):
        if "run.printCases()" in ln or "run.runCases(" in ln:
            lines.insert(i, block.rstrip("\n"))
            break
    set_src(target, "\n".join(lines))
    save(nb, nb_path)
    print(f"nb-scan-edit: registration inserted in {nb_path}")


def insert_cells_before_perf(nb_path, md_text, code_text):
    """Insert (markdown, code) scan cells before the Computer Performance
    section (or the tablePerf cell when the header is absent)."""
    nb = load(nb_path)
    idx = None
    for i, cell in enumerate(nb["cells"]):
        src = get_src(cell)
        if "Computer Performance" in src or "tablePerf" in src:
            idx = i
            break
    if idx is None:
        idx = len(nb["cells"])
    cid_md, cid_code = next_ids(nb, 2)
    mdc = md_cell(cid_md)
    set_src(mdc, md_text)
    cdc = code_cell(cid_code)
    set_src(cdc, code_text)
    nb["cells"][idx:idx] = [mdc, cdc]
    save(nb, nb_path)
    print(f"nb-scan-edit: scan cells inserted at position {idx} of {nb_path}")


def replace_markdown(nb_path, old, new, count=1):
    """Replace `old` by `new` inside markdown cells (`count` times)."""
    nb = load(nb_path)
    done = 0
    for cell in nb["cells"]:
        if done >= count:
            break
        if cell["cell_type"] != "markdown":
            continue
        src = get_src(cell)
        if old in src:
            set_src(cell, src.replace(old, new))
            done += 1
    if done != count:
        raise SystemExit(f"nb-scan-edit: replace_markdown: found {done}/{count} "
                         f"occurrences of: {old[:70]!r}")
    save(nb, nb_path)
    print(f"nb-scan-edit: markdown updated ({done} replacement(s))")


def main():
    """CLI: nb-scan-edit.py <cmd> <notebook> [args...]

    cmds:
      insert-registration NB BLOCK_FILE
      insert-cells NB MD_FILE CODE_FILE
      replace-markdown NB OLD_FILE NEW_FILE [COUNT]
    """
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cmd, nb_path = sys.argv[1], sys.argv[2]
    if cmd == "insert-registration":
        insert_registration(nb_path, Path(sys.argv[3]).read_text())
    elif cmd == "insert-cells":
        insert_cells_before_perf(nb_path, Path(sys.argv[3]).read_text(),
                                 Path(sys.argv[4]).read_text())
    elif cmd == "replace-markdown":
        replace_markdown(nb_path, Path(sys.argv[3]).read_text(),
                        Path(sys.argv[4]).read_text(),
                        int(sys.argv[5]) if len(sys.argv) > 5 else 1)
    else:
        print(f"nb-scan-edit: unknown command {cmd}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
