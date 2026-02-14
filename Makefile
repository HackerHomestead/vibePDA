.PHONY: build
build:
	go build -ldflags "-X github.com/you/vibe/internal/app.BuildNumber=$$(date +%s)" -o vibe ./cmd/vibe
