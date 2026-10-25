#!/usr/bin/env python3
"""Record a unit-test or validation-form result into .opencode/validation_status.json.

Usage:
  record-validation-status.py --repo <repo> --kind unit --status pass|fail|unknown \
      [--gtest-json <path>] [--returncode N] [--note "..."]
  record-validation-status.py --repo <repo> --kind validation --key "Cat/Form" \
      --status pass|fail|in_progress|unknown [--note "..."]
"""
import argparse
import json
import os
import sys
from datetime import datetime, timezone


def load(path):
    if os.path.exists(path):
        try:
            with open(path) as f:
                return json.load(f)
        except (json.JSONDecodeError, OSError):
            return {}
    return {}


def save(path, data):
    data["updated"] = datetime.now(timezone.utc).isoformat()
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")
    os.replace(tmp, path)


def parse_gtest(path):
    suites = {}
    if not path or not os.path.exists(path):
        return suites
    try:
        with open(path) as f:
            doc = json.load(f)
    except (json.JSONDecodeError, OSError):
        return suites
    # gtest JSON schema: {"testsuites": [{"name":..., "tests": N, "failures": M, "testsuite": [...]}]}
    for ts in doc.get("testsuites", []):
        name = ts.get("name", "unknown")
        tests = int(ts.get("tests", 0) or 0)
        failures = int(ts.get("failures", 0) or 0)
        # aggregate if a suite name repeats
        if name in suites:
            suites[name]["tests"] += tests
            suites[name]["failures"] += failures
        else:
            suites[name] = {"tests": tests, "failures": failures}
    return suites


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", required=True)
    ap.add_argument("--kind", required=True, choices=["unit", "validation"])
    ap.add_argument("--status", required=True)
    ap.add_argument("--key", help="validation form key, e.g. MagneticStandalone/Invariance")
    ap.add_argument("--gtest-json", help="path to gtest JSON output")
    ap.add_argument("--returncode", type=int, default=None)
    ap.add_argument("--note", default=None)
    args = ap.parse_args()

    repo = os.path.abspath(args.repo)
    status_path = os.path.join(repo, ".opencode", "validation_status.json")
    os.makedirs(os.path.dirname(status_path), exist_ok=True)
    data = load(status_path)

    if args.kind == "unit":
        entry = data.setdefault("unit_tests", {})
        entry["status"] = args.status
        if args.returncode is not None:
            entry["returncode"] = args.returncode
        suites = parse_gtest(args.gtest_json) if args.gtest_json else None
        if suites is not None:
            entry["suites"] = suites
        if args.note:
            entry["note"] = args.note
    else:
        if not args.key:
            print("error: --key required for validation kind", file=sys.stderr)
            sys.exit(2)
        v = data.setdefault("validation", {})
        e = v.setdefault(args.key, {})
        e["status"] = args.status
        if args.note:
            e["note"] = args.note
        from datetime import datetime, timezone
        e["last_run"] = datetime.now(timezone.utc).isoformat()

    save(status_path, data)
    print(f"recorded {args.kind} status -> {status_path}")


if __name__ == "__main__":
    main()
