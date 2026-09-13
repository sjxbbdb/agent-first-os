#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "capability_table.h"

static void lookup_ok(const AgentOsCapabilityTable *table,
                      CapabilityHandle handle,
                      uint64_t object,
                      uint64_t rights) {
    uint64_t got_object = 0;
    uint64_t got_rights = 0;
    assert(agent_os_capability_lookup(table, handle, 0, &got_object,
                                      &got_rights) == AGENT_OS_OK);
    assert(got_object == object && got_rights == rights);
}

static void lookup_revoked(const AgentOsCapabilityTable *table,
                           CapabilityHandle handle) {
    uint64_t object = 0;
    assert(agent_os_capability_lookup(table, handle, 0, &object, 0) ==
           AGENT_OS_E_REVOKED);
}

int main(void) {
    const uint64_t root_rights = CAP_RIGHT_READ | CAP_RIGHT_TRANSFER |
                                 CAP_RIGHT_REVOKE;
    const uint64_t child_rights = CAP_RIGHT_READ | CAP_RIGHT_TRANSFER |
                                  CAP_RIGHT_REVOKE;
    AgentOsCapabilityTable root;
    AgentOsCapabilityTable branch;
    AgentOsCapabilityTable grandchild;
    AgentOsCapabilityTable sibling;
    AgentOsCapabilityTable independent;
    CapabilityHandle root_handle;
    CapabilityHandle branch_handle;
    CapabilityHandle grandchild_handle;
    CapabilityHandle sibling_handle;
    CapabilityHandle independent_handle;

    agent_os_capability_table_init(&root);
    agent_os_capability_table_init(&branch);
    agent_os_capability_table_init(&grandchild);
    agent_os_capability_table_init(&sibling);
    agent_os_capability_table_init(&independent);
    assert(agent_os_capability_mint(&root, 0x1234, root_rights,
                                    &root_handle) == AGENT_OS_OK);
    assert(agent_os_capability_restrict(&root, root_handle, child_rights,
                                        &sibling_handle) == AGENT_OS_OK);
    assert(agent_os_capability_transfer(&root, root_handle, &branch,
                                        child_rights, &branch_handle) ==
           AGENT_OS_OK);
    assert(agent_os_capability_transfer(&branch, branch_handle, &grandchild,
                                        child_rights, &grandchild_handle) ==
           AGENT_OS_OK);
    assert(agent_os_capability_mint(&independent, 0x1234, CAP_RIGHT_READ,
                                    &independent_handle) == AGENT_OS_OK);

    /* Derived rights cannot exceed the source rights. */
    assert(agent_os_capability_restrict(&root, root_handle, CAP_RIGHT_WRITE,
                                        &independent_handle) ==
           AGENT_OS_E_DENIED);
    assert(agent_os_capability_transfer(&root, root_handle, &branch,
                                        CAP_RIGHT_WRITE, &independent_handle) ==
           AGENT_OS_E_DENIED);

    /* Revoking one branch leaves its sibling alive. */
    assert(agent_os_capability_revoke(&branch, branch_handle) == AGENT_OS_OK);
    lookup_revoked(&branch, branch_handle);
    lookup_revoked(&grandchild, grandchild_handle);
    lookup_ok(&root, root_handle, 0x1234, root_rights);
    lookup_ok(&root, sibling_handle, 0x1234, child_rights);

    /* Root revocation invalidates every remaining descendant. */
    assert(agent_os_capability_revoke(&root, root_handle) == AGENT_OS_OK);
    lookup_revoked(&root, root_handle);
    lookup_revoked(&root, sibling_handle);
    /* Independent mint of the same object is in its own namespace. */
    lookup_ok(&independent, independent_handle, 0x1234, CAP_RIGHT_READ);

    /* Reusing the root slot cannot resurrect the old branch. */
    CapabilityHandle replacement;
    assert(agent_os_capability_mint(&root, 0x9999, CAP_RIGHT_READ,
                                    &replacement) == AGENT_OS_OK);
    assert(replacement != root_handle);
    lookup_revoked(&grandchild, grandchild_handle);
    lookup_ok(&root, replacement, 0x9999, CAP_RIGHT_READ);

    /* Reinitializing a table retires old descendants through its incarnation. */
    AgentOsCapabilityTable reinit_root;
    AgentOsCapabilityTable reinit_child;
    AgentOsCapabilityTable reinit_grandchild;
    CapabilityHandle reinit_root_handle;
    CapabilityHandle reinit_child_handle;
    CapabilityHandle reinit_grandchild_handle;
    agent_os_capability_table_init(&reinit_root);
    agent_os_capability_table_init(&reinit_child);
    agent_os_capability_table_init(&reinit_grandchild);
    assert(agent_os_capability_mint(&reinit_root, 7, root_rights,
                                    &reinit_root_handle) == AGENT_OS_OK);
    assert(agent_os_capability_transfer(&reinit_root, reinit_root_handle,
                                        &reinit_child, child_rights,
                                        &reinit_child_handle) == AGENT_OS_OK);
    assert(agent_os_capability_transfer(&reinit_child, reinit_child_handle,
                                        &reinit_grandchild, child_rights,
                                        &reinit_grandchild_handle) ==
           AGENT_OS_OK);
    agent_os_capability_table_init(&reinit_child);
    lookup_revoked(&reinit_grandchild, reinit_grandchild_handle);

    /* UINT32_MAX is a permanently retired generation. */
    AgentOsCapabilityTable retired;
    CapabilityHandle max_handle;
    agent_os_capability_table_init(&retired);
    retired.entries[0].generation = UINT32_MAX;
    assert(agent_os_capability_mint(&retired, 8, CAP_RIGHT_READ | CAP_RIGHT_REVOKE,
                                    &max_handle) == AGENT_OS_OK);
    assert(agent_os_capability_revoke(&retired, max_handle) == AGENT_OS_OK);
    assert(agent_os_capability_mint(&retired, 9, CAP_RIGHT_READ,
                                    &replacement) == AGENT_OS_OK);
    assert((uint32_t)replacement != 1u);

    /* A chain is bounded at 64 derivations. */
    AgentOsCapabilityTable chain[AGENT_OS_CAPABILITY_MAX_DEPTH + 2u];
    CapabilityHandle chain_handle;
    for (uint32_t index = 0; index < AGENT_OS_CAPABILITY_MAX_DEPTH + 2u;
         ++index) {
        agent_os_capability_table_init(&chain[index]);
    }
    assert(agent_os_capability_mint(&chain[0], 0x55, root_rights,
                                    &chain_handle) == AGENT_OS_OK);
    for (uint32_t index = 1; index <= AGENT_OS_CAPABILITY_MAX_DEPTH; ++index) {
        assert(agent_os_capability_transfer(
                   &chain[index - 1u], chain_handle, &chain[index],
                   child_rights, &chain_handle) == AGENT_OS_OK);
    }
    assert(agent_os_capability_transfer(
               &chain[AGENT_OS_CAPABILITY_MAX_DEPTH], chain_handle,
               &chain[AGENT_OS_CAPABILITY_MAX_DEPTH + 1u], child_rights,
               &chain_handle) == AGENT_OS_E_INVAL);

    puts("G3 capability lineage contracts OK");
    return 0;
}
