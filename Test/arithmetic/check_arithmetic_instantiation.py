#!/usr/bin/env python3
"""Acceptance checks for GKC bounded numeric instantiation."""

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GKC = ROOT / "gkc"
CORPUS = ROOT / "Test" / "arithmetic"
EXAMPLES = ROOT / "Examples"
BASE = ["-parallel", "0", "-seconds", "5", "-print", "10"]


def strategy(mode=None):
    obj = {"strategy": ["unit"], "query_preference": 0}
    if mode is not None:
        obj["arith_instantiation"] = mode
    return json.dumps(obj, separators=(",", ":"))


def run(label, path, mode=None):
    command = [str(GKC), str(path)] + BASE + ["-strategytext", strategy(mode)]
    result = subprocess.run(command, cwd=ROOT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=15)
    if result.returncode != 0:
        raise AssertionError(
            f"{label}: exit {result.returncode}\nstdout:\n{result.stdout}"
            f"\nstderr:\n{result.stderr}")
    if result.stderr:
        raise AssertionError(f"{label}: unexpected stderr\n{result.stderr}")
    return result.stdout


def check(label, path, mode, proof, arith_history=False):
    output = run(label, path, mode)
    marker = "result: proof found" if proof else "result: proof not found"
    if marker not in output:
        raise AssertionError(f"{label}: missing {marker!r}\n{output}")
    if arith_history and "[arithinst," not in output:
        raise AssertionError(f"{label}: proof has no arithinst history\n{output}")


def main():
    if not GKC.is_file():
        print("FAIL: build ./gkc first", file=sys.stderr)
        return 2

    checks = 0

    def checked(label, path, mode, proof, arith_history=False):
        nonlocal checks
        check(label, path, mode, proof, arith_history)
        checks += 1

    for name in ("arithmetic_ground_regression1.txt",
                 "arithmetic_ground_regression2.txt",
                 "arithmetic_overflow_safety.txt"):
        checked(name + " mode 0", CORPUS / name, 0, True)

    conservative = {
        1: "", 2: "", 3: "", 4: "", 5: "", 7: "", 8: "", 9: "",
        10: "", 12: "_apples_comparison", 13: "_cars_answer",
        16: "_boxes_answer",
    }
    for number, suffix in conservative.items():
        name = f"arithmetic_instantiation{number}{suffix}.txt"
        path = CORPUS / name
        checked(name + " mode 0", path, 0, False)
        checked(name + " mode 1", path, 1, True, True)

    for number, suffix in ((14, "_apples_given_answer"),
                           (15, "_cars_given_prices")):
        name = f"arithmetic_instantiation{number}{suffix}.txt"
        path = CORPUS / name
        checked(name + " mode 0", path, 0, True)
        checked(name + " mode 1", path, 1, True)

    for number, suffix in ((17, "_ages_expected_hard"),
                           (18, "_cars_general_expected_hard")):
        name = f"arithmetic_instantiation{number}{suffix}.txt"
        checked(name + " mode 2", CORPUS / name, 2, False)

    apples = EXAMPLES / "arithmetic_instantiation_apples.txt"
    checked("apples explicit off", apples, 0, False)
    checked("apples compiled default", apples, None, True, True)
    checked("apples explicit mode 1", apples, 1, True, True)

    product = EXAMPLES / "arithmetic_instantiation_product.txt"
    checked("product mode 1", product, 1, False)
    checked("product mode 2", product, 2, True, True)

    print(f"OK: {checks} arithmetic-instantiation checks")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, subprocess.TimeoutExpired) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
