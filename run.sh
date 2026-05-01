#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

BIN="./build/exhaustive/exhaustive_weakly_stable_binary_col_prefs"
RESULT_DIR="./exhaustive/results"
PID_FILE="$RESULT_DIR/weakly_stable_symmetry.pid"
LATEST_FILE="$RESULT_DIR/weakly_stable_symmetry.latest"

start() {
    mkdir -p "$RESULT_DIR"

    if [[ -f "$PID_FILE" ]]; then
        old_pid="$(cat "$PID_FILE")"
        if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
            echo "Already running: pid=$old_pid"
            echo "Log: $(cat "$LATEST_FILE" 2>/dev/null || echo 'unknown')"
            exit 0
        fi
    fi

    if [[ ! -x "$BIN" ]]; then
        echo "Binary not found: $BIN"
        echo "Build first: cmake -S . -B build && cmake --build build -j"
        exit 1
    fi

    ts="$(date +%Y%m%d_%H%M%S)"
    out="$RESULT_DIR/weakly_stable_symmetry_${ts}.txt"

    nohup "$BIN" > "$out" 2>&1 &
    pid="$!"

    echo "$pid" > "$PID_FILE"
    echo "$out" > "$LATEST_FILE"

    echo "Started: pid=$pid"
    echo "Log: $out"
}

stop() {
    if [[ ! -f "$PID_FILE" ]]; then
        echo "No pid file: $PID_FILE"
        exit 0
    fi

    pid="$(cat "$PID_FILE")"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        kill "$pid"
        echo "Stopped: pid=$pid"
    else
        echo "Process already stopped: pid=$pid"
    fi
}

status() {
    if [[ ! -f "$PID_FILE" ]]; then
        echo "Not running (no pid file)."
        exit 0
    fi

    pid="$(cat "$PID_FILE")"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        echo "Running: pid=$pid"
        ps -p "$pid" -o pid,etimes,pcpu,pmem,cmd
        if [[ -f "$LATEST_FILE" ]]; then
            echo "Log: $(cat "$LATEST_FILE")"
        fi
    else
        echo "Not running (stale pid file): pid=$pid"
    fi
}

tail_log() {
    if [[ ! -f "$LATEST_FILE" ]]; then
        echo "No latest log file. Start first with ./run.sh start"
        exit 1
    fi

    log_file="$(cat "$LATEST_FILE")"
    if [[ ! -f "$log_file" ]]; then
        echo "Log file not found: $log_file"
        exit 1
    fi

    tail -n 80 "$log_file"
}

usage() {
    echo "Usage: ./run.sh {start|stop|status|tail}"
}

cmd="${1:-start}"
case "$cmd" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    status)
        status
        ;;
    tail)
        tail_log
        ;;
    *)
        usage
        exit 1
        ;;
esac
