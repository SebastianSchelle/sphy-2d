#!/usr/bin/env bash
# Record and report game-server performance with Linux perf.
#
# Usage (via CMake targets — recommended):
#   cmake --preset profile && cmake --build --preset profile
#   cmake --build build/profile -j 12 -t profile-server          # deploy + record
#   cmake --build build/profile -t profile-server-report         # overhead by symbol
#   cmake --build build/profile -t profile-server-source         # source lines
#
# Direct usage:
#   ./scripts/profile-server.sh record
#   ./scripts/profile-server.sh report
#   ./scripts/profile-server.sh report-source
#   ./scripts/profile-server.sh top
#   ./scripts/profile-server.sh annotate 'Engine::tick'
#
# Environment:
#   SPHY_PROFILE_DURATION=30     Timed capture in seconds (default: until Ctrl+C)
#   SPHY_PROFILE_FREQUENCY=997   Sample rate (Hz)
#   SPHY_PROFILE_CALL_GRAPH=     perf call-graph mode (default: dwarf,8192)
#   SPHY_PROFILE_DATA=           Output perf.data path
#   SPHY_PROFILE_OUTPUT=         Redirect report modes to a file

set -euo pipefail

MODE="${1:-record}"
shift || true

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BINARY_DIR="${SPHY_PROFILE_BINARY_DIR:-$PROJECT_ROOT/build/profile}"
DEPLOY_DIR="${SPHY_PROFILE_DEPLOY_DIR:-$PROJECT_ROOT/deploy}"
SAVE_DIR="${SPHY_PROFILE_SAVE_DIR:-$PROJECT_ROOT/saves/test}"
PERF_DATA="${SPHY_PROFILE_DATA:-$BINARY_DIR/perf.data}"
PERF_OUTPUT="${SPHY_PROFILE_OUTPUT:-}"
FREQUENCY="${SPHY_PROFILE_FREQUENCY:-997}"
CALL_GRAPH="${SPHY_PROFILE_CALL_GRAPH:-dwarf,8192}"
DURATION="${SPHY_PROFILE_DURATION:-}"

SERVER="$DEPLOY_DIR/game-server"
WORK_DIR="$DEPLOY_DIR"

usage() {
    cat <<EOF
Usage: $(basename "$0") <mode> [args]

Modes:
  record          Run game-server under perf record (Ctrl+C or SPHY_PROFILE_DURATION)
  report          Flat symbol overhead report (function names, not assembly)
  report-source   Source-line report with inline attribution
  top             Hierarchical DSO/symbol report on stdout
  annotate SYM    Source annotation for one symbol
  script          Dump perf script (for speedscope / flame graphs)
  check           Verify Profile binary has debug info

Environment: SPHY_PROFILE_DURATION, SPHY_PROFILE_FREQUENCY, SPHY_PROFILE_CALL_GRAPH
EOF
}

require_perf_data() {
    if [[ ! -f "$PERF_DATA" ]]; then
        echo "Error: no profile data at $PERF_DATA" >&2
        echo "Run: cmake --build build/profile --target profile-server" >&2
        exit 1
    fi
}

warn_paranoid() {
    local paranoid
    if [[ -r /proc/sys/kernel/perf_event_paranoid ]]; then
        paranoid="$(cat /proc/sys/kernel/perf_event_paranoid)"
        if (( paranoid > 1 )); then
            echo "Warning: kernel.perf_event_paranoid=$paranoid may block perf." >&2
            echo "  sudo sysctl -w kernel.perf_event_paranoid=-1" >&2
        fi
    fi
}

has_debug_info() {
    local binary="$1"
    readelf -S "$binary" 2>/dev/null | grep -q '\.debug_info'
}

check_debug_info() {
    if [[ ! -x "$SERVER" ]]; then
        echo "Error: server binary not found: $SERVER" >&2
        echo "Build and deploy Profile first:" >&2
        echo "  cmake --preset profile && cmake --build build/profile -j 12 -t deploy" >&2
        exit 1
    fi
    if has_debug_info "$SERVER"; then
        return
    fi

    echo "Error: $SERVER has no .debug_info" >&2
    echo >&2
    if readelf -S "$SERVER" 2>/dev/null | grep -q '\.symtab'; then
        echo "The binary has symbols but no DWARF debug info — Profile compile flags were likely missing." >&2
        echo "Reconfigure and rebuild:" >&2
        echo "  cmake --preset profile" >&2
        echo "  cmake --build build/profile -j 12 --clean-first -t game-server" >&2
        echo "  cmake --build build/profile -j 12 -t profile-server" >&2
    else
        echo "Profile builds need debug symbols. Build from the profile preset:" >&2
        echo "  cmake --preset profile" >&2
        echo "  cmake --build build/profile -j 12 -t profile-server" >&2
    fi
    exit 1
}

run_report() {
    local -a perf_args=("$@")
    if [[ -n "$PERF_OUTPUT" ]]; then
        perf "${perf_args[@]}" >"$PERF_OUTPUT"
        echo "Wrote $PERF_OUTPUT"
    else
        perf "${perf_args[@]}"
    fi
}

case "$MODE" in
    record)
        warn_paranoid
        check_debug_info
        mkdir -p "$(dirname "$PERF_DATA")"
        rm -f "$PERF_DATA"

        echo "Recording to $PERF_DATA"
        echo "  call-graph: $CALL_GRAPH"
        echo "  frequency:  ${FREQUENCY} Hz"
        echo "  server:     $SERVER"
        echo "  save:       $SAVE_DIR"
        if [[ -n "$DURATION" ]]; then
            echo "  duration:   ${DURATION}s"
        else
            echo "  duration:   until Ctrl+C"
        fi
        echo

        record_cmd=(
            perf record
            -F "$FREQUENCY"
            -g
            --call-graph "$CALL_GRAPH"
            -o "$PERF_DATA"
            -- "$SERVER" -w "$WORK_DIR" -s "$SAVE_DIR"
        )

        if [[ -n "$DURATION" ]]; then
            timeout --signal=INT "${DURATION}s" "${record_cmd[@]}"
        else
            "${record_cmd[@]}"
        fi

        echo
        echo "Recorded $(perf report -i "$PERF_DATA" --header-only 2>/dev/null | grep -m1 'sample' || true)"
        echo "Report:  cmake --build $BINARY_DIR --target profile-server-report"
        echo "Source:  cmake --build $BINARY_DIR --target profile-server-source"
        ;;

    report)
        require_perf_data
        run_report report -i "$PERF_DATA" --stdio \
            --sort=overhead,symbol \
            --percent-limit=0.5 \
            --no-children
        ;;

    report-source)
        require_perf_data
        run_report report -i "$PERF_DATA" --stdio \
            --sort=overhead,symbol \
            --percent-limit=0.3 \
            --source \
            --inline
        ;;

    top)
        require_perf_data
        run_report report -i "$PERF_DATA" --stdio \
            --sort=overhead,dso,symbol \
            --percent-limit=0.5 \
            --hierarchy \
            --no-children
        ;;

    annotate)
        require_perf_data
        symbol="${1:-}"
        if [[ -z "$symbol" ]]; then
            echo "Error: annotate requires a symbol name" >&2
            exit 1
        fi
        perf annotate -i "$PERF_DATA" --stdio --source --symbol="$symbol"
        ;;

    script)
        require_perf_data
        perf script -i "$PERF_DATA"
        ;;

    check)
        check_debug_info
        echo "OK: $SERVER has debug info for source-level profiling"
        ;;

    -h|--help|help)
        usage
        ;;

    *)
        echo "Error: unknown mode '$MODE'" >&2
        usage >&2
        exit 1
        ;;
esac
