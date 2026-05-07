#pragma once

#include "3do_types.h"
#include "audio.h"

typedef struct PDWTSceneClock
{
  AudioTime startTime;
  AudioTime pauseTime;
  uint32 totalFields;
  uint32 elapsedFields;
  uint32 audioTicks;
  uint32 audioTickFrames;
  uint32 elapsedTicks;
  uint32 tickRemainder;
  const uint32 *targetDSPFrames;
  uint32 targetOffsetDSPFrames;
  uint32 targetCount;
  uint32 targetIndex;
  int32 active;
  int32 paused;
} PDWTSceneClock;

void
PDWTSceneClockStart(PDWTSceneClock *clock_,
                    const uint8    *sceneName_,
                    uint32          totalFields_);
void
PDWTSceneClockPause(PDWTSceneClock *clock_);
void
PDWTSceneClockResume(PDWTSceneClock *clock_);
void
PDWTSceneClockAdvance(PDWTSceneClock *clock_,
                      int32           fields_);
int32
PDWTSceneClockReached(const PDWTSceneClock *clock_);
int32
PDWTSceneClockEndReached(const PDWTSceneClock *clock_);
int32
PDWTSceneClockWait(PDWTSceneClock *clock_,
                   Item            vblIOReq_,
                   int32           fields_,
                   uint32          stopMask_);
int32
PDWTSceneClockWaitForEnd(PDWTSceneClock *clock_,
                         Item            vblIOReq_,
                         uint32          stopMask_);
int32
PDWTSceneClockIsCalibrated(const PDWTSceneClock *clock_);
