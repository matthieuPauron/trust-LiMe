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
    "unknown": " ? not run",
    "not run": " ? not run",
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

    # --- Unit tests ---------------------------------------------------------
    lines.append("## Unit tests (GoogleTest)")
    lines.append("")
    lines.append("| Source file | gtest suites | Contains | Status |")
    lines.append("|---|---|---|---|")
    ut = status.get("unit_tests", {})
    for u in units:
        st, tests, failures, seen = unit_status_for(status, u["suites"])
        if seen and tests > 0:
            contains = f"{tests} tests" + (f", {failures} failing" if failures else "")
        else:
            contains = ", ".join(u["suites"]) if u["suites"] else "(none)"
        status_cell = glyph(st)
        if seen and tests > 0:
            status_cell += f" ({tests - failures}/{tests})"
        lines.append(f"| `{u['file']}` | {', '.join('`' + s + '`' for s in u['suites']) or '—'} | {contains} | {status_cell} |")
    lines.append("")
    agg = ut.get("status", "unknown")
    lines.append(f"Aggregate unit-test status: **{glyph(agg)}**"
                 + (f" (return code {ut['returncode']})" if "returncode" in ut else "")
                 + (f" — {ut.get('note')}" if ut.get("note") else "") + ".")
    lines.append("")

    # --- Validation forms ---------------------------------------------------
    lines.append("## Validation forms (notebooks)")
    lines.append("")
    lines.append("| Category | Form | Datasets (src/) | Contains | Status |")
    lines.append("|---|---|---|---|---|")
    vmap = status.get("validation", {}) or {}
    for f in forms:
        info = vmap.get(f["key"], {}) or {}
        st = info.get("status", "unknown")
        ds = ", ".join(f"`{d}`" for d in f["datasets"]) if f["datasets"] else "—"
        contains = f"{len(f['datasets'])} dataset template(s)" if f["datasets"] else "—"
        note = info.get("note")
        cell = glyph(st)
        if note:
            cell += f" ({note})"
        lines.append(f"| `{f['category']}` | [`{f['form']}`]({f['rel']}/{f['form']}.ipynb) | {ds} | {contains} | {cell} |")
    lines.append("")
    n = len(forms)
    npass = sum(1 for f in forms if (vmap.get(f["key"], {}) or {}).get("status") == "pass")
    lines.append(f"Validation forms: **{npass}/{n}** passing (status recorded).")
    lines.append("")

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w") as fh:
        fh.write("\n".join(lines))


if __name__ == "__main__":
    main()
