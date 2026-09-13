#ifndef AGENT_OS_BOOT_INFO_H
#define AGENT_OS_BOOT_INFO_H

#include "boot_info_v2.h"

/* Deprecated source aliases: all loaders now emit the v2 wire contract. */
#define AGENT_OS_BOOT_INFO_MAGIC AGENT_OS_BOOT_INFO_MAGIC_V2
#define AGENT_OS_BOOT_INFO_VERSION AGENT_OS_BOOT_INFO_VERSION_V2
#define AGENT_OS_HIGH_HALF_BASE UINT64_C(0xFFFFFFFF80000000)
typedef AgentOsE820EntryV2 E820Entry;
typedef AgentOsBootInfoV2 BootInfo;

#endif
