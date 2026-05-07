#pragma once

#include "3do_types.h"
#include "pdwt_decision_audio.h"
#include "pdwt_timing.h"

enum
{
  PDWT_SCENE_CONTROL_CONTINUE = 0,
  PDWT_SCENE_CONTROL_SKIP = 2,
  PDWT_SCENE_CONTROL_STEP_BACK = 3
};

typedef struct PDWTSceneControl
{
  ScreenContext  *screenContext;
  DecisionAudio  *audio;
  PDWTSceneClock *clock;
  Item vramIOReq;
  Item vblIOReq;
  uint32 previousButtons;
  int32 displayedScreen;
  int32 paused;
  int32 active;
}
PDWTSceneControl;

void
PDWTSceneControlInit(PDWTSceneControl *control_);
int32
PDWTSceneControlBegin(PDWTSceneControl *control_,
                      ScreenContext    *screenContext_,
                      Item              vramIOReq_,
                      Item              vblIOReq_,
                      DecisionAudio    *audio_,
                      PDWTSceneClock   *clock_);
void
PDWTSceneControlEnd(PDWTSceneControl *control_);
int32
PDWTSceneControlIsActive(void);
void
PDWTSceneControlPresent(int32 screen_);
int32
PDWTSceneControlPoll(void);
int32
PDWTSceneControlWaitFields(Item  vblIOReq_,
                           int32 fields_);
int32
PDWTSceneControlWaitClock(PDWTSceneClock *clock_,
                          Item            vblIOReq_,
                          int32           fields_);
int32
PDWTSceneControlWaitClockEnd(PDWTSceneClock *clock_,
                             Item            vblIOReq_);
int32
PDWTSceneControlWaitAudio(DecisionAudio *audio_,
                          Item           vblIOReq_);
