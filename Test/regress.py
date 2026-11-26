#!/usr/bin/env python3
"""Regression test of gkc on the inputs of Examples/ and Test/.

Run from any folder:

  python3 Test/regress.py                    # tests ./gkc
  python3 Test/regress.py --binary path/gkc  # tests another binary
  python3 Test/regress.py --only kb_steam    # runs cases whose name contains kb_steam

The case list is Test/regress_cases.tsv. Each case runs with -parallel 0 and
a wall-clock timeout. The whole process group of a case is killed at the
timeout. The script prints one line per failed case and a final count. The
exit code is 0 only when every case passes.

For every proof the script checks that the proof output is complete:
* plain output: a "proof:" line, steps numbered 1..n, every parent number
  smaller than its step, and a last step "false" or an answer clause;
* TPTP output: an "SZS output start" line, an "SZS output end" line and at
  least one clause between them;
* JSON output (-json): valid JSON with a non-empty proof for each answer.

The case kind "output:T1;;T2" needs every text Ti in the output and no
error text; the self tests of the term index (Test/termindex/, strategy
key ti_selftest) use it.

The case kind "kbcheck" builds a shared KB with -readkb, computes a sha256
checksum of the shared memory segment, runs the query with -usekb, computes
the checksum again and deletes the segment. The two checksums must be equal.

The case kind "kbdump" checks a dump round trip: -readkb, -writekb to a file
in a temporary folder (the segment checksum must not change), -deletekb,
-loadkb of the file and a -usekb query that must find a proof. The dump file
has the used size of the segment, about 1 GB, and is removed at the end.

The script also runs Test/arithmetic/check_arithmetic_instantiation.py. That
driver always tests ./gkc, so the script runs it only when --binary is ./gkc.
"""

import argparse
import ctypes
import hashlib
import json
import os
import re
import shlex
import signal
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CASES = ROOT / "Test" / "regress_cases.tsv"
ARITH_DRIVER = ROOT / "Test" / "arithmetic" / "check_arithmetic_instantiation.py"

# Text that must not occur in the output of a case that expects no error.
ERROR_PATTERNS = [
  re.compile(r'"error"\s*:'),
  re.compile(r"segmentation fault|core dumped|double free|corrupted", re.I),
  re.compile(r"cannot allocate memory|Unhandled alloc_err", re.I),
]

STEP_RE = re.compile(r"^\s*(\d+): \[([^\]]*)\] (.*)$")
SHM_RDONLY = 0o10000


class Case:
  def __init__(self, name, args, expect, timeout, lineno):
    self.name = name
    self.args = args
    self.expect = expect
    self.timeout = timeout
    self.lineno = lineno


def read_cases(path):
  cases = []
  names = set()
  for lineno, line in enumerate(path.read_text().splitlines(), 1):
    if not line.strip() or line.lstrip().startswith("#"):
      continue
    cols = line.split("\t")
    if len(cols) != 4:
      raise SystemExit(f"{path}:{lineno}: expected 4 TAB-separated columns")
    name, args, expect, timeout = cols
    if name in names:
      raise SystemExit(f"{path}:{lineno}: duplicate case name {name}")
    names.add(name)
    cases.append(Case(name, shlex.split(args), expect, float(timeout), lineno))
  return cases


def run_gkc(cmd, timeout, stdout_only=None):
  """Run cmd in its own process group. Return (rc, output, timed_out, secs).

  The output holds stdout and stderr. If stdout_only is a list, stderr is
  read apart (appended after stdout in the output), and stdout alone is
  stored as stdout_only[0]: a sanitizer build writes its reports to stderr,
  and they must not enter the JSON that a case checks.
  """
  start = time.monotonic()
  split = stdout_only is not None
  proc = subprocess.Popen(cmd, cwd=ROOT, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE if split else subprocess.STDOUT,
                          start_new_session=True)
  timed_out = False
  try:
    raw, err = proc.communicate(timeout=timeout)
  except subprocess.TimeoutExpired:
    timed_out = True
    try:
      os.killpg(proc.pid, signal.SIGKILL)
    except ProcessLookupError:
      pass
    raw, err = proc.communicate()
  finally:
    try:
      os.killpg(proc.pid, signal.SIGKILL)
    except ProcessLookupError:
      pass
  secs = time.monotonic() - start
  out = raw.decode("utf-8", "replace")
  if split:
    stdout_only.append(out)
    out = out + (err or b"").decode("utf-8", "replace")
  return proc.returncode, out, timed_out, secs


def classify(out, rc, timed_out):
  """Return the outcome class of one gkc run."""
  if timed_out:
    return "timeout"
  if rc is not None and rc < 0:
    return f"signal{-rc}"
  if re.search(r'"error"\s*:', out):
    return "error"
  if re.search(r"cannot allocate memory", out, re.I):
    return "buffer_full"
  if re.search(r"^result: proof found", out, re.M) or \
     re.search(r'"result":\s*"proof found"', out):
    return "proof"
  if re.search(r"^result: proof not found", out, re.M) or \
     re.search(r'"result":\s*"proof not found"', out):
    return "noproof"
  return "unknown"


def check_plain_proofs(out):
  """Check every plain proof block. Return an error text or None."""
  lines = out.splitlines()
  blocks = 0
  i = 0
  while i < len(lines):
    if lines[i].strip() != "proof:":
      i += 1
      continue
    blocks += 1
    i += 1
    steps = []
    while i < len(lines):
      m = STEP_RE.match(lines[i])
      if not m:
        break
      steps.append(m)
      i += 1
    if not steps:
      return f"proof {blocks}: no steps after 'proof:'"
    for k, m in enumerate(steps, 1):
      nr = int(m.group(1))
      if nr != k:
        return f"proof {blocks}: step {nr} found where step {k} was expected"
      for tok in m.group(2).split(",")[1:]:
        pm = re.match(r"\s*(\d+)", tok)
        if pm and int(pm.group(1)) >= nr:
          return f"proof {blocks}: step {nr} uses a later step {pm.group(1)}"
    last = steps[-1].group(3).strip()
    if last != "false" and "$ans" not in last:
      return f"proof {blocks}: last step is '{last}', not false or an answer"
  if blocks == 0:
    return "no 'proof:' line"
  return None


def check_tptp_proof(out):
  starts = len(re.findall(r"^% SZS output start", out, re.M))
  ends = len(re.findall(r"^% SZS output end", out, re.M))
  if starts == 0 or starts != ends:
    return f"{starts} 'SZS output start' and {ends} 'SZS output end' lines"
  body = out.split("% SZS output start", 1)[1].split("% SZS output end", 1)[0]
  if not re.search(r"^(cnf|fof)\(", body, re.M):
    return "no clause between the SZS output lines"
  return None


def json_answers(out):
  try:
    data = json.loads(out)
  except ValueError as e:
    return None, f"output is no valid JSON: {e}"
  answers = data.get("answers") if isinstance(data, dict) else None
  if not isinstance(answers, list) or not answers:
    return None, "JSON output has no answers list"
  return answers, None


def check_json_proof(out):
  answers, err = json_answers(out)
  if err:
    return err
  for k, ans in enumerate(answers, 1):
    proof = ans.get("proof") if isinstance(ans, dict) else None
    if not isinstance(proof, list) or not proof:
      return f"answer {k}: no proof steps"
    for n, step in enumerate(proof, 1):
      if not isinstance(step, list) or not step or step[0] != n:
        return f"answer {k}: step {n} has a wrong number"
  return None


def check_proof(case, out):
  if "-json" in case.args:
    return check_json_proof(out)
  if "-tptp" in case.args:
    return check_tptp_proof(out)
  return check_plain_proofs(out)


def check_answers(case, out, wanted):
  if "-json" in case.args:
    answers, err = json_answers(out)
    if err:
      return err
    texts = [json.dumps(a.get("answer"), separators=(",", ":"))
             for a in answers]
  else:
    texts = [l for l in out.splitlines() if l.startswith("answer:")]
  for w in wanted:
    if not any(w in t for t in texts):
      return f"answer {w!r} not found; answers: {texts}"
  return None


def run_case(binary, case):
  """Run one ordinary case. Return (ok, message, seconds)."""
  cmd = [str(binary)] + case.args + ["-parallel", "0"]
  stdout = [] if "-json" in case.args else None
  rc, out, timed_out, secs = run_gkc(cmd, case.timeout, stdout)
  jsonout = stdout[0] if stdout else out  # the JSON checks read stdout only
  got = classify(out, rc, timed_out)
  exp = case.expect
  if exp.startswith("error:"):
    text = exp[len("error:"):]
    if got != "error":
      return False, f"expected an error, got {got} (rc {rc})", secs
    if text not in out:
      return False, f"error text {text!r} not in output", secs
    return True, "", secs
  for pat in ERROR_PATTERNS:
    m = pat.search(out)
    if m:
      return False, f"error text in output: {m.group(0)!r} (class {got})", secs
  if exp.startswith("output:"):
    for text in exp[len("output:"):].split(";;"):
      if text not in out:
        return False, f"text {text!r} not in output", secs
    return True, "", secs
  if exp == "noproof":
    if got != "noproof":
      return False, f"expected noproof, got {got} (rc {rc})", secs
    return True, "", secs
  if exp == "proof" or exp.startswith("answer:"):
    if got != "proof":
      return False, f"expected proof, got {got} (rc {rc})", secs
    err = check_proof(case, jsonout)
    if err:
      return False, f"incomplete proof output: {err}", secs
    if exp.startswith("answer:"):
      err = check_answers(case, jsonout, exp[len("answer:"):].split(";;"))
      if err:
        return False, err, secs
    return True, "", secs
  return False, f"unknown expectation {exp!r} in the case list", secs


def find_segment(key):
  """Return (shmid, size) of the SysV segment with this key, or None."""
  shm = Path("/proc/sysvipc/shm")
  if not shm.is_file():
    return None
  for line in shm.read_text().splitlines()[1:]:
    cols = line.split()
    if len(cols) > 3 and cols[0] == str(key):
      return int(cols[1]), int(cols[3])
  return None


def segment_sha256(key):
  """Attach the segment read-only and return its sha256 hex digest."""
  seg = find_segment(key)
  if seg is None:
    raise OSError(f"no shared memory segment with key {key}")
  shmid, size = seg
  libc = ctypes.CDLL(None, use_errno=True)
  libc.shmat.restype = ctypes.c_void_p
  libc.shmat.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
  libc.shmdt.argtypes = [ctypes.c_void_p]
  addr = libc.shmat(shmid, None, SHM_RDONLY)
  if addr is None or addr == ctypes.c_void_p(-1).value:
    raise OSError(f"shmat failed, errno {ctypes.get_errno()}")
  try:
    buf = (ctypes.c_char * size).from_address(addr)
    return hashlib.sha256(memoryview(buf)).hexdigest()
  finally:
    libc.shmdt(ctypes.c_void_p(addr))


def run_kbcheck(binary, case):
  """Shared KB check. Return (ok or None for skip, message, seconds)."""
  if len(case.args) < 2:
    return False, "kbcheck needs a KB file and a query file", 0.0
  if not Path("/proc/sysvipc/shm").is_file():
    return None, "no /proc/sysvipc/shm on this platform", 0.0
  kbfile, query = case.args[:2]
  extra = case.args[2:]  # flags for the -usekb query, for example a strategy
  mbnr = 7000 + os.getpid() % 1000
  start = time.monotonic()
  try:
    rc, out, to, _ = run_gkc([str(binary), "-readkb", kbfile, "-mbnr",
                              str(mbnr), "-mbsize", "1000"], case.timeout)
    if to or rc != 0 or "Db ready in shared memory" not in out:
      return False, f"-readkb failed (rc {rc}): {out[-300:]!r}", 0.0
    try:
      before = segment_sha256(mbnr)
    except OSError as e:
      return None, f"cannot read the segment: {e}", 0.0
    rc, out, to, _ = run_gkc([str(binary), "-usekb", query, "-mbnr",
                              str(mbnr), "-parallel", "0"] + extra,
                             case.timeout)
    got = classify(out, rc, to)
    after = segment_sha256(mbnr)
    secs = time.monotonic() - start
    if got != "proof":
      return False, f"-usekb: expected proof, got {got} (rc {rc})", secs
    err = check_plain_proofs(out)
    if err:
      return False, f"-usekb: incomplete proof output: {err}", secs
    if before != after:
      return False, f"KB segment changed: {before[:16]} -> {after[:16]}", secs
    return True, "", secs
  finally:
    run_gkc([str(binary), "-deletekb", "-mbnr", str(mbnr)], 30)
    if find_segment(mbnr) is not None:
      print(f"WARNING: shared memory segment {mbnr} was not deleted")


def run_kbdump(binary, case):
  """Dump round trip. Return (ok or None for skip, message, seconds)."""
  if len(case.args) < 2:
    return False, "kbdump needs a KB file and a query file", 0.0
  if not Path("/proc/sysvipc/shm").is_file():
    return None, "no /proc/sysvipc/shm on this platform", 0.0
  kbfile, query = case.args[:2]
  extra = case.args[2:]  # flags for the -usekb query
  mbnr = 7000 + os.getpid() % 1000
  mbnr2 = mbnr + 1000  # segment of the loaded dump
  tmpdir = tempfile.mkdtemp(prefix="gkc_regress_")
  dump = str(Path(tmpdir) / "kb.dump")
  start = time.monotonic()
  try:
    rc, out, to, _ = run_gkc([str(binary), "-readkb", kbfile, "-mbnr",
                              str(mbnr), "-mbsize", "1000"], case.timeout)
    if to or rc != 0 or "Db ready in shared memory" not in out:
      return False, f"-readkb failed (rc {rc}): {out[-300:]!r}", 0.0
    try:
      before = segment_sha256(mbnr)
    except OSError as e:
      return None, f"cannot read the segment: {e}", 0.0
    rc, out, to, _ = run_gkc([str(binary), "-writekb", dump, "-mbnr",
                              str(mbnr)], case.timeout)
    if to or rc != 0 or not Path(dump).is_file():
      return False, f"-writekb failed (rc {rc}): {out[-300:]!r}", \
          time.monotonic() - start
    after = segment_sha256(mbnr)
    if before != after:
      return False, (f"KB segment changed by -writekb: {before[:16]} -> "
                     f"{after[:16]}"), time.monotonic() - start
    run_gkc([str(binary), "-deletekb", "-mbnr", str(mbnr)], 30)
    rc, out, to, _ = run_gkc([str(binary), "-loadkb", dump, "-mbnr",
                              str(mbnr2)], case.timeout)
    if to or rc != 0 or "Database imported" not in out:
      return False, f"-loadkb failed (rc {rc}): {out[-300:]!r}", \
          time.monotonic() - start
    rc, out, to, _ = run_gkc([str(binary), "-usekb", query, "-mbnr",
                              str(mbnr2), "-parallel", "0"] + extra,
                             case.timeout)
    got = classify(out, rc, to)
    secs = time.monotonic() - start
    if got != "proof":
      return False, f"-usekb on the loaded dump: expected proof, got {got} " \
          f"(rc {rc})", secs
    err = check_plain_proofs(out)
    if err:
      return False, f"-usekb: incomplete proof output: {err}", secs
    return True, "", secs
  finally:
    for n in (mbnr, mbnr2):
      if find_segment(n) is not None:
        run_gkc([str(binary), "-deletekb", "-mbnr", str(n)], 30)
      if find_segment(n) is not None:
        print(f"WARNING: shared memory segment {n} was not deleted")
    shutil.rmtree(tmpdir, ignore_errors=True)


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--binary", default=str(ROOT / "gkc"),
                  help="gkc binary to test (default: ./gkc)")
  ap.add_argument("--cases", default=str(DEFAULT_CASES),
                  help="case list (default: Test/regress_cases.tsv)")
  ap.add_argument("--only", default=None,
                  help="run only cases whose name contains this text")
  ap.add_argument("--verbose", action="store_true",
                  help="print one line per case, also for passed cases")
  args = ap.parse_args()

  binary = Path(args.binary).resolve()
  if not binary.is_file():
    print(f"FAIL: binary {binary} not found")
    return 2
  cases = read_cases(Path(args.cases))
  if args.only:
    cases = [c for c in cases if args.only in c.name]

  t0 = time.monotonic()
  passed = failed = skipped = 0
  for case in cases:
    if case.expect == "kbcheck":
      ok, msg, secs = run_kbcheck(binary, case)
    elif case.expect == "kbdump":
      ok, msg, secs = run_kbdump(binary, case)
    else:
      ok, msg, secs = run_case(binary, case)
    if ok is None:
      skipped += 1
      print(f"SKIP {case.name}: {msg}")
    elif ok:
      passed += 1
      if args.verbose:
        print(f"ok   {case.name} ({secs:.2f} s)")
    else:
      failed += 1
      print(f"FAIL {case.name} (line {case.lineno}): {msg}")

  if not args.only:
    if binary == (ROOT / "gkc").resolve():
      res = subprocess.run([sys.executable, str(ARITH_DRIVER)], cwd=ROOT,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, timeout=600)
      last = res.stdout.strip().splitlines()[-1] if res.stdout.strip() else ""
      if res.returncode == 0:
        passed += 1
        if args.verbose:
          print(f"ok   arithmetic driver: {last}")
      else:
        failed += 1
        print(f"FAIL arithmetic driver (exit {res.returncode}): {last}")
    else:
      skipped += 1
      print("SKIP arithmetic driver: it tests ./gkc only")

  total = time.monotonic() - t0
  print(f"regress: {passed} passed, {failed} failed, {skipped} skipped, "
        f"{total:.1f} s, binary {binary}")
  return 0 if failed == 0 else 1


if __name__ == "__main__":
  sys.exit(main())
