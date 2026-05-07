#pragma once

#include "3do_types.h"
#include "soundfile.h"

CCB *
PDWTLoadCel(const char *path_,
            uint32      mem_type_bits_);
int32
PDWTLoadSoundFile(SoundFilePlayer *player_,
                  const char      *path_);
Item
PDWTOpenDiskFile(const char *path_);

void
PDWTLogAsset(const char *kind_,
             const char *path_,
             int32       result_);
void
PDWTLogScene(const char *phase_,
             const char *name_,
             int32       result_);
void
PDWTLogProcess(const char *phase_,
               const char *name_,
               int32       result_);
int32
PDWTRunScene(const char *name_,
             int32     (*scene_function_)(void));
