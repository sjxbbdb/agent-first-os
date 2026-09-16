; Small statically linked Ring 3 program. It exercises the documented int 0x80 ABI.
bits 64
default rel

global user_entry
global user_entry_secondary
section .user_text
user_entry:
%ifdef AGENT_OS_TEST_G7_NATIVE_BLOCK_READ
    mov eax, 1
    lea rdi, [rel g7_block_read_start]
    mov esi, g7_block_read_start_end-g7_block_read_start-1
    int 0x80
    sub rsp, 512
    ; ABI, reserved sector, forged capability and unmapped destination must
    ; all fail before the device queue is touched.
    mov rdi, rbx
    mov esi, 1
    mov rdx, rsp
    mov r10d, 512
    xor r8d, r8d
    mov eax, 47
    int 0x80
    cmp rax, -22
    jne .g7_block_read_fail
    mov r8d, 1
    xor esi, esi
    mov eax, 47
    int 0x80
    cmp rax, -22
    jne .g7_block_read_fail
    mov rdi, 0x00000001000000ff
    mov esi, 1
    mov rdx, rsp
    mov r10d, 512
    mov r8d, 1
    mov eax, 47
    int 0x80
    cmp rax, -13
    jne .g7_block_read_fail
    mov rdi, rbx
    mov rdx, 0x00300000
    mov r10d, 512
    mov r8d, 1
    mov eax, 47
    int 0x80
    cmp rax, -14
    jne .g7_block_read_fail
    mov rdi, rbx
    mov esi, 1
    mov rdx, rsp
    mov r10d, 512
    mov r8d, 1
    mov eax, 47
    int 0x80
    cmp rax, 512
    jne .g7_block_read_fail
    mov rax, 0x31304b4f44414552 ; host seeded bytes: READOK01
    cmp qword [rsp], rax
    jne .g7_block_read_fail
    mov eax, 1
    lea rdi, [rel g7_block_read_ok]
    mov esi, g7_block_read_ok_end-g7_block_read_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g7_block_read_fail:
    add rsp, 512
    mov eax, 1
    lea rdi, [rel g7_block_read_fail_message]
    mov esi, g7_block_read_fail_message_end-g7_block_read_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
g7_block_read_start db 'G7 NATIVE BLOCK READ START',13,10,0
g7_block_read_start_end:
g7_block_read_ok db 'G7 NATIVE BLOCK READ OK',13,10,0
g7_block_read_ok_end:
g7_block_read_fail_message db 'G7 NATIVE BLOCK READ FAIL',13,10,0
g7_block_read_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_G7_NATIVE_BLOCK_FLUSH
    mov eax, 1
    lea rdi, [rel g7_block_flush_start]
    mov esi, g7_block_flush_start_end-g7_block_flush_start-1
    int 0x80
    ; Invalid opaque capability and ABI version both fail closed as
    ; EOPNOTSUPP, before the device queue can be touched.
    mov rdi, 0x00000001000000ff
    mov r8d, 1
    mov eax, 46                 ; SYS_VIRTIO_BLOCK_FLUSH
    int 0x80
    cmp rax, -95
    jne .g7_block_flush_fail
    mov rdi, rbx
    xor r8d, r8d
    mov eax, 46
    int 0x80
    cmp rax, -95
    jne .g7_block_flush_fail
    mov rdi, rbx
    mov r8d, 1                  ; AGENT_OS_VIRTIO_BLOCK_FLUSH_ABI_VERSION
    mov eax, 46
    int 0x80
    test rax, rax
    jnz .g7_block_flush_fail
    mov eax, 1
    lea rdi, [rel g7_block_flush_ok]
    mov esi, g7_block_flush_ok_end-g7_block_flush_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g7_block_flush_fail:
    mov eax, 1
    lea rdi, [rel g7_block_flush_fail_message]
    mov esi, g7_block_flush_fail_message_end-g7_block_flush_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
g7_block_flush_start db 'G7 NATIVE BLOCK FLUSH START',13,10,0
g7_block_flush_start_end:
g7_block_flush_ok db 'G7 NATIVE BLOCK FLUSH OK',13,10,0
g7_block_flush_ok_end:
g7_block_flush_fail_message db 'G7 NATIVE BLOCK FLUSH FAIL',13,10,0
g7_block_flush_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_G7_NATIVE_BLOCK_WRITE
    ; Test-gated native block service client.  The kernel passes the opaque
    ; device capability in RBX; the user buffer, sector and ABI fields are
    ; checked by Ring 0 before the virtio primitive can be reached.
    mov eax, 1
    lea rdi, [rel g7_block_write_start]
    mov esi, g7_block_write_start_end-g7_block_write_start-1
    int 0x80
    ; Invalid ABI version and reserved sector must fail before hardware I/O.
    mov rdi, rbx
    mov esi, 1
    lea rdx, [rel g7_block_write_data]
    mov r10d, 512
    xor r8d, r8d
    mov eax, 45
    int 0x80
    cmp rax, -22
    jne .g7_block_write_fail
    mov r8d, 1
    xor esi, esi
    mov eax, 45
    int 0x80
    cmp rax, -22
    jne .g7_block_write_fail
    ; A forged generation-tagged handle must fail closed as a capability error.
    mov rdi, 0x00000001000000ff
    mov esi, 1
    mov eax, 45
    int 0x80
    cmp rax, -13
    jne .g7_block_write_fail
    ; An unmapped user source buffer must be rejected before DMA submission.
    mov rdi, rbx
    mov esi, 1
    mov rdx, 0x00300000
    mov r10d, 512
    mov r8d, 1
    mov eax, 45
    int 0x80
    cmp rax, -14
    jne .g7_block_write_fail
    mov rdi, rbx
    mov esi, 1                  ; sector 0 is reserved for the boot image
    lea rdx, [rel g7_block_write_data]
    mov r10d, 512
    mov r8d, 1                  ; AGENT_OS_VIRTIO_BLOCK_WRITE_ABI_VERSION
    mov eax, 45                 ; SYS_VIRTIO_BLOCK_WRITE
    int 0x80
    cmp rax, 512
    jne .g7_block_write_fail
    mov eax, 1
    lea rdi, [rel g7_block_write_ok]
    mov esi, g7_block_write_ok_end-g7_block_write_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g7_block_write_fail:
    mov eax, 1
    lea rdi, [rel g7_block_write_fail_message]
    mov esi, g7_block_write_fail_message_end-g7_block_write_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
g7_block_write_start db 'G7 NATIVE BLOCK WRITE START',13,10,0
g7_block_write_start_end:
g7_block_write_ok db 'G7 NATIVE BLOCK WRITE OK',13,10,0
g7_block_write_ok_end:
g7_block_write_fail_message db 'G7 NATIVE BLOCK WRITE FAIL',13,10,0
g7_block_write_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_RUNTIME_SERVICE
    ; Bounded native Agent Runtime service bootstrap.  The primary Ring 3
    ; task sends START, then a HEARTBEAT; the sibling returns ACK before the
    ; checkpoint. Kernel code only transports/validates the IPC envelope.
    mov eax, 1
    lea rdi, [rel runtime_service_start]
    mov esi, runtime_service_start_end-runtime_service_start-1
    int 0x80
    mov eax, 16             ; SYS_IPC_CALL: START
    mov rdi, 0x0000000100000001
    mov esi, 0xD8
    mov edx, 1              ; sequence 1
    int 0x80
    test rax, rax
    jnz .runtime_service_fail
    mov eax, 2              ; let Supervisor-side service consume START
    int 0x80
    sub rsp, 128
    mov eax, 17             ; receive HEARTBEAT
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    test rax, rax
    jnz .runtime_service_fail_stack
    cmp dword [rsp+8], 0xD9
    jne .runtime_service_fail_stack
    cmp qword [rsp+24], 1
    jne .runtime_service_fail_stack
    mov eax, 16             ; ACK
    mov rdi, 0x0000000100000001
    mov esi, 0xDB
    mov edx, 1
    int 0x80
    test rax, rax
    jnz .runtime_service_fail_stack
    mov eax, 16             ; CHECKPOINT sequence 2
    mov rdi, 0x0000000100000001
    mov esi, 0xDA
    mov edx, 2
    int 0x80
    test rax, rax
    jnz .runtime_service_fail_stack
    mov eax, 2              ; let the Supervisor-side service persist it
    int 0x80
    mov eax, 1
    lea rdi, [rel runtime_service_checkpoint]
    mov esi, runtime_service_checkpoint_end-runtime_service_checkpoint-1
    int 0x80
    add rsp, 128
    mov eax, 1
    lea rdi, [rel runtime_service_ok]
    mov esi, runtime_service_ok_end-runtime_service_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.runtime_service_fail_stack:
    add rsp, 128
.runtime_service_fail:
    mov eax, 1
    lea rdi, [rel runtime_service_fail_message]
    mov esi, runtime_service_fail_message_end-runtime_service_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
runtime_service_start db 'AGENT RUNTIME SERVICE START',13,10,0
runtime_service_start_end:
runtime_service_checkpoint db 'AGENT RUNTIME CHECKPOINT SENT',13,10,0
runtime_service_checkpoint_end:
runtime_service_ok db 'AGENT RUNTIME SERVICE ABI OK',13,10,0
runtime_service_ok_end:
runtime_service_fail_message db 'AGENT RUNTIME SERVICE ABI FAIL',13,10,0
runtime_service_fail_message_end:
%else
%ifdef AGENT_OS_TEST_GROUP_RECURSIVE
    ; Let both descendants enter the scheduler, then terminate the entire
    ; group subtree and reap the direct child.
    mov eax, 2              ; SYS_YIELD
    int 0x80
    mov eax, 9              ; SYS_GROUP_TERMINATE_TREE
    mov rdi, 9
    mov rsi, -9
    int 0x80
    cmp rax, 2
    jne .recursive_group_fail
    mov eax, 3              ; SYS_WAIT direct child
    mov rdi, 0x0000000100000002
    int 0x80
    cmp rax, -9
    jnz .recursive_group_fail
    mov eax, 1
    lea rdi, [rel recursive_group_ok]
    mov esi, recursive_group_ok_end-recursive_group_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.recursive_group_fail:
    mov eax, 1
    lea rdi, [rel recursive_group_fail_message]
    mov esi, recursive_group_fail_message_end-recursive_group_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
recursive_group_ok db 'RECURSIVE GROUP TERMINATE OK',13,10,0
recursive_group_ok_end:
recursive_group_fail_message db 'RECURSIVE GROUP TERMINATE FAIL',13,10,0
recursive_group_fail_message_end:
%else
%ifdef AGENT_OS_TEST_SUPERVISOR_FAULT
    ; Native G4 fault-restart vertical slice.  The child service deliberately
    ; takes a user #PF; the exception path returns to this Ring 3 Supervisor,
    ; which restarts the parent-owned zombie and reaps its clean exit.
    mov eax, 1
    lea rdi, [rel g4_fault_supervisor_start]
    mov esi, g4_fault_supervisor_start_end-g4_fault_supervisor_start-1
    int 0x80
    sub rsp, 128
    mov eax, 2              ; SYS_YIELD: run the service
    int 0x80
    mov eax, 17             ; SYS_IPC_RECV: first heartbeat before #PF
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xB0
    jne .g4_fault_supervisor_fail
    mov eax, 1
    lea rdi, [rel g4_fault_supervisor_heartbeat]
    mov esi, g4_fault_supervisor_heartbeat_end-g4_fault_supervisor_heartbeat-1
    int 0x80
    mov eax, 5              ; SYS_RESTART: parent-owned zombie service
    mov rdi, 0x0000000100000002
    int 0x80
    mov eax, 2              ; SYS_YIELD: run the restarted service
    int 0x80
    mov eax, 17             ; SYS_IPC_RECV: heartbeat after restart
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xB1
    jne .g4_fault_supervisor_fail
    mov eax, 1
    lea rdi, [rel g4_fault_supervisor_restart]
    mov esi, g4_fault_supervisor_restart_end-g4_fault_supervisor_restart-1
    int 0x80
    mov eax, 3              ; SYS_WAIT: reap the restarted service
    mov rdi, 0x0000000100000002
    int 0x80
    mov eax, 1
    lea rdi, [rel g4_fault_supervisor_reaped]
    mov esi, g4_fault_supervisor_reaped_end-g4_fault_supervisor_reaped-1
    int 0x80
    add rsp, 128
    mov eax, 0              ; SYS_EXIT
    xor edi, edi
    int 0x80
.g4_fault_supervisor_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g4_fault_supervisor_fail_message]
    mov esi, g4_fault_supervisor_fail_message_end-g4_fault_supervisor_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
%else
%ifdef AGENT_OS_TEST_FAULT_RECOVERY
    ; Deliberately touch an unmapped user page.  The kernel must retire this
    ; task and return from the exception into the next scheduled task.
    mov rax, [abs 0x00300000]
.fault_recovery_halt:
    hlt
    jmp .fault_recovery_halt
%else
%ifdef AGENT_OS_TEST_IPC_CANCEL
    mov eax, 2              ; let child enter the blocking receive
    int 0x80
    mov eax, 20             ; SYS_IPC_CANCEL: parent cancels child wait
    mov rdi, 0x0000000100000002
    int 0x80
    test rax, rax
    jnz .ipc_cancel_fail
    mov eax, 2              ; child returns ECANCELED and exits
    int 0x80
    mov eax, 3              ; reap the cancelled child
    mov rdi, 0x0000000100000002
    int 0x80
    test rax, rax
    jnz .ipc_cancel_fail
    mov eax, 1
    lea rdi, [rel ipc_cancel_parent_ok]
    mov esi, ipc_cancel_parent_ok_end-ipc_cancel_parent_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.ipc_cancel_fail:
    mov eax, 1
    lea rdi, [rel ipc_cancel_fail_message]
    mov esi, ipc_cancel_fail_message_end-ipc_cancel_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
%endif
%ifdef AGENT_OS_TEST_IPC_BLOCKING_MULTI
    mov eax, 2              ; receiver two arrives first, independent of slot order
    int 0x80
    sub rsp, 128
    mov eax, 19             ; first receiver parks until sender wakes it
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xC1
    jne .ipc_multi_receiver_fail
    mov rax, 0x4D554C54494F4E01
    cmp qword [rsp+32], rax
    jne .ipc_multi_receiver_fail
    mov eax, 1
    lea rdi, [rel ipc_multi_receiver_one]
    mov esi, ipc_multi_receiver_one_end-ipc_multi_receiver_one-1
    int 0x80
    add rsp, 128
    mov eax, 0
    xor edi, edi
    int 0x80
.ipc_multi_receiver_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel ipc_multi_receiver_fail_message]
    mov esi, ipc_multi_receiver_fail_message_end-ipc_multi_receiver_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
%endif
%ifdef AGENT_OS_TEST_IPC_BLOCKING
    sub rsp, 128
    mov eax, 19             ; SYS_IPC_RECV_WAIT: no queued message yet
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xC0
    jne .ipc_blocking_fail
    mov rax, 0x424C4F434B4F4B01
    cmp qword [rsp+32], rax
    jne .ipc_blocking_fail
    add rsp, 128
    mov eax, 1
    lea rdi, [rel ipc_blocking_ok]
    mov esi, ipc_blocking_ok_end-ipc_blocking_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.ipc_blocking_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel ipc_blocking_fail_message]
    mov esi, ipc_blocking_fail_message_end-ipc_blocking_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
ipc_blocking_ok db 'IPC BLOCKING WAKE OK',13,10,0
ipc_blocking_ok_end:
ipc_blocking_fail_message db 'IPC BLOCKING FAIL',13,10,0
ipc_blocking_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_G6_NATIVE_IO
    mov eax, 1
    lea rdi, [rel g6_native_io_start]
    mov esi, g6_native_io_start_end-g6_native_io_start-1
    int 0x80
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xE8
    mov rdx, 0x4E45545F53454E44
    int 0x80
    mov eax, 2
    int 0x80
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xE9
    jne .g6_native_io_fail
    mov rax, 0x4E45545F4F4B01
    cmp qword [rsp+32], rax
    jne .g6_native_io_fail
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xEA
    mov rdx, 0x494E5055545F4556
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xEB
    jne .g6_native_io_fail
    mov rax, 0x494E5055544F4B01
    cmp qword [rsp+32], rax
    jne .g6_native_io_fail
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g6_native_io_ok]
    mov esi, g6_native_io_ok_end-g6_native_io_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g6_native_io_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g6_native_io_fail_message]
    mov esi, g6_native_io_fail_message_end-g6_native_io_fail_message-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
g6_native_io_start db 'G6 NATIVE IO START',13,10,0
g6_native_io_start_end:
g6_native_io_ok db 'G6 NATIVE NET INPUT OK',13,10,0
g6_native_io_ok_end:
g6_native_io_fail_message db 'G6 NATIVE NET INPUT FAIL',13,10,0
g6_native_io_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_G10_NATIVE
    mov eax, 1
    lea rdi, [rel g10_context]
    mov esi, g10_context_end-g10_context-1
    int 0x80
    mov eax, 1
    lea rdi, [rel g10_model]
    mov esi, g10_model_end-g10_model-1
    int 0x80
    mov eax, 1
    lea rdi, [rel g10_registry]
    mov esi, g10_registry_end-g10_registry-1
    int 0x80
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xB0
    xor edx, edx
    int 0x80
    mov eax, 2
    int 0x80
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xA1
    jne .g10_fail
    mov rax, 0x5245474953545259
    cmp qword [rsp+32], rax
    jne .g10_fail
    mov eax, 16
    mov esi, 0xB1
    xor edx, edx
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xA2
    jne .g10_fail
    mov rax, 0x46494C455F4F4B01
    cmp qword [rsp+32], rax
    jne .g10_fail
    mov eax, 16
    mov esi, 0xB2
    xor edx, edx
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xA3
    jne .g10_fail
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xB3
    xor edx, edx
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xA4
    jne .g10_fail
    mov rax, 0x46494C455F4F4B01
    cmp qword [rsp+32], rax
    jne .g10_fail
    mov eax, 1
    lea rdi, [rel g10_rollback_verified]
    mov esi, g10_rollback_verified_end-g10_rollback_verified-1
    int 0x80
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g10_ok]
    mov esi, g10_ok_end-g10_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g10_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g10_fail_message]
    mov esi, g10_fail_message_end-g10_fail_message-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
g10_context db 'CONTEXT COLLECTED',13,10,0
g10_context_end:
g10_model db 'MOCK MODEL PLAN VALID',13,10,0
g10_model_end:
g10_registry db 'REGISTRY RESOLVED',13,10,0
g10_registry_end:
g10_rollback_verified db 'ROLLBACK VERIFIED',13,10,0
g10_rollback_verified_end:
g10_ok db 'G10 NATIVE CHAIN OK',13,10,0
g10_ok_end:
g10_fail_message db 'G10 NATIVE CHAIN FAIL',13,10,0
g10_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_FILE_WRITE
    ; Agent submits a fixed FILE_WRITE request, then presents the one-shot
    ; policy token returned by the Ring 3 service.
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xE0                 ; FILE_WRITE request
    mov rdx, 0x46494C455F444154  ; fixed backend payload
    int 0x80
    mov eax, 2
    int 0x80                      ; let service mint and return token
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0xE1
    jne .file_write_fail
    mov rbx, [rsp + 32]
    add rsp, 128
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xE2                 ; authorized write
    mov rdx, rbx
    int 0x80
    mov eax, 2
    int 0x80
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    add rsp, 128
    ; Replay the same token; service must reject without a second write.
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xE2
    mov rdx, rbx
    int 0x80
    mov eax, 2
    int 0x80
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    add rsp, 128
    mov eax, 1
    lea rdi, [rel file_write_ok]
    mov esi, file_write_ok_end-file_write_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.file_write_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel file_write_fail_message]
    mov esi, file_write_fail_message_end-file_write_fail_message-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
file_write_ok db 'FILE_WRITE SUCCESS REPLAY_REJECTED SINGLE_WRITE',13,10,0
file_write_ok_end:
file_write_fail_message db 'FILE_WRITE FAIL',13,10,0
file_write_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_G6_NATIVE
    ; Native G6 file-service client.  The request and response are exchanged
    ; by two isolated Ring 3 processes through the kernel endpoint.  Ring 0
    ; transports bounded words and validates the capability; it does not
    ; interpret the file operation.
    mov eax, 16             ; SYS_IPC_CALL / FILE_READ
    mov rdi, 0x0000000100000001
    mov esi, 0xC0
    mov rdx, 0x706174682F64656D ; bounded path token: "path/demo"
    int 0x80
    mov eax, 2              ; let the Ring 3 file service handle it
    int 0x80
    sub rsp, 128
    mov eax, 17             ; SYS_IPC_RECV / FILE_DATA
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0xC1
    jne .g6_native_fail
    mov rax, 0x46494C452D4F4B01 ; bounded file result marker
    cmp qword [rsp + 32], rax
    jne .g6_native_fail
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g6_native_ok]
    mov esi, g6_native_ok_end - g6_native_ok - 1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g6_native_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g6_native_fail_message]
    mov esi, g6_native_fail_message_end - g6_native_fail_message - 1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
g6_native_ok db 'G6 NATIVE RING3 FILE IPC OK', 13, 10, 0
g6_native_ok_end:
g6_native_fail_message db 'G6 NATIVE RING3 FILE IPC FAIL', 13, 10, 0
g6_native_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_G8_NATIVE
    ; Native Ring 3 Semantic service slice.  Ring 0 only transports these
    ; bounded words; registry, context, action binding and Trusted Input are
    ; implemented by the Ring 3 Policy/Registry fixture below.
%ifdef AGENT_OS_TEST_G8_NATIVE_NEGATIVE
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0x7f
    mov rdx, 0x7368656c6c2e7772
    int 0x80
    mov eax, 16
    mov esi, 0x82
    mov rdx, 0xbadac71000000001
    int 0x80
    mov eax, 2
    int 0x80
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x9f
    jne .g8_native_negative_fail
    cmp qword [rsp + 32], 0
    jne .g8_native_negative_fail
    mov eax, 17
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x9f
    jne .g8_native_negative_fail
    cmp qword [rsp + 32], 0
    jne .g8_native_negative_fail
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g8_native_negative_ok]
    mov esi, g8_native_negative_ok_end-g8_native_negative_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g8_native_negative_fail:
    mov eax, 1
    lea rdi, [rel g8_native_negative_fail_message]
    mov esi, g8_native_negative_fail_message_end-g8_native_negative_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
g8_native_negative_ok db 'NATIVE REGISTRY NEGATIVE DENY OK', 13, 10, 0
g8_native_negative_ok_end:
g8_native_negative_fail_message db 'NATIVE REGISTRY NEGATIVE DENY FAIL', 13, 10, 0
g8_native_negative_fail_message_end:
%else
    mov eax, 16             ; unknown registry query (must be denied)
    mov rdi, 0x0000000100000001
    mov esi, 0x80
    mov rdx, 0xdeadbeef
    int 0x80
    mov eax, 16             ; valid registry query
    mov esi, 0x80
    mov rdx, 0x7368656c6c2e7772
    int 0x80
    mov eax, 16             ; current task window
    mov esi, 0x81
    mov rdx, 0x5441534b2d303031
    int 0x80
    mov eax, 16             ; action binding digest
    mov esi, 0x82
    mov rdx, 0x0a11ce0000000001
    int 0x80
    mov eax, 16             ; Trusted Input nonce
    mov esi, 0x83
    mov rdx, 0x5452555354454401
    int 0x80
    mov eax, 2              ; run the Ring 3 service
    int 0x80
    sub rsp, 128
    mov eax, 17             ; deny unknown tool
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x9f
    jne .g8_native_fail
    cmp qword [rsp + 32], 0
    jne .g8_native_fail
    mov eax, 17             ; registry descriptor
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x90
    jne .g8_native_fail
    mov rax, 0x0100000000000001
    cmp qword [rsp + 32], rax
    jne .g8_native_fail
    mov eax, 17             ; captured task window digest
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x91
    jne .g8_native_fail
    mov rax, 0x0c0ffee000000001
    cmp qword [rsp + 32], rax
    jne .g8_native_fail
    mov eax, 17             ; bound action digest
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x92
    jne .g8_native_fail
    mov rax, 0x0a11ce0000000001
    cmp qword [rsp + 32], rax
    jne .g8_native_fail
    mov eax, 17             ; Trusted Input admission
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x93
    jne .g8_native_fail
    mov rax, 0x5452555354454401
    cmp qword [rsp + 32], rax
    jne .g8_native_fail
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g8_native_ok]
    mov esi, g8_native_ok_end - g8_native_ok - 1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g8_native_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel g8_native_fail_message]
    mov esi, g8_native_fail_message_end - g8_native_fail_message - 1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
g8_native_ok db 'NATIVE REGISTRY CONTEXT TRUSTED INPUT OK', 13, 10, 0
g8_native_ok_end:
g8_native_fail_message db 'NATIVE REGISTRY CONTEXT TRUSTED INPUT FAIL', 13, 10, 0
g8_native_fail_message_end:
%endif
%endif
%ifdef AGENT_OS_TEST_AGENT_LOOP
    ; Agent -> Policy: structured action nonce over the endpoint, then yield
    ; so the policy service can validate and mint the bound token.
    mov eax, 16             ; SYS_IPC_CALL
    mov rdi, 0x0000000100000001
    mov esi, 0x70           ; ACTION_REQUEST
    mov edx, 0xABCD         ; action nonce
    int 0x80
    mov eax, 2              ; SYS_YIELD
    int 0x80
    sub rsp, 128
    mov eax, 17             ; SYS_IPC_RECV
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    mov rbx, [rsp + 32]     ; token in inline word 0
    add rsp, 128
    mov eax, 41             ; SYS_POLICY_TOKEN_CONSUME
    mov rdi, rbx
    mov rsi, 0x0000000100000001
    mov rdx, 0xABCD
    int 0x80
    mov eax, 1              ; SYS_WRITE
    lea rdi, [rel agent_action_committed]
    mov esi, agent_action_committed_end - agent_action_committed - 1
    int 0x80
%endif
%ifdef AGENT_OS_TEST_GROUP_FREEZE
    mov eax, 1
    lea rdi, [rel group_freeze_start]
    mov esi, group_freeze_start_end-group_freeze_start-1
    int 0x80
    mov eax, 7
    mov rdi, 7
    int 0x80
    cmp rax, 1
    jne .group_freeze_fail
    mov eax, 8
    mov rdi, 7
    int 0x80
    cmp rax, 1
    jne .group_freeze_fail
    mov eax, 2
    int 0x80
    mov eax, 3
    mov rdi, 0x0000000100000002
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.group_freeze_fail:
    mov eax, 0
    mov edi, 1
    int 0x80
group_freeze_start db 'GROUP FREEZE RING3 START',13,10,0
group_freeze_start_end:
%else
%ifdef AGENT_OS_TEST_IPC_CLOSE
    mov eax, 1
    lea rdi, [rel ipc_close_start]
    mov esi, ipc_close_start_end - ipc_close_start - 1
    int 0x80
    mov eax, 21             ; SYS_IPC_CLOSE, requires CAP_RIGHT_REVOKE
    mov rdi, 0x0000000100000001
    int 0x80
    test rax, rax
    jnz .ipc_close_fail
    mov eax, 2              ; let child prove old handle is closed
    int 0x80
 .ipc_close_wait:
    mov eax, 3
    mov rdi, 0x0000000100000002
    int 0x80
    cmp rax, -16             ; child may still be runnable after one yield
    je .ipc_close_wait_yield
    test rax, rax
    jnz .ipc_close_fail
    mov eax, 0
    xor edi, edi
    int 0x80
 .ipc_close_wait_yield:
    mov eax, 2
    int 0x80
    jmp .ipc_close_wait
.ipc_close_fail:
    mov eax, 1
    lea rdi, [rel ipc_close_fail_message]
    mov esi, ipc_close_fail_message_end - ipc_close_fail_message - 1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
ipc_close_start db 'IPC CLOSE RING3 START', 13, 10, 0
ipc_close_start_end:
ipc_close_fail_message db 'IPC CLOSE FAIL', 13, 10, 0
ipc_close_fail_message_end:
%else
%ifdef AGENT_OS_TEST_TASK_GROUP
    ; Native G4 task-group slice.  Group termination is parent-scoped in Ring
    ; 0 and returns the number of direct children transitioned to zombies.
    mov eax, 1
    lea rdi, [rel task_group_start]
    mov esi, task_group_start_end - task_group_start - 1
    int 0x80
    mov eax, 2              ; let the real Ring 3 child announce itself
    int 0x80
    mov eax, 6              ; SYS_GROUP_TERMINATE
    mov rdi, 7              ; child group assigned by the kernel bootstrap
    mov rsi, -15
    int 0x80
    cmp rax, 1
    jne .task_group_fail
    mov eax, 1
    lea rdi, [rel task_group_terminated]
    mov esi, task_group_terminated_end - task_group_terminated - 1
    int 0x80
    mov eax, 3              ; SYS_WAIT: reap the group child
    mov rdi, 0x0000000100000002
    int 0x80
    cmp rax, -15
    jnz .task_group_fail
    mov eax, 1
    lea rdi, [rel task_group_reaped]
    mov esi, task_group_reaped_end - task_group_reaped - 1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.task_group_fail:
    mov eax, 1
    lea rdi, [rel task_group_fail_message]
    mov esi, task_group_fail_message_end - task_group_fail_message - 1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
task_group_start db 'TASK GROUP RING3 START', 13, 10, 0
task_group_start_end:
task_group_terminated db 'TASK GROUP TERMINATE OK', 13, 10, 0
task_group_terminated_end:
task_group_reaped db 'TASK GROUP CHILD REAPED', 13, 10, 0
task_group_reaped_end:
task_group_fail_message db 'TASK GROUP FAIL', 13, 10, 0
task_group_fail_message_end:
%else
%ifdef AGENT_OS_TEST_SUPERVISOR
    ; Native G4 vertical slice.  This Ring 3 process is the Supervisor: it
    ; yields to the kernel-created child service, receives its heartbeat,
    ; restarts the exited child through the parent-checked syscall, then
    ; receives the second heartbeat and reaps the child.
    mov eax, 1              ; SYS_WRITE
    lea rdi, [rel supervisor_start]
    mov esi, supervisor_start_end - supervisor_start - 1
    int 0x80
    sub rsp, 128
%ifdef AGENT_OS_TEST_SUPERVISOR_READY_GATE
    ; READY is a Supervisor event: the dependent service is released only
    ; after this parent has announced its own startup.
    mov eax, 1
    lea rdi, [rel supervisor_ready]
    mov esi, supervisor_ready_end - supervisor_ready - 1
    int 0x80
%endif
    mov eax, 2              ; SYS_YIELD: run the service
    int 0x80
%ifdef AGENT_OS_TEST_SUPERVISOR_READY_GATE
    ; The first yield lets the dependent service block in SYS_IPC_RECV.  Only
    ; after the scheduler returns here is the READY release marker sent.
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xD8
    mov edx, 1
    int 0x80
    mov eax, 1
    lea rdi, [rel supervisor_gate_sent]
    mov esi, supervisor_gate_sent_end - supervisor_gate_sent - 1
    int 0x80
%endif
    mov eax, 17             ; SYS_IPC_RECV: heartbeat generation 1
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    mov eax, 1              ; SYS_WRITE
    lea rdi, [rel supervisor_heartbeat]
    mov esi, supervisor_heartbeat_end - supervisor_heartbeat - 1
    int 0x80
    mov eax, 5              ; SYS_RESTART: parent-owned zombie service
    mov rdi, 0x0000000100000002
    int 0x80
%ifdef AGENT_OS_TEST_SUPERVISOR_READY_GATE
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xD8
    mov edx, 2
    int 0x80
%endif
    mov eax, 2              ; SYS_YIELD: run restarted service
    int 0x80
    mov eax, 17             ; SYS_IPC_RECV: heartbeat generation 2
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    mov eax, 1              ; SYS_WRITE
    lea rdi, [rel supervisor_restarted]
    mov esi, supervisor_restarted_end - supervisor_restarted - 1
    int 0x80
    mov eax, 3              ; SYS_WAIT: observe/reap service exit
    mov rdi, 0x0000000100000002
    int 0x80
    mov eax, 1              ; SYS_WRITE
    lea rdi, [rel supervisor_exit]
    mov esi, supervisor_exit_end - supervisor_exit - 1
    int 0x80
    add rsp, 128
    mov eax, 0              ; SYS_EXIT
    xor edi, edi
    int 0x80
.supervisor_halt:
    hlt
    jmp .supervisor_halt
supervisor_start db 'SUPERVISOR RING3 START', 13, 10, 0
supervisor_start_end:
supervisor_ready db 'SUPERVISOR READY', 13, 10, 0
supervisor_ready_end:
supervisor_gate_sent db 'SUPERVISOR READY GATE SENT', 13, 10, 0
supervisor_gate_sent_end:
supervisor_heartbeat db 'SUPERVISOR HEARTBEAT 1', 13, 10, 0
supervisor_heartbeat_end:
supervisor_restarted db 'SUPERVISOR RESTART HEARTBEAT 2', 13, 10, 0
supervisor_restarted_end:
supervisor_exit db 'SUPERVISOR SERVICE EXIT REAPED', 13, 10, 0
supervisor_exit_end:
%else
%ifdef AGENT_OS_TEST_SHM
    mov eax, 33             ; SYS_SHM_MAP
    mov rdi, 0x0000000100000002
    mov rsi, 0x00500000
    int 0x80
    mov rax, 0x20474e4950204d48 ; "HM PING " in little endian
    mov [abs 0x00500000], rax
    mov byte [abs 0x00500008], 13
    mov byte [abs 0x00500009], 10
    mov eax, 1              ; SYS_WRITE
    mov rdi, 0x00500000
    mov esi, 10
    int 0x80
%endif
%ifdef AGENT_OS_TEST_IRQ_WAIT
    ; Stay in Ring 3 across a BIOS PIT period so inherited IRQ routing cannot
    ; hide behind the very short normal smoke-test execution.
    ; Keep the wait comfortably above one legacy PIT period while remaining
    ; bounded under the BIOS test's five-second QEMU timeout on slow TCG.
    mov ecx, 10000000
.irq_wait:
    dec ecx
    jnz .irq_wait
%endif
%ifdef AGENT_OS_TEST_IPC
    ; The fixture receives the opaque generation-tagged SEND capability minted
    ; by Ring 0 for the loopback endpoint, then submits one inline word.
    mov eax, 16             ; SYS_IPC_CALL
    mov rdi, 0x0000000100000001
    mov esi, 0x42
    mov edx, 0xCAFE
    int 0x80
    sub rsp, 128
    mov eax, 17             ; SYS_IPC_RECV
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    add rsp, 128
%endif
%ifdef AGENT_OS_TEST_CAPS
    ; Derive a narrower endpoint capability, then revoke the derived handle.
    ; Ring 0 must return the opaque generation-tagged handle in RAX and never
    ; trust a user-supplied object pointer.
    mov eax, 24             ; SYS_CAP_RESTRICT
    mov rdi, 0x0000000100000001
    mov rsi, 0x90           ; SEND | REVOKE
    int 0x80
    mov rdi, rax
    mov eax, 26             ; SYS_CAP_REVOKE
    int 0x80
%endif
%ifdef AGENT_OS_TEST_CAP_TRANSFER
    ; Transfer a receive-only endpoint capability into task 2.  Its table
    ; already has slots 0/1 occupied, so the returned destination handle is
    ; intentionally opaque to this task; task 2 observes it as slot 2.
    mov eax, 25             ; SYS_CAP_TRANSFER
    mov rdi, 0x0000000100000001
    mov rsi, 0x0000000100000002
    mov rdx, 0x20           ; RECV
    int 0x80
    mov eax, 16             ; SYS_IPC_CALL
    mov rdi, 0x0000000100000001
    mov esi, 0x43
    mov edx, 0xBEEF
    int 0x80
    ; Let task 2 consume the transferred descendant before this owner exits.
    ; Clean exit revokes the source lineage, so the fixture must model the
    ; receiver using the capability while its parent handle is still live.
    mov eax, 2              ; SYS_YIELD
    int 0x80
%endif
%ifdef AGENT_OS_TEST_WAIT_KILL
    ; Fixture IDs are generation 1, slots 0 and 1.  The kernel still checks
    ; the parent relationship before allowing these lifecycle syscalls.
    mov eax, 4              ; SYS_KILL
    mov rdi, 0x0000000100000002
    mov esi, 42
    int 0x80
    mov eax, 3              ; SYS_WAIT
    mov rdi, 0x0000000100000002
    int 0x80
%endif
%ifdef AGENT_OS_TEST_USER_KERNEL_READ
    ; The old user-enabled 2 MiB PDE allowed this read.  A supervisor leaf
    ; PTE must now raise #PF with P=1, W/R=0, U/S=1 (error code 5).
    mov rax, [abs 0x00100000]
%elifdef AGENT_OS_TEST_USER_KERNEL_WRITE
    mov qword [abs 0x00100000], 0
%elifdef AGENT_OS_TEST_USER_TEXT_WRITE
    mov byte [rel hello], 'X'
%elifdef AGENT_OS_TEST_NX_STACK
    mov byte [rsp - 8], 0xC3
    lea rax, [rsp - 8]
    call rax
%endif
    mov eax, 1              ; SYS_WRITE
    lea rdi, [rel hello]
    mov esi, hello_end - hello - 1
    int 0x80

    mov eax, 1              ; invalid user pointer must be rejected
    mov rdi, 0x0000000000200000
    mov esi, 4
    int 0x80

    mov eax, 0x7f            ; unknown syscall must be rejected
    xor edi, edi
    xor esi, esi
    int 0x80

    mov eax, 0                ; SYS_EXIT (terminal demo syscall)
    xor edi, edi
    int 0x80

.halt:
    hlt
    jmp .halt
%endif
%endif
%endif
%endif
%endif
%endif

%endif

%endif

hello db 'USER RING3 OK', 13, 10, 0
hello_end:

; A second deterministic Ring 3 fixture.  The kernel scheduler selects this
; process after the first process executes SYS_EXIT.
section .user_text
user_entry_secondary:
%ifdef AGENT_OS_TEST_RUNTIME_SERVICE
    sub rsp, 128
    mov eax, 17             ; consume START from the Runtime client
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xD8
    jne .runtime_service_child_fail
    cmp qword [rsp+24], 1
    jne .runtime_service_child_fail
    mov eax, 16             ; HEARTBEAT, same task/generation
    mov rdi, 0x0000000100000001
    mov esi, 0xD9
    mov edx, 1
    int 0x80
    test rax, rax
    jnz .runtime_service_child_fail
    mov eax, 2              ; let Runtime consume HEARTBEAT
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xDB
    jne .runtime_service_child_fail
    cmp qword [rsp+24], 1
    jne .runtime_service_child_fail
    mov eax, 2              ; wait for the checkpoint event
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xDA
    jne .runtime_service_child_fail
    cmp qword [rsp+24], 2
    jne .runtime_service_child_fail
    add rsp, 128
    mov eax, 1
    lea rdi, [rel runtime_service_heartbeat]
    mov esi, runtime_service_heartbeat_end-runtime_service_heartbeat-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.runtime_service_child_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel runtime_service_child_fail_message]
    mov esi, runtime_service_child_fail_message_end-runtime_service_child_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
runtime_service_heartbeat db 'AGENT RUNTIME HEARTBEAT ACK',13,10,0
runtime_service_heartbeat_end:
runtime_service_child_fail_message db 'AGENT RUNTIME CHILD ABI FAIL',13,10,0
runtime_service_child_fail_message_end:
%else
%ifdef AGENT_OS_TEST_GROUP_RECURSIVE
    mov eax, 1
    lea rdi, [rel recursive_group_child]
    mov esi, recursive_group_child_end-recursive_group_child-1
    int 0x80
.recursive_group_loop:
    mov eax, 2              ; stay READY until the parent terminates the tree
    int 0x80
    jmp .recursive_group_loop
recursive_group_child db 'RECURSIVE GROUP CHILD RUN',13,10,0
recursive_group_child_end:
%endif
%ifdef AGENT_OS_TEST_DYNAMIC_ENDPOINT
    ; Ring 3 asks the kernel-owned policy authority for one bounded endpoint
    ; slot, uses the returned generation-tagged capability, then destroys it.
    mov eax, 22             ; SYS_IPC_CREATE
    mov rdi, 0x0000000100000003 ; secondary policy-admin capability
    int 0x80
    test rax, rax
    js .dynamic_endpoint_fail
    mov r12, rax
    mov eax, 16             ; send one message through the new endpoint
    mov rdi, r12
    mov esi, 0xD2
    mov rdx, 0x44594E414D494301
    int 0x80
    test rax, rax
    jnz .dynamic_endpoint_fail
    sub rsp, 128
    mov eax, 17             ; receive through the same dynamic endpoint
    mov rdi, r12
    mov rsi, rsp
    int 0x80
    test rax, rax
    jnz .dynamic_endpoint_fail_stack
    cmp dword [rsp+8], 0xD2
    jne .dynamic_endpoint_fail_stack
    mov eax, 21             ; close leaves the capability valid but terminal
    mov rdi, r12
    int 0x80
    test rax, rax
    jnz .dynamic_endpoint_fail_stack
    mov eax, 19             ; closed empty endpoint must return immediately
    mov rdi, r12
    mov rsi, rsp
    int 0x80
    test rax, rax
    jz .dynamic_endpoint_fail_stack
    mov eax, 23             ; SYS_IPC_DESTROY retires the capability
    mov rdi, r12
    int 0x80
    test rax, rax
    jnz .dynamic_endpoint_fail_stack
    mov eax, 16             ; stale object/capability must fail closed
    mov rdi, r12
    mov esi, 0xD3
    mov edx, 1
    int 0x80
    test rax, rax
    jz .dynamic_endpoint_fail_stack
    add rsp, 128
    mov eax, 1
    lea rdi, [rel dynamic_endpoint_ok]
    mov esi, dynamic_endpoint_ok_end-dynamic_endpoint_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.dynamic_endpoint_fail_stack:
    add rsp, 128
.dynamic_endpoint_fail:
    mov eax, 1
    lea rdi, [rel dynamic_endpoint_fail_message]
    mov esi, dynamic_endpoint_fail_message_end-dynamic_endpoint_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
dynamic_endpoint_ok db 'DYNAMIC IPC ENDPOINT LIFECYCLE OK',13,10,0
dynamic_endpoint_ok_end:
dynamic_endpoint_fail_message db 'DYNAMIC IPC ENDPOINT LIFECYCLE FAIL',13,10,0
dynamic_endpoint_fail_message_end:
%else
%ifdef AGENT_OS_TEST_GROUP_FREEZE
    mov eax, 1
    lea rdi, [rel group_freeze_child]
    mov esi, group_freeze_child_end-group_freeze_child-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
group_freeze_child db 'GROUP FREEZE CHILD RUN',13,10,0
group_freeze_child_end:
%else
%ifdef AGENT_OS_TEST_IPC_CLOSE
    mov eax, 2
    int 0x80
    mov eax, 16             ; old capability must fail after endpoint close
    mov rdi, 0x0000000100000001
    mov esi, 0xD1
    mov edx, 1
    int 0x80
    test rax, rax
    jz .ipc_close_child_fail
    mov eax, 1
    lea rdi, [rel ipc_close_denied]
    mov esi, ipc_close_denied_end - ipc_close_denied - 1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.ipc_close_child_fail:
    mov eax, 1
    lea rdi, [rel ipc_close_fail_message]
    mov esi, ipc_close_fail_message_end - ipc_close_fail_message - 1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
ipc_close_denied db 'IPC CLOSE SEND DENIED', 13, 10, 0
ipc_close_denied_end:
%else
%ifdef AGENT_OS_TEST_TASK_GROUP
    mov eax, 1
    lea rdi, [rel task_group_child]
    mov esi, task_group_child_end - task_group_child - 1
    int 0x80
    ; Remain runnable until the parent group operation transitions us to a
    ; zombie; this marker proves the child was a real Ring 3 task.
    mov eax, 2
    int 0x80
    jmp .task_group_child_wait
.task_group_child_wait:
    mov eax, 2
    int 0x80
    jmp .task_group_child_wait
task_group_child db 'TASK GROUP CHILD RUN', 13, 10, 0
task_group_child_end:
%else
%ifdef AGENT_OS_TEST_SUPERVISOR_FAULT
    ; First invocation sends a heartbeat and faults.  The marker lives in the
    ; service's user stack page, so SYS_RESTART can re-enter this same image
    ; and take the clean second-generation path without kernel policy state.
    sub rsp, 128
    cmp dword [rsp+112], 0x4641554C
    je .g4_fault_service_restarted
    mov dword [rsp+112], 0x4641554C
    mov eax, 16             ; SYS_IPC_CALL: heartbeat generation 1
    mov rdi, 0x0000000100000001
    mov esi, 0xB0
    mov edx, 1
    int 0x80
    mov eax, 1
    lea rdi, [rel g4_fault_service_crashing]
    mov esi, g4_fault_service_crashing_end-g4_fault_service_crashing-1
    int 0x80
    mov rax, [abs 0x00300000] ; unmapped user page -> #PF
    jmp .g4_fault_service_halt
.g4_fault_service_restarted:
    mov eax, 16             ; SYS_IPC_CALL: heartbeat generation 2
    mov rdi, 0x0000000100000001
    mov esi, 0xB1
    mov edx, 2
    int 0x80
    mov eax, 1
    lea rdi, [rel g4_fault_service_restarted_message]
    mov esi, g4_fault_service_restarted_message_end-g4_fault_service_restarted_message-1
    int 0x80
    add rsp, 128
    mov eax, 0              ; SYS_EXIT
    xor edi, edi
    int 0x80
.g4_fault_service_halt:
    hlt
    jmp .g4_fault_service_halt
%else
%ifdef AGENT_OS_TEST_FAULT_RECOVERY
    mov eax, 1
    lea rdi, [rel fault_recovery_task2]
    mov esi, fault_recovery_task2_end-fault_recovery_task2-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
%else
%ifdef AGENT_OS_TEST_IPC_CANCEL
    sub rsp, 128
    mov eax, 19             ; child blocks until its parent cancels the wait
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp rax, -125
    jne .ipc_cancel_child_fail
    mov eax, 1
    lea rdi, [rel ipc_cancel_child_ok]
    mov esi, ipc_cancel_child_ok_end-ipc_cancel_child_ok-1
    int 0x80
    add rsp, 128
    mov eax, 0
    xor edi, edi
    int 0x80
.ipc_cancel_child_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel ipc_cancel_fail_message]
    mov esi, ipc_cancel_fail_message_end-ipc_cancel_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
%endif
%endif
%endif
%ifdef AGENT_OS_TEST_IPC_BLOCKING_MULTI
    sub rsp, 128
    mov eax, 19             ; second receiver also parks on the endpoint
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xC2
    jne .ipc_multi_receiver_fail
    mov rax, 0x4D554C5449545702
    cmp qword [rsp+32], rax
    jne .ipc_multi_receiver_fail
    mov eax, 1
    lea rdi, [rel ipc_multi_receiver_two]
    mov esi, ipc_multi_receiver_two_end-ipc_multi_receiver_two-1
    int 0x80
    add rsp, 128
    mov eax, 0
    xor edi, edi
    int 0x80
.ipc_multi_receiver_fail:
    add rsp, 128
    mov eax, 1
    lea rdi, [rel ipc_multi_receiver_fail_message]
    mov esi, ipc_multi_receiver_fail_message_end-ipc_multi_receiver_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
%endif
%ifdef AGENT_OS_TEST_IPC_BLOCKING
    mov eax, 16             ; SYS_IPC_CALL wakes the blocked receiver
    mov rdi, 0x0000000100000001
    mov esi, 0xC0
    mov rdx, 0x424C4F434B4F4B01
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
%endif
%ifdef AGENT_OS_TEST_G6_NATIVE_IO
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xE8
    jne .g6_native_io_service_fail
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xE9
    mov rdx, 0x4E45545F4F4B01
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xEA
    jne .g6_native_io_service_fail
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xEB
    mov rdx, 0x494E5055544F4B01
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g6_native_io_service_fail:
    mov eax, 1
    lea rdi, [rel g6_native_io_service_fail_message]
    mov esi, g6_native_io_service_fail_message_end-g6_native_io_service_fail_message-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
g6_native_io_service_fail_message db 'G6 NATIVE IO SERVICE FAIL',13,10,0
g6_native_io_service_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_G10_NATIVE
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xB0
    jne .g10_service_fail
    mov eax, 40
    mov rdi, 0x0000000100000003
    mov rsi, 0x0000000100000001
    mov rdx, 0xBEEF
    mov r8, 0x0000000100000002 ; service owns the token
    xor r10d, r10d
    int 0x80
    mov r12, rax
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xA1
    mov rdx, 0x5245474953545259 ; registry descriptor marker
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xB1
    jne .g10_service_fail
    mov rdi, r12
    mov rsi, 0x0000000100000001
    mov rdx, 0xBEEF
    mov eax, 41
    int 0x80
    test rax, rax
    jnz .g10_service_fail
    mov rax, 0x46494C455F4F4B01
    mov [rsp+112], rax
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xA2
    mov rdx, 0x46494C455F4F4B01
    int 0x80
    mov eax, 1
    lea rdi, [rel g10_prepare]
    mov esi, g10_prepare_end-g10_prepare-1
    int 0x80
    mov eax, 1
    lea rdi, [rel g10_commit]
    mov esi, g10_commit_end-g10_commit-1
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xB2
    jne .g10_service_fail
    mov rdi, r12
    mov rsi, 0x0000000100000001
    mov rdx, 0xBEEF
    mov eax, 41
    int 0x80
    test rax, rax
    jz .g10_service_fail
    mov rax, 0x46494C455F4F4B01
    cmp qword [rsp+112], rax
    jne .g10_service_fail
    mov eax, 1
    lea rdi, [rel g10_replay]
    mov esi, g10_replay_end-g10_replay-1
    int 0x80
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xA3
    xor edx, edx
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp+8], 0xB3
    jne .g10_service_fail
    mov eax, 40
    mov rdi, 0x0000000100000003
    mov rsi, 0x0000000100000001
    mov rdx, 0xCAFE
    mov r8, 0x0000000100000002
    xor r10d, r10d
    int 0x80
    mov r13, rax
    mov rdi, r13
    mov rsi, 0x0000000100000001
    mov rdx, 0xCAFE
    mov eax, 41
    int 0x80
    test rax, rax
    jnz .g10_service_fail
    mov r14, [rsp+112]
    mov rax, 0x524F4C4C4241434B
    mov [rsp+112], rax
    ; Fixed postcondition failure: restore the preimage before reporting.
    mov [rsp+112], r14
    mov eax, 1
    lea rdi, [rel g10_rollback]
    mov esi, g10_rollback_end-g10_rollback-1
    int 0x80
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xA4
    mov rdx, r14
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g10_service_fail:
    mov eax, 1
    lea rdi, [rel g10_service_fail_message]
    mov esi, g10_service_fail_message_end-g10_service_fail_message-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
g10_prepare db 'JOURNAL PREPARE',13,10,0
g10_prepare_end:
g10_commit db 'JOURNAL COMMIT',13,10,0
g10_commit_end:
g10_replay db 'JOURNAL REPLAY_REJECTED',13,10,0
g10_replay_end:
g10_rollback db 'JOURNAL ROLLBACK',13,10,0
g10_rollback_end:
g10_service_fail_message db 'G10 SERVICE FAIL',13,10,0
g10_service_fail_message_end:
%endif
%ifdef AGENT_OS_TEST_FILE_WRITE
    ; Ring 3 file service owns the fixed in-memory backend and policy flow.
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0xE0
    jne .file_service_fail
    add rsp, 128
    mov eax, 40                 ; service signs one endpoint+nonce token
    mov rdi, 0x0000000100000003
    mov rsi, 0x0000000100000001
    mov rdx, 0xF17E
    xor r8d, r8d              ; service owns and consumes token
    int 0x80
    mov r12, rax
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xE1
    mov rdx, r12
    int 0x80
    mov eax, 2
    int 0x80
    ; Authorized write.
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0xE2
    jne .file_service_fail
    mov rdi, r12
    mov rsi, 0x0000000100000001
    mov rdx, 0xF17E
    mov eax, 41
    int 0x80
    test rax, rax
    jnz .file_service_fail
    mov rax, 0x46494C455F444154
    mov [rsp + 64], rax
    add rsp, 128
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xE3
    mov rdx, 0x57495254454F4B01
    int 0x80
    mov eax, 2
    int 0x80
    ; Replay is consumed atomically as a denial; backend is untouched.
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    mov rdi, r12
    mov rsi, 0x0000000100000001
    mov rdx, 0xF17E
    mov eax, 41
    int 0x80
    test rax, rax
    jz .file_service_fail
    mov rax, 0x46494C455F444154
    cmp [rsp + 112], rax
    jne .file_service_fail
    add rsp, 128
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xE4
    mov rdx, 1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.file_service_fail:
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xEF
    xor edx, edx
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
%endif
%ifdef AGENT_OS_TEST_G6_NATIVE
    ; Ring 3 file service.  It consumes the request and produces a bounded
    ; response; no kernel-side file semantics are involved in this fixture.
    sub rsp, 128
    mov eax, 17             ; SYS_IPC_RECV / FILE_READ
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0xC0
    jne .g6_service_fail
    mov eax, 16             ; SYS_IPC_CALL / FILE_DATA
    mov rdi, 0x0000000100000001
    mov esi, 0xC1
    mov rdx, 0x46494C452D4F4B01
    int 0x80
    ; The syscall fixture carries the inline word unchanged; the service
    ; result marker is checked by the client from the response envelope.
    mov eax, 0
    xor edi, edi
    int 0x80
.g6_service_fail:
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xC1
    xor edx, edx
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
%endif
%ifdef AGENT_OS_TEST_G8_NATIVE
    ; Ring 3 Policy/Registry service.  It denies unknown descriptors and
    ; returns the deterministic metadata/digests bound to this task window.
%ifdef AGENT_OS_TEST_G8_NATIVE_NEGATIVE
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x7f
    jne .g8_service_negative_fail
    add rsp, 128
    mov eax, 16
    mov esi, 0x9f
    xor edx, edx
    int 0x80
    sub rsp, 128
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x82
    jne .g8_service_negative_fail
    mov rax, 0xbadac71000000001
    cmp qword [rsp + 32], rax
    jne .g8_service_negative_fail
    add rsp, 128
    mov eax, 16
    mov esi, 0x9f
    xor edx, edx
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g8_service_negative_fail:
    mov eax, 0
    mov edi, 1
    int 0x80
%else
    sub rsp, 128
    mov eax, 17             ; unknown tool query
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x80
    jne .g8_service_fail
    mov rax, 0xdeadbeef
    cmp qword [rsp + 32], rax
    jne .g8_service_fail
    add rsp, 128
    mov eax, 16
    mov esi, 0x9f
    xor edx, edx
    int 0x80
    sub rsp, 128
    mov eax, 17             ; valid registry descriptor
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x80
    jne .g8_service_fail
    mov rax, 0x7368656c6c2e7772
    cmp qword [rsp + 32], rax
    jne .g8_service_fail
    add rsp, 128
    sub rsp, 128            ; current task window request
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x81
    jne .g8_service_fail
    mov rax, 0x5441534b2d303031
    cmp qword [rsp + 32], rax
    jne .g8_service_fail
    add rsp, 128
    sub rsp, 128            ; action binding request
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x82
    jne .g8_service_fail
    mov rax, 0x0a11ce0000000001
    cmp qword [rsp + 32], rax
    jne .g8_service_fail
    add rsp, 128
    sub rsp, 128            ; Trusted Input request
    mov eax, 17
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    cmp dword [rsp + 8], 0x83
    jne .g8_service_fail
    mov rax, 0x5452555354454401
    cmp qword [rsp + 32], rax
    jne .g8_service_fail
    add rsp, 128
    mov eax, 16
    mov esi, 0x90
    mov rdx, 0x0100000000000001
    int 0x80
    mov eax, 16             ; Context Collector result
    mov esi, 0x91
    mov rdx, 0x0c0ffee000000001
    int 0x80
    mov eax, 16             ; action binding result
    mov esi, 0x92
    mov rdx, 0x0a11ce0000000001
    int 0x80
    mov eax, 16             ; Trusted Input admission
    mov esi, 0x93
    mov rdx, 0x5452555354454401
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.g8_service_fail:
    add rsp, 128
    mov eax, 16             ; fail closed with a deny reply
    mov rdi, 0x0000000100000001
    mov esi, 0x9f
    xor edx, edx
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
%endif
%endif
%ifdef AGENT_OS_TEST_SUPERVISOR
    ; The service emits a bounded inline heartbeat and exits.  Restarting the
    ; same process frame executes this exact path a second time, proving that
    ; the Supervisor owns lifecycle policy while Ring 0 only enforces it.
%ifdef AGENT_OS_TEST_SUPERVISOR_READY_GATE
    sub rsp, 128
    mov eax, 1
    lea rdi, [rel service_waiting]
    mov esi, service_waiting_end - service_waiting - 1
    int 0x80
.g4_supervisor_gate_wait:
    mov eax, 17
    test rbx, rbx
    jnz .g4_supervisor_gate_cap
    mov rdi, 0x0000000100000001
    jmp .g4_supervisor_gate_recv
.g4_supervisor_gate_cap:
    mov rdi, rbx
.g4_supervisor_gate_recv:
    mov rsi, rsp
    int 0x80
    test rax, rax
    jz .g4_supervisor_gate_ready
    mov eax, 2
    int 0x80
    jmp .g4_supervisor_gate_wait
.g4_supervisor_gate_ready:
    mov eax, 1
    lea rdi, [rel service_dependency_ready]
    mov esi, service_dependency_ready_end - service_dependency_ready - 1
    int 0x80
    add rsp, 128
%endif
    mov eax, 16             ; SYS_IPC_CALL
    test rbx, rbx
    jz .g4_supervisor_initial_cap
    mov rdi, rbx
    jmp .g4_supervisor_send
.g4_supervisor_initial_cap:
    mov rdi, 0x0000000100000001
.g4_supervisor_send:
    mov esi, 0xB0           ; supervisor heartbeat opcode
    mov edx, 0x00000001     ; service generation marker in this fixture
    int 0x80
    mov eax, 1              ; SYS_WRITE
    lea rdi, [rel service_heartbeat]
    mov esi, service_heartbeat_end - service_heartbeat - 1
    int 0x80
    mov eax, 0              ; SYS_EXIT
    xor edi, edi
    int 0x80
%else
%ifdef AGENT_OS_TEST_AGENT_LOOP
    ; Policy service receives the action, mints a token with its admin cap,
    ; and returns the opaque token through the same bounded endpoint.
    sub rsp, 128
    mov eax, 17             ; SYS_IPC_RECV
    mov rdi, 0x0000000100000001
    mov rsi, rsp
    int 0x80
    add rsp, 128
    mov eax, 40             ; SYS_POLICY_TOKEN_MINT
    mov rdi, 0x0000000100000003 ; policy authority
    mov rsi, 0x0000000100000001 ; bound capability
    mov rdx, 0xABCD
    mov r8, 0x0000000100000001 ; target Agent owner
    xor r10d, r10d
    int 0x80
    mov rdx, rax            ; preserve the opaque generation-tagged handle
    mov eax, 16             ; SYS_IPC_CALL / ACTION_TOKEN
    mov rdi, 0x0000000100000001
    mov esi, 0x71
    int 0x80
    mov eax, 0              ; SYS_EXIT
    xor edi, edi
    int 0x80
%endif
%ifdef AGENT_OS_TEST_POLICY_TOKEN
    ; The policy fixture owns admin slot 2 and mints a token bound to the
    ; endpoint capability plus an action nonce.  A forged nonce must fail
    ; without consuming the token; the real consume then succeeds, and the
    ; replay must fail because consumption is atomic and generation-tagged.
    mov eax, 40             ; SYS_POLICY_TOKEN_MINT
    mov rdi, 0x0000000100000003 ; policy authority admin capability
    mov rsi, 0x0000000100000001 ; bound endpoint capability
    mov rdx, 0x1234         ; action nonce
    xor r10d, r10d          ; no expiry in this fixture
    int 0x80
    mov rbx, rax
    mov eax, 41             ; forged action nonce must fail closed
    mov rdi, rbx
    mov rsi, 0x0000000100000001
    mov rdx, 0x9999
    int 0x80
    mov eax, 41             ; SYS_POLICY_TOKEN_CONSUME
    mov rdi, rbx
    mov rsi, 0x0000000100000001
    mov rdx, 0x1234
    int 0x80
    mov eax, 41             ; replay must fail closed
    mov rdi, rbx
    mov rsi, 0x0000000100000001
    mov rdx, 0x1234
    int 0x80
%endif
%ifdef AGENT_OS_TEST_POLICY_PAUSE
    ; Emergency pause is a kernel-enforced Policy Firewall gate.  The policy
    ; service mints one action token, pauses the gate, observes a fail-closed
    ; consume, resumes, then consumes successfully.  A second token with an
    ; expiry at logical tick 1 proves expiry is checked in Ring 0 as well.
    mov eax, 40             ; SYS_POLICY_TOKEN_MINT
    mov rdi, 0x0000000100000003 ; policy authority admin capability
    mov rsi, 0x0000000100000001 ; bound endpoint capability
    mov rdx, 0x2222         ; action digest/nonce
    xor r10d, r10d          ; no expiry for paused token
    int 0x80
    mov rbx, rax
    mov eax, 43             ; SYS_POLICY_PAUSE
    mov rdi, 0x0000000100000003
    int 0x80
    mov eax, 41             ; paused consume must fail closed
    mov rdi, rbx
    mov rsi, 0x0000000100000001
    mov rdx, 0x2222
    int 0x80
    mov eax, 44             ; SYS_POLICY_RESUME
    mov rdi, 0x0000000100000003
    int 0x80
    mov eax, 41             ; resumed consume succeeds
    mov rdi, rbx
    mov rsi, 0x0000000100000001
    mov rdx, 0x2222
    int 0x80
    mov eax, 40             ; mint an expiring action token
    mov rdi, 0x0000000100000003
    mov rsi, 0x0000000100000001
    mov rdx, 0x3333
    mov r10d, 1             ; next consume advances logical clock to 2
    int 0x80
    mov rbx, rax
    mov eax, 41             ; expired token must fail closed
    mov rdi, rbx
    mov rsi, 0x0000000100000001
    mov rdx, 0x3333
    int 0x80
%endif
%ifdef AGENT_OS_TEST_CAP_TRANSFER
    sub rsp, 128
    mov eax, 17             ; SYS_IPC_RECV
    mov rdi, 0x0000000100000004 ; transferred slot 3 (policy slot 2 reserved)
    mov rsi, rsp
    int 0x80
    add rsp, 128
%endif
%ifdef AGENT_OS_TEST_SHM
    mov eax, 33             ; SYS_SHM_MAP
    mov rdi, 0x0000000100000002
    mov rsi, 0x00500000
    int 0x80
    mov eax, 1              ; SYS_WRITE
    mov rdi, 0x00500000
    mov esi, 10
    int 0x80
%endif
    mov eax, 1              ; SYS_WRITE
    lea rdi, [rel hello_secondary]
    mov esi, hello_secondary_end - hello_secondary - 1
    int 0x80
    mov eax, 0              ; SYS_EXIT
    xor edi, edi
    int 0x80
.halt_secondary:
    hlt
    jmp .halt_secondary
%endif
%endif
%endif

hello_secondary db 'USER RING3 TASK2 OK', 13, 10, 0
hello_secondary_end:
fault_recovery_task2 db 'FAULT RECOVERY TASK2 OK', 13, 10, 0
fault_recovery_task2_end:
service_heartbeat db 'SERVICE HEARTBEAT SENT', 13, 10, 0
service_heartbeat_end:
service_waiting db 'SERVICE WAITING DEPENDENCY', 13, 10, 0
service_waiting_end:
service_dependency_ready db 'SERVICE DEPENDENCY READY', 13, 10, 0
service_dependency_ready_end:
agent_action_committed db 'AGENT RING3 ACTION COMMITTED', 13, 10, 0
agent_action_committed_end:
g4_fault_supervisor_start db 'SUPERVISOR FAULT RING3 START', 13, 10, 0
g4_fault_supervisor_start_end:
g4_fault_supervisor_heartbeat db 'SUPERVISOR FAULT HEARTBEAT 1', 13, 10, 0
g4_fault_supervisor_heartbeat_end:
g4_fault_supervisor_restart db 'SUPERVISOR FAULT RESTART HEARTBEAT 2', 13, 10, 0
g4_fault_supervisor_restart_end:
g4_fault_supervisor_reaped db 'SUPERVISOR FAULT SERVICE REAPED', 13, 10, 0
g4_fault_supervisor_reaped_end:
g4_fault_supervisor_fail_message db 'G4 SUPERVISOR FAULT CHAIN FAIL', 13, 10, 0
g4_fault_supervisor_fail_message_end:
g4_fault_service_crashing db 'SERVICE #PF CRASH INJECTED', 13, 10, 0
g4_fault_service_crashing_end:
g4_fault_service_restarted_message db 'SERVICE RESTARTED AFTER #PF', 13, 10, 0
g4_fault_service_restarted_message_end:
ipc_cancel_parent_ok db 'IPC CANCEL PARENT REAP OK', 13, 10, 0
ipc_cancel_parent_ok_end:
ipc_cancel_child_ok db 'IPC CANCEL CHILD OK', 13, 10, 0
ipc_cancel_child_ok_end:
ipc_cancel_fail_message db 'IPC CANCEL FAIL', 13, 10, 0
ipc_cancel_fail_message_end:

%ifdef AGENT_OS_TEST_IPC_BLOCKING_MULTI
global user_entry_tertiary
user_entry_tertiary:
    mov eax, 2              ; allow receiver one to join the existing waiter
    int 0x80
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xC2
    mov rdx, 0x4D554C5449545702
    int 0x80
    test rax, rax
    jnz .ipc_multi_sender_fail
    mov eax, 2              ; let the first-arriving receiver verify its buffer
    int 0x80
    mov eax, 16
    mov rdi, 0x0000000100000001
    mov esi, 0xC1
    mov rdx, 0x4D554C54494F4E01
    int 0x80
    test rax, rax
    jnz .ipc_multi_sender_fail
    mov eax, 1
    lea rdi, [rel ipc_multi_sender_ok]
    mov esi, ipc_multi_sender_ok_end-ipc_multi_sender_ok-1
    int 0x80
    mov eax, 0
    xor edi, edi
    int 0x80
.ipc_multi_sender_fail:
    mov eax, 1
    lea rdi, [rel ipc_multi_receiver_fail_message]
    mov esi, ipc_multi_receiver_fail_message_end-ipc_multi_receiver_fail_message-1
    int 0x80
    mov eax, 0
    mov edi, 1
    int 0x80
ipc_multi_receiver_one db 'IPC MULTI RECEIVER 1 OK',13,10,0
ipc_multi_receiver_one_end:
ipc_multi_receiver_two db 'IPC MULTI RECEIVER 2 OK',13,10,0
ipc_multi_receiver_two_end:
ipc_multi_sender_ok db 'IPC MULTI SENDER OK',13,10,0
ipc_multi_sender_ok_end:
ipc_multi_receiver_fail_message db 'IPC MULTI FAIL',13,10,0
ipc_multi_receiver_fail_message_end:
%endif
%endif
%endif
%endif

%ifdef AGENT_OS_TEST_GROUP_RECURSIVE
global user_entry_tertiary
user_entry_tertiary:
    mov eax, 1
    lea rdi, [rel recursive_group_grandchild]
    mov esi, recursive_group_grandchild_end-recursive_group_grandchild-1
    int 0x80
.recursive_group_grandchild_loop:
    mov eax, 2
    int 0x80
    jmp .recursive_group_grandchild_loop
recursive_group_grandchild db 'RECURSIVE GROUP GRANDCHILD RUN',13,10,0
recursive_group_grandchild_end:
%endif

section .user_data
align 8
file_backend dq 0
align 512
g7_block_write_data:
times 512 db 0xA5


section .note.GNU-stack noalloc noexec nowrite progbits
