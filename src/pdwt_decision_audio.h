#pragma once

#include "3do_types.h"
#include "soundfile.h"

typedef struct DecisionAudio
{
  SoundFilePlayer *player;
  int32 signalNeeded;
  int32 loaded;
  int32 started;
  int32 paused;
}
DecisionAudio;

void
DecisionAudioInit(DecisionAudio *audio_);
int32
DecisionAudioStart(DecisionAudio *audio_,
                   uint8         *fileName_,
                   int32          bufferSize_,
                   int32          amplitude_);
int32
DecisionAudioStartWithBuffers(DecisionAudio *audio_,
                              uint8         *fileName_,
                              int32          bufferCount_,
                              int32          bufferSize_,
                              int32          amplitude_);
int32
DecisionAudioService(DecisionAudio *audio_);
int32
DecisionAudioPause(DecisionAudio *audio_);
int32
DecisionAudioResume(DecisionAudio *audio_);
int32
DecisionAudioIsActive(const DecisionAudio *audio_);
int32
DecisionAudioStop(DecisionAudio *audio_);
