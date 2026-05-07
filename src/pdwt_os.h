#pragma once

#include "mem.h"

// Route recovered AllocMem/FreeMem call sites through LinkSupport.c.

void *
PDWTAllocMem(int32  size,
             uint32 memType);
void
PDWTFreeMem(void *memBlock_,
            int32 size_);

#ifdef AllocMem
  #undef AllocMem
#endif
#ifdef FreeMem
  #undef FreeMem
#endif

#define AllocMem(size, memType) PDWTAllocMem((int32)(size), (uint32)(memType))
#define FreeMem(memBlock, size) PDWTFreeMem((void *)(memBlock), (int32)(size))
