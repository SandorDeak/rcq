#include "foundation.h"


const VkAllocationCallbacks* rcq::host_memory_manager = nullptr;

const size_t POOL_MAT_STATIC_SIZE = 64;
const size_t POOL_MAT_DYNAMIC_SIZE = 64;
const size_t POOL_TR_STATIC_SIZE = 64;
const size_t POOL_TR_DYNAMIC_SIZE = 64;

const size_t DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE = 4;