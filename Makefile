GOOGLESQL_WASM_REPO     ?= goccy/googlesql-wasm
GOOGLESQL_WASM_VERSION  ?= v0.3.1
GOOGLESQL_WASM_WORKFLOW ?= goccy/googlesql-wasm/.github/workflows/build.yml

BRIDGE_ASSET     := googlesql_wasm2go.go
BRIDGE_FILE      := googlesql.go
RELEASE_URL       = https://github.com/$(GOOGLESQL_WASM_REPO)/releases/download/$(GOOGLESQL_WASM_VERSION)
ATTESTATION_API   = https://api.github.com/repos/$(GOOGLESQL_WASM_REPO)/attestations

.PHONY: googlesql verify download-upstream verify-upstream test

## googlesql: in the DoiT fork the bridge is not downloaded from a goccy
## release; it is regenerated with the pipeline in REGENERATION.md, which
## also refreshes googlesql.go.sha256. The upstream download/attestation
## targets are kept below for reference only.
googlesql:
	@echo "doitintl/go-googlesql: regenerate googlesql.go with the pipeline in REGENERATION.md" >&2; exit 2

## verify: confirm googlesql.go is the file the regeneration recorded.
verify:
	shasum -a 256 -c googlesql.go.sha256

## download-upstream / verify-upstream: goccy's release flow, unchanged.
download-upstream:
	curl -fSL --proto '=https' --tlsv1.2 -o $(BRIDGE_FILE) $(RELEASE_URL)/$(BRIDGE_ASSET)

verify-upstream:
	@set -eu; \
	tmpdir=$$(mktemp -d); \
	bundle=$$tmpdir/bundle.jsonl; \
	trap 'rm -rf $$tmpdir' EXIT; \
	digest=$$(shasum -a 256 $(BRIDGE_FILE) | awk '{print $$1}'); \
	echo "==> fetching attestation bundle for $(BRIDGE_FILE) (sha256:$$digest)"; \
	curl -fsSL --proto '=https' --tlsv1.2 \
	  "$(ATTESTATION_API)/sha256:$$digest" \
	  | jq -c '.attestations[].bundle' > $$bundle; \
	echo "==> verifying $(BRIDGE_FILE)"; \
	GH_TOKEN= GITHUB_TOKEN= gh attestation verify "$(BRIDGE_FILE)" \
	  -R $(GOOGLESQL_WASM_REPO) \
	  --bundle $$bundle \
	  --signer-workflow $(GOOGLESQL_WASM_WORKFLOW)

## test: run the Go test suite.
test:
	go test ./...
