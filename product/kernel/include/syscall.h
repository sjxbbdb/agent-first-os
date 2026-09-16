#ifndef AGENT_OS_SYSCALL_H
#define AGENT_OS_SYSCALL_H

#include <stdint.h>
#include "abi.h"

/* x86_64 v1: int 0x80; rax=number/result, rdi/rsi/rdx/r10/r8/r9=args.
 * Negative signed return values are errno-style failures. Numbers without a
 * kernel adapter return -ENOSYS until their gate has a native implementation. */
enum AgentOsSyscall {
    SYS_EXIT = 0,
    SYS_WRITE = 1,
    SYS_YIELD = 2,
    SYS_WAIT = 3,
    SYS_KILL = 4,
    /* Restart a parent-owned zombie service fixture without granting Ring 3
     * arbitrary process creation.  The kernel checks the parent relation and
     * restores the recorded entry frame before making it READY. */
    SYS_RESTART = 5,
    /* Terminate the caller's direct children in the supplied task group. */
    SYS_GROUP_TERMINATE = 6,
    SYS_GROUP_FREEZE = 7,
    SYS_GROUP_RESUME = 8,
    SYS_GROUP_TERMINATE_TREE = 9,
    SYS_IPC_CALL = 16,
    SYS_IPC_RECV = 17,
    SYS_IPC_REPLY = 18,
    /* Block the current task until one message is available.  The kernel
     * resumes it with the message copied into the supplied user buffer and
     * RAX=0; SYS_IPC_RECV remains non-blocking. */
    SYS_IPC_RECV_WAIT = 19,
    /* Parent/Supervisor cancellation of a child blocked in IPC. */
    SYS_IPC_CANCEL = 20,
    /* Close a capability-bound endpoint; subsequent use of old handles fails. */
    SYS_IPC_CLOSE = 21,
    /* Create/destroy a Ring-0-owned dynamic endpoint.  Create requires the
     * kernel-provisioned policy-admin capability; destroy requires endpoint
     * revoke rights and retires the endpoint capability generation. */
    SYS_IPC_CREATE = 22,
    SYS_IPC_DESTROY = 23,
    SYS_CAP_RESTRICT = 24,
    SYS_CAP_TRANSFER = 25,
    SYS_CAP_REVOKE = 26,
    SYS_SHM_CREATE = 32,
    SYS_SHM_MAP = 33,
    SYS_SHM_UNMAP = 34,
    SYS_POLICY_TOKEN_MINT = 40,
    SYS_POLICY_TOKEN_CONSUME = 41,
    SYS_POLICY_TOKEN_REVOKE = 42,
    /* Ring 3 Policy Firewall emergency gate.  These operations require the
     * kernel-owned policy authority capability; pause is fail-closed for
     * token mint/consume while revoke remains available for cleanup. */
    SYS_POLICY_PAUSE = 43,
    SYS_POLICY_RESUME = 44,
    /* Versioned Ring 3 adapter for one 512-byte virtio-block write.
     * RDI=device capability, RSI=sector (sector 0 is rejected),
     * RDX=user write buffer, R10=exact byte length, R8=ABI version. */
    SYS_VIRTIO_BLOCK_WRITE = 45,
    /* Test-gated versioned virtio-block flush.  RDI is the opaque device
     * capability and R8 is the ABI version; success returns 0. */
    SYS_VIRTIO_BLOCK_FLUSH = 46,
};

#define AGENT_OS_VIRTIO_BLOCK_WRITE_ABI_VERSION UINT64_C(1)
#define AGENT_OS_VIRTIO_BLOCK_WRITE_BYTES UINT64_C(512)
#define AGENT_OS_VIRTIO_BLOCK_FLUSH_ABI_VERSION UINT64_C(1)

#endif
