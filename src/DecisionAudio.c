// DecisionAudio.c - sound-file playback for decision screens.
#include "pdwt_decision_audio.h"

#include "3do_types.h"

#include "handy_tools.h"
#include "kernel.h"
#include "task.h"
#include "umemory.h"


#define DECISION_AUDIO_BUFFER_COUNT 2

static
void
_delete_incomplete_sound_file_player(SoundFilePlayer *player_);


static
int32
_is_complete_sound_file_player(const SoundFilePlayer *player_,
                               int32                  bufferCount_);
static
void
_delete_incomplete_sound_file_player(SoundFilePlayer *player_)
{
  int32 i;

  if(player_ == NULL)
    {
      return;
    }

  for(i = 0; i < (int32)player_->sfp_NumBuffers; i++)
    {
      if(player_->sfp_BufferAddrs[i] != 0)
        {
          EZMemFree(player_->sfp_BufferAddrs[i]);
        }
    }
  if(player_->sfp_Spooler != 0)
    {
      ssplDeleteSoundSpooler(player_->sfp_Spooler);
    }
  (void)Free(player_);
}


static
int32
_is_complete_sound_file_player(const SoundFilePlayer *player_,
                               int32                  bufferCount_)
{
  int32 i;

  if(player_ == NULL || player_->sfp_Spooler == NULL ||
     player_->sfp_NumBuffers != (uint32)bufferCount_)
    {
      return 0;
    }
  for(i = 0; i < bufferCount_; i++)
    {
      if(player_->sfp_BufferAddrs[i] == 0)
        {
          return 0;
        }
    }
  return 1;
}


void
DecisionAudioInit(DecisionAudio *audio_)
{
  if(audio_ == NULL)
    {
      return;
    }

  audio_->player       = 0;
  audio_->signalNeeded = 0;
  audio_->loaded       = 0;
  audio_->started      = 0;
  audio_->paused       = 0;
}


int32
DecisionAudioIsActive(const DecisionAudio *audio_)
{
  return audio_ != NULL && audio_->player != NULL && audio_->started;
}


int32
DecisionAudioPause(DecisionAudio *audio_)
{
  int32 result;

  if(audio_ == NULL)
    {
      return -1;
    }
  if(!DecisionAudioIsActive(audio_) || audio_->paused)
    {
      return 0;
    }

  result = ssplPause(audio_->player->sfp_Spooler);
  if(result < 0)
    {
      return result;
    }

  audio_->paused = 1;
  return 0;
}


int32
DecisionAudioResume(DecisionAudio *audio_)
{
  int32 result;

  if(audio_ == NULL)
    {
      return -1;
    }
  if(!DecisionAudioIsActive(audio_) || !audio_->paused)
    {
      return 0;
    }

  result = ssplResume(audio_->player->sfp_Spooler);
  if(result < 0)
    {
      return result;
    }

  audio_->paused = 0;
  return 0;
}


int32
DecisionAudioStop(DecisionAudio *audio_)
{
  int32 cleanup_result;
  int32 result;

  if(audio_ == NULL)
    {
      return -1;
    }

  if(audio_->player == NULL)
    {
      return 0;
    }

  result = 0;
  if(audio_->started)
    {
      cleanup_result = StopSoundFile(audio_->player);
      if(cleanup_result < 0)
        {
          result = cleanup_result;
        }
      else
        {
          audio_->started = 0;
          audio_->paused  = 0;
        }
    }

  if(audio_->loaded)
    {
      cleanup_result = UnloadSoundFile(audio_->player);
      if(cleanup_result < 0)
        {
          if(result >= 0)
            {
              result = cleanup_result;
            }
        }
      else
        {
          audio_->started = 0;
          audio_->loaded  = 0;
          audio_->paused  = 0;
        }
    }

  cleanup_result = DeleteSoundFilePlayer(audio_->player);
  if(cleanup_result < 0)
    {
      if(result >= 0)
        {
          result = cleanup_result;
        }
    }
  else
    {
      audio_->player       = NULL;
      audio_->signalNeeded = 0;
      audio_->started      = 0;
      audio_->loaded       = 0;
      audio_->paused       = 0;
    }

  return result;
}


int32
DecisionAudioStart(DecisionAudio *audio_,
                   uint8         *fileName_,
                   int32          bufferSize_,
                   int32          amplitude_)
{
  return DecisionAudioStartWithBuffers(
    audio_, fileName_, DECISION_AUDIO_BUFFER_COUNT, bufferSize_, amplitude_);
}


int32
DecisionAudioStartWithBuffers(DecisionAudio *audio_,
                              uint8         *fileName_,
                              int32          bufferCount_,
                              int32          bufferSize_,
                              int32          amplitude_)
{
  int32 result;

  if(audio_ == NULL ||
     fileName_ == NULL ||
     bufferCount_ < 2 ||
     bufferCount_ > MAX_SOUNDFILE_BUFS ||
     bufferSize_ <= 0)
    {
      return -1;
    }

  result = DecisionAudioStop(audio_);
  if(result < 0)
    {
      return result;
    }

  audio_->player = CreateSoundFilePlayer(bufferCount_, bufferSize_, 0);
  if(audio_->player == NULL)
    {
      return -1;
    }
  if(!_is_complete_sound_file_player(audio_->player, bufferCount_))
    {
      _delete_incomplete_sound_file_player(audio_->player);
      audio_->player = 0;
      return -1;
    }

  audio_->loaded = 1;
  result        = LoadSoundFile(audio_->player, (char *)fileName_);
  if(result < 0)
    {
      DecisionAudioStop(audio_);
      return result;
    }
  result = StartSoundFile(audio_->player, amplitude_);
  if(result < 0)
    {
      DecisionAudioStop(audio_);
      return result;
    }

  audio_->started      = 1;
  audio_->signalNeeded = result;
  if(audio_->signalNeeded == 0)
    {
      result = ServiceSoundFile(audio_->player, 0, &audio_->signalNeeded);
      if(result < 0)
        {
          DecisionAudioStop(audio_);
          return result;
        }
    }

  if(audio_->signalNeeded == 0)
    {
      return DecisionAudioStop(audio_);
    }

  return 0;
}


int32
DecisionAudioService(DecisionAudio *audio_)
{
  int32 pendingSignals;
  int32 result;
  int32 waitSignals;

  if(audio_ == NULL)
    {
      return -1;
    }
  if(audio_->player == NULL)
    {
      return 0;
    }

  pendingSignals = (int32)(GetCurrentSignals() & (uint32)audio_->signalNeeded);
  if(pendingSignals == 0)
    {
      return 1;
    }

  waitSignals    = WaitSignal((uint32)pendingSignals);
  pendingSignals = (int32)((uint32)waitSignals & (uint32)audio_->signalNeeded);
  if(pendingSignals == 0)
    {
      return 1;
    }
  result         = ServiceSoundFile(audio_->player, pendingSignals, &audio_->signalNeeded);
  if(result < 0)
    {
      DecisionAudioStop(audio_);
      return result;
    }
  if(audio_->signalNeeded == 0)
    {
      result = DecisionAudioStop(audio_);
      if(result < 0)
        {
          return result;
        }
      return 0;
    }

  return 1;
}
