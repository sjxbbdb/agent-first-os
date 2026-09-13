# G4 recursive task-group terminate

Ring 0 now provides `agent_os_process_group_terminate_tree`.  The caller must
be a live process; traversal follows parent links from each candidate and only
terminates descendants whose `group_id` matches.  The caller itself, ancestors,
unrelated sibling branches, and other groups remain untouched.  Traversal is
bounded by `AGENT_OS_MAX_PROCESSES` to fail closed on malformed parent chains.

Host evidence:

```text
bash product/tests/g4-group-recursive-test.sh
G4 recursive task-group terminate OK
```

The same API also has a three-process BIOS/QEMU fixture:

```text
bash product/tests/g4-native-recursive-task-group-test.sh
PASS: native Ring 3 recursive task-group termination reaches a grandchild
```

The native fixture is deliberately test-gated and bounded to one grandchild;
general process creation, recursive freeze/resume, and capability reaping on
clean exit remain separate work.
