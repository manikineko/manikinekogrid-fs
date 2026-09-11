#!/usr/bin/env bash
set -euo pipefail

# Prompts for the Discord API key (application ID) and stores it in the user's
# config directory. The key is read by CMake at build time and by the
# discord_activity.lua plugin at runtime.

config_dir=""
if [ -n "${XDG_CONFIG_HOME:-}" ]; then
    config_dir="$XDG_CONFIG_HOME/manikineko"
elif [ -n "${HOME:-}" ]; then
    config_dir="$HOME/.config/manikineko"
elif [ -n "${APPDATA:-}" ]; then
    config_dir="$APPDATA/Manikineko"
else
    echo "Unable to determine config directory." >&2
    exit 1
fi

mkdir -p "$config_dir"
chmod 700 "$config_dir"

key_file="$config_dir/discord_api_key"

if [ -t 0 ]; then
    read -rsp "Enter Discord API key (application ID): " discord_key
    echo
else
    echo "This script must be run interactively in a terminal." >&2
    exit 1
fi

if [ -z "$discord_key" ]; then
    echo "Error: no key entered." >&2
    exit 1
fi

printf '%s' "$discord_key" > "$key_file"
chmod 600 "$key_file"

echo "Discord API key saved to: $key_file"
