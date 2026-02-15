# BuildNumber is injected at build time; run "make build" or "make" to include version.
BUILD_VER := $(shell (cat VERSION 2>/dev/null || git describe --tags --always 2>/dev/null || echo dev) | tr -d '\n')

# Include config.mk from configure; fallback if not run
-include config.mk
GO ?= $(shell (command -v /usr/local/go/bin/go >/dev/null 2>&1 && echo /usr/local/go/bin/go) || echo go)

.PHONY: build all clean test check check-go
all: build

check-go:
	@ver=$$($(GO) version 2>/dev/null | sed -n 's/.*go\([0-9]*\.[0-9]*\.[0-9]*\).*/\1/p'); \
	major=$$(echo "$$ver" | cut -d. -f1); minor=$$(echo "$$ver" | cut -d. -f2); patch=$$(echo "$$ver" | cut -d. -f3); \
	need_major=1; need_minor=24; need_patch=2; \
	if [ -z "$$ver" ]; then echo "ERROR: go not found. Run: ./scripts/install-go.sh"; exit 1; fi; \
	if [ "$$major" -lt $$need_major ] || { [ "$$major" -eq $$need_major ] && [ "$$minor" -lt $$need_minor ]; }; then \
		echo "ERROR: Go 1.24.2+ required (found: $$ver). Run: ./scripts/install-go.sh"; exit 1; fi; \
	if [ "$$minor" -eq $$need_minor ] && [ -n "$$patch" ] && [ "$$patch" -lt $$need_patch ]; then \
		echo "ERROR: Go 1.24.2+ required (found: $$ver). Run: ./scripts/install-go.sh"; exit 1; fi

build: check-go
	$(GO) build -ldflags "-X github.com/you/vibe/internal/app.BuildNumber=$(BUILD_VER)" -o vibe ./cmd/vibe

clean:
	rm -f vibe

test: check-go
	$(GO) test ./...

check: check-go
	$(GO) test ./...
