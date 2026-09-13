# G6 synthetic QMP input boundary

`product/tests/g6-virtio-input-qmp-test.sh` sends a QEMU QMP
`input-send-event` key press/release for `virtio-keyboard-pci`. This is a
synthetic host-model injection probe, not physical keyboard evidence. It is
useful only if the guest has already armed the virtio-input queue. The script
exits 2 when QMP accepts the command but no guest `EVENT OK` completion is
observed; that result remains a blocker and is not converted into PASS.
