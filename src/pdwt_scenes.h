#pragma once

#include "3do_types.h"
#include "soundfile.h"

extern int32 point;
extern int32 startover;

int32
FUNeffectout(CCB *cel_);
int32
FUNSC02effectout(CCB *cel_);
int32
FUNeffectouth(CCB *cel_);
int32
FUNeffectin(CCB *cel_);
int32
FUNeffectinh(CCB *cel_);
int32
shakeeffect(CCB *cel_);
int32
PlaySoundFile(SoundFilePlayer *sfp_,
              uint8           *fileName_);

int32
DEC00(void);
int32
DEC07(void);
int32
DEC08(void);
int32
DEC12(void);
int32
DEC13(void);
int32
FunSC01(void);
int32
FunSC02(void);
int32
FunSC03(void);
int32
FunSC04(void);
int32
FunSC06(void);
int32
FunSC07(void);
int32
FunSC08(void);
int32
FunSC09(void);
int32
FunSC10(void);
int32
FunSC11(void);
int32
FunSC12(void);
int32
FunSC13(void);
int32
playplumber(void);

Boolean
InitBackPic(ScreenContext *screenContext_,
            Item           vramIOReq_);
int32
CopyBackPic(ScreenContext *screenContext_,
            Bitmap        *bitmap_,
            Item           vramIOReq_);
Boolean
InitPicture(ScreenContext *screenContext_);
int32
DisplayBarnDoorOpen(ScreenContext *screenContext_,
                    Item           vramIOReq_,
                    CCB           *underCel_,
                    CCB           *doorCel_);
int32
DisplayCenterMerge(ScreenContext *screenContext_,
                   Item           vramIOReq_,
                   CCB           *mergeCel_,
                   CCB           *underCel_);
void
PutNumXY(ScreenContext *screenContext_,
         int32          score_,
         Coord          x_,
         Coord          y_);
int32
bullethole(CCB           *backCel_,
           int32          remaining_,
           Item           vblIOReq_,
           Item           vramIOReq_,
           ScreenContext *screenContext_);
int32
decSpeech(DecDataPtr choice_);
PicDataPtr
ReadPictureData(uint8 *name_,
                int32 *count_);
int32
showpicture(ScreenContext *screenContext_,
            uint8         *sceneName_);
DecDataPtr
ReadDecisionData(uint8 *name_,
                 int32 *count_);
int32
makedecision(ScreenContext *screenContext_,
             uint8         *name_);
void
showOption(uint8 *option_,
           int32 *finish_);
int32
ShowCase(ScreenContext *screenContext_,
         uint8         *sceneName_);
