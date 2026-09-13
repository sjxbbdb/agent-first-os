#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
node "$repo_root/product/tests/g10-disconnect-recovery.test.js"
