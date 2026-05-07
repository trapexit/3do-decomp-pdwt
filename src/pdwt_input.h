#pragma once

#include "3do_types.h"
#include "event.h"

#define PDWT_INPUT_COMPLETE 0
#define PDWT_INPUT_STOP 1

extern ControlPadEventData controlPadPrevious;
extern int32 controlPadLastError;

uint32
GetJoypad(void);
int32
WaitForJoypad(Item   vblIOReq_,
              int32  fields_,
              uint32 stopMask_);
int32
WaitForTaskOrJoypad(Item   taskItem_,
                    Item   vblIOReq_,
                    uint32 stopMask_);
Boolean
PDWTTaskIsRunning(Item taskItem_);
