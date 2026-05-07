// JoyPad.c - controller input and task/input wait helpers.
#include "pdwt_input.h"

#include "3do_types.h"

#include "event.h"
#include "item.h"
#include "kernelnodes.h"

#define BUTTON_REPEAT_MASK 0x01000000UL
#define BUTTON_CONTINUOUS_MASK 0x003f0000UL
#define BUTTON_UPDOWN_MASK 0x0000ff00UL
#define BUTTON_PRESSED_MASK 0x000000ffUL

ControlPadEventData controlPadPrevious;
int32 controlPadLastError;


static
uint32
_filter_repeating_buttons(uint32 buttons_,
                          uint32 previousButtons_);
static
uint32
_filter_repeating_buttons(uint32 buttons_,
                          uint32 previousButtons_)
{
  if((previousButtons_ ^ buttons_) == 0)
    {
      uint32 nonRepeatButtons =
        buttons_ & ~BUTTON_REPEAT_MASK & ~BUTTON_CONTINUOUS_MASK & ~BUTTON_UPDOWN_MASK &
        ~BUTTON_PRESSED_MASK;
      if(nonRepeatButtons != 0)
        {
          return 0;
        }
    }
  return buttons_;
}


uint32
GetJoypad(void)
{
  ControlPadEventData data;
  int32 padResult;
  uint32 rawButtons;
  uint32 buttons;

  data.cped_ButtonBits = 0;
  padResult            = GetControlPad(1, 0, &data);
  if(padResult < 0)
    {
      controlPadLastError = padResult;
      return 0;
    }
  controlPadLastError = 0;
  rawButtons          = data.cped_ButtonBits;
  buttons             = _filter_repeating_buttons(rawButtons, controlPadPrevious.cped_ButtonBits);

  controlPadPrevious.cped_ButtonBits = data.cped_ButtonBits;
  return buttons;
}


int32
WaitForJoypad(Item   vblIOReq_,
              int32  fields_,
              uint32 stopMask_)
{
  int32 field;
  int32 waitResult;
  uint32 buttons;

  if(vblIOReq_ <= 0 || fields_ < 0)
    {
      return -1;
    }

  for(field = 0; field < fields_; field++)
    {
      waitResult = WaitVBL(vblIOReq_, 1);
      if(waitResult < 0)
        {
          return waitResult;
        }

      buttons = GetJoypad();
      if(controlPadLastError < 0)
        {
          return controlPadLastError;
        }
      if((buttons & stopMask_) != 0)
        {
          return PDWT_INPUT_STOP;
        }
    }

  return PDWT_INPUT_COMPLETE;
}


Boolean
PDWTTaskIsRunning(Item taskItem_)
{
  if(taskItem_ <= 0)
    {
      return 0;
    }
  return CheckItem(taskItem_, KERNELNODE, TASKNODE) != 0;
}


int32
WaitForTaskOrJoypad(Item   taskItem_,
                    Item   vblIOReq_,
                    uint32 stopMask_)
{
  int32 result;

  if(taskItem_ <= 0 || vblIOReq_ <= 0)
    {
      return -1;
    }

  while(PDWTTaskIsRunning(taskItem_))
    {
      result = WaitForJoypad(vblIOReq_, 1, stopMask_);
      if(result != PDWT_INPUT_COMPLETE)
        {
          return result;
        }
    }

  return PDWT_INPUT_COMPLETE;
}
