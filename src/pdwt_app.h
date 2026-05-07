#pragma once

#include "3do_types.h"

extern Item VRAMIOReq;
extern Item VBLIOReq;
extern Boolean secretpass;
extern const uchar  *passcode;
extern uchar secretcode[8];
extern int32 codenum;
extern int32 checksecret;

extern Item itemRenderBitmap1;
extern Item itemRenderBitmap2;
extern Item itemRenderBitmap3;
extern Item itemRenderBitmap4;
extern Item itemRenderBitmap5;
extern BitmapPtr renderBitmap1;
extern BitmapPtr renderBitmap2;
extern BitmapPtr renderBitmap3;
extern BitmapPtr renderBitmap4;
extern BitmapPtr renderBitmap5;
extern ubyte    *bitmapBuffer1;
extern ubyte    *bitmapBuffer2;
extern ubyte    *bitmapBuffer3;
extern ubyte    *bitmapBuffer4;
extern ubyte    *bitmapBuffer5;

extern CCBPtr Cel1;
extern CCBPtr Cel2;
extern CCBPtr Cel3;
extern CCBPtr Cel4;
extern CCBPtr Cel5;

extern ScreenContext gScreenContext;

Boolean
_start_up(void);
Boolean
MakeCels1(void);
Boolean
MakeCels2(void);
Boolean
MakeCels3(void);
Boolean
MakeCels4(void);
Boolean
MakeCels5(void);
Boolean
initbitmap(void);
int32
mySecretFun(PlayerPtr ctx_);
int32
myUserFunction(PlayerPtr ctx_);
void
Streamin(uint32 arg0_);
