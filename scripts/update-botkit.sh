#!/usr/bin/env bash
# Refresh the vendored botkit.hpp from WyattAu/bot-kit@main and re-pin.
# Run when bot-kit's shared layer changes; review the diff before committing.
set -euo pipefail
cd "$(dirname "$0")/.."

curl -fsSL "https://raw.githubusercontent.com/WyattAu/bot-kit/main/cpp/botkit/botkit.hpp" -o vendor/botkit.hpp

UPSTREAM="$(curl -fsSL "https://api.github.com/repos/WyattAu/bot-kit/commits/main" | python3 -c 'import json,sys; print(json.load(sys.stdin)["sha"])')"
echo "$UPSTREAM" > BOTKIT_PIN

echo "vendored botkit.hpp refreshed, pinned to bot-kit@$UPSTREAM"
git diff --stat
