// aether_sandbox.c — Global sandbox state
#include "aether_sandbox.h"

#ifdef AETHER_HAS_SANDBOX
// Default: no sandbox checker installed = everything allowed
aether_sandbox_check_fn _aether_sandbox_checker = 0;
#endif
