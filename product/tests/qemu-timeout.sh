#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -lt 3 ]]; then
    echo "usage: qemu-timeout.sh DURATION LOGFILE qemu-args..." >&2
    exit 2
fi

duration="$1"
log="$2"
shift 2

# QEMU's `-serial stdio` is not reliable when its stdout is a pipe on the
# WSL/PowerShell boundary.  Capture the guest UART with QEMU's file backend;
# retain stderr separately so boot diagnostics are still available.
qemu_args=("$@")
for ((index = 0; index + 1 < ${#qemu_args[@]}; index += 1)); do
    if [[ "${qemu_args[index]}" == "-serial" &&
          "${qemu_args[index + 1]}" == "stdio" ]]; then
        qemu_args[index + 1]="file:$log"
    fi
done
stderr_log="${log}.stderr"
: >"$log"
: >"$stderr_log"

# setsid gives this invocation its own process group.  The EXIT trap therefore
# only reaps the QEMU started by this runner, without matching unrelated QEMU
# processes owned by the caller or another test.
setsid timeout --foreground --signal=TERM --kill-after=2s "$duration" \
    qemu-system-x86_64 "${qemu_args[@]}" >"$stderr_log" 2>&1 &
runner_pid=$!
cleanup() {
    kill -- "-$runner_pid" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM

set +e
wait "$runner_pid"
status=$?
set -e
cat "$stderr_log" >>"$log"
# Under heavy TCG/debug tracing QEMU can outlive TERM and be reaped by
# timeout's kill-after path.  It is still the requested bounded timeout, so
# expose the same stable status expected by every fixture.
if [[ "$status" -eq 137 ]]; then
    status=124
fi
# Some QEMU builds exit cleanly after handling the TERM sent by `timeout`,
# even though the bounded runner did expire. Preserve the stable timeout
# contract when the diagnostic proves that this was the runner's signal.
if [[ "$status" -eq 0 ]] && grep -Fq '(timeout)' "$stderr_log"; then
    status=124
fi
exit "$status"
