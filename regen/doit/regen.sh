#!/bin/bash
# Runs the googlesql-wasm pipeline (.github/workflows/build.yml "Run wasmify
# pipeline", also `make wasm`) inside the wasmify toolchain container, with
# one deviation: the wasi-sdk compile phase, which wasmify runs serially, is
# replayed in parallel from .wasmify/wasm-build.json (doit/par-compile.py,
# identical commands), then wasmify resumes from its object cache.
#
# Usage, from the googlesql-wasm checkout root, inside the container:
#   bash doit/regen.sh [workers]
set -uo pipefail
WORKERS="${1:-10}"
step() { echo "### $(date -u +%FT%TZ) $*"; }
run() {
  step "BEGIN $*"; local t=$(date +%s)
  bash -c "$*"; local rc=$?
  step "END   $* rc=$rc elapsed=$(( $(date +%s) - t ))s"
  [ $rc -ne 0 ] && exit $rc
  return 0
}
run "wasmify build --non-interactive"
run "wasmify generate-build"
run "wasmify parse-headers"
run "wasmify gen-proto"
run "wasmify wasm-build --dry-run --non-interactive"
run "python3 doit/par-compile.py $WORKERS || true"
run "wasmify wasm-build --optimize --non-interactive"
run "buf generate --timeout 30m"
run "make bundle-gomod"
step "PIPELINE OK"
