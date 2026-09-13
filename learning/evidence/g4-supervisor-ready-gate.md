# G4 Supervisor READY dependency gate

The native Supervisor fixture now exercises an explicit dependency release:
the service announces `SERVICE WAITING DEPENDENCY` and blocks on its IPC
endpoint. The Supervisor announces READY, sends the release marker, and the
service then emits `SERVICE DEPENDENCY READY`. The test checks both markers and
their order in the QEMU serial log before accepting the later restart/reap
markers.

```text
PASS: native Supervisor releases a dependent Ring 3 service only after READY
```

This is a bounded two-process readiness fixture. It does not provide general
multi-service scheduling, persistent dependency state, or a production service
manager.
