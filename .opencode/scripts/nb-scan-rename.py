#!/usr/bin/env python3
"""nb-scan-rename.py — convert a form's mesh-convergence scan to the
coarse/standard/fine scale naming on the single-file trustutils template.

Per form (Category/Form under share/Validation/Rapports_automatiques/):

1. src/<case>.data becomes THE trustutils template: the old <case>.tpl.data
   content (a byte-copy of the baseline with $nx/$ny[$/$nz] placeholders)
   replaces the baseline file; the .tpl.data is deleted. ONE data file, with
   $ placeholders, per merged case.
2. The run cell is regenerated: all three scales run the SAME <case>.data via
   run.addCaseFromTemplate into build/<scale>/<case>.data — same data-file
   name across scales, one build subfolder per scale:
     coarse   = the baseline mesh
     standard = the refined mesh (the historical L1 level)
     fine     = the finest mesh (the historical L2 level)
   (the numeric values are spliced from the historical L1/L2 level dicts, so
   the meshes are unchanged — this is a pure refactor).
3. Read-back cells: textual rename L0/L1/L2 -> coarse/standard/fine,
   scan_cases -> cases, build/scan/<lvl> paths -> build/<lvl> paths, and
   read_son() learns to look in build/<scale>/ for the baseline probes.

Variants are auto-detected from the old run cell:
  - "mms3d"   : MMS_3D (4 merged cases, per-case literal mesh dicts);
  - "mms2d"   : MMS_2D (4 merged cases, LEVELS/LEVELSP formula dicts);
  - "coupled" : Coupled_Feedback (adds the twoway_feedback.data case);
  - "single"  : every other positive form.

All parsing asserts happen BEFORE any file is written; the src/ and notebook
writes are atomic (temp file + rename), so a failure leaves the form untouched.

Usage: nb-scan-rename.py <Category>/<Form> [<Category>/<Form> ...]
Exit: 0 = all forms converted, 1 = at least one form aborted (untouched).
"""
import json
import os
import re
import sys
from pathlib import Path

RAPORTS = Path(__file__).resolve().parent.parent.parent / "share" / "Validation" / "Rapports_automatiques"

# Rules shared by every variant (applied to the non-run cells, code+markdown).
COMMON_RULES = [
    ('f"scan/L1"', 'f"standard"'),
    ('f"scan/L2"', 'f"fine"'),
    ('"scan/L1"', '"standard"'),
    ('"scan/L2"', '"fine"'),
    ("scan/{base}/{lvl}", "{base}/{lvl}"),
    ("scan/MMS_", "MMS_"),
    ('"L0"', '"coarse"'),
    ('"L1"', '"standard"'),
    ('"L2"', '"fine"'),
    ("'L0'", "'coarse'"),
    ("'L1'", "'standard'"),
    ("'L2'", "'fine'"),
    ('("", case.dataFileName_)', '("coarse", case.dataFileName_)'),
    ("(L0)", "(coarse)"),
    ("(L1)", "(standard)"),
    ("(L2)", "(fine)"),
    ("L0 -> L1", "coarse -> standard"),
    ("L1 -> L2", "standard -> fine"),
    ("# baseline, build root", "# the baseline mesh (coarse), in build/coarse/"),
    # Size the table's level column for the longer scale labels.
    ("{lvl:<6} ", "{lvl:<9} "),
    # Stale helper text: the .son copy hack is gone; probes live next to the
    # dataset under build/ (build/<scale>/ for the scan levels, root for
    # plain addCase datasets) and read_son() looks in both places.
    ("probe files are written next to the dataset in `src/` and copied over in the\n"
     "run cell below, where `read_son()` expects them.",
     "probe files are written next to the dataset under `build/` (in\n"
     "`build/<scale>/` for the mesh-convergence levels, at the root for plain\n"
     "addCase datasets); `read_son()` looks in both places."),
    # read_son() must find the baseline probes in build/coarse/ (or, for the
    # merged MMS forms, in build/<Case>/coarse/) ...
    ('    path = f"{BUILD}/{case}_{name}.son"',
     '    cands = [f"{BUILD}/coarse/{case}_{name}.son",\n'
     '             f"{BUILD}/{case}_{name}.son"]\n'
     '    if case.startswith("PAR_"):      # per-case subdir (merged MMS forms)\n'
     '        cands.insert(1, f"{BUILD}/{case[4:]}/coarse/{case}_{name}.son")\n'
     '    path = next((p for p in cands if os.path.exists(p)), cands[1])'),
    # ... and the twoway-style variant of the same helper.
    ('    path = f"{BUILD}/{case}_{sonde}.son"',
     '    cands = [f"{BUILD}/coarse/{case}_{sonde}.son",\n'
     '             f"{BUILD}/{case}_{sonde}.son"]\n'
     '    if case.startswith("PAR_"):      # per-case subdir (merged MMS forms)\n'
     '        cands.insert(1, f"{BUILD}/{case[4:]}/coarse/{case}_{sonde}.son")\n'
     '    path = next((p for p in cands if os.path.exists(p)), cands[1])'),
    # os.path.exists needs os in the helper cell.
    ("from trustutils.files import SonFile",
     "import os\nfrom trustutils.files import SonFile"),
]

MD_RULES = [
    (re.compile(r"\bL0\b"), "coarse"),
    (re.compile(r"\bL1\b"), "standard"),
    (re.compile(r"\bL2\b"), "fine"),
    # stale prose describing the historical build/scan/ layout
    ("scan/<case>/<lvl>/", "<case>/<scale>/"),
    ("scan/<lvl>/", "<scale>/"),
]


class Abort(Exception):
    pass


def apply_rules(text, rules):
    for old, new in rules:
        text = text.replace(old, new)
    return text


def apply_md_rules(text):
    for old, new in MD_RULES:
        if hasattr(old, "sub"):  # compiled regex
            text = old.sub(new, text)
        else:  # plain string pair
            text = text.replace(old, new)
    return text


def cell_source(cell):
    src = cell.get("source", [])
    return "".join(src) if isinstance(src, list) else src


def set_cell_source(cell, text):
    cell["source"] = text.splitlines(keepends=True)


# --- parsing helpers ---------------------------------------------------------

def parse_baseline_counts(datapath):
    """(nx, ny[, nz]) from the Nombre_de_Noeuds line of a baseline .data."""
    m = re.search(r"Nombre_de_Noeuds\s+([0-9][0-9\s]*)", datapath.read_text(encoding="utf-8"))
    if not m:
        raise Abort(f"{datapath.name}: no Nombre_de_Noeuds line")
    vals = [int(v) for v in m.group(1).split()]
    if len(vals) not in (2, 3):
        raise Abort(f"{datapath.name}: unexpected node counts {vals}")
    return vals


def parse_levels(run_src, name):
    """Raw text of the `name = {"L1": ..., "L2": ...}` dict (nested ok)."""
    m = re.search(rf"{name}\s*=\s*(\{{[^{{}}]*\}}|\{{.*?\n\}})", run_src, re.DOTALL)
    if not m:
        raise Abort(f"{name} dict not found in the run cell")
    return m.group(1)


def parse_entries(levels_text):
    """[(key, raw_value_text), ...] — scalar expressions or nested dicts."""
    entries = []
    for m in re.finditer(r'"([^"]+)"\s*:\s*(\{[^{}]*\}|[^,\n}]+)', levels_text):
        entries.append((m.group(1), m.group(2).strip()))
    return entries


def eval_levels(entries, namespace):
    return {k: eval(v, {}, dict(namespace)) for k, v in entries}


def parse_count_assignments(run_src):
    """Baseline count assignments (NX0/NY0/NZ0/NXY0/N0/NP0...), verbatim."""
    return re.findall(r"^N[A-Z0-9]*0(?:\s*=\s*N[A-Z0-9]*0|\s*,\s*N[A-Z0-9]*0)*\s*=\s*[^\n]+",
                      run_src, re.M)


def names_in(text):
    """Count-variable names (NX0, NXY0, NZ0, N0, NP0, ...) referenced in text."""
    return set(re.findall(r"\b(N[A-Z0-9]*0)\b", text))


def eval_assignments(assignments):
    """Namespace from pure int count assignments (e.g. 'NXY0 = NZ0 = 5',
    'NX0, NY0 = 17, 65'). Our own files: int chains/tuples only."""
    ns = {}
    for a in assignments:
        if not re.fullmatch(r"N[A-Z0-9]*0(?:\s*[,=]\s*N[A-Z0-9]*0)*\s*=\s*"
                            r"\d+(?:\s*,\s*\d+)*(?:\s*#.*)?", a.strip()):
            raise Abort(f"unsupported count assignment form: {a.strip()!r}")
        exec(a.split("#")[0].strip(), {}, ns)
    return ns


def parse_addcase_baselines(run_src):
    """[(alias, case), ...] from `alias = run.addCase(".", "case.data", ...)`."""
    return re.findall(r'(\w+)\s*=\s*run\.addCase\("\.",\s*"(\w+)\.data"', run_src)


def parse_mms3d_levels(run_src):
    """{case: {"L1": dic_text, "L2": dic_text}} from the per-case registrations."""
    out = {}
    for m in re.finditer(r'addCaseFromTemplate\("(\w+)\.tpl\.data",\s*'
                         r'f"scan/(\w+)/(L1|L2)",\s*dic=(\{[^}]*\})', run_src):
        case, case_dir, lvl, dic = m.group(1), m.group(2), m.group(3), m.group(4)
        if case != case_dir:
            raise Abort(f"mms3d: case {case} registered under {case_dir}/")
        out.setdefault(case, {})[lvl] = dic
    return out


# --- new run cells -----------------------------------------------------------

def dir_key(n):
    return ("nx", "ny", "nz")[n]


def coarse_dict(counts):
    return "{" + ", ".join(f'"{dir_key(n)}": {v}' for n, v in enumerate(counts)) + "}"


def run_cell_single(case, alias, counts, nx_line, l1_text, l2_text):
    return f'''from trustutils import run

NB_PROCS = 2

run.reset()
# --- mesh-convergence scan (shared ladder) ----------------------------------
# The single dataset src/{case}.data carries $nx/$ny node-count placeholders;
# every scale runs the SAME file via trustutils substitution
# (run.addCaseFromTemplate -> string.Template) into build/<scale>/ — one
# data-file name across scales:
#   coarse   = the baseline mesh
#   standard = cells x2 (h/2)
#   fine     = cells x4 (h/4)
# Each scale runs in its own build/<scale>/ directory so that the
# partitioned DOM_000*.Zones of one level never overwrite another's.
{nx_line}
meshes = {{
    "coarse":   {coarse_dict(counts)},            # the baseline mesh
    "standard": {l1_text},
    "fine":     {l2_text},
}}
cases = {{}}
for lvl, dic in meshes.items():
    cases[lvl] = run.addCaseFromTemplate("{case}.data", lvl, dic=dic,
                                         nbProcs=NB_PROCS, excluNR=True)
    cases[lvl].partition()          # trust -partition {case}.data 2 -> PAR_{case}.data + DOM_000*.Zones
run.printCases()
run.runCases()
{alias} = cases["coarse"]            # alias kept for the read-back cells
'''


def run_cell_mms2d(case_ladders, n0_line, np0_line, l1_text, l2_text, l1p_text, l2p_text, addcases):
    case_list = ",\n                 ".join(f'("{c}", {lv})' for c, lv in case_ladders)
    alias_lines = "\n".join(f'{a} = cases["{c}"]["coarse"]' for a, c in addcases)
    return f'''from trustutils import run

NB_PROCS = 2

run.reset()
# --- mesh-convergence scan (shared ladder, per-case subdirectories) ---------
# Each single dataset src/<Case>.data carries $nx/$ny node-count placeholders;
# every scale runs the SAME file via trustutils substitution
# (run.addCaseFromTemplate -> string.Template) into build/<Case>/<scale>/ —
# one data-file name across scales:
#   coarse   = the baseline mesh
#   standard = cells x2 (h/2)
#   fine     = cells x4 (h/4)
{n0_line}
{np0_line}
LEVELS  = {{"coarse": N0, "standard": {l1_text}, "fine": {l2_text}}}
LEVELSP = {{"coarse": NP0, "standard": {l1p_text}, "fine": {l2p_text}}}
cases = {{}}
for base, lv in ({case_list}):
    cases[base] = {{}}
    for lvl, nn in lv.items():
        cases[base][lvl] = run.addCaseFromTemplate(
            f"{{base}}.data", f"{{base}}/{{lvl}}", dic={{"nx": nn, "ny": nn}},
            nbProcs=NB_PROCS, excluNR=True)
        cases[base][lvl].partition()   # trust -partition <Case>.data 2 -> PAR_<Case>.data + Zones
run.printCases()
run.runCases()
{alias_lines}
'''


def run_cell_mms3d(case_meshes, addcases):
    entries = []
    for base, lv in case_meshes:
        lines = [f'    ("{base}", {{']
        for lvl, dic in lv:
            lines.append(f'        "{lvl}": {dic},')
        lines.append("    }),")
        entries.append("\n".join(lines))
    entries_text = "\n".join(entries)
    alias_lines = "\n".join(f'{a} = cases["{c}"]["coarse"]' for a, c in addcases)
    return f'''from trustutils import run

NB_PROCS = 2

run.reset()
# --- mesh-convergence scan (shared ladder, per-case subdirectories) ---------
# Each single dataset src/<Case>.data carries $nx/$ny/$nz node-count
# placeholders; every scale runs the SAME file via trustutils substitution
# (run.addCaseFromTemplate -> string.Template) into build/<Case>/<scale>/ —
# one data-file name across scales:
#   coarse   = the baseline mesh
#   standard = the refined mesh (the historical L1 level)
#   fine     = the finest mesh (the historical L2 level)
cases = {{}}
for base, lv in (
{entries_text}
):
    cases[base] = {{}}
    for lvl, dic in lv.items():
        cases[base][lvl] = run.addCaseFromTemplate(
            f"{{base}}.data", f"{{base}}/{{lvl}}", dic=dic,
            nbProcs=NB_PROCS, excluNR=True)
        cases[base][lvl].partition()   # trust -partition <Case>.data 2 -> PAR_<Case>.data + Zones
run.printCases()
run.runCases()
{alias_lines}
'''


def run_cell_coupled(case, alias, counts, nx_line, l1_text, l2_text):
    return f'''from trustutils import run

NB_PROCS = 2

run.reset()
# --- mesh-convergence scan (shared ladder) ----------------------------------
# The single dataset src/{case}.data carries $nx/$ny node-count placeholders;
# every scale runs the SAME file via trustutils substitution
# (run.addCaseFromTemplate -> string.Template) into build/<scale>/ — one
# data-file name across scales:
#   coarse   = the baseline mesh
#   standard = cells x2 (h/2)
#   fine     = cells x4 (h/4)
# Each scale runs in its own build/<scale>/ directory so that the
# partitioned DOM_000*.Zones of one level never overwrite another's.
{nx_line}
meshes = {{
    "coarse":   {coarse_dict(counts)},            # the baseline mesh
    "standard": {l1_text},
    "fine":     {l2_text},
}}
cases = {{}}
for lvl, dic in meshes.items():
    cases[lvl] = run.addCaseFromTemplate("{case}.data", lvl, dic=dic,
                                         nbProcs=NB_PROCS, excluNR=True)
    cases[lvl].partition()          # trust -partition {case}.data 2 -> PAR_{case}.data + DOM_000*.Zones
# --- merged two-way twin (the historical TwoWayFeedback case) --------------
# Same physical case as the baseline above (b = (y,-x), eta = 0, Lorentz
# hydrostatic balance) but with vitesse_hydraulique: the SOLVED NS velocity
# feeds the magnetic convection. Runs in the same form folder (relaxed R4:
# the datasets of the form live in its own src/).
case_twoway = run.addCase(".", "twoway_feedback.data", nbProcs=NB_PROCS)
case_twoway.partition()    # trust -partition twoway_feedback.data 2
run.printCases()
run.runCases()
{alias} = cases["coarse"]            # alias kept for the read-back cells
'''


# --- per-form processing -----------------------------------------------------

def atomic_write(path, data):
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(data, encoding="utf-8")
    os.replace(tmp, path)


def build_rules(variant, addcases):
    """Ordered textual rules for the non-run cells, variant-specific."""
    rules = list(COMMON_RULES)
    if variant in ("single", "coupled"):
        rules.append(("scan_cases", "cases"))
    elif variant == "mms2d":
        rules.append(("scan_cases", "cases"))
        rules.append(('("", cvar.dataFileName_)', '(f"{base}/coarse", cvar.dataFileName_)'))
        rules.append(('("", c_exptrig.dataFileName_)', '(f"{base}/coarse", c_exptrig.dataFileName_)'))
    elif variant == "mms3d":
        rules.append(("scan_cases_3d", "cases"))
        # literal level names inside f-string paths: f"scan/<Case>/L1" ...
        rules.append(('/L1"', '/standard"'))
        rules.append(('/L2"', '/fine"'))
        for alias, case in addcases:
            rules.append((f'("", {alias}.dataFileName_)', f'("{case}/coarse", {alias}.dataFileName_)'))
    return rules


def process_form(form):
    formdir = RAPORTS / form
    nbpath = formdir / f"{formdir.name}.ipynb"
    if not nbpath.is_file():
        raise Abort(f"no notebook at {nbpath}")
    nb = json.loads(nbpath.read_text(encoding="utf-8"))
    cells = nb["cells"]

    # --- locate the run cell, detect the variant ---
    run_idx = next((i for i, c in enumerate(cells)
                    if c["cell_type"] == "code" and "addCaseFromTemplate" in cell_source(c)), None)
    if run_idx is None:
        raise Abort("no run cell (addCaseFromTemplate) found — already converted?")
    run_src = cell_source(cells[run_idx])

    if "scan_cases_3d" in run_src:
        variant = "mms3d"
    elif "LEVELSP" in run_src or "scan/{base}" in run_src:
        variant = "mms2d"
    elif "twoway_feedback" in run_src:
        variant = "coupled"
    else:
        variant = "single"

    # --- src/: template files -> the single data file ---
    tpls = sorted((formdir / "src").glob("*.tpl.data"))
    if not tpls:
        raise Abort("no .tpl.data in src/ — already converted?")
    n_mms = 4 if variant in ("mms2d", "mms3d") else 1
    if len(tpls) != n_mms:
        raise Abort(f"{variant} variant expects {n_mms} tpl file(s), found {len(tpls)}")

    # --- baseline counts from the CURRENT (pre-conversion) data files ---
    counts_by_case = {}
    for tpl in tpls:
        case = tpl.name[: -len(".tpl.data")]
        datapath = formdir / "src" / f"{case}.data"
        if not datapath.is_file():
            raise Abort(f"missing baseline {datapath}")
        text = datapath.read_text(encoding="utf-8")
        if "$" in text:
            raise Abort(f"{datapath.name} already carries $ placeholders — already converted?")
        counts_by_case[case] = parse_baseline_counts(datapath)

    # --- addCase registrations in the run cell (baseline alias <-> case) ---
    addcases = parse_addcase_baselines(run_src)
    if not addcases:
        raise Abort("no run.addCase(\".\", ...) baseline found in the run cell")

    # --- namespace for eval'ing the old level dicts ---
    ns = {}
    if variant == "mms3d":
        if len(addcases) != 4:
            raise Abort(f"mms3d variant expects 4 addCase baselines, found {len(addcases)}")
    elif variant == "mms2d":
        if len(addcases) != 4:
            raise Abort(f"mms2d variant expects 4 addCase baselines, found {len(addcases)}")
        for name in ("N0", "NP0"):
            m = re.search(rf"^{name}\s*=\s*(\d+)", run_src, re.M)
            if not m:
                raise Abort(f"{name} assignment not found in the run cell")
            ns[name] = int(m.group(1))
        for case, vals in counts_by_case.items():
            if vals not in ([ns["N0"]] * len(vals), [ns["NP0"]] * len(vals)):
                raise Abort(f"{case}: baseline {vals} matches neither N0 nor NP0")
    else:
        if len(addcases) != (2 if variant == "coupled" else 1):
            raise Abort(f"unexpected addCase count {len(addcases)} for the {variant} variant")
        case = addcases[0][1]
        if case not in counts_by_case:
            raise Abort(f"addCase baseline {case} has no tpl/data file")
        counts = counts_by_case[case]
        # namespace from the cell's own count assignments (NX0/NY0, NXY0, ...)
        ns = eval_assignments(parse_count_assignments(run_src))
        if not ns:
            raise Abort("no baseline count assignment found in the run cell")
        for name, v in zip(("NX0", "NY0", "NZ0"), counts):
            if name in ns and ns[name] != v:
                raise Abort(f"{name}={ns[name]} contradicts the data file counts {counts}")

    # --- build the new run cell ---
    assignments = [a.replace("(L0)", "(coarse)") for a in parse_count_assignments(run_src)]
    defined = set().union(*(names_in(a) for a in assignments)) if assignments else set()

    if variant == "mms3d":
        levels = parse_mms3d_levels(run_src)
        if set(levels) != set(counts_by_case):
            raise Abort(f"mms3d: registered cases {sorted(levels)} != tpl cases {sorted(counts_by_case)}")
        case_meshes = []
        for alias, case in addcases:
            counts = counts_by_case[case]
            case_meshes.append((case, [
                ("coarse", coarse_dict(counts)),
                ("standard", levels[case]["L1"]),
                ("fine", levels[case]["L2"]),
            ]))
        new_run = run_cell_mms3d(case_meshes, addcases)

    elif variant == "mms2d":
        entries = parse_entries(parse_levels(run_src, "LEVELS"))
        if [k for k, _ in entries][:2] != ["L1", "L2"]:
            raise Abort(f"unexpected LEVELS keys {[k for k, _ in entries]}")
        l1_text, l2_text = (v for k, v in entries if k in ("L1", "L2"))
        entriesp = parse_entries(parse_levels(run_src, "LEVELSP"))
        l1p_text, l2p_text = (v for k, v in entriesp if k in ("L1", "L2"))
        old_vals = eval_levels(entries, ns)
        old_valsp = eval_levels(entriesp, ns)
        for val, base in ((old_vals["L1"], ns["N0"]), (old_vals["L2"], ns["N0"]),
                          (old_valsp["L1"], ns["NP0"]), (old_valsp["L2"], ns["NP0"])):
            if val not in (2 * base - 1, 4 * base - 3, base):
                raise Abort(f"level value {val} deviates from the x2/x4 ladder")
        n0_line = next((l for l in assignments if l.startswith("N0")), None)
        np0_line = next((l for l in assignments if l.startswith("NP0")), None)
        if not n0_line or not np0_line:
            raise Abort("N0/NP0 assignments not found in the run cell")
        case_ladders = [(c, "LEVELSP" if vals == [ns["NP0"]] * len(vals) else "LEVELS")
                        for c, vals in counts_by_case.items()]
        new_run = run_cell_mms2d(case_ladders, n0_line, np0_line,
                                 l1_text, l2_text, l1p_text, l2p_text, addcases)
        assignments = [n0_line, np0_line]

    else:  # single / coupled
        entries = parse_entries(parse_levels(run_src, "LEVELS"))
        if [k for k, _ in entries][:2] != ["L1", "L2"]:
            raise Abort(f"unexpected LEVELS keys {[k for k, _ in entries]}")
        l1_text, l2_text = (v for k, v in entries if k in ("L1", "L2"))
        eval_levels(entries, ns)  # cross-check: the spliced expressions must eval
        alias, case = addcases[0]
        nx_line = assignments[0] if assignments else None
        if not nx_line:
            raise Abort("no baseline count assignment found in the run cell")
        if variant == "single":
            new_run = run_cell_single(case, alias, counts_by_case[case],
                                       nx_line, l1_text, l2_text)
        else:
            new_run = run_cell_coupled(case, alias, counts_by_case[case],
                                       nx_line, l1_text, l2_text)

    # sanity notes: mms2d/single/coupled expressions were eval'ed above (all
    # names resolved against the parsed namespace); mms3d meshes are literal
    # dicts parsed from the old cell, nothing further to check.

    # --- transform the notebook cells ---
    rules = build_rules(variant, addcases)
    new_cells = []
    for i, c in enumerate(cells):
        src = cell_source(c)
        if i == run_idx:
            src = new_run
        elif c["cell_type"] == "code":
            src = apply_rules(src, rules)
        else:  # markdown
            src = apply_md_rules(src)
        c = dict(c)
        set_cell_source(c, src)
        new_cells.append(c)
    nb["cells"] = new_cells

    # --- src/: tpl content becomes the single data file; delete the tpl ---
    for tpl in tpls:
        case = tpl.name[: -len(".tpl.data")]
        datapath = formdir / "src" / f"{case}.data"
        atomic_write(datapath, tpl.read_text(encoding="utf-8"))
        tpl.unlink()

    atomic_write(nbpath, json.dumps(nb, indent=1, ensure_ascii=False) + "\n")
    print(f"  {form}: converted ({variant}; {len(tpls)} data file(s))")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    failed = []
    for form in sys.argv[1:]:
        try:
            process_form(form)
        except Abort as e:
            print(f"  {form}: ABORTED — {e}", file=sys.stderr)
            failed.append(form)
        except Exception as e:  # unexpected: report, leave the form untouched
            print(f"  {form}: ABORTED (unexpected: {e!r})", file=sys.stderr)
            failed.append(form)
    if failed:
        print(f"nb-scan-rename: {len(failed)} form(s) aborted: {', '.join(failed)}",
              file=sys.stderr)
        return 1
    print("nb-scan-rename: all forms converted.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
