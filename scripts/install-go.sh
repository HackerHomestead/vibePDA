#!/bin/bash
# Install Go 1.23 to /usr/local/go. Requires sudo for system install.
# After install, add to ~/.bashrc: export PATH=/usr/local/go/bin:$PATH

set -e
GO_VER="1.24.2"
ARCH=$(uname -m)
OS=$(uname -s | tr '[:upper:]' '[:lower:]')

case "$ARCH" in
  x86_64|amd64) ARCH="amd64" ;;
  aarch64|arm64) ARCH="arm64" ;;
  *) echo "Unsupported arch: $ARCH"; exit 1 ;;
esac

URL="https://go.dev/dl/go${GO_VER}.${OS}-${ARCH}.tar.gz"
TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

echo "Downloading Go ${GO_VER}..."
curl -fsSL "$URL" -o "$TMP/go.tar.gz"

echo "Installing to /usr/local/go (requires sudo)..."
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf "$TMP/go.tar.gz"

echo "Done. Add to ~/.bashrc: export PATH=/usr/local/go/bin:\$PATH"
echo "Then run: source ~/.bashrc && go version"
