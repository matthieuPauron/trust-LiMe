#!/usr/bin/env python3
"""Generate a Doxygen Markdown dashboard page from the repo's test/validation
inventory and .opencode/validation_status.json.

Output is a generated page (lands under docs/build/generated_content/) so it
is never hand-edited.
"""
import argparse
import json
import os
import re
import glob


def load_status(path):
    if not os.path.exists(path):
        return {}
    try:
        with open(path) as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return {}


STATUS_GLYPH = {
    "pass": "✅ pass",
    "fail": "❌ fail",
    "in_progress": "🚧 in progress",
    "unknown": "⬜ not run",
    "not run": "⬜ not run",
}


def glyph(s):
    return STATUS_GLYPH.get((s or "unknown").lower(), f"• {s}")


def scan_unit_tests(repo):
    """Return list of dicts: file, suites(list), source path."""
    out = []
    base = os.path.join(repo, "tests", "UnitTests")
    for path in sorted(glob.glob(os.path.join(base, "unit_*.cpp"))):
        suites = []
        with open(path) as f:
            for line in f:
                m = re.match(r"\s*TEST(?:_F|_P)?\s*\(\s*([A-Za-z0-9_]+)\s*,", line)
                if m and m.group(1) not in suites:
                    suites.append(m.group(1))
        out.append({
            "file": os.path.basename(path),
            "suites": suites,
            "path": os.path.relpath(path, repo),
        })
    return out


def scan_validation(repo):
    """Return list of dicts: category, form, datasets(list of .data basenames), rel path."""
    out = []
    root = os.path.join(repo, "share", "Validation", "Rapports_automatiques")
    if not os.path.isdir(root):
        return out
    for cat in sorted(os.listdir(root)):
        catdir = os.path.join(root, cat)
        if not os.path.isdir(catdir):
            continue
        for form in sorted(os.listdir(catdir)):
            formdir = os.path.join(catdir, form)
            nb = os.path.join(formdir, form + ".ipynb")
            if not os.path.isfile(nb):
                continue
            srcdir = os.path.join(formdir, "src")
            datasets = []
            if os.path.isdir(srcdir):
                datasets = sorted(os.path.basename(p) for p in glob.glob(os.path.join(srcdir, "*.data")))
            out.append({
                "category": cat,
                "form": form,
                "key": f"{cat}/{form}",
                "datasets": datasets,
                "rel": os.path.relpath(formdir, repo),
            })
    return out


def unit_status_for(status, suites):
    ut = status.get("unit_tests", {})
    suite_map = ut.get("suites", {}) or {}
    # aggregate failures across the suites that belong to this file's suites
    failures = 0
    tests = 0
    seen = False
    for s in suites:
        if s in suite_map:
            seen = True
            tests += int(suite_map[s].get("tests", 0) or 0)
            failures += int(suite_map[s].get("failures", 0) or 0)
    if not seen:
        # fall back to aggregate status
        return ut.get("status", "unknown"), tests, failures, False
    st = "pass" if failures == 0 and tests > 0 else ("fail" if failures > 0 else ut.get("status", "unknown"))
    # if the whole run failed (e.g. SIGSEGV), reflect that
    if ut.get("status") in ("fail",) and failures == 0 and tests == 0:
        st = "fail"
    return st, tests, failures, True


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", required=True)
    ap.add_argument("--status", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    repo = os.path.abspath(args.repo)
    status = load_status(args.status)
    updated = status.get("updated", "never")

    units = scan_unit_tests(repo)
    forms = scan_validation(repo)

    vmap = status.get("validation", {}) or {}

    # group forms by category (already sorted within scan_validation)
    by_category = {}
    for f in forms:
        by_category.setdefault(f["category"], []).append(f)

    lines = []
    lines.append("@page TrustLiMe_validation_dashboard Validation dashboard")
    lines.append("")
    lines.append("Auto-generated from the repository's test inventory and "
                 "`.opencode/validation_status.json`. Regenerated on every "
                 "`make docs` / `make run_doxygen`. See "
                 "@ref TrustLiMe_testcases \"Test cases\" and "
                 "@ref TrustLiMe_validation \"Validation cases\" for the "
                 "detailed descriptions.")
    lines.append("")
    lines.append(f"**Last status update:** `{updated}`")
    lines.append("")
    lines.append("Status legend: ✅ pass · ❌ fail · 🚧 in progress · ⬜ not run.")
    lines.append("")

    # --- Summary ------------------------------------------------------------
    lines.append("## Summary")
    lines.append("")
    lines.append("| Category | Forms | ✅ pass | 🚧 in progress | ⬜ not run | ❌ fail |")
    lines.append("|---|---:|---:|---:|---:|---:|")
    totals = {"forms": 0, "pass": 0, "in_progress": 0, "not_run": 0, "fail": 0}
    for cat in sorted(by_category):
        fs = by_category[cat]
        c = {"pass": 0, "in_progress": 0, "not_run": 0, "fail": 0}
        for f in fs:
            st = (vmap.get(f["key"], {}) or {}).get("status", "unknown")
            if st == "pass":
                c["pass"] += 1
            elif st == "in_progress":
                c["in_progress"] += 1
            elif st in ("fail",):
                c["fail"] += 1
            else:
                c["not_run"] += 1
        lines.append(f"| `{cat}` | {len(fs)} | {c['pass']} | {c['in_progress']} | {c['not_run']} | {c['fail']} |")
        totals["forms"] += len(fs)
        totals["pass"] += c["pass"]
        totals["in_progress"] += c["in_progress"]
        totals["not_run"] += c["not_run"]
        totals["fail"] += c["fail"]
    lines.append(f"| **Total** | **{totals['forms']}** | **{totals['pass']}** | "
                 f"**{totals['in_progress']}** | **{totals['not_run']}** | **{totals['fail']}** |")
    lines.append("")

    # --- Needs attention ----------------------------------------------------
    attention = []
    for f in forms:
        if f["key"] not in vmap:
            continue
        info = vmap.get(f["key"], {}) or {}
        st = info.get("status", "unknown")
        if st != "pass":
            attention.append((f, st, info.get("note")))
    if attention:
        lines.append("## Needs attention")
        lines.append("")
        for f, st, note in attention:
            cell = glyph(st)
            if note:
                cell += f" — {note}"
            lines.append(f"- `{f['category']}/{f['form']}`: {cell}")
        lines.append("")

    # --- Unit tests ---------------------------------------------------------
    lines.append("## Unit tests (GoogleTest)")
    lines.append("")
    lines.append("| Source file | gtest suites | Tests | Status |")
    lines.append("|---|---|---:|---|")
    ut = status.get("unit_tests", {})
    for u in units:
        st, tests, failures, seen = unit_status_for(status, u["suites"])
        suites_cell = ", ".join("`" + s + "`" for s in u["suites"]) or "—"
        if seen and tests > 0:
            tests_cell = str(tests)
        else:
            tests_cell = "—"
        status_cell = glyph(st)
        if seen and tests > 0:
            status_cell += f" ({tests - failures}/{tests})"
        lines.append(f"| `{u['file']}` | {suites_cell} | {tests_cell} | {status_cell} |")
    lines.append("")
    agg = ut.get("status", "unknown")
    lines.append(f"Aggregate unit-test status: **{glyph(agg)}**"
                 + (f" (return code {ut['returncode']})" if "returncode" in ut else "")
                 + (f" — {ut.get('note')}" if ut.get("note") else "") + ".")
    lines.append("")

    # --- Validation forms ---------------------------------------------------
    lines.append("## Validation forms (notebooks)")
    lines.append("")
    npass = sum(1 for f in forms if (vmap.get(f["key"], {}) or {}).get("status") == "pass")
    lines.append(f"Validation forms: **{npass}/{len(forms)}** passing (status recorded).")
    lines.append("")

    for cat in sorted(by_category):
        fs = by_category[cat]
        cp = sum(1 for f in fs if (vmap.get(f["key"], {}) or {}).get("status") == "pass")
        lines.append(f"### `{cat}` — {cp}/{len(fs)} passing")
        lines.append("")
        lines.append("| Form | Datasets (src/) | Status |")
        lines.append("|---|---|---|")
        for f in fs:
            info = vmap.get(f["key"], {}) or {}
            st = info.get("status", "unknown")
            ds = ", ".join(f"`{d}`" for d in f["datasets"]) if f["datasets"] else "—"
            cell = glyph(st)
            note = info.get("note")
            if note:
                cell += f" ({note})"
            lines.append(f"| [`{f['form']}`]({f['rel']}/{f['form']}.ipynb) | {ds} | {cell} |")
        lines.append("")

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w") as fh:
        fh.write("\n".join(lines))


if __name__ == "__main__":
    main()
