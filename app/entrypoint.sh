#!/usr/bin/env bash
set -euo pipefail

export LD_LIBRARY_PATH="/app/lib:${LD_LIBRARY_PATH:-}"

WAKE_SCRIPT="${WAKE_SCRIPT:-/app/python/wake/wake_word.py}"
WAKE_RESTARTS="${WAKE_RESTARTS:-10}"      # max restarts in a burst
WAKE_BACKOFF_SEC="${WAKE_BACKOFF_SEC:-1}" # restart delay
REQUIRE_WAKE="${REQUIRE_WAKE:-1}"         # 1 = if wake can't stay up, stop all

wake_pid=""

cleanup() {
  echo "[entrypoint] shutting down..."
  if [[ -n "${wake_pid}" ]] && kill -0 "${wake_pid}" 2>/dev/null; then
    kill "${wake_pid}" 2>/dev/null || true
    wait "${wake_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

start_wake() {
  echo "[entrypoint] starting wakeword: python3 ${WAKE_SCRIPT}"
  python3 "${WAKE_SCRIPT}" &
  wake_pid=$!
}

# Start backend first (or wake first — your choice)
echo "[entrypoint] starting backend..."
/app/bmo_backend &
backend_pid=$!

# Start wakeword
echo "[entrypoint] starting wake word..."
start_wake

# Supervision loop
restarts=0
while true; do
  # If backend dies, stop everything.
  if ! kill -0 "${backend_pid}" 2>/dev/null; then
    echo "[entrypoint] backend exited; stopping"
    exit 1
  fi

  # If wake dies, decide restart policy.
  if ! kill -0 "${wake_pid}" 2>/dev/null; then
    restarts=$((restarts + 1))
    echo "[entrypoint] wakeword exited (restart ${restarts}/${WAKE_RESTARTS})"

    if [[ "${restarts}" -gt "${WAKE_RESTARTS}" ]]; then
      if [[ "${REQUIRE_WAKE}" == "1" ]]; then
        echo "[entrypoint] wakeword keeps crashing; stopping whole stack"
        exit 1
      else
        echo "[entrypoint] wakeword disabled after repeated crashes; backend continues"
        wake_pid=""
        # just keep backend alive
        wait "${backend_pid}"
        exit $?
      fi
    fi

    sleep "${WAKE_BACKOFF_SEC}"
    start_wake
  fi

  sleep 0.5
done
