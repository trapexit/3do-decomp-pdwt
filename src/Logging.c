// Logging.c - PDWT asset/scene/process logging and wrapped load helpers.
#include "pdwt_log.h"

#include "3do_types.h"

#include "celutils.h"
#include "debug.h"
#include "filefunctions.h"


void
PDWTLogAsset(const char *kind_,
             const char *path_,
             int32       result_)
{
  kprintf("[PDWT][asset] %s path=%s result=$%lx\n", kind_, path_, result_);
}


void
PDWTLogScene(const char *phase_,
             const char *name_,
             int32       result_)
{
  kprintf("[PDWT][scene] %s name=%s result=%ld\n", phase_, name_, result_);
}


void
PDWTLogProcess(const char *phase_,
               const char *name_,
               int32       result_)
{
  kprintf("[PDWT][process] %s name=%s result=%ld\n", phase_, name_, result_);
}


CCB *
PDWTLoadCel(const char *path_,
            uint32      mem_type_bits_)
{
  CCB *cel;

  cel = LoadCel((char *)path_, mem_type_bits_);
  PDWTLogAsset("cel", path_, (int32)cel);
  return cel;
}


int32
PDWTLoadSoundFile(SoundFilePlayer *player_,
                  const char      *path_)
{
  int32 result;

  result = LoadSoundFile(player_, (char *)path_);
  PDWTLogAsset("sound", path_, result);
  return result;
}


Item
PDWTOpenDiskFile(const char *path_)
{
  Item item;

  item = OpenDiskFile((char *)path_);
  PDWTLogAsset("file", path_, item);
  return item;
}


int32
PDWTRunScene(const char *name_,
             int32     (*scene_function_)(void))
{
  int32 result;

  if(name_ == NULL || scene_function_ == NULL)
    {
      return -1;
    }

  PDWTLogScene("enter", name_, 0);
  result = scene_function_();
  PDWTLogScene("exit", name_, result);
  return result;
}
