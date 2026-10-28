#!/usr/bin/env python3
# Replays the compile steps of .wasmify/wasm-build.json in parallel (same executable, args,
# work_dir as wasmify's serial executor). Skips steps whose output already exists.
import json, os, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor, as_completed
plan = json.load(open('.wasmify/wasm-build.json'))
steps = [s for s in plan['steps'] if s['type'] == 'compile' and not s.get('skipped')]
todo = [s for s in steps if not os.path.exists(s['output_file'])]
print(f"compile steps: {len(steps)} total, {len(todo)} to do", flush=True)
def run(s):
    os.makedirs(os.path.dirname(s['output_file']), exist_ok=True)
    t = time.time()
    p = subprocess.run([s['executable']] + s['args'], cwd=s['work_dir'], capture_output=True, text=True)
    return s, p.returncode, time.time() - t, p.stderr[-3000:]
fails = 0; done = 0; t0 = time.time()
with ThreadPoolExecutor(max_workers=int(sys.argv[1]) if len(sys.argv) > 1 else 9) as ex:
    futs = [ex.submit(run, s) for s in todo]
    for f in as_completed(futs):
        s, rc, dt, err = f.result(); done += 1
        if rc != 0:
            fails += 1
            try: os.remove(s['output_file'])
            except FileNotFoundError: pass
            print(f"FAILED rc={rc} {s['output_file']}\n{err}", flush=True)
        if done % 25 == 0 or rc != 0:
            print(f"[{done}/{len(todo)}] {int(time.time()-t0)}s fails={fails}", flush=True)
print(f"DONE {done} compiled, {fails} failed, {int(time.time()-t0)}s", flush=True)
sys.exit(1 if fails else 0)
