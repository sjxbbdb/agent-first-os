#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
evidence_dir="$repo_root/build/verification"
mkdir -p "$evidence_dir"

printf 'Running g6-mmio-map-test.sh\n'
bash "$repo_root/product/tests/g6-mmio-map-test.sh" \
    > "$evidence_dir/g6-mmio-map-test.log" 2>&1
tail -n 1 "$evidence_dir/g6-mmio-map-test.log"

for focused in g6-virtio-net-rx-test.sh g6-virtio-input-test.sh; do
    printf 'Running %s\n' "$focused"
    bash "$repo_root/product/tests/$focused" \
        > "$evidence_dir/${focused%.sh}.log" 2>&1
    tail -n 1 "$evidence_dir/${focused%.sh}.log"
done

# BIOS tests deliberately reuse build/bios and inject different images. They
# must run serially; running them in parallel corrupts artifacts and locks disks.
for test in g0-contract-test.sh g1-high-half-test.sh g2-physical-allocator-test.sh g2-process-test.sh g2-address-space-test.sh g2-elf-loader-test.sh g2-irq-test.sh g2-kernel-test.sh g2-wait-kill-test.sh g2-fault-recovery-test.sh g3-ipc-caps-test.sh g3-cap-lineage-test.sh g3-shm-test.sh g3-dynamic-endpoint-test.sh g3-endpoint-destroy-lineage-test.sh g3-kernel-ipc-test.sh g3-kernel-blocking-ipc-test.sh g3-kernel-multiwait-ipc-test.sh g3-kernel-ipc-cancel-test.sh g3-native-ipc-close-test.sh g3-native-dynamic-endpoint-test.sh g3-kernel-shm-test.sh g3-kernel-capability-test.sh g3-kernel-cap-transfer-test.sh g4-cap-reclaim-test.sh g4-clean-restart-cap-test.sh g4-group-recursive-test.sh g4-native-recursive-task-group-test.sh g4-native-supervisor-test.sh g4-supervisor-ready-gate-test.sh g4-supervisor-fault-restart-test.sh g4-task-group-test.sh g4-native-task-group-test.sh g4-group-freeze-test.sh g4-native-group-freeze-test.sh g4-uefi-initrd-test.sh g4-multi-initrd-test.sh g5-uefi-stage0-test.sh g5-uefi-kernel-test.sh g5-uefi-map-key-test.sh g5-uefi-negative-test.sh g6-virtio-block-test.sh g7-native-block-write-test.sh g7-native-block-flush-test.sh g7-native-block-read-test.sh g7-native-journal-recovery-test.sh g7-native-journal-commit-recovery-test.sh g6-virtio-input-bar-contract-test.sh g6-virtio-modern-caps-test.sh g6-virtio-rx-protocol-test.sh g6-virtio-rx-abi-test.sh g6-virtio-net-tx-test.sh g6-native-file-ipc-test.sh g6-native-file-write-test.sh g6-native-io-test.sh g6-native-input-window-test.sh g7-kernel-token-test.sh g7-kernel-pause-test.sh g9-native-agent-test.sh g9-runtime-service-abi-test.sh g9-native-runtime-service-test.sh g9-uefi-runtime-service-test.sh g10-uefi-agent-test.sh g5-g6-contract-test.sh stage1-test.sh stage2-test.sh \
            stage3-test.sh stage4-test.sh stage5-test.sh stage7-test.sh g2-kernel-test.sh \
            stage8-test.sh g1-permission-test.sh supervisor-protocol-test.sh supervisor-runtime-test.sh supervisor-g4-group-test.sh g6-service-test.sh g8-native-semantic-test.sh g10-uefi-semantic-test.sh g10-native-chain-test.sh g10-uefi-chain-test.sh; do
    printf 'Running %s\n' "$test"
    bash "$repo_root/product/tests/$test" > "$evidence_dir/${test%.sh}.log" 2>&1 || {
        cat "$evidence_dir/${test%.sh}.log" >&2
        exit 1
    }
    tail -n 1 "$evidence_dir/${test%.sh}.log"
done

printf 'Running g7-g10-host-loop-test.sh\n'
bash "$repo_root/product/tests/g7-g10-host-loop-test.sh" \
    > "$evidence_dir/g7-g10-host-loop.log" 2>&1
cat "$evidence_dir/g7-g10-host-loop.log"

printf 'Running g7-durable-journal-test.sh\n'
bash "$repo_root/product/tests/g7-durable-journal-test.sh" \
    > "$evidence_dir/g7-durable-journal.log" 2>&1
cat "$evidence_dir/g7-durable-journal.log"

printf 'Running g9-durable-checkpoint-test.sh\n'
bash "$repo_root/product/tests/g9-durable-checkpoint-test.sh" \
    > "$evidence_dir/g9-durable-checkpoint.log" 2>&1
cat "$evidence_dir/g9-durable-checkpoint.log"

printf 'Running g9-host-boundary-test.sh\n'
bash "$repo_root/product/tests/g9-host-boundary-test.sh" \
    > "$evidence_dir/g9-host-boundary.log" 2>&1
cat "$evidence_dir/g9-host-boundary.log"

printf 'Running g8-native-semantic-negative-test.sh\n'
bash "$repo_root/product/tests/g8-native-semantic-negative-test.sh" \
    > "$evidence_dir/g8-native-semantic-negative.log" 2>&1
cat "$evidence_dir/g8-native-semantic-negative.log"

printf 'Running g7-native-journal-contract-test.sh\n'
bash "$repo_root/product/tests/g7-native-journal-contract-test.sh" \
    > "$evidence_dir/g7-native-journal-contract.log" 2>&1
cat "$evidence_dir/g7-native-journal-contract.log"

printf 'Running g9-http-adapter-test.sh\n'
bash "$repo_root/product/tests/g9-http-adapter-test.sh" \
    > "$evidence_dir/g9-http-adapter.log" 2>&1
cat "$evidence_dir/g9-http-adapter.log"

printf 'Running g10-disconnect-recovery-test.sh\n'
bash "$repo_root/product/tests/g10-disconnect-recovery-test.sh" \
    > "$evidence_dir/g10-disconnect-recovery.log" 2>&1
cat "$evidence_dir/g10-disconnect-recovery.log"

node "$repo_root/product/agent-runtime/reference/conformance.test.js" \
    > "$evidence_dir/agent-runtime.log" 2>&1
cat "$evidence_dir/agent-runtime.log"
node "$repo_root/product/tests/g9-adapter-test.js" \
    > "$evidence_dir/g9-adapter.log" 2>&1
cat "$evidence_dir/g9-adapter.log"
node "$repo_root/product/tests/g9-pi-loop.test.js" \
    > "$evidence_dir/g9-pi-loop.log" 2>&1
cat "$evidence_dir/g9-pi-loop.log"
bash "$repo_root/product/tests/g9-pi-core-adapter-test.sh" \
    > "$evidence_dir/g9-pi-core-adapter.log" 2>&1
cat "$evidence_dir/g9-pi-core-adapter.log"

# Leave the default non-fault image available for manual use.
bash "$repo_root/product/tools/build-bios.sh" > "$evidence_dir/build-default.log" 2>&1
printf 'PASS: all currently implemented gates; logs: %s\n' "$evidence_dir"
