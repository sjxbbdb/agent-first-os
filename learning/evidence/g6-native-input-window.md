# G6 native synthetic input/window IPC evidence

`g6-native-input-window-test.sh` exercises the existing BIOS Ring 3 IPC path
with the fixed synthetic network/input service exchange and the existing native
window service protocol contract. `G6 NATIVE NET INPUT OK` is therefore native
Ring 3 IPC evidence, not evidence
of a physical keyboard, virtio-input queue, or window compositor. The modern
virtio-input MMIO blocker remains tracked separately.
