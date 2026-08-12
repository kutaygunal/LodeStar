#!/usr/bin/env bash
# monitor_agents.sh — Truncation watchdog for herdr/pi agents.
#
# Watches one or more agents' recent output for the truncation marker
# "Response was truncated before completion." and, when found, sends a
# "continue" prompt so the agent finishes its response.
#
# Usage:
#   monitor_agents.sh <agent> [<agent> ...] [--interval <sec>] [--cooldown <sec>]
#   monitor_agents.sh --all [--interval <sec>] [--cooldown <sec>]  # every registered agent
#
# Examples:
#   monitor_agents.sh orchestrator
#   monitor_agents.sh orchestrator planner scrum-master --interval 5
#   monitor_agents.sh --all --interval 8 --cooldown 30
#
# It only sends "continue" when the agent's output CHANGES to a new truncated
# state AND the per-agent cooldown has elapsed, so it never spams the same
# agent for the same stuck text.

set -euo pipefail

# --- resolve the herdr command ---------------------------------------------
# In a fresh pane the shell is WSL bash (paths /mnt/c/...) and herdr is a
# Windows exe; in the orchestrator shell it is Git Bash (/c/...). Try the known
# locations, then fall back to `herdr` on PATH.
HERDR=""
for cand in \
  "/mnt/c/Users/kutay/AppData/Local/Programs/Herdr/bin/herdr.exe" \
  "/c/Users/kutay/AppData/Local/Programs/Herdr/bin/herdr.exe" \
  "/c/Users/kutay/AppData/Local/Programs/Herdr/bin/herdr"; do
  if [ -x "$cand" ]; then HERDR="$cand"; break; fi
done
if [ -z "$HERDR" ] && command -v herdr >/dev/null 2>&1; then
  HERDR="herdr"
fi
if [ -z "$HERDR" ]; then
  echo "monitor_agents: cannot find herdr (checked known paths and PATH)" >&2
  exit 1
fi

MARKER="Response was truncated before completion"
INTERVAL=10
COOLDOWN=30   # seconds to wait after sending continue before nudging the same agent again
AGENTS=()
ALL=0

# --- parse args ------------------------------------------------------------
while [ $# -gt 0 ]; do
  case "$1" in
    --all) ALL=1 ;;
    --cooldown) COOLDOWN="${2:?--cooldown needs a value}"; shift ;;
    --cooldown=*) COOLDOWN="${1#*=}" ;;
    --interval) INTERVAL="${2:?--interval needs a value}"; shift ;;
    --interval=*) INTERVAL="${1#*=}" ;;
    -*) echo "unknown option: $1" >&2; exit 1 ;;
    *) AGENTS+=("$1") ;;
  esac
  shift
done

if [ "$ALL" -eq 1 ]; then
  AGENTS=()
  # Discover all registered agent names from `herdr agent list`.
  mapfile -t AGENTS < <("$HERDR" agent list 2>/dev/null | grep -o '"name":"[^"]*"' | sed 's/"name":"//; s/"$//' | sort -u)
fi

if [ "${#AGENTS[@]}" -eq 0 ]; then
  echo "No agents to monitor. Pass agent names or --all." >&2
  exit 1
fi

echo "monitor_agents: herdr=$HERDR"
echo "monitor_agents: watching ${#AGENTS[@]} agent(s): ${AGENTS[*]} (interval ${INTERVAL}s, cooldown ${COOLDOWN}s)"
echo "monitor_agents: marker = '$MARKER'"

# last-seen output per agent (avoid re-sending for the same text) and the
# epoch time of the last continue sent (cooldown guard).
declare -A LAST
declare -A LAST_SENT
PREV_AGENTS=""

now() { date +%s 2>/dev/null || echo 0; }

# discover_agents: refresh the agent list each cycle in --all mode so newly
# spawned agents are picked up and closed ones are dropped. Logs set changes.
discover_agents() {
  if [ "$ALL" -eq 1 ]; then
    mapfile -t AGENTS < <("$HERDR" agent list 2>/dev/null | grep -o '"name":"[^"]*"' | sed 's/"name":"//; s/"$//' | sort -u)
    local joined="${AGENTS[*]:-}"
    if [ "$joined" != "$PREV_AGENTS" ]; then
      echo "$(date -Is 2>/dev/null || date) | agents now: ${AGENTS[*]:-none}"
      PREV_AGENTS="$joined"
    fi
  fi
}

while true; do
  discover_agents
  for AGENT in "${AGENTS[@]}"; do
    OUT="$("$HERDR" agent read "$AGENT" --source recent-unwrapped --lines 40 2>&1 || true)"
    if printf '%s' "$OUT" | grep -qF "$MARKER"; then
      # Only nudge if the output changed AND the cooldown has elapsed.
      if [ "${LAST[$AGENT]:-}" != "$OUT" ] && [ $(( $(now) - ${LAST_SENT[$AGENT]:-0} )) -ge "$COOLDOWN" ]; then
        echo "$(date -Is 2>/dev/null || date) | $AGENT TRUNCATED -> sending continue"
        "$HERDR" agent prompt "$AGENT" "continue" --wait --timeout 0 >/dev/null 2>&1 || true
        LAST[$AGENT]="$OUT"
        LAST_SENT[$AGENT]=$(now)
      fi
    else
      # No marker now; clear the guard so a future truncation is caught.
      LAST[$AGENT]=""
    fi
  done
  sleep "$INTERVAL"
done
